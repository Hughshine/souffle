/*
 * Souffle - A Datalog Compiler
 * Copyright (c) 2013, 2015, Oracle and/or its affiliates. All rights reserved
 * Licensed under the Universal Permissive License v 1.0 as shown at:
 * - https://opensource.org/licenses/UPL
 * - <souffle root>/licenses/SOUFFLE-UPL.txt
 */

/************************************************************************
 *
 * @file UnitTranslator.cpp
 *
 ***********************************************************************/

#include "ast2ram/incremental/UnitTranslator.h"
#include "Global.h"
#include "LogStatement.h"
#include "ast/Clause.h"
#include "ast/Directive.h"
#include "ast/Relation.h"
#include "ast/SubsumptiveClause.h"
#include "ast/TranslationUnit.h"
#include "ast/UserDefinedFunctor.h"
#include "ast/analysis/TopologicallySortedSCCGraph.h"
#include "ast/utility/Utils.h"
#include "ast/utility/Visitor.h"
#include "ast2ram/ClauseTranslator.h"
#include "ast2ram/utility/TranslatorContext.h"
#include "ast2ram/utility/Utils.h"
#include "ram/Aggregate.h"
#include "ram/Assign.h"
#include "ram/Call.h"
#include "ram/Clear.h"
#include "ram/Condition.h"
#include "ram/Conjunction.h"
#include "ram/Constraint.h"
#include "ram/DebugInfo.h"
#include "ram/DeltaUnion.h"
#include "ram/EmptinessCheck.h"
#include "ram/Erase.h"
#include "ram/ExistenceCheck.h"
#include "ram/Exit.h"
#include "ram/Expression.h"
#include "ram/Filter.h"
#include "ram/IO.h"
#include "ram/Insert.h"
#include "ram/IntrinsicOperator.h"
#include "ram/LogRelationTimer.h"
#include "ram/LogSize.h"
#include "ram/LogTimer.h"
#include "ram/Loop.h"
#include "ram/MergeExtend.h"
#include "ram/Negation.h"
#include "ram/Parallel.h"
#include "ram/Program.h"
#include "ram/Query.h"
#include "ram/Relation.h"
#include "ram/RelationSize.h"
#include "ram/Scan.h"
#include "ram/Sequence.h"
#include "ram/SignedConstant.h"
#include "ram/Statement.h"
#include "ram/Swap.h"
#include "ram/TranslationUnit.h"
#include "ram/TupleElement.h"
#include "ram/UndefValue.h"
#include "ram/UnsignedConstant.h"
#include "ram/UserDefinedAggregator.h"
#include "ram/UserDefinedOperator.h"
#include "ram/Variable.h"
#include "ram/utility/Utils.h"
#include "reports/DebugReport.h"
#include "reports/ErrorReport.h"
#include "souffle/BinaryConstraintOps.h"
#include "souffle/TypeAttribute.h"
#include "souffle/utility/ContainerUtil.h"
#include "souffle/utility/FunctionalUtil.h"
#include "souffle/utility/MiscUtil.h"
#include "souffle/utility/StringUtil.h"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace souffle::ast2ram::incremental {

UnitTranslator::UnitTranslator() : ast2ram::UnitTranslator() {}

UnitTranslator::~UnitTranslator() = default;

void UnitTranslator::addRamSubroutine(std::string subroutineID, Own<ram::Statement> subroutine) {
    assert(!contains(ramSubroutines, subroutineID) && "subroutine ID should not already exist");
    ramSubroutines[subroutineID] = std::move(subroutine);
}

Own<ram::Statement> UnitTranslator::generateClearRelation(const ast::Relation* relation) const {
    return mk<ram::Clear>(getConcreteRelationName(relation->getQualifiedName()));
}
// TODO: fix copy paste
Own<ram::Statement> UnitTranslator::generateNonRecursiveRelationDel(const ast::Relation& rel) const {
    VecOwn<ram::Statement> result;

    // Get relation names
    std::string mainRelation = getConcreteRelationName(rel.getQualifiedName());

    // Iterate over all non-recursive clauses that belong to the relation
    for (auto&& clause : context->getProgram()->getClauses(rel)) {
        // Skip recursive and subsumptive clauses
        if (context->isRecursiveClause(clause) || isA<ast::SubsumptiveClause>(clause)) {
            continue;
        }

        // Translate clause
        TranslationMode mode = rel.getAuxiliaryArity() > 0 ? Auxiliary : Incremental;
        Own<ram::Statement> rule = context->translateNonRecursiveClauseDel(*clause, mode);  // TODO

        // Add logging
        if (glb->config().has("profile")) {
            const std::string& relationName = toString(rel.getQualifiedName());
            const auto& srcLocation = clause->getSrcLoc();
            const std::string clauseText = stringify(toString(*clause));
            const std::string logTimerStatement =
                    LogStatement::tNonrecursiveRule(relationName, srcLocation, clauseText);
            rule = mk<ram::LogRelationTimer>(std::move(rule), logTimerStatement, mainRelation);
        }

        // Add debug info
        std::ostringstream ds;
        clause->printForDebugInfo(ds);
        ds << "\nin file ";
        ds << clause->getSrcLoc();
        rule = mk<ram::DebugInfo>(std::move(rule), ds.str());

        // Add rule to result
        appendStmt(result, std::move(rule));
    }

    if (context->getProgram()->getClauses(rel).size() > 0) {
        auto headRelationName = getConcreteRelationName(rel.getQualifiedName());
        auto headOldRelationName = getOldRelationName(rel.getQualifiedName());
        auto headDeltaDervDeleteRelationName = getIncDeltaDervDeleteRelationName(rel.getQualifiedName());
        auto headDeltaTupleDeleteRelationName = getIncDeltaTupleDeleteRelationName(rel.getQualifiedName());

        appendStmt(result, mk<ram::DeltaUnion>(headRelationName,
            "", headRelationName,
            "", headDeltaDervDeleteRelationName,
            "", headDeltaTupleDeleteRelationName));
    }

    // Add logging for entire relation
    if (glb->config().has("profile")) {
        const std::string& relationName = toString(rel.getQualifiedName());
        const auto& srcLocation = rel.getSrcLoc();
        const std::string logSizeStatement = LogStatement::nNonrecursiveRelation(relationName, srcLocation);

        // Add timer if we did any work
        if (!result.empty()) {
            const std::string logTimerStatement =
                    LogStatement::tNonrecursiveRelation(relationName, srcLocation);
            auto newStmt = mk<ram::LogRelationTimer>(
                    mk<ram::Sequence>(std::move(result)), logTimerStatement, mainRelation);
            result.clear();
            appendStmt(result, std::move(newStmt));
        } else {
            // Add table size printer
            appendStmt(result, mk<ram::LogSize>(mainRelation, logSizeStatement));
        }
    }

    return mk<ram::Sequence>(std::move(result));
}
Own<ram::Statement> UnitTranslator::generateNonRecursiveRelationIns(const ast::Relation& rel) const {
    VecOwn<ram::Statement> result;

    // Get relation names
    std::string mainRelation = getConcreteRelationName(rel.getQualifiedName());

    // Iterate over all non-recursive clauses that belong to the relation
    for (auto&& clause : context->getProgram()->getClauses(rel)) {
        // Skip recursive and subsumptive clauses
        if (context->isRecursiveClause(clause) || isA<ast::SubsumptiveClause>(clause)) {
            continue;
        }

        // Translate clause
        TranslationMode mode = rel.getAuxiliaryArity() > 0 ? Auxiliary : Incremental;
        Own<ram::Statement> rule = context->translateNonRecursiveClauseIns(*clause, mode);  // TODO

        // Add logging
        if (glb->config().has("profile")) {
            const std::string& relationName = toString(rel.getQualifiedName());
            const auto& srcLocation = clause->getSrcLoc();
            const std::string clauseText = stringify(toString(*clause));
            const std::string logTimerStatement =
                    LogStatement::tNonrecursiveRule(relationName, srcLocation, clauseText);
            rule = mk<ram::LogRelationTimer>(std::move(rule), logTimerStatement, mainRelation);
        }

        // Add debug info
        std::ostringstream ds;
        clause->printForDebugInfo(ds);
        ds << "\nin file ";
        ds << clause->getSrcLoc();
        rule = mk<ram::DebugInfo>(std::move(rule), ds.str());

        // Add rule to result
        appendStmt(result, std::move(rule));
    }

    // TODO: perhaps we could keep delta union for insertion, because it's monotonic
    if (context->getProgram()->getClauses(rel).size() > 0) {
        auto headRelationName = getConcreteRelationName(rel.getQualifiedName());
        auto headOldRelationName = getOldRelationName(rel.getQualifiedName());
        auto headDeltaDervInsertRelationName = getIncDeltaDervInsertRelationName(rel.getQualifiedName());
        auto headDeltaTupleInsertRelationName = getIncDeltaTupleInsertRelationName(rel.getQualifiedName());

        appendStmt(result, mk<ram::DeltaUnion>(headRelationName,
            "", headRelationName,
            headDeltaDervInsertRelationName, "",
            headDeltaTupleInsertRelationName, ""));
    }

    // Add logging for entire relation
    if (glb->config().has("profile")) {
        const std::string& relationName = toString(rel.getQualifiedName());
        const auto& srcLocation = rel.getSrcLoc();
        const std::string logSizeStatement = LogStatement::nNonrecursiveRelation(relationName, srcLocation);

        // Add timer if we did any work
        if (!result.empty()) {
            const std::string logTimerStatement =
                    LogStatement::tNonrecursiveRelation(relationName, srcLocation);
            auto newStmt = mk<ram::LogRelationTimer>(
                    mk<ram::Sequence>(std::move(result)), logTimerStatement, mainRelation);
            result.clear();
            appendStmt(result, std::move(newStmt));
        } else {
            // Add table size printer
            appendStmt(result, mk<ram::LogSize>(mainRelation, logSizeStatement));
        }
    }

    return mk<ram::Sequence>(std::move(result));
}

Own<ram::Statement> UnitTranslator::generateNonRecursiveRelation(const ast::Relation& rel) const {
    VecOwn<ram::Statement> result;

    // Get relation names
    std::string mainRelation = getConcreteRelationName(rel.getQualifiedName());

    // Iterate over all non-recursive clauses that belong to the relation
    for (auto&& clause : context->getProgram()->getClauses(rel)) {
        // Skip recursive and subsumptive clauses
        if (context->isRecursiveClause(clause) || isA<ast::SubsumptiveClause>(clause)) {
            continue;
        }

        // Translate clause
        TranslationMode mode = rel.getAuxiliaryArity() > 0 ? Auxiliary : Incremental;
        Own<ram::Statement> rule = context->translateNonRecursiveClause(*clause, mode);

        // Add logging
        if (glb->config().has("profile")) {
            const std::string& relationName = toString(rel.getQualifiedName());
            const auto& srcLocation = clause->getSrcLoc();
            const std::string clauseText = stringify(toString(*clause));
            const std::string logTimerStatement =
                    LogStatement::tNonrecursiveRule(relationName, srcLocation, clauseText);
            rule = mk<ram::LogRelationTimer>(std::move(rule), logTimerStatement, mainRelation);
        }

        // Add debug info
        std::ostringstream ds;
        clause->printForDebugInfo(ds);
        ds << "\nin file ";
        ds << clause->getSrcLoc();
        rule = mk<ram::DebugInfo>(std::move(rule), ds.str());

        // Add rule to result
        appendStmt(result, std::move(rule));
    }

    if (context->getProgram()->getClauses(rel).size() > 0) {
        auto headRelationName = getConcreteRelationName(rel.getQualifiedName());
        auto headOldRelationName = getOldRelationName(rel.getQualifiedName());
        auto headDeltaDervInsertRelationName = getIncDeltaDervInsertRelationName(rel.getQualifiedName());
        auto headDeltaDervDeleteRelationName = getIncDeltaDervDeleteRelationName(rel.getQualifiedName());
        auto headDeltaTupleInsertRelationName = getIncDeltaTupleInsertRelationName(rel.getQualifiedName());
        auto headDeltaTupleDeleteRelationName = getIncDeltaTupleDeleteRelationName(rel.getQualifiedName());

        appendStmt(result, mk<ram::DeltaUnion>(headRelationName,
            headOldRelationName, headRelationName,
            headDeltaDervInsertRelationName, headDeltaDervDeleteRelationName,
            headDeltaTupleInsertRelationName, headDeltaTupleDeleteRelationName));
    }

    // Add logging for entire relation
    if (glb->config().has("profile")) {
        const std::string& relationName = toString(rel.getQualifiedName());
        const auto& srcLocation = rel.getSrcLoc();
        const std::string logSizeStatement = LogStatement::nNonrecursiveRelation(relationName, srcLocation);

        // Add timer if we did any work
        if (!result.empty()) {
            const std::string logTimerStatement =
                    LogStatement::tNonrecursiveRelation(relationName, srcLocation);
            auto newStmt = mk<ram::LogRelationTimer>(
                    mk<ram::Sequence>(std::move(result)), logTimerStatement, mainRelation);
            result.clear();
            appendStmt(result, std::move(newStmt));
        } else {
            // Add table size printer
            appendStmt(result, mk<ram::LogSize>(mainRelation, logSizeStatement));
        }
    }

    return mk<ram::Sequence>(std::move(result));
}

Own<ram::Statement> UnitTranslator::generateStratum(std::size_t scc) const {
    // Make a new ram statement for the current SCC
    VecOwn<ram::Statement> current;

    // Load all internal input relations from the facts dir with a .facts extension
    // INC: Also load all delta changes for IDB by files having .facts.insert and .facts.delete extension
    // TODO: here we suppose EDB does not change during fixpoint computation
    // TODO: i.e. they are not the heads of any rules
    for (const auto& relation : context->getInputRelationsInSCC(scc)) {
        appendStmt(current, generateLoadRelation(relation));
    }

    // Load all cached external output relations from the output dir
    // Cached derivation info maintained separately
    for (const auto& relation : context->getOutputRelationsInSCC(scc)) {
        appendStmt(current, generateLoadRelationForIDB(relation));
    }

    // Compute the current stratum
    const auto& sccRelations = context->getRelationsInSCC(scc);
    if (context->isRecursiveSCC(scc)) {
        appendStmt(current, generateRecursiveStratum(sccRelations, scc));
        // assert(false && "recursion not supported");
    } else {
        assert(sccRelations.size() == 1 && "only one relation should exist in non-recursive stratum");
        const auto* rel = *sccRelations.begin();
        appendStmt(current, generateNonRecursiveRelation(*rel));  // TODO

        // lub auxiliary arities using the @lub relation
        if (rel->getAuxiliaryArity() > 0) {
            assert (false && "lub auxiliary not supported");
        }

        if (context->hasSubsumptiveClause(rel->getQualifiedName())) {
            assert (false && "subsumption clause not supported");
        }
    }

    // Get all non-recursive relation statements, TODO
    const auto nonRecursiveJoinSizeStatements = context->getNonRecursiveJoinSizeStatementsInSCC(scc);
    assert(nonRecursiveJoinSizeStatements.empty() && "does not support join size statement currently");
    // auto joinSizeSequence = mk<ram::Sequence>(std::move(nonRecursiveJoinSizeStatements));
    // appendStmt(current, std::move(joinSizeSequence));

    // Store all internal output relations to the output dir with a .csv extension
    // Note: delta derivations will be dumped after all calculations
    // Note: do we also need to provide delta EDB? It seems this is already enough
    for (const auto& relation : context->getOutputRelationsInSCC(scc)) {
        appendStmt(current, generateStoreRelation(relation));
    }

    return mk<ram::Sequence>(std::move(current));
}

Own<ram::Statement> UnitTranslator::generateClearExpiredRelations(
        const ast::RelationSet& expiredRelations) const {
    VecOwn<ram::Statement> stmts;
    for (const auto& relation : expiredRelations) {
        appendStmt(stmts, generateClearRelation(relation));
    }
    return mk<ram::Sequence>(std::move(stmts));
}

Own<ram::Statement> UnitTranslator::generateEraseTuples(
        const ast::Relation* rel, const std::string& destRelation, const std::string& srcRelation) const {
    VecOwn<ram::Expression> values;
    for (std::size_t i = 0; i < rel->getArity(); i++) {
        values.push_back(mk<ram::TupleElement>(0, i));
    }
    auto insertion = mk<ram::Erase>(destRelation, std::move(values));
    return mk<ram::Query>(mk<ram::Scan>(srcRelation, 0, std::move(insertion)));
}

Own<ram::Statement> UnitTranslator::generateMergeRelationsWithFilter(const ast::Relation* rel,
        const std::string& destRelation, const std::string& srcRelation,
        const std::string& filterRelation) const {
    VecOwn<ram::Expression> values;
    VecOwn<ram::Expression> values2;

    // Proposition - insert if not empty
    if (rel->getArity() == 0) {
        auto insertion = mk<ram::Insert>(destRelation, std::move(values));
        return mk<ram::Query>(mk<ram::Filter>(
                mk<ram::Negation>(mk<ram::EmptinessCheck>(srcRelation)), std::move(insertion)));
    }

    // Predicate - insert all values
    for (std::size_t i = 0; i < rel->getArity(); i++) {
        values.push_back(mk<ram::TupleElement>(0, i));
        values2.push_back(mk<ram::TupleElement>(0, i));
    }
    auto insertion = mk<ram::Insert>(destRelation, std::move(values));
    auto filtered =
            mk<ram::Filter>(mk<ram::Negation>(mk<ram::ExistenceCheck>(filterRelation, std::move(values2))),
                    std::move(insertion));
    auto stmt = mk<ram::Query>(mk<ram::Scan>(srcRelation, 0, std::move(filtered)));

    if (rel->getRepresentation() == RelationRepresentation::EQREL) {
        return mk<ram::Sequence>(mk<ram::MergeExtend>(destRelation, srcRelation), std::move(stmt));
    }
    return stmt;
}

Own<ram::Statement> UnitTranslator::generateMergeRelationsWithNegativeFilter(const ast::Relation* rel,
        const std::string& destRelation, const std::string& srcRelation,
        const std::string& filterRelation) const {
    VecOwn<ram::Expression> values;
    VecOwn<ram::Expression> values2;

    // Proposition - insert if not empty
    if (rel->getArity() == 0) {
        auto insertion = mk<ram::Insert>(destRelation, std::move(values));
        return mk<ram::Query>(mk<ram::Filter>(
                (mk<ram::EmptinessCheck>(srcRelation)), std::move(insertion)));
    }

    // Predicate - insert all values
    for (std::size_t i = 0; i < rel->getArity(); i++) {
        values.push_back(mk<ram::TupleElement>(0, i));
        values2.push_back(mk<ram::TupleElement>(0, i));
    }
    auto insertion = mk<ram::Insert>(destRelation, std::move(values));
    auto filtered =
            mk<ram::Filter>(mk<ram::ExistenceCheck>(filterRelation, std::move(values2)),
                    std::move(insertion));
    auto stmt = mk<ram::Query>(mk<ram::Scan>(srcRelation, 0, std::move(filtered)));

    if (rel->getRepresentation() == RelationRepresentation::EQREL) {
        return mk<ram::Sequence>(mk<ram::MergeExtend>(destRelation, srcRelation), std::move(stmt));
    }
    return stmt;
}

Own<ram::Statement> UnitTranslator::generateMergeRelations(
        const ast::Relation* rel, const std::string& destRelation, const std::string& srcRelation) const {
    VecOwn<ram::Expression> values;

    // Proposition - insert if not empty
    if (rel->getArity() == 0) {
        auto insertion = mk<ram::Insert>(destRelation, std::move(values));
        return mk<ram::Query>(mk<ram::Filter>(
                mk<ram::Negation>(mk<ram::EmptinessCheck>(srcRelation)), std::move(insertion)));
    }

    // Predicate - insert all values
    if (rel->getRepresentation() == RelationRepresentation::EQREL) {
        return mk<ram::MergeExtend>(destRelation, srcRelation);
    }
    for (std::size_t i = 0; i < rel->getArity(); i++) {
        values.push_back(mk<ram::TupleElement>(0, i));  // TupleElement对应tuple的具体元。这里的逻辑看起来有些冗余，不知道生成代码有没有影响，相当于拆除一个tuple又重建回去。和RAM的设计直接相关。
    }
    auto insertion = mk<ram::Insert>(destRelation, std::move(values));
    auto stmt = mk<ram::Query>(mk<ram::Scan>(srcRelation, 0, std::move(insertion)));
    return stmt;
}

Own<ram::Statement> UnitTranslator::generateDebugRelation(const ast::Relation* rel,
        const std::string& destRelation, const std::string& srcRelation,
        Own<ram::Expression> iteration) const {
    VecOwn<ram::Expression> values;

    for (std::size_t i = 0; i < rel->getArity(); i++) {
        values.push_back(mk<ram::TupleElement>(0, i));
    }

    values.push_back(std::move(iteration));

    // Proposition - insert if not empty
    if (rel->getArity() == 0) {
        auto insertion = mk<ram::Insert>(destRelation, std::move(values));
        return mk<ram::Query>(mk<ram::Filter>(
                mk<ram::Negation>(mk<ram::EmptinessCheck>(srcRelation)), std::move(insertion)));
    }

    auto insertion = mk<ram::Insert>(destRelation, std::move(values));
    auto stmt = mk<ram::Query>(mk<ram::Scan>(srcRelation, 0, std::move(insertion)));
    return stmt;
}

Own<ram::Statement> UnitTranslator::translateRecursiveClauses(
        const ast::RelationSet& scc, const ast::Relation* rel, bool isDelete, bool isPrefill) const {
    assert(contains(scc, rel) && "relation should belong to scc");
    VecOwn<ram::Statement> code;

    // Translate each recursive clasue
    for (auto&& clause : context->getProgram()->getClauses(*rel)) {
        // Skip non-recursive and subsumptive clauses
        if (!context->isRecursiveClause(clause) || isA<ast::SubsumptiveClause>(clause)) {
            continue;
        }

        // generate all delta versions of a recursive clause
        auto clauseVersions =
            isPrefill?
            generateClauseVersionsPrefill(clause, scc, isDelete)
            :generateClauseVersions(clause, scc, isDelete);
        for (auto& clauseVersion : clauseVersions) {
            appendStmt(code, std::move(clauseVersion));
        }
    }
    return mk<ram::Sequence>(std::move(code));
}

Own<ram::Statement> UnitTranslator::translateSubsumptiveRecursiveClauses(
        const ast::RelationSet& scc, const ast::Relation* rel) const {
    assert(contains(scc, rel) && "relation should belong to scc");

    VecOwn<ram::Statement> code;
    if (!context->hasSubsumptiveClause(rel->getQualifiedName())) {
        return mk<ram::Sequence>(std::move(code));
    }

    std::string mainRelation = getConcreteRelationName(rel->getQualifiedName());
    std::string newRelation = getNewRelationName(rel->getQualifiedName());
    std::string deltaRelation = getDeltaRelationName(rel->getQualifiedName());
    std::string rejectRelation = getRejectRelationName(rel->getQualifiedName());
    std::string deleteRelation = getDeleteRelationName(rel->getQualifiedName());

    // old delta relation can be cleared
    appendStmt(code, mk<ram::Clear>(deltaRelation));

    // compute reject set using the subsumptive clauses
    for (const auto* clause : context->getProgram()->getClauses(*rel)) {
        // Skip non-subsumptive clauses
        if (!isA<ast::SubsumptiveClause>(clause)) {
            continue;
        }

        const std::size_t version = 0;
        // find dominated tuples in the newR by tuples in newR  and store them in rejectR
        appendStmt(code, context->translateRecursiveClause(*clause, scc, version, SubsumeRejectNewNew));
        // find dominated tuples in the newR by tuples in R and store them in rejectR
        appendStmt(code, context->translateRecursiveClause(*clause, scc, version, SubsumeRejectNewCurrent));
    }

    // compute new delta set, i.e., deltaR = newR \ rejectR
    appendStmt(code, generateMergeRelationsWithFilter(rel, deltaRelation, newRelation, rejectRelation));
    appendStmt(code, mk<ram::Clear>(rejectRelation));
    appendStmt(code, mk<ram::Clear>(newRelation));

    // compute delete set,  remove tuples from R, and clear delete set
    for (const auto* clause : context->getProgram()->getClauses(*rel)) {
        // Skip non-subsumptive clauses
        if (!isA<ast::SubsumptiveClause>(clause)) {
            continue;
        }

        const auto& sccAtoms = getSccAtoms(clause, scc);
        std::size_t sz = sccAtoms.size();
        for (std::size_t version = 0; version < sz; version++) {
            appendStmt(
                    code, context->translateRecursiveClause(*clause, scc, version,
                                  (version >= 1) ? SubsumeDeleteCurrentCurrent : SubsumeDeleteCurrentDelta));
        }
        appendStmt(code, generateEraseTuples(rel, mainRelation, deleteRelation));
        appendStmt(code, mk<ram::Clear>(deleteRelation));
    }

    return mk<ram::Sequence>(std::move(code));
}

std::vector<ast::Atom*> UnitTranslator::getSccAtoms(
        const ast::Clause* clause, const ast::RelationSet& scc) const {
    const auto& sccAtoms = filter(ast::getBodyLiterals<ast::Atom>(*clause), [&](const ast::Atom* atom) {
        if (isA<ast::SubsumptiveClause>(clause)) {
            const auto& body = clause->getBodyLiterals();
            // skip dominated head
            auto dominatedHeadAtom = dynamic_cast<const ast::Atom*>(body[0]);
            if (atom == dominatedHeadAtom) return false;
        }
        return contains(scc, context->getProgram()->getRelation(*atom));
    });
    return sccAtoms;
}

VecOwn<ram::Statement> UnitTranslator::generateClauseVersions(
        const ast::Clause* clause, const ast::RelationSet& scc, bool isDelete) const {
    const auto& sccAtoms = getSccAtoms(clause, scc);

    // Create each version
    VecOwn<ram::Statement> clauseVersions;
    for (std::size_t version = 0; version < sccAtoms.size(); version++) {
        // we have to have two loops for deletion and insertion in INC setting
        // which means we have to translate recursive clause separately
        // TODO: current interface is not enough to express that
        appendStmt(clauseVersions, context->translateRecursiveClause(*clause, scc, version, Incremental, isDelete));
        // for (auto& clauseVersion: clauseVersions) {
        //     clauseVersion->print(std::cout); // TODO: debug purpose
        // }
    }

    // Check that the correct number of versions have been created
    if (clause->getExecutionPlan() != nullptr) {
        std::optional<std::size_t> maxVersion;
        for (const auto& cur : clause->getExecutionPlan()->getOrders()) {
            maxVersion = std::max(cur.first, maxVersion.value_or(cur.first));
        }
        assert(sccAtoms.size() > *maxVersion && "missing clause versions");
    }

    return clauseVersions;
}

VecOwn<ram::Statement> UnitTranslator::generateClauseVersionsPrefill(
        const ast::Clause* clause, const ast::RelationSet& scc, bool isDelete) const {
    const auto& sccAtoms = getSccAtoms(clause, scc);

    // Create each version
    VecOwn<ram::Statement> clauseVersions;
    for (std::size_t version = 0; version < sccAtoms.size(); version++) {
        // we have to have two loops for deletion and insertion in INC setting
        // which means we have to translate recursive clause separately
        // TODO: current interface is not enough to express that
        appendStmt(clauseVersions, context->translateRecursiveClause(*clause, scc, version, Incremental, isDelete, true));
        // for (auto& clauseVersion: clauseVersions) {
        //     clauseVersion->print(std::cout); // TODO: debug purpose
        // }
    }

    // Check that the correct number of versions have been created
    if (clause->getExecutionPlan() != nullptr) {
        std::optional<std::size_t> maxVersion;
        for (const auto& cur : clause->getExecutionPlan()->getOrders()) {
            maxVersion = std::max(cur.first, maxVersion.value_or(cur.first));
        }
        assert(sccAtoms.size() > *maxVersion && "missing clause versions");
    }

    return clauseVersions;
}


Own<ram::Statement> UnitTranslator::generateNonRecursiveDelete(const ast::Relation& rel) const {
    VecOwn<ram::Statement> code;

    // Generate code for non-recursive subsumption
    if (!context->hasSubsumptiveClause(rel.getQualifiedName())) {
        return mk<ram::Sequence>(std::move(code));
    }

    std::string mainRelation = getConcreteRelationName(rel.getQualifiedName());
    std::string deleteRelation = getDeleteRelationName(rel.getQualifiedName());

    // Compute subsumptive deletions for non-recursive rules
    for (auto clause : context->getProgram()->getClauses(rel)) {
        if (!isA<ast::SubsumptiveClause>(clause)) {
            continue;
        }

        // Translate subsumptive clause
        Own<ram::Statement> rule = context->translateNonRecursiveClause(*clause, SubsumeDeleteCurrentCurrent);

        // Add logging for subsumptive clause
        if (glb->config().has("profile")) {
            const std::string& relationName = toString(rel.getQualifiedName());
            const auto& srcLocation = clause->getSrcLoc();
            const std::string clauseText = stringify(toString(*clause));
            const std::string logTimerStatement =
                    LogStatement::tNonrecursiveRule(relationName, srcLocation, clauseText);
            rule = mk<ram::LogRelationTimer>(std::move(rule), logTimerStatement, mainRelation);
        }

        // Add debug info for subsumptive clause
        std::ostringstream ds;
        ds << toString(*clause) << "\nin file ";
        ds << clause->getSrcLoc();
        rule = mk<ram::DebugInfo>(std::move(rule), ds.str());

        // Add subsumptive rule to result
        appendStmt(code, std::move(rule));
    }
    appendStmt(code, mk<ram::Sequence>(generateEraseTuples(&rel, mainRelation, deleteRelation),
                             mk<ram::Clear>(deleteRelation)));
    return mk<ram::Sequence>(std::move(code));
}

Own<ram::Statement> UnitTranslator::generateStratumPreamble(const ast::RelationSet& scc, bool isDelete) const {
    VecOwn<ram::Statement> preamble;


    for (const ast::Relation* rel : scc) {
        if (isDelete) {
            appendStmt(preamble, generateNonRecursiveRelationDel(*rel));
        }
        else {
            appendStmt(preamble, generateNonRecursiveRelationIns(*rel));
        }
    }

    // Generate code for priming relation
    // 初始化delta，将每个relation已有的都变成delta. 它们是可能不为空的，因为可能具有non recursive rules，再前面会先处理掉.
    // INC: initialize delta_deletion, delta_insertion
    for (const ast::Relation* rel : scc) {
        std::string deltaDelRelation = getDeltaDeletionRelationName(rel->getQualifiedName());
        std::string deltaInsRelation = getDeltaInsertionRelationName(rel->getQualifiedName());
        std::string deltaTupleDelRelation = getIncDeltaTupleDeleteRelationName(rel->getQualifiedName());
        std::string deltaTupleInsRelation = getIncDeltaTupleInsertRelationName(rel->getQualifiedName());
        if (isDelete) {
            appendStmt(preamble, generateMergeRelations(rel, deltaDelRelation, deltaTupleDelRelation));
        } else {
            appendStmt(preamble, generateMergeRelations(rel, deltaInsRelation, deltaTupleInsRelation));
        }
    }

    // TODO: 不知道debug relation是什么
    for (const ast::Relation* rel : scc) {
        if (const auto* debugRel = context->getDeltaDebugRelation(rel)) {
            const std::string debugRelation = getConcreteRelationName(debugRel->getQualifiedName());
            std::string deltaRelation = getDeltaRelationName(rel->getQualifiedName());
            appendStmt(preamble,
                    generateDebugRelation(rel, debugRelation, deltaRelation, mk<ram::UnsignedConstant>(0)));
        }
    }
    return mk<ram::Sequence>(std::move(preamble));
}

Own<ram::Statement> UnitTranslator::generateStratumPostamble(const ast::RelationSet& scc, bool isDelete) const {
    VecOwn<ram::Statement> postamble;

    for (const ast::Relation* rel : scc) {
        // Drop temporary tables after recursion
        if (isDelete) {
            appendStmt(postamble, mk<ram::Clear>(getDeltaDeletionRelationName(rel->getQualifiedName())));
            appendStmt(postamble, mk<ram::Clear>(getNewDeletionRelationName(rel->getQualifiedName())));
        } else {
            appendStmt(postamble, mk<ram::Clear>(getDeltaInsertionRelationName(rel->getQualifiedName())));
            appendStmt(postamble, mk<ram::Clear>(getNewInsertionRelationName(rel->getQualifiedName())));
        }
    }
    return mk<ram::Sequence>(std::move(postamble));
}

Own<ram::Statement> UnitTranslator::generateStratumTableUpdates(const ast::RelationSet& scc, bool isDelete) const {
    VecOwn<ram::Statement> updateTable;

    for (const ast::Relation* rel : scc) {
        // Copy @new into main relation, @delta := @new, and empty out @new
        std::string mainRelation = getConcreteRelationName(rel->getQualifiedName());
        std::string newRelation = getNewRelationName(rel->getQualifiedName());
        // std::string deltaRelation = getDeltaRelationName(rel->getQualifiedName());
        // relation, oldRel, newRel,
                    // deltaDervInsertRel, deltaDervDeleteRel, deltaTupleInsertRel, deltaTupleDeleteRel
        // swap new and and delta relation and clear new relation afterwards (if not a subsumptive relation)
        Own<ram::Statement> updateRelTable;
        if (rel->getAuxiliaryArity() > 0) {
            assert ( false && "no auxiliary arity");
        } else if (!context->hasSubsumptiveClause(rel->getQualifiedName())) {
            // TODO: DeltaUnion
            if (isDelete) {
                updateRelTable = mk<ram::Sequence>(
                mk<ram::Clear>(getDeltaDeletionRelationName(rel->getQualifiedName())),
                    mk<ram::DeltaUnion>(mainRelation, "", mainRelation,
                        "", getNewDeletionRelationName(rel->getQualifiedName()), "", getDeltaDeletionRelationName(rel->getQualifiedName())),
                        // TODO: 需要把delta同时插入到inc_delta_tuple里；rec的delta和inc的delta不同
                        generateMergeRelations(rel, getIncDeltaTupleDeleteRelationName(rel->getQualifiedName()), getDeltaDeletionRelationName(rel->getQualifiedName())),
                    mk<ram::Clear>(getNewDeletionRelationName(rel->getQualifiedName())));
            } else {
                updateRelTable = mk<ram::Sequence>(
                    mk<ram::Clear>(getDeltaInsertionRelationName(rel->getQualifiedName())), // clear old delta, use delta union to update it with new
                    mk<ram::DeltaUnion>(mainRelation, "", mainRelation,
                        getNewInsertionRelationName(rel->getQualifiedName()), "", getDeltaInsertionRelationName(rel->getQualifiedName()), ""),
                        generateMergeRelations(rel, getIncDeltaTupleInsertRelationName(rel->getQualifiedName()), getDeltaInsertionRelationName(rel->getQualifiedName())),
                        mk<ram::Clear>(getNewInsertionRelationName(rel->getQualifiedName())));
            }
        } else {
            assert (false && "no subsumptive clause");
        }

        // Measure update time
        if (glb->config().has("profile")) {
            updateRelTable = mk<ram::LogRelationTimer>(std::move(updateRelTable),
                    LogStatement::cRecursiveRelation(toString(rel->getQualifiedName()), rel->getSrcLoc()),
                    newRelation);
        }

        appendStmt(updateTable, std::move(updateRelTable));
    }
    return mk<ram::Sequence>(std::move(updateTable));
}

Own<ram::Statement> UnitTranslator::generateStratumLoopBody(const ast::RelationSet& scc, bool isDelete) const {
    VecOwn<ram::Statement> loopBody;

    // first translate regular recursive clauses
    // INC: deletion
    if (isDelete) {
        for (const ast::Relation* rel : scc) {
            auto relClauses = translateRecursiveClauses(scc, rel, true);
            appendStmt(loopBody, mk<ram::Sequence>(std::move(relClauses)));
        }
    } else {
        for (const ast::Relation* rel : scc) {
            auto relClauses = translateRecursiveClauses(scc, rel, false);
            appendStmt(loopBody, mk<ram::Sequence>(std::move(relClauses)));
        }
    }
    return mk<ram::Sequence>(std::move(loopBody));
    // INC: should derv info -> tuple info, delta union
    // INC: insertion

}

Own<ram::Statement> UnitTranslator::generateStratumNonSccPreFill(const ast::RelationSet& scc, bool isDelete) const {
    VecOwn<ram::Statement> prefill;

    // first translate regular recursive clauses
    // INC: deletion
    if (isDelete) {
        for (const ast::Relation* rel : scc) {
            auto relClauses = translateRecursiveClauses(scc, rel, true, true);
            appendStmt(prefill, mk<ram::Sequence>(std::move(relClauses)));
        }
    } else {
        for (const ast::Relation* rel : scc) {
            auto relClauses = translateRecursiveClauses(scc, rel, false, true);
            appendStmt(prefill, mk<ram::Sequence>(std::move(relClauses)));
        }
    }
    return mk<ram::Sequence>(std::move(prefill));
    // INC: should derv info -> tuple info, delta union
    // INC: insertion

}
/// assuming the @new() relation is populated with new tuples, generate RAM code
/// to populate the @delta() relation with the lubbed elements from @new()
Own<ram::Statement> UnitTranslator::generateStratumLubSequence(
        const ast::Relation& rel, bool inRecursiveLoop) const {
    VecOwn<ram::Statement> stmts;
    assert(rel.getAuxiliaryArity() > 0);

    auto attributes = rel.getAttributes();
    std::string name = getConcreteRelationName(rel.getQualifiedName());
    std::string lubName = getLubRelationName(rel.getQualifiedName());
    std::string newName = getNewRelationName(rel.getQualifiedName());

    const std::size_t arity = rel.getArity();
    const std::size_t auxiliaryArity = rel.getAuxiliaryArity();

    // Step 1 : populate @lub() from @new()
    VecOwn<ram::Expression> values;

    // index of the first auxiliary element of the relation
    std::size_t firstAuxiliary = arity - auxiliaryArity;

    for (std::size_t i = 0; i < arity; i++) {
        if (i >= firstAuxiliary) {
            values.push_back(mk<ram::TupleElement>(i - firstAuxiliary + 1, 0));
        } else {
            values.push_back(mk<ram::TupleElement>(0, i));
        }
    }
    Own<ram::Operation> op = mk<ram::Insert>(lubName, std::move(values));

    for (std::size_t i = arity; i >= firstAuxiliary + 1; i--) {
        const auto type = attributes[i - 1]->getTypeName();
        std::size_t level = i - firstAuxiliary;
        auto aggregator = context->getLatticeTypeLubAggregator(type, mk<ram::TupleElement>(0, i - 1));
        Own<ram::Condition> condition = mk<ram::Constraint>(
                BinaryConstraintOp::NE, mk<ram::TupleElement>(level, i - 1), mk<ram::TupleElement>(0, i - 1));
        for (std::size_t j = 0; j < attributes.size(); j++) {
            if (attributes[j]->getIsLattice()) break;
            condition = mk<ram::Conjunction>(std::move(condition),
                    mk<ram::Constraint>(BinaryConstraintOp::EQ, mk<ram::TupleElement>(level, j),
                            mk<ram::TupleElement>(0, j)));
        }
        op = mk<ram::Aggregate>(std::move(op), std::move(aggregator), newName,
                mk<ram::TupleElement>(level, i - 1), std::move(condition), level);
    }

    op = mk<ram::Scan>(newName, 0, std::move(op));
    appendStmt(stmts, mk<ram::Query>(std::move(op)));

    // clear @new() now that we no longer need it
    appendStmt(stmts, mk<ram::Clear>(newName));

    if (inRecursiveLoop) {
        // Step 2 : populate @delta() from @lub() for tuples that have to be lubbed with @concrete
        std::string deltaName = getDeltaRelationName(rel.getQualifiedName());

        Own<ram::Condition> condition;
        for (std::size_t i = 0; i < arity; i++) {
            if (i < firstAuxiliary) {
                values.push_back(mk<ram::TupleElement>(0, i));
            } else {
                assert(attributes[i]->getIsLattice());
                const auto type = attributes[i]->getTypeName();
                VecOwn<ram::Expression> args;
                args.push_back(mk<ram::TupleElement>(0, i));
                args.push_back(mk<ram::TupleElement>(1, i));
                auto lub = context->getLatticeTypeLubFunctor(type, std::move(args));
                auto cst =
                        mk<ram::Constraint>(BinaryConstraintOp::EQ, mk<ram::TupleElement>(1, i), clone(lub));
                if (condition) {
                    condition = mk<ram::Conjunction>(std::move(condition), std::move(cst));
                } else {
                    condition = std::move(cst);
                }
                values.push_back(std::move(lub));
            }
        }
        op = mk<ram::Insert>(deltaName, std::move(values));
        op = mk<ram::Filter>(mk<ram::Negation>(std::move(condition)), std::move(op));

        for (std::size_t i = 0; i < arity - auxiliaryArity; i++) {
            auto cst = mk<ram::Constraint>(
                    BinaryConstraintOp::EQ, mk<ram::TupleElement>(0, i), mk<ram::TupleElement>(1, i));
            if (condition) {
                condition = mk<ram::Conjunction>(std::move(condition), std::move(cst));
            } else {
                condition = std::move(cst);
            }
        }
        if (condition) {
            op = mk<ram::Filter>(std::move(condition), std::move(op));
        }
        op = mk<ram::Scan>(name, 1, std::move(op));
        op = mk<ram::Scan>(lubName, 0, std::move(op));
        appendStmt(stmts, mk<ram::Query>(std::move(op)));

        // Step 3 : populate @delta() from @lub() for tuples that have nothing to lub in @concrete
        for (std::size_t i = 0; i < arity; i++) {
            values.push_back(mk<ram::TupleElement>(0, i));
        }
        op = mk<ram::Insert>(deltaName, std::move(values));
        for (std::size_t i = 0; i < arity; i++) {
            if (i < firstAuxiliary) {
                values.push_back(mk<ram::TupleElement>(0, i));
            } else {
                values.push_back(mk<ram::UndefValue>());
            }
        }
        op = mk<ram::Filter>(
                mk<ram::Negation>(mk<ram::ExistenceCheck>(name, std::move(values))), std::move(op));
        op = mk<ram::Scan>(lubName, 0, std::move(op));
        appendStmt(stmts, mk<ram::Query>(std::move(op)));
    } else {
        // we are not in the recursive loop, so the concrete relation is empty for now
        // we can just insert the content of the @lub relation into the concrete one
        for (std::size_t i = 0; i < arity; i++) {
            values.push_back(mk<ram::TupleElement>(0, i));
        }
        op = mk<ram::Insert>(name, std::move(values));
        op = mk<ram::Scan>(lubName, 0, std::move(op));
        appendStmt(stmts, mk<ram::Query>(std::move(op)));
    }

    appendStmt(stmts, mk<ram::Clear>(lubName));

    return mk<ram::Sequence>(std::move(stmts));
}

Own<ram::Statement> UnitTranslator::generateStratumExitSequence(const ast::RelationSet& scc, bool isDelete) const {
    // Helper function to add a new term to a conjunctive condition
    auto addCondition = [&](Own<ram::Condition>& cond, Own<ram::Condition> term) {
        cond = (cond == nullptr) ? std::move(term) : mk<ram::Conjunction>(std::move(cond), std::move(term));
    };

    VecOwn<ram::Statement> exitConditions;

    // (1) if all relations in the scc are empty
    Own<ram::Condition> emptinessCheck;
    for (const ast::Relation* rel : scc) {
        if (!context->hasSubsumptiveClause(rel->getQualifiedName())) {
            if (isDelete) {
                addCondition(
                        emptinessCheck, mk<ram::EmptinessCheck>(getNewDeletionRelationName(rel->getQualifiedName())));
            } else {
                addCondition(
                        emptinessCheck, mk<ram::EmptinessCheck>(getNewInsertionRelationName(rel->getQualifiedName())));
            }
        } else {
            assert (false && "non subsumptive clause");
        }
    }
    appendStmt(exitConditions, mk<ram::Exit>(std::move(emptinessCheck)));

    // (2) if the size limit has been reached for any limitsize relations
    for (const ast::Relation* rel : scc) {
        if (context->hasSizeLimit(rel)) {
            Own<ram::Condition> limit = mk<ram::Constraint>(BinaryConstraintOp::GE,
                    mk<ram::RelationSize>(getConcreteRelationName(rel->getQualifiedName())),
                    mk<ram::SignedConstant>(context->getSizeLimit(rel)));
            appendStmt(exitConditions, mk<ram::Exit>(std::move(limit)));
        }
    }

    return mk<ram::Sequence>(std::move(exitConditions));
}

/** generate RAM code for recursive relations in a strongly-connected component */
Own<ram::Statement> UnitTranslator::generateRecursiveStratum(
        const ast::RelationSet& scc, std::size_t sccNumber) const {
    assert(!scc.empty() && "scc set should not be empty");
    VecOwn<ram::Statement> result;

    // preamble already starts here
    // explicit copy old to new for recursive case... delta union do not copy old to new in recursive case
    for (const ast::Relation* rel : scc) {
        appendStmt(result, generateMergeRelations(rel, getConcreteRelationName(rel->getQualifiedName()), getOldRelationName(rel->getQualifiedName())));
    }
    // Add in the preamble
    appendStmt(result, generateStratumPreamble(scc, true));

    // Get all recursive relation statements // TODO: what are statements here? joinsize seems orthogonal statement to semi-naive evaluation
    /*
     pre
     counter = 1
     Tp-op for non-sccAtoms delta input update
     while true:
        delta semi-naive comp
        if exit: exit
        update rels
        counter++
    clear all tmp rels
     */
    {
        auto prefill = generateStratumNonSccPreFill(scc, true);
        auto updateSequence = generateStratumTableUpdates(scc, true);
        appendStmt(result, std::move(prefill));
        appendStmt(result, std::move(updateSequence));
    }
    {
        // for deletion
        const std::string loop_counter = "loop_counter1";
        VecOwn<ram::Expression> inc;
        inc.push_back(mk<ram::Variable>(loop_counter));
        inc.push_back(mk<ram::UnsignedConstant>(1));
        auto increment_counter = mk<ram::Assign>(mk<ram::Variable>(loop_counter),
                mk<ram::IntrinsicOperator>(FunctorOp::UADD, std::move(inc)), false);  // counter每一次迭代后的累加
        // Add in the main fixpoint loop
        auto loopBody = generateStratumLoopBody(scc, true);
        // loopBody->print(std::cout);
        auto exitSequence = generateStratumExitSequence(scc, true);  // 每次迭代后，如果new都为空，说明迭代结束，离开循环体；否则更新tables，开始下一次迭代
        // exitSequence->print(std::cout);
        auto updateSequence = generateStratumTableUpdates(scc, true); // 每次迭代后，将new->真正的table，new->delta, new清空
        auto fixpointLoop = mk<ram::Loop>(mk<ram::Sequence>(std::move(loopBody),
                std::move(exitSequence), std::move(updateSequence), std::move(increment_counter)));

        appendStmt(result, mk<ram::Assign>(mk<ram::Variable>(loop_counter), mk<ram::UnsignedConstant>(1), true));  // counter最初的赋值1
        appendStmt(result, std::move(fixpointLoop)); // semi-naive 循环计算主体
        // TODO: delta union only for delete
        // Add in the postamble
        appendStmt(result, generateStratumPostamble(scc, true)); // 最终删除全部的临时变量
    }

    appendStmt(result, generateStratumPreamble(scc, false));
    {
        auto prefill = generateStratumNonSccPreFill(scc, false);
        auto updateSequence = generateStratumTableUpdates(scc, false);
        appendStmt(result, std::move(prefill));
        appendStmt(result, std::move(updateSequence));
    }
    {
        // for insertion
        const std::string loop_counter = "loop_counter2";
        VecOwn<ram::Expression> inc;
        inc.push_back(mk<ram::Variable>(loop_counter));
        inc.push_back(mk<ram::UnsignedConstant>(1));
        auto increment_counter = mk<ram::Assign>(mk<ram::Variable>(loop_counter),
                mk<ram::IntrinsicOperator>(FunctorOp::UADD, std::move(inc)), false);  // counter每一次迭代后的累加
        // Add in the main fixpoint loop
        auto loopBody = generateStratumLoopBody(scc, false);
        auto exitSequence = generateStratumExitSequence(scc, false);  // 每次迭代后，如果new都为空，说明迭代结束，离开循环体；否则更新tables，开始下一次迭代
        auto updateSequence = generateStratumTableUpdates(scc, false); // 每次迭代后，将new->真正的table，new->delta, new清空
        auto fixpointLoop = mk<ram::Loop>(mk<ram::Sequence>(std::move(loopBody),
                std::move(exitSequence), std::move(updateSequence), std::move(increment_counter)));

        appendStmt(result, mk<ram::Assign>(mk<ram::Variable>(loop_counter), mk<ram::UnsignedConstant>(1), true));  // counter最初的赋值1
        appendStmt(result, std::move(fixpointLoop)); // semi-naive 循环计算主体
        // TODO: delta union only for delete
        // Add in the postamble
        appendStmt(result, generateStratumPostamble(scc, false)); // 最终删除全部的临时变量

    }
    // TODO derv delta to tuple delta
    return mk<ram::Sequence>(std::move(result));
}

void UnitTranslator::addAuxiliaryArity(
        const ast::Relation* /* relation */, std::map<std::string, std::string>& directives) const {
    directives.insert(std::make_pair("auxArity", "0"));
}

Own<ram::Statement> UnitTranslator::generateLoadRelation(const ast::Relation* relation) const {
    VecOwn<ram::Statement> loadStmts;
    for (const auto* load : context->getLoadDirectives(relation->getQualifiedName())) {
        // Set up the corresponding directive map
        std::map<std::string, std::string> directives;
        for (const auto& [key, value] : load->getParameters()) {
            directives.insert(std::make_pair(key, unescape(value)));
        }
        if (glb->config().has("no-warn")) {
            directives.insert(std::make_pair("no-warn", "true"));
        }
        addAuxiliaryArity(relation, directives);
        directives.insert(std::make_pair("incDelta", "false"));
        directives.insert(std::make_pair("inc-insert", "false"));
        directives.insert(std::make_pair("inc-delete", "false"));

        // base relation
        std::string ramRelationName = getRelationName(relation->getQualifiedName());

        std::string ramOldRelationName = getOldRelationName(relation->getQualifiedName());
        Own<ram::Statement> loadStmt = mk<ram::IO>(ramOldRelationName, directives);

        // Also add IO for delta's IO; when loading those facts, the derivation mapping is also their
        directives["incDelta"] = "true";
        directives["inc-insert"] = "true";

        std::string tmpName = getTmpRelationName(relation->getQualifiedName()); // insert
        std::string tmp2Name = getTmp2RelationName(relation->getQualifiedName()); // delete
        std::string tmp3Name = getTmp3RelationName(relation->getQualifiedName()); // insert2
        std::string tmp4Name = getTmp4RelationName(relation->getQualifiedName()); // delete3

        std::string ramIncDeltaInsertRelationName = getIncDeltaTupleInsertRelationName((relation->getQualifiedName()));
        Own<ram::Statement> loadIncDeltaInsertStmt = mk<ram::IO>(tmpName, directives);

        directives["inc-insert"] = "false";
        directives["inc-delete"] = "true";

        std::string ramIncDeltaDeleteRelationName = getIncDeltaTupleDeleteRelationName((relation->getQualifiedName()));
        Own<ram::Statement> loadIncDeltaDeleteStmt = mk<ram::IO>(tmp2Name, directives);

        // remove redundancy in new and old
        // tmp = delta_R_insert - delta_R_delete
        // delta_R_delete = delta_R_delete - delta_R_insert
        // delta_R_insert = tmp
        // remove all insert that already in old
        // remove all delete that not in old


        // do not use swap for non-tmp relations... souffle does not handle them correctly
        auto removeRedundancyStmt = mk<ram::Sequence>(
            generateMergeRelationsWithFilter(
                relation,
                tmp3Name, //ramIncDeltaInsertRelationName
                tmpName,
                tmp2Name
            ),
            generateMergeRelationsWithFilter(
                relation,
                tmp4Name,
                tmp2Name,
                tmpName
            ),
            generateMergeRelationsWithNegativeFilter(
                relation,
                ramIncDeltaDeleteRelationName,
                tmp4Name,
                getOldRelationName(relation->getQualifiedName())
            ),
            generateMergeRelationsWithFilter(
                relation,
                ramIncDeltaInsertRelationName,
                tmp3Name,
                getOldRelationName(relation->getQualifiedName())
            ),
            mk<ram::Clear>(tmpName),
            mk<ram::Clear>(tmp2Name),
            mk<ram::Clear>(tmp3Name),
            mk<ram::Clear>(tmp4Name)
        );

        // join get "new" input relation
        auto mergeToNewStmt =
            mk<ram::Sequence>(
                generateMergeRelationsWithFilter(relation,
                    getConcreteRelationName(relation->getQualifiedName()),  // new relation
                    getOldRelationName(relation->getQualifiedName()),
                    getIncDeltaTupleDeleteRelationName(relation->getQualifiedName())),
                generateMergeRelations(relation,
                    getConcreteRelationName(relation->getQualifiedName()),
                    getIncDeltaTupleInsertRelationName(relation->getQualifiedName()))
            );

        // join the information for deletion and insertion
        // ramIncDeltaRelation contains all tuples whose derivations change
        // std::string ramIncDeltaRelationName = getIncDeltaRelationName(relation->getQualifiedName());
        // Own<ram::Statement> combineDeltaInsertAndDeleteStmt = mk<ram::Sequence>(
        //     mk<ram::Swap>(ramIncDeltaRelationName, ramIncDeltaInsertRelationName),
        //     generateMergeRelations(relation, ramIncDeltaRelationName, ramIncDeltaDeleteRelationName)
        // );

        //

        loadStmt = mk<ram::Sequence>(std::move(loadStmt), std::move(loadIncDeltaInsertStmt), std::move(loadIncDeltaDeleteStmt), std::move(removeRedundancyStmt), std::move(mergeToNewStmt));
        if (glb->config().has("profile")) {
            const std::string logTimerStatement =
                    LogStatement::tRelationLoadTime(ramRelationName, relation->getSrcLoc());
            loadStmt = mk<ram::LogRelationTimer>(std::move(loadStmt), logTimerStatement, ramRelationName);
        }
        appendStmt(loadStmts, std::move(loadStmt));
    }
    return mk<ram::Sequence>(std::move(loadStmts));
}


Own<ram::Statement> UnitTranslator::generateLoadRelationForIDB(const ast::Relation* relation) const {
    VecOwn<ram::Statement> storeStmts;
    for (const auto* store : context->getStoreDirectives(relation->getQualifiedName())) {
        // Set up the corresponding directive map
        std::map<std::string, std::string> directives;
        for (const auto& [key, value] : store->getParameters()) {
            directives.insert(std::make_pair(key, unescape(value)));
        }
        directives["operation"] = "input";
        directives["fact-dir"] = directives["output-dir"];
        directives.insert(std::make_pair("incDelta", "false")); // TODO
        directives.insert(std::make_pair("inc-insert", "false"));
        directives.insert(std::make_pair("inc-delete", "false"));
        directives.insert(std::make_pair("suffix", "")); // TODO: read from old

        addAuxiliaryArity(relation, directives);

        // load IDB to "old relation"
        std::string ramRelationName = getOldRelationName(relation->getQualifiedName());
        Own<ram::Statement> loadStmt = mk<ram::IO>(ramRelationName, directives);

        if (glb->config().has("profile")) {
            const std::string logTimerStatement =
                    LogStatement::tRelationSaveTime(ramRelationName, relation->getSrcLoc());
            loadStmt = mk<ram::LogRelationTimer>(std::move(loadStmt), logTimerStatement, ramRelationName);
        }
        appendStmt(storeStmts, std::move(loadStmt));
    }
    // TODO: dump delta relations for inc computation
    return mk<ram::Sequence>(std::move(storeStmts));
}

Own<ram::Statement> UnitTranslator::generateStoreRelation(const ast::Relation* relation) const {
    VecOwn<ram::Statement> storeStmts;
    for (const auto* store : context->getStoreDirectives(relation->getQualifiedName())) {
        // Set up the corresponding directive map
        std::map<std::string, std::string> directives;
        for (const auto& [key, value] : store->getParameters()) {
            directives.insert(std::make_pair(key, unescape(value)));
        }
        directives.insert(std::make_pair("incDelta", "false")); // TODO
        directives.insert(std::make_pair("inc-insert", "false"));
        directives.insert(std::make_pair("inc-delete", "false"));
        directives.insert(std::make_pair("suffix", "-debug-new")); // TODO: try not overlap the old for development

        addAuxiliaryArity(relation, directives);

        // Create the resultant store statement, with profile information
        std::string ramRelationName = getConcreteRelationName(relation->getQualifiedName());
        Own<ram::Statement> storeStmt = mk<ram::IO>(ramRelationName, directives);
        if (glb->config().has("profile")) {
            const std::string logTimerStatement =
                    LogStatement::tRelationSaveTime(ramRelationName, relation->getSrcLoc());
            storeStmt = mk<ram::LogRelationTimer>(std::move(storeStmt), logTimerStatement, ramRelationName);
        }
        appendStmt(storeStmts, std::move(storeStmt));
    }
    return mk<ram::Sequence>(std::move(storeStmts));
}

Own<ram::Relation> UnitTranslator::createRamRelation(
        const ast::Relation* baseRelation, std::string ramRelationName) const {
    auto arity = baseRelation->getArity();

    bool mergeAuxiliary = (ramRelationName != getNewRelationName(baseRelation->getQualifiedName()));

    auto auxArity = mergeAuxiliary ? baseRelation->getAuxiliaryArity() : 0;
    // auto representation = baseRelation->getRepresentation(); TODO
    auto representation = RelationRepresentation::BTREE_DELETE;
    if (representation == RelationRepresentation::BTREE_DELETE && ramRelationName[0] == '@') {
        representation = RelationRepresentation::DEFAULT;
    }
    // TODO: BTREE_DELETE
    // if (ramRelationName == getIncDeltaTupleDeleteRelationName(baseRelation->getQualifiedName())) {
    //     std::cout << "delete!!!" << std::endl;
    //     representation = RelationRepresentation::BTREE_DELETE;
    // }
    std::vector<std::string> attributeNames;
    std::vector<std::string> attributeTypeQualifiers;
    for (const auto& attribute : baseRelation->getAttributes()) {
        attributeNames.push_back(attribute->getName());
        attributeTypeQualifiers.push_back(context->getAttributeTypeQualifier(attribute->getTypeName()));
    }

    return mk<ram::Relation>(
            ramRelationName, arity, auxArity, attributeNames, attributeTypeQualifiers, representation);
}

VecOwn<ram::Relation> UnitTranslator::createRamRelations(const std::vector<std::size_t>& sccOrdering) const {
    VecOwn<ram::Relation> ramRelations;
    for (const auto& scc : sccOrdering) {
        bool isRecursive = context->isRecursiveSCC(scc);
        for (const auto& rel : context->getRelationsInSCC(scc)) {
            // Add main relation
            std::string mainName = getConcreteRelationName(rel->getQualifiedName());
            ramRelations.push_back(createRamRelation(rel, mainName));

            if (isRecursive || rel->getAuxiliaryArity() > 0) {
                // Add new relation
                std::string newName = getNewRelationName(rel->getQualifiedName());
                ramRelations.push_back(createRamRelation(rel, newName));
            }

            // Recursive relations also require @delta and @new variants, with the same signature
            if (isRecursive) {
                // Add delta relation
                std::string deltaName = getDeltaRelationName(rel->getQualifiedName());
                ramRelations.push_back(createRamRelation(rel, deltaName));
            }

            // Add relation that cache old result
            std::string oldName = getOldRelationName(rel->getQualifiedName());
            ramRelations.push_back(createRamRelation(rel, oldName));

            // INC: Add delta relation for derivation-changing tuples in incremental computation
            // We will have @inc_delta_derv_[insert|delete]_ and @inc_delta_tuple_[insert|delete}_ relations
            // The former one tracks those tuples that change its derivations
            // The later one tracks the inserted and deleted tuples for real
            // i.e., newly inserted tuples and newly deleted tuples
            // Only those tuples cause further derivation info (rule application) changes in our setting

            std::string incDeltaDervInsertName = getIncDeltaDervInsertRelationName(rel->getQualifiedName());
            ramRelations.push_back(createRamRelation(rel, incDeltaDervInsertName));

            std::string incDeltaDervDeleteName = getIncDeltaDervDeleteRelationName(rel->getQualifiedName());
            ramRelations.push_back(createRamRelation(rel, incDeltaDervDeleteName));

            std::string incDeltaTupleInsertName = getIncDeltaTupleInsertRelationName(rel->getQualifiedName());
            ramRelations.push_back(createRamRelation(rel, incDeltaTupleInsertName));

            std::string incDeltaTupleDeleteName = getIncDeltaTupleDeleteRelationName(rel->getQualifiedName());
            ramRelations.push_back(createRamRelation(rel, incDeltaTupleDeleteName));

            std::string tmpName = getTmpRelationName(rel->getQualifiedName());
            ramRelations.push_back(createRamRelation(rel, tmpName));

            std::string tmp2Name = getTmp2RelationName(rel->getQualifiedName());
            ramRelations.push_back(createRamRelation(rel, tmp2Name));

            std::string tmp3Name = getTmp3RelationName(rel->getQualifiedName());
            ramRelations.push_back(createRamRelation(rel, tmp3Name));

            std::string tmp4Name = getTmp4RelationName(rel->getQualifiedName());
            ramRelations.push_back(createRamRelation(rel, tmp4Name));

            // for recursion: delta and new, now have deletion and insertion version
            // delta - real tuple change, new - derivation change & record derivation

            if (rel->getAuxiliaryArity() > 0) {
                assert(false && "does not support lub relation");
                // Add lub relation
                std::string lubName = getLubRelationName(rel->getQualifiedName());
                ramRelations.push_back(createRamRelation(rel, lubName));
            }

            if (rel->getAuxiliaryArity() > 0) {
                assert(false && "does not support AUXILIARY ARITY");
            }

            // TODO: for inc, need extra delta relations for delta rules, for those rels in recursive
            // Recursive relations also require @delta and @new variants, with the same signature
            // if (isRecursive) {
                // Note that this UnitTranslator is only for incremental case

                // Add delta relation
                std::string deltaDelName = getDeltaDeletionRelationName(rel->getQualifiedName());
                ramRelations.push_back(createRamRelation(rel, deltaDelName));
                std::string deltaInsertName = getDeltaInsertionRelationName(rel->getQualifiedName());
                ramRelations.push_back(createRamRelation(rel, deltaInsertName));
                std::string newDelName = getNewDeletionRelationName(rel->getQualifiedName());
                ramRelations.push_back(createRamRelation(rel, newDelName));
                std::string newInsertName = getNewInsertionRelationName(rel->getQualifiedName());
                ramRelations.push_back(createRamRelation(rel, newInsertName));

            // }
        }
    }
    return ramRelations;
}

Own<ram::Sequence> UnitTranslator::generateProgram(const ast::TranslationUnit& translationUnit) {
    // Check if trivial program
    if (context->getNumberOfSCCs() == 0) {
        return mk<ram::Sequence>();
    }
    // TODO
    const auto& sccOrdering =
            translationUnit.getAnalysis<ast::analysis::TopologicallySortedSCCGraphAnalysis>().order();
    VecOwn<ram::Statement> res;

    // Create subroutines for each SCC according to topological order
    for (std::size_t i = 0; i < sccOrdering.size(); i++) {
        // Generate the main stratum code
        auto stratum = generateStratum(sccOrdering.at(i));

        // Clear expired relations
        const auto& expiredRelations = context->getExpiredRelations(i);
        stratum = mk<ram::Sequence>(std::move(stratum), generateClearExpiredRelations(expiredRelations));

        // Add the subroutine
        const ast::Relation* rel = *context->getRelationsInSCC(sccOrdering.at(i)).begin();

        std::string stratumID = rel->getQualifiedName().toString();
        addRamSubroutine(stratumID, std::move(stratum));

        // invoke the strata
        appendStmt(res, mk<ram::Call>("stratum_" + stratumID));
    }

    // Add main timer if profiling
    if (!res.empty() && glb->config().has("profile")) {
        auto newStmt = mk<ram::LogTimer>(mk<ram::Sequence>(std::move(res)), LogStatement::runtime());
        res.clear();
        appendStmt(res, std::move(newStmt));
    }

    // Program translated!
    return mk<ram::Sequence>(std::move(res));
}

Own<ram::TranslationUnit> UnitTranslator::translateUnit(ast::TranslationUnit& tu) {
    glb = &tu.global();

    /* -- Set-up -- */
    auto ram_start = std::chrono::high_resolution_clock::now();
    context = mk<TranslatorContext>(tu);

    /* -- Translation -- */
    // Generate the RAM program code
    auto ramMain = generateProgram(tu);

    // Create the relevant RAM relations
    const auto& sccOrdering = tu.getAnalysis<ast::analysis::TopologicallySortedSCCGraphAnalysis>().order();
    auto ramRelations = createRamRelations(sccOrdering);

    // Combine all parts into the final RAM program
    ErrorReport& errReport = tu.getErrorReport();
    DebugReport& debugReport = tu.getDebugReport();
    auto ramProgram =
            mk<ram::Program>(std::move(ramRelations), std::move(ramMain), std::move(ramSubroutines));

    // Add the translated program to the debug report
    if (glb->config().has("debug-report")) {
        auto ram_end = std::chrono::high_resolution_clock::now();
        std::string runtimeStr =
                "(" + std::to_string(std::chrono::duration<double>(ram_end - ram_start).count()) + "s)";
        std::stringstream ramProgramStr;
        ramProgramStr << *ramProgram;
        debugReport.addSection("ram-program", "RAM Program " + runtimeStr, ramProgramStr.str());
    }

    // Wrap the program into a translation unit
    return mk<ram::TranslationUnit>(tu.global(), std::move(ramProgram), errReport, debugReport);
}

}  // namespace souffle::ast2ram::seminaive
