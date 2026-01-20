#pragma once

#include <map>
#include <set>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include <type_traits>
#include <cmath>
#include <chrono>
#include <cassert>
#include <cstdlib>
#include <sstream>
#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/ConstAnalysis.h"
#include "souffle/problog/IncRegionAnalyzer.h"
#include "souffle/problog/formula/FormulaManager.h"

static inline const std::unordered_set<std::string>& incRegionalTraceTargets() {
    static std::unordered_set<std::string> targets;
    static bool loaded = false;
    if (loaded) {
        return targets;
    }
    loaded = true;
    const char* raw = std::getenv("SOUFFLE_INC_REGIONAL_TRACE_TUPLES");
    if (!raw || !*raw) {
        return targets;
    }
    std::stringstream ss(raw);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        if (!tok.empty()) {
            targets.insert(tok);
        }
    }
    return targets;
}

static inline bool incRegionalTraceEnabled() {
    return !incRegionalTraceTargets().empty();
}

static inline bool incRegionalTraceMatch(const NodePtr& node) {
    if (!node || !incRegionalTraceEnabled()) {
        return false;
    }
    const auto& targets = incRegionalTraceTargets();
    return targets.find(node->getTuple().toString()) != targets.end();
}

// forward declare the classic incremental builder to avoid header cycles
template<typename FormulaNodeRef>
void buildFormulasIncCyclewise(
    IncrementalDerivationGraphViewInterface& view,
    FormulaManager<FormulaNodeRef>& formulaManager,
    std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
    std::map<EdgePtr, FormulaNodeRef>& edgeFormulas,
    std::set<NodePtr>& changedNodes);

// Utility: ensure region is SCC-closed by expanding to include full SCCs of
// any node already inside the region.
struct RegionSccClosure {
    static bool closeToScc(
        incra::Region& R,
        const incra::Region& reachFilter,
        const CycleDependencyGraph& scc) {
        bool changed = false;
        std::unordered_set<size_t> touched;
        for (auto n : R.nodes) {
            auto it = scc.nodeToCycleIndex.find(n);
            if (it != scc.nodeToCycleIndex.end()) {
                touched.insert(it->second);
            }
        }
        for (auto cid : touched) {
            if (cid >= scc.nodeCycles.size()) continue;
            for (auto n : scc.nodeCycles[cid]) {
                if (!reachFilter.nodes.empty() && !reachFilter.nodes.count(n)) continue;
                if (R.nodes.insert(n).second) changed = true;
            }
            if (cid < scc.edgeCycles.size()) {
                for (auto e : scc.edgeCycles[cid]) {
                    if (!reachFilter.edges.empty() && !reachFilter.edges.count(e)) continue;
                    if (R.edges.insert(e).second) changed = true;
                }
            }
        }
        return changed;
    }
};

// Analysis cache to hand downstream without recomputing.
struct AnalyzerResultCache {
    incra::Region region;
    incra::Boundaries boundaries;
    std::unordered_map<NodePtr, std::vector<incra::IncRegionAnalysis::AnchorCandidate>> mergeableAnchorsByHead;
};

struct RegionalInsertPlan {
    std::set<NodePtr> regionNodes;
    std::set<NodePtr> boundaryNodes;
    std::unordered_map<NodePtr, std::vector<incra::IncRegionAnalysis::AnchorCandidate>> anchorCandidates;
    bool mergeReady = false;
    bool regionClosed = false;
};

struct RegionalInsertPlanBuilder {
    static RegionalInsertPlan build(const AnalyzerResultCache& analysis) {
        RegionalInsertPlan plan;
        plan.regionNodes.insert(analysis.region.nodes.begin(), analysis.region.nodes.end());
        std::set<NodePtr> b;
        b.insert(analysis.boundaries.out_induced.begin(), analysis.boundaries.out_induced.end());
        b.insert(analysis.boundaries.scope_induced.begin(), analysis.boundaries.scope_induced.end());
        b.insert(analysis.boundaries.residual.begin(), analysis.boundaries.residual.end());
        plan.boundaryNodes.swap(b);
        plan.regionClosed = plan.boundaryNodes.empty();
        plan.anchorCandidates = analysis.mergeableAnchorsByHead;
        plan.mergeReady = plan.regionClosed;
        if (!plan.regionClosed) {
            bool ok = true;
            for (auto& v : plan.boundaryNodes) {
                auto it = plan.anchorCandidates.find(v);
                if (it == plan.anchorCandidates.end() || it->second.empty()) {
                    ok = false;
                    break;
                }
            }
            plan.mergeReady = ok;
        }
        return plan;
    }
};

struct RegionPrioritizedEdge {
    EdgePtr edge;
    size_t priority;
    int sequence_id;
    bool operator<(const RegionPrioritizedEdge& other) const {
        if (priority != other.priority) return priority > other.priority;
        return sequence_id > other.sequence_id;
    }
};

template <typename FormulaManagerT, typename FormulaNodeRef>
class RegionalDDRebuilder {
public:
    struct Snapshot {
        std::map<NodePtr, FormulaNodeRef> boundarySnapshots;
    };
    struct Timing {
        double snapshotMs = 0.0;
        double initNodesMs = 0.0;
        double initEdgesMs = 0.0;
        double depGraphMs = 0.0;
        double regionCyclesMs = 0.0;
        double indegreeMs = 0.0;
        double rebuildLoopMs = 0.0;
        double totalMs = 0.0;
        double reorderMs = 0.0;
        size_t regionCycleCount = 0;
        size_t edgesProcessed = 0;
        size_t edgesRebuilt = 0;
        size_t nodesUpdated = 0;
    };
    struct Result {
        Snapshot snapshot;
        std::set<NodePtr> changedNodes;
        std::unordered_set<EdgePtr> rebuiltEdges;
        Timing timing;
    };

    static Result rebuildInsertRegion(
        IncrementalDerivationGraphViewInterface& view,
        FormulaManagerT& formulaManager,
        std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
        std::map<EdgePtr, FormulaNodeRef>& edgeFormulas,
        const RegionalInsertPlan& plan,
        const ConstAnalysisResult* constInfo = nullptr) {
        Result res;
        auto nowMs = []{ return std::chrono::steady_clock::now(); };
        auto toMs = [](auto dur){
            return std::chrono::duration<double, std::milli>(dur).count();
        };
        auto t0 = nowMs();
        double reorderStartSec = getReorderSeconds(formulaManager, 0);
        ConstFormulaAccess<FormulaNodeRef> constAccess{constInfo, formulaManager};

        // Snapshot old boundary formulas
        auto tSnapshotStart = nowMs();
        for (auto& v : plan.boundaryNodes) {
            auto it = nodeFormulas.find(v);
            if (it != nodeFormulas.end()) {
                res.snapshot.boundarySnapshots[v] = it->second;
            } else {
                res.snapshot.boundarySnapshots[v] = formulaManager.getFalse();
            }
        }
        auto tSnapshotEnd = nowMs();

        const auto& deltaInsertedNodes = view.getDeltaInsertNodes();
        const auto& deltaInsertedEdges = view.getDeltaInsertEdges();

        // Initialize inserted nodes
        auto tInitNodesStart = nowMs();
        for (auto node : deltaInsertedNodes) {
            if (node->isFact) {
                int idx = formulaManager.getVarIndex(*node);
                if (node->getProbability() == 1.0) {
                    nodeFormulas[node] = formulaManager.getTrue();
                } else {
                    nodeFormulas[node] = formulaManager.createVar(idx, *node);
                    formulaManager.setVariableWeight(idx, node->getProbability(), 1 - node->getProbability());
                }
            } else {
                nodeFormulas[node] = formulaManager.getFalse();
            }
            res.changedNodes.insert(node);
        }
        auto tInitNodesEnd = nowMs();
        // Initialize inserted edges
        auto tInitEdgesStart = nowMs();
        for (auto edge : deltaInsertedEdges) {
            edgeFormulas[edge] = formulaManager.getFalse();
            if (!edge->isDeterministic()) {
                int idx = formulaManager.getVarIndex(*edge);
                formulaManager.createVar(idx, *edge);
                formulaManager.setVariableWeight(idx, edge->getProbability(), 1 - edge->getProbability());
            }
        }
        auto tInitEdgesEnd = nowMs();

        auto tDepGraphStart = nowMs();
        auto& depGraph = view.getCycleDependencyGraph();
        auto tDepGraphEnd = nowMs();
        std::unordered_set<size_t> regionCycles;
        auto tRegionCyclesStart = nowMs();
        for (auto& n : plan.regionNodes) {
            auto it = depGraph.nodeToCycleIndex.find(n);
            if (it != depGraph.nodeToCycleIndex.end()) {
                regionCycles.insert(it->second);
            }
        }
        auto tRegionCyclesEnd = nowMs();

        // Build filtered indegree
        auto tIndegreeStart = nowMs();
        std::unordered_map<size_t, size_t> indeg;
        for (auto cid : regionCycles) indeg[cid] = 0;
        for (auto cid : regionCycles) {
            for (auto succ : depGraph.reverseDependencies[cid]) {
                if (regionCycles.count(succ)) {
                    indeg[succ]++;
                }
            }
        }

        std::queue<size_t> ready;
        for (auto& kv : indeg) if (kv.second == 0) ready.push(kv.first);
        auto tIndegreeEnd = nowMs();

        auto shouldRebuildEdge = [&](const EdgePtr& e) {
            if (!e) return false;
            auto head = view.getOutput(e);
            if (!head || !plan.regionNodes.count(head)) return false;
            if (deltaInsertedEdges.count(e)) return true;
            for (auto& in : view.getInputs(e)) {
                if (plan.regionNodes.count(in)) return true;
            }
            if (incRegionalTraceMatch(head)) {
                std::cout << "[inc-regional-rebuild] skip edge head=" << incra::node_id(head)
                          << " reason=no_inputs_in_region\n";
            }
            return false;
        };

        auto tRebuildStart = nowMs();
        size_t nodesUpdated = 0;
        while (!ready.empty()) {
            auto cid = ready.front(); ready.pop();
            std::priority_queue<RegionPrioritizedEdge> worklist;
            std::set<EdgePtr> inWorklist;
            int seq = 0;
            for (auto e : depGraph.edgeCycles[cid]) {
                if (shouldRebuildEdge(e)) {
                    if (incRegionalTraceMatch(view.getOutput(e))) {
                        std::cout << "[inc-regional-rebuild] queue edge head="
                                  << incra::node_id(view.getOutput(e)) << "\n";
                    }
                    worklist.push({e, depGraph.edgeDepthsGlobal.at(e), seq++});
                    inWorklist.insert(e);
                }
            }
            while (!worklist.empty()) {
                EdgePtr edge = worklist.top().edge;
                worklist.pop();
                res.timing.edgesProcessed += 1;
                inWorklist.erase(edge);
                const auto& ins = view.getInputs(edge);
                const auto& negs = view.getBodyNegations(edge);
                if (incRegionalTraceMatch(view.getOutput(edge))) {
                    std::cout << "[inc-regional-rebuild] edge head=" << incra::node_id(view.getOutput(edge))
                              << " edge_det=" << (edge->isDeterministic() ? 1 : 0)
                              << " inputs=[";
                    for (size_t ii = 0; ii < ins.size(); ++ii) {
                        if (ii) std::cout << ", ";
                        std::cout << incra::node_id(ins[ii])
                                  << " fact=" << (ins[ii] && ins[ii]->isFact ? 1 : 0)
                                  << " p=" << (ins[ii] ? ins[ii]->getProbability() : 0.0)
                                  << " has_formula=" << (ins[ii] && nodeFormulas.count(ins[ii]) ? 1 : 0)
                                  << " neg=" << (negs[ii] ? 1 : 0);
                    }
                    std::cout << "]\n";
                }
                FormulaNodeRef newEdge;
                bool edgeIsConst = constAccess.edgeFormula(edge, newEdge);
                bool allAvail = true;
                bool missingInsideRegion = false;
                if (!edgeIsConst) {
                    FormulaNodeRef base = edge->isDeterministic()
                        ? formulaManager.getTrue()
                        : formulaManager.createVar(formulaManager.getVarIndex(*edge), *edge);
                    std::vector<FormulaNodeRef> inputs{base};
                    for (size_t i = 0; i < ins.size(); ++i) {
                        FormulaNodeRef lit;
                        if (!constAccess.inputLiteral(nodeFormulas, ins[i], negs[i], lit)) {
                            allAvail = false;
                            if (plan.regionNodes.count(ins[i])) {
                                missingInsideRegion = true;
                            }
                            if (incRegionalTraceMatch(view.getOutput(edge))) {
                                std::cout << "[inc-regional-rebuild] missing input for head="
                                          << incra::node_id(view.getOutput(edge))
                                          << " input=" << incra::node_id(ins[i])
                                          << " in_region=" << (plan.regionNodes.count(ins[i]) ? 1 : 0)
                                          << " has_formula=" << (nodeFormulas.count(ins[i]) ? 1 : 0)
                                          << " neg=" << (negs[i] ? 1 : 0)
                                          << "\n";
                            }
                            break;
                        }
                        inputs.push_back(lit);
                    }
                    if (!allAvail) {
                        // If missing inputs are outside the region, do not requeue forever.
                        if (missingInsideRegion) {
                            if (!inWorklist.count(edge)) {
                                worklist.push({edge, depGraph.edgeDepthsGlobal.at(edge), seq++});
                                inWorklist.insert(edge);
                            }
                        }
                        continue;
                    }
                    newEdge = (inputs.size()==1) ? inputs[0] : formulaManager.makeAnd(inputs);
                }
                if (!edgeFormulas.count(edge) || !formulaManager.isSame(edgeFormulas[edge], newEdge)) {
                    edgeFormulas[edge] = newEdge;
                    res.rebuiltEdges.insert(edge);
                    NodePtr out = view.getOutput(edge);
                    if (!out || out->isFact) continue;
                    FormulaNodeRef newNode;
                    bool hasNewNode = constAccess.nodeFormula(out, newNode);
                    if (!hasNewNode) {
                        std::vector<FormulaNodeRef> incoming;
                        for (auto eIn : view.getIncomingEdges(out)) {
                            auto it = edgeFormulas.find(eIn);
                            if (it != edgeFormulas.end() && it->second.get()) incoming.push_back(it->second);
                        }
                        if (incRegionalTraceMatch(out)) {
                            std::cout << "[inc-regional-rebuild] head=" << incra::node_id(out)
                                      << " incoming_total=" << view.getIncomingEdges(out).size()
                                      << " incoming_with_formula=" << incoming.size()
                                      << "\n";
                        }
                        if (incoming.empty()) continue;
                        newNode = (incoming.size()==1) ? incoming[0] : formulaManager.makeOr(incoming);
                        hasNewNode = true;
                    }
                    if (hasNewNode && (!nodeFormulas.count(out) || !formulaManager.isSame(nodeFormulas[out], newNode))) {
                        nodeFormulas[out] = newNode;
                        res.changedNodes.insert(out);
                        nodesUpdated += 1;
                        for (auto outEdge : view.getOutgoingEdges(out)) {
                            auto eit = depGraph.edgeToCycleIndex.find(outEdge);
                            if (eit != depGraph.edgeToCycleIndex.end() && eit->second == cid && shouldRebuildEdge(outEdge)) {
                                if (!inWorklist.count(outEdge)) {
                                    worklist.push({outEdge, depGraph.edgeDepthsGlobal.at(outEdge), seq++});
                                    inWorklist.insert(outEdge);
                                }
                            }
                        }
                    }
                }
            }
            for (auto succ : depGraph.reverseDependencies[cid]) {
                if (regionCycles.count(succ)) {
                    if (--indeg[succ]==0) ready.push(succ);
                }
            }
        }
        auto tRebuildEnd = nowMs();

        double reorderEndSec = getReorderSeconds(formulaManager, 0);
        auto tEnd = nowMs();
        res.timing.snapshotMs = toMs(tSnapshotEnd - tSnapshotStart);
        res.timing.initNodesMs = toMs(tInitNodesEnd - tInitNodesStart);
        res.timing.initEdgesMs = toMs(tInitEdgesEnd - tInitEdgesStart);
        res.timing.depGraphMs = toMs(tDepGraphEnd - tDepGraphStart);
        res.timing.regionCyclesMs = toMs(tRegionCyclesEnd - tRegionCyclesStart);
        res.timing.indegreeMs = toMs(tIndegreeEnd - tIndegreeStart);
        res.timing.rebuildLoopMs = toMs(tRebuildEnd - tRebuildStart);
        res.timing.totalMs = toMs(tEnd - t0);
        res.timing.reorderMs = (reorderEndSec > reorderStartSec)
            ? (reorderEndSec - reorderStartSec) * 1000.0
            : 0.0;
        res.timing.regionCycleCount = regionCycles.size();
        res.timing.nodesUpdated = nodesUpdated;
        res.timing.edgesRebuilt = res.rebuiltEdges.size();

        return res;
    }

private:
    template <typename T>
    static auto getReorderSeconds(T& fm, int) -> decltype(fm.getReorderingTimeSeconds(), double()) {
        return fm.getReorderingTimeSeconds();
    }
    template <typename T>
    static double getReorderSeconds(T&, ...) {
        return 0.0;
    }
};

template <typename FormulaManagerT, typename FormulaNodeRef>
class BoundaryGateCalibrator {
public:
    struct CalibrationRecord {
        NodePtr v;
        incra::IncRegionAnalysis::AnchorCandidate anchor;
        int varIdx = -1;
        double alpha = 0.0;
        double beta  = 0.0;
        double pStar = 0.0;
        double target = 0.0;      // Pr_new[v]
        double oldValue = 0.0;    // WMC(BDD_old(v)) under old weights
    };
    struct Result {
        std::vector<CalibrationRecord> applied;
        std::unordered_set<NodePtr> failedNodes;
        std::unordered_map<int, std::pair<double,double>> weightOverrides;
    };

    static Result calibrate(
        IncrementalDerivationGraphViewInterface& view,
        FormulaManagerT& formulaManager,
        const RegionalInsertPlan& plan,
        const typename RegionalDDRebuilder<FormulaManagerT, FormulaNodeRef>::Snapshot& snapshot,
        const std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
        double eps = 1e-12) {
        Result res;
        for (auto& v : plan.boundaryNodes) {
            auto itNF = nodeFormulas.find(v);
            if (itNF == nodeFormulas.end()) {
                if (incRegionalProfileEnabled) {
                    std::cout << "[inc-regional] calibrate fail head=" << incra::node_id(v)
                              << " reason=missing_node_formula\n";
                }
                res.failedNodes.insert(v);
                continue;
            }
            double target = formulaManager.computeWeightedModelCount(itNF->second);
            auto anchorIt = plan.anchorCandidates.find(v);
            if (anchorIt == plan.anchorCandidates.end() || anchorIt->second.empty()) {
                if (incRegionalProfileEnabled) {
                    std::cout << "[inc-regional] calibrate fail head=" << incra::node_id(v)
                              << " reason=no_anchor_candidates\n";
                }
                res.failedNodes.insert(v);
                continue;
            }
            bool calibrated = false;
            size_t tried = 0;
            size_t missingSnapshot = 0;
            size_t degenerate = 0;
            for (auto& anchor : anchorIt->second) {
                int varIdx = -1;
                if (anchor.kind == incra::IncRegionAnalysis::AnchorKind::Node) {
                    if (!anchor.node) {
                        if (incRegionalProfileEnabled) {
                            std::cout << "[inc-regional] calibrate skip head=" << incra::node_id(v)
                                      << " anchor=node:<null> reason=node_null\n";
                        }
                        continue;
                    }
                    if (!anchor.node->isFact) {
                        if (incRegionalProfileEnabled) {
                            std::cout << "[inc-regional] calibrate skip head=" << incra::node_id(v)
                                      << " anchor=node:" << incra::node_id(anchor.node)
                                      << " reason=node_not_fact\n";
                        }
                        continue;
                    }
                    if (anchor.node->getProbability() >= 1.0) {
                        if (incRegionalProfileEnabled) {
                            std::cout << "[inc-regional] calibrate skip head=" << incra::node_id(v)
                                      << " anchor=node:" << incra::node_id(anchor.node)
                                      << " reason=node_prob_one\n";
                        }
                        continue;
                    }
                    varIdx = formulaManager.getVarIndex(*anchor.node);
                } else {
                    if (!anchor.edge) {
                        if (incRegionalProfileEnabled) {
                            std::cout << "[inc-regional] calibrate skip head=" << incra::node_id(v)
                                      << " anchor=edge:<null> reason=edge_null\n";
                        }
                        continue;
                    }
                    varIdx = formulaManager.getVarIndex(*anchor.edge);
                }
                tried++;
                auto oldW = formulaManager.getVariableWeight(varIdx);
                auto snapIt = snapshot.boundarySnapshots.find(v);
                if (snapIt == snapshot.boundarySnapshots.end()) {
                    missingSnapshot++;
                    if (incRegionalProfileEnabled) {
                        std::cout << "[inc-regional] calibrate skip head=" << incra::node_id(v)
                                  << " anchor=" << incra::anchor_id(anchor, view)
                                  << " reason=missing_snapshot\n";
                    }
                    continue;
                }
                double oldVal = formulaManager.computeWeightedModelCount(snapIt->second);
                formulaManager.setVariableWeight(varIdx, 0.0, 1.0);
                double alpha = formulaManager.computeWeightedModelCount(snapIt->second);
                formulaManager.setVariableWeight(varIdx, 1.0, 0.0);
                double beta  = formulaManager.computeWeightedModelCount(snapIt->second);
                formulaManager.setVariableWeight(varIdx, oldW.posWeight, oldW.negWeight);
                if (std::fabs(beta - alpha) < eps) {
                    degenerate++;
                    continue;
                }
                double pStar = (target - alpha) / (beta - alpha);
                if (!std::isfinite(pStar) || pStar < -eps || pStar > 1.0 + eps) {
                    if (incRegionalProfileEnabled) {
                        std::cout << "[inc-regional] calibrate skip head=" << incra::node_id(v)
                                  << " anchor=" << incra::anchor_id(anchor, view)
                                  << " reason=pstar_out_of_range"
                                  << " target=" << target
                                  << " alpha=" << alpha
                                  << " beta=" << beta
                                  << " pStar=" << pStar
                                  << "\n";
                    }
                    continue;
                }
                if (pStar < 0.0) pStar = 0.0;
                if (pStar > 1.0) pStar = 1.0;
                CalibrationRecord rec;
                rec.v = v;
                rec.anchor = anchor;
                rec.varIdx = varIdx;
                rec.alpha = alpha;
                rec.beta = beta;
                rec.pStar = pStar;
                rec.target = target;
                rec.oldValue = oldVal;
                res.applied.push_back(rec);
                res.weightOverrides[varIdx] = {pStar, 1 - pStar};
                calibrated = true;
                break;
            }
            if (!calibrated) {
                if (incRegionalProfileEnabled) {
                    std::string reason = "no_valid_anchor";
                    if (tried == 0) {
                        reason = "no_anchor_candidates";
                    } else if (missingSnapshot == tried) {
                        reason = "missing_snapshot";
                    } else if (degenerate == tried) {
                        reason = "degenerate_anchor";
                    }
                    std::cout << "[inc-regional] calibrate fail head=" << incra::node_id(v)
                              << " reason=" << reason
                              << " tried=" << tried
                              << " missing_snapshot=" << missingSnapshot
                              << " degenerate=" << degenerate
                              << "\n";
                }
                res.failedNodes.insert(v);
            }
        }
        return res;
    }
};

template <typename FormulaManagerT, typename FormulaNodeRef>
class RegionalIncrementalForwardCompilation {
public:
    struct Options {
        bool enableFallbackToClassicInsertion = true;
        double eps = 1e-12;
    };
    struct Stats {
        size_t regionNodeCount = 0;
        size_t boundaryNodeCount = 0;
        size_t calibratedCount = 0;
        bool usedFallback = false;
        bool sccExpanded = false;
    };
    struct Timing {
        double analyzeMs = 0.0;
        double sccCloseMs = 0.0;
        double planMs = 0.0;
        double rebuildMs = 0.0;
        double calibrateMs = 0.0;
        double totalMs = 0.0;
        std::string fallbackReason;
        typename RegionalDDRebuilder<FormulaManagerT, FormulaNodeRef>::Timing rebuildDetail;
    };

    explicit RegionalIncrementalForwardCompilation(Options opt = {}) : opt_(opt) {}

    using CalibrationRecord = typename BoundaryGateCalibrator<FormulaManagerT, FormulaNodeRef>::CalibrationRecord;

    void applyUpdate(
        IncrementalDerivationGraphViewInterface& view,
        FormulaManagerT& formulaManager,
        std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
        std::map<EdgePtr, FormulaNodeRef>& edgeFormulas,
        std::set<NodePtr>& changedNodes,
        const ConstAnalysisResult* constInfo = nullptr) {
        Debugger& debugger = Debugger::getInstance();
        const auto& deltaInsertedEdges = view.getDeltaInsertEdges();
        const auto& deltaInsertedNodes = view.getDeltaInsertNodes();
        if (deltaInsertedEdges.empty() && deltaInsertedNodes.empty()) {
            formulaManager.dumpProfilingStatistics();
            for (auto& [key, value]: formulaManager.getProfilingStatistics()) {
                debugger.addInfo(key, value);
            }
            return;
        }

        auto nowMs = []{ return std::chrono::steady_clock::now(); };
        auto toMs = [](auto dur){
            return std::chrono::duration<double, std::milli>(dur).count();
        };
        auto t0 = nowMs();

        // === 1) Analyze region ===
        incra::RegionAnalyzer analyzer(view);
        std::unordered_set<NodePtr> delta_input_set;
        delta_input_set.reserve(deltaInsertedNodes.size() + deltaInsertedEdges.size());
        for (auto n : deltaInsertedNodes) {
            if (n) {
                delta_input_set.insert(n);
            }
        }
        for (auto e : deltaInsertedEdges) {
            NodePtr out = view.getOutput(e);
            if (out) {
                delta_input_set.insert(out);
            }
        }
        std::vector<NodePtr> delta_inputs;
        delta_inputs.reserve(delta_input_set.size());
        for (auto n : delta_input_set) {
            delta_inputs.push_back(n);
        }
        // Regional pipeline expects an existing baseline; enforce it.
        assert(!(nodeFormulas.empty() && edgeFormulas.empty()) &&
               "Regional incremental requires pre-existing formulas; build baseline first.");
        analyzer.analyze(delta_inputs);
        auto analysis = analyzer.getLastAnalysis();
        auto t1 = nowMs();

        // === 2) Ensure SCC-closed ===
        auto& depGraph = view.getCycleDependencyGraph();
        bool expanded = RegionSccClosure::closeToScc(analysis.region, analyzer.lastDeltaReachable(), depGraph);
        stats_.sccExpanded = expanded;
        if (expanded) {
            analysis.boundaries = analyzer.recomputeBoundaries(analysis.region);
            analysis.mergeableAnchorsByHead = analyzer.recomputeAnchors(analysis.region, analysis.boundaries);
        }
        auto t2 = nowMs();

        auto boundariesEmpty = [&](const incra::Boundaries& b) {
            return b.out_induced.empty() && b.scope_induced.empty() && b.residual.empty();
        };
        if (boundariesEmpty(analysis.boundaries)) {
            const auto& dr = analyzer.lastDeltaReachable();
            bool missing = false;
            for (const auto& n : dr.nodes) {
                if (!analysis.region.nodes.count(n)) {
                    missing = true;
                    break;
                }
            }
            if (missing) {
                if (incRegionalProfileEnabled) {
                    std::cout << "[inc-regional] boundary empty but region misses delta-reachable nodes; "
                              << "expanding region to delta-reachable\n";
                }
                analysis.region.nodes = dr.nodes;
                analysis.region.edges = dr.edges;
                analysis.boundaries = analyzer.recomputeBoundaries(analysis.region);
                analysis.mergeableAnchorsByHead = analyzer.recomputeAnchors(analysis.region, analysis.boundaries);
            }
        }

        auto buildPlan = [&]() {
            AnalyzerResultCache cache;
            cache.region = analysis.region;
            cache.boundaries = analysis.boundaries;
            cache.mergeableAnchorsByHead = analysis.mergeableAnchorsByHead;
            RegionalInsertPlan plan = RegionalInsertPlanBuilder::build(cache);

            // Filter out degenerate anchors (alpha ~= beta) using boundary snapshots.
            if (!plan.boundaryNodes.empty()) {
                std::unordered_map<NodePtr, FormulaNodeRef> boundarySnapshots;
                boundarySnapshots.reserve(plan.boundaryNodes.size());
                for (const auto& v : plan.boundaryNodes) {
                    auto it = nodeFormulas.find(v);
                    if (it != nodeFormulas.end()) {
                        boundarySnapshots[v] = it->second;
                    } else {
                        boundarySnapshots[v] = formulaManager.getFalse();
                    }
                }
                size_t degenerateRemoved = 0;
                for (const auto& v : plan.boundaryNodes) {
                    auto it = plan.anchorCandidates.find(v);
                    if (it == plan.anchorCandidates.end() || it->second.empty()) {
                        continue;
                    }
                    std::vector<incra::IncRegionAnalysis::AnchorCandidate> filtered;
                    filtered.reserve(it->second.size());
                    for (const auto& anchor : it->second) {
                        int varIdx = -1;
                        if (anchor.kind == incra::IncRegionAnalysis::AnchorKind::Node) {
                            if (!anchor.node || !anchor.node->isFact || anchor.node->getProbability() >= 1.0) {
                                continue;
                            }
                            varIdx = formulaManager.getVarIndex(*anchor.node);
                        } else {
                            if (!anchor.edge) continue;
                            varIdx = formulaManager.getVarIndex(*anchor.edge);
                        }
                        auto oldW = formulaManager.getVariableWeight(varIdx);
                        auto snapIt = boundarySnapshots.find(v);
                        if (snapIt == boundarySnapshots.end()) {
                            continue;
                        }
                        double alpha = 0.0;
                        double beta = 0.0;
                        formulaManager.setVariableWeight(varIdx, 0.0, 1.0);
                        alpha = formulaManager.computeWeightedModelCount(snapIt->second);
                        formulaManager.setVariableWeight(varIdx, 1.0, 0.0);
                        beta = formulaManager.computeWeightedModelCount(snapIt->second);
                        formulaManager.setVariableWeight(varIdx, oldW.posWeight, oldW.negWeight);
                        if (std::fabs(beta - alpha) < opt_.eps) {
                            degenerateRemoved++;
                            if (incRegionalProfileEnabled) {
                                std::cout << "[inc-regional] filtered degenerate anchor head="
                                          << incra::node_id(v)
                                          << " anchor=" << incra::anchor_id(anchor, view) << "\n";
                            }
                            continue;
                        }
                        filtered.push_back(anchor);
                    }
                    it->second.swap(filtered);
                    if (incRegionalProfileEnabled && it->second.empty()) {
                        std::cout << "[inc-regional] boundary head has no non-degenerate anchors: "
                                  << incra::node_id(v) << "\n";
                    }
                }
                if (degenerateRemoved > 0 && incRegionalProfileEnabled) {
                    std::cout << "[inc-regional] degenerate anchors filtered=" << degenerateRemoved << "\n";
                }
                // Re-evaluate mergeReady with filtered anchors.
                if (plan.boundaryNodes.empty()) {
                    plan.mergeReady = true;
                } else {
                    bool ok = true;
                    for (const auto& v : plan.boundaryNodes) {
                        auto it = plan.anchorCandidates.find(v);
                        if (it == plan.anchorCandidates.end() || it->second.empty()) {
                            ok = false;
                            break;
                        }
                    }
                    plan.mergeReady = ok;
                }
            }
            return plan;
        };

        // === 3) Build plan (with degenerate filtering) ===
        RegionalInsertPlan plan = buildPlan();

        lastTiming_ = Timing{};
        lastTiming_.analyzeMs = toMs(t1 - t0);
        lastTiming_.sccCloseMs = toMs(t2 - t1);

        // If anchors are missing, expand the region from failed boundary nodes
        // instead of falling back immediately.
        int expandAttempts = 0;
        while (!plan.mergeReady && opt_.enableFallbackToClassicInsertion && expandAttempts < 3) {
            std::unordered_set<NodePtr> failed;
            for (const auto& v : plan.boundaryNodes) {
                auto it = plan.anchorCandidates.find(v);
                if (it == plan.anchorCandidates.end() || it->second.empty()) {
                    failed.insert(v);
                }
            }
            if (failed.empty()) break;
            bool expanded = analyzer.expandRegionFromSources(analysis.region, failed, analyzer.lastDeltaReachable());
            if (!expanded) break;
            analysis.boundaries = analyzer.recomputeBoundaries(analysis.region);
            analysis.mergeableAnchorsByHead = analyzer.recomputeAnchors(analysis.region, analysis.boundaries);
            bool sccExpanded = RegionSccClosure::closeToScc(analysis.region, analyzer.lastDeltaReachable(), depGraph);
            if (sccExpanded) {
                analysis.boundaries = analyzer.recomputeBoundaries(analysis.region);
                analysis.mergeableAnchorsByHead = analyzer.recomputeAnchors(analysis.region, analysis.boundaries);
            }
            if (incRegionalProfileEnabled) {
                std::cout << "[inc-regional] expanded region after missing anchors: "
                          << "attempt=" << (expandAttempts + 1)
                          << " added_nodes=" << analysis.region.nodes.size()
                          << " added_edges=" << analysis.region.edges.size()
                          << " failed=" << failed.size() << "\n";
            }
            plan = buildPlan();
            expandAttempts++;
        }

        // Safety: ensure region covers delta-reachable nodes to avoid stale outputs.
        const auto& dr = analyzer.lastDeltaReachable();
        bool missingDeltaReach = false;
        for (const auto& n : dr.nodes) {
            if (!plan.regionNodes.count(n)) {
                missingDeltaReach = true;
                break;
            }
        }
        if (missingDeltaReach) {
            if (incRegionalProfileEnabled) {
                std::cout << "[inc-regional] plan region misses delta-reachable nodes; "
                          << "expanding region to delta-reachable\n";
            }
            analysis.region.nodes = dr.nodes;
            analysis.region.edges = dr.edges;
            analysis.boundaries = analyzer.recomputeBoundaries(analysis.region);
            analysis.mergeableAnchorsByHead = analyzer.recomputeAnchors(analysis.region, analysis.boundaries);
            plan = buildPlan();
        }

        if (incRegionalProfileEnabled && std::getenv("SOUFFLE_INC_REGIONAL_TRACE_TUPLES")) {
            std::unordered_map<std::string, NodePtr> tuple_index;
            tuple_index.reserve(view.getValidNodes().size());
            for (const auto& n : view.getValidNodes()) {
                if (!n) continue;
                tuple_index.emplace(n->getTuple().toString(), n);
            }
            std::stringstream ss(std::getenv("SOUFFLE_INC_REGIONAL_TRACE_TUPLES"));
            std::string tok;
            while (std::getline(ss, tok, ',')) {
                if (tok.empty()) continue;
                auto it = tuple_index.find(tok);
                if (it == tuple_index.end()) {
                    std::cout << "[inc-regional-plan-trace] tuple=" << tok << " found=0\n";
                    continue;
                }
                NodePtr node = it->second;
                std::cout << "[inc-regional-plan-trace] tuple=" << tok
                          << " in_region=" << (plan.regionNodes.count(node) ? 1 : 0)
                          << " in_boundary=" << (plan.boundaryNodes.count(node) ? 1 : 0)
                          << "\n";
            }
        }

        auto t3 = nowMs();
        lastTiming_.planMs = toMs(t3 - t2);

        stats_.regionNodeCount = plan.regionNodes.size();
        stats_.boundaryNodeCount = plan.boundaryNodes.size();

        auto fallbackClassic = [&]() {
            buildFormulasIncCyclewise(view, formulaManager, nodeFormulas, edgeFormulas, changedNodes);
            stats_.usedFallback = true;
        };

        if (!plan.mergeReady && opt_.enableFallbackToClassicInsertion) {
            if (incRegionalProfileEnabled) {
                std::cout << "[inc-regional] mergeReady=false, fallback to classic\n";
            }
            lastTiming_.fallbackReason = "mergeReady=false";
            lastTiming_.totalMs = toMs(nowMs() - t0);
            fallbackClassic();
            return;
        }

        // === 4) Regional rebuild ===
        auto rebuildRes = RegionalDDRebuilder<FormulaManagerT, FormulaNodeRef>::rebuildInsertRegion(
            view, formulaManager, nodeFormulas, edgeFormulas, plan, constInfo);
        changedNodes.insert(rebuildRes.changedNodes.begin(), rebuildRes.changedNodes.end());
        const auto& rt = rebuildRes.timing;
        if (incRegionalProfileEnabled) {
            std::cout << "[inc-regional rebuild] timing(ms):"
                      << " snapshot=" << rt.snapshotMs
                      << " initNodes=" << rt.initNodesMs
                      << " initEdges=" << rt.initEdgesMs
                      << " depGraph=" << rt.depGraphMs
                      << " regionCycles=" << rt.regionCyclesMs
                      << " indegree=" << rt.indegreeMs
                      << " rebuildLoop=" << rt.rebuildLoopMs
                      << " total=" << rt.totalMs
                      << " reorder=" << rt.reorderMs
                      << " edgesProcessed=" << rt.edgesProcessed
                      << " edgesRebuilt=" << rt.edgesRebuilt
                      << " nodesUpdated=" << rt.nodesUpdated
                      << " cycleCount=" << rt.regionCycleCount
                      << "\n";
        }
        auto t4 = nowMs();
        lastTiming_.rebuildMs = toMs(t4 - t3);
        lastTiming_.rebuildDetail = rebuildRes.timing;

        if (incRegionalTraceEnabled()) {
            for (const auto& node : plan.regionNodes) {
                if (!incRegionalTraceMatch(node)) {
                    continue;
                }
                auto it = nodeFormulas.find(node);
                if (it == nodeFormulas.end()) {
                    std::cout << "[inc-regional-rebuild] post-rebuild head=" << incra::node_id(node)
                              << " formula_missing\n";
                    continue;
                }
                double wmc = formulaManager.computeWeightedModelCount(it->second);
                std::cout << "[inc-regional-rebuild] post-rebuild head=" << incra::node_id(node)
                          << " wmc=" << wmc << "\n";
            }
        }

        // === 5) Calibrate boundary gates ===
        auto calibRes = BoundaryGateCalibrator<FormulaManagerT, FormulaNodeRef>::calibrate(
            view, formulaManager, plan, rebuildRes.snapshot, nodeFormulas, opt_.eps);
        stats_.calibratedCount = calibRes.applied.size();
        lastCalibrations_ = calibRes.applied;
        lastOverrides_ = calibRes.weightOverrides;
        lastPlanRegionNodes_ = plan.regionNodes;
        lastPlanBoundaryNodes_ = plan.boundaryNodes;
        auto t5 = nowMs();
        lastTiming_.calibrateMs = toMs(t5 - t4);
        lastTiming_.totalMs = toMs(t5 - t0);

        if (!calibRes.failedNodes.empty() && opt_.enableFallbackToClassicInsertion) {
            if (incRegionalProfileEnabled) {
                std::cout << "[inc-regional] calibration failed for " << calibRes.failedNodes.size()
                          << " nodes, fallback to classic\n";
            }
            lastTiming_.fallbackReason = "calibration_failed";
            fallbackClassic();
            return;
        }

        // Apply calibrated gate weights so the existing (non-rebuilt) outside-of-region formulas
        // evaluate consistently under the new boundary targets.
        for (const auto& [varIdx, weights] : calibRes.weightOverrides) {
            formulaManager.setVariableWeight(varIdx, weights.first, weights.second);
        }
        if (incRegionalProfileEnabled) {
            std::cout << "[inc-regional] applied_gate_overrides=" << calibRes.weightOverrides.size() << "\n";
        }

        stats_.usedFallback = false;

        if (incRegionalProfileEnabled) {
            std::cout << "[inc-regional] timing(ms): analyze=" << lastTiming_.analyzeMs
                      << " sccClose=" << lastTiming_.sccCloseMs
                      << " plan=" << lastTiming_.planMs
                      << " rebuild=" << lastTiming_.rebuildMs
                      << " calibrate=" << lastTiming_.calibrateMs
                      << " total=" << lastTiming_.totalMs << "\n";
        }
    }

    const Stats& getStats() const { return stats_; }
    const Timing& getTiming() const { return lastTiming_; }
    const std::vector<CalibrationRecord>& getCalibrations() const { return lastCalibrations_; }
    const std::unordered_map<int, std::pair<double,double>>& getWeightOverrides() const {
        return lastOverrides_;
    }
    const std::set<NodePtr>& getRegionNodes() const { return lastPlanRegionNodes_; }
    const std::set<NodePtr>& getBoundaryNodes() const { return lastPlanBoundaryNodes_; }

private:
    Options opt_;
    Stats stats_;
    Timing lastTiming_;
    std::vector<CalibrationRecord> lastCalibrations_;
    std::unordered_map<int, std::pair<double,double>> lastOverrides_;
    std::set<NodePtr> lastPlanRegionNodes_;
    std::set<NodePtr> lastPlanBoundaryNodes_;
};
