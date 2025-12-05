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

inline void applyEvidence(
        IncrementalDerivationGraph& graph,
        const std::vector<std::pair<UntypedTuple, bool>>& evidences) {
    auto start = std::chrono::steady_clock::now();
    for (const auto& e : evidences) {
        NodePtr node = graph.findNode(e.first);
        if (!node) {
            std::cerr << "Error: evidence " << e.first.toString()
                      << " is not found in the graph." << std::endl;
            exit(1);
        }
        node->setEvidence(e.second);
    }
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start)
                       .count();
    std::cout << "[pipeline] evidence tagging took " << dur << " ms\n";
}

inline void dumpSisoRegions(const DerivationGraphViewInterface& view) {
    auto start = std::chrono::steady_clock::now();
    auto regions = GraphAnalyzer::detectAllSISOStrictFromExit(view);
    auto dur = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - start)
                       .count();
    size_t pureTwoNodeCount = 0;
    size_t singleHyperedgeCount = 0;
    size_t generalCount = 0;
    for (const auto& r : regions) {
        switch (r.kind) {
        case SISORegionKind::PureTwoNode:
            ++pureTwoNodeCount;
            break;
        case SISORegionKind::SingleHyperedge:
            ++singleHyperedgeCount;
            break;
        case SISORegionKind::General:
            ++generalCount;
            break;
        default:
            break;
        }
    }
    std::cout << "Found " << regions.size() << " SISO regions"
              << " (pure-two-node=" << pureTwoNodeCount
              << ", single-hyperedge=" << singleHyperedgeCount
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
        IncrementalDerivationGraph& graph,
        SubgraphView& view,
        const std::vector<std::pair<UntypedTuple, bool>>& evidences,
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
        applyEvidence(graph, evidences);
        auto t3 = std::chrono::steady_clock::now();

        auto evidenceBdd = bddManager.getTrue();
        for (const auto& [node, bdd] : nodeFormulas) {
            if (node->hasEvidence()) {
                evidenceBdd = bddManager.makeAnd(evidenceBdd, bdd);
            }
        }
        auto t4 = std::chrono::steady_clock::now();
        double evidenceWeight = bddManager.computeWeightedModelCount(evidenceBdd);
        std::cout << "Evidence WMC: " << evidenceWeight << std::endl;
        auto t5 = std::chrono::steady_clock::now();

        probResult.clear();
        for (const auto& [node, bdd] : nodeFormulas) {
            auto conditionedBdd = bdd;
            for (const auto& [eNode, ebdd] : nodeFormulas) {
                if (eNode->hasEvidence()) {
                    conditionedBdd = bddManager.makeAnd(conditionedBdd, ebdd);
                }
            }
            double weightedCount = bddManager.computeWeightedModelCount(conditionedBdd);
            double prob = (evidenceWeight == 0.0) ? 0.0 : weightedCount / evidenceWeight;
            probResult[node] = prob;
        }
        auto t6 = std::chrono::steady_clock::now();
        std::cout << "[pipeline] evidence conditioning took "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()
                  << " ms\n";
        std::cout << "[pipeline] evidence WMC took "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(t5 - t4).count()
                  << " ms\n";
        std::cout << "[pipeline] per-node conditional WMC took "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(t6 - t5).count()
                  << " ms\n";
        debugger.endStage();

        debugger.startStage(StageKind::IO_DUMP_FULL);
        auto t7 = std::chrono::steady_clock::now();
        dumpProbabilities(probResult, opt.getOutputFileDir());
        auto t8 = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - t7)
                          .count();
        std::cout << "[pipeline] probability dump took " << t8 << " ms\n";
        debugger.endStage();
    }

    debugger.endTurn();
    dumpInitialInputRelations(opt.getOutputFileDir() + "/initial-input-relations-iter0.txt");

    if (enableOnlineCli) {
        IncrementalCLI<BddNodeRef> cli(
                &program, &graph, &ruleManager, &bddManager, &nodeFormulas, &edgeFormulas, false);
        cli.setCmdOptions(opt);
        cli.run();
    }
}

inline void runSddPipeline(
        const CmdOptions& opt,
        SouffleProgram& program,
        RuleManager& ruleManager,
        IncrementalDerivationGraph& graph,
        SubgraphView& view,
        bool enableOnlineCli) {
    Debugger& debugger = Debugger::getInstance();

    std::map<NodePtr, SddNodeRef> nodeFormulas;
    std::map<EdgePtr, SddNodeRef> edgeFormulas;
    SddFormulaManager sddManager(view.getNodes().size() + view.getEdges().size());
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
        probResult.clear();
        for (const auto& [node, sdd] : nodeFormulas) {
            probResult[node] = sddManager.computeWeightedModelCount(sdd);
        }
        auto t3 = std::chrono::steady_clock::now();
        std::cout << "[pipeline] SDD WMC took "
                  << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()
                  << " ms\n";
        debugger.endStage();

        debugger.startStage(StageKind::IO_DUMP_FULL);
        auto t4 = std::chrono::steady_clock::now();
        dumpProbabilities(probResult, opt.getOutputFileDir());
        auto t5 = std::chrono::duration_cast<std::chrono::milliseconds>(
                          std::chrono::steady_clock::now() - t4)
                          .count();
        std::cout << "[pipeline] probability dump took " << t5 << " ms\n";
        debugger.endStage();
    }

    debugger.endTurn();
    dumpInitialInputRelations(opt.getOutputFileDir() + "/initial-input-relations-iter0.txt");

    if (enableOnlineCli) {
        IncrementalCLI<SddNodeRef> cli(
                &program, &graph, &ruleManager, &sddManager, &nodeFormulas, &edgeFormulas);
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

    graph->dumpDot("before_prune.dot");

    debugger.startStage(StageKind::PRUNING_FULL);
    auto t2 = std::chrono::steady_clock::now();
    auto view = graph->prune(program.getOutputRelations());
    auto t3 = std::chrono::steady_clock::now();
    std::cout << "[pipeline] pruning took "
              << std::chrono::duration_cast<std::chrono::milliseconds>(t3 - t2).count()
              << " ms\n";
    debugger.endStage();

    view.dumpDot("after_prune.dot");
    view.dumpJson("derivation.json");
    if (opt.isRewriteEnabled()) {
        debugger.startStage(StageKind::PRECONFIG_FULL);
        auto rewriteStart = std::chrono::steady_clock::now();
        GraphRewriter rewriter;
        auto rewriteStats = rewriter.rewriteUntilFixpoint(*graph, view, opt.isProfiling());
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
        view.dumpDot("rewrite_final.dot");
        debugger.endStage();
    }

    if (program.getKnowledge() == souffle::Knowledge::BDD) {
        runBddPipeline(opt, program, ruleManager, *graph, view, evidences, enableOnlineCli);
    } else if (program.getKnowledge() == souffle::Knowledge::SDD) {
        runSddPipeline(opt, program, ruleManager, *graph, view, enableOnlineCli);
    } else {
        std::cerr << "Unknown knowledge representation" << std::endl;
    }
}

}  // namespace souffle::problog
