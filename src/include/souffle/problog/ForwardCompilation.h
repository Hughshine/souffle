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
        if (node->getIncomingEdges().empty()) {
            nodeFormulas[node] = formulaManager.createVar(node->getId(), *node);
            baseNodeFormulas.insert({node, nodeFormulas[node]});
//            std::cout << "Setting weight for node " << node->getId() << " with probability " << node->getProbability() << std::endl;
            formulaManager.setVariableWeight(node->getId(), node->getProbability(), 1-node->getProbability());
        }
    }
//    std::cout << "Weight set for nodes" << std::endl;

    // Initialize formulas for rule instantiations (hyperedges)
    for (const auto& edge : graph.getEdges()) {
        // Create a variable using the edge's unique ID; need to plus the size of graph.getNodes() to avoid conflict with node's id
        auto baseEdgeFormula = formulaManager.createVar(graph.getNodes().size() + edge->getId(), *edge);
        formulaManager.setVariableWeight(graph.getNodes().size() + edge->getId(), edge->getRule()->getProbability(), 1-edge->getRule()->getProbability());
        baseEdgeFormulas.insert({edge, baseEdgeFormula});
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

    while (!worklist.empty()) {
        auto edge = worklist.front();
        worklist.pop();
        inWorklist.erase(edge);

        // Store the old edge formula to check if it changes
        FormulaNodeRef oldEdgeFormula = edgeFormulas[edge];

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
                allInputsAvailable = false;
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
//                std::cout << "Old formula: " << formulaManager.toString(oldNodeFormula) << std::endl;
//                for (const auto& inEdge : output->getIncomingEdges()) {
//                    std::cout << "Incoming edge formula: " << formulaManager.toString(edgeFormulas[inEdge]) << std::endl;
//                }
//
//                std::cout << "New formula: " << formulaManager.toString(newNodeFormula) << std::endl;
                auto prob = formulaManager.computeWeightedModelCount(newNodeFormula);
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
    std::cout << "Successfully build formulas" << std::endl;
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
    if (graph.deltaInsertEdges.empty() && graph.deltaDeleteEdges.empty()) {
        std::cout << "No changes to apply, skipping incremental update" << std::endl;
        return;
    }

    // Function to update edge formula based on its inputs
    auto updateEdgeFormula = [&](EdgePtr edge) -> FormulaNodeRef {
        // Get base edge formula (the rule probability)
        auto baseEdgeFormula = formulaManager.createVar(graph.getNodes().size() + edge->getId(), *edge);

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
    std::set<EdgePtr> updatedSet(graph.deltaDeleteEdges.begin(), graph.deltaDeleteEdges.end());
//    auto deltaDeleteEdgesCopy = graph.deltaDeleteEdges;
    // Process deleted edges - update their formulas and propagate changes
    for (auto node : graph.deltaDeleteNodes) {
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
        if (graph.deltaDeleteEdges.find(edge) != graph.deltaDeleteEdges.end()) {
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
            if (graph.deltaDeleteNodes.find(output) != graph.deltaDeleteNodes.end()) {
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
                    if (graph.deltaDeleteEdges.find(outEdge) == graph.deltaDeleteEdges.end()) {
                        updatedSet.insert(outEdge);
                    }
                }
            }
        }
    }

    // Initialize updated set with all inserted edges
    updatedSet.clear();
    updatedSet.insert(graph.deltaInsertEdges.begin(), graph.deltaInsertEdges.end());
    for (auto node : graph.deltaInsertNodes) {
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
    for (auto edge: graph.deltaDeleteEdges) {
        edgeFormulas.erase(edge);
    }
    std::cout << "Successfully completed incremental formula update" << std::endl;
}

#endif //FORWARDCOMPILATION_H
