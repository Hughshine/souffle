#ifndef FORWARDCOMPILATION_H
#define FORWARDCOMPILATION_H

#include <iostream>
#include "souffle/Derivation.h"
#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/RuleManager.h"
#include "souffle/problog/ConstAnalysis.h"
#include "souffle/problog/formula/FormulaManager.h"
#include "souffle/problog/formula/LogicFormulaManager.h"
#include <queue>
#include "souffle/problog/formula/CuddManager.h"
#include <queue>
#include <set>
#include <map>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <limits>
#include <climits>
#include <type_traits>
#include <utility>
#include <fstream>
#include <cstdlib>
#include <sstream>
#include "souffle/problog/debug/Debugger.h"


Debugger& debugger = Debugger::getInstance();

static inline const std::unordered_set<std::string>& fcTraceTargets() {
    static std::unordered_set<std::string> targets;
    static bool loaded = false;
    if (loaded) {
        return targets;
    }
    loaded = true;
    const char* raw = std::getenv("SOUFFLE_FC_TRACE_TUPLES");
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

static inline bool fcTraceEnabled() {
    return !fcTraceTargets().empty();
}

static inline bool fcTraceMatch(const NodePtr& node) {
    if (!node || !fcTraceEnabled()) {
        return false;
    }
    const auto& targets = fcTraceTargets();
    return targets.find(node->getTuple().toString()) != targets.end();
}

inline void assertProbabilityInRange(double p, const std::string& ctx) {
//    std::cout << "[ForwardCompilation] probability check " << p << " at " << ctx << std::endl;
    if (p < 0.0 || p > 1.0) {
        std::cerr << "[ForwardCompilation] invalid probability " << p << " at " << ctx << std::endl;
        assert(false && "probability out of [0,1]");
    }
}


// Currently support CuddManager only; not optimized version
// 1. no stratum-by-stratum and cycle-by-cycle processing
// 2. no special designed logic formula manager - mainly for formula's semantic equivalence checking
template<typename FormulaNodeRef>
void buildFormulas(
    const SubgraphView& view,
    FormulaManager<FormulaNodeRef>& formulaManager,
    std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
    std::map<EdgePtr, FormulaNodeRef>& edgeFormulas
) {
    FunctionTimer timer(" forward compilation, building formulas ");
    const bool useConst = DerivationGraph::isConstFoldEnabled();
    const bool dumpConst = DerivationGraph::isConstDumpEnabled();
    ConstAnalysisResult constInfo;
    const ConstAnalysisResult* constInfoPtr = nullptr;
    if (useConst || dumpConst) {
        auto constStart = std::chrono::steady_clock::now();
        constInfo = analyzeConstants(view, true);
        auto constMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - constStart).count();
        if (useConst) {
            constInfoPtr = &constInfo;
        }
        std::cout << "[const-pre] tag=full-worklist took " << constMs << " ms" << std::endl;
        logConstAnalysis(constInfo, view, "full-worklist");
    }
    ConstFormulaAccess<FormulaNodeRef> constAccess{constInfoPtr, formulaManager};

    // Initialize formulas for input facts (nodes)
    std::map<NodePtr, FormulaNodeRef> baseNodeFormulas;
    std::map<EdgePtr, FormulaNodeRef> baseEdgeFormulas;
    for (const auto& node : view.getNodes()) {
        // Create a variable using the node's unique ID
        if (node->isFact) {
            int idx = formulaManager.getVarIndex(*node);
            if (node->getProbability() == 1.0) {
                nodeFormulas[node] = formulaManager.getTrue();
            } else {
                nodeFormulas[node] = formulaManager.createVar(idx, *node);
                formulaManager.setVariableWeight(idx, node->getProbability(), 1 - node->getProbability());
            }
            baseNodeFormulas.insert({node, nodeFormulas[node]});
        }
    }

    // Initialize formulas for rule instantiations (hyperedges)
    for (const auto& edge : view.getEdges()) {
        // Create a variable using the edge's unique ID; need to plus the size of graph.getNodes() to avoid conflict with node's id
        if (edge->isDeterministic()) {
            auto baseEdgeFormula = formulaManager.getTrue();
            baseEdgeFormulas.insert({edge, baseEdgeFormula});
        } else {
            int idx = formulaManager.getVarIndex(*edge);
            auto baseEdgeFormula = formulaManager.createVar(idx, *edge);
            formulaManager.setVariableWeight(idx, edge->getProbability(), 1 - edge->getProbability());
            baseEdgeFormulas.insert({edge, baseEdgeFormula});
        }
    }

    // Worklist algorithm
    std::queue<EdgePtr> worklist;
    std::set<EdgePtr> inWorklist; // Track edges in the worklist to avoid duplicates

    // Initialize worklist with all edges
    for (const auto& edge : view.getEdges()) {
        worklist.push(edge);
        inWorklist.insert(edge);
    }

    long long iteration = 0;
    while (!worklist.empty()) {
//        std::cout << "Iteration: " << iteration << std::endl;
//        std::cout << "Worklist size: " << worklist.size() << std::endl;
//        formulaManager.dumpProfilingStatistics();
        iteration++;
        auto edge = worklist.front();
        worklist.pop();
        inWorklist.erase(edge);
//        std::cout << "Processing edge " << edge->getId() << " " << edge->toString() << std::endl;
        // Store the old edge formula to check if it changes
        FormulaNodeRef oldEdgeFormula = edgeFormulas[edge];
//        std::cout << "Old edge formula: " << formulaManager.toString(oldEdgeFormula) << std::endl;
        // Compute new formula for the edge (conjunction of input node formulas and rule formula)
        FormulaNodeRef newEdgeFormula;
        bool edgeIsConst = constAccess.edgeFormula(edge, newEdgeFormula);
        bool allInputsAvailable = true;
        if (!edgeIsConst) {
            std::vector<FormulaNodeRef> inputFormulas;
            // Add the rule formula (the edge's base formula)
            inputFormulas.push_back(baseEdgeFormulas[edge]);

            // Add the input node formulas
            assert (view.getInputs(edge).size() == view.getBodyNegations(edge).size());
            assert (view.getInputs(edge).size() == edge->getInputs().size());
            for (size_t i = 0; i < view.getInputs(edge).size(); i++) {
                auto input = view.getInputs(edge)[i];
                auto isNegated = view.getBodyNegations(edge)[i];
                FormulaNodeRef lit;
                if (!constAccess.inputLiteral(nodeFormulas, input, isNegated, lit)) {
                    // Input node formula not available yet, skip this edge for now
                    // cannot skip, since there is cycle
                    allInputsAvailable = false;
//                std::cout << "Input node formula not available yet: " << input->getTuple().toString() << std::endl;
                    break;
                }
                inputFormulas.push_back(lit);
            }

            if (!allInputsAvailable) {
                // Put the edge back in the worklist for later processing
                worklist.push(edge);
                inWorklist.insert(edge);
                continue;
            }

            // Compute the conjunction of all input formulas
            if (inputFormulas.size() == 1) {
                newEdgeFormula = inputFormulas[0];
            } else {
                newEdgeFormula = formulaManager.makeAnd(inputFormulas);
            }
        }

        // Check if the edge formula actually changed
        bool edgeFormulaChanged = !formulaManager.isSame(oldEdgeFormula, newEdgeFormula);

        if (edgeFormulaChanged) {
            // Update the edge formula
            edgeFormulas[edge] = newEdgeFormula;
            // Update the output node formula
            auto output = view.getOutput(edge);
            // Store the old node formula to check if it changes
            FormulaNodeRef oldNodeFormula;
            bool nodeHasFormula = nodeFormulas.find(output) != nodeFormulas.end();
            if (nodeHasFormula) {
                oldNodeFormula = nodeFormulas[output];
            }

            // Compute the disjunction of all incoming edge formulas
            FormulaNodeRef newNodeFormula;
            bool hasNewNodeFormula = constAccess.nodeFormula(output, newNodeFormula);
            if (!hasNewNodeFormula) {
                // Collect formulas from all incoming edges
                std::vector<FormulaNodeRef> incomingFormulas;
                for (const auto& inEdge : view.getIncomingEdges(output)) {
                    auto it = edgeFormulas.find(inEdge);
                    if (it != edgeFormulas.end()) {
                        if (it->second.get()) {
                            // TODO: don't know why it can be NULL
                            incomingFormulas.push_back(it->second);
                        }
                    }
                }

                if (incomingFormulas.empty()) {
                    assert (false && "No incoming edge formula found for the output node");
                } else if (incomingFormulas.size() == 1) {
                    newNodeFormula = incomingFormulas[0];
                    hasNewNodeFormula = true;
                } else {
                    newNodeFormula = formulaManager.makeOr(incomingFormulas);
                    hasNewNodeFormula = true;
                }
            }

            if (!hasNewNodeFormula) {
                continue;
            }

            // Check if the node formula actually changed
            bool nodeFormulaChanged = !nodeHasFormula ||
                                     !formulaManager.isSame(oldNodeFormula, newNodeFormula);

            if (nodeFormulaChanged) {
                // Update the node formula
                nodeFormulas[output] = newNodeFormula;
                // Add outgoing edges to the worklist
                for (const auto& outEdge : view.getOutgoingEdges(output)) {
                    if (inWorklist.find(outEdge) == inWorklist.end()) {
                        worklist.push(outEdge);
                        inWorklist.insert(outEdge);
                    }
                }
            }
        }
    }
    std::cout << "Successfully build formulas" << std::endl;
}

struct PrioritizedEdge {
    EdgePtr edge;
    size_t priority;
    int sequence_id;
    bool operator<(const PrioritizedEdge& other) const {
        if (priority != other.priority)
            return priority > other.priority;  // Smaller value first
        return sequence_id > other.sequence_id;  // Smaller sequence id first
    }
};

struct FcProfileStats {
    std::size_t edge_processed = 0;
    std::size_t edge_requeued = 0;
    std::size_t edge_updated = 0;
    std::size_t edge_const = 0;
    std::size_t edge_nonconst = 0;
    std::size_t node_recomputed = 0;
    std::size_t node_updated = 0;
    std::size_t node_const = 0;
    std::size_t make_and_calls = 0;
    double make_and_ms = 0.0;
    std::size_t make_or_calls = 0;
    double make_or_ms = 0.0;
    std::size_t make_condition_calls = 0;
    double make_condition_ms = 0.0;
    std::size_t input_literal_calls = 0;
    std::size_t input_literal_missing = 0;
    double input_literal_ms = 0.0;
};

struct FcHeartbeatSnapshot {
    std::size_t elapsedMs = 0;
    std::size_t totalCycles = 0;
    std::size_t completedCycles = 0;
    std::size_t currentCycleId = 0;
    std::size_t round = 0;
    std::size_t readyQueueSize = 0;
    std::size_t worklistSize = 0;
    std::size_t nodeFormulaCount = 0;
    std::size_t edgeFormulaCount = 0;
};

struct ScbfCyclewiseStats {
    std::size_t cyclesProcessed = 0;
    std::size_t boundaryNodesKept = 0;
    std::size_t purgedNodeFormulas = 0;
    std::size_t purgedEdgeFormulas = 0;
    std::size_t outputWmcMs = 0;
};
template<typename FormulaNodeRef>
void buildFormulasCyclewiseInternal(
    SubgraphView& view,
    FormulaManager<FormulaNodeRef>& formulaManager,
    std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
    std::map<EdgePtr, FormulaNodeRef>& edgeFormulas,
    const std::unordered_set<NodePtr>& seedTrueNodes = {},
    std::vector<double>* roundTimingsMs = nullptr,
    bool allowConst = true,
    bool allowDumpConst = true,
    const std::function<void(const FcHeartbeatSnapshot&)>& heartbeatCallback = nullptr,
    std::size_t heartbeatIntervalMs = 5000,
    bool breakCyclesForScbf = false,
    ScbfCyclewiseStats* scbfStatsOut = nullptr
) {
     FunctionTimer timer("Build Formulas Cyclewise using DAG + Depth");
     const bool fcProfile = fcProfileEnabled;
     using Clock = std::chrono::steady_clock;
     auto toMs = [](auto d) {
         return std::chrono::duration<double, std::milli>(d).count();
     };
     auto overallStart = Clock::now();
     double constMs = 0.0;
     FcProfileStats stats;
     const std::size_t nodeCount = view.getNodes().size();
     const std::size_t edgeCount = view.getEdges().size();
     std::size_t factNodes = 0;
     std::size_t detEdges = 0;
     std::size_t nonDetEdges = 0;
     ScbfCyclewiseStats scbfLocalStats;
     ScbfCyclewiseStats* scbfStats = breakCyclesForScbf
             ? (scbfStatsOut ? scbfStatsOut : &scbfLocalStats)
             : nullptr;
     if (scbfStats) {
         *scbfStats = ScbfCyclewiseStats{};
     }
     const bool useConst = allowConst && DerivationGraph::isConstFoldEnabled();
     const bool dumpConst = allowDumpConst && DerivationGraph::isConstDumpEnabled();
     ConstAnalysisResult constInfo;
     const ConstAnalysisResult* constInfoPtr = nullptr;
     if (useConst || dumpConst) {
         auto constStart = Clock::now();
         constInfo = analyzeConstants(view, true);
         constMs = toMs(Clock::now() - constStart);
         if (useConst) {
             constInfoPtr = &constInfo;
         }
         if (seedTrueNodes.empty()) {
             std::cout << "[const-pre] tag=full-cyclewise took " << constMs << " ms" << std::endl;
             logConstAnalysis(constInfo, view, "full-cyclewise");
         }
     }
     ConstFormulaAccess<FormulaNodeRef> constAccess{constInfoPtr, formulaManager};

    auto preStart = Clock::now();
     setCuddPreConfigTag("full_cyclewise");
     formulaManager.preConfig(view);
     setCuddPreConfigTag("");
    auto preConfigMs = toMs(Clock::now() - preStart);
    debugger.logMessage(Level::INFO,
            "preConfig (cache clear + var scan/create + dyn-reorder setup) took " +
                    std::to_string(preConfigMs) + " ms");

    auto depStart = Clock::now();
    auto& depGraph = view.getCycleDependencyGraph();
//    depGraph.dumpCycles(std::cout);
    depGraph.dumpDot("scc.dot");
    auto depMs = toMs(Clock::now() - depStart);

    auto baseStart = Clock::now();
    std::map<NodePtr, FormulaNodeRef> baseNodeFormulas;
    std::map<EdgePtr, FormulaNodeRef> baseEdgeFormulas;
    size_t round = 0;
    auto assertProb = [](double p, const std::string& ctx) {
//        std::cout << "[ForwardCompilation] probability check " << p << " at " << ctx << std::endl;
        if (p < 0.0 || p > 1.0) {
            std::cerr << "[ForwardCompilation] invalid probability " << p << " at " << ctx << std::endl;
            assert(false && "probability out of [0,1]");
        }
    };

    // 1. Initialize formulas
    for (const auto& node : view.getNodes()) {
        if (seedTrueNodes.count(node)) {
            FormulaNodeRef var = formulaManager.getTrue();
            nodeFormulas[node] = var;
            baseNodeFormulas[node] = var;
        } else if (node->isFact) {
            ++factNodes;
            int idx = formulaManager.getVarIndex(*node);
            assertProb(node->getProbability(), "fact init " + node->toString());
            FormulaNodeRef var = (node->getProbability() == 1.0)
                ? formulaManager.getTrue()
                : formulaManager.createVar(idx, *node);
            assertProbabilityInRange(node->getProbability(), "fact init " + node->toString());
            formulaManager.setVariableWeight(idx, node->getProbability(), 1 - node->getProbability());
            nodeFormulas[node] = var;
            baseNodeFormulas[node] = var;
        }

    }

    for (const auto& edge : view.getEdges()) {
        int idx = edge->isDeterministic() ? -1 : formulaManager.getVarIndex(*edge);
        if (edge->isDeterministic()) {
            ++detEdges;
        } else {
            ++nonDetEdges;
        }
        FormulaNodeRef f = edge->isDeterministic()
            ? formulaManager.getTrue()
            : formulaManager.createVar(idx, *edge);
        if (!edge->isDeterministic()) {
            assertProb(edge->getProbability(), "edge init " + edge->toString());
            formulaManager.setVariableWeight(idx, edge->getProbability(), 1 - edge->getProbability());
        }
        baseEdgeFormulas[edge] = f;
    }
    auto baseInitMs = toMs(Clock::now() - baseStart);

    // 2. Schedule SCCs
    auto cycleTotalStart = Clock::now();
    std::vector<size_t> remainingInDegrees = depGraph.inDegrees;
    std::vector<bool> visited(depGraph.nodeCycles.size(), false);
    std::queue<size_t> ready;
    for (size_t i = 0; i < remainingInDegrees.size(); ++i) {
        if (remainingInDegrees[i] == 0)
            ready.push(i);
    }

    while (!ready.empty()) {
        auto cycleStart = Clock::now();
        size_t cid = ready.front(); ready.pop();
        if (visited[cid]) continue;
        visited[cid] = true;

        const auto& cycleEdges = depGraph.edgeCycles[cid];
        std::priority_queue<PrioritizedEdge> worklist;
        std::set<EdgePtr> inWorklist;
//        std::cout << "Processing cycle " << cid << ", edges: ";

        int _seqId = 0;
        for (auto edge : cycleEdges) {
            worklist.push({edge, depGraph.edgeDepthsGlobal.at(edge), _seqId++});
        //    std::cout << "Adding edge " << edge->getId() << " " << edge->toString()
        //              << " with depth " << depGraph.edgeDepthsGlobal.at(edge) << " to worklist.\n";
            inWorklist.insert(edge);
        }

        // Track repeated stalls to help diagnose infinite loops.
        std::map<EdgePtr, size_t> stallCount;
        constexpr size_t kMaxStall = 100000;  // defensive cap to avoid infinite requeue
        std::set<EdgePtr> loggedFirstStall;

        while (!worklist.empty()) {
            auto roundStart = Clock::now();
            round++;
//            std::cout << "Round: " << round << std::endl;
//            std::cout << "Processing cycle " << cid << ", worklist size: " << worklist.size() << std::endl;
            formulaManager.dumpProfilingStatistics();

            EdgePtr edge = worklist.top().edge;
            size_t depth = worklist.top().priority;

//            std::cout << edge->toString() << " with depth " << depth << std::endl;
            worklist.pop();
            inWorklist.erase(edge);
            if (fcProfile) {
                stats.edge_processed++;
            }
//            std::cout << "Processing edge " << edge->getId() << " " << edge->toString() << std::endl;
//            std::cout << "Edge depth: " << depth << std::endl;
            FormulaNodeRef newEdgeF;
            bool edgeIsConst = constAccess.edgeFormula(edge, newEdgeF);
            if (fcProfile) {
                if (edgeIsConst) {
                    stats.edge_const++;
                } else {
                    stats.edge_nonconst++;
                }
            }
            bool allAvailable = true;
            if (!edgeIsConst) {
                std::vector<FormulaNodeRef> inputs = { baseEdgeFormulas[edge] };
                for (size_t i = 0; i < view.getInputs(edge).size(); ++i) {
                    NodePtr input = view.getInputs(edge)[i];
                    FormulaNodeRef lit;
                    bool ok = true;
                    if (fcProfile) {
                        auto litStart = Clock::now();
                        ok = constAccess.inputLiteral(nodeFormulas, input, view.getBodyNegations(edge)[i], lit);
                        stats.input_literal_calls++;
                        stats.input_literal_ms += toMs(Clock::now() - litStart);
                        if (!ok) {
                            stats.input_literal_missing++;
                        }
                    } else {
                        ok = constAccess.inputLiteral(nodeFormulas, input, view.getBodyNegations(edge)[i], lit);
                    }
                    if (!ok) {
//                    std::cout << "Input node formula not available: " << input->getId() << " " << input->toString() << std::endl;
                        allAvailable = false;
                        auto& sc = stallCount[edge];
                        sc++;
                        if (loggedFirstStall.insert(edge).second || sc == 100 || sc == 1000) {
                            std::cout << "[buildFormulasCyclewise] stall edge " << edge->toString()
                                      << " missing input formula for node " << input->toString()
                                      << " (stall #" << sc << ")" << std::endl;
                        }
                        if (sc > kMaxStall) {
                            std::cout << "[buildFormulasCyclewise] giving up on edge " << edge->toString()
                                      << " after " << sc << " stalls; setting formula to False to continue."
                                      << std::endl;
                            edgeFormulas[edge] = formulaManager.getFalse();
                            allAvailable = true;  // allow propagation of False to break the cycle
                        }
                        break;
                    }
                    inputs.push_back(lit);
                }

                if (!allAvailable) {
//                std::cout << "Not all inputs available for edge " << edge->getId() << ", re-adding to worklist.\n";
                    worklist.push({edge, depGraph.edgeDepthsGlobal.at(edge), _seqId++});
                    inWorklist.insert(edge);
                    if (fcProfile) {
                        stats.edge_requeued++;
                    }
                    continue;
                }

                if (inputs.size() == 1) {
                    newEdgeF = inputs[0];
                } else if (fcProfile) {
                    auto andStart = Clock::now();
                    newEdgeF = formulaManager.makeAnd(inputs);
                    stats.make_and_calls++;
                    stats.make_and_ms += toMs(Clock::now() - andStart);
                } else {
                    newEdgeF = formulaManager.makeAnd(inputs);
                }
            }
            if (!formulaManager.isSame(edgeFormulas[edge], newEdgeF)) {
                edgeFormulas[edge] = newEdgeF;
                if (fcProfile) {
                    stats.edge_updated++;
                }
                NodePtr out = view.getOutput(edge);

                FormulaNodeRef newNodeF;
                bool hasNewNodeF = constAccess.nodeFormula(out, newNodeF);
                if (fcProfile && hasNewNodeF) {
                    stats.node_const++;
                }
                if (!hasNewNodeF) {
                    std::vector<FormulaNodeRef> inFs;
                    for (auto& inEdge : view.getIncomingEdges(out)) {
                        auto it = edgeFormulas.find(inEdge);
                        if (it != edgeFormulas.end() && it->second.get()) {
                            inFs.push_back(it->second);
                        }
                    }

                    if (!inFs.empty()) {
                        if (fcProfile) {
                            stats.node_recomputed++;
                        }
                        if (inFs.size() == 1) {
                            newNodeF = inFs[0];
                        } else if (fcProfile) {
                            auto orStart = Clock::now();
                            newNodeF = formulaManager.makeOr(inFs);
                            stats.make_or_calls++;
                            stats.make_or_ms += toMs(Clock::now() - orStart);
                        } else {
                            newNodeF = formulaManager.makeOr(inFs);
                        }
                        hasNewNodeF = true;
                    }
                }

                if (hasNewNodeF) {
                    if (!nodeFormulas.count(out) || !formulaManager.isSame(nodeFormulas[out], newNodeF)) {
                        nodeFormulas[out] = newNodeF;
                        if (fcProfile) {
                            stats.node_updated++;
                        }

                        for (auto& outEdge : view.getOutgoingEdges(out)) {
                            auto it = depGraph.edgeToCycleIndex.find(outEdge);
                            if (it != depGraph.edgeToCycleIndex.end() && it->second == cid && !inWorklist.count(outEdge)) {
                                worklist.push({outEdge, depGraph.edgeDepthsGlobal.at(outEdge), _seqId++});
                                inWorklist.insert(outEdge);
                            }
                        }
                    }
                }
            }

            if (roundTimingsMs != nullptr) {
                auto roundEnd = Clock::now();
                double roundMs = std::chrono::duration<double, std::milli>(roundEnd - roundStart).count();
                roundTimingsMs->push_back(roundMs);
            }
        }

        if (breakCyclesForScbf) {
            std::unordered_set<NodePtr> boundaryNodes;
            boundaryNodes.reserve(depGraph.nodeCycles[cid].size());
            for (auto node : depGraph.nodeCycles[cid]) {
                bool isBoundary = false;
                for (const auto& outEdge : view.getOutgoingEdges(node)) {
                    auto it = depGraph.edgeToCycleIndex.find(outEdge);
                    if (it != depGraph.edgeToCycleIndex.end() && it->second != cid) {
                        isBoundary = true;
                        break;
                    }
                }
                if (isBoundary) {
                    boundaryNodes.insert(node);
                }
            }
            if (scbfStats) {
                scbfStats->cyclesProcessed++;
                scbfStats->boundaryNodesKept += boundaryNodes.size();
            }

            auto wmcStart = Clock::now();
            for (auto node : depGraph.nodeCycles[cid]) {
                if (!node->needOutput) {
                    continue;
                }
                auto it = nodeFormulas.find(node);
                if (it == nodeFormulas.end() || !it->second.get()) {
                    continue;
                }
                probResult[node] = formulaManager.computeWeightedModelCount(it->second);
            }
            auto wmcMs = static_cast<std::size_t>(toMs(Clock::now() - wmcStart));
            if (scbfStats) {
                scbfStats->outputWmcMs += wmcMs;
            }

            std::size_t purgedEdges = 0;
            for (auto edge : depGraph.edgeCycles[cid]) {
                purgedEdges += edgeFormulas.erase(edge);
            }
            std::size_t purgedNodes = 0;
            for (auto node : depGraph.nodeCycles[cid]) {
                if (boundaryNodes.count(node)) {
                    continue;
                }
                purgedNodes += nodeFormulas.erase(node);
            }
            if (scbfStats) {
                scbfStats->purgedEdgeFormulas += purgedEdges;
                scbfStats->purgedNodeFormulas += purgedNodes;
            }
            formulaManager.tryGarbageCollection();
        }

        for (auto succ : depGraph.reverseDependencies[cid]) {
            if (--remainingInDegrees[succ] == 0) {
                ready.push(succ);
            }
        }

        (void)cycleStart;  // silence unused warning if roundTimingsMs is null
    }
    auto cycleMs = toMs(Clock::now() - cycleTotalStart);

    auto end = Clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - baseStart).count();
    formulaManager.dumpProfilingStatistics();
    for (auto& [key, value]: formulaManager.getProfilingStatistics()) {
        debugger.addInfo(key, value);
    }
    debugger.logMessage(Level::INFO, "Total rounds: " + std::to_string(round));
    debugger.logMessage(Level::INFO, "Insertion time: " + std::to_string(duration) + " ms");
    double overallMs = toMs(Clock::now() - overallStart);
    std::cout << "[buildFormulasCyclewise] timings(ms): total=" << overallMs
              << " preConfig=" << preConfigMs
              << " depGraph=" << depMs
              << " baseInit=" << baseInitMs
              << " cycles=" << cycleMs
              << " rounds=" << round
              << std::endl;
    if (fcProfile) {
        std::cout << "[fc-profile] stage=FORWARD_COMPILATION_FULL total_ms=" << overallMs
                  << " const_ms=" << constMs
                  << " preConfig_ms=" << preConfigMs
                  << " depGraph_ms=" << depMs
                  << " baseInit_ms=" << baseInitMs
                  << " cycles_ms=" << cycleMs
                  << " rounds=" << round
                  << " nodes=" << nodeCount
                  << " edges=" << edgeCount
                  << " fact_nodes=" << factNodes
                  << " det_edges=" << detEdges
                  << " nondet_edges=" << nonDetEdges
                  << " edge_processed=" << stats.edge_processed
                  << " edge_requeued=" << stats.edge_requeued
                  << " edge_updated=" << stats.edge_updated
                  << " edge_const=" << stats.edge_const
                  << " edge_nonconst=" << stats.edge_nonconst
                  << " node_recomputed=" << stats.node_recomputed
                  << " node_updated=" << stats.node_updated
                  << " node_const=" << stats.node_const
                  << " make_and_calls=" << stats.make_and_calls
                  << " make_and_ms=" << stats.make_and_ms
                  << " make_or_calls=" << stats.make_or_calls
                  << " make_or_ms=" << stats.make_or_ms
                  << " make_condition_calls=" << stats.make_condition_calls
                  << " make_condition_ms=" << stats.make_condition_ms
                  << " input_literal_calls=" << stats.input_literal_calls
                  << " input_literal_missing=" << stats.input_literal_missing
                  << " input_literal_ms=" << stats.input_literal_ms
                  << std::endl;
    }
    if (breakCyclesForScbf) {
        const std::size_t cycles = scbfStats ? scbfStats->cyclesProcessed : 0;
        const std::size_t boundaryNodes = scbfStats ? scbfStats->boundaryNodesKept : 0;
        const std::size_t purgedNodes = scbfStats ? scbfStats->purgedNodeFormulas : 0;
        const std::size_t purgedEdges = scbfStats ? scbfStats->purgedEdgeFormulas : 0;
        const std::size_t outputWmcMs = scbfStats ? scbfStats->outputWmcMs : 0;
        debugger.addInfo("scbf_cycles", std::to_string(cycles));
        debugger.addInfo("scbf_boundary_nodes", std::to_string(boundaryNodes));
        debugger.addInfo("scbf_purged_nodes", std::to_string(purgedNodes));
        debugger.addInfo("scbf_purged_edges", std::to_string(purgedEdges));
        debugger.addInfo("scbf_output_wmc_ms", std::to_string(outputWmcMs));
        std::cout << "[scbf-cyclewise] cycles=" << cycles
                  << " boundary_nodes=" << boundaryNodes
                  << " purged_nodes=" << purgedNodes
                  << " purged_edges=" << purgedEdges
                  << " output_wmc_ms=" << outputWmcMs
                  << std::endl;
    }

//    std::cout << "鉁?buildFormulasCyclewiseNew completed using global depth info.\n";
}

template<typename FormulaNodeRef>
void buildFormulasCyclewise(
    SubgraphView& view,
    FormulaManager<FormulaNodeRef>& formulaManager,
    std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
    std::map<EdgePtr, FormulaNodeRef>& edgeFormulas,
    const std::unordered_set<NodePtr>& seedTrueNodes = {},
    std::vector<double>* roundTimingsMs = nullptr,
    bool allowConst = true,
    bool allowDumpConst = true,
    const std::function<void(const FcHeartbeatSnapshot&)>& heartbeatCallback = nullptr,
    std::size_t heartbeatIntervalMs = 5000
) {
    buildFormulasCyclewiseInternal(view, formulaManager, nodeFormulas, edgeFormulas, seedTrueNodes,
            roundTimingsMs, allowConst, allowDumpConst, heartbeatCallback, heartbeatIntervalMs,
            false, nullptr);
}

template<typename FormulaNodeRef>
void buildFormulasCyclewiseScbf(
    SubgraphView& view,
    FormulaManager<FormulaNodeRef>& formulaManager,
    std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
    std::map<EdgePtr, FormulaNodeRef>& edgeFormulas,
    const std::unordered_set<NodePtr>& seedTrueNodes = {},
    ScbfCyclewiseStats* scbfStatsOut = nullptr
) {
    buildFormulasCyclewiseInternal(view, formulaManager, nodeFormulas, edgeFormulas, seedTrueNodes,
            nullptr, true, true, nullptr, 5000, true, scbfStatsOut);
}

struct ComponentSubgraph {
    size_t id;
    std::unordered_set<NodePtr> nodes;
    std::unordered_set<EdgePtr> edges;
};

inline std::size_t countComponentRandomVars(const ComponentSubgraph& comp) {
    std::size_t count = 0;
    for (const auto& node : comp.nodes) {
        if (node->isFact && node->getProbability() != 1.0) {
            ++count;
        }
    }
    for (const auto& edge : comp.edges) {
        if (!edge->isDeterministic()) {
            ++count;
        }
    }
    return count;
}

inline std::vector<ComponentSubgraph> buildComponentSubgraphs(const DerivationGraphViewInterface& view) {
    auto& depGraph = view.getCycleDependencyGraph();
    const size_t componentCount = depGraph.getComponentCount();
    std::vector<std::unordered_set<NodePtr>> nodesByComponent(componentCount);
    std::vector<std::unordered_set<EdgePtr>> edgesByComponent(componentCount);

    for (const auto& node : view.getNodes()) {
        size_t cid = depGraph.getComponentId(node);
        nodesByComponent[cid].insert(node);
    }
    for (const auto& edge : view.getEdges()) {
        NodePtr out = view.getOutput(edge);
        if (!out) {
            continue;
        }
        size_t cid = depGraph.getComponentId(out);
        edgesByComponent[cid].insert(edge);
    }

    std::vector<ComponentSubgraph> components;
    components.reserve(componentCount);
    for (size_t cid = 0; cid < componentCount; ++cid) {
        if (nodesByComponent[cid].empty() && edgesByComponent[cid].empty()) {
            continue;
        }
        components.push_back(ComponentSubgraph{cid, std::move(nodesByComponent[cid]),
                std::move(edgesByComponent[cid])});
    }
    return components;
}

struct SingleRandVarInfo {
    NodePtr node;
    EdgePtr edge;
    double probability = 1.0;
};

inline bool findSingleRandVar(const ComponentSubgraph& comp, SingleRandVarInfo& out) {
    std::size_t count = 0;
    out = SingleRandVarInfo{};

    for (const auto& node : comp.nodes) {
        if (node->isFact && node->getProbability() != 1.0) {
            ++count;
            if (count > 1) {
                return false;
            }
            out.node = node;
            out.edge.reset();
            out.probability = node->getProbability();
        }
    }
    for (const auto& edge : comp.edges) {
        if (!edge->isDeterministic()) {
            ++count;
            if (count > 1) {
                return false;
            }
            out.node.reset();
            out.edge = edge;
            out.probability = edge->getProbability();
        }
    }
    return count == 1;
}

struct BoolNodeRef {
    bool value = false;
    bool valid = false;
    void* get() const { return valid ? const_cast<BoolNodeRef*>(this) : nullptr; }
};

class BoolFormulaManager final : public FormulaManager<BoolNodeRef> {
public:
    BoolFormulaManager(NodePtr targetNode, EdgePtr targetEdge, bool varValue)
            : targetNode(std::move(targetNode)), targetEdge(std::move(targetEdge)), varValue(varValue) {}

    BoolNodeRef createVar(int) override {
        return markVar(nullptr, nullptr);
    }
    BoolNodeRef createVar(int, const Node& node) override {
        return markVar(&node, nullptr);
    }
    BoolNodeRef createVar(int, const Hyperedge& edge) override {
        return markVar(nullptr, &edge);
    }

    BoolNodeRef makeAnd(const BoolNodeRef& a, const BoolNodeRef& b) override {
        return BoolNodeRef{a.value && b.value, true};
    }
    BoolNodeRef makeAnd(const std::vector<BoolNodeRef>& nodes) override {
        bool value = true;
        for (const auto& node : nodes) {
            value = value && node.value;
            if (!value) break;
        }
        return BoolNodeRef{value, true};
    }
    BoolNodeRef makeOr(const BoolNodeRef& a, const BoolNodeRef& b) override {
        return BoolNodeRef{a.value || b.value, true};
    }
    BoolNodeRef makeOr(const std::vector<BoolNodeRef>& nodes) override {
        bool value = false;
        for (const auto& node : nodes) {
            value = value || node.value;
            if (value) break;
        }
        return BoolNodeRef{value, true};
    }
    BoolNodeRef makeNot(const BoolNodeRef& a) override {
        return BoolNodeRef{!a.value, true};
    }
    BoolNodeRef makeCondition(const BoolNodeRef& f, const std::vector<int>&,
            const std::vector<int>&) override {
        return f;
    }
    BoolNodeRef getTrue() override {
        return BoolNodeRef{true, true};
    }
    BoolNodeRef getFalse() override {
        return BoolNodeRef{false, true};
    }
    bool isSame(const BoolNodeRef& a, const BoolNodeRef& b) override {
        return a.valid == b.valid && a.value == b.value;
    }
    std::string toString(const BoolNodeRef& node) override {
        return node.value ? "true" : "false";
    }
    void setVariableWeight(int, double, double) override {}
    double computeWeightedModelCount(const BoolNodeRef& node) override {
        return node.value ? 1.0 : 0.0;
    }
    int getVarIndex(const Node&) override { return 0; }
    int getVarIndex(const Hyperedge&) override { return 0; }
    void printInfo(const BoolNodeRef&, const std::string&) override {}
    void dumpProfilingStatistics() override {}

    bool isValid() const { return !invalid && sawVar; }

private:
    BoolNodeRef markVar(const Node* node, const Hyperedge* edge) {
        if (sawVar) {
            invalid = true;
            return BoolNodeRef{varValue, true};
        }
        if (node != nullptr) {
            if (!targetNode || targetNode.get() != node) {
                invalid = true;
            }
        } else if (edge != nullptr) {
            if (!targetEdge || targetEdge.get() != edge) {
                invalid = true;
            }
        } else {
            invalid = true;
        }
        sawVar = true;
        return BoolNodeRef{varValue, true};
    }

    NodePtr targetNode;
    EdgePtr targetEdge;
    bool varValue = false;
    bool sawVar = false;
    bool invalid = false;
};

struct ConjNodeRef {
    std::vector<int> vars;
    bool valid = true;
    bool isFalse = false;
    void* get() const { return valid ? const_cast<ConjNodeRef*>(this) : nullptr; }
};

class ConjFormulaManager final : public FormulaManager<ConjNodeRef> {
public:
    ConjNodeRef createVar(int idx) override {
        registerVar(idx, 1.0);
        return makeVar(idx);
    }
    ConjNodeRef createVar(int idx, const Node& node) override {
        registerVar(idx, node.getProbability());
        return makeVar(idx);
    }
    ConjNodeRef createVar(int idx, const Hyperedge& edge) override {
        registerVar(idx, edge.getProbability());
        return makeVar(idx);
    }

    ConjNodeRef makeAnd(const ConjNodeRef& a, const ConjNodeRef& b) override {
        if (!a.valid || !b.valid) return invalidRef();
        if (a.isFalse || b.isFalse) return getFalse();
        return ConjNodeRef{mergeVars(a.vars, b.vars), true, false};
    }
    ConjNodeRef makeAnd(const std::vector<ConjNodeRef>& nodes) override {
        std::vector<int> vars;
        for (const auto& node : nodes) {
            if (!node.valid) return invalidRef();
            if (node.isFalse) return getFalse();
            vars = mergeVars(vars, node.vars);
        }
        return ConjNodeRef{std::move(vars), true, false};
    }
    ConjNodeRef makeOr(const ConjNodeRef& a, const ConjNodeRef& b) override {
        invalid = true;
        return invalidRef();
    }
    ConjNodeRef makeOr(const std::vector<ConjNodeRef>& nodes) override {
        if (nodes.size() == 1) {
            return nodes[0];
        }
        invalid = true;
        return invalidRef();
    }
    ConjNodeRef makeNot(const ConjNodeRef& a) override {
        invalid = true;
        return invalidRef();
    }
    ConjNodeRef makeCondition(const ConjNodeRef& f, const std::vector<int>&,
            const std::vector<int>&) override {
        return f;
    }
    ConjNodeRef getTrue() override {
        return ConjNodeRef{{}, true, false};
    }
    ConjNodeRef getFalse() override {
        return ConjNodeRef{{}, true, true};
    }
    bool isSame(const ConjNodeRef& a, const ConjNodeRef& b) override {
        return a.valid == b.valid && a.isFalse == b.isFalse && a.vars == b.vars;
    }
    std::string toString(const ConjNodeRef& node) override {
        if (!node.valid) return "invalid";
        if (node.isFalse) return "false";
        if (node.vars.empty()) return "true";
        return "conj(" + std::to_string(node.vars.size()) + ")";
    }
    void setVariableWeight(int idx, double posWeight, double) override {
        registerVar(idx, posWeight);
    }
    double computeWeightedModelCount(const ConjNodeRef& node) override {
        if (!node.valid) return 0.0;
        if (node.isFalse) return 0.0;
        double prob = 1.0;
        for (int var : node.vars) {
            if (var < 0 || static_cast<size_t>(var) >= varProb_.size()) {
                return 0.0;
            }
            prob *= varProb_[static_cast<size_t>(var)];
        }
        return prob;
    }
    int getVarIndex(const Node& node) override {
        auto it = nodeIndex_.find(&node);
        if (it != nodeIndex_.end()) return it->second;
        int idx = nextVarIndex_++;
        nodeIndex_[&node] = idx;
        registerVar(idx, node.getProbability());
        return idx;
    }
    int getVarIndex(const Hyperedge& edge) override {
        auto it = edgeIndex_.find(&edge);
        if (it != edgeIndex_.end()) return it->second;
        int idx = nextVarIndex_++;
        edgeIndex_[&edge] = idx;
        registerVar(idx, edge.getProbability());
        return idx;
    }
    void printInfo(const ConjNodeRef&, const std::string&) override {}
    void dumpProfilingStatistics() override {}

    bool isValid() const { return !invalid; }

private:
    ConjNodeRef makeVar(int idx) {
        return ConjNodeRef{{idx}, true, false};
    }
    ConjNodeRef invalidRef() {
        return ConjNodeRef{{}, false, false};
    }
    void registerVar(int idx, double prob) {
        if (idx < 0) return;
        if (static_cast<size_t>(idx) >= varProb_.size()) {
            varProb_.resize(static_cast<size_t>(idx) + 1, 1.0);
        }
        varProb_[static_cast<size_t>(idx)] = prob;
    }
    static std::vector<int> mergeVars(const std::vector<int>& a, const std::vector<int>& b) {
        if (a.empty()) return b;
        if (b.empty()) return a;
        std::vector<int> out;
        out.reserve(a.size() + b.size());
        size_t i = 0;
        size_t j = 0;
        while (i < a.size() || j < b.size()) {
            int va = (i < a.size()) ? a[i] : std::numeric_limits<int>::max();
            int vb = (j < b.size()) ? b[j] : std::numeric_limits<int>::max();
            if (va == vb) {
                out.push_back(va);
                ++i;
                ++j;
            } else if (va < vb) {
                out.push_back(va);
                ++i;
            } else {
                out.push_back(vb);
                ++j;
            }
        }
        return out;
    }

    std::vector<double> varProb_;
    int nextVarIndex_ = 0;
    std::unordered_map<const Node*, int> nodeIndex_;
    std::unordered_map<const Hyperedge*, int> edgeIndex_;
    bool invalid = false;
};

inline bool evaluateSingleRandComponent(
        const ComponentSubgraph& comp,
        const SingleRandVarInfo& var,
        bool varValue,
        std::unordered_map<NodePtr, bool>& nodeValues,
        long long* evalMs = nullptr) {
    BoolFormulaManager manager(var.node, var.edge, varValue);
    SubgraphView subview(comp.nodes, comp.edges);
    std::map<NodePtr, BoolNodeRef> nodeFormulas;
    std::map<EdgePtr, BoolNodeRef> edgeFormulas;
    auto start = std::chrono::steady_clock::now();
    buildFormulasCyclewise(subview, manager, nodeFormulas, edgeFormulas, {}, nullptr, true, false);
    auto end = std::chrono::steady_clock::now();
    if (evalMs) {
        *evalMs = static_cast<long long>(
                std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    }
    if (!manager.isValid()) {
        return false;
    }
    nodeValues.clear();
    nodeValues.reserve(comp.nodes.size());
    for (const auto& node : comp.nodes) {
        auto it = nodeFormulas.find(node);
        if (it == nodeFormulas.end() || !it->second.get()) {
            continue;
        }
        nodeValues.emplace(node, it->second.value);
    }
    return true;
}

inline bool evaluateConjComponent(
        const ComponentSubgraph& comp,
        std::unordered_map<NodePtr, double>& nodeProbs,
        long long* evalMs = nullptr) {
    ConjFormulaManager manager;
    SubgraphView subview(comp.nodes, comp.edges);
    std::map<NodePtr, ConjNodeRef> nodeFormulas;
    std::map<EdgePtr, ConjNodeRef> edgeFormulas;
    auto start = std::chrono::steady_clock::now();
    buildFormulasCyclewise(subview, manager, nodeFormulas, edgeFormulas, {}, nullptr, true, false);
    auto end = std::chrono::steady_clock::now();
    if (evalMs) {
        *evalMs = static_cast<long long>(
                std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    }
    if (!manager.isValid()) {
        return false;
    }
    nodeProbs.clear();
    nodeProbs.reserve(comp.nodes.size());
    for (const auto& node : comp.nodes) {
        auto it = nodeFormulas.find(node);
        if (it == nodeFormulas.end() || !it->second.get()) {
            continue;
        }
        double prob = manager.computeWeightedModelCount(it->second);
        nodeProbs.emplace(node, prob);
    }
    return true;
}

template <typename ManagerT, typename FormulaRef>
struct ComponentFormulaBundle {
    size_t id;
    std::unique_ptr<ManagerT> manager;
    std::map<NodePtr, FormulaRef> nodeFormulas;
};

template <typename ManagerT, typename FormulaRef, typename ManagerFactory>
inline std::vector<ComponentFormulaBundle<ManagerT, FormulaRef>>
buildFormulasCyclewiseByComponentList(
        std::vector<ComponentSubgraph> components,
        ManagerFactory&& makeManager,
        long long* initMsTotal = nullptr,
        long long* initMsMax = nullptr) {
    std::vector<ComponentFormulaBundle<ManagerT, FormulaRef>> bundles;
    bundles.reserve(components.size());
    if (initMsTotal) *initMsTotal = 0;
    if (initMsMax) *initMsMax = 0;

    for (auto& comp : components) {
        auto compId = comp.id;
        auto nodeCount = comp.nodes.size();
        auto edgeCount = comp.edges.size();
        auto randVars = countComponentRandomVars(comp);

        SubgraphView subview(std::move(comp.nodes), std::move(comp.edges));
        auto initStart = std::chrono::steady_clock::now();
        auto manager = makeManager(subview);
        long long initMs = static_cast<long long>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - initStart)
                        .count());
        if (initMsTotal) *initMsTotal += initMs;
        if (initMsMax) *initMsMax = std::max(*initMsMax, initMs);

        auto buildStart = std::chrono::steady_clock::now();
        std::map<NodePtr, FormulaRef> nodeFormulas;
        std::map<EdgePtr, FormulaRef> edgeFormulas;
        buildFormulasCyclewise(subview, *manager, nodeFormulas, edgeFormulas);
        auto buildMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - buildStart)
                               .count();

        std::cout << "[fc-component] id=" << compId
                  << " nodes=" << nodeCount
                  << " edges=" << edgeCount
                  << " rand_vars=" << randVars
                  << " init_ms=" << initMs
                  << " build_ms=" << buildMs
                  << " total_ms=" << (initMs + buildMs)
                  << std::endl;

        bundles.push_back(ComponentFormulaBundle<ManagerT, FormulaRef>{
                compId, std::move(manager), std::move(nodeFormulas)});
    }
    return bundles;
}

template <typename ManagerT, typename FormulaRef, typename ManagerFactory>
inline std::vector<ComponentFormulaBundle<ManagerT, FormulaRef>>
buildFormulasCyclewiseByComponent(
        const DerivationGraphViewInterface& view,
        ManagerFactory&& makeManager,
        long long* initMsTotal = nullptr,
        long long* initMsMax = nullptr) {
    return buildFormulasCyclewiseByComponentList<ManagerT, FormulaRef>(
            buildComponentSubgraphs(view), std::forward<ManagerFactory>(makeManager), initMsTotal,
            initMsMax);
}

#endif //FORWARDCOMPILATION_H
