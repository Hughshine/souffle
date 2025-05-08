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
    const DerivationGraph& graph,
    FormulaManager<FormulaNodeRef>& formulaManager,
    std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
    std::map<EdgePtr, FormulaNodeRef>& edgeFormulas
) {
    FunctionTimer timer(" forward compilation, building formulas ");

    // Initialize formulas for input facts (nodes)
    std::map<NodePtr, FormulaNodeRef> baseNodeFormulas;
    std::map<EdgePtr, FormulaNodeRef> baseEdgeFormulas;
    for (const auto& node : graph.getNodes()) {
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
    for (const auto& edge : graph.getEdges()) {
        // Create a variable using the edge's unique ID; need to plus the size of graph.getNodes() to avoid conflict with node's id
        if (edge->getRule()->isDeterminstic()) {
            auto baseEdgeFormula = formulaManager.getTrue();
            baseEdgeFormulas.insert({edge, baseEdgeFormula});
        } else {
            auto baseEdgeFormula = formulaManager.createVar(graph.getNodes().size() + edge->getId(), *edge);
            formulaManager.setVariableWeight(graph.getNodes().size() + edge->getId(), edge->getRule()->getProbability(), 1-edge->getRule()->getProbability());
            baseEdgeFormulas.insert({edge, baseEdgeFormula});
        }
    }
//    std::cout << "Initialize formulas for rule instantiations (hyperedges)" << std::endl;

    // Worklist algorithm
    std::queue<EdgePtr> worklist;
    std::set<EdgePtr> inWorklist; // Track edges in the worklist to avoid duplicates

    // Initialize worklist with all edges
    for (const auto& edge : graph.getEdges()) {
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
        for (size_t i = 0; i < edge->getInputs().size(); i++) {
            auto input = edge->getInputs()[i];
            auto isNegated = edge->getBodyNegations()[i];
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
            auto output = edge->getOutput();
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
            for (const auto& inEdge : output->getIncomingEdges()) {

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
                for (const auto& outEdge : output->getOutgoingEdges()) {
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
    const DerivationGraph& graph,
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

    computeSCCOrderedCyclesWithDepth(graph, nodeCycles, edgeCycles, nodeToCycleIndex, edgeToCycleIndex, nodeDepths, edgeDepths);

    map<NodePtr, FormulaNodeRef> baseNodeFormulas;
    map<EdgePtr, FormulaNodeRef> baseEdgeFormulas;

    for (const auto& node : graph.getNodes()) {
        if (node->isFact) {
            FormulaNodeRef var = (node->getProbability() == 1.0)
                ? formulaManager.getTrue()
                : formulaManager.createVar(node->getId(), *node);
            formulaManager.setVariableWeight(node->getId(), node->getProbability(), 1 - node->getProbability());
            nodeFormulas[node] = var;
            baseNodeFormulas[node] = var;
        }
    }

    for (const auto& edge : graph.getEdges()) {
        FormulaNodeRef f = edge->getRule()->isDeterminstic()
            ? formulaManager.getTrue()
            : formulaManager.createVar(graph.getNodes().size() + edge->getId(), *edge);
        if (!edge->getRule()->isDeterminstic()) {
            formulaManager.setVariableWeight(graph.getNodes().size() + edge->getId(), edge->getProbability(), 1 - edge->getProbability());
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
            for (size_t i = 0; i < edge->getInputs().size(); ++i) {
                auto input = edge->getInputs()[i];
                auto it = nodeFormulas.find(input);
                if (it == nodeFormulas.end()) {
                    cout << "Input node formula not available yet: " << input->getTuple().toString() << endl;
                    allAvailable = false;
//                    break;
//                    assert (false && "Input node formula not available yet; should not happen if priority queue is used");

                    continue;
                }
                inputs.push_back(edge->getBodyNegations()[i] ? formulaManager.makeNot(it->second) : it->second);
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
                NodePtr out = edge->getOutput();
                vector<FormulaNodeRef> inFs;
                for (auto& inEdge : out->getIncomingEdges()) {
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
                        for (auto& outEdge : out->getOutgoingEdges()) {
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
    const IncrementalDerivationGraph& graph,
    FormulaManager<FormulaNodeRef>& formulaManager,
    std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
    std::map<EdgePtr, FormulaNodeRef>& edgeFormulas
) {
    FunctionTimer timer(" forward compilation, incremental update ");

    // Skip if no changes
    if (graph.getDeltaInsertEdges().empty() && graph.getDeltaDeleteEdges().empty()) {
        std::cout << "No changes to apply, skipping incremental update" << std::endl;
        return;
    }

    // Function to update edge formula based on its inputs
    auto updateEdgeFormula = [&](EdgePtr edge) -> FormulaNodeRef {
        // Get base edge formula (the rule probability)
        FormulaNodeRef baseEdgeFormula;
        if (edge->getRule()->isDeterminstic()) {
            baseEdgeFormula = formulaManager.getTrue();
        } else {
            baseEdgeFormula = formulaManager.createVar(graph.getNodes().size() + edge->getId(), *edge);
            formulaManager.setVariableWeight(graph.getNodes().size() + edge->getId(), edge->getRule()->getProbability(), 1-edge->getRule()->getProbability());
        }

//        auto baseEdgeFormula = formulaManager.createVar(graph.getNodes().size() + edge->getId(), *edge);

        // Collect input formulas
        std::vector<FormulaNodeRef> inputFormulas;
        inputFormulas.push_back(baseEdgeFormula);

        // Add all input node formulas
        bool allInputsAvailable = true;
        for (size_t i = 0; i < edge->getInputs().size(); i++) {
            auto input = edge->getInputs()[i];
            auto isNegated = edge->getBodyNegations()[i];
            auto it = nodeFormulas.find(input);

            if (it != nodeFormulas.end()) {
                if (!isNegated) {
                    inputFormulas.push_back(it->second);
                } else {
                    inputFormulas.push_back(formulaManager.makeNot(it->second));
                }
            } else {
                allInputsAvailable = false;
                break;
            }
        }

        // If all inputs available, compute conjunction
        if (allInputsAvailable) {
            if (inputFormulas.size() == 1) {
                return inputFormulas[0];
            } else {
                return formulaManager.makeAnd(inputFormulas);
            }
        }

        // Return empty formula if inputs not available
        return formulaManager.getFalse();
    };

    // Function to update node formula based on incoming edges
    auto updateNodeFormula = [&](NodePtr node) -> FormulaNodeRef {
        // Collect formulas from all incoming edges
        std::vector<FormulaNodeRef> incomingFormulas;

        for (const auto& inEdge : node->getIncomingEdges()) {
            auto it = edgeFormulas.find(inEdge);
            if (it != edgeFormulas.end() && it->second.get()) {
                incomingFormulas.push_back(it->second);
            }
        }

        // Compute disjunction of all incoming edge formulas
        if (incomingFormulas.empty()) {
            // If no incoming edges, node is not derivable (False)
            return formulaManager.getFalse();
        } else if (incomingFormulas.size() == 1) {
            return incomingFormulas[0];
        } else {
            return formulaManager.makeOr(incomingFormulas);
        }
    };

    // Initialize updated set with all deleted edges
    std::set<EdgePtr> updatedSet(graph.getDeltaDeleteEdges().begin(), graph.getDeltaDeleteEdges().end());
//    auto deltaDeleteEdgesCopy = graph.deltaDeleteEdges;
    // Process deleted edges - update their formulas and propagate changes
    for (auto node : graph.getDeltaDeleteNodes()) {
        nodeFormulas.erase(node);
    }
    while (!updatedSet.empty()) {
        // Get an edge from the updated set
        auto edge = *updatedSet.begin();
        updatedSet.erase(updatedSet.begin());

        // Store old edge formula
        FormulaNodeRef oldEdgeFormula = edgeFormulas[edge];

        // For deleted edges, set formula to False or recompute
        FormulaNodeRef newEdgeFormula;
        if (graph.getDeltaDeleteEdges().find(edge) != graph.getDeltaDeleteEdges().end()) {
            // Set to False (empty formula) TODO
            newEdgeFormula = formulaManager.getFalse();
        } else {
            // Recompute based on inputs
            newEdgeFormula = updateEdgeFormula(edge);
        }

        // Check if formula changed
        bool edgeFormulaChanged = !formulaManager.isSame(oldEdgeFormula, newEdgeFormula);

        if (edgeFormulaChanged) {
            // Update edge formula
            edgeFormulas[edge] = newEdgeFormula;

            // Update output node formula
            auto output = edge->getOutput();

            // Skip if output node is also deleted
            if (graph.getDeltaDeleteNodes().find(output) != graph.getDeltaDeleteNodes().end()) {
                continue;
            }

            // Store old node formula
            FormulaNodeRef oldNodeFormula;
            bool nodeHasFormula = nodeFormulas.find(output) != nodeFormulas.end();
            if (nodeHasFormula) {
                oldNodeFormula = nodeFormulas[output];
            }

            // Compute new node formula
            FormulaNodeRef newNodeFormula = updateNodeFormula(output);

            // Check if node formula changed
            bool nodeFormulaChanged = !nodeHasFormula ||
                                     !formulaManager.isSame(oldNodeFormula, newNodeFormula);

            if (nodeFormulaChanged) {
                // Update node formula
                nodeFormulas[output] = newNodeFormula;

                // Add outgoing edges to updated set
                for (const auto& outEdge : output->getOutgoingEdges()) {
                    if (graph.getDeltaDeleteEdges().find(outEdge) == graph.getDeltaDeleteEdges().end()) {
                        updatedSet.insert(outEdge);
                    }
                }
            }
        }
    }

    // Initialize updated set with all inserted edges
    updatedSet.clear();
    updatedSet.insert(graph.getDeltaInsertEdges().begin(), graph.getDeltaInsertEdges().end());
    for (auto node : graph.getDeltaInsertNodes()) {
        // Create a variable using the fact's unique ID
        if (node->getIncomingEdges().empty()) {
            nodeFormulas[node] = formulaManager.createVar(node->getId(), *node);
            formulaManager.setVariableWeight(node->getId(), node->getProbability(), 1-node->getProbability());
        }
    }
    // Process inserted edges - update their formulas and propagate changes
    while (!updatedSet.empty()) {
        // Get an edge from the updated set
        auto edge = *updatedSet.begin();
        updatedSet.erase(updatedSet.begin());

        // Store old edge formula
        FormulaNodeRef oldEdgeFormula;
        bool edgeHasFormula = edgeFormulas.find(edge) != edgeFormulas.end();
        if (edgeHasFormula) {
            oldEdgeFormula = edgeFormulas[edge];
        }

        // Compute new edge formula
        FormulaNodeRef newEdgeFormula = updateEdgeFormula(edge);

        // Check if formula changed
        bool edgeFormulaChanged = !edgeHasFormula ||
                                 !formulaManager.isSame(oldEdgeFormula, newEdgeFormula);

        if (edgeFormulaChanged) {
            // Update edge formula
            edgeFormulas[edge] = newEdgeFormula;

            // Update output node formula
            auto output = edge->getOutput();

            // Store old node formula
            FormulaNodeRef oldNodeFormula;
            bool nodeHasFormula = nodeFormulas.find(output) != nodeFormulas.end();
            if (nodeHasFormula) {
                oldNodeFormula = nodeFormulas[output];
            }

            // Compute new node formula
            FormulaNodeRef newNodeFormula = updateNodeFormula(output);

            // Check if node formula changed
            bool nodeFormulaChanged = !nodeHasFormula ||
                                     !formulaManager.isSame(oldNodeFormula, newNodeFormula);

            if (nodeFormulaChanged) {
                // Update node formula
                nodeFormulas[output] = newNodeFormula;

                // Add outgoing edges to updated set
                for (const auto& outEdge : output->getOutgoingEdges()) {
                    updatedSet.insert(outEdge);
                }
            }
        }
    }
    for (auto edge: graph.getDeltaDeleteEdges()) {
        edgeFormulas.erase(edge);
    }
    std::cout << "Successfully completed incremental formula update" << std::endl;
}

#endif //FORWARDCOMPILATION_H
