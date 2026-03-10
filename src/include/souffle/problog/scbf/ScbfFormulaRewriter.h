#pragma once

#include "souffle/problog/scbf/ScbfFormula.h"

#include <string>

namespace souffle::problog::scbf {

struct ScbfFormulaRewriteConfig {
    bool useGraphRewriterAligned = true;
    std::size_t maxIterations = 8;
    bool foldConstants = true;
    bool collapseSingleLocalAliases = true;
    bool pruneUnreachableNodes = true;

    // Aligned with GraphRewriter::RewriteFeatureFlags.
    bool enableSingleHyperedge = true;
    bool enableAllFactsToSO = true;
    bool enableLinearTwoEdge = true;
    bool enableParallelEdge = true;
    bool enableFanOutConverge = true;
    bool enableGeneral = true;
    bool enableCompaction = true;
    bool forceCompleteSisoDetect = false;

    enum class SplitMode {
        None = 0,
        Naive,
        Complete,
    };
    SplitMode splitMode = SplitMode::Naive;
    std::size_t splitMaxNewNodesPerPass = 5000;
    std::size_t splitMaxNewEdgesPerPass = 50000;
    std::size_t splitMaxGroupsPerNode = 2;
    std::size_t splitMinGroupEdges = 1;
    bool enableCleanupIsolated = true;

    bool dumpBeforeRewrite = false;
    bool dumpAfterRewrite = false;
    std::string dumpDir;
    std::string dumpPrefix = "scbf";
};

struct ScbfFormulaRewriteStats {
    std::size_t iterations = 0;
    std::size_t targetsProcessed = 0;
    std::size_t nodesBefore = 0;
    std::size_t nodesAfter = 0;
    std::size_t rulesBefore = 0;
    std::size_t rulesAfter = 0;
    std::size_t literalsBefore = 0;
    std::size_t literalsAfter = 0;
    std::size_t rulesDroppedUnsat = 0;
    std::size_t literalsDroppedConstTrue = 0;
    std::size_t aliasesApplied = 0;
    std::size_t unreachableNodesRemoved = 0;

    // GraphRewriter-aligned pass stats.
    std::size_t graphRewriteRuns = 0;
    std::size_t graphRewriteIterations = 0;
    std::size_t graphRegionsRewritten = 0;
    std::size_t graphNodesRemoved = 0;
    std::size_t graphEdgesRemoved = 0;
    std::size_t graphEdgesAdded = 0;
    std::size_t graphRandomVarsBefore = 0;
    std::size_t graphRandomVarsAfter = 0;
};

ScbfTargetFormula rewriteScbfTargetFormula(const ScbfTargetFormula& input,
        const ScbfFormulaRewriteConfig& config = {}, ScbfFormulaRewriteStats* stats = nullptr);

ScbfStratumFormulaBundle rewriteScbfStratumFormulaBundle(const ScbfStratumFormulaBundle& input,
        const ScbfFormulaRewriteConfig& config = {}, ScbfFormulaRewriteStats* stats = nullptr);

std::string summarizeScbfFormulaRewriteStats(const ScbfFormulaRewriteStats& stats);

}  // namespace souffle::problog::scbf
