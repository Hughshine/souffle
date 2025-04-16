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
#include "souffle/problog/Atom.h"

class Rule {
public:
    Rule(std::size_t ruleId, Atom head, std::vector<Atom> bodyAtoms = {}, std::vector<std::string> vars = {}, double probability = 1.0);

    const Atom& getHead() const;
    const std::vector<Atom>& getBodyAtoms() const;
    std::size_t getRuleId() const;
    double getProbability() const;
    bool isFact() const;
    std::string toString() const;
    std::vector<std::string> getVars() const {
        return vars;
    }
private:
    std::size_t ruleId;
    Atom head;
    std::vector<Atom> bodyAtoms;
    std::vector<std::string> vars;
    double probability;

    void addBodyAtom(Atom atom);
};


Rule::Rule(std::size_t ruleId, Atom head, std::vector<Atom> bodyAtoms, std::vector<std::string> vars, double probability)
    : ruleId(ruleId)
    , head(std::move(head))
    , bodyAtoms(std::move(bodyAtoms))
    , vars(std::move(vars))
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
    oss << ". <";
    for (const auto& var : vars) {
        oss << " " << var;
    }
    oss << " >";
    return oss.str();
}

#endif //RULE_H