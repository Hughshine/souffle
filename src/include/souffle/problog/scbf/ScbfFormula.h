#pragma once

#include "souffle/problog/scbf/ScbfIr.h"

#include <string>
#include <vector>

namespace souffle::problog::scbf {

enum class ScbfLiteralSource {
    LocalNode = 0,
    ImportNode,
    Constant,
};

struct ScbfLiteralRef {
    ScbfLiteralSource source = ScbfLiteralSource::Constant;
    std::size_t index = 0;
    bool negated = false;
    double constantProbability = 0.0;
};

struct ScbfRuleEquation {
    EdgePtr sourceEdge;
    bool deterministic = true;
    double edgeProbability = 1.0;
    std::vector<ScbfLiteralRef> bodyLiterals;
};

struct ScbfLocalNodeDef {
    NodePtr node;
    bool isFact = false;
    double factProbability = 0.0;
    bool needOutput = false;
};

struct ScbfImportDef {
    NodePtr node;
    std::size_t producerCycleId = 0;
};

struct ScbfTargetFormula {
    NodePtr target;
    std::size_t targetLocalIndex = 0;
    std::vector<ScbfLocalNodeDef> localNodes;
    std::vector<ScbfImportDef> imports;
    std::vector<std::vector<ScbfRuleEquation>> localNodeRules;
};

struct ScbfStratumFormulaBundle {
    std::size_t cycleId = 0;
    std::size_t topoIndex = 0;
    bool allowCrossTargetSharing = false;
    std::vector<ScbfTargetFormula> targets;
};

ScbfStratumFormulaBundle buildScbfStratumFormulaBundle(const DerivationGraphViewInterface& view,
        const ScbfProgram& program, std::size_t cycleId);

bool validateScbfStratumFormulaBundle(const DerivationGraphViewInterface& view, const ScbfProgram& program,
        const ScbfStratumFormulaBundle& bundle, std::string* errorMessage = nullptr);

std::string summarizeScbfStratumFormulaBundle(const ScbfStratumFormulaBundle& bundle);

std::string toScbfStratumFormulaBundleJson(const ScbfStratumFormulaBundle& bundle);
bool dumpScbfStratumFormulaBundleJson(
        const ScbfStratumFormulaBundle& bundle, const std::string& outputPath);
bool dumpScbfStratumFormulaBundleDot(
        const ScbfStratumFormulaBundle& bundle, const std::string& outputPath);

}  // namespace souffle::problog::scbf
