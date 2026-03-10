#define SOUFFLE_SCBF_GLOBAL_REWRITE_GLOBAL_FORMULA 1
#include "problog/scbf/ScbfGlobalFormulaRewriter.cpp"
#include "problog/scbf/ScbfFormulaRewriter.cpp"

#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

namespace souffle::problog::scbf {
namespace {

using Clock = std::chrono::steady_clock;

struct Options {
    std::string jsonPath;
    bool skipGraphRewrite = false;
    bool skipBundleRewrite = false;
    bool dumpGlobal = false;
    std::string dumpPrefix;
};

struct BundleTotals {
    std::size_t cycles = 0;
    std::size_t targets = 0;
    std::size_t localNodes = 0;
    std::size_t imports = 0;
    std::size_t rules = 0;
    std::size_t literals = 0;
};

class ScopedCoutSilencer {
public:
    ScopedCoutSilencer() : old_(std::cout.rdbuf(sink_.rdbuf())) {}
    ~ScopedCoutSilencer() {
        std::cout.rdbuf(old_);
    }

private:
    std::ostringstream sink_;
    std::streambuf* old_ = nullptr;
};

double elapsedMs(const Clock::time_point& start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}

[[noreturn]] void usage(const char* argv0) {
    std::cerr << "usage: " << argv0 << " --json <derivation.json> [--skip-graph-rewrite]"
              << " [--skip-bundle-rewrite] [--dump-global <prefix>]\n";
    throw std::runtime_error("invalid arguments");
}

Options parseArgs(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--json" && i + 1 < argc) {
            opt.jsonPath = argv[++i];
        } else if (arg == "--skip-graph-rewrite") {
            opt.skipGraphRewrite = true;
        } else if (arg == "--skip-bundle-rewrite") {
            opt.skipBundleRewrite = true;
        } else if (arg == "--dump-global" && i + 1 < argc) {
            opt.dumpGlobal = true;
            opt.dumpPrefix = argv[++i];
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

BundleTotals countBundle(const ScbfStratumFormulaBundle& bundle) {
    BundleTotals out;
    out.cycles = 1;
    out.targets = bundle.targets.size();
    for (const auto& target : bundle.targets) {
        out.localNodes += target.localNodes.size();
        out.imports += target.imports.size();
        for (const auto& rules : target.localNodeRules) {
            out.rules += rules.size();
            for (const auto& rule : rules) {
                out.literals += rule.bodyLiterals.size();
            }
        }
    }
    return out;
}

void accumulate(BundleTotals& total, const BundleTotals& delta) {
    total.cycles += delta.cycles;
    total.targets += delta.targets;
    total.localNodes += delta.localNodes;
    total.imports += delta.imports;
    total.rules += delta.rules;
    total.literals += delta.literals;
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

}  // namespace

}  // namespace souffle::problog::scbf

int main(int argc, char** argv) {
    using namespace souffle::problog;
    using namespace souffle::problog::scbf;

    try {
        const Options opt = parseArgs(argc, argv);
        std::cout << std::fixed << std::setprecision(3);

        const auto loadStart = Clock::now();
        auto graphInfo = std::unique_ptr<IncrementalDerivationGraph>(
                IncrementalDerivationGraph::loadFromJsonInc(opt.jsonPath));
        ensureMeaningfulOutputs(*graphInfo, std::cout);
        const double loadInfoMs = elapsedMs(loadStart);
        std::cout << "[input] json=" << opt.jsonPath
                  << " nodes=" << graphInfo->getNodes().size()
                  << " edges=" << graphInfo->getEdges().size()
                  << " facts=" << countFacts(*graphInfo)
                  << " outputs=" << countOutputNodes(*graphInfo)
                  << " load_ms=" << loadInfoMs
                  << "\n";

        if (!opt.skipGraphRewrite) {
            const auto graphLoadStart = Clock::now();
            auto graph = std::unique_ptr<IncrementalDerivationGraph>(
                    IncrementalDerivationGraph::loadFromJsonInc(opt.jsonPath));
            ensureMeaningfulOutputs(*graph, std::cout);
            const double reloadMs = elapsedMs(graphLoadStart);
            auto view = buildFullView(*graph);
            GraphRewriter rewriter;
            RewriteFeatureFlags flags;
            const auto rewriteStart = Clock::now();
            GraphRewriteStats stats;
            {
                ScopedCoutSilencer silence;
                stats = rewriter.rewriteUntilFixpoint(*graph, view, false, flags);
            }
            const double rewriteMs = elapsedMs(rewriteStart);
            std::cout << "[graph-rewrite] reload_ms=" << reloadMs
                      << " rewrite_ms=" << rewriteMs
                      << " iterations=" << stats.numIterations
                      << " regions=" << stats.numRegionsRewritten
                      << " nodes_removed=" << stats.numNodesRemoved
                      << " edges_removed=" << stats.numEdgesRemoved
                      << " edges_added=" << stats.numEdgesAdded
                      << " rv_before=" << stats.randomVarsBefore
                      << " rv_after=" << stats.randomVarsAfter
                      << " remaining_nodes=" << view.getNodes().size()
                      << " remaining_edges=" << view.getEdges().size()
                      << "\n";
        }

        const auto scbfLoadStart = Clock::now();
        auto scbfGraph = std::unique_ptr<IncrementalDerivationGraph>(
                IncrementalDerivationGraph::loadFromJsonInc(opt.jsonPath));
        ensureMeaningfulOutputs(*scbfGraph, std::cout);
        const double scbfReloadMs = elapsedMs(scbfLoadStart);

        const auto programStart = Clock::now();
        ScbfProgram program = buildScbfProgram(*scbfGraph);
        const double programMs = elapsedMs(programStart);
        std::string err;
        if (!validateScbfProgram(*scbfGraph, program, &err)) {
            throw std::runtime_error("validateScbfProgram failed: " + err);
        }
        std::cout << "[scbf-program] reload_ms=" << scbfReloadMs
                  << " build_ms=" << programMs
                  << " " << summarizeScbfProgram(program)
                  << "\n";

        const auto globalStart = Clock::now();
        ScbfGlobalFormula global = buildScbfGlobalFormula(*scbfGraph, program);
        const double globalMs = elapsedMs(globalStart);
        if (!validateScbfGlobalFormula(global, &err)) {
            throw std::runtime_error("validateScbfGlobalFormula failed: " + err);
        }
        std::cout << "[scbf-global] build_ms=" << globalMs
                  << " " << summarizeScbfGlobalFormula(global)
                  << "\n";
        if (opt.dumpGlobal) {
            if (!dumpScbfGlobalFormulaJson(global, opt.dumpPrefix + ".json")) {
                throw std::runtime_error("failed to dump global JSON");
            }
            if (!dumpScbfGlobalFormulaDot(global, opt.dumpPrefix + ".dot")) {
                throw std::runtime_error("failed to dump global DOT");
            }
            std::cout << "[scbf-global-dump] prefix=" << opt.dumpPrefix << "\n";
        }

        const auto globalRewriteStart = Clock::now();
        ScbfGlobalFormulaRewriteStats globalRewriteStats;
        std::unique_ptr<ScbfGlobalFormula> rewrittenGlobal;
        {
            ScopedCoutSilencer silence;
            rewrittenGlobal = std::make_unique<ScbfGlobalFormula>(
                    rewriteScbfGlobalFormula(global, {}, &globalRewriteStats));
        }
        const double globalRewriteMs = elapsedMs(globalRewriteStart);
        std::cout << "[scbf-global-rewrite] rewrite_ms=" << globalRewriteMs
                  << " " << summarizeScbfGlobalFormula(*rewrittenGlobal)
                  << " " << summarizeScbfGlobalFormulaRewriteStats(globalRewriteStats)
                  << "\n";

        const auto bundleBuildStart = Clock::now();
        BundleTotals bundleTotals;
        std::vector<ScbfStratumFormulaBundle> bundles;
        bundles.reserve(program.topoOrderCycleIds.size());
        for (const auto cycleId : program.topoOrderCycleIds) {
            auto bundle = buildScbfStratumFormulaBundle(*scbfGraph, program, cycleId);
            if (!validateScbfStratumFormulaBundle(*scbfGraph, program, bundle, &err)) {
                throw std::runtime_error("validateScbfStratumFormulaBundle failed: " + err);
            }
            accumulate(bundleTotals, countBundle(bundle));
            bundles.push_back(std::move(bundle));
        }
        const double bundleBuildMs = elapsedMs(bundleBuildStart);
        std::cout << "[scbf-bundles] build_ms=" << bundleBuildMs
                  << " cycles=" << bundleTotals.cycles
                  << " targets=" << bundleTotals.targets
                  << " local_nodes=" << bundleTotals.localNodes
                  << " imports=" << bundleTotals.imports
                  << " rules=" << bundleTotals.rules
                  << " literals=" << bundleTotals.literals
                  << "\n";

        if (!opt.skipBundleRewrite) {
            const auto rewriteStart = Clock::now();
            BundleTotals rewrittenTotals;
            ScbfFormulaRewriteStats rewriteStats;
            {
                ScopedCoutSilencer silence;
                for (const auto& bundle : bundles) {
                    auto rewritten = rewriteScbfStratumFormulaBundle(bundle, {}, &rewriteStats);
                    accumulate(rewrittenTotals, countBundle(rewritten));
                }
            }
            const double rewriteMs = elapsedMs(rewriteStart);
            std::cout << "[scbf-bundle-rewrite] rewrite_ms=" << rewriteMs
                      << " cycles=" << rewrittenTotals.cycles
                      << " targets=" << rewrittenTotals.targets
                      << " local_nodes=" << rewrittenTotals.localNodes
                      << " imports=" << rewrittenTotals.imports
                      << " rules=" << rewrittenTotals.rules
                      << " literals=" << rewrittenTotals.literals
                      << " " << summarizeScbfFormulaRewriteStats(rewriteStats)
                      << "\n";
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[scbf-json-benchmark] failed: " << ex.what() << "\n";
        return 1;
    }
}
