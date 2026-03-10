#pragma once

#include "souffle/problog/scbf/ScbfFormulaRewriter.h"
#include "souffle/problog/scbf/ScbfGlobalFormula.h"

#include <string>

namespace souffle::problog::scbf {

struct ScbfGlobalFormulaRewriteStats {
    double materializeMs = 0.0;
    double graphRewriteMs = 0.0;
    double rebuildMs = 0.0;
    double validateMs = 0.0;

    std::size_t componentsBefore = 0;
    std::size_t componentsAfter = 0;
    std::size_t nodesBefore = 0;
    std::size_t nodesAfter = 0;
    std::size_t rulesBefore = 0;
    std::size_t rulesAfter = 0;
    std::size_t literalsBefore = 0;
    std::size_t literalsAfter = 0;
    std::size_t importsBefore = 0;
    std::size_t importsAfter = 0;
    std::size_t shadowNodesAfter = 0;

    std::size_t graphRewriteRuns = 0;
    std::size_t graphRewriteIterations = 0;
    std::size_t graphRegionsRewritten = 0;
    std::size_t graphNodesRemoved = 0;
    std::size_t graphEdgesRemoved = 0;
    std::size_t graphEdgesAdded = 0;
    std::size_t graphRandomVarsBefore = 0;
    std::size_t graphRandomVarsAfter = 0;
};

ScbfGlobalFormula rewriteScbfGlobalFormula(const ScbfGlobalFormula& input,
        const ScbfFormulaRewriteConfig& config = {}, ScbfGlobalFormulaRewriteStats* stats = nullptr);

std::string summarizeScbfGlobalFormulaRewriteStats(const ScbfGlobalFormulaRewriteStats& stats);

}  // namespace souffle::problog::scbf
