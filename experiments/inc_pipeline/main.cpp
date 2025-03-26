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
    std::cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>> Full ..." << std::endl;
    auto graph = IncrementalDerivationGraph::createFrom(ExampleRuleComponents::exampleRuleApps, ExampleRuleComponents::ruleManager, ExampleRuleComponents::fact_prob);

    graph->dumpDot("derivation-full.dot");
    std::map<NodePtr, BddNodeRef> nodeFormulas;
    std::map<EdgePtr, BddNodeRef> edgeFormulas;
    WeightedBDDManager bddManager;
    buildFormulas(*graph, bddManager, nodeFormulas, edgeFormulas);

    for (const auto& [node, bdd] : nodeFormulas) {
        std::cout << "Node" << node->getId() << " " << node->getTuple().toString() << ": ";
        std::cout << bddManager.toString(bdd) << "\t";
        auto prob = bddManager.computeWeightedModelCount(bdd);
        std::cout << "Probability: " << prob << std::endl;
    }
    for (const auto& [edge, bdd] : edgeFormulas) {
        std::cout << edge->toString() << " : ";
        std::cout << bddManager.toString(bdd) << "\t";
        auto prob = bddManager.computeWeightedModelCount(bdd);
        std::cout << "Probability: " << prob << std::endl;
    }
    std::cout << "Done" << std::endl;

    std::cout << ">>>>>>>>>>>>>>>>>>>>>>>>>>>> Incremental ..." << std::endl;

    graph->applyDelta(ExampleRuleComponents::exampleDeltaInsertRuleApps, ExampleRuleComponents::exampleDeltaDeleteRuleApps, ExampleRuleComponents::ruleManager, ExampleRuleComponents::fact_prob_inc, ExampleRuleComponents::deletedFacts);
    // Dump to DOT file
    graph->dumpDotInc("derivation-inc.dot");
    buildFormulasInc(*graph, bddManager, nodeFormulas, edgeFormulas);
    for (const auto& [node, bdd] : nodeFormulas) {
        std::cout << "Node" << node->getId() << " " << node->getTuple().toString() << ": ";
        std::cout << bddManager.toString(bdd) << "\t";
        auto prob = bddManager.computeWeightedModelCount(bdd);
        std::cout << "Probability: " << prob << std::endl;
    }
    for (const auto& [edge, bdd] : edgeFormulas) {
        std::cout << edge->toString() << " : ";
        std::cout << bddManager.toString(bdd) << "\t";
        auto prob = bddManager.computeWeightedModelCount(bdd);
        std::cout << "Probability: " << prob << std::endl;
    }
    std::cout << "Done" << std::endl;

    return 0;
}