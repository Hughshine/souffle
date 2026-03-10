#pragma once

#include "souffle/problog/DerivationGraph.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace souffle::problog::scbf {

struct ScbfExportRef {
    std::size_t producerCycleId = 0;
    NodePtr node;
};

struct ScbfTargetPlan {
    NodePtr target;
    std::vector<EdgePtr> localIncomingEdges;
    std::vector<ScbfExportRef> imports;
};

struct ScbfFormulaArenaPlan {
    std::size_t cycleId = 0;
    std::vector<ScbfTargetPlan> targets;
    bool allowIntraTargetSharing = true;
    bool allowCrossTargetSharing = false;
};

struct ScbfStratum {
    std::size_t cycleId = 0;
    std::size_t topoIndex = 0;
    std::vector<NodePtr> localNodes;
    std::vector<EdgePtr> localEdges;
    std::vector<NodePtr> boundaryInNodes;
    std::vector<NodePtr> boundaryOutNodes;
    std::vector<NodePtr> outputNodes;
    std::vector<std::size_t> dependencies;
    std::vector<std::size_t> reverseDependencies;
};

struct ScbfProgram {
    std::vector<std::size_t> topoOrderCycleIds;
    std::vector<ScbfStratum> strata;
    std::unordered_map<std::size_t, std::size_t> cycleIdToTopoIndex;
    std::unordered_map<NodePtr, std::size_t> nodeToCycleId;
    std::unordered_map<EdgePtr, std::size_t> edgeToCycleId;
};

ScbfProgram buildScbfProgram(const DerivationGraphViewInterface& view);

ScbfFormulaArenaPlan buildScbfFormulaArenaPlan(
        const DerivationGraphViewInterface& view,
        const ScbfProgram& program,
        std::size_t cycleId);

bool validateScbfProgram(const DerivationGraphViewInterface& view, const ScbfProgram& program,
        std::string* errorMessage = nullptr);

std::string summarizeScbfProgram(const ScbfProgram& program);

}  // namespace souffle::problog::scbf

