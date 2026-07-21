#include "souffle/problog/ProvenanceFingerprint.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

using souffle::problog::fingerprintMaterializedProvenance;
using souffle::problog::MaterializedDerivationMap;
using souffle::problog::ProvenanceFingerprintOptions;

namespace {

SymbolicField var(const std::string &name) {
  return SymbolicField::makeVariable(name);
}

Atom atom(const std::string &relation, std::vector<SymbolicField> fields,
          bool negated = false) {
  return Atom(relation, std::move(fields), negated);
}

UntypedTuple tuple(const std::string &relation,
                   std::vector<souffle::RamDomain> fields) {
  return UntypedTuple{relation, std::move(fields)};
}

RuleApplication application(souffle::RamDomain ruleId,
                            std::vector<souffle::RamDomain> values) {
  return RuleApplication{ruleId, std::move(values)};
}

class DerivationFixture {
public:
  void add(const UntypedTuple &head,
           std::initializer_list<RuleApplication> applications) {
    auto values = std::make_unique<std::unordered_set<RuleApplication>>(
        applications.begin(), applications.end());
    derivations.emplace(head, values.get());
    owned.push_back(std::move(values));
  }

  MaterializedDerivationMap derivations;

private:
  std::vector<std::unique_ptr<std::unordered_set<RuleApplication>>> owned;
};

void require(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << "provenance fingerprint test failed: " << message << '\n';
    std::exit(1);
  }
}

} // namespace

int main() {
  std::vector<Rule> rules;
  rules.emplace_back(1, atom("base", {var("X")}), std::vector<Atom>{},
                     std::vector<std::string>{"X"}, 0.8);
  rules.emplace_back(2, atom("p", {var("X")}),
                     std::vector<Atom>{atom("base", {var("X")})},
                     std::vector<std::string>{"X"}, 0.9);
  rules.emplace_back(3, atom("p", {var("A")}),
                     std::vector<Atom>{atom("guard", {var("B")}),
                                       atom("edge", {var("A"), var("B")})},
                     std::vector<std::string>{"B", "A"}, 0.7);
  rules.emplace_back(4, atom("p", {var("X")}),
                     std::vector<Atom>{atom("edge", {var("X"), var("Y")}),
                                       atom("guard", {var("Y")})},
                     std::vector<std::string>{"X", "Y"}, 0.7);
  rules.emplace_back(5, atom("cycle", {var("X")}),
                     std::vector<Atom>{atom("link", {var("X"), var("Y")}),
                                       atom("cycle", {var("Y")})},
                     std::vector<std::string>{"X", "Y"}, 1.0, true, true);
  RuleManager manager(std::move(rules));

  DerivationFixture fixture;
  fixture.add(tuple("base", {10}), {application(1, {10})});
  fixture.add(tuple("base", {20}), {application(1, {20})});
  fixture.add(tuple("p", {10}), {application(2, {10})});
  fixture.add(tuple("p", {20}), {application(2, {20})});

  fixture.add(tuple("edge", {30, 31}), {});
  fixture.add(tuple("edge", {40, 41}), {});
  fixture.add(tuple("guard", {31}), {});
  fixture.add(tuple("guard", {41}), {});
  fixture.add(tuple("p", {30}), {application(3, {31, 30})});
  fixture.add(tuple("p", {40}), {application(4, {40, 41})});

  fixture.add(tuple("p", {50}),
              {application(4, {50, 51}), application(4, {50, 52})});
  fixture.add(tuple("edge", {50, 51}), {});
  fixture.add(tuple("edge", {50, 52}), {});
  fixture.add(tuple("guard", {51}), {});
  fixture.add(tuple("guard", {52}), {});

  fixture.add(tuple("cycle", {60}), {application(5, {60, 61})});
  fixture.add(tuple("cycle", {61}), {application(5, {61, 60})});
  fixture.add(tuple("cycle", {70}), {application(5, {70, 71})});
  fixture.add(tuple("cycle", {71}), {application(5, {71, 70})});
  fixture.add(tuple("link", {60, 61}), {});
  fixture.add(tuple("link", {61, 60}), {});
  fixture.add(tuple("link", {70, 71}), {});
  fixture.add(tuple("link", {71, 70}), {});

  const auto result =
      fingerprintMaterializedProvenance(fixture.derivations, manager);
  require(result.complete, "supported fixture should be complete");
  require(result.converged, "partition refinement should converge");
  require(result.tuples.at(tuple("p", {10})).fingerprint ==
              result.tuples.at(tuple("p", {20})).fingerprint,
          "constant-renamed proofs should share a fingerprint");
  require(result.tuples.at(tuple("p", {30})).fingerprint ==
              result.tuples.at(tuple("p", {40})).fingerprint,
          "alpha-renamed and body-reordered rules should share a fingerprint");
  require(result.tuples.at(tuple("p", {10})).fingerprint !=
              result.tuples.at(tuple("p", {30})).fingerprint,
          "different body structure should separate fingerprints");
  require(result.tuples.at(tuple("p", {40})).fingerprint !=
              result.tuples.at(tuple("p", {50})).fingerprint,
          "witness multiplicity should affect the fingerprint");
  require(result.tuples.at(tuple("cycle", {60})).fingerprint ==
              result.tuples.at(tuple("cycle", {70})).fingerprint,
          "isomorphic recursive cycles should share a fingerprint");
  require(result.candidateBuckets > 0 && result.maxBucketSize > 1,
          "candidate bucket statistics should expose reusable classes");

  DerivationFixture equalityFixture;
  equalityFixture.add(tuple("same", {1, 1}), {});
  equalityFixture.add(tuple("same", {2, 3}), {});
  const auto equalityResult = fingerprintMaterializedProvenance(
      equalityFixture.derivations, RuleManager({}));
  require(equalityResult.tuples.at(tuple("same", {1, 1})).fingerprint !=
              equalityResult.tuples.at(tuple("same", {2, 3})).fingerprint,
          "tuple-position equality pattern should be preserved");

  DerivationFixture missingRuleFixture;
  missingRuleFixture.add(tuple("unknown", {1}), {application(999, {1})});
  const auto missingRuleResult = fingerprintMaterializedProvenance(
      missingRuleFixture.derivations, RuleManager({}));
  require(!missingRuleResult.complete,
          "missing rules should mark the result incomplete");
  require(!missingRuleResult.tuples.at(tuple("unknown", {1})).complete,
          "missing-rule incompleteness should reach the tuple");

  std::unordered_set<std::string> opaque{"p"};
  const auto opaqueResult = fingerprintMaterializedProvenance(
      fixture.derivations, manager, {}, {}, opaque);
  require(opaqueResult.tuples.at(tuple("p", {50})).opaqueLeaf,
          "configured deterministic boundary should be opaque");
  require(opaqueResult.tuples.at(tuple("p", {50})).incomingApplications == 0,
          "opaque leaves must not replay incoming applications");

  ProvenanceFingerprintOptions noRounds;
  noRounds.maxRefinementRounds = 0;
  const auto bounded = fingerprintMaterializedProvenance(
      fixture.derivations, manager, {}, {}, {}, noRounds);
  require(!bounded.converged, "non-empty analysis with zero refinement rounds "
                              "must report non-convergence");

  std::cout << "provenance fingerprint tests passed\n";
  return 0;
}
