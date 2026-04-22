//
// Created by Hugh on 2024/11/30.
//

/************************************************************************
 *
 * @file RecordDerivation.h
 *
 ***********************************************************************/

#pragma once

#include "ram/Expression.h"
#include "ram/Node.h"
#include "ram/Operation.h"
#include "ram/Relation.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StreamUtil.h"
#include <cassert>
#include <iosfwd>
#include <memory>
#include <ostream>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ram {

/**
 * @class RecordDerivation
 * @brief Record a tuple's derivation
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * FOR t0 IN A
 *   ...
 *     INSERT (t0.a, t0.b, t0.c) INTO @new_X
 *     Record (t0.a, t0.b, t0.c) DERIVED BY <ruleId> <mapping>, <INSERT|DELETE>, <DELTA|COMPLETE>
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class RecordDerivation : public Operation {
public:

    RecordDerivation(std::string rel, VecOwn<Expression> expressions)
            : RecordDerivation(std::move(rel),
                std::move(expressions), -1, "UNKNOWN CLAUSE", {}, {}) {}

    RecordDerivation(std::string rel, VecOwn<Expression> expressions,
            std::size_t clauseID, std::string clauseStr)
            : RecordDerivation(std::move(rel), std::move(expressions)
                , clauseID, std::move(clauseStr), {}, {}) {}

    RecordDerivation(std::string rel, VecOwn<Expression> expressions,
            std::size_t clauseID, std::string clauseStr, std::map<std::string, Own<Expression>>&& varExprMap,
            std::vector<Own<ram::Expression>>&& varExprs)
            : Operation(NK_RecordDerivation), relation(std::move(rel)), expressions(std::move(expressions)),
              clauseID(clauseID), clauseStr(std::move(clauseStr)), varExprMap(std::move(varExprMap)),
              varExprs(std::move(varExprs)) {
        assert(allValidPtrs(expressions));
    }


    /** @brief Get relation */
    const std::string& getRelation() const {
        return relation;
    }

    /** @brief Get expressions */
    std::vector<Expression*> getValues() const {
        return toPtrVector(expressions);
    }

    std::size_t getClauseID() const {
        return clauseID;
    }

    RecordDerivation* cloning() const override {
        VecOwn<Expression> newValues;
        for (auto& expr : expressions) {
            newValues.emplace_back(expr->cloning());
        }
        std::map<std::string, Own<ram::Expression>> newVarExprMap{};
        for (auto& [var, expr] : varExprMap) {
            newVarExprMap.emplace(var, expr->cloning());
        }
        std::vector<Own<ram::Expression>> newVarExprs{};
        for (auto& expr : varExprs) {
            newVarExprs.emplace_back(expr->cloning());
        }
        return new RecordDerivation(
                relation, std::move(newValues), clauseID, clauseStr, std::move(newVarExprMap), std::move(newVarExprs));
    }

    void apply(const NodeMapper& map) override {
        for (auto& expr : expressions) {
            expr = map(std::move(expr));
        }
        for (auto& [var, expr] : varExprMap) {
            expr = map(std::move(expr));
        }
        for (auto& expr : varExprs) {
            expr = map(std::move(expr));
        }
    }

    static bool classof(const Node* n) {
        const NodeKind kind = n->getKind();
        return (kind == NK_RecordDerivation);
    }

    void print(std::ostream& os, int tabpos) const override {
        os << times(" ", tabpos);
        os << "RECORD TUPLE " << relation << "(" << join(expressions, ", ", print_deref<Own<Expression>>())
            << ") DERIVE BY RULE (" << std::to_string(clauseID) << ") " << clauseStr
            << "WITH MAPPING " << "<placeholder>" << std::endl;
        for (auto& [var, expr] : varExprMap) {
            os << times(" ", tabpos + 2) << var << ": " << *expr << std::endl;
        }
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<RecordDerivation>(node);
        return relation == other.relation && equal_targets(expressions, other.expressions) &&
               clauseID == other.clauseID && clauseStr == other.clauseStr;
        // TODO: equal mapping?
    }

    NodeVec getChildren() const override {
        NodeVec nodes;
        for (auto& [_, v] : varExprMap) {
            nodes.push_back(v.get());
        }
        auto vec = toPtrVector<Node const>(expressions);
        nodes.insert(nodes.end(), vec.begin(), vec.end());
        return nodes;
    }

    /** Relation name */
    std::string relation;

    /* Arguments of insert operation */
    VecOwn<Expression> expressions;

    const std::size_t clauseID;
    const std::string clauseStr;
    std::map<std::string, Own<ram::Expression>> varExprMap;  // TODO
    std::vector<Own<ram::Expression>> varExprs; // TODO: remove this
};

}  // namespace souffle::ram
