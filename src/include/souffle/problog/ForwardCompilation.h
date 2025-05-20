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
                nodeFormulas[node] = formulaManager.createVar(node->getId(), *node);
            }
            baseNodeFormulas.insert({node, nodeFormulas[node]});
            formulaManager.setVariableWeight(node->getId(), node->getProbability(), 1-node->getProbability());
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
            auto baseEdgeFormula = formulaManager.createVar(view.getNodes().size() + edge->getId(), *edge);
            formulaManager.setVariableWeight(view.getNodes().size() + edge->getId(), edge->getRule()->getProbability(), 1-edge->getRule()->getProbability());
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
    bool operator<(const PrioritizedEdge& other) const {
        return priority > other.priority; // lower depth = higher priority
    }
};

template<typename FormulaNodeRef>
void buildFormulasCyclewise(
    const SubgraphView& view,
    FormulaManager<FormulaNodeRef>& formulaManager,
    std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
    std::map<EdgePtr, FormulaNodeRef>& edgeFormulas
) {
    FunctionTimer timer("Build Formulas Cyclewise");

    using namespace std;
    vector<unordered_set<NodePtr>> nodeCycles;
    vector<unordered_set<EdgePtr>> edgeCycles;
    unordered_map<NodePtr, size_t> nodeToCycleIndex;
    unordered_map<EdgePtr, size_t> edgeToCycleIndex;
    unordered_map<NodePtr, size_t> nodeDepths;
    unordered_map<EdgePtr, size_t> edgeDepths;

    computeSCCOrderedCyclesWithDepth(view, nodeCycles, edgeCycles, nodeToCycleIndex, edgeToCycleIndex, nodeDepths, edgeDepths);

    map<NodePtr, FormulaNodeRef> baseNodeFormulas;
    map<EdgePtr, FormulaNodeRef> baseEdgeFormulas;

    for (const auto& node : view.getNodes()) {
        if (node->isFact) {
            FormulaNodeRef var = (node->getProbability() == 1.0)
                ? formulaManager.getTrue()
                : formulaManager.createVar(node->getId(), *node);
            formulaManager.setVariableWeight(node->getId(), node->getProbability(), 1 - node->getProbability());
            nodeFormulas[node] = var;
            baseNodeFormulas[node] = var;
        }
    }

    for (const auto& edge : view.getEdges()) {
        FormulaNodeRef f = edge->getRule()->isDeterminstic()
            ? formulaManager.getTrue()
            : formulaManager.createVar(view.getNodes().size() + edge->getId(), *edge);
        if (!edge->getRule()->isDeterminstic()) {
            formulaManager.setVariableWeight(view.getNodes().size() + edge->getId(), edge->getProbability(), 1 - edge->getProbability());
        }
        baseEdgeFormulas[edge] = f;
    }

    size_t iteration = 0;
    for (size_t cid = 0; cid < edgeCycles.size(); ++cid) {
        priority_queue<PrioritizedEdge> worklist;
        set<EdgePtr> inWorklist;
        for (const auto& edge : edgeCycles[cid]) {
            worklist.push({edge, edgeDepths[edge]});
            inWorklist.insert(edge);
        }

        while (!worklist.empty()) {
            std::cout << "Cycle id: " << cid << std::endl;
            std::cout << "Iteration: " << ++iteration << std::endl;
            std::cout << "Worklist size: " << worklist.size() << std::endl;
            formulaManager.dumpProfilingStatistics();

            EdgePtr edge = worklist.top().edge;
            worklist.pop();
            inWorklist.erase(edge);
            std::cout << "Processing edge " << edge->getId() << " " << edge->toString() << std::endl;

            vector<FormulaNodeRef> inputs = { baseEdgeFormulas[edge] };
            bool allAvailable = true;
            for (size_t i = 0; i < view.getInputs(edge).size(); ++i) {
                auto input = view.getInputs(edge)[i];
                auto it = nodeFormulas.find(input);
                if (it == nodeFormulas.end()) {
                    cout << "Input node formula not available yet: " << input->getTuple().toString() << endl;
                    allAvailable = false;
//                    break;
//                    assert (false && "Input node formula not available yet; should not happen if priority queue is used");

                    continue;
                }
                inputs.push_back(view.getBodyNegations(edge)[i] ? formulaManager.makeNot(it->second) : it->second);
            }
            if (!allAvailable) {
                worklist.push({edge, edgeDepths[edge]});
                inWorklist.insert(edge);
                continue;
            }
            FormulaNodeRef newEdgeF = inputs.size() == 1 ? inputs[0] : formulaManager.makeAnd(inputs);
            bool edgeChanged = !formulaManager.isSame(edgeFormulas[edge], newEdgeF);
            if (edgeChanged) {
                cout << "Edge " << edge->getId() << " changed.\n";
                edgeFormulas[edge] = newEdgeF;
                NodePtr out = view.getOutput(edge);
                vector<FormulaNodeRef> inFs;
                for (auto& inEdge : view.getIncomingEdges(out)) {
                    if (edgeFormulas.count(inEdge) && edgeFormulas[inEdge].get()) {
                        inFs.push_back(edgeFormulas[inEdge]);
                    }
                }
                if (!inFs.empty()) {
                    FormulaNodeRef newNodeF = inFs.size() == 1 ? inFs[0] : formulaManager.makeOr(inFs);
                    bool nodeChanged = nodeFormulas.count(out) == 0 || !formulaManager.isSame(nodeFormulas[out], newNodeF);
                    if (nodeChanged) {
                        cout << "Node " << out->getId() << " changed.\n";
                        nodeFormulas[out] = newNodeF;
                        for (auto& outEdge : view.getOutgoingEdges(out)) {
                            size_t targetCid = edgeToCycleIndex[outEdge];
                            if (targetCid == cid && !inWorklist.count(outEdge)) {
                                worklist.push({outEdge, edgeDepths[outEdge]});
                                inWorklist.insert(outEdge);
                            }
                        }
                    }
                }
            }
        }
    }

    std::cout << "Successfully built formulas with cyclewise priority" << std::endl;
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
    std::deque<EdgePtr> worklist(view.getDeltaDeleteEdges().begin(), view.getDeltaDeleteEdges().end());

    // TODO: we need to do over-deletion first
    for (auto node : view.getDeltaDeleteNodes()) {
        nodeFormulas.erase(node);
    }
    for (auto edge : view.getDeltaDeleteEdges()) {
        edgeFormulas.erase(edge);
    }

    // TODO: and have a similar re-derivation phrase
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
            auto outputNode = edge->getOutput();
            if (deltaDeletedNodes.count(outputNode)) {
                // the output node is deleted, so we can skip it
                // we do not need to add its outgoing edges to the worklist since they should have already been in deltaDeletedEdges
                continue;
            }
            assert (outputNode->isFact == false);
            std::vector<FormulaNodeRef> incoming;
            for (auto e : view.getIncomingEdges(outputNode)) {
                auto it = edgeFormulas.find(e);
                if (it != edgeFormulas.end() && it->second.get()) {
                    // TODO: needs to do overdeletion
                    incoming.push_back(it->second);
                }
            }
            assert (!incoming.empty());
            auto newNodeFormula = formulaManager.makeOr(incoming);
            nodeFormulas[outputNode] = newNodeFormula;
            std::cout << "Node " << outputNode->toString() << " changed.\n";
            std::cout << "Node new formula: " << formulaManager.toString(nodeFormulas[outputNode]) << std::endl;
            std::cout << "Node new formula???: " << formulaManager.toString(newNodeFormula) << std::endl;

            for (auto outEdge : view.getOutgoingEdges(edge->getOutput())) {
                worklist.push_back(outEdge);
            }
            continue;
        }

        // a propagated edge / an unknown normal edge
        // now the edge should have its old formula (not deleted)
        FormulaNodeRef oldEdgeFormula = edgeFormulas[edge];
        // calculate the new formula
        FormulaNodeRef baseFormula = edge->getRule()->isDeterminstic()
            ? formulaManager.getTrue()
            : formulaManager.createVar(view.getNodes().size() + edge->getId(), *edge);
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
                : formulaManager.createVar(node->getId(), *node);
            if (node->getProbability() != 1.0)
                formulaManager.setVariableWeight(node->getId(), node->getProbability(), 1 - node->getProbability());
        } else {
            nodeFormulas[node] = formulaManager.getFalse();
        }
    }
    for (auto edge : deltaInsertedEdges) {
        if (edge->getRule()->isDeterminstic()) {
            edgeFormulas[edge] = formulaManager.getFalse();
        } else {
            edgeFormulas[edge] = formulaManager.getFalse();
            formulaManager.createVar(view.getNodes().size() + edge->getId(), *edge);
            formulaManager.setVariableWeight(view.getNodes().size() + edge->getId(), edge->getRule()->getProbability(), 1 - edge->getRule()->getProbability());
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
            : formulaManager.createVar(view.getNodes().size() + edge->getId(), *edge);
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


#endif //FORWARDCOMPILATION_H
