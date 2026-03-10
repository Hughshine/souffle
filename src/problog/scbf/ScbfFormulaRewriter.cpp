#include "souffle/problog/scbf/ScbfFormulaRewriter.h"

#ifdef SOUFFLE_SCBF_REWRITE_BUNDLE_FORMULA
#ifndef SOUFFLE_SCBF_FORMULA_BUNDLE_IR
#define SOUFFLE_SCBF_FORMULA_BUNDLE_IR 1
#endif
#include "problog/scbf/ScbfFormula.cpp"
#endif

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "souffle/problog/GraphRewriter.h"

namespace souffle::problog::scbf {
namespace {

constexpr double kProbabilityEps = 1e-12;

struct TargetCounts {
    std::size_t nodes = 0;
    std::size_t rules = 0;
    std::size_t literals = 0;
};

TargetCounts countTarget(const ScbfTargetFormula& target) {
    TargetCounts counts;
    counts.nodes = target.localNodes.size();
    for (const auto& rules : target.localNodeRules) {
        counts.rules += rules.size();
        for (const auto& rule : rules) {
            counts.literals += rule.bodyLiterals.size();
        }
    }
    return counts;
}

bool nearlyZero(double x) {
    return std::abs(x) <= kProbabilityEps;
}

bool nearlyOne(double x) {
    return std::abs(1.0 - x) <= kProbabilityEps;
}

std::string sanitizePathToken(const std::string& token) {
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
    return out.empty() ? "scbf" : out;
}

void dumpBundleSnapshotIfEnabled(const ScbfStratumFormulaBundle& bundle,
        const ScbfFormulaRewriteConfig& config, const std::string& phaseTag) {
    const bool enabled = (phaseTag == "before" && config.dumpBeforeRewrite) ||
            (phaseTag == "after" && config.dumpAfterRewrite);
    if (!enabled || config.dumpDir.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(config.dumpDir, ec);
    const std::string prefix = sanitizePathToken(config.dumpPrefix);
    const std::string stem = prefix + "_cycle" + std::to_string(bundle.cycleId) + "_topo" +
            std::to_string(bundle.topoIndex) + "_" + sanitizePathToken(phaseTag);
    const std::string jsonPath = config.dumpDir + "/" + stem + ".json";
    const std::string dotPath = config.dumpDir + "/" + stem + ".dot";
    dumpScbfStratumFormulaBundleJson(bundle, jsonPath);
    dumpScbfStratumFormulaBundleDot(bundle, dotPath);
}

std::string rewriteNodeSortKey(const NodePtr& node) {
    return node ? node->getTuple().toString() + "#" + std::to_string(node->getId()) : "<null>";
}

std::vector<NodePtr> sortRewriteNodes(const std::unordered_set<NodePtr>& nodes) {
    std::vector<NodePtr> result(nodes.begin(), nodes.end());
    std::sort(result.begin(), result.end(), [](const NodePtr& a, const NodePtr& b) {
        return rewriteNodeSortKey(a) < rewriteNodeSortKey(b);
    });
    return result;
}

std::vector<EdgePtr> sortRewriteEdges(const std::unordered_set<EdgePtr>& edges) {
    std::vector<EdgePtr> result(edges.begin(), edges.end());
    std::sort(result.begin(), result.end(), [](const EdgePtr& a, const EdgePtr& b) {
        const std::size_t aid = a ? a->getId() : 0;
        const std::size_t bid = b ? b->getId() : 0;
        if (aid != bid) {
            return aid < bid;
        }
        return (a && b) ? (a->toString() < b->toString()) : (a.get() < b.get());
    });
    return result;
}

enum class TempNodeOriginKind {
    OriginalLocal = 0,
    OriginalImport,
    ConstantHelper,
    TrueHelper,
    SyntheticShadow,
    SyntheticOther,
};

struct TempNodeOrigin {
    TempNodeOriginKind kind = TempNodeOriginKind::SyntheticOther;
    std::size_t index = 0;
    NodePtr originalNode;
};

struct TargetGraphMaterialization {
    std::unique_ptr<IncrementalDerivationGraph> graph;
    std::unordered_map<std::size_t, NodePtr> localByIndex;
    std::unordered_map<std::size_t, NodePtr> importByIndex;
    NodePtr targetNode;
    std::unordered_map<NodePtr, TempNodeOrigin> baseOriginByNode;
    std::unordered_map<std::size_t, TempNodeOrigin> baseOriginByNodeId;
    std::size_t constCounter = 0;
    std::size_t trueCounter = 0;
};

std::optional<std::size_t> parseShadowSourceNodeId(const std::string& relationName) {
    const auto prefix = std::string("Shadow_");
    if (relationName.rfind(prefix, 0) != 0) {
        return std::nullopt;
    }
    const auto last = relationName.find_last_of('_');
    if (last == std::string::npos || last <= prefix.size()) {
        return std::nullopt;
    }
    const auto prev = relationName.find_last_of('_', last - 1);
    if (prev == std::string::npos || prev <= prefix.size()) {
        return std::nullopt;
    }
    const auto idText = relationName.substr(prev + 1, last - prev - 1);
    try {
        return static_cast<std::size_t>(std::stoull(idText));
    } catch (...) {
        return std::nullopt;
    }
}

UntypedTuple makeSyntheticTuple(const std::string& relation, std::size_t id) {
    UntypedTuple tuple;
    tuple.relation_name = relation;
    tuple.fields = {static_cast<souffle::RamDomain>(id)};
    return tuple;
}

NodePtr createFactNode(IncrementalDerivationGraph& graph, const std::string& relation, std::size_t id,
        double probability) {
    auto node = graph.createNode(makeSyntheticTuple(relation, id), probability);
    node->isFact = true;
    node->setProbability(probability);
    node->needOutput = false;
    return node;
}

NodePtr createOpaqueInputNode(
        IncrementalDerivationGraph& graph, const std::string& relation, std::size_t id) {
    auto node = graph.createNode(makeSyntheticTuple(relation, id), 1.0);
    node->isFact = false;
    node->setProbability(1.0);
    node->needOutput = false;
    return node;
}

NodePtr ensureTrueHelperNode(TargetGraphMaterialization& m) {
    auto node = createFactNode(
            *m.graph, "SCBF_TRUE_HELPER_" + std::to_string(m.trueCounter), m.trueCounter, 1.0);
    m.baseOriginByNode[node] = TempNodeOrigin{TempNodeOriginKind::TrueHelper, m.trueCounter, nullptr};
    m.baseOriginByNodeId[node->getId()] = m.baseOriginByNode[node];
    m.trueCounter++;
    return node;
}

NodePtr createConstHelperNode(TargetGraphMaterialization& m, double probability) {
    auto node = createFactNode(*m.graph,
            "SCBF_CONST_HELPER_" + std::to_string(m.constCounter), m.constCounter, probability);
    m.baseOriginByNode[node] =
            TempNodeOrigin{TempNodeOriginKind::ConstantHelper, m.constCounter, nullptr};
    m.baseOriginByNodeId[node->getId()] = m.baseOriginByNode[node];
    m.constCounter++;
    return node;
}

TargetGraphMaterialization materializeTargetToGraph(const ScbfTargetFormula& target) {
    TargetGraphMaterialization m;
    m.graph = std::make_unique<IncrementalDerivationGraph>();

    for (std::size_t i = 0; i < target.localNodes.size(); ++i) {
        const auto& local = target.localNodes[i];
        NodePtr tmp = m.graph->createNode(makeSyntheticTuple("SCBF_LOCAL_" + std::to_string(i), i),
                local.isFact ? local.factProbability : 1.0);
        tmp->isFact = local.isFact;
        if (local.isFact) {
            tmp->setProbability(local.factProbability);
        } else {
            tmp->setProbability(1.0);
        }
        tmp->needOutput = (i == target.targetLocalIndex);
        m.localByIndex[i] = tmp;
        m.baseOriginByNode[tmp] = TempNodeOrigin{TempNodeOriginKind::OriginalLocal, i, local.node};
        m.baseOriginByNodeId[tmp->getId()] = m.baseOriginByNode[tmp];
        if (i == target.targetLocalIndex) {
            m.targetNode = tmp;
        }
    }

    for (std::size_t i = 0; i < target.imports.size(); ++i) {
        const auto& import = target.imports[i];
        // Imports are external references, not local facts. Materialize them as opaque
        // inputs so graph rewrite can see the dependency boundary without constant-folding it.
        NodePtr tmp = createOpaqueInputNode(*m.graph, "SCBF_IMPORT_" + std::to_string(i), i);
        m.importByIndex[i] = tmp;
        m.baseOriginByNode[tmp] = TempNodeOrigin{TempNodeOriginKind::OriginalImport, i, import.node};
        m.baseOriginByNodeId[tmp->getId()] = m.baseOriginByNode[tmp];
    }

    for (std::size_t head = 0; head < target.localNodeRules.size(); ++head) {
        auto itHead = m.localByIndex.find(head);
        if (itHead == m.localByIndex.end()) {
            continue;
        }
        NodePtr headNode = itHead->second;
        for (const auto& rule : target.localNodeRules[head]) {
            std::vector<NodePtr> inputs;
            std::vector<bool> negs;
            inputs.reserve(rule.bodyLiterals.size());
            negs.reserve(rule.bodyLiterals.size());
            for (const auto& lit : rule.bodyLiterals) {
                NodePtr input = nullptr;
                switch (lit.source) {
                    case ScbfLiteralSource::LocalNode: {
                        auto it = m.localByIndex.find(lit.index);
                        if (it != m.localByIndex.end()) {
                            input = it->second;
                        }
                        break;
                    }
                    case ScbfLiteralSource::ImportNode: {
                        auto it = m.importByIndex.find(lit.index);
                        if (it != m.importByIndex.end()) {
                            input = it->second;
                        }
                        break;
                    }
                    case ScbfLiteralSource::Constant:
                        input = createConstHelperNode(m, lit.constantProbability);
                        break;
                }
                if (!input) {
                    continue;
                }
                inputs.push_back(input);
                negs.push_back(lit.negated);
            }
            if (inputs.empty()) {
                inputs.push_back(ensureTrueHelperNode(m));
                negs.push_back(false);
            }

            EdgePtr edge = m.graph->createHyperedge(inputs, headNode, nullptr, negs, naiveRuleApplication);
            if (!edge) {
                continue;
            }
            edge->setProbability(rule.deterministic ? 1.0 : rule.edgeProbability);
        }
    }
    return m;
}

RewriteFeatureFlags buildGraphRewriteFlags(const ScbfFormulaRewriteConfig& config) {
    RewriteFeatureFlags flags;
    flags.enableSingleHyperedge = config.enableSingleHyperedge;
    flags.enableAllFactsToSO = config.enableAllFactsToSO;
    flags.enableLinearTwoEdge = config.enableLinearTwoEdge;
    flags.enableParallelEdge = config.enableParallelEdge;
    flags.enableFanOutConverge = config.enableFanOutConverge;
    flags.enableGeneral = config.enableGeneral;
    flags.enableCompaction = config.enableCompaction;
    flags.forceCompleteSisoDetect = config.forceCompleteSisoDetect;
    flags.enableCleanupIsolated = config.enableCleanupIsolated;
    flags.splitMaxNewNodesPerPass = config.splitMaxNewNodesPerPass;
    flags.splitMaxNewEdgesPerPass = config.splitMaxNewEdgesPerPass;
    flags.splitMaxGroupsPerNode = config.splitMaxGroupsPerNode;
    flags.splitMinGroupEdges = config.splitMinGroupEdges;
    switch (config.splitMode) {
        case ScbfFormulaRewriteConfig::SplitMode::None:
            flags.splitMode = SplitMode::None;
            break;
        case ScbfFormulaRewriteConfig::SplitMode::Naive:
            flags.splitMode = SplitMode::Naive;
            break;
        case ScbfFormulaRewriteConfig::SplitMode::Complete:
            flags.splitMode = SplitMode::Complete;
            break;
    }
    return flags;
}

TempNodeOrigin resolveNodeOrigin(const NodePtr& node, const TargetGraphMaterialization& m) {
    auto itBase = m.baseOriginByNode.find(node);
    if (itBase != m.baseOriginByNode.end()) {
        return itBase->second;
    }
    if (node && node->isShadow) {
        auto parsedId = parseShadowSourceNodeId(node->getTuple().relation_name);
        if (parsedId.has_value()) {
            auto itSource = m.baseOriginByNodeId.find(parsedId.value());
            if (itSource != m.baseOriginByNodeId.end()) {
                TempNodeOrigin origin = itSource->second;
                origin.kind = TempNodeOriginKind::SyntheticShadow;
                return origin;
            }
        }
        return TempNodeOrigin{TempNodeOriginKind::SyntheticShadow, 0, nullptr};
    }
    return TempNodeOrigin{TempNodeOriginKind::SyntheticOther, 0, nullptr};
}

bool isDirectImportNode(const NodePtr& node, const TargetGraphMaterialization& m) {
    auto it = m.baseOriginByNode.find(node);
    return it != m.baseOriginByNode.end() && it->second.kind == TempNodeOriginKind::OriginalImport;
}

ScbfTargetFormula rebuildTargetFromGraph(
        const ScbfTargetFormula& original, const TargetGraphMaterialization& m, const IncSubgraphView& view) {
    ScbfTargetFormula rewritten;
    rewritten.target = original.target;

    std::unordered_set<NodePtr> localNodeSet;
    std::unordered_set<NodePtr> importNodeSet;
    for (const auto& edge : view.getEdges()) {
        if (!edge) {
            continue;
        }
        NodePtr out = view.getOutput(edge);
        if (out) {
            localNodeSet.insert(out);
        }
        const auto& inputs = view.getInputs(edge);
        for (const auto& in : inputs) {
            if (isDirectImportNode(in, m)) {
                importNodeSet.insert(in);
            } else {
                localNodeSet.insert(in);
            }
        }
    }
    if (m.targetNode) {
        localNodeSet.insert(m.targetNode);
    }

    auto sortedLocal = sortRewriteNodes(localNodeSet);
    auto sortedImports = sortRewriteNodes(importNodeSet);
    std::unordered_map<NodePtr, std::size_t> localIndexByTmp;
    std::unordered_map<NodePtr, std::size_t> importIndexByTmp;
    localIndexByTmp.reserve(sortedLocal.size());
    importIndexByTmp.reserve(sortedImports.size());

    rewritten.localNodes.reserve(sortedLocal.size());
    rewritten.localNodeRules.resize(sortedLocal.size());
    for (std::size_t i = 0; i < sortedLocal.size(); ++i) {
        NodePtr tmp = sortedLocal[i];
        TempNodeOrigin origin = resolveNodeOrigin(tmp, m);
        NodePtr canonical = tmp;
        if (origin.kind == TempNodeOriginKind::OriginalLocal && origin.originalNode) {
            canonical = origin.originalNode;
        }
        rewritten.localNodes.push_back(
                ScbfLocalNodeDef{canonical, tmp ? tmp->isFact : false, tmp ? tmp->getProbability() : 0.0, false});
        localIndexByTmp[tmp] = i;
    }

    if (m.targetNode && localIndexByTmp.count(m.targetNode)) {
        rewritten.targetLocalIndex = localIndexByTmp[m.targetNode];
        rewritten.localNodes[rewritten.targetLocalIndex].needOutput = true;
    } else if (!rewritten.localNodes.empty()) {
        rewritten.targetLocalIndex = 0;
    } else {
        rewritten.targetLocalIndex = 0;
    }

    rewritten.imports.reserve(sortedImports.size());
    for (std::size_t i = 0; i < sortedImports.size(); ++i) {
        NodePtr tmp = sortedImports[i];
        TempNodeOrigin origin = resolveNodeOrigin(tmp, m);
        if (origin.kind == TempNodeOriginKind::OriginalImport && origin.index < original.imports.size()) {
            rewritten.imports.push_back(original.imports[origin.index]);
            importIndexByTmp[tmp] = rewritten.imports.size() - 1;
        }
    }

    const auto edges = sortRewriteEdges(view.getEdges());
    for (const auto& edge : edges) {
        if (!edge) {
            continue;
        }
        NodePtr out = view.getOutput(edge);
        auto itHead = localIndexByTmp.find(out);
        if (itHead == localIndexByTmp.end()) {
            continue;
        }
        ScbfRuleEquation rule;
        rule.sourceEdge = edge;
        rule.deterministic = edge->isDeterministic();
        rule.edgeProbability = edge->isDeterministic() ? 1.0 : edge->getProbability();
        const auto& inputs = view.getInputs(edge);
        const auto& negs = view.getBodyNegations(edge);
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            const auto& in = inputs[i];
            ScbfLiteralRef lit;
            lit.negated = i < negs.size() ? negs[i] : false;
            auto itImport = importIndexByTmp.find(in);
            auto itLocal = localIndexByTmp.find(in);
            if (itImport != importIndexByTmp.end() && itLocal == localIndexByTmp.end()) {
                lit.source = ScbfLiteralSource::ImportNode;
                lit.index = itImport->second;
            } else if (itLocal != localIndexByTmp.end()) {
                lit.source = ScbfLiteralSource::LocalNode;
                lit.index = itLocal->second;
            } else if (in && in->isFact) {
                lit.source = ScbfLiteralSource::Constant;
                lit.constantProbability = in->getProbability();
            } else {
                lit.source = ScbfLiteralSource::Constant;
                lit.constantProbability = 0.0;
            }
            rule.bodyLiterals.push_back(lit);
        }
        rewritten.localNodeRules[itHead->second].push_back(std::move(rule));
    }
    return rewritten;
}

ScbfTargetFormula rewriteTargetViaGraphRewriter(const ScbfTargetFormula& input,
        const ScbfFormulaRewriteConfig& config, ScbfFormulaRewriteStats& stats) {
    TargetGraphMaterialization materialized = materializeTargetToGraph(input);
    if (!materialized.graph) {
        return input;
    }

    std::set<NodePtr> emptyInsertNodes;
    std::set<EdgePtr> emptyInsertEdges;
    std::set<NodePtr> emptyDeleteNodes;
    std::set<EdgePtr> emptyDeleteEdges;
    IncSubgraphView view(materialized.graph->getNodes(), materialized.graph->getEdges(), emptyInsertNodes,
            emptyInsertEdges, emptyDeleteNodes, emptyDeleteEdges);

    GraphRewriter rewriter;
    RewriteFeatureFlags flags = buildGraphRewriteFlags(config);
    auto rwStats = rewriter.rewriteUntilFixpoint(*materialized.graph, view, false, flags);

    stats.graphRewriteRuns++;
    stats.graphRewriteIterations += rwStats.numIterations;
    stats.graphRegionsRewritten += rwStats.numRegionsRewritten;
    stats.graphNodesRemoved += rwStats.numNodesRemoved;
    stats.graphEdgesRemoved += rwStats.numEdgesRemoved;
    stats.graphEdgesAdded += rwStats.numEdgesAdded;
    stats.graphRandomVarsBefore += rwStats.randomVarsBefore;
    stats.graphRandomVarsAfter += rwStats.randomVarsAfter;

    return rebuildTargetFromGraph(input, materialized, view);
}

bool foldRuleConstants(ScbfTargetFormula& target, ScbfFormulaRewriteStats& stats) {
    bool changed = false;
    for (auto& rules : target.localNodeRules) {
        std::vector<ScbfRuleEquation> kept;
        kept.reserve(rules.size());
        for (auto& rule : rules) {
            if (!rule.deterministic && nearlyZero(rule.edgeProbability)) {
                stats.rulesDroppedUnsat++;
                changed = true;
                continue;
            }

            ScbfRuleEquation rewritten = rule;
            std::vector<ScbfLiteralRef> folded;
            folded.reserve(rule.bodyLiterals.size());
            bool unsat = false;
            for (auto lit : rule.bodyLiterals) {
                if (lit.source == ScbfLiteralSource::Constant) {
                    double p = lit.constantProbability;
                    if (lit.negated) {
                        p = 1.0 - p;
                        lit.negated = false;
                        changed = true;
                    }
                    lit.constantProbability = p;
                    if (nearlyZero(p)) {
                        unsat = true;
                        break;
                    }
                    if (nearlyOne(p)) {
                        stats.literalsDroppedConstTrue++;
                        changed = true;
                        continue;
                    }
                }
                folded.push_back(lit);
            }

            if (unsat) {
                stats.rulesDroppedUnsat++;
                changed = true;
                continue;
            }
            rewritten.bodyLiterals = std::move(folded);
            kept.push_back(std::move(rewritten));
        }
        if (kept.size() != rules.size()) {
            changed = true;
        }
        rules = std::move(kept);
    }
    return changed;
}

std::size_t resolveAliasIndex(std::size_t idx, const std::vector<std::optional<std::size_t>>& alias,
        std::vector<int>& state, std::vector<std::size_t>& memo) {
    if (idx >= alias.size()) {
        return idx;
    }
    if (state[idx] == 2) {
        return memo[idx];
    }
    if (state[idx] == 1) {
        // Cycle in alias graph: keep this node as its own representative.
        return idx;
    }

    state[idx] = 1;
    std::size_t resolved = idx;
    if (alias[idx].has_value() && alias[idx].value() < alias.size()) {
        const auto next = alias[idx].value();
        const auto nextResolved = resolveAliasIndex(next, alias, state, memo);
        if (nextResolved != idx) {
            resolved = nextResolved;
        }
    }
    memo[idx] = resolved;
    state[idx] = 2;
    return resolved;
}

bool collapseSingleLocalAliases(ScbfTargetFormula& target, ScbfFormulaRewriteStats& stats) {
    const std::size_t n = target.localNodes.size();
    if (n == 0 || target.localNodeRules.size() != n) {
        return false;
    }

    std::vector<std::optional<std::size_t>> alias(n);
    for (std::size_t i = 0; i < n; ++i) {
        const auto& nodeDef = target.localNodes[i];
        if (nodeDef.isFact) {
            continue;
        }
        const auto& rules = target.localNodeRules[i];
        if (rules.size() != 1) {
            continue;
        }
        const auto& rule = rules.front();
        if (!rule.deterministic && !nearlyOne(rule.edgeProbability)) {
            continue;
        }
        if (rule.bodyLiterals.size() != 1) {
            continue;
        }
        const auto& lit = rule.bodyLiterals.front();
        if (lit.source != ScbfLiteralSource::LocalNode || lit.negated || lit.index >= n || lit.index == i) {
            continue;
        }
        alias[i] = lit.index;
    }

    std::vector<int> state(n, 0);
    std::vector<std::size_t> memo(n);
    for (std::size_t i = 0; i < n; ++i) {
        memo[i] = i;
    }

    bool hasAlias = false;
    for (std::size_t i = 0; i < n; ++i) {
        const auto root = resolveAliasIndex(i, alias, state, memo);
        if (root != i) {
            hasAlias = true;
        }
    }
    if (!hasAlias) {
        return false;
    }

    std::size_t aliasCount = 0;
    for (std::size_t i = 0; i < n; ++i) {
        if (memo[i] != i) {
            aliasCount++;
        }
    }
    stats.aliasesApplied += aliasCount;

    for (auto& rules : target.localNodeRules) {
        for (auto& rule : rules) {
            for (auto& lit : rule.bodyLiterals) {
                if (lit.source != ScbfLiteralSource::LocalNode || lit.index >= n) {
                    continue;
                }
                lit.index = memo[lit.index];
            }
        }
    }
    if (target.targetLocalIndex < n) {
        target.targetLocalIndex = memo[target.targetLocalIndex];
    }
    return true;
}

bool pruneUnreachableNodes(ScbfTargetFormula& target, ScbfFormulaRewriteStats& stats) {
    const std::size_t n = target.localNodes.size();
    if (n == 0 || target.targetLocalIndex >= n || target.localNodeRules.size() != n) {
        return false;
    }

    std::vector<bool> reachable(n, false);
    std::queue<std::size_t> work;
    reachable[target.targetLocalIndex] = true;
    work.push(target.targetLocalIndex);

    while (!work.empty()) {
        const auto head = work.front();
        work.pop();
        for (const auto& rule : target.localNodeRules[head]) {
            for (const auto& lit : rule.bodyLiterals) {
                if (lit.source != ScbfLiteralSource::LocalNode || lit.index >= n) {
                    continue;
                }
                if (!reachable[lit.index]) {
                    reachable[lit.index] = true;
                    work.push(lit.index);
                }
            }
        }
    }

    std::size_t kept = 0;
    for (bool flag : reachable) {
        kept += flag ? 1 : 0;
    }
    if (kept == n) {
        return false;
    }

    stats.unreachableNodesRemoved += (n - kept);
    std::vector<std::size_t> remap(n, std::numeric_limits<std::size_t>::max());
    std::vector<ScbfLocalNodeDef> newNodes;
    newNodes.reserve(kept);
    std::vector<std::vector<ScbfRuleEquation>> newRules;
    newRules.reserve(kept);

    for (std::size_t i = 0; i < n; ++i) {
        if (!reachable[i]) {
            continue;
        }
        remap[i] = newNodes.size();
        newNodes.push_back(target.localNodes[i]);
        newRules.push_back(target.localNodeRules[i]);
    }

    for (auto& rules : newRules) {
        for (auto& rule : rules) {
            for (auto& lit : rule.bodyLiterals) {
                if (lit.source == ScbfLiteralSource::LocalNode && lit.index < n) {
                    const auto mapped = remap[lit.index];
                    if (mapped == std::numeric_limits<std::size_t>::max()) {
                        throw std::runtime_error("SCBF rewrite internal error: unmapped reachable literal");
                    }
                    lit.index = mapped;
                }
            }
        }
    }

    const auto mappedTarget = remap[target.targetLocalIndex];
    if (mappedTarget == std::numeric_limits<std::size_t>::max()) {
        throw std::runtime_error("SCBF rewrite internal error: target became unreachable");
    }
    target.targetLocalIndex = mappedTarget;
    target.localNodes = std::move(newNodes);
    target.localNodeRules = std::move(newRules);
    return true;
}

double clamp01(double x) {
    if (x < 0.0) {
        return 0.0;
    }
    if (x > 1.0) {
        return 1.0;
    }
    return x;
}

double evaluateTargetFormulaNoisyOr(const ScbfTargetFormula& target,
        const std::unordered_map<NodePtr, double>& importProbabilities,
        std::size_t maxIterations = 1024, double epsilon = 1e-12) {
    if (target.localNodes.empty() || target.targetLocalIndex >= target.localNodes.size()) {
        return 0.0;
    }
    std::vector<double> nodeProb(target.localNodes.size(), 0.0);
    for (std::size_t i = 0; i < target.localNodes.size(); ++i) {
        if (target.localNodes[i].isFact) {
            nodeProb[i] = clamp01(target.localNodes[i].factProbability);
        }
    }

    for (std::size_t iter = 0; iter < maxIterations; ++iter) {
        std::vector<double> next = nodeProb;
        double maxDelta = 0.0;
        for (std::size_t head = 0; head < target.localNodes.size(); ++head) {
            if (target.localNodes[head].isFact) {
                next[head] = clamp01(target.localNodes[head].factProbability);
                maxDelta = std::max(maxDelta, std::abs(next[head] - nodeProb[head]));
                continue;
            }
            double nodeOr = 0.0;
            for (const auto& rule : target.localNodeRules[head]) {
                double ruleProb = rule.deterministic ? 1.0 : clamp01(rule.edgeProbability);
                for (const auto& lit : rule.bodyLiterals) {
                    double litProb = 0.0;
                    if (lit.source == ScbfLiteralSource::LocalNode) {
                        if (lit.index < nodeProb.size()) {
                            litProb = nodeProb[lit.index];
                        }
                    } else if (lit.source == ScbfLiteralSource::ImportNode) {
                        if (lit.index < target.imports.size()) {
                            const auto importNode = target.imports[lit.index].node;
                            auto it = importProbabilities.find(importNode);
                            if (it != importProbabilities.end()) {
                                litProb = it->second;
                            } else if (importNode && importNode->isFact) {
                                litProb = importNode->getProbability();
                            }
                        }
                    } else {
                        litProb = lit.constantProbability;
                    }
                    litProb = clamp01(litProb);
                    if (lit.negated) {
                        litProb = 1.0 - litProb;
                    }
                    ruleProb *= clamp01(litProb);
                }
                ruleProb = clamp01(ruleProb);
                nodeOr = 1.0 - (1.0 - nodeOr) * (1.0 - ruleProb);
            }
            next[head] = clamp01(nodeOr);
            maxDelta = std::max(maxDelta, std::abs(next[head] - nodeProb[head]));
        }
        nodeProb.swap(next);
        if (maxDelta <= epsilon) {
            break;
        }
    }
    return nodeProb[target.targetLocalIndex];
}

const ScbfTargetFormula* findTargetFormula(const ScbfStratumFormulaBundle& bundle, const NodePtr& target) {
    for (const auto& tf : bundle.targets) {
        if (tf.target == target) {
            return &tf;
        }
    }
    return nullptr;
}

}  // namespace

ScbfTargetFormula rewriteScbfTargetFormula(const ScbfTargetFormula& input,
        const ScbfFormulaRewriteConfig& config, ScbfFormulaRewriteStats* stats) {
    ScbfTargetFormula target = input;
    ScbfFormulaRewriteStats localDummyStats;
    ScbfFormulaRewriteStats& activeStats = stats ? *stats : localDummyStats;
    const auto before = countTarget(input);
    activeStats.targetsProcessed++;
    activeStats.nodesBefore += before.nodes;
    activeStats.rulesBefore += before.rules;
    activeStats.literalsBefore += before.literals;

    if (config.useGraphRewriterAligned) {
        target = rewriteTargetViaGraphRewriter(target, config, activeStats);
    }

    for (std::size_t iter = 0; iter < config.maxIterations; ++iter) {
        bool changed = false;
        if (config.foldConstants) {
            changed = foldRuleConstants(target, activeStats) || changed;
        }
        if (config.collapseSingleLocalAliases) {
            changed = collapseSingleLocalAliases(target, activeStats) || changed;
        }
        if (config.pruneUnreachableNodes) {
            changed = pruneUnreachableNodes(target, activeStats) || changed;
        }
        if (!changed) {
            break;
        }
        activeStats.iterations++;
    }

    const auto after = countTarget(target);
    activeStats.nodesAfter += after.nodes;
    activeStats.rulesAfter += after.rules;
    activeStats.literalsAfter += after.literals;
    return target;
}

ScbfStratumFormulaBundle rewriteScbfStratumFormulaBundle(const ScbfStratumFormulaBundle& input,
        const ScbfFormulaRewriteConfig& config, ScbfFormulaRewriteStats* stats) {
    dumpBundleSnapshotIfEnabled(input, config, "before");
    ScbfStratumFormulaBundle rewritten = input;
    rewritten.targets.clear();
    rewritten.targets.reserve(input.targets.size());
    for (const auto& target : input.targets) {
        rewritten.targets.push_back(rewriteScbfTargetFormula(target, config, stats));
    }
    dumpBundleSnapshotIfEnabled(rewritten, config, "after");
    return rewritten;
}

std::string summarizeScbfFormulaRewriteStats(const ScbfFormulaRewriteStats& stats) {
    std::ostringstream oss;
    oss << "[scbf-rewrite] targets=" << stats.targetsProcessed
        << " iterations=" << stats.iterations
        << " nodes_before=" << stats.nodesBefore
        << " nodes_after=" << stats.nodesAfter
        << " rules_before=" << stats.rulesBefore
        << " rules_after=" << stats.rulesAfter
        << " literals_before=" << stats.literalsBefore
        << " literals_after=" << stats.literalsAfter
        << " rules_dropped_unsat=" << stats.rulesDroppedUnsat
        << " literals_dropped_const_true=" << stats.literalsDroppedConstTrue
        << " aliases_applied=" << stats.aliasesApplied
        << " unreachable_nodes_removed=" << stats.unreachableNodesRemoved
        << " graph_runs=" << stats.graphRewriteRuns
        << " graph_iters=" << stats.graphRewriteIterations
        << " graph_regions=" << stats.graphRegionsRewritten
        << " graph_nodes_removed=" << stats.graphNodesRemoved
        << " graph_edges_removed=" << stats.graphEdgesRemoved
        << " graph_edges_added=" << stats.graphEdgesAdded
        << " graph_rv_before=" << stats.graphRandomVarsBefore
        << " graph_rv_after=" << stats.graphRandomVarsAfter;
    return oss.str();
}

}  // namespace souffle::problog::scbf

#ifdef SOUFFLE_SCBF_REWRITE_SMOKE_MAIN
using namespace souffle::problog;
using namespace souffle::problog::scbf;

namespace {

void require(bool cond, const std::string& msg) {
    if (!cond) {
        throw std::runtime_error(msg);
    }
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

        // Local SCC chain: a <- b <- c <- a
        EdgePtr e1 = graph.createHyperedge({b}, a);
        EdgePtr e2 = graph.createHyperedge({c}, b);
        EdgePtr e3 = graph.createHyperedge({a}, c);
        // External support: c <- f with probability 0.9
        EdgePtr e4 = graph.createHyperedge({f}, c);
        require(e1 && e2 && e3 && e4, "failed to create rewrite smoke edges");
        e4->setProbability(0.9);

        ScbfProgram program = buildScbfProgram(graph);
        std::string err;
        require(validateScbfProgram(graph, program, &err), "invalid SCBF program: " + err);
        NodePtr query = findNodeByRelation(graph, "c");
        require(query != nullptr, "query node c not found");
        std::size_t queryCycle = program.nodeToCycleId.at(query);

        auto bundle = buildScbfStratumFormulaBundle(graph, program, queryCycle);
        require(validateScbfStratumFormulaBundle(graph, program, bundle, &err),
                "invalid pre-rewrite bundle: " + err);
        const auto* beforeTarget = findTargetFormula(bundle, query);
        require(beforeTarget != nullptr, "query target formula missing before rewrite");
        const double beforeProb = evaluateTargetFormulaNoisyOr(*beforeTarget, {});

        ScbfFormulaRewriteStats stats;
        auto rewritten = rewriteScbfStratumFormulaBundle(bundle, {}, &stats);
        const auto* afterTarget = findTargetFormula(rewritten, query);
        require(afterTarget != nullptr, "query target formula missing after rewrite");
        const double afterProb = evaluateTargetFormulaNoisyOr(*afterTarget, {});

        const double diff = std::abs(beforeProb - afterProb);
        require(diff <= 1e-8, "rewrite changed probability by " + std::to_string(diff));
        require(stats.nodesAfter <= stats.nodesBefore, "rewrite increased node count");
        require(stats.rulesAfter <= stats.rulesBefore, "rewrite increased rule count");
        require(stats.literalsAfter <= stats.literalsBefore, "rewrite increased literal count");
        require(stats.graphRewriteRuns > 0, "expected graph rewrite execution");

        std::cout << summarizeScbfStratumFormulaBundle(bundle) << "\n";
        std::cout << summarizeScbfStratumFormulaBundle(rewritten) << "\n";
        std::cout << summarizeScbfFormulaRewriteStats(stats) << "\n";
        std::cout << "[scbf-rewrite-smoke] query=" << query->getTuple().toString()
                  << " before=" << beforeProb
                  << " after=" << afterProb
                  << " diff=" << diff
                  << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[scbf-rewrite-smoke] failed: " << ex.what() << "\n";
        return 1;
    }
}
#endif
