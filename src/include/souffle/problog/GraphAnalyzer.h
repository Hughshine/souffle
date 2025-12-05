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
    ParallelTwoEdge,  // two parallel edges SI -> SO
    AllFactsToSO,     // single edge, inputs all facts (no incoming edges or evidence/output)
};

struct SISORegionInfo {
    std::vector<NodePtr> internalNodes;      // region 中所有节点（包含 SI / SO）
    std::vector<EdgePtr> internalEdges;      // region 中所有边（SI→SO 所有路径上的边）
    NodePtr entry = nullptr;                 // SI node（入口）
    std::vector<NodePtr> entryPreds;         // SI 的外部前驱节点（可为空）
    NodePtr exit = nullptr;                  // SO node（出口）
    bool valid = false;
    bool prefixAllFactsRequired = false;     // 先保留这个标志，后续如果要用 support 做更细分判断
    SISORegionKind kind = SISORegionKind::Unknown;  // SISO 类别标记，便于快速路径处理
};

class GraphAnalyzer {
private:
    using NodeSet     = std::unordered_set<NodePtr>;
    using EdgeSet     = std::unordered_set<EdgePtr>;
    using SupportMap  = std::unordered_map<NodePtr, NodeSet>;
    using EdgeDomInputs = std::unordered_map<EdgePtr, std::vector<NodePtr>>;

    // ======= 调试输出 =======
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

    // ======= 支配结构 & prefix 结构 =======
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

    // Fast-path detectors (<=2 edges)
    static std::vector<SISORegionInfo> detectFastPathRegions(const DerivationGraphViewInterface& g) {
        std::vector<SISORegionInfo> regions;
        bool debug = std::getenv("SOUFFLE_SISO_FAST_DEBUG") != nullptr;
        auto isBlockedFact = [](NodePtr n) {
            return !n || !n->isFact || n->hasEvidence() || n->needOutput;
        };

        // 1) Single hyperedge: one edge exit, inputs.size()>=1, exactly one non-fact (SI), others are input facts
        for (auto e : g.getEdges()) {
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
            continue;
        }

        // 3) Linear two-edge: entry->mid->exit, each edge single input
        for (auto e1 : g.getEdges()) {
            if (!e1) continue;
            auto in1 = g.getInputs(e1);
            if (in1.size() != 1) continue;
            NodePtr entry = in1[0];
            NodePtr mid = g.getOutput(e1);
            if (!entry || !mid) continue;
            if (entry == mid) continue;
            if (mid->hasEvidence() || mid->needOutput) continue;  // mid cannot be query/evidence
            // mid should have exactly one outgoing edge for the chain
            auto midOut = g.getOutgoingEdges(mid);
            if (midOut.size() != 1) continue;
            EdgePtr e2 = midOut[0];
            if (!e2) continue;
            auto in2 = g.getInputs(e2);
            if (in2.size() != 1 || in2[0] != mid) continue;
            NodePtr exit = g.getOutput(e2);
            if (!exit || exit == entry || exit == mid) continue;
            regions.push_back(makeRegion(entry, exit, {e1, e2}, SISORegionKind::LinearTwoEdge));
        }

        // 4) Parallel two-edge: two edges SI->SO, single-input edges
        std::vector<EdgePtr> edgesVec(g.getEdges().begin(), g.getEdges().end());
        for (size_t i = 0; i < edgesVec.size(); ++i) {
            EdgePtr e1 = edgesVec[i];
            if (!e1) continue;
            auto in1 = g.getInputs(e1);
            if (in1.size() != 1) continue;
            NodePtr entry = in1[0];
            NodePtr exit = g.getOutput(e1);
            if (!entry || !exit) continue;
            for (size_t j = i + 1; j < edgesVec.size(); ++j) {
                EdgePtr e2 = edgesVec[j];
                if (!e2) continue;
                auto in2 = g.getInputs(e2);
                if (in2.size() != 1) continue;
                if (in2[0] != entry) continue;
                NodePtr exit2 = g.getOutput(e2);
                if (exit2 != exit) continue;
                if (entry == exit) continue;
                regions.push_back(makeRegion(entry, exit, {e1, e2}, SISORegionKind::ParallelTwoEdge));
            }
        }

        // 2) All-facts single hyperedge: single edge with all fact inputs and inputs have no incoming edges.
        for (auto e : g.getEdges()) {
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
                if (!n || !n->isFact || n->hasEvidence() || n->needOutput) {
                    allFacts = false;
                    if (debug) {
                        std::cout << "  skip: input not pure fact or evidence/output" << std::endl;
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
            NodePtr entry = inputs[0];
            auto region = makeRegion(entry, exit, {e}, SISORegionKind::AllFactsToSO);
            regions.push_back(std::move(region));
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

    // ========= support（反向收集所有能到达的 facts） =========
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

    // ========= 计算每条超边的 dominant input group =========
    static EdgeDomInputs computeEdgeDomInputs(
        const DerivationGraphViewInterface& g,
        const SupportMap& sup)
    {
        EdgeDomInputs dpi;

        // 调试日志
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

                // 按 support overlap 分组
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

                // 选 supUnion 最大的 group 作为 dominant input group
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

    // ========= 构造 dom-graph =========
    static DomGraph buildDomGraph(const DerivationGraphViewInterface& g) {
        DomGraph dg;

        // 为所有节点分配 index
        int idx = 0;
        for (NodePtr n : g.getNodes()) {
            if (!n) continue;
            dg.indexOf[n] = idx++;
            dg.nodes.push_back(n);
        }

        const int N = (int)dg.nodes.size();
        dg.preds.assign(N, {});

        // 根据超边把输入指向输出，构成有向图
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

        // 选一个无前驱节点作为支配树根；如果不存在，就选 0
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

    // ========= 标准 dominator 算法 =========
    static DomInfo computeDominators(const DomGraph& dg) {
        DomInfo info;
        const int N = (int)dg.nodes.size();
        info.domSets.assign(N, {});
        info.idom.assign(N, -1);

        if (N == 0 || dg.entryIndex < 0) return info;

        // 初始化：entry 的 dom 集 = {entry}，其余 = 全集
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

        // 计算 immediate dominator
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

    // ========= 支配树上的 LCA / 合流点 =========
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

    // ========= 从 candidate SO node 找 SI =========
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

        // 收集所有 incoming hyperedge 的 dominant inputs，在支配树上求合流点
        std::vector<int> srcIdx;
        for (EdgePtr eIn : g.getIncomingEdges(exitNode)) {
            if (!eIn) continue;

            const std::vector<NodePtr>* domSet = nullptr;
            std::vector<NodePtr> tmp;

            auto it = pre.domInputs.find(eIn);
            if (it != pre.domInputs.end()) {
                domSet = &it->second;       // 已经选过的 dominant input group
            } else {
                tmp = g.getInputs(eIn);     // 没有分组信息，就全部当 dominant
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
            // 没有前驱，不能形成「从 SI 到 SO」的 region
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

    // ========= 构建 strict / full region =========
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

    // ========= 收集 SI 的外部前驱（不再作为合法性条件） =========
    static bool selectEntryEdgeAndPreds(   // 现在永远返回 true，只作为收集信息
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
                entryEdge = eIn;   // 仅调试用途
            }
        }

        log("selectEntryEdgeAndPreds entry=" + entryNode->toString() +
            " outsidePreds=" + std::to_string(entryPreds.size()));
        return true;
    }

    // ========= escape 检查：只看「内部节点」的 outgoing edge 是否连到 region 外 =========
    static bool checkNoEscape(
        const DerivationGraphViewInterface& g,
        const Region& fullRegion,
        NodePtr entryNode,
        NodePtr exitNode)
    {
        for (NodePtr n : fullRegion.nodes) {
            if (!n) continue;

            // entry / exit 都是边界节点，不检查它们的 outgoing
            if (n == entryNode || n == exitNode) continue;

            for (EdgePtr e : g.getOutgoingEdges(n)) {
                if (!e) continue;
                NodePtr out = g.getOutput(e);

                bool outInside = (out && fullRegion.nodes.count(out));

                // 只要有 outgoing 指到了 region 之外，就是 escape
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

    // ========= 将 Region + 边界节点组装成 SISORegionInfo =========
    static SISORegionInfo assembleSISO(
        const Region& strictRegion,
        const Region& fullRegion,
        NodePtr entryNode,
        const std::vector<NodePtr>& entryPreds,
        NodePtr exitNode)
    {
        (void)strictRegion;  // 目前没有单独用 strictRegion，可留着以后细分
        SISORegionInfo info;

        // 至少要有 SI 和 SO 两个点
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

    // ========= 从 candidate SO node 识别 SISO（核心 pipeline） =========
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

    // ========= 兼容旧接口：从 exitEdge 出发 =========
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
    // ===================== 对外接口 =====================

    // 旧接口保留签名，当前实现为轻量 fast-path 检测（最多两条边），找不到则返回空。
    static inline SISORegionInfo detectSISOStrictFromExit(
        const DerivationGraphViewInterface& g,
        EdgePtr exitEdge)
    {
        (void)g;
        (void)exitEdge;
        return {};
    }

    static inline std::vector<SISORegionInfo> detectAllSISOStrictFromExit(
        const DerivationGraphViewInterface& g)
    {
        auto regions = detectFastPathRegions(g);
        // 去重：小 region 在前，避免重叠
        std::sort(regions.begin(), regions.end(),
                [](const SISORegionInfo& a, const SISORegionInfo& b) {
                    return a.internalNodes.size() < b.internalNodes.size();
                });
        std::vector<SISORegionInfo> result;
        NodeSet usedNodes;
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
        size_t singleHyperedgeCount = 0;
        size_t linearTwoEdgeCount = 0;
        size_t parallelTwoEdgeCount = 0;
        size_t allFactsToSOCount = 0;
        size_t generalCount = 0;
        size_t unknownCount = 0;
        for (const auto& r : result) {
            switch (r.kind) {
            case SISORegionKind::SingleHyperedge: ++singleHyperedgeCount; break;
            case SISORegionKind::LinearTwoEdge: ++linearTwoEdgeCount; break;
            case SISORegionKind::ParallelTwoEdge: ++parallelTwoEdgeCount; break;
            case SISORegionKind::AllFactsToSO: ++allFactsToSOCount; break;
            case SISORegionKind::General: ++generalCount; break;
            default: ++unknownCount; break;
            }
        }
        std::cout << "[siso-detect] fast-path regions " << result.size()
                  << " (candidates=" << regions.size()
                  << ", single-hyperedge=" << singleHyperedgeCount
                  << ", linear-two-edge=" << linearTwoEdgeCount
                  << ", parallel-two-edge=" << parallelTwoEdgeCount
                  << ", all-facts=" << allFactsToSOCount
                  << ", general=" << generalCount
                  << ", unknown=" << unknownCount
                  << ")" << std::endl;
        return result;
    }

    // 输出完整图，并用不同颜色高亮一个 SISO 区域
    static inline void dumpRegionAsDot(
        const DerivationGraphViewInterface& g,
        const SISORegionInfo& r,
        const std::string& filename)
    {
        std::ofstream out(filename);
        if (!out.is_open()) {
            std::cerr << "Cannot open dot file: " << filename << "\n";
            return;
        }

        NodeSet regionNodes(r.internalNodes.begin(), r.internalNodes.end());
        EdgeSet regionEdges(r.internalEdges.begin(), r.internalEdges.end());

        const auto& nodes = g.getNodes();
        const auto& edges = g.getEdges();

        out << "digraph DerivationGraphWithSISO {\n";
        out << "  rankdir=LR;\n";

        // 先输出节点：entry / exit / 其他 region / 非 region
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
                    fill = "khaki";      // region 内部其它节点
                }
            }

            out << "  node" << n->getId()
                << " [label=\""
                << n->getTuple().toString()
                << "\", fillcolor=" << fill << "];\n";
        }

        // 再输出 hyperedge（虚点），region 内的边用红色，其它用灰色
        out << "  node [shape=point, width=0.2];\n";
        for (const auto& e : edges) {
            if (!e) continue;
            if (e->pruned) continue;

            NodePtr outNode = g.getOutput(e);
            if (!outNode) continue;

            auto inputs = g.getInputs(e);
            if (std::find(inputs.begin(), inputs.end(), outNode) != inputs.end()) {
                // 跳过 head∈body 的伪环
                continue;
            }

            bool inRegion = regionEdges.count(e) > 0;

            // 画 edge 点本身
            out << "  edge" << e->getId()
                << " ["
                << "color=" << (inRegion ? "red" : "gray")
                << "];\n";

            // 输入到 edge
            for (NodePtr in : inputs) {
                if (!in) continue;
                if (!nodes.count(in)) continue;
                out << "  node" << in->getId()
                    << " -> edge" << e->getId()
                    << " [color=" << (inRegion ? "red" : "gray") << "];\n";
            }

            // edge 到输出
            if (nodes.count(outNode)) {
                out << "  edge" << e->getId()
                    << " -> node" << outNode->getId()
                    << " [color=" << (inRegion ? "red" : "gray") << "];\n";
            }
        }

        out << "}\n";
        out.close();
    }

    // 输出完整图，同时用不同颜色标注所有 SISO 区域（非 SISO 节点/边用浅灰）
    static inline void dumpAllRegionsAsDot(
        const DerivationGraphViewInterface& g,
        const std::vector<SISORegionInfo>& regions,
        const std::string& filename)
    {
        std::ofstream out(filename);
        if (!out.is_open()) {
            std::cerr << "Cannot open dot file: " << filename << "\n";
            return;
        }
        std::vector<std::string> palette = {
            "#1f77b4", "#ff7f0e", "#2ca02c", "#d62728",
            "#9467bd", "#8c564b", "#e377c2", "#7f7f7f",
            "#bcbd22", "#17becf"
        };

        // 记录节点/边的区域颜色（多区域时取第一个匹配色）以及 entry/exit 标记
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

        // 所有节点：在 region 的染色，否则浅灰
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

        // hyperedge 作为 point 节点
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

    // 简单 CSV 打印 SISO 信息（更新为 node-only 版本）
    static inline void printSISOInfo(
        const DerivationGraphViewInterface& g,
        const SISORegionInfo& r)
    {
        (void)g; // 目前没用到 g，本函数只是 dump region 信息

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
