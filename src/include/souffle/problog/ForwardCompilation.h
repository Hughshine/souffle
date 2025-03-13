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
        for (auto i = 0; i < edge->getInputs().size(); i++) {
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
                for (const auto& formula : incomingFormulas) {
                    std::cout << formulaManager.toString(formula) << std::endl;
                }
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

#endif //FORWARDCOMPILATION_H
