#include "souffle/problog/Pipeline.h"

#include "souffle/cli/Cli.h"
#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/ForwardCompilation.h"
#include "souffle/problog/GraphAnalyzer.h"
#include "souffle/problog/GraphRewriter.h"
#include "souffle/problog/QueryManager.h"
#include "souffle/problog/RuleManager.h"
#include "souffle/problog/debug/Debugger.h"
#include "souffle/problog/formula/CuddManager.h"
#include "souffle/problog/formula/SddManager.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <limits>
#include <unordered_map>
#include <utility>

namespace souffle::problog {

namespace {

static std::size_t estimateBddVarCount(const SubgraphView& view) {
    std::size_t count = 0;
    for (const auto& node : view.getNodes()) {
        if (node->isFact && node->getProbability() != 1.0) {
            ++count;
        }
    }
    for (const auto& edge : view.getEdges()) {
        if (!edge->isDeterministic()) {
            ++count;
        }
    }
    return count;
}

static WeightedBDDManager::InitConfig makeCuddInitConfig(std::size_t varCount) {
    WeightedBDDManager::InitConfig cfg;
    // Smaller graphs downscale cache/memory; large graphs keep the default (largest) config.
    if (varCount <= 256) {
        cfg.numVars = 256;
        cfg.numSlots = 2048;
        cfg.cacheSize = 1u << 18;
        cfg.maxMemory = 1UL << 30;
    } else if (varCount <= 1024) {
        cfg.numVars = 512;
        cfg.numSlots = 4096;
        cfg.cacheSize = 1u << 20;
        cfg.maxMemory = 4UL << 30;
    } else if (varCount <= 4096) {
        cfg.numVars = 1000;
        cfg.numSlots = 4096;
        cfg.cacheSize = 1u << 22;
        cfg.maxMemory = 8UL << 30;
    }
    return cfg;
}

static std::vector<std::vector<std::pair<NodePtr, bool>>> groupEvidencesByComponent(
        const DerivationGraphViewInterface& view,
        const std::vector<std::pair<NodePtr, bool>>& evidences) {
    auto& depGraph = view.getCycleDependencyGraph();
    const size_t componentCount = depGraph.getComponentCount();
    std::vector<std::vector<std::pair<NodePtr, bool>>> byComponent(componentCount);

    for (const auto& ev : evidences) {
        const NodePtr& node = ev.first;
        if (!node || view.getNodes().count(node) == 0) {
            throw std::runtime_error("Evidence node not found in view: " +
                    (node ? node->toString() : std::string("null")));
        }
        size_t cid = depGraph.getComponentId(node);
        byComponent[cid].push_back(ev);
    }
    return byComponent;
}

static bool evidenceSatisfied(
        const std::vector<std::pair<NodePtr, bool>>& evidences,
        const std::unordered_map<NodePtr, bool>& values) {
    for (const auto& [node, expected] : evidences) {
        auto it = values.find(node);
        bool actual = (it != values.end()) ? it->second : false;
        if (actual != expected) {
            return false;
        }
    }
    return true;
}

struct FastComponentEval {
    ComponentSubgraph comp;
    SingleRandVarInfo var;
    std::unordered_map<NodePtr, bool> valuesTrue;
    std::unordered_map<NodePtr, bool> valuesFalse;
    long long evalMsTrue = 0;
    long long evalMsFalse = 0;
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

static std::vector<ComponentSubgraph> collectSingleRandFastComponents(
        std::vector<ComponentSubgraph> components,
        bool enableFast,
        std::vector<FastComponentEval>& fastComponents,
        FastComponentStats& stats) {
    if (!enableFast) {
        return components;
    }

    std::vector<ComponentSubgraph> slowComponents;
    slowComponents.reserve(components.size());

    for (auto& comp : components) {
        SingleRandVarInfo var;
        if (!findSingleRandVar(comp, var)) {
            slowComponents.push_back(std::move(comp));
            continue;
        }

        stats.candidates++;
        FastComponentEval eval;
        eval.comp = std::move(comp);
        eval.var = var;

        if (!evaluateSingleRandComponent(
                    eval.comp, eval.var, true, eval.valuesTrue, &eval.evalMsTrue) ||
                !evaluateSingleRandComponent(
                        eval.comp, eval.var, false, eval.valuesFalse, &eval.evalMsFalse)) {
            stats.skipped++;
            slowComponents.push_back(std::move(eval.comp));
            continue;
        }

        stats.used++;
        stats.evalMs += eval.evalMsTrue + eval.evalMsFalse;
        std::cout << "[fc-component] id=" << eval.comp.id
                  << " fast_path=1"
                  << " nodes=" << eval.comp.nodes.size()
                  << " edges=" << eval.comp.edges.size()
                  << " rand_vars=" << countComponentRandomVars(eval.comp)
                  << " eval_ms_true=" << eval.evalMsTrue
                  << " eval_ms_false=" << eval.evalMsFalse
                  << " total_ms=" << (eval.evalMsTrue + eval.evalMsFalse)
                  << std::endl;
        fastComponents.push_back(std::move(eval));
    }

    return slowComponents;
}

} // namespace

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

static std::vector<std::pair<NodePtr, bool>> applyEvidence(
        IncrementalDerivationGraph& graph,
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

static void runBddPipeline(
        const CmdOptions& opt,
        SouffleProgram& program,
        RuleManager& ruleManager,
        QueryManager& queryManager,
        IncrementalDerivationGraph& graph,
        SubgraphView& view,
        const std::vector<std::pair<UntypedTuple, bool>>& evidences,
        bool enableOnlineCli) {
    Debugger& debugger = Debugger::getInstance();

    std::map<NodePtr, BddNodeRef> nodeFormulas;
    std::map<EdgePtr, BddNodeRef> edgeFormulas;
    std::unique_ptr<WeightedBDDManager> bddManager;
    const bool computeProbabilities = !opt.isDerivationOnly();

    if (computeProbabilities) {
        auto* stage = debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
        auto varEstimate = estimateBddVarCount(view);
        debugger.addInfo("rand_vars", std::to_string(varEstimate));
        if (stage) {
            stage->logMessage(Level::INFO, "rand_vars=" + std::to_string(varEstimate));
        }

        if (opt.isRewriteEnabled()) {
            auto components = buildComponentSubgraphs(view);
            std::vector<FastComponentEval> fastComponents;
            FastComponentStats fastStats;
            auto slowComponents = collectSingleRandFastComponents(
                    std::move(components), opt.isSingleRandFastEnabled(), fastComponents, fastStats);

            std::vector<SlowComponentEval> slowEvals;
            slowEvals.reserve(slowComponents.size());
            for (auto& comp : slowComponents) {
                SlowComponentEval eval;
                eval.randVars = countComponentRandomVars(comp);
                eval.comp = std::move(comp);
                slowEvals.push_back(std::move(eval));
            }
            std::sort(slowEvals.begin(), slowEvals.end(),
                    [](const SlowComponentEval& a, const SlowComponentEval& b) {
                        return a.randVars > b.randVars;
                    });

            long long initMsTotal = 0;
            long long initMsMax = 0;
            long long buildMs = 0;
            WeightedBDDManager::InitConfig initConfig;
            if (!slowEvals.empty()) {
                initConfig = makeCuddInitConfig(slowEvals.front().randVars);
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
            debugger.addInfo("slow_components", std::to_string(slowEvals.size()));
            debugger.addInfo("fastpath_components", std::to_string(fastStats.used));
            debugger.addInfo("fastpath_candidates", std::to_string(fastStats.candidates));
            debugger.addInfo("fastpath_skipped", std::to_string(fastStats.skipped));
            debugger.addInfo("fastpath_eval_ms", std::to_string(fastStats.evalMs));
            if (stage) {
                stage->logMessage(Level::INFO, "manager_init_vars=" + std::to_string(initConfig.numVars));
                stage->logMessage(Level::INFO, "manager_init_slots=" + std::to_string(initConfig.numSlots));
                stage->logMessage(Level::INFO, "manager_init_cache=" + std::to_string(initConfig.cacheSize));
                stage->logMessage(Level::INFO, "manager_init_maxmem_mb=" +
                        std::to_string(initConfig.maxMemory / (1024UL * 1024UL)));
                stage->logMessage(Level::INFO, "manager_init_ms=" + std::to_string(initMsTotal));
                stage->logMessage(Level::INFO, "manager_init_ms_max=" + std::to_string(initMsMax));
                stage->logMessage(Level::INFO, "manager_init_components=" +
                        std::to_string(slowEvals.empty() ? 0 : 1));
                stage->logMessage(Level::INFO, "slow_components=" + std::to_string(slowEvals.size()));
                stage->logMessage(Level::INFO, "fastpath_components=" + std::to_string(fastStats.used));
                stage->logMessage(Level::INFO, "fastpath_candidates=" + std::to_string(fastStats.candidates));
                stage->logMessage(Level::INFO, "fastpath_skipped=" + std::to_string(fastStats.skipped));
                stage->logMessage(Level::INFO, "fastpath_eval_ms=" + std::to_string(fastStats.evalMs));
            }

            debugger.endStage();

            debugger.startStage(StageKind::FC_WMC_HYBRID_FULL);
            auto t2 = std::chrono::steady_clock::now();
            auto resolvedEvs = applyEvidence(graph, evidences);
            auto t3 = std::chrono::steady_clock::now();
            auto evidencesByComponent = groupEvidencesByComponent(view, resolvedEvs);
            long long evidenceBuildMs = 0;
            long long evidenceWmcMs = 0;
            long long perNodeWmcMs = 0;
            long long fastPathMs = 0;
            long long liveNodesSum = 0;

            probResult.clear();
            if (!fastComponents.empty()) {
                auto fastStart = std::chrono::steady_clock::now();
                for (const auto& fast : fastComponents) {
                    const auto& compEvs = evidencesByComponent[fast.comp.id];
                    bool eTrue = evidenceSatisfied(compEvs, fast.valuesTrue);
                    bool eFalse = evidenceSatisfied(compEvs, fast.valuesFalse);
                    double p = fast.var.probability;
                    double evidenceWeight = (eTrue ? p : 0.0) + (eFalse ? (1.0 - p) : 0.0);

                    for (const auto& node : fast.comp.nodes) {
                        auto itTrue = fast.valuesTrue.find(node);
                        bool vTrue = (itTrue != fast.valuesTrue.end()) ? itTrue->second : false;
                        auto itFalse = fast.valuesFalse.find(node);
                        bool vFalse = (itFalse != fast.valuesFalse.end()) ? itFalse->second : false;
                        double numerator = (eTrue && vTrue ? p : 0.0) +
                                (eFalse && vFalse ? (1.0 - p) : 0.0);
                        double prob = (evidenceWeight == 0.0) ? 0.0 : numerator / evidenceWeight;
                        probResult[node] = prob;
                    }
                }
                fastPathMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - fastStart).count();
            }
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
                    evidenceBdd = bddManager->makeAnd(evidenceBdd, lit);
                }
                auto evidenceBuildMsComp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                   std::chrono::steady_clock::now() - evidenceBuildStart)
                                                   .count();
                evidenceBuildMs += evidenceBuildMsComp;

                auto wmcStart = std::chrono::steady_clock::now();
                double evidenceWeight = 1.0;
                if (!componentEvs.empty()) {
                    evidenceWeight = bddManager->computeWeightedModelCount(evidenceBdd);
                }
                auto evidenceWmcMsComp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                 std::chrono::steady_clock::now() - wmcStart)
                                                 .count();
                evidenceWmcMs += evidenceWmcMsComp;

                auto perNodeStart = std::chrono::steady_clock::now();
                for (const auto& [node, bdd] : compNodeFormulas) {
                    double prob = 0.0;
                    if (componentEvs.empty()) {
                        prob = bddManager->computeWeightedModelCount(bdd);
                    } else if (evidenceWeight == 0.0) {
                        prob = 0.0;
                    } else {
                        auto joint = bddManager->makeAnd(bdd, evidenceBdd);
                        double jointW = bddManager->computeWeightedModelCount(joint);
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
            if (fastStats.used > 0) {
                std::cout << "[pipeline] fastpath single-rand WMC took " << fastPathMs << " ms\n";
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
            auto initConfig = makeCuddInitConfig(varEstimate);
            debugger.addInfo("manager_init_vars", std::to_string(initConfig.numVars));
            debugger.addInfo("manager_init_slots", std::to_string(initConfig.numSlots));
            debugger.addInfo("manager_init_cache", std::to_string(initConfig.cacheSize));
            debugger.addInfo("manager_init_maxmem_mb",
                    std::to_string(initConfig.maxMemory / (1024UL * 1024UL)));
            if (stage) {
                stage->logMessage(Level::INFO, "manager_init_vars=" + std::to_string(initConfig.numVars));
                stage->logMessage(Level::INFO, "manager_init_slots=" + std::to_string(initConfig.numSlots));
                stage->logMessage(Level::INFO, "manager_init_cache=" + std::to_string(initConfig.cacheSize));
                stage->logMessage(Level::INFO, "manager_init_maxmem_mb=" +
                        std::to_string(initConfig.maxMemory / (1024UL * 1024UL)));
            }
            auto initStart = std::chrono::steady_clock::now();
            bddManager = std::make_unique<WeightedBDDManager>(initConfig);
            auto initMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now() - initStart)
                                  .count();
            debugger.addInfo("manager_init_ms", std::to_string(initMs));
            if (stage) {
                stage->logMessage(Level::INFO, "manager_init_ms=" + std::to_string(initMs));
            }
            auto t0 = std::chrono::steady_clock::now();
            buildFormulasCyclewise(view, *bddManager, nodeFormulas, edgeFormulas);
            auto t1 = std::chrono::steady_clock::now();
            std::cout << "[pipeline] BDD formula build took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
                      << " ms\n";
            debugger.endStage();

            debugger.startStage(StageKind::WEIGHTED_MODEL_COUNTING_FULL);

            auto t2 = std::chrono::steady_clock::now();
            auto resolvedEvs = applyEvidence(graph, evidences);
            auto t3 = std::chrono::steady_clock::now();

            auto evidenceBdd = bddManager->getTrue();

            for (const auto& [eNode, val] : resolvedEvs) {
                auto it = nodeFormulas.find(eNode);
                if (it == nodeFormulas.end()) {
                    throw std::runtime_error("Evidence node has no formula: " +
                            eNode->getTuple().toString());
                }

                auto lit = it->second;
                if (!val) {
                    lit = bddManager->makeNot(lit);
                }
                evidenceBdd = bddManager->makeAnd(evidenceBdd, lit);
            }

            auto t4 = std::chrono::steady_clock::now();

            double evidenceWeight = 1.0;
            if (!resolvedEvs.empty()) {
                evidenceWeight = bddManager->computeWeightedModelCount(evidenceBdd);
            }

            auto t5 = std::chrono::steady_clock::now();

            probResult.clear();
            for (const auto& [node, bdd] : nodeFormulas) {
                double prob = 0.0;

                if (resolvedEvs.empty()) {
                    prob = bddManager->computeWeightedModelCount(bdd);
                } else if (evidenceWeight == 0.0) {
                    prob = 0.0;
                } else {
                    auto joint = bddManager->makeAnd(bdd, evidenceBdd);
                    double jointW = bddManager->computeWeightedModelCount(joint);
                    prob = jointW / evidenceWeight;
                }

                probResult[node] = prob;
            }
            for (const auto& [node, prob] : precomputedProbResult) {
                probResult.emplace(node, prob);
            }

            auto t6 = std::chrono::steady_clock::now();

            std::cout << "[pipeline] evidence resolve/tag took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()
                      << " ms\n";
            std::cout << "[pipeline] evidence BDD build took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count()
                      << " ms\n";
            std::cout << "[pipeline] evidence WMC took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t5 - t4).count()
                      << " ms\n";
            std::cout << "[pipeline] per-node conditional WMC took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t6 - t5).count()
                      << " ms\n";

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

    if (enableOnlineCli) {
        IncrementalCLI<BddNodeRef> cli(
                &program, &graph, &ruleManager, &queryManager, bddManager.get(), &nodeFormulas,
                &edgeFormulas, false);
        cli.setCmdOptions(opt);
        cli.run();
    }
}

static void runSddPipeline(
        const CmdOptions& opt,
        SouffleProgram& program,
        RuleManager& ruleManager,
        QueryManager& queryManager,
        IncrementalDerivationGraph& graph,
        SubgraphView& view,
        const std::vector<std::pair<UntypedTuple, bool>>& evidences,
        bool enableOnlineCli) {
    Debugger& debugger = Debugger::getInstance();

    std::map<NodePtr, SddNodeRef> nodeFormulas;
    std::map<EdgePtr, SddNodeRef> edgeFormulas;
    std::unique_ptr<SddFormulaManager> sddManager;
    const bool computeProbabilities = !opt.isDerivationOnly();

    if (computeProbabilities) {
        auto* stage = debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
        if (opt.isRewriteEnabled()) {
            auto components = buildComponentSubgraphs(view);
            std::vector<FastComponentEval> fastComponents;
            FastComponentStats fastStats;
            auto slowComponents = collectSingleRandFastComponents(
                    std::move(components), opt.isSingleRandFastEnabled(), fastComponents, fastStats);

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

            debugger.addInfo("manager_init_ms", std::to_string(initMsTotal));
            debugger.addInfo("manager_init_ms_max", std::to_string(initMsMax));
            debugger.addInfo("manager_init_components", std::to_string(bundles.size()));
            debugger.addInfo("fastpath_components", std::to_string(fastStats.used));
            debugger.addInfo("fastpath_candidates", std::to_string(fastStats.candidates));
            debugger.addInfo("fastpath_skipped", std::to_string(fastStats.skipped));
            debugger.addInfo("fastpath_eval_ms", std::to_string(fastStats.evalMs));
            if (stage) {
                stage->logMessage(Level::INFO, "manager_init_ms=" + std::to_string(initMsTotal));
                stage->logMessage(Level::INFO, "manager_init_ms_max=" + std::to_string(initMsMax));
                stage->logMessage(Level::INFO, "manager_init_components=" +
                        std::to_string(bundles.size()));
                stage->logMessage(Level::INFO, "fastpath_components=" + std::to_string(fastStats.used));
                stage->logMessage(Level::INFO, "fastpath_candidates=" + std::to_string(fastStats.candidates));
                stage->logMessage(Level::INFO, "fastpath_skipped=" + std::to_string(fastStats.skipped));
                stage->logMessage(Level::INFO, "fastpath_eval_ms=" + std::to_string(fastStats.evalMs));
            }

            std::cout << "[pipeline] SDD formula build took " << buildMs << " ms\n";
            debugger.endStage();

            debugger.startStage(StageKind::WEIGHTED_MODEL_COUNTING_FULL);

            auto t2 = std::chrono::steady_clock::now();
            auto resolvedEvs = applyEvidence(graph, evidences);
            auto t3 = std::chrono::steady_clock::now();
            auto evidencesByComponent = groupEvidencesByComponent(view, resolvedEvs);
            long long evidenceBuildMs = 0;
            long long evidenceWmcMs = 0;
            long long perNodeWmcMs = 0;
            long long fastPathMs = 0;

            probResult.clear();
            if (!fastComponents.empty()) {
                auto fastStart = std::chrono::steady_clock::now();
                for (const auto& fast : fastComponents) {
                    const auto& compEvs = evidencesByComponent[fast.comp.id];
                    bool eTrue = evidenceSatisfied(compEvs, fast.valuesTrue);
                    bool eFalse = evidenceSatisfied(compEvs, fast.valuesFalse);
                    double p = fast.var.probability;
                    double evidenceWeight = (eTrue ? p : 0.0) + (eFalse ? (1.0 - p) : 0.0);

                    for (const auto& node : fast.comp.nodes) {
                        auto itTrue = fast.valuesTrue.find(node);
                        bool vTrue = (itTrue != fast.valuesTrue.end()) ? itTrue->second : false;
                        auto itFalse = fast.valuesFalse.find(node);
                        bool vFalse = (itFalse != fast.valuesFalse.end()) ? itFalse->second : false;
                        double numerator = (eTrue && vTrue ? p : 0.0) +
                                (eFalse && vFalse ? (1.0 - p) : 0.0);
                        double prob = (evidenceWeight == 0.0) ? 0.0 : numerator / evidenceWeight;
                        probResult[node] = prob;
                    }
                }
                fastPathMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - fastStart).count();
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
            if (fastStats.used > 0) {
                std::cout << "[pipeline] fastpath single-rand WMC took " << fastPathMs << " ms\n";
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
            auto initStart = std::chrono::steady_clock::now();
            sddManager = std::make_unique<SddFormulaManager>();
            auto initMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                  std::chrono::steady_clock::now() - initStart)
                                  .count();
            debugger.addInfo("manager_init_ms", std::to_string(initMs));
            if (stage) {
                stage->logMessage(Level::INFO, "manager_init_ms=" + std::to_string(initMs));
            }
            auto t0 = std::chrono::steady_clock::now();
            buildFormulasCyclewise(view, *sddManager, nodeFormulas, edgeFormulas);
            auto t1 = std::chrono::steady_clock::now();
            std::cout << "[pipeline] SDD formula build took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
                      << " ms\n";
            debugger.endStage();

            debugger.startStage(StageKind::WEIGHTED_MODEL_COUNTING_FULL);

            auto t2 = std::chrono::steady_clock::now();
            auto resolvedEvs = applyEvidence(graph, evidences);
            auto t3 = std::chrono::steady_clock::now();

            auto evidenceSdd = sddManager->getTrue();
            for (const auto& [eNode, val] : resolvedEvs) {
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

            auto t4 = std::chrono::steady_clock::now();

            double evidenceWeight = 1.0;
            if (!resolvedEvs.empty()) {
                evidenceWeight = sddManager->computeWeightedModelCount(evidenceSdd);
            }

            auto t5 = std::chrono::steady_clock::now();

            probResult.clear();
            for (const auto& [node, sdd] : nodeFormulas) {
                double prob = 0.0;

                if (resolvedEvs.empty()) {
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
            for (const auto& [node, prob] : precomputedProbResult) {
                probResult.emplace(node, prob);
            }

            auto t6 = std::chrono::steady_clock::now();

            view.dumpStatistics(std::cout);
            std::cout << "[pipeline] component evidence build took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()
                      << " ms\n";
            std::cout << "[pipeline] component evidence WMC took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count()
                      << " ms\n";
            std::cout << "[pipeline] per-node conditional WMC took "
                      << std::chrono::duration_cast<std::chrono::milliseconds>(t6 - t5).count()
                      << " ms\n";

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

    if (enableOnlineCli) {
        IncrementalCLI<SddNodeRef> cli(
                &program, &graph, &ruleManager, &queryManager, sddManager.get(), &nodeFormulas,
                &edgeFormulas, false);
        cli.setCmdOptions(opt);
        cli.run();
    }
}

void runPipeline(
        const CmdOptions& opt,
        SouffleProgram& program,
        RuleManager& ruleManager,
        QueryManager& queryManager,
        const std::unordered_map<UntypedTuple, double>& factProb,
        const std::vector<std::pair<UntypedTuple, bool>>& evidences,
        bool enableOnlineCli) {
    std::cout << std::fixed << std::setprecision(8);
    Debugger& debugger = Debugger::getInstance();
    DerivationGraphViewInterface::setDumpDotEnabled(opt.isDumpDotEnabled());
    DerivationGraphViewInterface::setDumpJsonEnabled(opt.isDumpJsonEnabled());
    DerivationGraphViewInterface::setDumpStatsEnabled(opt.isDumpStatEnabled());
    DerivationGraphViewInterface::setDumpOutputDir(opt.getOutputFileDir());
    DerivationGraph::setMergeBiImpEnabled(opt.isMergeBiImpEnabled());
    DerivationGraph::setConstFoldEnabled(opt.isConstFoldEnabled());
    DerivationGraph::setConstDumpEnabled(opt.isDumpConstEnabled());
    precomputedProbResult.clear();
    bool rewritePerformed = false;

    debugger.startStage(StageKind::CREATE_GRAPH_FULL);
    auto t0 = std::chrono::steady_clock::now();
    auto graph = IncrementalDerivationGraph::createFrom(
            DerivationManager::untypedTuple2RuleApplications, ruleManager, queryManager, factProb, evidences);
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
    auto view = graph->prune(program.getOutputRelations());
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
    if (opt.isRewriteEnabled()) {
        debugger.startStage(StageKind::PRECONFIG_FULL);
        auto rewriteStart = std::chrono::steady_clock::now();
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
        auto rewriteStats = rewriter.rewriteUntilFixpoint(*graph, view, opt.isProfiling(), rewriteFlags);
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
                  << ", nodesRemoved=" << rewriteStats.numNodesRemoved
                  << ", edgesRemoved=" << rewriteStats.numEdgesRemoved
                  << ", edgesAdded=" << rewriteStats.numEdgesAdded
                  << ", randomVarsBefore=" << rewriteStats.randomVarsBefore
                  << ", randomVarsAfter=" << rewriteStats.randomVarsAfter
                  << ", randomVarsDelta=" << randomVarsDelta
                  << ", randomVarsRatio=" << randomVarsRatio
                  << ", randomVarsRemoved=" << rewriteStats.totalRandomVars
                  << ", simpleFactRegions=" << rewriteStats.simpleFactRegions << std::endl;
        if (opt.isDumpDotEnabled()) {
            view.dumpDot(makeOutputPath(opt, "rewrite_final.dot"));
        }
        debugger.endStage();
        rewritePerformed = true;
    }

    bool allowOnlineCli = enableOnlineCli && !rewritePerformed;
    if (enableOnlineCli && rewritePerformed) {
        std::cout << "[pipeline] rewrite performed in full run; skip incremental CLI" << std::endl;
    }

    if (program.getKnowledge() == souffle::Knowledge::BDD) {
        runBddPipeline(opt, program, ruleManager, queryManager, *graph, view, evidences, allowOnlineCli);
    } else if (program.getKnowledge() == souffle::Knowledge::SDD) {
        runSddPipeline(opt, program, ruleManager, queryManager, *graph, view, evidences, allowOnlineCli);
    } else {
        std::cerr << "Unknown knowledge representation" << std::endl;
    }
}

}  // namespace souffle::problog
