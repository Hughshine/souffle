#pragma once

#include "souffle/problog/scbf/ScbfFormula.h"
#include "souffle/problog/scbf/ScbfFormulaRewriter.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace souffle::problog::scbf {

struct ScbfEvaluatorConfig {
    std::size_t maxIterations = 256;
    double epsilon = 1e-10;
    bool failOnMissingImports = false;
    bool clampToUnitInterval = true;
    bool rewriteBeforeEvaluate = false;
    ScbfFormulaRewriteConfig rewriteConfig;
    bool dumpFormulaBundle = false;
    std::string dumpDir;
    std::string dumpPrefix = "scbf_eval";
};

struct ScbfEvaluatorTargetStats {
    std::size_t cycleId = 0;
    std::size_t targetIndex = 0;
    NodePtr target;
    std::size_t iterations = 0;
    double maxDelta = 0.0;
    bool converged = false;
    std::size_t missingImports = 0;
};

struct ScbfEvaluatorResult {
    std::unordered_map<NodePtr, double> nodeProbabilities;
    std::unordered_map<NodePtr, double> exportProbabilities;
    std::vector<ScbfEvaluatorTargetStats> targetStats;
    std::size_t strataEvaluated = 0;
    std::size_t targetsEvaluated = 0;
    std::size_t missingImports = 0;
    bool converged = true;
    bool rewriteApplied = false;
    ScbfFormulaRewriteStats rewriteStats;
};

ScbfEvaluatorResult evaluateScbfProgramProbability(const DerivationGraphViewInterface& view,
        const ScbfProgram& program, const ScbfEvaluatorConfig& config = {});

ScbfEvaluatorResult evaluateScbfProgramProbabilityViaFormulaBundle(const DerivationGraphViewInterface& view,
        const ScbfProgram& program, const ScbfEvaluatorConfig& config = {});

std::string summarizeScbfEvaluatorResult(const ScbfEvaluatorResult& result);

}  // namespace souffle::problog::scbf
