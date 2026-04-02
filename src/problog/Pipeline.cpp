#include "souffle/problog/Pipeline.h"

#include "souffle/Derivation.h"
#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/ForwardCompilation.h"
#include "souffle/problog/GraphAnalyzer.h"
#include "souffle/problog/GraphRewriter.h"
#include "souffle/problog/ImplicitSplitRewrite.h"
#include "souffle/problog/QueryManager.h"
#include "souffle/problog/RuleManager.h"
#include "souffle/problog/debug/Debugger.h"
#include "souffle/problog/formula/CuddManager.h"
#include "souffle/problog/formula/SddManager.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <limits>
#include <string>
#include <unordered_map>
#include <utility>

#include "ImplicitSplitRewrite.cpp"

namespace souffle::problog {

namespace {
bool fullOnlyMode = false;

static std::size_t countInitialInputFacts() {
    return inputFactSet.size();
}

static std::size_t estimateBddVarCount(const SubgraphView& view) {
    auto isSemanticRandomProb = [](double p) {
        return p > 0.0 && p < 1.0;
    };
    std::size_t count = 0;
    for (const auto& node : view.getNodes()) {
        if (node->isFact && isSemanticRandomProb(node->getProbability())) {
            ++count;
        }
    }
    for (const auto& edge : view.getEdges()) {
        if (isSemanticRandomProb(edge->getProbability())) {
            ++count;
        }
    }
    return count;
}

static IncSubgraphView buildFullIncViewLocal(IncrementalDerivationGraph& graph) {
    return IncSubgraphView(graph.getNodes(), graph.getEdges(), {}, {}, {}, {});
}

static IncSubgraphView buildFullIncViewLocal(
        const std::unordered_set<NodePtr>& nodes, const std::unordered_set<EdgePtr>& edges) {
    return IncSubgraphView(nodes, edges, {}, {}, {}, {});
}

static std::size_t precomputeIsolatedOutputFactsLocal(SubgraphView& view) {
    std::size_t count = 0;
    for (const auto& node : view.getNodes()) {
        if (!node || !node->needOutput || !node->isFact || node->hasEvidence()) {
            continue;
        }
        if (!view.getIncomingEdges(node).empty() || !view.getOutgoingEdges(node).empty()) {
            continue;
        }
        if (precomputedProbResult.count(node)) {
            continue;
        }
        precomputedProbResult[node] = node->getProbability();
        node->needOutput = false;
        node->isQuery = false;
        ++count;
    }
    return count;
}

static ImplicitSplitMode resolveImplicitSplitMode(const std::string& splitMode) {
    if (splitMode == "no-split") {
        return ImplicitSplitMode::None;
    }
    if (splitMode == "complete-split") {
        return ImplicitSplitMode::Complete;
    }
    return ImplicitSplitMode::Naive;
}

struct GraphSummary {
    std::size_t nodes = 0;
    std::size_t edges = 0;
    std::size_t factNodes = 0;
    std::size_t derivedNodes = 0;
    std::size_t queryNodes = 0;
    std::size_t outputNodes = 0;
    std::size_t evidenceNodes = 0;
    std::size_t shadowNodes = 0;
    std::size_t probabilisticFactNodes = 0;
    std::size_t probabilisticEdges = 0;
    std::size_t randomVariables = 0;
    std::size_t disjunctionNodes = 0;
    std::size_t maxInDegree = 0;
    std::size_t maxOutDegree = 0;
    std::size_t maxHyperedgeInputs = 0;
};

static GraphSummary summarizeGraphLight(const DerivationGraphViewInterface& view) {
    GraphSummary s;
    const auto& nodes = view.getNodes();
    const auto& edges = view.getEdges();
    s.nodes = nodes.size();
    s.edges = edges.size();

    std::unordered_map<NodePtr, std::size_t> indeg;
    std::unordered_map<NodePtr, std::size_t> outdeg;
    indeg.reserve(nodes.size());
    outdeg.reserve(nodes.size());
    for (const auto& n : nodes) {
        indeg.emplace(n, 0);
        outdeg.emplace(n, 0);
    }

    for (const auto& e : edges) {
        const auto inputs = view.getInputs(e);
        s.maxHyperedgeInputs = std::max(s.maxHyperedgeInputs, inputs.size());
        NodePtr out = view.getOutput(e);
        if (out) {
            auto it = indeg.find(out);
            if (it != indeg.end()) {
                ++it->second;
            }
        }
        for (const auto& in : inputs) {
            auto it = outdeg.find(in);
            if (it != outdeg.end()) {
                ++it->second;
            }
        }
        if (!e->isDeterministic()) {
            ++s.probabilisticEdges;
        }
    }

    for (const auto& n : nodes) {
        if (n->isFact) {
            ++s.factNodes;
            if (n->getProbability() < 1.0) {
                ++s.probabilisticFactNodes;
            }
        } else {
            ++s.derivedNodes;
        }
        if (n->isQuery) {
            ++s.queryNodes;
        }
        if (n->needOutput) {
            ++s.outputNodes;
        }
        if (n->hasEvidence()) {
            ++s.evidenceNodes;
        }
        if (n->isShadow) {
            ++s.shadowNodes;
        }
        const auto inIt = indeg.find(n);
        const auto outIt = outdeg.find(n);
        const std::size_t in = (inIt == indeg.end()) ? 0 : inIt->second;
        const std::size_t out = (outIt == outdeg.end()) ? 0 : outIt->second;
        s.maxInDegree = std::max(s.maxInDegree, in);
        s.maxOutDegree = std::max(s.maxOutDegree, out);
        if ((!n->isFact && in > 1) || (n->isFact && in > 0)) {
            ++s.disjunctionNodes;
        }
    }
    s.randomVariables = s.probabilisticFactNodes + s.probabilisticEdges;
    return s;
}

static void addGraphSummaryInfo(
        Debugger& debugger, const std::string& prefix, const GraphSummary& s) {
    auto add = [&](const std::string& key, const std::size_t value) {
        debugger.addInfo(prefix + key, std::to_string(value));
    };
    add("nodes", s.nodes);
    add("edges", s.edges);
    add("fact_nodes", s.factNodes);
    add("derived_nodes", s.derivedNodes);
    add("query_nodes", s.queryNodes);
    add("output_nodes", s.outputNodes);
    add("evidence_nodes", s.evidenceNodes);
    add("shadow_nodes", s.shadowNodes);
    add("prob_fact_nodes", s.probabilisticFactNodes);
    add("prob_rule_edges", s.probabilisticEdges);
    add("random_variables", s.randomVariables);
    add("disjunction_nodes", s.disjunctionNodes);
    add("max_in_degree", s.maxInDegree);
    add("max_out_degree", s.maxOutDegree);
    add("max_hyperedge_inputs", s.maxHyperedgeInputs);
}

static void recordFcHeartbeat(
        Debugger& debugger,
        StageInfo* stage,
        const FcHeartbeatSnapshot& hb,
        const std::string& mode,
        std::size_t slowDone = 0,
        std::size_t slowTotal = 0,
        std::size_t componentId = std::numeric_limits<std::size_t>::max(),
        std::size_t liveNodes = 0) {
    debugger.addInfo("fc_heartbeat_mode", mode);
    debugger.addInfo("fc_heartbeat_elapsed_ms", std::to_string(hb.elapsedMs));
    debugger.addInfo("fc_heartbeat_round", std::to_string(hb.round));
    debugger.addInfo("fc_heartbeat_cycle_id", std::to_string(hb.currentCycleId));
    debugger.addInfo("fc_heartbeat_cycles_done", std::to_string(hb.completedCycles));
    debugger.addInfo("fc_heartbeat_cycles_total", std::to_string(hb.totalCycles));
    debugger.addInfo("fc_heartbeat_worklist_size", std::to_string(hb.worklistSize));
    debugger.addInfo("fc_heartbeat_ready_queue_size", std::to_string(hb.readyQueueSize));
    debugger.addInfo("fc_heartbeat_node_formulas", std::to_string(hb.nodeFormulaCount));
    debugger.addInfo("fc_heartbeat_edge_formulas", std::to_string(hb.edgeFormulaCount));
    debugger.addInfo("fc_heartbeat_live_nodes", std::to_string(liveNodes));
    if (slowTotal > 0) {
        debugger.addInfo("fc_heartbeat_slow_components_done", std::to_string(slowDone));
        debugger.addInfo("fc_heartbeat_slow_components_total", std::to_string(slowTotal));
    }
    if (componentId != std::numeric_limits<std::size_t>::max()) {
        debugger.addInfo("fc_heartbeat_component_id", std::to_string(componentId));
    }
    if (stage) {
        std::string msg = "heartbeat mode=" + mode + " elapsed_ms=" + std::to_string(hb.elapsedMs) +
                " round=" + std::to_string(hb.round) + " cycle=" + std::to_string(hb.currentCycleId) +
                " cycles=" + std::to_string(hb.completedCycles) + "/" + std::to_string(hb.totalCycles) +
                " worklist=" + std::to_string(hb.worklistSize) +
                " ready=" + std::to_string(hb.readyQueueSize) +
                " node_formulas=" + std::to_string(hb.nodeFormulaCount) +
                " edge_formulas=" + std::to_string(hb.edgeFormulaCount) +
                " live_nodes=" + std::to_string(liveNodes);
        if (componentId != std::numeric_limits<std::size_t>::max()) {
            msg += " component=" + std::to_string(componentId);
        }
        if (slowTotal > 0) {
            msg += " slow_components=" + std::to_string(slowDone) + "/" + std::to_string(slowTotal);
        }
        stage->logMessage(Level::INFO, msg);
    }
    debugger.dumpReportJsonToFile();
}
static WeightedBDDManager::InitConfig makeCuddInitConfig(std::size_t varCount) {
    WeightedBDDManager::InitConfig cfg;
    const auto maxVars = std::numeric_limits<unsigned int>::max();
    const auto doubledVars = varCount > maxVars / 2 ? maxVars : static_cast<unsigned int>(varCount * 2);
    cfg.numVars = doubledVars;
    cfg.numSlots = 512;
    // Smaller graphs downscale cache/memory; large graphs keep the default (largest) config.
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

static std::string join(const std::vector<std::string>& parts, const char* sep) {
    std::string out;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i > 0) {
            out += sep;
        }
        out += parts[i];
    }
    return out;
}

static std::vector<std::vector<std::pair<NodePtr, bool>>> groupEvidencesByComponent(
        const DerivationGraphViewInterface& view,
        const std::vector<std::pair<NodePtr, bool>>& evidences) {
    auto& depGraph = view.getCycleDependencyGraph();
    const size_t componentCount = depGraph.getComponentCount();
    std::vector<std::vector<std::pair<NodePtr, bool>>> byComponent(componentCount);
    std::vector<std::unordered_map<NodePtr, bool>> seen(componentCount);

    for (const auto& ev : evidences) {
        const NodePtr& node = ev.first;
        if (!node || view.getNodes().count(node) == 0) {
            throw std::runtime_error("Evidence node not found in view: " +
                    (node ? node->toString() : std::string("null")));
        }
        size_t cid = depGraph.getComponentId(node);
        auto& seenMap = seen[cid];
        auto it = seenMap.find(node);
        if (it != seenMap.end()) {
            if (it->second != ev.second) {
                throw std::runtime_error("Conflicting evidence for node: " + node->toString());
            }
            continue;
        }
        seenMap.emplace(node, ev.second);
        byComponent[cid].push_back(ev);
    }
    return byComponent;
}

struct FastComponentEval {
    ComponentSubgraph comp;
    SingleRandVarInfo var;
    std::unordered_map<NodePtr, bool> valuesTrue;
    std::unordered_map<NodePtr, bool> valuesFalse;
    long long evalMsTrue = 0;
    long long evalMsFalse = 0;
};

struct ConjComponentEval {
    ComponentSubgraph comp;
    std::unordered_map<NodePtr, double> probabilities;
    long long evalMs = 0;
};

struct ComponentAnalysis {
    ComponentSubgraph comp;
    SingleRandVarInfo singleRand;
    std::size_t randVars = 0;
    bool hasNegation = false;
    bool hasOr = false;
    bool hasCycle = false;
};

struct SlowComponentEval {
    ComponentSubgraph comp;
    std::size_t randVars = 0;
};

struct FastComponentStats {
    size_t candidates = 0;
    size_t used = 0;
    size_t skipped = 0;
    long long evalMs = 0;
};

struct ConjFastStats {
    size_t candidates = 0;
    size_t used = 0;
    size_t skipped = 0;
    long long evalMs = 0;
};

struct ComponentDecision {
    size_t id = 0;
    size_t nodes = 0;
    size_t edges = 0;
    size_t randVars = 0;
    bool hasEvidence = false;
    bool hasNegation = false;
    bool hasOr = false;
    bool hasCycle = false;
    std::string mode;
    std::string reason;
};

static bool evaluateZeroRandConjComponent(
        const ComponentSubgraph& comp,
        std::unordered_map<NodePtr, double>& nodeProbs,
        long long* evalMs = nullptr) {
    using Clock = std::chrono::steady_clock;
    const auto start = Clock::now();
    SubgraphView subview(comp.nodes, comp.edges);
    std::unordered_map<NodePtr, std::size_t> indegree;
    indegree.reserve(comp.nodes.size());
    for (const auto& node : comp.nodes) {
        if (node) {
            indegree.emplace(node, 0);
        }
    }
    for (const auto& edge : comp.edges) {
        if (!edge) {
            continue;
        }
        NodePtr out = subview.getOutput(edge);
        if (out) {
            ++indegree[out];
        }
    }

    std::queue<NodePtr> ready;
    std::vector<NodePtr> topo;
    topo.reserve(comp.nodes.size());
    for (const auto& node : comp.nodes) {
        if (node && indegree[node] == 0) {
            ready.push(node);
        }
    }
    while (!ready.empty()) {
        NodePtr node = ready.front();
        ready.pop();
        topo.push_back(node);
        for (const auto& edge : subview.getOutgoingEdges(node)) {
            NodePtr out = subview.getOutput(edge);
            if (!out) {
                continue;
            }
            auto it = indegree.find(out);
            if (it == indegree.end()) {
                continue;
            }
            if (it->second == 0) {
                continue;
            }
            --it->second;
            if (it->second == 0) {
                ready.push(out);
            }
        }
    }
    if (topo.size() != comp.nodes.size()) {
        return false;
    }

    nodeProbs.clear();
    nodeProbs.reserve(comp.nodes.size());
    for (const auto& node : topo) {
        if (!node) {
            continue;
        }
        const auto& incoming = subview.getIncomingEdges(node);
        if (incoming.empty()) {
            nodeProbs[node] = node->isFact ? node->getProbability() : 0.0;
            continue;
        }
        if (incoming.size() != 1) {
            return false;
        }
        const auto& edge = incoming.front();
        double prob = edge->isDeterministic() ? 1.0 : edge->getProbability();
        const auto& inputs = subview.getInputs(edge);
        const auto& negs = subview.getBodyNegations(edge);
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            auto it = nodeProbs.find(inputs[i]);
            if (it == nodeProbs.end()) {
                return false;
            }
            double inputProb = it->second;
            if (i < negs.size() && negs[i]) {
                inputProb = 1.0 - inputProb;
            }
            prob *= inputProb;
        }
        nodeProbs[node] = prob;
    }
    if (evalMs) {
        *evalMs = static_cast<long long>(
                std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - start).count());
    }
    return true;
}

static std::vector<ComponentAnalysis> analyzeComponents(
        const DerivationGraphViewInterface& view,
        std::vector<ComponentSubgraph> components) {
    auto isSemanticRandomProb = [](double p) {
        return p > 0.0 && p < 1.0;
    };
    auto& depGraph = view.getCycleDependencyGraph();
    std::vector<ComponentAnalysis> analyses;
    analyses.reserve(components.size());

    for (auto& comp : components) {
        ComponentAnalysis analysis;
        analysis.comp = std::move(comp);
        analysis.singleRand = SingleRandVarInfo{};
        std::unordered_map<NodePtr, size_t> incomingCounts;
        incomingCounts.reserve(analysis.comp.nodes.size());

        for (const auto& node : analysis.comp.nodes) {
            if (node->isFact && isSemanticRandomProb(node->getProbability())) {
                analysis.randVars++;
                if (analysis.randVars == 1) {
                    analysis.singleRand.node = node;
                    analysis.singleRand.edge.reset();
                    analysis.singleRand.probability = node->getProbability();
                }
            }
            auto it = depGraph.nodeToCycleIndex.find(node);
            if (it != depGraph.nodeToCycleIndex.end()) {
                if (depGraph.nodeCycles[it->second].size() > 1) {
                    analysis.hasCycle = true;
                }
            }
        }

        for (const auto& edge : analysis.comp.edges) {
            if (isSemanticRandomProb(edge->getProbability())) {
                analysis.randVars++;
                if (analysis.randVars == 1) {
                    analysis.singleRand.node.reset();
                    analysis.singleRand.edge = edge;
                    analysis.singleRand.probability = edge->getProbability();
                }
            }
            auto negs = view.getBodyNegations(edge);
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
                // A fact with derived support is only an OR-source if the base fact itself is probabilistic.
                if (node->isFact && it->second > 0 && isSemanticRandomProb(node->getProbability())) {
                    analysis.hasOr = true;
                }
            }
        }

        analyses.push_back(std::move(analysis));
    }

    return analyses;
}

} // namespace

void setFullOnlyMode(bool enabled) {
    fullOnlyMode = enabled;
}

bool isFullOnlyMode() {
    return fullOnlyMode;
}

std::string makeOutputPath(const CmdOptions& opt, const std::string& filename) {
    const std::string& dir = opt.getOutputFileDir();
    if (dir.empty()) {
        return filename;
    }
    if (dir.back() == '/') {
        return dir + filename;
    }
    return dir + "/" + filename;
}

static void dumpDeterministicProbabilities(const CmdOptions& opt, SouffleProgram& program) {
    std::vector<std::string> outputTuples;
    for (auto* rel : program.getOutputRelations()) {
        if (rel == nullptr) {
            continue;
        }
        for (auto& tup : *rel) {
            outputTuples.push_back(tup.toString());
        }
    }
    std::sort(outputTuples.begin(), outputTuples.end());
    const std::string outPath = makeOutputPath(opt, "facts.prob");
    std::ofstream outputFile(outPath);
    outputFile << std::setprecision(8);
    for (const auto& tupleStr : outputTuples) {
        outputFile << tupleStr << " : 1.0\n";
    }
    std::cout << "[det-force] dumpProbabilities outputs=" << outputTuples.size()
              << " file=" << outPath << std::endl;
}

static std::vector<std::pair<NodePtr, bool>> applyEvidence(
        DerivationGraph& graph,
        const std::vector<std::pair<UntypedTuple, bool>>& evidences) {
    std::vector<std::pair<NodePtr, bool>> resolved;
    resolved.reserve(evidences.size());

    for (const auto& [tup, val] : evidences) {
        NodePtr node = graph.findNode(tup);
        if (!node) {
            throw std::runtime_error("Evidence " + tup.toString() + " is not found in the graph.");
        }

        resolved.emplace_back(node, val);
    }
    return resolved;
}

static void dumpSisoRegions(const DerivationGraphViewInterface& view) {
    auto start = std::chrono::steady_clock::now();
    auto regions = GraphAnalyzer::detectAllSISOStrictFromExit(view);
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start)
                       .count();
    size_t singleHyperedgeCount = 0;
    size_t linearTwoEdgeCount = 0;
    size_t parallelTwoEdgeCount = 0;
    size_t allFactsToSOCount = 0;
    size_t generalCount = 0;
    for (const auto& r : regions) {
        switch (r.kind) {
        case SISORegionKind::SingleHyperedge:
            ++singleHyperedgeCount;
            break;
        case SISORegionKind::LinearTwoEdge:
            ++linearTwoEdgeCount;
            break;
        case SISORegionKind::ParallelEdge:
            ++parallelTwoEdgeCount;
            break;
        case SISORegionKind::AllFactsToSO:
            ++allFactsToSOCount;
            break;
        case SISORegionKind::General:
            ++generalCount;
            break;
        default:
            break;
        }
    }
    std::cout << "Found " << regions.size() << " SISO regions"
              << " (single-hyperedge=" << singleHyperedgeCount
              << ", linear-two-edge=" << linearTwoEdgeCount
              << ", parallel-two-edge=" << parallelTwoEdgeCount
              << ", all-facts=" << allFactsToSOCount
              << ", general=" << generalCount << ")" << std::endl;
    std::cout << "[pipeline] SISO detection took " << dur << " ms\n";
    for (const auto& r : regions) {
        GraphAnalyzer::printSISOInfo(view, r);
    }
    auto dotStart = std::chrono::steady_clock::now();
    GraphAnalyzer::dumpAllRegionsAsDot(view, regions, "siso_regions.dot");
    auto dotMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                         std::chrono::steady_clock::now() - dotStart)
                         .count();
    std::cout << "[pipeline] dumpAllRegionsAsDot took " << dotMs << " ms\n";
}

static bool tryRunScbfBdd(
        const CmdOptions& opt,
        SubgraphView& view,
        const std::vector<std::pair<UntypedTuple, bool>>& evidences,
        const std::unordered_set<NodePtr>& seedTrueNodes,
        std::map<NodePtr, BddNodeRef>& nodeFormulas,
        std::map<EdgePtr, BddNodeRef>& edgeFormulas,
        std::unique_ptr<WeightedBDDManager>& bddManager,
        StageInfo* rewriteHybridStage) {
    if (!opt.isScbfEnabled()) {
        return false;
    }
    if (!evidences.empty()) {
        std::cout << "[pipeline] --scbf fallback: evidence-conditioned path not enabled yet; use default FC/WMC"
                  << std::endl;
        return false;
    }

    Debugger& debugger = Debugger::getInstance();
    debugger.addInfo("scbf_mode", "1");

    if (rewriteHybridStage == nullptr) {
        debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
    }

    const auto varEstimate = estimateBddVarCount(view);
    debugger.addInfo("rand_vars", std::to_string(varEstimate));
    auto initConfig = makeCuddInitConfig(varEstimate);
    debugger.addInfo("manager_init_vars", std::to_string(initConfig.numVars));
    debugger.addInfo("manager_init_slots", std::to_string(initConfig.numSlots));
    debugger.addInfo("manager_init_cache", std::to_string(initConfig.cacheSize));
    debugger.addInfo("manager_init_maxmem_mb",
            std::to_string(initConfig.maxMemory / (1024UL * 1024UL)));

    auto initStart = std::chrono::steady_clock::now();
    bddManager = std::make_unique<WeightedBDDManager>(initConfig);
    auto initMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - initStart)
                          .count();
    debugger.addInfo("manager_init_ms", std::to_string(initMs));

    probResult.clear();
    auto t0 = std::chrono::steady_clock::now();
    ScbfCyclewiseStats scbfStats;
    buildFormulasCyclewiseScbf(view, *bddManager, nodeFormulas, edgeFormulas, seedTrueNodes, &scbfStats);
    auto t1 = std::chrono::steady_clock::now();
    auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    auto wmcMs = static_cast<long long>(scbfStats.outputWmcMs);
    auto fcMs = totalMs > wmcMs ? (totalMs - wmcMs) : 0;
    debugger.addInfo("fc_build_ms", std::to_string(fcMs));
    debugger.addInfo("wmc_ms", std::to_string(wmcMs));
    std::cout << "[pipeline] SCBF(BDD) cyclewise total=" << totalMs << " ms"
              << " (fc=" << fcMs << " ms, wmc=" << wmcMs << " ms)\n";

    for (const auto& [node, prob] : precomputedProbResult) {
        probResult.emplace(node, prob);
    }

    debugger.endStage();
    debugger.startStage(StageKind::IO_DUMP_FULL);
    auto tDumpStart = std::chrono::steady_clock::now();
    dumpProbabilities(probResult, opt.getOutputFileDir());
    auto tDumpMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - tDumpStart)
                           .count();
    std::cout << "[pipeline] probability dump took " << tDumpMs << " ms\n";
    debugger.endStage();
    return true;
}

static bool tryRunScbfSdd(
        const CmdOptions& opt,
        SubgraphView& view,
        const std::vector<std::pair<UntypedTuple, bool>>& evidences,
        const std::unordered_set<NodePtr>& seedTrueNodes,
        std::map<NodePtr, SddNodeRef>& nodeFormulas,
        std::map<EdgePtr, SddNodeRef>& edgeFormulas,
        std::unique_ptr<SddFormulaManager>& sddManager,
        StageInfo* rewriteHybridStage) {
    if (!opt.isScbfEnabled()) {
        return false;
    }
    if (!evidences.empty()) {
        std::cout << "[pipeline] --scbf fallback: evidence-conditioned path not enabled yet; use default FC/WMC"
                  << std::endl;
        return false;
    }

    Debugger& debugger = Debugger::getInstance();
    debugger.addInfo("scbf_mode", "1");
    if (rewriteHybridStage == nullptr) {
        debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
    }

    auto initStart = std::chrono::steady_clock::now();
    sddManager = std::make_unique<SddFormulaManager>();
    auto initMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - initStart)
                          .count();
    debugger.addInfo("manager_init_ms", std::to_string(initMs));

    probResult.clear();
    auto t0 = std::chrono::steady_clock::now();
    ScbfCyclewiseStats scbfStats;
    buildFormulasCyclewiseScbf(view, *sddManager, nodeFormulas, edgeFormulas, seedTrueNodes, &scbfStats);
    auto t1 = std::chrono::steady_clock::now();
    auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    auto wmcMs = static_cast<long long>(scbfStats.outputWmcMs);
    auto fcMs = totalMs > wmcMs ? (totalMs - wmcMs) : 0;
    debugger.addInfo("fc_build_ms", std::to_string(fcMs));
    debugger.addInfo("wmc_ms", std::to_string(wmcMs));
    std::cout << "[pipeline] SCBF(SDD) cyclewise total=" << totalMs << " ms"
              << " (fc=" << fcMs << " ms, wmc=" << wmcMs << " ms)\n";

    for (const auto& [node, prob] : precomputedProbResult) {
        probResult.emplace(node, prob);
    }

    debugger.endStage();
    debugger.startStage(StageKind::IO_DUMP_FULL);
    auto tDumpStart = std::chrono::steady_clock::now();
    dumpProbabilities(probResult, opt.getOutputFileDir());
    auto tDumpMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now() - tDumpStart)
                           .count();
    std::cout << "[pipeline] probability dump took " << tDumpMs << " ms\n";
    debugger.endStage();
    return true;
}

static void runBddPipeline(
        const CmdOptions& opt,
        SouffleProgram& /*program*/,
        RuleManager& /*ruleManager*/,
        QueryManager& /*queryManager*/,
        DerivationGraph& graph,
        SubgraphView& view,
        const std::vector<std::pair<UntypedTuple, bool>>& evidences,
        StageInfo* rewriteHybridStage) {
    Debugger& debugger = Debugger::getInstance();

    std::map<NodePtr, BddNodeRef> nodeFormulas;
    std::map<EdgePtr, BddNodeRef> edgeFormulas;
    std::unique_ptr<WeightedBDDManager> bddManager;
    const bool computeProbabilities = !opt.isDerivationOnly();
    std::unordered_set<NodePtr> seedTrueNodes;
    seedTrueNodes.reserve(view.getNodes().size());
    for (const auto& n : view.getNodes()) {
        const std::string s = n->getTuple().toString();
        if (s.rfind("@magic.", 0) != 0) continue;

        if (view.getIncomingEdges(n).empty()) {
            seedTrueNodes.insert(n);
        }
    }
    //std::cout << "[dbg] seedTrueNodes size = " << seedTrueNodes.size() << "\

    if (computeProbabilities) {
        if (tryRunScbfBdd(
                    opt, view, evidences, seedTrueNodes, nodeFormulas, edgeFormulas, bddManager,
                    rewriteHybridStage)) {
            // SCBF path handled FC/WMC and dump.
        } else if (opt.isRewriteEnabled()) {
            auto* hybridStage = rewriteHybridStage;
            if (!hybridStage) {
                hybridStage = debugger.startStage(StageKind::FC_WMC_HYBRID_FULL);
            }
            auto varEstimate = estimateBddVarCount(view);
            debugger.addInfo("rand_vars", std::to_string(varEstimate));
            if (hybridStage) {
                hybridStage->logMessage(Level::INFO, "rand_vars=" + std::to_string(varEstimate));
            }

            auto components = buildComponentSubgraphs(view);
            auto analyses = analyzeComponents(view, std::move(components));
            long long initMsTotal = 0;
            long long initMsMax = 0;
            long long buildMs = 0;
            WeightedBDDManager::InitConfig initConfig;
            auto t2 = std::chrono::steady_clock::now();
            auto resolvedEvs = applyEvidence(graph, evidences);
            auto t3 = std::chrono::steady_clock::now();
            auto evidencesByComponent = groupEvidencesByComponent(view, resolvedEvs);
            long long evidenceBuildMs = 0;
            long long evidenceWmcMs = 0;
            long long perNodeWmcMs = 0;
            long long fastPathMs = 0;
            long long liveNodesSum = 0;
            const bool wmcProfile = opt.isWmcProfileEnabled();
            using Clock = std::chrono::steady_clock;
            auto toMs = [](Clock::time_point start) {
                return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            };
            double evidenceMakeAndMs = 0.0;
            double evidenceWmcComputeMs = 0.0;
            double nodeMakeAndMs = 0.0;
            double nodeWmcComputeMs = 0.0;
            std::size_t evidenceWmcCalls = 0;
            std::size_t nodeWmcCalls = 0;
            std::size_t evidenceMakeAndCalls = 0;
            std::size_t nodeMakeAndCalls = 0;
            auto makeAndProfile = [&](const BddNodeRef& lhs, const BddNodeRef& rhs,
                                      double& ms, std::size_t& calls) {
                if (!wmcProfile) {
                    return bddManager->makeAnd(lhs, rhs);
                }
                auto andStart = Clock::now();
                auto res = bddManager->makeAnd(lhs, rhs);
                ms += toMs(andStart);
                calls++;
                return res;
            };
            auto computeWmcProfile = [&](const BddNodeRef& node, double& ms, std::size_t& calls) {
                calls++;
                if (!wmcProfile) {
                    return bddManager->computeWeightedModelCount(node);
                }
                auto wmcStart = Clock::now();
                double res = bddManager->computeWeightedModelCount(node);
                ms += toMs(wmcStart);
                return res;
            };

            FastComponentStats fastStats;
            ConjFastStats conjStats;
            std::vector<FastComponentEval> fastComponents;
            std::vector<ConjComponentEval> conjComponents;
            std::vector<SlowComponentEval> slowEvals;
            const bool logFastReasons = opt.isDumpDotEnabled();
            std::vector<ComponentDecision> decisions;
            decisions.reserve(analyses.size());

            probResult.clear();
            const bool enableFast = opt.isSingleRandFastEnabled();
            for (auto& analysis : analyses) {
                const auto& comp = analysis.comp;
                const auto& compEvs = evidencesByComponent[comp.id];
                const bool hasEvidence = !compEvs.empty();
                const bool singleCandidate = analysis.randVars == 1;
                const bool conjCandidate = !singleCandidate && !analysis.hasNegation &&
                        !analysis.hasOr && !analysis.hasCycle;  // fast path excludes cycles/OR/negation
                ComponentDecision decision{
                        comp.id,
                        comp.nodes.size(),
                        comp.edges.size(),
                        analysis.randVars,
                        hasEvidence,
                        analysis.hasNegation,
                        analysis.hasOr,
                        analysis.hasCycle,
                        "",
                        "",
                };

                if (enableFast) {
                    if (singleCandidate) fastStats.candidates++;
                    if (conjCandidate) conjStats.candidates++;
                }
                if (logFastReasons) {
                    std::vector<std::string> singleReasons;
                    if (!enableFast) singleReasons.emplace_back("fast_disabled");
                    if (!singleCandidate) singleReasons.emplace_back("randvars!=1");
                    if (hasEvidence) singleReasons.emplace_back("has_evidence");
                    if (!singleReasons.empty()) {
                        std::cout << "[fc-component] id=" << comp.id
                                  << " single_skip=" << join(singleReasons, ",")
                                  << " rand_vars=" << analysis.randVars
                                  << " has_negation=" << analysis.hasNegation
                                  << " has_or=" << analysis.hasOr
                                  << " has_cycle=" << analysis.hasCycle
                                  << std::endl;
                    }
                    std::vector<std::string> conjReasons;
                    if (!enableFast) conjReasons.emplace_back("fast_disabled");
                    if (singleCandidate) conjReasons.emplace_back("single_randvar");
                    if (analysis.hasNegation) conjReasons.emplace_back("negation");
                    if (analysis.hasOr) conjReasons.emplace_back("or");
                    if (analysis.hasCycle) conjReasons.emplace_back("cycle");
                    if (hasEvidence) conjReasons.emplace_back("has_evidence");
                    if (!conjReasons.empty()) {
                        std::cout << "[fc-component] id=" << comp.id
                                  << " conj_skip=" << join(conjReasons, ",")
                                  << " rand_vars=" << analysis.randVars
                                  << " has_negation=" << analysis.hasNegation
                                  << " has_or=" << analysis.hasOr
                                  << " has_cycle=" << analysis.hasCycle
                                  << std::endl;
                    }
                }

                if (!enableFast || hasEvidence) {
                    if (enableFast && singleCandidate) fastStats.skipped++;
                    if (enableFast && conjCandidate) conjStats.skipped++;
                    decision.mode = "slow";
                    decision.reason = !enableFast ? "fast_disabled" : "has_evidence";
                    decisions.push_back(decision);
                    SlowComponentEval slow;
                    slow.randVars = analysis.randVars;
                    slow.comp = std::move(analysis.comp);
                    slowEvals.push_back(std::move(slow));
                    continue;
                }

                if (singleCandidate) {
                    FastComponentEval eval;
                    eval.var = analysis.singleRand;
                    if (!evaluateSingleRandComponent(
                                analysis.comp, eval.var, true, eval.valuesTrue, &eval.evalMsTrue) ||
                            !evaluateSingleRandComponent(
                                    analysis.comp, eval.var, false, eval.valuesFalse, &eval.evalMsFalse)) {
                        if (logFastReasons) {
                            std::cout << "[fc-component] id=" << comp.id
                                      << " single_skip=eval_failed"
                                      << " rand_vars=" << analysis.randVars
                                      << std::endl;
                        }
                        fastStats.skipped++;
                        decision.mode = "slow";
                        decision.reason = "eval_failed";
                        decisions.push_back(decision);
                        SlowComponentEval slow;
                        slow.randVars = analysis.randVars;
                        slow.comp = std::move(analysis.comp);
                        slowEvals.push_back(std::move(slow));
                        continue;
                    }
                    eval.comp = std::move(analysis.comp);
                    fastStats.used++;
                    fastStats.evalMs += eval.evalMsTrue + eval.evalMsFalse;
                    decision.mode = "fast_single";
                    decisions.push_back(decision);
                    std::cout << "[fc-component] id=" << eval.comp.id
                              << " fast_path=1"
                              << " nodes=" << eval.comp.nodes.size()
                              << " edges=" << eval.comp.edges.size()
                              << " rand_vars=" << analysis.randVars
                              << " eval_ms_true=" << eval.evalMsTrue
                              << " eval_ms_false=" << eval.evalMsFalse
                              << " total_ms=" << (eval.evalMsTrue + eval.evalMsFalse)
                              << std::endl;
                    if (hybridStage) {
                        hybridStage->logMessage(Level::INFO,
                                "component id=" + std::to_string(eval.comp.id) +
                                        " fast_path=single" +
                                        " nodes=" + std::to_string(eval.comp.nodes.size()) +
                                        " edges=" + std::to_string(eval.comp.edges.size()) +
                                        " rand_vars=" + std::to_string(analysis.randVars) +
                                        " eval_ms_true=" + std::to_string(eval.evalMsTrue) +
                                        " eval_ms_false=" + std::to_string(eval.evalMsFalse) +
                                        " total_ms=" + std::to_string(eval.evalMsTrue + eval.evalMsFalse));
                    }
                    fastComponents.push_back(std::move(eval));
                    continue;
                }

                if (conjCandidate) {
                    ConjComponentEval eval;
                    const bool zeroRandConj = (analysis.randVars == 0);
                    const bool conjOk = zeroRandConj
                            ? evaluateZeroRandConjComponent(analysis.comp, eval.probabilities, &eval.evalMs)
                            : evaluateConjComponent(analysis.comp, eval.probabilities, &eval.evalMs);
                    if (!conjOk) {
                        if (logFastReasons) {
                            std::cout << "[fc-component] id=" << comp.id
                                      << " conj_skip=eval_failed"
                                      << " rand_vars=" << analysis.randVars
                                      << std::endl;
                        }
                        conjStats.skipped++;
                        decision.mode = "slow";
                        decision.reason = "eval_failed";
                        decisions.push_back(decision);
                        SlowComponentEval slow;
                        slow.randVars = analysis.randVars;
                        slow.comp = std::move(analysis.comp);
                        slowEvals.push_back(std::move(slow));
                        continue;
                    }
                    eval.comp = std::move(analysis.comp);
                    conjStats.used++;
                    conjStats.evalMs += eval.evalMs;
                    decision.mode = "fast_conj";
                    decisions.push_back(decision);
                    std::cout << "[fc-component] id=" << eval.comp.id
                              << " fast_path=conj"
                              << " nodes=" << eval.comp.nodes.size()
                              << " edges=" << eval.comp.edges.size()
                              << " rand_vars=" << analysis.randVars
                              << " eval_ms=" << eval.evalMs
                              << " total_ms=" << eval.evalMs
                              << std::endl;
                    if (hybridStage) {
                        hybridStage->logMessage(Level::INFO,
                                "component id=" + std::to_string(eval.comp.id) +
                                        " fast_path=conj" +
                                        " nodes=" + std::to_string(eval.comp.nodes.size()) +
                                        " edges=" + std::to_string(eval.comp.edges.size()) +
                                        " rand_vars=" + std::to_string(analysis.randVars) +
                                        " eval_ms=" + std::to_string(eval.evalMs));
                    }
                    conjComponents.push_back(std::move(eval));
                    continue;
                }

                SlowComponentEval slow;
                slow.randVars = analysis.randVars;
                decision.mode = "slow";
                if (analysis.hasOr) {
                    decision.reason = "or";
                } else if (analysis.hasNegation) {
                    decision.reason = "negation";
                } else if (analysis.hasCycle) {
                    decision.reason = "cycle";
                } else {
                    decision.reason = "other";
                }
                decisions.push_back(decision);
                slow.comp = std::move(analysis.comp);
                slowEvals.push_back(std::move(slow));
            }

            std::sort(slowEvals.begin(), slowEvals.end(),
                    [](const SlowComponentEval& a, const SlowComponentEval& b) {
                        return a.randVars > b.randVars;
                    });

            std::size_t maxRandVars = 0;
            for (const auto& slow : slowEvals) {
                maxRandVars = std::max(maxRandVars, slow.randVars);
            }
            if (!slowEvals.empty()) {
                initConfig = makeCuddInitConfig(maxRandVars);
                auto initStart = std::chrono::steady_clock::now();
                bddManager = std::make_unique<WeightedBDDManager>(initConfig);
                initMsTotal = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::steady_clock::now() - initStart)
                                      .count();
                initMsMax = initMsTotal;
            } else {
                initConfig.numVars = 0;
                initConfig.numVarsZ = 0;
                initConfig.numSlots = 0;
                initConfig.cacheSize = 0;
                initConfig.maxMemory = 0;
            }

            debugger.addInfo("manager_init_vars", std::to_string(initConfig.numVars));
            debugger.addInfo("manager_init_slots", std::to_string(initConfig.numSlots));
            debugger.addInfo("manager_init_cache", std::to_string(initConfig.cacheSize));
            debugger.addInfo("manager_init_maxmem_mb",
                    std::to_string(initConfig.maxMemory / (1024UL * 1024UL)));
            debugger.addInfo("manager_init_ms", std::to_string(initMsTotal));
            debugger.addInfo("manager_init_ms_max", std::to_string(initMsMax));
            debugger.addInfo("manager_init_components", std::to_string(slowEvals.empty() ? 0 : 1));
            if (hybridStage) {
                hybridStage->logMessage(Level::INFO, "manager_init_vars=" + std::to_string(initConfig.numVars));
                hybridStage->logMessage(Level::INFO, "manager_init_slots=" + std::to_string(initConfig.numSlots));
                hybridStage->logMessage(Level::INFO, "manager_init_cache=" + std::to_string(initConfig.cacheSize));
                hybridStage->logMessage(Level::INFO, "manager_init_maxmem_mb=" +
                        std::to_string(initConfig.maxMemory / (1024UL * 1024UL)));
                hybridStage->logMessage(Level::INFO, "manager_init_ms=" + std::to_string(initMsTotal));
                hybridStage->logMessage(Level::INFO, "manager_init_ms_max=" + std::to_string(initMsMax));
                hybridStage->logMessage(Level::INFO, "manager_init_components=" +
                        std::to_string(slowEvals.empty() ? 0 : 1));
            }

            debugger.addInfo("slow_components", std::to_string(slowEvals.size()));
            debugger.addInfo("fastpath_components", std::to_string(fastStats.used));
            debugger.addInfo("fastpath_candidates", std::to_string(fastStats.candidates));
            debugger.addInfo("fastpath_skipped", std::to_string(fastStats.skipped));
            debugger.addInfo("fastpath_eval_ms", std::to_string(fastStats.evalMs));
            debugger.addInfo("fastpath_conj_components", std::to_string(conjStats.used));
            debugger.addInfo("fastpath_conj_candidates", std::to_string(conjStats.candidates));
            debugger.addInfo("fastpath_conj_skipped", std::to_string(conjStats.skipped));
            debugger.addInfo("fastpath_conj_eval_ms", std::to_string(conjStats.evalMs));

            if (hybridStage) {
                hybridStage->logMessage(Level::INFO, "slow_components=" +
                        std::to_string(slowEvals.size()));
                hybridStage->logMessage(Level::INFO, "fastpath_components=" +
                        std::to_string(fastStats.used));
                hybridStage->logMessage(Level::INFO, "fastpath_candidates=" +
                        std::to_string(fastStats.candidates));
                hybridStage->logMessage(Level::INFO, "fastpath_skipped=" +
                        std::to_string(fastStats.skipped));
                hybridStage->logMessage(Level::INFO, "fastpath_eval_ms=" +
                        std::to_string(fastStats.evalMs));
                hybridStage->logMessage(Level::INFO, "fastpath_conj_components=" +
                        std::to_string(conjStats.used));
                hybridStage->logMessage(Level::INFO, "fastpath_conj_candidates=" +
                        std::to_string(conjStats.candidates));
                hybridStage->logMessage(Level::INFO, "fastpath_conj_skipped=" +
                        std::to_string(conjStats.skipped));
                hybridStage->logMessage(Level::INFO, "fastpath_conj_eval_ms=" +
                        std::to_string(conjStats.evalMs));
            }
            if (logFastReasons) {
                std::size_t fastSingle = 0;
                std::size_t fastConj = 0;
                std::size_t slowCount = 0;
                for (const auto& decision : decisions) {
                    if (decision.mode == "fast_single") {
                        fastSingle++;
                    } else if (decision.mode == "fast_conj") {
                        fastConj++;
                    } else {
                        slowCount++;
                    }
                }
                std::cout << "[fc-component-info] total=" << decisions.size()
                          << " fast_single=" << fastSingle
                          << " fast_conj=" << fastConj
                          << " slow=" << slowCount
                          << std::endl;
                for (const auto& decision : decisions) {
                    std::cout << "[fc-component-info] id=" << decision.id
                              << " nodes=" << decision.nodes
                              << " edges=" << decision.edges
                              << " rand_vars=" << decision.randVars
                              << " has_negation=" << decision.hasNegation
                              << " has_or=" << decision.hasOr
                              << " has_cycle=" << decision.hasCycle
                              << " has_evidence=" << decision.hasEvidence
                              << " mode=" << decision.mode
                              << " reason=" << decision.reason
                              << std::endl;
                }
            }

            if (!fastComponents.empty()) {
                for (const auto& fast : fastComponents) {
                    double p = fast.var.probability;
                    for (const auto& node : fast.comp.nodes) {
                        if (!node->needOutput) {
                            continue;
                        }
                        auto itTrue = fast.valuesTrue.find(node);
                        auto itFalse = fast.valuesFalse.find(node);
                        if (itTrue == fast.valuesTrue.end() && itFalse == fast.valuesFalse.end()) {
                            continue;
                        }
                        bool vTrue = (itTrue != fast.valuesTrue.end()) ? itTrue->second : false;
                        bool vFalse = (itFalse != fast.valuesFalse.end()) ? itFalse->second : false;
                        double numerator = (vTrue ? p : 0.0) + (vFalse ? (1.0 - p) : 0.0);
                        probResult[node] = numerator;
                    }
                }
            }
            if (!conjComponents.empty()) {
                for (const auto& conj : conjComponents) {
                    for (const auto& [node, prob] : conj.probabilities) {
                        if (!node->needOutput) {
                            continue;
                        }
                        probResult[node] = prob;
                    }
                }
            }
            fastPathMs = fastStats.evalMs + conjStats.evalMs;
            for (auto& slow : slowEvals) {
                if (!bddManager) {
                    throw std::runtime_error("Missing BDD manager for slow components.");
                }
                auto compId = slow.comp.id;
                auto nodeCount = slow.comp.nodes.size();
                auto edgeCount = slow.comp.edges.size();
                auto randVars = slow.randVars;
                auto compStart = std::chrono::steady_clock::now();

                bddManager->reset();
                std::map<NodePtr, BddNodeRef> compNodeFormulas;
                std::map<EdgePtr, BddNodeRef> compEdgeFormulas;
                SubgraphView subview(std::move(slow.comp.nodes), std::move(slow.comp.edges));

                auto buildStart = std::chrono::steady_clock::now();
                buildFormulasCyclewise(subview, *bddManager, compNodeFormulas, compEdgeFormulas);
                auto buildMsComp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - buildStart)
                                           .count();
                buildMs += buildMsComp;
                liveNodesSum += static_cast<long long>(
                        Cudd_ReadNodeCount(bddManager->getManager()));

                const auto& componentEvs = evidencesByComponent[compId];
                auto evidenceBuildStart = std::chrono::steady_clock::now();
                auto evidenceBdd = bddManager->getTrue();
                for (const auto& [eNode, val] : componentEvs) {
                    auto it = compNodeFormulas.find(eNode);
                    if (it == compNodeFormulas.end()) {
                        throw std::runtime_error("Evidence node has no formula: " +
                                eNode->getTuple().toString());
                    }
                    auto lit = it->second;
                    if (!val) {
                        lit = bddManager->makeNot(lit);
                    }
                    evidenceBdd = makeAndProfile(evidenceBdd, lit, evidenceMakeAndMs, evidenceMakeAndCalls);
                }
                auto evidenceBuildMsComp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                   std::chrono::steady_clock::now() - evidenceBuildStart)
                                                   .count();
                evidenceBuildMs += evidenceBuildMsComp;

                auto wmcStart = std::chrono::steady_clock::now();
                double evidenceWeight = 1.0;
                if (!componentEvs.empty()) {
                    evidenceWeight = computeWmcProfile(evidenceBdd, evidenceWmcComputeMs, evidenceWmcCalls);
                }
                auto evidenceWmcMsComp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                 std::chrono::steady_clock::now() - wmcStart)
                                                 .count();
                evidenceWmcMs += evidenceWmcMsComp;

                auto perNodeStart = std::chrono::steady_clock::now();
                for (const auto& node : subview.getNodes()) {
                    if (!node->needOutput) {
                        continue;
                    }
                    auto it = compNodeFormulas.find(node);
                    if (it == compNodeFormulas.end()) {
                        continue;
                    }
                    const auto& bdd = it->second;
                    double prob = 0.0;
                    if (componentEvs.empty()) {
                        prob = computeWmcProfile(bdd, nodeWmcComputeMs, nodeWmcCalls);
                    } else if (evidenceWeight == 0.0) {
                        prob = 0.0;
                    } else {
                        auto joint = makeAndProfile(bdd, evidenceBdd, nodeMakeAndMs, nodeMakeAndCalls);
                        double jointW = computeWmcProfile(joint, nodeWmcComputeMs, nodeWmcCalls);
                        prob = jointW / evidenceWeight;
                    }
                    probResult[node] = prob;
                }
                auto perNodeWmcMsComp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                std::chrono::steady_clock::now() - perNodeStart)
                                                .count();
                perNodeWmcMs += perNodeWmcMsComp;

                auto compTotalMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - compStart)
                                           .count();
                std::cout << "[fc-component] id=" << compId
                          << " fast_path=0"
                          << " nodes=" << nodeCount
                          << " edges=" << edgeCount
                          << " rand_vars=" << randVars
                          << " build_ms=" << buildMsComp
                          << " evidence_build_ms=" << evidenceBuildMsComp
                          << " evidence_wmc_ms=" << evidenceWmcMsComp
                          << " per_node_wmc_ms=" << perNodeWmcMsComp
                          << " total_ms=" << compTotalMs
                          << std::endl;
            }
            for (const auto& [node, prob] : precomputedProbResult) {
                probResult.emplace(node, prob);
            }

            debugger.addInfo("live_nodes", std::to_string(liveNodesSum));
            debugger.addInfo("fc_build_ms", std::to_string(buildMs));
            debugger.addInfo("wmc_ms", std::to_string(evidenceBuildMs + evidenceWmcMs + perNodeWmcMs + fastPathMs));
            debugger.addInfo("evidence_build_ms", std::to_string(evidenceBuildMs));
            debugger.addInfo("evidence_wmc_ms", std::to_string(evidenceWmcMs));
            debugger.addInfo("per_node_wmc_ms", std::to_string(perNodeWmcMs));
            debugger.addInfo("fastpath_wmc_ms", std::to_string(fastPathMs));
            std::cout << "[pipeline] BDD formula build took " << buildMs << " ms\n";
            std::cout << "[pipeline] evidence resolve/tag took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()
                      << " ms\n";
            std::cout << "[pipeline] evidence BDD build took " << evidenceBuildMs << " ms\n";
            std::cout << "[pipeline] evidence WMC took " << evidenceWmcMs << " ms\n";
            std::cout << "[pipeline] per-node conditional WMC took " << perNodeWmcMs << " ms\n";
            if (fastStats.used > 0 || conjStats.used > 0) {
                std::cout << "[pipeline] fastpath WMC took " << fastPathMs << " ms\n";
            }
            if (wmcProfile) {
                std::cout << "[wmc-profile] stage=FULL"
                          << " mode=full-rewrite"
                          << " total_ms=" << (evidenceBuildMs + evidenceWmcMs + perNodeWmcMs + fastPathMs)
                          << " components=" << analyses.size()
                          << " nodes=" << view.getNodes().size()
                          << " evidence_build_ms=" << evidenceBuildMs
                          << " evidence_make_and_calls=" << evidenceMakeAndCalls
                          << " evidence_make_and_ms=" << evidenceMakeAndMs
                          << " evidence_wmc_calls=" << evidenceWmcCalls
                          << " evidence_wmc_compute_ms=" << evidenceWmcComputeMs
                          << " node_make_and_calls=" << nodeMakeAndCalls
                          << " node_make_and_ms=" << nodeMakeAndMs
                          << " node_wmc_calls=" << nodeWmcCalls
                          << " node_wmc_compute_ms=" << nodeWmcComputeMs
                          << " fastpath_ms=" << fastPathMs
                          << " live_nodes=" << liveNodesSum
                          << std::endl;
            }

            debugger.endStage();
            debugger.startStage(StageKind::IO_DUMP_FULL);
            auto tDumpStart = std::chrono::steady_clock::now();
            dumpProbabilities(probResult, opt.getOutputFileDir());
            auto tDumpMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - tDumpStart)
                                   .count();
            std::cout << "[pipeline] probability dump took " << tDumpMs << " ms\n";
            debugger.endStage();
        } else {
            auto* fcStage = debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
            auto varEstimate = estimateBddVarCount(view);
            debugger.addInfo("rand_vars", std::to_string(varEstimate));
            if (fcStage) {
                fcStage->logMessage(Level::INFO, "rand_vars=" + std::to_string(varEstimate));
            }

            auto initConfig = makeCuddInitConfig(varEstimate);
            debugger.addInfo("manager_init_vars", std::to_string(initConfig.numVars));
            debugger.addInfo("manager_init_slots", std::to_string(initConfig.numSlots));
            debugger.addInfo("manager_init_cache", std::to_string(initConfig.cacheSize));
            debugger.addInfo("manager_init_maxmem_mb",
                    std::to_string(initConfig.maxMemory / (1024UL * 1024UL)));
            if (fcStage) {
                fcStage->logMessage(Level::INFO, "manager_init_vars=" + std::to_string(initConfig.numVars));
                fcStage->logMessage(Level::INFO, "manager_init_slots=" + std::to_string(initConfig.numSlots));
                fcStage->logMessage(Level::INFO, "manager_init_cache=" + std::to_string(initConfig.cacheSize));
                fcStage->logMessage(Level::INFO, "manager_init_maxmem_mb=" +
                        std::to_string(initConfig.maxMemory / (1024UL * 1024UL)));
            }
            auto initStart = std::chrono::steady_clock::now();
            bddManager = std::make_unique<WeightedBDDManager>(initConfig);
            auto initMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now() - initStart)
                                  .count();
            debugger.addInfo("manager_init_ms", std::to_string(initMs));
            if (fcStage) {
                fcStage->logMessage(Level::INFO, "manager_init_ms=" + std::to_string(initMs));
            }
            auto t0 = std::chrono::steady_clock::now();
            buildFormulasCyclewise(view, *bddManager, nodeFormulas, edgeFormulas, seedTrueNodes);
            auto t1 = std::chrono::steady_clock::now();
            std::cout << "[pipeline] BDD formula build took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
                      << " ms\n";
            debugger.endStage();

            debugger.startStage(StageKind::WEIGHTED_MODEL_COUNTING_FULL);

            auto t2 = std::chrono::steady_clock::now();
            auto resolvedEvs = applyEvidence(graph, evidences);
            auto t3 = std::chrono::steady_clock::now();

            auto components = buildComponentSubgraphs(view);
            auto evidencesByComponent = groupEvidencesByComponent(view, resolvedEvs);
            long long evidenceBuildMs = 0;
            long long evidenceWmcMs = 0;
            long long perNodeWmcMs = 0;
            const bool wmcProfile = opt.isWmcProfileEnabled();
            using Clock = std::chrono::steady_clock;
            auto toMs = [](Clock::time_point start) {
                return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
            };
            double evidenceMakeAndMs = 0.0;
            double evidenceWmcComputeMs = 0.0;
            double nodeMakeAndMs = 0.0;
            double nodeWmcComputeMs = 0.0;
            std::size_t evidenceWmcCalls = 0;
            std::size_t nodeWmcCalls = 0;
            std::size_t evidenceMakeAndCalls = 0;
            std::size_t nodeMakeAndCalls = 0;
            auto makeAndProfile = [&](const BddNodeRef& lhs, const BddNodeRef& rhs,
                                      double& ms, std::size_t& calls) {
                if (!wmcProfile) {
                    return bddManager->makeAnd(lhs, rhs);
                }
                auto andStart = Clock::now();
                auto res = bddManager->makeAnd(lhs, rhs);
                ms += toMs(andStart);
                calls++;
                return res;
            };
            auto computeWmcProfile = [&](const BddNodeRef& node, double& ms, std::size_t& calls) {
                calls++;
                if (!wmcProfile) {
                    return bddManager->computeWeightedModelCount(node);
                }
                auto wmcStart = Clock::now();
                double res = bddManager->computeWeightedModelCount(node);
                ms += toMs(wmcStart);
                return res;
            };

            probResult.clear();
            for (const auto& comp : components) {
                const auto& componentEvs = evidencesByComponent[comp.id];
                auto evidenceBuildStart = std::chrono::steady_clock::now();
                auto evidenceBdd = bddManager->getTrue();
                for (const auto& [eNode, val] : componentEvs) {
                    auto it = nodeFormulas.find(eNode);
                    if (it == nodeFormulas.end()) {
                        throw std::runtime_error("Evidence node has no formula: " +
                                eNode->getTuple().toString());
                    }
                    auto lit = it->second;
                    if (!val) {
                        lit = bddManager->makeNot(lit);
                    }
                    evidenceBdd = makeAndProfile(evidenceBdd, lit, evidenceMakeAndMs, evidenceMakeAndCalls);
                }
                evidenceBuildMs += std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - evidenceBuildStart)
                                           .count();

                auto wmcStart = std::chrono::steady_clock::now();
                double evidenceWeight = 1.0;
                if (!componentEvs.empty()) {
                    evidenceWeight = computeWmcProfile(evidenceBdd, evidenceWmcComputeMs, evidenceWmcCalls);
                }
                evidenceWmcMs += std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::steady_clock::now() - wmcStart)
                                         .count();

                auto perNodeStart = std::chrono::steady_clock::now();
                for (const auto& node : comp.nodes) {
                    if (!node->needOutput) {
                        continue;
                    }
                    auto it = nodeFormulas.find(node);
                    if (it == nodeFormulas.end()) {
                        continue;
                    }
                    const auto& bdd = it->second;
                    double prob = 0.0;
                    if (componentEvs.empty()) {
                        prob = computeWmcProfile(bdd, nodeWmcComputeMs, nodeWmcCalls);
                    } else if (evidenceWeight == 0.0) {
                        prob = 0.0;
                    } else {
                        auto joint = makeAndProfile(bdd, evidenceBdd, nodeMakeAndMs, nodeMakeAndCalls);
                        double jointW = computeWmcProfile(joint, nodeWmcComputeMs, nodeWmcCalls);
                        prob = jointW / evidenceWeight;
                    }
                    probResult[node] = prob;
                }
                perNodeWmcMs += std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::steady_clock::now() - perNodeStart)
                                        .count();
            }
            for (const auto& [node, prob] : precomputedProbResult) {
                probResult.emplace(node, prob);
            }

            std::cout << "[pipeline] evidence resolve/tag took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()
                      << " ms\n";
            std::cout << "[pipeline] component evidence build took " << evidenceBuildMs << " ms\n";
            std::cout << "[pipeline] component evidence WMC took " << evidenceWmcMs << " ms\n";
            std::cout << "[pipeline] per-node conditional WMC took "
                      << perNodeWmcMs << " ms\n";
            if (wmcProfile) {
                std::cout << "[wmc-profile] stage=FULL"
                          << " mode=full"
                          << " total_ms=" << (evidenceBuildMs + evidenceWmcMs + perNodeWmcMs)
                          << " components=" << components.size()
                          << " nodes=" << view.getNodes().size()
                          << " evidence_build_ms=" << evidenceBuildMs
                          << " evidence_make_and_calls=" << evidenceMakeAndCalls
                          << " evidence_make_and_ms=" << evidenceMakeAndMs
                          << " evidence_wmc_calls=" << evidenceWmcCalls
                          << " evidence_wmc_compute_ms=" << evidenceWmcComputeMs
                          << " node_make_and_calls=" << nodeMakeAndCalls
                          << " node_make_and_ms=" << nodeMakeAndMs
                          << " node_wmc_calls=" << nodeWmcCalls
                          << " node_wmc_compute_ms=" << nodeWmcComputeMs
                          << " live_nodes=" << bddManager->getLiveNodeCount()
                          << std::endl;
            }

            debugger.endStage();
            debugger.startStage(StageKind::IO_DUMP_FULL);
            auto tDumpStart = std::chrono::steady_clock::now();
            dumpProbabilities(probResult, opt.getOutputFileDir());
            auto tDumpMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - tDumpStart)
                                   .count();
            std::cout << "[pipeline] probability dump took " << tDumpMs << " ms\n";
            debugger.endStage();
        }
    }

    debugger.endTurn();
    dumpInitialInputRelations(opt.getOutputFileDir() + "/initial-input-relations-iter0.txt");
}

static void runSddPipeline(
        const CmdOptions& opt,
        SouffleProgram& /*program*/,
        RuleManager& /*ruleManager*/,
        QueryManager& /*queryManager*/,
        DerivationGraph& graph,
        SubgraphView& view,
        const std::vector<std::pair<UntypedTuple, bool>>& evidences,
        StageInfo* rewriteHybridStage) {
    Debugger& debugger = Debugger::getInstance();

    std::map<NodePtr, SddNodeRef> nodeFormulas;
    std::map<EdgePtr, SddNodeRef> edgeFormulas;
    std::unique_ptr<SddFormulaManager> sddManager;
    const bool computeProbabilities = !opt.isDerivationOnly();
    std::unordered_set<NodePtr> seedTrueNodes;
    seedTrueNodes.reserve(view.getNodes().size());
    for (const auto& n : view.getNodes()) {
        const std::string s = n->getTuple().toString();
        if (s.rfind("@magic.", 0) != 0) continue;

        if (view.getIncomingEdges(n).empty()) {
            seedTrueNodes.insert(n);
        }
    }
    //std::cout << "[dbg] seedTrueNodes size = " << seedTrueNodes.size() << "\

    if (computeProbabilities) {
        if (tryRunScbfSdd(
                    opt, view, evidences, seedTrueNodes, nodeFormulas, edgeFormulas, sddManager,
                    rewriteHybridStage)) {
            // SCBF path handled FC/WMC and dump.
        } else if (opt.isRewriteEnabled()) {
            auto* hybridStage = rewriteHybridStage;
            if (!hybridStage) {
                hybridStage = debugger.startStage(StageKind::FC_WMC_HYBRID_FULL);
            }
            auto varEstimate = estimateBddVarCount(view);
            debugger.addInfo("rand_vars", std::to_string(varEstimate));
            if (hybridStage) {
                hybridStage->logMessage(Level::INFO, "rand_vars=" + std::to_string(varEstimate));
            }

            auto components = buildComponentSubgraphs(view);
            auto analyses = analyzeComponents(view, std::move(components));

            auto t2 = std::chrono::steady_clock::now();
            auto resolvedEvs = applyEvidence(graph, evidences);
            auto t3 = std::chrono::steady_clock::now();
            auto evidencesByComponent = groupEvidencesByComponent(view, resolvedEvs);

            std::vector<FastComponentEval> fastComponents;
            std::vector<ConjComponentEval> conjComponents;
            std::vector<ComponentSubgraph> slowComponents;
            FastComponentStats fastStats;
            ConjFastStats conjStats;
            const bool logFastReasons = opt.isDumpDotEnabled();
            std::vector<ComponentDecision> decisions;
            decisions.reserve(analyses.size());

            const bool enableFast = opt.isSingleRandFastEnabled();
            for (auto& analysis : analyses) {
                const auto& comp = analysis.comp;
                const auto& compEvs = evidencesByComponent[comp.id];
                const bool hasEvidence = !compEvs.empty();
                const bool singleCandidate = analysis.randVars == 1;
                const bool conjCandidate = !singleCandidate && !analysis.hasNegation &&
                        !analysis.hasOr && !analysis.hasCycle;  // fast path excludes cycles/OR/negation
                ComponentDecision decision{
                        comp.id,
                        comp.nodes.size(),
                        comp.edges.size(),
                        analysis.randVars,
                        hasEvidence,
                        analysis.hasNegation,
                        analysis.hasOr,
                        analysis.hasCycle,
                        "",
                        "",
                };

                if (enableFast) {
                    if (singleCandidate) fastStats.candidates++;
                    if (conjCandidate) conjStats.candidates++;
                }
                if (logFastReasons) {
                    std::vector<std::string> singleReasons;
                    if (!enableFast) singleReasons.emplace_back("fast_disabled");
                    if (!singleCandidate) singleReasons.emplace_back("randvars!=1");
                    if (hasEvidence) singleReasons.emplace_back("has_evidence");
                    if (!singleReasons.empty()) {
                        std::cout << "[fc-component] id=" << comp.id
                                  << " single_skip=" << join(singleReasons, ",")
                                  << " rand_vars=" << analysis.randVars
                                  << " has_negation=" << analysis.hasNegation
                                  << " has_or=" << analysis.hasOr
                                  << " has_cycle=" << analysis.hasCycle
                                  << std::endl;
                    }
                    std::vector<std::string> conjReasons;
                    if (!enableFast) conjReasons.emplace_back("fast_disabled");
                    if (singleCandidate) conjReasons.emplace_back("single_randvar");
                    if (analysis.hasNegation) conjReasons.emplace_back("negation");
                    if (analysis.hasOr) conjReasons.emplace_back("or");
                    if (analysis.hasCycle) conjReasons.emplace_back("cycle");
                    if (hasEvidence) conjReasons.emplace_back("has_evidence");
                    if (!conjReasons.empty()) {
                        std::cout << "[fc-component] id=" << comp.id
                                  << " conj_skip=" << join(conjReasons, ",")
                                  << " rand_vars=" << analysis.randVars
                                  << " has_negation=" << analysis.hasNegation
                                  << " has_or=" << analysis.hasOr
                                  << " has_cycle=" << analysis.hasCycle
                                  << std::endl;
                    }
                }

                if (!enableFast || hasEvidence) {
                    if (enableFast && singleCandidate) fastStats.skipped++;
                    if (enableFast && conjCandidate) conjStats.skipped++;
                    decision.mode = "slow";
                    decision.reason = !enableFast ? "fast_disabled" : "has_evidence";
                    decisions.push_back(decision);
                    slowComponents.push_back(std::move(analysis.comp));
                    continue;
                }

                if (singleCandidate) {
                    FastComponentEval eval;
                    eval.var = analysis.singleRand;
                    if (!evaluateSingleRandComponent(
                                analysis.comp, eval.var, true, eval.valuesTrue, &eval.evalMsTrue) ||
                            !evaluateSingleRandComponent(
                                    analysis.comp, eval.var, false, eval.valuesFalse, &eval.evalMsFalse)) {
                        if (logFastReasons) {
                            std::cout << "[fc-component] id=" << comp.id
                                      << " single_skip=eval_failed"
                                      << " rand_vars=" << analysis.randVars
                                      << std::endl;
                        }
                        fastStats.skipped++;
                        decision.mode = "slow";
                        decision.reason = "eval_failed";
                        decisions.push_back(decision);
                        slowComponents.push_back(std::move(analysis.comp));
                        continue;
                    }
                    eval.comp = std::move(analysis.comp);
                    fastStats.used++;
                    fastStats.evalMs += eval.evalMsTrue + eval.evalMsFalse;
                    decision.mode = "fast_single";
                    decisions.push_back(decision);
                    fastComponents.push_back(std::move(eval));
                    continue;
                }

                if (conjCandidate) {
                    ConjComponentEval eval;
                    if (!evaluateConjComponent(analysis.comp, eval.probabilities, &eval.evalMs)) {
                        if (logFastReasons) {
                            std::cout << "[fc-component] id=" << comp.id
                                      << " conj_skip=eval_failed"
                                      << " rand_vars=" << analysis.randVars
                                      << std::endl;
                        }
                        conjStats.skipped++;
                        decision.mode = "slow";
                        decision.reason = "eval_failed";
                        decisions.push_back(decision);
                        slowComponents.push_back(std::move(analysis.comp));
                        continue;
                    }
                    eval.comp = std::move(analysis.comp);
                    conjStats.used++;
                    conjStats.evalMs += eval.evalMs;
                    decision.mode = "fast_conj";
                    decisions.push_back(decision);
                    conjComponents.push_back(std::move(eval));
                    continue;
                }

                decision.mode = "slow";
                if (analysis.hasOr) {
                    decision.reason = "or";
                } else if (analysis.hasNegation) {
                    decision.reason = "negation";
                } else if (analysis.hasCycle) {
                    decision.reason = "cycle";
                } else {
                    decision.reason = "other";
                }
                decisions.push_back(decision);
                slowComponents.push_back(std::move(analysis.comp));
            }

            long long initMsTotal = 0;
            long long initMsMax = 0;
            auto makeManager = [&](SubgraphView&) {
                return std::make_unique<SddFormulaManager>();
            };

            std::vector<ComponentFormulaBundle<SddFormulaManager, SddNodeRef>> bundles;
            long long buildMs = 0;
            if (!slowComponents.empty()) {
                auto t0 = std::chrono::steady_clock::now();
                bundles = buildFormulasCyclewiseByComponentList<SddFormulaManager, SddNodeRef>(
                        std::move(slowComponents), makeManager, &initMsTotal, &initMsMax);
                auto t1 = std::chrono::steady_clock::now();
                auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
                buildMs = totalMs - initMsTotal;
                if (buildMs < 0) {
                    buildMs = totalMs;
                }
            }
            long long liveNodesSum = 0;
            for (const auto& bundle : bundles) {
                liveNodesSum += static_cast<long long>(bundle.manager->getLiveNodeCount());
            }

            debugger.addInfo("manager_init_ms", std::to_string(initMsTotal));
            debugger.addInfo("manager_init_ms_max", std::to_string(initMsMax));
            debugger.addInfo("manager_init_components", std::to_string(bundles.size()));
            debugger.addInfo("live_nodes", std::to_string(liveNodesSum));
            debugger.addInfo("fastpath_components", std::to_string(fastStats.used));
            debugger.addInfo("fastpath_candidates", std::to_string(fastStats.candidates));
            debugger.addInfo("fastpath_skipped", std::to_string(fastStats.skipped));
            debugger.addInfo("fastpath_eval_ms", std::to_string(fastStats.evalMs));
            debugger.addInfo("fastpath_conj_components", std::to_string(conjStats.used));
            debugger.addInfo("fastpath_conj_candidates", std::to_string(conjStats.candidates));
            debugger.addInfo("fastpath_conj_skipped", std::to_string(conjStats.skipped));
            debugger.addInfo("fastpath_conj_eval_ms", std::to_string(conjStats.evalMs));
            if (hybridStage) {
                hybridStage->logMessage(Level::INFO, "manager_init_ms=" + std::to_string(initMsTotal));
                hybridStage->logMessage(Level::INFO, "manager_init_ms_max=" + std::to_string(initMsMax));
                hybridStage->logMessage(Level::INFO, "manager_init_components=" +
                        std::to_string(bundles.size()));
                hybridStage->logMessage(Level::INFO, "live_nodes=" + std::to_string(liveNodesSum));
                hybridStage->logMessage(Level::INFO, "fastpath_components=" + std::to_string(fastStats.used));
                hybridStage->logMessage(Level::INFO, "fastpath_candidates=" + std::to_string(fastStats.candidates));
                hybridStage->logMessage(Level::INFO, "fastpath_skipped=" + std::to_string(fastStats.skipped));
                hybridStage->logMessage(Level::INFO, "fastpath_eval_ms=" + std::to_string(fastStats.evalMs));
                hybridStage->logMessage(Level::INFO, "fastpath_conj_components=" +
                        std::to_string(conjStats.used));
                hybridStage->logMessage(Level::INFO, "fastpath_conj_candidates=" +
                        std::to_string(conjStats.candidates));
                hybridStage->logMessage(Level::INFO, "fastpath_conj_skipped=" +
                        std::to_string(conjStats.skipped));
                hybridStage->logMessage(Level::INFO, "fastpath_conj_eval_ms=" +
                        std::to_string(conjStats.evalMs));
            }
            if (logFastReasons) {
                std::size_t fastSingle = 0;
                std::size_t fastConj = 0;
                std::size_t slowCount = 0;
                for (const auto& decision : decisions) {
                    if (decision.mode == "fast_single") {
                        fastSingle++;
                    } else if (decision.mode == "fast_conj") {
                        fastConj++;
                    } else {
                        slowCount++;
                    }
                }
                std::cout << "[fc-component-info] total=" << decisions.size()
                          << " fast_single=" << fastSingle
                          << " fast_conj=" << fastConj
                          << " slow=" << slowCount
                          << std::endl;
                for (const auto& decision : decisions) {
                    std::cout << "[fc-component-info] id=" << decision.id
                              << " nodes=" << decision.nodes
                              << " edges=" << decision.edges
                              << " rand_vars=" << decision.randVars
                              << " has_negation=" << decision.hasNegation
                              << " has_or=" << decision.hasOr
                              << " has_cycle=" << decision.hasCycle
                              << " has_evidence=" << decision.hasEvidence
                              << " mode=" << decision.mode
                              << " reason=" << decision.reason
                              << std::endl;
                }
            }

            std::cout << "[pipeline] SDD formula build took " << buildMs << " ms\n";

            long long evidenceBuildMs = 0;
            long long evidenceWmcMs = 0;
            long long perNodeWmcMs = 0;
            long long fastPathMs = fastStats.evalMs + conjStats.evalMs;

            probResult.clear();
            if (!fastComponents.empty()) {
                for (const auto& fast : fastComponents) {
                    double p = fast.var.probability;
                    for (const auto& node : fast.comp.nodes) {
                        if (!node->needOutput) {
                            continue;
                        }
                        auto itTrue = fast.valuesTrue.find(node);
                        auto itFalse = fast.valuesFalse.find(node);
                        if (itTrue == fast.valuesTrue.end() && itFalse == fast.valuesFalse.end()) {
                            continue;
                        }
                        bool vTrue = (itTrue != fast.valuesTrue.end()) ? itTrue->second : false;
                        bool vFalse = (itFalse != fast.valuesFalse.end()) ? itFalse->second : false;
                        double numerator = (vTrue ? p : 0.0) + (vFalse ? (1.0 - p) : 0.0);
                        double prob = numerator;
                        probResult[node] = prob;
                    }
                }
            }
            if (!conjComponents.empty()) {
                for (const auto& conj : conjComponents) {
                    for (const auto& [node, prob] : conj.probabilities) {
                        if (!node->needOutput) {
                            continue;
                        }
                        probResult[node] = prob;
                    }
                }
            }
            for (auto& bundle : bundles) {
                auto& manager = *bundle.manager;
                const auto& componentEvs = evidencesByComponent[bundle.id];

                auto buildStart = std::chrono::steady_clock::now();
                auto evidenceSdd = manager.getTrue();
                for (const auto& [eNode, val] : componentEvs) {
                    auto it = bundle.nodeFormulas.find(eNode);
                    if (it == bundle.nodeFormulas.end()) {
                        throw std::runtime_error("Evidence node has no formula: " +
                                eNode->getTuple().toString());
                    }
                    auto lit = it->second;
                    if (!val) {
                        lit = manager.makeNot(lit);
                    }
                    evidenceSdd = manager.makeAnd(evidenceSdd, lit);
                }
                evidenceBuildMs += std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - buildStart)
                                           .count();

                auto wmcStart = std::chrono::steady_clock::now();
                double evidenceWeight = 1.0;
                if (!componentEvs.empty()) {
                    evidenceWeight = manager.computeWeightedModelCount(evidenceSdd);
                }
                evidenceWmcMs += std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::steady_clock::now() - wmcStart)
                                         .count();

                auto perNodeStart = std::chrono::steady_clock::now();
                for (const auto& [node, sdd] : bundle.nodeFormulas) {
                    if (!node->needOutput) {
                        continue;
                    }
                    double prob = 0.0;
                    if (componentEvs.empty()) {
                        prob = manager.computeWeightedModelCount(sdd);
                    } else if (evidenceWeight == 0.0) {
                        prob = 0.0;
                    } else {
                        auto joint = manager.makeAnd(sdd, evidenceSdd);
                        double jointW = manager.computeWeightedModelCount(joint);
                        prob = jointW / evidenceWeight;
                    }
                    probResult[node] = prob;
                }
                perNodeWmcMs += std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::steady_clock::now() - perNodeStart)
                                        .count();
            }
            for (const auto& [node, prob] : precomputedProbResult) {
                probResult.emplace(node, prob);
            }

            view.dumpStatistics(std::cout);
            std::cout << "[pipeline] evidence resolve/tag took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()
                      << " ms\n";
            std::cout << "[pipeline] component evidence build took " << evidenceBuildMs << " ms\n";
            std::cout << "[pipeline] component evidence WMC took " << evidenceWmcMs << " ms\n";
            std::cout << "[pipeline] per-node conditional WMC took " << perNodeWmcMs << " ms\n";
            if (fastStats.used > 0 || conjStats.used > 0) {
                std::cout << "[pipeline] fastpath WMC took " << fastPathMs << " ms\n";
            }

            debugger.endStage();
            debugger.startStage(StageKind::IO_DUMP_FULL);
            auto tDumpStart = std::chrono::steady_clock::now();
            dumpProbabilities(probResult, opt.getOutputFileDir());
            auto tDumpMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - tDumpStart)
                                   .count();
            std::cout << "[pipeline] probability dump took " << tDumpMs << " ms\n";
            debugger.endStage();
        } else {
            auto* fcStage = debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
            auto initStart = std::chrono::steady_clock::now();
            sddManager = std::make_unique<SddFormulaManager>();
            auto initMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now() - initStart)
                                  .count();
            debugger.addInfo("manager_init_ms", std::to_string(initMs));
            if (fcStage) {
                fcStage->logMessage(Level::INFO, "manager_init_ms=" + std::to_string(initMs));
            }
            auto t0 = std::chrono::steady_clock::now();
            buildFormulasCyclewise(view, *sddManager, nodeFormulas, edgeFormulas, seedTrueNodes);
            auto t1 = std::chrono::steady_clock::now();
            std::cout << "[pipeline] SDD formula build took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
                      << " ms\n";
            debugger.endStage();

            debugger.startStage(StageKind::WEIGHTED_MODEL_COUNTING_FULL);

            auto t2 = std::chrono::steady_clock::now();
            auto resolvedEvs = applyEvidence(graph, evidences);
            auto t3 = std::chrono::steady_clock::now();

            auto components = buildComponentSubgraphs(view);
            auto evidencesByComponent = groupEvidencesByComponent(view, resolvedEvs);
            long long evidenceBuildMs = 0;
            long long evidenceWmcMs = 0;
            long long perNodeWmcMs = 0;

            probResult.clear();
            for (const auto& comp : components) {
                const auto& componentEvs = evidencesByComponent[comp.id];
                auto evidenceBuildStart = std::chrono::steady_clock::now();
                auto evidenceSdd = sddManager->getTrue();
                for (const auto& [eNode, val] : componentEvs) {
                    auto it = nodeFormulas.find(eNode);
                    if (it == nodeFormulas.end()) {
                        throw std::runtime_error("Evidence node has no formula: " +
                                eNode->getTuple().toString());
                    }
                    auto lit = it->second;
                    if (!val) {
                        lit = sddManager->makeNot(lit);
                    }
                    evidenceSdd = sddManager->makeAnd(evidenceSdd, lit);
                }
                evidenceBuildMs += std::chrono::duration_cast<std::chrono::milliseconds>(
                                           std::chrono::steady_clock::now() - evidenceBuildStart)
                                           .count();

                auto wmcStart = std::chrono::steady_clock::now();
                double evidenceWeight = 1.0;
                if (!componentEvs.empty()) {
                    evidenceWeight = sddManager->computeWeightedModelCount(evidenceSdd);
                }
                evidenceWmcMs += std::chrono::duration_cast<std::chrono::milliseconds>(
                                         std::chrono::steady_clock::now() - wmcStart)
                                         .count();

                auto perNodeStart = std::chrono::steady_clock::now();
                for (const auto& node : comp.nodes) {
                    if (!node->needOutput) {
                        continue;
                    }
                    auto it = nodeFormulas.find(node);
                    if (it == nodeFormulas.end()) {
                        continue;
                    }
                    const auto& sdd = it->second;
                    double prob = 0.0;
                    if (componentEvs.empty()) {
                        prob = sddManager->computeWeightedModelCount(sdd);
                    } else if (evidenceWeight == 0.0) {
                        prob = 0.0;
                    } else {
                        auto joint = sddManager->makeAnd(sdd, evidenceSdd);
                        double jointW = sddManager->computeWeightedModelCount(joint);
                        prob = jointW / evidenceWeight;
                    }
                    probResult[node] = prob;
                }
                perNodeWmcMs += std::chrono::duration_cast<std::chrono::milliseconds>(
                                        std::chrono::steady_clock::now() - perNodeStart)
                                        .count();
            }
            for (const auto& [node, prob] : precomputedProbResult) {
                probResult.emplace(node, prob);
            }

            view.dumpStatistics(std::cout);
            std::cout << "[pipeline] evidence resolve/tag took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()
                      << " ms\n";
            std::cout << "[pipeline] component evidence build took " << evidenceBuildMs << " ms\n";
            std::cout << "[pipeline] component evidence WMC took " << evidenceWmcMs << " ms\n";
            std::cout << "[pipeline] per-node conditional WMC took "
                      << perNodeWmcMs << " ms\n";

            debugger.endStage();
            debugger.startStage(StageKind::IO_DUMP_FULL);
            auto tDumpStart = std::chrono::steady_clock::now();
            dumpProbabilities(probResult, opt.getOutputFileDir());
            auto tDumpMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - tDumpStart)
                                   .count();
            std::cout << "[pipeline] probability dump took " << tDumpMs << " ms\n";
            debugger.endStage();
        }
    }

    debugger.endTurn();
    dumpInitialInputRelations(opt.getOutputFileDir() + "/initial-input-relations-iter0.txt");
}

void runPipeline(
        const CmdOptions& opt,
        SouffleProgram& program,
        RuleManager& ruleManager,
        QueryManager& queryManager,
        const std::unordered_map<UntypedTuple, double>& factProb,
        const std::vector<std::pair<UntypedTuple, bool>>& evidences) {
    std::cout << std::fixed << std::setprecision(8);
    Debugger& debugger = Debugger::getInstance();
    fcProfileEnabled = opt.isFcProfileEnabled();
    wmcProfileEnabled = opt.isWmcProfileEnabled();
    depGraphProfileEnabled = opt.isDepGraphProfileEnabled();
    postDelEnabled = opt.isPostDelEnabled();
    DerivationGraphViewInterface::setDumpDotEnabled(opt.isDumpDotEnabled());
    DerivationGraphViewInterface::setDumpJsonEnabled(opt.isDumpJsonEnabled());
    DerivationGraphViewInterface::setDumpStatsEnabled(opt.isDumpStatEnabled());
    DerivationGraphViewInterface::setDumpOutputDir(opt.getOutputFileDir());
    DerivationGraph::setMergeBiImpEnabled(fullOnlyMode && opt.isMergeBiImpEnabled());
    DerivationGraph::setPruneExtraEnabled(opt.isPruneExtraEnabled());
    DerivationGraph::setConstFoldEnabled(opt.isConstFoldEnabled());
    DerivationGraph::setConstDumpEnabled(opt.isDumpConstEnabled());
    precomputedProbResult.clear();
    precomputedTupleProbResult.clear();

    if (::detForceEnabled) {
        std::cout << "[det-force] enabled; skip derivation graph and emit prob=1.0" << std::endl;
        dumpDeterministicProbabilities(opt, program);
        debugger.endTurn();
        dumpInitialInputRelations(opt.getOutputFileDir() + "/initial-input-relations-iter0.txt");
        return;
    }

    debugger.startStage(StageKind::CREATE_GRAPH_FULL);
    debugger.addInfo("input_fact_size", std::to_string(countInitialInputFacts()));
    auto t0 = std::chrono::steady_clock::now();
    auto graph = std::unique_ptr<IncrementalDerivationGraph>(IncrementalDerivationGraph::createFrom(
            DerivationManager::untypedTuple2RuleApplications, ruleManager, queryManager, factProb, evidences));
    auto t1 = std::chrono::steady_clock::now();
    std::cout << "[pipeline] create graph took "
              << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
              << " ms\n";
    debugger.endStage();

    if (opt.isDumpDotEnabled()) {
        graph->dumpDot(makeOutputPath(opt, "before_prune.dot"));
    }

    debugger.startStage(StageKind::PRUNING_FULL);
    auto t2 = std::chrono::steady_clock::now();
    auto prunedView = graph->prune(program.getOutputRelations());
    auto view = buildFullIncViewLocal(prunedView.getNodes(), prunedView.getEdges());
    auto t3 = std::chrono::steady_clock::now();
    std::cout << "[pipeline] pruning took "
              << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()
              << " ms\n";
    debugger.endStage();

    if (opt.isDumpDotEnabled()) {
        view.dumpDot(makeOutputPath(opt, "after_prune.dot"));
    }
    if (opt.isDumpJsonEnabled()) {
        view.dumpJson(makeOutputPath(opt, "derivation.json"));
    }
    StageInfo* rewriteHybridStage = nullptr;
    if (opt.isRewriteEnabled() && !opt.isDerivationOnly()) {
        rewriteHybridStage = debugger.startStage(StageKind::FC_WMC_HYBRID_FULL);
    }
    if (opt.isRewriteEnabled() && !opt.isDerivationOnly()) {
        auto rewriteStart = std::chrono::steady_clock::now();
        GraphRewriteStats rewriteStats;
        if (opt.isImplicitRewriteEnabled()) {
            std::unordered_set<UntypedTuple> originalOutputTuples;
            std::unordered_set<std::string> originalOutputRelations;
            for (const auto& node : view.getNodes()) {
                if (node && node->needOutput) {
                    originalOutputTuples.insert(node->getTuple());
                    originalOutputRelations.insert(node->getTuple().relation_name);
                }
            }
            ImplicitSplitPipelineOptions rewriteOptions;
            rewriteOptions.splitMode = resolveImplicitSplitMode(opt.getSplitMode());
            rewriteOptions.runOverlayFastPaths = true;
            rewriteOptions.runOverlaySingleHyperedge = false;
            rewriteOptions.runOverlayAllFacts = true;
            rewriteOptions.computeOutputMarginals = false;
            rewriteOptions.collectPatternStats = false;
            rewriteOptions.iterateSplitRewrite = false;
            auto implicitResult = runImplicitSplitRewritePipeline(view, rewriteOptions);
            rewriteStats = implicitResult.stats.graphRewriteStats;
            precomputedTupleProbResult.clear();
            for (const auto& [tupleStr, prob] : implicitResult.carriedPrecomputedTupleProbs) {
                precomputedTupleProbResult[tupleStr] = prob;
            }
            graph = std::move(implicitResult.materialized.graph);
            if (!graph) {
                graph = std::make_unique<IncrementalDerivationGraph>();
            }
            view = buildFullIncViewLocal(
                    implicitResult.materialized.liveNodes, implicitResult.materialized.liveEdges);
            std::unordered_set<UntypedTuple> liveViewTuples;
            liveViewTuples.reserve(view.getNodes().size());
            for (const auto& node : view.getNodes()) {
                if (node) {
                    liveViewTuples.insert(node->getTuple());
                }
            }
            std::unordered_set<UntypedTuple> precomputedTuples;
            std::unordered_set<std::string> precomputedTupleStrings;
            precomputedTuples.reserve(precomputedProbResult.size() + precomputedTupleProbResult.size());
            precomputedTupleStrings.reserve(precomputedProbResult.size() + precomputedTupleProbResult.size());
            for (const auto& [node, _] : precomputedProbResult) {
                if (node) {
                    precomputedTuples.insert(node->getTuple());
                    precomputedTupleStrings.insert(node->getTuple().toString());
                }
            }
            for (const auto& [tupleStr, _] : precomputedTupleProbResult) {
                precomputedTupleStrings.insert(tupleStr);
            }
            for (const auto& node : view.getNodes()) {
                if (node && originalOutputRelations.count(node->getTuple().relation_name) &&
                        !precomputedTuples.count(node->getTuple()) &&
                        !precomputedTupleStrings.count(node->getTuple().toString())) {
                    node->setQuery();
                }
            }
            const std::size_t recoveredIsolatedFacts = precomputeIsolatedOutputFactsLocal(view);
            if (recoveredIsolatedFacts > 0) {
                precomputedTuples.clear();
                precomputedTupleStrings.clear();
                precomputedTuples.reserve(precomputedProbResult.size() + precomputedTupleProbResult.size());
                precomputedTupleStrings.reserve(precomputedProbResult.size() + precomputedTupleProbResult.size());
                for (const auto& [node, _] : precomputedProbResult) {
                    if (node) {
                        precomputedTuples.insert(node->getTuple());
                        precomputedTupleStrings.insert(node->getTuple().toString());
                    }
                }
                for (const auto& [tupleStr, _] : precomputedTupleProbResult) {
                    precomputedTupleStrings.insert(tupleStr);
                }
            }
            std::size_t recoveredOutputFacts = 0;
            for (const auto& tuple : originalOutputTuples) {
                if (liveViewTuples.count(tuple) || precomputedTuples.count(tuple) ||
                        precomputedTupleStrings.count(tuple.toString())) {
                    continue;
                }
                NodePtr recovered = graph ? graph->findNode(tuple) : nullptr;
                if (!recovered || !recovered->isFact) {
                    continue;
                }
                precomputedTupleProbResult.emplace(tuple.toString(), recovered->getProbability());
                ++recoveredOutputFacts;
            }
            if (recoveredIsolatedFacts > 0 || recoveredOutputFacts > 0) {
                std::cout << "[pipeline] implicit recovered isolated_output_facts="
                          << recoveredIsolatedFacts
                          << " tuple_output_facts=" << recoveredOutputFacts << std::endl;
            }
            const GraphSummary implicitHandoffSummary = summarizeGraphLight(view);
            std::cout << "[pipeline] implicit handoff"
                      << " nodes=" << implicitHandoffSummary.nodes
                      << " edges=" << implicitHandoffSummary.edges
                      << " random_vars=" << implicitHandoffSummary.randomVariables
                      << " output_nodes=" << implicitHandoffSummary.outputNodes
                      << " precomputed_nodes=" << precomputedProbResult.size()
                      << " precomputed_tuples=" << precomputedTupleProbResult.size()
                      << std::endl;
            std::cout << "[pipeline] implicit rewrite total_ms=" << implicitResult.stats.totalMs
                      << " overlay_prep_ms=" << implicitResult.stats.overlayPrepMs
                      << " overlay_split_ms=" << implicitResult.stats.overlaySplitMs
                      << " overlay_fastpath_ms=" << implicitResult.stats.overlayFastPathMs
                      << " overlay_siso_detect_ms=" << implicitResult.stats.overlayStats.fastPathDetectMs
                      << " overlay_siso_summarize_ms=" << implicitResult.stats.overlayStats.fastPathSummarizeMs
                      << " materialize_ms=" << implicitResult.stats.materializeMs
                      << " detect_ms=" << implicitResult.stats.graphDetectMs
                      << " graph_rewrite_ms=" << implicitResult.stats.graphRewriteMs
                      << " graph_bdd_compile_ms=" << implicitResult.stats.graphRewriteStats.totalBddBuildMs
                      << " graph_detected_regions=" << implicitResult.stats.graphRewriteStats.numRegionsDetected
                      << " overlay_aliases=" << implicitResult.stats.overlayStats.aliasesCreated
                      << " overlay_edges_aliased=" << implicitResult.stats.overlayStats.edgesAliased
                      << " overlay_all_facts=" << implicitResult.stats.overlayStats.allFactsRewrites
                      << " overlay_single=" << implicitResult.stats.overlayStats.singleHyperedgeRewrites
                      << std::endl;
            if (rewriteHybridStage) {
                const auto formatMs = [](double value) {
                    std::ostringstream oss;
                    oss << std::fixed << std::setprecision(6) << value;
                    return oss.str();
                };
                const auto addImplicitInfo = [&](const std::string& key, double value) {
                    const std::string text = formatMs(value);
                    debugger.addInfo(key, text);
                    rewriteHybridStage->logMessage(Level::INFO, key + "=" + text);
                };
                const auto addImplicitTextInfo = [&](const std::string& key, const std::string& value) {
                    debugger.addInfo(key, value);
                    rewriteHybridStage->logMessage(Level::INFO, key + "=" + value);
                };
                const auto& implicitGraphStats = implicitResult.stats.graphRewriteStats;
                debugger.addInfo("rewrite_engine", "implicit");
                rewriteHybridStage->logMessage(Level::INFO, "rewrite_engine=implicit");
                addImplicitInfo("implicit_total_ms", implicitResult.stats.totalMs);
                addImplicitInfo("implicit_overlay_prep_ms", implicitResult.stats.overlayPrepMs);
                addImplicitInfo("implicit_overlay_split_ms", implicitResult.stats.overlaySplitMs);
                addImplicitInfo("implicit_overlay_fastpath_ms", implicitResult.stats.overlayFastPathMs);
                addImplicitInfo("implicit_overlay_siso_detect_ms",
                        implicitResult.stats.overlayStats.fastPathDetectMs);
                addImplicitInfo("implicit_overlay_siso_summarize_ms",
                        implicitResult.stats.overlayStats.fastPathSummarizeMs);
                addImplicitInfo("implicit_overlay_rebuild_index_ms",
                        implicitResult.stats.overlayStats.rebuildIndexMs);
                addImplicitInfo("implicit_overlay_fastpath_single_ms",
                        implicitResult.stats.overlayStats.fastPathSingleMs);
                addImplicitInfo("implicit_overlay_fastpath_linear_ms",
                        implicitResult.stats.overlayStats.fastPathLinearMs);
                addImplicitInfo("implicit_overlay_fastpath_parallel_ms",
                        implicitResult.stats.overlayStats.fastPathParallelMs);
                addImplicitInfo("implicit_overlay_fastpath_fan_out_ms",
                        implicitResult.stats.overlayStats.fastPathFanOutMs);
                addImplicitInfo("implicit_overlay_fastpath_allfacts_ms",
                        implicitResult.stats.overlayStats.fastPathAllFactsMs);
                addImplicitInfo("implicit_materialize_ms", implicitResult.stats.materializeMs);
                addImplicitInfo("implicit_graph_detect_ms", implicitResult.stats.graphDetectMs);
                addImplicitInfo("implicit_graph_rewrite_ms", implicitResult.stats.graphRewriteMs);
                addImplicitInfo("implicit_graph_detect_total_ms",
                        implicitGraphStats.totalDetectMs);
                addImplicitInfo("implicit_graph_bdd_manager_init_ms",
                        implicitGraphStats.totalBddManagerInitMs);
                addImplicitInfo("implicit_graph_bdd_compile_ms",
                        implicitGraphStats.totalBddBuildMs);
                addImplicitInfo("implicit_graph_bdd_wmc_ms",
                        implicitGraphStats.totalBddWmcMs);
                addImplicitInfo("implicit_graph_apply_ms", implicitGraphStats.totalApplyMs);
                addImplicitTextInfo("implicit_graph_detected_regions",
                        std::to_string(implicitGraphStats.numRegionsDetected));
                addImplicitTextInfo("implicit_graph_detected_region_total_edges",
                        std::to_string(implicitGraphStats.totalDetectedRegionEdges));
                addImplicitTextInfo("implicit_graph_detected_region_total_nodes",
                        std::to_string(implicitGraphStats.totalDetectedRegionNodes));
                debugger.addInfo("implicit_graph_general_regions",
                        std::to_string(implicitGraphStats.numGeneralRegionsRewritten));
                rewriteHybridStage->logMessage(Level::INFO,
                        "implicit_graph_general_regions=" +
                                std::to_string(implicitGraphStats.numGeneralRegionsRewritten));
            }
        } else {
            GraphRewriter rewriter;
            RewriteFeatureFlags rewriteFlags;
            const auto& splitMode = opt.getSplitMode();
            if (splitMode == "no-split") {
                rewriteFlags.splitMode = SplitMode::None;
            } else if (splitMode == "complete-split") {
                rewriteFlags.splitMode = SplitMode::Complete;
            } else {
                rewriteFlags.splitMode = SplitMode::Naive;
            }
            rewriteStats = rewriter.rewriteUntilFixpoint(*graph, view, opt.isProfiling(), rewriteFlags);
        }
        auto rewriteMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                 std::chrono::steady_clock::now() - rewriteStart)
                                 .count();
        auto randomVarsDelta = static_cast<long long>(rewriteStats.randomVarsBefore) -
                static_cast<long long>(rewriteStats.randomVarsAfter);
        double randomVarsRatio = rewriteStats.randomVarsBefore == 0
                                         ? 0.0
                                         : static_cast<double>(rewriteStats.randomVarsAfter) /
                                                   static_cast<double>(rewriteStats.randomVarsBefore);
        std::cout << "[pipeline] rewrite took " << rewriteMs << " ms; iterations="
                  << rewriteStats.numIterations << ", regions=" << rewriteStats.numRegionsRewritten
                  << ", detectedRegions=" << rewriteStats.numRegionsDetected
                  << ", nodesRemoved=" << rewriteStats.numNodesRemoved
                  << ", edgesRemoved=" << rewriteStats.numEdgesRemoved
                  << ", edgesAdded=" << rewriteStats.numEdgesAdded
                  << ", randomVarsBefore=" << rewriteStats.randomVarsBefore
                  << ", randomVarsAfter=" << rewriteStats.randomVarsAfter
                  << ", randomVarsDelta=" << randomVarsDelta
                  << ", randomVarsRatio=" << randomVarsRatio
                  << ", randomVarsRemoved=" << rewriteStats.totalRandomVars
                  << ", simpleFactRegions=" << rewriteStats.simpleFactRegions << std::endl;
        if (rewriteHybridStage) {
            const std::string rewriteEngine = opt.isImplicitRewriteEnabled() ? "implicit" : "legacy";
            debugger.addInfo("rewrite_engine", rewriteEngine);
            debugger.addInfo("rewrite_ms", std::to_string(rewriteMs));
            rewriteHybridStage->logMessage(Level::INFO, "rewrite_engine=" + rewriteEngine);
            rewriteHybridStage->logMessage(Level::INFO, "rewrite_ms=" + std::to_string(rewriteMs));

            if (!opt.isImplicitRewriteEnabled()) {
                const auto formatMs = [](double value) {
                    std::ostringstream oss;
                    oss << std::fixed << std::setprecision(6) << value;
                    return oss.str();
                };
                const auto addExplicitInfo = [&](const std::string& key, double value) {
                    const std::string text = formatMs(value);
                    debugger.addInfo(key, text);
                    rewriteHybridStage->logMessage(Level::INFO, key + "=" + text);
                };
                const auto addExplicitTextInfo = [&](const std::string& key, const std::string& value) {
                    debugger.addInfo(key, value);
                    rewriteHybridStage->logMessage(Level::INFO, key + "=" + value);
                };
                const double rewriteMsDouble = static_cast<double>(rewriteMs);
                addExplicitInfo("explicit_total_ms", rewriteMsDouble);
                addExplicitInfo("explicit_overlay_prep_ms", 0.0);
                addExplicitInfo("explicit_overlay_split_ms", 0.0);
                addExplicitInfo("explicit_overlay_fastpath_ms", 0.0);
                addExplicitInfo("explicit_overlay_siso_detect_ms", 0.0);
                addExplicitInfo("explicit_overlay_siso_summarize_ms", 0.0);
                addExplicitInfo("explicit_overlay_rebuild_index_ms", 0.0);
                addExplicitInfo("explicit_overlay_fastpath_single_ms", 0.0);
                addExplicitInfo("explicit_overlay_fastpath_linear_ms", 0.0);
                addExplicitInfo("explicit_overlay_fastpath_parallel_ms", 0.0);
                addExplicitInfo("explicit_overlay_fastpath_fan_out_ms", 0.0);
                addExplicitInfo("explicit_overlay_fastpath_allfacts_ms", 0.0);
                addExplicitInfo("explicit_materialize_ms", 0.0);
                addExplicitInfo("explicit_graph_detect_ms", rewriteStats.totalDetectMs);
                addExplicitInfo("explicit_graph_rewrite_ms", rewriteMsDouble);
                addExplicitInfo("explicit_graph_detect_total_ms", rewriteStats.totalDetectMs);
                addExplicitInfo("explicit_graph_bdd_manager_init_ms", rewriteStats.totalBddManagerInitMs);
                addExplicitInfo("explicit_graph_bdd_compile_ms", rewriteStats.totalBddBuildMs);
                addExplicitInfo("explicit_graph_bdd_wmc_ms", rewriteStats.totalBddWmcMs);
                addExplicitInfo("explicit_graph_apply_ms", rewriteStats.totalApplyMs);
                addExplicitTextInfo("explicit_graph_detected_regions",
                        std::to_string(rewriteStats.numRegionsDetected));
                addExplicitTextInfo("explicit_graph_detected_region_total_edges",
                        std::to_string(rewriteStats.totalDetectedRegionEdges));
                addExplicitTextInfo("explicit_graph_detected_region_total_nodes",
                        std::to_string(rewriteStats.totalDetectedRegionNodes));
                debugger.addInfo("explicit_graph_general_regions",
                        std::to_string(rewriteStats.numGeneralRegionsRewritten));
                rewriteHybridStage->logMessage(Level::INFO,
                        "explicit_graph_general_regions=" +
                                std::to_string(rewriteStats.numGeneralRegionsRewritten));
            }
        }
        if (opt.isDumpDotEnabled()) {
            view.dumpDot(makeOutputPath(opt, "rewrite_final.dot"));
        }
    } else if (opt.isRewriteEnabled() && opt.isDerivationOnly()) {
        std::cout << "[pipeline] derivation-only mode; skip rewrite" << std::endl;
    }

    if (program.getKnowledge() == souffle::Knowledge::BDD) {
        runBddPipeline(opt, program, ruleManager, queryManager, *graph, view, evidences, rewriteHybridStage);
    } else if (program.getKnowledge() == souffle::Knowledge::SDD) {
        runSddPipeline(opt, program, ruleManager, queryManager, *graph, view, evidences, rewriteHybridStage);
    } else {
        std::cerr << "Unknown knowledge representation" << std::endl;
    }
}

}  // namespace souffle::problog
