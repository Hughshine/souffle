/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2021, The Souffle Developers. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file ClauseTranslator.h
 *
 * Translator for clauses from AST to RAM
 *
 ***********************************************************************/

#include "ast2ram/incremental/ClauseTranslator.h"
#include "Global.h"
#include "LogStatement.h"
#include "ast/Aggregator.h"
#include "ast/BranchInit.h"
#include "ast/Clause.h"
#include "ast/Constant.h"
#include "ast/IntrinsicAggregator.h"
#include "ast/IntrinsicFunctor.h"
#include "ast/NilConstant.h"
#include "ast/NumericConstant.h"
#include "ast/RecordInit.h"
#include "ast/Relation.h"
#include "ast/StringConstant.h"
#include "ast/SubsumptiveClause.h"
#include "ast/UnnamedVariable.h"
#include "ast/analysis/Functor.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "ast2ram/utility/Location.h"
#include "ast2ram/utility/SipsMetric.h"
#include "ast2ram/utility/TranslatorContext.h"
#include "ast2ram/utility/Utils.h"
#include "ast2ram/utility/ValueIndex.h"
#include "ram/Aggregate.h"
#include "ram/Break.h"
#include "ram/Constraint.h"
#include "ram/DebugInfo.h"
#include "ram/DeltaUnion.h"
#include "ram/DerivationCheck.h"
#include "ram/EmptinessCheck.h"
#include "ram/EstimateJoinSize.h"
#include "ram/ExistenceCheck.h"
#include "ram/Filter.h"
#include "ram/FloatConstant.h"
#include "ram/GuardedInsert.h"
#include "ram/Insert.h"
#include "ram/IntrinsicAggregator.h"
#include "ram/LogRelationTimer.h"
#include "ram/Negation.h"
#include "ram/NestedIntrinsicOperator.h"
#include "ram/Query.h"
#include "ram/RecordDerivation.h"
#include "ram/Scan.h"
#include "ram/SequantialOperation.h"
#include "ram/Sequence.h"
#include "ram/SignedConstant.h"
#include "ram/StringConstant.h"
#include "ram/TupleElement.h"
#include "ram/UnpackRecord.h"
#include "ram/UnsignedConstant.h"
#include "ram/UserDefinedAggregator.h"
#include "ram/utility/Utils.h"
#include "souffle/TypeAttribute.h"
#include "souffle/utility/StringUtil.h"
#include <map>
#include <unordered_set>
#include <vector>

namespace souffle::ast2ram::incremental {

std::map<std::string, Own<ram::Expression>> cloneClauseVarMap(const std::map<std::string, Own<ram::Expression>>& varExprMap) {
    std::map<std::string, Own<ram::Expression>> newVarExprMap{};
    for (auto& [var, expr] : varExprMap) {
        newVarExprMap.emplace(var, expr->cloning());
    }
    return newVarExprMap;
}

ClauseTranslator::ClauseTranslator(const TranslatorContext& context, TranslationMode mode)
        : ast2ram::ClauseTranslator(context, mode), valueIndex(mk<ValueIndex>()) {}

ClauseTranslator::~ClauseTranslator() = default;

bool ClauseTranslator::isRecursive() const {
    return !sccAtoms.empty();
}

std::string ClauseTranslator::getClauseString(const ast::Clause& clause) const {
    auto renamedClone = clone(clause);

    // Update the head atom
    renamedClone->getHead()->setQualifiedName(
            ast::QualifiedName::fromString(getClauseAtomName(clause, clause.getHead())));

    // Update the body atoms
    const auto& cloneAtoms = ast::getBodyLiterals<ast::Atom>(*renamedClone);
    const auto& originalAtoms = ast::getBodyLiterals<ast::Atom>(clause);
    assert(originalAtoms.size() == cloneAtoms.size() && "clone should have same atoms");
    for (std::size_t i = 0; i < cloneAtoms.size(); i++) {
        auto cloneAtom = cloneAtoms.at(i);
        const auto* originalAtom = originalAtoms.at(i);
        assert(originalAtom->getQualifiedName() == cloneAtom->getQualifiedName() &&
                "atom sequence in clone should match");
        cloneAtom->setQualifiedName(ast::QualifiedName::fromString(getClauseAtomName(clause, originalAtom)));
    }

    return toString(*renamedClone);
}

// version含义：rule的第几个版本；版本总数取决于rule的body有几个sccAtom
std::string ClauseTranslator::getClauseAtomName(const ast::Clause& clause, const ast::Atom* atom) const {
    return getAtomName(clause, atom, sccAtoms, version, isRecursive(), mode, true);
}

// TODO: change to heap
std::map<std::string, Own<ram::Expression>> ClauseTranslator::getClauseVars(const ast::Clause& clause) const {
    std::map<std::string, Own<ram::Expression>> varExprMap{};
    // iterate all body literals and translate it to expression
    for (const auto& lit: clause.getBodyLiterals()) {
        ast::Atom* atom;
        if (isA<ast::Atom>(lit)) {
            atom = as<ast::Atom>(lit);
        } else if (isA<ast::Negation>(lit)) {
            atom = as<ast::Negation>(lit)->getAtom();
        } else {
            assert(false && "constraints are not supported");
        }
        for (const auto* arg : atom->getArguments()) {
            if (const auto& var = as<ast::Variable>(arg)) {
                const auto& varName = var->getName();
                if (varExprMap.find(varName) == varExprMap.end()) {
                    varExprMap.insert({varName, context.translateValue(*valueIndex, var)});
                }
            }
        }
    }
    return varExprMap;
}

Own<ram::Statement> ClauseTranslator::translateRecursiveClause(
        const ast::Clause& clause, const ast::RelationSet& scc, std::size_t version, bool isDelete, bool isPrefill) {
    // Update version config
    sccAtoms = filter(ast::getBodyLiterals<ast::Atom>(clause),
            [&](auto* atom) { return contains(scc, context.getProgram()->getRelation(*atom)); });
    this->version = version;

    // TODO: maybe missing fact case
    // TODO: all delta rules are different for recursive to non-recursive
    // Own<ram::Statement> rule = translateNonRecursiveClause(clause);
    if (isFact(clause)) {
        // TODO: Currently we allow no rule changes, so the fact must already be inserted
        // return mk<ram::EmptyStatement>();  // TODO: add EmptyStatement
        return createRamFactQuery(clause);
    }

    Own<ram::Statement> rule = createRamRecDeltaRulesQuery(clause, isDelete, isPrefill);

    // Add debug info
    std::ostringstream ds;
    clause.printForDebugInfo(ds);
    ds << "\nin file ";
    ds << clause.getSrcLoc();
    rule = mk<ram::DebugInfo>(std::move(rule), ds.str());

    // Add to loop body
    return mk<ram::Sequence>(std::move(rule));
}

Own<ram::Statement> ClauseTranslator::translateNonRecursiveClause(const ast::Clause& clause) {
    // Create the appropriate query
    if (isFact(clause)) {
        // Currently we allow no input program changes, so the fact must already be inserted
        return createRamFactQuery(clause);  // TODO: we can avoid the reinsertion
    }
    // INC: for each rule p :- s1, ..., sn, we need to create n queries
    // each query corresponds to a delta rule, dp :- news_1, ..., ds_i, ..., olds_n
    // note that s_1 ... s_i-1 have new facts, s_i+1 ... s_n should be the old version
    // we create n delta rules (clauses), and invoke createRamRuleQuery on each o them
    // ds_i here only contains added or removed tuples; we don't care about how its derivation set changes in details, because rule application is not a transitive information
    // so delta relation is just newly inserted or newly deleted tuples (insert set and delete set has no overlap)
    // but when we calculate delta relations, we need to update corresponding delta derivation information (rule application information) [i.e. delta derivation information is just "output"]
    // whether a delta relation has a specific tuple, is decided by whether its derivation information is empty
    return createRamDeltaRulesQuery(clause);
    // return createRamRuleQuery(clause);
}

Own<ram::Statement> ClauseTranslator::translateNonRecursiveClauseDel(const ast::Clause& clause) {
    // Create the appropriate query
    if (isFact(clause)) {
        // Currently we allow no input program changes, so the fact must already be inserted
        return mk<ram::EmptyStatement>();  // TODO: return nullptr ok?
        // return createRamFactQuery(clause);  // TODO: we can avoid the reinsertion
    }
    return createRamDeltaRulesQueryDel(clause);
}

Own<ram::Statement> ClauseTranslator::translateNonRecursiveClauseIns(const ast::Clause& clause) {
    // Create the appropriate query
    if (isFact(clause)) {
        // Currently we allow no input program changes, so the fact must already be inserted
        return mk<ram::EmptyStatement>();  // TODO: return nullptr ok?
        // return createRamFactQuery(clause);  // TODO: we can avoid the reinsertion
    }
    return createRamDeltaRulesQueryIns(clause);
}
Own<ram::Statement> ClauseTranslator::createRamRecDeltaRulesQuery(const ast::Clause& clause, bool isDelete, bool isPrefill) {
    assert(isRule(clause) && "clause should be rule");
    VecOwn<ram::Statement> stmts;
    indexClause(clause);
    const auto head = clause.getHead();
    auto headRelationName = getBaseRelationName(head->getQualifiedName());
    auto headDeltaDeleteRelationName = getNewDeletionRelationName(head->getQualifiedName());
    auto headDeltaInsertRelationName = getNewInsertionRelationName(head->getQualifiedName());
    auto clauseStr = clause.toString();
    auto clauseVarMap = getClauseVars(clause);
    VecOwn<ram::Expression> values;  // how head argument is computed by its body (relation name is anonymous)
    for (const auto* arg : head->getArguments()) {
        values.push_back(context.translateValue(*valueIndex, arg));  // TODO
    }
    if (isDelete) {
        for (int i = 0; i < operators.size(); i++) {
            const auto& lit = operators[i];
            if (const auto& atom = as<ast::Atom>(lit)) {
                // if (sccAtoms.) {  // delta rules for
                    // continue;
                // }
                if (!isPrefill) {
                    bool inScc = false;
                    for (int j = 0; j < sccAtoms.size(); j++) {
                        if (sccAtoms.at(j) == atom) {
                            inScc = true;
                            break;
                        }
                    }
                    if (!inScc) {continue;}
                }
                // Propositions
                if (head->getArity() == 0) {
                    assert (false && "proposition (0 arity relation) not supported");
                    // TODO: maybe there is a clever way
                }
                // Relations with functional dependency constraints
                if (auto guardedConditions = getFunctionalDependencies(clause)) {
                    assert (false && "functional dependencies not supported");
                }
                Own<ram::Operation> op = mk<ram::Insert>(headDeltaDeleteRelationName, std::move(clone(values)),
                    context.getClauseNum(&clause), clauseStr, std::move(cloneClauseVarMap(clauseVarMap)));
                op = mk<ram::SequentialOperation>(std::move(op),
                    mk<ram::RecordDerivation>(
                        headRelationName, std::move(clone(values)), context.getClauseNum(&clause), clauseStr,
                        std::move(cloneClauseVarMap(clauseVarMap)), false, false, true)); // delete
                op = addBodyLiteralConstraints(clause, std::move(op), isDelete);
                op = addVariableBindingConstraints(std::move(op));
                op = addGeneratorLevels(std::move(op), clause);
                op = addVariableIntroductions(clause, std::move(op), i, false);
                op = addEntryPoint(clause, std::move(op));
                appendStmt(stmts, std::move(mk<ram::Query>(std::move(op))));
            } else if (const auto& negation = as<ast::Negation>(lit)) {
                // simply omit negation is fine because it's just a "filter"
                // assert (false && "negation not supported");
                const auto& atom = negation->getAtom();
                // TODO: INC NEG
            } else {
                assert(false && "constraints are not supported");
            }
        }
    } else {
        for (int i = 0; i < operators.size(); i++) {
            const auto& lit = operators[i];
            if (const auto& atom = as<ast::Atom>(lit)) {  // TODO: delta rule for non sccAtoms should executes only ones? no fixpoint computation? lifting to loop front.
                if (!isPrefill) {
                    bool inScc = false;
                    for (int j = 0; j < sccAtoms.size(); j++) {
                        if (sccAtoms.at(j) == atom) {
                            inScc = true;
                            break;
                        }
                    }
                    if (!inScc) {continue;}
                }
                // Propositions
                if (head->getArity() == 0) {
                    assert (false && "proposition (0 arity relation) not supported");
                    // TODO: maybe there is a clever way
                }
                // Relations with functional dependency constraints
                if (auto guardedConditions = getFunctionalDependencies(clause)) {
                    assert (false && "functional dependencies not supported");
                }
                Own<ram::Operation> op = mk<ram::Insert>(headDeltaInsertRelationName, std::move(clone(values)),
                    context.getClauseNum(&clause), clauseStr, std::move(cloneClauseVarMap(clauseVarMap)));
                op = mk<ram::SequentialOperation>(std::move(op),
                    mk<ram::RecordDerivation>(
                        headRelationName, std::move(clone(values)), context.getClauseNum(&clause), clauseStr,
                        std::move(cloneClauseVarMap(clauseVarMap)), true, false, true)); // insert
                op = addBodyLiteralConstraints(clause, std::move(op), isDelete);
                op = addVariableBindingConstraints(std::move(op));
                op = addGeneratorLevels(std::move(op), clause);
                op = addVariableIntroductions(clause, std::move(op), i, true);
                op = addEntryPoint(clause, std::move(op));
                appendStmt(stmts, std::move(mk<ram::Query>(std::move(op))));
            } else if (const auto& negation = as<ast::Negation>(lit)) {
                // simply omit negation is fine because it's just a "filter"
                // assert (false && "negation not supported");
                const auto& atom = negation->getAtom();
                // TODO: INC NEG
            } else {
                assert(false && "constraints are not supported");
            }
        }
    }

    // auto headOldR
    return mk<ram::Sequence>(std::move(stmts));
}

Own<ram::Statement> ClauseTranslator::createRamDeltaRulesQuery(const ast::Clause& clause) {
    assert(isRule(clause) && "clause should be rule");
    VecOwn<ram::Statement> stmts;
    // Index all variables and generators in the clause
    indexClause(clause);  // TODO: what does it do?

    const auto head = clause.getHead();
    auto headRelationName = getClauseAtomName(clause, head);
    auto headDeltaDervInsertRelationName = getIncDeltaDervInsertRelationName(head->getQualifiedName());
    auto headDeltaDervDeleteRelationName = getIncDeltaDervDeleteRelationName(head->getQualifiedName());
    auto clauseStr = clause.toString();
    auto clauseVarMap = getClauseVars(clause);
    VecOwn<ram::Expression> values;  // how head argument is computed by its body (relation name is anonymous)
    for (const auto* arg : head->getArguments()) {
        values.push_back(context.translateValue(*valueIndex, arg));  // TODO
    }

    // (bodyLiterals<as Atom>.size() == operators.size());
    for (int i = 0; i < operators.size(); i++) {
        const auto& lit = operators[i];
        // INC: now we try to create delta rule for lit
        if (const auto& atom = as<ast::Atom>(lit)) {
            // TODO: delta rule for non sccAtoms should executes only ones? no fixpoint computation? lifting to loop front.
            // Propositions
            if (head->getArity() == 0) {
                assert (false && "proposition (0 arity relation) not supported");
                // TODO: maybe there is a clever way
            }

            // Relations with functional dependency constraints
            if (auto guardedConditions = getFunctionalDependencies(clause)) {
                assert (false && "functional dependencies not supported");
            }

            {
                // Insert
                Own<ram::Operation> op = mk<ram::Insert>(headDeltaDervInsertRelationName, std::move(clone(values)),
                    context.getClauseNum(&clause), clauseStr, std::move(cloneClauseVarMap(clauseVarMap)));
                op = mk<ram::SequentialOperation>(std::move(op),
                    mk<ram::RecordDerivation>(
                        headRelationName, std::move(clone(values)), context.getClauseNum(&clause), clauseStr,
                        std::move(cloneClauseVarMap(clauseVarMap)), true, false));
                op = addBodyLiteralConstraints(clause, std::move(op), false);
                op = addVariableBindingConstraints(std::move(op));
                op = addGeneratorLevels(std::move(op), clause);
                op = addVariableIntroductions(clause, std::move(op), i, true);  // delta level, isInsert = true
                op = addEntryPoint(clause, std::move(op));
                appendStmt(stmts, std::move(mk<ram::Query>(std::move(op))));
            }
            {
                // Delete
                Own<ram::Operation> op = mk<ram::Insert>(headDeltaDervDeleteRelationName, std::move(clone(values)),
                    context.getClauseNum(&clause), clauseStr, std::move(cloneClauseVarMap(clauseVarMap)));
                op = mk<ram::SequentialOperation>(std::move(op),
                    mk<ram::RecordDerivation>(
                        headRelationName, std::move(clone(values)), context.getClauseNum(&clause), clauseStr,
                        std::move(cloneClauseVarMap(clauseVarMap)), false, false)); // delete
                op = addBodyLiteralConstraints(clause, std::move(op), true);
                op = addVariableBindingConstraints(std::move(op));
                op = addGeneratorLevels(std::move(op), clause);
                op = addVariableIntroductions(clause, std::move(op), i, false);
                op = addEntryPoint(clause, std::move(op));
                appendStmt(stmts, std::move(mk<ram::Query>(std::move(op))));
            }
            // delete to delta deletion head
        } else if (const auto& negation = as<ast::Negation>(lit)) {
            // simply omit negation is fine because it's just a "filter"
            // assert (false && "negation not supported");
            const auto& atom = negation->getAtom();
            // TODO: INC NEG
        } else {
            assert(false && "constraints are not supported");
        }
    }

    return mk<ram::Sequence>(std::move(stmts)); // TODO: stmts
}

Own<ram::Statement> ClauseTranslator::createRamDeltaRulesQueryDel(const ast::Clause& clause) {
    assert(isRule(clause) && "clause should be rule");
    VecOwn<ram::Statement> stmts;
    // Index all variables and generators in the clause
    indexClause(clause);  // TODO: what does it do?

    const auto head = clause.getHead();
    auto headRelationName = getClauseAtomName(clause, head);
    auto headDeltaDervInsertRelationName = getIncDeltaDervInsertRelationName(head->getQualifiedName());
    auto headDeltaDervDeleteRelationName = getIncDeltaDervDeleteRelationName(head->getQualifiedName());
    auto clauseStr = clause.toString();
    auto clauseVarMap = getClauseVars(clause);
    VecOwn<ram::Expression> values;  // how head argument is computed by its body (relation name is anonymous)
    for (const auto* arg : head->getArguments()) {
        values.push_back(context.translateValue(*valueIndex, arg));  // TODO
    }

    // (bodyLiterals<as Atom>.size() == operators.size());
    for (int i = 0; i < operators.size(); i++) {
        const auto& lit = operators[i];
        // INC: now we try to create delta rule for lit
        if (const auto& atom = as<ast::Atom>(lit)) {
            // Propositions
            if (head->getArity() == 0) {
                assert (false && "proposition (0 arity relation) not supported");
                // TODO: maybe there is a clever way
            }

            // Relations with functional dependency constraints
            if (auto guardedConditions = getFunctionalDependencies(clause)) {
                assert (false && "functional dependencies not supported");
            }
            {
                // Delete
                Own<ram::Operation> op = mk<ram::Insert>(headDeltaDervDeleteRelationName, std::move(clone(values)),
                    context.getClauseNum(&clause), clauseStr, std::move(cloneClauseVarMap(clauseVarMap)));
                op = mk<ram::SequentialOperation>(std::move(op),
                    mk<ram::RecordDerivation>(
                        headRelationName, std::move(clone(values)), context.getClauseNum(&clause), clauseStr,
                        std::move(cloneClauseVarMap(clauseVarMap)), false, false)); // delete
                op = addBodyLiteralConstraints(clause, std::move(op), true);
                op = addVariableBindingConstraints(std::move(op));
                op = addGeneratorLevels(std::move(op), clause);
                op = addVariableIntroductions(clause, std::move(op), i, false);
                op = addEntryPoint(clause, std::move(op));
                appendStmt(stmts, std::move(mk<ram::Query>(std::move(op))));
            }
            // delete to delta deletion head
        } else if (const auto& negation = as<ast::Negation>(lit)) {
            // simply omit negation is fine because it's just a "filter"
            // assert (false && "negation not supported");
            const auto& atom = negation->getAtom();
            // TODO: INC NEG
        } else {
            assert(false && "constraints are not supported");
        }
    }

    return mk<ram::Sequence>(std::move(stmts)); // TODO: stmts
}

Own<ram::Statement> ClauseTranslator::createRamDeltaRulesQueryIns(const ast::Clause& clause) {
    assert(isRule(clause) && "clause should be rule");
    VecOwn<ram::Statement> stmts;
    // Index all variables and generators in the clause
    indexClause(clause);  // TODO: what does it do?

    const auto head = clause.getHead();
    auto headRelationName = getClauseAtomName(clause, head);
    auto headDeltaDervInsertRelationName = getIncDeltaDervInsertRelationName(head->getQualifiedName());
    // auto headDeltaDervDeleteRelationName = getIncDeltaDervDeleteRelationName(head->getQualifiedName());
    auto clauseStr = clause.toString();
    auto clauseVarMap = getClauseVars(clause);
    VecOwn<ram::Expression> values;  // how head argument is computed by its body (relation name is anonymous)
    for (const auto* arg : head->getArguments()) {
        values.push_back(context.translateValue(*valueIndex, arg));  // TODO
    }

    // (bodyLiterals<as Atom>.size() == operators.size());
    for (int i = 0; i < operators.size(); i++) {
        const auto& lit = operators[i];
        // INC: now we try to create delta rule for lit
        if (const auto& atom = as<ast::Atom>(lit)) {
            // Propositions
            if (head->getArity() == 0) {
                assert (false && "proposition (0 arity relation) not supported");
                // TODO: maybe there is a clever way
            }

            // Relations with functional dependency constraints
            if (auto guardedConditions = getFunctionalDependencies(clause)) {
                assert (false && "functional dependencies not supported");
            }

            {
                // Insert
                Own<ram::Operation> op = mk<ram::Insert>(headDeltaDervInsertRelationName, std::move(clone(values)),
                    context.getClauseNum(&clause), clauseStr, std::move(cloneClauseVarMap(clauseVarMap)));
                op = mk<ram::SequentialOperation>(std::move(op),
                    mk<ram::RecordDerivation>(
                        headRelationName, std::move(clone(values)), context.getClauseNum(&clause), clauseStr,
                        std::move(cloneClauseVarMap(clauseVarMap)), true, false));
                op = addBodyLiteralConstraints(clause, std::move(op), false);
                op = addVariableBindingConstraints(std::move(op));
                op = addGeneratorLevels(std::move(op), clause);
                op = addVariableIntroductions(clause, std::move(op), i, true);  // delta level, isInsert = true
                op = addEntryPoint(clause, std::move(op));
                appendStmt(stmts, std::move(mk<ram::Query>(std::move(op))));
            }
        } else if (const auto& negation = as<ast::Negation>(lit)) {
            // simply omit negation is fine because it's just a "filter"
            // assert (false && "negation not supported");
            const auto& atom = negation->getAtom();
            // TODO: INC NEG
        } else {
            assert(false && "constraints are not supported");
        }
    }

    return mk<ram::Sequence>(std::move(stmts)); // TODO: stmts
}



Own<ram::Statement> ClauseTranslator::createRamFactQuery(const ast::Clause& clause) const {
    assert(isFact(clause) && "clause should be fact");
    assert(!isRecursive() && "recursive clauses cannot have facts");

    // Create a fact statement
    return mk<ram::Query>(createInsertion(clause));
}

Own<ram::Statement> ClauseTranslator::createRamRuleQuery(const ast::Clause& clause) {
    assert (false && "only support delta rules, WIP");
    // assert(isRule(clause) && "clause should be rule");
    //
    // // Index all variables and generators in the clause
    // indexClause(clause);
    //
    // // Set up the RAM statement bottom-up
    // auto op = createInsertion(clause);
    // op = addBodyLiteralConstraints(clause, std::move(op));
    // op = addVariableBindingConstraints(std::move(op));
    // op = addGeneratorLevels(std::move(op), clause);
    // op = addVariableIntroductions(clause, std::move(op), false);
    // op = addEntryPoint(clause, std::move(op));
    // return mk<ram::Query>(std::move(op));
}

Own<ram::Operation> ClauseTranslator::addEntryPoint(const ast::Clause& clause, Own<ram::Operation> op) const {
    auto cond = createCondition(clause);
    return cond != nullptr ? mk<ram::Filter>(std::move(cond), std::move(op)) : std::move(op);
}

Own<ram::Operation> ClauseTranslator::addVariableBindingConstraints(Own<ram::Operation> op) const {
    for (const auto& [_, references] : valueIndex->getVariableReferences()) {
        // Equate the first appearance to all other appearances
        assert(!references.empty() && "variable should appear at least once");
        const auto& first = *references.begin();
        for (const auto& reference : references) {
            if (first != reference && !valueIndex->isGenerator(reference.identifier)) {
                // TODO: float type equivalence check
                op = addEqualityCheck(
                        std::move(op), makeRamTupleElement(first), makeRamTupleElement(reference), false);
            }
        }
    }
    return op;
}

Own<ram::Operation> ClauseTranslator::createInsertion(const ast::Clause& clause) const {
    const auto head = clause.getHead();
    auto headRelationName = getClauseAtomName(clause, head);

    auto clauseVarMap = getClauseVars(clause);

    VecOwn<ram::Expression> values;
    for (const auto* arg : head->getArguments()) {
        values.push_back(context.translateValue(*valueIndex, arg));
    }

    auto clauseStr = clause.toString();


    // Propositions
    if (head->getArity() == 0) {
        assert (false && "proposition (0 arity relation) not supported"); // TODO: maybe there is a clever way
        // return mk<ram::Filter>(mk<ram::EmptinessCheck>(headRelationName),
                // mk<ram::Insert>(headRelationName, std::move(values), context.getClauseNum(&clause), clauseStr, std::move(cloneClauseVarMap(clauseVarMap))));
    }

    // Relations with functional dependency constraints
    if (auto guardedConditions = getFunctionalDependencies(clause)) {
        // return mk<ram::GuardedInsert>(headRelationName, std::move(values), std::move(guardedConditions),
        //     context.getClauseNum(&clause), clauseStr, std::move(cloneClauseVarMap(clauseVarMap)));
        assert (false && "functional dependencies not supported");
    }

    // Everything else
    return mk<ram::Insert>(headRelationName, std::move(values),
        context.getClauseNum(&clause), clauseStr, std::move(cloneClauseVarMap(clauseVarMap)));
}

std::string ClauseTranslator::getAtomNameForIncDeltaRule(const ast::Clause& clause, const ast::Atom* atom, const std::size_t curIndex, const std::size_t deltaIndex, const bool isInsert) const {
    assert(!isRecursive() && "recursive not supported");
    if (curIndex < deltaIndex) {
        // new relation (should change the name
        // this new relation is not the new relation for delta cases )
        return getConcreteRelationName(atom->getQualifiedName());
    }
    if (curIndex > deltaIndex) {
        // old relation
        return getOldRelationName(atom->getQualifiedName());
    }
        // delta case
    if (isInsert) {
        return getIncDeltaTupleInsertRelationName(atom->getQualifiedName());
    } else {
        return getIncDeltaTupleDeleteRelationName(atom->getQualifiedName());
    }
}

std::string ClauseTranslator::getAtomNameForRecIncDeltaRule(const ast::Clause& clause, const ast::Atom* atom, const std::size_t curIndex, const std::size_t deltaIndex, const bool isInsert/*, const bool isPrefill*/) const {
    if (curIndex < deltaIndex || curIndex > deltaIndex) {
        // for rec and inc, "old" means different things (for sccAtoms...)...
        // for rec, delta rules make no difference to < and >
        bool inScc = false;
        for (int i = 0; i < sccAtoms.size(); ++i) {
            if (sccAtoms[i] == atom) {
                inScc = true; break;
            }
        }
        if (inScc) {
            if (isInsert) {
                return getConcreteRelationName(atom->getQualifiedName());
            } else {
                return getOldRelationName(atom->getQualifiedName());
            }
        } else {
            if (isInsert) {
                return getConcreteRelationName(atom->getQualifiedName());
            }
            return getOldRelationName(atom->getQualifiedName());
        }
    }
    // if (curIndex > deltaIndex) {
    //     bool inScc = false;
    //     for (int i = 0; i < sccAtoms.size(); ++i) {
    //         if (sccAtoms[i] == atom) {
    //             inScc = true; break;
    //         }
    //     }
    //     if (inScc) {
    //         return getConcreteRelationName(atom->getQualifiedName());
    //     } else {
    //         if (isInsert) {
    //             return getConcreteRelationName(atom->getQualifiedName());
    //         }
    //         return getOldRelationName(atom->getQualifiedName());
    //     }
    // }
    // delta case
    // TODO: non sccAtoms should be different
    if (isInsert) {
        if (sccAtoms.at(version) == atom) {
            return getDeltaInsertionRelationName(atom->getQualifiedName());
        } else {
            // non sccAtoms, need to consider their delta; TODO: but should only need to consider them once???
            return getIncDeltaTupleInsertRelationName(atom->getQualifiedName());
        }
    } else {
        if (sccAtoms.at(version) == atom) {
            return getDeltaDeletionRelationName(atom->getQualifiedName());
        } else {
            return getIncDeltaTupleDeleteRelationName(atom->getQualifiedName());
        }
    }
}


Own<ram::Operation> ClauseTranslator::addAtomScanRec(Own<ram::Operation> op, const ast::Atom* atom,
        const ast::Clause& clause, const std::size_t curLevel, const std::size_t deltaLevel, const bool isInsert) const {
    const ast::Atom* head = clause.getHead();

    // add constraints
    op = addConstantConstraints(curLevel, atom->getArguments(), std::move(op));

    auto relName = getAtomNameForRecIncDeltaRule(clause, atom, curLevel, deltaLevel, isInsert);
    // add check for emptiness for an atom
    op = mk<ram::Filter>(
            mk<ram::Negation>(mk<ram::EmptinessCheck>(relName)), std::move(op));

    // check whether all arguments are unnamed variables
    bool isAllArgsUnnamed = all_of(
            atom->getArguments(), [&](const ast::Argument* arg) { return isA<ast::UnnamedVariable>(arg); });

    // add a scan level
    if (atom->getArity() != 0 && !isAllArgsUnnamed) {
        if (head->getArity() == 0) {
            assert (false && "zero arity");
            op = mk<ram::Break>(mk<ram::Negation>(mk<ram::EmptinessCheck>(getClauseAtomName(clause, head))),
                    std::move(op));
        }

        std::stringstream ss;
        if (context.getGlobal()->config().has("profile")) {
            ss << "@frequency-atom" << ';';
            ss << clause.getHead()->getQualifiedName() << ';';
            ss << version << ';';
            ss << stringify(getClauseString(clause)) << ';';
            ss << stringify(getClauseAtomName(clause, atom)) << ';';
            ss << stringify(toString(clause)) << ';';
            ss << curLevel << ';';
        }

        op = mk<ram::Scan>(relName, curLevel, std::move(op), ss.str());
    }

    return op;
}

Own<ram::Operation> ClauseTranslator::addAtomScan(Own<ram::Operation> op, const ast::Atom* atom,
        const ast::Clause& clause, const std::size_t curLevel, const std::size_t deltaLevel, const bool isInsert) const {
    const ast::Atom* head = clause.getHead();

    // add constraints
    op = addConstantConstraints(curLevel, atom->getArguments(), std::move(op));

    auto relName = getAtomNameForIncDeltaRule(clause, atom, curLevel, deltaLevel, isInsert);
    // add check for emptiness for an atom
    op = mk<ram::Filter>(
            mk<ram::Negation>(mk<ram::EmptinessCheck>(relName)), std::move(op));

    // check whether all arguments are unnamed variables
    bool isAllArgsUnnamed = all_of(
            atom->getArguments(), [&](const ast::Argument* arg) { return isA<ast::UnnamedVariable>(arg); });

    // add a scan level
    if (atom->getArity() != 0 && !isAllArgsUnnamed) {
        if (head->getArity() == 0) {
            assert (false && "zero arity");
            op = mk<ram::Break>(mk<ram::Negation>(mk<ram::EmptinessCheck>(getClauseAtomName(clause, head))),
                    std::move(op));
        }

        std::stringstream ss;
        if (context.getGlobal()->config().has("profile")) {
            ss << "@frequency-atom" << ';';
            ss << clause.getHead()->getQualifiedName() << ';';
            ss << version << ';';
            ss << stringify(getClauseString(clause)) << ';';
            ss << stringify(getClauseAtomName(clause, atom)) << ';';
            ss << stringify(toString(clause)) << ';';
            ss << curLevel << ';';
        }

        op = mk<ram::Scan>(relName, curLevel, std::move(op), ss.str());
    }

    return op;
}

Own<ram::Operation> ClauseTranslator::addRecordUnpack(
        Own<ram::Operation> op, const ast::RecordInit* rec, std::size_t curLevel) const {
    // add constant constraints
    op = addConstantConstraints(curLevel, rec->getArguments(), std::move(op));

    // add an unpack level
    const Location& loc = valueIndex->getDefinitionPoint(*rec);
    op = mk<ram::UnpackRecord>(std::move(op), curLevel, makeRamTupleElement(loc), rec->getArguments().size());
    return op;
}

Own<ram::Operation> ClauseTranslator::addAdtUnpack(
        Own<ram::Operation> op, const ast::BranchInit* adt, std::size_t curLevel) const {
    assert(!context.isADTEnum(adt) && "ADT enums should not be unpacked");

    std::vector<ast::Argument*> branchArguments;

    std::size_t branchLevel;
    // only for ADT with arity less than two (= simple)
    // add padding for branch id
    auto dummyArg = mk<ast::UnnamedVariable>();

    if (context.isADTBranchSimple(adt)) {
        // for ADT with arity < 2, we have a single level
        branchLevel = curLevel;
        branchArguments.push_back(dummyArg.get());
    } else {
        // for ADT with arity < 2, we have two levels of
        // nesting, the second one being for the arguments
        branchLevel = curLevel - 1;
    }

    for (auto* arg : adt->getArguments()) {
        branchArguments.push_back(arg);
    }

    // set branch tag constraint
    op = addEqualityCheck(std::move(op), mk<ram::TupleElement>(branchLevel, 0),
            mk<ram::SignedConstant>(context.getADTBranchId(adt)), false);

    if (context.isADTBranchSimple(adt)) {
        op = addConstantConstraints(branchLevel, branchArguments, std::move(op));
    } else {
        op = addConstantConstraints(curLevel, branchArguments, std::move(op));
        op = mk<ram::UnpackRecord>(
                std::move(op), curLevel, mk<ram::TupleElement>(branchLevel, 1), branchArguments.size());
    }

    const Location& loc = valueIndex->getDefinitionPoint(*adt);
    // add an unpack level for main record
    op = mk<ram::UnpackRecord>(std::move(op), branchLevel, makeRamTupleElement(loc), 2);

    return op;
}

Own<ram::Operation> ClauseTranslator::addVariableIntroductions(
        const ast::Clause& clause, Own<ram::Operation> op, std::size_t deltaLevel, bool isInsert) {
    for (std::size_t p = operators.size(); p > 0; p--) {
        std::size_t i = p - 1;
        const auto* curOp = operators.at(i);
        if (const auto* atom = as<ast::Atom>(curOp)) {
            // add atom arguments through a scan
            if (isRecursive()) {
                op = addAtomScanRec(std::move(op), atom, clause, i, deltaLevel, isInsert);
            } else {
                op = addAtomScan(std::move(op), atom, clause, i, deltaLevel, isInsert);
            }
        } else if (const auto* negation = as<ast::Negation>(curOp)) {
            // in probabilistic setting, negation can be simply omit; though there is room for optimization for deterministic facts (prob of 1)
            // and still, it only introduces new constraints, TODO
            // negation do not introduce new bounded variables, so no additional layer of scan
        } else {
            fatal("Unsupported AST node for creation of scan-level!");
        }
    }
    return op;
}

Own<ram::Operation> ClauseTranslator::instantiateAggregator(Own<ram::Operation> op, const ast::Clause& clause,
        const ast::Aggregator* agg, std::size_t curLevel) const {
    auto addAggEqCondition = [&](Own<ram::Condition> aggr, Own<ram::Expression> value, std::size_t pos) {
        if (isUndefValue(value.get())) return aggr;

        // TODO: float type equivalence check
        return addConjunctiveTerm(
                std::move(aggr), mk<ram::Constraint>(BinaryConstraintOp::EQ,
                                         mk<ram::TupleElement>(curLevel, pos), std::move(value)));
    };

    Own<ram::Condition> aggCond;

    // translate constraints of sub-clause
    for (const auto* lit : agg->getBodyLiterals()) {
        // literal becomes a constraint
        if (auto condition = context.translateConstraint(*valueIndex, lit)) {
            aggCond = addConjunctiveTerm(std::move(aggCond), std::move(condition));
        }
    }

    // translate arguments of atom to conditions
    const auto& aggBodyAtoms =
            filter(agg->getBodyLiterals(), [&](const ast::Literal* lit) { return isA<ast::Atom>(lit); });
    assert(aggBodyAtoms.size() == 1 && "exactly one atom should exist per aggregator body");
    const auto* aggAtom = static_cast<const ast::Atom*>(aggBodyAtoms.at(0));

    const auto& aggAtomArgs = aggAtom->getArguments();
    for (std::size_t i = 0; i < aggAtomArgs.size(); i++) {
        const auto* arg = aggAtomArgs.at(i);

        // variable bindings are issued differently since we don't want self
        // referential variable bindings
        if (auto* var = as<ast::Variable>(arg)) {
            for (auto&& loc : valueIndex->getVariableReferences(var->getName())) {
                if (curLevel != loc.identifier || i != loc.element) {
                    aggCond = addAggEqCondition(std::move(aggCond), makeRamTupleElement(loc), i);
                    break;
                }
            }
        } else {
            assert(arg != nullptr && "aggregator argument cannot be nullptr");
            auto value = context.translateValue(*valueIndex, arg);
            aggCond = addAggEqCondition(std::move(aggCond), std::move(value), i);
        }
    }

    // translate aggregate expression
    const auto* aggExpr = agg->getTargetExpression();
    auto expr = aggExpr ? context.translateValue(*valueIndex, aggExpr) : nullptr;

    auto aggregator = [&]() -> Own<ram::Aggregator> {
        if (const auto* ia = as<ast::IntrinsicAggregator>(agg)) {
            return mk<ram::IntrinsicAggregator>(context.getOverloadedAggregatorOperator(*ia));
        } else if (const auto* uda = as<ast::UserDefinedAggregator>(agg)) {
            return mk<ram::UserDefinedAggregator>(uda->getBaseOperatorName(),
                    context.translateValue(*valueIndex, uda->getInit()),
                    context.getFunctorParamTypeAtributes(*uda), context.getFunctorReturnTypeAttribute(*uda),
                    context.isStatefulFunctor(*uda));
        } else {
            fatal("Unhandled aggregate operation");
        }
    }();

    // add Ram-Aggregation layer
    return mk<ram::Aggregate>(std::move(op), std::move(aggregator), getClauseAtomName(clause, aggAtom),
            expr ? std::move(expr) : mk<ram::UndefValue>(), aggCond ? std::move(aggCond) : mk<ram::True>(),
            curLevel);
}

Own<ram::Operation> ClauseTranslator::instantiateMultiResultFunctor(
        Own<ram::Operation> op, const ast::IntrinsicFunctor& inf, std::size_t curLevel) const {
    VecOwn<ram::Expression> args;
    for (auto&& x : inf.getArguments()) {
        args.push_back(context.translateValue(*valueIndex, x));
    }

    auto func_op = [&]() -> ram::NestedIntrinsicOp {
        switch (context.getOverloadedFunctorOp(inf)) {
            case FunctorOp::RANGE: return ram::NestedIntrinsicOp::RANGE;
            case FunctorOp::URANGE: return ram::NestedIntrinsicOp::URANGE;
            case FunctorOp::FRANGE: return ram::NestedIntrinsicOp::FRANGE;

            default: fatal("missing case handler or bad code-gen");
        }
    };

    return mk<ram::NestedIntrinsicOperator>(func_op(), std::move(args), std::move(op), curLevel);
}

Own<ram::Operation> ClauseTranslator::addGeneratorLevels(
        Own<ram::Operation> op, const ast::Clause& clause) const {
    std::size_t curLevel = operators.size() + generators.size() - 1;
    for (const auto* generator : reverse(generators)) {
        if (auto agg = as<ast::Aggregator>(generator)) {
            op = instantiateAggregator(std::move(op), clause, agg, curLevel);
        } else if (const auto* inf = as<ast::IntrinsicFunctor>(generator)) {
            op = instantiateMultiResultFunctor(std::move(op), *inf, curLevel);
        } else {
            assert(false && "unhandled generator");
        }
        curLevel--;
    }
    return op;
}

Own<ram::Operation> ClauseTranslator::addDistinct(
        Own<ram::Operation> op, const ast::Atom* atom1, const ast::Atom* atom2) const {
    std::size_t arity = atom1->getArity();

    VecOwn<ram::Condition> conditions;
    auto args1 = atom1->getArguments();
    auto args2 = atom2->getArguments();
    for (std::size_t i = 0; i < arity; i++) {
        Own<ram::Expression> a1 = context.translateValue(*valueIndex, args1[i]);
        Own<ram::Expression> a2 = context.translateValue(*valueIndex, args2[i]);
        if (*a1 != *a2) {
            conditions.push_back(mk<ram::Constraint>(BinaryConstraintOp::EQ, std::move(a1), std::move(a2)));
        }
    }
    return mk<ram::Filter>(mk<ram::Negation>(toCondition(conditions)), std::move(op));
}

Own<ram::Operation> ClauseTranslator::addNegatedDeltaAtom(
        Own<ram::Operation> op, const ast::Atom* atom, const bool isDelete) const {
    std::size_t arity = atom->getArity();
    std::string name;
    if (isDelete) {
        name = getDeltaDeletionRelationName(atom->getQualifiedName());
    } else {
        name = getDeltaInsertionRelationName(atom->getQualifiedName());
    }
    if (arity == 0) {
        // for a nullary, negation is a simple emptiness check
        return mk<ram::Filter>(mk<ram::EmptinessCheck>(name), std::move(op));
    }

    // else, we construct the atom and create a negation
    VecOwn<ram::Expression> values;
    auto args = atom->getArguments();
    for (std::size_t i = 0; i < arity; i++) {
        values.push_back(context.translateValue(*valueIndex, args[i]));
    }

    return mk<ram::Filter>(
            mk<ram::Negation>(mk<ram::ExistenceCheck>(name, std::move(values))), std::move(op));
}

Own<ram::Operation> ClauseTranslator::addNegatedAtom(
        Own<ram::Operation> op, const ast::Clause& /* clause */, const ast::Atom* atom) const {
    std::size_t arity = atom->getArity();
    std::string name = getConcreteRelationName(atom->getQualifiedName());

    if (arity == 0) {
        // for a nullary, negation is a simple emptiness check
        return mk<ram::Filter>(mk<ram::EmptinessCheck>(name), std::move(op));
    }

    // else, we construct the atom and create a negation
    VecOwn<ram::Expression> values;
    auto args = atom->getArguments();
    for (std::size_t i = 0; i < arity; i++) {
        values.push_back(context.translateValue(*valueIndex, args[i]));
    }
    return mk<ram::Filter>(
            mk<ram::Negation>(mk<ram::ExistenceCheck>(name, std::move(values))), std::move(op));
}


Own<ram::Operation> ClauseTranslator::addNegatedAtomDerived(
        Own<ram::Operation> op, const ast::Clause& clause, const ast::Atom* atom) const {
    const auto head = clause.getHead();
    auto headRelationName = getConcreteRelationName(head->getQualifiedName());

    auto clauseVarMap = getClauseVars(clause);

    VecOwn<ram::Expression> values;
    for (const auto* arg : head->getArguments()) {
        values.push_back(context.translateValue(*valueIndex, arg));
    }

    auto clauseStr = clause.toString();
    op = mk<ram::Filter>(
    mk<ram::Negation>(mk<ram::DerivationCheck>(headRelationName, std::move(values), context.getClauseNum(&clause), std::move(cloneClauseVarMap(clauseVarMap)))), std::move(op));
    return mk<ram::Filter>(
    mk<ram::Negation>(mk<ram::ExistenceCheck>(headRelationName, std::move(values))), std::move(op));
}

Own<ram::Operation> ClauseTranslator::addAtomDerived(
        Own<ram::Operation> op, const ast::Clause& clause, const ast::Atom* atom) const {
    const auto head = clause.getHead();
    auto headRelationName = getConcreteRelationName(head->getQualifiedName());

    auto clauseVarMap = getClauseVars(clause);

    VecOwn<ram::Expression> values;
    for (const auto* arg : head->getArguments()) {
        values.push_back(context.translateValue(*valueIndex, arg));
    }

    auto clauseStr = clause.toString();
    op = mk<ram::Filter>(
    (mk<ram::DerivationCheck>(headRelationName, std::move(values), context.getClauseNum(&clause), std::move(cloneClauseVarMap(clauseVarMap)))), std::move(op));
    return mk<ram::Filter>(
    mk<ram::Negation>(mk<ram::ExistenceCheck>(headRelationName, std::move(values))), std::move(op));
}



Own<ram::Operation> ClauseTranslator::addBodyLiteralConstraints(
        const ast::Clause& clause, Own<ram::Operation> op, const bool isDelete) const {
    // note that for non-recursive cases, isDelete flag is useless
    for (const auto* lit : clause.getBodyLiterals()) {
        // constraints become literals
        if (auto condition = context.translateConstraint(*valueIndex, lit)) {
            op = mk<ram::Filter>(std::move(condition), std::move(op));
        }
    }

    if (isRecursive()) {
        if (clause.getHead()->getArity() > 0) {
            // also negate the head, but for prob, we should only negate the "derivation".
            if (!isDelete) {
                op = addNegatedAtomDerived(std::move(op), clause, clause.getHead());
            } else {
                op = addAtomDerived(std::move(op), clause, clause.getHead());
            }
        }
        // also add in prev stuff
        // every former delta rule do not need to scan the delta of later atoms
        // since they have their own delta rules
        // only for optimization I suppose
        // we do not need to change this for prob, but we should change it for inc
        // because we have different name for delta relation for inc
        for (std::size_t i = version + 1; i < sccAtoms.size(); i++) {
            op = addNegatedDeltaAtom(std::move(op), sccAtoms.at(i), isDelete);
        }
    }

    return op;
}

Own<ram::Condition> ClauseTranslator::createCondition(const ast::Clause& clause) const {
    const auto head = clause.getHead();

    // add stopping criteria for nullary relations
    // (if it contains already the null tuple, don't re-compute)
    if (isRecursive() && head->getArity() == 0) {
        return mk<ram::EmptinessCheck>(getConcreteRelationName(head->getQualifiedName()));
    }
    return nullptr;
}

Own<ram::Expression> ClauseTranslator::translateConstant(const ast::Constant& constant) const {
    if (auto strConstant = as<ast::StringConstant>(constant)) {
        return mk<ram::StringConstant>(strConstant->getConstant());
    } else if (isA<ast::NilConstant>(&constant)) {
        return mk<ram::SignedConstant>(0);
    } else if (auto* numConstant = as<ast::NumericConstant>(constant)) {
        switch (context.getInferredNumericConstantType(*numConstant)) {
            case ast::NumericConstant::Type::Int:
                return mk<ram::SignedConstant>(RamSignedFromString(numConstant->getConstant(), nullptr, 0));
            case ast::NumericConstant::Type::Uint:
                return mk<ram::UnsignedConstant>(
                        RamUnsignedFromString(numConstant->getConstant(), nullptr, 0));
            case ast::NumericConstant::Type::Float:
                return mk<ram::FloatConstant>(RamFloatFromString(numConstant->getConstant()));
        }
    }
    fatal("unaccounted-for constant");
}

Own<ram::Operation> ClauseTranslator::addEqualityCheck(
        Own<ram::Operation> op, Own<ram::Expression> lhs, Own<ram::Expression> rhs, bool isFloat) const {
    auto eqOp = isFloat ? BinaryConstraintOp::FEQ : BinaryConstraintOp::EQ;
    auto eqConstraint = mk<ram::Constraint>(eqOp, std::move(lhs), std::move(rhs));
    return mk<ram::Filter>(std::move(eqConstraint), std::move(op));
}

Own<ram::Operation> ClauseTranslator::addConstantConstraints(
        std::size_t curLevel, const std::vector<ast::Argument*>& arguments, Own<ram::Operation> op) const {
    for (std::size_t i = 0; i < arguments.size(); i++) {
        const auto* argument = arguments.at(i);
        if (const auto* numericConstant = as<ast::NumericConstant>(argument)) {
            bool isFloat = context.getInferredNumericConstantType(*numericConstant) ==
                           ast::NumericConstant::Type::Float;
            auto lhs = mk<ram::TupleElement>(curLevel, i);
            auto rhs = translateConstant(*numericConstant);
            op = addEqualityCheck(std::move(op), std::move(lhs), std::move(rhs), isFloat);
        } else if (const auto* constant = as<ast::Constant>(argument)) {
            auto lhs = mk<ram::TupleElement>(curLevel, i);
            auto rhs = translateConstant(*constant);
            op = addEqualityCheck(std::move(op), std::move(lhs), std::move(rhs), false);
        } else if (const auto* adt = as<ast::BranchInit>(argument)) {
            if (context.isADTEnum(adt)) {
                auto lhs = mk<ram::TupleElement>(curLevel, i);
                auto rhs = mk<ram::SignedConstant>(context.getADTBranchId(adt));
                op = addEqualityCheck(std::move(op), std::move(lhs), std::move(rhs), false);
            }
        }
    }

    return op;
}

Own<ram::Condition> ClauseTranslator::getFunctionalDependencies(const ast::Clause& clause) const {
    const auto* head = clause.getHead();
    const auto* relation = context.getProgram()->getRelation(clause);
    if (relation->getFunctionalDependencies().empty()) {
        return nullptr;
    }

    std::string headRelationName = getClauseAtomName(clause, head);
    const auto& attributes = relation->getAttributes();
    const auto& headArgs = head->getArguments();

    // Impose the functional dependencies of the relation on each INSERT
    VecOwn<ram::Condition> dependencies;
    std::vector<const ast::FunctionalConstraint*> addedConstraints;
    for (const auto* fd : relation->getFunctionalDependencies()) {
        // Skip if already seen
        bool alreadySeen = false;
        for (const auto* other : addedConstraints) {
            if (other->equivalentConstraint(*fd)) {
                alreadySeen = true;
                break;
            }
        }
        if (alreadySeen) {
            continue;
        }

        // Remove redundant attributes within the same key
        addedConstraints.push_back(fd);
        std::set<std::string> keys;
        for (auto key : fd->getKeys()) {
            keys.insert(key->getName());
        }

        // Grab the necessary head arguments
        VecOwn<ram::Expression> vals;
        VecOwn<ram::Expression> valsCopy;
        for (std::size_t i = 0; i < attributes.size(); ++i) {
            const auto attribute = attributes[i];
            if (contains(keys, attribute->getName())) {
                // If this particular source argument matches the head argument, insert it.
                vals.push_back(context.translateValue(*valueIndex, headArgs.at(i)));
                valsCopy.push_back(context.translateValue(*valueIndex, headArgs.at(i)));
            } else {
                // Otherwise insert ⊥
                vals.push_back(mk<ram::UndefValue>());
                valsCopy.push_back(mk<ram::UndefValue>());
            }
        }

        if (isRecursive()) {
            // If we are in a recursive clause, need to guard both new and original relation.
            dependencies.push_back(
                    mk<ram::Negation>(mk<ram::ExistenceCheck>(headRelationName, std::move(vals))));
            dependencies.push_back(mk<ram::Negation>(mk<ram::ExistenceCheck>(
                    getConcreteRelationName(relation->getQualifiedName()), std::move(valsCopy))));
        } else {
            dependencies.push_back(
                    mk<ram::Negation>(mk<ram::ExistenceCheck>(headRelationName, std::move(vals))));
        }
    }

    return ram::toCondition(dependencies);
}

std::vector<ast::Atom*> ClauseTranslator::getAtomOrdering(const ast::Clause& clause) const {
    auto atoms = ast::getBodyLiterals<ast::Atom>(clause);

    // stick to the plan if we have one set
    auto* plan = clause.getExecutionPlan();
    if (plan != nullptr) {
        auto orders = plan->getOrders();
        if (contains(orders, version)) {
            // get the imposed order, and change it to start at zero
            const auto& order = orders.at(version);
            std::vector<std::size_t> newOrder(order->getOrder().size());
            std::transform(order->getOrder().begin(), order->getOrder().end(), newOrder.begin(),
                    [](std::size_t i) -> std::size_t { return i - 1; });
            return reorderAtoms(atoms, newOrder);
        }
    }

    std::vector<std::string> atomNames;
    for (auto* atom : atoms) {
        atomNames.push_back(getClauseAtomName(clause, atom));
    }
    auto newOrder = context.getSipsMetric()->getReordering(&clause, atomNames);
    return reorderAtoms(atoms, newOrder);
}

std::size_t ClauseTranslator::addOperatorLevel(const ast::Node* node) {
    std::size_t nodeLevel = operators.size() + generators.size();
    operators.push_back(node);
    return nodeLevel;
}

std::size_t ClauseTranslator::addGeneratorLevel(const ast::Argument* arg) {
    std::size_t generatorLevel = operators.size() + generators.size();
    generators.push_back(arg);
    return generatorLevel;
}

void ClauseTranslator::indexNodeArguments(
        std::size_t nodeLevel, const std::vector<ast::Argument*>& nodeArgs) {
    for (std::size_t i = 0; i < nodeArgs.size(); i++) {
        const auto& arg = nodeArgs.at(i);

        // check for variable references
        if (const auto* var = as<ast::Variable>(arg)) {
            valueIndex->addVarReference(var->getName(), nodeLevel, i);
        }

        // check for nested records
        if (const auto* rec = as<ast::RecordInit>(arg)) {
            valueIndex->setRecordDefinition(*rec, nodeLevel, i);

            // introduce new nesting level for unpack
            auto unpackLevel = addOperatorLevel(rec);
            indexNodeArguments(unpackLevel, rec->getArguments());
        }

        // check for nested ADT branches
        if (const auto* adt = as<ast::BranchInit>(arg)) {
            if (!context.isADTEnum(adt)) {
                valueIndex->setAdtDefinition(*adt, nodeLevel, i);
                auto unpackLevel = addOperatorLevel(adt);

                if (context.isADTBranchSimple(adt)) {
                    std::vector<ast::Argument*> arguments;
                    auto dummyArg = mk<ast::UnnamedVariable>();
                    arguments.push_back(dummyArg.get());
                    for (auto* arg : adt->getArguments()) {
                        arguments.push_back(arg);
                    }
                    indexNodeArguments(unpackLevel, arguments);
                } else {
                    auto argumentUnpackLevel = addOperatorLevel(adt);
                    indexNodeArguments(argumentUnpackLevel, adt->getArguments());
                }
            }
        }
    }
}

void ClauseTranslator::indexGenerator(const ast::Argument& arg) {
    std::size_t aggLoc = addGeneratorLevel(&arg);
    valueIndex->setGeneratorLoc(arg, Location({aggLoc, 0}));
}

void ClauseTranslator::indexAtoms(const ast::Clause& clause) {
    for (const auto* atom : getAtomOrdering(clause)) {
        // give the atom the current level
        std::size_t scanLevel = addOperatorLevel(atom);
        indexNodeArguments(scanLevel, atom->getArguments());
    }
}

void ClauseTranslator::indexAggregatorBody(const ast::Aggregator& agg) {
    auto aggLoc = valueIndex->getGeneratorLoc(agg);

    // Get the single body atom inside the aggregator
    const auto& aggBodyAtoms =
            filter(agg.getBodyLiterals(), [&](const ast::Literal* lit) { return isA<ast::Atom>(lit); });
    assert(aggBodyAtoms.size() == 1 && "exactly one atom should exist per aggregator body");
    const auto* aggAtom = static_cast<const ast::Atom*>(aggBodyAtoms.at(0));

    // Add the variable references inside this atom
    const auto& aggAtomArgs = aggAtom->getArguments();
    for (std::size_t i = 0; i < aggAtomArgs.size(); i++) {
        const auto* arg = aggAtomArgs.at(i);
        if (const auto* var = as<ast::Variable>(arg)) {
            valueIndex->addVarReference(var->getName(), aggLoc.identifier, i);
        }
    }
}

void ClauseTranslator::indexAggregators(const ast::Clause& clause) {
    // Index aggregator bodies
    visit(clause, [&](const ast::Aggregator& agg) { indexAggregatorBody(agg); });

    // Add aggregator value introductions
    visit(clause, [&](const ast::BinaryConstraint& bc) {
        if (!isEqConstraint(bc.getBaseOperator())) return;
        const auto* lhs = as<ast::Variable>(bc.getLHS());
        const auto* rhs = as<ast::Aggregator>(bc.getRHS());
        if (lhs == nullptr || rhs == nullptr) return;
        valueIndex->addVarReference(lhs->getName(), valueIndex->getGeneratorLoc(*rhs));
    });
}

void ClauseTranslator::indexMultiResultFunctors(const ast::Clause& clause) {
    // Add multi-result functor value introductions
    visit(clause, [&](const ast::BinaryConstraint& bc) {
        if (!isEqConstraint(bc.getBaseOperator())) return;
        const auto* lhs = as<ast::Variable>(bc.getLHS());
        const auto* rhs = as<ast::IntrinsicFunctor>(bc.getRHS());
        if (lhs == nullptr || rhs == nullptr) return;
        if (!ast::analysis::FunctorAnalysis::isMultiResult(*rhs)) return;
        valueIndex->addVarReference(lhs->getName(), valueIndex->getGeneratorLoc(*rhs));
    });
}

void ClauseTranslator::indexGenerators(const ast::Clause& clause) {
    // generators must be indexed in topological order according to
    // dependencies between them.
    // see issue #2416

    std::map<std::string, const ast::Argument*> varGenerator;

    visit(clause, [&](const ast::BinaryConstraint& bc) {
        if (!isEqConstraint(bc.getBaseOperator())) return;
        const auto* lhs = as<ast::Variable>(bc.getLHS());
        const ast::Argument* rhs = as<ast::IntrinsicFunctor>(bc.getRHS());
        if (rhs == nullptr) {
            rhs = as<ast::Aggregator>(bc.getRHS());
        } else {
            if (!ast::analysis::FunctorAnalysis::isMultiResult(*as<ast::IntrinsicFunctor>(rhs))) return;
        }
        if (lhs == nullptr || rhs == nullptr) return;
        varGenerator[lhs->getName()] = rhs;
    });

    // all the generators in the clause
    std::vector<const ast::Argument*> generators;

    // 'predecessor' mapping from a generator to the generators that must
    // evaluate before.
    std::multimap<const ast::Argument*, const ast::Argument*> dependencies;

    // harvest generators and dependencies
    visit(clause, [&](const ast::Argument& arg) {
        if (const ast::IntrinsicFunctor* func = as<ast::IntrinsicFunctor>(arg)) {
            if (ast::analysis::FunctorAnalysis::isMultiResult(*func)) {
                generators.emplace_back(func);
                visit(func, [&](const ast::Variable& use) {
                    if (varGenerator.count(use.getName()) > 0) {
                        dependencies.emplace(func, varGenerator.at(use.getName()));
                    }
                });
            }
        } else if (const ast::Aggregator* agg = as<ast::Aggregator>(arg)) {
            generators.emplace_back(agg);
            visit(agg, [&](const ast::Variable& use) {
                if (varGenerator.count(use.getName()) > 0) {
                    dependencies.emplace(agg, varGenerator.at(use.getName()));
                }
            });
        }
    });

    // the set of already indexed generators
    std::set<const ast::Argument*> indexed;
    // the recursion stack to detect a cycle in the depth-first traversal
    std::set<const ast::Argument*> recStack;

    // recursive depth-first traversal, perform a post-order indexing of genertors.
    const std::function<void(const ast::Argument*)> dfs = [&](const ast::Argument* reached) {
        if (indexed.count(reached)) {
            return;
        }

        if (!recStack.emplace(reached).second) {
            // cycle detected
            fatal("cyclic dependency");
        }

        auto range = dependencies.equal_range(reached);
        for (auto it = range.first; it != range.second; ++it) {
            if (it->second == reached) {
                continue;  // ignore self-dependency
            }
            dfs(it->second);
        }

        // index this generator
        indexGenerator(*reached);

        indexed.insert(reached);
        recStack.erase(reached);
    };

    // topological sorting by depth-first search
    for (const ast::Argument* root : generators) {
        dfs(root);
    }
}

void ClauseTranslator::indexClause(const ast::Clause& clause) {
    indexAtoms(clause);
    indexGenerators(clause);
    indexAggregators(clause);
    indexMultiResultFunctors(clause);
}

}  // namespace souffle::ast2ram::seminaive
