#ifndef GRAPHANALYZER_H
#define GRAPHANALYZER_H

#include "souffle/problog/DerivationGraph.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <array>
#include <cstdlib>

enum class SISORegionKind {
    Unknown = 0,
    General,
    SingleHyperedge,  // single hyperedge from SI to SO
    LinearTwoEdge,    // SI -> mid -> SO (two edges chain)
    ParallelEdge,  // >=2 parallel single-input edges SI -> SO
    AllFactsToSO,     // single edge, inputs all facts (no incoming edges or evidence/output)
    FanOutConverge,   // SI fan-out to xi, then single AND edge xi... -> SO (polarity-aware)
};

struct SISORegionInfo {
    std::vector<NodePtr> internalNodes;      // All nodes in the region (including SI / SO)
    std::vector<EdgePtr> internalEdges;      // All edges in the region (edges on SI->SO paths)
    NodePtr entry = nullptr;                 // SI node (entry)
    std::vector<NodePtr> entryPreds;         // External predecessors of SI (may be empty)
    NodePtr exit = nullptr;                  // SO node (exit)
    bool valid = false;
    bool prefixAllFactsRequired = false;     // Keep this flag for now; may use support for finer checks later
    SISORegionKind kind = SISORegionKind::Unknown;  // SISO kind tag for fast-path handling
};

class GraphAnalyzer {
private:
    using NodeSet     = std::unordered_set<NodePtr>;
    using EdgeSet     = std::unordered_set<EdgePtr>;
    using SupportMap  = std::unordered_map<NodePtr, NodeSet>;
    using EdgeDomInputs = std::unordered_map<EdgePtr, std::vector<NodePtr>>;

    // ======= Debug output =======
    static std::ostream& dbg() {
        static std::ofstream out("siso_debug.log", std::ios::app);
        if (out.is_open()) return out;
        return std::cerr;
    }

    static std::string ts() {
        auto now = std::chrono::system_clock::now();
        std::time_t t = std::chrono::system_clock::to_time_t(now);
        std::tm tm{};
#if defined(_MSC_VER)
        localtime_s(&tm, &t);
#else
        if (auto* p = std::localtime(&t)) tm = *p;
#endif
        std::ostringstream ss;
        ss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    }

    static void log(const std::string& msg) {
        dbg() << "[" << ts() << "] " << msg << '\n';
    }

    static std::string escapeDot(const std::string& s) {
        std::string r;
        r.reserve(s.size());
        for (char c : s) {
            if (c == '"') r.push_back('\'');
            else if (c == '\\') {
                r.push_back('\\');
                r.push_back('\\');
            } else {
                r.push_back(c);
            }
        }
        return r;
    }

    // ======= Dominator structure & prefix structure =======
    struct DomGraph {
        std::vector<NodePtr>              nodes;    // nodes in dom-graph
        std::unordered_map<NodePtr, int>  indexOf;  // node -> index
        std::vector<std::vector<int>>     preds;    // preds[i] = predecessor indices
        int entryIndex = -1;                        // chosen root for dominance
    };

    struct DomInfo {
        std::vector<std::unordered_set<int>> domSets;  // domSets[i] = set of dominators of i
        std::vector<int>                     idom;     // immediate dominator of i
    };

    struct PrefixStructure {
        SupportMap    support;
        EdgeDomInputs domInputs;
        DomGraph      dg;
        DomInfo       dom;
    };

    struct Region {
        NodeSet nodes;
        EdgeSet edges;
    };

    static SISORegionInfo makeRegion(NodePtr entry, NodePtr exit, const std::vector<EdgePtr>& edges,
            SISORegionKind kind) {
        SISORegionInfo info;
        if (!entry || !exit) return info;
        info.entry = entry;
        info.exit = exit;
        info.entryPreds = {};  // not used in fast path
        info.internalEdges.assign(edges.begin(), edges.end());
        // collect nodes from edges plus entry/exit
        NodeSet nodeSet;
        nodeSet.insert(entry);
        nodeSet.insert(exit);
        for (auto e : edges) {
            if (!e) continue;
            nodeSet.insert(e->getInputs().begin(), e->getInputs().end());
            NodePtr out = e->getOutput();
            if (out) nodeSet.insert(out);
        }
        info.internalNodes.assign(nodeSet.begin(), nodeSet.end());
        info.valid = true;
        info.kind = kind;
        return info;
    }

    struct FastPathDetectStats {
        long singleHyperedgeMs = 0;
        long linearTwoEdgeMs   = 0;
        long parallelTwoEdgeMs = 0;
        long allFactsToSOMs    = 0;
        long fanOutConvergeMs  = 0;
        size_t singleHyperedgeCount = 0;
        size_t linearTwoEdgeCount   = 0;
        size_t parallelTwoEdgeCount = 0;
        size_t allFactsToSOCount    = 0;
        size_t fanOutConvergeCount  = 0;
    };

    struct DetectionFrontier {
        NodeSet nodes;
        EdgeSet edges;
    };

    static bool isNodeInView(const DerivationGraphViewInterface& g, NodePtr n) {
        return n && g.getNodes().count(n) > 0;
    }

    static bool isEdgeInView(const DerivationGraphViewInterface& g, EdgePtr e) {
        return e && g.getEdges().count(e) > 0;
    }

    static void addEdgeEndpoints(const DerivationGraphViewInterface& g, EdgePtr e, NodeSet& nodes) {
        if (!isEdgeInView(g, e)) return;
        NodePtr out = g.getOutput(e);
        if (isNodeInView(g, out)) {
            nodes.insert(out);
        }
        for (auto in : g.getInputs(e)) {
            if (isNodeInView(g, in)) {
                nodes.insert(in);
            }
        }
    }

    static DetectionFrontier buildDetectionFrontier(
            const DerivationGraphViewInterface& g,
            const NodeSet& dirtyNodes,
            const EdgeSet& dirtyEdges) {
        DetectionFrontier frontier;

        for (auto n : dirtyNodes) {
            if (isNodeInView(g, n)) {
                frontier.nodes.insert(n);
            }
        }
        for (auto e : dirtyEdges) {
            if (!isEdgeInView(g, e)) continue;
            frontier.edges.insert(e);
        }
        for (auto e : frontier.edges) {
            addEdgeEndpoints(g, e, frontier.nodes);
        }

        // Two-hop expansion around dirty seeds to capture local pattern changes:
        // edge-local rewrites, SO-grouping, and small chain updates.
        constexpr int kExpandRounds = 2;
        for (int round = 0; round < kExpandRounds; ++round) {
            std::vector<NodePtr> nodeSnapshot(frontier.nodes.begin(), frontier.nodes.end());
            for (auto n : nodeSnapshot) {
                if (!isNodeInView(g, n)) continue;
                for (auto inEdge : g.getIncomingEdges(n)) {
                    if (!isEdgeInView(g, inEdge)) continue;
                    frontier.edges.insert(inEdge);
                    addEdgeEndpoints(g, inEdge, frontier.nodes);
                }
                for (auto outEdge : g.getOutgoingEdges(n)) {
                    if (!isEdgeInView(g, outEdge)) continue;
                    frontier.edges.insert(outEdge);
                    addEdgeEndpoints(g, outEdge, frontier.nodes);
                }
            }
        }

        return frontier;
    }

    // Fast-path detectors (<=2 edges).
    // If candidate sets are provided, scanning is restricted to that local frontier.
    static std::vector<SISORegionInfo> detectFastPathRegions(
            const DerivationGraphViewInterface& g,
            FastPathDetectStats* stats = nullptr,
            const EdgeSet* candidateEdges = nullptr,
            const NodeSet* candidateNodes = nullptr) {
        std::vector<SISORegionInfo> regions;
        bool debug = std::getenv("SOUFFLE_SISO_FAST_DEBUG") != nullptr;
        std::vector<EdgePtr> edgeScan;
        std::vector<NodePtr> nodeScan;
        if (candidateEdges) {
            edgeScan.reserve(candidateEdges->size());
            for (auto e : *candidateEdges) {
                if (isEdgeInView(g, e)) edgeScan.push_back(e);
            }
        } else {
            edgeScan.reserve(g.getEdges().size());
            for (auto e : g.getEdges()) {
                if (e) edgeScan.push_back(e);
            }
        }
        if (candidateNodes) {
            nodeScan.reserve(candidateNodes->size());
            for (auto n : *candidateNodes) {
                if (isNodeInView(g, n)) nodeScan.push_back(n);
            }
        } else {
            nodeScan.reserve(g.getNodes().size());
            for (auto n : g.getNodes()) {
                if (n) nodeScan.push_back(n);
            }
        }

        // 1) Single hyperedge: one edge exit, inputs.size()>=1, exactly one non-fact (SI), others are input facts
        auto tSingleStart = std::chrono::steady_clock::now();
        for (auto e : edgeScan) {
            if (!e) continue;
            auto inputs = g.getInputs(e);
            if (inputs.size() <= 1) {
                if (debug && inputs.size() == 1) {
                    std::cout << "[siso-fast] edge " << e->getId()
                              << " -> " << (g.getOutput(e) ? g.getOutput(e)->toString() : "null")
                              << " skip: single-input edge is trivial\n";
                }
                continue;
            }
            NodePtr exit = g.getOutput(e);
            if (!exit) continue;
            size_t factInputs = 0;
            NodePtr si = nullptr;
            bool invalid = false;
            if (debug) {
                std::cout << "[siso-fast] edge " << e->getId() << " -> " << (exit ? exit->toString() : "null")
                          << " as single-hyperedge candidate" << std::endl;
            }
            for (auto n : inputs) {
                if (!n) {
                    invalid = true;
                    if (debug) std::cout << "  skip: null input\n";
                    break;
                }
                if (n->isFact && g.getIncomingEdges(n).empty() && !n->hasEvidence() && !n->needOutput) {
                    auto outs = g.getOutgoingEdges(n);
                    if (outs.size() != 1) {
                        invalid = true;
                        if (debug) {
                            std::cout << "  skip: fact input has " << outs.size()
                                      << " outgoing edges (must be exactly 1)" << std::endl;
                        }
                        break;
                    }
                    ++factInputs;
                } else if (!si) {
                    si = n;
                } else {
                    invalid = true;
                    if (debug) std::cout << "  skip: more than one non-fact input\n";
                    break;  // more than one non-fact
                }
            }
            if (invalid) continue;
            if (!si) {
                if (debug) std::cout << "  skip: all inputs are facts (handled elsewhere)\n";
                continue;  // all facts handled elsewhere
            }
            if (factInputs == 0) {
                if (debug) std::cout << "  skip: no fact inputs to absorb\n";
                continue;  // nothing to absorb; would loop after rewrite
            }
            if (si->isFact) {
                if (debug) std::cout << "  skip: SI is fact (overlaps all-facts)\n";
                continue;  // force SI to be non-fact to avoid overlap with all-facts
            }
            if (factInputs + 1 != inputs.size()) {
                if (debug) std::cout << "  skip: inputs not exactly one non-fact + facts\n";
                continue;
            }
            if (si == exit) {
                if (debug) std::cout << "  skip: SI==SO\n";
                continue;
            }
            auto region = makeRegion(si, exit, {e}, SISORegionKind::SingleHyperedge);
            regions.push_back(std::move(region));
            if (stats) stats->singleHyperedgeCount++;
            continue;
        }
        auto tSingleEnd = std::chrono::steady_clock::now();
        if (stats) {
            stats->singleHyperedgeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    tSingleEnd - tSingleStart)
                                               .count();
        }

        // 3) Linear two-edge: entry->mid->exit, each edge single input
        auto tLinearStart = std::chrono::steady_clock::now();
        for (auto e1 : edgeScan) {
            if (!e1) continue;
            auto in1 = g.getInputs(e1);
            if (in1.size() != 1) continue;
            auto neg1 = g.getBodyNegationsStable(e1);
            if (neg1.size() > 1) continue;  // expect single-input edge
            NodePtr entry = in1[0];
            NodePtr mid = g.getOutput(e1);
            if (!entry || !mid) continue;
            if (entry == mid) continue;
            if (mid->hasEvidence() || mid->needOutput) continue;  // mid cannot be query/evidence
            // Linear contraction deletes `mid`, so `mid` must be single-source from this edge.
            auto midIn = g.getIncomingEdges(mid);
            if (midIn.size() != 1 || midIn[0] != e1) continue;
            // mid should have exactly one outgoing edge for the chain
            auto midOut = g.getOutgoingEdges(mid);
            if (midOut.size() != 1) continue;
            EdgePtr e2 = midOut[0];
            if (!e2) continue;
            auto in2 = g.getInputs(e2);
            if (in2.size() != 1 || in2[0] != mid) continue;
            auto neg2 = g.getBodyNegationsStable(e2);
            if (neg2.size() > 1) continue;  // expect single-input edge
            if (!neg2.empty() && neg2[0]) continue;  // do not fast-path if mid->exit is negated
            NodePtr exit = g.getOutput(e2);
            if (!exit || exit == entry || exit == mid) continue;
            regions.push_back(makeRegion(entry, exit, {e1, e2}, SISORegionKind::LinearTwoEdge));
            if (stats) stats->linearTwoEdgeCount++;
        }
        auto tLinearEnd = std::chrono::steady_clock::now();
        if (stats) {
            stats->linearTwoEdgeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    tLinearEnd - tLinearStart)
                                            .count();
        }

        // 4) Parallel edges: same SI -> same SO, each edge has exactly one input.
        // Linear-time grouping by SO and then SI to avoid O(m^2).
        auto tParallelStart = std::chrono::steady_clock::now();
        for (NodePtr so : nodeScan) {
            if (!so) continue;
            auto incoming = g.getIncomingEdges(so);
            if (incoming.size() < 2) continue;  // need at least two edges to form parallel region
            // group single-input edges by their sole input (SI) and negation flag
            std::unordered_map<NodePtr, std::array<std::vector<EdgePtr>, 2>> bySiNeg;
            for (EdgePtr e : incoming) {
                if (!e) continue;
                auto ins = g.getInputs(e);
                if (ins.size() != 1) continue;
                const auto& negs = g.getBodyNegationsStable(e);
                if (!negs.empty() && negs.size() != 1) continue;  // keep only single-input with aligned neg flag
                bool isNeg = (!negs.empty() && negs[0]);
                NodePtr si = ins[0];
                if (!si) continue;
                bySiNeg[si][isNeg ? 1 : 0].push_back(e);
            }
            for (auto& kv : bySiNeg) {
                NodePtr si = kv.first;
                if (si == so) continue;
                auto& groups = kv.second;
                for (int idx = 0; idx < 2; ++idx) {
                    auto& edges = groups[idx];
                    if (edges.size() < 2) continue;
                    regions.push_back(makeRegion(si, so, edges, SISORegionKind::ParallelEdge));
                    if (stats) stats->parallelTwoEdgeCount++;
                }
            }
        }
        auto tParallelEnd = std::chrono::steady_clock::now();
        if (stats) {
            stats->parallelTwoEdgeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    tParallelEnd - tParallelStart)
                                              .count();
        }

        // 2) Fan-out converge (SI fact fan-out to xi, xi converge to SO via one multi-input edge)
        auto tFanStart = std::chrono::steady_clock::now();
        for (NodePtr si : nodeScan) {
            if (!si) continue;
            if (!si->isFact || si->hasEvidence() || si->needOutput) continue;
            auto outsSi = g.getOutgoingEdges(si);
            if (outsSi.size() < 2) continue;  // need fan-out
            bool bad = false;
            std::vector<EdgePtr> fanEdges;
            std::vector<NodePtr> xiNodes;
            for (EdgePtr e : outsSi) {
                if (!e) continue;
                auto ins = g.getInputs(e);
                if (ins.size() != 1 || ins[0] != si) {
                    bad = true; break;
                }
                fanEdges.push_back(e);
                xiNodes.push_back(g.getOutput(e));
            }
            if (bad || fanEdges.size() < 2) continue;
            // ensure xi nodes are unique and only used here
            std::unordered_set<NodePtr> xiSet;
            for (NodePtr x : xiNodes) {
                if (!x) { bad = true; break; }
                if (!xiSet.insert(x).second) { bad = true; break; }
                auto inX = g.getIncomingEdges(x);
                auto outX = g.getOutgoingEdges(x);
                if (inX.size() != 1 || outX.size() != 1) { bad = true; break; }
            }
            if (bad) continue;
            // all xi must share the same convergence edge
            EdgePtr conv = nullptr;
            for (NodePtr x : xiSet) {
                auto outX = g.getOutgoingEdges(x);
                if (outX.empty()) { bad = true; break; }
                if (!conv) {
                    conv = outX[0];
                } else if (conv != outX[0]) {
                    bad = true; break;
                }
            }
            if (bad || !conv) continue;
            auto convInputs = g.getInputs(conv);
            if (convInputs.size() != xiSet.size()) continue;
            // inputs of conv must be exactly xi and all positive
            auto convNeg = g.getBodyNegationsStable(conv);
            if (!convNeg.empty()) {
                bool allFalse = std::all_of(convNeg.begin(), convNeg.end(), [](bool b){return !b;});
                if (!allFalse) continue;
            }
            std::unordered_set<NodePtr> convInSet(convInputs.begin(), convInputs.end());
            if (convInSet != xiSet) continue;
            NodePtr so = g.getOutput(conv);
            if (!so || so == si) continue;
            // build region: all fan edges + conv edge
            std::vector<EdgePtr> regEdges = fanEdges;
            regEdges.push_back(conv);
            auto region = makeRegion(si, so, regEdges, SISORegionKind::FanOutConverge);
            regions.push_back(std::move(region));
            if (stats) stats->fanOutConvergeCount++;
        }
        auto tFanEnd = std::chrono::steady_clock::now();
        if (stats) {
            stats->fanOutConvergeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    tFanEnd - tFanStart)
                                              .count();
        }

        // 2) All-facts single hyperedge: single edge with all fact inputs and inputs have no incoming edges.
        auto tAllFactsStart = std::chrono::steady_clock::now();
        for (auto e : edgeScan) {
            if (!e) continue;
            auto inputs = g.getInputs(e);
            if (inputs.empty()) continue;
            if (debug) {
                std::cout << "[siso-fast] edge " << e->getId()
                          << " -> " << (g.getOutput(e) ? g.getOutput(e)->toString() : "null")
                          << " as all-facts candidate" << std::endl;
            }
            bool allFacts = true;
            for (auto n : inputs) {
                if (!n || !n->isFact || n->hasEvidence()) {
                    allFacts = false;
                    if (debug) {
                        std::cout << "  skip: input not pure fact or evidence" << std::endl;
                    }
                    break;
                }
                auto outs = g.getOutgoingEdges(n);
                if (outs.size() != 1 || outs[0] != e) {
                    allFacts = false;
                    if (debug) {
                        std::cout << "  skip: input fact has outgoing edges not limited to this region" << std::endl;
                    }
                    break;
                }
            }
            if (!allFacts) continue;
            NodePtr exit = g.getOutput(e);
            if (!exit) continue;
            auto exitIns = g.getIncomingEdges(exit);
            if (exitIns.size() != 1 || exitIns[0] != e) {
                if (debug) {
                    std::cout << "  skip: exit has multiple incoming edges\n";
                }
                continue;
            }
            NodePtr entry = inputs[0];
            auto region = makeRegion(entry, exit, {e}, SISORegionKind::AllFactsToSO);
            regions.push_back(std::move(region));
            if (stats) stats->allFactsToSOCount++;
        }
        auto tAllFactsEnd = std::chrono::steady_clock::now();
        if (stats) {
            stats->allFactsToSOMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                    tAllFactsEnd - tAllFactsStart)
                                           .count();
        }

        return regions;
    }

    static std::size_t minRandomVars() {
        // allow overriding the minimum via env for experiments; default 0 to allow deterministic simplifications
        const char* env = std::getenv("SOUFFLE_SISO_MIN_RANDOM");
        if (env) {
            try {
                return static_cast<std::size_t>(std::stoul(env));
            } catch (...) {
            }
        }
        return 0;
    }

    struct Candidate {
        NodePtr si = nullptr;   // candidate SI
        NodePtr so = nullptr;   // candidate SO
    };

    /**
     * Count internal random variables (probability in (0,1)) in a region,
     * excluding the boundary entry/exit nodes.
     */
    static size_t countRandomVariables(const Region& region, NodePtr entryNode, NodePtr exitNode) {
        size_t randomCount = 0;
        for (const auto& n : region.nodes) {
            if (!n) continue;
            if (n == entryNode || n == exitNode) continue;
            if (!n->isFact) continue;
            double p = n->getProbability();
            if (p > 0.0 && p < 1.0) {
                ++randomCount;
            }
        }
        for (const auto& e : region.edges) {
            if (!e) continue;
            double p = e->getProbability();
            if (p > 0.0 && p < 1.0) {
                ++randomCount;
            }
        }
        return randomCount;
    }

    // ========= support (collect all reachable facts backward) =========
    static NodeSet backwardCollectFacts(
        const DerivationGraphViewInterface& g,
        NodePtr start)
    {
        NodeSet facts;
        std::queue<NodePtr> q;
        std::unordered_set<NodePtr> vis;

        if (!start) return facts;
        q.push(start);
        vis.insert(start);

        while (!q.empty()) {
            NodePtr u = q.front();
            q.pop();

            auto inEdges = g.getIncomingEdges(u);
            if (inEdges.empty() || (u && u->isFact)) {
                facts.insert(u);
                continue;
            }

            for (EdgePtr e : inEdges) {
                if (!e) continue;
                for (NodePtr v : g.getInputs(e)) {
                    if (!v) continue;
                    if (vis.insert(v).second) q.push(v);
                }
            }
        }
        return facts;
    }

    static SupportMap buildSupportMap(const DerivationGraphViewInterface& g) {
        SupportMap mp;
        for (NodePtr n : g.getNodes()) {
            if (!n) continue;
            mp[n] = backwardCollectFacts(g, n);
        }
        log("Support map built for " + std::to_string(mp.size()) + " nodes");
        return mp;
    }

    // ========= Compute dominant input group for each hyperedge =========
    static EdgeDomInputs computeEdgeDomInputs(
        const DerivationGraphViewInterface& g,
        const SupportMap& sup)
    {
        EdgeDomInputs dpi;

        // Debug log
        static const std::string LOGF = "siso_edge_dom_inputs.log";
        std::ofstream logf(LOGF, std::ios::app);
        if (!logf.is_open()) {
            std::ofstream create(LOGF, std::ios::out);
            if (!create.is_open()) {
                std::cerr << "ERROR: cannot open log file: " << LOGF << "\n";
            } else {
                create.close();
                logf.open(LOGF, std::ios::app);
                if (!logf.is_open()) {
                    std::cerr << "ERROR: cannot open log file after creation: " << LOGF << "\n";
                }
            }
        }
        GraphAnalyzer::log("computeEdgeDomInputs start");

        auto overlap = [](const NodeSet& a, const NodeSet& b) {
            if (a.size() < b.size()) {
                for (auto x : a) if (b.count(x)) return true;
            } else {
                for (auto x : b) if (a.count(x)) return true;
            }
            return false;
        };

        for (NodePtr n : g.getNodes()) {
            if (!n) continue;

            for (EdgePtr e : g.getOutgoingEdges(n)) {
                if (!e) continue;
                auto inputs = g.getInputs(e);
                if (inputs.empty()) continue;

                struct Group {
                    std::vector<NodePtr> members;
                    NodeSet              supUnion;
                };
                std::vector<Group> groups;

                logf << "\n===========================================\n";
                logf << "Hyperedge e=" << e->getId()
                     << " → " << g.getOutput(e)->toString()
                     << "(id=" << g.getOutput(e)->getId() << ")\n";

                logf << "Inputs:\n";
                for (NodePtr v : inputs) {
                    logf << "  - " << v->toString()
                         << "(id=" << v->getId() << ")\n";
                }

                logf << "\nSupport sets:\n";
                for (NodePtr v : inputs) {
                    const NodeSet& sv = sup.at(v);
                    logf << "  Input " << v->toString()
                         << "(id=" << v->getId() << ") supports: { ";
                    for (NodePtr f : sv) {
                        logf << f->toString()
                             << "(id=" << f->getId() << ") ";
                    }
                    logf << "}\n";
                }

                // Group by support overlap.
                for (NodePtr v : inputs) {
                    const NodeSet& sv = sup.at(v);

                    int grp = -1;
                    for (int i = 0; i < (int)groups.size(); i++) {
                        if (overlap(sv, groups[i].supUnion)) {
                            grp = i;
                            break;
                        }
                    }

                    if (grp < 0) {
                        groups.push_back(Group{{v}, NodeSet(sv.begin(), sv.end())});
                    } else {
                        groups[grp].members.push_back(v);
                        groups[grp].supUnion.insert(sv.begin(), sv.end());
                    }
                }

                logf << "\nGroups formed:\n";
                for (int gi = 0; gi < (int)groups.size(); gi++) {
                    logf << "  Group " << gi << " members: ";
                    for (NodePtr m : groups[gi].members) {
                        logf << m->toString() << "(id=" << m->getId() << ") ";
                    }
                    logf << "\n    supUnion: { ";
                    for (NodePtr f : groups[gi].supUnion) {
                        logf << f->toString() << "(id=" << f->getId() << ") ";
                    }
                    logf << "}\n";
                }

                if (groups.empty()) continue;

                // Choose the group with the largest supUnion as the dominant input group.
                int   best     = 0;
                size_t bestSize = groups[0].supUnion.size();
                for (int i = 1; i < (int)groups.size(); i++) {
                    if (groups[i].supUnion.size() > bestSize) {
                        best     = i;
                        bestSize = groups[i].supUnion.size();
                    }
                }

                dpi[e] = groups[best].members;

                logf << "\nChosen dominant group:\n  ";
                for (NodePtr m : groups[best].members) {
                    logf << m->toString() << "(id=" << m->getId() << ") ";
                }
                logf << "\n";
            }
        }

        GraphAnalyzer::log("computeEdgeDomInputs done, edges=" + std::to_string(dpi.size()));
        return dpi;
    }

    // ========= Build dom-graph =========
    static DomGraph buildDomGraph(const DerivationGraphViewInterface& g) {
        DomGraph dg;

        // Assign an index to every node.
        int idx = 0;
        for (NodePtr n : g.getNodes()) {
            if (!n) continue;
            dg.indexOf[n] = idx++;
            dg.nodes.push_back(n);
        }

        const int N = (int)dg.nodes.size();
        dg.preds.assign(N, {});

        // Build a directed graph by connecting inputs to outputs via hyperedges.
        for (EdgePtr e : g.getEdges()) {
            if (!e) continue;
            NodePtr out = g.getOutput(e);
            if (!out) continue;

            auto itOut = dg.indexOf.find(out);
            if (itOut == dg.indexOf.end()) continue;
            int outIdx = itOut->second;

            for (NodePtr in : g.getInputs(e)) {
                if (!in) continue;
                auto itIn = dg.indexOf.find(in);
                if (itIn == dg.indexOf.end()) continue;
                int inIdx = itIn->second;

                dg.preds[outIdx].push_back(inIdx);
            }
        }

        // Choose a node with no predecessors as the dominator tree root; if none, use 0.
        dg.entryIndex = -1;
        for (int i = 0; i < N; i++) {
            if (dg.preds[i].empty()) {
                dg.entryIndex = i;
                break;
            }
        }
        if (dg.entryIndex < 0 && N > 0) dg.entryIndex = 0;

        GraphAnalyzer::log("buildDomGraph: N=" + std::to_string(N) +
                           " entryIndex=" + std::to_string(dg.entryIndex));
        return dg;
    }

    // ========= Standard dominator algorithm =========
    static DomInfo computeDominators(const DomGraph& dg) {
        DomInfo info;
        const int N = (int)dg.nodes.size();
        info.domSets.assign(N, {});
        info.idom.assign(N, -1);

        if (N == 0 || dg.entryIndex < 0) return info;

        // Initialize: entry dom set = {entry}, others = all nodes.
        for (int i = 0; i < N; i++) {
            if (i == dg.entryIndex) {
                info.domSets[i] = {i};
            } else {
                for (int j = 0; j < N; j++) {
                    info.domSets[i].insert(j);
                }
            }
        }

        bool changed = true;
        while (changed) {
            changed = false;
            for (int n = 0; n < N; n++) {
                if (n == dg.entryIndex) continue;

                std::unordered_set<int> newDom;
                bool firstPred = true;

                for (int p : dg.preds[n]) {
                    if (firstPred) {
                        newDom = info.domSets[p];
                        firstPred = false;
                    } else {
                        std::unordered_set<int> tmp;
                        for (int x : newDom) {
                            if (info.domSets[p].count(x)) tmp.insert(x);
                        }
                        newDom.swap(tmp);
                    }
                }

                newDom.insert(n);

                if (newDom != info.domSets[n]) {
                    info.domSets[n].swap(newDom);
                    changed = true;
                }
            }
        }

        // Compute immediate dominator
        for (int n = 0; n < N; n++) {
            if (n == dg.entryIndex) {
                info.idom[n] = -1;
                continue;
            }

            int idom = -1;
            for (int d : info.domSets[n]) {
                if (d == n) continue;
                bool isImm = true;
                for (int other : info.domSets[n]) {
                    if (other == n || other == d) continue;
                    if (info.domSets[other].count(d) && info.domSets[n].count(other)) {
                        isImm = false;
                        break;
                    }
                }
                if (isImm) {
                    idom = d;
                    break;
                }
            }
            info.idom[n] = idom;
        }

        GraphAnalyzer::log("computeDominators done");
        return info;
    }

    static PrefixStructure buildPrefixStructure(const DerivationGraphViewInterface& g) {
        PrefixStructure pre;
        auto tSupportStart = std::chrono::steady_clock::now();
        pre.support   = buildSupportMap(g);
        auto tSupportEnd = std::chrono::steady_clock::now();
        std::cout << "[siso-detect] support map took "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(tSupportEnd - tSupportStart).count()
                  << " ms" << std::endl;

        auto tDomInputsStart = std::chrono::steady_clock::now();
        pre.domInputs = computeEdgeDomInputs(g, pre.support);
        auto tDomInputsEnd = std::chrono::steady_clock::now();
        std::cout << "[siso-detect] edge dom inputs took "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(tDomInputsEnd - tDomInputsStart).count()
                  << " ms" << std::endl;

        auto tDomGraphStart = std::chrono::steady_clock::now();
        pre.dg        = buildDomGraph(g);
        auto tDomGraphEnd = std::chrono::steady_clock::now();
        std::cout << "[siso-detect] dom graph build took "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(tDomGraphEnd - tDomGraphStart).count()
                  << " ms" << std::endl;

        auto tDomStart = std::chrono::steady_clock::now();
        pre.dom       = computeDominators(pre.dg);
        auto tDomEnd = std::chrono::steady_clock::now();
        std::cout << "[siso-detect] dominators took "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(tDomEnd - tDomStart).count()
                  << " ms" << std::endl;
        return pre;
    }

    // ========= LCA / join point in the dominator tree =========
    static int lcaInDomTree(int a, int b, const std::vector<int>& idom) {
        if (a < 0 || b < 0) return -1;
        if (a == b) return a;

        auto depth = [&](int x) {
            int d = 0;
            while (x >= 0) {
                x = idom[x];
                d++;
            }
            return d;
        };

        int da = depth(a);
        int db = depth(b);

        while (da > db) {
            a = idom[a];
            --da;
        }
        while (db > da) {
            b = idom[b];
            --db;
        }

        while (a != b && a >= 0 && b >= 0) {
            a = idom[a];
            b = idom[b];
        }
        return (a == b ? a : -1);
    }

    static int confluencePoint(
        const std::vector<int>& nodes,
        const std::vector<int>& idom)
    {
        if (nodes.empty()) return -1;
        int cur = nodes[0];
        for (size_t i = 1; i < nodes.size(); i++) {
            cur = lcaInDomTree(cur, nodes[i], idom);
            if (cur < 0) break;
        }
        return cur;
    }

    // ========= Find SI from a candidate SO node =========
    static Candidate findCandidate(
        const DerivationGraphViewInterface& g,
        NodePtr exitNode,
        const PrefixStructure& pre)
    {
        Candidate cand;
        if (!exitNode) return cand;

        log("findCandidate: exitNode=" + exitNode->toString());

        auto itExitIdx = pre.dg.indexOf.find(exitNode);
        if (itExitIdx == pre.dg.indexOf.end()) {
            log("findCandidate rejected: exitNode not in dom graph");
            return cand;
        }
        int exitIdx = itExitIdx->second;

        // Collect dominant inputs for all incoming hyperedges and compute confluence in the dominator tree.
        std::vector<int> srcIdx;
        for (EdgePtr eIn : g.getIncomingEdges(exitNode)) {
            if (!eIn) continue;

            const std::vector<NodePtr>* domSet = nullptr;
            std::vector<NodePtr> tmp;

            auto it = pre.domInputs.find(eIn);
            if (it != pre.domInputs.end()) {
                domSet = &it->second;       // Previously chosen dominant input group
            } else {
                tmp = g.getInputs(eIn);     // No grouping info; treat all as dominant
                domSet = &tmp;
            }

            for (NodePtr v : *domSet) {
                if (!v) continue;
                auto itV = pre.dg.indexOf.find(v);
                if (itV == pre.dg.indexOf.end()) continue;
                srcIdx.push_back(itV->second);
            }
        }

        if (srcIdx.empty()) {
            // No predecessors: cannot form an SI->SO region
            log("findCandidate rejected: exitNode has no incoming edges");
            return cand;
        }

        int entryIdx = confluencePoint(srcIdx, pre.dom.idom);
        if (entryIdx < 0 || entryIdx == exitIdx) {
            log("findCandidate rejected: invalid entryIdx=" + std::to_string(entryIdx));
            return cand;
        }

        NodePtr entryNode = pre.dg.nodes[entryIdx];
        if (!entryNode) {
            log("findCandidate rejected: entryNode null");
            return cand;
        }

        cand.si = entryNode;
        cand.so = exitNode;
        log("findCandidate success: entry=" + entryNode->toString() +
            " exit=" + exitNode->toString());
        return cand;
    }

    // ========= Build strict / full region =========
    static Region buildStrictRegion(
        const DerivationGraphViewInterface& g,
        NodePtr entryNode,
        NodePtr exitNode,
        const EdgeDomInputs& domInputs)
    {
        Region reg;
        if (!entryNode || !exitNode) return reg;

        std::queue<NodePtr> q;
        q.push(exitNode);
        reg.nodes.insert(exitNode);

        bool reachedEntry = (exitNode == entryNode);

        while (!q.empty()) {
            NodePtr u = q.front();
            q.pop();

            if (u == entryNode) {
                reachedEntry = true;
                continue;
            }

            auto inEdges = g.getIncomingEdges(u);
            for (EdgePtr e : inEdges) {
                if (!e) continue;
                reg.edges.insert(e);

                auto it = domInputs.find(e);
                if (it != domInputs.end()) {
                    for (NodePtr v : it->second) {
                        if (!v) continue;
                        if (reg.nodes.insert(v).second) {
                            q.push(v);
                        }
                    }
                } else {
                    for (NodePtr v : g.getInputs(e)) {
                        if (!v) continue;
                        if (reg.nodes.insert(v).second) {
                            q.push(v);
                        }
                    }
                }
            }
        }

        if (!reachedEntry) {
            reg.nodes.clear();
            reg.edges.clear();
        }
        log("buildStrictRegion " + std::string(reachedEntry ? "reached" : "missed") +
            " entry; nodes=" + std::to_string(reg.nodes.size()) +
            " edges=" + std::to_string(reg.edges.size()));
        return reg;
    }

    static Region buildFullRegion(
        const DerivationGraphViewInterface& g,
        NodePtr entryNode,
        NodePtr exitNode)
    {
        Region reg;
        if (!entryNode || !exitNode) return reg;

        std::queue<NodePtr> q;
        q.push(exitNode);
        reg.nodes.insert(exitNode);

        bool reachedEntry = (exitNode == entryNode);

        while (!q.empty()) {
            NodePtr u = q.front();
            q.pop();

            if (u == entryNode) {
                reachedEntry = true;
                continue;
            }

            auto inEdges = g.getIncomingEdges(u);
            for (EdgePtr e : inEdges) {
                if (!e) continue;
                reg.edges.insert(e);

                for (NodePtr v : g.getInputs(e)) {
                    if (!v) continue;
                    if (reg.nodes.insert(v).second) {
                        q.push(v);
                    }
                }
            }
        }

        if (!reachedEntry) {
            reg.nodes.clear();
            reg.edges.clear();
        }
        log("buildFullRegion " + std::string(reachedEntry ? "reached" : "missed") +
            " entry; nodes=" + std::to_string(reg.nodes.size()) +
            " edges=" + std::to_string(reg.edges.size()));
        return reg;
    }

    // ========= Collect external predecessors of SI (no longer a validity condition) =========
    static bool selectEntryEdgeAndPreds(   // Always returns true now; only collects info
        const DerivationGraphViewInterface& g,
        const Region& fullRegion,
        NodePtr entryNode,
        EdgePtr& entryEdge,
        std::vector<NodePtr>& entryPreds)
    {
        if (!entryNode) return false;

        entryPreds.clear();
        entryEdge = nullptr;

        std::unordered_set<NodePtr> predSet;

        for (EdgePtr eIn : g.getIncomingEdges(entryNode)) {
            if (!eIn) continue;
            NodePtr out = g.getOutput(eIn);
            if (out != entryNode) continue;

            bool hasOutsidePred = false;

            for (NodePtr p : g.getInputs(eIn)) {
                if (!p) continue;

                if (!fullRegion.nodes.count(p)) {
                    hasOutsidePred = true;
                    if (predSet.insert(p).second) {
                        entryPreds.push_back(p);
                    }
                }
            }

            if (hasOutsidePred && !entryEdge) {
                entryEdge = eIn;   // Debug only
            }
        }

        log("selectEntryEdgeAndPreds entry=" + entryNode->toString() +
            " outsidePreds=" + std::to_string(entryPreds.size()));
        return true;
    }

    // ========= Escape check: only look at outgoing edges from internal nodes to outside the region =========
    static bool checkNoEscape(
        const DerivationGraphViewInterface& g,
        const Region& fullRegion,
        NodePtr entryNode,
        NodePtr exitNode)
    {
        for (NodePtr n : fullRegion.nodes) {
            if (!n) continue;

            // entry/exit are boundary nodes; do not check their outgoing
            if (n == entryNode || n == exitNode) continue;

            for (EdgePtr e : g.getOutgoingEdges(n)) {
                if (!e) continue;
                NodePtr out = g.getOutput(e);

                bool outInside = (out && fullRegion.nodes.count(out));

                // Any outgoing edge to outside the region is an escape
                if (!outInside) {
                    log("checkNoEscape fail: node=" + n->toString() +
                        " edge=" + std::to_string(e ? e->getId() : -1) +
                        " out=" + (out ? out->toString() : std::string("null")));
                    return false;
                }
            }
        }
        log("checkNoEscape ok");
        return true;
    }

    // ========= Assemble Region + boundary nodes into SISORegionInfo =========
    static SISORegionInfo assembleSISO(
        const Region& strictRegion,
        const Region& fullRegion,
        NodePtr entryNode,
        const std::vector<NodePtr>& entryPreds,
        NodePtr exitNode)
    {
        (void)strictRegion;  // Not used separately yet; keep for future refinement
        SISORegionInfo info;

        // Must have at least SI and SO nodes
        if (!entryNode || !exitNode) return info;
        if (fullRegion.nodes.size() < 2)    return info;

        // Skip trivial regions with too few internal random variables.
        size_t randomVars = countRandomVariables(fullRegion, entryNode, exitNode);
        if (randomVars < minRandomVars()) {
            log("assembleSISO skip trivial region (randomVars=" + std::to_string(randomVars) + ")");
            return info;
        }

        info.entry = entryNode;
        info.exit  = exitNode;
        info.entryPreds = entryPreds;

        info.internalNodes.assign(fullRegion.nodes.begin(), fullRegion.nodes.end());
        info.internalEdges.assign(fullRegion.edges.begin(), fullRegion.edges.end());

        info.valid = true;
        info.prefixAllFactsRequired = false;

        log("assembleSISO success nodes=" + std::to_string(info.internalNodes.size()) +
            " edges=" + std::to_string(info.internalEdges.size()));
        return info;
    }

    // ========= Detect SISO from candidate SO node (core pipeline) =========
    static SISORegionInfo detectSISOFromExitNodeWithPrefix(
        const DerivationGraphViewInterface& g,
        NodePtr exitNode,
        const PrefixStructure& pre)
    {
        (void)g;
        (void)exitNode;
        (void)pre;
        return {};
    }

    // ========= Compatibility with old interface: start from exitEdge =========
    static SISORegionInfo detectSISOStrictFromExitWithPrefix(
        const DerivationGraphViewInterface& g,
        EdgePtr exitEdge,
        const PrefixStructure& pre)
    {
        SISORegionInfo info;
        if (!exitEdge) return info;

        NodePtr exitNode = g.getOutput(exitEdge);
        if (!exitNode) {
            log("detectSISOStrictFromExitWithPrefix: exitEdge has null output");
            return info;
        }

        return detectSISOFromExitNodeWithPrefix(g, exitNode, pre);
    }

public:
    // ===================== Public API =====================

    // Old interface keeps its signature; current implementation is lightweight fast-path detection (up to two edges), returns empty if none.
    static inline SISORegionInfo detectSISOStrictFromExit(
        const DerivationGraphViewInterface& g,
        EdgePtr exitEdge)
    {
        (void)g;
        (void)exitEdge;
        return {};
    }

    static inline std::vector<SISORegionInfo> detectAllSISOStrictFromExit(
        const DerivationGraphViewInterface& g,
        const std::unordered_set<NodePtr>* dirtyNodes,
        const std::unordered_set<EdgePtr>* dirtyEdges,
        bool forceCompleteDetect = false)
    {
        const char* dirtyEnv = std::getenv("SOUFFLE_SISO_DIRTY_DETECT");
        const bool disableDirtyFromEnv = dirtyEnv && (std::string(dirtyEnv) == "0" ||
                        std::string(dirtyEnv) == "false" || std::string(dirtyEnv) == "FALSE");
        if (forceCompleteDetect || disableDirtyFromEnv) {
            dirtyNodes = nullptr;
            dirtyEdges = nullptr;
        }
        auto t0 = std::chrono::steady_clock::now();
        FastPathDetectStats fastStats;
        std::vector<SISORegionInfo> regions;
        bool usedDirtyFrontier = false;
        bool fallbackToFull = false;
        size_t seedNodes = 0;
        size_t seedEdges = 0;
        size_t frontierNodes = 0;
        size_t frontierEdges = 0;

        if (dirtyNodes && dirtyEdges && (!dirtyNodes->empty() || !dirtyEdges->empty())) {
            seedNodes = dirtyNodes->size();
            seedEdges = dirtyEdges->size();
            auto frontier = buildDetectionFrontier(g, *dirtyNodes, *dirtyEdges);
            frontierNodes = frontier.nodes.size();
            frontierEdges = frontier.edges.size();

            const double nodeRatio = g.getNodes().empty()
                                             ? 0.0
                                             : static_cast<double>(frontierNodes) /
                                                       static_cast<double>(g.getNodes().size());
            const double edgeRatio = g.getEdges().empty()
                                             ? 0.0
                                             : static_cast<double>(frontierEdges) /
                                                       static_cast<double>(g.getEdges().size());
            // Dirty detection is only useful when frontier is materially smaller than full graph.
            constexpr double kFallbackRatio = 0.60;
            if (frontierNodes == 0 || frontierEdges == 0 ||
                    nodeRatio > kFallbackRatio || edgeRatio > kFallbackRatio) {
                fallbackToFull = true;
            } else {
                usedDirtyFrontier = true;
                regions = detectFastPathRegions(g, &fastStats, &frontier.edges, &frontier.nodes);
            }
        }

        if (!usedDirtyFrontier) {
            regions = detectFastPathRegions(g, &fastStats);
        }

        auto t1 = std::chrono::steady_clock::now();
        // Dedup: smaller regions first to avoid overlap.
        auto countKinds = [](const std::vector<SISORegionInfo>& vec) {
            size_t singleHyperedgeCount = 0;
            size_t linearTwoEdgeCount = 0;
            size_t parallelTwoEdgeCount = 0;
            size_t allFactsToSOCount = 0;
            size_t fanOutConvergeCount = 0;
            size_t generalCount = 0;
            size_t unknownCount = 0;
            for (const auto& r : vec) {
                switch (r.kind) {
                case SISORegionKind::SingleHyperedge: ++singleHyperedgeCount; break;
                case SISORegionKind::LinearTwoEdge: ++linearTwoEdgeCount; break;
                case SISORegionKind::ParallelEdge: ++parallelTwoEdgeCount; break;
                case SISORegionKind::AllFactsToSO: ++allFactsToSOCount; break;
                case SISORegionKind::FanOutConverge: ++fanOutConvergeCount; break;
                case SISORegionKind::General: ++generalCount; break;
                default: ++unknownCount; break;
                }
            }
            return std::array<size_t, 7>{
                singleHyperedgeCount, linearTwoEdgeCount, parallelTwoEdgeCount,
                allFactsToSOCount, fanOutConvergeCount, generalCount, unknownCount};
        };
        auto candidatesByKind = countKinds(regions);

        auto tSortStart = std::chrono::steady_clock::now();
        std::sort(regions.begin(), regions.end(),
                [](const SISORegionInfo& a, const SISORegionInfo& b) {
                    return a.internalNodes.size() < b.internalNodes.size();
                });
        auto tSortEnd = std::chrono::steady_clock::now();
        std::vector<SISORegionInfo> result;
        NodeSet usedNodes;
        auto tFilterStart = std::chrono::steady_clock::now();
        for (auto& r : regions) {
            bool overlap = false;
            for (auto n : r.internalNodes) {
                if (!n) continue;
                if (usedNodes.count(n)) {
                    overlap = true;
                    break;
                }
            }
            if (overlap) continue;
            for (auto n : r.internalNodes) {
                if (n) usedNodes.insert(n);
            }
            result.push_back(std::move(r));
        }
        auto tFilterEnd = std::chrono::steady_clock::now();
        auto keptByKind = countKinds(result);

        auto detectMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
        auto sortMs = std::chrono::duration_cast<std::chrono::milliseconds>(tSortEnd - tSortStart).count();
        auto filterMs = std::chrono::duration_cast<std::chrono::milliseconds>(tFilterEnd - tFilterStart).count();

        std::cout << "[siso-detect] mode="
                  << (usedDirtyFrontier ? "dirty-frontier" : "full")
                  << " fallback=" << (fallbackToFull ? 1 : 0)
                  << " seeds(nodes=" << seedNodes << ",edges=" << seedEdges << ")"
                  << " frontier(nodes=" << frontierNodes << ",edges=" << frontierEdges << ")"
                  << std::endl;
        std::cout << "[siso-detect] fast-path regions " << result.size()
                  << " (candidates=" << regions.size()
                  << ", single-hyperedge=" << keptByKind[0]
                  << ", linear-two-edge=" << keptByKind[1]
                  << ", parallel-two-edge=" << keptByKind[2]
                  << ", all-facts=" << keptByKind[3]
                  << ", fan-out-converge=" << keptByKind[4]
                  << ", general=" << keptByKind[5]
                  << ", unknown=" << keptByKind[6]
                  << ")" << std::endl;
        std::cout << "[siso-prof] fast-detect breakdown: "
                  << "single=" << fastStats.singleHyperedgeMs << " ms (" << fastStats.singleHyperedgeCount << ") "
                  << "linear=" << fastStats.linearTwoEdgeMs << " ms (" << fastStats.linearTwoEdgeCount << ") "
                  << "parallel=" << fastStats.parallelTwoEdgeMs << " ms (" << fastStats.parallelTwoEdgeCount << ") "
                  << "all-facts=" << fastStats.allFactsToSOMs << " ms (" << fastStats.allFactsToSOCount << ") "
                  << "fan-out-conv=" << fastStats.fanOutConvergeMs << " ms (" << fastStats.fanOutConvergeCount << ")"
                  << std::endl;
        std::cout << "[siso-prof] detect=" << detectMs << " ms"
                  << " sort=" << sortMs << " ms"
                  << " filter=" << filterMs << " ms"
                  << " candidates(kind:sh/lin/par/all/fan/gen/unk)="
                  << candidatesByKind[0] << "/" << candidatesByKind[1] << "/" << candidatesByKind[2] << "/"
                  << candidatesByKind[3] << "/" << candidatesByKind[4] << "/" << candidatesByKind[5] << "/"
                  << candidatesByKind[6]
                  << " kept=" << result.size()
                  << std::endl;
        return result;
    }

    static inline std::vector<SISORegionInfo> detectAllSISOStrictFromExit(
        const DerivationGraphViewInterface& g,
        bool forceCompleteDetect = false)
    {
        return detectAllSISOStrictFromExit(g, nullptr, nullptr, forceCompleteDetect);
    }

    // Output the full graph and highlight a SISO region with different colors.
    static inline void dumpRegionAsDot(
        const DerivationGraphViewInterface& g,
        const SISORegionInfo& r,
        const std::string& filename)
    {
        const std::string path = DerivationGraphViewInterface::qualifyDumpPath(filename);
        std::ofstream out(path);
        if (!out.is_open()) {
            std::cerr << "Cannot open dot file: " << path << "\n";
            return;
        }

        NodeSet regionNodes(r.internalNodes.begin(), r.internalNodes.end());
        EdgeSet regionEdges(r.internalEdges.begin(), r.internalEdges.end());

        const auto& nodes = g.getNodes();
        const auto& edges = g.getEdges();

        out << "digraph DerivationGraphWithSISO {\n";
        out << "  rankdir=LR;\n";

        // Output nodes first: entry / exit / other region / non-region
        out << "  node [shape=box, style=filled, fillcolor=lightblue];\n";
        for (const auto& n : nodes) {
            if (!n) continue;

            std::string fill = "lightblue";
            if (regionNodes.count(n)) {
                if (r.entry && n == r.entry) {
                    fill = "palegreen";   // SI
                } else if (r.exit && n == r.exit) {
                    fill = "lightcoral"; // SO
                } else {
                    fill = "khaki";      // Other nodes inside the region
                }
            }

            out << "  node" << n->getId()
                << " [label=\""
                << n->getTuple().toString()
                << "\", fillcolor=" << fill << "];\n";
        }

        // Then output hyperedges (point nodes): region edges in red, others in gray.
        out << "  node [shape=point, width=0.2];\n";
        for (const auto& e : edges) {
            if (!e) continue;
            if (e->pruned) continue;

            NodePtr outNode = g.getOutput(e);
            if (!outNode) continue;

            auto inputs = g.getInputs(e);
            if (std::find(inputs.begin(), inputs.end(), outNode) != inputs.end()) {
                // Skip pseudo-cycles where head is in body.
                continue;
            }

            bool inRegion = regionEdges.count(e) > 0;

            // Draw the edge node itself.
            out << "  edge" << e->getId()
                << " ["
                << "color=" << (inRegion ? "red" : "gray")
                << "];\n";

            // Input to edge
            for (NodePtr in : inputs) {
                if (!in) continue;
                if (!nodes.count(in)) continue;
                out << "  node" << in->getId()
                    << " -> edge" << e->getId()
                    << " [color=" << (inRegion ? "red" : "gray") << "];\n";
            }

            // Edge to output
            if (nodes.count(outNode)) {
                out << "  edge" << e->getId()
                    << " -> node" << outNode->getId()
                    << " [color=" << (inRegion ? "red" : "gray") << "];\n";
            }
        }

        out << "}\n";
        out.close();
    }

    // Output the full graph and color all SISO regions (non-SISO nodes/edges in light gray).
    static inline void dumpAllRegionsAsDot(
        const DerivationGraphViewInterface& g,
        const std::vector<SISORegionInfo>& regions,
        const std::string& filename)
    {
        if (!DerivationGraphViewInterface::isDumpDotEnabled()) {
            return;
        }
        const std::string path = DerivationGraphViewInterface::qualifyDumpPath(filename);
        std::ofstream out(path);
        if (!out.is_open()) {
            std::cerr << "Cannot open dot file: " << path << "\n";
            return;
        }
        std::vector<std::string> palette = {
            "#1f77b4", "#ff7f0e", "#2ca02c", "#d62728",
            "#9467bd", "#8c564b", "#e377c2", "#7f7f7f",
            "#bcbd22", "#17becf"
        };

        // Record node/edge region colors (first match when multiple regions) and entry/exit markers.
        std::unordered_map<NodePtr, std::string> nodeColor;
        std::unordered_map<EdgePtr, std::string> edgeColor;
        std::unordered_map<NodePtr, int> entryCount;
        std::unordered_map<NodePtr, int> exitCount;
        for (size_t i = 0; i < regions.size(); ++i) {
            const auto& r = regions[i];
            const std::string col = palette[i % palette.size()];
            for (NodePtr n : r.internalNodes) {
                if (!n) continue;
                nodeColor.emplace(n, col);
            }
            for (EdgePtr e : r.internalEdges) {
                if (!e) continue;
                edgeColor.emplace(e, col);
            }
            if (r.entry) entryCount[r.entry]++;
            if (r.exit) exitCount[r.exit]++;
        }

        out << "digraph SISO_All {\n";
        out << "  rankdir=LR;\n";
        out << "  node [shape=box, style=filled, fillcolor=lightgray, color=gray];\n";

        // All nodes: color if in region, otherwise light gray.
        for (const auto& n : g.getNodes()) {
            if (!n) continue;
            auto it = nodeColor.find(n);
            const std::string col = (it != nodeColor.end()) ? it->second : "#dddddd";
            int periph = 1;
            bool isEntry = entryCount.count(n);
            bool isExit  = exitCount.count(n);
            if (isEntry && isExit) periph = 3;
            else if (isEntry || isExit) periph = 2;
            std::string role;
            if (isEntry) role += "[SI]";
            if (isExit) role += "[SO]";
            out << "  node" << n->getId()
                << " [label=\"" << escapeDot(n->getTuple().toString())
                << "\\n(id=" << n->getId() << ")" << role << "\", fillcolor=\"" << col
                << "\", color=\"" << col << "\", fontcolor=\"black\", peripheries=" << periph << "];\n";
        }

        // Hyperedges as point nodes.
        out << "  node [shape=point, width=0.2, height=0.2, style=filled];\n";
        for (const auto& e : g.getEdges()) {
            if (!e) continue;
            if (e->pruned) continue;
            NodePtr outNode = g.getOutput(e);
            if (!outNode) continue;

            std::string col = "#cccccc";
            auto itCol = edgeColor.find(e);
            if (itCol != edgeColor.end()) col = itCol->second;

            out << "  edge" << e->getId()
                << " [label=\"\", fillcolor=\"" << col << "\", color=\"" << col << "\"];\n";

            for (NodePtr in : g.getInputs(e)) {
                if (!in) continue;
                out << "  node" << in->getId() << " -> edge" << e->getId()
                    << " [color=\"" << col << "\"];\n";
            }
            if (outNode) {
                out << "  edge" << e->getId() << " -> node" << outNode->getId()
                    << " [color=\"" << col << "\"];\n";
            }
        }

        out << "}\n";
    }

    // Simple CSV print of SISO info (updated to node-only version).
    static inline void printSISOInfo(
        const DerivationGraphViewInterface& g,
        const SISORegionInfo& r)
    {
        (void)g; // Not used yet; this function only dumps region info.

        std::ofstream out("siso_info.csv", std::ios::app);
        if (!out.is_open()) {
            std::cout << "cannot open siso_info.csv\n";
            return;
        }

        if (!r.valid) {
            out << "invalid\n";
            return;
        }

        out << "entry_node,entry_preds,exit_node,internal_nodes,internal_edges\n";

        // entry node
        out << "\"";
        if (r.entry) out << r.entry->toString() << "(id=" << r.entry->getId() << ")";
        else out << "null";
        out << "\",";

        // entry preds
        out << "\"";
        if (r.entryPreds.empty()) {
            out << "none";
        } else {
            for (auto p : r.entryPreds) {
                if (!p) continue;
                out << p->toString() << "(id=" << p->getId() << ") ";
            }
        }
        out << "\",";

        // exit node
        out << "\"";
        if (r.exit) out << r.exit->toString() << "(id=" << r.exit->getId() << ")";
        else out << "null";
        out << "\",";

        // internal nodes
        out << "\"";
        for (auto n : r.internalNodes) {
            if (!n) continue;
            out << n->toString() << "(id=" << n->getId() << ") ";
        }
        out << "\",";

        // internal edges
        out << "\"";
        for (auto e : r.internalEdges) {
            if (!e) continue;
            out << "edge" << e->getId() << " ";
        }
        out << "\"\n";
    }
};

#endif // GRAPHANALYZER_H
