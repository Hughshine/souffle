#include "souffle/problog/CanonicalRuleSchema.h"

#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using souffle::problog::canonicalizeRuleSchema;
using souffle::problog::CanonicalRuleSchemaOptions;

namespace {

SymbolicField var(const std::string &name) {
  return SymbolicField::makeVariable(name);
}

Atom atom(const std::string &relation, std::vector<SymbolicField> fields,
          bool negated = false) {
  return Atom(relation, std::move(fields), negated);
}

Rule rule(std::size_t id, Atom head, std::vector<Atom> body,
          std::vector<std::string> vars, double probability = 1.0) {
  return Rule(id, std::move(head), std::move(body), std::move(vars),
              probability);
}

void require(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << "canonical rule schema test failed: " << message << '\n';
    std::exit(1);
  }
}

void requireSame(const Rule &lhs, const Rule &rhs, const std::string &message) {
  const auto left = canonicalizeRuleSchema(lhs);
  const auto right = canonicalizeRuleSchema(rhs);
  require(left.exact && right.exact, message + " (unexpected inexact result)");
  require(left.text == right.text, message + " (canonical text differs)");
  require(left.fingerprint == right.fingerprint,
          message + " (fingerprint differs)");
}

void requireDifferent(const Rule &lhs, const Rule &rhs,
                      const std::string &message) {
  const auto left = canonicalizeRuleSchema(lhs);
  const auto right = canonicalizeRuleSchema(rhs);
  require(left.text != right.text, message);
}

} // namespace

int main() {
  const Rule alphaLeft =
      rule(1, atom("p", {var("X"), var("Z")}),
           {atom("q", {var("X"), var("Y")}), atom("r", {var("Y"), var("Z")})},
           {"X", "Y", "Z"}, 0.75);
  const Rule alphaRight =
      rule(99, atom("p", {var("A"), var("C")}),
           {atom("r", {var("B"), var("C")}), atom("q", {var("A"), var("B")})},
           {"C", "B", "A"}, 0.75);
  requireSame(alphaLeft, alphaRight,
              "alpha-renaming, rule ID, variable inventory order, and body "
              "order must be ignored");

  const Rule sharedWitness = rule(
      2, atom("p", {var("X")}),
      {atom("q", {var("X"), var("Y")}), atom("r", {var("Y")})}, {"X", "Y"});
  const Rule splitWitness =
      rule(3, atom("p", {var("A")}),
           {atom("q", {var("A"), var("B")}), atom("r", {var("C")})},
           {"A", "B", "C"});
  requireDifferent(sharedWitness, splitWitness,
                   "witness equality pattern must be preserved");

  const Rule positive =
      rule(4, atom("p", {var("X")}),
           {atom("q", {var("X")}), atom("r", {var("X")})}, {"X"});
  const Rule negated =
      rule(5, atom("p", {var("A")}),
           {atom("q", {var("A")}), atom("r", {var("A")}, true)}, {"A"});
  requireDifferent(positive, negated, "negation must be preserved");

  const Rule integerConstant =
      rule(6, atom("p", {var("X")}), {atom("q", {var("X"), SymbolicField(1)})},
           {"X"});
  const Rule stringConstant =
      rule(7, atom("p", {var("A")}),
           {atom("q", {var("A"), SymbolicField(StringField{"1"})})}, {"A"});
  requireDifferent(integerConstant, stringConstant,
                   "typed constants must be preserved");

  const Rule probabilityA =
      rule(8, atom("p", {var("X")}), {atom("q", {var("X")})}, {"X"}, 0.5);
  const Rule probabilityB =
      rule(9, atom("p", {var("A")}), {atom("q", {var("A")})}, {"A"},
           0.5000000000000001);
  requireDifferent(probabilityA, probabilityB,
                   "probability bits must be preserved");

  auto expressionX = ExprField::makeAdd(AtomicField{VariableField{"X"}},
                                        AtomicField{IntegerField{1}});
  auto expressionA = ExprField::makeAdd(AtomicField{VariableField{"A"}},
                                        AtomicField{IntegerField{1}});
  const Rule expressionLeft =
      rule(10, atom("p", {var("X")}), {atom("q", {SymbolicField(expressionX)})},
           {"X"});
  const Rule expressionRight =
      rule(11, atom("p", {var("A")}), {atom("q", {SymbolicField(expressionA)})},
           {"A"});
  requireSame(expressionLeft, expressionRight,
              "variables inside expressions must be renamed");

  const Rule aggregateLeft(
      14, atom("p", {var("X"), var("R")}), {}, {"X", "Y", "R"}, 1.0, false,
      false, false,
      {AggregateSpec("sum", "R", atom("q", {var("X"), var("Y")}), var("Y"))});
  const Rule aggregateRight(
      15, atom("p", {var("A"), var("C")}), {}, {"C", "B", "A"}, 1.0, false,
      false, false,
      {AggregateSpec("sum", "C", atom("q", {var("A"), var("B")}), var("B"))});
  requireSame(aggregateLeft, aggregateRight,
              "aggregate variables must be alpha-renamed");
  const Rule differentAggregate(
      16, atom("p", {var("A"), var("C")}), {}, {"A", "B", "C"}, 1.0, false,
      false, false,
      {AggregateSpec("count", "C", atom("q", {var("A"), var("B")}), var("B"))});
  requireDifferent(aggregateLeft, differentAggregate,
                   "aggregate operator must be preserved");

  const Rule recursiveRule(17, atom("p", {var("X")}),
                           {atom("q", {var("X")}), atom("r", {var("X")})},
                           {"X"}, 1.0, true);
  requireDifferent(positive, recursiveRule,
                   "recursion metadata must be preserved");

  std::vector<Atom> symmetricBodyLeft;
  std::vector<Atom> symmetricBodyRight;
  std::vector<std::string> symmetricVarsLeft{"X"};
  std::vector<std::string> symmetricVarsRight{"A"};
  for (const auto &name : {"U", "V", "W", "T"}) {
    symmetricBodyLeft.push_back(atom("q", {var("X"), var(name)}));
    symmetricVarsLeft.push_back(name);
  }
  for (const auto &name : {"D", "C", "B", "E"}) {
    symmetricBodyRight.push_back(atom("q", {var("A"), var(name)}));
    symmetricVarsRight.push_back(name);
  }
  const Rule symmetricLeft =
      rule(12, atom("p", {var("X")}), std::move(symmetricBodyLeft),
           std::move(symmetricVarsLeft));
  const Rule symmetricRight =
      rule(13, atom("p", {var("A")}), std::move(symmetricBodyRight),
           std::move(symmetricVarsRight));
  requireSame(symmetricLeft, symmetricRight,
              "symmetric witness cells must canonicalize exactly");

  CanonicalRuleSchemaOptions tinyBudget;
  tinyBudget.maxAlphaPermutations = 8;
  const auto budgeted = canonicalizeRuleSchema(symmetricLeft, tinyBudget);
  require(!budgeted.exact, "budget exhaustion must be reported");
  require(budgeted.permutationsExamined == 1,
          "budget fallback must emit one deterministic conservative schema");

  std::cout << "canonical rule schema tests passed\n";
  return 0;
}
