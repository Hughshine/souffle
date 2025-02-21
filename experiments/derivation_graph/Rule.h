#ifndef RULE_H
#define RULE_H

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

class Tuple;
class Atom;
class Rule;


struct IntegerField {
    int value;
};

struct FloatField {
    double value;
};

struct StringField {
    std::string value;
};

struct VariableField {
    std::string name;
};

struct SymbolicField {
    std::variant<IntegerField,
                 FloatField,
                 StringField,
                 VariableField> field;

    explicit SymbolicField(int value) : field(IntegerField{value}) {}
    explicit SymbolicField(double value) : field(FloatField{value}) {}
    explicit SymbolicField(const StringField& field) : field(field) {}
    explicit SymbolicField(const VariableField& field) : field(field) {}

    static SymbolicField makeVariable(const std::string& varname) {
        SymbolicField result{VariableField{varname}};
        return result;
    }

    std::string toString() const {
        if (std::holds_alternative<IntegerField>(field)) {
            return std::to_string(std::get<IntegerField>(field).value);
        } else if (std::holds_alternative<FloatField>(field)) {
            return std::to_string(std::get<FloatField>(field).value);
        } else if (std::holds_alternative<StringField>(field)) {
            return '"' + std::get<StringField>(field).value + '"';
        } else if (std::holds_alternative<VariableField>(field)) {
            return std::get<VariableField>(field).name;
        }
        assert (false && "Unknown field type");
    }
};



class Atom {
public:
    Atom(std::string relation, std::vector<SymbolicField> fields, bool isNegated = false)
        : relation(std::move(relation)), fields(std::move(fields)), isNegated(isNegated) {}

    Atom(const Atom& other)
        : relation(other.relation), fields(other.fields), isNegated(other.isNegated) {}

    Atom(Atom&& other) noexcept
        : relation(std::move(other.relation))
        , fields(std::move(other.fields))
        , isNegated(other.isNegated) {}

    std::string toString() const {
        std::string result = isNegated ? "!" : "";
        result += relation + "(";

        for (size_t i = 0; i < fields.size(); ++i) {
            if (i > 0) result += ", ";
            result += fields[i].toString();
        }
        result += ")";
        return result;
    }

    const std::string& getRelation() const {
        return relation;
    }

    const std::vector<SymbolicField>& getFields() const {
        return fields;
    }

    bool isNegatedAtom() const {
        return isNegated;
    }

    std::vector<std::string> getVars() {
        std::vector<std::string> vars;
        for (const auto& field : fields) {
            if (std::holds_alternative<VariableField>(field.field)) {
                vars.push_back(std::get<VariableField>(field.field).name);
            }
        }
        return vars;
    }

    std::vector<souffle::RamDomain> instantiatedFields(const std::map<std::string, int>& varValues) const {
        std::vector<souffle::RamDomain> result;  // TODO
        for (const auto& field : fields) {
            if (std::holds_alternative<VariableField>(field.field)) {
                const std::string& varName = std::get<VariableField>(field.field).name;
                if (varValues.find(varName) != varValues.end()) {
                    result.push_back(varValues.at(varName));
                } else {
					assert(false && "Variable not found in map");
                }
        } else if (std::holds_alternative<IntegerField>(field.field)) {
	            result.push_back(std::get<IntegerField>(field.field).value);
            }
        }
        return result;
    }
private:
    std::string relation;
    std::vector<SymbolicField> fields;
    bool isNegated;
};


class Rule {
public:
    Rule(std::size_t ruleId, Atom head, std::vector<Atom> bodyAtoms = {}, double probability = 1.0);

    const Atom& getHead() const;
    const std::vector<Atom>& getBodyAtoms() const;
    std::size_t getRuleId() const;
    double getProbability() const;
    bool isFact() const;
    std::string toString() const;

private:
    std::size_t ruleId;
    Atom head;
    std::vector<Atom> bodyAtoms;
    double probability;

    void addBodyAtom(Atom atom);
};


//Rule exampleRule1
extern const Rule rule1;
extern const Rule rule2;

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
};

//class ExampleRuleComponents {
//    static SymbolicField var_x = SymbolicField::makeVariable("x");
//    SymbolicField var_y = SymbolicField::makeVariable("y");
//    SymbolicField var_z = SymbolicField::makeVariable("z");
//
//    std::string edge_relation = "edge";
//    std::string path_relation = "path";
//
//    Atom edge_xy = Atom{edge_relation, {var_x, var_y}};
//    Atom edge_zy = Atom{edge_relation, {var_z, var_y}};
//    Atom path_xy = Atom{path_relation, {var_x, var_y}};
//    Atom path_xz = Atom{path_relation, {var_x, var_z}};
//
//    Rule rule1 = Rule(1, path_xy, {edge_xy});
//    Rule rule2 = Rule(2, path_xy, {path_xz, edge_zy});
//
//    RuleManager ruleManager = RuleManager({rule1, rule2});
//};

#endif //RULE_H
