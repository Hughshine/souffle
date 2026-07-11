#include "souffle/problog/LiftedWmc.h"

#include "souffle/problog/Atom.h"
#include "souffle/problog/Rule.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <functional>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace souffle::problog {
namespace {

std::string escapeDotLabel(const std::string &value) {
  std::string escaped;
  escaped.reserve(value.size());
  for (const char ch : value) {
    switch (ch) {
    case '\\':
      escaped += "\\\\";
      break;
    case '"':
      escaped += "\\\"";
      break;
    case '\n':
      escaped += "\\n";
      break;
    case '\r':
      escaped += "\\r";
      break;
    case '\t':
      escaped += "\\t";
      break;
    default:
      escaped += ch;
      break;
    }
  }
  return escaped;
}

enum class ParamTermKind {
  RootParameter,
  WitnessParameter,
  Constant,
};

struct ParamTerm {
  ParamTermKind kind = ParamTermKind::Constant;
  std::size_t parameter = 0;
  std::size_t witnessEdge = 0;
  RamDomain constant = 0;

  static ParamTerm root(std::size_t index) {
    ParamTerm term;
    term.kind = ParamTermKind::RootParameter;
    term.parameter = index;
    return term;
  }

  static ParamTerm witness(std::size_t edge, std::size_t index) {
    ParamTerm term;
    term.kind = ParamTermKind::WitnessParameter;
    term.parameter = index;
    term.witnessEdge = edge;
    return term;
  }

  static ParamTerm value(RamDomain value) {
    ParamTerm term;
    term.kind = ParamTermKind::Constant;
    term.constant = value;
    return term;
  }

  bool operator==(const ParamTerm &other) const {
    return std::tie(kind, parameter, witnessEdge, constant) ==
           std::tie(other.kind, other.parameter, other.witnessEdge,
                    other.constant);
  }

  bool operator<(const ParamTerm &other) const {
    return std::tie(kind, parameter, witnessEdge, constant) <
           std::tie(other.kind, other.parameter, other.witnessEdge,
                    other.constant);
  }

  std::string toString() const {
    if (kind == ParamTermKind::RootParameter) {
      return "$" + std::to_string(parameter);
    }
    if (kind == ParamTermKind::WitnessParameter) {
      return "$e" + std::to_string(witnessEdge) + "v" +
             std::to_string(parameter);
    }
    return std::to_string(constant);
  }
};

enum class SymbolicVariableKind {
  RuleApplicationGuard,
  ExtensionalFact,
  ProbabilisticRule,
};

using AbstractNodeId = std::uint32_t;
using AbstractEdgeId = std::uint32_t;

struct SymbolicVariable {
  SymbolicVariableKind kind = SymbolicVariableKind::ExtensionalFact;
  AbstractNodeId node = 0;
  AbstractEdgeId edge = 0;

  bool operator==(const SymbolicVariable &other) const {
    return std::tie(kind, node, edge) ==
           std::tie(other.kind, other.node, other.edge);
  }

  bool operator<(const SymbolicVariable &other) const {
    return std::tie(kind, node, edge) <
           std::tie(other.kind, other.node, other.edge);
  }
};

using BddId = std::uint32_t;

enum class BddOperation {
  And,
  Or,
};

struct BddNode {
  SymbolicVariable variable;
  BddId low = 0;
  BddId high = 0;
  bool terminal = true;
};

struct BddNodeKey {
  SymbolicVariable variable;
  BddId low = 0;
  BddId high = 0;

  bool operator<(const BddNodeKey &other) const {
    if (variable < other.variable) {
      return true;
    }
    if (other.variable < variable) {
      return false;
    }
    return std::tie(low, high) < std::tie(other.low, other.high);
  }
};

class SymbolicBdd {
public:
  SymbolicBdd() {
    nodes.emplace_back();
    nodes.emplace_back();
  }

  BddId getFalse() const { return 0; }

  BddId getTrue() const { return 1; }

  BddId makeVariable(const SymbolicVariable &variable) {
    variables.insert(variable);
    return makeNode(variable, getFalse(), getTrue());
  }

  BddId makeAnd(BddId lhs, BddId rhs) {
    return apply(BddOperation::And, lhs, rhs);
  }

  BddId makeOr(BddId lhs, BddId rhs) {
    return apply(BddOperation::Or, lhs, rhs);
  }

  const BddNode &getNode(BddId id) const { return nodes.at(id); }

  const std::set<SymbolicVariable> &getVariables() const { return variables; }

  std::size_t nodeCount() const { return nodes.size(); }

private:
  BddId makeNode(const SymbolicVariable &variable, BddId low, BddId high) {
    if (low == high) {
      return low;
    }
    BddNodeKey key{variable, low, high};
    auto it = uniqueNodes.find(key);
    if (it != uniqueNodes.end()) {
      return it->second;
    }
    if (nodes.size() >= std::numeric_limits<BddId>::max()) {
      throw std::runtime_error("lifted BDD node limit exceeded");
    }
    const auto id = static_cast<BddId>(nodes.size());
    BddNode node;
    node.variable = variable;
    node.low = low;
    node.high = high;
    node.terminal = false;
    nodes.push_back(std::move(node));
    uniqueNodes.emplace(std::move(key), id);
    return id;
  }

  BddId apply(BddOperation operation, BddId lhs, BddId rhs) {
    if (lhs > rhs) {
      std::swap(lhs, rhs);
    }
    if (operation == BddOperation::And) {
      if (lhs == getFalse() || rhs == getFalse()) {
        return getFalse();
      }
      if (lhs == getTrue()) {
        return rhs;
      }
      if (lhs == rhs) {
        return lhs;
      }
    } else {
      if (lhs == getTrue() || rhs == getTrue()) {
        return getTrue();
      }
      if (lhs == getFalse()) {
        return rhs;
      }
      if (lhs == rhs) {
        return lhs;
      }
    }

    const auto cacheKey = std::make_tuple(operation, lhs, rhs);
    auto cached = applyCache.find(cacheKey);
    if (cached != applyCache.end()) {
      return cached->second;
    }

    const BddNode &lhsNode = nodes.at(lhs);
    const BddNode &rhsNode = nodes.at(rhs);
    SymbolicVariable top;
    if (lhsNode.terminal) {
      top = rhsNode.variable;
    } else if (rhsNode.terminal || lhsNode.variable < rhsNode.variable) {
      top = lhsNode.variable;
    } else {
      top = rhsNode.variable;
    }

    auto cofactor = [&](BddId id, bool high) {
      const BddNode &node = nodes.at(id);
      if (!node.terminal && node.variable == top) {
        return high ? node.high : node.low;
      }
      return id;
    };

    const BddId low =
        apply(operation, cofactor(lhs, false), cofactor(rhs, false));
    const BddId high =
        apply(operation, cofactor(lhs, true), cofactor(rhs, true));
    const BddId result = makeNode(top, low, high);
    applyCache.emplace(cacheKey, result);
    return result;
  }

  std::vector<BddNode> nodes;
  std::map<BddNodeKey, BddId> uniqueNodes;
  std::map<std::tuple<BddOperation, BddId, BddId>, BddId> applyCache;
  std::set<SymbolicVariable> variables;
};

struct RelationCallKey {
  std::string relation;
  std::vector<ParamTerm> arguments;

  bool operator<(const RelationCallKey &other) const {
    if (relation != other.relation) {
      return relation < other.relation;
    }
    return std::lexicographical_compare(arguments.begin(), arguments.end(),
                                        other.arguments.begin(),
                                        other.arguments.end());
  }
};

struct AbstractBindingDomain {
  std::size_t ruleId = 0;
  std::string headRelation;
  std::vector<ParamTerm> headArguments;
  std::vector<ParamTerm> groundingKey;
};

struct AbstractDerivationNode {
  AbstractNodeId id = 0;
  std::string relation;
  std::vector<ParamTerm> arguments;
  bool extensional = false;
  std::vector<AbstractEdgeId> incomingEdges;
};

struct AbstractDerivationEdge {
  AbstractEdgeId id = 0;
  AbstractNodeId head = 0;
  std::vector<AbstractNodeId> body;
  AbstractBindingDomain domain;
  double probability = 1.0;
};

class AbstractDerivationGraph {
public:
  AbstractDerivationGraph(const RuleManager &ruleManager,
                          const std::unordered_set<std::size_t> &activeRuleIds,
                          std::unordered_set<std::string> inputRelations)
      : ruleManager(ruleManager), activeRuleIds(activeRuleIds),
        inputRelations(std::move(inputRelations)) {}

  AbstractNodeId buildOutput(const Relation &relation) {
    std::vector<ParamTerm> arguments;
    arguments.reserve(relation.getArity());
    for (std::size_t i = 0; i < relation.getArity(); ++i) {
      arguments.push_back(ParamTerm::root(i));
    }
    return getOrCreateNode(relation.getName(), arguments);
  }

  const AbstractDerivationNode &getNode(AbstractNodeId id) const {
    return nodes.at(id);
  }

  const AbstractDerivationEdge &getEdge(AbstractEdgeId id) const {
    return edges.at(id);
  }

  std::size_t nodeCount() const { return nodes.size(); }

  std::size_t edgeCount() const { return edges.size(); }

  std::string toDot() const {
    auto formatCall = [](const AbstractDerivationNode &node) {
      std::ostringstream label;
      label << node.relation << "(";
      for (std::size_t i = 0; i < node.arguments.size(); ++i) {
        if (i > 0) {
          label << ",";
        }
        label << node.arguments[i].toString();
      }
      label << ")";
      if (node.extensional) {
        label << "\ninput";
      }
      return label.str();
    };

    std::ostringstream out;
    out << "digraph AbstractDerivationGraph {\n";
    out << "  rankdir=LR;\n";
    out << "  node [shape=box, style=filled, fillcolor=lightblue];\n";
    for (const auto &node : nodes) {
      out << "  node" << node.id << " [label=\""
          << escapeDotLabel(formatCall(node)) << "\"];\n";
    }
    out << "  node [shape=point, fillcolor=red, width=0.2];\n";
    for (const auto &edge : edges) {
      out << "  edge" << edge.id << " [xlabel=\"rule " << edge.domain.ruleId;
      if (edge.probability != 1.0) {
        out << ", p=" << edge.probability;
      }
      out << "\"];\n";
      for (const auto bodyNode : edge.body) {
        out << "  node" << bodyNode << " -> edge" << edge.id << ";\n";
      }
      out << "  edge" << edge.id << " -> node" << edge.head << ";\n";
    }
    out << "}\n";
    return out.str();
  }

  std::string toTsv() const {
    auto joinTerms = [](const std::vector<ParamTerm> &terms) {
      std::ostringstream out;
      for (std::size_t i = 0; i < terms.size(); ++i) {
        if (i > 0) {
          out << ",";
        }
        out << terms[i].toString();
      }
      return out.str();
    };
    auto joinIds = [](const std::vector<AbstractNodeId> &ids) {
      std::ostringstream out;
      for (std::size_t i = 0; i < ids.size(); ++i) {
        if (i > 0) {
          out << ",";
        }
        out << ids[i];
      }
      return out.str();
    };

    std::ostringstream out;
    out << "kind\tid\trelation_or_head\targuments_or_body"
        << "\trule_id\tprobability\tbinding_key\n";
    for (const auto &node : nodes) {
      out << "node\t" << node.id << "\t" << node.relation << "\t"
          << joinTerms(node.arguments) << "\t\t\t"
          << (node.extensional ? "extensional" : "derived") << "\n";
    }
    for (const auto &edge : edges) {
      out << "edge\t" << edge.id << "\t" << edge.head << "\t"
          << joinIds(edge.body) << "\t" << edge.domain.ruleId << "\t"
          << edge.probability << "\t" << joinTerms(edge.domain.groundingKey)
          << "\n";
    }
    return out.str();
  }

private:
  ParamTerm substituteField(
      const SymbolicField &field,
      const std::unordered_map<std::string, ParamTerm> &bindings) const {
    if (std::holds_alternative<VariableField>(field.field)) {
      const auto &name = std::get<VariableField>(field.field).name;
      auto it = bindings.find(name);
      if (it == bindings.end()) {
        throw std::runtime_error(
            "abstract graph variable binding is missing: " + name);
      }
      return it->second;
    }
    static const std::vector<std::string> noVariables;
    static const std::vector<RamDomain> noValues;
    return ParamTerm::value(
        evaluateSymbolicField(field, noVariables, noValues));
  }

  AbstractNodeId getOrCreateNode(const std::string &relation,
                                 const std::vector<ParamTerm> &arguments) {
    RelationCallKey key{relation, arguments};
    if (activeCalls.count(key) > 0) {
      throw std::runtime_error("recursive abstract derivation graph");
    }
    auto cached = nodeByCall.find(key);
    if (cached != nodeByCall.end()) {
      return cached->second;
    }
    if (nodes.size() >= std::numeric_limits<AbstractNodeId>::max()) {
      throw std::runtime_error("abstract derivation node limit exceeded");
    }

    const auto nodeId = static_cast<AbstractNodeId>(nodes.size());
    AbstractDerivationNode node;
    node.id = nodeId;
    node.relation = relation;
    node.arguments = arguments;
    nodes.push_back(std::move(node));
    nodeByCall.emplace(key, nodeId);

    activeCalls.insert(key);

    auto rules = ruleManager.getRulesForPredicate(relation);
    std::sort(rules.begin(), rules.end(), [](const Rule *lhs, const Rule *rhs) {
      return lhs->getRuleId() < rhs->getRuleId();
    });
    nodes[nodeId].extensional = inputRelations.count(relation) > 0;
    if (detOptEnabled && isDetRelation(relation)) {
      nodes[nodeId].extensional = true;
      activeCalls.erase(key);
      return nodeId;
    }
    for (const Rule *rule : rules) {
      if (rule == nullptr || activeRuleIds.count(rule->getRuleId()) == 0) {
        continue;
      }
      if (edges.size() >= std::numeric_limits<AbstractEdgeId>::max()) {
        throw std::runtime_error("abstract derivation edge limit exceeded");
      }
      const auto edgeId = static_cast<AbstractEdgeId>(edges.size());
      AbstractDerivationEdge edge;
      edge.id = edgeId;
      edge.head = nodeId;
      edge.probability = rule->getProbability();
      edge.domain.ruleId = rule->getRuleId();
      edge.domain.headRelation = relation;
      edge.domain.headArguments = arguments;
      edges.push_back(std::move(edge));

      std::unordered_map<std::string, ParamTerm> bindings;
      const auto &headFields = rule->getHead().getFields();
      for (std::size_t i = 0; i < headFields.size(); ++i) {
        const auto &name = std::get<VariableField>(headFields[i].field).name;
        bindings.emplace(name, arguments.at(i));
      }

      const auto ruleVariables = rule->getVars();
      for (std::size_t i = 0; i < ruleVariables.size(); ++i) {
        bindings.emplace(ruleVariables[i], ParamTerm::witness(edgeId, i));
      }

      std::vector<ParamTerm> groundingKey;
      groundingKey.reserve(ruleVariables.size());
      for (const auto &variable : ruleVariables) {
        groundingKey.push_back(bindings.at(variable));
      }

      std::vector<AbstractNodeId> body;
      body.reserve(rule->getBodyAtoms().size());
      for (const auto &bodyAtom : rule->getBodyAtoms()) {
        std::vector<ParamTerm> bodyArguments;
        bodyArguments.reserve(bodyAtom.getFields().size());
        for (const auto &field : bodyAtom.getFields()) {
          bodyArguments.push_back(substituteField(field, bindings));
        }
        body.push_back(getOrCreateNode(bodyAtom.getRelation(), bodyArguments));
      }

      edges[edgeId].body = std::move(body);
      edges[edgeId].domain.groundingKey = std::move(groundingKey);
      nodes[nodeId].incomingEdges.push_back(edgeId);
    }

    activeCalls.erase(key);
    return nodeId;
  }

  const RuleManager &ruleManager;
  const std::unordered_set<std::size_t> &activeRuleIds;
  std::unordered_set<std::string> inputRelations;
  std::vector<AbstractDerivationNode> nodes;
  std::vector<AbstractDerivationEdge> edges;
  std::map<RelationCallKey, AbstractNodeId> nodeByCall;
  std::set<RelationCallKey> activeCalls;
};

class PointwiseEligibility {
public:
  PointwiseEligibility(SouffleProgram &program, const RuleManager &ruleManager,
                       const std::unordered_set<std::size_t> &activeRuleIds)
      : program(program), ruleManager(ruleManager),
        activeRuleIds(activeRuleIds) {}

  bool analyze(const std::vector<Relation *> &outputs) {
    for (const auto *output : outputs) {
      if (output == nullptr || !analyzeRelation(output->getName())) {
        return false;
      }
    }
    return true;
  }

  const std::string &failureReason() const { return reason; }

  const std::unordered_set<std::string> &relations() const {
    return relationClosure;
  }

private:
  bool analyzeRelation(const std::string &relation) {
    if (accepted.count(relation) > 0) {
      return true;
    }
    if (active.count(relation) > 0) {
      return fail("recursive relation dependency: " + relation);
    }
    if (program.getRelation(relation) == nullptr) {
      return fail("relation is not materialized at runtime: " + relation);
    }

    active.insert(relation);
    relationClosure.insert(relation);
    if (detOptEnabled && isDetRelation(relation)) {
      active.erase(relation);
      accepted.insert(relation);
      return true;
    }
    auto rules = ruleManager.getRulesForPredicate(relation);
    std::sort(rules.begin(), rules.end(), [](const Rule *lhs, const Rule *rhs) {
      return lhs->getRuleId() < rhs->getRuleId();
    });

    for (const Rule *rule : rules) {
      if (rule == nullptr || rule->isFact()) {
        return fail("source fact rules are not supported for relation: " +
                    relation);
      }
      if (activeRuleIds.count(rule->getRuleId()) == 0) {
        continue;
      }
      // Recursive rules (and any rule in a recursive stratum) are not lifted;
      // their output relations fall back to concrete WMC.
      if (rule->isRecursive() || rule->isInRecursiveStratum()) {
        return fail("recursive rule is not supported: " +
                    std::to_string(rule->getRuleId()));
      }
      if (!rule->getAggregates().empty()) {
        return fail("aggregate rule is not supported: " +
                    std::to_string(rule->getRuleId()));
      }

      std::unordered_set<std::string> headVariables;
      const auto &headFields = rule->getHead().getFields();
      auto *headRelation = program.getRelation(relation);
      if (headRelation == nullptr ||
          headFields.size() != headRelation->getArity()) {
        return fail("head arity mismatch for rule: " +
                    std::to_string(rule->getRuleId()));
      }
      for (const auto &field : headFields) {
        if (!std::holds_alternative<VariableField>(field.field)) {
          return fail("head is not a distinct-variable tuple for rule: " +
                      std::to_string(rule->getRuleId()));
        }
        const auto &name = std::get<VariableField>(field.field).name;
        if (name == "_" || !headVariables.insert(name).second) {
          return fail("head contains wildcard or repeated variable for rule: " +
                      std::to_string(rule->getRuleId()));
        }
      }

      const auto ruleVariables = rule->getVars();
      std::unordered_set<std::string> ruleVariableSet(ruleVariables.begin(),
                                                      ruleVariables.end());
      if (ruleVariableSet.size() != ruleVariables.size()) {
        return fail("rule variable inventory contains duplicates for rule: " +
                    std::to_string(rule->getRuleId()));
      }
      for (const auto &headVariable : headVariables) {
        if (ruleVariableSet.count(headVariable) == 0) {
          return fail(
              "head variable is absent from runtime grounding for rule: " +
              std::to_string(rule->getRuleId()));
        }
      }

      for (const auto &bodyAtom : rule->getBodyAtoms()) {
        if (bodyAtom.isNegatedAtom()) {
          return fail("negated body atom is not supported for rule: " +
                      std::to_string(rule->getRuleId()));
        }
        auto *bodyRelation = program.getRelation(bodyAtom.getRelation());
        if (bodyRelation == nullptr ||
            bodyRelation->getArity() != bodyAtom.getFields().size()) {
          return fail("body relation is missing or has mismatched arity: " +
                      bodyAtom.getRelation());
        }
        for (const auto &field : bodyAtom.getFields()) {
          if (std::holds_alternative<VariableField>(field.field)) {
            const auto &name = std::get<VariableField>(field.field).name;
            if (name == "_" || ruleVariableSet.count(name) == 0) {
              return fail("body contains an ungrounded variable for rule: " +
                          std::to_string(rule->getRuleId()));
            }
          } else if (std::holds_alternative<std::shared_ptr<ExprField>>(
                         field.field)) {
            return fail("body expression field is not supported for rule: " +
                        std::to_string(rule->getRuleId()));
          }
        }
      }

      if (!hasConnectedBindingPath(*rule, headVariables, ruleVariableSet)) {
        return false;
      }
      if (ruleVariableSet != headVariables &&
          !hasUniqueRuntimeWitnesses(*rule, relation)) {
        return false;
      }

      for (const auto &bodyAtom : rule->getBodyAtoms()) {
        if (!analyzeRelation(bodyAtom.getRelation())) {
          return false;
        }
      }
    }

    active.erase(relation);
    accepted.insert(relation);
    return true;
  }

  bool hasConnectedBindingPath(
      const Rule &rule, const std::unordered_set<std::string> &headVariables,
      const std::unordered_set<std::string> &ruleVariables) {
    std::vector<std::unordered_set<std::string>> pendingAtoms;
    pendingAtoms.reserve(rule.getBodyAtoms().size());
    for (const auto &bodyAtom : rule.getBodyAtoms()) {
      std::unordered_set<std::string> atomVariables;
      for (const auto &field : bodyAtom.getFields()) {
        if (std::holds_alternative<VariableField>(field.field)) {
          atomVariables.insert(std::get<VariableField>(field.field).name);
        }
      }
      pendingAtoms.push_back(std::move(atomVariables));
    }

    std::unordered_set<std::string> bound = headVariables;
    std::vector<bool> selected(pendingAtoms.size(), false);
    std::size_t selectedCount = 0;
    while (selectedCount < pendingAtoms.size()) {
      bool madeProgress = false;
      for (std::size_t i = 0; i < pendingAtoms.size(); ++i) {
        if (selected[i]) {
          continue;
        }
        const auto &variables = pendingAtoms[i];
        const bool connected =
            variables.empty() || std::any_of(variables.begin(), variables.end(),
                                             [&](const std::string &variable) {
                                               return bound.count(variable) > 0;
                                             });
        if (!connected) {
          continue;
        }
        bound.insert(variables.begin(), variables.end());
        selected[i] = true;
        ++selectedCount;
        madeProgress = true;
      }
      if (!madeProgress) {
        return fail(
            "body has a disconnected free-variable component for rule: " +
            std::to_string(rule.getRuleId()));
      }
    }

    for (const auto &variable : ruleVariables) {
      if (bound.count(variable) == 0) {
        return fail("runtime grounding variable is not bound by a body atom "
                    "for rule: " +
                    std::to_string(rule.getRuleId()));
      }
    }
    return true;
  }

  bool hasUniqueRuntimeWitnesses(const Rule &rule,
                                 const std::string &relation) {
    const auto ruleVariables = rule.getVars();
    for (const auto &[tuple, applications] :
         DerivationManager::untypedTuple2RuleApplications) {
      if (tuple.relation_name != relation || applications == nullptr) {
        continue;
      }
      std::size_t matchingApplications = 0;
      for (const auto &application : *applications) {
        if (static_cast<std::size_t>(application.ruleId) != rule.getRuleId()) {
          continue;
        }
        if (application.varValuesPure.size() != ruleVariables.size()) {
          return fail("runtime grounding arity mismatch for rule: " +
                      std::to_string(rule.getRuleId()));
        }
        ++matchingApplications;
        if (matchingApplications > 1) {
          return fail("multiple runtime witnesses for rule: " +
                      std::to_string(rule.getRuleId()));
        }
      }
    }
    return true;
  }

  bool fail(std::string message) {
    if (reason.empty()) {
      reason = std::move(message);
    }
    return false;
  }

  SouffleProgram &program;
  const RuleManager &ruleManager;
  const std::unordered_set<std::size_t> &activeRuleIds;
  std::unordered_set<std::string> accepted;
  std::unordered_set<std::string> active;
  std::unordered_set<std::string> relationClosure;
  std::string reason;
};

class ParameterizedForwardCompiler {
public:
  ParameterizedForwardCompiler(const AbstractDerivationGraph &graph,
                               SymbolicBdd &bdd)
      : graph(graph), bdd(bdd) {}

  BddId compile(AbstractNodeId nodeId) {
    auto cached = nodeFormulas.find(nodeId);
    if (cached != nodeFormulas.end()) {
      return cached->second;
    }
    if (!activeNodes.insert(nodeId).second) {
      throw std::runtime_error("recursive parameterized forward compilation");
    }

    const auto &node = graph.getNode(nodeId);
    BddId result = bdd.getFalse();
    if (node.extensional) {
      SymbolicVariable fact;
      fact.kind = SymbolicVariableKind::ExtensionalFact;
      fact.node = nodeId;
      result = bdd.makeVariable(fact);
    }
    for (const auto edgeId : node.incomingEdges) {
      const auto &edge = graph.getEdge(edgeId);
      SymbolicVariable guard;
      guard.kind = SymbolicVariableKind::RuleApplicationGuard;
      guard.edge = edgeId;
      BddId alternative = bdd.makeVariable(guard);

      if (edge.probability <= 0.0) {
        alternative = bdd.getFalse();
      } else if (edge.probability < 1.0) {
        SymbolicVariable random;
        random.kind = SymbolicVariableKind::ProbabilisticRule;
        random.edge = edgeId;
        alternative = bdd.makeAnd(alternative, bdd.makeVariable(random));
      }

      for (const auto bodyNode : edge.body) {
        alternative = bdd.makeAnd(alternative, compile(bodyNode));
      }
      result = bdd.makeOr(result, alternative);
    }

    activeNodes.erase(nodeId);
    nodeFormulas.emplace(nodeId, result);
    return result;
  }

private:
  const AbstractDerivationGraph &graph;
  SymbolicBdd &bdd;
  std::unordered_map<AbstractNodeId, BddId> nodeFormulas;
  std::unordered_set<AbstractNodeId> activeNodes;
};

struct ConcreteVariableKey {
  SymbolicVariableKind kind = SymbolicVariableKind::ExtensionalFact;
  std::string relation;
  std::size_t ruleId = 0;
  std::vector<RamDomain> arguments;

  bool operator<(const ConcreteVariableKey &other) const {
    if (kind != other.kind) {
      return kind < other.kind;
    }
    if (relation != other.relation) {
      return relation < other.relation;
    }
    if (ruleId != other.ruleId) {
      return ruleId < other.ruleId;
    }
    return std::lexicographical_compare(arguments.begin(), arguments.end(),
                                        other.arguments.begin(),
                                        other.arguments.end());
  }
};

class RuntimeBindingContext {
public:
  RuntimeBindingContext(const AbstractDerivationGraph &graph,
                        const std::vector<RamDomain> &rootValues)
      : graph(graph), rootValues(rootValues) {}

  RamDomain evaluate(const ParamTerm &term) {
    if (term.kind == ParamTermKind::Constant) {
      return term.constant;
    }
    if (term.kind == ParamTermKind::RootParameter) {
      if (term.parameter >= rootValues.size()) {
        throw std::runtime_error("lifted root parameter index is out of range");
      }
      return rootValues[term.parameter];
    }

    const RuleApplication *application = resolve(term.witnessEdge);
    if (application == nullptr) {
      throw std::runtime_error(
          "witness parameter has no runtime rule application");
    }
    if (term.parameter >= application->varValuesPure.size()) {
      throw std::runtime_error(
          "lifted witness parameter index is out of range");
    }
    return application->varValuesPure[term.parameter];
  }

  std::vector<RamDomain> evaluateTerms(const std::vector<ParamTerm> &terms) {
    std::vector<RamDomain> values;
    values.reserve(terms.size());
    for (const auto &term : terms) {
      values.push_back(evaluate(term));
    }
    return values;
  }

  const RuleApplication *resolve(AbstractEdgeId edgeId) {
    if (resolvedEdges.count(edgeId) > 0) {
      return applications.at(edgeId);
    }
    if (!activeEdges.insert(edgeId).second) {
      throw std::runtime_error("recursive lifted witness resolution");
    }

    const auto &edge = graph.getEdge(edgeId);
    const auto &domain = edge.domain;
    UntypedTuple headTuple{domain.headRelation,
                           evaluateTerms(domain.headArguments)};
    auto tupleIt =
        DerivationManager::untypedTuple2RuleApplications.find(headTuple);
    const RuleApplication *match = nullptr;
    if (tupleIt != DerivationManager::untypedTuple2RuleApplications.end() &&
        tupleIt->second != nullptr) {
      for (const auto &application : *tupleIt->second) {
        if (static_cast<std::size_t>(application.ruleId) != domain.ruleId) {
          continue;
        }
        if (match != nullptr) {
          throw std::runtime_error(
              "multiple runtime witnesses reached lifted evaluation");
        }
        match = &application;
      }
    }

    activeEdges.erase(edgeId);
    resolvedEdges.insert(edgeId);
    applications.emplace(edgeId, match);
    return match;
  }

private:
  const AbstractDerivationGraph &graph;
  const std::vector<RamDomain> &rootValues;
  std::unordered_set<AbstractEdgeId> resolvedEdges;
  std::unordered_set<AbstractEdgeId> activeEdges;
  std::unordered_map<AbstractEdgeId, const RuleApplication *> applications;
};

class LiftedEvaluator {
public:
  LiftedEvaluator(SouffleProgram &program,
                  const std::unordered_map<UntypedTuple, double> &factProb,
                  const AbstractDerivationGraph &graph, const SymbolicBdd &bdd)
      : program(program), factProb(factProb), graph(graph), bdd(bdd) {}

  double evaluate(BddId root, const std::vector<RamDomain> &rootValues) {
    RuntimeBindingContext bindings(graph, rootValues);
    std::map<SymbolicVariable, double> variableWeights;
    std::map<ConcreteVariableKey, SymbolicVariable> concreteOwners;
    auto getVariableWeight = [&](const SymbolicVariable &variable) {
      auto cached = variableWeights.find(variable);
      if (cached != variableWeights.end()) {
        return cached->second;
      }

      if (variable.kind == SymbolicVariableKind::RuleApplicationGuard) {
        const double weight =
            bindings.resolve(variable.edge) == nullptr ? 0.0 : 1.0;
        variableWeights.emplace(variable, weight);
        return weight;
      }

      ConcreteVariableKey concrete = concretize(variable, bindings);
      auto [it, inserted] = concreteOwners.emplace(concrete, variable);
      if (!inserted && !(it->second == variable)) {
        throw std::runtime_error(
            "symbolic random-variable families alias for a runtime binding");
      }
      const double weight = evaluateWeight(variable, concrete);
      variableWeights.emplace(variable, weight);
      return weight;
    };

    std::vector<double> memo(bdd.nodeCount(),
                             std::numeric_limits<double>::quiet_NaN());
    memo[bdd.getFalse()] = 0.0;
    memo[bdd.getTrue()] = 1.0;
    std::function<double(BddId)> visit = [&](BddId id) -> double {
      if (!std::isnan(memo.at(id))) {
        return memo[id];
      }
      const BddNode &node = bdd.getNode(id);
      const double weight = getVariableWeight(node.variable);
      double value;
      if (weight <= 0.0) {
        value = visit(node.low);
      } else if (weight >= 1.0) {
        value = visit(node.high);
      } else {
        value = weight * visit(node.high) + (1.0 - weight) * visit(node.low);
      }
      memo[id] = value;
      return value;
    };
    return visit(root);
  }

private:
  ConcreteVariableKey concretize(const SymbolicVariable &variable,
                                 RuntimeBindingContext &bindings) const {
    ConcreteVariableKey result;
    result.kind = variable.kind;
    if (variable.kind == SymbolicVariableKind::ExtensionalFact) {
      const auto &node = graph.getNode(variable.node);
      result.relation = node.relation;
      result.arguments = bindings.evaluateTerms(node.arguments);
    } else {
      const auto &edge = graph.getEdge(variable.edge);
      result.ruleId = edge.domain.ruleId;
      result.arguments = bindings.evaluateTerms(edge.domain.groundingKey);
    }
    return result;
  }

  double evaluateWeight(const SymbolicVariable &variable,
                        const ConcreteVariableKey &concrete) const {
    if (variable.kind == SymbolicVariableKind::ExtensionalFact) {
      const auto &node = graph.getNode(variable.node);
      UntypedTuple tuple{node.relation, concrete.arguments};
      auto probIt = factProb.find(tuple);
      if (probIt != factProb.end()) {
        return probIt->second;
      }
      Relation *relation = program.getRelation(node.relation);
      if (relation == nullptr ||
          relation->getArity() != concrete.arguments.size()) {
        return 0.0;
      }
      souffle::tuple probe(relation);
      for (std::size_t i = 0; i < concrete.arguments.size(); ++i) {
        probe[i] = concrete.arguments[i];
      }
      return relation->contains(probe) ? 1.0 : 0.0;
    }

    const auto &edge = graph.getEdge(variable.edge);
    if (variable.kind == SymbolicVariableKind::ProbabilisticRule) {
      return edge.probability;
    }
    throw std::runtime_error(
        "rule guard must be evaluated before concretizing");
  }

  SouffleProgram &program;
  const std::unordered_map<UntypedTuple, double> &factProb;
  const AbstractDerivationGraph &graph;
  const SymbolicBdd &bdd;
};

struct ConcreteBddNode {
  ConcreteVariableKey variable;
  std::uint32_t low = 0;
  std::uint32_t high = 0;
  bool terminal = true;
};

struct ConcreteBddNodeKey {
  ConcreteVariableKey variable;
  std::uint32_t low = 0;
  std::uint32_t high = 0;

  bool operator<(const ConcreteBddNodeKey &other) const {
    if (variable < other.variable) {
      return true;
    }
    if (other.variable < variable) {
      return false;
    }
    return std::tie(low, high) < std::tie(other.low, other.high);
  }
};

class ConcreteWeightedBdd {
public:
  ConcreteWeightedBdd() {
    nodes.emplace_back();
    nodes.emplace_back();
  }

  std::uint32_t getFalse() const { return 0; }
  std::uint32_t getTrue() const { return 1; }

  std::uint32_t makeVariable(const ConcreteVariableKey &variable,
                             double weight) {
    auto [it, inserted] = variableWeights.emplace(variable, weight);
    if (!inserted && std::fabs(it->second - weight) > 1e-12) {
      throw std::runtime_error("multi-witness variable weight conflict");
    }
    return makeNode(variable, getFalse(), getTrue());
  }

  std::uint32_t makeAnd(std::uint32_t lhs, std::uint32_t rhs) {
    return apply(BddOperation::And, lhs, rhs);
  }

  std::uint32_t makeOr(std::uint32_t lhs, std::uint32_t rhs) {
    return apply(BddOperation::Or, lhs, rhs);
  }

  std::uint32_t makeNot(std::uint32_t node) {
    if (node == getFalse()) {
      return getTrue();
    }
    if (node == getTrue()) {
      return getFalse();
    }
    auto it = notCache.find(node);
    if (it != notCache.end()) {
      return it->second;
    }
    // Copy fields before recursing: makeNode may reallocate `nodes`.
    const auto variable = nodes.at(node).variable;
    const auto low = nodes.at(node).low;
    const auto high = nodes.at(node).high;
    const auto result = makeNode(variable, makeNot(low), makeNot(high));
    notCache.emplace(node, result);
    return result;
  }

  double wmc(std::uint32_t root) const {
    std::vector<double> memo(nodes.size(),
                             std::numeric_limits<double>::quiet_NaN());
    memo[getFalse()] = 0.0;
    memo[getTrue()] = 1.0;
    std::function<double(std::uint32_t)> visit = [&](std::uint32_t id) {
      if (!std::isnan(memo.at(id))) {
        return memo[id];
      }
      const auto &node = nodes.at(id);
      const auto weightIt = variableWeights.find(node.variable);
      if (weightIt == variableWeights.end()) {
        throw std::runtime_error("multi-witness BDD variable has no weight");
      }
      const double weight = weightIt->second;
      const double value = weight * visit(node.high) +
                           (1.0 - weight) * visit(node.low);
      memo[id] = value;
      return value;
    };
    return visit(root);
  }

  std::size_t nodeCount() const { return nodes.size(); }
  std::size_t variableCount() const { return variableWeights.size(); }

private:
  std::uint32_t makeNode(const ConcreteVariableKey &variable,
                         std::uint32_t low, std::uint32_t high) {
    if (low == high) {
      return low;
    }
    ConcreteBddNodeKey key{variable, low, high};
    auto it = uniqueNodes.find(key);
    if (it != uniqueNodes.end()) {
      return it->second;
    }
    if (nodes.size() >= std::numeric_limits<std::uint32_t>::max()) {
      throw std::runtime_error("multi-witness BDD node limit exceeded");
    }
    const auto id = static_cast<std::uint32_t>(nodes.size());
    ConcreteBddNode node;
    node.variable = variable;
    node.low = low;
    node.high = high;
    node.terminal = false;
    nodes.push_back(std::move(node));
    uniqueNodes.emplace(std::move(key), id);
    return id;
  }

  std::uint32_t apply(BddOperation operation, std::uint32_t lhs,
                      std::uint32_t rhs) {
    if (lhs > rhs) {
      std::swap(lhs, rhs);
    }
    if (operation == BddOperation::And) {
      if (lhs == getFalse() || rhs == getFalse()) {
        return getFalse();
      }
      if (lhs == getTrue()) {
        return rhs;
      }
      if (lhs == rhs) {
        return lhs;
      }
    } else {
      if (lhs == getTrue() || rhs == getTrue()) {
        return getTrue();
      }
      if (lhs == getFalse()) {
        return rhs;
      }
      if (lhs == rhs) {
        return lhs;
      }
    }

    const auto cacheKey = std::make_tuple(operation, lhs, rhs);
    auto cached = applyCache.find(cacheKey);
    if (cached != applyCache.end()) {
      return cached->second;
    }

    const auto &lhsNode = nodes.at(lhs);
    const auto &rhsNode = nodes.at(rhs);
    ConcreteVariableKey top;
    if (lhsNode.terminal) {
      top = rhsNode.variable;
    } else if (rhsNode.terminal || lhsNode.variable < rhsNode.variable) {
      top = lhsNode.variable;
    } else {
      top = rhsNode.variable;
    }

    auto cofactor = [&](std::uint32_t id, bool high) {
      const auto &node = nodes.at(id);
      if (!node.terminal && !(top < node.variable) && !(node.variable < top)) {
        return high ? node.high : node.low;
      }
      return id;
    };

    const auto low =
        apply(operation, cofactor(lhs, false), cofactor(rhs, false));
    const auto high =
        apply(operation, cofactor(lhs, true), cofactor(rhs, true));
    const auto result = makeNode(top, low, high);
    applyCache.emplace(cacheKey, result);
    return result;
  }

  std::vector<ConcreteBddNode> nodes;
  std::map<ConcreteBddNodeKey, std::uint32_t> uniqueNodes;
  std::map<std::tuple<BddOperation, std::uint32_t, std::uint32_t>,
           std::uint32_t>
      applyCache;
  std::map<std::uint32_t, std::uint32_t> notCache;
  std::map<ConcreteVariableKey, double> variableWeights;
};

static bool isUsableProbability(double value) {
  return value > 0.0 && value < 1.0;
}

static bool relationContainsTuple(Relation *relation,
                                  const std::vector<RamDomain> &fields) {
  if (relation == nullptr || relation->getArity() != fields.size()) {
    return false;
  }
  souffle::tuple probe(relation);
  for (std::size_t i = 0; i < fields.size(); ++i) {
    probe[i] = fields[i];
  }
  return relation->contains(probe);
}

static std::string sanitizeTemplateField(std::string value);
static bool hasUnsupportedSymbolicField(const std::vector<SymbolicField> &fields);
static std::string joinStrings(const std::vector<std::string> &items,
                               const char *separator);

static void appendLiftedArtifact(LiftedWmcResult &target,
                                 const LiftedWmcResult &extra) {
  if (!extra.abstractGraph.empty()) {
    target.abstractGraph += extra.abstractGraph;
  }
  if (!extra.abstractGraphDot.empty() &&
      (target.abstractGraphDot.empty() ||
          target.abstractGraphDot.find("->") == std::string::npos)) {
    target.abstractGraphDot = extra.abstractGraphDot;
  }
  target.witnessIndexedTemplates += extra.witnessIndexedTemplates;
  target.witnessIndexedWitnesses += extra.witnessIndexedWitnesses;
  target.witnessIndexedRelations += extra.witnessIndexedRelations;
  target.witnessIndexedTupleStats += extra.witnessIndexedTupleStats;
  target.witnessIndexedTemplatesCount += extra.witnessIndexedTemplatesCount;
  target.witnessIndexedRows += extra.witnessIndexedRows;
  target.witnessIndexedTupleFormulas += extra.witnessIndexedTupleFormulas;
}

static std::string sanitizeTemplateField(std::string value) {
  for (char &ch : value) {
    if (ch == '\t' || ch == '\n' || ch == '\r') {
      ch = ' ';
    }
  }
  return value;
}

static std::string symbolicFieldToTemplateString(const SymbolicField &field) {
  if (std::holds_alternative<VariableField>(field.field)) {
    return std::get<VariableField>(field.field).name;
  }
  return "<const>";
}

static std::string atomToTemplateString(const Atom &atom) {
  std::ostringstream out;
  if (atom.isNegatedAtom()) {
    out << '!';
  }
  out << atom.getRelation() << '(';
  const auto &fields = atom.getFields();
  for (std::size_t i = 0; i < fields.size(); ++i) {
    if (i > 0) {
      out << ',';
    }
    out << symbolicFieldToTemplateString(fields[i]);
  }
  out << ')';
  return out.str();
}

static std::string joinStrings(const std::vector<std::string> &items,
                               const char *separator) {
  std::ostringstream out;
  for (std::size_t i = 0; i < items.size(); ++i) {
    if (i > 0) {
      out << separator;
    }
    out << items[i];
  }
  return out.str();
}

static std::string joinRamDomains(const std::vector<RamDomain> &values,
                                  const char *separator = ",") {
  std::ostringstream out;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i > 0) {
      out << separator;
    }
    out << values[i];
  }
  return out.str();
}

static std::string makeWitnessTemplateId(const std::string &relation,
                                         std::size_t ruleId) {
  return relation + "#rule" + std::to_string(ruleId);
}

static std::string describeWitnessTemplate(const Rule &rule,
                                           const std::string &templateId) {
  const auto ruleVars = rule.getVars();
  std::vector<std::string> headVars;
  std::unordered_set<std::string> headVarSet;
  for (const auto &field : rule.getHead().getFields()) {
    const auto text = symbolicFieldToTemplateString(field);
    headVars.push_back(text);
    headVarSet.insert(text);
  }

  std::vector<std::string> witnessVars;
  for (const auto &var : ruleVars) {
    if (headVarSet.count(var) == 0) {
      witnessVars.push_back(var);
    }
  }

  std::vector<std::string> bodyAtoms;
  bodyAtoms.reserve(rule.getBodyAtoms().size());
  for (const auto &atom : rule.getBodyAtoms()) {
    bodyAtoms.push_back(atomToTemplateString(atom));
  }

  std::ostringstream formula;
  formula << "OrOver " << templateId << "(head; witness): And(";
  for (std::size_t i = 0; i < bodyAtoms.size(); ++i) {
    if (i > 0) {
      formula << ", ";
    }
    formula << "Ref(" << bodyAtoms[i] << ")";
  }
  if (!rule.isDeterminstic()) {
    if (!bodyAtoms.empty()) {
      formula << ", ";
    }
    formula << "Event(rule=" << rule.getRuleId() << ", head, witness)";
  }
  formula << ')';

  std::ostringstream out;
  out << templateId << '\t' << rule.getHead().getRelation() << '\t'
      << rule.getRuleId() << '\t' << joinStrings(headVars, ",") << '\t'
      << joinStrings(witnessVars, ",") << '\t'
      << sanitizeTemplateField(joinStrings(bodyAtoms, " & ")) << '\t'
      << sanitizeTemplateField(formula.str()) << '\n';
  return out.str();
}

static std::vector<RamDomain> extractWitnessValues(
    const Rule &rule, const std::vector<RamDomain> &allValues) {
  const auto ruleVars = rule.getVars();
  std::unordered_set<std::string> headVarSet;
  for (const auto &field : rule.getHead().getFields()) {
    if (std::holds_alternative<VariableField>(field.field)) {
      headVarSet.insert(std::get<VariableField>(field.field).name);
    }
  }

  std::vector<RamDomain> witnessValues;
  for (std::size_t i = 0; i < ruleVars.size() && i < allValues.size(); ++i) {
    if (headVarSet.count(ruleVars[i]) == 0) {
      witnessValues.push_back(allValues[i]);
    }
  }
  return witnessValues;
}

enum class TemplateSlotKind {
  RuleEvent,
  BodyRef,
};

struct TemplateSlot {
  TemplateSlotKind kind = TemplateSlotKind::BodyRef;
  std::size_t index = 0;

  bool operator<(const TemplateSlot &other) const {
    return std::tie(kind, index) < std::tie(other.kind, other.index);
  }

  bool operator==(const TemplateSlot &other) const {
    return std::tie(kind, index) == std::tie(other.kind, other.index);
  }
};

struct TemplateBddNode {
  TemplateSlot slot;
  std::uint32_t low = 0;
  std::uint32_t high = 0;
  bool terminal = true;
};

struct TemplateBddNodeKey {
  TemplateSlot slot;
  std::uint32_t low = 0;
  std::uint32_t high = 0;

  bool operator<(const TemplateBddNodeKey &other) const {
    if (slot < other.slot) {
      return true;
    }
    if (other.slot < slot) {
      return false;
    }
    return std::tie(low, high) < std::tie(other.low, other.high);
  }
};

class SymbolicTemplateBdd {
public:
  SymbolicTemplateBdd() {
    nodes.emplace_back();
    nodes.emplace_back();
  }

  std::uint32_t getFalse() const { return 0; }
  std::uint32_t getTrue() const { return 1; }

  std::uint32_t makeVariable(TemplateSlot slot) {
    return makeNode(slot, getFalse(), getTrue());
  }

  std::uint32_t makeAnd(std::uint32_t lhs, std::uint32_t rhs) {
    return apply(BddOperation::And, lhs, rhs);
  }

  const TemplateBddNode &getNode(std::uint32_t id) const {
    return nodes.at(id);
  }

  std::size_t nodeCount() const { return nodes.size(); }

  std::size_t internalNodeCount() const {
    return nodes.size() >= 2 ? nodes.size() - 2 : 0;
  }

private:
  std::uint32_t makeNode(TemplateSlot slot, std::uint32_t low,
                         std::uint32_t high) {
    if (low == high) {
      return low;
    }
    TemplateBddNodeKey key{slot, low, high};
    auto it = uniqueNodes.find(key);
    if (it != uniqueNodes.end()) {
      return it->second;
    }
    if (nodes.size() >= std::numeric_limits<std::uint32_t>::max()) {
      throw std::runtime_error("template BDD node limit exceeded");
    }
    const auto id = static_cast<std::uint32_t>(nodes.size());
    TemplateBddNode node;
    node.slot = slot;
    node.low = low;
    node.high = high;
    node.terminal = false;
    nodes.push_back(std::move(node));
    uniqueNodes.emplace(std::move(key), id);
    return id;
  }

  std::uint32_t apply(BddOperation operation, std::uint32_t lhs,
                      std::uint32_t rhs) {
    if (lhs > rhs) {
      std::swap(lhs, rhs);
    }
    if (operation == BddOperation::And) {
      if (lhs == getFalse() || rhs == getFalse()) {
        return getFalse();
      }
      if (lhs == getTrue()) {
        return rhs;
      }
      if (lhs == rhs) {
        return lhs;
      }
    }

    const auto cacheKey = std::make_tuple(operation, lhs, rhs);
    auto cached = applyCache.find(cacheKey);
    if (cached != applyCache.end()) {
      return cached->second;
    }

    const auto &lhsNode = nodes.at(lhs);
    const auto &rhsNode = nodes.at(rhs);
    TemplateSlot top;
    if (lhsNode.terminal) {
      top = rhsNode.slot;
    } else if (rhsNode.terminal || lhsNode.slot < rhsNode.slot) {
      top = lhsNode.slot;
    } else {
      top = rhsNode.slot;
    }

    auto cofactor = [&](std::uint32_t id, bool high) {
      const auto &node = nodes.at(id);
      if (!node.terminal && node.slot == top) {
        return high ? node.high : node.low;
      }
      return id;
    };

    const auto low =
        apply(operation, cofactor(lhs, false), cofactor(rhs, false));
    const auto high =
        apply(operation, cofactor(lhs, true), cofactor(rhs, true));
    const auto result = makeNode(top, low, high);
    applyCache.emplace(cacheKey, result);
    return result;
  }

  std::vector<TemplateBddNode> nodes;
  std::map<TemplateBddNodeKey, std::uint32_t> uniqueNodes;
  std::map<std::tuple<BddOperation, std::uint32_t, std::uint32_t>,
           std::uint32_t>
      applyCache;
};

struct WitnessIndexedBodyRef {
  std::string relation;
  std::vector<SymbolicField> fields;
};

struct WitnessIndexedTemplate {
  std::string id;
  const Rule *rule = nullptr;
  std::size_t ruleId = 0;
  std::string headRelation;
  std::vector<std::string> ruleVariables;
  std::vector<std::string> witnessVariables;
  std::vector<WitnessIndexedBodyRef> bodyRefs;
  bool hasRuleEvent = false;
  SymbolicTemplateBdd templateBdd;
  std::uint32_t templateRoot = 1;

  std::size_t templateDdNodes() const {
    return templateBdd.internalNodeCount();
  }
};

struct WitnessIndexedWitness {
  const WitnessIndexedTemplate *tmpl = nullptr;
  UntypedTuple outputTuple;
  std::vector<RamDomain> ruleVariableValues;
  std::vector<RamDomain> witnessValues;
  std::vector<std::string> bodyTupleTexts;
  std::uint32_t formula = 0;
};

struct WitnessIndexedTupleFormula {
  UntypedTuple outputTuple;
  std::vector<WitnessIndexedWitness> witnesses;
  std::set<std::size_t> ruleIds;
};

struct WitnessIndexedRelationProgram {
  std::string relation;
  std::map<std::size_t, WitnessIndexedTemplate> templatesByRule;
  std::vector<WitnessIndexedTupleFormula> tuples;
};

static WitnessIndexedTemplate compileWitnessIndexedTemplate(
    const Rule &rule, const std::string &templateId) {
  WitnessIndexedTemplate tmpl;
  tmpl.id = templateId;
  tmpl.rule = &rule;
  tmpl.ruleId = rule.getRuleId();
  tmpl.headRelation = rule.getHead().getRelation();
  tmpl.ruleVariables = rule.getVars();
  tmpl.hasRuleEvent = !rule.isDeterminstic();

  std::unordered_set<std::string> headVarSet;
  for (const auto &field : rule.getHead().getFields()) {
    if (std::holds_alternative<VariableField>(field.field)) {
      headVarSet.insert(std::get<VariableField>(field.field).name);
    }
  }
  for (const auto &variable : tmpl.ruleVariables) {
    if (headVarSet.count(variable) == 0) {
      tmpl.witnessVariables.push_back(variable);
    }
  }

  tmpl.bodyRefs.reserve(rule.getBodyAtoms().size());
  for (const auto &atom : rule.getBodyAtoms()) {
    WitnessIndexedBodyRef ref;
    ref.relation = atom.getRelation();
    ref.fields = atom.getFields();
    tmpl.bodyRefs.push_back(std::move(ref));
  }

  std::uint32_t root = tmpl.templateBdd.getTrue();
  if (tmpl.hasRuleEvent) {
    root = tmpl.templateBdd.makeAnd(
        root, tmpl.templateBdd.makeVariable(
                  TemplateSlot{TemplateSlotKind::RuleEvent, 0}));
  }
  for (std::size_t i = 0; i < tmpl.bodyRefs.size(); ++i) {
    root = tmpl.templateBdd.makeAnd(
        root, tmpl.templateBdd.makeVariable(
                  TemplateSlot{TemplateSlotKind::BodyRef, i}));
  }
  tmpl.templateRoot = root;
  return tmpl;
}

static bool isDirectMultiWitnessRuleSupported(
    SouffleProgram &program, const Rule &rule,
    const std::unordered_set<std::string> &inputRelations) {
  if (rule.isFact() || rule.isRecursive() || rule.isInRecursiveStratum() ||
      !rule.getAggregates().empty()) {
    return false;
  }
  if (rule.getBodyAtoms().empty()) {
    return false;
  }
  if (rule.getBodyAtoms().size() < 2) {
    return false;
  }
  for (const auto &atom : rule.getBodyAtoms()) {
    if (atom.isNegatedAtom() || inputRelations.count(atom.getRelation()) == 0) {
      return false;
    }
    Relation *relation = program.getRelation(atom.getRelation());
    if (relation == nullptr || relation->getArity() != atom.getFields().size()) {
      return false;
    }
    for (const auto &field : atom.getFields()) {
      if (std::holds_alternative<std::shared_ptr<ExprField>>(field.field)) {
        return false;
      }
    }
  }
  const auto &head = rule.getHead();
  Relation *headRelation = program.getRelation(head.getRelation());
  if (headRelation == nullptr || headRelation->getArity() != head.getFields().size()) {
    return false;
  }
  const auto ruleVars = rule.getVars();
  std::unordered_set<std::string> ruleVariables(ruleVars.begin(),
                                                ruleVars.end());
  for (const auto &field : head.getFields()) {
    if (!std::holds_alternative<VariableField>(field.field)) {
      return false;
    }
    const auto &name = std::get<VariableField>(field.field).name;
    if (name == "_" || ruleVariables.count(name) == 0) {
      return false;
    }
  }
  return true;
}

static std::vector<RamDomain> evaluateAtomTuple(
    const Atom &atom, const std::vector<std::string> &vars,
    const std::vector<RamDomain> &values) {
  std::vector<RamDomain> fields;
  fields.reserve(atom.getFields().size());
  for (const auto &field : atom.getFields()) {
    fields.push_back(evaluateSymbolicField(field, vars, values));
  }
  return fields;
}

static std::uint32_t instantiateWitnessTemplateSlot(
    SouffleProgram &program,
    const std::unordered_map<UntypedTuple, double> &factProb,
    ConcreteWeightedBdd &bdd, const WitnessIndexedTemplate &tmpl,
    WitnessIndexedWitness &witness, TemplateSlot slot) {
  if (slot.kind == TemplateSlotKind::RuleEvent) {
    ConcreteVariableKey key;
    key.kind = SymbolicVariableKind::ProbabilisticRule;
    key.relation = tmpl.headRelation;
    key.ruleId = tmpl.ruleId;
    key.arguments = witness.ruleVariableValues;
    return bdd.makeVariable(key, tmpl.rule->getProbability());
  }

  if (slot.index >= tmpl.bodyRefs.size()) {
    throw std::runtime_error("template body-ref slot index out of range");
  }
  const auto &ref = tmpl.bodyRefs[slot.index];
  const auto fields =
      evaluateAtomTuple(Atom(ref.relation, ref.fields, false), tmpl.ruleVariables,
                        witness.ruleVariableValues);
  Relation *bodyRelation = program.getRelation(ref.relation);
  if (!relationContainsTuple(bodyRelation, fields)) {
    return bdd.getFalse();
  }
  UntypedTuple bodyTuple{ref.relation, fields};
  witness.bodyTupleTexts.push_back(bodyTuple.toString());
  auto probIt = factProb.find(bodyTuple);
  if (probIt != factProb.end() && isUsableProbability(probIt->second)) {
    ConcreteVariableKey key;
    key.kind = SymbolicVariableKind::ExtensionalFact;
    key.relation = ref.relation;
    key.arguments = fields;
    return bdd.makeVariable(key, probIt->second);
  }
  return bdd.getTrue();
}

static std::uint32_t instantiateWitnessTemplateBdd(
    SouffleProgram &program,
    const std::unordered_map<UntypedTuple, double> &factProb,
    ConcreteWeightedBdd &bdd, const WitnessIndexedTemplate &tmpl,
    WitnessIndexedWitness &witness, std::uint32_t nodeId,
    std::unordered_map<std::uint32_t, std::uint32_t> &nodeMemo,
    std::map<TemplateSlot, std::uint32_t> &slotMemo) {
  if (nodeId == tmpl.templateBdd.getFalse()) {
    return bdd.getFalse();
  }
  if (nodeId == tmpl.templateBdd.getTrue()) {
    return bdd.getTrue();
  }
  auto memoIt = nodeMemo.find(nodeId);
  if (memoIt != nodeMemo.end()) {
    return memoIt->second;
  }

  const auto &node = tmpl.templateBdd.getNode(nodeId);
  auto slotIt = slotMemo.find(node.slot);
  if (slotIt == slotMemo.end()) {
    slotIt = slotMemo
                 .emplace(node.slot,
                          instantiateWitnessTemplateSlot(program, factProb, bdd,
                                                         tmpl, witness, node.slot))
                 .first;
  }
  const auto low = instantiateWitnessTemplateBdd(
      program, factProb, bdd, tmpl, witness, node.low, nodeMemo, slotMemo);
  const auto high = instantiateWitnessTemplateBdd(
      program, factProb, bdd, tmpl, witness, node.high, nodeMemo, slotMemo);
  const auto result =
      bdd.makeOr(bdd.makeAnd(slotIt->second, high),
                 bdd.makeAnd(/* not(slot) branch is encoded by low only when slot=false */
                             slotIt->second == bdd.getFalse() ? bdd.getTrue()
                                                              : bdd.getFalse(),
                             low));
  // The current template compiler only emits conjunction templates whose low
  // branch is false. If this changes, add concrete BDD negation before using
  // arbitrary low branches.
  if (low != bdd.getFalse()) {
    throw std::runtime_error(
        "template BDD instantiation requires negation support for non-false low branch");
  }
  nodeMemo.emplace(nodeId, result);
  return result;
}

static std::uint32_t compileWitnessIndexedRow(
    SouffleProgram &program,
    const std::unordered_map<UntypedTuple, double> &factProb,
    ConcreteWeightedBdd &bdd, const WitnessIndexedTemplate &tmpl,
    WitnessIndexedWitness &witness) {
  if (tmpl.rule == nullptr ||
      witness.ruleVariableValues.size() != tmpl.ruleVariables.size()) {
    throw std::runtime_error("witness-indexed row has invalid rule binding");
  }

  witness.witnessValues = extractWitnessValues(*tmpl.rule, witness.ruleVariableValues);
  witness.bodyTupleTexts.clear();
  std::unordered_map<std::uint32_t, std::uint32_t> nodeMemo;
  std::map<TemplateSlot, std::uint32_t> slotMemo;
  std::uint32_t term = instantiateWitnessTemplateBdd(
      program, factProb, bdd, tmpl, witness, tmpl.templateRoot, nodeMemo,
      slotMemo);
  witness.formula = term;
  return term;
}

static bool hasUnsupportedSymbolicField(const std::vector<SymbolicField> &fields) {
  for (const auto &field : fields) {
    if (std::holds_alternative<std::shared_ptr<ExprField>>(field.field)) {
      return true;
    }
  }
  return false;
}

static double evaluateWitnessIndexedTuple(
    SouffleProgram &program,
    const std::unordered_map<UntypedTuple, double> &factProb,
    WitnessIndexedTupleFormula &tupleFormula, ConcreteWeightedBdd &bdd) {
  std::uint32_t root = bdd.getFalse();
  for (auto &witness : tupleFormula.witnesses) {
    if (witness.tmpl == nullptr) {
      throw std::runtime_error("witness-indexed tuple has a witness without template");
    }
    const std::uint32_t term =
        compileWitnessIndexedRow(program, factProb, bdd, *witness.tmpl, witness);
    root = bdd.makeOr(root, term);
  }
  return bdd.wmc(root);
}

static bool tryEvaluateDirectMultiWitnessOutputs(
    const CmdOptions &opt, SouffleProgram &program,
    const RuleManager &ruleManager,
    const std::unordered_map<UntypedTuple, double> &factProb,
    LiftedWmcResult &result) {
  const auto outputs = program.getOutputRelations();
  std::unordered_set<std::string> inputRelations;
  for (const auto *input : program.getInputRelations()) {
    if (input != nullptr) {
      inputRelations.insert(input->getName());
    }
  }

  std::size_t totalOutputRelations = 0;
  std::size_t totalBddNodes = 0;
  std::size_t totalVariables = 0;
  std::size_t totalAbstractEdges = 0;
  std::size_t totalAbstractNodes = 0;
  std::size_t totalWitnessTemplates = 0;
  std::size_t totalWitnessRows = 0;
  std::size_t totalWitnessTemplateDdNodes = 0;
  std::set<std::string> handledRelations;
  std::ostringstream graphTsv;
  graphTsv << "kind\trelation\ttuple\trule_id\twitnesses\tformula\n";
  std::ostringstream graphDot;
  graphDot << "digraph MultiWitnessLiftedFormulas {\n  rankdir=LR;\n";
  std::ostringstream templateTsv;
  templateTsv << "template_id\thead_relation\trule_id\thead_vars\twitness_vars"
              << "\tbody_atoms\tformula\n";
  std::ostringstream witnessTsv;
  witnessTsv << "template_id\toutput_tuple\twitness_values\tbody_tuples\n";
  std::ostringstream relationStatsTsv;
  relationStatsTsv << "relation\ttemplates\ttuple_formulas\twitness_rows"
                   << "\ttemplate_dd_nodes\tbdd_nodes\tbdd_variables\thandled\n";
  std::ostringstream tupleStatsTsv;
  tupleStatsTsv << "relation\toutput_tuple\trule_ids\twitness_rows"
                << "\tbdd_nodes\tbdd_variables\tprobability\n";

  for (auto *output : outputs) {
    if (output == nullptr) {
      continue;
    }
    ++totalOutputRelations;
    const std::string relation = output->getName();
    if (output->size() < opt.getLiftedWmcThreshold()) {
      result.rejectedOutputReasons.emplace(relation, "below_runtime_threshold");
      continue;
    }

    std::vector<const Rule *> supportedRules;
    std::unordered_set<std::size_t> supportedRuleIds;
    for (const Rule *rule : ruleManager.getRulesForPredicate(relation)) {
      if (rule != nullptr &&
          isDirectMultiWitnessRuleSupported(program, *rule, inputRelations)) {
        supportedRules.push_back(rule);
        supportedRuleIds.insert(rule->getRuleId());
      }
    }
    if (supportedRules.empty()) {
      result.rejectedOutputReasons.emplace(
          relation, "no_supported_direct_multi_witness_rule");
      continue;
    }

    bool relationHadMultiWitness = false;
    bool relationOk = true;
    std::map<std::string, double> relationProbabilities;
    std::ostringstream relationTemplateTsv;
    std::ostringstream relationWitnessTsv;
    std::ostringstream relationTupleStatsTsv;
    std::unordered_set<std::size_t> emittedTemplateRules;
    WitnessIndexedRelationProgram relationProgram;
    relationProgram.relation = relation;
    std::size_t relationEdges = 0;
    std::size_t relationNodes = 0;
    std::size_t relationBddNodes = 0;
    std::size_t relationBddVariables = 0;
    std::size_t relationTemplateDdNodes = 0;

    for (const Rule *rule : supportedRules) {
      if (rule != nullptr && emittedTemplateRules.insert(rule->getRuleId()).second) {
        auto tmpl = compileWitnessIndexedTemplate(
            *rule, makeWitnessTemplateId(relation, rule->getRuleId()));
        relationTemplateDdNodes += tmpl.templateDdNodes();
        relationProgram.templatesByRule.emplace(rule->getRuleId(), std::move(tmpl));
        relationTemplateTsv << describeWitnessTemplate(
            *rule, makeWitnessTemplateId(relation, rule->getRuleId()));
      }
    }

    for (auto it = output->begin(); it != output->end(); ++it) {
      const UntypedTuple tuple = UntypedTuple::fromSouffleTuple(*it);
      auto appsIt = DerivationManager::untypedTuple2RuleApplications.find(tuple);
      if (appsIt == DerivationManager::untypedTuple2RuleApplications.end() ||
          appsIt->second == nullptr || appsIt->second->empty()) {
        relationOk = false;
        result.rejectedOutputReasons.emplace(relation,
                                             "missing_runtime_witnesses");
        break;
      }

      WitnessIndexedTupleFormula tupleFormula;
      tupleFormula.outputTuple = tuple;
      std::set<std::size_t> tupleRuleIds;
      for (const auto &application : *appsIt->second) {
        const auto ruleId = static_cast<std::size_t>(application.ruleId);
        if (supportedRuleIds.count(ruleId) == 0) {
          relationOk = false;
          result.rejectedOutputReasons.emplace(
              relation, "unsupported_runtime_witness_rule");
          break;
        }
        const Rule *rule = nullptr;
        for (const Rule *candidate : supportedRules) {
          if (candidate->getRuleId() == ruleId) {
            rule = candidate;
            break;
          }
        }
        if (rule == nullptr ||
            application.varValuesPure.size() != rule->getVars().size()) {
          relationOk = false;
          result.rejectedOutputReasons.emplace(relation,
                                               "runtime_witness_arity_mismatch");
          break;
        }

        auto tmplIt = relationProgram.templatesByRule.find(ruleId);
        if (tmplIt == relationProgram.templatesByRule.end()) {
          relationOk = false;
          result.rejectedOutputReasons.emplace(relation,
                                               "missing_witness_template");
          break;
        }
        WitnessIndexedWitness witness;
        witness.tmpl = &tmplIt->second;
        witness.outputTuple = tuple;
        witness.ruleVariableValues = application.varValuesPure;
        witness.witnessValues = extractWitnessValues(*rule, witness.ruleVariableValues);
        tupleRuleIds.insert(ruleId);
        tupleFormula.ruleIds.insert(ruleId);
        tupleFormula.witnesses.push_back(std::move(witness));
      }
      if (!relationOk) {
        break;
      }

      ConcreteWeightedBdd bdd;
      const double probability =
          evaluateWitnessIndexedTuple(program, factProb, tupleFormula, bdd);
      const std::size_t witnessCount = tupleFormula.witnesses.size();
      relationProgram.tuples.push_back(std::move(tupleFormula));
      const auto &storedTuple = relationProgram.tuples.back();
      for (const auto &witness : storedTuple.witnesses) {
        if (witness.tmpl == nullptr) {
          relationOk = false;
          result.rejectedOutputReasons.emplace(relation,
                                               "missing_witness_template");
          break;
        }
        relationWitnessTsv << witness.tmpl->id << '\t'
                           << sanitizeTemplateField(witness.outputTuple.toString()) << '\t'
                           << joinRamDomains(witness.witnessValues) << '\t'
                           << sanitizeTemplateField(joinStrings(witness.bodyTupleTexts, " & "))
                           << '\n';
      }
      if (!relationOk) {
        break;
      }
      if (witnessCount > 1) {
        relationHadMultiWitness = true;
      }
      relationProbabilities.emplace(tuple.toString(), probability);
      totalBddNodes += bdd.nodeCount();
      totalVariables += bdd.variableCount();
      relationBddNodes += bdd.nodeCount();
      relationBddVariables += bdd.variableCount();
      relationEdges += witnessCount;
      relationNodes += 1;

      graphTsv << "multi_witness\t" << relation << '\t' << tuple.toString()
               << '\t';
      std::ostringstream tupleRuleIdsText;
      bool first = true;
      for (const auto ruleId : tupleRuleIds) {
        if (!first) {
          graphTsv << ',';
          tupleRuleIdsText << ',';
        }
        graphTsv << ruleId;
        tupleRuleIdsText << ruleId;
        first = false;
      }
      graphTsv << '\t' << witnessCount
               << "\tOR_w AND(body_facts(rule,w), rule_event(rule,w))\n";
      relationTupleStatsTsv
          << relation << '\t' << sanitizeTemplateField(tuple.toString())
          << '\t' << tupleRuleIdsText.str() << '\t' << witnessCount << '\t'
          << bdd.nodeCount() << '\t' << bdd.variableCount() << '\t'
          << std::setprecision(17) << probability << '\n';
      graphDot << "  \"" << escapeDotLabel(relation)
               << "_template\" [shape=ellipse,label=\""
               << escapeDotLabel(relation) << "\\nmulti-witness template\"];\n";
      graphDot << "  \"" << escapeDotLabel(tuple.toString())
               << "\" [shape=box,label=\"" << escapeDotLabel(tuple.toString())
               << "\\nwitnesses=" << witnessCount << "\"];\n";
      graphDot << "  \"" << escapeDotLabel(relation) << "_template\" -> \""
               << escapeDotLabel(tuple.toString()) << "\";\n";
    }

    if (!relationOk || !relationHadMultiWitness) {
      if (!relationHadMultiWitness &&
          result.rejectedOutputReasons.count(relation) == 0) {
        result.rejectedOutputReasons.emplace(relation, "no_multi_witness_tuple");
      }
      continue;
    }

    result.probabilities.insert(relationProbabilities.begin(),
                                relationProbabilities.end());
    templateTsv << relationTemplateTsv.str();
    witnessTsv << relationWitnessTsv.str();
    tupleStatsTsv << relationTupleStatsTsv.str();
    relationStatsTsv << relation << '\t' << relationProgram.templatesByRule.size()
                     << '\t' << relationProgram.tuples.size() << '\t'
                     << relationEdges << '\t' << relationTemplateDdNodes << '\t'
                     << relationBddNodes << '\t' << relationBddVariables << "\t1\n";
    result.handledOutputRelations.push_back(relation);
    result.liftedOutputTuples += output->size();
    handledRelations.insert(relation);
    for (const Rule *rule : supportedRules) {
      handledRelations.insert(rule->getHead().getRelation());
      for (const auto &atom : rule->getBodyAtoms()) {
        handledRelations.insert(atom.getRelation());
      }
    }
    totalAbstractEdges += relationEdges;
    totalAbstractNodes += relationNodes;
    totalWitnessTemplates += relationProgram.templatesByRule.size();
    totalWitnessTemplateDdNodes += relationTemplateDdNodes;
    for (const auto &tupleFormula : relationProgram.tuples) {
      totalWitnessRows += tupleFormula.witnesses.size();
    }
  }

  std::set<std::string> outputRelationNames;
  for (const auto *output : outputs) {
    if (output != nullptr) {
      outputRelationNames.insert(output->getName());
    }
  }
  std::set<std::string> candidateInternalRelations;
  for (const Rule *rule : ruleManager.getAllRules()) {
    if (rule == nullptr) {
      continue;
    }
    const std::string relation = rule->getHead().getRelation();
    if (outputRelationNames.count(relation) == 0 &&
        handledRelations.count(relation) == 0) {
      candidateInternalRelations.insert(relation);
    }
  }

  for (const auto &relation : candidateInternalRelations) {
    Relation *runtimeRelation = program.getRelation(relation);
    if (runtimeRelation == nullptr ||
        runtimeRelation->size() < opt.getLiftedWmcThreshold()) {
      continue;
    }

    std::vector<const Rule *> supportedRules;
    std::unordered_set<std::size_t> supportedRuleIds;
    for (const Rule *rule : ruleManager.getRulesForPredicate(relation)) {
      if (rule != nullptr &&
          isDirectMultiWitnessRuleSupported(program, *rule, inputRelations)) {
        supportedRules.push_back(rule);
        supportedRuleIds.insert(rule->getRuleId());
      }
    }
    if (supportedRules.empty()) {
      continue;
    }

    bool relationOk = true;
    bool relationHadMultiWitness = false;
    std::size_t relationEdges = 0;
    std::size_t relationTemplateDdNodes = 0;
    std::unordered_set<std::size_t> emittedTemplateRules;
    std::ostringstream relationTemplateTsv;
    for (const Rule *rule : supportedRules) {
      if (rule != nullptr &&
          emittedTemplateRules.insert(rule->getRuleId()).second) {
        auto tmpl = compileWitnessIndexedTemplate(
            *rule, makeWitnessTemplateId(relation, rule->getRuleId()));
        relationTemplateDdNodes += tmpl.templateDdNodes();
        relationTemplateTsv << describeWitnessTemplate(
            *rule, makeWitnessTemplateId(relation, rule->getRuleId()));
      }
    }

    for (auto it = runtimeRelation->begin(); it != runtimeRelation->end();
         ++it) {
      const UntypedTuple tuple = UntypedTuple::fromSouffleTuple(*it);
      auto appsIt = DerivationManager::untypedTuple2RuleApplications.find(tuple);
      if (appsIt == DerivationManager::untypedTuple2RuleApplications.end() ||
          appsIt->second == nullptr || appsIt->second->empty()) {
        relationOk = false;
        break;
      }
      std::size_t witnessCount = 0;
      for (const auto &application : *appsIt->second) {
        const auto ruleId = static_cast<std::size_t>(application.ruleId);
        if (supportedRuleIds.count(ruleId) == 0) {
          relationOk = false;
          break;
        }
        ++witnessCount;
      }
      if (!relationOk) {
        break;
      }
      if (witnessCount > 1) {
        relationHadMultiWitness = true;
      }
      relationEdges += witnessCount;
    }

    if (!relationOk || !relationHadMultiWitness) {
      continue;
    }

    templateTsv << relationTemplateTsv.str();
    relationStatsTsv << relation << '\t' << supportedRules.size() << '\t'
                     << runtimeRelation->size() << '\t' << relationEdges
                     << '\t' << relationTemplateDdNodes
                     << "\t0\t0\t1\n";
    handledRelations.insert(relation);
    for (const Rule *rule : supportedRules) {
      handledRelations.insert(rule->getHead().getRelation());
      for (const auto &atom : rule->getBodyAtoms()) {
        handledRelations.insert(atom.getRelation());
      }
    }
    totalAbstractEdges += relationEdges;
    totalAbstractNodes += runtimeRelation->size();
    totalWitnessTemplates += supportedRules.size();
    totalWitnessRows += relationEdges;
    totalWitnessTemplateDdNodes += relationTemplateDdNodes;
    graphTsv << "internal_multi_witness\t" << relation
             << "\t*\t*\t" << relationEdges
             << "\tTemplateRef-only boundary relation\n";
  }

  graphDot << "}\n";
  if (result.handledOutputRelations.empty() && handledRelations.empty()) {
    return false;
  }
  result.handled = true;
  result.complete = result.handledOutputRelations.size() == totalOutputRelations;
  result.concreteOutputTuples = result.outputTuples - result.liftedOutputTuples;
  result.relationTemplates = handledRelations.size();
  result.handledRelations.assign(handledRelations.begin(), handledRelations.end());
  result.abstractEdges = totalAbstractEdges;
  result.abstractNodes = totalAbstractNodes;
  result.symbolicVariables = totalVariables;
  result.bddNodes = totalBddNodes;
  result.witnessIndexedTemplatesCount = totalWitnessTemplates;
  result.witnessIndexedRows = totalWitnessRows;
  result.witnessIndexedTupleFormulas = result.liftedOutputTuples;
  result.witnessIndexedTemplateDdNodes = totalWitnessTemplateDdNodes;
  result.executionMode = "witness_indexed_template";
  result.abstractGraph = graphTsv.str();
  result.abstractGraphDot = graphDot.str();
  result.witnessIndexedTemplates = templateTsv.str();
  result.witnessIndexedWitnesses = witnessTsv.str();
  result.witnessIndexedRelations = relationStatsTsv.str();
  result.witnessIndexedTupleStats = tupleStatsTsv.str();
  result.reason = result.complete ? "multi_witness_lifted"
                                  : "partial_multi_witness_lifted";
  return true;
}

// ---------------------------------------------------------------------------
// Post-hoc stratification round: topological depth of each derived tuple over
// the concrete derivation DAG.  round(t) = 0 for EDB/fact leaves; otherwise
// 1 + max round over its DERIVED body tuples, across all rule applications.
// Because the concrete tuple derivation graph is acyclic, every derived body
// tuple is guaranteed a strictly smaller round -- a sound stratification of a
// recursive SCC into layers that only depend on strictly-earlier layers.  This
// is the foundation for stratified lifting; it does not change WMC yet.
// ---------------------------------------------------------------------------
int computeDerivationRound(const UntypedTuple &tuple,
                           const RuleManager &ruleManager,
                           std::unordered_map<UntypedTuple, int> &memo,
                           std::unordered_set<UntypedTuple> &active) {
  auto cached = memo.find(tuple);
  if (cached != memo.end()) {
    return cached->second;
  }
  auto it = DerivationManager::untypedTuple2RuleApplications.find(tuple);
  if (it == DerivationManager::untypedTuple2RuleApplications.end() ||
      it->second == nullptr || it->second->empty()) {
    memo.emplace(tuple, 0);
    return 0;  // EDB / input fact leaf
  }
  if (!active.insert(tuple).second) {
    // The concrete tuple derivation graph is acyclic; guard defensively so a
    // malformed cycle cannot loop forever.
    return 0;
  }
  int best = 0;
  for (const auto &app : *it->second) {
    const Rule *rule =
        ruleManager.getRule(static_cast<std::size_t>(app.ruleId));
    if (rule == nullptr) {
      continue;
    }
    const auto ruleVars = rule->getVars();
    if (app.varValuesPure.size() != ruleVars.size()) {
      continue;
    }
    std::unordered_set<std::string> varSet(ruleVars.begin(), ruleVars.end());
    for (const auto &atom : rule->getBodyAtoms()) {
      if (atom.isNegatedAtom()) {
        continue;
      }
      bool resolvable = true;
      for (const auto &field : atom.getFields()) {
        if (std::holds_alternative<VariableField>(field.field) &&
            varSet.count(std::get<VariableField>(field.field).name) == 0) {
          resolvable = false;
          break;
        }
      }
      if (!resolvable) {
        continue;
      }
      std::vector<RamDomain> values;
      values.reserve(atom.getFields().size());
      for (const auto &field : atom.getFields()) {
        values.push_back(
            evaluateSymbolicField(field, ruleVars, app.varValuesPure));
      }
      UntypedTuple body{atom.getRelation(), std::move(values)};
      auto bodyIt =
          DerivationManager::untypedTuple2RuleApplications.find(body);
      if (bodyIt == DerivationManager::untypedTuple2RuleApplications.end() ||
          bodyIt->second == nullptr || bodyIt->second->empty()) {
        continue;  // EDB / det leaf contributes round 0
      }
      const int r =
          computeDerivationRound(body, ruleManager, memo, active) + 1;
      if (r > best) {
        best = r;
      }
    }
  }
  active.erase(tuple);
  memo.emplace(tuple, best);
  return best;
}

std::unordered_map<UntypedTuple, int> computeDerivationRounds(
    const RuleManager &ruleManager) {
  std::unordered_map<UntypedTuple, int> memo;
  std::unordered_set<UntypedTuple> active;
  for (const auto &entry : DerivationManager::untypedTuple2RuleApplications) {
    computeDerivationRound(entry.first, ruleManager, memo, active);
  }
  return memo;
}

// Gated by env var LIFTED_DUMP_ROUNDS: dump per-tuple rounds to a TSV and print
// a per-relation summary.  Non-invasive validation hook for the round layering.
void maybeDumpDerivationRounds(const RuleManager &ruleManager) {
  const char *path = std::getenv("LIFTED_DUMP_ROUNDS");
  if (path == nullptr) {
    return;
  }
  const auto rounds = computeDerivationRounds(ruleManager);
  std::map<std::string, std::pair<int, std::size_t>> relSummary;  // rel->(max,count)
  int maxRound = 0;
  std::ofstream out(path);
  out << "relation\tround\tfields\n";
  for (const auto &entry : rounds) {
    const UntypedTuple &t = entry.first;
    const int r = entry.second;
    out << t.relation_name << "\t" << r << "\t";
    for (std::size_t i = 0; i < t.fields.size(); ++i) {
      if (i > 0) {
        out << ",";
      }
      out << t.fields[i];
    }
    out << "\n";
    auto &s = relSummary[t.relation_name];
    s.first = std::max(s.first, r);
    s.second += 1;
    maxRound = std::max(maxRound, r);
  }
  std::cout << "[lifted-rounds] tuples=" << rounds.size()
            << " max_round=" << maxRound << " dump=" << path << "\n";
  for (const auto &entry : relSummary) {
    std::cout << "[lifted-rounds]   " << entry.first
              << " max_round=" << entry.second.first
              << " tuples=" << entry.second.second << "\n";
  }
}

} // namespace

LiftedWmcResult tryEvaluateLiftedPointwise(
    const CmdOptions &opt, SouffleProgram &program,
    const RuleManager &ruleManager,
    const std::unordered_map<UntypedTuple, double> &factProb,
    const std::vector<std::pair<UntypedTuple, bool>> &evidences) {
  LiftedWmcResult result;
  result.executionMode = "none";
  if (!opt.isLiftedWmcEnabled()) {
    result.reason = "disabled";
    return result;
  }
  if (opt.isDerivationOnly()) {
    result.reason = "derivation_only";
    return result;
  }
  if (opt.getKnowledgeRepresentation() != "bdd") {
    result.reason = "non_bdd_backend";
    return result;
  }
  if (!evidences.empty()) {
    result.reason = "evidence_present";
    return result;
  }

  const auto outputs = program.getOutputRelations();
  if (outputs.empty()) {
    result.reason = "no_output_relations";
    return result;
  }
  std::size_t outputRelationCount = 0;
  for (const auto *output : outputs) {
    if (output != nullptr) {
      ++outputRelationCount;
      result.outputTuples += output->size();
    }
  }

  LiftedWmcResult directResult = result;
  if (tryEvaluateDirectMultiWitnessOutputs(opt, program, ruleManager, factProb,
                                           directResult)) {
    if (!directResult.handledOutputRelations.empty() || directResult.complete) {
      return directResult;
    }
    return directResult;
  }
  result.rejectedOutputReasons.clear();

  maybeDumpDerivationRounds(ruleManager);

  std::unordered_set<std::size_t> activeRuleIds;
  for (const auto &[tuple, applications] :
       DerivationManager::untypedTuple2RuleApplications) {
    (void)tuple;
    if (applications == nullptr) {
      continue;
    }
    for (const auto &application : *applications) {
      activeRuleIds.insert(static_cast<std::size_t>(application.ruleId));
    }
  }

  std::unordered_set<std::string> inputRelations;
  for (const auto *input : program.getInputRelations()) {
    if (input != nullptr) {
      inputRelations.insert(input->getName());
    }
  }

  std::vector<Relation *> liftedOutputs;
  std::unordered_set<std::string> relationTemplates;
  for (auto *output : outputs) {
    if (output == nullptr) {
      continue;
    }
    const std::string relation = output->getName();
    if (output->size() < opt.getLiftedWmcThreshold()) {
      result.rejectedOutputReasons.emplace(relation, "below_runtime_threshold");
      continue;
    }

    PointwiseEligibility eligibility(program, ruleManager, activeRuleIds);
    if (!eligibility.analyze({output})) {
      result.rejectedOutputReasons.emplace(relation,
                                           eligibility.failureReason());
      continue;
    }
    liftedOutputs.push_back(output);
    result.handledOutputRelations.push_back(relation);
    result.liftedOutputTuples += output->size();
    relationTemplates.insert(eligibility.relations().begin(),
                             eligibility.relations().end());
  }
  result.concreteOutputTuples = result.outputTuples - result.liftedOutputTuples;
  if (liftedOutputs.empty()) {
    if (!result.rejectedOutputReasons.empty()) {
      result.reason = result.rejectedOutputReasons.begin()->second;
    } else {
      result.reason = "no_eligible_output_relations";
    }
    return result;
  }
  result.relationTemplates = relationTemplates.size();
  result.handledRelations.assign(relationTemplates.begin(),
                                 relationTemplates.end());
  std::sort(result.handledRelations.begin(), result.handledRelations.end());

  try {
    AbstractDerivationGraph graph(ruleManager, activeRuleIds,
                                  std::move(inputRelations));
    std::map<std::string, AbstractNodeId> outputNodes;
    for (const auto *output : liftedOutputs) {
      if (output != nullptr) {
        outputNodes.emplace(output->getName(), graph.buildOutput(*output));
      }
    }
    result.abstractNodes = graph.nodeCount();
    result.abstractEdges = graph.edgeCount();
    result.abstractGraph = graph.toTsv();
    result.abstractGraphDot = graph.toDot();

    SymbolicBdd bdd;
    ParameterizedForwardCompiler compiler(graph, bdd);
    std::map<std::string, BddId> outputRoots;
    for (const auto *output : liftedOutputs) {
      if (output != nullptr) {
        outputRoots.emplace(output->getName(), compiler.compile(outputNodes.at(
                                                   output->getName())));
      }
    }

    LiftedEvaluator evaluator(program, factProb, graph, bdd);
    for (const auto *output : liftedOutputs) {
      if (output == nullptr) {
        continue;
      }
      const BddId root = outputRoots.at(output->getName());
      for (auto it = output->begin(); it != output->end(); ++it) {
        const UntypedTuple tuple = UntypedTuple::fromSouffleTuple(*it);
        result.probabilities.emplace(tuple.toString(),
                                     evaluator.evaluate(root, tuple.fields));
      }
    }

    result.symbolicVariables = bdd.getVariables().size();
    result.bddNodes = bdd.nodeCount();
    result.handled = true;
    result.complete =
        result.handledOutputRelations.size() == outputRelationCount;
    result.executionMode = "pointwise_template";
    result.reason =
        result.complete ? "pointwise_lifted" : "partial_pointwise_lifted";
  } catch (const std::exception &error) {
    result.handled = false;
    result.complete = false;
    result.reason = std::string("lifted_runtime_fallback: ") + error.what();
    result.probabilities.clear();
    result.handledOutputRelations.clear();
    result.handledRelations.clear();
    result.liftedOutputTuples = 0;
    result.concreteOutputTuples = result.outputTuples;
  }
  return result;
}

void dumpLiftedProbabilities(const LiftedWmcResult &result,
                             const std::string &outputDir,
                             const std::string &fileName, bool dumpDot,
                             bool writeProbabilities) {
  if (writeProbabilities) {
    std::ofstream outputFile(joinOutputPath(outputDir, fileName + ".prob"));
    outputFile << std::setprecision(8);
    for (const auto &[tuple, probability] : result.probabilities) {
      outputFile << tuple << " : " << probability << '\n';
    }
  }
  if (!result.abstractGraph.empty()) {
    std::ofstream graphFile(
        joinOutputPath(outputDir, "abstract-derivation-graph.tsv"));
    graphFile << result.abstractGraph;
  }
  if (dumpDot && !result.abstractGraphDot.empty()) {
    std::ofstream graphFile(
        joinOutputPath(outputDir, "abstract-derivation-graph.dot"));
    graphFile << result.abstractGraphDot;
  }
  if (!result.witnessIndexedTemplates.empty()) {
    std::ofstream templateFile(
        joinOutputPath(outputDir, "witness-indexed-templates.tsv"));
    templateFile << result.witnessIndexedTemplates;
  }
  if (!result.witnessIndexedWitnesses.empty()) {
    std::ofstream witnessFile(
        joinOutputPath(outputDir, "witness-indexed-witnesses.tsv"));
    witnessFile << result.witnessIndexedWitnesses;
  }
  if (!result.witnessIndexedRelations.empty()) {
    std::ofstream relationFile(
        joinOutputPath(outputDir, "witness-indexed-relations.tsv"));
    relationFile << result.witnessIndexedRelations;
  }
  if (!result.witnessIndexedTupleStats.empty()) {
    std::ofstream tupleFile(
        joinOutputPath(outputDir, "witness-indexed-tuple-stats.tsv"));
    tupleFile << result.witnessIndexedTupleStats;
  }
}

} // namespace souffle::problog
