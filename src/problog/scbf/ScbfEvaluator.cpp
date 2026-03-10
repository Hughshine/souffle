#include "souffle/problog/scbf/ScbfEvaluator.h"

#ifdef SOUFFLE_SCBF_EVAL_BUNDLE_REWRITE
#ifndef SOUFFLE_SCBF_REWRITE_BUNDLE_FORMULA
#define SOUFFLE_SCBF_REWRITE_BUNDLE_FORMULA 1
#endif
#include "problog/scbf/ScbfFormulaRewriter.cpp"
#elif defined(SOUFFLE_SCBF_EVAL_BUNDLE_FORMULA)
#ifndef SOUFFLE_SCBF_FORMULA_BUNDLE_IR
#define SOUFFLE_SCBF_FORMULA_BUNDLE_IR 1
#endif
#include "problog/scbf/ScbfFormula.cpp"
#elif defined(SOUFFLE_SCBF_EVAL_BUNDLE_IR)
#include "problog/scbf/ScbfIr.cpp"
#endif

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace souffle::problog::scbf {
namespace {

double evalClamp01(double x) {
    if (x < 0.0) {
        return 0.0;
    }
    if (x > 1.0) {
        return 1.0;
    }
    return x;
}

double absDiff(double a, double b) {
    return std::abs(a - b);
}

std::string nodeLabel(const NodePtr& node) {
    return node ? node->getTuple().toString() + "#" + std::to_string(node->getId()) : "<null>";
}

std::string evalSanitizePathToken(const std::string& token) {
    std::string out;
    out.reserve(token.size());
    for (char c : token) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' ||
                c == '-' || c == '.') {
            out.push_back(c);
        } else {
            out.push_back('_');
        }
    }
    return out.empty() ? "scbf_eval" : out;
}

void dumpEvalBundleIfEnabled(const ScbfStratumFormulaBundle& bundle, const ScbfEvaluatorConfig& cfg,
        const std::string& phaseTag) {
    if (!cfg.dumpFormulaBundle || cfg.dumpDir.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(cfg.dumpDir, ec);
    const std::string prefix = evalSanitizePathToken(cfg.dumpPrefix);
    const std::string stem = prefix + "_cycle" + std::to_string(bundle.cycleId) + "_topo" +
            std::to_string(bundle.topoIndex) + "_" + evalSanitizePathToken(phaseTag);
    dumpScbfStratumFormulaBundleJson(bundle, cfg.dumpDir + "/" + stem + ".json");
    dumpScbfStratumFormulaBundleDot(bundle, cfg.dumpDir + "/" + stem + ".dot");
}

struct TargetEvaluationResult {
    double probability = 0.0;
    std::size_t iterations = 0;
    double maxDelta = 0.0;
    bool converged = false;
    std::size_t missingImports = 0;
};

TargetEvaluationResult evaluateTargetFromPlan(const DerivationGraphViewInterface& view, const ScbfStratum& stratum,
        const ScbfTargetPlan& targetPlan,
        const std::unordered_map<NodePtr, double>& exportProbs, const ScbfEvaluatorConfig& cfg) {
    TargetEvaluationResult out;
    if (!targetPlan.target) {
        return out;
    }

    std::unordered_set<NodePtr> localNodeSet(stratum.localNodes.begin(), stratum.localNodes.end());
    std::unordered_map<NodePtr, double> nodeProb;
    nodeProb.reserve(stratum.localNodes.size());
    for (const auto& node : stratum.localNodes) {
        if (node && node->isFact) {
            nodeProb[node] = evalClamp01(node->getProbability());
        } else {
            nodeProb[node] = 0.0;
        }
    }

    for (std::size_t iter = 0; iter < cfg.maxIterations; ++iter) {
        std::unordered_map<NodePtr, double> edgeOrByOutput;
        edgeOrByOutput.reserve(stratum.localNodes.size());

        for (const auto& edge : stratum.localEdges) {
            if (!edge) {
                continue;
            }
            double edgeProb = edge->isDeterministic() ? 1.0 : evalClamp01(edge->getProbability());
            const auto inputs = view.getInputs(edge);
            const auto negs = view.getBodyNegations(edge);
            for (std::size_t i = 0; i < inputs.size(); ++i) {
                const auto& input = inputs[i];
                const bool neg = i < negs.size() ? negs[i] : false;
                double inputProb = 0.0;
                bool found = false;

                auto itLocal = localNodeSet.find(input);
                if (itLocal != localNodeSet.end()) {
                    auto itNode = nodeProb.find(input);
                    if (itNode != nodeProb.end()) {
                        inputProb = itNode->second;
                        found = true;
                    }
                } else {
                    auto itExport = exportProbs.find(input);
                    if (itExport != exportProbs.end()) {
                        inputProb = itExport->second;
                        found = true;
                    } else if (input && input->isFact) {
                        inputProb = evalClamp01(input->getProbability());
                        found = true;
                    }
                }

                if (!found) {
                    out.missingImports++;
                    if (cfg.failOnMissingImports) {
                        throw std::runtime_error(
                                "SCBF evaluator missing import for input " + nodeLabel(input) +
                                " while evaluating target " + nodeLabel(targetPlan.target));
                    }
                    inputProb = 0.0;
                }
                const double litProb = neg ? (1.0 - inputProb) : inputProb;
                edgeProb *= cfg.clampToUnitInterval ? evalClamp01(litProb) : litProb;
            }
            if (cfg.clampToUnitInterval) {
                edgeProb = evalClamp01(edgeProb);
            }
            NodePtr outNode = view.getOutput(edge);
            if (!outNode) {
                continue;
            }
            double prev = 0.0;
            auto itPrev = edgeOrByOutput.find(outNode);
            if (itPrev != edgeOrByOutput.end()) {
                prev = itPrev->second;
            }
            double combined = 1.0 - (1.0 - prev) * (1.0 - edgeProb);
            edgeOrByOutput[outNode] = cfg.clampToUnitInterval ? evalClamp01(combined) : combined;
        }

        double maxDelta = 0.0;
        std::unordered_map<NodePtr, double> next = nodeProb;
        for (const auto& node : stratum.localNodes) {
            if (!node) {
                continue;
            }
            double newProb = 0.0;
            if (node->isFact) {
                newProb = evalClamp01(node->getProbability());
            } else {
                auto it = edgeOrByOutput.find(node);
                if (it != edgeOrByOutput.end()) {
                    newProb = it->second;
                }
            }
            if (cfg.clampToUnitInterval) {
                newProb = evalClamp01(newProb);
            }
            maxDelta = std::max(maxDelta, absDiff(nodeProb[node], newProb));
            next[node] = newProb;
        }

        nodeProb.swap(next);
        out.iterations = iter + 1;
        out.maxDelta = maxDelta;
        if (maxDelta <= cfg.epsilon) {
            out.converged = true;
            break;
        }
    }

    auto it = nodeProb.find(targetPlan.target);
    out.probability = it == nodeProb.end() ? 0.0 : it->second;
    return out;
}

double resolveFormulaLiteralProbability(const ScbfTargetFormula& targetFormula, const ScbfLiteralRef& literal,
        const std::vector<double>& localNodeProbabilities,
        const std::unordered_map<NodePtr, double>& exportProbs, std::size_t& missingImports,
        const ScbfEvaluatorConfig& cfg, const NodePtr& targetNode) {
    double probability = 0.0;
    bool found = true;
    switch (literal.source) {
        case ScbfLiteralSource::LocalNode:
            if (literal.index < localNodeProbabilities.size()) {
                probability = localNodeProbabilities[literal.index];
            } else {
                found = false;
            }
            break;
        case ScbfLiteralSource::ImportNode:
            if (literal.index < targetFormula.imports.size()) {
                const NodePtr importNode = targetFormula.imports[literal.index].node;
                auto it = exportProbs.find(importNode);
                if (it != exportProbs.end()) {
                    probability = it->second;
                } else if (importNode && importNode->isFact) {
                    probability = evalClamp01(importNode->getProbability());
                } else {
                    found = false;
                }
            } else {
                found = false;
            }
            break;
        case ScbfLiteralSource::Constant:
            probability = literal.constantProbability;
            break;
    }

    if (!found) {
        missingImports++;
        if (cfg.failOnMissingImports) {
            throw std::runtime_error(
                    "SCBF evaluator missing import while evaluating target " + nodeLabel(targetNode));
        }
        probability = 0.0;
    }
    probability = cfg.clampToUnitInterval ? evalClamp01(probability) : probability;
    return literal.negated ? (1.0 - probability) : probability;
}

TargetEvaluationResult evaluateTargetFromFormula(const ScbfTargetFormula& targetFormula,
        const std::unordered_map<NodePtr, double>& exportProbs, const ScbfEvaluatorConfig& cfg) {
    TargetEvaluationResult out;
    if (!targetFormula.target || targetFormula.localNodes.empty() ||
            targetFormula.targetLocalIndex >= targetFormula.localNodes.size()) {
        return out;
    }

    std::vector<double> nodeProb(targetFormula.localNodes.size(), 0.0);
    for (std::size_t i = 0; i < targetFormula.localNodes.size(); ++i) {
        if (targetFormula.localNodes[i].isFact) {
            nodeProb[i] = cfg.clampToUnitInterval
                    ? evalClamp01(targetFormula.localNodes[i].factProbability)
                    : targetFormula.localNodes[i].factProbability;
        }
    }

    for (std::size_t iter = 0; iter < cfg.maxIterations; ++iter) {
        std::vector<double> next = nodeProb;
        double maxDelta = 0.0;

        for (std::size_t localIndex = 0; localIndex < targetFormula.localNodes.size(); ++localIndex) {
            const auto& localNode = targetFormula.localNodes[localIndex];
            if (localNode.isFact) {
                const double fixed = cfg.clampToUnitInterval ? evalClamp01(localNode.factProbability)
                                                             : localNode.factProbability;
                next[localIndex] = fixed;
                maxDelta = std::max(maxDelta, absDiff(nodeProb[localIndex], fixed));
                continue;
            }

            const auto& rules = targetFormula.localNodeRules[localIndex];
            double nodeNoisyOr = 0.0;
            for (const auto& rule : rules) {
                double ruleProb = rule.deterministic ? 1.0 : rule.edgeProbability;
                if (cfg.clampToUnitInterval) {
                    ruleProb = evalClamp01(ruleProb);
                }
                for (const auto& literal : rule.bodyLiterals) {
                    const double litProb = resolveFormulaLiteralProbability(
                            targetFormula, literal, nodeProb, exportProbs, out.missingImports, cfg,
                            targetFormula.target);
                    ruleProb *= cfg.clampToUnitInterval ? evalClamp01(litProb) : litProb;
                }
                if (cfg.clampToUnitInterval) {
                    ruleProb = evalClamp01(ruleProb);
                }
                nodeNoisyOr = 1.0 - (1.0 - nodeNoisyOr) * (1.0 - ruleProb);
            }
            if (cfg.clampToUnitInterval) {
                nodeNoisyOr = evalClamp01(nodeNoisyOr);
            }
            next[localIndex] = nodeNoisyOr;
            maxDelta = std::max(maxDelta, absDiff(nodeProb[localIndex], nodeNoisyOr));
        }

        nodeProb.swap(next);
        out.iterations = iter + 1;
        out.maxDelta = maxDelta;
        if (maxDelta <= cfg.epsilon) {
            out.converged = true;
            break;
        }
    }

    out.probability = nodeProb[targetFormula.targetLocalIndex];
    return out;
}

void appendTargetStats(ScbfEvaluatorResult& result, std::size_t cycleId, std::size_t targetIndex,
        const NodePtr& targetNode, const TargetEvaluationResult& tr) {
    ScbfEvaluatorTargetStats stats;
    stats.cycleId = cycleId;
    stats.targetIndex = targetIndex;
    stats.target = targetNode;
    stats.iterations = tr.iterations;
    stats.maxDelta = tr.maxDelta;
    stats.converged = tr.converged;
    stats.missingImports = tr.missingImports;
    result.targetStats.push_back(stats);

    result.targetsEvaluated++;
    result.missingImports += tr.missingImports;
    result.converged = result.converged && tr.converged;

    if (targetNode) {
        result.nodeProbabilities[targetNode] = tr.probability;
        result.exportProbabilities[targetNode] = tr.probability;
    }
}

}  // namespace

ScbfEvaluatorResult evaluateScbfProgramProbability(const DerivationGraphViewInterface& view,
        const ScbfProgram& program, const ScbfEvaluatorConfig& config) {
    ScbfEvaluatorResult result;
    for (const auto cycleId : program.topoOrderCycleIds) {
        auto itTopo = program.cycleIdToTopoIndex.find(cycleId);
        if (itTopo == program.cycleIdToTopoIndex.end()) {
            continue;
        }
        const auto& stratum = program.strata[itTopo->second];
        auto plan = buildScbfFormulaArenaPlan(view, program, cycleId);
        for (std::size_t idx = 0; idx < plan.targets.size(); ++idx) {
            const auto& targetPlan = plan.targets[idx];
            TargetEvaluationResult tr =
                    evaluateTargetFromPlan(view, stratum, targetPlan, result.exportProbabilities, config);
            appendTargetStats(result, cycleId, idx, targetPlan.target, tr);
        }
        result.strataEvaluated++;
    }
    return result;
}

ScbfEvaluatorResult evaluateScbfProgramProbabilityViaFormulaBundle(
        const DerivationGraphViewInterface& view, const ScbfProgram& program,
        const ScbfEvaluatorConfig& config) {
    ScbfEvaluatorResult result;
    for (const auto cycleId : program.topoOrderCycleIds) {
        ScbfStratumFormulaBundle bundle = buildScbfStratumFormulaBundle(view, program, cycleId);
        dumpEvalBundleIfEnabled(bundle, config, "pre_eval");
#ifdef SOUFFLE_SCBF_EVAL_HAS_REWRITE
        if (config.rewriteBeforeEvaluate) {
            bundle = rewriteScbfStratumFormulaBundle(bundle, config.rewriteConfig, &result.rewriteStats);
            result.rewriteApplied = true;
            dumpEvalBundleIfEnabled(bundle, config, "post_rewrite");
        }
#else
        if (config.rewriteBeforeEvaluate) {
            throw std::runtime_error(
                    "SCBF evaluator rewrite requested but rewrite support is not compiled in");
        }
#endif
        for (std::size_t idx = 0; idx < bundle.targets.size(); ++idx) {
            const auto& targetFormula = bundle.targets[idx];
            TargetEvaluationResult tr =
                    evaluateTargetFromFormula(targetFormula, result.exportProbabilities, config);
            appendTargetStats(result, cycleId, idx, targetFormula.target, tr);
        }
        result.strataEvaluated++;
    }
    return result;
}

std::string summarizeScbfEvaluatorResult(const ScbfEvaluatorResult& result) {
    std::ostringstream oss;
    oss << "[scbf-eval] strata=" << result.strataEvaluated
        << " targets=" << result.targetsEvaluated
        << " node_probs=" << result.nodeProbabilities.size()
        << " exports=" << result.exportProbabilities.size()
        << " missing_imports=" << result.missingImports
        << " converged=" << (result.converged ? 1 : 0)
        << " rewrite_applied=" << (result.rewriteApplied ? 1 : 0);
    if (result.rewriteApplied) {
        oss << " rewrite_targets=" << result.rewriteStats.targetsProcessed
            << " rewrite_graph_regions=" << result.rewriteStats.graphRegionsRewritten
            << " rewrite_graph_iters=" << result.rewriteStats.graphRewriteIterations;
    }
    return oss.str();
}

}  // namespace souffle::problog::scbf

#ifdef SOUFFLE_SCBF_EVAL_SMOKE_MAIN
using namespace souffle::problog;
using namespace souffle::problog::scbf;

namespace {

void require(bool cond, const std::string& msg) {
    if (!cond) {
        throw std::runtime_error(msg);
    }
}

std::unordered_map<NodePtr, double> evalWholeGraphNoisyOr(
        const DerivationGraphViewInterface& view, std::size_t maxIterations, double epsilon) {
    std::unordered_map<NodePtr, double> nodeProb;
    for (const auto& node : view.getNodes()) {
        nodeProb[node] = node && node->isFact ? evalClamp01(node->getProbability()) : 0.0;
    }
    for (std::size_t iter = 0; iter < maxIterations; ++iter) {
        std::unordered_map<NodePtr, double> edgeOrByOutput;
        for (const auto& edge : view.getEdges()) {
            if (!edge) {
                continue;
            }
            double edgeProb = edge->isDeterministic() ? 1.0 : evalClamp01(edge->getProbability());
            const auto inputs = view.getInputs(edge);
            const auto negs = view.getBodyNegations(edge);
            for (std::size_t i = 0; i < inputs.size(); ++i) {
                const bool neg = i < negs.size() ? negs[i] : false;
                const auto it = nodeProb.find(inputs[i]);
                double ip = (it == nodeProb.end()) ? 0.0 : it->second;
                edgeProb *= neg ? (1.0 - ip) : ip;
            }
            NodePtr out = view.getOutput(edge);
            if (!out) {
                continue;
            }
            double prev = 0.0;
            auto itPrev = edgeOrByOutput.find(out);
            if (itPrev != edgeOrByOutput.end()) {
                prev = itPrev->second;
            }
            edgeOrByOutput[out] = evalClamp01(1.0 - (1.0 - prev) * (1.0 - edgeProb));
        }
        double maxDelta = 0.0;
        auto next = nodeProb;
        for (const auto& node : view.getNodes()) {
            double np = node && node->isFact ? evalClamp01(node->getProbability()) : 0.0;
            if (!node->isFact) {
                auto it = edgeOrByOutput.find(node);
                if (it != edgeOrByOutput.end()) {
                    np = it->second;
                }
            }
            maxDelta = std::max(maxDelta, absDiff(next[node], np));
            next[node] = np;
        }
        nodeProb.swap(next);
        if (maxDelta <= epsilon) {
            break;
        }
    }
    return nodeProb;
}

NodePtr findNodeByRelation(const DerivationGraphViewInterface& view, const std::string& relation) {
    for (const auto& node : view.getNodes()) {
        if (node && node->getTuple().relation_name == relation) {
            return node;
        }
    }
    return nullptr;
}

}  // namespace

int main() {
    try {
        DerivationGraph graph;
        NodePtr f = graph.createNode(UntypedTuple{"f", {1}}, 0.8);
        f->isFact = true;
        NodePtr a = graph.createNode(UntypedTuple{"a", {1}}, 1.0);
        NodePtr b = graph.createNode(UntypedTuple{"b", {1}}, 1.0);
        NodePtr c = graph.createNode(UntypedTuple{"c", {1}}, 1.0);
        c->setQuery();

        EdgePtr e1 = graph.createHyperedge({f}, a);
        EdgePtr e2 = graph.createHyperedge({a}, b);
        EdgePtr e3 = graph.createHyperedge({b}, a);
        EdgePtr e4 = graph.createHyperedge({b}, c);
        require(e1 && e2 && e3 && e4, "failed to create test edges");
        e1->setProbability(0.9);
        e2->setProbability(0.7);
        e3->setProbability(0.4);
        e4->setProbability(0.5);

        ScbfProgram program = buildScbfProgram(graph);
        std::string err;
        require(validateScbfProgram(graph, program, &err), "invalid SCBF program: " + err);
        ScbfEvaluatorConfig cfg;
        cfg.maxIterations = 500;
        cfg.epsilon = 1e-12;
        ScbfEvaluatorResult evalPlan = evaluateScbfProgramProbability(graph, program, cfg);
        ScbfEvaluatorResult evalFormula = evaluateScbfProgramProbabilityViaFormulaBundle(graph, program, cfg);
#ifdef SOUFFLE_SCBF_EVAL_HAS_REWRITE
        ScbfEvaluatorConfig cfgRewrite = cfg;
        cfgRewrite.rewriteBeforeEvaluate = true;
        cfgRewrite.rewriteConfig.useGraphRewriterAligned = true;
        cfgRewrite.rewriteConfig.splitMode = ScbfFormulaRewriteConfig::SplitMode::Naive;
        cfgRewrite.dumpFormulaBundle = true;
        cfgRewrite.dumpDir = "/tmp/scbf-eval-dump";
        cfgRewrite.dumpPrefix = "scbf_eval_smoke";
        cfgRewrite.rewriteConfig.dumpBeforeRewrite = true;
        cfgRewrite.rewriteConfig.dumpAfterRewrite = true;
        cfgRewrite.rewriteConfig.dumpDir = cfgRewrite.dumpDir;
        cfgRewrite.rewriteConfig.dumpPrefix = "scbf_rewrite_smoke";
        ScbfEvaluatorResult evalFormulaRewrite =
                evaluateScbfProgramProbabilityViaFormulaBundle(graph, program, cfgRewrite);
#endif

        NodePtr query = findNodeByRelation(graph, "c");
        require(query != nullptr, "query node c not found");
        auto itPlan = evalPlan.nodeProbabilities.find(query);
        auto itFormula = evalFormula.nodeProbabilities.find(query);
        require(itPlan != evalPlan.nodeProbabilities.end(), "query c missing in plan evaluator result");
        require(itFormula != evalFormula.nodeProbabilities.end(), "query c missing in formula evaluator result");
        double planProb = itPlan->second;
        double formulaProb = itFormula->second;
        require(planProb >= 0.0 && planProb <= 1.0, "plan query prob not in [0,1]");
        require(formulaProb >= 0.0 && formulaProb <= 1.0, "formula query prob not in [0,1]");

        auto baseline = evalWholeGraphNoisyOr(graph, 500, 1e-12);
        auto itBase = baseline.find(query);
        require(itBase != baseline.end(), "query c missing in baseline result");
        double baselineProb = itBase->second;
        double diffPlan = std::abs(planProb - baselineProb);
        double diffFormula = std::abs(formulaProb - baselineProb);
        double diffBetween = std::abs(planProb - formulaProb);
        require(diffPlan <= 1e-8, "plan evaluator differs from baseline by " + std::to_string(diffPlan));
        require(diffFormula <= 1e-8, "formula evaluator differs from baseline by " + std::to_string(diffFormula));
        require(diffBetween <= 1e-8,
                "plan and formula evaluator differ by " + std::to_string(diffBetween));
#ifdef SOUFFLE_SCBF_EVAL_HAS_REWRITE
        auto itFormulaRewrite = evalFormulaRewrite.nodeProbabilities.find(query);
        require(itFormulaRewrite != evalFormulaRewrite.nodeProbabilities.end(),
                "query c missing in formula+rewrite evaluator result");
        double formulaRewriteProb = itFormulaRewrite->second;
        double diffFormulaRewrite = std::abs(formulaRewriteProb - baselineProb);
        double diffFormulaVsRewrite = std::abs(formulaRewriteProb - formulaProb);
        require(evalFormulaRewrite.rewriteApplied, "formula+rewrite evaluator did not apply rewrite");
        require(formulaRewriteProb >= 0.0 && formulaRewriteProb <= 1.0,
                "formula+rewrite query prob not in [0,1]");
#endif

        std::cout << summarizeScbfProgram(program) << "\n";
        std::cout << summarizeScbfEvaluatorResult(evalPlan) << "\n";
        std::cout << summarizeScbfEvaluatorResult(evalFormula) << "\n";
#ifdef SOUFFLE_SCBF_EVAL_HAS_REWRITE
        std::cout << summarizeScbfEvaluatorResult(evalFormulaRewrite) << "\n";
#endif
        std::cout << "[scbf-eval-smoke] query=" << query->getTuple().toString()
                  << " plan=" << planProb
                  << " formula=" << formulaProb
                  << " baseline=" << baselineProb
                  << " diff_plan=" << diffPlan
                  << " diff_formula=" << diffFormula
                  << " diff_between=" << diffBetween
#ifdef SOUFFLE_SCBF_EVAL_HAS_REWRITE
                  << " formula_rewrite=" << evalFormulaRewrite.nodeProbabilities.at(query)
                  << " diff_formula_rewrite=" << std::abs(evalFormulaRewrite.nodeProbabilities.at(query) - baselineProb)
                  << " diff_formula_vs_rewrite=" << std::abs(evalFormulaRewrite.nodeProbabilities.at(query) - formulaProb)
#endif
                  << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[scbf-eval-smoke] failed: " << ex.what() << "\n";
        return 1;
    }
}
#endif
