#include <iostream>
#include <string>
#include <vector>
#include <set>

#include "souffle/Derivation.h"
#include "souffle/problog/Rule.h"
#include "souffle/problog/RuleManager.h"
#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/PreDerivationGraph.h"

// 辅助函数，用于打印增量信息
void printGraphState(int turn, IncrementalDerivationGraph& graph) {
    std::cout << "--- Turn " << turn << " State ---" << std::endl;

    // 打印当前图中的所有节点
    std::cout << "Total Nodes in Graph (" << graph.getNodes().size() << "):" << std::endl;
    std::set<std::string> node_names;
    for (const auto& node : graph.getNodes()) {
        node_names.insert(node->getTuple().toString());
    }
    for (const auto& name : node_names) {
        std::cout << "  " << name << std::endl;
    }

    // 打印增量信息
    std::cout << "Delta Inserts (" << graph.deltaInsertNodes.size() << " nodes, "
              << graph.deltaInsertEdges.size() << " edges):" << std::endl;
    for (const auto& node : graph.deltaInsertNodes) {
        std::cout << "  + Node: " << node->getTuple().toString() << std::endl;
    }
    for (const auto& edge : graph.deltaInsertEdges) {
        std::cout << "  + Edge: " << edge->toString() << std::endl;
    }

    std::cout << "Delta Deletes (" << graph.deltaDeleteNodes.size() << " nodes, "
              << graph.deltaDeleteEdges.size() << " edges):" << std::endl;
    for (const auto& node : graph.deltaDeleteNodes) {
        std::cout << "  - Node: " << node->getTuple().toString() << std::endl;
    }
    for (const auto& edge : graph.deltaDeleteEdges) {
        std::cout << "  - Edge: " << edge->toString() << std::endl;
    }
    std::cout << "-----------------------\n" << std::endl;
}

int main() {
    // 1. 初始化
    PreDerivationGraph preGraph;
    IncrementalDerivationGraph graph; // 这是我们将要持续更新的图

    // 2. 定义图的结构 (规则)
    // path(X,Y) :- edge(X,Y)
    // path(X,Y) :- path(X,Z), edge(Z,Y)
    // 我们通过手动添加 ground rule 的方式来模拟

    // 为了方便，我们先创建所有可能用到的节点
    NodeId edge_ab = preGraph.getOrAddNode({ "edge", {1, 2} });
    NodeId edge_bc = preGraph.getOrAddNode({ "edge", {2, 3} });
    NodeId edge_cd = preGraph.getOrAddNode({ "edge", {3, 4} });

    NodeId path_ab = preGraph.getOrAddNode({ "path", {1, 2} });
    NodeId path_bc = preGraph.getOrAddNode({ "path", {2, 3} });
    NodeId path_cd = preGraph.getOrAddNode({ "path", {3, 4} });
    NodeId path_ac = preGraph.getOrAddNode({ "path", {1, 3} });
    NodeId path_bd = preGraph.getOrAddNode({ "path", {2, 4} });
    NodeId path_ad = preGraph.getOrAddNode({ "path", {1, 4} });

    // 添加 ground rules (超边)
    preGraph.addEdge({edge_ab}, path_ab); // path(a,b) :- edge(a,b)
    preGraph.addEdge({edge_bc}, path_bc); // path(b,c) :- edge(b,c)
    preGraph.addEdge({edge_cd}, path_cd); // path(c,d) :- edge(c,d)

    preGraph.addEdge({path_ab, edge_bc}, path_ac); // path(a,c) :- path(a,b), edge(b,c)
    preGraph.addEdge({path_bc, edge_cd}, path_bd); // path(b,d) :- path(b,c), edge(c,d)
    preGraph.addEdge({path_ac, edge_cd}, path_ad); // path(a,d) :- path(a,c), edge(c,d)


    // =======================================================
    // Turn 1: 初始构建
    // =======================================================
    std::cout << ">>> Turn 1: Initial materialization with facts edge(a,b) and edge(b,c).\n" << std::endl;

    preGraph.seedFacts({edge_ab, edge_bc});
    preGraph.recompute();
    preGraph.materialize(graph);

    printGraphState(1, graph);
    // 预期：deltaInsertNodes 应该包含 edge(1,2), edge(2,3), path(1,2), path(2,3), path(1,3)
    //      deltaDeleteNodes 应该为空

    // =======================================================
    // Turn 2: 增量删除
    // =======================================================
    std::cout << ">>> Turn 2: Retracting fact edge(b,c) and rematerializing.\n" << std::endl;

    preGraph.retractFacts({edge_bc});
    preGraph.recompute();
    preGraph.materialize(graph);

    printGraphState(2, graph);
    // 预期：deltaDeleteNodes 应该包含 path(1,3), path(2,3) (因为它们的推导路径断了)
    //      edge(2,3) 也会被删除，因为它不再是fact，且没有其他推导来源
    //      deltaInsertNodes 应该为空

    // =======================================================
    // Turn 3: 增量插入
    // =======================================================
    std::cout << ">>> Turn 3: Seeding new fact edge(c,d) and rematerializing.\n" << std::endl;

    preGraph.seedFacts({edge_cd}, PreDerivationGraph::SeedMode::Accumulate); // 累加事实
    preGraph.recompute();
    preGraph.materialize(graph);

    printGraphState(3, graph);
    // 预期：deltaInsertNodes 应该包含 edge(3,4), path(3,4).
    //      deltaDeleteNodes 应该为空

    return 0;
}

/// -----

//#include <iostream>
//#include "souffle/problog/PreDerivationGraph.h"   // 上一步给你的 header-only 预设图
//#include "souffle/problog/DerivationGraph.h"      // 你已有的真实导出图
//
//using souffle::RamDomain;
//
//static void showPreDG(const char* title, const PreDerivationGraph& G) {
//    static size_t iteration = 0;
//    std::cout << "\n==== " << title << " (PreDG view) ====\n";
//    auto V = G.view();
//    std::cout << "Alive node count: " << V.nodes().size()
//              << ", Alive edge count: " << V.edges().size() << "\n";
//    // 导出 DOT 到 stdout（也可以写文件）
//    std::ofstream ofs("predg" + std::to_string(iteration++) + ".dot");
//    G.toDot(ofs);
//}
//
//static void materializeAndShowDG(const char* title, const PreDerivationGraph& G) {
//    static size_t iteration = 0;
//    std::cout << "\n==== " << title << " (Materialized DerivationGraph) ====\n";
//    DerivationGraph DG;
//    G.materialize(DG);
//
//    try {
//        DG.dumpDot("dg" + std::to_string(iteration++) + ".dot");
//    } catch (...) {
//        std::cout << "(hint) If your DerivationGraph uses a different API to export DOT/JSON, "
//                     "replace this with the appropriate call.\n";
//    }
//}
//
//int main() {
//    PreDerivationGraph G;
//
//    // === 1) 定义若干 ground 原子（关系名 + 常量参数，参数类型为 RamDomain） ===
//    //    为了便于观察，构造几条可达路径 + 一条不会满足的边
//    NodeId S1 = G.getOrAddNode({"S", {RamDomain(1)}});
//    NodeId T2 = G.getOrAddNode({"T", {RamDomain(2)}});
//    NodeId R3 = G.getOrAddNode({"R", {RamDomain(3)}});
//    NodeId U4 = G.getOrAddNode({"U", {RamDomain(4)}});
//    NodeId V5 = G.getOrAddNode({"V", {RamDomain(5)}});
//    NodeId W8 = G.getOrAddNode({"W", {RamDomain(8)}});
//    NodeId R9 = G.getOrAddNode({"R", {RamDomain(9)}});
//    NodeId U7 = G.getOrAddNode({"U", {RamDomain(7)}}); // 故意留一条不可达的头结点
//
//    // === 2) 定义 ground 规则（超边）：inputs -> output ===
//    // e0: S(1) & T(2) -> R(3)
//    EdgeId e0 = G.addEdge({S1, T2}, R3);
//    // e1: R(3) & S(1) -> U(4)
//    EdgeId e1 = G.addEdge({R3, S1}, U4);
//    // e2: T(2) -> U(4)  （另一条到 U(4) 的路径）
//    EdgeId e2 = G.addEdge({T2}, U4);
//    // e3: V(5) -> U(7)  （不会满足的边，用来测试剪枝）
//    EdgeId e3 = G.addEdge({V5}, U7);
//    // e4: W(8) -> R(9)  （晚点提供 W(8) 的输入）
//    EdgeId e4 = G.addEdge({W8}, R9);
//
//    // === 3) 无输入，先看看视图 ===
//    G.seedFacts({}, PreDerivationGraph::SeedMode::Replace);
//    G.recompute();
//    showPreDG("A) no inputs", G);
//    materializeAndShowDG("A) no inputs", G);
//
//    // === 4) 累加输入：只给 S(1) ===
//    G.seedFacts({S1}, PreDerivationGraph::SeedMode::Accumulate);
//    G.recompute();
//    showPreDG("B) inputs = {S(1)}", G);
//    materializeAndShowDG("B) inputs = {S(1)}", G);
//
//    // === 5) 再追加 T(2)：应当推出 R(3)，进而 U(4)（e1/e2 均能使 U(4) 可达）===
//    G.seedFacts({T2}, PreDerivationGraph::SeedMode::Accumulate);
//    G.recompute();
//    showPreDG("C) inputs = {S(1), T(2)}", G);
//    materializeAndShowDG("C) inputs = {S(1), T(2)}", G);
//
//
//    // === 7) 禁用一条边（e2: T(2) -> U(4)），仍可经 e1 推出 U(4) ===
//    G.removeEdge(e2);
//    G.recompute();
//    showPreDG("E) disable e2, still derive U(4) via e1", G);
//    materializeAndShowDG("E) disable e2, still derive U(4) via e1", G);
//
//    // === 8) 替换输入为 {T(2)}（清空此前输入，仅保留 T(2)）===
//    G.seedFacts({T2}, PreDerivationGraph::SeedMode::Replace);
//    G.recompute();
//    showPreDG("F) replace inputs = {T(2)}", G);
//    materializeAndShowDG("F) replace inputs = {T(2)}", G);
//
//    // 结束
//    return 0;
//}
