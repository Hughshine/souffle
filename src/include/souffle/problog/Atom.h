#ifndef ATOM_H
#define ATOM_H

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

#endif //ATOM_H
