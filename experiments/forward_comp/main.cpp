#include <iostream>
#include "souffle/Derivation.h"
#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/RuleManager.h"
#include "souffle/problog/formula/FormulaManager.h"
#include "souffle/problog/formula/LogicFormulaManager.h"
#include <queue>
#include "souffle/problog/formula/CuddManager.h"

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
        }
    }

    // Initialize formulas for rule instantiations (hyperedges)
    for (const auto& edge : graph.getEdges()) {
        // Create a variable using the edge's unique ID; need to plus the size of graph.getNodes() to avoid conflict with node's id
        edgeFormulas[edge] = formulaManager.createVar(graph.getNodes().size() + edge->getId(), *edge);
        baseEdgeFormulas.insert({edge, edgeFormulas[edge]});
    }

    // Worklist algorithm
    std::queue<EdgePtr> worklist;
    std::set<EdgePtr> inWorklist; // Track edges in the worklist to avoid duplicates

    // Initialize worklist with all edges
    for (const auto& edge : graph.getEdges()) {
        worklist.push(edge);
        inWorklist.insert(edge);
    }

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
        for (const auto& input : edge->getInputs()) {
            auto it = nodeFormulas.find(input);
            if (it != nodeFormulas.end()) {
                inputFormulas.push_back(it->second);
//                formulaManager.printInfo(it->second, "input");
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
            for (const auto& inEdge : output->getIncomingEdges()) {
                auto it = edgeFormulas.find(inEdge);
                if (it != edgeFormulas.end()) {
                    incomingFormulas.push_back(it->second);
                }
            }

            // Compute the disjunction of all incoming edge formulas
            FormulaNodeRef newNodeFormula;
            if (incomingFormulas.empty()) {
                // This shouldn't happen, but use the node's base formula if it does
                newNodeFormula = nodeFormulas[output];
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
                for (const auto& outEdge : output->getOutgoingEdges()) {
                    if (inWorklist.find(outEdge) == inWorklist.end()) {
                        worklist.push(outEdge);
                        inWorklist.insert(outEdge);
                    }
                }
            }
        }
    }
}

int main() {
    RuleManager ruleManager = ExampleRuleComponents::ruleManager;
    std::cout << ruleManager.toString() << std::endl;
    auto graph = DerivationGraph::createFrom(exampleRuleApps, ExampleRuleComponents::ruleManager);
    // Dump to DOT file
    graph->dumpDot("derivation.dot");
    std::map<NodePtr, BddNodeRef> nodeFormulas;
    std::map<EdgePtr, BddNodeRef> edgeFormulas;
//    LogicFormulaManager formulaManager;
    WeightedBDDManager bddManager;
    buildFormulas(*graph, bddManager, nodeFormulas, edgeFormulas);



    // TODO: automatically set weights based on facts and rule probabilities
    bddManager.setVariableWeight(1, 0.5, 0.5);  // edge(1, 2)
    bddManager.setVariableWeight(3, 0.5, 0.5);  // edge(2, 3)
    bddManager.setVariableWeight(5, 0.9, 0.1);  // rule1
    bddManager.setVariableWeight(6, 0.9, 0.1);  // rule2
    bddManager.setVariableWeight(7, 0.9, 0.1);  // rule1

    for (const auto& [node, bdd] : nodeFormulas) {
        std::cout << "Node" << node->getId() << " " << node->getTuple().toString() << ": ";
//        formulaManager.printInfo(formula, "formula");
//        auto bdd = transform(formula, formulaManager, bddManager);
        auto prob = bddManager.computeWeightedModelCount(bdd);
        std::cout << "Probability: " << prob << std::endl;
    }
    for (const auto& [edge, bdd] : edgeFormulas) {
        std::cout << "Edge" << edge->getId() << " : ";
//        formulaManager.printInfo(formula, "formula");
//        auto bdd = transform(formula, formulaManager, bddManager);
        auto prob = bddManager.computeWeightedModelCount(bdd);
        std::cout << "Probability: " << prob << std::endl;
    }
    std::cout << "Done" << std::endl;
    return 0;
}