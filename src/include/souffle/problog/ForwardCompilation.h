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

int nextFormulaNodeId = 0;
std::unordered_map<size_t, int> nodeIdMap;
std::unordered_map<size_t, int> edgeIdMap;

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
            std::cout << "Creating Fact Node " << node->getId() << " " << node->toString() << std::endl;
            if (node->getProbability() == 1.0) {
                nodeFormulas[node] = formulaManager.getTrue();
            } else {
                nodeFormulas[node] = formulaManager.createVar(mapNodeId(node->getId()), *node);
            }
            baseNodeFormulas.insert({node, nodeFormulas[node]});
            formulaManager.setVariableWeight(mapNodeId(node->getId()), node->getProbability(), 1-node->getProbability());
            std::cout << "Setting weight for node " << node->toString() << " with probability " << node->getProbability() << std::endl;
        }
//            std::cout << "Setting weight for node " << node->getId() << " with probability " << node->getProbability() << std::endl;
    }
//    std::cout << "Weight set for nodes" << std::endl;

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
//    std::cout << "Initialize formulas for rule instantiations (hyperedges)" << std::endl;

    // Worklist algorithm
    std::queue<EdgePtr> worklist;
    std::set<EdgePtr> inWorklist; // Track edges in the worklist to avoid duplicates

    // Initialize worklist with all edges
    for (const auto& edge : view.getEdges()) {
        worklist.push(edge);
        inWorklist.insert(edge);
    }
//    std::cout << "Initialize worklist with all edges" << std::endl;
    long long iteration = 0;
    while (!worklist.empty()) {
//        if (iteration % 1000 == 0) {
            std::cout << "Iteration: " << iteration << std::endl;
            std::cout << "Worklist size: " << worklist.size() << std::endl;
            formulaManager.dumpProfilingStatistics();

//        }
        iteration++;
        auto edge = worklist.front();
        worklist.pop();
        inWorklist.erase(edge);
        std::cout << "Processing edge " << edge->getId() << " " << edge->toString() << std::endl;
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
//            std::cout << "Edge" << edge->getId() << " " << edge->toString() << " changed.\n";
//            std::cout << "Old formula: " << formulaManager.toString(oldEdgeFormula) << std::endl;
//            std::cout << "New formula: " << formulaManager.toString(newEdgeFormula) << std::endl;
            edgeFormulas[edge] = newEdgeFormula;

            // Update the output node formula
            auto output = view.getOutput(edge);
//            if (output->isFact) {
//                // Skip fact nodes
//                continue;
//            }

            // Store the old node formula to check if it changes
            FormulaNodeRef oldNodeFormula;
            bool nodeHasFormula = nodeFormulas.find(output) != nodeFormulas.end();
            if (nodeHasFormula) {
                oldNodeFormula = nodeFormulas[output];
            }

            // Collect formulas from all incoming edges
            std::vector<FormulaNodeRef> incomingFormulas;
//            std::cout << "Output node " << output->getId() << " " << output->getTuple().toString() << std::endl;
            for (const auto& inEdge : view.getIncomingEdges(output)) {

//                std::cout << "Incoming edge " << inEdge->getId() << " " << inEdge->toString() << std::endl;
                auto it = edgeFormulas.find(inEdge);
                if (it != edgeFormulas.end()) {
//                    std::cout << "Incoming edge formula: " << formulaManager.toString(it->second) << std::endl;
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
//                for (const auto& formula : incomingFormulas) {
//                    std::cout << formulaManager.toString(formula) << std::endl;
//                }
                newNodeFormula = formulaManager.makeOr(incomingFormulas);
            }

            // Check if the node formula actually changed
            bool nodeFormulaChanged = !nodeHasFormula ||
                                     !formulaManager.isSame(oldNodeFormula, newNodeFormula);

            if (nodeFormulaChanged) {
                // Update the node formula
                nodeFormulas[output] = newNodeFormula;
//                std::cout << "Node" << output->getId() << " " << output->getTuple().toString() << " changed.\n";
//                std::cout << "Node Old formula: " << formulaManager.toString(oldNodeFormula) << std::endl;
//                for (const auto& inEdge : output->getIncomingEdges()) {
//                    std::cout << "Incoming edge formula: " << formulaManager.toString(edgeFormulas[inEdge]) << std::endl;
//                }
//
//                std::cout << "Node New formula: " << formulaManager.toString(newNodeFormula) << std::endl;
//                auto prob = formulaManager.computeWeightedModelCount(newNodeFormula);
//                std::cout << "Probability: " << prob << std::endl;
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
//    for (auto& [node, formula] : nodeFormulas) {
//        std::cout << "Node " << node->getId() << " " << node->toString() << " formula: " << formulaManager.toString(formula) << std::endl;
//    }
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
    depGraph.dumpCycles(std::cout);
    depGraph.dumpDot("scc.dot");

    std::map<NodePtr, FormulaNodeRef> baseNodeFormulas;
    std::map<EdgePtr, FormulaNodeRef> baseEdgeFormulas;

    // 1. 初始化公式
    for (const auto& node : view.getNodes()) {
        if (node->isFact) {
            FormulaNodeRef var = (node->getProbability() == 1.0)
                ? formulaManager.getTrue()
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
        std::cout << "Processing cycle " << cid << ", edges: ";

        int _seqId = 0;
        for (auto edge : cycleEdges) {
            worklist.push({edge, depGraph.edgeDepthsGlobal[edge], _seqId++});
            std::cout << "Adding edge " << edge->getId() << " " << edge->toString()
                      << " with depth " << depGraph.edgeDepthsGlobal[edge] << " to worklist.\n";
            inWorklist.insert(edge);
        }

        while (!worklist.empty()) {
            iteration++;
            std::cout << "Iteration: " << iteration << std::endl;
            std::cout << "Processing cycle " << cid << ", worklist size: " << worklist.size() << std::endl;
            formulaManager.dumpProfilingStatistics();
            EdgePtr edge = worklist.top().edge;
            size_t depth = worklist.top().priority;
            worklist.pop();
            inWorklist.erase(edge);
            std::cout << "Processing edge " << edge->getId() << " " << edge->toString() << std::endl;
            std::cout << "Edge depth: " << depth << std::endl;
            std::vector<FormulaNodeRef> inputs = { baseEdgeFormulas[edge] };
            bool allAvailable = true;

            for (size_t i = 0; i < view.getInputs(edge).size(); ++i) {
                NodePtr input = view.getInputs(edge)[i];
                if (!nodeFormulas.count(input)) {
                    std::cout << "Input node formula not available: " << input->getId() << " " << input->toString() << std::endl;
                    allAvailable = false;
                    break;
                }
                inputs.push_back(view.getBodyNegations(edge)[i]
                    ? formulaManager.makeNot(nodeFormulas[input])
                    : nodeFormulas[input]);
            }

            if (!allAvailable) {
                std::cout << "Not all inputs available for edge " << edge->getId() << ", re-adding to worklist.\n";
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

    std::cout << "✅ buildFormulasCyclewiseNew completed using global depth info.\n";
}



// TODO: formula上可以增加一个change标记？或者node/edge上，表示它的formula没有变过，于是避免重复wmc.
// 不过目前的实现中有cache，所以其实就是优化了的. 先不管这件事.
//template<typename FormulaNodeRef>
//void buildFormulasInc(
//    const IncrementalDerivationGraph& graph,
//    FormulaManager<FormulaNodeRef>& formulaManager,
//    std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
//    std::map<EdgePtr, FormulaNodeRef>& edgeFormulas
//) {
//    nodeFormulas.clear();
//    edgeFormulas.clear();
//    buildFormulas(graph, formulaManager, nodeFormulas, edgeFormulas);
//}

template<typename FormulaNodeRef>
void buildFormulasInc(
    const IncrementalDerivationGraphViewInterface& view,
    FormulaManager<FormulaNodeRef>& formulaManager,
    std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
    std::map<EdgePtr, FormulaNodeRef>& edgeFormulas
) {
    FunctionTimer timer("forward compilation, incremental update");
    auto& deltaInsertedEdges = view.getDeltaInsertEdges();
    auto& deltaDeletedEdges = view.getDeltaDeleteEdges();
    auto& deltaInsertedNodes = view.getDeltaInsertNodes();
    auto& deltaDeletedNodes = view.getDeltaDeleteNodes();

    std::cout << "[Info] Processing deleted edges\n";
    if (deltaInsertedEdges.empty() && deltaDeletedEdges.empty()) {
        std::cout << "[Info] No changes to apply, skipping incremental update\n";
        return;
    }

    // deletion
    std::deque<EdgePtr> worklist;
    // over-delete all formulas that are impacted by the deleted edges
    {
        // change all impacted edges formula to false
        // update all impacted nodes formula
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
    size_t iteration = 0;
    std::cout << "[Info] Processing deleted edges\n";
    while (!worklist.empty()) {
        std::cout << "  Iteration: " << ++iteration
                  << ", Worklist size: " << worklist.size() << std::endl;
        formulaManager.dumpProfilingStatistics();

        EdgePtr edge = worklist.front();
        worklist.pop_front();
        std::cout << "Processing edge " << edge->getId() << " " << edge->toString() << std::endl;
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
            continue;
        }

        FormulaNodeRef newEdgeFormula = formulaManager.makeAnd(inputFormulas);

        // check if the edge formula actually changed
        if (formulaManager.isSame(oldEdgeFormula, newEdgeFormula)) continue;
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
            nodeFormulas[output] = newNode;
            for (auto outEdge : view.getOutgoingEdges(output)) {
                if (!view.getDeltaDeleteEdges().count(outEdge)) {
                    worklist.push_back(outEdge);
                }
            }
        }
    }

    // === 插入阶段 ===
    std::cout << "[Info] Processing inserted edges\n";
    worklist.assign(deltaInsertedEdges.begin(), deltaInsertedEdges.end());
    for (auto node : deltaInsertedNodes) {
        if (node->isFact) {
            nodeFormulas[node] = (node->getProbability() == 1.0)
                ? formulaManager.getTrue()
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

    std::cout << "[Info] Processing inserted edges\n";

    while (!worklist.empty()) {
        std::cout << "  Iteration: " << ++iteration
                  << ", Worklist size: " << worklist.size() << std::endl;

        formulaManager.dumpProfilingStatistics();

        EdgePtr edge = worklist.front();
        worklist.pop_front();
//        std::cout << "Processing edge " << edge->getId() << " " << edge->toString() << std::endl;
//        if (deltaInsertedEdges.count(edge)) {
//            // the edge is inserted, so we can skip it
//            continue;
//        }
        FormulaNodeRef oldEdge = edgeFormulas[edge];
        // calculate the new formula
        FormulaNodeRef baseFormula = edge->getRule()->isDeterminstic()
            ? formulaManager.getTrue()
            : formulaManager.createVar(mapEdgeId(edge->getId()), *edge);
        std::vector<FormulaNodeRef> inputFormulas = {baseFormula};
        const auto& inputs = view.getInputs(edge);
        const auto& negs = view.getBodyNegations(edge);
//        for (size_t i = 0; i < inputs.size(); ++i) {
//            std::cout << "Input " << i << ": " << inputs[i]->getId() << " " << inputs[i]->toString() << std::endl;
//            std::cout << "Negation " << i << ": " << negs[i] << std::endl;
//        }
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



    std::cout << "[Info] Incremental formula update completed successfully.\n";
}

// TODO: 目前每个worklist内没有按照depth排序
template<typename FormulaNodeRef>
void buildFormulasIncCyclewise(
    const IncrementalDerivationGraphViewInterface& view,
    FormulaManager<FormulaNodeRef>& formulaManager,
    std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
    std::map<EdgePtr, FormulaNodeRef>& edgeFormulas
) {
    FunctionTimer timer("forward compilation, incremental update (cyclewise)");

    CycleDependencyGraph depGraph(view);  // 包括 computeSCCs, computeDependencies, computeDepths

    const auto& deltaInsertedEdges = view.getDeltaInsertEdges();
    const auto& deltaDeletedEdges = view.getDeltaDeleteEdges();
    const auto& deltaInsertedNodes = view.getDeltaInsertNodes();
    const auto& deltaDeletedNodes = view.getDeltaDeleteNodes();

    std::cout << "[Info] Processing deleted edges\n";
    if (deltaInsertedEdges.empty() && deltaDeletedEdges.empty()) {
        std::cout << "[Info] No changes to apply, skipping incremental update\n";
        return;
    }
    std::map<size_t, std::deque<EdgePtr>> cycleWorklists;
//    std::set<EdgePtr> inWorklist;

    std::cout << "[Info] Performing overdeletion...\n";
    // over-delete all formulas that are impacted by the deleted edges
    {
        std::deque<EdgePtr> que(deltaDeletedEdges.begin(), deltaDeletedEdges.end());
        std::set<NodePtr> affectedNodes;

        while (!que.empty()) {
            EdgePtr edge = que.front(); que.pop_front();
            edgeFormulas[edge] = formulaManager.getFalse();
            if (!(deltaDeletedEdges.count(edge))) {
                size_t cid = depGraph.edgeToCycleIndex.at(edge);
                cycleWorklists[cid].push_back(edge);
                std::cout << "Edge " << edge->getId() << " is impacted by deletion, added to cycle worklist " << cid << std::endl;
            }

            NodePtr outNode = view.getOutput(edge);
            if (!outNode || deltaDeletedNodes.count(outNode)) continue;

            affectedNodes.insert(outNode);
            nodeFormulas[outNode] = formulaManager.getFalse();

            for (EdgePtr outEdge : view.getOutgoingEdges(outNode)) {
                if (deltaDeletedEdges.count(outEdge)) continue;
                if (!depGraph.edgeToCycleIndex.count(outEdge)) continue;
                if (edgeFormulas.count(outEdge) == 0) continue;
                if (formulaManager.isSame(edgeFormulas[outEdge], formulaManager.getFalse())) continue;
                std::cout << "Edge " << outEdge->getId() << " is impacted by deletion but not deleted" << std::endl;
                que.push_back(outEdge);
            }
        }

        for (NodePtr node : affectedNodes) {
            std::vector<FormulaNodeRef> incoming;
            for (EdgePtr e : view.getIncomingEdges(node)) {
                if (edgeFormulas.count(e) && edgeFormulas[e].get()) {
                    incoming.push_back(edgeFormulas[e]);
                }
            }
            assert (!incoming.empty() && "Node should have at least one incoming edge formula, or it is deleted");
            if (!incoming.empty()) {
                nodeFormulas[node] = formulaManager.makeOr(incoming);
            }
        }
    }

    for (auto n : deltaDeletedNodes) nodeFormulas.erase(n);
    for (auto e : deltaDeletedEdges) edgeFormulas.erase(e);

    std::cout << "[Info] rederive formula after over-deletions, cyclewise...\n";

    std::vector<size_t> inDegree = depGraph.inDegrees;
    std::queue<size_t> ready;  // cycles with in-degree 0
    std::vector<bool> scheduled(depGraph.nodeCycles.size(), false);  // whether the cycle has been scheduled for deletion phase

    for (size_t cid = 0; cid < depGraph.nodeCycles.size(); ++cid) {
        if (inDegree[cid] == 0) {
            ready.push(cid); scheduled[cid] = true;  // scheduled is used to avoid re-insertion to worklist
        }
    }


    while (!ready.empty()) {
        size_t cid = ready.front(); ready.pop();
        scheduled[cid] = true;
        auto& worklist = cycleWorklists[cid];

        std::cout << "[CYCLE " << cid << "] Begin Deletion Phase, worklist size: " << worklist.size() << std::endl;

        size_t round = 0;
        while (!worklist.empty()) {
            round++;
            EdgePtr edge = worklist.front(); worklist.pop_front();

            std::cout << "  [EVAL] Edge " << edge->getId() << ": " << edge->toString() << std::endl;

            if (deltaDeletedEdges.count(edge)) {
                assert (false && "Deleted edge should not be in the worklist");
            }

            FormulaNodeRef oldEdgeFormula = edgeFormulas[edge];
            FormulaNodeRef baseFormula = edge->getRule()->isDeterminstic()
                ? formulaManager.getTrue()
                : formulaManager.createVar(mapEdgeId(edge->getId()), *edge);
            std::vector<FormulaNodeRef> inputFormulas{baseFormula};

            bool allInputsAvailable = true;
            const auto& inputs = view.getInputs(edge);
            const auto& negs = view.getBodyNegations(edge);
            for (size_t i = 0; i < inputs.size(); ++i) {
                auto it = nodeFormulas.find(inputs[i]);
                if (it == nodeFormulas.end()) {
                    std::cout << "    [WAIT] Missing input node formula: " << inputs[i]->toString() << std::endl;
                    allInputsAvailable = false;
                    break;
                }
                inputFormulas.push_back(negs[i] ? formulaManager.makeNot(it->second) : it->second);
            }
            if (!allInputsAvailable) {
                worklist.push_back(edge);
                continue;
            }

            FormulaNodeRef newEdgeFormula = formulaManager.makeAnd(inputFormulas);
            if (formulaManager.isSame(oldEdgeFormula, newEdgeFormula)) {
                std::cout << "    [SKIP] No change in edge formula\n";
                continue;
            }

            std::cout << "    [CHANGE] Edge formula updated\n";
            edgeFormulas[edge] = newEdgeFormula;

            NodePtr output = view.getOutput(edge);
            if (!output || output->isFact || deltaDeletedNodes.count(output)) continue;

            FormulaNodeRef oldNodeFormula = nodeFormulas[output];
            std::vector<FormulaNodeRef> incoming;
            for (EdgePtr e : view.getIncomingEdges(output)) {
                if (edgeFormulas.count(e) && edgeFormulas[e].get()) {
                    incoming.push_back(edgeFormulas[e]);
                }
            }

            FormulaNodeRef newNodeFormula = formulaManager.makeOr(incoming);
            if (!formulaManager.isSame(oldNodeFormula, newNodeFormula)) {
                std::cout << "    [UPDATE] Node formula changed: " << output->toString() << std::endl;
                nodeFormulas[output] = newNodeFormula;
                for (EdgePtr outEdge : view.getOutgoingEdges(output)) {
                    if (deltaDeletedEdges.count(outEdge)) continue;
                    if (!depGraph.edgeToCycleIndex.count(outEdge)) continue;
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
    }

    // === 插入阶段 ===
    std::cout << "[Info] Processing insertions...\n";

    // initialized formulas for newly inserted nodes and edges
    for (auto node : deltaInsertedNodes) {
        if (node->isFact) {
            nodeFormulas[node] = node->getProbability() == 1.0
                ? formulaManager.getTrue()
                : formulaManager.createVar(mapNodeId(node->getId()), *node);
            if (node->getProbability() != 1.0)
                formulaManager.setVariableWeight(mapNodeId(node->getId()), node->getProbability(), 1 - node->getProbability());
        } else {
            nodeFormulas[node] = formulaManager.getFalse();
        }
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
        std::cout << "[CYCLE " << cid << "] Begin Insertion Phase, worklist size: " << worklist.size() << std::endl;

        while (!worklist.empty()) {
            EdgePtr edge = worklist.front(); worklist.pop_front();

            std::cout << "  [EVAL] Edge " << edge->getId() << ": " << edge->toString() << std::endl;

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
                    std::cout << "    [WAIT] Missing input: " << inputs[i]->toString() << std::endl;
                    allAvailable = false;
                    break;
                }
                inputFormulas.push_back(negs[i] ? formulaManager.makeNot(it->second) : it->second);
            }

            if (!allAvailable) {
                worklist.push_back(edge);
                continue;
            }

            FormulaNodeRef newEdge = formulaManager.makeAnd(inputFormulas);
            if (formulaManager.isSame(edgeFormulas[edge], newEdge)) {
                std::cout << "    [SKIP] No change\n";
                continue;
            }

            std::cout << "    [CHANGE] Edge formula changed\n";
            edgeFormulas[edge] = newEdge;

            NodePtr output = view.getOutput(edge);
            if (!output || output->isFact) continue;

            std::vector<FormulaNodeRef> incoming;
            for (EdgePtr e : view.getIncomingEdges(output)) {
                if (edgeFormulas.count(e) && edgeFormulas[e].get()) {
                    incoming.push_back(edgeFormulas[e]);
                }
            }

            FormulaNodeRef newNode = formulaManager.makeOr(incoming);
            if (!formulaManager.isSame(nodeFormulas[output], newNode)) {
                std::cout << "    [UPDATE] Node formula changed: " << output->toString() << std::endl;
                nodeFormulas[output] = newNode;
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
    }

    std::cout << "[Info] Incremental cyclewise update completed.\n";
}

#endif //FORWARDCOMPILATION_H
