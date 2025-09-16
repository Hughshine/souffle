// compilation pipeline
#include "souffle/CompiledSouffle.h"
#include "souffle/cli/Cli.h"
#include "souffle/io/IOSystem.h"
#include "souffle/problog/Atom.h"
#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/ForwardCompilation.h"
#include "souffle/problog/PreDerivationGraph.h"
#include "souffle/problog/Rule.h"
#include "souffle/problog/RuleManager.h"
#include "souffle/problog/debug/Debugger.h"
#include "souffle/problog/formula/CuddManager.h"
#include "souffle/problog/formula/SddManager.h"
#include <cassert>
#include <fstream>
#include <string>
#include <unordered_map>
#include <vector>
// ground full: preDG -> preDG updating (reading input relations)
// -> dg -> dg pruning -> formula construction -> wmc

void fullComp(IncrementalDerivationGraph& dg, souffle::CmdOptions& opt, std::vector<std::string> outputRelations = {}) {
    dg.dumpDot("dg_before_prune-0.dot");
    Debugger::getInstance().startStage(StageKind::PRUNING_FULL);
    auto view = dg.prune(outputRelations);
    Debugger::getInstance().endStage();
    dg.dumpDot("dg_after_prune.dot");

    Debugger::getInstance().startStage(StageKind::FORWARD_COMPILATION_FULL);
    std::cout << "Trying to construct formulas\n";
    std::map<NodePtr, BddNodeRef> nodeFormulas;std::map<EdgePtr, BddNodeRef> edgeFormulas;WeightedBDDManager bddManager;
    buildFormulasCyclewise(view, bddManager, nodeFormulas, edgeFormulas);
    Debugger::getInstance().endStage();

    Debugger::getInstance().startStage(StageKind::WEIGHTED_MODEL_COUNTING_FULL);
    std::cout << "Trying to calculate probability\n";
    for (const auto& [node, bdd] : nodeFormulas) {
        auto prob = bddManager.computeWeightedModelCount(bdd);
        probResult[node] = prob;
    }
    Debugger::getInstance().endStage();

    debugger.startStage(StageKind::IO_DUMP_FULL);
    dumpProbabilities(probResult, opt.getOutputFileDir());
    debugger.endStage();
}


void fullCompOnDemand(IncrementalDerivationGraph& dg, souffle::CmdOptions& opt, std::vector<std::string> outputRelations = {}) {
    dg.dumpDot("dg_before_prune-0.dot");
    Debugger::getInstance().startStage(StageKind::PRUNING_FULL);
    auto view = dg.prune(outputRelations);
    Debugger::getInstance().endStage();
    dg.dumpDot("dg_after_prune.dot");

    std::map<NodePtr, BddNodeRef> nodeFormulas;std::map<EdgePtr, BddNodeRef> edgeFormulas;WeightedBDDManager bddManager;
    Debugger::getInstance().startStage(StageKind::FORWARD_COMPILATION_FULL);
    std::cout << "Trying to construct formulas\n";
    buildFormulasCyclewiseOnDemand(view, bddManager, nodeFormulas, edgeFormulas);
    Debugger::getInstance().endStage();

    debugger.startStage(StageKind::IO_DUMP_FULL);
    dumpProbabilities(probResult, opt.getOutputFileDir());
    debugger.endStage();
}

// normal full:

// ground incr:
// preDG -> preDG updating (by delta relations) -> inc-dg
// -> inc-dg pruning -> inc formula construction -> inc wmc