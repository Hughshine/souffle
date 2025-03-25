#include <iostream>
#include "souffle/Derivation.h"
#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/RuleManager.h"
#include "souffle/problog/formula/FormulaManager.h"
#include "souffle/problog/formula/LogicFormulaManager.h"
#include <queue>
#include "souffle/problog/formula/CuddManager.h"
#include "souffle/problog/ForwardCompilation.h"

int main() {
    RuleManager ruleManager = ExampleRuleComponents::ruleManager;
    std::cout << ruleManager.toString() << std::endl;
    auto graph = IncrementalDerivationGraph::createFrom(ExampleRuleComponents::exampleRuleApps2, ExampleRuleComponents::ruleManager);
    // Dump to DOT file
    graph->dumpDot("derivation.dot");

    std::map<NodePtr, BddNodeRef> nodeFormulas;
    std::map<EdgePtr, BddNodeRef> edgeFormulas;
    WeightedBDDManager bddManager;



    // TODO: automatically set weights based on facts and rule probabilities
//    bddManager.setVariableWeight(1, 1.0, 0.0);  // edge(1, 2)
//    bddManager.setVariableWeight(3, 1.0, 0.0);  // edge(1, 2)
//    bddManager.setVariableWeight(4, 1.0, 0.0);  // edge(1, 2)
    bddManager.setVariableWeight(1, 0.5, 0.5);  // edge(1, 2)
    bddManager.setVariableWeight(3, 0.8, 0.2);  // edge(3, 2)
    bddManager.setVariableWeight(4, 0.9, 0.1);  // edge(2, 3)
//    bddManager.setVariableWeight(9, 0.9, 0.1);
//    bddManager.setVariableWeight(11, 0.9, 0.1);
//    bddManager.setVariableWeight(10, 0.9, 0.1);
//    bddManager.setVariableWeight(12, 0.9, 0.1);
//    bddManager.setVariableWeight(13, 0.9, 0.1);
//    bddManager.setVariableWeight(14, 0.9, 0.1);
//    bddManager.setVariableWeight(15, 0.9, 0.1);
//    bddManager.setVariableWeight(16, 0.9, 0.1);
//    bddManager.setVariableWeight(17, 0.9, 0.1);

    buildFormulas(*graph, bddManager, nodeFormulas, edgeFormulas);

    for (const auto& [node, bdd] : nodeFormulas) {
        std::cout << "Node" << node->getId() << " " << node->getTuple().toString() << ": ";
//        formulaManager.printInfo(formula, "formula");
//        auto bdd = transform(formula, formulaManager, bddManager);
        std::cout << bddManager.toString(bdd) << "\t";
        auto prob = bddManager.computeWeightedModelCount(bdd);
        std::cout << "Probability: " << prob << std::endl;
    }
    for (const auto& [edge, bdd] : edgeFormulas) {
        std::cout << edge->toString() << " : ";
//        formulaManager.printInfo(formula, "formula");
//        auto bdd = transform(formula, formulaManager, bddManager);
        auto prob = bddManager.computeWeightedModelCount(bdd);
        std::cout << "Probability: " << prob << std::endl;
    }
    std::cout << "Done" << std::endl;
    return 0;
}