#ifndef SOUFFLE_PROBLOG_EXACT_PROVENANCE_TEMPLATE_H
#define SOUFFLE_PROBLOG_EXACT_PROVENANCE_TEMPLATE_H

#include "souffle/Derivation.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <tuple>
#include <vector>

class Node;
class Hyperedge;

namespace souffle::problog {

/** Canonical tuple/event slots used by pointwise template instances. */
struct RootedTupleBinding {
  std::size_t canonicalNode = 0;
  UntypedTuple tuple;
};

struct RootedApplicationBinding {
  std::size_t canonicalNode = 0;
  RuleApplication application;
};

enum class ExactTemplateVariableKind {
  Fact,
  Rule
};

struct ExactTemplateVariable {
  ExactTemplateVariableKind kind = ExactTemplateVariableKind::Fact;
  std::size_t canonicalNode = 0;

  bool operator<(const ExactTemplateVariable &other) const {
    return std::tie(kind, canonicalNode) <
           std::tie(other.kind, other.canonicalNode);
  }
};

struct ExactTemplateBddNode {
  std::size_t variable = 0;
  std::uint32_t low = 0;
  std::uint32_t high = 0;
};

/** Immutable canonical Boolean structure shared by every isomorphic member. */
struct ExactTemplateDefinition {
  std::size_t templateId = 0;
  std::vector<ExactTemplateVariable> variables;
  // Node 0 is false and node 1 is true; all other nodes are BDD decisions.
  std::vector<ExactTemplateBddNode> nodes;
};

/** Canonical-to-ground substitution shared by all tuple roots of one member. */
struct ExactTemplateMemberBinding {
  std::shared_ptr<const ExactTemplateDefinition> definition;
  std::vector<RootedTupleBinding> tupleBindings;
  std::vector<RootedApplicationBinding> applicationBindings;
};

/** Pre-graph reference. Its event slots are bound after residual graph creation. */
struct PendingExactTemplateInstantiation {
  std::shared_ptr<const ExactTemplateMemberBinding> member;
  std::size_t canonicalTuple = 0;
  std::uint32_t root = 0;
  UntypedTuple tuple;
};

struct ExactTemplateBoundEvent {
  ExactTemplateVariableKind kind = ExactTemplateVariableKind::Fact;
  std::shared_ptr<Node> fact;
  std::shared_ptr<Hyperedge> rule;
};

struct ExactTemplateBoundMember {
  std::shared_ptr<const ExactTemplateMemberBinding> substitution;
  std::vector<ExactTemplateBoundEvent> events;
};

/** Runtime reference carried by a concrete dependency edge. */
struct ExactTemplateInstantiation {
  std::shared_ptr<const ExactTemplateBoundMember> member;
  std::size_t canonicalTuple = 0;
  std::uint32_t root = 0;
  UntypedTuple tuple;
};

struct ExactTemplateBindingStats {
  std::size_t dependencyEdges = 0;
  std::size_t references = 0;
  std::size_t memberBindings = 0;
  std::size_t factEvents = 0;
  std::size_t ruleEvents = 0;
};

} // namespace souffle::problog

#endif // SOUFFLE_PROBLOG_EXACT_PROVENANCE_TEMPLATE_H
