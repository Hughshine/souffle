
#include <iostream>
#include <vector>
#include <string>
#include "IncRegionAnalyzer.h"

using namespace incra;

int main() {
    // Build a typical slightly-larger graph
    ExampleIncView view;
    view.build_demo();

    // Analyzer
    RegionAnalyzer analyzer(view);

    // Seed delta input facts = delta insert facts by default (you can customize)
    std::vector<NodePtr> delta_facts;
    for (auto& n : view.getDeltaInsertNodes()) delta_facts.push_back(n);
    for (auto& n : view.getDeltaDeleteNodes()) delta_facts.push_back(n);

    // Analyze
    auto stats = analyzer.analyze(delta_facts, "out.json", "out.csv");

    // Visualize
    bool ok = analyzer.toDot("out.dot");
    std::cout << "toDot saved: " << (ok ? "out.dot" : "(failed)") << std::endl;

    return 0;
}
