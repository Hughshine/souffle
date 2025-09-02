#include <iostream>
#include "souffle/problog/PreDerivationGraph.h"   // 上一步给你的 header-only 预设图
#include "souffle/problog/DerivationGraph.h"      // 你已有的真实导出图

using souffle::RamDomain;

static void showPreDG(const char* title, const PreDerivationGraph& G) {
    static size_t iteration = 0;
    std::cout << "\n==== " << title << " (PreDG view) ====\n";
    auto V = G.view();
    std::cout << "Alive node count: " << V.nodes().size()
              << ", Alive edge count: " << V.edges().size() << "\n";
    // 导出 DOT 到 stdout（也可以写文件）
    std::ofstream ofs("predg" + std::to_string(iteration++) + ".dot");
    G.toDot(ofs, V);
}

static void materializeAndShowDG(const char* title, const PreDerivationGraph& G) {
    static size_t iteration = 0;
    std::cout << "\n==== " << title << " (Materialized DerivationGraph) ====\n";
    DerivationGraph DG;
    G.materialize(DG);

    try {
        DG.dumpDot("dg" + std::to_string(iteration++) + ".dot");
    } catch (...) {
        std::cout << "(hint) If your DerivationGraph uses a different API to export DOT/JSON, "
                     "replace this with the appropriate call.\n";
    }
}

int main() {
    PreDerivationGraph G;

    // === 1) 定义若干 ground 原子（关系名 + 常量参数，参数类型为 RamDomain） ===
    //    为了便于观察，构造几条可达路径 + 一条不会满足的边
    NodeId S1 = G.getOrAddNode({"S", {RamDomain(1)}});
    NodeId T2 = G.getOrAddNode({"T", {RamDomain(2)}});
    NodeId R3 = G.getOrAddNode({"R", {RamDomain(3)}});
    NodeId U4 = G.getOrAddNode({"U", {RamDomain(4)}});
    NodeId V5 = G.getOrAddNode({"V", {RamDomain(5)}});
    NodeId W8 = G.getOrAddNode({"W", {RamDomain(8)}});
    NodeId R9 = G.getOrAddNode({"R", {RamDomain(9)}});
    NodeId U7 = G.getOrAddNode({"U", {RamDomain(7)}}); // 故意留一条不可达的头结点

    // === 2) 定义 ground 规则（超边）：inputs -> output ===
    // e0: S(1) & T(2) -> R(3)
    EdgeId e0 = G.addEdge({S1, T2}, R3);
    // e1: R(3) & S(1) -> U(4)
    EdgeId e1 = G.addEdge({R3, S1}, U4);
    // e2: T(2) -> U(4)  （另一条到 U(4) 的路径）
    EdgeId e2 = G.addEdge({T2}, U4);
    // e3: V(5) -> U(7)  （不会满足的边，用来测试剪枝）
    EdgeId e3 = G.addEdge({V5}, U7);
    // e4: W(8) -> R(9)  （晚点提供 W(8) 的输入）
    EdgeId e4 = G.addEdge({W8}, R9);

    // === 3) 无输入，先看看视图 ===
    G.seedFacts({}, PreDerivationGraph::SeedMode::Replace);
    G.recompute();
    showPreDG("A) no inputs", G);
    materializeAndShowDG("A) no inputs", G);

    // === 4) 累加输入：只给 S(1) ===
    G.seedFacts({S1}, PreDerivationGraph::SeedMode::Accumulate);
    G.recompute();
    showPreDG("B) inputs = {S(1)}", G);
    materializeAndShowDG("B) inputs = {S(1)}", G);

    // === 5) 再追加 T(2)：应当推出 R(3)，进而 U(4)（e1/e2 均能使 U(4) 可达）===
    G.seedFacts({T2}, PreDerivationGraph::SeedMode::Accumulate);
    G.recompute();
    showPreDG("C) inputs = {S(1), T(2)}", G);
    materializeAndShowDG("C) inputs = {S(1), T(2)}", G);


    // === 7) 禁用一条边（e2: T(2) -> U(4)），仍可经 e1 推出 U(4) ===
    G.removeEdge(e2);
    G.recompute();
    showPreDG("E) disable e2, still derive U(4) via e1", G);
    materializeAndShowDG("E) disable e2, still derive U(4) via e1", G);

    // === 8) 替换输入为 {T(2)}（清空此前输入，仅保留 T(2)）===
    G.seedFacts({T2}, PreDerivationGraph::SeedMode::Replace);
    G.recompute();
    showPreDG("F) replace inputs = {T(2)}", G);
    materializeAndShowDG("F) replace inputs = {T(2)}", G);

    // 结束
    return 0;
}
