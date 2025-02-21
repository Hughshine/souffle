#include "Rule.h"

Rule::Rule(std::size_t ruleId, Atom head, std::vector<Atom> bodyAtoms, double probability)
    : ruleId(ruleId)
    , head(std::move(head))
    , bodyAtoms(std::move(bodyAtoms))
    , probability(probability) {
}
const Atom& Rule::getHead() const {
    return head;
}

const std::vector<Atom>& Rule::getBodyAtoms() const {
    return bodyAtoms;
}

std::size_t Rule::getRuleId() const {
    return ruleId;
}

double Rule::getProbability() const {
    return probability;
}

bool Rule::isFact() const {
    return bodyAtoms.empty();
}

void Rule::addBodyAtom(Atom atom) {
    bodyAtoms.push_back(std::move(atom));
}

std::string Rule::toString() const {
    std::ostringstream oss;
    oss << "[Rule " << ruleId;
    if (probability != 1.0) {
        oss << ", prob=" << probability;
    }
    oss << "] " << head.toString();

    if (!bodyAtoms.empty()) {
        oss << " :- ";
        for (size_t i = 0; i < bodyAtoms.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << bodyAtoms[i].toString();
        }
    }
    oss << ".";
    return oss.str();
}

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
    {ExampleRuleComponents::path_xz, ExampleRuleComponents::edge_zy}
);

const RuleManager ExampleRuleComponents::ruleManager = RuleManager({rule1, rule2});
