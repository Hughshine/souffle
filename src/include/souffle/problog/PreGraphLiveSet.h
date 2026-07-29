#ifndef SOUFFLE_PROBLOG_PRE_GRAPH_LIVE_SET_H
#define SOUFFLE_PROBLOG_PRE_GRAPH_LIVE_SET_H

#include "souffle/Derivation.h"
#include "souffle/problog/RuleManager.h"

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace souffle::problog {

using MaterializedDerivationMap =
    std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>;

/**
 * Concrete tuple/rule-application closure required by the output/query roots.
 * Template-backed tuples are opaque symbolic references and therefore do not
 * enter the concrete closure.
 */
struct PreGraphLiveSetResult {
  bool handled = false;
  std::string reason;
  std::size_t sourceTuples = 0;
  std::size_t sourceRuleApplications = 0;
  std::size_t roots = 0;
  std::size_t bodyReferences = 0;
  std::size_t selfDependentApplicationsSkipped = 0;
  std::unordered_set<UntypedTuple> tuples;
  std::unordered_set<RuleApplication> ruleApplications;
};

PreGraphLiveSetResult collectPreGraphLiveSet(
    const MaterializedDerivationMap &derivations,
    const RuleManager &ruleManager,
    const std::unordered_set<UntypedTuple> &roots,
    const std::unordered_set<UntypedTuple> &factTuples = {},
    const std::unordered_set<std::string> &deterministicRelations = {},
    const std::unordered_set<std::string> &excludedRelations = {},
    const std::unordered_set<UntypedTuple> &templateBackedTuples = {});

} // namespace souffle::problog

#endif // SOUFFLE_PROBLOG_PRE_GRAPH_LIVE_SET_H
