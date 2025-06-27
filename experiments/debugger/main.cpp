#include "souffle/problog/debug/Debugger.h"
#include <thread>
#include <chrono>

int main() {
    Debugger& dbg = Debugger::getInstance();

    // Start a turn
    dbg.startTurn("FULL");
    dbg.addInfo("turn_param", "test_mode");

    // Stage 1
    dbg.startStage(StageKind::SEMINAIVE_FULL);
    dbg.addInfo("stage1_param", "alpha");

    for (int i = 0; i < 3; ++i) {
        dbg.startIteration();
        std::this_thread::sleep_for(std::chrono::milliseconds(100 + i * 50));
        dbg.addInfo("iteration_stat", std::to_string(i * 10));
        dbg.logMessage(Level::DEBUG, "Iteration running...");
        dbg.endIteration();
    }

    dbg.addInfo("stage1_output", "final facts");
    dbg.endStage();

    // Stage 2
    dbg.startStage(StageKind::PRUNING_FULL);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    dbg.addInfo("pruned_rules", "50");
    dbg.logMessage(Level::INFO, "Stage 2 pruning done.");
    dbg.endStage();

    dbg.endTurn();

    dbg.printReport(std::cout);
    return 0;
}
