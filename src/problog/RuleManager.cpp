#include "souffle/problog/RuleManager.h"

RuleManager::RuleManager(std::vector<Rule> rules, std::vector<std::string> eqrelRelations) {
    for (const auto& rel : eqrelRelations) {
        addEqrelRelation(rel);
    }
    for (auto& rule : rules) {
        addRule(std::move(rule));
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

    oss << "RuleManager contains " << rules.size() << " rules:\n";

    std::map<std::string, std::vector<const Rule*>> rulesByPredicate;
    for (const auto& [ruleId, rule] : rules) {
        const std::string& predicate = rule.getHead().getRelation();
        rulesByPredicate[predicate].push_back(&rule);
    }

    for (const auto& [predicate, predicateRules] : rulesByPredicate) {
        oss << "\nRules defining '" << predicate << "':\n";
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
                  {UntypedTuple{"path", {1, 3}}, new std::unordered_set<RuleApplication>{{2, {1, 2, 3}}}},
          },
          exampleDeltaInsertRuleApps{
                  {UntypedTuple{"path", {3, 4}}, new std::unordered_set<RuleApplication>{{1, {3, 4}}}},
                  {UntypedTuple{"path", {2, 4}}, new std::unordered_set<RuleApplication>{{2, {2, 4, 3}}}},
          },
          exampleDeltaDeleteRuleApps{
                  {UntypedTuple{"path", {1, 2}}, new std::unordered_set<RuleApplication>{{1, {1, 2}}}},
                  {UntypedTuple{"path", {1, 3}}, new std::unordered_set<RuleApplication>{{2, {1, 3, 2}}}},
          },
          fact_prob{
                  {UntypedTuple{"edge", {1, 2}}, 0.9},
                  {UntypedTuple{"edge", {2, 3}}, 0.8},
          },
          fact_prob_inc{
                  {UntypedTuple{"edge", {3, 4}}, 0.7},
          },
          deletedFacts{
                  UntypedTuple{"edge", {1, 2}},
          },
          exampleRuleApps2{
                  {UntypedTuple{"path", {1, 2}}, new std::unordered_set<RuleApplication>{{1, {1, 2}}, {2, {1, 2, 3}}}},
                  {UntypedTuple{"path", {2, 3}}, new std::unordered_set<RuleApplication>{{1, {2, 3}}, {2, {2, 3, 2}}}},
                  {UntypedTuple{"path", {1, 3}}, new std::unordered_set<RuleApplication>{{2, {1, 3, 2}}}},
                  {UntypedTuple{"path", {3, 2}}, new std::unordered_set<RuleApplication>{{1, {3, 2}}, {2, {3, 2, 3}}}},
                  {UntypedTuple{"path", {2, 2}}, new std::unordered_set<RuleApplication>{{2, {2, 2, 3}}}},
                  {UntypedTuple{"path", {3, 3}}, new std::unordered_set<RuleApplication>{{2, {3, 3, 2}}}},
          } {}

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
