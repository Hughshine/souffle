#include "souffle/problog/debug/Debugger.h"
#include <thread>     // for std::this_thread::sleep_for
#include <chrono>     // for std::chrono::milliseconds
#include <cstdlib>    // for rand()

int main() {
    Debugger dbg;
    DebuggerFinalizer guard(dbg);  // 自动记录程序运行时间 + 输出结果到 debug_output.json

    // 设置 meta 信息
    dbg.setMetaConfig("representation", "BDD");
    dbg.setMetaConfig("approximate", "false");
    dbg.setMetaConfig("input_size", "100000");

    const std::string stage = "BuildFormulas";
    dbg.startStage(stage);

    // 模拟多个迭代轮次
    for (int i = 0; i < 3; ++i) {
        auto start = std::chrono::steady_clock::now();

        // 模拟计算逻辑
        std::this_thread::sleep_for(std::chrono::milliseconds(50 + rand() % 50));

        auto end = std::chrono::steady_clock::now();
        double dur = std::chrono::duration<double>(end - start).count();

        // 构造迭代信息
        Debugger::IterationInfo info;
        info.iteration = i;
        info.duration_sec = dur;
        info.memory_kb = dbg.toJson()["meta"]["peak_memory_kb"].int_value() + rand() % 100;
        info.bdd_node_count = 1000 + i * 300;

        // 偶尔包含 GC / 重排时间
        if (i == 1) {
            info.gc_time = 0.012;
            info.reordering_time = 0.045;
        }

        dbg.addIteration(stage, info);
        dbg.log(Debugger::Level::INFO, stage, "Completed iteration " + std::to_string(i));
    }

    dbg.endStage(stage, /*bdd_nodes=*/1900);
    return 0;
}