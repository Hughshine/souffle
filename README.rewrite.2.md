# Rewrite Alternative Plan (Historical)

## Source references
- [src/include/souffle/problog/GraphRewriter.h](src/include/souffle/problog/GraphRewriter.h)


## Status
- Historical experimental plan (fact-prefix folding and node merges).
- Current behavior is the conservative rewriter in `README.rewrite.impl.md`.
- Current SISO detection is fast-path only; sections that assume the full detector
  are historical.

## Scope
- Historical rewrite plan; not current behavior.
- Full-mode only; incremental modes skip rewrite.

Bottom line:

The current GraphAnalyzer.h can continue to be used (no major changes needed); keep the SISO detection logic with the existing "node-centered" design.

Below is an updated GraphRewriter.h based on the current version, adding:

Simple SISO (2 nodes, 1 edge) special case: when extra structural conditions hold, SI/SO can be merged into a new fact node (but I am being conservative: keep the current fast path that only updates edge probability, and hang the merge logic off the fact-prefix pass to avoid changing too much at once).

Fact-prefix SISO (cone from input facts back to a node b): do backward BFS inside GraphRewriter, identify subgraphs that are "closed and non-escaping from facts", and run BDD forward compilation only on this subgraph to compute Pr(b), then rewrite b into a new fact.

More reasonable stop condition: if in a round neither SISO nor fact-prefix rewrites any region, consider the fixpoint reached.

Extend statistics fields: count fact-prefix regions, etc., and output in Pipeline logs.

You can directly replace the current GraphRewriter.h with the version below, then apply the Pipeline.h patch shown later.
GraphAnalyzer.h does not need changes for now (your local version is already updated and fully compatible with this GraphRewriter).

1. Updated GraphRewriter.h

Notes

Based entirely on the current GraphRewriter.h in your repo (I used it as the baseline and made minimal incremental changes).

Main additions:

fact-prefix region struct and construction logic

fact-prefix rewrite pass

computeRegionMarginalProbability for region Pr(exit)

Insert fact-prefix pass into rewriteUntilFixpoint, and fix the simple SISO condProb bug.

Simple SISO merge (si/so merge) is currently only done inside fact-prefix rewrite (the "generalized SISO" from facts back to b). For normal entry->exit SISO, it still only synthesizes an edge, to avoid changing too much external behavior.

#pragma once

#include <algorithm>
#include <limits>
#include <map>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <chrono>
#include <queue>
#include <cstdlib>

#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/ForwardCompilation.h"
#include "souffle/problog/GraphAnalyzer.h"
#include "souffle/problog/formula/CuddManager.h"  // WeightedBDDManager, BddNodeRef

namespace souffle::problog {

using SISORegionInfo = ::SISORegionInfo;

/**
 * Statistics collected during SISO-based graph rewriting.
 *
 * The numbers are informational only and do not affect semantics.
 */
struct GraphRewriteStats {
    size_t numIterations = 0;          ///< Number of outer iterations
    size_t numRegionsRewritten = 0;    ///< Total regions rewritten (SISO + fact-prefix)
    size_t numNodesRemoved = 0;        ///< Internal nodes removed from the view
    size_t numEdgesRemoved = 0;        ///< Internal edges removed from the view
    size_t numEdgesAdded = 0;          ///< Synthetic edges added
    size_t totalRandomVars = 0;        ///< Sum of random vars across regions (facts + edges, excl. entry/exit facts)
    size_t maxRandomVars = 0;          ///< Max random vars in a single region
    size_t simpleFactRegions = 0;      ///< Count of simple fact-based SISO regions (2-node, fast path)
    size_t factPrefixRegions = 0;      ///< Count of fact-prefix regions rewritten into facts
};

/**
 * Timing / profiling info for a single region.
 */
struct RegionTiming {
    double mgrInitMs = 0.0;
    double buildMs = 0.0;
    double wmcMs = 0.0;
    double applyMs = 0.0;
    std::vector<double> roundTimingsMs;
    size_t liveNodes = 0;
    double memMb = 0.0;
};

/**
 * A fact-prefix region: backward cone from a node `exit` down to input facts.
 *
 * All boundary entries are facts, internal nodes have no outgoing edges to outside the region
 * (only to other nodes in the cone, including `exit`).
 */
struct FactPrefixRegionInfo {
    NodePtr exit;
    std::vector<NodePtr> nodes;
    std::vector<EdgePtr> edges;
    size_t randomVars = 0;

    bool empty() const {
        return !exit || nodes.empty() || edges.empty();
    }
};

/**
 * Implements SISO-based dependency decomposition.
 *
 * Repeatedly finds SISO regions in the working view, summarizes each region
 * into a single probabilistic edge entry->exit with probability Pr(exit|entry),
 * and updates the view in-place. Additionally, tries to summarize "fact-prefix"
 * cones (from input facts to a node b) directly into a single fact node with
 * probability Pr(b).
 */
class GraphRewriter {
public:
    /**
     * Rewrite all non-trivial SISO regions until a fixpoint on the given view.
     *
     * @param graph   Underlying derivation graph. Only extended (new hyperedges).
     * @param view    Working view mutated in-place (nodes/edges removed or added).
     * @param debug   If true, emit per-region tracing to stdout.
     */
    GraphRewriteStats rewriteUntilFixpoint(
            IncrementalDerivationGraph& graph,
            IncSubgraphView& view,
            bool debug = false) const {
        GraphRewriteStats stats;
        view.invalidateCaches();

        auto managerStart = std::chrono::steady_clock::now();
        WeightedBDDManager bddManager;
        auto managerEnd = std::chrono::steady_clock::now();
        double managerInitMs =
                std::chrono::duration<double, std::milli>(managerEnd - managerStart).count();
        if (debug) {
            std::cout << "[GraphRewriter] CUDD manager init took "
                      << managerInitMs << " ms" << std::endl;
        }

        // Only the first region (SISO or fact-prefix) should attribute manager init time.
        bool firstRegionTiming = true;

        while (true) {
            ++stats.numIterations;
            auto iterStart = std::chrono::steady_clock::now();

            // -----------------------------------------------------------------
            // Pass 0: fact-prefix rewriting (from input facts to shallow nodes)
            // -----------------------------------------------------------------
            size_t factRewrittenThisRound = 0;
            if (isFactPrefixRewriteEnabled()) {
                factRewrittenThisRound = rewriteFactPrefixRegions(
                        graph, view, bddManager, managerInitMs, firstRegionTiming, stats, debug);
            }

            // -----------------------------------------------------------------
            // Pass 1: SISO-based rewrites using GraphAnalyzer
            // -----------------------------------------------------------------
            view.cachedSortedIncomingEdges.clear();
            if (debug) {
                std::ostringstream dotBefore;
                dotBefore << "rewrite_iter" << stats.numIterations << "_before.dot";
                view.dumpDot(dotBefore.str());
            }

            auto regions = GraphAnalyzer::detectAllSISOStrictFromExit(view);

            if (regions.empty()) {
                auto iterMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::steady_clock::now() - iterStart)
                                      .count();
                if (debug) {
                    std::cout << "[GraphRewriter] No SISO regions found in iteration "
                              << stats.numIterations
                              << " (fact-prefix rewrites in this round: "
                              << factRewrittenThisRound << "). Stop in "
                              << iterMs << " ms." << std::endl;
                }
                if (factRewrittenThisRound == 0) {
                    // Nothing changed in this outer iteration.
                    break;
                } else {
                    // Only fact-prefix rewrites happened; run another iteration to see
                    // whether new SISO regions appear.
                    stats.numRegionsRewritten += factRewrittenThisRound;
                    continue;
                }
            }

            if (debug) {
                std::cout << "[GraphRewriter] Iteration " << stats.numIterations
                          << " : detected " << regions.size()
                          << " SISO region(s)." << std::endl;
                std::ostringstream sisoDot;
                sisoDot << "siso_regions_iter" << stats.numIterations << ".dot";
                GraphAnalyzer::dumpAllRegionsAsDot(view, regions, sisoDot.str());
            }

            size_t sisoRewrittenThisRound = 0;

            // Regions produced by GraphAnalyzer are non-overlapping.
            for (const auto& region : regions) {
                if (!region.valid) {
                    continue;
                }

                auto regionStart = std::chrono::steady_clock::now();

                if (!isRegionNonTrivial(region)) {
                    if (debug) {
                        std::cout << "[GraphRewriter]   Skip trivial SISO: "
                                  << regionToString(region) << std::endl;
                    }
                    continue;
                }

                size_t regionRandomVars = countRandomVars(region);

                std::unordered_set<NodePtr> regionNodes(
                        region.internalNodes.begin(), region.internalNodes.end());
                std::unordered_set<EdgePtr> regionEdges(
                        region.internalEdges.begin(), region.internalEdges.end());
                SubgraphView regionView(std::move(regionNodes), std::move(regionEdges));

                RegionTiming timing;
                double effectiveMgrInitMs = firstRegionTiming ? managerInitMs : 0.0;
                double condProb = 0.0;

                bool isSimple = isSimpleFactRegion(region);
                EdgePtr oldSimpleEdge = nullptr;
                if (isSimple && region.internalEdges.size() == 1) {
                    oldSimpleEdge = region.internalEdges.front();
                    if (oldSimpleEdge &&
                        simpleProcessedEdges_.count(oldSimpleEdge->getId()) > 0) {
                        if (debug) {
                            std::cout << "[GraphRewriter]   Skip already processed simple SISO "
                                      << regionToString(region) << std::endl;
                        }
                        continue;
                    }
                }

                if (isSimple) {
                    // Fast path: entry fact -> single edge -> exit.
                    // The edge probability is exactly Pr(exit | entry).
                    EdgePtr onlyEdge = region.internalEdges.front();
                    double pEdge = onlyEdge->getProbability();
                    if (pEdge < 0.0) pEdge = 0.0;
                    if (pEdge > 1.0) pEdge = 1.0;
                    condProb = pEdge;
                    if (debug) {
                        double pEntry = region.entry->getProbability();
                        std::cout << "[GraphRewriter]   Simple fact region "
                                  << regionToString(region)
                                  << " with pEntry=" << pEntry
                                  << ", pEdge=" << pEdge
                                  << " => Pr(exit|entry)=" << condProb
                                  << std::endl;
                    }
                    timing.mgrInitMs = effectiveMgrInitMs;
                    timing.buildMs = 0.0;
                    timing.wmcMs = 0.0;
                    timing.applyMs = 0.0;
                } else {
                    condProb = computeRegionConditionalProbability(
                            bddManager,
                            effectiveMgrInitMs,
                            regionView,
                            region.entry,
                            region.exit,
                            debug,
                            &timing);
                }
                firstRegionTiming = false;

                if (condProb <= 0.0) {
                    if (debug) {
                        std::cout << "[GraphRewriter]   Skip SISO with Pr(exit|entry)=0: "
                                  << regionToString(region) << std::endl;
                    }
                    continue;
                }

                auto applyStart = std::chrono::steady_clock::now();
                EdgePtr newEdge = applyRegionRewrite(
                        graph, view, region, condProb, stats, debug, isSimple);
                auto applyMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::steady_clock::now() - applyStart)
                                       .count();
                timing.applyMs = applyMs;

                if (debug) {
                    auto regionMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                            std::chrono::steady_clock::now() - regionStart)
                                            .count();
                    std::cout << "[GraphRewriter]   Region rewrite time: "
                              << regionMs << " ms for " << regionToString(region)
                              << " (apply=" << applyMs << " ms)"
                              << std::endl;
                    std::cout << "[GraphRewriter]     Steps: mgrInit=" << timing.mgrInitMs
                              << " ms, build=" << timing.buildMs
                              << " ms, WMC=" << timing.wmcMs
                              << " ms, apply=" << timing.applyMs << " ms";
                    if (!timing.roundTimingsMs.empty()) {
                        std::cout << ", rounds(ms)=";
                        for (size_t i = 0; i < timing.roundTimingsMs.size(); ++i) {
                            std::cout << (i == 0 ? "[" : ", ") << timing.roundTimingsMs[i];
                        }
                        std::cout << "]";
                    }
                    std::cout << ", BDD live nodes=" << timing.liveNodes
                              << ", mem=" << timing.memMb << " MB"
                              << ", randomVars=" << regionRandomVars
                              << ", kind=" << (isSimple ? "simple_fact" : "general")
                              << std::endl;
                }
                ++sisoRewrittenThisRound;
                stats.totalRandomVars += regionRandomVars;
                stats.maxRandomVars = std::max(stats.maxRandomVars, regionRandomVars);
                if (isSimple) {
                    ++stats.simpleFactRegions;
                    if (oldSimpleEdge) {
                        simpleProcessedEdges_.insert(oldSimpleEdge->getId());
                    }
                    if (newEdge) {
                        simpleProcessedEdges_.insert(newEdge->getId());
                    }
                }
            }

            auto iterMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now() - iterStart)
                                  .count();
            size_t totalRewrittenThisRound = factRewrittenThisRound + sisoRewrittenThisRound;
            if (totalRewrittenThisRound == 0) {
                if (debug) {
                    std::cout << "[GraphRewriter] No region rewritten in iteration "
                              << stats.numIterations
                              << " (fixpoint reached in " << iterMs << " ms)." << std::endl;
                }
                break;
            }

            stats.numRegionsRewritten += totalRewrittenThisRound;

            if (debug) {
                if (totalRewrittenThisRound > 0) {
                    std::ostringstream dotAfter;
                    dotAfter << "rewrite_iter" << stats.numIterations << "_after.dot";
                    view.dumpDot(dotAfter.str());
                }
                std::cout << "[GraphRewriter]   Rewrote " << totalRewrittenThisRound
                          << " region(s) in iteration " << stats.numIterations
                          << " in " << iterMs << " ms. Current stats: "
                          << "nodesRemoved=" << stats.numNodesRemoved
                          << ", edgesRemoved=" << stats.numEdgesRemoved
                          << ", edgesAdded=" << stats.numEdgesAdded
                          << std::endl;
            }
        }

        return stats;
    }

private:
    // ---------------------------------------------------------------------
    // Configuration helpers
    // ---------------------------------------------------------------------

    static size_t maxEdgesForRegion() {
        const char* env = std::getenv("SOUFFLE_SISO_MAX_EDGES");
        if (!env) return 5000;  // default upper bound
        try {
            return static_cast<size_t>(std::stoul(env));
        } catch (...) {
            return 5000;
        }
    }

    static bool isFactPrefixRewriteEnabled() {
        // Gate for fact-prefix rewriting; can be disabled for experiments.
        const char* env = std::getenv("SOUFFLE_FACT_PREFIX_REWRITE");
        if (!env) return true;  // default on
        std::string v(env);
        return (v == "1" || v == "true" || v == "TRUE" || v == "on" || v == "ON");
    }

    static size_t maxFactPrefixRandomVars() {
        const char* env = std::getenv("SOUFFLE_FACT_PREFIX_MAX_RANDOM_VARS");
        if (!env) return 2;   // conservative default
        try {
            return static_cast<size_t>(std::stoul(env));
        } catch (...) {
            return 2;
        }
    }

    static size_t maxFactPrefixNodes() {
        const char* env = std::getenv("SOUFFLE_FACT_PREFIX_MAX_NODES");
        if (!env) return 64;
        try {
            return static_cast<size_t>(std::stoul(env));
        } catch (...) {
            return 64;
        }
    }

    static size_t maxFactPrefixEdges() {
        const char* env = std::getenv("SOUFFLE_FACT_PREFIX_MAX_EDGES");
        if (!env) return 256;
        try {
            return static_cast<size_t>(std::stoul(env));
        } catch (...) {
            return 256;
        }
    }

    // ---------------------------------------------------------------------
    // Region classification utilities
    // ---------------------------------------------------------------------

    bool isRegionNonTrivial(const SISORegionInfo& region) const {
        // ignore invalid regions
        if (!region.entry || !region.exit) return false;
        if (region.internalEdges.empty()) return false;

        size_t edgeCount = region.internalEdges.size();
        size_t nodeCount = region.internalNodes.size();

        // Always allow 2-node simple fact region (entry fact -> single edge -> exit).
        if (isSimpleFactRegion(region)) {
            return true;
        }

        // For larger regions, enforce a global edge limit.
        size_t maxEdges = maxEdgesForRegion();
        if (edgeCount > maxEdges) {
            return false;
        }

        // At this point, any region with at least one edge is considered non-trivial.
        return true;
    }

    size_t countRandomVars(const SISORegionInfo& region) const {
        size_t randomCount = 0;

        // Count random facts inside the region (excluding entry/exit themselves).
        for (const auto& node : region.internalNodes) {
            if (!node) continue;
            if (node == region.entry || node == region.exit) {
                continue;
            }
            if (!node->isFact) {
                continue;
            }
            double p = node->getProbability();
            if (p > 0.0 && p < 1.0) {
                ++randomCount;
            }
        }

        // Count random edges.
        for (const auto& edge : region.internalEdges) {
            if (!edge) continue;
            double p = edge->getProbability();
            if (p > 0.0 && p < 1.0) {
                ++randomCount;
            }
        }

        return randomCount;
    }

    static bool isSimpleFactRegion(const SISORegionInfo& region) {
        // simple chain: entry fact -> exit (single edge), only two nodes and one edge
        if (region.internalNodes.size() != 2) return false;
        if (region.internalEdges.size() != 1) return false;
        if (!region.entry || !region.exit) return false;
        if (!region.entry->isFact) return false;
        // ignore regions where entry==exit
        if (region.entry == region.exit) return false;
        return true;
    }

    static std::string regionToString(const SISORegionInfo& region) {
        std::ostringstream oss;
        oss << "[entry=";
        if (region.entry) {
            oss << region.entry->toString();
        } else {
            oss << "null";
        }
        oss << ", exit=";
        if (region.exit) {
            oss << region.exit->toString();
        } else {
            oss << "null";
        }
        oss << ", |nodes|=" << region.internalNodes.size()
            << ", |edges|=" << region.internalEdges.size()
            << "]";
        return oss.str();
    }

    // ---------------------------------------------------------------------
    // BDD-based probability computations
    // ---------------------------------------------------------------------

    double computeRegionConditionalProbability(
            WeightedBDDManager& bddManager,
            double managerInitMs,
            SubgraphView& regionView,
            NodePtr entry,
            NodePtr exit,
            bool debug,
            RegionTiming* timingOut = nullptr) const {
        if (!entry || !exit) {
            return 0.0;
        }

        std::map<NodePtr, BddNodeRef> nodeFormulas;
        std::map<EdgePtr, BddNodeRef> edgeFormulas;
        std::unordered_set<NodePtr> seedTrue = { entry };
        std::vector<double> roundTimings;

        auto t0 = std::chrono::steady_clock::now();
        buildFormulasCyclewise(regionView,
                               bddManager,
                               nodeFormulas,
                               edgeFormulas,
                               seedTrue,
                               debug ? &roundTimings : nullptr);
        auto t1 = std::chrono::steady_clock::now();
        double buildMs =
                std::chrono::duration<double, std::milli>(t1 - t0).count();

        auto itExit = nodeFormulas.find(exit);
        if (itExit == nodeFormulas.end()) {
            if (debug) {
                std::cout << "[GraphRewriter]   WARNING: No formula for exit node "
                          << exit->toString() << " in region." << std::endl;
            }
            return 0.0;
        }

        auto itEntry = nodeFormulas.find(entry);
        if (itEntry == nodeFormulas.end()) {
            double pExit = bddManager.computeWeightedModelCount(itExit->second);
            if (debug) {
                std::cout << "[GraphRewriter]   Entry has no local formula; use Pr(exit)="
                          << pExit << " as conditional probability."
                          << " [mgrInit " << managerInitMs
                          << " ms, local build "
                          << std::chrono::duration_cast<std::chrono::milliseconds>(
                                     t1 - t0)
                                     .count()
                          << " ms]" << std::endl;
            }
            return pExit;
        }

        auto t2 = std::chrono::steady_clock::now();
        double pExit = bddManager.computeWeightedModelCount(itExit->second);
        double pEntry = bddManager.computeWeightedModelCount(itEntry->second);
        auto t3 = std::chrono::steady_clock::now();
        double wmcMs =
                std::chrono::duration<double, std::milli>(t3 - t2).count();

        if (timingOut) {
            timingOut->mgrInitMs = managerInitMs;
            timingOut->buildMs = buildMs;
            timingOut->wmcMs = wmcMs;
            timingOut->roundTimingsMs = roundTimings;
            timingOut->liveNodes = Cudd_ReadNodeCount(bddManager.getManager());
            timingOut->memMb =
                    Cudd_ReadMemoryInUse(bddManager.getManager()) /
                    (1024.0 * 1024);
        }
        if (debug) {
            if (!roundTimings.empty()) {
                std::cout << "[GraphRewriter]   Forward compilation rounds (ms):";
                for (size_t i = 0; i < roundTimings.size(); ++i) {
                    std::cout << (i == 0 ? " " : ", ") << roundTimings[i];
                }
                std::cout << std::endl;
            }

            size_t liveNodes = Cudd_ReadNodeCount(bddManager.getManager());
            double memMb =
                    Cudd_ReadMemoryInUse(bddManager.getManager()) /
                    (1024.0 * 1024);

            std::cout << "[GraphRewriter]   Region "
                      << exit->toString() << " <- " << entry->toString()
                      << " : Pr(exit)=" << pExit
                      << ", Pr(entry)=" << pEntry
                      << " [mgrInit " << managerInitMs
                      << " ms, build " << buildMs
                      << " ms, WMC " << wmcMs
                      << " ms]"
                      << " ; BDD live nodes=" << liveNodes
                      << ", mem=" << memMb << " MB";
        }

        if (pEntry <= std::numeric_limits<double>::epsilon()) {
            if (debug) {
                std::cout << " (entryProb≈0, treat as 0)" << std::endl;
            }
            return 0.0;
        }

        double pCond = pExit / pEntry;

        if (pCond < 0.0) pCond = 0.0;
        if (pCond > 1.0) pCond = 1.0;

        if (debug) {
            std::cout << ", Pr(exit|entry)=" << pCond << std::endl;
        }

        return pCond;
    }

    /**
     * Compute Pr(exit) within a fact-prefix region using BDD.
     * Seeds the forward compilation from `exit` itself.
     */
    double computeRegionMarginalProbability(
            WeightedBDDManager& bddManager,
            double managerInitMs,
            SubgraphView& regionView,
            NodePtr exit,
            bool debug,
            RegionTiming* timingOut = nullptr) const {
        if (!exit) {
            return 0.0;
        }

        std::map<NodePtr, BddNodeRef> nodeFormulas;
        std::map<EdgePtr, BddNodeRef> edgeFormulas;
        std::unordered_set<NodePtr> seedTrue = { exit };
        std::vector<double> roundTimings;

        auto t0 = std::chrono::steady_clock::now();
        buildFormulasCyclewise(regionView,
                               bddManager,
                               nodeFormulas,
                               edgeFormulas,
                               seedTrue,
                               debug ? &roundTimings : nullptr);
        auto t1 = std::chrono::steady_clock::now();
        double buildMs =
                std::chrono::duration<double, std::milli>(t1 - t0).count();

        auto itExit = nodeFormulas.find(exit);
        if (itExit == nodeFormulas.end()) {
            if (debug) {
                std::cout << "[GraphRewriter]   WARNING: No formula for exit node "
                          << exit->toString()
                          << " in fact-prefix region." << std::endl;
            }
            return 0.0;
        }

        auto t2 = std::chrono::steady_clock::now();
        double pExit = bddManager.computeWeightedModelCount(itExit->second);
        auto t3 = std::chrono::steady_clock::now();
        double wmcMs =
                std::chrono::duration<double, std::milli>(t3 - t2).count();

        if (timingOut) {
            timingOut->mgrInitMs = managerInitMs;
            timingOut->buildMs = buildMs;
            timingOut->wmcMs = wmcMs;
            timingOut->roundTimingsMs = roundTimings;
            timingOut->liveNodes = Cudd_ReadNodeCount(bddManager.getManager());
            timingOut->memMb =
                    Cudd_ReadMemoryInUse(bddManager.getManager()) /
                    (1024.0 * 1024);
        }

        if (debug) {
            if (!roundTimings.empty()) {
                std::cout << "[GraphRewriter]   Fact-prefix region forward compilation rounds (ms):";
                for (size_t i = 0; i < roundTimings.size(); ++i) {
                    std::cout << (i == 0 ? " " : ", ") << roundTimings[i];
                }
                std::cout << std::endl;
            }
            size_t liveNodes = Cudd_ReadNodeCount(bddManager.getManager());
            double memMb =
                    Cudd_ReadMemoryInUse(bddManager.getManager()) /
                    (1024.0 * 1024);
            std::cout << "[GraphRewriter]   Fact-prefix region Pr(exit)=" << pExit
                      << " [mgrInit " << managerInitMs
                      << " ms, build " << buildMs
                      << " ms, WMC " << wmcMs << " ms]"
                      << " ; BDD live nodes=" << liveNodes
                      << ", mem=" << memMb << " MB" << std::endl;
        }

        if (pExit < 0.0) pExit = 0.0;
        if (pExit > 1.0) pExit = 1.0;
        return pExit;
    }

    // ---------------------------------------------------------------------
    // Generic SISO region rewrite
    // ---------------------------------------------------------------------

    EdgePtr applyRegionRewrite(
            IncrementalDerivationGraph& graph,
            IncSubgraphView& view,
            const SISORegionInfo& region,
            double condProb,
            GraphRewriteStats& stats,
            bool debug,
            bool isSimpleFact = false) const {
        if (!region.entry || !region.exit) {
            return nullptr;
        }

        if (isSimpleFact && region.internalEdges.size() == 1) {
            // 2-node fact-based SISO: entry fact -> single edge -> exit
            // We keep the "update edge" behaviour here to avoid surprising
            // changes to global semantics. More aggressive "merge SI/SO into a
            // single fact" is handled by the fact-prefix pass, where we can
            // guarantee that the entry facts are not used elsewhere.
            EdgePtr oldEdge = region.internalEdges.front();
            if (oldEdge) {
                oldEdge->setProbability(condProb);
            }
            if (debug) {
                std::cout << "[GraphRewriter]   Updated simple fact region edge "
                          << regionToString(region)
                          << " with new Pr(exit|entry)=" << condProb
                          << " (no node/edge removal)." << std::endl;
            }
            return oldEdge;
        }

        // General SISO: synthesize a new hyperedge entry -> exit.
        std::vector<NodePtr> inputs = { region.entry };
        EdgePtr newEdge = graph.createHyperedge(inputs, region.exit);
        if (!newEdge) {
            if (debug) {
                std::cout << "[GraphRewriter]   WARNING: createHyperedge failed for region "
                          << regionToString(region) << std::endl;
            }
            return nullptr;
        }

        newEdge->setProbability(condProb);

        SubgraphView& baseView = static_cast<SubgraphView&>(view);
        auto& nodes = baseView.mutableNodes();
        auto& edges = baseView.mutableEdges();

        edges.insert(newEdge);
        ++stats.numEdgesAdded;

        size_t removedEdges = 0;
        for (const auto& e : region.internalEdges) {
            if (edges.erase(e) > 0) {
                ++removedEdges;
            }
        }

        size_t removedNodes = 0;
        for (const auto& n : region.internalNodes) {
            if (!n) continue;
            if (n == region.entry || n == region.exit) {
                continue;
            }
            if (nodes.erase(n) > 0) {
                ++removedNodes;
            }
        }

        stats.numEdgesRemoved += removedEdges;
        stats.numNodesRemoved += removedNodes;

        view.invalidateCaches();

        if (debug) {
            std::cout << "[GraphRewriter]   Rewrote SISO region "
                      << regionToString(region)
                      << " -> new edge id=" << newEdge->getId()
                      << " with Pr(exit|entry)=" << condProb
                      << " ; removed " << removedNodes << " node(s), "
                      << removedEdges << " edge(s)." << std::endl;
        }
        return newEdge;
    }

    // ---------------------------------------------------------------------
    // Fact-prefix region construction & rewrite
    // ---------------------------------------------------------------------

    /**
     * Given a candidate exit node, try to build its backward cone down to
     * input facts as a closed region.
     *
     * Requirements:
     *  - randomVars within limits
     *  - node / edge counts within limits
     *  - all boundary entries are facts
     *  - internal nodes have no outgoing edges to nodes outside the region
     */
    bool buildFactPrefixRegion(
            const IncSubgraphView& view,
            const NodePtr& exit,
            FactPrefixRegionInfo& outRegion,
            size_t maxRandomVars,
            size_t maxNodes,
            size_t maxEdges,
            bool debug) const {
        outRegion = FactPrefixRegionInfo{};
        if (!exit) return false;
        if (exit->isFact) return false; // already fact; nothing to compress

        std::unordered_set<NodePtr> nodeSet;
        std::unordered_set<EdgePtr> edgeSet;

        std::queue<NodePtr> work;
        work.push(exit);
        nodeSet.insert(exit);

        size_t randomVars = 0;

        auto countRandomNode = [&](const NodePtr& n) {
            if (!n) return;
            if (!n->isFact) return;
            double p = n->getProbability();
            if (p > 0.0 && p < 1.0) {
                ++randomVars;
            }
        };

        auto countRandomEdge = [&](const EdgePtr& e) {
            if (!e) return;
            double p = e->getProbability();
            if (p > 0.0 && p < 1.0) {
                ++randomVars;
            }
        };

        // We count randomness only inside the region (including facts).
        while (!work.empty()) {
            NodePtr n = work.front();
            work.pop();
            if (!n) continue;

            const auto& inEdges = view.getIncomingEdges(n);
            for (const auto& e : inEdges) {
                if (!e) continue;
                if (edgeSet.insert(e).second) {
                    if (edgeSet.size() > maxEdges) {
                        if (debug) {
                            std::cout << "[GraphRewriter]   Fact-prefix: abort (too many edges) at node "
                                      << exit->toString() << std::endl;
                        }
                        return false;
                    }
                    countRandomEdge(e);
                    if (randomVars > maxRandomVars) {
                        if (debug) {
                            std::cout << "[GraphRewriter]   Fact-prefix: abort (too many random vars) at node "
                                      << exit->toString() << std::endl;
                        }
                        return false;
                    }
                }
                for (const auto& src : e->getInputs()) {
                    if (!src) continue;
                    if (nodeSet.insert(src).second) {
                        if (nodeSet.size() > maxNodes) {
                            if (debug) {
                                std::cout << "[GraphRewriter]   Fact-prefix: abort (too many nodes) at node "
                                          << exit->toString() << std::endl;
                            }
                            return false;
                        }
                        if (src->isFact) {
                            countRandomNode(src);
                            if (randomVars > maxRandomVars) {
                                if (debug) {
                                    std::cout << "[GraphRewriter]   Fact-prefix: abort (too many random vars) at node "
                                              << exit->toString() << std::endl;
                                }
                                return false;
                            }
                            // For facts, we stop; no need to go further backwards.
                        } else {
                            work.push(src);
                        }
                    }
                }
            }
        }

        if (edgeSet.empty()) {
            // Nothing to summarize.
            return false;
        }

        // Check "no escape": all internal nodes except exit must not have outgoing edges
        // to nodes outside the region.
        for (const auto& n : nodeSet) {
            if (!n) continue;
            if (n == exit) continue;
            const auto& outEdges = view.getOutgoingEdges(n);
            for (const auto& e : outEdges) {
                if (!e) continue;
                NodePtr out = e->getOutput();
                if (!out) continue;
                if (nodeSet.find(out) == nodeSet.end()) {
                    if (debug) {
                        std::cout << "[GraphRewriter]   Fact-prefix: escape edge "
                                  << n->toString() << " -> "
                                  << out->toString() << " ; skip region for exit "
                                  << exit->toString() << std::endl;
                    }
                    return false;
                }
            }
        }

        // Check boundary entries: any node that does not have an incoming edge
        // from inside the region must be a fact, and must not have any incoming
        // edge from outside the region.
        for (const auto& n : nodeSet) {
            if (!n) continue;
            const auto& inEdges = view.getIncomingEdges(n);
            bool hasInternalInEdge = false;
            for (const auto& e : inEdges) {
                if (!e) continue;
                if (edgeSet.count(e) > 0) {
                    hasInternalInEdge = true;
                } else {
                    // incoming from outside: not a pure fact-prefix cone.
                    if (debug) {
                        std::cout << "[GraphRewriter]   Fact-prefix: node "
                                  << n->toString()
                                  << " has incoming edge from outside region; skip."
                                  << std::endl;
                    }
                    return false;
                }
            }
            if (!hasInternalInEdge) {
                // boundary entry
                if (!n->isFact) {
                    if (debug) {
                        std::cout << "[GraphRewriter]   Fact-prefix: boundary node "
                                  << n->toString()
                                  << " is not a fact; skip." << std::endl;
                    }
                    return false;
                }
            }
        }

        // Build region info
        outRegion.exit = exit;
        outRegion.randomVars = randomVars;
        outRegion.nodes.assign(nodeSet.begin(), nodeSet.end());
        outRegion.edges.assign(edgeSet.begin(), edgeSet.end());

        if (outRegion.randomVars == 0) {
            // Deterministic cone; rewriting to a fact doesn't buy us much, skip.
            return false;
        }

        if (debug) {
            std::cout << "[GraphRewriter]   Fact-prefix region for exit "
                      << exit->toString()
                      << " : |nodes|=" << outRegion.nodes.size()
                      << ", |edges|=" << outRegion.edges.size()
                      << ", randomVars=" << outRegion.randomVars << std::endl;
        }

        return true;
    }

    /**
     * Apply a fact-prefix region rewrite: replace the entire cone with a single fact node `exit`
     * whose probability is `pExit`. All internal nodes/edges (except exit) are removed from the view.
     */
    void applyFactPrefixRewrite(
            IncrementalDerivationGraph& /*graph*/,
            IncSubgraphView& view,
            const FactPrefixRegionInfo& region,
            double pExit,
            GraphRewriteStats& stats,
            bool debug) const {
        if (!region.exit) return;

        SubgraphView& baseView = static_cast<SubgraphView&>(view);
        auto& nodes = baseView.mutableNodes();
        auto& edges = baseView.mutableEdges();

        size_t removedEdges = 0;
        for (const auto& e : region.edges) {
            if (!e) continue;
            if (edges.erase(e) > 0) {
                ++removedEdges;
            }
        }

        size_t removedNodes = 0;
        for (const auto& n : region.nodes) {
            if (!n) continue;
            if (n == region.exit) continue;
            if (nodes.erase(n) > 0) {
                ++removedNodes;
            }
        }

        stats.numEdgesRemoved += removedEdges;
        stats.numNodesRemoved += removedNodes;

        // Turn exit into a fact with probability pExit.
        region.exit->setFact(true);
        region.exit->setProbability(pExit);

        view.invalidateCaches();

        if (debug) {
            std::cout << "[GraphRewriter]   Fact-prefix rewrite: exit "
                      << region.exit->toString()
                      << " becomes fact with Pr=" << pExit
                      << " ; removed " << removedNodes << " node(s), "
                      << removedEdges << " edge(s)." << std::endl;
        }
    }

    /**
     * Pass: for each node in the view, try to build a fact-prefix region and rewrite it.
     * Returns the number of regions rewritten.
     */
    size_t rewriteFactPrefixRegions(
            IncrementalDerivationGraph& graph,
            IncSubgraphView& view,
            WeightedBDDManager& bddManager,
            double managerInitMs,
            bool& firstRegionTiming,
            GraphRewriteStats& stats,
            bool debug) const {
        size_t rewritten = 0;

        size_t maxRand = maxFactPrefixRandomVars();
        size_t maxNodes = maxFactPrefixNodes();
        size_t maxEdges = maxFactPrefixEdges();

        // Snapshot nodes to iterate; view may mutate inside loop.
        std::vector<NodePtr> nodes(view.getNodes().begin(), view.getNodes().end());

        for (const auto& n : nodes) {
            if (!n) continue;
            if (n->isFact) continue;       // already fact
            if (!view.containsNode(n)) continue; // may have been removed

            FactPrefixRegionInfo region;
            if (!buildFactPrefixRegion(view, n, region, maxRand, maxNodes, maxEdges, debug)) {
                continue;
            }

            // Build subgraph view for BDD.
            std::unordered_set<NodePtr> regionNodes(region.nodes.begin(), region.nodes.end());
            std::unordered_set<EdgePtr> regionEdges(region.edges.begin(), region.edges.end());
            SubgraphView regionView(std::move(regionNodes), std::move(regionEdges));

            RegionTiming timing;
            double effectiveMgrInitMs = firstRegionTiming ? managerInitMs : 0.0;
            double pExit = computeRegionMarginalProbability(
                    bddManager,
                    effectiveMgrInitMs,
                    regionView,
                    region.exit,
                    debug,
                    &timing);
            firstRegionTiming = false;

            if (pExit <= 0.0) {
                if (debug) {
                    std::cout << "[GraphRewriter]   Skip fact-prefix region for exit "
                              << n->toString()
                              << " because Pr(exit)<=0 (" << pExit << ")." << std::endl;
                }
                continue;
            }

            applyFactPrefixRewrite(graph, view, region, pExit, stats, debug);
            ++rewritten;
        }

        stats.factPrefixRegions += rewritten;
        return rewritten;
    }

    mutable std::unordered_set<size_t> simpleProcessedEdges_;
};

}  // namespace souffle::problog

2. Minimal changes to Pipeline.h

Two places in Pipeline.h need to be updated:

Add factPrefixRegions to console/log output.

(Optional) If you later add a finer-grained switch in CmdOptions (e.g., --rewrite-fact-prefix), you can use it here to decide whether to enable the fact-prefix pass. The current version is controlled via env var SOUFFLE_FACT_PREFIX_REWRITE, so Pipeline does not need logic changes, just output changes.

1. Console output

Currently (your version):

std::cout << "[pipeline] SISO rewrite took " << rewriteMs << " ms"
          << " (iterations=" << rewriteStats.numIterations
          << ", regions=" << rewriteStats.numRegionsRewritten
          << ", nodesRemoved=" << rewriteStats.numNodesRemoved
          << ", edgesRemoved=" << rewriteStats.numEdgesRemoved
          << ", edgesAdded=" << rewriteStats.numEdgesAdded
          << ", avgRandomVars=" << (rewriteStats.numRegionsRewritten == 0
                ? 0.0
                : static_cast<double>(rewriteStats.totalRandomVars) /
                  static_cast<double>(rewriteStats.numRegionsRewritten))
          << ", maxRandomVars=" << rewriteStats.maxRandomVars
          << ", simpleFactRegions=" << rewriteStats.simpleFactRegions
          << ")" << std::endl;


Suggested change (add fact-prefix count):

std::cout << "[pipeline] SISO rewrite took " << rewriteMs << " ms"
          << " (iterations=" << rewriteStats.numIterations
          << ", regions=" << rewriteStats.numRegionsRewritten
          << ", nodesRemoved=" << rewriteStats.numNodesRemoved
          << ", edgesRemoved=" << rewriteStats.numEdgesRemoved
          << ", edgesAdded=" << rewriteStats.numEdgesAdded
          << ", avgRandomVars=" << (rewriteStats.numRegionsRewritten == 0
                ? 0.0
                : static_cast<double>(rewriteStats.totalRandomVars) /
                  static_cast<double>(rewriteStats.numRegionsRewritten))
          << ", maxRandomVars=" << rewriteStats.maxRandomVars
          << ", simpleFactRegions=" << rewriteStats.simpleFactRegions
          << ", factPrefixRegions=" << rewriteStats.factPrefixRegions
          << ")" << std::endl;

2. rewrite.log output

Currently:

rewriteLog << "duration_ms=" << rewriteMs
           << ",iterations=" << rewriteStats.numIterations
           << ",regions=" << rewriteStats.numRegionsRewritten
           << ",nodesRemoved=" << rewriteStats.numNodesRemoved
           << ",edgesRemoved=" << rewriteStats.numEdgesRemoved
           << ",edgesAdded=" << rewriteStats.numEdgesAdded
           << ",avgRandomVars=" << (rewriteStats.numRegionsRewritten == 0
                 ? 0.0
                 : static_cast<double>(rewriteStats.totalRandomVars) /
                   static_cast<double>(rewriteStats.numRegionsRewritten))
           << ",maxRandomVars=" << rewriteStats.maxRandomVars
           << ",simpleFactRegions=" << rewriteStats.simpleFactRegions
           << std::endl;


Change to:

rewriteLog << "duration_ms=" << rewriteMs
           << ",iterations=" << rewriteStats.numIterations
           << ",regions=" << rewriteStats.numRegionsRewritten
           << ",nodesRemoved=" << rewriteStats.numNodesRemoved
           << ",edgesRemoved=" << rewriteStats.numEdgesRemoved
           << ",edgesAdded=" << rewriteStats.numEdgesAdded
           << ",avgRandomVars=" << (rewriteStats.numRegionsRewritten == 0
                 ? 0.0
                 : static_cast<double>(rewriteStats.totalRandomVars) /
                   static_cast<double>(rewriteStats.numRegionsRewritten))
           << ",maxRandomVars=" << rewriteStats.maxRandomVars
           << ",simpleFactRegions=" << rewriteStats.simpleFactRegions
           << ",factPrefixRegions=" << rewriteStats.factPrefixRegions
           << std::endl;

3. Command-line parameter suggestions (guidance for Codex)

In the current implementation:

SISO rewrite is controlled by opt.isRewriteEnabled().

fact-prefix pass is controlled by environment variable SOUFFLE_FACT_PREFIX_REWRITE (default on).

If you want command-line instead of env, add to CmdOptions:

bool isFactPrefixRewriteEnabled() const;


And a parameter (name up to you), for example:

--rewrite-fact-prefix[=on|off] or

Extend existing --rewrite to support mode: none | siso | siso+fact-prefix.

Pipeline.h could be:

if (opt.isRewriteEnabled()) {
    GraphRewriter rewriter;
    if (!opt.isFactPrefixRewriteEnabled()) {
        // Explicitly disable fact-prefix; add a static switch in GraphRewriter or use env.
        std::setenv("SOUFFLE_FACT_PREFIX_REWRITE", "0", /*overwrite*/1);
    }
    bool rewriteDebug = std::getenv("SOUFFLE_REWRITE_DEBUG") != nullptr;
    ...
}


When implementing, Codex should:
Modify CmdOptions parse logic and help docs.

Ensure the default behavior stays the same (i.e., when --rewrite is enabled, both SISO + fact-prefix are enabled).

4. Test/experiment suggestions (Codex checklist)
1. Simple single SISO (2 nodes, 1 edge)

Construct programmatically:

fact a, Pr(a)=0.3

rule b :- a, edge probability p=0.5

Without rewrite (or rewrite off):

Whole-graph forward compilation yields Pr(b) = 0.15.

With rewrite (including simple SISO fast path):

SISO detection should find the a -> b region (2 nodes, 1 edge).

Current implementation only updates edge probability to pEdge (0.5), no semantic change;

Final BDD pipeline should still give Pr(b)=0.15.

Rewrite log should show simpleFactRegions=1, factPrefixRegions=0.

If you later want to actually merge a/b into one fact, this test becomes checking that a is removed, b becomes a fact, Pr(b)=0.15. Then you only need to change the applyRegionRewrite simple branch to "merge nodes".

2. fact-prefix scenario: a -> b / a' -> b

Construct:

a, a' are input facts, Pr(a)=0.2, Pr(a')=0.4

Rules: b :- a (p=1), b :- a' (p=1)

No rewrite:

Whole-graph forward compilation yields:
Pr(b) = 1 - (1 - 0.2) * (1 - 0.4) = 0.52.

With rewrite:

fact-prefix pass does backward cone for b: nodes = {a,a',b}, edges = {a->b, a'->b}.

Satisfies:

boundary only facts a, a';

no escape edges;

randomVars count=2 (two random facts), within default threshold 2;

BDD pass runs only on this subgraph, compute Pr(b)=0.52.

b rewritten to fact, Pr(b)=0.52, a, a' and the two edges removed from the view.

Downstream queries only see b, whole-graph forward compilation remains consistent.

Log should show factPrefixRegions=1, simpleFactRegions=0.

3. Complex structure / scalability

Use existing benchmarks:

With --rewrite on/off:

Confirm final query probabilities are unchanged;

Compare BDD build and WMC times, observe speedup from fact-prefix+SISO;

Check rewrite.log metrics like regions, nodesRemoved, edgesRemoved for reasonableness.

4. Fallback/robustness

Deliberately construct cases that do not meet fact-prefix conditions:

a fact connects to both b and c;

the cone contains a non-fact node with in-degree 0;

cone internal nodes have escape edges to outside;

By design, these are rejected in buildFactPrefixRegion (debug mode has detailed reasons), and safely fall back to the original BDD pipeline without affecting correctness.

## Related commits
- `812ea4081` — docs(repo): refine README narratives
- `4dd403de4` — Translate Chinese comments and docs to English
- `4c4bd26b2` — docs(readme): restructure online incremental docs
