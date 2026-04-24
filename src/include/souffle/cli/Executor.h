#ifndef SOUFFLE_CLI_EXECUTOR_H
#define SOUFFLE_CLI_EXECUTOR_H

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <vector>

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
                return false;
            case CommandKind::UNKNOWN:
                std::cout << "Unknown command: " << command.verb << std::endl;
                std::cout << "Use 'help' to see available commands" << std::endl;
                return true;
        }
        return true;
    }

    void commit() {
        DerivationManager::freeRuleApplicationMap(
                DerivationManager::untypedTuple2DeltaInsertRuleApplications);
        DerivationManager::freeRuleApplicationMap(
                DerivationManager::untypedTuple2DeltaDeleteRuleApplications);
        DerivationManager::freeRuleApplicationMap(
                DerivationManager::untypedTuple2DeltaDeltaInsertRuleApplications);
        DerivationManager::freeRuleApplicationMap(
                DerivationManager::untypedTuple2DeltaDeltaDeleteRuleApplications);
        if (cli.program == nullptr) {
            assert(false && "No program loaded.");
        }

        const IncrementalModeSpec requestedMode = cli.modeSpec;
        const IncrementalModeSpec effectiveMode = cli.resolveCommittedModeSpec();
        cli.logCommittedModeResolution(requestedMode, effectiveMode);
        cli.assertModeCompatibleWithGraphState(effectiveMode);
        struct ModeRestoreGuard {
            Cli& cli;
            IncrementalModeSpec saved;
            ~ModeRestoreGuard() { cli.modeSpec = saved; }
        } modeRestore{cli, requestedMode};
        cli.modeSpec = effectiveMode;

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

        cli.updateFcStateAfterTurn(requestedMode, effectiveMode);

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
        commit();
    }

    void handleIncrementalSemCommit() {
        const bool useRegional = cli.isRegionalFcMode();
        Debugger& debugger = Debugger::getInstance();
        const bool memProbe = std::getenv("SOUFFLE_MEM_PROBE") != nullptr;
        std::size_t maxTurnRssKb = 0;
        auto readStatusValueKb = [](const char* key) -> std::size_t {
            std::ifstream status("/proc/self/status");
            std::string line;
            while (std::getline(status, line)) {
                if (line.rfind(key, 0) == 0) {
                    std::istringstream iss(line);
                    std::string label, value, unit;
                    iss >> label >> value >> unit;
                    return static_cast<std::size_t>(std::stoull(value));
                }
            }
            return 0;
        };
        auto dumpMemProbe = [&](const std::string& label) {
            if (!memProbe) {
                return;
            }
            const std::size_t rssKb = readStatusValueKb("VmRSS:");
            const std::size_t hwmKb = readStatusValueKb("VmHWM:");
            maxTurnRssKb = std::max(maxTurnRssKb, rssKb);
            std::cout << "[mem-probe] " << label
                      << " rss_kb=" << rssKb
                      << " hwm_kb=" << hwmKb
                      << " complete_tuples="
                      << DerivationManager::countRuleApplicationTuples(
                                 DerivationManager::untypedTuple2RuleApplications)
                      << " complete_ruleapps="
                      << DerivationManager::countRuleApplications(
                                 DerivationManager::untypedTuple2RuleApplications)
                      << " delta_insert_tuples="
                      << DerivationManager::countRuleApplicationTuples(
                                 DerivationManager::untypedTuple2DeltaInsertRuleApplications)
                      << " delta_insert_ruleapps="
                      << DerivationManager::countRuleApplications(
                                 DerivationManager::untypedTuple2DeltaInsertRuleApplications)
                      << " delta_delete_tuples="
                      << DerivationManager::countRuleApplicationTuples(
                                 DerivationManager::untypedTuple2DeltaDeleteRuleApplications)
                      << " delta_delete_ruleapps="
                      << DerivationManager::countRuleApplications(
                                 DerivationManager::untypedTuple2DeltaDeleteRuleApplications)
                      << std::endl;
        };
        auto dumpRelationSummary = [&](const std::string& label) {
            if (!memProbe) {
                return;
            }
            std::size_t totalTuples = 0;
            std::vector<std::pair<std::size_t, std::string>> relationSizes;
            relationSizes.reserve(cli.program->getAllRelations().size());
            for (auto* rel : cli.program->getAllRelations()) {
                const std::size_t sz = rel->size();
                totalTuples += sz;
                relationSizes.emplace_back(sz, rel->getName());
            }
            std::sort(relationSizes.begin(), relationSizes.end(),
                    [](const auto& a, const auto& b) { return a.first > b.first; });
            std::cout << "[mem-probe] " << label << " relation_total_tuples=" << totalTuples;
            const std::size_t limit = std::min<std::size_t>(relationSizes.size(), 8);
            for (std::size_t i = 0; i < limit; ++i) {
                std::cout << " top_rel_" << i << "=" << relationSizes[i].second << ":" << relationSizes[i].first;
            }
            std::cout << std::endl;
        };
        {
            auto emitDredInfo = [&]() {
                const auto& stats = DerivationManager::dredStats;
                debugger.addInfo("dred_del_ruleapp_recorded", std::to_string(stats.del_ruleapp_recorded));
                debugger.addInfo("dred_del_ruleapp_delta_delta", std::to_string(stats.del_ruleapp_delta_delta));
                debugger.addInfo("dred_del_ruleapp_overdelete", std::to_string(stats.del_ruleapp_overdelete));
                debugger.addInfo("dred_del_complete_scan_calls", std::to_string(stats.del_complete_scan_calls));
                debugger.addInfo("dred_del_complete_scan_elems", std::to_string(stats.del_complete_scan_elems));
                debugger.addInfo("dred_del_delta_tuples", std::to_string(stats.del_delta_tuples));
                debugger.addInfo("dred_del_delta_ruleapps", std::to_string(stats.del_delta_ruleapps));
                debugger.addInfo("dred_del_ruleapp_erases", std::to_string(stats.del_ruleapp_erases));
                debugger.addInfo("dred_del_tuple_deletes", std::to_string(stats.del_tuple_deletes));
                debugger.addInfo("dred_ins_ruleapp_recorded", std::to_string(stats.ins_ruleapp_recorded));
                debugger.addInfo("dred_ins_ruleapp_delta_delta", std::to_string(stats.ins_ruleapp_delta_delta));
                debugger.addInfo(
                        "dred_ins_ruleapp_rederive_erases", std::to_string(stats.ins_ruleapp_rederive_erases));
                debugger.addInfo("dred_ins_delta_tuples", std::to_string(stats.ins_delta_tuples));
                debugger.addInfo("dred_ins_delta_ruleapps", std::to_string(stats.ins_delta_ruleapps));
                debugger.addInfo("dred_rederive_delta_tuples", std::to_string(stats.rederive_delta_tuples));
                debugger.addInfo("dred_rederive_delta_ruleapps", std::to_string(stats.rederive_delta_ruleapps));
                debugger.addInfo("dred_ins_ruleapp_merges", std::to_string(stats.ins_ruleapp_merges));
                debugger.addInfo("dred_ins_tuple_inserts", std::to_string(stats.ins_tuple_inserts));
                debugger.addInfo("dred_del_time_total_ns", std::to_string(stats.del_time_total_ns));
                debugger.addInfo("dred_ins_time_total_ns", std::to_string(stats.ins_time_total_ns));
                debugger.addInfo("dred_red_time_total_ns", std::to_string(stats.red_time_total_ns));
            };
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
            DerivationManager::resetDredStats();
            DerivationManager::clearDetDeltaTuples();
            cli.beginTurnTrace();
            dumpMemProbe("inc_turn_before_runAllInc");
            debugger.startStage(StageKind::SEMINAIVE_INC);
            cli.program->runAllInc(cli.program->getInputDirectory(), cli.program->getOutputDirectory(), true);
            debugger.addInfo("dred_phase", phaseLabel);
            emitDredInfo();
            debugger.endStage();
            dumpMemProbe("inc_turn_after_runAllInc");
            dumpRelationSummary("inc_turn_after_runAllInc");
            if (DerivationManager::isSemStatsEnabled()) {
                std::ostringstream label;
                label << "iter=" << cli.iteration << " phase=" << phaseLabel;
                DerivationManager::dumpRuleApplicationSummary(std::cout, label.str());
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
        if (memProbe && cli.graph != nullptr) {
            std::cout << "[mem-probe] inc_turn_after_applyDelta graph_nodes="
                      << cli.graph->getNodes().size()
                      << " graph_edges=" << cli.graph->getEdges().size() << std::endl;
        }
        dumpMemProbe("inc_turn_after_applyDelta");
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
        if (cli.opt.isDumpJsonBeforePruneEnabled()) {
            FunctionTimer timer("PRUNING_INC: dumpJson-before-prune");
            cli.graph->dumpJsonInc(
                    cli.outputTimestampedPath("derivation-inc-before-prune", cli.iteration, ".json"));
        }
        IncSubgraphView view = [&] {
            FunctionTimer timer("PRUNING_INC: prune");
            cli.graph->setBuildInsertImpacts(useRegional);
            return cli.graph->prune(cli.program->getOutputRelations());
        }();
        if (memProbe) {
            std::cout << "[mem-probe] inc_turn_after_prune view_nodes="
                      << view.getNodes().size()
                      << " view_edges=" << view.getEdges().size() << std::endl;
        }
        dumpMemProbe("inc_turn_after_prune");
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
        if (cli.ddManager != nullptr) {
            std::set<int> deletedVarsIndex;
            std::set<int> insertedVarsIndex;
            std::size_t insertedFactVars = 0;
            std::size_t insertedEdgeVars = 0;

            for (const auto& node : view.getDeltaDeleteNodes()) {
                if (!node || !node->isFact || node->getProbability() == 1.0) {
                    continue;
                }
                deletedVarsIndex.insert(cli.ddManager->getVarIndex(*node));
            }
            for (const auto& edge : view.getDeltaDeleteEdges()) {
                if (!edge || edge->isDeterministic()) {
                    continue;
                }
                deletedVarsIndex.insert(cli.ddManager->getVarIndex(*edge));
            }

            for (const auto& node : view.getDeltaInsertFactNodes()) {
                if (!node || node->getProbability() == 1.0) {
                    continue;
                }
                if (insertedVarsIndex.insert(cli.ddManager->getVarIndex(*node)).second) {
                    insertedFactVars++;
                }
            }
            for (const auto& edge : view.getDeltaInsertEdges()) {
                if (!edge || edge->isDeterministic()) {
                    continue;
                }
                if (insertedVarsIndex.insert(cli.ddManager->getVarIndex(*edge)).second) {
                    insertedEdgeVars++;
                }
            }

            debugger.addInfo("fc_deleted_random_vars", std::to_string(deletedVarsIndex.size()));
            debugger.addInfo("fc_inserted_random_vars", std::to_string(insertedVarsIndex.size()));
            debugger.addInfo("fc_inserted_fact_random_vars", std::to_string(insertedFactVars));
            debugger.addInfo("fc_inserted_edge_random_vars", std::to_string(insertedEdgeVars));
        }
        debugger.endStage();
        cli.logPrunedDeltaSummary(view, useRegional ? "INC_REGIONAL" : "INC_NAIVE");
        cli.changedNodes.clear();
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
            dumpMemProbe("inc_turn_after_fc");
            cli.runIncrementalWmc(view, useRegional);
            dumpMemProbe("inc_turn_after_wmc");
            cli.dumpCurrentTurnProbabilities();
            dumpMemProbe("inc_turn_after_dump");
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
            assert(false && "Unsupported incremental turn mode");
        }
        if (memProbe) {
            std::cout << "[mem-probe] inc_turn_peak_rss_kb=" << maxTurnRssKb << std::endl;
        }
        cli.finishTurn();
    }

    void handleFullSemCommit() {
        const bool useIncFc = cli.isIncrementalFcMode();
        const bool useRegionalFc = (cli.modeSpec.fc == FcMode::INC_REGIONAL);
        Debugger& debugger = Debugger::getInstance();
        const bool memProbe = std::getenv("SOUFFLE_MEM_PROBE") != nullptr;
        auto readStatusValueKb = [](const char* key) -> std::size_t {
            std::ifstream status("/proc/self/status");
            std::string line;
            while (std::getline(status, line)) {
                if (line.rfind(key, 0) == 0) {
                    std::istringstream iss(line);
                    std::string label, value, unit;
                    iss >> label >> value >> unit;
                    return static_cast<std::size_t>(std::stoull(value));
                }
            }
            return 0;
        };
        auto dumpMemProbe = [&](const std::string& label) {
            if (!memProbe) {
                return;
            }
            std::cout << "[mem-probe] " << label
                      << " rss_kb=" << readStatusValueKb("VmRSS:")
                      << " hwm_kb=" << readStatusValueKb("VmHWM:")
                      << " complete_tuples="
                      << DerivationManager::countRuleApplicationTuples(
                                 DerivationManager::untypedTuple2RuleApplications)
                      << " complete_ruleapps="
                      << DerivationManager::countRuleApplications(
                                 DerivationManager::untypedTuple2RuleApplications)
                      << std::endl;
        };
        auto dumpRelationSummary = [&](const std::string& label) {
            if (!memProbe) {
                return;
            }
            std::size_t totalTuples = 0;
            std::vector<std::pair<std::size_t, std::string>> relationSizes;
            relationSizes.reserve(cli.program->getAllRelations().size());
            for (auto* rel : cli.program->getAllRelations()) {
                const std::size_t sz = rel->size();
                totalTuples += sz;
                relationSizes.emplace_back(sz, rel->getName());
            }
            std::sort(relationSizes.begin(), relationSizes.end(),
                    [](const auto& a, const auto& b) { return a.first > b.first; });
            std::cout << "[mem-probe] " << label << " relation_total_tuples=" << totalTuples;
            const std::size_t limit = std::min<std::size_t>(relationSizes.size(), 8);
            for (std::size_t i = 0; i < limit; ++i) {
                std::cout << " top_rel_" << i << "=" << relationSizes[i].second << ":" << relationSizes[i].first;
            }
            std::cout << std::endl;
        };
        IncrementalDerivationGraph* oldGraph = cli.graph;
        std::unique_ptr<IncSubgraphView> oldPrunedView;
        std::vector<std::pair<UntypedTuple, bool>> evidenceList;
        if (cli.graph) {
            evidenceList = cli.graph->getEvidences();
        }
        if (useIncFc && cli.graph != nullptr) {
            oldPrunedView = std::make_unique<IncSubgraphView>(cli.graph->prune(cli.program->getOutputRelations()));
        } else if (cli.graphOwner != nullptr && cli.graphOwner->get() != nullptr) {
            cli.graphOwner->reset();
            cli.graph = nullptr;
            dumpMemProbe("full_turn2_after_free_old_graph");
        }

        cli.beginTurnTrace();
        dumpMemProbe("full_turn2_after_begin_turn_trace");
        if (cli.ddManager != nullptr && !useIncFc) {
            cli.nodeFormulas->clear();
            dumpMemProbe("full_turn2_after_clear_node_formulas");
            cli.edgeFormulas->clear();
            dumpMemProbe("full_turn2_after_clear_edge_formulas");
            if (cli.modeSpec.fc == FcMode::FULL_HARD) {
                cli.ddManager->resetHard();
                dumpMemProbe("full_turn2_after_reset_hard");
            } else {
                cli.ddManager->reset();
                dumpMemProbe("full_turn2_after_reset_soft");
            }
        }
        cli.purgeAllRelations();
        dumpMemProbe("full_turn2_after_purge_relations");
        cli.loadInitialInputRelations();
        dumpMemProbe("full_turn2_after_reload_inputs");
        DerivationManager::freeRuleApplicationMap(DerivationManager::untypedTuple2RuleApplications);
        dumpMemProbe("full_turn2_after_free_complete_ruleapps");
        debugger.startStage(StageKind::SEMINAIVE_FULL);
        cli.program->runAll(cli.opt.getInputFileDir(), cli.opt.getOutputFileDir(), false);
        dumpMemProbe("full_turn2_after_runAll");
        dumpRelationSummary("full_turn2_after_runAll");
        auto newGraph = std::unique_ptr<IncrementalDerivationGraph>(IncrementalDerivationGraph::createFrom(
                DerivationManager::untypedTuple2RuleApplications, *cli.ruleManager, *cli.queryManager,
                fact_prob, evidenceList));
        if (memProbe && newGraph != nullptr) {
            std::cout << "[mem-probe] full_turn2_after_createFrom graph_nodes="
                      << newGraph->getNodes().size()
                      << " graph_edges=" << newGraph->getEdges().size() << std::endl;
        }
        dumpMemProbe("full_turn2_after_createFrom");
        if (useIncFc && oldGraph != nullptr && newGraph != nullptr) {
            stabilizeFactSemanticIds(*oldGraph, *newGraph);
        }
        cli.replaceGraph(std::move(newGraph));
        dumpMemProbe("full_turn2_after_replace_graph");
        debugger.endStage();
        if (cli.opt.isDumpDotEnabled()) {
            FunctionTimer timer("PRUNING_FULL: dumpDot-before-prune");
            cli.graph->dumpDotInc(
                    cli.outputPath("derivation-full-before-prune" + std::to_string(cli.iteration) + ".dot"));
        }
        if (cli.opt.isDumpJsonBeforePruneEnabled()) {
            FunctionTimer timer("PRUNING_FULL: dumpJson-before-prune");
            cli.graph->dumpJsonInc(
                    cli.outputTimestampedPath("derivation-full-before-prune", cli.iteration, ".json"));
        }
        debugger.startStage(StageKind::PRUNING_FULL);
        IncSubgraphView view = [&] {
            FunctionTimer timer("PRUNING_FULL: prune");
            return cli.graph->prune(cli.program->getOutputRelations());
        }();
        if (memProbe) {
            std::cout << "[mem-probe] full_turn2_after_prune view_nodes="
                      << view.getNodes().size()
                      << " view_edges=" << view.getEdges().size() << std::endl;
        }
        dumpMemProbe("full_turn2_after_prune");
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

        std::unique_ptr<IncSubgraphView> diffView;
        IncSubgraphView* activeView = &view;
        if (useIncFc) {
            if (!oldPrunedView) {
                assert(false && "Full refresh with incremental FC requires existing previous pruned view");
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
