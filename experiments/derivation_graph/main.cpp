#include <iostream>
#include "DerivationGraph.h"
#include "souffle/Derivation.h"
#include "Rule.h"

//auto exampleRule = Rule::createExample();

int main() {
    // Create an example graph
    RuleManager ruleManager = ExampleRuleComponents::ruleManager;
    std::cout << ruleManager.toString() << std::endl;

    auto graph = DerivationGraph::createFrom(exampleRuleApps, ExampleRuleComponents::ruleManager);

    // Dump to DOT file
    graph->dumpDot("derivation.dot");

    return 0;
}