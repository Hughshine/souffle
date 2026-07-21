#ifndef SOUFFLE_PROBLOG_PROVENANCE_FINGERPRINT_H
#define SOUFFLE_PROBLOG_PROVENANCE_FINGERPRINT_H

#include "souffle/Derivation.h"
#include "souffle/problog/RuleManager.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace souffle::problog {

struct ProvenanceFingerprint128 {
  std::uint64_t high = 0;
  std::uint64_t low = 0;

  bool operator==(const ProvenanceFingerprint128 &other) const;
  bool operator!=(const ProvenanceFingerprint128 &other) const;
  bool operator<(const ProvenanceFingerprint128 &other) const;
  std::string toHex() const;
};

struct TupleProvenanceFingerprint {
  ProvenanceFingerprint128 fingerprint;
  std::size_t color = 0;
  std::size_t incomingApplications = 0;
  std::size_t bodyReferences = 0;
  bool complete = true;
  bool materialized = false;
  bool opaqueLeaf = false;
};

struct ProvenanceFingerprintOptions {
  /** Maximum synchronous partition-refinement rounds. */
  std::size_t maxRefinementRounds = 64;

  /** Canonical-rule alpha-renaming budget per distinct rule. */
  std::size_t maxRuleAlphaPermutations = 65536;
};

struct ProvenanceFingerprintResult {
  std::map<UntypedTuple, TupleProvenanceFingerprint> tuples;
  std::map<ProvenanceFingerprint128, std::vector<UntypedTuple>> buckets;

  std::size_t materializedTuples = 0;
  std::size_t referencedLeafTuples = 0;
  std::size_t ruleApplications = 0;
  std::size_t bodyReferences = 0;
  std::size_t colorClasses = 0;
  std::size_t candidateBuckets = 0;
  std::size_t maxBucketSize = 0;
  std::size_t refinementRounds = 0;
  bool converged = true;
  bool complete = true;
};

using MaterializedDerivationMap =
    std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication> *>;

/**
 * Compute candidate structural fingerprints directly from materialized rule
 * applications. This function does not create concrete derivation graph
 * nodes/edges. Fingerprint equality is only a candidate relation; an exact
 * isomorphism verifier is still required before template reuse.
 */
ProvenanceFingerprintResult fingerprintMaterializedProvenance(
    const MaterializedDerivationMap &derivations,
    const RuleManager &ruleManager,
    const std::unordered_map<UntypedTuple, double> &probabilisticFacts = {},
    const std::unordered_set<UntypedTuple> &inputFacts = {},
    const std::unordered_set<std::string> &opaqueLeafRelations = {},
    const ProvenanceFingerprintOptions &options = {});

} // namespace souffle::problog

#endif // SOUFFLE_PROBLOG_PROVENANCE_FINGERPRINT_H
