#ifndef SOUFFLE_CLI_EXECUTOR_H
#define SOUFFLE_CLI_EXECUTOR_H

#include "souffle/cli/Command.h"
#include "souffle/cli/PendingOperation.h"

template <typename NodeRef>
class IncrementalCLI;

namespace souffle::cli {

template <typename NodeRef>
class IncrementalCommandExecutor {
public:
    using Cli = ::IncrementalCLI<NodeRef>;
    using ParsedCommand = souffle::cli::ParsedCommand;
    using CommandKind = souffle::cli::CommandKind;

    explicit IncrementalCommandExecutor(Cli& cli) : cli(cli) {}

    bool execute(const ParsedCommand& command) {
        switch (command.kind) {
            case CommandKind::HELP:
                cli.printHelp();
                return true;
            case CommandKind::INSERT:
                cli.handleInsertCommand(command);
                return true;
            case CommandKind::DELETE:
                cli.handleDeleteCommand(command);
                return true;
            case CommandKind::LIST:
                cli.handleListCommand();
                return true;
            case CommandKind::SETMODE:
                cli.handleSetModeCommand(command);
                return true;
            case CommandKind::SET:
                cli.handleSetCommand(command);
                return true;
            case CommandKind::UNSET:
                cli.handleUnsetCommand(command);
                return true;
            case CommandKind::SHOW:
                cli.handleShowCommand(command);
                return true;
            case CommandKind::COMMIT:
                handleCommitCommand();
                return true;
            case CommandKind::DUMP:
                cli.handleDumpCommand();
                return true;
            case CommandKind::EXIT:
                std::cout << "PARSED EXIT: Exiting CLI" << std::endl;
                return false;
            case CommandKind::UNKNOWN:
                std::cout << "Unknown command: " << command.verb << std::endl;
                std::cout << "Use 'help' to see available commands" << std::endl;
                return true;
        }
        return true;
    }

    void commit() {
        DerivationManager::untypedTuple2DeltaInsertRuleApplications.clear();
        DerivationManager::untypedTuple2DeltaDeleteRuleApplications.clear();
        DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications.clear();
        DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications.clear();
        if (cli.program == nullptr) {
            assert(false && "No program loaded.");
        }

        cli.assertCurrentModeCompatibleWithGraphState();
        if (cli.isElasticFcMode()) {
            std::cout << "[cli] fc-mode elastic currently falls back to inc-naive" << std::endl;
        }
        cli.stagePendingOperationsForProgram();
        dumpInitialInputRelations(
                cli.opt.getOutputFileDir() + "/initial-input-relations-iter" +
                std::to_string(cli.iteration) + ".txt");

        if (cli.isIncrementalSemMode()) {
            handleIncrementalSemCommit();
        } else if (cli.isFullSemMode()) {
            handleFullSemCommit();
        } else {
            assert(false && "Unsupported online mode");
        }

        cli.pendingOperations.clear();
    }

private:
    static void stabilizeFactSemanticIds(
            const IncrementalDerivationGraph& oldGraph, IncrementalDerivationGraph& newGraph) {
        std::unordered_map<UntypedTuple, size_t> oldFactSemanticIds;
        oldFactSemanticIds.reserve(oldGraph.getNodes().size());
        size_t nextSemanticId = 0;
        for (const auto& node : oldGraph.getNodes()) {
            if (!node || !node->isFact) {
                continue;
            }
            oldFactSemanticIds.emplace(node->getTuple(), node->getSemanticFactId());
            nextSemanticId = std::max(nextSemanticId, node->getSemanticFactId() + 1);
        }

        std::vector<NodePtr> newFactNodes;
        newFactNodes.reserve(newGraph.getNodes().size());
        for (const auto& node : newGraph.getNodes()) {
            if (node && node->isFact) {
                newFactNodes.push_back(node);
            }
        }
        std::sort(newFactNodes.begin(), newFactNodes.end(),
                [](const NodePtr& lhs, const NodePtr& rhs) {
                    return lhs->getTuple() < rhs->getTuple();
                });

        for (const auto& node : newFactNodes) {
            auto it = oldFactSemanticIds.find(node->getTuple());
            if (it != oldFactSemanticIds.end()) {
                node->setSemanticFactId(it->second);
            } else {
                node->setSemanticFactId(nextSemanticId++);
            }
        }
    }

    void handleCommitCommand() {
        std::cout << "PARSED COMMIT: Would apply " << cli.pendingOperations.size()
                  << " pending changes and run incremental computation" << std::endl;
        commit();
    }

    void handleIncrementalSemCommit() {
        const bool useRegional = cli.isRegionalFcMode();
        Debugger& debugger = Debugger::getInstance();
        {
            bool hasDelete = false;
            bool hasInsert = false;
            for (const auto& op : cli.pendingOperations) {
                if (!op.valid) {
                    continue;
                }
                if (op.type == Cli::Operation::DELETE) {
                    hasDelete = true;
                } else if (op.type == Cli::Operation::INSERT) {
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
            cli.beginTurnTrace();
            debugger.startStage(StageKind::SEMINAIVE_INC);
            cli.program->runAllInc(cli.program->getInputDirectory(), cli.program->getOutputDirectory(), true);
            debugger.endStage();
            if (DerivationManager::isSemStatsEnabled()) {
                std::ostringstream label;
                label << "iter=" << cli.iteration << " phase=" << phaseLabel;
                DerivationManager::dumpDredStats(std::cout, label.str());
            }
            if (cli.opt.isDredProfileEnabled()) {
                std::cout << "[dred-debug] relation sizes after SEMINAIVE_INC:\n";
                const std::array<std::string, 4> prefixes = {
                        "$inc_delta_derv_delete_",
                        "$inc_delta_tuple_delete_",
                        "$inc_derv_overdelete_",
                        "$inc_tuple_overdelete_",
                };
                for (auto* rel : cli.program->getAllRelations()) {
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
        DerivationGraphViewInterface::setDumpOutputDir(cli.opt.getOutputFileDir());
        debugger.startStage(StageKind::PRUNING_INC);
        if (cli.opt.isDumpStatEnabled()) {
            std::cout << "[prune-inc] pre-applyDelta statistics:\n";
            cli.graph->dumpStatisticsInc(std::cout);
        }
        auto factProbInc = cli.getFactProbInc();
        auto deletedFacts = cli.getDeletedFacts(&factProbInc);
        {
            FunctionTimer timer("PRUNING_INC: applyDelta");
            cli.graph->applyDelta(
                    DerivationManager::untypedTuple2DeltaInsertRuleApplications,
                    DerivationManager::untypedTuple2DeltaDeleteRuleApplications,
                    *cli.ruleManager, factProbInc, deletedFacts);
        }
        DerivationManager::clearDetDeltaTuples();
        cli.logApplyDeltaOpsSummary(
                DerivationManager::untypedTuple2DeltaInsertRuleApplications,
                DerivationManager::untypedTuple2DeltaDeleteRuleApplications, factProbInc, deletedFacts,
                useRegional ? "INC_REGIONAL" : "INC_NAIVE");
        cli.logApplyDeltaSummary(*cli.graph, useRegional ? "INC_REGIONAL" : "INC_NAIVE");
        cli.logApplyDeltaGraphSummary(*cli.graph, useRegional ? "INC_REGIONAL" : "INC_NAIVE");
        if (cli.opt.isDumpDotEnabled()) {
            FunctionTimer timer("PRUNING_INC: dumpDot-before-prune");
            cli.graph->dumpDotInc(
                    cli.outputPath("derivation-inc-before-prune" + std::to_string(cli.iteration) + ".dot"));
        }
        IncSubgraphView view = [&] {
            FunctionTimer timer("PRUNING_INC: prune");
            DerivationGraph::setMergeBiImpEnabled(false);
            cli.graph->setBuildInsertImpacts(useRegional);
            return cli.graph->prune(cli.program->getOutputRelations());
        }();
        if (cli.opt.isDumpDotEnabled()) {
            FunctionTimer timer("PRUNING_INC: dumpDot-after-prune");
            view.dumpDotInc(cli.outputPath(
                    "derivation-inc-after-prune" + std::to_string(cli.iteration) + ".dot"));
        }
        if (cli.opt.isDumpJsonEnabled()) {
            FunctionTimer timer("PRUNING_INC: dumpJson-after-prune");
            view.dumpJsonInc(
                    cli.outputTimestampedPath("derivation-inc-after-prune", cli.iteration, ".json"));
        }
        debugger.endStage();
        cli.logPrunedDeltaSummary(view, useRegional ? "INC_REGIONAL" : "INC_NAIVE");
        cli.changedNodes.clear();
        if (cli.derivationOnly) {
            cli.finishTurn();
            return;
        }
        if (cli.isIncrementalFcMode()) {
            debugger.startStage(StageKind::FORWARD_COMPILATION_INC);
            if (useRegional) {
                buildFormulasIncRegionalCyclewise(
                        view, *cli.ddManager, *cli.nodeFormulas, *cli.edgeFormulas, cli.changedNodes);
            } else {
                buildFormulasIncCyclewise(
                        view, *cli.ddManager, *cli.nodeFormulas, *cli.edgeFormulas, cli.changedNodes);
            }
            debugger.endStage();
            cli.runIncrementalWmc(view, useRegional);
            cli.dumpCurrentTurnProbabilities();
        } else if (cli.isFullFcMode()) {
            debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
            cli.nodeFormulas->clear();
            cli.edgeFormulas->clear();
            if (cli.modeSpec.fc == FcMode::FULL_HARD) {
                cli.ddManager->resetHard();
            } else {
                cli.ddManager->reset();
            }
            buildFormulasCyclewise(view, *cli.ddManager, *cli.nodeFormulas, *cli.edgeFormulas);
            debugger.endStage();

            debugger.startStage(StageKind::WEIGHTED_MODEL_COUNTING_FULL);
            cli.runFullWeightedModelCounting(view, souffle::incrementalFcProfileModeLabel(cli.modeSpec.fc));
            debugger.endStage();
            cli.dumpCurrentTurnProbabilitiesWithStage(StageKind::IO_DUMP_FULL);
        } else {
            assert(false && "Unsupported fc mode for sem=inc");
        }
        cli.finishTurn();
    }

    void handleFullSemCommit() {
        const bool useIncFc = cli.isIncrementalFcMode();
        const bool useRegionalFc = (cli.modeSpec.fc == FcMode::INC_REGIONAL);
        Debugger& debugger = Debugger::getInstance();
        IncrementalDerivationGraph* oldGraph = cli.graph;
        std::unique_ptr<IncSubgraphView> oldPrunedView;
        if (useIncFc && cli.graph != nullptr) {
            DerivationGraph::setMergeBiImpEnabled(false);
            oldPrunedView = std::make_unique<IncSubgraphView>(cli.graph->prune(cli.program->getOutputRelations()));
        }

        cli.beginTurnTrace();
        if (cli.ddManager != nullptr && !useIncFc) {
            cli.nodeFormulas->clear();
            cli.edgeFormulas->clear();
            if (cli.modeSpec.fc == FcMode::FULL_HARD) {
                cli.ddManager->resetHard();
            } else {
                cli.ddManager->reset();
            }
        }

        cli.purgeAllRelations();
        cli.loadInitialInputRelations();
        DerivationManager::untypedTuple2RuleApplications.clear();
        debugger.startStage(StageKind::SEMINAIVE_FULL);
        cli.program->runAll(cli.opt.getInputFileDir(), cli.opt.getOutputFileDir(), false);
        std::vector<std::pair<UntypedTuple, bool>> evidenceList;
        if (cli.graph) {
            evidenceList = cli.graph->getEvidences();
        }
        cli.graph = IncrementalDerivationGraph::createFrom(
                DerivationManager::untypedTuple2RuleApplications, *cli.ruleManager, *cli.queryManager,
                fact_prob, evidenceList);
        if (useIncFc && oldGraph != nullptr) {
            stabilizeFactSemanticIds(*oldGraph, *cli.graph);
        }
        debugger.endStage();
        if (cli.opt.isDumpDotEnabled()) {
            FunctionTimer timer("PRUNING_FULL: dumpDot-before-prune");
            cli.graph->dumpDotInc(
                    cli.outputPath("derivation-full-before-prune" + std::to_string(cli.iteration) + ".dot"));
        }
        debugger.startStage(StageKind::PRUNING_FULL);
        IncSubgraphView view = [&] {
            FunctionTimer timer("PRUNING_FULL: prune");
            DerivationGraph::setMergeBiImpEnabled(false);
            return cli.graph->prune(cli.program->getOutputRelations());
        }();
        if (cli.opt.isDumpDotEnabled()) {
            FunctionTimer timer("PRUNING_FULL: dumpDot-after-prune");
            view.dumpDotInc(
                    cli.outputPath("derivation-full-after-prune" + std::to_string(cli.iteration) + ".dot"));
        }
        if (cli.opt.isDumpJsonEnabled()) {
            FunctionTimer timer("PRUNING_FULL: dumpJson-after-prune");
            view.dumpJsonInc(
                    cli.outputTimestampedPath("derivation-full-after-prune", cli.iteration, ".json"));
        }
        debugger.endStage();
        if (cli.derivationOnly) {
            cli.finishTurn();
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
                        cli.buildPostPruneDiffView(*oldPrunedView, view, oldToNewNodes, oldToNewEdges));
                cli.remapStateForPostPruneDiff(oldToNewNodes, oldToNewEdges);
            }
            cli.changedNodes.clear();
            debugger.startStage(StageKind::FORWARD_COMPILATION_INC);
            if (useRegionalFc) {
                buildFormulasIncRegionalCyclewise(
                        *diffView, *cli.ddManager, *cli.nodeFormulas, *cli.edgeFormulas, cli.changedNodes);
            } else {
                buildFormulasIncCyclewise(
                        *diffView, *cli.ddManager, *cli.nodeFormulas, *cli.edgeFormulas, cli.changedNodes);
            }
            debugger.endStage();
            activeView = diffView.get();
            cli.logPrunedDeltaSummary(*diffView, useRegionalFc ? "INC_REGIONAL" : "INC_NAIVE");
        } else {
            debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
            cli.nodeFormulas->clear();
            cli.edgeFormulas->clear();
            buildFormulasCyclewise(view, *cli.ddManager, *cli.nodeFormulas, *cli.edgeFormulas);
            debugger.endStage();
        }

        if (useIncFc) {
            cli.runIncrementalWmc(*activeView, useRegionalFc);
        } else {
            debugger.startStage(StageKind::WEIGHTED_MODEL_COUNTING_FULL);
            cli.runFullWeightedModelCounting(
                    *activeView,
                    souffle::fullSemProfileModeLabel(useIncFc ? cli.modeSpec.fc : FcMode::FULL_HARD));
            debugger.endStage();
        }
        cli.dumpCurrentTurnProbabilitiesWithStage(StageKind::IO_DUMP_FULL);
        cli.finishTurn();
    }

    Cli& cli;
};

}  // namespace souffle::cli

#endif
