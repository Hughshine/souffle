#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/ForwardCompilation.h"
#include "souffle/problog/GraphRewriter.h"
#include "souffle/problog/formula/CuddManager.h"
#ifdef SOUFFLE_STANDALONE_HAS_SDD
#include "souffle/problog/formula/SddManager.h"
#endif

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <limits>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

enum class Backend {
    Bdd,
    Sdd,
};

struct Options {
    std::string jsonPath;
    std::vector<std::string> querySpecs;
    std::vector<std::string> queryAllRelations;
    bool rewrite = false;
    bool fullGraph = false;
    bool listQueries = false;
    Backend backend = Backend::Bdd;
};

const char* backendChoices() {
#ifdef SOUFFLE_STANDALONE_HAS_SDD
    return "bdd|sdd";
#else
    return "bdd";
#endif
}

[[noreturn]] void failUsage(const std::string& message, const char* argv0) {
    std::ostringstream oss;
    if (!message.empty()) {
        oss << message << "\n\n";
    }
    oss << "Usage:\n"
        << "  " << argv0
        << " --json <derivation.json> --query <Rel(a,b,...)> [--query ...]"
        << " [--backend " << backendChoices() << "] [--rewrite] [--full-graph]\n"
        << "  " << argv0
        << " --json <derivation.json> --query-all <REL1/REL2/...>"
        << " [--backend " << backendChoices() << "] [--rewrite] [--full-graph]\n"
        << "  " << argv0 << " --json <derivation.json> --list-queries\n\n"
        << "Options:\n"
        << "  --json <path>         Path to derivation JSON file\n"
        << "  --query <tuple>       Query tuple like path(1,3). Repeatable\n"
        << "  --query all <rels>    Query all tuples under relations in <rels> (separator: '/' or ',')\n"
        << "  --query-all <rels>    Same as above, e.g. KEY_SENSITIVE/KEY_IND\n"
        << "  --backend <" << backendChoices() << ">   Probability backend (default: bdd)\n"
        << "  --rewrite             Run optimize rewrite on the query subgraph before FC\n"
        << "  --full-graph          Compile entire graph (default: backward slice from query)\n"
        << "  --list-queries        Print all tuples present in graph and exit\n"
        << "  -h, --help            Show this message\n";
    throw std::runtime_error(oss.str());
}

std::string trim(const std::string& in) {
    std::size_t start = 0;
    while (start < in.size() && std::isspace(static_cast<unsigned char>(in[start]))) {
        ++start;
    }
    std::size_t end = in.size();
    while (end > start && std::isspace(static_cast<unsigned char>(in[end - 1]))) {
        --end;
    }
    return in.substr(start, end - start);
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

Backend parseBackend(const std::string& raw) {
    const std::string val = toLower(trim(raw));
    if (val == "bdd") {
        return Backend::Bdd;
    }
    if (val == "sdd") {
#ifdef SOUFFLE_STANDALONE_HAS_SDD
        return Backend::Sdd;
#else
        throw std::runtime_error("Unsupported backend 'sdd' (this build does not include SDD support)");
#endif
    }
    throw std::runtime_error("Unsupported backend '" + raw + "' (expected " + std::string(backendChoices()) + ")");
}

const char* backendName(Backend backend) {
    return backend == Backend::Bdd ? "bdd" : "sdd";
}

std::vector<std::string> parseRelationList(const std::string& raw) {
    std::string normalized = raw;
    for (char& c : normalized) {
        if (c == '/' || c == ';') {
            c = ',';
        }
    }

    std::vector<std::string> rels;
    std::stringstream ss(normalized);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        tok = trim(tok);
        if (!tok.empty()) {
            rels.push_back(tok);
        }
    }
    if (rels.empty()) {
        throw std::runtime_error("Empty relation list in query-all: " + raw);
    }
    return rels;
}

void appendRelationList(std::vector<std::string>& out, const std::string& raw) {
    const auto rels = parseRelationList(raw);
    out.insert(out.end(), rels.begin(), rels.end());
}

UntypedTuple parseTuple(const std::string& spec) {
    const std::string s = trim(spec);
    if (s.empty()) {
        throw std::runtime_error("Empty tuple spec");
    }

    const auto lp = s.find('(');
    if (lp == std::string::npos) {
        return UntypedTuple{s, {}};
    }
    const auto rp = s.rfind(')');
    if (rp == std::string::npos || rp <= lp) {
        throw std::runtime_error("Bad tuple spec (missing ')'): " + s);
    }
    if (!trim(s.substr(rp + 1)).empty()) {
        throw std::runtime_error("Bad tuple spec (trailing content): " + s);
    }

    std::string relation = trim(s.substr(0, lp));
    if (relation.empty()) {
        throw std::runtime_error("Bad tuple spec (missing relation): " + s);
    }

    std::vector<souffle::RamDomain> fields;
    std::string inside = s.substr(lp + 1, rp - lp - 1);
    std::stringstream ss(inside);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        tok = trim(tok);
        if (tok.empty()) {
            continue;
        }
        long long value = std::stoll(tok);
        fields.push_back(static_cast<souffle::RamDomain>(value));
    }

    return UntypedTuple{std::move(relation), std::move(fields)};
}

Options parseArgs(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--json") {
            if (i + 1 >= argc) {
                failUsage("Missing value after --json", argv[0]);
            }
            opt.jsonPath = argv[++i];
        } else if (arg == "--query") {
            if (i + 1 >= argc) {
                failUsage("Missing value after --query", argv[0]);
            }
            const std::string value = argv[++i];
            if (toLower(value) == "all") {
                if (i + 1 >= argc) {
                    failUsage("Missing relation list after '--query all'", argv[0]);
                }
                appendRelationList(opt.queryAllRelations, argv[++i]);
            } else {
                opt.querySpecs.push_back(value);
            }
        } else if (arg == "--query-all") {
            if (i + 1 >= argc) {
                failUsage("Missing value after --query-all", argv[0]);
            }
            appendRelationList(opt.queryAllRelations, argv[++i]);
        } else if (arg == "--backend") {
            if (i + 1 >= argc) {
                failUsage("Missing value after --backend", argv[0]);
            }
            opt.backend = parseBackend(argv[++i]);
        } else if (arg == "--rewrite") {
            opt.rewrite = true;
        } else if (arg == "--full-graph") {
            opt.fullGraph = true;
        } else if (arg == "--list-queries") {
            opt.listQueries = true;
        } else if (arg == "-h" || arg == "--help") {
            failUsage("", argv[0]);
        } else {
            failUsage("Unknown argument: " + arg, argv[0]);
        }
    }

    if (opt.jsonPath.empty()) {
        failUsage("--json is required", argv[0]);
    }
    if (!opt.listQueries && opt.querySpecs.empty() && opt.queryAllRelations.empty()) {
        failUsage("At least one query is required (use --query or --query-all)", argv[0]);
    }

    return opt;
}

std::vector<UntypedTuple> resolveQueryTuples(
        const IncrementalDerivationGraph& graph,
        const std::vector<std::string>& querySpecs,
        const std::vector<std::string>& queryAllRelations) {
    std::set<UntypedTuple> querySet;

    for (const auto& spec : querySpecs) {
        querySet.insert(parseTuple(spec));
    }

    if (!queryAllRelations.empty()) {
        std::unordered_set<std::string> wanted(queryAllRelations.begin(), queryAllRelations.end());
        std::unordered_set<std::string> seen;

        for (const auto& node : graph.getNodes()) {
            if (!node) {
                continue;
            }
            const auto& tuple = node->getTuple();
            if (wanted.count(tuple.relation_name)) {
                querySet.insert(tuple);
                seen.insert(tuple.relation_name);
            }
        }

        for (const auto& rel : wanted) {
            if (!seen.count(rel)) {
                std::cerr << "[warning] No tuple found for relation in query-all: " << rel
                          << "; ignored" << std::endl;
            }
        }
    }

    return std::vector<UntypedTuple>(querySet.begin(), querySet.end());
}

std::vector<NodePtr> resolveQueryNodes(
        IncrementalDerivationGraph& graph, const std::vector<UntypedTuple>& queries) {
    std::vector<NodePtr> result;
    result.reserve(queries.size());

    for (const auto& q : queries) {
        NodePtr node = graph.findNode(q);
        if (!node) {
            throw std::runtime_error("Query tuple not found in derivation graph: " + q.toString());
        }
        result.push_back(node);
    }

    return result;
}

SubgraphView buildBackwardSlice(const std::vector<NodePtr>& roots) {
    std::unordered_set<NodePtr> nodes;
    std::unordered_set<EdgePtr> edges;
    std::queue<NodePtr> work;

    for (const auto& root : roots) {
        if (!root) {
            continue;
        }
        if (nodes.insert(root).second) {
            work.push(root);
        }
    }

    while (!work.empty()) {
        NodePtr cur = work.front();
        work.pop();

        for (const auto& edge : cur->getIncomingEdges()) {
            edges.insert(edge);
            for (const auto& in : edge->getInputs()) {
                if (nodes.insert(in).second) {
                    work.push(in);
                }
            }
        }
    }

    return SubgraphView(std::move(nodes), std::move(edges));
}

NodePtr findByTupleInView(const SubgraphView& view, const UntypedTuple& tuple) {
    for (const auto& node : view.getNodes()) {
        if (node && node->getTuple() == tuple) {
            return node;
        }
    }
    return nullptr;
}

std::optional<double> findPrecomputedProbability(
        const IncrementalDerivationGraph& graph,
        const SubgraphView& activeView,
        const UntypedTuple& queryTuple) {
    if (NodePtr target = graph.findNode(queryTuple)) {
        auto it = precomputedProbResult.find(target);
        if (it != precomputedProbResult.end()) {
            return it->second;
        }
    }

    if (NodePtr inView = findByTupleInView(activeView, queryTuple)) {
        auto it = precomputedProbResult.find(inView);
        if (it != precomputedProbResult.end()) {
            return it->second;
        }
    }

    for (const auto& [node, prob] : precomputedProbResult) {
        if (node && node->getTuple() == queryTuple) {
            return prob;
        }
    }

    return std::nullopt;
}

double elapsedSeconds(const Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
}

WeightedBDDManager::InitConfig makeCuddInitConfig(std::size_t varCount) {
    WeightedBDDManager::InitConfig cfg;
    const auto maxVars = std::numeric_limits<unsigned int>::max();
    const auto doubledVars = varCount > maxVars / 2 ? maxVars : static_cast<unsigned int>(varCount * 2);
    cfg.numVars = doubledVars;
    cfg.numSlots = 512;
    // Match full pipeline sizing to keep CUDD behavior comparable.
    if (varCount <= 256) {
        cfg.cacheSize = 1u << 18;
        cfg.maxMemory = 1UL << 30;
    } else if (varCount <= 1024) {
        cfg.cacheSize = 1u << 20;
        cfg.maxMemory = 4UL << 30;
    } else if (varCount <= 4096) {
        cfg.cacheSize = 1u << 22;
        cfg.maxMemory = 8UL << 30;
    } else if (varCount > 10000) {
        cfg.cacheSize = 1u << 26;
    }
    return cfg;
}

struct ComponentAnalysisStandalone {
    ComponentSubgraph comp;
    SingleRandVarInfo singleRand;
    std::size_t randVars = 0;
    bool hasNegation = false;
    bool hasOr = false;
    bool hasCycle = false;
};

std::vector<ComponentAnalysisStandalone> analyzeComponentsStandalone(
        const DerivationGraphViewInterface& view, std::vector<ComponentSubgraph> components) {
    auto& depGraph = view.getCycleDependencyGraph();
    std::vector<ComponentAnalysisStandalone> analyses;
    analyses.reserve(components.size());

    for (auto& comp : components) {
        ComponentAnalysisStandalone analysis;
        analysis.comp = std::move(comp);
        std::unordered_map<NodePtr, std::size_t> incomingCounts;
        incomingCounts.reserve(analysis.comp.nodes.size());

        for (const auto& node : analysis.comp.nodes) {
            if (node->isFact && node->getProbability() != 1.0) {
                analysis.randVars++;
                if (analysis.randVars == 1) {
                    analysis.singleRand.node = node;
                    analysis.singleRand.edge.reset();
                    analysis.singleRand.probability = node->getProbability();
                }
            }
            auto it = depGraph.nodeToCycleIndex.find(node);
            if (it != depGraph.nodeToCycleIndex.end() && depGraph.nodeCycles[it->second].size() > 1) {
                analysis.hasCycle = true;
            }
        }

        for (const auto& edge : analysis.comp.edges) {
            if (!edge->isDeterministic()) {
                analysis.randVars++;
                if (analysis.randVars == 1) {
                    analysis.singleRand.node.reset();
                    analysis.singleRand.edge = edge;
                    analysis.singleRand.probability = edge->getProbability();
                }
            }

            const auto negs = view.getBodyNegations(edge);
            for (bool neg : negs) {
                if (neg) {
                    analysis.hasNegation = true;
                    break;
                }
            }

            NodePtr out = view.getOutput(edge);
            if (out) {
                incomingCounts[out]++;
                for (const auto& in : view.getInputs(edge)) {
                    if (in == out) {
                        analysis.hasCycle = true;
                        break;
                    }
                }
            }
        }

        for (const auto& node : analysis.comp.nodes) {
            auto it = incomingCounts.find(node);
            if (it != incomingCounts.end()) {
                if (it->second > 1) {
                    analysis.hasOr = true;
                }
                if (node->isFact && it->second > 0) {
                    analysis.hasOr = true;
                }
            }
        }

        analyses.push_back(std::move(analysis));
    }

    return analyses;
}

std::unordered_set<std::string> parseFeatureSet(const char* raw) {
    std::unordered_set<std::string> features;
    if (!raw || !*raw) {
        return features;
    }
    std::string text(raw);
    for (char& c : text) {
        if (c == ';' || c == '/') {
            c = ',';
        }
    }
    std::stringstream ss(text);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        tok = toLower(trim(tok));
        if (!tok.empty()) {
            features.insert(tok);
        }
    }
    return features;
}

bool envTruthy(const char* raw) {
    if (!raw) {
        return false;
    }
    const std::string val = toLower(trim(raw));
    return val == "1" || val == "true" || val == "yes" || val == "on";
}

bool standaloneFastPathEnabled() {
    return !envTruthy(std::getenv("SOUFFLE_STANDALONE_DISABLE_FASTPATH"));
}

bool hasNonTrivialCycles(SubgraphView& view) {
    const auto& cdg = view.getCycleDependencyGraph();
    for (const auto& cycle : cdg.nodeCycles) {
        if (cycle.size() > 1) {
            return true;
        }
    }
    return false;
}

souffle::problog::RewriteFeatureFlags buildStandaloneRewriteFlags() {
    // Keep default behavior aligned with the main pipeline.
    // Env override still supports targeted feature scans.
    souffle::problog::RewriteFeatureFlags flags;

    const auto features = parseFeatureSet(std::getenv("SOUFFLE_STANDALONE_RW_FEATURES"));
    if (features.empty()) {
        return flags;
    }

    flags.splitMode = souffle::problog::SplitMode::None;
    flags.enableCompaction = false;
    flags.enableCleanupIsolated = false;
    flags.enableSingleHyperedge = false;
    flags.enableAllFactsToSO = false;
    flags.enableGeneral = false;
    flags.enableLinearTwoEdge = false;
    flags.enableParallelEdge = false;
    flags.enableFanOutConverge = false;
    const auto enabled = [&](const std::string& name) {
        return features.count("all") || features.count(name);
    };

    if (enabled("single")) flags.enableSingleHyperedge = true;
    if (enabled("allfacts")) flags.enableAllFactsToSO = true;
    if (enabled("linear")) flags.enableLinearTwoEdge = true;
    if (enabled("parallel")) flags.enableParallelEdge = true;
    if (enabled("fanout")) flags.enableFanOutConverge = true;
    if (enabled("general")) flags.enableGeneral = true;
    if (enabled("compaction")) flags.enableCompaction = true;
    if (enabled("cleanup")) flags.enableCleanupIsolated = true;
    if (enabled("force-complete-detect")) flags.forceCompleteSisoDetect = true;
    if (enabled("split-naive")) {
        flags.splitMode = souffle::problog::SplitMode::Naive;
    } else if (enabled("split-complete")) {
        flags.splitMode = souffle::problog::SplitMode::Complete;
    }
    return flags;
}

struct RewriteSafetyAdjustments {
    bool hasCycles = false;
    bool disabledSingle = false;
    bool disabledCompaction = false;
};

RewriteSafetyAdjustments enforceStandaloneRewriteSafety(
        souffle::problog::RewriteFeatureFlags& flags, SubgraphView& view) {
    RewriteSafetyAdjustments out;
    if (!flags.enableAllFactsToSO) {
        return out;
    }
    if (envTruthy(std::getenv("SOUFFLE_STANDALONE_ALLOW_RISKY_SINGLE_ALLFACTS")) ||
            envTruthy(std::getenv("SOUFFLE_STANDALONE_ALLOW_RISKY_FACT_ABSORB_IN_CYCLES"))) {
        return out;
    }

    out.hasCycles = hasNonTrivialCycles(view);
    if (!out.hasCycles) {
        return out;
    }

    // In standalone JSON replay, fact-absorption rewrites on cyclic graphs can
    // erase the anchoring structure FC relies on, which leads to repeated SCC
    // stalls and eventual "giving up on edge" fallbacks.
    if (flags.enableSingleHyperedge) {
        flags.enableSingleHyperedge = false;
        out.disabledSingle = true;
    }
    if (flags.enableCompaction) {
        flags.enableCompaction = false;
        out.disabledCompaction = true;
    }
    return out;
}

struct ViewSanitizeStats {
    std::size_t removedNullNodes = 0;
    std::size_t removedNullEdges = 0;
    std::size_t removedDanglingEdges = 0;
};

struct ViewAdjacencyConsistencyStats {
    std::size_t missingOutIncomingMembership = 0;
    std::size_t missingInputOutgoingMembership = 0;
    std::size_t malformedNodeIncomingMembership = 0;
    std::size_t malformedNodeOutgoingMembership = 0;
};

ViewSanitizeStats sanitizeView(SubgraphView& view) {
    ViewSanitizeStats stats;
    auto& nodes = view.mutableNodes();
    auto& edges = view.mutableEdges();

    for (auto it = nodes.begin(); it != nodes.end();) {
        if (!(*it)) {
            ++stats.removedNullNodes;
            it = nodes.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = edges.begin(); it != edges.end();) {
        const EdgePtr& edge = *it;
        if (!edge) {
            ++stats.removedNullEdges;
            it = edges.erase(it);
            continue;
        }

        NodePtr out = edge->getOutput();
        if (!out || !nodes.count(out)) {
            ++stats.removedDanglingEdges;
            it = edges.erase(it);
            continue;
        }

        bool valid = true;
        for (const auto& in : edge->getInputs()) {
            if (!in || !nodes.count(in)) {
                valid = false;
                break;
            }
        }
        if (!valid) {
            ++stats.removedDanglingEdges;
            it = edges.erase(it);
            continue;
        }

        ++it;
    }

    if (stats.removedNullNodes || stats.removedNullEdges || stats.removedDanglingEdges) {
        view.clearViewCaches();
    }
    return stats;
}

ViewAdjacencyConsistencyStats checkViewAdjacencyConsistency(
        const SubgraphView& view, std::size_t sampleLimit = 8) {
    ViewAdjacencyConsistencyStats stats;
    const auto& nodes = view.getNodes();
    const auto& edges = view.getEdges();

    auto containsEdgePtr = [](const std::vector<EdgePtr>& vec, const EdgePtr& edge) {
        return std::find(vec.begin(), vec.end(), edge) != vec.end();
    };

    std::size_t shown = 0;
    const auto maybePrintSample = [&](const std::string& msg) {
        if (shown < sampleLimit) {
            std::cout << "[diag] adjacency-mismatch " << msg << '\n';
            ++shown;
        }
    };

    // Forward membership: every active edge must appear in endpoint node lists.
    for (const auto& edge : edges) {
        if (!edge) {
            continue;
        }
        const NodePtr out = edge->getOutput();
        if (!out || !nodes.count(out) || !containsEdgePtr(out->getIncomingEdges(), edge)) {
            ++stats.missingOutIncomingMembership;
            maybePrintSample("edge missing from output.incoming: " + edge->toString());
        }

        for (const auto& in : edge->getInputs()) {
            if (!in || !nodes.count(in) || !containsEdgePtr(in->getOutgoingEdges(), edge)) {
                ++stats.missingInputOutgoingMembership;
                maybePrintSample("edge missing from input.outgoing: " + edge->toString());
            }
        }
    }

    // Reverse membership: every node-side edge in active view must reference back correctly.
    for (const auto& node : nodes) {
        if (!node) {
            continue;
        }
        for (const auto& inEdge : node->getIncomingEdges()) {
            if (!inEdge || !edges.count(inEdge)) {
                continue;
            }
            if (inEdge->getOutput() != node) {
                ++stats.malformedNodeIncomingMembership;
                maybePrintSample(
                        "node.incoming has wrong output: node=" + node->toString() +
                        " edge=" + inEdge->toString());
            }
        }
        for (const auto& outEdge : node->getOutgoingEdges()) {
            if (!outEdge || !edges.count(outEdge)) {
                continue;
            }
            const auto& ins = outEdge->getInputs();
            if (std::find(ins.begin(), ins.end(), node) == ins.end()) {
                ++stats.malformedNodeOutgoingMembership;
                maybePrintSample(
                        "node.outgoing missing node in inputs: node=" + node->toString() +
                        " edge=" + outEdge->toString());
            }
        }
    }

    return stats;
}

void reportProblematicSources(const SubgraphView& view, std::size_t limit = 30) {
    std::size_t count = 0;
    for (const auto& node : view.getNodes()) {
        if (!node || node->isFact) {
            continue;
        }
        const auto inEdges = view.getIncomingEdges(node);
        const auto outEdges = view.getOutgoingEdges(node);
        if (!inEdges.empty() || outEdges.empty()) {
            continue;
        }
        std::cout << "[diag] source-nonfact " << node->getTuple().toString()
                  << " in=0 out=" << outEdges.size()
                  << " prob=" << node->getProbability()
                  << '\n';
        std::size_t edgeShown = 0;
        for (const auto& out : outEdges) {
            if (!out || edgeShown >= 3) {
                break;
            }
            std::cout << "[diag]   out-edge " << out->toString() << '\n';
            ++edgeShown;
        }
        ++count;
        if (count >= limit) {
            break;
        }
    }
    if (count > 0) {
        std::cout << "[diag] source-nonfact-total=" << count << '\n';
    }
}

template <typename ManagerT, typename FormulaNodeRef>
void runBackend(
        const char* backend,
        SubgraphView& activeView,
        IncrementalDerivationGraph& graph,
        const std::vector<UntypedTuple>& queryTuples,
        bool enableDebugger,
        double& fcSec,
        double& evalSec,
        std::size_t* liveNodesOut = nullptr,
        double* reorderSecOut = nullptr,
        std::size_t* reorderCountOut = nullptr) {
    const auto fcStart = Clock::now();
    ManagerT manager;
    std::map<NodePtr, FormulaNodeRef> nodeFormulas;
    std::map<EdgePtr, FormulaNodeRef> edgeFormulas;
    if (enableDebugger) {
        debugger.startTurn("FULL");
        debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
    }
    buildFormulasCyclewise(activeView, manager, nodeFormulas, edgeFormulas);
    if (enableDebugger) {
        debugger.endStage();
        debugger.endTurn();
    }
    if (liveNodesOut) {
        *liveNodesOut = manager.getLiveNodeCount();
    }
    if (reorderSecOut || reorderCountOut) {
        if constexpr (std::is_same_v<ManagerT, WeightedBDDManager>) {
            DdManager* dd = manager.getManager();
            if (reorderSecOut) {
                *reorderSecOut = dd ? static_cast<double>(Cudd_ReadReorderingTime(dd)) / 1000.0 : 0.0;
            }
            if (reorderCountOut) {
                *reorderCountOut = dd ? static_cast<std::size_t>(Cudd_ReadReorderings(dd)) : 0;
            }
        } else {
            if (reorderSecOut) {
                *reorderSecOut = 0.0;
            }
            if (reorderCountOut) {
                *reorderCountOut = 0;
            }
        }
    }
    fcSec = elapsedSeconds(fcStart);

    const auto evalStart = Clock::now();
    std::cout << "[backend] " << backend << '\n';
    std::cout << std::setprecision(17);
    for (const auto& queryTuple : queryTuples) {
        NodePtr target = graph.findNode(queryTuple);
        auto it = nodeFormulas.find(target);
        if (it == nodeFormulas.end()) {
            if (auto precomputed = findPrecomputedProbability(graph, activeView, queryTuple)) {
                std::cout << "[result] " << queryTuple.toString() << " = " << *precomputed << '\n';
                continue;
            }
            target = findByTupleInView(activeView, queryTuple);
            if (!target) {
                throw std::runtime_error("Query tuple is outside active view: " + queryTuple.toString());
            }
            it = nodeFormulas.find(target);
            if (it == nodeFormulas.end()) {
                if (auto precomputed = findPrecomputedProbability(graph, activeView, queryTuple)) {
                    std::cout << "[result] " << queryTuple.toString() << " = " << *precomputed << '\n';
                    continue;
                }
                throw std::runtime_error("No formula built for query tuple: " + queryTuple.toString());
            }
        }

        const double prob = manager.computeWeightedModelCount(it->second);
        std::cout << "[result] " << queryTuple.toString() << " = " << prob << '\n';
    }
    evalSec = elapsedSeconds(evalStart);
}

void runBackendBddHybrid(
        const char* backend,
        SubgraphView& activeView,
        IncrementalDerivationGraph& graph,
        const std::vector<UntypedTuple>& queryTuples,
        bool enableDebugger,
        double& fcSec,
        double& evalSec,
        std::size_t& liveNodesOut,
        double* reorderSecOut = nullptr,
        std::size_t* reorderCountOut = nullptr) {
    struct SlowComponentEval {
        ComponentSubgraph comp;
        std::size_t randVars = 0;
    };

    const auto fcStart = Clock::now();
    if (enableDebugger) {
        debugger.startTurn("FULL");
        debugger.startStage(StageKind::FC_WMC_HYBRID_FULL);
    }

    auto components = buildComponentSubgraphs(activeView);
    auto analyses = analyzeComponentsStandalone(activeView, std::move(components));
    const bool enableFast = standaloneFastPathEnabled();

    std::map<NodePtr, double> probResult;
    std::vector<SlowComponentEval> slowEvals;
    slowEvals.reserve(analyses.size());
    std::size_t liveNodesSum = 0;

    for (auto& analysis : analyses) {
        const bool singleCandidate = analysis.randVars == 1;
        const bool conjCandidate =
                !singleCandidate && !analysis.hasNegation && !analysis.hasOr && !analysis.hasCycle;

        if (enableFast && singleCandidate) {
            std::unordered_map<NodePtr, bool> valuesTrue;
            std::unordered_map<NodePtr, bool> valuesFalse;
            if (evaluateSingleRandComponent(analysis.comp, analysis.singleRand, true, valuesTrue) &&
                    evaluateSingleRandComponent(analysis.comp, analysis.singleRand, false, valuesFalse)) {
                const double p = analysis.singleRand.probability;
                for (const auto& node : analysis.comp.nodes) {
                    const auto itTrue = valuesTrue.find(node);
                    const auto itFalse = valuesFalse.find(node);
                    const bool vTrue = itTrue != valuesTrue.end() && itTrue->second;
                    const bool vFalse = itFalse != valuesFalse.end() && itFalse->second;
                    if (!vTrue && !vFalse) {
                        continue;
                    }
                    probResult[node] = (vTrue ? p : 0.0) + (vFalse ? (1.0 - p) : 0.0);
                }
                std::cout << "[fc-component] id=" << analysis.comp.id
                          << " fast_path=single"
                          << " nodes=" << analysis.comp.nodes.size()
                          << " edges=" << analysis.comp.edges.size()
                          << " rand_vars=" << analysis.randVars
                          << '\n';
                continue;
            }
        }

        if (enableFast && conjCandidate) {
            std::unordered_map<NodePtr, double> nodeProbs;
            if (evaluateConjComponent(analysis.comp, nodeProbs)) {
                for (const auto& [node, prob] : nodeProbs) {
                    probResult[node] = prob;
                }
                std::cout << "[fc-component] id=" << analysis.comp.id
                          << " fast_path=conj"
                          << " nodes=" << analysis.comp.nodes.size()
                          << " edges=" << analysis.comp.edges.size()
                          << " rand_vars=" << analysis.randVars
                          << '\n';
                continue;
            }
        }

        std::cout << "[fc-component] id=" << analysis.comp.id
                  << " fast_path=0"
                  << " nodes=" << analysis.comp.nodes.size()
                  << " edges=" << analysis.comp.edges.size()
                  << " rand_vars=" << analysis.randVars
                  << '\n';
        slowEvals.push_back(SlowComponentEval{std::move(analysis.comp), analysis.randVars});
    }

    std::sort(slowEvals.begin(), slowEvals.end(),
            [](const SlowComponentEval& a, const SlowComponentEval& b) {
                return a.randVars > b.randVars;
            });

    if (!slowEvals.empty()) {
        std::size_t maxRandVars = 0;
        for (const auto& slow : slowEvals) {
            maxRandVars = std::max(maxRandVars, slow.randVars);
        }
        auto initConfig = makeCuddInitConfig(maxRandVars);
        WeightedBDDManager manager(initConfig);

        for (auto& slow : slowEvals) {
            manager.reset();
            std::map<NodePtr, BddNodeRef> compNodeFormulas;
            std::map<EdgePtr, BddNodeRef> compEdgeFormulas;
            SubgraphView subview(std::move(slow.comp.nodes), std::move(slow.comp.edges));
            buildFormulasCyclewise(subview, manager, compNodeFormulas, compEdgeFormulas);
            liveNodesSum += manager.getLiveNodeCount();
            for (const auto& node : subview.getNodes()) {
                auto it = compNodeFormulas.find(node);
                if (it == compNodeFormulas.end()) {
                    continue;
                }
                probResult[node] = manager.computeWeightedModelCount(it->second);
            }
        }

        DdManager* dd = manager.getManager();
        if (reorderSecOut) {
            *reorderSecOut = dd ? static_cast<double>(Cudd_ReadReorderingTime(dd)) / 1000.0 : 0.0;
        }
        if (reorderCountOut) {
            *reorderCountOut = dd ? static_cast<std::size_t>(Cudd_ReadReorderings(dd)) : 0;
        }
    } else {
        if (reorderSecOut) {
            *reorderSecOut = 0.0;
        }
        if (reorderCountOut) {
            *reorderCountOut = 0;
        }
    }

    for (const auto& [node, prob] : precomputedProbResult) {
        if (node) {
            probResult.emplace(node, prob);
        }
    }

    if (enableDebugger) {
        debugger.endStage();
        debugger.endTurn();
    }
    liveNodesOut = liveNodesSum;
    fcSec = elapsedSeconds(fcStart);

    const auto evalStart = Clock::now();
    std::cout << "[backend] " << backend << '\n';
    std::cout << std::setprecision(17);
    for (const auto& queryTuple : queryTuples) {
        NodePtr target = graph.findNode(queryTuple);
        auto it = probResult.find(target);
        if (it == probResult.end()) {
            if (auto precomputed = findPrecomputedProbability(graph, activeView, queryTuple)) {
                std::cout << "[result] " << queryTuple.toString() << " = " << *precomputed << '\n';
                continue;
            }
            target = findByTupleInView(activeView, queryTuple);
            if (!target) {
                throw std::runtime_error("Query tuple is outside active view: " + queryTuple.toString());
            }
            it = probResult.find(target);
            if (it == probResult.end()) {
                if (auto precomputed = findPrecomputedProbability(graph, activeView, queryTuple)) {
                    std::cout << "[result] " << queryTuple.toString() << " = " << *precomputed << '\n';
                    continue;
                }
                throw std::runtime_error("No probability computed for query tuple: " + queryTuple.toString());
            }
        }
        std::cout << "[result] " << queryTuple.toString() << " = " << it->second << '\n';
    }
    evalSec = elapsedSeconds(evalStart);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options opt = parseArgs(argc, argv);
        if (envTruthy(std::getenv("SOUFFLE_STANDALONE_DUMP_JSON"))) {
            DerivationGraphViewInterface::setDumpJsonEnabled(true);
        }
        if (envTruthy(std::getenv("SOUFFLE_STANDALONE_DUMP_DOT"))) {
            DerivationGraphViewInterface::setDumpDotEnabled(true);
        }
        if (envTruthy(std::getenv("SOUFFLE_STANDALONE_DUMP_STATS"))) {
            DerivationGraphViewInterface::setDumpStatsEnabled(true);
        }
        if (const char* dumpDir = std::getenv("SOUFFLE_STANDALONE_DUMP_DIR")) {
            const std::string dir = trim(dumpDir);
            if (!dir.empty()) {
                DerivationGraphViewInterface::setDumpOutputDir(dir);
            }
        }

        const auto totalStart = Clock::now();

        const auto loadStart = Clock::now();
        auto graph = std::unique_ptr<IncrementalDerivationGraph>(
                IncrementalDerivationGraph::loadFromJsonInc(opt.jsonPath));
        const double loadSec = elapsedSeconds(loadStart);

        if (opt.listQueries) {
            std::vector<std::string> allTuples;
            allTuples.reserve(graph->getNodes().size());
            for (const auto& node : graph->getNodes()) {
                if (node) {
                    allTuples.push_back(node->getTuple().toString());
                }
            }
            std::sort(allTuples.begin(), allTuples.end());
            for (const auto& tuple : allTuples) {
                std::cout << tuple << '\n';
            }
            return 0;
        }

        const auto expandStart = Clock::now();
        const std::vector<UntypedTuple> queryTuples = resolveQueryTuples(
                *graph, opt.querySpecs, opt.queryAllRelations);
        const double expandSec = elapsedSeconds(expandStart);

        if (queryTuples.empty()) {
            std::cerr << "[warning] No query tuple resolved; nothing to evaluate." << std::endl;
            std::cout << std::fixed << std::setprecision(6)
                      << "[summary] nodes=0 edges=0 queries=0" << std::endl;
            std::cout << std::fixed << std::setprecision(6)
                      << "[timing_s] load=" << loadSec
                      << " expand=" << expandSec
                      << " resolve=0.000000"
                      << " slice=0.000000"
                      << " rewrite=0.000000"
                      << " fc=0.000000"
                      << " eval=0.000000"
                      << " total=" << elapsedSeconds(totalStart)
                      << std::endl;
            return 0;
        }

        const auto resolveStart = Clock::now();
        const std::vector<NodePtr> queryNodes = resolveQueryNodes(*graph, queryTuples);
        const double resolveSec = elapsedSeconds(resolveStart);

        // Match pipeline semantics: protect query tuples from rewrite-side
        // simplification/removal and keep them in output accounting.
        for (const auto& node : queryNodes) {
            if (node) {
                node->setQuery();
            }
        }

        const auto sliceStart = Clock::now();
        SubgraphView queryView = opt.fullGraph
                ? SubgraphView(graph->getNodes(), graph->getEdges())
                : buildBackwardSlice(queryNodes);
        const double sliceSec = elapsedSeconds(sliceStart);
        if (DerivationGraphViewInterface::isDumpJsonEnabled()) {
            queryView.dumpJson("standalone_query_view_before_rewrite.json");
        }

        if (opt.rewrite && !envTruthy(std::getenv("SOUFFLE_STANDALONE_SKIP_MARK_ALL_OUTPUTS"))) {
            // JSON snapshots do not serialize pipeline output metadata.
            // Preserve all nodes in the active view as outputs so rewrite
            // cannot drop semantics needed by downstream queries.
            for (const auto& node : queryView.getNodes()) {
                if (!node) {
                    continue;
                }
                node->setQuery();
            }
        } else if (opt.rewrite) {
            std::cout << "[rewrite-warning] skip mark-all-outputs enabled; "
                         "rewrite now relies only on explicit query tuples as outputs."
                      << '\n';
        }

        std::unique_ptr<IncSubgraphView> rewrittenView;
        SubgraphView* activeView = &queryView;
        double rewriteSec = 0.0;

        if (opt.rewrite) {
            const auto rewriteStart = Clock::now();
            rewrittenView = std::make_unique<IncSubgraphView>(
                    queryView.getNodes(),
                    queryView.getEdges(),
                    std::set<NodePtr>{},
                    std::set<EdgePtr>{},
                    std::set<NodePtr>{},
                    std::set<EdgePtr>{});

            souffle::problog::GraphRewriter rewriter;
            auto rewriteFlags = buildStandaloneRewriteFlags();
            const auto safety = enforceStandaloneRewriteSafety(rewriteFlags, queryView);
            if (safety.hasCycles && (safety.disabledSingle || safety.disabledCompaction)) {
                std::cout << "[rewrite-warning] detected non-trivial cycles with allfacts enabled; "
                             "disabled";
                if (safety.disabledSingle) {
                    std::cout << " single";
                }
                if (safety.disabledCompaction) {
                    if (safety.disabledSingle) {
                        std::cout << " and";
                    }
                    std::cout << " compaction";
                }
                std::cout << " to avoid unresolved SCC stalls in standalone JSON replay; "
                             "set SOUFFLE_STANDALONE_ALLOW_RISKY_FACT_ABSORB_IN_CYCLES=1 "
                             "(or SOUFFLE_STANDALONE_ALLOW_RISKY_SINGLE_ALLFACTS=1) to force."
                          << '\n';
            }
            const auto stats = rewriter.rewriteUntilFixpoint(*graph, *rewrittenView, false, rewriteFlags);
            rewriteSec = elapsedSeconds(rewriteStart);
            activeView = rewrittenView.get();

            std::cout << "[rewrite] iterations=" << stats.numIterations
                      << " regions=" << stats.numRegionsRewritten
                      << " nodes_removed=" << stats.numNodesRemoved
                      << " edges_removed=" << stats.numEdgesRemoved
                      << " edges_added=" << stats.numEdgesAdded
                      << '\n';
            if (DerivationGraphViewInterface::isDumpJsonEnabled()) {
                activeView->dumpJson("standalone_query_view_after_rewrite.json");
            }
        }

        const auto sanitizeStats = sanitizeView(*activeView);
        if (sanitizeStats.removedNullNodes || sanitizeStats.removedNullEdges ||
                sanitizeStats.removedDanglingEdges) {
            std::cout << "[sanitize] removed_null_nodes=" << sanitizeStats.removedNullNodes
                      << " removed_null_edges=" << sanitizeStats.removedNullEdges
                      << " removed_dangling_edges=" << sanitizeStats.removedDanglingEdges
                      << '\n';
        }
        const auto consistencyStats = checkViewAdjacencyConsistency(*activeView);
        if (consistencyStats.missingOutIncomingMembership ||
                consistencyStats.missingInputOutgoingMembership ||
                consistencyStats.malformedNodeIncomingMembership ||
                consistencyStats.malformedNodeOutgoingMembership) {
            std::cout << "[diag] adjacency-consistency"
                      << " missing_out_incoming=" << consistencyStats.missingOutIncomingMembership
                      << " missing_input_outgoing=" << consistencyStats.missingInputOutgoingMembership
                      << " malformed_node_incoming=" << consistencyStats.malformedNodeIncomingMembership
                      << " malformed_node_outgoing=" << consistencyStats.malformedNodeOutgoingMembership
                      << '\n';
        }
        if (opt.rewrite) {
            reportProblematicSources(*activeView);
        }

        double fcSec = 0.0;
        double evalSec = 0.0;
        std::size_t bddLiveNodes = 0;
        double bddReorderSec = 0.0;
        std::size_t bddReorderCount = 0;
        const bool enableDebugger = envTruthy(std::getenv("SOUFFLE_STANDALONE_USE_DEBUGGER"));
        if (opt.backend == Backend::Bdd) {
            if (opt.rewrite) {
                runBackendBddHybrid(
                        backendName(opt.backend), *activeView, *graph, queryTuples, enableDebugger, fcSec,
                        evalSec, bddLiveNodes, &bddReorderSec, &bddReorderCount);
            } else {
                runBackend<WeightedBDDManager, BddNodeRef>(
                        backendName(opt.backend), *activeView, *graph, queryTuples, enableDebugger, fcSec,
                        evalSec, &bddLiveNodes, &bddReorderSec, &bddReorderCount);
            }
        } else {
#ifdef SOUFFLE_STANDALONE_HAS_SDD
            runBackend<SddFormulaManager, SddNodeRef>(
                    backendName(opt.backend), *activeView, *graph, queryTuples, enableDebugger, fcSec, evalSec);
#else
            throw std::runtime_error("backend=sdd requested, but this binary was built without SDD support");
#endif
        }
        if (enableDebugger) {
            std::cout << "[debugger-report-begin]\n";
            debugger.printReport(std::cout);
            std::cout << "[debugger-report-end]\n";
        }

        std::cout << std::fixed << std::setprecision(6)
                  << "[summary] nodes=" << activeView->getNodes().size()
                  << " edges=" << activeView->getEdges().size()
                  << " queries=" << queryTuples.size()
                  << '\n';
        if (opt.backend == Backend::Bdd) {
            std::cout << "[bdd-stats] live_nodes=" << bddLiveNodes << '\n';
            std::cout << std::fixed << std::setprecision(6)
                      << "[bdd-stats] reordering_runtime_s=" << bddReorderSec << '\n';
            std::cout << "[bdd-stats] reorderings=" << bddReorderCount << '\n';
        }
        std::cout << std::fixed << std::setprecision(6)
                  << "[timing_s] load=" << loadSec
                  << " expand=" << expandSec
                  << " resolve=" << resolveSec
                  << " slice=" << sliceSec
                  << " rewrite=" << rewriteSec
                  << " fc=" << fcSec
                  << " eval=" << evalSec
                  << " total=" << elapsedSeconds(totalStart)
                  << '\n';

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << std::endl;
        return 1;
    }
}
