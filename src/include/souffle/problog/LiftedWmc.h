#ifndef SOUFFLE_PROBLOG_LIFTED_WMC_H
#define SOUFFLE_PROBLOG_LIFTED_WMC_H

#include "souffle/RamTypes.h"
#include "souffle/CompiledOptions.h"
#include "souffle/Derivation.h"
#include "souffle/SouffleInterface.h"
#include "souffle/problog/RuleManager.h"
#include "souffle/problog/ExactProvenanceTemplate.h"

#include <cstddef>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class WorkingDerivationGraph;
class WorkingSubgraphView;

namespace souffle::problog {

struct LiftedWmcResult {
  bool handled = false;
  bool complete = false;
  std::string reason;
  std::size_t outputTuples = 0;
  std::size_t liftedOutputTuples = 0;
  std::size_t concreteOutputTuples = 0;
  std::size_t relationTemplates = 0;
  std::size_t abstractNodes = 0;
  std::size_t abstractEdges = 0;
  std::size_t symbolicVariables = 0;
  std::size_t bddNodes = 0;
  std::size_t witnessIndexedTemplatesCount = 0;
  std::size_t witnessIndexedRows = 0;
  std::size_t witnessIndexedTupleFormulas = 0;
  std::size_t witnessIndexedTemplateDdNodes = 0;
  std::size_t templateBackedTuples = 0;
  std::size_t templateDefinitions = 0;
  std::size_t templateEventBindings = 0;
  std::size_t directPreflightRelations = 0;
  std::size_t directEvaluatedRelations = 0;
  std::size_t directPreflightTuples = 0;
  std::size_t directPreflightRows = 0;
  std::size_t composedBodyReferences = 0;
  std::size_t partialTemplateTuples = 0;
  std::size_t partialTemplateRuleApplications = 0;
  std::size_t wmcFamilyCacheHits = 0;
  std::size_t wmcFamilyCacheMisses = 0;
  double directMultiWitnessMs = 0.0;
  double directPreflightMs = 0.0;
  double directEvaluationMs = 0.0;
  double eligibilityMs = 0.0;
  double abstractGraphMs = 0.0;
  double symbolicDdMs = 0.0;
  double instantiateWmcMs = 0.0;
  std::string executionMode;
  std::vector<std::string> handledOutputRelations;
  std::vector<std::string> handledRelations;
  std::map<std::string, std::string> rejectedOutputReasons;
  std::string abstractGraph;
  std::string abstractGraphDot;
  std::string witnessIndexedTemplates;
  std::string witnessIndexedWitnesses;
  std::string witnessIndexedRelations;
  std::string witnessIndexedTupleStats;
  std::map<std::string, double> probabilities;
  std::unordered_set<UntypedTuple> excludedTuples;
  std::unordered_map<UntypedTuple,
      std::shared_ptr<const PendingExactTemplateInstantiation>>
      templateInstantiations;
  /** Supported producer disjuncts attached to a residual concrete tuple. */
  std::unordered_map<UntypedTuple,
      std::shared_ptr<const PendingExactTemplateInstantiation>>
      rootTemplateInstantiations;
  /** Ground producers represented by rootTemplateInstantiations. */
  std::unordered_set<RuleApplication> suppressedRuleApplications;
};

struct LiftedBoundaryInliningStats {
  std::size_t candidateNodes = 0;
  std::size_t inlinedNodes = 0;
  std::size_t removedEdges = 0;
  std::size_t addedEdges = 0;
  std::size_t embeddedEventReferences = 0;
};

LiftedWmcResult tryEvaluateLiftedPointwise(
    const CmdOptions &opt, SouffleProgram &program,
    const RuleManager &ruleManager,
    const std::unordered_map<UntypedTuple, double> &factProb,
    const std::vector<std::pair<UntypedTuple, bool>> &evidences);

LiftedBoundaryInliningStats inlineLiftedBoundaryFormulas(
    WorkingDerivationGraph &graph, WorkingSubgraphView &view,
    const std::unordered_set<std::string> &liftedRelations);

void dumpLiftedProbabilities(const LiftedWmcResult &result,
                             const std::string &outputDir,
                             const std::string &fileName = "facts",
                             bool dumpDot = false,
                             bool writeProbabilities = true);

} // namespace souffle::problog

#endif
