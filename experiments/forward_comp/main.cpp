#include <iostream>
#include "souffle/Derivation.h"
#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/RuleManager.h"
#include "souffle/problog/formula/FormulaManager.h"



int main() {
    RuleManager ruleManager = ExampleRuleComponents::ruleManager;
    std::cout << ruleManager.toString() << std::endl;
    auto graph = DerivationGraph::createFrom(exampleRuleApps, ExampleRuleComponents::ruleManager);
    // Dump to DOT file
    graph->dumpDot("derivation.dot");



    return 0;
}