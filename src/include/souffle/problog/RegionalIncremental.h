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
#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/IncRegionAnalyzer.h"
#include "souffle/problog/formula/FormulaManager.h"

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
    std::unordered_map<NodePtr, std::vector<EdgePtr>> mergeableAnchorsByHead;
};

struct RegionalInsertPlan {
    std::set<NodePtr> regionNodes;
    std::set<NodePtr> boundaryNodes;
    std::unordered_map<NodePtr, std::vector<EdgePtr>> anchorCandidates;
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
        const RegionalInsertPlan& plan) {
        Result res;
        auto nowMs = []{ return std::chrono::steady_clock::now(); };
        auto toMs = [](auto dur){
            return std::chrono::duration<double, std::milli>(dur).count();
        };
        auto t0 = nowMs();
        double reorderStartSec = getReorderSeconds(formulaManager, 0);

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
        CycleDependencyGraph depGraph(view);
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
                    worklist.push({e, depGraph.edgeDepthsGlobal[e], seq++});
                    inWorklist.insert(e);
                }
            }
            while (!worklist.empty()) {
                EdgePtr edge = worklist.top().edge;
                worklist.pop();
                res.timing.edgesProcessed += 1;
                inWorklist.erase(edge);
                FormulaNodeRef base = edge->isDeterministic()
                    ? formulaManager.getTrue()
                    : formulaManager.createVar(formulaManager.getVarIndex(*edge), *edge);
                std::vector<FormulaNodeRef> inputs{base};
                const auto& ins = view.getInputs(edge);
                const auto& negs = view.getBodyNegations(edge);
                bool allAvail = true;
                bool missingInsideRegion = false;
                for (size_t i = 0; i < ins.size(); ++i) {
                    auto it = nodeFormulas.find(ins[i]);
                    if (it == nodeFormulas.end()) {
                        allAvail = false;
                        if (plan.regionNodes.count(ins[i])) {
                            missingInsideRegion = true;
                        }
                        break;
                    }
                    inputs.push_back(negs[i] ? formulaManager.makeNot(it->second) : it->second);
                }
                if (!allAvail) {
                    // If missing inputs are outside the region, do not requeue forever.
                    if (missingInsideRegion) {
                        if (!inWorklist.count(edge)) worklist.push({edge, depGraph.edgeDepthsGlobal[edge], seq++});
                    }
                    continue;
                }
                FormulaNodeRef newEdge = (inputs.size()==1) ? inputs[0] : formulaManager.makeAnd(inputs);
                if (!edgeFormulas.count(edge) || !formulaManager.isSame(edgeFormulas[edge], newEdge)) {
                    edgeFormulas[edge] = newEdge;
                    res.rebuiltEdges.insert(edge);
                    NodePtr out = view.getOutput(edge);
                    if (!out || out->isFact) continue;
                    std::vector<FormulaNodeRef> incoming;
                    for (auto eIn : view.getIncomingEdges(out)) {
                        auto it = edgeFormulas.find(eIn);
                        if (it != edgeFormulas.end() && it->second.get()) incoming.push_back(it->second);
                    }
                    if (incoming.empty()) continue;
                    FormulaNodeRef newNode = (incoming.size()==1) ? incoming[0] : formulaManager.makeOr(incoming);
                    if (!nodeFormulas.count(out) || !formulaManager.isSame(nodeFormulas[out], newNode)) {
                        nodeFormulas[out] = newNode;
                        res.changedNodes.insert(out);
                        nodesUpdated += 1;
                        for (auto outEdge : view.getOutgoingEdges(out)) {
                            auto eit = depGraph.edgeToCycleIndex.find(outEdge);
                            if (eit != depGraph.edgeToCycleIndex.end() && eit->second == cid && shouldRebuildEdge(outEdge)) {
                                if (!inWorklist.count(outEdge)) {
                                    worklist.push({outEdge, depGraph.edgeDepthsGlobal[outEdge], seq++});
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
        EdgePtr anchor;
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
                res.failedNodes.insert(v);
                continue;
            }
            double target = formulaManager.computeWeightedModelCount(itNF->second);
            auto anchorIt = plan.anchorCandidates.find(v);
            if (anchorIt == plan.anchorCandidates.end() || anchorIt->second.empty()) {
                res.failedNodes.insert(v);
                continue;
            }
            bool calibrated = false;
            for (auto& anchor : anchorIt->second) {
                if (!anchor) continue;
                int varIdx = formulaManager.getVarIndex(*anchor);
                auto oldW = formulaManager.getVariableWeight(varIdx);
                auto snapIt = snapshot.boundarySnapshots.find(v);
                if (snapIt == snapshot.boundarySnapshots.end()) continue;
                double oldVal = formulaManager.computeWeightedModelCount(snapIt->second);
                formulaManager.setVariableWeight(varIdx, 0.0, 1.0);
                double alpha = formulaManager.computeWeightedModelCount(snapIt->second);
                formulaManager.setVariableWeight(varIdx, 1.0, 0.0);
                double beta  = formulaManager.computeWeightedModelCount(snapIt->second);
                formulaManager.setVariableWeight(varIdx, oldW.posWeight, oldW.negWeight);
                if (std::fabs(beta - alpha) < eps) {
                    continue;
                }
                double pStar = (target - alpha) / (beta - alpha);
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

    explicit RegionalIncrementalForwardCompilation(Options opt = {}) : opt_(opt) {}

    using CalibrationRecord = typename BoundaryGateCalibrator<FormulaManagerT, FormulaNodeRef>::CalibrationRecord;

    void applyUpdate(
        IncrementalDerivationGraphViewInterface& view,
        FormulaManagerT& formulaManager,
        std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
        std::map<EdgePtr, FormulaNodeRef>& edgeFormulas,
        std::set<NodePtr>& changedNodes) {
        const auto& deltaInsertedEdges = view.getDeltaInsertEdges();
        const auto& deltaInsertedNodes = view.getDeltaInsertNodes();
        if (deltaInsertedEdges.empty() && deltaInsertedNodes.empty()) {
            return;
        }

        auto nowMs = []{ return std::chrono::steady_clock::now(); };
        auto toMs = [](auto dur){
            return std::chrono::duration<double, std::milli>(dur).count();
        };
        auto t0 = nowMs();

        // === 1) Analyze region ===
        incra::RegionAnalyzer analyzer(view);
        std::vector<NodePtr> delta_inputs;
        for (auto n : deltaInsertedNodes) {
            if (n->isFact) delta_inputs.push_back(n);
        }
        // Regional pipeline expects an existing baseline; enforce it.
        assert(!(nodeFormulas.empty() && edgeFormulas.empty()) &&
               "Regional incremental requires pre-existing formulas; build baseline first.");
        analyzer.analyze(delta_inputs);
        auto analysis = analyzer.getLastAnalysis();
        auto t1 = nowMs();

        // === 2) Ensure SCC-closed ===
        CycleDependencyGraph depGraph(view);
        bool expanded = RegionSccClosure::closeToScc(analysis.region, analyzer.lastDeltaReachable(), depGraph);
        stats_.sccExpanded = expanded;
        auto t2 = nowMs();

        // === 3) Build plan ===
        AnalyzerResultCache cache;
        cache.region = analysis.region;
        cache.boundaries = analysis.boundaries;
        cache.mergeableAnchorsByHead = analysis.mergeableAnchorsByHead;
        RegionalInsertPlan plan = RegionalInsertPlanBuilder::build(cache);
        stats_.regionNodeCount = plan.regionNodes.size();
        stats_.boundaryNodeCount = plan.boundaryNodes.size();
        auto t3 = nowMs();

        auto fallbackClassic = [&]() {
            buildFormulasIncCyclewise(view, formulaManager, nodeFormulas, edgeFormulas, changedNodes);
            stats_.usedFallback = true;
        };

        if (!plan.mergeReady && opt_.enableFallbackToClassicInsertion) {
            std::cout << "[inc-regional] mergeReady=false, fallback to classic\n";
            fallbackClassic();
            return;
        }

        // === 4) Regional rebuild ===
        auto rebuildRes = RegionalDDRebuilder<FormulaManagerT, FormulaNodeRef>::rebuildInsertRegion(
            view, formulaManager, nodeFormulas, edgeFormulas, plan);
        changedNodes.insert(rebuildRes.changedNodes.begin(), rebuildRes.changedNodes.end());
        const auto& rt = rebuildRes.timing;
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
        auto t4 = nowMs();

        // === 5) Calibrate boundary gates ===
        auto calibRes = BoundaryGateCalibrator<FormulaManagerT, FormulaNodeRef>::calibrate(
            view, formulaManager, plan, rebuildRes.snapshot, nodeFormulas, opt_.eps);
        stats_.calibratedCount = calibRes.applied.size();
        lastCalibrations_ = calibRes.applied;
        lastOverrides_ = calibRes.weightOverrides;
        lastPlanRegionNodes_ = plan.regionNodes;
        lastPlanBoundaryNodes_ = plan.boundaryNodes;
        auto t5 = nowMs();

        if (!calibRes.failedNodes.empty() && opt_.enableFallbackToClassicInsertion) {
            std::cout << "[inc-regional] calibration failed for " << calibRes.failedNodes.size()
                      << " nodes, fallback to classic\n";
            fallbackClassic();
            return;
        }

        stats_.usedFallback = false;

        std::cout << "[inc-regional] timing(ms): analyze=" << toMs(t1 - t0)
                  << " sccClose=" << toMs(t2 - t1)
                  << " plan=" << toMs(t3 - t2)
                  << " rebuild=" << toMs(t4 - t3)
                  << " calibrate=" << toMs(t5 - t4)
                  << " total=" << toMs(t5 - t0) << "\n";
    }

    const Stats& getStats() const { return stats_; }
    const std::vector<CalibrationRecord>& getCalibrations() const { return lastCalibrations_; }
    const std::unordered_map<int, std::pair<double,double>>& getWeightOverrides() const {
        return lastOverrides_;
    }
    const std::set<NodePtr>& getRegionNodes() const { return lastPlanRegionNodes_; }
    const std::set<NodePtr>& getBoundaryNodes() const { return lastPlanBoundaryNodes_; }

private:
    Options opt_;
    Stats stats_;
    std::vector<CalibrationRecord> lastCalibrations_;
    std::unordered_map<int, std::pair<double,double>> lastOverrides_;
    std::set<NodePtr> lastPlanRegionNodes_;
    std::set<NodePtr> lastPlanBoundaryNodes_;
};
