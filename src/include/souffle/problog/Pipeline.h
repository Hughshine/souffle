#pragma once

#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "souffle/Derivation.h"
#include "souffle/CompiledOptions.h"
#include "souffle/CompiledSouffle.h"

class RuleManager;
class QueryManager;

namespace souffle::problog {

std::string makeOutputPath(const CmdOptions& opt, const std::string& filename);

// full-only is a compile-time flag; expose it to the runtime pipeline
// to guard features that are only safe in full-only mode.
void setFullOnlyMode(bool enabled);
bool isFullOnlyMode();

void runPipeline(
        const CmdOptions& opt,
        SouffleProgram& program,
        RuleManager& ruleManager,
        QueryManager& queryManager,
        const std::unordered_map<UntypedTuple, double>& factProb,
        const std::vector<std::pair<UntypedTuple, bool>>& evidences,
        bool enableOnlineCli = false);

}  // namespace souffle::problog
