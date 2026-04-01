#include "souffle/problog/ImplicitSplitRewrite.h"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace souffle::problog {
namespace {

using Clock = std::chrono::steady_clock;

struct Options {
    std::string jsonPath;
    bool skipLegacyNoSplit = false;
    bool skipLegacySplit = false;
    bool skipImplicit = false;
    bool runtimeLikeImplicit = false;
    ImplicitSplitMode splitMode = ImplicitSplitMode::Naive;
};

class BenchmarkScopedCoutSilencer {
public:
    BenchmarkScopedCoutSilencer() : old_(std::cout.rdbuf(sink_.rdbuf())) {}
    ~BenchmarkScopedCoutSilencer() {
        std::cout.rdbuf(old_);
    }

private:
    std::ostringstream sink_;
    std::streambuf* old_ = nullptr;
};

double benchmarkElapsedMs(const Clock::time_point& start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

[[noreturn]] void usage(const char* argv0) {
    std::cerr << "usage: " << argv0 << " --json <derivation.json>"
              << " [--skip-legacy-no-split] [--skip-legacy-split] [--skip-implicit]"
              << " [--runtime-like-implicit]"
              << " [--split-complete]\n";
    throw std::runtime_error("invalid arguments");
}

Options parseArgs(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--json" && i + 1 < argc) {
            opt.jsonPath = argv[++i];
        } else if (arg == "--skip-legacy-no-split") {
            opt.skipLegacyNoSplit = true;
        } else if (arg == "--skip-legacy-split") {
            opt.skipLegacySplit = true;
        } else if (arg == "--skip-implicit") {
            opt.skipImplicit = true;
        } else if (arg == "--runtime-like-implicit") {
            opt.runtimeLikeImplicit = true;
        } else if (arg == "--split-complete") {
            opt.splitMode = ImplicitSplitMode::Complete;
        } else {
            usage(argv[0]);
        }
    }
    if (opt.jsonPath.empty()) {
        usage(argv[0]);
    }
    return opt;
}

IncSubgraphView buildFullView(IncrementalDerivationGraph& graph) {
    return IncSubgraphView(graph.getNodes(), graph.getEdges(), {}, {}, {}, {});
}

std::size_t countOutputNodes(const IncrementalDerivationGraph& graph) {
    std::size_t out = 0;
    for (const auto& node : graph.getNodes()) {
        if (node && node->needOutput) {
            ++out;
        }
    }
    return out;
}

std::size_t countFacts(const IncrementalDerivationGraph& graph) {
    std::size_t out = 0;
    for (const auto& node : graph.getNodes()) {
        if (node && node->isFact) {
            ++out;
        }
    }
    return out;
}

std::size_t countSemanticRandomVars(const IncrementalDerivationGraphViewInterface& view) {
    std::size_t out = 0;
    for (const auto& node : view.getNodes()) {
        if (node && node->isFact) {
            const double p = node->getProbability();
            if (p > 0.0 && p < 1.0) {
                ++out;
            }
        }
    }
    for (const auto& edge : view.getEdges()) {
        if (edge) {
            const double p = edge->getProbability();
            if (p > 0.0 && p < 1.0) {
                ++out;
            }
        }
    }
    return out;
}

std::size_t countLegacyRandomVars(const IncrementalDerivationGraphViewInterface& view) {
    std::size_t out = 0;
    for (const auto& node : view.getNodes()) {
        if (node && node->isFact && node->getProbability() != 1.0) {
            ++out;
        }
    }
    for (const auto& edge : view.getEdges()) {
        if (edge && !edge->isDeterministic()) {
            ++out;
        }
    }
    return out;
}

std::size_t countSemanticRandomVars(const ComponentSubgraph& comp) {
    std::size_t out = 0;
    for (const auto& node : comp.nodes) {
        if (node && node->isFact) {
            const double p = node->getProbability();
            if (p > 0.0 && p < 1.0) {
                ++out;
            }
        }
    }
    for (const auto& edge : comp.edges) {
        if (edge) {
            const double p = edge->getProbability();
            if (p > 0.0 && p < 1.0) {
                ++out;
            }
        }
    }
    return out;
}

std::size_t countSinkNodes(const IncrementalDerivationGraph& graph) {
    std::size_t out = 0;
    for (const auto& node : graph.getNodes()) {
        if (node && graph.getOutgoingEdges(node).empty()) {
            ++out;
        }
    }
    return out;
}

std::size_t markSinkNodesAsOutputs(IncrementalDerivationGraph& graph) {
    std::size_t marked = 0;
    for (const auto& node : graph.getNodes()) {
        if (!node) {
            continue;
        }
        if (!graph.getOutgoingEdges(node).empty()) {
            continue;
        }
        if (!node->needOutput) {
            node->setQuery();
            ++marked;
        }
    }
    return marked;
}

void ensureMeaningfulOutputs(IncrementalDerivationGraph& graph, std::ostream& out) {
    const auto outputs = countOutputNodes(graph);
    if (outputs > 0) {
        out << "[output-policy] mode=preserve outputs=" << outputs << "\n";
        return;
    }
    const auto sinks = countSinkNodes(graph);
    const auto marked = markSinkNodesAsOutputs(graph);
    out << "[output-policy] mode=mark-sinks previous_outputs=0 sinks=" << sinks
        << " marked=" << marked << "\n";
}

struct LegacyRunResult {
    double loadMs = 0.0;
    double rewriteMs = 0.0;
    std::size_t nodesBefore = 0;
    std::size_t edgesBefore = 0;
    std::size_t nodesAfter = 0;
    std::size_t edgesAfter = 0;
    GraphRewriteStats stats;
};

LegacyRunResult runLegacyRewrite(const std::string& jsonPath, SplitMode splitMode) {
    const auto loadStart = Clock::now();
    auto graph = std::unique_ptr<IncrementalDerivationGraph>(
            IncrementalDerivationGraph::loadFromJsonInc(jsonPath));
    ensureMeaningfulOutputs(*graph, std::cout);
    LegacyRunResult out;
    out.loadMs = benchmarkElapsedMs(loadStart);

    auto view = buildFullView(*graph);
    out.nodesBefore = view.getNodes().size();
    out.edgesBefore = view.getEdges().size();

    GraphRewriter rewriter;
    RewriteFeatureFlags flags;
    flags.splitMode = splitMode;
    const auto rewriteStart = Clock::now();
    {
        BenchmarkScopedCoutSilencer silence;
        out.stats = rewriter.rewriteUntilFixpoint(*graph, view, false, flags);
    }
    out.rewriteMs = benchmarkElapsedMs(rewriteStart);
    out.nodesAfter = view.getNodes().size();
    out.edgesAfter = view.getEdges().size();
    return out;
}

ImplicitSplitPipelineResult runImplicitRewrite(const std::string& jsonPath, ImplicitSplitMode splitMode,
        bool runtimeLike,
        double* loadMsOut) {
    const auto loadStart = Clock::now();
    auto graph = std::unique_ptr<IncrementalDerivationGraph>(
            IncrementalDerivationGraph::loadFromJsonInc(jsonPath));
    ensureMeaningfulOutputs(*graph, std::cout);
    if (loadMsOut) {
        *loadMsOut = benchmarkElapsedMs(loadStart);
    }
    auto view = buildFullView(*graph);
    ImplicitSplitPipelineOptions options;
    options.splitMode = splitMode;
    options.computeOutputMarginals = false;
    if (runtimeLike) {
        options.runOverlayFastPaths = false;
        options.collectPatternStats = false;
        options.iterateSplitRewrite = false;
    }
    return runImplicitSplitRewritePipeline(view, options);
}

void printLegacySummary(const std::string& label, const LegacyRunResult& run) {
    std::cout << "[" << label << "]"
              << " load_ms=" << run.loadMs
              << " rewrite_ms=" << run.rewriteMs
              << " iterations=" << run.stats.numIterations
              << " regions=" << run.stats.numRegionsRewritten
              << " rv_before=" << run.stats.randomVarsBefore
              << " rv_after=" << run.stats.randomVarsAfter
              << " simple_fact_regions=" << run.stats.simpleFactRegions
              << " nodes_before=" << run.nodesBefore
              << " edges_before=" << run.edgesBefore
              << " nodes_after=" << run.nodesAfter
              << " edges_after=" << run.edgesAfter
              << " edges_added=" << run.stats.numEdgesAdded
              << " nodes_removed=" << run.stats.numNodesRemoved
              << " edges_removed=" << run.stats.numEdgesRemoved
              << "\n";
}

void printImplicitSummary(const ImplicitSplitPipelineResult& run, double loadMs) {
    const auto& stats = run.stats;
    std::cout << "[implicit-split]"
              << " load_ms=" << loadMs
              << " total_ms=" << stats.totalMs
              << " overlay_prep_ms=" << stats.overlayPrepMs
              << " overlay_split_ms=" << stats.overlaySplitMs
              << " overlay_fastpath_ms=" << stats.overlayFastPathMs
              << " overlay_siso_detect_ms=" << stats.overlayStats.fastPathDetectMs
              << " overlay_siso_summarize_ms=" << stats.overlayStats.fastPathSummarizeMs
              << " materialize_ms=" << stats.materializeMs
              << " detect_ms=" << stats.graphDetectMs
              << " graph_rewrite_ms=" << stats.graphRewriteMs
              << " overlay_aliases=" << stats.overlayStats.aliasesCreated
              << " overlay_edges_aliased=" << stats.overlayStats.edgesAliased
              << " overlay_all_facts=" << stats.overlayStats.allFactsRewrites
              << " overlay_single=" << stats.overlayStats.singleHyperedgeRewrites
              << " split_facts=" << stats.overlayStats.splitFactsConsidered
              << " split_facts_aliased=" << stats.overlayStats.splitFactsAliased
              << " split_naive_ms=" << stats.overlayStats.splitNaiveMs
              << " split_complete_ms=" << stats.overlayStats.splitCompleteMs
              << " split_alias_apply_ms=" << stats.overlayStats.splitAliasApplyMs
              << " split_naive_bfs=" << stats.overlayStats.splitNaiveReachabilityRuns
              << " split_naive_visited=" << stats.overlayStats.splitNaiveReachabilityVisited
              << " split_naive_cap_skips=" << stats.overlayStats.splitNaiveCapSkips
              << " rebuild_index_count=" << stats.overlayStats.rebuildIndexCount
              << " rebuild_index_ms=" << stats.overlayStats.rebuildIndexMs
              << " fastpath_iterations=" << stats.overlayStats.fastPathIterations
              << " fastpath_single_ms=" << stats.overlayStats.fastPathSingleMs
              << " fastpath_linear_ms=" << stats.overlayStats.fastPathLinearMs
              << " fastpath_parallel_ms=" << stats.overlayStats.fastPathParallelMs
              << " fastpath_fan_out_ms=" << stats.overlayStats.fastPathFanOutMs
              << " fastpath_allfacts_ms=" << stats.overlayStats.fastPathAllFactsMs
              << " materialized_alias_nodes=" << stats.materializedAliasNodes
              << " nodes_before=" << stats.materializedNodesBefore
              << " edges_before=" << stats.materializedEdgesBefore
              << " nodes_after=" << stats.materializedNodesAfter
              << " edges_after=" << stats.materializedEdgesAfter
              << " graph_iterations=" << stats.graphRewriteStats.numIterations
              << " graph_regions=" << stats.graphRewriteStats.numRegionsRewritten
              << " graph_general_regions=" << stats.graphRewriteStats.numGeneralRegionsRewritten
              << " graph_rv_before=" << stats.graphRewriteStats.randomVarsBefore
              << " graph_rv_after=" << stats.graphRewriteStats.randomVarsAfter
              << " graph_detect_total_ms=" << stats.graphRewriteStats.totalDetectMs
              << " graph_bdd_manager_init_ms=" << stats.graphRewriteStats.totalBddManagerInitMs
              << " graph_bdd_compile_ms=" << stats.graphRewriteStats.totalBddBuildMs
              << " graph_bdd_wmc_ms=" << stats.graphRewriteStats.totalBddWmcMs
              << " graph_apply_ms=" << stats.graphRewriteStats.totalApplyMs
              << " before=" << summarizeRewritePatternCounts(stats.materializedDetectedBefore)
              << " after=" << summarizeRewritePatternCounts(stats.materializedDetectedAfter)
              << "\n";

    if (run.materialized.graph) {
        auto handoffView =
                IncSubgraphView(run.materialized.liveNodes, run.materialized.liveEdges, {}, {}, {}, {});
        const auto components = buildComponentSubgraphs(handoffView);
        std::size_t legacyRvTotal = 0;
        std::size_t semanticRvTotal = 0;
        std::size_t legacyRvComponents = 0;
        std::size_t semanticRvComponents = 0;
        std::size_t tiny4e1LegacyRv2 = 0;
        std::size_t tiny4e1SemanticRv2 = 0;
        std::size_t tiny4e1SemanticRv0LegacyRv2 = 0;
        for (const auto& comp : components) {
            const auto legacyRv = countComponentRandomVars(comp);
            const auto semanticRv = countSemanticRandomVars(comp);
            legacyRvTotal += legacyRv;
            semanticRvTotal += semanticRv;
            if (legacyRv > 0) {
                ++legacyRvComponents;
            }
            if (semanticRv > 0) {
                ++semanticRvComponents;
            }
            if (comp.nodes.size() == 4 && comp.edges.size() == 1) {
                if (legacyRv == 2) {
                    ++tiny4e1LegacyRv2;
                }
                if (semanticRv == 2) {
                    ++tiny4e1SemanticRv2;
                }
                if (semanticRv == 0 && legacyRv == 2) {
                    ++tiny4e1SemanticRv0LegacyRv2;
                }
            }
        }
        std::cout << "[implicit-handoff]"
                  << " nodes=" << handoffView.getNodes().size()
                  << " edges=" << handoffView.getEdges().size()
                  << " outputs=" << countOutputNodes(*run.materialized.graph)
                  << " precomputed_nodes=" << precomputedProbResult.size()
                  << " precomputed_tuples=" << precomputedTupleProbResult.size()
                  << " legacy_rv_total=" << legacyRvTotal
                  << " semantic_rv_total=" << semanticRvTotal
                  << " legacy_rv_components=" << legacyRvComponents
                  << " semantic_rv_components=" << semanticRvComponents
                  << " tiny4e1_legacy_rv2=" << tiny4e1LegacyRv2
                  << " tiny4e1_semantic_rv2=" << tiny4e1SemanticRv2
                  << " tiny4e1_semantic_rv0_legacy_rv2=" << tiny4e1SemanticRv0LegacyRv2
                  << "\n";
    }
}

}  // namespace
}  // namespace souffle::problog

int runImplicitSplitJsonBenchmarkMain(int argc, char** argv) {
    using namespace souffle::problog;

    try {
        const Options opt = parseArgs(argc, argv);
        std::cout << std::fixed << std::setprecision(3);

        const auto loadStart = Clock::now();
        auto graphInfo = std::unique_ptr<IncrementalDerivationGraph>(
                IncrementalDerivationGraph::loadFromJsonInc(opt.jsonPath));
        ensureMeaningfulOutputs(*graphInfo, std::cout);
        const double loadInfoMs = benchmarkElapsedMs(loadStart);
        std::cout << "[input] json=" << opt.jsonPath
                  << " nodes=" << graphInfo->getNodes().size()
                  << " edges=" << graphInfo->getEdges().size()
                  << " facts=" << countFacts(*graphInfo)
                  << " outputs=" << countOutputNodes(*graphInfo)
                  << " load_ms=" << loadInfoMs
                  << "\n";

        if (!opt.skipLegacyNoSplit) {
            printLegacySummary("legacy-no-split", runLegacyRewrite(opt.jsonPath, SplitMode::None));
        }
        if (!opt.skipLegacySplit) {
            printLegacySummary("legacy-explicit-split", runLegacyRewrite(opt.jsonPath, SplitMode::Naive));
        }
        if (!opt.skipImplicit) {
            double loadMs = 0.0;
            const auto result = runImplicitRewrite(
                    opt.jsonPath, opt.splitMode, opt.runtimeLikeImplicit, &loadMs);
            printImplicitSummary(result, loadMs);
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[implicit-split-json-benchmark] failed: " << ex.what() << "\n";
        return 1;
    }
}
