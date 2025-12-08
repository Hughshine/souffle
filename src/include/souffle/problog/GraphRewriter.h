#pragma once

#include <algorithm>
#include <chrono>
#include <limits>
#include <map>
#include <sstream>
#include <unordered_set>
#include <vector>
#include <iostream>

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
    size_t numRegionsRewritten = 0;    ///< Total SISO regions rewritten
    size_t numNodesRemoved = 0;        ///< Internal nodes removed from the view
    size_t numEdgesRemoved = 0;        ///< Internal edges removed from the view
    size_t numEdgesAdded = 0;          ///< Synthetic edges added
    size_t totalRandomVars = 0;        ///< Sum of random vars across regions (facts + edges, excl. entry/exit facts)
    size_t maxRandomVars = 0;          ///< Max random vars in a single region
    size_t simpleFactRegions = 0;      ///< Count of simple fact-based regions rewritten
    size_t randomVarsBefore = 0;       ///< Random vars in the view before rewrite
    size_t randomVarsAfter = 0;        ///< Random vars in the view after rewrite
};

/**
 * Implements SISO-based dependency decomposition.
 *
 * Repeatedly finds SISO regions in the working view, summarizes each region
 * into a single probabilistic edge entry->exit with probability Pr(exit|entry),
 * and updates the view in-place.
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
    GraphRewriteStats rewriteUntilFixpoint(IncrementalDerivationGraph& graph,
                                           IncSubgraphView& view,
                                           bool debug = false) const {
        GraphRewriteStats stats;
        view.invalidateCaches();
        std::unique_ptr<WeightedBDDManager> bddManager;
        double managerInitMs = 0.0;
        bool managerInitialized = false;

        auto toMs = [](auto duration) {
            return std::chrono::duration<double, std::milli>(duration).count();
        };

        stats.randomVarsBefore = countRandomVarsInView(view);
        stats.randomVarsAfter = stats.randomVarsBefore;

        // Only the first region should attribute manager init time; subsequent regions reuse the same manager.
        bool firstRegionTiming = true;

        auto rewriteStart = std::chrono::steady_clock::now();

        while (true) {
            auto iterStart = std::chrono::steady_clock::now();
            ++stats.numIterations;
            auto countBeforeStart = std::chrono::steady_clock::now();
            size_t iterRandomVarsBefore = countRandomVarsInView(view);
            double countBeforeMs = toMs(std::chrono::steady_clock::now() - countBeforeStart);

            view.cachedSortedIncomingEdges.clear();
            double dumpBeforeDotMs = 0.0;
            if (debug) {
                std::ostringstream dotBefore;
                dotBefore << "rewrite_iter" << stats.numIterations << "_before.dot";
                auto dotBeforeStart = std::chrono::steady_clock::now();
                view.dumpDot(dotBefore.str());
                dumpBeforeDotMs = toMs(std::chrono::steady_clock::now() - dotBeforeStart);
                std::cout << "[GraphRewriter] dumpDot(before) took "
                          << dumpBeforeDotMs << " ms" << std::endl;
            }
            auto detectStart = std::chrono::steady_clock::now();
            auto regions = GraphAnalyzer::detectAllSISOStrictFromExit(view);
            double detectMs = toMs(std::chrono::steady_clock::now() - detectStart);
            std::cout << "[GraphRewriter] SISO detection took "
                      << detectMs << " ms" << std::endl;
            size_t detectedSingle = 0, detectedLinear = 0, detectedParallel = 0, detectedAllFacts = 0, detectedGeneral = 0;
            for (const auto& r : regions) {
                switch (r.kind) {
                    case SISORegionKind::SingleHyperedge: ++detectedSingle; break;
                    case SISORegionKind::LinearTwoEdge: ++detectedLinear; break;
                    case SISORegionKind::ParallelEdge: ++detectedParallel; break;
                    case SISORegionKind::AllFactsToSO: ++detectedAllFacts; break;
                    case SISORegionKind::General: ++detectedGeneral; break;
                    default: break;
                }
            }

            if (regions.empty()) {
                stats.randomVarsAfter = iterRandomVarsBefore;
                long long iterDelta = 0;
                double iterRatio = iterRandomVarsBefore == 0 ? 0.0 : 1.0;
                std::cout << "[GraphRewriter]   Iteration " << stats.numIterations
                          << " random vars: before=" << iterRandomVarsBefore
                          << ", after=" << stats.randomVarsAfter
                          << ", delta=" << iterDelta
                          << ", ratio=" << iterRatio << std::endl;
                if (debug) {
                    std::cout << "[GraphRewriter] No SISO regions found, stop at iteration "
                              << stats.numIterations << std::endl;
                }
                break;
            }

            double dumpRegionsMs = 0.0;
            if (debug) {
                std::cout << "[GraphRewriter] Iteration " << stats.numIterations
                          << " : detected " << regions.size()
                          << " SISO region(s)." << std::endl;
                std::ostringstream sisoDot;
                sisoDot << "siso_regions_iter" << stats.numIterations << ".dot";
                auto dotStart = std::chrono::steady_clock::now();
                GraphAnalyzer::dumpAllRegionsAsDot(view, regions, sisoDot.str());
                dumpRegionsMs = toMs(std::chrono::steady_clock::now() - dotStart);
                std::cout << "[GraphRewriter] dumpAllRegionsAsDot took "
                          << dumpRegionsMs << " ms" << std::endl;
            }

            size_t rewrittenThisRound = 0;
            auto loopStart = std::chrono::steady_clock::now();
            double loopRewrittenMs = 0.0;
            double loopCondMs = 0.0;

            // Regions produced by GraphAnalyzer are non-overlapping.
            for (const auto& region : regions) {
                if (!region.valid) {
                    continue;
                }

                // Fast-path by SISO kind.
                switch (region.kind) {
                    case SISORegionKind::AllFactsToSO: {
                        if (region.internalEdges.size() != 1) continue;
                        EdgePtr edge = region.internalEdges.front();
                        if (!edge) continue;
                        NodePtr exit = region.exit;
                        if (!exit) continue;
                        auto inputs = view.getInputs(edge);
                        auto negs   = view.getBodyNegations(edge);
                        double p = edge->getProbability();
                        if (p < 0.0) p = 0.0;
                        if (p > 1.0) p = 1.0;
                        size_t regionRandomVars = 0;
                        if (p > 0.0 && p < 1.0) ++regionRandomVars;
                        for (size_t i = 0; i < inputs.size(); ++i) {
                            NodePtr n = inputs[i];
                            if (!n) continue;
                            bool isNegated = i < negs.size() ? negs[i] : false;
                            double np = n->getProbability();
                            if (np < 0.0) np = 0.0;
                            if (np > 1.0) np = 1.0;
                            p *= isNegated ? (1.0 - np) : np;
                            if (np > 0.0 && np < 1.0) ++regionRandomVars;
                        }
                        exit->isFact = true;
                        exit->setProbability(p);

                        auto& edges = view.mutableEdges();
                        auto& nodes = view.mutableNodes();
                        size_t removedEdges = edges.erase(edge);
                        view.invalidateCaches();  // ensure degree queries reflect removal
                        size_t removedNodes = 0;
                        for (auto n : inputs) {
                            if (!n) continue;
                            // Drop isolated fact inputs to avoid keeping pruned nodes alive.
                            if (view.getIncomingEdges(n).empty() && view.getOutgoingEdges(n).empty()) {
                                if (nodes.erase(n) > 0) {
                                    ++removedNodes;
                                }
                            }
                        }
                        stats.numEdgesRemoved += removedEdges;
                        stats.numNodesRemoved += removedNodes;
                        stats.totalRandomVars += regionRandomVars;
                        stats.maxRandomVars = std::max(stats.maxRandomVars, regionRandomVars);
                        view.invalidateCaches();

                        if (debug) {
                            // Fast-path all-facts debug logging elided to reduce overhead.
                        }
                        ++rewrittenThisRound;
                        ++stats.numRegionsRewritten;
                        continue;
                    }
                    case SISORegionKind::SingleHyperedge: {
                        if (region.internalEdges.size() != 1) continue;
                        EdgePtr edge = region.internalEdges.front();
                        if (!edge) continue;
                        if (!region.entry || !region.exit) continue;
                        auto inputs = view.getInputs(edge);
                        auto negs   = view.getBodyNegations(edge);
                        if (inputs.empty()) continue;
                        double p = edge->getProbability();
                        if (p < 0.0) p = 0.0;
                        if (p > 1.0) p = 1.0;
                        size_t regionRandomVars = 0;
                        if (p > 0.0 && p < 1.0) ++regionRandomVars;
                        for (size_t i = 0; i < inputs.size(); ++i) {
                            NodePtr n = inputs[i];
                            if (!n) continue;
                            bool isNegated = i < negs.size() ? negs[i] : false;
                            if (!n->isFact) continue;
                            double np = n->getProbability();
                            if (np < 0.0) np = 0.0;
                            if (np > 1.0) np = 1.0;
                            p *= isNegated ? (1.0 - np) : np;
                            if (np > 0.0 && np < 1.0) ++regionRandomVars;
                        }
                        std::vector<NodePtr> siInput = {region.entry};
                        // Preserve the negation flag (if any) on the entry input.
                        bool entryNeg = false;
                        for (size_t i = 0; i < inputs.size(); ++i) {
                            if (inputs[i] == region.entry) {
                                entryNeg = i < negs.size() ? negs[i] : false;
                                break;
                            }
                        }
                        std::vector<bool> newNegs = {entryNeg};
                        EdgePtr newEdge = graph.createHyperedge(siInput, region.exit, nullptr, newNegs);
                        if (!newEdge) {
                            continue;
                        }
                        newEdge->setProbability(p);

                        auto& edges = view.mutableEdges();
                        auto& nodes = view.mutableNodes();
                        size_t removedEdges = edges.erase(edge);
                        edges.insert(newEdge);
                        view.invalidateCaches();  // update degree queries after edge replacement
                        size_t removedNodes = 0;
                        for (auto n : inputs) {
                            if (!n) continue;
                            if (view.getIncomingEdges(n).empty() && view.getOutgoingEdges(n).empty()) {
                                if (nodes.erase(n) > 0) {
                                    ++removedNodes;
                                }
                            }
                        }
                        stats.totalRandomVars += regionRandomVars;
                        stats.maxRandomVars = std::max(stats.maxRandomVars, regionRandomVars);
                        stats.numEdgesRemoved += removedEdges;
                        stats.numEdgesAdded += 1;
                        stats.numNodesRemoved += removedNodes;
                        view.invalidateCaches();
                        if (debug) {
                            // Fast-path single-hyperedge debug logging elided to reduce overhead.
                        }
                        ++rewrittenThisRound;
                        ++stats.numRegionsRewritten;
                        continue;
                    }
                    case SISORegionKind::LinearTwoEdge: {
                        if (region.internalEdges.size() != 2) continue;
                        EdgePtr e1 = region.internalEdges[0];
                        EdgePtr e2 = region.internalEdges[1];
                        if (!e1 || !e2) continue;
                        NodePtr mid = nullptr;
                        EdgePtr intoMid = nullptr;
                        EdgePtr outMid = nullptr;
                        for (auto e : region.internalEdges) {
                            if (!e) continue;
                            NodePtr out = e->getOutput();
                            if (out == region.entry || out == region.exit) {
                                continue;
                            }
                            mid = out;
                            intoMid = e;
                            break;
                        }
                        if (!mid) continue;
                        for (auto e : region.internalEdges) {
                            if (!e || e == intoMid) continue;
                            auto inputs = view.getInputs(e);
                            if (inputs.size() == 1 && inputs[0] == mid) {
                                outMid = e;
                                break;
                            }
                        }
                        if (!intoMid || !outMid) continue;
                        auto negInto = view.getBodyNegations(intoMid);
                        if (negInto.size() > 1) continue;  // expect single-input edge
                        bool entryNeg = (!negInto.empty() && negInto[0]);
                        auto negOut = view.getBodyNegations(outMid);
                        if (negOut.size() > 1) continue;   // expect single-input edge
                        if (!negOut.empty() && negOut[0]) continue;  // do not fast-path if mid->exit is negated
                        double p1 = intoMid->getProbability();
                        double p2 = outMid->getProbability();
                        if (p1 < 0.0) p1 = 0.0;
                        if (p1 > 1.0) p1 = 1.0;
                        if (p2 < 0.0) p2 = 0.0;
                        if (p2 > 1.0) p2 = 1.0;
                        double p = p1 * p2;
                        size_t regionRandomVars = 0;
                        if (p1 > 0.0 && p1 < 1.0) ++regionRandomVars;
                        if (p2 > 0.0 && p2 < 1.0) ++regionRandomVars;

                        std::vector<NodePtr> inputsNew = {region.entry};
                        std::vector<bool> negsNew = {entryNeg};
                        EdgePtr newEdge = graph.createHyperedge(inputsNew, region.exit, nullptr, negsNew);
                        if (!newEdge) continue;
                        newEdge->setProbability(p);

                        auto& edges = view.mutableEdges();
                        auto& nodes = view.mutableNodes();
                        size_t removedEdges = 0;
                        removedEdges += edges.erase(intoMid);
                        removedEdges += edges.erase(outMid);
                        edges.insert(newEdge);
                        size_t removedNodes = 0;
                        if (nodes.erase(mid) > 0) {
                            ++removedNodes;
                        }
                        stats.numEdgesRemoved += removedEdges;
                        stats.numEdgesAdded += 1;
                        stats.numNodesRemoved += removedNodes;
                        stats.totalRandomVars += regionRandomVars;
                        stats.maxRandomVars = std::max(stats.maxRandomVars, regionRandomVars);
                        view.invalidateCaches();

                        if (debug) {
                            // Fast-path linear-two-edge debug logging elided to reduce overhead.
                        }
                        ++rewrittenThisRound;
                        ++stats.numRegionsRewritten;
                        continue;
                    }
                    case SISORegionKind::ParallelEdge:
                        if (debug) {
                            // Fast-path placeholder logging elided to reduce overhead.
                        }
                        continue;  // skip default handling for now
                    default:
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

                std::unordered_set<NodePtr> regionNodes(region.internalNodes.begin(),
                                                        region.internalNodes.end());
                std::unordered_set<EdgePtr> regionEdges(region.internalEdges.begin(),
                                                        region.internalEdges.end());
                SubgraphView regionView(std::move(regionNodes), std::move(regionEdges));

                RegionTiming timing;
                double condProb = 0.0;
                auto condStart = std::chrono::steady_clock::now();

                bool isSimple = isSimpleFactRegion(region);
                EdgePtr oldSimpleEdge = nullptr;
                if (isSimple && region.internalEdges.size() == 1) {
                    oldSimpleEdge = region.internalEdges.front();
                    if (oldSimpleEdge && simpleProcessedEdges_.count(oldSimpleEdge->getId()) > 0) {
                        if (debug) {
                            std::cout << "[GraphRewriter]   Skip already processed simple SISO "
                                      << regionToString(region) << std::endl;
                        }
                        continue;
                    }
                }
                if (isSimple) {
                    // Fast path: entry fact -> single edge -> exit
                    EdgePtr onlyEdge = region.internalEdges.front();
                    double pEntry = region.entry->getProbability();
                    double pEdge = onlyEdge->getProbability();
                    if (pEdge < 0.0) pEdge = 0.0;
                    if (pEdge > 1.0) pEdge = 1.0;
                    condProb = pEntry * pEdge;
                    if (debug) {
                        std::cout << "[GraphRewriter]   Simple fact region "
                                  << regionToString(region)
                                  << " with pEntry=" << pEntry
                                  << ", pEdge=" << pEdge
                                  << " => newPr=" << condProb << std::endl;
                    }
                    timing.mgrInitMs = 0.0;
                    timing.buildMs = 0.0;
                    timing.wmcMs = 0.0;
                    timing.applyMs = 0.0;
                } else {
                    // Lazy init BDD manager when first needed.
                    if (!managerInitialized) {
                        auto managerStart = std::chrono::steady_clock::now();
                        bddManager = std::make_unique<WeightedBDDManager>();
                        auto managerEnd = std::chrono::steady_clock::now();
                        managerInitMs = std::chrono::duration<double, std::milli>(
                                                managerEnd - managerStart)
                                                .count();
                        managerInitialized = true;
                        if (debug) {
                            std::cout << "[GraphRewriter] CUDD manager init took "
                                      << managerInitMs << " ms" << std::endl;
                        }
                    }
                    double effectiveMgrInitMs = managerInitialized ? managerInitMs : 0.0;
                    condProb = computeRegionConditionalProbability(
                        *bddManager, effectiveMgrInitMs, regionView, region.entry, region.exit, debug, &timing);
                }
                double condMs = toMs(std::chrono::steady_clock::now() - condStart);
                loopCondMs += condMs;
                firstRegionTiming = false;

                if (condProb <= 0.0) {
                    if (debug) {
                        std::cout << "[GraphRewriter]   Skip SISO with Pr(exit|entry)=0: "
                                  << regionToString(region) << std::endl;
                    }
                    continue;
                }

                auto applyStart = std::chrono::steady_clock::now();
                EdgePtr newEdge = applyRegionRewrite(graph, view, region, condProb, stats, debug, isSimple);
                double applyMs = toMs(std::chrono::steady_clock::now() - applyStart);
                timing.applyMs = applyMs;
                loopRewrittenMs += applyMs;

                if (debug) {
                    auto regionMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - regionStart).count();
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
                ++rewrittenThisRound;
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

            double loopMs = toMs(std::chrono::steady_clock::now() - loopStart);
            double loopSkipMs = loopMs - loopRewrittenMs;
            if (loopSkipMs < 0) loopSkipMs = 0.0;

            // After each SISO pass, compact edges by absorbing pure fact inputs into edge probability.
            auto compactStart = std::chrono::steady_clock::now();
            size_t compactedEdges = 0, compactRemovedEdges = 0, compactAddedEdges = 0, compactRemovedNodes = 0;
            auto edgeListStart = std::chrono::steady_clock::now();
            auto edgeList = view.getEdges();
            double edgeListMs = toMs(std::chrono::steady_clock::now() - edgeListStart);
            for (auto edge : edgeList) {
                if (!edge) continue;
                auto inputs = view.getInputs(edge);
                if (inputs.empty()) continue;
                std::vector<NodePtr> keepInputs;
                std::vector<NodePtr> factInputs;
                double p = edge->getProbability();
                if (p < 0.0) p = 0.0;
                if (p > 1.0) p = 1.0;
                bool changed = false;
                for (auto n : inputs) {
                    if (!n) continue;
                    if (n->isFact && !n->hasEvidence() && !n->needOutput) {
                        double np = n->getProbability();
                        if (np < 0.0) np = 0.0;
                        if (np > 1.0) np = 1.0;
                        p *= np;
                        factInputs.push_back(n);
                        changed = true;
                    } else {
                        keepInputs.push_back(n);
                    }
                }
                // If nothing to absorb or no non-fact inputs remain, skip.
                if (!changed || keepInputs.empty()) continue;

                EdgePtr newEdge = graph.createHyperedge(keepInputs, edge->getOutput());
                if (!newEdge) continue;
                newEdge->setProbability(p);

                auto& edges = view.mutableEdges();
                auto& nodes = view.mutableNodes();
                if (edges.erase(edge) > 0) {
                    ++compactRemovedEdges;
                }
                edges.insert(newEdge);
                ++compactAddedEdges;

                // Remove fact inputs that became isolated.
                for (auto n : factInputs) {
                    if (!n) continue;
                    if (view.getIncomingEdges(n).empty() && view.getOutgoingEdges(n).empty()) {
                        if (nodes.erase(n) > 0) {
                            ++compactRemovedNodes;
                        }
                    }
                }

                ++compactedEdges;
            }
            double compactMs = toMs(std::chrono::steady_clock::now() - compactStart);

            if (compactedEdges > 0) {
                stats.numEdgesRemoved += compactRemovedEdges;
                stats.numEdgesAdded += compactAddedEdges;
                stats.numNodesRemoved += compactRemovedNodes;
                view.invalidateCaches();
                if (debug) {
                    std::cout << "[GraphRewriter]   Edge compaction: compacted=" << compactedEdges
                              << " removedEdges=" << compactRemovedEdges
                              << " addedEdges=" << compactAddedEdges
                              << " removedNodes=" << compactRemovedNodes
                              << " time=" << compactMs << " ms"
                              << std::endl;
                }
            }

            auto countAfterStart = std::chrono::steady_clock::now();
            size_t iterRandomVarsAfter = countRandomVarsInView(view);
            double countAfterMs = toMs(std::chrono::steady_clock::now() - countAfterStart);

            auto preLogStart = std::chrono::steady_clock::now();

            stats.randomVarsAfter = iterRandomVarsAfter;
            auto iterDelta =
                    static_cast<long long>(iterRandomVarsBefore) - static_cast<long long>(iterRandomVarsAfter);
            double iterRatio = iterRandomVarsBefore == 0
                                       ? 0.0
                                       : static_cast<double>(iterRandomVarsAfter) /
                                                 static_cast<double>(iterRandomVarsBefore);

            double iterMs = toMs(std::chrono::steady_clock::now() - iterStart);

            double dumpAfterDotMs = 0.0;
            double preLogPrepMs = toMs(std::chrono::steady_clock::now() - preLogStart);
            auto logStart = std::chrono::steady_clock::now();

            std::cout << "[GraphRewriter] Iteration " << stats.numIterations
                      << " detected(single=" << detectedSingle
                      << ", linear=" << detectedLinear
                      << ", parallel=" << detectedParallel
                      << ", all-facts=" << detectedAllFacts
                      << ", general=" << detectedGeneral
                      << "), rewritten(total=" << rewrittenThisRound
                      << ") in " << iterMs << " ms" << std::endl;
            std::cout << "[GraphRewriter]   timings(ms): total=" << iterMs
                      << " countBefore=" << countBeforeMs
                      << " detect=" << detectMs
                      << " loop=" << loopMs
                      << " loopCond=" << loopCondMs
                      << " loopRewritten=" << loopRewrittenMs
                      << " loopSkip=" << loopSkipMs
                      << " edgeList=" << edgeListMs
                      << " compact=" << compactMs
                      << " countAfter=" << countAfterMs
                      << " preLog=" << preLogPrepMs
                      << " dumpRegions=" << dumpRegionsMs;
            double logMs = toMs(std::chrono::steady_clock::now() - logStart);
            double remainderMs = iterMs - countBeforeMs
                    - detectMs
                    - loopMs
                    - dumpRegionsMs
                    - dumpBeforeDotMs
                    - dumpAfterDotMs
                    - edgeListMs
                    - compactMs
                    - countAfterMs
                    - preLogPrepMs
                    - logMs;
            if (remainderMs < 0) remainderMs = 0.0;
            std::cout << " log=" << logMs
                      << " other=" << remainderMs
                      << " (apply includes dot if debug)" << std::endl;
            std::cout << "[GraphRewriter]   random vars: before=" << iterRandomVarsBefore
                      << ", after=" << iterRandomVarsAfter
                      << ", delta=" << iterDelta
                      << ", ratio=" << iterRatio << std::endl;

            if (rewrittenThisRound == 0) {
                if (debug) {
                    std::cout << "[GraphRewriter] No region rewritten in iteration "
                              << stats.numIterations << " (fixpoint reached in "
                              << iterMs << " ms)." << std::endl;
                }
                break;
            }

            stats.numRegionsRewritten += rewrittenThisRound;

            if (debug) {
                if (rewrittenThisRound > 0) {
                    std::ostringstream dotAfter;
                    dotAfter << "rewrite_iter" << stats.numIterations << "_after.dot";
                    auto dotAfterStart = std::chrono::steady_clock::now();
                    view.dumpDot(dotAfter.str());
                    dumpAfterDotMs = toMs(std::chrono::steady_clock::now() - dotAfterStart);
                    std::cout << "[GraphRewriter]   dumpDot(after) took "
                              << dumpAfterDotMs << " ms" << std::endl;
                }
                std::cout << "[GraphRewriter]   Rewrote " << rewrittenThisRound
                          << " region(s) in iteration " << stats.numIterations
                          << " in " << iterMs << " ms. Current stats: "
                          << "nodesRemoved=" << stats.numNodesRemoved
                          << ", edgesRemoved=" << stats.numEdgesRemoved
                          << ", edgesAdded=" << stats.numEdgesAdded
                          << std::endl;
            }
        }

        if (stats.numRegionsRewritten > 0) {
            double avgRandomVars = static_cast<double>(stats.totalRandomVars) /
                    static_cast<double>(stats.numRegionsRewritten);
            std::cout << "[GraphRewriter] Avg random vars per region: "
                      << avgRandomVars << " (max=" << stats.maxRandomVars << ")"
                      << std::endl;
        }

        auto rewriteTotalMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - rewriteStart)
                                      .count();
        std::cout << "[GraphRewriter] Rewrite total: " << rewriteTotalMs
                  << " ms over " << stats.numIterations << " iterations"
                  << " (randomVars " << stats.randomVarsBefore << " -> " << stats.randomVarsAfter << ")"
                  << std::endl;

        return stats;
    }

private:
    struct RegionTiming {
        double mgrInitMs = 0.0;
        double buildMs = 0.0;
        double wmcMs = 0.0;
        double applyMs = 0.0;
        std::vector<double> roundTimingsMs;
        size_t liveNodes = 0;
        double memMb = 0.0;
    };

    bool isRegionNonTrivial(const SISORegionInfo& region) const {
        constexpr size_t kDefaultMaxEdges = 5;
        size_t maxEdges = kDefaultMaxEdges;
        if (const char* env = std::getenv("SOUFFLE_SISO_MAX_EDGES")) {
            try {
                maxEdges = std::stoul(env);
            } catch (...) {
                maxEdges = kDefaultMaxEdges;
            }
        }

        size_t edgeCount = region.internalEdges.size();
        size_t nodeCount = region.internalNodes.size();
        if (isSimpleFactRegion(region)) {
            return true;
        }
        if (edgeCount == 0 || nodeCount <= 2) {
            return false;
        }
        if (edgeCount > maxEdges) {
            return false;
        }
        return true;
    }

    size_t countRandomVars(const SISORegionInfo& region) const {
        size_t randomCount = 0;

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

        for (const auto& edge : region.internalEdges) {
            if (!edge) continue;
            double p = edge->getProbability();
            if (p > 0.0 && p < 1.0) {
                ++randomCount;
            }
        }

        return randomCount;
    }

    size_t countRandomVarsInView(const DerivationGraphViewInterface& view) const {
        size_t randomCount = 0;

        for (const auto& node : view.getNodes()) {
            if (!node) continue;
            if (!node->isFact) {
                continue;
            }
            double p = node->getProbability();
            if (p > 0.0 && p < 1.0) {
                ++randomCount;
            }
        }

        for (const auto& edge : view.getEdges()) {
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

    double computeRegionConditionalProbability(WeightedBDDManager& bddManager,
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
        buildFormulasCyclewise(regionView, bddManager, nodeFormulas, edgeFormulas, seedTrue,
                               debug ? &roundTimings : nullptr);
        auto t1 = std::chrono::steady_clock::now();
        double buildMs = std::chrono::duration<double, std::milli>(t1 - t0).count();

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
                          << " ms, local build " << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
                          << " ms]" << std::endl;
            }
            return pExit;
        }

        auto t2 = std::chrono::steady_clock::now();
        double pExit  = bddManager.computeWeightedModelCount(itExit->second);
        double pEntry = bddManager.computeWeightedModelCount(itEntry->second);
        auto t3 = std::chrono::steady_clock::now();
        double wmcMs = std::chrono::duration<double, std::milli>(t3 - t2).count();

        if (timingOut) {
            timingOut->mgrInitMs = managerInitMs;
            timingOut->buildMs = buildMs;
            timingOut->wmcMs = wmcMs;
            timingOut->roundTimingsMs = roundTimings;
            timingOut->liveNodes = Cudd_ReadNodeCount(bddManager.getManager());
            timingOut->memMb = Cudd_ReadMemoryInUse(bddManager.getManager()) / (1024.0 * 1024);
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
            double memMb = Cudd_ReadMemoryInUse(bddManager.getManager()) / (1024.0 * 1024);

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

    EdgePtr applyRegionRewrite(IncrementalDerivationGraph& graph,
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
            // Fold only when SI has no other outgoing edges and SO has no other incoming edges,
            // and SO is not an output/query node; otherwise just update the edge prob.
            EdgePtr oldEdge = region.internalEdges.front();
            // Update the single edge probability (keep nodes/edge to preserve correlations).
            if (oldEdge) {
                oldEdge->setProbability(condProb);
            }
            if (debug) {
                std::cout << "[GraphRewriter]   Updated simple fact region edge "
                          << regionToString(region)
                          << " with newPr=" << condProb << " (no node/edge removal)."
                          << std::endl;
            }
            return oldEdge;
        }

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
            std::cout << "[GraphRewriter]   Rewrote region "
                      << regionToString(region)
                      << " -> new edge id=" << newEdge->getId()
                      << " with Pr(exit|entry)=" << condProb
                      << " ; removed " << removedNodes << " node(s), "
                      << removedEdges << " edge(s)." << std::endl;
        }
        return newEdge;
    }

    mutable std::unordered_set<size_t> simpleProcessedEdges_;
};

}  // namespace souffle::problog
