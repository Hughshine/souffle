/************************************************************************
 *
 * @file DeltaUnion.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Node.h"
#include "ram/Relation.h"
#include "ram/RelationStatement.h"
#include "souffle/utility/StreamUtil.h"
#include <memory>
#include <ostream>
#include <string>

namespace souffle::ram {

/**
 * @class DeltaUnion
 * @brief calculate new relation and real deltas from old relation and derivation deltas
 *
 * Rnew, delta_Rreal <= Rold, delta_Rder
 */
class DeltaUnion : public RelationStatement {
public:
    DeltaUnion(std::string rel)
            : RelationStatement(NK_DeltaUnion, rel) {}

    DeltaUnion* cloning() const override {
        return new DeltaUnion(relation);
    }

    static bool classof(const Node* n) {
        return n->getKind() == NK_DeltaUnion;
    }

protected:
    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "DELTA UNION " << relation << std::endl;
    };

    bool equal(const Node& node) const override {
        const auto& other = asAssert<DeltaUnion>(node);
        return RelationStatement::equal(other);
    }
};

}  // namespace souffle::ram
