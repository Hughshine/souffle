#ifndef CLI_H
#define CLI_H
#include <chrono>
#include <ctime>
#include <array>
#include <algorithm>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <regex>
#include <iomanip>
#include <fstream>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <memory>
#include <stdexcept>
#include <readline/readline.h>
#include <readline/history.h>
#include "souffle/SouffleInterface.h"
#include "souffle/Derivation.h" // TODO: should change timer to Misc header
#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/RuleManager.h"
#include "souffle/problog/formula/CuddManager.h"
#include "souffle/problog/ForwardCompilation.h"
#include "souffle/CompiledOptions.h"
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

enum class SemMode {
    INC,
    FULL
};

enum class FcMode {
    FULL_HARD,
    FULL_SOFT,
    INC_NAIVE,
    INC_REGIONAL,
    ELASTIC
};

enum class WmcMode {
    FULL,
    INC_NAIVE,
    INC_REGIONAL
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

    bool isIncrementalSemMode() const {
        return semMode == SemMode::INC;
    }

    bool isFullSemMode() const {
        return semMode == SemMode::FULL;
    }

    bool isIncrementalFcMode() const {
        return fcMode == FcMode::INC_NAIVE || fcMode == FcMode::INC_REGIONAL;
    }

    bool isFullFcMode() const {
        return fcMode == FcMode::FULL_HARD || fcMode == FcMode::FULL_SOFT;
    }

    bool isRegionalFcMode() const {
        return fcMode == FcMode::INC_REGIONAL;
    }

    bool isElasticFcMode() const {
        return fcMode == FcMode::ELASTIC;
    }

    const char* semModeLabel() const {
        return semMode == SemMode::FULL ? "SEM-FULL" : "SEM-INC";
    }

    const char* fcModeLabel() const {
        switch (fcMode) {
            case FcMode::FULL_HARD:
                return "FULL-HARD";
            case FcMode::FULL_SOFT:
                return "FULL-SOFT";
            case FcMode::INC_NAIVE:
                return "INC-NAIVE";
            case FcMode::INC_REGIONAL:
                return "INC-REGIONAL";
            case FcMode::ELASTIC:
                return "ELASTIC";
        }
        return "UNKNOWN";
    }

    WmcMode getWmcMode() const {
        switch (fcMode) {
            case FcMode::FULL_HARD:
            case FcMode::FULL_SOFT:
                return WmcMode::FULL;
            case FcMode::INC_NAIVE:
                return WmcMode::INC_NAIVE;
            case FcMode::INC_REGIONAL:
                return WmcMode::INC_REGIONAL;
            case FcMode::ELASTIC:
                return WmcMode::FULL;
        }
        return WmcMode::FULL;
    }

    const char* wmcModeLabel() const {
        switch (getWmcMode()) {
            case WmcMode::FULL:
                return "WMC-FULL";
            case WmcMode::INC_NAIVE:
                return "WMC-INC-NAIVE";
            case WmcMode::INC_REGIONAL:
                return "WMC-INC-REGIONAL";
        }
        return "WMC-UNKNOWN";
    }

    std::string modeSummaryLabel() const {
        std::ostringstream oss;
        oss << semModeLabel() << "+" << fcModeLabel() << "+" << wmcModeLabel();
        return oss.str();
    }

    const char* debuggerTurnModeLabel() const {
        if (isFullSemMode()) {
            if (fcMode == FcMode::FULL_SOFT) {
                return "FULL-SOFT";
            }
            if (fcMode == FcMode::FULL_HARD) {
                return "FULL-HARD";
            }
            return "FULL";
        }
        return "INC";
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

    void logApplyDeltaSummary(const IncrementalDerivationGraph& graph, const std::string& modeLabel) const {
        const size_t insNodes = graph.getDeltaInsertNodes().size();
        const size_t insEdges = graph.getDeltaInsertEdges().size();
        const size_t delNodes = graph.getDeltaDeleteNodes().size();
        const size_t delEdges = graph.getDeltaDeleteEdges().size();
        std::ostringstream oss;
        oss << "[inc-iter " << iteration << "] mode=" << modeLabel
            << " apply_delta_view: insNodes=" << insNodes
            << " insEdges=" << insEdges
            << " delNodes=" << delNodes
            << " delEdges=" << delEdges;
        std::cout << oss.str() << std::endl;
        debugger.logMessage(Level::INFO, oss.str());
    }

    void logApplyDeltaGraphSummary(const IncrementalDerivationGraph& graph, const std::string& modeLabel) const {
        const size_t totalNodes = graph.getNodes().size();
        const size_t totalEdges = graph.getEdges().size();
        std::ostringstream oss;
        oss << "[inc-iter " << iteration << "] mode=" << modeLabel
            << " apply_delta_graph: totalNodes=" << totalNodes
            << " totalEdges=" << totalEdges;
        std::cout << oss.str() << std::endl;
        debugger.logMessage(Level::INFO, oss.str());
    }

    void logApplyDeltaOpsSummary(
            const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& deltaInsertRuleApps,
            const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& deltaDeleteRuleApps,
            const std::unordered_map<UntypedTuple, double>& factProb,
            const std::vector<UntypedTuple>& deletedFacts,
            const std::string& modeLabel) const {
        auto countRuleApps = [](const auto& m) {
            size_t total = 0;
            for (const auto& [_, s] : m) {
                total += s ? s->size() : 0;
            }
            return total;
        };
        const size_t delTuples = deltaDeleteRuleApps.size();
        const size_t delRuleApps = countRuleApps(deltaDeleteRuleApps);
        const size_t delFacts = deletedFacts.size();
        const size_t insTuples = deltaInsertRuleApps.size();
        const size_t insRuleApps = countRuleApps(deltaInsertRuleApps);
        const size_t insFacts = factProb.size();
        std::ostringstream oss;
        oss << "[inc-iter " << iteration << "] mode=" << modeLabel
            << " apply_delta_ops: delTuples=" << delTuples
            << " delRuleApps=" << delRuleApps
            << " delFacts=" << delFacts
            << " insTuples=" << insTuples
            << " insRuleApps=" << insRuleApps
            << " insFacts=" << insFacts;
        std::cout << oss.str() << std::endl;
        debugger.logMessage(Level::INFO, oss.str());
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
        // NOTE: Temporarily disabled because deltaInsertReachableNodes/Edges are not populated yet.
        // Re-enable once reachability is computed during pruning or elsewhere.
        // if (modeLabel == "INC_REGIONAL") {
        //     const size_t reachNodes = view.getDeltaInsertReachableNodes().size();
        //     const size_t reachEdges = view.getDeltaInsertReachableEdges().size();
        //     std::cout << "[inc-iter " << iteration << "] mode=" << modeLabel
        //               << " deltaReach_ratio: insNodes=" << formatRatio(insNodes, reachNodes)
        //               << " insEdges=" << formatRatio(insEdges, reachEdges)
        //               << " reachNodes=" << reachNodes
        //               << " reachEdges=" << reachEdges
        //               << std::endl;
        // }
    }

    static std::unordered_map<UntypedTuple, NodePtr> buildNodeTupleIndex(
            const std::unordered_set<NodePtr>& nodes) {
        std::unordered_map<UntypedTuple, NodePtr> byTuple;
        byTuple.reserve(nodes.size());
        for (const auto& node : nodes) {
            if (!node) {
                continue;
            }
            byTuple.emplace(node->getTuple(), node);
        }
        return byTuple;
    }

    static std::map<EdgeKey, EdgePtr> buildEdgeKeyIndex(const std::unordered_set<EdgePtr>& edges) {
        std::map<EdgeKey, EdgePtr> byKey;
        for (const auto& edge : edges) {
            if (!edge) {
                continue;
            }
            byKey.emplace(edge->getEdgeKey(), edge);
        }
        return byKey;
    }

    IncSubgraphView buildPostPruneDiffView(
            const IncSubgraphView& oldView,
            const IncSubgraphView& newView,
            std::unordered_map<NodePtr, NodePtr>& oldToNewNodes,
            std::unordered_map<EdgePtr, EdgePtr>& oldToNewEdges) const {
        oldToNewNodes.clear();
        oldToNewEdges.clear();

        const auto oldNodeByTuple = buildNodeTupleIndex(oldView.getNodes());
        const auto newNodeByTuple = buildNodeTupleIndex(newView.getNodes());
        const auto oldEdgeByKey = buildEdgeKeyIndex(oldView.getEdges());
        const auto newEdgeByKey = buildEdgeKeyIndex(newView.getEdges());

        std::set<NodePtr> deltaInsertNodes;
        std::set<EdgePtr> deltaInsertEdges;
        std::set<NodePtr> deltaDeleteNodes;
        std::set<EdgePtr> deltaDeleteEdges;
        std::set<NodePtr> deltaInsertFactNodes;
        std::set<NodePtr> explicitDeletedFacts;
        std::vector<NodePtr> deletedOutputNodes;

        oldToNewNodes.reserve(oldNodeByTuple.size());
        oldToNewEdges.reserve(oldEdgeByKey.size());
        deletedOutputNodes.reserve(oldNodeByTuple.size());

        for (const auto& [tuple, oldNode] : oldNodeByTuple) {
            auto itNew = newNodeByTuple.find(tuple);
            if (itNew == newNodeByTuple.end()) {
                deltaDeleteNodes.insert(oldNode);
                if (oldNode->isFact) {
                    explicitDeletedFacts.insert(oldNode);
                }
                if (oldNode->needOutput || oldNode->isQueryNode()) {
                    deletedOutputNodes.push_back(oldNode);
                }
                continue;
            }
            oldToNewNodes.emplace(oldNode, itNew->second);
        }

        for (const auto& [tuple, newNode] : newNodeByTuple) {
            if (!oldNodeByTuple.count(tuple)) {
                deltaInsertNodes.insert(newNode);
                if (newNode->isFact) {
                    deltaInsertFactNodes.insert(newNode);
                }
            }
        }

        for (const auto& [key, oldEdge] : oldEdgeByKey) {
            auto itNew = newEdgeByKey.find(key);
            if (itNew == newEdgeByKey.end()) {
                deltaDeleteEdges.insert(oldEdge);
                continue;
            }
            oldToNewEdges.emplace(oldEdge, itNew->second);
        }

        for (const auto& [key, newEdge] : newEdgeByKey) {
            if (!oldEdgeByKey.count(key)) {
                deltaInsertEdges.insert(newEdge);
            }
        }

        std::unordered_set<NodePtr> liveNodes = newView.getNodes();
        std::unordered_set<EdgePtr> liveEdges = newView.getEdges();
        std::vector<NodePtr> outputNodes = newView.getOutputNodes();
        std::vector<NodePtr> evidenceNodes = newView.getEvidenceNodes();

        std::cout << "[inc-full-diff] pruned-diff"
                  << " insNodes=" << deltaInsertNodes.size()
                  << " insEdges=" << deltaInsertEdges.size()
                  << " delNodes=" << deltaDeleteNodes.size()
                  << " delEdges=" << deltaDeleteEdges.size()
                  << " oldNodes=" << oldNodeByTuple.size()
                  << " oldEdges=" << oldEdgeByKey.size()
                  << " newNodes=" << newNodeByTuple.size()
                  << " newEdges=" << newEdgeByKey.size()
                  << std::endl;

        return IncSubgraphView(std::move(liveNodes), std::move(liveEdges), std::move(deltaInsertNodes),
                std::move(deltaInsertEdges), std::move(deltaDeleteNodes), std::move(deltaDeleteEdges),
                std::move(deltaInsertFactNodes), {}, {}, {}, {}, {}, {}, {}, {}, {}, {},
                std::move(explicitDeletedFacts), std::move(deletedOutputNodes), std::move(outputNodes),
                std::move(evidenceNodes));
    }

    void remapStateForPostPruneDiff(const std::unordered_map<NodePtr, NodePtr>& oldToNewNodes,
            const std::unordered_map<EdgePtr, EdgePtr>& oldToNewEdges) {
        std::map<NodePtr, NodeRef> remappedNodeFormulas;
        std::map<EdgePtr, NodeRef> remappedEdgeFormulas;
        std::unordered_map<NodePtr, double> remappedProbResult;
        std::unordered_map<NodePtr, double> remappedPrecomputed;
        remappedProbResult.reserve(oldToNewNodes.size());
        remappedPrecomputed.reserve(oldToNewNodes.size());
        size_t reusedNodeFormulas = 0;
        size_t reusedEdgeFormulas = 0;
        size_t reusedProbNodes = 0;
        size_t reusedPrecomputedNodes = 0;
        size_t reboundNodeIndices = 0;
        size_t reboundEdgeIndices = 0;

        for (const auto& [oldNode, newNode] : oldToNewNodes) {
            auto nodeIt = nodeFormulas->find(oldNode);
            if (nodeIt != nodeFormulas->end()) {
                remappedNodeFormulas[newNode] = nodeIt->second;
                reusedNodeFormulas++;
            }
            auto probIt = probResult.find(oldNode);
            if (probIt != probResult.end()) {
                remappedProbResult[newNode] = probIt->second;
                reusedProbNodes++;
            }
            auto preIt = precomputedProbResult.find(oldNode);
            if (preIt != precomputedProbResult.end()) {
                remappedPrecomputed[newNode] = preIt->second;
                reusedPrecomputedNodes++;
            }
            if (ddManager != nullptr) {
                int idx = -1;
                if (ddManager->peekVarIndex(*oldNode, idx)) {
                    ddManager->bindVarIndex(*newNode, idx);
                    reboundNodeIndices++;
                }
            }
        }

        for (const auto& [oldEdge, newEdge] : oldToNewEdges) {
            auto edgeIt = edgeFormulas->find(oldEdge);
            if (edgeIt != edgeFormulas->end()) {
                remappedEdgeFormulas[newEdge] = edgeIt->second;
                reusedEdgeFormulas++;
            }
            if (ddManager != nullptr) {
                int idx = -1;
                if (ddManager->peekVarIndex(*oldEdge, idx)) {
                    ddManager->bindVarIndex(*newEdge, idx);
                    reboundEdgeIndices++;
                }
            }
        }

        nodeFormulas->swap(remappedNodeFormulas);
        edgeFormulas->swap(remappedEdgeFormulas);
        probResult.swap(remappedProbResult);
        precomputedProbResult.swap(remappedPrecomputed);

        std::cout << "[inc-full-diff] remap"
                  << " nodeFormulas=" << reusedNodeFormulas
                  << " edgeFormulas=" << reusedEdgeFormulas
                  << " probNodes=" << reusedProbNodes
                  << " precomputedNodes=" << reusedPrecomputedNodes
                  << " nodeVarRebind=" << reboundNodeIndices
                  << " edgeVarRebind=" << reboundEdgeIndices
                  << std::endl;
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


    SemMode semMode = SemMode::INC;
    FcMode fcMode = FcMode::INC_NAIVE;
    bool derivationOnly = false;
public:
    IncrementalCLI(souffle::SouffleProgram* prog = nullptr,
            IncrementalDerivationGraph* graph = nullptr,
            RuleManager* rm = nullptr,
            QueryManager* qm = nullptr,
            DDManager<NodeRef>* ddManager = nullptr,
            std::map<NodePtr, NodeRef>* nodeFormulas = {},
            std::map<EdgePtr, NodeRef>* edgeFormulas = {}
            )
            : program(prog),
              graph(graph),
              ruleManager(rm),
              queryManager(qm),
              ddManager(ddManager),
              nodeFormulas(nodeFormulas),
              edgeFormulas(edgeFormulas),
              changedNodes(),
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
        DerivationGraph::setMergeBiImpEnabled(false);
        DerivationGraph::setConstFoldEnabled(options.isConstFoldEnabled());
        DerivationGraph::setConstDumpEnabled(options.isDumpConstEnabled());
        DerivationGraphViewInterface::setDumpDotEnabled(options.isDumpDotEnabled());
        DerivationGraphViewInterface::setDumpJsonEnabled(options.isDumpJsonEnabled());
        DerivationGraphViewInterface::setDumpStatsEnabled(options.isDumpStatEnabled());
        DerivationManager::setSemStatsEnabled(options.isDumpStatEnabled());
        incProfileEnabled = options.isIncProfileEnabled();
        fcProfileEnabled = options.isFcProfileEnabled();
        incDeleteProfileEnabled = options.isIncDeleteProfileEnabled();
        wmcProfileEnabled = options.isWmcProfileEnabled();
        incRegionalProfileEnabled = options.isIncRegionalProfileEnabled();
        incRegionalProfileHeavyEnabled = options.isIncRegionalProfileHeavyEnabled();
        incRegionalTraceTuples = options.getIncRegionalTraceTuples();
        depGraphProfileEnabled = options.isDepGraphProfileEnabled();
        postDelEnabled = options.isPostDelEnabled();
        reuseVarIndexEnabled = options.isReuseVarIndexEnabled();
        incReorderEnabled = options.isIncReorderEnabled();
        auto& mode = options.getIncMode();
        if (!setLegacyMode(mode, false)) {
            std::cerr << "Unknown incremental mode: " << mode << ", defaulting to inc." << std::endl;
            setLegacyMode("inc", false);
        }
    }

    static bool parseLegacyModeSpec(const std::string& token, SemMode& sem, FcMode& fc) {
        if (token == "inc" || token == "incremental" || token == "incr" || token == "inc-naive") {
            sem = SemMode::INC;
            fc = FcMode::INC_NAIVE;
            return true;
        }
        if (token == "inc-regional" || token == "regional") {
            sem = SemMode::INC;
            fc = FcMode::INC_REGIONAL;
            return true;
        }
        if (token == "full" || token == "full-hard") {
            sem = SemMode::FULL;
            fc = FcMode::FULL_HARD;
            return true;
        }
        if (token == "full-soft") {
            sem = SemMode::FULL;
            fc = FcMode::FULL_SOFT;
            return true;
        }
        if (token == "elastic") {
            sem = SemMode::INC;
            fc = FcMode::ELASTIC;
            return true;
        }
        return false;
    }

    static bool parseSemModeSpec(const std::string& token, SemMode& sem) {
        if (token == "inc" || token == "incremental" || token == "incr" || token == "inc-naive" ||
                token == "inc-regional" || token == "regional") {
            sem = SemMode::INC;
            return true;
        }
        if (token == "full" || token == "full-hard" || token == "full-soft") {
            sem = SemMode::FULL;
            return true;
        }
        return false;
    }

    static bool parseFcModeSpec(const std::string& token, FcMode& fc) {
        if (token == "full" || token == "full-hard") {
            fc = FcMode::FULL_HARD;
            return true;
        }
        if (token == "full-soft") {
            fc = FcMode::FULL_SOFT;
            return true;
        }
        if (token == "inc" || token == "incremental" || token == "incr" || token == "inc-naive") {
            fc = FcMode::INC_NAIVE;
            return true;
        }
        if (token == "inc-regional" || token == "regional") {
            fc = FcMode::INC_REGIONAL;
            return true;
        }
        if (token == "elastic") {
            fc = FcMode::ELASTIC;
            return true;
        }
        return false;
    }

    void setModes(SemMode sem, FcMode fc) {
        if ((sem == SemMode::INC || fc == FcMode::INC_NAIVE || fc == FcMode::INC_REGIONAL) &&
                graph != nullptr && graph->isBiImpMerged()) {
            assert(false && "bi-imp merged graph cannot switch to incremental mode");
        }
        if ((sem == SemMode::INC || fc == FcMode::INC_NAIVE || fc == FcMode::INC_REGIONAL) &&
                graph != nullptr && graph->isConstFolded()) {
            assert(false && "const-folded graph cannot switch to incremental mode");
        }
        semMode = sem;
        fcMode = fc;
    }

    bool setLegacyMode(const std::string& token, bool print = true) {
        SemMode sem = semMode;
        FcMode fc = fcMode;
        if (!parseLegacyModeSpec(token, sem, fc)) {
            return false;
        }
        setModes(sem, fc);
        if (print) {
            std::cout << "Set mode to " << modeSummaryLabel() << std::endl;
        }
        return true;
    }

    void setDerivationOnly(bool val) {
        derivationOnly = val;
    }

private:
    std::vector<std::pair<NodePtr, bool>> resolveEvidenceNodes() const {
        if (!graph) {
            throw std::runtime_error("IncrementalCLI: graph is null for evidence resolution");
        }
        return graph->resolveEvidenceNodes();
    }

    NodeRef buildEvidenceFormula(const std::vector<std::pair<NodePtr, bool>>& resolved) const {
        NodeRef evidenceNode = ddManager->getTrue();
        for (const auto& [node, val] : resolved) {
            auto it = nodeFormulas->find(node);
            if (it == nodeFormulas->end()) {
                throw std::runtime_error("Evidence node has no formula: " + node->getTuple().toString());
            }
            NodeRef lit = it->second;
            if (!val) {
                lit = ddManager->makeNot(lit);
            }
            evidenceNode = ddManager->makeAnd(evidenceNode, lit);
        }
        return evidenceNode;
    }

public:
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
            std::vector<std::string> specs;
            std::string token;
            while (iss >> token) {
                size_t begin = 0;
                while (begin <= token.size()) {
                    size_t comma = token.find(',', begin);
                    const size_t end = (comma == std::string::npos) ? token.size() : comma;
                    if (end > begin) {
                        specs.push_back(token.substr(begin, end - begin));
                    }
                    if (comma == std::string::npos) {
                        break;
                    }
                    begin = comma + 1;
                }
            }
            if (specs.empty()) {
                std::cout << "Usage: setmode <legacy-mode> OR setmode sem=<inc|full> fc=<full-hard|full-soft|inc-naive|inc-regional>" << std::endl;
                std::cout << "Current mode: " << modeSummaryLabel() << std::endl;
                return true;
            }
            if (specs.size() == 1 && specs[0].find('=') == std::string::npos) {
                if (!setLegacyMode(specs[0], true)) {
                    std::cout << "Unknown mode: " << specs[0] << std::endl;
                    std::cout << "Available legacy modes: inc-naive (inc/incr), inc-regional, full (full-hard), full-soft, elastic" << std::endl;
                    std::cout << "Current mode unchanged: " << modeSummaryLabel() << std::endl;
                }
                return true;
            }
            auto trim = [](std::string s) {
                const auto first = s.find_first_not_of(" \t\r\n");
                if (first == std::string::npos) {
                    return std::string();
                }
                const auto last = s.find_last_not_of(" \t\r\n");
                return s.substr(first, last - first + 1);
            };
            SemMode nextSem = semMode;
            FcMode nextFc = fcMode;
            bool hasSem = false;
            bool hasFc = false;
            bool ok = true;
            for (const auto& specRaw : specs) {
                const std::string spec = trim(specRaw);
                const auto eq = spec.find('=');
                if (eq == std::string::npos) {
                    std::cout << "Invalid setmode item: " << spec << " (expected key=value)" << std::endl;
                    ok = false;
                    break;
                }
                const std::string key = trim(spec.substr(0, eq));
                const std::string value = trim(spec.substr(eq + 1));
                if (key == "sem") {
                    SemMode semTmp = nextSem;
                    if (!parseSemModeSpec(value, semTmp)) {
                        std::cout << "Unknown sem mode: " << value << std::endl;
                        ok = false;
                        break;
                    }
                    nextSem = semTmp;
                    hasSem = true;
                } else if (key == "fc") {
                    FcMode fcTmp = nextFc;
                    if (!parseFcModeSpec(value, fcTmp)) {
                        std::cout << "Unknown fc mode: " << value << std::endl;
                        ok = false;
                        break;
                    }
                    nextFc = fcTmp;
                    hasFc = true;
                } else if (key == "wmc") {
                    if (value != "follow" && value != "fc" && value != "auto") {
                        std::cout << "Unsupported wmc mode: " << value << " (wmc follows fc in current design)" << std::endl;
                        ok = false;
                        break;
                    }
                } else {
                    std::cout << "Unknown setmode key: " << key << std::endl;
                    ok = false;
                    break;
                }
            }
            if (ok && hasSem && !hasFc) {
                if (nextSem == SemMode::FULL) {
                    if (!isFullFcMode()) {
                        nextFc = FcMode::FULL_HARD;
                    }
                } else {
                    if (!isIncrementalFcMode()) {
                        nextFc = FcMode::INC_NAIVE;
                    }
                }
            }
            if (ok) {
                setModes(nextSem, nextFc);
                std::cout << "Set mode to " << modeSummaryLabel() << std::endl;
            } else {
                std::cout << "Current mode unchanged: " << modeSummaryLabel() << std::endl;
            }
        } else if (cmd == "set") {
            std::string key;
            iss >> key;
            if (key == "dumpjson") {
                opt.setDumpJsonEnabled(true);
                DerivationGraphViewInterface::setDumpJsonEnabled(true);
                std::cout << "Set dumpjson to true" << std::endl;
            } else if (key == "dumpdot") {
                opt.setDumpDotEnabled(true);
                DerivationGraphViewInterface::setDumpDotEnabled(true);
                std::cout << "Set dumpdot to true" << std::endl;
            } else if (key == "dumpstat") {
                opt.setDumpStatEnabled(true);
                DerivationGraphViewInterface::setDumpStatsEnabled(true);
                DerivationManager::setSemStatsEnabled(true);
                std::cout << "Set dumpstat to true" << std::endl;
            } else {
                std::cout << "Unknown option: " << key << std::endl;
            }
        } else if (cmd == "unset") {
            std::string key;
            iss >> key;
            if (key == "dumpjson") {
                opt.setDumpJsonEnabled(false);
                DerivationGraphViewInterface::setDumpJsonEnabled(false);
                std::cout << "Set dumpjson to false" << std::endl;
            } else if (key == "dumpdot") {
                opt.setDumpDotEnabled(false);
                DerivationGraphViewInterface::setDumpDotEnabled(false);
                std::cout << "Set dumpdot to false" << std::endl;
            } else if (key == "dumpstat") {
                opt.setDumpStatEnabled(false);
                DerivationGraphViewInterface::setDumpStatsEnabled(false);
                DerivationManager::setSemStatsEnabled(false);
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
            assert(this->program != nullptr);
            for (auto rel : this->program->getAllRelations()) {
                std::cout << rel->getName() << std::endl;
                for (auto ele : *rel) {
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
        if (detOptEnabled) {
            const std::string prefix = "$inc_delta_tuple_insert_";
            for (auto* rel : program->getAllRelations()) {
                const std::string& relName = rel->getName();
                if (relName.rfind(prefix, 0) != 0) {
                    continue;
                }
                const std::string baseName = relName.substr(prefix.size());
                if (!isDetRelation(baseName)) {
                    continue;
                }
                const auto arity = rel->getArity();
                for (auto& tuple : *rel) {
                    UntypedTuple detTuple{baseName, {}};
                    detTuple.fields.reserve(arity);
                    for (size_t i = 0; i < arity; ++i) {
                        detTuple.fields.push_back(tuple[i]);
                    }
                    if (fact_prob_inc.find(detTuple) == fact_prob_inc.end()) {
                        fact_prob_inc.emplace(std::move(detTuple), 1.0);
                    }
                }
            }
        }
        return fact_prob_inc;
    }

    std::vector<UntypedTuple> getDeletedFacts(
            const std::unordered_map<UntypedTuple, double>* insertedFacts = nullptr) {
        std::unordered_set<UntypedTuple> deletedFacts;
        deletedFacts.reserve(pendingOperations.size());
        for (const auto& op : pendingOperations) {
            if (op.valid && op.type == Operation::DELETE) {
                deletedFacts.insert(getTuple(op));
            }
        }
        if (detOptEnabled) {
            const std::string prefix = "$inc_delta_tuple_delete_";
            for (auto* rel : program->getAllRelations()) {
                const std::string& relName = rel->getName();
                if (relName.rfind(prefix, 0) != 0) {
                    continue;
                }
                const std::string baseName = relName.substr(prefix.size());
                if (!isDetRelation(baseName)) {
                    continue;
                }
                const auto arity = rel->getArity();
                for (auto& tuple : *rel) {
                    UntypedTuple detTuple{baseName, {}};
                    detTuple.fields.reserve(arity);
                    for (size_t i = 0; i < arity; ++i) {
                        detTuple.fields.push_back(tuple[i]);
                    }
                    deletedFacts.insert(std::move(detTuple));
                }
            }
            const auto& detDeletes = DerivationManager::getDetDeltaDeleteTuples();
            const auto& detInserts = DerivationManager::getDetDeltaInsertTuples();
            for (const auto& tuple : detDeletes) {
                if (detInserts.count(tuple)) {
                    continue;
                }
                if (graph && graph->findNode(tuple) == nullptr) {
                    continue;
                }
                deletedFacts.insert(tuple);
            }
        }
        if (insertedFacts != nullptr && !insertedFacts->empty()) {
            for (const auto& [tuple, _] : *insertedFacts) {
                deletedFacts.erase(tuple);
            }
        }
        std::vector<UntypedTuple> out;
        out.reserve(deletedFacts.size());
        for (const auto& tuple : deletedFacts) {
            out.push_back(tuple);
        }
        return out;
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


    void runIncrementalWmc(IncSubgraphView& view, bool useRegional) {
        struct WeightRestore {
            DDManager<NodeRef>* mgr = nullptr;
            const std::unordered_map<int, std::pair<double, double>>* base = nullptr;
            struct SavedWeight {
                int varIdx = -1;
                double pos = 0.0;
                double neg = 0.0;
            };
            std::vector<SavedWeight> saved;
            WeightRestore(DDManager<NodeRef>* manager,
                          const std::unordered_map<int, std::pair<double, double>>* baseWeights)
                    : mgr(manager), base(baseWeights) {}
            void apply(const std::vector<int>& vars) {
                if (!mgr || !base) {
                    return;
                }
                for (int varIdx : vars) {
                    auto it = base->find(varIdx);
                    if (it == base->end()) {
                        continue;
                    }
                    auto cur = mgr->getVariableWeight(varIdx);
                    if (cur.posWeight == it->second.first && cur.negWeight == it->second.second) {
                        continue;
                    }
                    saved.push_back(SavedWeight{varIdx, cur.posWeight, cur.negWeight});
                    mgr->setVariableWeight(varIdx, it->second.first, it->second.second);
                }
            }
            ~WeightRestore() {
                if (!mgr) {
                    return;
                }
                for (const auto& entry : saved) {
                    mgr->setVariableWeight(entry.varIdx, entry.pos, entry.neg);
                }
            }
        };
        auto upstreamKey = [](const std::vector<int>& vars) -> std::string {
            if (vars.empty()) {
                return {};
            }
            std::ostringstream oss;
            for (size_t i = 0; i < vars.size(); ++i) {
                if (i) {
                    oss << ",";
                }
                oss << vars[i];
            }
            return oss.str();
        };
        std::unordered_map<size_t, std::unordered_map<std::string, double>> evidenceWeightOverrideCache;
        evidenceWeightOverrideCache.clear();
        debugger.startStage(StageKind::WEIGHTED_MODEL_COUNTING_INC);
        FunctionTimer timer("incrementally compute probabilities, size " + std::to_string(changedNodes.size()));
        const bool incProfile = incProfileEnabled;
        const bool wmcProfile = wmcProfileEnabled;
        using Clock = std::chrono::steady_clock;
        auto toMs = [](Clock::time_point start) {
            return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
        };
        auto stageStart = Clock::now();
        double evidenceBuildMs = 0.0;
        double evidenceWmcMs = 0.0;
        double nodeLoopMs = 0.0;
        double evidenceMakeAndMs = 0.0;
        double evidenceWmcComputeMs = 0.0;
        double nodeMakeAndMs = 0.0;
        double nodeWmcComputeMs = 0.0;
        double weightApplyMs = 0.0;
        std::size_t evidenceWmcCalls = 0;
        std::size_t nodeWmcCalls = 0;
        std::size_t evidenceMakeAndCalls = 0;
        std::size_t nodeMakeAndCalls = 0;
        std::size_t weightApplyCalls = 0;
        std::size_t weightApplyVars = 0;
        std::size_t weightToggleCalls = 0;
        std::size_t nodeReuse = 0;
        std::size_t nodeRecompute = 0;
        std::size_t nodeZero = 0;
        const auto& outputNodes = view.getOutputNodes();
        const auto& evidenceNodes = view.getEvidenceNodes();
        const auto& deletedOutputNodes = view.getDeletedOutputNodes();
        CycleDependencyGraph* depGraphPtr = nullptr;
        double depGraphMs = 0.0;
        double outputClassifyMs = 0.0;
        double regionLookupMs = 0.0;
        double changedLookupMs = 0.0;
        double componentIdMs = 0.0;
        double probLookupMs = 0.0;
        double probWriteMs = 0.0;
        double outputLoopMs = 0.0;
        if (!evidenceNodes.empty()) {
            auto depStart = Clock::now();
            depGraphPtr = &view.getCycleDependencyGraph();
            if (wmcProfile) {
                depGraphMs = toMs(depStart);
            }
        }
        size_t componentCount = depGraphPtr ? depGraphPtr->getComponentCount() : 0;
        std::vector<NodeRef> componentEvidence(componentCount, ddManager->getTrue());
        std::vector<double> componentEvidenceWeight(componentCount, 1.0);
        std::vector<double> componentEvidenceWeightOriginal;
        std::vector<bool> componentHasEvidence(componentCount, false);
        std::vector<bool> componentEvidenceChanged(componentCount, false);
        const bool regionalOutputProfile = (useRegional && incRegionalOutputProfile.active);
        const bool hasOverrideWeights =
                regionalOutputProfile && !incRegionalOutputProfile.overrideWeights.empty();
        if (useRegional && incRegionalProfileEnabled) {
            debugger.addInfo("inc_regional_profile_active", regionalOutputProfile ? "1" : "0");
        }
        auto applyWeights = [&](const std::unordered_map<int, std::pair<double, double>>& weights) {
            if (!wmcProfile) {
                for (const auto& [varIdx, w] : weights) {
                    ddManager->setVariableWeight(varIdx, w.first, w.second);
                }
                return;
            }
            auto applyStart = Clock::now();
            for (const auto& [varIdx, w] : weights) {
                ddManager->setVariableWeight(varIdx, w.first, w.second);
            }
            weightApplyMs += toMs(applyStart);
            weightApplyCalls++;
            weightApplyVars += weights.size();
        };
        auto makeAndProfile = [&](const NodeRef& lhs, const NodeRef& rhs,
                                  double& ms, std::size_t& calls) {
            if (!wmcProfile) {
                return ddManager->makeAnd(lhs, rhs);
            }
            auto andStart = Clock::now();
            auto res = ddManager->makeAnd(lhs, rhs);
            ms += toMs(andStart);
            calls++;
            return res;
        };
        auto computeWmcProfile = [&](const NodeRef& node, double& ms, std::size_t& calls) {
            calls++;
            if (!wmcProfile) {
                return ddManager->computeWeightedModelCount(node);
            }
            auto wmcStart = Clock::now();
            double res = ddManager->computeWeightedModelCount(node);
            ms += toMs(wmcStart);
            return res;
        };

        if (depGraphPtr) {
            auto evidenceBuildStart = Clock::now();
            for (size_t cid = 0; cid < componentCount; ++cid) {
                const auto& evidences = depGraphPtr->getComponentEvidences(cid);
                if (evidences.empty()) {
                    continue;
                }
                componentHasEvidence[cid] = true;
                NodeRef evidenceNode = ddManager->getTrue();
                for (const auto& [node, val] : evidences) {
                    if (changedNodes.find(node) != changedNodes.end() ||
                            (regionalOutputProfile &&
                             incRegionalOutputProfile.deltaReachableNodes.count(node) > 0)) {
                        componentEvidenceChanged[cid] = true;
                    }
                    auto it = nodeFormulas->find(node);
                    if (it == nodeFormulas->end()) {
                        throw std::runtime_error("Evidence node has no formula: " + node->getTuple().toString());
                    }
                    NodeRef lit = it->second;
                    if (!val) {
                        lit = ddManager->makeNot(lit);
                    }
                    evidenceNode = makeAndProfile(evidenceNode, lit, evidenceMakeAndMs, evidenceMakeAndCalls);
                }
                componentEvidence[cid] = evidenceNode;
            }
            if (incProfile) {
                evidenceBuildMs = toMs(evidenceBuildStart);
            }

            auto evidenceWmcStart = Clock::now();
            if (hasOverrideWeights) {
                for (size_t cid = 0; cid < componentCount; ++cid) {
                    if (componentHasEvidence[cid]) {
                        componentEvidenceWeight[cid] = computeWmcProfile(
                                componentEvidence[cid], evidenceWmcComputeMs, evidenceWmcCalls);
                    }
                }
                if (wmcProfile) {
                    weightToggleCalls++;
                }
                applyWeights(incRegionalOutputProfile.originalWeights);
                componentEvidenceWeightOriginal.assign(componentCount, 1.0);
                for (size_t cid = 0; cid < componentCount; ++cid) {
                    if (componentHasEvidence[cid]) {
                        componentEvidenceWeightOriginal[cid] = computeWmcProfile(
                                componentEvidence[cid], evidenceWmcComputeMs, evidenceWmcCalls);
                    }
                }
                if (wmcProfile) {
                    weightToggleCalls++;
                }
                applyWeights(incRegionalOutputProfile.overrideWeights);
            } else {
                for (size_t cid = 0; cid < componentCount; ++cid) {
                    if (componentHasEvidence[cid]) {
                        componentEvidenceWeight[cid] = computeWmcProfile(
                                componentEvidence[cid], evidenceWmcComputeMs, evidenceWmcCalls);
                    }
                }
            }
            if (incProfile) {
                evidenceWmcMs = toMs(evidenceWmcStart);
            }
        }

        auto nodeLoopStart = Clock::now();
        auto logOutputDecision = [&](const NodePtr& node, const char* action, const char* reason,
                                     bool useOriginalWeights) {
            if (!regionalOutputProfile || (!incRegionalProfileEnabled && !incRegionalProfileHeavyEnabled) || !node) {
                return;
            }
            const bool inRegion = incRegionalOutputProfile.regionNodes.count(node) > 0;
            const bool inBoundary = incRegionalOutputProfile.boundaryNodes.count(node) > 0;
            const bool inDeltaReach = incRegionalOutputProfile.deltaReachableNodes.count(node) > 0;
            const bool changed = changedNodes.find(node) != changedNodes.end();
            const char* mode = inRegion ? "region_new_bdd" : "outside_old_bdd";
            const char* weightMode = useOriginalWeights ? "orig" : "calib";
            debugger.logMessage(
                Level::INFO,
                std::string("[inc-regional-output] node=") + node->getTuple().toString() +
                    " in_region=" + (inRegion ? "1" : "0") +
                    " in_boundary=" + (inBoundary ? "1" : "0") +
                    " in_delta_reach=" + (inDeltaReach ? "1" : "0") +
                    " changed=" + (changed ? "1" : "0") +
                    " action=" + action +
                    " reason=" + reason +
                    " mode=" + mode +
                    " weight_mode=" + weightMode +
                    " override_count=" + std::to_string(incRegionalOutputProfile.overrideCount));
        };
        auto logUpstreamRestore = [&](const NodePtr& node, const std::vector<int>& vars,
                                      const char* reason) {
            if (!regionalOutputProfile || (!incRegionalProfileEnabled && !incRegionalProfileHeavyEnabled) ||
                    !node || vars.empty()) {
                return;
            }
            debugger.logMessage(
                Level::INFO,
                std::string("[inc-regional-upstream-restore] node=") +
                    node->getTuple().toString() +
                    " vars=" + std::to_string(vars.size()) +
                    " reason=" + reason);
        };
        auto applyBoundaryTarget = [&](const NodePtr& node, bool useOriginalWeights,
                                       bool hasEvidence) -> bool {
            if (!regionalOutputProfile || !useOriginalWeights || hasEvidence || !node) {
                return false;
            }
            auto it = incRegionalOutputProfile.boundaryOutputTargets.find(node);
            if (it == incRegionalOutputProfile.boundaryOutputTargets.end()) {
                return false;
            }
            auto writeStart = Clock::now();
            probResult[node] = it->second;
            if (wmcProfile) {
                probWriteMs += toMs(writeStart);
            }
            nodeReuse++;
            logOutputDecision(node, "reuse_precomputed", "boundary_target", useOriginalWeights);
            return true;
        };
        bool weightsCalibrated = true;
        auto ensureWeights = [&](bool wantCalibrated) {
            if (!hasOverrideWeights) {
                return;
            }
            if (wantCalibrated == weightsCalibrated) {
                return;
            }
            if (wmcProfile) {
                weightToggleCalls++;
            }
            applyWeights(wantCalibrated ? incRegionalOutputProfile.overrideWeights
                                        : incRegionalOutputProfile.originalWeights);
            weightsCalibrated = wantCalibrated;
        };
        const bool hasUpstreamRestore =
                hasOverrideWeights && regionalOutputProfile &&
                !incRegionalOutputProfile.boundaryUpstreamAnchorVars.empty();
        std::unordered_map<NodePtr, std::vector<int>> upstreamCache;
        auto inDrNode = [&](const NodePtr& n) {
            return incRegionalOutputProfile.deltaReachableNodes.empty() ||
                   incRegionalOutputProfile.deltaReachableNodes.count(n);
        };
        auto inDrEdge = [&](const EdgePtr& e) {
            return incRegionalOutputProfile.deltaReachableEdges.empty() ||
                   incRegionalOutputProfile.deltaReachableEdges.count(e);
        };
        auto collectUpstreamVars = [&](const NodePtr& node) -> const std::vector<int>& {
            static const std::vector<int> empty;
            if (!hasUpstreamRestore || !node) {
                return empty;
            }
            auto it = upstreamCache.find(node);
            if (it != upstreamCache.end()) {
                return it->second;
            }
            if (!inDrNode(node)) {
                upstreamCache.emplace(node, std::vector<int>{});
                return upstreamCache[node];
            }
            std::unordered_set<NodePtr> visited;
            std::unordered_set<NodePtr> boundaryHits;
            std::queue<NodePtr> q;
            visited.insert(node);
            q.push(node);
            while (!q.empty()) {
                NodePtr cur = q.front();
                q.pop();
                if (incRegionalOutputProfile.boundaryNodes.count(cur)) {
                    boundaryHits.insert(cur);
                }
                for (const auto& e : view.getIncomingEdges(cur)) {
                    if (!inDrEdge(e)) {
                        continue;
                    }
                    const auto& ins = view.getInputs(e);
                    for (const auto& in : ins) {
                        if (!inDrNode(in)) {
                            continue;
                        }
                        if (visited.insert(in).second) {
                            q.push(in);
                        }
                    }
                }
            }
            std::unordered_set<int> vars;
            for (const auto& b : boundaryHits) {
                auto itB = incRegionalOutputProfile.boundaryUpstreamAnchorVars.find(b);
                if (itB == incRegionalOutputProfile.boundaryUpstreamAnchorVars.end()) {
                    continue;
                }
                for (int varIdx : itB->second) {
                    vars.insert(varIdx);
                }
            }
            std::vector<int> varList(vars.begin(), vars.end());
            std::sort(varList.begin(), varList.end());
            upstreamCache.emplace(node, std::move(varList));
            return upstreamCache[node];
        };
        if (!deletedOutputNodes.empty()) {
            for (const auto& node : deletedOutputNodes) {
                probResult.erase(node);
            }
        }
        struct OutputInfo {
            NodePtr node;
            bool inRegion;
            bool inDeltaReach;
        };
        std::vector<OutputInfo> orderedOutputs;
        const std::vector<OutputInfo>* outputInfoPtr = nullptr;
        if (hasOverrideWeights && regionalOutputProfile) {
            orderedOutputs.reserve(outputNodes.size());
            std::vector<OutputInfo> nonRegionOutputs;
            nonRegionOutputs.reserve(outputNodes.size());
            for (const auto& node : outputNodes) {
                auto regionStart = Clock::now();
                bool inDeltaReach = incRegionalOutputProfile.deltaReachableNodes.count(node) > 0;
                bool inRegion = incRegionalOutputProfile.regionNodes.count(node) > 0;
                if (wmcProfile) {
                    regionLookupMs += toMs(regionStart);
                }
                OutputInfo info{node, inRegion, inDeltaReach};
                if (inRegion) {
                    orderedOutputs.push_back(info);
                } else {
                    nonRegionOutputs.push_back(info);
                }
            }
            orderedOutputs.insert(orderedOutputs.end(),
                                  nonRegionOutputs.begin(),
                                  nonRegionOutputs.end());
            outputInfoPtr = &orderedOutputs;
        }
        auto forEachOutput = [&](auto&& fn) {
            if (outputInfoPtr) {
                for (const auto& info : *outputInfoPtr) {
                    fn(info.node, info.inRegion, info.inDeltaReach);
                }
                return;
            }
            for (const auto& node : outputNodes) {
                bool inRegion = false;
                bool inDeltaReach = false;
                if (regionalOutputProfile) {
                    auto regionStart = Clock::now();
                    inDeltaReach = incRegionalOutputProfile.deltaReachableNodes.count(node) > 0;
                    inRegion = incRegionalOutputProfile.regionNodes.count(node) > 0;
                    if (wmcProfile) {
                        regionLookupMs += toMs(regionStart);
                    }
                }
                fn(node, inRegion, inDeltaReach);
            }
        };
        if (!depGraphPtr) {
            forEachOutput([&](const NodePtr& node, bool inRegion, bool inDeltaReach) {
                if (!node->needOutput) {
                    return;
                }
                auto classifyStart = Clock::now();
                const bool regionalTouched = regionalOutputProfile && (inRegion || inDeltaReach);
                const bool useOriginalWeights = hasOverrideWeights && inRegion;
                if (wmcProfile) {
                    outputClassifyMs += toMs(classifyStart);
                }

                bool isChanged = false;
                auto changedStart = Clock::now();
                if (changedNodes.find(node) != changedNodes.end()) {
                    isChanged = true;
                }
                if (wmcProfile) {
                    changedLookupMs += toMs(changedStart);
                }
                if (regionalTouched || isChanged) {
                    ensureWeights(!useOriginalWeights);
                    WeightRestore restore(ddManager, &incRegionalOutputProfile.originalWeights);
                    const auto& upstreamVars = collectUpstreamVars(node);
                    if (!useOriginalWeights && !upstreamVars.empty()) {
                        logUpstreamRestore(node, upstreamVars, "node_wmc");
                        restore.apply(upstreamVars);
                    }
                    auto value = computeWmcProfile((*nodeFormulas)[node],
                                                   nodeWmcComputeMs, nodeWmcCalls);
                    auto writeStart = Clock::now();
                    probResult[node] = value;
                    if (wmcProfile) {
                        probWriteMs += toMs(writeStart);
                    }
                    nodeRecompute++;
                    logOutputDecision(node, "recompute_wmc",
                                      regionalTouched ? "regional_no_evidence" : "no_evidence_changed",
                                      useOriginalWeights);
                } else {
                    auto lookupStart = Clock::now();
                    auto it = probResult.find(node);
                    if (wmcProfile) {
                        probLookupMs += toMs(lookupStart);
                    }
                    if (it != probResult.end()) {
                        auto writeStart = Clock::now();
                        probResult[node] = it->second;
                        if (wmcProfile) {
                            probWriteMs += toMs(writeStart);
                        }
                        nodeReuse++;
                        logOutputDecision(node, "reuse_old_prob", "no_evidence_no_change",
                                          useOriginalWeights);
                    } else {
                        WeightRestore restore(ddManager, &incRegionalOutputProfile.originalWeights);
                        const auto& upstreamVars = collectUpstreamVars(node);
                        if (!useOriginalWeights && !upstreamVars.empty()) {
                            logUpstreamRestore(node, upstreamVars, "node_wmc");
                            restore.apply(upstreamVars);
                        }
                        auto value = computeWmcProfile((*nodeFormulas)[node],
                                                       nodeWmcComputeMs, nodeWmcCalls);
                        auto writeStart = Clock::now();
                        probResult[node] = value;
                        if (wmcProfile) {
                            probWriteMs += toMs(writeStart);
                        }
                        nodeRecompute++;
                        logOutputDecision(node, "recompute_wmc", "no_evidence_missing_prob",
                                          useOriginalWeights);
                    }
                }
            });
        } else {
            forEachOutput([&](const NodePtr& node, bool inRegion, bool inDeltaReach) {
                if (!node->needOutput) {
                    return;
                }
                size_t cid = 0;
                auto compStart = Clock::now();
                cid = depGraphPtr->getComponentId(node);
                if (wmcProfile) {
                    componentIdMs += toMs(compStart);
                }
                auto classifyStart = Clock::now();
                const bool regionalTouched = regionalOutputProfile && (inRegion || inDeltaReach);
                const bool useOriginalWeights = hasOverrideWeights && inRegion;
                if (wmcProfile) {
                    outputClassifyMs += toMs(classifyStart);
                }
                const auto& upstreamVars = collectUpstreamVars(node);
                auto evidenceWeightFor = [&](size_t compId) {
                    if (useOriginalWeights && !componentEvidenceWeightOriginal.empty()) {
                        return componentEvidenceWeightOriginal[compId];
                    }
                    if (upstreamVars.empty()) {
                        return componentEvidenceWeight[compId];
                    }
                    auto key = upstreamKey(upstreamVars);
                    auto& cache = evidenceWeightOverrideCache[compId];
                    auto it = cache.find(key);
                    if (it != cache.end()) {
                        return it->second;
                    }
                    WeightRestore restore(ddManager, &incRegionalOutputProfile.originalWeights);
                    logUpstreamRestore(node, upstreamVars, "evidence_wmc");
                    restore.apply(upstreamVars);
                    double w = computeWmcProfile(componentEvidence[compId],
                                                 evidenceWmcComputeMs, evidenceWmcCalls);
                    cache.emplace(std::move(key), w);
                    return w;
                };

                if (!componentHasEvidence[cid]) {
                    bool isChanged = false;
                    auto changedStart = Clock::now();
                    if (changedNodes.find(node) != changedNodes.end()) {
                        isChanged = true;
                    }
                    if (wmcProfile) {
                        changedLookupMs += toMs(changedStart);
                    }
                    if (regionalTouched || isChanged) {
                        ensureWeights(!useOriginalWeights);
                        WeightRestore restore(ddManager, &incRegionalOutputProfile.originalWeights);
                        if (!useOriginalWeights && !upstreamVars.empty()) {
                            logUpstreamRestore(node, upstreamVars, "node_wmc");
                            restore.apply(upstreamVars);
                        }
                        auto value = computeWmcProfile((*nodeFormulas)[node],
                                                       nodeWmcComputeMs, nodeWmcCalls);
                        auto writeStart = Clock::now();
                        probResult[node] = value;
                        if (wmcProfile) {
                            probWriteMs += toMs(writeStart);
                        }
                        nodeRecompute++;
                        logOutputDecision(node, "recompute_wmc",
                                          regionalTouched ? "regional_no_evidence" : "no_evidence_changed",
                                          useOriginalWeights);
                    } else {
                        auto lookupStart = Clock::now();
                        auto it = probResult.find(node);
                        if (wmcProfile) {
                            probLookupMs += toMs(lookupStart);
                        }
                        if (it != probResult.end()) {
                            auto writeStart = Clock::now();
                            probResult[node] = it->second;
                            if (wmcProfile) {
                                probWriteMs += toMs(writeStart);
                            }
                            nodeReuse++;
                            logOutputDecision(node, "reuse_old_prob", "no_evidence_no_change",
                                              useOriginalWeights);
                        } else {
                            WeightRestore restore(ddManager, &incRegionalOutputProfile.originalWeights);
                            if (!useOriginalWeights && !upstreamVars.empty()) {
                                logUpstreamRestore(node, upstreamVars, "node_wmc");
                                restore.apply(upstreamVars);
                            }
                            auto value =
                                    computeWmcProfile((*nodeFormulas)[node],
                                                      nodeWmcComputeMs, nodeWmcCalls);
                            auto writeStart = Clock::now();
                            probResult[node] = value;
                            if (wmcProfile) {
                                probWriteMs += toMs(writeStart);
                            }
                            nodeRecompute++;
                            logOutputDecision(node, "recompute_wmc", "no_evidence_missing_prob",
                                              useOriginalWeights);
                        }
                    }
                    return;
                }
                if (evidenceWeightFor(cid) == 0.0) {
                    auto writeStart = Clock::now();
                    probResult[node] = 0.0;
                    if (wmcProfile) {
                        probWriteMs += toMs(writeStart);
                    }
                    nodeZero++;
                    logOutputDecision(node, "assign_zero", "evidence_weight_zero", useOriginalWeights);
                    return;
                }
                const bool evidenceChanged = componentEvidenceChanged[cid];
                bool isChanged = false;
                auto changedStart = Clock::now();
                if (changedNodes.find(node) != changedNodes.end()) {
                    isChanged = true;
                }
                if (wmcProfile) {
                    changedLookupMs += toMs(changedStart);
                }
                if (regionalTouched || isChanged || evidenceChanged) {
                    ensureWeights(!useOriginalWeights);
                    WeightRestore restore(ddManager, &incRegionalOutputProfile.originalWeights);
                    if (!useOriginalWeights && !upstreamVars.empty()) {
                        logUpstreamRestore(node, upstreamVars, "node_wmc");
                        restore.apply(upstreamVars);
                    }
                    auto joint = makeAndProfile((*nodeFormulas)[node], componentEvidence[cid],
                                                nodeMakeAndMs, nodeMakeAndCalls);
                    double jointW = computeWmcProfile(joint, nodeWmcComputeMs, nodeWmcCalls);
                    auto writeStart = Clock::now();
                    probResult[node] = jointW / evidenceWeightFor(cid);
                    if (wmcProfile) {
                        probWriteMs += toMs(writeStart);
                    }
                    nodeRecompute++;
                    logOutputDecision(node, "recompute_wmc",
                                      regionalTouched ? "regional_evidence" : "evidence_changed_or_node_changed",
                                      useOriginalWeights);
                    return;
                }
                auto lookupStart = Clock::now();
                auto it = probResult.find(node);
                if (wmcProfile) {
                    probLookupMs += toMs(lookupStart);
                }
                if (it != probResult.end()) {
                    auto writeStart = Clock::now();
                    probResult[node] = it->second;
                    if (wmcProfile) {
                        probWriteMs += toMs(writeStart);
                    }
                    nodeReuse++;
                    logOutputDecision(node, "reuse_old_prob", "evidence_unchanged_no_change",
                                      useOriginalWeights);
                } else {
                    WeightRestore restore(ddManager, &incRegionalOutputProfile.originalWeights);
                    if (!useOriginalWeights && !upstreamVars.empty()) {
                        logUpstreamRestore(node, upstreamVars, "node_wmc");
                        restore.apply(upstreamVars);
                    }
                    auto joint = makeAndProfile((*nodeFormulas)[node], componentEvidence[cid],
                                                nodeMakeAndMs, nodeMakeAndCalls);
                    double jointW = computeWmcProfile(joint, nodeWmcComputeMs, nodeWmcCalls);
                    auto writeStart = Clock::now();
                    probResult[node] = jointW / evidenceWeightFor(cid);
                    if (wmcProfile) {
                        probWriteMs += toMs(writeStart);
                    }
                    nodeRecompute++;
                    logOutputDecision(node, "recompute_wmc", "evidence_unchanged_missing_prob",
                                      useOriginalWeights);
                }
            });
        }
        if (incProfile) {
            nodeLoopMs = toMs(nodeLoopStart);
        }
        if (wmcProfile) {
            outputLoopMs = toMs(nodeLoopStart);
        }
        if (hasOverrideWeights) {
            applyWeights(incRegionalOutputProfile.originalWeights);
            incRegionalOutputProfile.reset();
        }
        if (incProfile) {
            const double totalMs = toMs(stageStart);
            std::size_t componentWithEvidence = 0;
            for (bool hasEv : componentHasEvidence) {
                if (hasEv) {
                    componentWithEvidence++;
                }
            }
            std::cout << "[inc-profile] stage=WMC_INC total_ms=" << totalMs
                      << " evidence_build_ms=" << evidenceBuildMs
                      << " evidence_wmc_ms=" << evidenceWmcMs
                      << " node_ms=" << nodeLoopMs
                      << " components=" << componentCount
                      << " components_ev=" << componentWithEvidence
                      << " nodes=" << outputNodes.size()
                      << " changed_nodes=" << changedNodes.size()
                      << " node_reuse=" << nodeReuse
                      << " node_recompute=" << nodeRecompute
                      << " node_zero=" << nodeZero
                      << " evidence_wmc_calls=" << evidenceWmcCalls
                      << " node_wmc_calls=" << nodeWmcCalls
                      << std::endl;
        }
        if (wmcProfile) {
            const double totalMs = toMs(stageStart);
            std::size_t componentWithEvidence = 0;
            for (bool hasEv : componentHasEvidence) {
                if (hasEv) {
                    componentWithEvidence++;
                }
            }
            std::cout << "[wmc-profile] stage=INC"
                      << " mode=" << (useRegional ? "inc-regional" : "inc-naive")
                      << " total_ms=" << totalMs
                      << " components=" << componentCount
                      << " components_ev=" << componentWithEvidence
                      << " nodes=" << outputNodes.size()
                      << " changed_nodes=" << changedNodes.size()
                      << " node_reuse=" << nodeReuse
                      << " node_recompute=" << nodeRecompute
                      << " evidence_build_ms=" << evidenceBuildMs
                      << " evidence_make_and_calls=" << evidenceMakeAndCalls
                      << " evidence_make_and_ms=" << evidenceMakeAndMs
                      << " evidence_wmc_calls=" << evidenceWmcCalls
                      << " evidence_wmc_compute_ms=" << evidenceWmcComputeMs
                      << " node_make_and_calls=" << nodeMakeAndCalls
                      << " node_make_and_ms=" << nodeMakeAndMs
                      << " node_wmc_calls=" << nodeWmcCalls
                      << " node_wmc_compute_ms=" << nodeWmcComputeMs
                      << " dep_graph_ms=" << depGraphMs
                      << " output_loop_ms=" << outputLoopMs
                      << " output_classify_ms=" << outputClassifyMs
                      << " region_lookup_ms=" << regionLookupMs
                      << " changed_lookup_ms=" << changedLookupMs
                      << " component_id_ms=" << componentIdMs
                      << " prob_lookup_ms=" << probLookupMs
                      << " prob_write_ms=" << probWriteMs
                      << " weight_apply_calls=" << weightApplyCalls
                      << " weight_apply_vars=" << weightApplyVars
                      << " weight_apply_ms=" << weightApplyMs
                      << " weight_toggle_calls=" << weightToggleCalls
                      << " live_nodes=" << ddManager->getLiveNodeCount()
                      << std::endl;
        }
        debugger.endStage();
    }

    std::vector<std::string> outputRelations;
    void setOutputRelations(const std::vector<std::string> outputRelationNames) {
        this->outputRelations = outputRelationNames;
    }

    size_t iteration = 1;
    void commit() {
        DerivationManager::untypedTuple2DeltaInsertRuleApplications.clear();
        DerivationManager::untypedTuple2DeltaDeleteRuleApplications.clear();
        DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications.clear();
        DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications.clear();
        static size_t commitCount = 0;
        // TODO: should clean all delta relations after each commit
        if (program) {  // for non-ground program ...
            if ((isIncrementalSemMode() || isIncrementalFcMode()) &&
                    graph != nullptr && graph->isBiImpMerged()) {
                assert(false && "bi-imp merged graph cannot run incremental mode");
            }
            if ((isIncrementalSemMode() || isIncrementalFcMode()) &&
                    graph != nullptr && graph->isConstFolded()) {
                assert(false && "const-folded graph cannot run incremental mode");
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
                        const auto untypedTuple = UntypedTuple::fromSouffleTuple(origTuple);
                        auto& currentInputs = initialInputRelations[op.relationName];
                        if (currentInputs.count(untypedTuple)) {
                            std::cout << "Relation already contains the tuple to insert, omitted: " << relTuple.toString() << std::endl;
                            op.valid = false;
                            continue;
                        } else {
                            std::cout << "Inserting tuple: " << origTuple.toString() << std::endl;
                        }
                        rel->insert(relTuple);
                        currentInputs.insert(untypedTuple);
                        fact_prob[untypedTuple] = op.probability;
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
                        const auto untypedTuple = UntypedTuple::fromSouffleTuple(origTuple);
                        auto& currentInputs = initialInputRelations[op.relationName];
                        if (!currentInputs.count(untypedTuple)) {
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
                        currentInputs.erase(untypedTuple);
                        fact_prob.erase(untypedTuple);
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
            bool useRegional = isRegionalFcMode();
            if (isIncrementalSemMode()) {
                {
                    bool hasDelete = false;
                    bool hasInsert = false;
                    for (const auto& op : pendingOperations) {
                        if (!op.valid) {
                            continue;
                        }
                        if (op.type == Operation::DELETE) {
                            hasDelete = true;
                        } else if (op.type == Operation::INSERT) {
                            hasInsert = true;
                        }
                    }
                    std::string phaseLabel = "mixed";
                    if (hasDelete && !hasInsert) {
                        phaseLabel = "delete";
                    } else if (hasInsert && !hasDelete) {
                        phaseLabel = "insert";
                    }
                    if (DerivationManager::isSemStatsEnabled()) {
                        DerivationManager::resetDredStats();
                    }
                    DerivationManager::clearDetDeltaTuples();
                    debugger.startTurn(debuggerTurnModeLabel());
                    debugger.startStage(StageKind::SEMINAIVE_INC);
                    program->runAllInc(program->getInputDirectory(), program->getOutputDirectory(), true);
                    debugger.endStage();
                    if (DerivationManager::isSemStatsEnabled()) {
                        std::ostringstream label;
                        label << "iter=" << iteration << " phase=" << phaseLabel;
                        DerivationManager::dumpDredStats(std::cout, label.str());
                    }
                    if (opt.isDredProfileEnabled()) {
                        std::cout << "[dred-debug] relation sizes after SEMINAIVE_INC:\n";
                        const std::array<std::string, 4> prefixes = {
                                "$inc_delta_derv_delete_",
                                "$inc_delta_tuple_delete_",
                                "$inc_derv_overdelete_",
                                "$inc_tuple_overdelete_",
                        };
                        for (auto* rel : program->getAllRelations()) {
                            const std::string& name = rel->getName();
                            for (const auto& prefix : prefixes) {
                                if (name.rfind(prefix, 0) == 0) {
                                    std::cout << "  " << name << " size=" << rel->size() << "\n";
                                    break;
                                }
                            }
                        }
                    }
                }
                DerivationGraphViewInterface::setDumpOutputDir(opt.getOutputFileDir());
                debugger.startStage(StageKind::PRUNING_INC);
                if (opt.isDumpStatEnabled()) {
                    std::cout << "[prune-inc] pre-applyDelta statistics:\n";
                    graph->dumpStatisticsInc(std::cout);
                }
                auto factProbInc = getFactProbInc();
                auto deletedFacts = getDeletedFacts(&factProbInc);
                {
                    FunctionTimer timer("PRUNING_INC: applyDelta");
                    graph->applyDelta(
                        DerivationManager::untypedTuple2DeltaInsertRuleApplications,
                        DerivationManager::untypedTuple2DeltaDeleteRuleApplications,
                        *ruleManager,
                        factProbInc,
                        deletedFacts
                    );
                }
                DerivationManager::clearDetDeltaTuples();
                logApplyDeltaOpsSummary(
                    DerivationManager::untypedTuple2DeltaInsertRuleApplications,
                    DerivationManager::untypedTuple2DeltaDeleteRuleApplications,
                    factProbInc,
                    deletedFacts,
                    useRegional ? "INC_REGIONAL" : "INC_NAIVE");
                logApplyDeltaSummary(*graph, useRegional ? "INC_REGIONAL" : "INC_NAIVE");
                logApplyDeltaGraphSummary(*graph, useRegional ? "INC_REGIONAL" : "INC_NAIVE");
                {
                    if (opt.isDumpDotEnabled()) {
                        FunctionTimer timer("PRUNING_INC: dumpDot-before-prune");
                        graph->dumpDotInc(outputPath("derivation-inc-before-prune" + std::to_string(iteration) + ".dot"));
                    }
                }
                IncSubgraphView view = [&] {
                    FunctionTimer timer("PRUNING_INC: prune");
                    DerivationGraph::setMergeBiImpEnabled(false);
                    graph->setBuildInsertImpacts(useRegional);
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
                if (isIncrementalFcMode()) {
                    debugger.startStage(StageKind::FORWARD_COMPILATION_INC);
                    if (useRegional) {
                        buildFormulasIncRegionalCyclewise(view, *ddManager, *nodeFormulas, *edgeFormulas, changedNodes);
                    } else {
                        buildFormulasIncCyclewise(view, *ddManager, *nodeFormulas, *edgeFormulas, changedNodes);  // TODO: should only update the changed ones.
                    }
                    debugger.endStage();
                    runIncrementalWmc(view, useRegional);
                    std::string incTag = useRegional ? "-inc-regional" : "-inc-naive";
                    std::string incPrefix = "fact-iter" + std::to_string(iteration) + incTag;
                    dumpProbabilities(probResult, opt.getOutputFileDir() + "/", incPrefix);
                } else if (isFullFcMode()) {
                    debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
                    nodeFormulas->clear();
                    edgeFormulas->clear();
                    if (fcMode == FcMode::FULL_HARD) {
                        ddManager->resetHard();
                    } else {
                        ddManager->reset();
                    }
                    buildFormulasCyclewise(view, *ddManager, *nodeFormulas, *edgeFormulas);
                    debugger.endStage();

                    debugger.startStage(StageKind::WEIGHTED_MODEL_COUNTING_FULL);
                    probResult.clear();
                    {
                        const bool wmcProfile = wmcProfileEnabled;
                        using Clock = std::chrono::steady_clock;
                        auto toMs = [](Clock::time_point start) {
                            return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
                        };
                        auto stageStart = Clock::now();
                        double evidenceBuildMs = 0.0;
                        double evidenceMakeAndMs = 0.0;
                        double evidenceWmcComputeMs = 0.0;
                        double nodeMakeAndMs = 0.0;
                        double nodeWmcComputeMs = 0.0;
                        std::size_t evidenceWmcCalls = 0;
                        std::size_t nodeWmcCalls = 0;
                        std::size_t evidenceMakeAndCalls = 0;
                        std::size_t nodeMakeAndCalls = 0;
                        auto makeAndProfile = [&](const NodeRef& lhs, const NodeRef& rhs,
                                                  double& ms, std::size_t& calls) {
                            if (!wmcProfile) {
                                return ddManager->makeAnd(lhs, rhs);
                            }
                            auto andStart = Clock::now();
                            auto res = ddManager->makeAnd(lhs, rhs);
                            ms += toMs(andStart);
                            calls++;
                            return res;
                        };
                        auto computeWmcProfile = [&](const NodeRef& node, double& ms, std::size_t& calls) {
                            calls++;
                            if (!wmcProfile) {
                                return ddManager->computeWeightedModelCount(node);
                            }
                            auto wmcStart = Clock::now();
                            double res = ddManager->computeWeightedModelCount(node);
                            ms += toMs(wmcStart);
                            return res;
                        };

                        auto& depGraph = view.getCycleDependencyGraph();
                        size_t componentCount = depGraph.getComponentCount();
                        std::vector<NodeRef> componentEvidence(componentCount, ddManager->getTrue());
                        std::vector<double> componentEvidenceWeight(componentCount, 1.0);
                        std::vector<bool> componentHasEvidence(componentCount, false);

                        auto evidenceBuildStart = Clock::now();
                        for (size_t cid = 0; cid < componentCount; ++cid) {
                            const auto& evidences = depGraph.getComponentEvidences(cid);
                            if (evidences.empty()) {
                                continue;
                            }
                            componentHasEvidence[cid] = true;
                            NodeRef evidenceNode = ddManager->getTrue();
                            for (const auto& [node, val] : evidences) {
                                auto it = nodeFormulas->find(node);
                                if (it == nodeFormulas->end()) {
                                    throw std::runtime_error("Evidence node has no formula: " + node->getTuple().toString());
                                }
                                NodeRef lit = it->second;
                                if (!val) {
                                    lit = ddManager->makeNot(lit);
                                }
                                evidenceNode = makeAndProfile(evidenceNode, lit, evidenceMakeAndMs, evidenceMakeAndCalls);
                            }
                            componentEvidence[cid] = evidenceNode;
                        }
                        evidenceBuildMs = toMs(evidenceBuildStart);

                        for (size_t cid = 0; cid < componentCount; ++cid) {
                            if (componentHasEvidence[cid]) {
                                componentEvidenceWeight[cid] = computeWmcProfile(
                                        componentEvidence[cid], evidenceWmcComputeMs, evidenceWmcCalls);
                            }
                        }

                        for (auto& [node, formula] : *nodeFormulas) {
                            if (!node->needOutput) {
                                continue;
                            }
                            size_t cid = depGraph.getComponentId(node);
                            if (!componentHasEvidence[cid]) {
                                probResult[node] = computeWmcProfile(formula, nodeWmcComputeMs, nodeWmcCalls);
                                continue;
                            }
                            if (componentEvidenceWeight[cid] == 0.0) {
                                probResult[node] = 0.0;
                                continue;
                            }
                            auto joint = makeAndProfile(formula, componentEvidence[cid],
                                                        nodeMakeAndMs, nodeMakeAndCalls);
                            double jointW = computeWmcProfile(joint, nodeWmcComputeMs, nodeWmcCalls);
                            probResult[node] = jointW / componentEvidenceWeight[cid];
                        }
                        for (const auto& [node, prob] : precomputedProbResult) {
                            probResult.emplace(node, prob);
                        }
                        if (wmcProfile) {
                            std::size_t componentWithEvidence = 0;
                            for (bool hasEv : componentHasEvidence) {
                                if (hasEv) {
                                    componentWithEvidence++;
                                }
                            }
                            std::cout << "[wmc-profile] stage=FULL"
                                      << " mode=" << (fcMode == FcMode::FULL_SOFT ? "inc-full-soft" : "inc-full-hard")
                                      << " total_ms=" << toMs(stageStart)
                                      << " components=" << componentCount
                                      << " components_ev=" << componentWithEvidence
                                      << " nodes=" << view.getValidNodes().size()
                                      << " evidence_build_ms=" << evidenceBuildMs
                                      << " evidence_make_and_calls=" << evidenceMakeAndCalls
                                      << " evidence_make_and_ms=" << evidenceMakeAndMs
                                      << " evidence_wmc_calls=" << evidenceWmcCalls
                                      << " evidence_wmc_compute_ms=" << evidenceWmcComputeMs
                                      << " node_make_and_calls=" << nodeMakeAndCalls
                                      << " node_make_and_ms=" << nodeMakeAndMs
                                      << " node_wmc_calls=" << nodeWmcCalls
                                      << " node_wmc_compute_ms=" << nodeWmcComputeMs
                                      << " live_nodes=" << ddManager->getLiveNodeCount()
                                      << std::endl;
                        }
                    }
                    debugger.endStage();
                    debugger.startStage(StageKind::IO_DUMP_FULL);
                    const std::string fullTag = (fcMode == FcMode::FULL_SOFT) ? "-inc-full-soft" : "-inc-full-hard";
                    const std::string outPrefix = "fact-iter" + std::to_string(iteration) + fullTag;
                    dumpProbabilities(probResult, opt.getOutputFileDir() + "/", outPrefix);
                    debugger.endStage();
                } else {
                    assert(false && "Unsupported fc mode for sem=inc");
                }
                if (ddManager != nullptr) {
                    ddManager->tryGarbageCollection();
                }
                debugger.endTurn();
                iteration++;
            } else if (isFullSemMode()) {
                const bool useIncFc = isIncrementalFcMode();
                const bool useRegionalFc = (fcMode == FcMode::INC_REGIONAL);
                std::unique_ptr<IncSubgraphView> oldPrunedView;
                if (useIncFc && graph != nullptr) {
                    DerivationGraph::setMergeBiImpEnabled(false);
                    oldPrunedView = std::make_unique<IncSubgraphView>(graph->prune(program->getOutputRelations()));
                }

                debugger.startTurn(debuggerTurnModeLabel());
                DerivationGraphViewInterface::setDumpOutputDir(opt.getOutputFileDir());
                logTurnMode(modeSummaryLabel());
                if (ddManager != nullptr && !useIncFc) {
                    nodeFormulas->clear();
                    edgeFormulas->clear();
                    if (fcMode == FcMode::FULL_HARD) {
                        ddManager->resetHard();
                    } else {
                        ddManager->reset();
                    }
                }

                purgeAllRelations();
                loadInitialInputRelations();
                DerivationManager::untypedTuple2RuleApplications.clear();
                debugger.startStage(StageKind::SEMINAIVE_FULL);
                program->runAll(opt.getInputFileDir(), opt.getOutputFileDir(), false);
                std::vector<std::pair<UntypedTuple, bool>> evidenceList;
                if (graph) {
                    evidenceList = graph->getEvidences();
                }
                graph = IncrementalDerivationGraph::createFrom(
                        DerivationManager::untypedTuple2RuleApplications, *ruleManager, *queryManager,
                        fact_prob, evidenceList);
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
                    DerivationGraph::setMergeBiImpEnabled(false);
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

                std::unique_ptr<IncSubgraphView> diffView;
                IncSubgraphView* activeView = &view;
                if (useIncFc) {
                    if (!oldPrunedView) {
                        assert(false && "sem=full with fc=inc-* requires existing previous pruned view");
                    }
                    std::unordered_map<NodePtr, NodePtr> oldToNewNodes;
                    std::unordered_map<EdgePtr, EdgePtr> oldToNewEdges;
                    {
                        FunctionTimer timer("PRUNING_FULL: post-prune-diff");
                        diffView = std::make_unique<IncSubgraphView>(
                                buildPostPruneDiffView(*oldPrunedView, view, oldToNewNodes, oldToNewEdges));
                        remapStateForPostPruneDiff(oldToNewNodes, oldToNewEdges);
                    }
                    changedNodes.clear();
                    debugger.startStage(StageKind::FORWARD_COMPILATION_INC);
                    if (useRegionalFc) {
                        buildFormulasIncRegionalCyclewise(
                                *diffView, *ddManager, *nodeFormulas, *edgeFormulas, changedNodes);
                    } else {
                        buildFormulasIncCyclewise(*diffView, *ddManager, *nodeFormulas, *edgeFormulas, changedNodes);
                    }
                    debugger.endStage();
                    activeView = diffView.get();
                    logPrunedDeltaSummary(*diffView, useRegionalFc ? "INC_REGIONAL" : "INC_NAIVE");
                } else {
                    debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
                    nodeFormulas->clear(), edgeFormulas->clear();
                    buildFormulasCyclewise(view, *ddManager, *nodeFormulas, *edgeFormulas);
                    debugger.endStage();
                }

                if (useIncFc) {
                    runIncrementalWmc(*activeView, useRegionalFc);
                } else {
                    debugger.startStage(StageKind::WEIGHTED_MODEL_COUNTING_FULL);
                    probResult.clear();
                    {
                    const bool wmcProfile = wmcProfileEnabled;
                    using Clock = std::chrono::steady_clock;
                    auto toMs = [](Clock::time_point start) {
                        return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
                    };
                    auto stageStart = Clock::now();
                    double evidenceBuildMs = 0.0;
                    double evidenceMakeAndMs = 0.0;
                    double evidenceWmcComputeMs = 0.0;
                    double nodeMakeAndMs = 0.0;
                    double nodeWmcComputeMs = 0.0;
                    std::size_t evidenceWmcCalls = 0;
                    std::size_t nodeWmcCalls = 0;
                    std::size_t evidenceMakeAndCalls = 0;
                    std::size_t nodeMakeAndCalls = 0;
                    auto makeAndProfile = [&](const NodeRef& lhs, const NodeRef& rhs,
                                              double& ms, std::size_t& calls) {
                        if (!wmcProfile) {
                            return ddManager->makeAnd(lhs, rhs);
                        }
                        auto andStart = Clock::now();
                        auto res = ddManager->makeAnd(lhs, rhs);
                        ms += toMs(andStart);
                        calls++;
                        return res;
                    };
                    auto computeWmcProfile = [&](const NodeRef& node, double& ms, std::size_t& calls) {
                        calls++;
                        if (!wmcProfile) {
                            return ddManager->computeWeightedModelCount(node);
                        }
                        auto wmcStart = Clock::now();
                        double res = ddManager->computeWeightedModelCount(node);
                        ms += toMs(wmcStart);
                        return res;
                    };

                    auto& depGraph = activeView->getCycleDependencyGraph();
                    size_t componentCount = depGraph.getComponentCount();
                    std::vector<NodeRef> componentEvidence(componentCount, ddManager->getTrue());
                    std::vector<double> componentEvidenceWeight(componentCount, 1.0);
                    std::vector<bool> componentHasEvidence(componentCount, false);

                    auto evidenceBuildStart = Clock::now();
                    for (size_t cid = 0; cid < componentCount; ++cid) {
                        const auto& evidences = depGraph.getComponentEvidences(cid);
                        if (evidences.empty()) {
                            continue;
                        }
                        componentHasEvidence[cid] = true;
                        NodeRef evidenceNode = ddManager->getTrue();
                        for (const auto& [node, val] : evidences) {
                            auto it = nodeFormulas->find(node);
                            if (it == nodeFormulas->end()) {
                                throw std::runtime_error("Evidence node has no formula: " + node->getTuple().toString());
                            }
                            NodeRef lit = it->second;
                            if (!val) {
                                lit = ddManager->makeNot(lit);
                            }
                            evidenceNode = makeAndProfile(evidenceNode, lit, evidenceMakeAndMs, evidenceMakeAndCalls);
                        }
                        componentEvidence[cid] = evidenceNode;
                    }
                    evidenceBuildMs = toMs(evidenceBuildStart);

                    for (size_t cid = 0; cid < componentCount; ++cid) {
                        if (componentHasEvidence[cid]) {
                            componentEvidenceWeight[cid] = computeWmcProfile(
                                    componentEvidence[cid], evidenceWmcComputeMs, evidenceWmcCalls);
                        }
                    }

                    for (auto& [node, formula] : *nodeFormulas) {
                        if (!node->needOutput) {
                            continue;
                        }
                        size_t cid = depGraph.getComponentId(node);
                        if (!componentHasEvidence[cid]) {
                            probResult[node] = computeWmcProfile(formula, nodeWmcComputeMs, nodeWmcCalls);
                            continue;
                        }
                        if (componentEvidenceWeight[cid] == 0.0) {
                            probResult[node] = 0.0;
                            continue;
                        }
                        auto joint = makeAndProfile(formula, componentEvidence[cid],
                                                    nodeMakeAndMs, nodeMakeAndCalls);
                        double jointW = computeWmcProfile(joint, nodeWmcComputeMs, nodeWmcCalls);
                        probResult[node] = jointW / componentEvidenceWeight[cid];
                    }
                    for (const auto& [node, prob] : precomputedProbResult) {
                        probResult.emplace(node, prob);
                    }
                    if (wmcProfile) {
                        std::size_t componentWithEvidence = 0;
                        for (bool hasEv : componentHasEvidence) {
                            if (hasEv) {
                                componentWithEvidence++;
                            }
                        }
                        std::cout << "[wmc-profile] stage=FULL"
                                  << " mode=" << (useIncFc ? (useRegionalFc ? "cli-full-inc-regional" : "cli-full-inc-naive") : "cli-full")
                                  << " total_ms=" << toMs(stageStart)
                                  << " components=" << componentCount
                                  << " components_ev=" << componentWithEvidence
                                  << " nodes=" << activeView->getValidNodes().size()
                                  << " evidence_build_ms=" << evidenceBuildMs
                                  << " evidence_make_and_calls=" << evidenceMakeAndCalls
                                  << " evidence_make_and_ms=" << evidenceMakeAndMs
                                  << " evidence_wmc_calls=" << evidenceWmcCalls
                                  << " evidence_wmc_compute_ms=" << evidenceWmcComputeMs
                                  << " node_make_and_calls=" << nodeMakeAndCalls
                                  << " node_make_and_ms=" << nodeMakeAndMs
                                  << " node_wmc_calls=" << nodeWmcCalls
                                  << " node_wmc_compute_ms=" << nodeWmcComputeMs
                                  << " live_nodes=" << ddManager->getLiveNodeCount()
                                  << std::endl;
                    }
                }
                    debugger.endStage();
                }
                debugger.startStage(StageKind::IO_DUMP_FULL);
                std::string basePrefix = "fact-iter" + std::to_string(iteration);
                if (useIncFc) {
                    const std::string incTag = useRegionalFc ? "-inc-regional" : "-inc-naive";
                    dumpProbabilities(probResult, opt.getOutputFileDir() + "/", basePrefix + incTag);
                } else {
                    dumpProbabilities(probResult, opt.getOutputFileDir() + "/", basePrefix + "-full");
                }
                debugger.endStage();
                if (ddManager != nullptr) {
                    ddManager->tryGarbageCollection();
                }
                debugger.endTurn();
                iteration++;
            } else if (isElasticFcMode()) {
                // try to decide whether to do incremental or full
                // by approximating the cost of both, etc, TODO
                assert (false);
            } else {
                assert (false);
            }
        } else {
            assert(false && "No program loaded.");
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

        // Add the raw input (possibly multi-line) to history (only once).
        add_history(line);

        // Convert C string to a C++ stringstream for line splitting.
        std::stringstream ss(line);
        free(line); // Free memory immediately after conversion.

        std::string single_command;
        // Use std::getline to split lines.
        while (instance->running && std::getline(ss, single_command)) {
            // Line endings may include '\r'; trim it.
            if (!single_command.empty() && single_command.back() == '\r') {
                single_command.pop_back();
            }

            if (!single_command.empty()) {
                // Process each split command line.
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
        // 1. Install the callback handler.
        //    Arg 1: interactive prompt
        //    Arg 2: pointer to the line_handler defined above
        rl_callback_handler_install("> ", line_handler);

        // 2. Enter the main event loop.
        while (this->running) {
            fd_set fds;
            FD_ZERO(&fds);
            FD_SET(STDIN_FILENO, &fds); // STDIN_FILENO is the file descriptor for stdin, usually 0.

            int result = select(STDIN_FILENO + 1, &fds, NULL, NULL, NULL);

            if (result < 0) { // If select fails
                perror("select"); // Print error info
                break;
            }

            if (FD_ISSET(STDIN_FILENO, &fds)) {
                rl_callback_read_char();
            }
        }

        // 5. On exit, clean up and remove the callback handler.
        rl_callback_handler_remove();
    }

};

template <typename T>
IncrementalCLI<T>* IncrementalCLI<T>::instance = nullptr;
#endif //CLI_H
