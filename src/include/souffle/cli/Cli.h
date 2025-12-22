#ifndef CLI_H
#define CLI_H
#include <chrono>
#include <ctime>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <regex>
#include <iomanip>
#include <fstream>
#include <unordered_map>
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

inline std::string makeTimestampLabel() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm localTm{};
#if defined(_MSC_VER)
    localtime_s(&localTm, &nowTime);
#else
    if (auto* tmPtr = std::localtime(&nowTime)) {
        localTm = *tmPtr;
    }
#endif
    std::ostringstream oss;
    oss << std::put_time(&localTm, "%Y%m%d-%H%M%S");
    return oss.str();
}

inline std::string makeTimestampedFilename(const std::string& prefix, size_t iteration,
        const std::string& extension) {
    return prefix + std::to_string(iteration) + "-" + makeTimestampLabel() + extension;
}

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
  FULL_HARD,
  FULL_SOFT,
  INC_NAIVE,
  INC_REGIONAL,
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

    void logTurnMode(const std::string& modeLabel) const {
        std::cout << "[inc-iter " << iteration << "] mode=" << modeLabel << std::endl;
    }

    bool isFullMode() const {
        return incMode == IncMode::FULL_HARD || incMode == IncMode::FULL_SOFT;
    }

    const char* fullModeLabel() const {
        return incMode == IncMode::FULL_SOFT ? "FULL-SOFT" : "FULL-HARD";
    }

    std::string outputPath(const std::string& filename) const {
        const std::string& dir = opt.getOutputFileDir();
        if (dir.empty()) {
            return filename;
        }
        if (dir.back() == '/') {
            return dir + filename;
        }
        return dir + "/" + filename;
    }

    std::string outputTimestampedPath(const std::string& prefix, size_t iter,
            const std::string& extension) const {
        return outputPath(makeTimestampedFilename(prefix, iter, extension));
    }

    static std::string formatRatio(size_t part, size_t total) {
        if (total == 0) {
            return "n/a";
        }
        const double pct = 100.0 * static_cast<double>(part) / static_cast<double>(total);
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(2) << pct << "%";
        return oss.str();
    }

    void logPrunedDeltaSummary(const IncSubgraphView& view, const std::string& modeLabel) const {
        const size_t insNodes = view.getDeltaInsertNodes().size();
        const size_t insEdges = view.getDeltaInsertEdges().size();
        const size_t delNodes = view.getDeltaDeleteNodes().size();
        const size_t delEdges = view.getDeltaDeleteEdges().size();
        const size_t totalNodes = view.getNodes().size();
        const size_t totalEdges = view.getEdges().size();
        std::cout << "[inc-iter " << iteration << "] mode=" << modeLabel
                  << " pruned_delta: insNodes=" << insNodes
                  << " insEdges=" << insEdges
                  << " delNodes=" << delNodes
                  << " delEdges=" << delEdges
                  << " totalNodes=" << totalNodes
                  << " totalEdges=" << totalEdges
                  << std::endl;
        std::cout << "[inc-iter " << iteration << "] mode=" << modeLabel
                  << " pruned_delta_ratio: insNodes=" << formatRatio(insNodes, totalNodes)
                  << " insEdges=" << formatRatio(insEdges, totalEdges)
                  << " delNodes=" << formatRatio(delNodes, totalNodes)
                  << " delEdges=" << formatRatio(delEdges, totalEdges)
                  << std::endl;
        if (modeLabel == "INC_REGIONAL") {
            const size_t reachNodes = view.getDeltaInsertReachableNodes().size();
            const size_t reachEdges = view.getDeltaInsertReachableEdges().size();
            std::cout << "[inc-iter " << iteration << "] mode=" << modeLabel
                      << " deltaReach_ratio: insNodes=" << formatRatio(insNodes, reachNodes)
                      << " insEdges=" << formatRatio(insEdges, reachEdges)
                      << " reachNodes=" << reachNodes
                      << " reachEdges=" << reachEdges
                      << std::endl;
        }
    }

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
    QueryManager* queryManager;
//    std::map<NodePtr, BddNodeRef>* nodeFormulas;
    DDManager<NodeRef>* ddManager = nullptr;
    std::map<NodePtr, NodeRef>* nodeFormulas;
    std::map<EdgePtr, NodeRef>* edgeFormulas;
    std::set<NodePtr> changedNodes;


    IncMode incMode = IncMode::INC_NAIVE;
    bool derivationOnly = false;
    bool isGround = false;
public:
    IncrementalCLI(souffle::SouffleProgram* prog = nullptr,
            IncrementalDerivationGraph* graph = nullptr,
            RuleManager* rm = nullptr,
            QueryManager* qm = nullptr,
            DDManager<NodeRef>* ddManager = nullptr,
            std::map<NodePtr, NodeRef>* nodeFormulas = {},
            std::map<EdgePtr, NodeRef>* edgeFormulas = {},
            bool isGround = false,
            PreDerivationGraph* preDG = nullptr
            )
            : program(prog),
              graph(graph),
              ruleManager(rm),
              queryManager(qm),
              ddManager(ddManager),
              nodeFormulas(nodeFormulas),
              edgeFormulas(edgeFormulas),
              changedNodes(),
              isGround(isGround),
              preDG(preDG),
              running(true) {
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
        if (options.isMergeBiImpEnabled()) {
            DerivationGraph::setMergeBiImpEnabled(true);
        } else {
            DerivationGraph::setMergeBiImpEnabled(false);
        }
        DerivationGraphViewInterface::setDumpDotEnabled(options.isDumpDotEnabled());
        DerivationGraphViewInterface::setDumpStatsEnabled(options.isDumpStatEnabled());
        auto& mode = options.getIncMode();
        if (mode == "full" || mode == "full-hard") {
            setIncMode(IncMode::FULL_HARD);
        } else if (mode == "full-soft") {
            setIncMode(IncMode::FULL_SOFT);
        } else if (mode == "inc" || mode == "incremental" || mode == "incr" || mode == "inc-naive") {
            setIncMode(IncMode::INC_NAIVE);
        } else if (mode == "inc-regional" || mode == "regional") {
            setIncMode(IncMode::INC_REGIONAL);
        } else if (mode == "elastic") {
            setIncMode(IncMode::ELASTIC);
        } else {
            std::cerr << "Unknown incremental mode: " << mode << ", defaulting to INC-NAIVE." << std::endl;
            setIncMode(IncMode::INC_NAIVE);
        }
    }
    void setIncMode(IncMode mode) {
        if ((mode == IncMode::INC_NAIVE || mode == IncMode::INC_REGIONAL) &&
                graph != nullptr && graph->isBiImpMerged()) {
            assert(false && "bi-imp merged graph cannot switch to incremental mode");
        }
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
            if (mode == "incremental" || mode == "incr" || mode == "inc-naive" || mode == "inc") {
                incMode = IncMode::INC_NAIVE;
                std::cout << "Set incremental mode to INC-NAIVE" << std::endl;
            } else if (mode == "inc-regional" || mode == "regional") {
                incMode = IncMode::INC_REGIONAL;
                std::cout << "Set incremental mode to INC-REGIONAL" << std::endl;
            } else if (mode == "full" || mode == "full-hard") {
                incMode = IncMode::FULL_HARD;
                std::cout << "Set incremental mode to FULL-HARD" << std::endl;
            } else if (mode == "full-soft") {
                incMode = IncMode::FULL_SOFT;
                std::cout << "Set incremental mode to FULL-SOFT" << std::endl;
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
                std::cout << "Available modes: inc-naive (inc/incr), inc-regional, full (full-hard), full-soft, elastic" << std::endl;
                std::cout << "Current mode unchanged." << std::endl;
            }
        } else if (cmd == "set") {
            std::string key;
            iss >> key;
            if (key == "dumpjson") {
                opt.setDumpJsonEnabled(true);
                std::cout << "Set dumpjson to true" << std::endl;
            } else if (key == "dumpdot") {
                opt.setDumpDotEnabled(true);
                DerivationGraphViewInterface::setDumpDotEnabled(true);
                std::cout << "Set dumpdot to true" << std::endl;
            } else if (key == "dumpstat") {
                opt.setDumpStatEnabled(true);
                DerivationGraphViewInterface::setDumpStatsEnabled(true);
                std::cout << "Set dumpstat to true" << std::endl;
            } else {
                std::cout << "Unknown option: " << key << std::endl;
            }
        } else if (cmd == "unset") {
            std::string key;
            iss >> key;
            if (key == "dumpjson") {
                opt.setDumpJsonEnabled(false);
                std::cout << "Set dumpjson to false" << std::endl;
            } else if (key == "dumpdot") {
                opt.setDumpDotEnabled(false);
                DerivationGraphViewInterface::setDumpDotEnabled(false);
                std::cout << "Set dumpdot to false" << std::endl;
            } else if (key == "dumpstat") {
                opt.setDumpStatEnabled(false);
                DerivationGraphViewInterface::setDumpStatsEnabled(false);
                std::cout << "Set dumpstat to false" << std::endl;
            } else {
                std::cout << "Unknown option: " << key << std::endl;
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
            if ((incMode == IncMode::INC_NAIVE || incMode == IncMode::INC_REGIONAL) &&
                    graph != nullptr && graph->isBiImpMerged()) {
                assert(false && "bi-imp merged graph cannot run incremental mode");
            }
            DerivationGraphViewInterface::setDumpOutputDir(opt.getOutputFileDir());
            debugger.startTurn(isFullMode() ? fullModeLabel() : "DEFAULT");
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
            bool useRegional = incMode == IncMode::INC_REGIONAL;
            if (incMode == IncMode::INC_NAIVE || useRegional) {
                debugger.startStage(StageKind::PRUNING_INC);
                if (opt.isDumpStatEnabled()) {
                    std::cout << "[prune-inc] pre-applyDelta statistics:\n";
                    graph->dumpStatisticsInc(std::cout);
                }
                if (opt.isDumpDotEnabled()) {
                    graph->dumpDotInc(outputPath("derivation-inc-before-prune" + std::to_string(iteration) + ".dot"));
                }
                DerivationGraph::setMergeBiImpEnabled(false);
                auto view = graph->prune(this->outputRelations);
                debugger.endStage();
                if (opt.isDumpDotEnabled()) {
                    view.dumpDotInc(outputPath("derivation-inc-after-prune" + std::to_string(iteration) + ".dot"));
                }
                if (opt.isDumpJsonEnabled()) {
                    view.dumpJsonInc(outputTimestampedPath("derivation-inc-after-prune", iteration, ".json"));
                }
                logPrunedDeltaSummary(view, useRegional ? "INC_REGIONAL" : "INC_NAIVE");
                changedNodes.clear();

                std::cout << view.getDeltaInsertNodes().size() << " nodes with inserted derivations.\n";
                std::cout << view.getDeltaDeleteNodes().size() << " nodes with deleted derivations.\n";
                std::cout << view.getDeltaInsertEdges().size() << " edges with inserted derivations.\n";
                std::cout << view.getDeltaDeleteEdges().size() << " edges with deleted derivations.\n";
                if (derivationOnly) {
                    if (ddManager != nullptr) {
                        ddManager->tryGarbageCollection();
                    }
                    debugger.endTurn();
                    iteration++;
                    pendingOperations.clear();
                    return;
                }
                // knowledge representation
                debugger.startStage(StageKind::FORWARD_COMPILATION_INC);
                if (useRegional) {
                    buildFormulasIncRegionalCyclewise(view, *ddManager, *nodeFormulas, *edgeFormulas, changedNodes);
                } else {
                    buildFormulasIncCyclewise(view, *ddManager, *nodeFormulas, *edgeFormulas, changedNodes);  // TODO: should only update the changed ones.
                }
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

                if (ddManager != nullptr) {
                    ddManager->tryGarbageCollection();
                }
                debugger.endTurn();
                std::string incTag = useRegional ? "-inc-regional" : "-inc-naive";
                std::string incPrefix = "fact-iter" + std::to_string(iteration) + incTag;
                dumpProbabilities(probResult, opt.getOutputFileDir() + "/", incPrefix);
                iteration++;
            } else if (isFullMode()) {
                logTurnMode(fullModeLabel());
                if (ddManager != nullptr) {
                    nodeFormulas->clear();
                    edgeFormulas->clear();
                    if (incMode == IncMode::FULL_HARD) {
                        ddManager->resetHard();
                    } else {
                        ddManager->reset();
                    }
                }
                debugger.startStage(StageKind::PRUNING_FULL);
                {
                    if (opt.isDumpDotEnabled()) {
                        FunctionTimer timer("PRUNING_FULL: dumpDot-before-prune");
                        graph->dumpDotInc(outputPath("derivation-full-before-prune" + std::to_string(iteration) + ".dot"));
                    }
                }
                IncSubgraphView view = [&] {
                    FunctionTimer timer("PRUNING_FULL: prune");
                    DerivationGraph::setMergeBiImpEnabled(true);
                    return graph->prune(this->outputRelations);
                }();
                {
                    if (opt.isDumpDotEnabled()) {
                        FunctionTimer timer("PRUNING_FULL: dumpDot-after-prune");
                        view.dumpDotInc(outputPath("derivation-full-after-prune" + std::to_string(iteration) + ".dot"));
                    }
                }
                {
                    if (opt.isDumpJsonEnabled()) {
                        FunctionTimer timer("PRUNING_FULL: dumpJson-after-prune");
                        view.dumpJsonInc(outputTimestampedPath("derivation-full-after-prune", iteration, ".json"));
                    }
                }
                debugger.endStage();

                if (derivationOnly) {
                    if (ddManager != nullptr) {
                        ddManager->tryGarbageCollection();
                    }
                    debugger.endTurn();
                    iteration++;
                    pendingOperations.clear();
                    return;
                }
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
                std::string basePrefix = "fact-iter" + std::to_string(iteration);
                dumpProbabilities(probResult, opt.getOutputFileDir() + "/", basePrefix + "-full");
                debugger.endStage();
                if (ddManager != nullptr) {
                    ddManager->tryGarbageCollection();
                }
                debugger.endTurn();
                iteration++;
            } else if (incMode == IncMode::ELASTIC) {
                // TODO
                assert (false);
            }
        }
        else if (!isGround && program) {  // for non-ground program ...
            if ((incMode == IncMode::INC_NAIVE || incMode == IncMode::INC_REGIONAL) &&
                    graph != nullptr && graph->isBiImpMerged()) {
                assert(false && "bi-imp merged graph cannot run incremental mode");
            }
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
            bool useRegional = incMode == IncMode::INC_REGIONAL;
            if (incMode == IncMode::INC_NAIVE || useRegional) {
                {
                    debugger.startTurn();
                    debugger.startStage(StageKind::SEMINAIVE_INC);
                    program->runAllInc(program->getInputDirectory(), program->getOutputDirectory(), true);
                    debugger.endStage();
                }
                DerivationGraphViewInterface::setDumpOutputDir(opt.getOutputFileDir());
                debugger.startStage(StageKind::PRUNING_INC);
                if (opt.isDumpStatEnabled()) {
                    std::cout << "[prune-inc] pre-applyDelta statistics:\n";
                    graph->dumpStatisticsInc(std::cout);
                }
                {
                    FunctionTimer timer("PRUNING_INC: applyDelta");
                    graph->applyDelta(
                        DerivationManager::untypedTuple2DeltaInsertRuleApplications,
                        DerivationManager::untypedTuple2DeltaDeleteRuleApplications,
                        *ruleManager,
                        getFactProbInc(),// fact_prob_inc; cli should collect this
                        getDeletedFacts() // deletedFacts; cli should collect this
                    );
                }
                {
                    if (opt.isDumpDotEnabled()) {
                        FunctionTimer timer("PRUNING_INC: dumpDot-before-prune");
                        graph->dumpDotInc(outputPath("derivation-inc-before-prune" + std::to_string(iteration) + ".dot"));
                    }
                }
                IncSubgraphView view = [&] {
                    FunctionTimer timer("PRUNING_INC: prune");
                    DerivationGraph::setMergeBiImpEnabled(false);
                    return graph->prune(program->getOutputRelations());
                }();  // will be assigned below
                {
                    if (opt.isDumpDotEnabled()) {
                        FunctionTimer timer("PRUNING_INC: dumpDot-after-prune");
                        view.dumpDotInc(outputPath("derivation-inc-after-prune" + std::to_string(iteration) + ".dot"));
                    }
                }
                {
                    if (opt.isDumpJsonEnabled()) {
                        FunctionTimer timer("PRUNING_INC: dumpJson-after-prune");
                        view.dumpJsonInc(outputTimestampedPath("derivation-inc-after-prune", iteration, ".json"));
                    }
                }
                debugger.endStage();
                logPrunedDeltaSummary(view, useRegional ? "INC_REGIONAL" : "INC_NAIVE");
                changedNodes.clear();
                if (derivationOnly) {
                    if (ddManager != nullptr) {
                        ddManager->tryGarbageCollection();
                    }
                    debugger.endTurn();
                    iteration++;
                    pendingOperations.clear();
                    return;
                }
                debugger.startStage(StageKind::FORWARD_COMPILATION_INC);
                if (useRegional) {
                    buildFormulasIncRegionalCyclewise(view, *ddManager, *nodeFormulas, *edgeFormulas, changedNodes);
                } else {
                    buildFormulasIncCyclewise(view, *ddManager, *nodeFormulas, *edgeFormulas, changedNodes);  // TODO: should only update the changed ones.
                }
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
                if (ddManager != nullptr) {
                    ddManager->tryGarbageCollection();
                }
                debugger.endTurn();
                std::string incTag = useRegional ? "-inc-regional" : "-inc-naive";
                std::string incPrefix = "fact-iter" + std::to_string(iteration) + incTag;
                dumpProbabilities(probResult, opt.getOutputFileDir() + "/", incPrefix);
                iteration++;
            } else if (isFullMode()) {
                debugger.startTurn(fullModeLabel());
                DerivationGraphViewInterface::setDumpOutputDir(opt.getOutputFileDir());
                logTurnMode(fullModeLabel());
                if (ddManager != nullptr) {
                    nodeFormulas->clear();
                    edgeFormulas->clear();
                    if (incMode == IncMode::FULL_HARD) {
                        ddManager->resetHard();
                    } else {
                        ddManager->reset();
                    }
                }

//                purgeAllNonIncDeltaRelations();
//                program->loadAllExcept(opt.getInputFileDir());  //
                purgeAllRelations();
                loadInitialInputRelations();
                DerivationManager::untypedTuple2RuleApplications.clear();
                // TODO: write delta inc to original
                debugger.startStage(StageKind::SEMINAIVE_FULL);
                program->runAll(opt.getInputFileDir(), opt.getOutputFileDir(), false);
                graph = IncrementalDerivationGraph::createFrom(DerivationManager::untypedTuple2RuleApplications, *ruleManager, *queryManager, fact_prob);
                debugger.endStage();
                {
                    if (opt.isDumpDotEnabled()) {
                        FunctionTimer timer("PRUNING_FULL: dumpDot-before-prune");
                        graph->dumpDotInc(outputPath("derivation-full-before-prune" + std::to_string(iteration) + ".dot"));
                    }
                }
                debugger.startStage(StageKind::PRUNING_FULL);
                IncSubgraphView view = [&] {
                    FunctionTimer timer("PRUNING_FULL: prune");
                    DerivationGraph::setMergeBiImpEnabled(true);
                    return graph->prune(program->getOutputRelations());
                }();
                {
                    if (opt.isDumpDotEnabled()) {
                        FunctionTimer timer("PRUNING_FULL: dumpDot-after-prune");
                        view.dumpDotInc(outputPath("derivation-full-after-prune" + std::to_string(iteration) + ".dot"));
                    }
                }
                {
                    if (opt.isDumpJsonEnabled()) {
                        FunctionTimer timer("PRUNING_FULL: dumpJson-after-prune");
                        view.dumpJsonInc(outputTimestampedPath("derivation-full-after-prune", iteration, ".json"));
                    }
                }
                debugger.endStage();
                if (derivationOnly) {
                    if (ddManager != nullptr) {
                        ddManager->tryGarbageCollection();
                    }
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
                std::string basePrefix = "fact-iter" + std::to_string(iteration);
                dumpProbabilities(probResult, opt.getOutputFileDir() + "/", basePrefix + "-full");
                debugger.endStage();
                if (ddManager != nullptr) {
                    ddManager->tryGarbageCollection();
                }
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
        if (!isatty(STDIN_FILENO)) {
            std::string line;
            while (this->running && std::getline(std::cin, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                if (!line.empty()) {
                    this->running = this->processCommand(line);
                }
            }
            return;
        }
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
