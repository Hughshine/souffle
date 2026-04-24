#ifndef SOUFFLE_CLI_PENDING_OPERATION_STAGER_H
#define SOUFFLE_CLI_PENDING_OPERATION_STAGER_H

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "souffle/Derivation.h"
#include "souffle/SouffleInterface.h"

namespace souffle::cli {

inline std::string getConcreteRelationName(const std::string& name, const std::string prefix) {
    return prefix + name;
}

inline std::string getIncDeltaTupleDeleteRelationName(const std::string& name) {
    return getConcreteRelationName(name, "$inc_delta_tuple_delete_");
}

inline std::string getIncDeltaTupleInsertRelationName(const std::string& name) {
    return getConcreteRelationName(name, "$inc_delta_tuple_insert_");
}

inline std::size_t readProcStatusValueKb(const char* key) {
    std::ifstream status("/proc/self/status");
    std::string line;
    while (std::getline(status, line)) {
        if (line.rfind(key, 0) == 0) {
            std::istringstream iss(line);
            std::string label, value, unit;
            iss >> label >> value >> unit;
            return static_cast<std::size_t>(std::stoull(value));
        }
    }
    return 0;
}

template <typename OperationT>
class PendingOperationStager {
public:
    using InputRelationMap = std::map<std::string, std::set<UntypedTuple>>;

    PendingOperationStager(
            souffle::SouffleProgram* program,
            InputRelationMap& initialInputRelations,
            std::unordered_map<UntypedTuple, double>& factProb)
            : program(program), initialInputRelations(initialInputRelations), factProb(factProb) {}

    bool stageInsert(OperationT& op) {
        auto* origRel = program->getRelation(op.relationName);
        auto* rel = program->getRelation(getIncDeltaTupleInsertRelationName(op.relationName));
        if (rel == nullptr) {
            std::cout << "Relation not found, omitted: " << op.relationName << std::endl;
            op.valid = false;
            return false;
        }
        if (op.values.size() != rel->getArity()) {
            std::cout << "Relation arity mismatch, omitted: " << op.relationName << std::endl;
            op.valid = false;
            return false;
        }

        souffle::tuple relTuple = souffle::tuple(rel);
        souffle::tuple origTuple = souffle::tuple{origRel};
        appendOperationValuesToTuple(relTuple, op);
        appendOperationValuesToTuple(origTuple, op);

        const auto untypedTuple = UntypedTuple::fromSouffleTuple(origTuple);
        auto& currentInputs = initialInputRelations[op.relationName];
        if (currentInputs.count(untypedTuple)) {
            std::cout << "Relation already contains the tuple to insert, omitted: "
                      << relTuple.toString() << std::endl;
            op.valid = false;
            return false;
        }

        std::cout << "Inserting tuple: " << origTuple.toString() << std::endl;
        rel->insert(relTuple);
        currentInputs.insert(untypedTuple);
        factProb[untypedTuple] = op.probability;
        return true;
    }

    bool stageDelete(OperationT& op) {
        auto* origRel = program->getRelation(op.relationName);
        auto* rel = program->getRelation(getIncDeltaTupleDeleteRelationName(op.relationName));
        if (rel == nullptr) {
            std::cout << "Relation not found, omitted: " << op.relationName << std::endl;
            op.valid = false;
            return false;
        }
        if (op.values.size() != rel->getArity()) {
            std::cout << "Relation arity mismatch, omitted: " << op.relationName << std::endl;
            op.valid = false;
            return false;
        }

        souffle::tuple relTuple = souffle::tuple(rel);
        souffle::tuple origTuple = souffle::tuple{origRel};
        appendOperationValuesToTuple(relTuple, op);
        appendOperationValuesToTuple(origTuple, op);

        const auto untypedTuple = UntypedTuple::fromSouffleTuple(origTuple);
        auto& currentInputs = initialInputRelations[op.relationName];
        if (!currentInputs.count(untypedTuple)) {
            std::cout << "Relation does not contains the tuple to delete, omitted: "
                      << origTuple.toString() << std::endl;
            op.valid = false;
            return false;
        }
        if (rel->contains(relTuple)) {
            std::cout << "Already deleted the tuple, omitted: " << relTuple.toString() << std::endl;
            op.valid = false;
            return false;
        }

        rel->insert(relTuple);
        currentInputs.erase(untypedTuple);
        factProb.erase(untypedTuple);
        auto& deletedFactRuleAppSet =
                DerivationManager::untypedTuple2RuleApplications[UntypedTuple::fromSouffleTuple(origTuple)];
        if (deletedFactRuleAppSet != nullptr && !deletedFactRuleAppSet->empty()) {
            auto& deltaDeletedFactRuleAppSet =
                    DerivationManager::untypedTuple2DeltaDeleteRuleApplications[
                            UntypedTuple::fromSouffleTuple(origTuple)];
            if (deltaDeletedFactRuleAppSet == nullptr) {
                deltaDeletedFactRuleAppSet = new std::unordered_set<RuleApplication>();
            }
            for (auto& ruleApp : *deletedFactRuleAppSet) {
                deltaDeletedFactRuleAppSet->insert(ruleApp);
            }
            deletedFactRuleAppSet->clear();
            delete deletedFactRuleAppSet;
            deletedFactRuleAppSet = nullptr;
            DerivationManager::untypedTuple2RuleApplications.erase(
                    UntypedTuple::fromSouffleTuple(origTuple));
        }
        return true;
    }

    void stageAll(std::vector<OperationT>& operations) {
        purgeAllIncDeltaRelations();
        for (auto& op : operations) {
            if (op.type == OperationT::INSERT) {
                stageInsert(op);
            } else if (op.type == OperationT::DELETE) {
                stageDelete(op);
            }
        }
    }

    void purgeAllIncDeltaRelations() {
        for (auto* rel : program->getAllRelations()) {
            if (rel->getName()[0] == '$') {
                rel->purge();
            }
        }
    }

    void purgeAllNonIncDeltaRelations() {
        for (auto* rel : program->getAllRelations()) {
            if (rel->getName()[0] != '$') {
                rel->purge();
            }
        }
    }

    void purgeAllRelations() {
        const bool relPurgeProbe = std::getenv("SOUFFLE_REL_PURGE_PROBE") != nullptr;
        for (auto* rel : program->getAllRelations()) {
            std::size_t tuplesBefore = 0;
            std::size_t rssBefore = 0;
            std::size_t hwmBefore = 0;
            if (relPurgeProbe) {
                tuplesBefore = rel->size();
                rssBefore = readProcStatusValueKb("VmRSS:");
                hwmBefore = readProcStatusValueKb("VmHWM:");
            }
            rel->purge();
            if (relPurgeProbe) {
                const std::size_t rssAfter = readProcStatusValueKb("VmRSS:");
                const std::size_t hwmAfter = readProcStatusValueKb("VmHWM:");
                std::cout << "[rel-purge-probe] relation=" << rel->getName()
                          << " tuples_before=" << tuplesBefore
                          << " rss_before_kb=" << rssBefore
                          << " rss_after_kb=" << rssAfter
                          << " rss_delta_kb=" << static_cast<long long>(rssAfter) -
                                         static_cast<long long>(rssBefore)
                          << " hwm_before_kb=" << hwmBefore
                          << " hwm_after_kb=" << hwmAfter
                          << " hwm_delta_kb=" << static_cast<long long>(hwmAfter) -
                                         static_cast<long long>(hwmBefore)
                          << std::endl;
            }
        }
    }

    void loadInitialInputRelations() {
        for (auto* rel : program->getInputRelations()) {
            for (auto& tuple : initialInputRelations[rel->getName()]) {
                souffle::tuple relTuple = souffle::tuple(rel);
                for (const auto& field : tuple.fields) {
                    relTuple << field;
                }
                rel->insert(relTuple);
            }
        }
    }

private:
    void appendOperationValuesToTuple(souffle::tuple& relTuple, const OperationT& op) const {
        for (const auto& value : op.values) {
            relTuple << std::stoi(value);
        }
    }

    souffle::SouffleProgram* program = nullptr;
    InputRelationMap& initialInputRelations;
    std::unordered_map<UntypedTuple, double>& factProb;
};

}  // namespace souffle::cli

#endif
