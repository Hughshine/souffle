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
    RuleManager(std::vector<Rule> rules);
    void addRule(Rule rule);
    const Rule* getRule(std::size_t ruleId) const;
    std::vector<const Rule*> getRulesForPredicate(const std::string& predicate) const;
    std::vector<const Rule*> getRulesDependingOn(const std::string& predicate) const;
    std::vector<const Rule*> getAllRules() const;
    bool removeRule(std::size_t ruleId);
    bool hasRule(std::size_t ruleId) const;
    std::size_t size() const;
    std::string toString() const;

private:
    std::unordered_map<std::size_t, Rule> rules;
    std::unordered_map<std::string, std::unordered_set<std::size_t>> predicateToRules;
};

class ExampleRuleComponents {
public:
    // Declare static members
    static const SymbolicField var_x;
    static const SymbolicField var_y;
    static const SymbolicField var_z;

    static const std::string edge_relation;
    static const std::string path_relation;

    static const Atom edge_xy;
    static const Atom edge_zy;
    static const Atom path_xy;
    static const Atom path_xz;

    static const Rule rule1;
    static const Rule rule2;

    static const RuleManager ruleManager;

    static const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> exampleRuleApps;
    static const std::unordered_map<UntypedTuple, double> fact_prob;
    static const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> exampleDeltaInsertRuleApps;
    static const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> exampleDeltaDeleteRuleApps;
    static const std::unordered_map<UntypedTuple, double> fact_prob_inc;
    static const std::vector<UntypedTuple> deletedFacts;

    static const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> exampleRuleApps2;
};

RuleManager::RuleManager(std::vector<Rule> rules) {
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


const SymbolicField ExampleRuleComponents::var_x = SymbolicField::makeVariable("x");
const SymbolicField ExampleRuleComponents::var_y = SymbolicField::makeVariable("y");
const SymbolicField ExampleRuleComponents::var_z = SymbolicField::makeVariable("z");

const std::string ExampleRuleComponents::edge_relation = "edge";
const std::string ExampleRuleComponents::path_relation = "path";

// We need to use the class scope resolution operator (::) for each definition
const Atom ExampleRuleComponents::edge_xy = Atom{
    ExampleRuleComponents::edge_relation,
    {ExampleRuleComponents::var_x, ExampleRuleComponents::var_y}
};

const Atom ExampleRuleComponents::edge_zy = Atom{
    ExampleRuleComponents::edge_relation,
    {ExampleRuleComponents::var_z, ExampleRuleComponents::var_y}
};

const Atom ExampleRuleComponents::path_xy = Atom{
    ExampleRuleComponents::path_relation,
    {ExampleRuleComponents::var_x, ExampleRuleComponents::var_y}
};

const Atom ExampleRuleComponents::path_xz = Atom{
    ExampleRuleComponents::path_relation,
    {ExampleRuleComponents::var_x, ExampleRuleComponents::var_z}
};

const Rule ExampleRuleComponents::rule1 = Rule(
    1,
    ExampleRuleComponents::path_xy,
    {ExampleRuleComponents::edge_xy}
);

const Rule ExampleRuleComponents::rule2 = Rule(
    2,
    ExampleRuleComponents::path_xy,
    {ExampleRuleComponents::path_xz, ExampleRuleComponents::edge_zy},
    {"x", "y", "z"},
    1.0
);

const RuleManager ExampleRuleComponents::ruleManager = RuleManager({rule1, rule2});

const UntypedTuple tuple1{"edge", {1, 2}};
const UntypedTuple tuple2{"edge", {2, 3}};
const UntypedTuple tuple3{"path", {1, 2}};
const UntypedTuple tuple4{"path", {2, 3}};
const UntypedTuple tuple5{"path", {1, 3}};

const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> ExampleRuleComponents::exampleRuleApps{
            {tuple3, new std::unordered_set<RuleApplication>{
                RuleApplication{1, {1, 2}} // {{"x", 1}, {"y", 2}}
            }},
            {tuple4, new std::unordered_set<RuleApplication>{
                RuleApplication{1, {2, 3} } // {{"x", 2}, {"y", 3}}
            }},
            {tuple5, new std::unordered_set<RuleApplication>{
                RuleApplication{2, {1, 2, 3}} // {{"x", 1}, {"y", 3}, {"z", 2}}
            }}
};

// for incremental changes test
const UntypedTuple tupleInsert1{"edge", {3, 4}};
const UntypedTuple tupleDelete1{"edge", {1, 2}};

const UntypedTuple tupleInsert2{"path", {3, 4}}; // 由 rule1 派生: path(3, 4) :- edge(3, 4)
const UntypedTuple tupleInsert3{"path", {2, 4}}; // 由 rule2 派生: path(2, 4) :- path(2, 3), edge(3, 4)

const UntypedTuple tupleDelete2{"path", {1, 2}}; // 删除 path(1, 2)，它依赖于 edge(1, 2)
const UntypedTuple tupleDelete3{"path", {1, 3}}; // 删除 path(1, 3)，它依赖于 path(1, 2) 和 edge(2, 3)

const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> ExampleRuleComponents::exampleDeltaInsertRuleApps{
    // 对于 path(3, 4)，应用规则 1: path(x,y) :- edge(x,y)
        {tupleInsert2, new std::unordered_set<RuleApplication>{
            RuleApplication{1, {3, 4}} // {{"x", 3}, {"y", 4}}
        }},
    // 对于 path(2, 4)，应用规则 2: path(x,y) :- path(x,z), edge(z,y)
        {tupleInsert3, new std::unordered_set<RuleApplication>{
            RuleApplication{2, {2, 4, 3}}  // {{"x", 2}, {"y", 4}, {"z", 3}}
        }}
};

// 定义增量删除的规则应用
const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> ExampleRuleComponents::exampleDeltaDeleteRuleApps{
    // 对于 path(1, 2)，删除通过规则 1 生成的规则应用
        {tupleDelete2, new std::unordered_set<RuleApplication>{
            RuleApplication{1, {1, 2}}  // {{"x", 1}, {"y", 2}}
        }},
        // 对于 path(1, 3)，删除通过规则 2 生成的规则应用
        {tupleDelete3, new std::unordered_set<RuleApplication>{
            RuleApplication{2, {1, 3, 2}}  // {{"x", 1}, {"y", 3}, {"z", 2}}
        }}
};

const std::vector<UntypedTuple> ExampleRuleComponents::deletedFacts{
    tupleDelete1
};

// 初始事实的概率
const std::unordered_map<UntypedTuple, double> ExampleRuleComponents::fact_prob{
        {tuple1, 0.9},  // edge(1, 2) 概率为 0.9
        {tuple2, 0.8}   // edge(2, 3) 概率为 0.8
};

// 增量插入的事实概率
const std::unordered_map<UntypedTuple, double> ExampleRuleComponents::fact_prob_inc{
        {tupleInsert1, 0.7}  // 新增 edge(3, 4) 概率为 0.7
};


const UntypedTuple tuple6{"edge", {3, 2}};
const UntypedTuple tuple7{"path", {3, 2}};
const UntypedTuple tuple8{"path", {2, 2}};
const UntypedTuple tuple9{"path", {3, 3}};
const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*> ExampleRuleComponents::exampleRuleApps2{
                {tuple3, new std::unordered_set<RuleApplication>{  // path(1,2)	[1[x->1,y->2],2[x->1,y->2,z->3]]
                    RuleApplication{1, {1, 2}},  // {{"x", 1}, {"y", 2}}
                    RuleApplication{2, {1, 2, 3}}  // {{"x", 1}, {"y", 2}, {"z", 3}}
                }},
                {tuple4, new std::unordered_set<RuleApplication>{  // path(2,3)	[1[x->2,y->3],2[x->2,y->3,z->2]]
                    RuleApplication{1, {2, 3}},  // {{"x", 2}, {"y", 3}}
                    RuleApplication{2, {2, 3 ,2}}  // {{"x", 2}, {"y", 3}, {"z", 2}}
                }},
                {tuple5, new std::unordered_set<RuleApplication>{  //path(1,3)	[2[x->1,y->3,z->2]]
                    RuleApplication{2, {1, 3, 2}} // {{"x", 1}, {"y", 3}, {"z", 2}}
                }},
                {tuple7, new std::unordered_set<RuleApplication>{  // path(3,2)	[1[x->3,y->2],2[x->3,y->2,z->3]]
                    RuleApplication{1, {3, 2}}, // {{"x", 3}, {"y", 2}}
                    RuleApplication{2, {3, 2, 3}}  // {{"x", 3}, {"y", 2}, {"z", 3}}
                }},
                {tuple8, new std::unordered_set<RuleApplication>{  // path(2,2)   [2[x->2,y->2,z->3]]
                    RuleApplication{2, {2, 2, 3}}  // {{"x", 2}, {"y", 2}, {"z", 3}}
                }},
                {tuple9, new std::unordered_set<RuleApplication>{  // path(3,3)   [2[x->3,y->3,z->2]]
                    RuleApplication{2, {3, 3, 2}}  // {{"x", 3}, {"y", 3}, {"z", 2}}
                }}
};

#endif //RULEMANAGER_H
