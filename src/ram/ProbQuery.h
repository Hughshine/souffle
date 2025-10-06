#pragma once

#include "ram/Statement.h"
#include "souffle/profile/EventProcessor.h"
#include "ram/Relation.h"
#include <string>

namespace souffle::ram {

/**
 * @class ProbQuery
 * @brief Represents an query, e.g., query(a(1)).
 **/

class ProbQuery : public Statement {
public:
    ProbQuery(std::string rel)
        : Statement(NK_ProbQuery), relation(std::move(rel)) {}

    const std::string& getRelation() const { return relation; }

    ProbQuery* cloning() const override {
        return new ProbQuery(relation);
    }

    void apply(const NodeMapper&) override {};

    static bool classof(const Node* n) {
        return n->getKind() == NK_ProbQuery;
    }
protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos) << "ProbQuery " << relation << "\n";
    }

private:
    std::string relation;
};

}