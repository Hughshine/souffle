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
static RuleManager ruleManager({});
#endif //RULEMANAGER_H
