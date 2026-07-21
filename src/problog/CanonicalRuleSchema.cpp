#include "souffle/problog/CanonicalRuleSchema.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <iomanip>
#include <map>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace souffle::problog {
namespace {

using VariableRenderer = std::function<std::string(const std::string &)>;

std::string sized(const std::string &value) {
  return std::to_string(value.size()) + ":" + value;
}

template <typename UInt> std::string hexBits(UInt value) {
  std::ostringstream out;
  out << std::hex << std::setfill('0') << std::setw(sizeof(UInt) * 2) << value;
  return out.str();
}

std::string serializeDouble(double value) {
  static_assert(sizeof(double) == sizeof(std::uint64_t));
  std::uint64_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return "f64:" + hexBits(bits);
}

std::string serializeAtomicField(const AtomicField &field,
                                 const VariableRenderer &renderVariable);
std::string serializeExpression(const ExprField &expression,
                                const VariableRenderer &renderVariable);

std::string serializeExprReal(const ExprFieldReal &real,
                              const VariableRenderer &renderVariable) {
  if (std::holds_alternative<AtomicField>(real)) {
    return serializeAtomicField(std::get<AtomicField>(real), renderVariable);
  }
  const auto &expression = std::get<ExprFieldPtr>(real);
  return expression == nullptr
             ? "expr:null"
             : serializeExpression(*expression, renderVariable);
}

const char *expressionOperatorName(ExprField::OpType op) {
  switch (op) {
  case ExprField::OpType::And:
    return "and";
  case ExprField::OpType::Or:
    return "or";
  case ExprField::OpType::Neg:
    return "neg";
  case ExprField::OpType::Atom:
    return "atom";
  case ExprField::OpType::Add:
    return "add";
  case ExprField::OpType::Sub:
    return "sub";
  }
  return "invalid";
}

std::string serializeExpression(const ExprField &expression,
                                const VariableRenderer &renderVariable) {
  std::vector<std::string> operands;
  operands.reserve(expression.operands.size());
  for (const auto &operand : expression.operands) {
    operands.push_back(serializeExprReal(operand, renderVariable));
  }

  std::ostringstream out;
  out << "expr{" << expressionOperatorName(expression.op) << ";";
  for (const auto &operand : operands) {
    out << sized(operand);
  }
  out << "}";
  return out.str();
}

std::string serializeAtomicField(const AtomicField &field,
                                 const VariableRenderer &renderVariable) {
  if (std::holds_alternative<IntegerField>(field)) {
    return "int:" + std::to_string(std::get<IntegerField>(field).value);
  }
  if (std::holds_alternative<FloatField>(field)) {
    return serializeDouble(std::get<FloatField>(field).value);
  }
  if (std::holds_alternative<StringField>(field)) {
    return "str:" + sized(std::get<StringField>(field).value);
  }
  return "var:" + sized(renderVariable(std::get<VariableField>(field).name));
}

std::string serializeField(const SymbolicField &field,
                           const VariableRenderer &renderVariable) {
  if (std::holds_alternative<IntegerField>(field.field)) {
    return serializeAtomicField(
        AtomicField{std::get<IntegerField>(field.field)}, renderVariable);
  }
  if (std::holds_alternative<FloatField>(field.field)) {
    return serializeAtomicField(AtomicField{std::get<FloatField>(field.field)},
                                renderVariable);
  }
  if (std::holds_alternative<StringField>(field.field)) {
    return serializeAtomicField(AtomicField{std::get<StringField>(field.field)},
                                renderVariable);
  }
  if (std::holds_alternative<VariableField>(field.field)) {
    return serializeAtomicField(
        AtomicField{std::get<VariableField>(field.field)}, renderVariable);
  }
  const auto &expression = std::get<std::shared_ptr<ExprField>>(field.field);
  return expression == nullptr
             ? "expr:null"
             : serializeExpression(*expression, renderVariable);
}

std::string serializeAtom(const Atom &atom,
                          const VariableRenderer &renderVariable) {
  std::ostringstream out;
  out << "atom{" << (atom.isNegatedAtom() ? "neg" : "pos") << ";"
      << sized(atom.getRelation()) << ";";
  for (const auto &field : atom.getFields()) {
    out << sized(serializeField(field, renderVariable));
  }
  out << "}";
  return out.str();
}

void collectAtomicVariables(const AtomicField &field,
                            std::vector<std::string> &ordered,
                            std::unordered_set<std::string> &seen) {
  if (!std::holds_alternative<VariableField>(field)) {
    return;
  }
  const auto &name = std::get<VariableField>(field).name;
  if (name != "_" && seen.insert(name).second) {
    ordered.push_back(name);
  }
}

void collectExpressionVariables(const ExprField &expression,
                                std::vector<std::string> &ordered,
                                std::unordered_set<std::string> &seen) {
  for (const auto &operand : expression.operands) {
    if (std::holds_alternative<AtomicField>(operand)) {
      collectAtomicVariables(std::get<AtomicField>(operand), ordered, seen);
    } else {
      const auto &nested = std::get<ExprFieldPtr>(operand);
      if (nested != nullptr) {
        collectExpressionVariables(*nested, ordered, seen);
      }
    }
  }
}

void collectFieldVariables(const SymbolicField &field,
                           std::vector<std::string> &ordered,
                           std::unordered_set<std::string> &seen) {
  if (std::holds_alternative<VariableField>(field.field)) {
    collectAtomicVariables(AtomicField{std::get<VariableField>(field.field)},
                           ordered, seen);
  } else if (std::holds_alternative<std::shared_ptr<ExprField>>(field.field)) {
    const auto &expression = std::get<std::shared_ptr<ExprField>>(field.field);
    if (expression != nullptr) {
      collectExpressionVariables(*expression, ordered, seen);
    }
  }
}

void collectAtomVariables(const Atom &atom, std::vector<std::string> &ordered,
                          std::unordered_set<std::string> &seen) {
  for (const auto &field : atom.getFields()) {
    collectFieldVariables(field, ordered, seen);
  }
}

std::string
serializeRule(const Rule &rule,
              const std::unordered_map<std::string, std::string> &variableNames,
              const std::vector<std::string> &variableInventory) {
  const auto renderVariable = [&](const std::string &name) {
    if (name == "_") {
      return std::string("_");
    }
    const auto it = variableNames.find(name);
    return it == variableNames.end() ? std::string("unbound:") + sized(name)
                                     : it->second;
  };

  std::vector<std::string> body;
  body.reserve(rule.getBodyAtoms().size());
  for (const auto &atom : rule.getBodyAtoms()) {
    body.push_back(serializeAtom(atom, renderVariable));
  }
  std::sort(body.begin(), body.end());

  std::vector<std::string> aggregates;
  aggregates.reserve(rule.getAggregates().size());
  for (const auto &aggregate : rule.getAggregates()) {
    std::ostringstream item;
    item << "aggregate{" << sized(aggregate.op) << ";"
         << sized(renderVariable(aggregate.resultVar)) << ";"
         << sized(serializeAtom(aggregate.witnessAtom, renderVariable)) << ";"
         << sized(serializeField(aggregate.weightExpr, renderVariable)) << "}";
    aggregates.push_back(item.str());
  }
  std::sort(aggregates.begin(), aggregates.end());

  std::vector<std::string> inventory;
  inventory.reserve(variableInventory.size());
  for (const auto &variable : variableInventory) {
    inventory.push_back(renderVariable(variable));
  }
  std::sort(inventory.begin(), inventory.end());

  std::ostringstream out;
  out << "rule{head=" << sized(serializeAtom(rule.getHead(), renderVariable))
      << ";body=";
  for (const auto &atom : body) {
    out << sized(atom);
  }
  out << ";aggregates=";
  for (const auto &aggregate : aggregates) {
    out << sized(aggregate);
  }
  out << ";vars=";
  for (const auto &variable : inventory) {
    out << sized(variable);
  }
  out << ";prob=" << serializeDouble(rule.getProbability())
      << ";recursive=" << (rule.isRecursive() ? 1 : 0)
      << ";recursive_stratum=" << (rule.isInRecursiveStratum() ? 1 : 0)
      << ";eqrel=" << (rule.isEqrel() ? 1 : 0) << "}";
  return out.str();
}

std::string witnessOccurrenceSignature(
    const Rule &rule, const std::string &target,
    const std::unordered_map<std::string, std::string> &headNames,
    const std::vector<std::string> &variableInventory) {
  std::unordered_map<std::string, std::string> marked = headNames;
  for (const auto &variable : variableInventory) {
    if (marked.count(variable) == 0) {
      marked.emplace(variable, variable == target ? "@self" : "@other");
    }
  }
  return serializeRule(rule, marked, variableInventory);
}

} // namespace

std::uint64_t stableRuleSchemaFingerprint(const std::string &canonicalText) {
  constexpr std::uint64_t offsetBasis = 14695981039346656037ULL;
  constexpr std::uint64_t prime = 1099511628211ULL;
  std::uint64_t hash = offsetBasis;
  for (const unsigned char byte : canonicalText) {
    hash ^= byte;
    hash *= prime;
  }
  return hash;
}

CanonicalRuleSchema
canonicalizeRuleSchema(const Rule &rule,
                       const CanonicalRuleSchemaOptions &options) {
  std::vector<std::string> headVariables;
  std::unordered_set<std::string> headSeen;
  collectAtomVariables(rule.getHead(), headVariables, headSeen);

  std::unordered_map<std::string, std::string> headNames;
  for (std::size_t i = 0; i < headVariables.size(); ++i) {
    headNames.emplace(headVariables[i], "h" + std::to_string(i));
  }

  std::vector<std::string> allVariables;
  std::unordered_set<std::string> allSeen;
  const auto addVariable = [&](const std::string &name) {
    if (name != "_" && allSeen.insert(name).second) {
      allVariables.push_back(name);
    }
  };
  for (const auto &variable : rule.getVars()) {
    addVariable(variable);
  }
  std::vector<std::string> syntaxVariables;
  std::unordered_set<std::string> syntaxSeen;
  collectAtomVariables(rule.getHead(), syntaxVariables, syntaxSeen);
  for (const auto &atom : rule.getBodyAtoms()) {
    collectAtomVariables(atom, syntaxVariables, syntaxSeen);
  }
  for (const auto &aggregate : rule.getAggregates()) {
    if (aggregate.resultVar != "_") {
      if (syntaxSeen.insert(aggregate.resultVar).second) {
        syntaxVariables.push_back(aggregate.resultVar);
      }
    }
    collectAtomVariables(aggregate.witnessAtom, syntaxVariables, syntaxSeen);
    collectFieldVariables(aggregate.weightExpr, syntaxVariables, syntaxSeen);
  }
  for (const auto &variable : syntaxVariables) {
    addVariable(variable);
  }

  std::vector<std::string> witnessVariables;
  for (const auto &variable : allVariables) {
    if (headNames.count(variable) == 0) {
      witnessVariables.push_back(variable);
    }
  }

  std::map<std::string, std::vector<std::string>> partitioned;
  for (const auto &witness : witnessVariables) {
    partitioned[witnessOccurrenceSignature(rule, witness, headNames,
                                           allVariables)]
        .push_back(witness);
  }

  struct Cell {
    std::vector<std::string> variables;
    std::size_t firstCanonicalIndex = 0;
  };
  std::vector<Cell> cells;
  std::size_t nextWitnessIndex = 0;
  std::size_t candidateCount = 1;
  bool overBudget = false;
  for (auto &[signature, variables] : partitioned) {
    (void)signature;
    std::sort(variables.begin(), variables.end());
    Cell cell;
    cell.variables = std::move(variables);
    cell.firstCanonicalIndex = nextWitnessIndex;
    nextWitnessIndex += cell.variables.size();
    for (std::size_t factor = 2; factor <= cell.variables.size(); ++factor) {
      if (candidateCount > options.maxAlphaPermutations / factor) {
        overBudget = true;
        break;
      }
      candidateCount *= factor;
    }
    cells.push_back(std::move(cell));
  }

  CanonicalRuleSchema result;
  result.headVariables = headVariables.size();
  result.witnessVariables = witnessVariables.size();
  result.exact = !overBudget && candidateCount <= options.maxAlphaPermutations;

  std::string best;
  auto consider =
      [&](const std::unordered_map<std::string, std::string> &names) {
        const auto candidate = serializeRule(rule, names, allVariables);
        ++result.permutationsExamined;
        if (best.empty() || candidate < best) {
          best = candidate;
        }
      };

  if (!result.exact) {
    auto names = headNames;
    for (const auto &cell : cells) {
      for (std::size_t i = 0; i < cell.variables.size(); ++i) {
        names.emplace(cell.variables[i],
                      "w" + std::to_string(cell.firstCanonicalIndex + i));
      }
    }
    consider(names);
  } else {
    std::unordered_map<std::string, std::string> names = headNames;
    std::function<void(std::size_t)> enumerate = [&](std::size_t cellIndex) {
      if (cellIndex == cells.size()) {
        consider(names);
        return;
      }
      auto permutation = cells[cellIndex].variables;
      do {
        for (std::size_t i = 0; i < permutation.size(); ++i) {
          names[permutation[i]] =
              "w" + std::to_string(cells[cellIndex].firstCanonicalIndex + i);
        }
        enumerate(cellIndex + 1);
      } while (std::next_permutation(permutation.begin(), permutation.end()));
    };
    enumerate(0);
  }

  result.text = std::move(best);
  result.fingerprint = stableRuleSchemaFingerprint(result.text);
  return result;
}

} // namespace souffle::problog
