#include "souffle/problog/PreGraphLiveSet.h"

#include <deque>
#include <utility>
#include <vector>

namespace souffle::problog {
namespace {

bool tupleIsExcluded(
    const UntypedTuple &tuple,
    const std::unordered_set<std::string> &excludedRelations,
    const std::unordered_set<UntypedTuple> &templateBackedTuples) {
  return excludedRelations.count(tuple.relation_name) > 0 ||
         templateBackedTuples.count(tuple) > 0;
}

} // namespace

PreGraphLiveSetResult collectPreGraphLiveSet(
    const MaterializedDerivationMap &derivations,
    const RuleManager &ruleManager,
    const std::unordered_set<UntypedTuple> &roots,
    const std::unordered_set<UntypedTuple> &factTuples,
    const std::unordered_set<std::string> &deterministicRelations,
    const std::unordered_set<std::string> &excludedRelations,
    const std::unordered_set<UntypedTuple> &templateBackedTuples) {
  PreGraphLiveSetResult result;
  result.sourceTuples = derivations.size();
  for (const auto &[tuple, applications] : derivations) {
    (void)tuple;
    if (applications != nullptr) {
      result.sourceRuleApplications += applications->size();
    }
  }

  std::unordered_set<UntypedTuple> universe = factTuples;
  universe.reserve(universe.size() + derivations.size());
  for (const auto &[tuple, applications] : derivations) {
    (void)applications;
    if (!tupleIsExcluded(tuple, excludedRelations, templateBackedTuples)) {
      universe.insert(tuple);
    }
  }

  std::deque<UntypedTuple> pending;
  for (const auto &root : roots) {
    if (!tupleIsExcluded(root, excludedRelations, templateBackedTuples)) {
      universe.insert(root);
      pending.push_back(root);
    }
  }

  while (!pending.empty()) {
    const auto tuple = pending.front();
    pending.pop_front();
    if (!result.tuples.insert(tuple).second) {
      continue;
    }

    if (factTuples.count(tuple) > 0 ||
        deterministicRelations.count(tuple.relation_name) > 0) {
      continue;
    }
    const auto tupleIt = derivations.find(tuple);
    if (tupleIt == derivations.end() || tupleIt->second == nullptr) {
      continue;
    }

    bool representedAsFact = false;
    for (const auto &application : *tupleIt->second) {
      const Rule *rule =
          ruleManager.getRule(static_cast<std::size_t>(application.ruleId));
      if (rule != nullptr && rule->isFact()) {
        result.ruleApplications.insert(application);
        representedAsFact = true;
      }
    }
    if (representedAsFact) {
      continue;
    }

    for (const auto &application : *tupleIt->second) {
      const Rule *rule =
          ruleManager.getRule(static_cast<std::size_t>(application.ruleId));
      if (rule == nullptr ||
          rule->getVars().size() != application.varValuesPure.size()) {
        result.reason = "invalid materialized rule application";
        return result;
      }
      if (!rule->getAggregates().empty()) {
        result.reason = "aggregate rule in live closure";
        return result;
      }

      const auto variables = rule->getVars();
      std::vector<UntypedTuple> concreteBodies;
      concreteBodies.reserve(rule->getBodyAtoms().size());
      bool selfDependent = false;
      for (const auto &atom : rule->getBodyAtoms()) {
        UntypedTuple bodyTuple{
            atom.getRelation(),
            atom.instantiatedFields(variables, application.varValuesPure)};
        if (templateBackedTuples.count(bodyTuple) > 0) {
          continue;
        }
        if (excludedRelations.count(bodyTuple.relation_name) > 0) {
          if (atom.isNegatedAtom()) {
            continue;
          }
          result.reason = "live rule references excluded concrete relation";
          return result;
        }
        if (atom.isNegatedAtom() && universe.count(bodyTuple) == 0) {
          continue;
        }
        if (bodyTuple == tuple) {
          selfDependent = true;
        }
        concreteBodies.push_back(std::move(bodyTuple));
      }
      if (selfDependent) {
        ++result.selfDependentApplicationsSkipped;
        continue;
      }

      result.ruleApplications.insert(application);
      result.bodyReferences += concreteBodies.size();
      for (auto &bodyTuple : concreteBodies) {
        universe.insert(bodyTuple);
        pending.push_back(std::move(bodyTuple));
      }
    }
  }

  for (const auto &root : roots) {
    result.roots += result.tuples.count(root) > 0 ? 1 : 0;
  }
  result.handled = true;
  result.reason = "ok";
  return result;
}

} // namespace souffle::problog
