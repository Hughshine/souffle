/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2020 The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file Utils.cpp
 *
 * A collection of utilities used in translation
 *
 ***********************************************************************/

#include "ast2ram/utility/Utils.h"
#include "ast/Atom.h"
#include "ast/Clause.h"
#include "ast/QualifiedName.h"
#include "ast/Relation.h"
#include "ast/SubsumptiveClause.h"
#include "ast/UnnamedVariable.h"
#include "ast/Variable.h"
#include "ast/utility/Utils.h"
#include "ast2ram/ClauseTranslator.h"
#include "ast2ram/utility/Location.h"
#include "ram/Clear.h"
#include "ram/Condition.h"
#include "ram/Conjunction.h"
#include "ram/TupleElement.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/StringUtil.h"
#include <string>
#include <vector>

namespace souffle::ast2ram {

std::string getAtomName(const ast::Clause& clause, const ast::Atom* atom,
        const std::vector<ast::Atom*>& sccAtoms, std::size_t version, bool isRecursive,
        TranslationMode mode, bool isIncremental) {
    if (isA<ast::SubsumptiveClause>(clause)) {
        assert(false && "subsumptive clause not supported");
    }

    if (!isRecursive) {
        if (mode == Auxiliary && clause.getHead() == atom) {
            assert (false && "auxiliary mode not supported");
            return getNewRelationName(atom->getQualifiedName());
        }
        return getConcreteRelationName(atom->getQualifiedName());
    }
    if (!isIncremental) {
        if (clause.getHead() == atom) {
            return getNewRelationName(atom->getQualifiedName());
        }
        if (sccAtoms.at(version) == atom) {
            return getDeltaRelationName(atom->getQualifiedName());
        }
        return getConcreteRelationName(atom->getQualifiedName());
    } else {
        assert (false && "INC: recursive clause not supported");
    }
}

std::string getConcreteRelationName(const ast::QualifiedName& name, const std::string prefix) {
    return prefix + getRelationName(name);
}

std::string getOldRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@old_");
}

std::string getDeltaRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@delta_");
}

std::string getNewRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@new_");
}

/**
 * For inc + recursion
 */
std::string getDeltaDeletionRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@delta_tuple_delete");
}

std::string getDeltaInsertionRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@delta_tuple_insert");
}

std::string getNewDeletionRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@new_derv_delete");
}

std::string getNewInsertionRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@new_derv_insert");
}

std::string getLubRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@lub_");
}

std::string getRejectRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@reject_");
}

std::string getDeleteRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@delete_");
}

// std::string getIncDeltaRelationName(const ast::QualifiedName& name) {
//     return getConcreteRelationName(name, "@inc_delta_");
// }

std::string getIncDeltaDervInsertRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@inc_delta_derv_insert_");
}

std::string getIncDeltaDervDeleteRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@inc_delta_derv_delete_");
}

std::string getIncDeltaTupleInsertRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@inc_delta_tuple_insert_");
}

std::string getIncDeltaTupleDeleteRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@inc_delta_tuple_delete_");
}

std::string getTmpRelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@tmp_");
}

std::string getTmp2RelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@tmp2_");
}

std::string getTmp3RelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@tmp3_");
}

std::string getTmp4RelationName(const ast::QualifiedName& name) {
    return getConcreteRelationName(name, "@tmp4_");
}
const std::string& getRelationName(const ast::QualifiedName& name) {
    return name.toString();
}

std::string getBaseRelationName(const ast::QualifiedName& name) {
    return
    stripPrefix("@inc_delta_tuple_delete_",
    stripPrefix("@inc_delta_tuple_insert_",
    stripPrefix("@inc_delta_derv_delete_",
    stripPrefix("@inc_delta_derv_insert_",
    stripPrefix("@tmp4_",
        stripPrefix("@tmp3_",
        stripPrefix("@tmp2_",
            stripPrefix("@tmp_",
        stripPrefix("@old_",
            stripPrefix("@inc_delta_tuple_delete_",
                stripPrefix("@inc_delta_tuple_insert_",
                    stripPrefix("@inc_delta_derv_delete_",
                        stripPrefix("@inc_delta_derv_insert_",
                            stripPrefix("@new_",
                                stripPrefix("@delta_",
                                    stripPrefix("@info_", name.toString()))))))))))))))));
}

void appendStmt(VecOwn<ram::Statement>& stmtList, Own<ram::Statement> stmt) {
    if (stmt) {
        stmtList.push_back(std::move(stmt));
    }
}

void nameUnnamedVariables(ast::Clause* clause) {
    // the node mapper conducting the actual renaming
    struct Instantiator : public ast::NodeMapper {
        mutable int counter = 0;

        Instantiator() = default;

        Own<ast::Node> operator()(Own<ast::Node> node) const override {
            // apply recursive
            node->apply(*this);

            // replace unknown variables
            if (isA<ast::UnnamedVariable>(node)) {
                auto name = " _unnamed_var" + toString(++counter);
                return mk<ast::Variable>(name);
            }

            // otherwise nothing
            return node;
        }
    };

    // name all variables in the atoms
    Instantiator init;
    for (auto& atom : ast::getBodyLiterals<ast::Atom>(*clause)) {
        atom->apply(init);
    }
}

Own<ram::TupleElement> makeRamTupleElement(const Location& loc) {
    return mk<ram::TupleElement>(loc.identifier, loc.element);
}

Own<ram::Condition> addConjunctiveTerm(Own<ram::Condition> curCondition, Own<ram::Condition> newTerm) {
    return curCondition ? mk<ram::Conjunction>(std::move(curCondition), std::move(newTerm))
                        : std::move(newTerm);
}

}  // namespace souffle::ast2ram
