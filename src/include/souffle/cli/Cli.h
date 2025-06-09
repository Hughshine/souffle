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

std::string getConcreteRelationName(const std::string& name, const std::string prefix) {
    return prefix + name;
}

std::string getIncDeltaTupleDeleteRelationName(const std::string& name) {
    return getConcreteRelationName(name, "$inc_delta_tuple_delete_");
}

std::string getIncDeltaTupleInsertRelationName(const std::string& name) {
    return getConcreteRelationName(name, "$inc_delta_tuple_insert_");
}

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
    std::map<NodePtr, NodeRef>* nodeFormulas;
    std::map<EdgePtr, NodeRef>* edgeFormulas;
    DDManager<NodeRef>* ddManager = nullptr;

public:
    IncrementalCLI(souffle::SouffleProgram* prog = nullptr,
            IncrementalDerivationGraph* graph = nullptr,
            RuleManager* rm = nullptr,
            DDManager<NodeRef>* ddManager = nullptr,
            std::map<NodePtr, NodeRef>* nodeFormulas = {},
            std::map<EdgePtr, NodeRef>* edgeFormulas = {}
            )
            : program(prog), graph(graph), ruleManager(rm), ddManager(ddManager), nodeFormulas(nodeFormulas), edgeFormulas(edgeFormulas) {
        // Initialize readline
        using_history();
    }

    ~IncrementalCLI() {
        // Clean up readline history
        clear_history();
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
        } else if (cmd == "commit") {
            std::cout << "PARSED COMMIT: Would apply " << pendingOperations.size()
                      << " pending changes and run incremental computation" << std::endl;

            // In a real implementation, we would actually apply the changes here
            commit();
            // Clear pending operations after commit
            pendingOperations.clear();

        } else if (cmd == "dump") {
            for (auto rel: this->program->getAllRelations()) {
                std::cout << rel->getName() << std::endl;
                for (auto ele: *rel) {
                    std::cout << ele.toString() << std::endl;
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
    void commit() {
        static size_t commitCount = 0;
        // TODO: should clean all delta relations after each commit
        if (program) {
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
                    } else if (op.type == Operation::DELETE) {
                        auto* origRel = program->getRelation(op.relationName);
                        auto* rel = program->getRelation(getIncDeltaTupleDeleteRelationName(op.relationName));
                        auto insRel = program->getRelation(getIncDeltaTupleInsertRelationName(op.relationName));
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
                        std::cout << origRel->getName() << std::endl;
//                        for (auto t: *origRel) {
//                            std::cout << "original tuple: " << t.toString() << std::endl;
//                        }
                        if (!origRel->contains(origTuple)) {
                            std::cout << "Relation does not contains the tuple to delete, omitted: " << origTuple.toString() << std::endl;
                            op.valid = false;
                            continue;
                        }
                        rel->insert(relTuple);
                    }
                }
            }
            {
                DerivationManager::untypedTuple2DeltaInsertRuleApplications.clear();
                DerivationManager::untypedTuple2DeltaDeleteRuleApplications.clear();
                DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications.clear();
                DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications.clear();
                FunctionTimer timer("runAllInc" + std::to_string(++commitCount));
                std::cout << "runAllInc()..." << std::endl;
                program->runAllInc(program->getInputDirectory(), program->getOutputDirectory(), true);
            }
//            {
//                for (auto ruleapp: DerivationManager::untypedTuple2DeltaInsertRuleApplications) {
//                    std::cout << "insert rule application: " << ruleapp.first.toString() << std::endl;
//                    for (auto t: *(ruleapp.second)) {
//                        std::cout << "insert rule application tuple: " << RuleApplication::toString(t) << std::endl;
//                    }
//                }
//                for (auto ruleapp: DerivationManager::untypedTuple2DeltaDeleteRuleApplications) {
//                    std::cout << "delete rule application: " << ruleapp.first.toString() << std::endl;
//                    for (auto t: *(ruleapp.second)) {
//                        std::cout << "delete rule application tuple: " << RuleApplication::toString(t) << std::endl;
//                    }
//                }
//            }
            graph->applyDelta(
                DerivationManager::untypedTuple2DeltaInsertRuleApplications,
                DerivationManager::untypedTuple2DeltaDeleteRuleApplications,
                *ruleManager,
                getFactProbInc(),// fact_prob_inc; cli should collect this
                getDeletedFacts() // deletedFacts; cli should collect this
            );
            auto view = graph->prune(program->getOutputRelations());
            view.dumpDotInc("derivation-inc.dot");
            for (auto edge: view.getEdges()) {
                std::cout << edge->toString() << std::endl;
            }
            buildFormulasInc(view, *ddManager, *nodeFormulas, *edgeFormulas);  // TODO: should only update the changed ones.
            probResult.clear();
            {
                FunctionTimer timer("incrementally compute probabilities, size " + std::to_string(nodeFormulas->size()));
//                std::ofstream formulaFile("./formula.txt");
                for (const auto& [node, dd] : *nodeFormulas) {
                    if (view.getNodes().find(node) == view.getNodes().end()) {
//                        std::cout << "isNullptr: " << (node == nullptr) << std::endl;
                        std::cout << "Node " << node->toString() << " not in view, skipped." << std::endl;
                        continue;
                    }
//                    formulaFile << "Node" << node->getId() << " " << node->getTuple().toString() << ": ";
//                    formulaFile << bddManager->toString(bdd) << "\t";
                    auto prob = ddManager->computeWeightedModelCount(dd);
//                    formulaFile << "Probability: " << prob << std::endl;
//                    auto prob = bddManager->computeWeightedModelCount(bdd);
                    probResult[node] = prob;
                }
            }
            dumpProbabilities(probResult,"./output/","fact-inc");
//            for (const auto& [edge, bdd] : *edgeFormulas) {
//                std::cout << edge->toString() << " : ";
//                std::cout << bddManager->toString(bdd) << "\t";
//                auto prob = bddManager->computeWeightedModelCount(bdd);
//                std::cout << "Probability: " << prob << std::endl;
//            }
            std::cout << "Done" << std::endl;
        } else {
            std::cout << "No program loaded." << std::endl;
        }
        pendingOperations.clear();

    }
    void run() {
        bool running = true;

        std::cout << "Incremental Souffle CLI (with command history)" << std::endl;
        std::cout << "Type 'help' for a list of available commands" << std::endl;

        while (running) {
            // Use readline to get input with history support
            char* line = readline("> ");

            // Check for EOF
            if (!line) {
                std::cout << std::endl;
                break;
            }

            // Skip empty lines
            if (line[0] != '\0') {
                // Add to readline history
                add_history(line);

                // Process the command
                std::string command(line);
                running = processCommand(command);
            }

            // Free the memory allocated by readline
            free(line);
        }

        std::cout << "Exiting CLI" << std::endl;
    }
};
#endif //CLI_H
