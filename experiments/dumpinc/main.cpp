// main.cpp
#include <iostream>
#include <memory>
#include "souffle/problog/DerivationGraph.h"

int main() {
    // 构建一个增量图
    auto g = std::make_unique<IncrementalDerivationGraph>();

    // ---- 基线 facts ----
    auto e23 = g->createNode(UntypedTuple{"edge", {2, 3}}); e23->isFact = true; e23->setProbability(0.9);
    // 注意：e12 将作为 delta-delete，这里不加入基线 facts
    auto p23 = g->createNode(UntypedTuple{"path", {2, 3}});
    auto p12 = g->createNode(UntypedTuple{"path", {1, 2}});
    auto p13 = g->createNode(UntypedTuple{"path", {1, 3}});

    // 基线规则：path(2,3) :- edge(2,3)
    auto r2 = g->createHyperedge({e23}, p23, /*rule*/nullptr, std::vector<bool>{false});
    if (r2) r2->setProbability(1.0);

    // 基线规则：path(1,3) :- path(1,2), edge(2,3)
    auto r3 = g->createHyperedge({p12, e23}, p13, /*rule*/nullptr, std::vector<bool>{false, false});
    if (r3) r3->setProbability(1.0);

    // ---- delta.delete：删除 edge(1,2) 及其影响 ----
    auto e12 = g->createNode(UntypedTuple{"edge", {1, 2}}); e12->isFact = true; e12->setProbability(0.5);
    // 标为删除
    g->deltaDeleteNodes.insert(e12);
    // 删除带来的影响（示例）：使 path(1,2) 与相关边受到影响
    g->deletedFactImpactedNodes[e12].insert(p12);
    // path(1,2) :- edge(1,2) 这条边在基线中未显式创建，这里演示把其也记入受影响边集合
    auto r1 = g->createHyperedge({e12}, p12, nullptr, std::vector<bool>{false});
    if (r1) r1->setProbability(1.0);
    g->deletedFactImpactedEdges[e12].insert(r1);
    g->deletedFactImpactedEdges[e12].insert(r3); // path(1,3) 的产生式同样受影响

    // ---- delta.insert：新增 edge(3,4) 以及相应规则 ----
    auto e34 = g->createNode(UntypedTuple{"edge", {3, 4}}); e34->isFact = true; e34->setProbability(0.8);
    g->deltaInsertNodes.insert(e34);

    auto p34 = g->createNode(UntypedTuple{"path", {3, 4}});
    auto r4 = g->createHyperedge({e34}, p34, nullptr, std::vector<bool>{false});
    if (r4) r4->setProbability(1.0);
    g->deltaInsertEdges.insert(r4);

    g->insertedFactImpactedNodes[e34].insert(p34);
    g->insertedFactImpactedEdges[e34].insert(r4);

    // ---- 导出并回读测试 ----
    try {
        g->dumpDotInc("inc.dot");
        g->dumpJsonInc("inc.json");
        std::cout << "Wrote inc.json\n";

        auto g2 = std::unique_ptr<IncrementalDerivationGraph>(
            IncrementalDerivationGraph::loadFromJsonInc("inc.json")
        );
        g2->dumpJsonInc("inc_copy.json");
        g2->dumpDotInc("inc_copy.dot");
        std::cout << "Wrote inc_copy.json\n";
        std::cout << "If you diff inc.json and inc_copy.json, they should be semantically equivalent.\n";
    } catch (const std::exception& ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
