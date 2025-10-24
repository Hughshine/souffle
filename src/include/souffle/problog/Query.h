#ifndef QUERY_H
#define QUERY_H

#include "Atom.h"
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

class Query {
public:
    Query(Atom atom)
        : queryAtom(std::move(atom)) {}

    std::string toString() const {
        std::ostringstream ss;
        ss << "query(" << queryAtom.toString() << ")";
        return ss.str();
    }

    bool hasWildcard() const {
        for (const auto& f : queryAtom.getFields()) {
            if (std::holds_alternative<VariableField>(f.field)) {
                const auto& var = std::get<VariableField>(f.field);
                if (var.name == "_") return true;
            }
        }
        return false;
    }

    const std::string& getRelationName() const {
        return queryAtom.getRelation();
    }

    std::vector<std::string> getBoundVariables() const {
        std::vector<std::string> vars;
        for (const auto& f : queryAtom.getFields()) {
            if (std::holds_alternative<VariableField>(f.field)) {
                const auto& var = std::get<VariableField>(f.field);
                vars.push_back(var.name);
            } else if (std::holds_alternative<IntegerField>(f.field)) {
                vars.push_back(std::to_string(std::get<IntegerField>(f.field).value));
            } else if (std::holds_alternative<FloatField>(f.field)) {
                vars.push_back(std::to_string(std::get<FloatField>(f.field).value));
            } else if (std::holds_alternative<StringField>(f.field)) {
                vars.push_back(std::get<StringField>(f.field).value);
            }
        }
        return vars;
    }

private:
    Atom queryAtom;
};

#endif
