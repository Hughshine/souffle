#include "souffle/problog/scbf/ScbfGlobalFormulaRewriter.h"

#ifdef SOUFFLE_SCBF_GLOBAL_REWRITE_GLOBAL_FORMULA
#ifndef SOUFFLE_SCBF_GLOBAL_FORMULA_BUNDLE_FORMULA
#define SOUFFLE_SCBF_GLOBAL_FORMULA_BUNDLE_FORMULA 1
#endif
#include "problog/scbf/ScbfGlobalFormula.cpp"
#endif

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

#include "souffle/problog/GraphRewriter.h"

namespace souffle::problog::scbf {
namespace {

constexpr double kGlobalProbabilityEps = 1e-12;

struct GlobalCounts {
    std::size_t components = 0;
    std::size_t nodes = 0;
    std::size_t rules = 0;
    std::size_t literals = 0;
    std::size_t imports = 0;
};

GlobalCounts countGlobal(const ScbfGlobalFormula& global) {
    GlobalCounts counts;
    counts.components = global.components.size();
    counts.nodes = global.ownedNodes.size();
    counts.rules = global.ownedRules.size();
    for (const auto& component : global.components) {
        counts.imports += component.importNodes.size();
        for (const auto* rule : component.rules) {
            if (rule) {
                counts.literals += rule->bodyLiterals.size();
            }
        }
    }
    return counts;
}

bool globalNearlyZero(double x) {
    return std::abs(x) <= kGlobalProbabilityEps;
}

bool globalNearlyOne(double x) {
    return std::abs(1.0 - x) <= kGlobalProbabilityEps;
}

std::string globalRewriteSanitizePathToken(const std::string& token) {
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
    return out.empty() ? "scbf_global" : out;
}

void dumpGlobalSnapshotIfEnabled(const ScbfGlobalFormula& global,
        const ScbfFormulaRewriteConfig& config, const std::string& phaseTag) {
    const bool enabled = (phaseTag == "before" && config.dumpBeforeRewrite) ||
            (phaseTag == "after" && config.dumpAfterRewrite);
    if (!enabled || config.dumpDir.empty()) {
        return;
    }
    std::error_code ec;
    std::filesystem::create_directories(config.dumpDir, ec);
    const std::string prefix = globalRewriteSanitizePathToken(config.dumpPrefix);
    const std::string stem = prefix + "_global_" + globalRewriteSanitizePathToken(phaseTag);
    dumpScbfGlobalFormulaJson(global, config.dumpDir + "/" + stem + ".json");
    dumpScbfGlobalFormulaDot(global, config.dumpDir + "/" + stem + ".dot");
}

std::string globalRewriteNodeSortKey(const NodePtr& node) {
    return node ? node->getTuple().toString() + "#" + std::to_string(node->getId()) : "<null>";
}

std::vector<NodePtr> sortGlobalRewriteNodes(const std::unordered_set<NodePtr>& nodes) {
    std::vector<NodePtr> result(nodes.begin(), nodes.end());
    std::sort(result.begin(), result.end(), [](const NodePtr& a, const NodePtr& b) {
        return globalRewriteNodeSortKey(a) < globalRewriteNodeSortKey(b);
    });
    return result;
}

std::vector<EdgePtr> sortGlobalRewriteEdges(const std::unordered_set<EdgePtr>& edges) {
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

enum class GlobalTempNodeOriginKind {
    OriginalNode = 0,
    ConstantHelper,
    TrueHelper,
    SyntheticShadow,
    SyntheticOther,
};

struct GlobalTempNodeOrigin {
    GlobalTempNodeOriginKind kind = GlobalTempNodeOriginKind::SyntheticOther;
    std::size_t index = 0;
    const ScbfGlobalFormulaNode* originalNode = nullptr;
};

struct GlobalGraphMaterialization {
    std::unique_ptr<IncrementalDerivationGraph> graph;
    std::unordered_map<const ScbfGlobalFormulaNode*, NodePtr> tempByOriginalNode;
    std::unordered_map<NodePtr, GlobalTempNodeOrigin> baseOriginByNode;
    std::unordered_map<std::size_t, GlobalTempNodeOrigin> baseOriginByNodeId;
    std::size_t constCounter = 0;
    std::size_t trueCounter = 0;
};

std::optional<std::size_t> parseGlobalShadowSourceNodeId(const std::string& relationName) {
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

UntypedTuple makeGlobalSyntheticTuple(const std::string& relation, std::size_t id) {
    UntypedTuple tuple;
    tuple.relation_name = relation;
    tuple.fields = {static_cast<souffle::RamDomain>(id)};
    return tuple;
}

NodePtr createGlobalFactNode(IncrementalDerivationGraph& graph, const std::string& relation,
        std::size_t id, double probability) {
    auto node = graph.createNode(makeGlobalSyntheticTuple(relation, id), probability);
    node->isFact = true;
    node->setProbability(probability);
    node->needOutput = false;
    return node;
}

NodePtr ensureGlobalTrueHelperNode(GlobalGraphMaterialization& m) {
    auto node = createGlobalFactNode(*m.graph,
            "SCBF_GLOBAL_TRUE_HELPER_" + std::to_string(m.trueCounter), m.trueCounter, 1.0);
    m.baseOriginByNode[node] =
            GlobalTempNodeOrigin{GlobalTempNodeOriginKind::TrueHelper, m.trueCounter, nullptr};
    m.baseOriginByNodeId[node->getId()] = m.baseOriginByNode[node];
    m.trueCounter++;
    return node;
}

NodePtr createGlobalConstHelperNode(GlobalGraphMaterialization& m, double probability) {
    auto node = createGlobalFactNode(*m.graph,
            "SCBF_GLOBAL_CONST_HELPER_" + std::to_string(m.constCounter), m.constCounter, probability);
    m.baseOriginByNode[node] =
            GlobalTempNodeOrigin{GlobalTempNodeOriginKind::ConstantHelper, m.constCounter, nullptr};
    m.baseOriginByNodeId[node->getId()] = m.baseOriginByNode[node];
    m.constCounter++;
    return node;
}

GlobalGraphMaterialization materializeGlobalToGraph(const ScbfGlobalFormula& global) {
    GlobalGraphMaterialization m;
    m.graph = std::make_unique<IncrementalDerivationGraph>();

    for (const auto& component : global.components) {
        for (const auto* node : component.localNodes) {
            if (!node) {
                continue;
            }
            auto tuple = makeGlobalSyntheticTuple(
                    "SCBF_GLOBAL_NODE_C" + std::to_string(node->componentId) +
                            "_L" + std::to_string(node->localIndex),
                    node->componentId * 1000000ULL + node->localIndex);
            NodePtr tmp = m.graph->createNode(tuple, node->isFact ? node->factProbability : 1.0);
            tmp->isFact = node->isFact;
            tmp->setProbability(node->isFact ? node->factProbability : 1.0);
            // Preserve only real query/output roots; exported targets can now be
            // reconstructed from surviving nodes or precomputed constants.
            tmp->needOutput = node->needOutput;
            m.tempByOriginalNode[node] = tmp;
            m.baseOriginByNode[tmp] =
                    GlobalTempNodeOrigin{GlobalTempNodeOriginKind::OriginalNode, node->localIndex, node};
            m.baseOriginByNodeId[tmp->getId()] = m.baseOriginByNode[tmp];
        }
    }

    for (const auto& component : global.components) {
        for (const auto* rule : component.rules) {
            if (!rule || !rule->head) {
                continue;
            }
            auto itHead = m.tempByOriginalNode.find(rule->head);
            if (itHead == m.tempByOriginalNode.end()) {
                continue;
            }
            NodePtr headNode = itHead->second;
            std::vector<NodePtr> inputs;
            std::vector<bool> negs;
            inputs.reserve(rule->bodyLiterals.size());
            negs.reserve(rule->bodyLiterals.size());
            for (const auto& lit : rule->bodyLiterals) {
                NodePtr input = nullptr;
                if (lit.node) {
                    auto itInput = m.tempByOriginalNode.find(lit.node);
                    if (itInput != m.tempByOriginalNode.end()) {
                        input = itInput->second;
                    }
                } else {
                    input = createGlobalConstHelperNode(m, lit.constantProbability);
                }
                if (!input) {
                    continue;
                }
                inputs.push_back(input);
                negs.push_back(lit.negated);
            }
            if (inputs.empty()) {
                inputs.push_back(ensureGlobalTrueHelperNode(m));
                negs.push_back(false);
            }
            EdgePtr edge = m.graph->createHyperedge(inputs, headNode, nullptr, negs, naiveRuleApplication);
            if (!edge) {
                continue;
            }
            edge->setProbability(rule->deterministic ? 1.0 : rule->edgeProbability);
        }
    }

    return m;
}

GlobalTempNodeOrigin resolveGlobalNodeOrigin(const NodePtr& node, const GlobalGraphMaterialization& m) {
    auto itBase = m.baseOriginByNode.find(node);
    if (itBase != m.baseOriginByNode.end()) {
        return itBase->second;
    }
    if (node && node->isShadow) {
        auto parsedId = parseGlobalShadowSourceNodeId(node->getTuple().relation_name);
        if (parsedId.has_value()) {
            auto itSource = m.baseOriginByNodeId.find(parsedId.value());
            if (itSource != m.baseOriginByNodeId.end()) {
                auto origin = itSource->second;
                origin.kind = GlobalTempNodeOriginKind::SyntheticShadow;
                return origin;
            }
        }
        return GlobalTempNodeOrigin{GlobalTempNodeOriginKind::SyntheticShadow, 0, nullptr};
    }
    return GlobalTempNodeOrigin{GlobalTempNodeOriginKind::SyntheticOther, 0, nullptr};
}

bool isGlobalConstantLike(const GlobalTempNodeOrigin& origin) {
    return origin.kind == GlobalTempNodeOriginKind::ConstantHelper ||
            origin.kind == GlobalTempNodeOriginKind::TrueHelper;
}

const ScbfGlobalFormulaComponent* findOriginalComponentById(
        const ScbfGlobalFormula& global, std::size_t componentId) {
    for (const auto& component : global.components) {
        if (component.componentId == componentId) {
            return &component;
        }
    }
    return nullptr;
}

ScbfGlobalFormula rebuildGlobalFromGraph(
        const ScbfGlobalFormula& original, const GlobalGraphMaterialization& m, const IncSubgraphView& view,
        ScbfGlobalFormulaRewriteStats& stats) {
    ScbfGlobalFormula rewritten;
    rewritten.program = original.program;

    std::unordered_set<NodePtr> localNodeSet;
    for (const auto& edge : view.getEdges()) {
        if (!edge) {
            continue;
        }
        NodePtr out = view.getOutput(edge);
        if (out) {
            localNodeSet.insert(out);
        }
        for (const auto& in : view.getInputs(edge)) {
            const auto origin = resolveGlobalNodeOrigin(in, m);
            if (!isGlobalConstantLike(origin)) {
                localNodeSet.insert(in);
            }
        }
    }
    const auto sortedNodes = sortGlobalRewriteNodes(localNodeSet);
    std::unordered_map<NodePtr, ScbfGlobalFormulaNode*> newByTemp;
    std::unordered_map<std::size_t, std::vector<ScbfGlobalFormulaNode*>> nodesByComponent;
    std::unordered_map<std::size_t, std::size_t> shadowCountByComponent;

    for (const auto& tmp : sortedNodes) {
        const auto origin = resolveGlobalNodeOrigin(tmp, m);
        if (isGlobalConstantLike(origin)) {
            continue;
        }
        if (!origin.originalNode) {
            throw std::runtime_error(
                    "SCBF global rewrite internal error: surviving temp node lacks original origin");
        }
        auto owned = std::make_unique<ScbfGlobalFormulaNode>();
        owned->originalNode = origin.originalNode->originalNode;
        owned->cycleId = origin.originalNode->cycleId;
        owned->topoIndex = origin.originalNode->topoIndex;
        owned->componentId = origin.originalNode->componentId;
        owned->localIndex = 0;
        owned->isFact = tmp ? tmp->isFact : false;
        owned->factProbability = (tmp && tmp->isFact) ? tmp->getProbability() : 0.0;
        owned->needOutput = tmp ? tmp->needOutput : false;
        owned->isTargetRoot = false;
        owned->isBoundaryRoot = false;
        if (origin.kind == GlobalTempNodeOriginKind::SyntheticShadow) {
            shadowCountByComponent[owned->componentId]++;
        }
        auto* ptr = owned.get();
        rewritten.instancesByOriginalNode[owned->originalNode].push_back(ptr);
        rewritten.ownedNodes.push_back(std::move(owned));
        newByTemp[tmp] = ptr;
        nodesByComponent[ptr->componentId].push_back(ptr);
    }

    std::unordered_map<std::size_t, std::size_t> componentIndexById;
    rewritten.components.reserve(original.components.size());
    for (const auto& originalComponent : original.components) {
        ScbfGlobalFormulaComponent component;
        component.cycleId = originalComponent.cycleId;
        component.topoIndex = originalComponent.topoIndex;
        component.componentId = originalComponent.componentId;
        component.target = originalComponent.target;

        auto& localNodes = nodesByComponent[originalComponent.componentId];

        std::sort(localNodes.begin(), localNodes.end(), [](const ScbfGlobalFormulaNode* a,
                                                   const ScbfGlobalFormulaNode* b) {
            if (a->isTargetRoot != b->isTargetRoot) {
                return a->isTargetRoot;
            }
            if (a->isFact != b->isFact) {
                return a->isFact;
            }
            if (a->originalNode != b->originalNode) {
                return globalNodeSortKey(a->originalNode) < globalNodeSortKey(b->originalNode);
            }
            return a < b;
        });
        for (std::size_t i = 0; i < localNodes.size(); ++i) {
            localNodes[i]->localIndex = i;
        }
        component.localNodes = localNodes;

        auto makeNodeExport = [](ScbfGlobalFormulaNode* node) {
            ScbfGlobalExportRef ref;
            ref.kind = ScbfGlobalExportRef::Kind::Node;
            ref.node = node;
            return ref;
        };
        auto makeConstExport = [](double prob) {
            ScbfGlobalExportRef ref;
            ref.kind = ScbfGlobalExportRef::Kind::Constant;
            ref.constantProbability = prob;
            return ref;
        };
        auto makeSummaryExport = [](ScbfGlobalSummaryNode* summary) {
            ScbfGlobalExportRef ref;
            ref.kind = ScbfGlobalExportRef::Kind::Summary;
            ref.summary = summary;
            return ref;
        };

        std::unordered_map<const ScbfGlobalFormulaNode*, ScbfGlobalExportRef> summaryMemo;
        std::unordered_set<const ScbfGlobalFormulaNode*> summaryBuilding;
        std::function<ScbfGlobalExportRef(const ScbfGlobalFormulaNode*, bool)> buildSummaryRef =
                [&](const ScbfGlobalFormulaNode* originalNode, bool isTargetSummary) -> ScbfGlobalExportRef {
            if (!originalNode) {
                return makeConstExport(0.0);
            }
            if (originalNode->componentId != originalComponent.componentId) {
                auto itImported = rewritten.exportedTargetsByOriginalNode.find(originalNode->originalNode);
                if (itImported == rewritten.exportedTargetsByOriginalNode.end()) {
                    throw std::runtime_error(
                            "SCBF global rewrite missing imported export summary for node " +
                            originalNode->originalNode->getTuple().toString());
                }
                return itImported->second;
            }
            auto itTemp = m.tempByOriginalNode.find(originalNode);
            if (itTemp == m.tempByOriginalNode.end()) {
                throw std::runtime_error("SCBF global rewrite missing temp node during summary rebuild");
            }
            auto itRebuilt = newByTemp.find(itTemp->second);
            if (itRebuilt != newByTemp.end()) {
                return makeNodeExport(itRebuilt->second);
            }
            auto itPre = precomputedProbResult.find(itTemp->second);
            if (itPre != precomputedProbResult.end()) {
                return makeConstExport(itPre->second);
            }
            if (itTemp->second && itTemp->second->isFact) {
                return makeConstExport(itTemp->second->getProbability());
            }
            auto itMemo = summaryMemo.find(originalNode);
            if (itMemo != summaryMemo.end()) {
                return itMemo->second;
            }
            if (!summaryBuilding.insert(originalNode).second) {
                throw std::runtime_error(
                        "SCBF global rewrite detected recursive summary rebuild for node " +
                        originalNode->originalNode->getTuple().toString());
            }

            auto ownedSummary = std::make_unique<ScbfGlobalSummaryNode>();
            ownedSummary->originalNode = originalNode->originalNode;
            ownedSummary->cycleId = originalNode->cycleId;
            ownedSummary->topoIndex = originalNode->topoIndex;
            ownedSummary->componentId = originalNode->componentId;
            ownedSummary->localIndex = originalNode->localIndex;
            ownedSummary->isTargetSummary = isTargetSummary;
            auto* summaryPtr = ownedSummary.get();
            rewritten.ownedSummaryNodes.push_back(std::move(ownedSummary));
            component.summaryNodes.push_back(summaryPtr);
            summaryMemo[originalNode] = makeSummaryExport(summaryPtr);

            for (const auto* originalRule : originalNode->incomingRules) {
                if (!originalRule) {
                    continue;
                }
                auto ownedRule = std::make_unique<ScbfGlobalSummaryRule>();
                ownedRule->sourceEdge = originalRule->sourceEdge;
                ownedRule->deterministic = originalRule->deterministic;
                ownedRule->edgeProbability = originalRule->edgeProbability;
                ownedRule->head = summaryPtr;
                for (const auto& lit : originalRule->bodyLiterals) {
                    ScbfGlobalSummaryLiteral summaryLit;
                    summaryLit.negated = lit.negated;
                    if (!lit.node) {
                        summaryLit.ref = makeConstExport(lit.constantProbability);
                    } else {
                        summaryLit.ref = buildSummaryRef(lit.node, false);
                    }
                    if (summaryLit.ref.isElided()) {
                        throw std::runtime_error(
                                "SCBF global rewrite cannot build summary from elided dependency for node " +
                                originalNode->originalNode->getTuple().toString());
                    }
                    ownedRule->bodyLiterals.push_back(summaryLit);
                }
                auto* rulePtr = ownedRule.get();
                summaryPtr->incomingRules.push_back(rulePtr);
                rewritten.ownedSummaryRules.push_back(std::move(ownedRule));
            }

            summaryBuilding.erase(originalNode);
            return summaryMemo.at(originalNode);
        };

        auto itTargetTemp = m.tempByOriginalNode.find(originalComponent.targetNode);
        if (itTargetTemp == m.tempByOriginalNode.end()) {
            throw std::runtime_error("SCBF global rewrite missing temp target during rebuild");
        }
        auto itRebuiltTarget = newByTemp.find(itTargetTemp->second);
        if (itRebuiltTarget != newByTemp.end()) {
            component.exportedValue = makeNodeExport(itRebuiltTarget->second);
            if (itRebuiltTarget->second->componentId == originalComponent.componentId) {
                component.targetNode = itRebuiltTarget->second;
            }
        } else {
            auto itPre = precomputedProbResult.find(itTargetTemp->second);
            if (itPre != precomputedProbResult.end()) {
                component.exportedValue = makeConstExport(itPre->second);
            } else if (itTargetTemp->second && itTargetTemp->second->isFact) {
                component.exportedValue = makeConstExport(itTargetTemp->second->getProbability());
            } else if (localNodes.size() == 1) {
                component.exportedValue = makeNodeExport(localNodes.front());
                component.targetNode = localNodes.front();
            } else {
                component.exportedValue = buildSummaryRef(originalComponent.targetNode, true);
                if (component.exportedValue.isNode() &&
                        component.exportedValue.node->componentId == originalComponent.componentId) {
                    component.targetNode = component.exportedValue.node;
                } else if (component.exportedValue.isElided() && originalComponent.target &&
                           !originalComponent.target->needOutput) {
                    component.exportedValue.kind = ScbfGlobalExportRef::Kind::Elided;
                } else if (component.exportedValue.isElided()) {
                    throw std::runtime_error(
                            "SCBF global rewrite could not recover output export representative for target " +
                            originalComponent.target->getTuple().toString());
                }
            }
        }

        if (component.targetNode != nullptr) {
            component.targetNode->isTargetRoot = true;
            component.targetNode->needOutput =
                    component.targetNode->needOutput || (originalComponent.target && originalComponent.target->needOutput);
            component.targetNode->isBoundaryRoot = originalComponent.targetNode
                    ? originalComponent.targetNode->isBoundaryRoot
                    : false;
        }

        rewritten.exportedTargetsByOriginalNode[component.target] = component.exportedValue;
        rewritten.rootsInTopoOrder.push_back(component.exportedValue);
        componentIndexById[component.componentId] = rewritten.components.size();
        rewritten.components.push_back(std::move(component));
    }

    std::vector<std::set<ScbfGlobalFormulaNode*>> importSets(rewritten.components.size());
    const auto sortedEdges = sortGlobalRewriteEdges(view.getEdges());
    for (const auto& edge : sortedEdges) {
        if (!edge) {
            continue;
        }
        NodePtr out = view.getOutput(edge);
        auto itHead = newByTemp.find(out);
        if (itHead == newByTemp.end()) {
            continue;
        }
        auto* headNode = itHead->second;
        auto itCompIndex = componentIndexById.find(headNode->componentId);
        if (itCompIndex == componentIndexById.end()) {
            throw std::runtime_error("SCBF global rewrite missing rebuilt component for rule head");
        }
        auto& component = rewritten.components[itCompIndex->second];

        auto ownedRule = std::make_unique<ScbfGlobalFormulaRule>();
        ownedRule->sourceEdge = edge;
        ownedRule->deterministic = edge->isDeterministic();
        ownedRule->edgeProbability = edge->isDeterministic() ? 1.0 : edge->getProbability();
        ownedRule->head = headNode;

        const auto& inputs = view.getInputs(edge);
        const auto& negs = view.getBodyNegations(edge);
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            const auto& in = inputs[i];
            const auto origin = resolveGlobalNodeOrigin(in, m);
            ScbfGlobalLiteralRef lit;
            lit.negated = i < negs.size() ? negs[i] : false;
            if (isGlobalConstantLike(origin)) {
                lit.node = nullptr;
                lit.constantProbability = in ? in->getProbability() : 0.0;
                if (origin.kind == GlobalTempNodeOriginKind::TrueHelper) {
                    lit.constantProbability = 1.0;
                }
            } else {
                auto itNode = newByTemp.find(in);
                if (itNode == newByTemp.end()) {
                    if (in && in->isFact) {
                        lit.node = nullptr;
                        lit.constantProbability = in->getProbability();
                    } else {
                        throw std::runtime_error(
                                "SCBF global rewrite internal error: missing rebuilt input node");
                    }
                } else {
                    lit.node = itNode->second;
                    if (lit.node->componentId != component.componentId) {
                        importSets[itCompIndex->second].insert(lit.node);
                    }
                }
            }
            ownedRule->bodyLiterals.push_back(lit);
        }

        auto* rulePtr = ownedRule.get();
        headNode->incomingRules.push_back(rulePtr);
        component.rules.push_back(rulePtr);
        rewritten.ownedRules.push_back(std::move(ownedRule));
    }

    for (std::size_t i = 0; i < rewritten.components.size(); ++i) {
        auto& component = rewritten.components[i];
        component.importNodes.assign(importSets[i].begin(), importSets[i].end());
        std::sort(component.importNodes.begin(), component.importNodes.end(),
                [](const ScbfGlobalFormulaNode* a, const ScbfGlobalFormulaNode* b) {
                    if (a->topoIndex != b->topoIndex) {
                        return a->topoIndex < b->topoIndex;
                    }
                    if (a->componentId != b->componentId) {
                        return a->componentId < b->componentId;
                    }
                    return globalNodeSortKey(a->originalNode) < globalNodeSortKey(b->originalNode);
                });
    }

    for (const auto& [_, count] : shadowCountByComponent) {
        stats.shadowNodesAfter += count;
    }
    return rewritten;
}

RewriteFeatureFlags buildGlobalGraphRewriteFlags(const ScbfFormulaRewriteConfig& config) {
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

}  // namespace

ScbfGlobalFormula rewriteScbfGlobalFormula(const ScbfGlobalFormula& input,
        const ScbfFormulaRewriteConfig& config, ScbfGlobalFormulaRewriteStats* stats) {
    dumpGlobalSnapshotIfEnabled(input, config, "before");

    ScbfGlobalFormulaRewriteStats localStats;
    auto& activeStats = stats ? *stats : localStats;
    activeStats = ScbfGlobalFormulaRewriteStats{};
    const auto before = countGlobal(input);
    activeStats.componentsBefore = before.components;
    activeStats.nodesBefore = before.nodes;
    activeStats.rulesBefore = before.rules;
    activeStats.literalsBefore = before.literals;
    activeStats.importsBefore = before.imports;

    const auto materializeStart = std::chrono::steady_clock::now();
    GlobalGraphMaterialization materialized = materializeGlobalToGraph(input);
    activeStats.materializeMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - materializeStart)
                                        .count();
    if (!materialized.graph) {
        throw std::runtime_error("SCBF global rewrite failed to materialize graph");
    }

    std::set<NodePtr> emptyInsertNodes;
    std::set<EdgePtr> emptyInsertEdges;
    std::set<NodePtr> emptyDeleteNodes;
    std::set<EdgePtr> emptyDeleteEdges;
    IncSubgraphView view(materialized.graph->getNodes(), materialized.graph->getEdges(), emptyInsertNodes,
            emptyInsertEdges, emptyDeleteNodes, emptyDeleteEdges);

    GraphRewriter rewriter;
    RewriteFeatureFlags flags = buildGlobalGraphRewriteFlags(config);
    const auto graphRewriteStart = std::chrono::steady_clock::now();
    const auto rwStats = rewriter.rewriteUntilFixpoint(*materialized.graph, view, false, flags);
    activeStats.graphRewriteMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - graphRewriteStart)
                                          .count();
    activeStats.graphRewriteRuns = 1;
    activeStats.graphRewriteIterations = rwStats.numIterations;
    activeStats.graphRegionsRewritten = rwStats.numRegionsRewritten;
    activeStats.graphNodesRemoved = rwStats.numNodesRemoved;
    activeStats.graphEdgesRemoved = rwStats.numEdgesRemoved;
    activeStats.graphEdgesAdded = rwStats.numEdgesAdded;
    activeStats.graphRandomVarsBefore = rwStats.randomVarsBefore;
    activeStats.graphRandomVarsAfter = rwStats.randomVarsAfter;

    const auto rebuildStart = std::chrono::steady_clock::now();
    ScbfGlobalFormula rewritten = rebuildGlobalFromGraph(input, materialized, view, activeStats);
    activeStats.rebuildMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - rebuildStart)
                                     .count();
    std::string err;
    const auto validateStart = std::chrono::steady_clock::now();
    if (!validateScbfGlobalFormula(rewritten, &err)) {
        throw std::runtime_error("validateScbfGlobalFormula failed after rewrite: " + err);
    }
    activeStats.validateMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - validateStart)
                                      .count();

    const auto after = countGlobal(rewritten);
    activeStats.componentsAfter = after.components;
    activeStats.nodesAfter = after.nodes;
    activeStats.rulesAfter = after.rules;
    activeStats.literalsAfter = after.literals;
    activeStats.importsAfter = after.imports;

    dumpGlobalSnapshotIfEnabled(rewritten, config, "after");
    return rewritten;
}

std::string summarizeScbfGlobalFormulaRewriteStats(const ScbfGlobalFormulaRewriteStats& stats) {
    std::ostringstream oss;
    oss << "[scbf-global-rewrite] materialize_ms=" << stats.materializeMs
        << " graph_rewrite_ms=" << stats.graphRewriteMs
        << " rebuild_ms=" << stats.rebuildMs
        << " validate_ms=" << stats.validateMs
        << " components_before=" << stats.componentsBefore
        << " components_after=" << stats.componentsAfter
        << " nodes_before=" << stats.nodesBefore
        << " nodes_after=" << stats.nodesAfter
        << " rules_before=" << stats.rulesBefore
        << " rules_after=" << stats.rulesAfter
        << " literals_before=" << stats.literalsBefore
        << " literals_after=" << stats.literalsAfter
        << " imports_before=" << stats.importsBefore
        << " imports_after=" << stats.importsAfter
        << " shadow_nodes_after=" << stats.shadowNodesAfter
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

#ifdef SOUFFLE_SCBF_GLOBAL_REWRITE_SMOKE_MAIN
using namespace souffle::problog;
using namespace souffle::problog::scbf;

namespace {

void require(bool cond, const std::string& msg) {
    if (!cond) {
        throw std::runtime_error(msg);
    }
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

        graph.createHyperedge({f}, a);
        graph.createHyperedge({a}, b);
        graph.createHyperedge({b}, c);

        ScbfProgram program = buildScbfProgram(graph);
        std::string err;
        require(validateScbfProgram(graph, program, &err), "invalid SCBF program: " + err);
        ScbfGlobalFormula global = buildScbfGlobalFormula(graph, program);
        require(validateScbfGlobalFormula(global, &err), "invalid SCBF global formula: " + err);

        ScbfGlobalFormulaRewriteStats stats;
        ScbfGlobalFormula rewritten = rewriteScbfGlobalFormula(global, {}, &stats);
        require(validateScbfGlobalFormula(rewritten, &err), "invalid rewritten global formula: " + err);
        require(stats.graphRegionsRewritten > 0, "expected at least one rewritten region");
        require(stats.nodesAfter <= stats.nodesBefore, "rewrite should not increase node count");
        require(stats.rulesAfter <= stats.rulesBefore, "rewrite should not increase rule count");
        require(rewritten.components.size() == global.components.size(), "target components should remain stable");
        require(rewritten.exportedTargetsByOriginalNode.size() == global.exportedTargetsByOriginalNode.size(),
                "exports should remain stable");
        std::cout << summarizeScbfGlobalFormula(global) << "\n";
        std::cout << summarizeScbfGlobalFormula(rewritten) << "\n";
        std::cout << summarizeScbfGlobalFormulaRewriteStats(stats) << "\n";
        std::cout << "[scbf-global-rewrite-smoke] ok\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[scbf-global-rewrite-smoke] failed: " << ex.what() << "\n";
        return 1;
    }
}
#endif
