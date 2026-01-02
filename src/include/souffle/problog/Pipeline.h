#pragma once

#include "souffle/CompiledOptions.h"
#include "souffle/CompiledSouffle.h"
#include "souffle/Derivation.h"
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

#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <string>
#include <unordered_map>
#include <chrono>
#include <utility>
#include <vector>

namespace souffle::problog {

inline std::string makeOutputPath(const CmdOptions& opt, const std::string& filename) {
    const std::string& dir = opt.getOutputFileDir();
    if (dir.empty()) {
        return filename;
    }
    if (dir.back() == '/') {
        return dir + filename;
    }
    return dir + "/" + filename;
}

inline void dumpSisoRegions(const DerivationGraphViewInterface& view) {
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

inline void runBddPipeline(
        const CmdOptions& opt,
        SouffleProgram& program,
        RuleManager& ruleManager,
        QueryManager& queryManager,
        IncrementalDerivationGraph& graph,
        SubgraphView& view,
        bool enableOnlineCli) {
    Debugger& debugger = Debugger::getInstance();

    std::map<NodePtr, BddNodeRef> nodeFormulas;
    std::map<EdgePtr, BddNodeRef> edgeFormulas;
    WeightedBDDManager bddManager;
    const bool computeProbabilities = !opt.isDerivationOnly();

    if (computeProbabilities) {
        debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
        auto t0 = std::chrono::steady_clock::now();
        buildFormulasCyclewise(view, bddManager, nodeFormulas, edgeFormulas);
        auto t1 = std::chrono::steady_clock::now();
        std::cout << "[pipeline] BDD formula build took "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
                  << " ms\n";
        debugger.endStage();

        debugger.startStage(StageKind::WEIGHTED_MODEL_COUNTING_FULL);

        auto t2 = std::chrono::steady_clock::now();
        auto& depGraph = view.getCycleDependencyGraph();
        size_t componentCount = depGraph.getComponentCount();
        std::vector<BddNodeRef> componentEvidenceBdd(componentCount, bddManager.getTrue());
        std::vector<double> componentEvidenceWeight(componentCount, 1.0);
        std::vector<bool> componentHasEvidence(componentCount, false);

        for (size_t cid = 0; cid < componentCount; ++cid) {
            const auto& evidences = depGraph.getComponentEvidences(cid);
            if (evidences.empty()) {
                continue;
            }
            componentHasEvidence[cid] = true;
            BddNodeRef evidenceBdd = bddManager.getTrue();
            for (const auto& [eNode, val] : evidences) {
                auto it = nodeFormulas.find(eNode);
                if (it == nodeFormulas.end()) {
                    throw std::runtime_error("Evidence node has no formula: " + eNode->getTuple().toString());
                }
                auto lit = it->second;
                if (!val) {
                    lit = bddManager.makeNot(lit);
                }
                evidenceBdd = bddManager.makeAnd(evidenceBdd, lit);
            }
            componentEvidenceBdd[cid] = evidenceBdd;
        }
        auto t3 = std::chrono::steady_clock::now();

        for (size_t cid = 0; cid < componentCount; ++cid) {
            if (componentHasEvidence[cid]) {
                componentEvidenceWeight[cid] =
                        bddManager.computeWeightedModelCount(componentEvidenceBdd[cid]);
            }
        }
        auto t4 = std::chrono::steady_clock::now();

        probResult.clear();
        for (const auto& [node, bdd] : nodeFormulas) {
            if (!node->needOutput) {
                continue;
            }
            double prob = 0.0;
            size_t cid = depGraph.getComponentId(node);
            if (!componentHasEvidence[cid]) {
                prob = bddManager.computeWeightedModelCount(bdd);
            } else if (componentEvidenceWeight[cid] == 0.0) {
                prob = 0.0; // inconsistent evidence in this component
            } else {
                auto joint = bddManager.makeAnd(bdd, componentEvidenceBdd[cid]);
                double jointW = bddManager.computeWeightedModelCount(joint);
                prob = jointW / componentEvidenceWeight[cid];
            }
            probResult[node] = prob;
        }

        auto t5 = std::chrono::steady_clock::now();

        std::cout << "[pipeline] component evidence build took "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()
                  << " ms\n";
        std::cout << "[pipeline] component evidence WMC took "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count()
                  << " ms\n";
        std::cout << "[pipeline] per-node conditional WMC took "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(t5 - t4).count()
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

        debugger.endStage();
    }

    debugger.endTurn();
    dumpInitialInputRelations(opt.getOutputFileDir() + "/initial-input-relations-iter0.txt");

    if (enableOnlineCli) {
        IncrementalCLI<BddNodeRef> cli(
                &program, &graph, &ruleManager, &queryManager, &bddManager, &nodeFormulas,
                &edgeFormulas, false);
        cli.setCmdOptions(opt);
        cli.run();
    }
}

inline void runSddPipeline(
        const CmdOptions& opt,
        SouffleProgram& program,
        RuleManager& ruleManager,
        QueryManager& queryManager,
        IncrementalDerivationGraph& graph,
        SubgraphView& view,
        bool enableOnlineCli) {
    Debugger& debugger = Debugger::getInstance();

    std::map<NodePtr, SddNodeRef> nodeFormulas;
    std::map<EdgePtr, SddNodeRef> edgeFormulas;
    SddFormulaManager sddManager;
    const bool computeProbabilities = !opt.isDerivationOnly();

    if (computeProbabilities) {
        debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
        auto t0 = std::chrono::steady_clock::now();
        buildFormulasCyclewise(view, sddManager, nodeFormulas, edgeFormulas);
        auto t1 = std::chrono::steady_clock::now();
        std::cout << "[pipeline] SDD formula build took "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count()
                  << " ms\n";
        debugger.endStage();

        debugger.startStage(StageKind::WEIGHTED_MODEL_COUNTING_FULL);

        auto t2 = std::chrono::steady_clock::now();
        auto& depGraph = view.getCycleDependencyGraph();
        size_t componentCount = depGraph.getComponentCount();
        std::vector<SddNodeRef> componentEvidenceSdd(componentCount, sddManager.getTrue());
        std::vector<double> componentEvidenceWeight(componentCount, 1.0);
        std::vector<bool> componentHasEvidence(componentCount, false);

        for (size_t cid = 0; cid < componentCount; ++cid) {
            const auto& evidences = depGraph.getComponentEvidences(cid);
            if (evidences.empty()) {
                continue;
            }
            componentHasEvidence[cid] = true;
            SddNodeRef evidenceSdd = sddManager.getTrue();
            for (const auto& [eNode, val] : evidences) {
                auto it = nodeFormulas.find(eNode);
                if (it == nodeFormulas.end()) {
                    throw std::runtime_error("Evidence node has no formula: " + eNode->getTuple().toString());
                }
                auto lit = it->second;
                if (!val) {
                    lit = sddManager.makeNot(lit);
                }
                evidenceSdd = sddManager.makeAnd(evidenceSdd, lit);
            }
            componentEvidenceSdd[cid] = evidenceSdd;
        }
        auto t3 = std::chrono::steady_clock::now();

        for (size_t cid = 0; cid < componentCount; ++cid) {
            if (componentHasEvidence[cid]) {
                componentEvidenceWeight[cid] =
                        sddManager.computeWeightedModelCount(componentEvidenceSdd[cid]);
            }
        }
        auto t4 = std::chrono::steady_clock::now();

        probResult.clear();
        for (const auto& [node, sdd] : nodeFormulas) {
            if (!node->needOutput) {
                continue;
            }
            double prob = 0.0;
            size_t cid = depGraph.getComponentId(node);
            if (!componentHasEvidence[cid]) {
                prob = sddManager.computeWeightedModelCount(sdd);
            } else if (componentEvidenceWeight[cid] == 0.0) {
                prob = 0.0;
            } else {
                auto joint = sddManager.makeAnd(sdd, componentEvidenceSdd[cid]);
                double jointW = sddManager.computeWeightedModelCount(joint);
                prob = jointW / componentEvidenceWeight[cid];
            }
            probResult[node] = prob;
        }

        auto t5 = std::chrono::steady_clock::now();

        view.dumpStatistics(std::cout);
        std::cout << "[pipeline] component evidence build took "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()
                  << " ms\n";
        std::cout << "[pipeline] component evidence WMC took "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count()
                  << " ms\n";
        std::cout << "[pipeline] per-node conditional WMC took "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(t5 - t4).count()
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
        debugger.endStage();
    }

    debugger.endTurn();
    dumpInitialInputRelations(opt.getOutputFileDir() + "/initial-input-relations-iter0.txt");

    if (enableOnlineCli) {
        IncrementalCLI<SddNodeRef> cli(
                &program, &graph, &ruleManager, &queryManager, &sddManager, &nodeFormulas,
                &edgeFormulas);
        cli.setCmdOptions(opt);
        cli.run();
    }
}

inline void runPipeline(
        const CmdOptions& opt,
        SouffleProgram& program,
        RuleManager& ruleManager,
        QueryManager& queryManager,
        const std::unordered_map<UntypedTuple, double>& factProb,
        const std::vector<std::pair<UntypedTuple, bool>>& evidences,
        bool enableOnlineCli = false) {
    std::cout << std::fixed << std::setprecision(8);
    Debugger& debugger = Debugger::getInstance();
    DerivationGraphViewInterface::setDumpDotEnabled(opt.isDumpDotEnabled());
    DerivationGraphViewInterface::setDumpStatsEnabled(opt.isDumpStatEnabled());
    DerivationGraphViewInterface::setDumpOutputDir(opt.getOutputFileDir());
    DerivationGraph::setConstFoldEnabled(opt.isConstFoldEnabled());
    DerivationGraph::setConstDumpEnabled(opt.isDumpConstEnabled());
    bool rewritePerformed = false;

    debugger.startStage(StageKind::CREATE_GRAPH_FULL);
    auto t0 = std::chrono::steady_clock::now();
    auto graph = IncrementalDerivationGraph::createFrom(
            DerivationManager::untypedTuple2RuleApplications, ruleManager, queryManager, factProb,
            evidences);
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
        runBddPipeline(opt, program, ruleManager, queryManager, *graph, view, allowOnlineCli);
    } else if (program.getKnowledge() == souffle::Knowledge::SDD) {
        runSddPipeline(opt, program, ruleManager, queryManager, *graph, view, allowOnlineCli);
    } else {
        std::cerr << "Unknown knowledge representation" << std::endl;
    }
}

}  // namespace souffle::problog
