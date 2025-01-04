/************************************************************************
 *
 * @file DerivationCheck.h
 *
 * Defines a condition that check whether a specific derivation of a tuple
 * is already derived in the Relational Algebra Machine.
 * For probabilistic setting.
 *
 ***********************************************************************/

#pragma once

#include "Expression.h"
#include "ram/Condition.h"
#include "ram/Node.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/MiscUtil.h"
#include <cassert>
#include <memory>
#include <sstream>
#include <utility>
#include <vector>

#include <souffle/utility/StreamUtil.h>

namespace souffle::ram {

/**
 * @class DerivationCheck
 * @brief Check the existence of a derivation
 *
 * For example:
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * (Derived t0 in A with delete/insert ruleapp)
 * will be an existence check plus a derivation check
 * may need further optimization
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */
class DerivationCheck : public Condition {
public:
    DerivationCheck() : Condition(NK_DerivationCheck) {
        // assert(operand != nullptr && "operand of negation is a null-pointer");
        assert( false && "not impl");
    }

    /** @brief Get operand of negation */
    // const Condition& getOperand() const {
    //     return *operand;
    // }

    DerivationCheck* cloning() const override {
        return new DerivationCheck();
    }

    // TODO: when do we call this function???
    void apply(const NodeMapper& map) override {
        assert(false && "did not expect this");
        for (auto& expr : expressions) {
            expr = map(std::move(expr));
        }
    }

    static bool classof(const Node* n) {
        return n->getKind() == NK_DerivationCheck;
    }

// protected:

    DerivationCheck(std::string rel, VecOwn<Expression> expressions, std::size_t clauseID, bool isDelete, bool isComplete, std::map<std::string, Own<ram::Expression>>&& varExprMap) //
        : Condition(NK_RecordDerivation), rel(std::move(rel)), expressions(std::move(expressions)),
        varExprMap(std::move(varExprMap)),
        isComplete(isComplete), isDelete(isDelete),
        clauseID(clauseID)
    {
        assert(allValidPtrs(expressions));
        // TODO
    }

    void print(std::ostream& os) const override {
        os << "(IF "
        << join(expressions, ", ", print_deref<Own<Expression>>())
        << " DERIVED BY RULE" << std::to_string(clauseID)
        << " OF REL " << rel
        << ", WITH MAPPING " << "<placeholder>" << ","
        << (!isDelete?"INSERT":"DELETE") << ", "
        << (isComplete?"COMPLETE":"DELTA") << ", "
        << ")";
    }

    bool equal(const Node& node) const override {
        const auto& other = asAssert<DerivationCheck>(node);
        return rel == other.rel
        && equal_targets(expressions, other.expressions)
        && clauseID == other.clauseID;
        // TODO: equal mapping?
    }

    NodeVec getChildren() const override {
        return toPtrVector<Node const>(expressions); // TODO: varExprMap?
    }

    std::string rel;
    VecOwn<ram::Expression> expressions;
    std::map<std::string, Own<ram::Expression>> varExprMap;  // TODO

    bool isComplete;
    bool isDelete;
    size_t clauseID;
};

}  // namespace souffle::ram
