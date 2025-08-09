#ifndef FORWARDCOMPILATION_H
#define FORWARDCOMPILATION_H

#include <iostream>
#include "souffle/Derivation.h"
#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/RuleManager.h"
#include "souffle/problog/formula/FormulaManager.h"
#include "souffle/problog/formula/LogicFormulaManager.h"
#include <queue>
#include "souffle/problog/formula/CuddManager.h"
#include <queue>
#include <set>
#include <map>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <climits>
#include "souffle/problog/debug/Debugger.h"

int nextFormulaNodeId = 0;
std::unordered_map<size_t, int> nodeIdMap;
std::unordered_map<size_t, int> edgeIdMap;
Debugger& debugger = Debugger::getInstance();

int mapNodeId(size_t id) {
    if (nodeIdMap.find(id) == nodeIdMap.end()) {
        nodeIdMap[id] = nextFormulaNodeId++;
    }
    return nodeIdMap[id];
}
int mapEdgeId(size_t id) {
    if (edgeIdMap.find(id) == edgeIdMap.end()) {
        edgeIdMap[id] = nextFormulaNodeId++;
    }
    return edgeIdMap[id];
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

    // Initialize formulas for input facts (nodes)
    std::map<NodePtr, FormulaNodeRef> baseNodeFormulas;
    std::map<EdgePtr, FormulaNodeRef> baseEdgeFormulas;
    for (const auto& node : view.getNodes()) {
        // Create a variable using the node's unique ID
        if (node->isFact) {
            if (node->getProbability() == 1.0) {
//                nodeFormulas[node] = formulaManager.getTrue();
                nodeFormulas[node] = formulaManager.createVar(mapNodeId(node->getId()), *node);
            } else {
                nodeFormulas[node] = formulaManager.createVar(mapNodeId(node->getId()), *node);
            }
            baseNodeFormulas.insert({node, nodeFormulas[node]});
            formulaManager.setVariableWeight(mapNodeId(node->getId()), node->getProbability(), 1-node->getProbability());
        }
    }

    // Initialize formulas for rule instantiations (hyperedges)
    for (const auto& edge : view.getEdges()) {
        // Create a variable using the edge's unique ID; need to plus the size of graph.getNodes() to avoid conflict with node's id
        if (edge->getRule()->isDeterminstic()) {
            auto baseEdgeFormula = formulaManager.getTrue();
            baseEdgeFormulas.insert({edge, baseEdgeFormula});
        } else {
            auto baseEdgeFormula = formulaManager.createVar(mapEdgeId(edge->getId()), *edge);
            formulaManager.setVariableWeight(mapEdgeId(edge->getId()), edge->getRule()->getProbability(), 1-edge->getRule()->getProbability());
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
        std::vector<FormulaNodeRef> inputFormulas;
        // Add the rule formula (the edge's base formula)
        inputFormulas.push_back(baseEdgeFormulas[edge]);

        // Add the input node formulas
        bool allInputsAvailable = true;
        assert (view.getInputs(edge).size() == view.getBodyNegations(edge).size());
        assert (view.getInputs(edge).size() == edge->getInputs().size());
        for (size_t i = 0; i < view.getInputs(edge).size(); i++) {
            auto input = view.getInputs(edge)[i];
            auto isNegated = view.getBodyNegations(edge)[i];
            auto it = nodeFormulas.find(input);
            if (it != nodeFormulas.end()) {
                if (!isNegated) {
                    inputFormulas.push_back(it->second);
                } else {
                    inputFormulas.push_back(formulaManager.makeNot(it->second));
                }
            } else {
                // Input node formula not available yet, skip this edge for now
                // cannot skip, since there is cycle
                allInputsAvailable = false;
//                std::cout << "Input node formula not available yet: " << input->getTuple().toString() << std::endl;
                break;
            }
        }

        if (!allInputsAvailable) {
            // Put the edge back in the worklist for later processing
            worklist.push(edge);
            inWorklist.insert(edge);
            continue;
        }

        // Compute the conjunction of all input formulas
        FormulaNodeRef newEdgeFormula;
        if (inputFormulas.size() == 1) {
            newEdgeFormula = inputFormulas[0];
        } else {
            newEdgeFormula = formulaManager.makeAnd(inputFormulas);
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

            // Compute the disjunction of all incoming edge formulas
            FormulaNodeRef newNodeFormula;
            if (incomingFormulas.empty()) {
                assert (false && "No incoming edge formula found for the output node");
            } else if (incomingFormulas.size() == 1) {
                newNodeFormula = incomingFormulas[0];
            } else {
                newNodeFormula = formulaManager.makeOr(incomingFormulas);
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
            return priority > other.priority;  // 小值优先
        return sequence_id > other.sequence_id;  // 小编号优先er depth = higher priority
    }
};

template<typename FormulaNodeRef>
void buildFormulasCyclewise(
    const SubgraphView& view,
    FormulaManager<FormulaNodeRef>& formulaManager,
    std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
    std::map<EdgePtr, FormulaNodeRef>& edgeFormulas
) {
     FunctionTimer timer("Build Formulas Cyclewise using DAG + Depth");

    CycleDependencyGraph depGraph(view);
//    depGraph.dumpCycles(std::cout);
    depGraph.dumpDot("scc.dot");

    std::map<NodePtr, FormulaNodeRef> baseNodeFormulas;
    std::map<EdgePtr, FormulaNodeRef> baseEdgeFormulas;

    // 1. 初始化公式
    for (const auto& node : view.getNodes()) {
        if (node->isFact) {
            FormulaNodeRef var = (node->getProbability() == 1.0)
//                ? formulaManager.getTrue()
                ? formulaManager.createVar(mapNodeId(node->getId()), *node)
                : formulaManager.createVar(mapNodeId(node->getId()), *node);
            formulaManager.setVariableWeight(mapNodeId(node->getId()), node->getProbability(), 1 - node->getProbability());
            nodeFormulas[node] = var;
            baseNodeFormulas[node] = var;
        }
    }

    for (const auto& edge : view.getEdges()) {
        FormulaNodeRef f = edge->getRule()->isDeterminstic()
            ? formulaManager.getTrue()
            : formulaManager.createVar(mapEdgeId(edge->getId()), *edge);
        if (!edge->getRule()->isDeterminstic()) {
            formulaManager.setVariableWeight(mapEdgeId(edge->getId()), edge->getProbability(), 1 - edge->getProbability());
        }
        baseEdgeFormulas[edge] = f;
    }

    // 2. 调度 SCC
    std::vector<size_t> remainingInDegrees = depGraph.inDegrees;
    std::vector<bool> visited(depGraph.nodeCycles.size(), false);
    std::queue<size_t> ready;
    for (size_t i = 0; i < remainingInDegrees.size(); ++i) {
        if (remainingInDegrees[i] == 0)
            ready.push(i);
    }

    int iteration = 0;
    while (!ready.empty()) {
        size_t cid = ready.front(); ready.pop();
        if (visited[cid]) continue;
        visited[cid] = true;

        const auto& cycleEdges = depGraph.edgeCycles[cid];
        std::priority_queue<PrioritizedEdge> worklist;
        std::set<EdgePtr> inWorklist;
//        std::cout << "Processing cycle " << cid << ", edges: ";

        int _seqId = 0;
        for (auto edge : cycleEdges) {
            worklist.push({edge, depGraph.edgeDepthsGlobal[edge], _seqId++});
//            std::cout << "Adding edge " << edge->getId() << " " << edge->toString()
//                      << " with depth " << depGraph.edgeDepthsGlobal[edge] << " to worklist.\n";
            inWorklist.insert(edge);
        }

        while (!worklist.empty()) {
            iteration++;
//            std::cout << "Iteration: " << iteration << std::endl;
//            std::cout << "Processing cycle " << cid << ", worklist size: " << worklist.size() << std::endl;
//            formulaManager.dumpProfilingStatistics();
            EdgePtr edge = worklist.top().edge;
            size_t depth = worklist.top().priority;
            worklist.pop();
            inWorklist.erase(edge);
//            std::cout << "Processing edge " << edge->getId() << " " << edge->toString() << std::endl;
//            std::cout << "Edge depth: " << depth << std::endl;
            std::vector<FormulaNodeRef> inputs = { baseEdgeFormulas[edge] };
            bool allAvailable = true;

            for (size_t i = 0; i < view.getInputs(edge).size(); ++i) {
                NodePtr input = view.getInputs(edge)[i];
                if (!nodeFormulas.count(input)) {
//                    std::cout << "Input node formula not available: " << input->getId() << " " << input->toString() << std::endl;
                    allAvailable = false;
                    break;
                }
                inputs.push_back(view.getBodyNegations(edge)[i]
                    ? formulaManager.makeNot(nodeFormulas[input])
                    : nodeFormulas[input]);
            }

            if (!allAvailable) {
//                std::cout << "Not all inputs available for edge " << edge->getId() << ", re-adding to worklist.\n";
                worklist.push({edge, depGraph.edgeDepthsGlobal[edge], _seqId++});
                inWorklist.insert(edge);
                continue;
            }

            FormulaNodeRef newEdgeF = (inputs.size() == 1) ? inputs[0] : formulaManager.makeAnd(inputs);
            if (!formulaManager.isSame(edgeFormulas[edge], newEdgeF)) {
                edgeFormulas[edge] = newEdgeF;
                NodePtr out = view.getOutput(edge);

                std::vector<FormulaNodeRef> inFs;
                for (auto& inEdge : view.getIncomingEdges(out)) {
                    auto it = edgeFormulas.find(inEdge);
                    if (it != edgeFormulas.end() && it->second.get()) {
                        inFs.push_back(it->second);
                    }
                }

                if (!inFs.empty()) {
                    FormulaNodeRef newNodeF = (inFs.size() == 1) ? inFs[0] : formulaManager.makeOr(inFs);
                    if (!nodeFormulas.count(out) || !formulaManager.isSame(nodeFormulas[out], newNodeF)) {
                        nodeFormulas[out] = newNodeF;

                        for (auto& outEdge : view.getOutgoingEdges(out)) {
                            auto it = depGraph.edgeToCycleIndex.find(outEdge);
                            if (it != depGraph.edgeToCycleIndex.end() && it->second == cid && !inWorklist.count(outEdge)) {
                                worklist.push({outEdge, depGraph.edgeDepthsGlobal[outEdge], _seqId++});
                                inWorklist.insert(outEdge);
                            }
                        }
                    }
                }
            }
        }

        for (auto succ : depGraph.reverseDependencies[cid]) {
            if (--remainingInDegrees[succ] == 0) {
                ready.push(succ);
            }
        }
    }
    formulaManager.dumpProfilingStatistics();
//    std::cout << "✅ buildFormulasCyclewiseNew completed using global depth info.\n";
}

template<typename FormulaNodeRef>
void buildFormulasInc(
    const IncrementalDerivationGraphViewInterface& view,
    FormulaManager<FormulaNodeRef>& formulaManager,
    std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
    std::map<EdgePtr, FormulaNodeRef>& edgeFormulas
) {
    auto* stage = debugger.startStage(StageKind::FORWARD_COMPILATION_INC);
//    FunctionTimer timer("forward compilation, incremental update");
    auto& deltaInsertedEdges = view.getDeltaInsertEdges();
    auto& deltaDeletedEdges = view.getDeltaDeleteEdges();
    auto& deltaInsertedNodes = view.getDeltaInsertNodes();
    auto& deltaDeletedNodes = view.getDeltaDeleteNodes();

    if (deltaInsertedEdges.empty() && deltaDeletedEdges.empty()) {
        stage->logMessage(Level::INFO, "No changes to apply, skipping incremental update\n");
        return;
    }

    // deletion
    if (deltaDeletedEdges.empty()) {
        stage->logMessage(Level::INFO, "No deleted edges, skipping deletion phase\n");
    } else {
        stage->logMessage(Level::INFO, "Processing deleted edges");
        std::deque<EdgePtr> worklist;
        stage->logMessage(Level::INFO, "Change all impacted formulas to False");
        // over-delete all formulas that are impacted by the deleted edges
        {
            std::deque<EdgePtr> que(deltaDeletedEdges.begin(), deltaDeletedEdges.end());
            std::set<NodePtr> nodes;
            while (!que.empty()) {
                auto edge = que.front();
                que.pop_front();
                edgeFormulas[edge] = formulaManager.getFalse();
                if (!(deltaDeletedEdges.count(edge))) {
                    worklist.push_back(edge);  // worklist only contains impacted but not deleted edges
                }
                auto outNode = view.getOutput(edge);
                if (outNode == nullptr) continue;  // is not deleted
                nodes.insert(outNode);
                nodeFormulas[outNode] = formulaManager.getFalse();
                for (auto outEdge: view.getOutgoingEdges(outNode)) {
                    if (edgeFormulas.count(outEdge) == 0 || formulaManager.isSame(edgeFormulas[outEdge], formulaManager.getFalse())) {
                        continue;
                    }
                    que.push_back(outEdge);
                }
            }
            // for all impacted but not deleted nodes, update their formulas
            for (auto node : nodes) {
                std::vector<FormulaNodeRef> incoming;
                for (auto e : view.getIncomingEdges(node)) {
                    auto it = edgeFormulas.find(e);
                    if (it != edgeFormulas.end() && it->second.get()) {
                        incoming.push_back(it->second);
                    }
                }
                assert (!incoming.empty());
                auto newNodeFormula = formulaManager.makeOr(incoming);
                nodeFormulas[node] = newNodeFormula;
            }
        }

        for (auto node : view.getDeltaDeleteNodes()) {
            nodeFormulas.erase(node);
        }
        for (auto edge : view.getDeltaDeleteEdges()) {
            edgeFormulas.erase(edge);
        }

        // TODO: can be optimized - not over-delete to False in previous step
        // TODO: re-derivation phrase
        while (!worklist.empty()) {
            auto* iteration = debugger.startIteration();
//            formulaManager.dumpProfilingStatistics();

            EdgePtr edge = worklist.front();
            worklist.pop_front();
            iteration->setInfo("edge_id", std::to_string(edge->getId()));
            iteration->setInfo("edge", edge->toString());

            // if its a deleted edge
            if (deltaDeletedEdges.count(edge)) {
                assert (false && "Deleted edge should not be in the worklist");
            }
            // a propagated edge / an unknown normal edge
            // now the edge should have its old formula (not deleted)
            FormulaNodeRef oldEdgeFormula = edgeFormulas[edge];
            // calculate the new formula
            FormulaNodeRef baseFormula = edge->getRule()->isDeterminstic()
                ? formulaManager.getTrue()
                : formulaManager.createVar(mapEdgeId(edge->getId()), *edge);
            std::vector<FormulaNodeRef> inputFormulas = {baseFormula};
            const auto& inputs = view.getInputs(edge);
            const auto& negs = view.getBodyNegations(edge);

            bool allInputsAvailable = true;
            for (size_t i = 0; i < inputs.size(); ++i) {
                auto it = nodeFormulas.find(inputs[i]);
                if (it == nodeFormulas.end()) {
                    worklist.push_back(edge);
                    allInputsAvailable = false;
                    break;
                } else
                    inputFormulas.push_back(negs[i] ? formulaManager.makeNot(it->second) : it->second);
            }
            if (!allInputsAvailable) {
                // put the edge back in the worklist for later processing
                iteration->logMessage(Level::INFO, "Input node formula not available yet, re-adding edge to worklist");
                continue;
            }

            FormulaNodeRef newEdgeFormula = formulaManager.makeAnd(inputFormulas);
            // check if the edge formula actually changed
            if (formulaManager.isSame(oldEdgeFormula, newEdgeFormula)) {
                iteration->logMessage(Level::INFO, "Edge formula did not change, skipping edge");
                continue;
            }
            edgeFormulas[edge] = newEdgeFormula;

            NodePtr output = edge->getOutput();
            assert (output->isFact == false);
            assert (deltaDeletedNodes.count(output) == 0);

            FormulaNodeRef oldNode = nodeFormulas[output];

            std::vector<FormulaNodeRef> incoming;
            for (auto e : view.getIncomingEdges(output)) {
                auto it = edgeFormulas.find(e);
                if (it != edgeFormulas.end() && it->second.get()) {
                    incoming.push_back(it->second);
                }
            }
            FormulaNodeRef newNode = formulaManager.makeOr(incoming);
            assert (!formulaManager.isSame(newNode, formulaManager.getFalse()));

            if (!formulaManager.isSame(oldNode, newNode)) {
                iteration->logMessage(Level::INFO, "Node formula changed, updating node");
                nodeFormulas[output] = newNode;
                for (auto outEdge : view.getOutgoingEdges(output)) {
                    if (!view.getDeltaDeleteEdges().count(outEdge)) {
                        worklist.push_back(outEdge);
                    }
                }
            } else {
                iteration->logMessage(Level::INFO, "Node formula did not change, skipping node");
            }
            debugger.endIteration();
        }
    }
    // === 插入阶段 ===
    if (deltaInsertedEdges.empty()) {
        stage->logMessage(Level::INFO, "No inserted edges, skipping insertion phase\n");
        return;
    }
    stage->logMessage(Level::INFO, "Processing inserted edges\n");
    std::deque<EdgePtr> worklist;
    worklist.assign(deltaInsertedEdges.begin(), deltaInsertedEdges.end());
    for (auto node : deltaInsertedNodes) {
        if (node->isFact) {
            nodeFormulas[node] = (node->getProbability() == 1.0)
//                ? formulaManager.getTrue()
                ? formulaManager.createVar(mapNodeId(node->getId()), *node)
                : formulaManager.createVar(mapNodeId(node->getId()), *node);
            if (node->getProbability() != 1.0)
                formulaManager.setVariableWeight(mapNodeId(node->getId()), node->getProbability(), 1 - node->getProbability());
        } else {
            nodeFormulas[node] = formulaManager.getFalse();
        }
    }
    for (auto edge : deltaInsertedEdges) {
        if (edge->getRule()->isDeterminstic()) {
            edgeFormulas[edge] = formulaManager.getFalse();
        } else {
            edgeFormulas[edge] = formulaManager.getFalse();
            formulaManager.createVar(mapEdgeId(edge->getId()), *edge);
            formulaManager.setVariableWeight(mapEdgeId(edge->getId()), edge->getRule()->getProbability(), 1 - edge->getRule()->getProbability());
        }
    }

    debugger.logMessage(Level::INFO, "Starting incremental update for inserted edges");

    while (!worklist.empty()) {
//        std::cout << "  Iteration: " << ++iteration
//                  << ", Worklist size: " << worklist.size() << std::endl;
        auto* iteration = debugger.startIteration();
//        formulaManager.dumpProfilingStatistics();

        EdgePtr edge = worklist.front();
        worklist.pop_front();
        FormulaNodeRef oldEdge = edgeFormulas[edge];
        // calculate the new formula
        FormulaNodeRef baseFormula = edge->getRule()->isDeterminstic()
            ? formulaManager.getTrue()
            : formulaManager.createVar(mapEdgeId(edge->getId()), *edge);
        std::vector<FormulaNodeRef> inputFormulas = {baseFormula};
        const auto& inputs = view.getInputs(edge);
        const auto& negs = view.getBodyNegations(edge);
        bool allInputsAvailable = true;
        for (size_t i = 0; i < inputs.size(); ++i) {
            auto it = nodeFormulas.find(inputs[i]);
            if (it == nodeFormulas.end()) {
//                std::cout << "Input node formula not available yet: " << inputs[i]->getTuple().toString() << std::endl;
                allInputsAvailable = false;
                worklist.push_back(edge);
                break;
            }
            inputFormulas.push_back(negs[i] ? formulaManager.makeNot(it->second) : it->second);
        }
        if (!allInputsAvailable) {
            // put the edge back in the worklist for later processing
            continue;
        }
        FormulaNodeRef newEdge = formulaManager.makeAnd(inputFormulas);

        if (!formulaManager.isSame(oldEdge, newEdge)) {
//            std::cout << "oldEdge: " << formulaManager.toString(oldEdge) << std::endl;
//            std::cout << "newEdge: " << formulaManager.toString(newEdge) << std::endl;
            edgeFormulas[edge] = newEdge;
            NodePtr output = view.getOutput(edge);
            assert (output->isFact == false);
//            assert (deltaInsertedNodes.count(output) == 0);
            FormulaNodeRef oldNode = nodeFormulas[output];
            std::vector<FormulaNodeRef> incoming;
            for (auto e : view.getIncomingEdges(output)) {
                auto it = edgeFormulas.find(e);
                if (it != edgeFormulas.end() && it->second.get()) {
                    incoming.push_back(it->second);
                }
            }
            FormulaNodeRef newNode = formulaManager.makeOr(incoming);

            if (!formulaManager.isSame(oldNode, newNode)) {
                nodeFormulas[output] = newNode;
                for (auto outEdge : view.getOutgoingEdges(output)) {
                    worklist.push_back(outEdge);
                }
            }
        }
    }
    debugger.endStage();
}

// TODO: 目前每个worklist内没有按照depth排序
template<typename FormulaNodeRef>
void buildFormulasIncCyclewise(
    const IncrementalDerivationGraphViewInterface& view,
    FormulaManager<FormulaNodeRef>& formulaManager,
    std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
    std::map<EdgePtr, FormulaNodeRef>& edgeFormulas,
    std::set<NodePtr>& changedNodes
) {
//    formulaManager.stopDynamicOptimization();
//    FunctionTimer timer("forward compilation, incremental update (cyclewise)");
//    auto* stage = debugger.startStage(StageKind::FORWARD_COMPILATION_INC);
    CycleDependencyGraph depGraph(view);  // 包括 computeSCCs, computeDependencies, computeDepths

    const auto& deltaInsertedEdges = view.getDeltaInsertEdges();
    const auto& deltaDeletedEdges = view.getDeltaDeleteEdges();
    const auto& deltaInsertedNodes = view.getDeltaInsertNodes();
    const auto& deltaDeletedNodes = view.getDeltaDeleteNodes();

    if (deltaInsertedEdges.empty() && deltaDeletedEdges.empty()) {
        debugger.logMessage(Level::INFO, "No changes to apply, skipping incremental update");
        return;
    }
    debugger.logMessage(Level::INFO, "Starting incremental update for deleted edges");
    std::map<size_t, std::deque<EdgePtr>> cycleWorklists;
//    std::set<EdgePtr> inWorklist;

    /// just realize that we do not need to do over-deletion
    /// we just need to apply neg to all impacted nodes and edges
    debugger.logMessage(Level::INFO, "Performing deletion");
    {
        // for each deleted node, apply its neg to all its reachable edges and nodes
        // however, since we still want the optimizations for deterministic facts, we sperate them
        std::set<NodePtr> deletedFacts;
//        std::set<NodePtr> deletedDeterminsticFacts;
        for (auto node : deltaDeletedNodes) {
//            std::cout << "Processing deleted node: " << node->toString() << std::endl;
            if (node->isFact) {
                deletedFacts.insert(node);
//                std::cout << "Deleted fact: " << node->toString() << " have probability " << node->getProbability() << std::endl;
//                if (node->getProbability() == 1.0) {
//                    deletedDeterminsticFacts.insert(node);
//                }
                formulaManager.setVariableWeight(mapNodeId(node->getId()), 0.0, 1.0);  // set weight to 0
            }
            changedNodes.insert(node);
        }
//        std::cout << "Deleted facts size: " << deletedFacts.size() << std::endl;
//        std::cout << "Determinstic deleted facts size: " << deletedDeterminsticFacts.size() << std::endl;

        auto& nodeImpactedByDeltaDelete = view.getNodeImpactedByDeltaDelete();
        auto& edgeImpactedByDeltaDelete = view.getEdgeImpactedByDeltaDelete();
        // TODO: update via reachability information
        for (const auto& [deletedFact, impactedNodes]: nodeImpactedByDeltaDelete) {
            for (auto node: impactedNodes) {
                if (deltaDeletedNodes.count(node)) {
                    continue;  // skip deleted nodes
                }
                if (nodeFormulas.count(node) == 0 || formulaManager.isSame(nodeFormulas[node], formulaManager.getFalse())) {
                    continue;  // no need to update
                }
                auto newNodeFormula = formulaManager.makeCondition(nodeFormulas[node], {}, {mapNodeId(deletedFact->getId())});
                if (!formulaManager.isSame(nodeFormulas[node], newNodeFormula)) {
                    nodeFormulas[node] = newNodeFormula;
                    changedNodes.insert(node);
                }
            }
        }
        for (const auto& [deletedFact, impactedEdges]: edgeImpactedByDeltaDelete) {
            for (auto edge: impactedEdges) {
                if (deltaDeletedEdges.count(edge)) {
                    continue;  // skip deleted edges
                }
                edgeFormulas[edge] = formulaManager.makeCondition(edgeFormulas[edge], {}, {mapNodeId(deletedFact->getId())});
            }
        }
        // update deleted derived nodes formulas
        for (auto node: deltaDeletedNodes) {
            nodeFormulas.erase(node);
        }
        // update deleted edges formulas
        for (auto edge: deltaDeletedEdges) {
            edgeFormulas.erase(edge);
        }
        // update the variable ordering for deleted facts
        std::set<int> deletedFactsIndex;
        for (auto node: deletedFacts) {
            auto index = mapNodeId(node->getId());
            deletedFactsIndex.insert(index);
        }


        formulaManager.dumpProfilingStatistics();
        if (!deletedFactsIndex.empty()) {
            formulaManager.postprocessUselessVariables(deletedFactsIndex);
            formulaManager.dumpProfilingStatistics();
        }
    }


    std::vector<size_t> inDegree = depGraph.inDegrees;
    std::queue<size_t> ready;  // cycles with in-degree 0
    std::vector<bool> scheduled(depGraph.nodeCycles.size(), false);  // whether the cycle has been scheduled for insertion phase

    // === 插入阶段 ===
    std::cout << "Processing inserted edges" << std::endl;
    debugger.logMessage(Level::INFO, "Processing inserted edges");
    // initialized formulas for newly inserted nodes and edges
    for (auto node : deltaInsertedNodes) {
        if (node->isFact) {
            nodeFormulas[node] = node->getProbability() == 1.0
                ? formulaManager.createVar(mapNodeId(node->getId()), *node)//; formulaManager.getTrue()
                : formulaManager.createVar(mapNodeId(node->getId()), *node);
            formulaManager.setVariableWeight(mapNodeId(node->getId()), node->getProbability(), 1 - node->getProbability());
        } else {
            nodeFormulas[node] = formulaManager.getFalse();
        }
        changedNodes.insert(node);
    }

    for (auto edge : deltaInsertedEdges) {
        edgeFormulas[edge] = formulaManager.getFalse();
        if (!edge->getRule()->isDeterminstic()) {
            formulaManager.createVar(mapEdgeId(edge->getId()), *edge);
            formulaManager.setVariableWeight(mapEdgeId(edge->getId()), edge->getRule()->getProbability(), 1 - edge->getRule()->getProbability());
        }
        assert (depGraph.edgeToCycleIndex.count(edge));
        size_t cid = depGraph.edgeToCycleIndex.at(edge);
        cycleWorklists[cid].push_back(edge);
    }

    inDegree = depGraph.inDegrees;
    std::fill(scheduled.begin(), scheduled.end(), false);

    assert (ready.empty());  // should be empty after deletion phase

    for (size_t cid = 0; cid < depGraph.nodeCycles.size(); ++cid)
        if (inDegree[cid] == 0) {ready.push(cid); scheduled[cid] = true;}  // schedule cycles with in-degree 0

    while (!ready.empty()) {
        size_t cid = ready.front(); ready.pop();
        scheduled[cid] = true;
        auto& worklist = cycleWorklists[cid];
        // note that this worklist only contains impacted nodes; however,
//        std::cout << "[CYCLE " << cid << "] Begin Insertion Phase, worklist size: " << worklist.size() << std::endl;
        int round = 0;
        while (!worklist.empty()) {
            auto* iteration = debugger.startIteration();
            EdgePtr edge = worklist.front(); worklist.pop_front();
//            formulaManager.dumpProfilingStatistics();
            round++;
//            std::cout << "  [INSERTION ROUND " << round << "] Cycle " << cid
//                      << ", Worklist size: " << worklist.size() << std::endl;
//            std::cout << "  [EVAL] Edge " << edge->getId() << ": " << edge->toString() << std::endl;

            FormulaNodeRef baseFormula = edge->getRule()->isDeterminstic()
                ? formulaManager.getTrue()
                : formulaManager.createVar(mapEdgeId(edge->getId()), *edge);
            std::vector<FormulaNodeRef> inputFormulas{baseFormula};

            const auto& inputs = view.getInputs(edge);
            const auto& negs = view.getBodyNegations(edge);
            bool allAvailable = true;
            for (size_t i = 0; i < inputs.size(); ++i) {
                auto it = nodeFormulas.find(inputs[i]);
                if (it == nodeFormulas.end()) {
//                    std::cout << "    [WAIT] Missing input: " << inputs[i]->toString() << std::endl;
                    allAvailable = false;
                    break;
                }
                inputFormulas.push_back(negs[i] ? formulaManager.makeNot(it->second) : it->second);
            }

            if (!allAvailable) {
                worklist.push_back(edge);
                debugger.endIteration();
                continue;
            }

            FormulaNodeRef newEdge = formulaManager.makeAnd(inputFormulas);
            if (formulaManager.isSame(edgeFormulas[edge], newEdge)) {
//                std::cout << "    [SKIP] No change\n";
                debugger.endIteration();
                continue;
            }

//            std::cout << "    [CHANGE] Edge formula changed\n";
            edgeFormulas[edge] = newEdge;

            NodePtr output = view.getOutput(edge);
            if (!output || output->isFact) {
                debugger.endIteration();
                continue;
            }

            std::vector<FormulaNodeRef> incoming;
            for (EdgePtr e : view.getIncomingEdges(output)) {
                if (edgeFormulas.count(e) && edgeFormulas[e].get()) {
                    incoming.push_back(edgeFormulas[e]);
                }
            }

            FormulaNodeRef newNode = formulaManager.makeOr(incoming);
            if (!formulaManager.isSame(nodeFormulas[output], newNode)) {
//                std::cout << "    [UPDATE] Node formula changed: " << output->toString() << std::endl;
                nodeFormulas[output] = newNode;
                changedNodes.insert(output);
                for (EdgePtr outEdge : view.getOutgoingEdges(output)) {
                    assert (depGraph.edgeToCycleIndex.count(outEdge));
                    size_t outCid = depGraph.edgeToCycleIndex.at(outEdge);
                    cycleWorklists[outCid].push_back(outEdge);
                }
            }
        }

        for (size_t succ : depGraph.reverseDependencies[cid]) {
            if (--inDegree[succ] == 0 && !scheduled[succ]) {
                ready.push(succ);
            }
        }
        debugger.endIteration();
    }
//    debugger.endStage();
    formulaManager.dumpProfilingStatistics();
}

#endif //FORWARDCOMPILATION_H
