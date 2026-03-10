#pragma once

#include "souffle/problog/scbf/ScbfFormula.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace souffle::problog::scbf {

struct ScbfGlobalFormulaNode;
struct ScbfGlobalFormulaRule;
struct ScbfGlobalSummaryNode;
struct ScbfGlobalSummaryRule;

struct ScbfGlobalLiteralRef {
    ScbfGlobalFormulaNode* node = nullptr;
    bool negated = false;
    double constantProbability = 0.0;

    bool isConstant() const {
        return node == nullptr;
    }
};

struct ScbfGlobalExportRef {
    enum class Kind {
        Node,
        Constant,
        Summary,
        Elided,
    };

    Kind kind = Kind::Node;
    ScbfGlobalFormulaNode* node = nullptr;
    ScbfGlobalSummaryNode* summary = nullptr;
    double constantProbability = 0.0;

    bool isNode() const {
        return kind == Kind::Node;
    }

    bool isConstant() const {
        return kind == Kind::Constant;
    }

    bool isSummary() const {
        return kind == Kind::Summary;
    }

    bool isElided() const {
        return kind == Kind::Elided;
    }
};

struct ScbfGlobalSummaryLiteral {
    ScbfGlobalExportRef ref;
    bool negated = false;
};

struct ScbfGlobalSummaryRule {
    EdgePtr sourceEdge;
    bool deterministic = true;
    double edgeProbability = 1.0;
    ScbfGlobalSummaryNode* head = nullptr;
    std::vector<ScbfGlobalSummaryLiteral> bodyLiterals;
};

struct ScbfGlobalSummaryNode {
    NodePtr originalNode;
    std::size_t cycleId = 0;
    std::size_t topoIndex = 0;
    std::size_t componentId = 0;
    std::size_t localIndex = 0;
    bool isTargetSummary = false;
    std::vector<ScbfGlobalSummaryRule*> incomingRules;
};

struct ScbfGlobalFormulaRule {
    EdgePtr sourceEdge;
    bool deterministic = true;
    double edgeProbability = 1.0;
    ScbfGlobalFormulaNode* head = nullptr;
    std::vector<ScbfGlobalLiteralRef> bodyLiterals;
};

struct ScbfGlobalFormulaNode {
    NodePtr originalNode;
    std::size_t cycleId = 0;
    std::size_t topoIndex = 0;
    std::size_t componentId = 0;
    std::size_t localIndex = 0;
    bool isFact = false;
    double factProbability = 0.0;
    bool needOutput = false;
    bool isTargetRoot = false;
    bool isBoundaryRoot = false;
    std::vector<ScbfGlobalFormulaRule*> incomingRules;
};

struct ScbfGlobalFormulaComponent {
    std::size_t cycleId = 0;
    std::size_t topoIndex = 0;
    std::size_t componentId = 0;
    NodePtr target;
    ScbfGlobalFormulaNode* targetNode = nullptr;
    ScbfGlobalExportRef exportedValue;
    std::vector<ScbfGlobalFormulaNode*> localNodes;
    std::vector<ScbfGlobalFormulaNode*> importNodes;
    std::vector<ScbfGlobalSummaryNode*> summaryNodes;
    std::vector<ScbfGlobalFormulaRule*> rules;
};

struct ScbfGlobalFormula {
    ScbfProgram program;
    std::vector<ScbfGlobalFormulaComponent> components;
    std::vector<std::unique_ptr<ScbfGlobalFormulaNode>> ownedNodes;
    std::vector<std::unique_ptr<ScbfGlobalFormulaRule>> ownedRules;
    std::vector<std::unique_ptr<ScbfGlobalSummaryNode>> ownedSummaryNodes;
    std::vector<std::unique_ptr<ScbfGlobalSummaryRule>> ownedSummaryRules;
    std::unordered_map<NodePtr, std::vector<ScbfGlobalFormulaNode*>> instancesByOriginalNode;
    std::unordered_map<NodePtr, ScbfGlobalExportRef> exportedTargetsByOriginalNode;
    std::vector<ScbfGlobalExportRef> rootsInTopoOrder;
};

ScbfGlobalFormula buildScbfGlobalFormula(
        const DerivationGraphViewInterface& view,
        const ScbfProgram& program);

ScbfGlobalFormula buildScbfGlobalFormula(const DerivationGraphViewInterface& view);

bool validateScbfGlobalFormula(const ScbfGlobalFormula& global, std::string* errorMessage = nullptr);

std::string summarizeScbfGlobalFormula(const ScbfGlobalFormula& global);

std::string toScbfGlobalFormulaJson(const ScbfGlobalFormula& global);

bool dumpScbfGlobalFormulaJson(const ScbfGlobalFormula& global, const std::string& outputPath);

bool dumpScbfGlobalFormulaDot(const ScbfGlobalFormula& global, const std::string& outputPath);

}  // namespace souffle::problog::scbf
