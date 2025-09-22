#ifndef CLI_H
#define CLI_H
#include <iostream>
#include <string>
#include <sstream>
#include <vector>
#include <regex>
#include <iomanip>
#include <readline/readline.h>
#include <readline/history.h>
#include "souffle/SouffleInterface.h"
#include "souffle/Derivation.h" // TODO: should change timer to Misc header
#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/RuleManager.h"
#include "souffle/problog/formula/CuddManager.h"
#include "souffle/problog/ForwardCompilation.h"
#include "souffle/CompiledOptions.h"
#include "souffle/problog/PreDerivationGraph.h"
#include <unistd.h> // Required for isatty()

std::string getConcreteRelationName(const std::string& name, const std::string prefix) {
    return prefix + name;
}

std::string getIncDeltaTupleDeleteRelationName(const std::string& name) {
    return getConcreteRelationName(name, "$inc_delta_tuple_delete_");
}

std::string getIncDeltaTupleInsertRelationName(const std::string& name) {
    return getConcreteRelationName(name, "$inc_delta_tuple_insert_");
}

enum IncMode {
  FULL,
  INC,
  ELASTIC
};

template<typename NodeRef>
class IncrementalCLI {
private:
    // Structure to represent a pending operation
    struct Operation {
        enum Type { INSERT, DELETE } type;
        bool valid = true;
        std::string relationName;
        std::vector<std::string> values;
        double probability;

        std::string toString() const {
            std::stringstream ss;
            ss << (type == INSERT ? "insert " : "delete ");
            if (type == INSERT) {
                ss << probability << "::";
            }
            ss << relationName << "(";
            for (size_t i = 0; i < values.size(); i++) {
                if (i > 0) ss << ", ";
                ss << values[i];
            }
            ss << ")";
            return ss.str();
        }
    };

    // Store all pending operations
    std::vector<Operation> pendingOperations;

    // Parse a tuple with potential probability
    // Returns: relation name, values, probability, success flag
    std::tuple<std::string, std::vector<std::string>, double, bool>
    parseInsertCommand(const std::string& str) {
        std::string relName;
        std::vector<std::string> values;
        double probability = 1.0; // Default probability
        bool success = false;

        // Pattern for prefix probability format: probability::relation_name(...)
        std::regex prefixProbRegex("(0?\\.[0-9]+)\\s*::\\s*([a-zA-Z][a-zA-Z0-9_]*)\\s*\\(([^)]*)\\)");

        // Pattern for suffix probability format: relation_name(...) probability
        std::regex suffixProbRegex("([a-zA-Z][a-zA-Z0-9_]*)\\s*\\(([^)]*)\\)\\s*(0?\\.[0-9]+)");

        // Pattern for standard format without explicit probability: relation_name(...)
        std::regex standardRegex("([a-zA-Z][a-zA-Z0-9_]*)\\s*\\(([^)]*)\\)");

        std::smatch matches;

        // Try matching prefix probability pattern
        if (std::regex_search(str, matches, prefixProbRegex) && matches.size() > 3) {
            try {
                probability = std::stod(matches[1].str());
                if (probability < 0.0 || probability > 1.0) {
                    return std::make_tuple("", std::vector<std::string>(), 0.0, false);
                }
                relName = matches[2].str();
                std::string valuesStr = matches[3].str();
                values = parseValues(valuesStr);
                success = true;
            } catch (const std::exception&) {
                return std::make_tuple("", std::vector<std::string>(), 0.0, false);
            }
        }
        // Try matching suffix probability pattern
        else if (std::regex_search(str, matches, suffixProbRegex) && matches.size() > 3) {
            try {
                relName = matches[1].str();
                std::string valuesStr = matches[2].str();
                values = parseValues(valuesStr);

                probability = std::stod(matches[3].str());
                if (probability < 0.0 || probability > 1.0) {
                    return std::make_tuple("", std::vector<std::string>(), 0.0, false);
                }
                success = true;
            } catch (const std::exception&) {
                return std::make_tuple("", std::vector<std::string>(), 0.0, false);
            }
        }
        // Try matching standard pattern (default probability = 1.0)
        else if (std::regex_search(str, matches, standardRegex) && matches.size() > 2) {
            relName = matches[1].str();
            std::string valuesStr = matches[2].str();
            values = parseValues(valuesStr);
            success = true;
        }

        return std::make_tuple(relName, values, probability, success);
    }

    // Helper function to parse values from a comma-separated string
    std::vector<std::string> parseValues(const std::string& valuesStr) {
        std::vector<std::string> values;

        // Split values by comma
        std::regex valueRegex("\\s*([^,]+)\\s*,?");
        std::string::const_iterator searchStart(valuesStr.cbegin());
        std::smatch valueMatch;

        while (std::regex_search(searchStart, valuesStr.cend(), valueMatch, valueRegex)) {
            std::string value = valueMatch[1].str();
            // Trim whitespace
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            values.push_back(value);
            searchStart = valueMatch.suffix().first;
        }

        return values;
    }
    souffle::SouffleProgram* program;
    IncrementalDerivationGraph* graph;
    RuleManager* ruleManager;
//    std::map<NodePtr, BddNodeRef>* nodeFormulas;
    DDManager<NodeRef>* ddManager = nullptr;
    std::map<NodePtr, NodeRef>* nodeFormulas;
    std::map<EdgePtr, NodeRef>* edgeFormulas;
    std::set<NodePtr> changedNodes;


    IncMode incMode = IncMode::INC;
    bool derivationOnly = false;
    bool isGround = false;
public:
    IncrementalCLI(souffle::SouffleProgram* prog = nullptr,
            IncrementalDerivationGraph* graph = nullptr,
            RuleManager* rm = nullptr,
            DDManager<NodeRef>* ddManager = nullptr,
            std::map<NodePtr, NodeRef>* nodeFormulas = {},
            std::map<EdgePtr, NodeRef>* edgeFormulas = {},
            bool isGround = false,
            PreDerivationGraph* preDG = nullptr
            )
            : program(prog), graph(graph), ruleManager(rm), ddManager(ddManager), nodeFormulas(nodeFormulas), edgeFormulas(edgeFormulas), changedNodes(), isGround(isGround), preDG(preDG) {
        // Initialize readline
        using_history();
    }

//    IncrementalCLI(): {}

    ~IncrementalCLI() {
        // Clean up readline history
        clear_history();
    }

    souffle::CmdOptions opt;
    void setCmdOptions(const souffle::CmdOptions& options) {
        opt = options;
        setDerivationOnly(options.isDerivationOnly());
        auto& mode = options.getIncMode();
        if (mode == "full") {
            setIncMode(IncMode::FULL);
        } else if (mode == "inc" || mode == "incremental" || mode == "incr" ) {
            setIncMode(IncMode::INC);
        } else if (mode == "elastic") {
            setIncMode(IncMode::ELASTIC);
        } else {
            std::cerr << "Unknown incremental mode: " << mode << ", defaulting to INCREMENTAL." << std::endl;
            setIncMode(IncMode::INC);
        }
    }
    void setIncMode(IncMode mode) {
        incMode = mode;
    }
    void setDerivationOnly(bool val) {
        derivationOnly = val;
    }
    bool processCommand(const std::string& command) {
        // Skip empty commands
        if (command.empty()) {
            return true;
        }

        // Split command into parts
        std::istringstream iss(command);
        std::string cmd;
        iss >> cmd;

        if (cmd == "help" || cmd == "h") {
            std::cout << "Incremental Souffle CLI Commands:\n"
                      << "--------------------------------\n"
                      << "insert [probability::]relation_name(val1, val2, ...) [probability]\n"
                      << "       Queue a tuple for insertion with optional probability (0-1)\n"
                      << "delete/remove relation_name(val1, val2, ...)\n"
                      << "       Queue a tuple for deletion\n"
                      << "list   List all pending operations\n"
                      << "commit Apply queued changes and run incremental computation\n"
                      << "help, h Display this help message\n"
                      << "exit, quit, q Exit the CLI\n"
                      << std::endl;
            return true;
        }
        else if (cmd == "insert") {
            // Get the rest of the line
            std::string tupleSpec;
            std::getline(iss >> std::ws, tupleSpec);

            // Parse tuple with probability
            auto [relName, values, probability, success] = parseInsertCommand(tupleSpec);

            if (!success || relName.empty()) {
                std::cout << "Error: Invalid format. Use: [probability::]relation_name(val1, val2, ...) [probability]" << std::endl;
                return true;
            }

            // Add to pending operations
            Operation op;
            op.type = Operation::INSERT;
            op.relationName = relName;
            op.values = values;
            op.probability = probability;
            pendingOperations.push_back(op);

            // Output parsed information
            std::cout << "PARSED INSERT: Relation = " << relName
                      << ", Values = [";
            for (size_t i = 0; i < values.size(); i++) {
                if (i > 0) std::cout << ", ";
                std::cout << values[i];
            }
            std::cout << "], Probability = " << std::setprecision(8) << probability << std::endl;

        } else if (cmd == "delete" || cmd == "remove") {
            // Get the rest of the line
            std::string tupleSpec;
            std::getline(iss >> std::ws, tupleSpec);

            // For deletion we use the same parser but ignore probability
            auto [relName, values, probability, success] = parseInsertCommand(tupleSpec);

            if (!success || relName.empty()) {
                std::cout << "Error: Invalid format. Use: relation_name(val1, val2, ...)" << std::endl;
                return true;
            }

            // Add to pending operations
            Operation op;
            op.type = Operation::DELETE;
            op.relationName = relName;
            op.values = values;
            op.probability = 1.0;  // Deletion always has probability 1.0

            bool overlap = false;
            for (size_t i = 0; i < pendingOperations.size(); i++) {
                // if overlapped insertion, remove that insertion
                if (pendingOperations[i].type == Operation::INSERT && pendingOperations[i].relationName == relName) {
                    bool same = true;
                    for (size_t j = 0; j < pendingOperations[i].values.size(); j++) {  // TODO: optimize
                        if (pendingOperations[i].values[j] != values[j]) {
                            same = false;
                            break;
                        }
                    }
                    if (same) {
                        std::cout << "Overlapped insertion and deletion removed." << std::endl;
                        overlap = true;
                        pendingOperations.erase(pendingOperations.begin() + i);
                        break;
                    }
                }
            }
            if (!overlap) {
                pendingOperations.push_back(op);
            }

            // Output parsed information
            std::cout << "PARSED DELETE: Relation = " << relName
                      << ", Values = [";
            for (size_t i = 0; i < values.size(); i++) {
                if (i > 0) std::cout << ", ";
                std::cout << values[i];
            }
            std::cout << "]" << std::endl;

        } else if (cmd == "list") {
            // List all pending operations
            if (pendingOperations.empty()) {
                std::cout << "No pending operations." << std::endl;
            } else {
                std::cout << "Pending operations:" << std::endl;
                for (size_t i = 0; i < pendingOperations.size(); i++) {
                    const Operation& op = pendingOperations[i];
                    std::cout << i+1 << ". " << op.toString() << std::endl;
                }
            }
        } else if (cmd == "setmode") {
            std::string mode;
            iss >> mode;
            if (mode == "incremental" || mode == "incr") {
                incMode = IncMode::INC;
                std::cout << "Set incremental mode to INCREMENTAL" << std::endl;
            } else if (mode == "full") {
                incMode = IncMode::FULL;
                std::cout << "Set incremental mode to FULL" << std::endl;
            } else if (mode == "elastic") {
                incMode = IncMode::ELASTIC;
                std::cout << "Set incremental mode to ELASTIC" << std::endl;
            }
//            else if (mode == "compute-all") {
//                derivationOnly = false;
//            } else if (mode == "compute-derv-only") {
//                derivationOnly = true;
//            }
            else {
                std::cout << "Unknown mode: " << mode << std::endl;
                std::cout << "Available modes: incremental (incr), full, elastic" << std::endl;
                std::cout << "Current mode unchanged." << std::endl;
            }
        } else if (cmd == "commit") {
            std::cout << "PARSED COMMIT: Would apply " << pendingOperations.size()
                      << " pending changes and run incremental computation" << std::endl;

            // In a real implementation, we would actually apply the changes here
            commit();
            // Clear pending operations after commit
            pendingOperations.clear();

        } else if (cmd == "dump") {
            if (isGround) {
                std::cout << "Dumping PreDG to 'preDG_dump.dot'" << std::endl;
                preDG->toDot("preDG_dump.dot");
            }
            else {
                assert (this->program != nullptr);
                for (auto rel: this->program->getAllRelations()) {
                    std::cout << rel->getName() << std::endl;
                    for (auto ele: *rel) {
                        std::cout << ele.toString() << std::endl;
                    }
                }
            }
        } else if (cmd == "exit" || cmd == "quit" || cmd == "q") {
            std::cout << "PARSED EXIT: Exiting CLI" << std::endl;
            return false;

        } else {
            std::cout << "Unknown command: " << cmd << std::endl;
            std::cout << "Use 'help' to see available commands" << std::endl;
        }

        return true;
    }

    UntypedTuple getTuple(const Operation& op) {
        UntypedTuple tuple{op.relationName, {}};
        for (const auto& value : op.values) {
            tuple.fields.push_back(std::stoi(value));
        }
        return tuple;
    }

    AtomKey getAtomKey(const Operation& op) {
        AtomKey key;
        key.rel = op.relationName;
        for (const auto& value : op.values) {
            key.args.push_back(std::stoi(value));
        }
        return key;
    }

    std::unordered_map<UntypedTuple, double> getFactProbInc() {
        std::unordered_map<UntypedTuple, double> fact_prob_inc;
        for (const auto& op : pendingOperations) {
            if (op.valid && op.type == Operation::INSERT) {
                fact_prob_inc[getTuple(op)] = op.probability;
            }
        }
        return fact_prob_inc;
    }

    std::vector<UntypedTuple> getDeletedFacts() {
        std::vector<UntypedTuple> deletedFacts;
        for (const auto& op : pendingOperations) {
            if (op.valid && op.type == Operation::DELETE) {
                deletedFacts.push_back(getTuple(op));
            }
        }
        return deletedFacts;
    }

    void purgeAllIncDeltaRelations() {
        for (auto* rel : program->getAllRelations()) {
             if (rel->getName()[0] == '$') {
//                std::cout << "Purging relation: " << rel->getName() << std::endl;
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
        for (auto* rel : program->getAllRelations()) {
            rel->purge();
        }
    }

    void loadInitialInputRelations() {
        for (auto* rel : program->getInputRelations()) {
            for (auto& tuple: initialInputRelations[rel->getName()]) {
                souffle::tuple relTuple = souffle::tuple(rel);
                for (const auto& field : tuple.fields) {
                    relTuple << field;
                }
                rel->insert(relTuple);
            }
        }
    }


    std::vector<std::string> outputRelations;
    void setOutputRelations(const std::vector<std::string> outputRelationNames) {
        this->outputRelations = outputRelationNames;
    }

    size_t iteration = 1;
    PreDerivationGraph* preDG;
    void commit() {
        DerivationManager::untypedTuple2DeltaInsertRuleApplications.clear();
        DerivationManager::untypedTuple2DeltaDeleteRuleApplications.clear();
        DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications.clear();
        DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications.clear();
        static size_t commitCount = 0;
        // TODO: should clean all delta relations after each commit
        if (isGround) {
            debugger.startTurn();
            // for ground program ...
            // get the incremental derivation graph -> build formulas -> compute probabilities
//             1.
            for (auto& op : pendingOperations) {
                if (op.type == Operation::INSERT) {
                    auto tuple = getAtomKey(op);
                    if (preDG->hasFact(tuple)) {
                        std::cout << "PreDG already contains the tuple to insert, omitted: " << tuple.toString() << std::endl;
                        op.valid = false;
                        continue;
                    }
                    preDG->seedFactsByKey({tuple});
                } else if (op.type == Operation::DELETE) {
                    auto tuple = getAtomKey(op);
                    if (!preDG->hasFact(tuple)) {
                        std::cout << "PreDG does not contains the tuple to delete, omitted: " << tuple.toString() << std::endl;
                        op.valid = false;
                        continue;
                    }
                    preDG->retractFactsByKey({tuple});
                }
            }
            debugger.startStage(StageKind::SEMINAIVE_INC);
            preDG->recompute();
            preDG->materialize(*graph);
            debugger.endStage();
            // TODO should maintain initialInputRelations and fact_prob
            dumpInitialInputRelations(opt.getOutputFileDir() + "/initial-input-relations-iter" + std::to_string(iteration) + ".txt");
            if (incMode == IncMode::INC) {
                debugger.startStage(StageKind::PRUNING_INC);
                graph->dumpDotInc("derivation-inc-before-prune" + std::to_string(iteration) + ".dot");
                auto view = graph->prune(this->outputRelations);
                debugger.endStage();
                view.dumpDotInc("derivation-inc-after-prune" + std::to_string(iteration) + ".dot");
                changedNodes.clear();

                // TODO: derv-only
                std::cout << view.getDeltaInsertNodes().size() << " nodes with inserted derivations.\n";
                std::cout << view.getDeltaDeleteNodes().size() << " nodes with deleted derivations.\n";
                std::cout << view.getDeltaInsertEdges().size() << " edges with inserted derivations.\n";
                std::cout << view.getDeltaDeleteEdges().size() << " edges with deleted derivations.\n";
                // knowledge representation
                debugger.startStage(StageKind::FORWARD_COMPILATION_INC);
                buildFormulasIncCyclewise(view, *ddManager, *nodeFormulas, *edgeFormulas, changedNodes);  // TODO: should only update the changed ones.
                debugger.endStage();

                {
                    debugger.startStage(StageKind::WEIGHTED_MODEL_COUNTING_INC);
                    FunctionTimer timer("incrementally compute probabilities, size " + std::to_string(changedNodes.size()));
                    std::unordered_map<NodePtr, double> newProbResult;
                    for (const auto& node: view.getValidNodes()) {
                        if (changedNodes.find(node) == changedNodes.end()) {
                            newProbResult[node] = probResult[node];
                        } else {
                            newProbResult[node] = ddManager->computeWeightedModelCount((*nodeFormulas)[node]);
                        }
                    }
                    probResult.clear();
                    probResult = newProbResult;
                    debugger.endStage();
                }

                debugger.endTurn();
                dumpProbabilities(probResult,"./output/","fact-iter" + std::to_string(iteration) + "-inc");
                iteration++;
            } else if (incMode == IncMode::FULL) {
                debugger.startStage(StageKind::PRUNING_FULL);
                graph->dumpDotInc("derivation-full-before-prune" + std::to_string(iteration) + ".dot");
                auto view = graph->prune(this->outputRelations);
                debugger.endStage();
                view.dumpDotInc("derivation-full-after-prune" + std::to_string(iteration) + ".dot");
                debugger.endStage();

                debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
                nodeFormulas->clear(), edgeFormulas->clear();
                buildFormulasCyclewise(view, *ddManager, *nodeFormulas, *edgeFormulas);
                debugger.endStage();

                debugger.startStage(StageKind::WEIGHTED_MODEL_COUNTING_FULL);
                probResult.clear();
                for (auto& [node, formula]: *nodeFormulas) {
                    probResult[node] = ddManager->computeWeightedModelCount(formula);
                }
                debugger.endStage();
                debugger.startStage(StageKind::IO_DUMP_FULL);
                dumpProbabilities(probResult,"./output/","fact-iter" + std::to_string(iteration) + "-full");
                debugger.endStage();
                debugger.endTurn();
                iteration++;
            } else if (incMode == IncMode::ELASTIC) {
                // TODO
                assert (false);
            }
        }
        else if (!isGround && program) {  // for non-ground program ...
            {
                purgeAllIncDeltaRelations();
                // insert delta into relations for real
                for (auto& op : pendingOperations) {
                    if (op.type == Operation::INSERT) {
                        auto* origRel = program->getRelation(op.relationName);
                        auto* rel = program->getRelation(getIncDeltaTupleInsertRelationName(op.relationName));
                        if (rel == nullptr) {
                            std::cout << "Relation not found, omitted: " << op.relationName << std::endl;
                            op.valid = false;
                            continue;
                        }
                        if (op.values.size() != rel->getArity()) {
                            std::cout << "Relation arity mismatch, omitted: " << op.relationName << std::endl;
                            op.valid = false;
                            continue;
                        }
                        souffle::tuple relTuple = souffle::tuple(rel);
                        souffle::tuple origTuple = souffle::tuple{origRel};

                        for (size_t i = 0; i < op.values.size(); i++) {
                            relTuple << std::stoi(op.values[i]);  // TODO optimize
                            origTuple << std::stoi(op.values[i]);
                        }
                        if (origRel->contains(origTuple)) {
                            std::cout << "Relation already contains the tuple to insert, omitted: " << relTuple.toString() << std::endl;
                            op.valid = false;
                            continue;
                        } else {
                            std::cout << "Inserting tuple: " << origTuple.toString() << std::endl;
                        }
                        rel->insert(relTuple);
                        initialInputRelations[op.relationName].insert(UntypedTuple::fromSouffleTuple(origTuple));
                        fact_prob[UntypedTuple::fromSouffleTuple(origTuple)] = op.probability;
                    } else if (op.type == Operation::DELETE) {
                        auto* origRel = program->getRelation(op.relationName);
                        auto* rel = program->getRelation(getIncDeltaTupleDeleteRelationName(op.relationName));
                        auto insRel = program->getRelation(getIncDeltaTupleInsertRelationName(op.relationName));
                        // TODO: filter out pending deletions
                        if (rel == nullptr) {
                            std::cout << "Relation not found, omitted: " << op.relationName << std::endl;
                            op.valid = false;
                            continue;
                        }
                        if (op.values.size() != rel->getArity()) {
                            std::cout << "Relation arity mismatch, omitted: " << op.relationName << std::endl;
                            op.valid = false;
                            continue;
                        }
                        souffle::tuple relTuple = souffle::tuple(rel);
                        souffle::tuple origTuple = souffle::tuple{origRel};
                        souffle::tuple insTuple = souffle::tuple{insRel};
                        for (size_t i = 0; i < op.values.size(); i++) {
                            relTuple << std::stoi(op.values[i]);  // TODO optimize
                            origTuple << std::stoi(op.values[i]);
                            insTuple << std::stoi(op.values[i]);
                        }
                        if (!origRel->contains(origTuple)) {
                            std::cout << "Relation does not contains the tuple to delete, omitted: " << origTuple.toString() << std::endl;
                            op.valid = false;
                            continue;
                        }
                        if (rel->contains(relTuple)) {
                            std::cout << "Already deleted the tuple, omitted: " << relTuple.toString() << std::endl;
                            op.valid = false;
                            continue;
                        }
                        rel->insert(relTuple);
                        initialInputRelations[op.relationName].erase(UntypedTuple::fromSouffleTuple(origTuple));
                        fact_prob.erase(UntypedTuple::fromSouffleTuple(origTuple));
                        // should also delete all its derivations...
                        // input fact is possibly derivable
                        auto& deletedFactRuleAppSet = DerivationManager::untypedTuple2RuleApplications[UntypedTuple::fromSouffleTuple(origTuple)];
                        if (deletedFactRuleAppSet != nullptr && !deletedFactRuleAppSet->empty()) {
//                            std::cout << "Deleted tuple: " << origTuple.toString() << std::endl;
                            auto& deltaDeletedFactRuleAppSet =
                                DerivationManager::untypedTuple2DeltaDeleteRuleApplications[UntypedTuple::fromSouffleTuple(origTuple)];
                            if (deltaDeletedFactRuleAppSet == nullptr) {
                                deltaDeletedFactRuleAppSet = new std::unordered_set<RuleApplication>();
                            }
                            for (auto& ruleApp: *deletedFactRuleAppSet) {
//                                std::cout << "Rule application: " << RuleApplication::toString(ruleApp) << std::endl;
                                deltaDeletedFactRuleAppSet->insert(ruleApp);
                            }
                            deletedFactRuleAppSet->clear();
                            DerivationManager::untypedTuple2RuleApplications.erase(UntypedTuple::fromSouffleTuple(origTuple));
                        }
                    }
                }
            }
            dumpInitialInputRelations(opt.getOutputFileDir() + "/initial-input-relations-iter" + std::to_string(iteration) + ".txt");
            if (incMode == IncMode::INC) {
                {
                    debugger.startTurn();
                    debugger.startStage(StageKind::SEMINAIVE_INC);
                    program->runAllInc(program->getInputDirectory(), program->getOutputDirectory(), true);
                    debugger.endStage();
                }
                debugger.startStage(StageKind::PRUNING_INC);
                graph->applyDelta(
                    DerivationManager::untypedTuple2DeltaInsertRuleApplications,
                    DerivationManager::untypedTuple2DeltaDeleteRuleApplications,
                    *ruleManager,
                    getFactProbInc(),// fact_prob_inc; cli should collect this
                    getDeletedFacts() // deletedFacts; cli should collect this
                );
                graph->dumpDotInc("derivation-inc-before-prune" + std::to_string(iteration) + ".dot");
                auto view = graph->prune(program->getOutputRelations());
                debugger.endStage();
                view.dumpDotInc("derivation-inc-after-prune" + std::to_string(iteration) + ".dot");
                changedNodes.clear();
                if (derivationOnly) {
                    debugger.endTurn();
                    iteration++;
                    pendingOperations.clear();
                    return;
                }
                debugger.startStage(StageKind::FORWARD_COMPILATION_INC);
                buildFormulasIncCyclewise(view, *ddManager, *nodeFormulas, *edgeFormulas, changedNodes);  // TODO: should only update the changed ones.
                debugger.endStage();
                {
                    debugger.startStage(StageKind::WEIGHTED_MODEL_COUNTING_INC);
                    FunctionTimer timer("incrementally compute probabilities, size " + std::to_string(changedNodes.size()));
                    std::unordered_map<NodePtr, double> newProbResult;
                    for (const auto& node: view.getValidNodes()) {
                        if (changedNodes.find(node) == changedNodes.end()) {
                            newProbResult[node] = probResult[node];
                        } else {
                            newProbResult[node] = ddManager->computeWeightedModelCount((*nodeFormulas)[node]);
                        }
                    }
                    probResult.clear();
                    probResult = newProbResult;
                    debugger.endStage();
                }
                debugger.endTurn();
                dumpProbabilities(probResult,"./output/","fact-iter" + std::to_string(iteration) + "-inc");
                iteration++;
            } else if (incMode == IncMode::FULL) {
                debugger.startTurn();

//                purgeAllNonIncDeltaRelations();
//                program->loadAllExcept(opt.getInputFileDir());  //
                purgeAllRelations();
                loadInitialInputRelations();
                DerivationManager::untypedTuple2RuleApplications.clear();
                // TODO: write delta inc to original
                debugger.startStage(StageKind::SEMINAIVE_FULL);
                program->runAll(opt.getInputFileDir(), opt.getOutputFileDir(), false);
                graph = IncrementalDerivationGraph::createFrom(DerivationManager::untypedTuple2RuleApplications, *ruleManager, fact_prob);
                debugger.endStage();
                graph->dumpDotInc("derivation-full-before-prune" + std::to_string(iteration) + ".dot");
                debugger.startStage(StageKind::PRUNING_FULL);
                auto view = graph->prune(program->getOutputRelations());
                debugger.endStage();
                view.dumpDotInc("derivation-full-after-prune" + std::to_string(iteration) + ".dot");
                if (derivationOnly) {
                    debugger.endTurn();
                    iteration++;
                    pendingOperations.clear();
                    return;
                }
                // clear formula, build formula
                debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
                nodeFormulas->clear(), edgeFormulas->clear();
                buildFormulasCyclewise(view, *ddManager, *nodeFormulas, *edgeFormulas);
                debugger.endStage();
                // compute probabilities
                debugger.startStage(StageKind::WEIGHTED_MODEL_COUNTING_FULL);
                probResult.clear();
                for (auto& [node, formula]: *nodeFormulas) {
                    probResult[node] = ddManager->computeWeightedModelCount(formula);
                }
                debugger.endStage();
                debugger.startStage(StageKind::IO_DUMP_FULL);
                dumpProbabilities(probResult,"./output/","fact-iter" + std::to_string(iteration) + "-full");
                debugger.endStage();
                debugger.endTurn();
                iteration++;
                // wmc
                // output
            } else if (incMode == IncMode::ELASTIC) {
                // try to decide whether to do incremental or full
                // by approximating the cost of both, etc, TODO
                assert (false);
            } else {
                assert (false);
            }
        } else {
            // preDG -> preDG updating (by delta relations) -> inc-dg
            // -> inc-dg pruning -> inc formula construction -> inc wmc
            // TODO: react to derv-only
            // TODO: react to inc mode
            assert (false && "No program loaded for non-ground program.");
        }
        pendingOperations.clear();
    }

    static IncrementalCLI* instance;
    bool running;
    static void line_handler(char* line) {
        if (!line) {
            instance->running = false;
            std::cout << std::endl;
            return;
        }

        // 将原始的、可能包含多行的输入添加到历史记录（只加一次）
        add_history(line);

        // 将 C 字符串转换为 C++ 字符串流，以便按行分割
        std::stringstream ss(line);
        free(line); // 转换后立刻释放内存

        std::string single_command;
        // 使用 std::getline 循环分割字符串
        while (instance->running && std::getline(ss, single_command)) {
            // 有时行尾会带有\r字符，这里做个简单的清理
            if (!single_command.empty() && single_command.back() == '\r') {
                single_command.pop_back();
            }

            if (!single_command.empty()) {
                // 逐行处理分割出来的命令
                instance->running = instance->processCommand(single_command);
            }
        }
    }


    void run() {
        std::cout << "Incremental Souffle CLI (Callback Version)" << std::endl;
        std::cout << "Type 'help' for a list of available commands" << std::endl;
        IncrementalCLI::instance = this;
        // 1. 安装回调处理器
        //    参数1: 交互式提示符
        //    参数2: 指向我们上面定义的 line_handler 函数的指针
        rl_callback_handler_install("> ", line_handler);

        // 2. 进入主事件循环
        while (this->running) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds); // STDIN_FILENO 是标准输入的文件描述符，通常是 0

            int result = select(STDIN_FILENO + 1, &fds, NULL, NULL, NULL);

            if (result < 0) { // 如果 select 出错
                perror("select"); // 打印错误信息
                break;
            }

            if (FD_ISSET(STDIN_FILENO, &fds)) {
                rl_callback_read_char();
            }
        }

        // 5. 程序即将退出，清理并移除回调处理器
        rl_callback_handler_remove();
    }

};

template <typename T>
IncrementalCLI<T>* IncrementalCLI<T>::instance = nullptr;
#endif //CLI_H
