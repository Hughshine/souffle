#ifndef RULEMANAGER_H
#define RULEMANAGER_H

#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <cassert>
#include <cstddef>
#include <iostream>
#include <variant>
#include <memory>
#include <sstream>
#include <map>
#include "souffle/RamTypes.h"
#include "souffle/problog/Atom.h"
#include "souffle/problog/Rule.h"
#include "souffle/Derivation.h"

class RuleManager {
public:
    RuleManager(std::vector<Rule> rules, std::vector<std::string> eqrelRelations = {});
    void addRule(Rule rule);
    const Rule* getRule(std::size_t ruleId) const;
    std::vector<const Rule*> getRulesForPredicate(const std::string& predicate) const;
    std::vector<const Rule*> getRulesDependingOn(const std::string& predicate) const;
    std::vector<const Rule*> getAllRules() const;
    bool removeRule(std::size_t ruleId);
    bool hasRule(std::size_t ruleId) const;
    std::size_t size() const;
    std::string toString() const;
    bool isRecursive(std::size_t ruleId) const {
        return getRule(ruleId)->isRecursive();
    }
    bool isInRecursiveStratum(std::size_t ruleId) const {
        return getRule(ruleId)->isInRecursiveStratum();
    }
    bool isEqrelRelation(const std::string& predicate) const {
        return eqrelRelations.count(predicate) != 0;
     }
    void addEqrelRelation(const std::string& predicate) {
        eqrelRelations.insert(predicate);
    }

private:
    std::unordered_map<std::size_t, Rule> rules;
    std::unordered_map<std::string, std::unordered_set<std::size_t>> predicateToRules;
    std::unordered_set<std::string> eqrelRelations;
};

class ExampleRuleComponents {
public:
    static const ExampleRuleComponents& getInstance();

    const SymbolicField var_x;
    const SymbolicField var_y;
    const SymbolicField var_z;

    const std::string edge_relation;
    const std::string path_relation;

    const Atom edge_xy;
    const Atom edge_zy;
    const Atom path_xy;
    const Atom path_xz;

    const Rule rule1;
    const Rule rule2;

    const RuleManager ruleManager;

    const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> exampleRuleApps;
    const std::unordered_map<UntypedTuple, double> fact_prob;
    const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> exampleDeltaInsertRuleApps;
    const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> exampleDeltaDeleteRuleApps;
    const std::unordered_map<UntypedTuple, double> fact_prob_inc;
    const std::vector<UntypedTuple> deletedFacts;
    const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> exampleRuleApps2;

private:
    ExampleRuleComponents();
    ~ExampleRuleComponents();
    ExampleRuleComponents(const ExampleRuleComponents&) = delete;
    ExampleRuleComponents& operator=(const ExampleRuleComponents&) = delete;
};


RuleManager::RuleManager(std::vector<Rule> rules, std::vector<std::string> eqrelRelations) {
    for (const auto& rel : eqrelRelations) {
        addEqrelRelation(rel);
    }
    for (const auto& rule : rules) {
        addRule(rule);
    }
}

void RuleManager::addRule(Rule rule) {
    auto ruleId = rule.getRuleId();
    const std::string& headPredicate = rule.getHead().getRelation();
    predicateToRules[headPredicate].insert(ruleId);
    rules.emplace(ruleId, std::move(rule));
}

const Rule* RuleManager::getRule(std::size_t ruleId) const {
    auto it = rules.find(ruleId);
    return it != rules.end() ? &(it->second) : nullptr;
}

std::vector<const Rule*> RuleManager::getRulesForPredicate(const std::string& predicate) const {
    std::vector<const Rule*> result;
    auto it = predicateToRules.find(predicate);
    if (it != predicateToRules.end()) {
        for (auto ruleId : it->second) {
            if (auto rule = getRule(ruleId)) {
                result.push_back(rule);
            }
        }
    }
    return result;
}

std::vector<const Rule*> RuleManager::getRulesDependingOn(const std::string& predicate) const {
    std::vector<const Rule*> result;
    for (const auto& [ruleId, rule] : rules) {
        for (const auto& bodyAtom : rule.getBodyAtoms()) {
            if (bodyAtom.getRelation() == predicate) {
                result.push_back(&rule);
                break;
            }
        }
    }
    return result;
}

std::vector<const Rule*> RuleManager::getAllRules() const {
    std::vector<const Rule*> result;
    result.reserve(rules.size());
    for (const auto& [_, rule] : rules) {
        result.push_back(&rule);
    }
    return result;
}

bool RuleManager::removeRule(std::size_t ruleId) {
    auto it = rules.find(ruleId);
    if (it == rules.end()) {
        return false;
    }
    const std::string& headPredicate = it->second.getHead().getRelation();
    auto& ruleSet = predicateToRules[headPredicate];
    ruleSet.erase(ruleId);
    if (ruleSet.empty()) {
        predicateToRules.erase(headPredicate);
    }
    rules.erase(it);
    return true;
}

bool RuleManager::hasRule(std::size_t ruleId) const {
    return rules.find(ruleId) != rules.end();
}

std::size_t RuleManager::size() const {
    return rules.size();
}

std::string RuleManager::toString() const {
    std::ostringstream oss;

    // Add header with total number of rules
    oss << "RuleManager contains " << rules.size() << " rules:\n";

    // Group rules by their head predicate for better organization
    std::map<std::string, std::vector<const Rule*>> rulesByPredicate;

    // First, organize all rules by their head predicate
    for (const auto& [ruleId, rule] : rules) {
        const std::string& predicate = rule.getHead().getRelation();
        rulesByPredicate[predicate].push_back(&rule);
    }

    // Now output rules grouped by predicate
    for (const auto& [predicate, predicateRules] : rulesByPredicate) {
        // Add a header for each predicate group
        oss << "\nRules defining '" << predicate << "':\n";

        // Output each rule in the group
        for (const Rule* rule : predicateRules) {
            oss << "  " << rule->toString() << "\n";
        }
    }

    return oss.str();
}


const ExampleRuleComponents& ExampleRuleComponents::getInstance() {
    static ExampleRuleComponents instance;
    return instance;
}

ExampleRuleComponents::ExampleRuleComponents()
    : var_x(SymbolicField::makeVariable("x")),
      var_y(SymbolicField::makeVariable("y")),
      var_z(SymbolicField::makeVariable("z")),
      edge_relation("edge"),
      path_relation("path"),
      edge_xy(edge_relation, {var_x, var_y}),
      edge_zy(edge_relation, {var_z, var_y}),
      path_xy(path_relation, {var_x, var_y}),
      path_xz(path_relation, {var_x, var_z}),
      rule1(1, path_xy, {edge_xy}),
      rule2(2, path_xy, {path_xz, edge_zy}, {"x", "y", "z"}, 1.0),
      ruleManager({rule1, rule2}),
      exampleRuleApps{
        {UntypedTuple{"path", {1, 2}}, new std::unordered_set<RuleApplication>{{1, {1, 2}}}},
        {UntypedTuple{"path", {2, 3}}, new std::unordered_set<RuleApplication>{{1, {2, 3}}}},
        {UntypedTuple{"path", {1, 3}}, new std::unordered_set<RuleApplication>{{2, {1, 2, 3}}}}
      },
      exampleDeltaInsertRuleApps{
        {UntypedTuple{"path", {3, 4}}, new std::unordered_set<RuleApplication>{{1, {3, 4}}}},
        {UntypedTuple{"path", {2, 4}}, new std::unordered_set<RuleApplication>{{2, {2, 4, 3}}}}
      },
      exampleDeltaDeleteRuleApps{
        {UntypedTuple{"path", {1, 2}}, new std::unordered_set<RuleApplication>{{1, {1, 2}}}},
        {UntypedTuple{"path", {1, 3}}, new std::unordered_set<RuleApplication>{{2, {1, 3, 2}}}}
      },
      fact_prob{
        {UntypedTuple{"edge", {1, 2}}, 0.9},
        {UntypedTuple{"edge", {2, 3}}, 0.8}
      },
      fact_prob_inc{
        {UntypedTuple{"edge", {3, 4}}, 0.7}
      },
      deletedFacts{
        UntypedTuple{"edge", {1, 2}}
      },
      exampleRuleApps2{
        {UntypedTuple{"path", {1, 2}}, new std::unordered_set<RuleApplication>{{1, {1, 2}}, {2, {1, 2, 3}}}},
        {UntypedTuple{"path", {2, 3}}, new std::unordered_set<RuleApplication>{{1, {2, 3}}, {2, {2, 3, 2}}}},
        {UntypedTuple{"path", {1, 3}}, new std::unordered_set<RuleApplication>{{2, {1, 3, 2}}}},
        {UntypedTuple{"path", {3, 2}}, new std::unordered_set<RuleApplication>{{1, {3, 2}}, {2, {3, 2, 3}}}},
        {UntypedTuple{"path", {2, 2}}, new std::unordered_set<RuleApplication>{{2, {2, 2, 3}}}},
        {UntypedTuple{"path", {3, 3}}, new std::unordered_set<RuleApplication>{{2, {3, 3, 2}}}}
      }
{
}

ExampleRuleComponents::~ExampleRuleComponents() {
    auto freeMap = [](const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& map) {
        for (const auto& [_, ptr] : map) {
            delete ptr;
        }
    };
    freeMap(exampleRuleApps);
    freeMap(exampleDeltaInsertRuleApps);
    freeMap(exampleDeltaDeleteRuleApps);
    freeMap(exampleRuleApps2);
}

static RuleManager ruleManager({});
#endif //RULEMANAGER_H
