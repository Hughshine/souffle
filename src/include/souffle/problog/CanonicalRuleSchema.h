#ifndef SOUFFLE_PROBLOG_CANONICAL_RULE_SCHEMA_H
#define SOUFFLE_PROBLOG_CANONICAL_RULE_SCHEMA_H

#include "souffle/problog/Rule.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace souffle::problog {

struct CanonicalRuleSchemaOptions {
  /** Maximum alpha-renaming candidates considered for one rule. */
  std::size_t maxAlphaPermutations = 65536;
};

struct CanonicalRuleSchema {
  /** Collision-safe representation used for final schema equality. */
  std::string text;

  /** Stable FNV-1a fingerprint used only for candidate bucketing. */
  std::uint64_t fingerprint = 0;

  /**
   * True iff every alpha-renaming allowed by the invariant partition was
   * considered.  Inexact schemas are conservative diagnostics and must not
   * be used as proof that two rules are equivalent.
   */
  bool exact = true;

  std::size_t permutationsExamined = 0;
  std::size_t headVariables = 0;
  std::size_t witnessVariables = 0;
};

CanonicalRuleSchema
canonicalizeRuleSchema(const Rule &rule,
                       const CanonicalRuleSchemaOptions &options = {});

std::uint64_t stableRuleSchemaFingerprint(const std::string &canonicalText);

} // namespace souffle::problog

#endif // SOUFFLE_PROBLOG_CANONICAL_RULE_SCHEMA_H
