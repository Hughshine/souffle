#include "souffle/problog/scbf/ScbfGlobalFormula.h"

#ifdef SOUFFLE_SCBF_GLOBAL_FORMULA_BUNDLE_FORMULA
#ifndef SOUFFLE_SCBF_FORMULA_BUNDLE_IR
#define SOUFFLE_SCBF_FORMULA_BUNDLE_IR 1
#endif
#include "problog/scbf/ScbfFormula.cpp"
#endif

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <unordered_set>

namespace souffle::problog::scbf {
namespace {

std::string globalNodeSortKey(const NodePtr& node) {
    return node ? node->getTuple().toString() + "#" + std::to_string(node->getId()) : "<null>";
}

std::string globalNodeLabel(const NodePtr& node) {
    if (!node) {
        return "<null>";
    }
    return node->getTuple().toString() + "#" + std::to_string(node->getId());
}

std::string scbfGlobalJsonEscape(const std::string& in) {
    std::ostringstream oss;
    for (char c : in) {
        switch (c) {
            case '\\':
                oss << "\\\\";
                break;
            case '\"':
                oss << "\\\"";
                break;
            case '\n':
                oss << "\\n";
                break;
            case '\r':
                oss << "\\r";
                break;
            case '\t':
                oss << "\\t";
                break;
            default:
                oss << c;
                break;
        }
    }
    return oss.str();
}

std::string scbfGlobalDotEscape(const std::string& in) {
    std::string out;
    out.reserve(in.size() + 8);
    for (char c : in) {
        if (c == '\"' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

const ScbfStratum* findStratum(const ScbfProgram& program, std::size_t cycleId) {
    auto itTopo = program.cycleIdToTopoIndex.find(cycleId);
    if (itTopo == program.cycleIdToTopoIndex.end() || itTopo->second >= program.strata.size()) {
        return nullptr;
    }
    return &program.strata[itTopo->second];
}

std::unordered_set<NodePtr> makeNodeSet(const std::vector<NodePtr>& nodes) {
    return std::unordered_set<NodePtr>(nodes.begin(), nodes.end());
}

std::vector<ScbfGlobalFormulaNode*> collectNodesInStableOrder(const ScbfGlobalFormula& global) {
    std::vector<ScbfGlobalFormulaNode*> ordered;
    ordered.reserve(global.ownedNodes.size());
    for (const auto& component : global.components) {
        for (const auto* node : component.localNodes) {
            ordered.push_back(const_cast<ScbfGlobalFormulaNode*>(node));
        }
    }
    return ordered;
}

std::vector<ScbfGlobalSummaryNode*> collectSummaryNodesInStableOrder(const ScbfGlobalFormula& global) {
    std::vector<ScbfGlobalSummaryNode*> ordered;
    ordered.reserve(global.ownedSummaryNodes.size());
    for (const auto& component : global.components) {
        for (const auto* node : component.summaryNodes) {
            ordered.push_back(const_cast<ScbfGlobalSummaryNode*>(node));
        }
    }
    return ordered;
}

std::string summaryNodeLabel(const ScbfGlobalSummaryNode* node) {
    if (!node) {
        return "<null-summary>";
    }
    return globalNodeLabel(node->originalNode) + "/summary";
}

}  // namespace

ScbfGlobalFormula buildScbfGlobalFormula(
        const DerivationGraphViewInterface& view,
        const ScbfProgram& program) {
    ScbfGlobalFormula global;
    global.program = program;

    std::size_t componentId = 0;
    for (const auto cycleId : program.topoOrderCycleIds) {
        const auto* stratum = findStratum(program, cycleId);
        if (!stratum) {
            continue;
        }
        const auto boundaryRoots = makeNodeSet(stratum->boundaryOutNodes);
        const auto bundle = buildScbfStratumFormulaBundle(view, program, cycleId);
        for (const auto& target : bundle.targets) {
            if (!target.target || target.targetLocalIndex >= target.localNodes.size()) {
                continue;
            }

            ScbfGlobalFormulaComponent component;
            component.cycleId = bundle.cycleId;
            component.topoIndex = bundle.topoIndex;
            component.componentId = componentId;
            component.target = target.target;

            std::vector<ScbfGlobalFormulaNode*> locals;
            locals.reserve(target.localNodes.size());
            for (std::size_t i = 0; i < target.localNodes.size(); ++i) {
                const auto& local = target.localNodes[i];
                auto owned = std::make_unique<ScbfGlobalFormulaNode>();
                owned->originalNode = local.node;
                owned->cycleId = bundle.cycleId;
                owned->topoIndex = bundle.topoIndex;
                owned->componentId = componentId;
                owned->localIndex = i;
                owned->isFact = local.isFact;
                owned->factProbability = local.factProbability;
                owned->needOutput = local.needOutput;
                owned->isTargetRoot = (i == target.targetLocalIndex);
                owned->isBoundaryRoot = owned->isTargetRoot && boundaryRoots.count(target.target) > 0;
                auto* ptr = owned.get();
                global.instancesByOriginalNode[local.node].push_back(ptr);
                global.ownedNodes.push_back(std::move(owned));
                locals.push_back(ptr);
            }

            component.localNodes = locals;
            component.targetNode = locals[target.targetLocalIndex];
            component.targetNode->needOutput = component.targetNode->needOutput || target.target->needOutput;
            component.exportedValue.kind = ScbfGlobalExportRef::Kind::Node;
            component.exportedValue.node = component.targetNode;
            global.exportedTargetsByOriginalNode[target.target] = component.exportedValue;
            global.rootsInTopoOrder.push_back(component.exportedValue);

            std::set<ScbfGlobalFormulaNode*> importSet;
            for (std::size_t head = 0; head < target.localNodeRules.size(); ++head) {
                if (head >= locals.size()) {
                    continue;
                }
                auto* headNode = locals[head];
                for (const auto& rule : target.localNodeRules[head]) {
                    auto ownedRule = std::make_unique<ScbfGlobalFormulaRule>();
                    ownedRule->sourceEdge = rule.sourceEdge;
                    ownedRule->deterministic = rule.deterministic;
                    ownedRule->edgeProbability = rule.edgeProbability;
                    ownedRule->head = headNode;

                    for (const auto& lit : rule.bodyLiterals) {
                        ScbfGlobalLiteralRef stitched;
                        stitched.negated = lit.negated;
                        if (lit.source == ScbfLiteralSource::LocalNode) {
                            if (lit.index >= locals.size()) {
                                throw std::runtime_error("SCBF global formula local literal index out of range");
                            }
                            stitched.node = locals[lit.index];
                        } else if (lit.source == ScbfLiteralSource::ImportNode) {
                            if (lit.index >= target.imports.size()) {
                                throw std::runtime_error("SCBF global formula import literal index out of range");
                            }
                            const auto importNode = target.imports[lit.index].node;
                            auto itImport = global.exportedTargetsByOriginalNode.find(importNode);
                            if (itImport == global.exportedTargetsByOriginalNode.end()) {
                                throw std::runtime_error(
                                        "SCBF global formula missing stitched import target for " +
                                        globalNodeLabel(importNode));
                            }
                            if (itImport->second.isConstant()) {
                                stitched.node = nullptr;
                                stitched.constantProbability = itImport->second.constantProbability;
                            } else if (itImport->second.isElided()) {
                                throw std::runtime_error(
                                        "SCBF global formula encountered elided import for " +
                                        globalNodeLabel(importNode));
                            } else {
                                stitched.node = itImport->second.node;
                                importSet.insert(itImport->second.node);
                            }
                        } else {
                            stitched.constantProbability = lit.constantProbability;
                        }
                        ownedRule->bodyLiterals.push_back(stitched);
                    }

                    auto* rulePtr = ownedRule.get();
                    headNode->incomingRules.push_back(rulePtr);
                    component.rules.push_back(rulePtr);
                    global.ownedRules.push_back(std::move(ownedRule));
                }
            }

            component.importNodes.assign(importSet.begin(), importSet.end());
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

            global.components.push_back(std::move(component));
            componentId++;
        }
    }
    return global;
}

ScbfGlobalFormula buildScbfGlobalFormula(const DerivationGraphViewInterface& view) {
    return buildScbfGlobalFormula(view, buildScbfProgram(view));
}

bool validateScbfGlobalFormula(const ScbfGlobalFormula& global, std::string* errorMessage) {
    auto fail = [&](const std::string& msg) {
        if (errorMessage) {
            *errorMessage = msg;
        }
        return false;
    };
    auto validateExportRef = [&](const ScbfGlobalExportRef& exported, const std::string& context,
                                 const std::unordered_map<const ScbfGlobalFormulaNode*,
                                         const ScbfGlobalFormulaComponent*>& ownerByNode,
                                 const std::unordered_map<const ScbfGlobalSummaryNode*,
                                         const ScbfGlobalFormulaComponent*>& ownerBySummary) {
        if (exported.isConstant()) {
            if (exported.constantProbability < 0.0 || exported.constantProbability > 1.0) {
                return fail(context + " constant probability out of [0,1]");
            }
            return true;
        }
        if (exported.isElided()) {
            return true;
        }
        if (exported.isNode()) {
            if (exported.node == nullptr) {
                return fail(context + " node export is null");
            }
            if (!ownerByNode.count(exported.node)) {
                return fail(context + " node export has no owner");
            }
            return true;
        }
        if (exported.isSummary()) {
            if (exported.summary == nullptr) {
                return fail(context + " summary export is null");
            }
            if (!ownerBySummary.count(exported.summary)) {
                return fail(context + " summary export has no owner");
            }
            return true;
        }
        return fail(context + " has unknown export kind");
    };

    if (global.components.empty()) {
        return fail("global formula has no components");
    }

    std::unordered_map<const ScbfGlobalFormulaNode*, const ScbfGlobalFormulaComponent*> ownerByNode;
    std::unordered_map<const ScbfGlobalSummaryNode*, const ScbfGlobalFormulaComponent*> ownerBySummary;
    std::unordered_set<const ScbfGlobalFormulaComponent*> componentsWithTargetNode;
    for (const auto& component : global.components) {
        if (!component.target) {
            return fail("component missing target");
        }
        for (const auto* node : component.localNodes) {
            if (!node) {
                return fail("null local node in component");
            }
            ownerByNode[node] = &component;
            if (node == component.targetNode) {
                componentsWithTargetNode.insert(&component);
            }
            if (node->componentId != component.componentId) {
                return fail("node/component id mismatch");
            }
            if (node->cycleId != component.cycleId || node->topoIndex != component.topoIndex) {
                return fail("node cycle/topo mismatch with component");
            }
        }
        for (const auto* node : component.summaryNodes) {
            if (!node) {
                return fail("null summary node in component");
            }
            ownerBySummary[node] = &component;
            if (node->componentId != component.componentId) {
                return fail("summary/component id mismatch");
            }
            if (node->cycleId != component.cycleId || node->topoIndex != component.topoIndex) {
                return fail("summary cycle/topo mismatch with component");
            }
        }
    }

    for (const auto& component : global.components) {
        if (!validateExportRef(
                    component.exportedValue, "component export", ownerByNode, ownerBySummary)) {
            return false;
        }
        if (component.targetNode != nullptr) {
            if (!componentsWithTargetNode.count(&component) || !component.targetNode->isTargetRoot) {
                return fail("component targetNode not marked as target root");
            }
            if (!component.exportedValue.isNode() || component.exportedValue.node != component.targetNode) {
                return fail("component targetNode/export mismatch");
            }
        } else if (component.exportedValue.isNode()) {
            auto itExportOwner = ownerByNode.find(component.exportedValue.node);
            if (itExportOwner == ownerByNode.end()) {
                return fail("component export node missing owning component");
            }
            if (itExportOwner->second->topoIndex > component.topoIndex) {
                return fail("component export node points to later stratum");
            }
        } else if (component.exportedValue.isSummary()) {
            auto itExportOwner = ownerBySummary.find(component.exportedValue.summary);
            if (itExportOwner == ownerBySummary.end()) {
                return fail("component export summary missing owning component");
            }
            if (itExportOwner->second->topoIndex > component.topoIndex) {
                return fail("component export summary points to later stratum");
            }
        } else if (component.exportedValue.isElided() && component.target && component.target->needOutput) {
            return fail("output component cannot have elided export");
        }
        for (const auto* rule : component.rules) {
            if (!rule || !rule->head) {
                return fail("component contains null rule/head");
            }
            auto itHeadOwner = ownerByNode.find(rule->head);
            if (itHeadOwner == ownerByNode.end() || itHeadOwner->second->componentId != component.componentId) {
                return fail("rule head not owned by component");
            }
            for (const auto& lit : rule->bodyLiterals) {
                if (!lit.node) {
                    if (lit.constantProbability < 0.0 || lit.constantProbability > 1.0) {
                        return fail("constant literal probability out of [0,1]");
                    }
                    continue;
                }
                auto itBodyOwner = ownerByNode.find(lit.node);
                if (itBodyOwner == ownerByNode.end()) {
                    return fail("literal node missing owning component");
                }
                if (itBodyOwner->second->componentId != component.componentId &&
                        itBodyOwner->second->topoIndex >= component.topoIndex) {
                    return fail("cross-component literal does not point to earlier stratum");
                }
            }
        }
        for (const auto* summary : component.summaryNodes) {
            if (!summary) {
                return fail("component contains null summary node");
            }
            for (const auto* rule : summary->incomingRules) {
                if (!rule || rule->head != summary) {
                    return fail("summary contains null rule/head mismatch");
                }
                for (const auto& lit : rule->bodyLiterals) {
                    if (!validateExportRef(lit.ref, "summary literal", ownerByNode, ownerBySummary)) {
                        return false;
                    }
                    if (lit.ref.isNode() && ownerByNode.at(lit.ref.node)->topoIndex > component.topoIndex) {
                        return fail("summary literal node points to later stratum");
                    }
                    if (lit.ref.isSummary() &&
                            ownerBySummary.at(lit.ref.summary)->topoIndex > component.topoIndex) {
                        return fail("summary literal summary points to later stratum");
                    }
                }
            }
        }
    }

    for (const auto& [node, exported] : global.exportedTargetsByOriginalNode) {
        if (!node) {
            return fail("null exported target mapping");
        }
        if (!validateExportRef(exported, "exported target mapping", ownerByNode, ownerBySummary)) {
            return false;
        }
    }
    return true;
}

std::string summarizeScbfGlobalFormula(const ScbfGlobalFormula& global) {
    std::size_t totalLiterals = 0;
    std::size_t crossRefs = 0;
    std::size_t boundaryRoots = 0;
    std::size_t summaryRules = 0;
    std::size_t summaryLiterals = 0;
    std::size_t exportNodes = 0;
    std::size_t exportConstants = 0;
    std::size_t exportSummaries = 0;
    std::size_t exportElided = 0;
    for (const auto& component : global.components) {
        for (const auto* node : component.localNodes) {
            if (node && node->isBoundaryRoot) {
                boundaryRoots++;
            }
        }
        for (const auto* rule : component.rules) {
            if (!rule) {
                continue;
            }
            totalLiterals += rule->bodyLiterals.size();
            for (const auto& lit : rule->bodyLiterals) {
                if (lit.node && lit.node->componentId != component.componentId) {
                    crossRefs++;
                }
            }
        }
        for (const auto* summary : component.summaryNodes) {
            if (!summary) {
                continue;
            }
            summaryRules += summary->incomingRules.size();
            for (const auto* rule : summary->incomingRules) {
                if (rule) {
                    summaryLiterals += rule->bodyLiterals.size();
                }
            }
        }
        if (component.exportedValue.isNode()) {
            exportNodes++;
        } else if (component.exportedValue.isConstant()) {
            exportConstants++;
        } else if (component.exportedValue.isSummary()) {
            exportSummaries++;
        } else if (component.exportedValue.isElided()) {
            exportElided++;
        }
    }

    std::ostringstream oss;
    oss << "[scbf-global-formula] strata=" << global.program.strata.size()
        << " components=" << global.components.size()
        << " nodes=" << global.ownedNodes.size()
        << " rules=" << global.ownedRules.size()
        << " literals=" << totalLiterals
        << " summary_nodes=" << global.ownedSummaryNodes.size()
        << " summary_rules=" << summaryRules
        << " summary_literals=" << summaryLiterals
        << " cross_component_refs=" << crossRefs
        << " boundary_roots=" << boundaryRoots
        << " exports=" << global.exportedTargetsByOriginalNode.size()
        << " export_nodes=" << exportNodes
        << " export_constants=" << exportConstants
        << " export_summaries=" << exportSummaries
        << " export_elided=" << exportElided;
    return oss.str();
}

std::string toScbfGlobalFormulaJson(const ScbfGlobalFormula& global) {
    const auto orderedNodes = collectNodesInStableOrder(global);
    const auto orderedSummaryNodes = collectSummaryNodesInStableOrder(global);
    std::unordered_map<const ScbfGlobalFormulaNode*, std::size_t> nodeIds;
    std::unordered_map<const ScbfGlobalSummaryNode*, std::size_t> summaryIds;
    nodeIds.reserve(orderedNodes.size());
    for (std::size_t i = 0; i < orderedNodes.size(); ++i) {
        nodeIds[orderedNodes[i]] = i;
    }
    summaryIds.reserve(orderedSummaryNodes.size());
    for (std::size_t i = 0; i < orderedSummaryNodes.size(); ++i) {
        summaryIds[orderedSummaryNodes[i]] = i;
    }

    std::ostringstream oss;
    oss << "{";
    oss << "\"strata\":" << global.program.strata.size() << ",";
    oss << "\"components\":[";
    for (std::size_t ci = 0; ci < global.components.size(); ++ci) {
        const auto& component = global.components[ci];
        if (ci > 0) {
            oss << ",";
        }
        oss << "{";
        oss << "\"componentId\":" << component.componentId << ",";
        oss << "\"cycleId\":" << component.cycleId << ",";
        oss << "\"topoIndex\":" << component.topoIndex << ",";
        oss << "\"target\":\"" << scbfGlobalJsonEscape(globalNodeLabel(component.target)) << "\",";
        oss << "\"targetNodeId\":";
        if (component.targetNode != nullptr) {
            oss << nodeIds.at(component.targetNode);
        } else {
            oss << -1;
        }
        oss << ",";
        oss << "\"export\":{";
        if (component.exportedValue.isConstant()) {
            oss << "\"kind\":\"constant\",";
            oss << "\"nodeId\":-1,";
            oss << "\"summaryId\":-1,";
            oss << "\"constantProbability\":" << component.exportedValue.constantProbability;
        } else if (component.exportedValue.isSummary()) {
            oss << "\"kind\":\"summary\",";
            oss << "\"nodeId\":-1,";
            oss << "\"summaryId\":" << summaryIds.at(component.exportedValue.summary) << ",";
            oss << "\"constantProbability\":0";
        } else if (component.exportedValue.isElided()) {
            oss << "\"kind\":\"elided\",";
            oss << "\"nodeId\":-1,";
            oss << "\"summaryId\":-1,";
            oss << "\"constantProbability\":0";
        } else {
            oss << "\"kind\":\"node\",";
            oss << "\"nodeId\":" << nodeIds.at(component.exportedValue.node) << ",";
            oss << "\"summaryId\":-1,";
            oss << "\"constantProbability\":0";
        }
        oss << "},";
        oss << "\"localNodes\":[";
        for (std::size_t i = 0; i < component.localNodes.size(); ++i) {
            const auto* node = component.localNodes[i];
            if (i > 0) {
                oss << ",";
            }
            oss << "{";
            oss << "\"nodeId\":" << nodeIds.at(node) << ",";
            oss << "\"node\":\"" << scbfGlobalJsonEscape(globalNodeLabel(node->originalNode)) << "\",";
            oss << "\"localIndex\":" << node->localIndex << ",";
            oss << "\"isFact\":" << (node->isFact ? "true" : "false") << ",";
            oss << "\"factProbability\":" << node->factProbability << ",";
            oss << "\"needOutput\":" << (node->needOutput ? "true" : "false") << ",";
            oss << "\"isTargetRoot\":" << (node->isTargetRoot ? "true" : "false") << ",";
            oss << "\"isBoundaryRoot\":" << (node->isBoundaryRoot ? "true" : "false");
            oss << "}";
        }
        oss << "],";
        oss << "\"imports\":[";
        for (std::size_t i = 0; i < component.importNodes.size(); ++i) {
            const auto* node = component.importNodes[i];
            if (i > 0) {
                oss << ",";
            }
            oss << "{";
            oss << "\"nodeId\":" << nodeIds.at(node) << ",";
            oss << "\"node\":\"" << scbfGlobalJsonEscape(globalNodeLabel(node->originalNode)) << "\",";
            oss << "\"producerTopoIndex\":" << node->topoIndex;
            oss << "}";
        }
        oss << "],";
        oss << "\"summaryNodes\":[";
        for (std::size_t i = 0; i < component.summaryNodes.size(); ++i) {
            const auto* node = component.summaryNodes[i];
            if (i > 0) {
                oss << ",";
            }
            oss << "{";
            oss << "\"summaryId\":" << summaryIds.at(node) << ",";
            oss << "\"node\":\"" << scbfGlobalJsonEscape(globalNodeLabel(node->originalNode)) << "\",";
            oss << "\"localIndex\":" << node->localIndex << ",";
            oss << "\"isTargetSummary\":" << (node->isTargetSummary ? "true" : "false") << ",";
            oss << "\"rules\":[";
            for (std::size_t ri = 0; ri < node->incomingRules.size(); ++ri) {
                const auto* rule = node->incomingRules[ri];
                if (ri > 0) {
                    oss << ",";
                }
                oss << "{";
                oss << "\"sourceEdgeId\":" << (rule->sourceEdge ? rule->sourceEdge->getId() : 0) << ",";
                oss << "\"deterministic\":" << (rule->deterministic ? "true" : "false") << ",";
                oss << "\"edgeProbability\":" << rule->edgeProbability << ",";
                oss << "\"literals\":[";
                for (std::size_t li = 0; li < rule->bodyLiterals.size(); ++li) {
                    const auto& lit = rule->bodyLiterals[li];
                    if (li > 0) {
                        oss << ",";
                    }
                    oss << "{";
                    if (lit.ref.isNode()) {
                        oss << "\"kind\":\"node\",";
                        oss << "\"nodeId\":" << nodeIds.at(lit.ref.node) << ",";
                        oss << "\"summaryId\":-1,";
                        oss << "\"node\":\"" << scbfGlobalJsonEscape(globalNodeLabel(lit.ref.node->originalNode))
                            << "\",";
                        oss << "\"constantProbability\":0";
                    } else if (lit.ref.isSummary()) {
                        oss << "\"kind\":\"summary\",";
                        oss << "\"nodeId\":-1,";
                        oss << "\"summaryId\":" << summaryIds.at(lit.ref.summary) << ",";
                        oss << "\"node\":\"" << scbfGlobalJsonEscape(summaryNodeLabel(lit.ref.summary)) << "\",";
                        oss << "\"constantProbability\":0";
                    } else {
                        oss << "\"kind\":\"constant\",";
                        oss << "\"nodeId\":-1,";
                        oss << "\"summaryId\":-1,";
                        oss << "\"node\":\"<const>\",";
                        oss << "\"constantProbability\":" << lit.ref.constantProbability;
                    }
                    oss << ",";
                    oss << "\"negated\":" << (lit.negated ? "true" : "false");
                    oss << "}";
                }
                oss << "]";
                oss << "}";
            }
            oss << "]";
            oss << "}";
        }
        oss << "],";
        oss << "\"rules\":[";
        for (std::size_t ri = 0; ri < component.rules.size(); ++ri) {
            const auto* rule = component.rules[ri];
            if (ri > 0) {
                oss << ",";
            }
            oss << "{";
            oss << "\"headNodeId\":" << nodeIds.at(rule->head) << ",";
            oss << "\"sourceEdgeId\":" << (rule->sourceEdge ? rule->sourceEdge->getId() : 0) << ",";
            oss << "\"deterministic\":" << (rule->deterministic ? "true" : "false") << ",";
            oss << "\"edgeProbability\":" << rule->edgeProbability << ",";
            oss << "\"literals\":[";
            for (std::size_t li = 0; li < rule->bodyLiterals.size(); ++li) {
                const auto& lit = rule->bodyLiterals[li];
                if (li > 0) {
                    oss << ",";
                }
                oss << "{";
                if (lit.node) {
                    oss << "\"kind\":\"node\",";
                    oss << "\"nodeId\":" << nodeIds.at(lit.node) << ",";
                    oss << "\"node\":\"" << scbfGlobalJsonEscape(globalNodeLabel(lit.node->originalNode)) << "\",";
                    oss << "\"constantProbability\":0,";
                    oss << "\"producerComponentId\":" << lit.node->componentId << ",";
                    oss << "\"producerTopoIndex\":" << lit.node->topoIndex << ",";
                } else {
                    oss << "\"kind\":\"constant\",";
                    oss << "\"nodeId\":-1,";
                    oss << "\"node\":\"<const>\",";
                    oss << "\"constantProbability\":" << lit.constantProbability << ",";
                    oss << "\"producerComponentId\":-1,";
                    oss << "\"producerTopoIndex\":-1,";
                }
                oss << "\"negated\":" << (lit.negated ? "true" : "false");
                oss << "}";
            }
            oss << "]";
            oss << "}";
        }
        oss << "]";
        oss << "}";
    }
    oss << "]";
    oss << "}";
    return oss.str();
}

bool dumpScbfGlobalFormulaJson(const ScbfGlobalFormula& global, const std::string& outputPath) {
    std::ofstream out(outputPath);
    if (!out.is_open()) {
        return false;
    }
    out << toScbfGlobalFormulaJson(global) << "\n";
    out.close();
    return true;
}

bool dumpScbfGlobalFormulaDot(const ScbfGlobalFormula& global, const std::string& outputPath) {
    const auto orderedNodes = collectNodesInStableOrder(global);
    const auto orderedSummaryNodes = collectSummaryNodesInStableOrder(global);
    std::unordered_map<const ScbfGlobalFormulaNode*, std::size_t> nodeIds;
    std::unordered_map<const ScbfGlobalSummaryNode*, std::size_t> summaryIds;
    nodeIds.reserve(orderedNodes.size());
    for (std::size_t i = 0; i < orderedNodes.size(); ++i) {
        nodeIds[orderedNodes[i]] = i;
    }
    summaryIds.reserve(orderedSummaryNodes.size());
    for (std::size_t i = 0; i < orderedSummaryNodes.size(); ++i) {
        summaryIds[orderedSummaryNodes[i]] = i;
    }

    std::ofstream out(outputPath);
    if (!out.is_open()) {
        return false;
    }

    out << "digraph scbf_global_formula {\n";
    out << "  rankdir=LR;\n";
    out << "  graph [label=\"SCBF global stitched formula\", labelloc=t, fontsize=20];\n";

    for (const auto& component : global.components) {
        out << "  subgraph cluster_c" << component.componentId << " {\n";
        out << "    label=\"component " << component.componentId
            << " cycle " << component.cycleId
            << " topo " << component.topoIndex
            << " target " << scbfGlobalDotEscape(globalNodeLabel(component.target));
        if (component.exportedValue.isConstant()) {
            out << " export const=" << component.exportedValue.constantProbability;
        } else if (component.exportedValue.isSummary()) {
            out << " export summary=" << summaryIds.at(component.exportedValue.summary);
        } else if (component.exportedValue.isElided()) {
            out << " export elided";
        } else if (component.exportedValue.node != nullptr) {
            out << " export->" << scbfGlobalDotEscape(globalNodeLabel(component.exportedValue.node->originalNode));
        }
        out << "\";\n";
        out << "    color=lightgrey;\n";
        for (const auto* node : component.localNodes) {
            out << "    n" << nodeIds.at(node) << " [shape=ellipse,label=\""
                << scbfGlobalDotEscape(globalNodeLabel(node->originalNode)) << "\\n"
                << "L" << node->localIndex
                << (node->isFact ? "\\nfact" : "\\nderived")
                << (node->isTargetRoot ? "\\nTARGET" : "")
                << (node->isBoundaryRoot ? "\\nBOUNDARY" : "")
                << "\"];\n";
        }
        for (const auto* node : component.summaryNodes) {
            out << "    s" << summaryIds.at(node) << " [shape=hexagon,label=\""
                << scbfGlobalDotEscape(summaryNodeLabel(node)) << "\\nS" << node->localIndex
                << (node->isTargetSummary ? "\\nTARGET-SUMMARY" : "") << "\"];\n";
        }
        for (std::size_t ri = 0; ri < component.rules.size(); ++ri) {
            const auto* rule = component.rules[ri];
            out << "    r" << component.componentId << "_" << ri << " [shape=diamond,label=\"p="
                << rule->edgeProbability << "\"];\n";
            out << "    r" << component.componentId << "_" << ri << " -> n" << nodeIds.at(rule->head) << ";\n";
            for (std::size_t li = 0; li < rule->bodyLiterals.size(); ++li) {
                const auto& lit = rule->bodyLiterals[li];
                if (lit.node) {
                    out << "    n" << nodeIds.at(lit.node) << " -> r" << component.componentId << "_" << ri
                        << " [label=\"" << (lit.negated ? "!" : "") << "\"];\n";
                } else {
                    out << "    c" << component.componentId << "_" << ri << "_" << li
                        << " [shape=box,style=dashed,label=\"const\\n" << lit.constantProbability << "\"];\n";
                    out << "    c" << component.componentId << "_" << ri << "_" << li
                        << " -> r" << component.componentId << "_" << ri
                        << " [label=\"" << (lit.negated ? "!" : "") << "\"];\n";
                }
            }
        }
        for (std::size_t ri = 0; ri < component.summaryNodes.size(); ++ri) {
            const auto* summary = component.summaryNodes[ri];
            for (std::size_t rj = 0; rj < summary->incomingRules.size(); ++rj) {
                const auto* rule = summary->incomingRules[rj];
                out << "    sr" << component.componentId << "_" << ri << "_" << rj
                    << " [shape=diamond,label=\"p=" << rule->edgeProbability << "\"];\n";
                out << "    sr" << component.componentId << "_" << ri << "_" << rj
                    << " -> s" << summaryIds.at(summary) << ";\n";
                for (std::size_t li = 0; li < rule->bodyLiterals.size(); ++li) {
                    const auto& lit = rule->bodyLiterals[li];
                    if (lit.ref.isNode()) {
                        out << "    n" << nodeIds.at(lit.ref.node) << " -> sr" << component.componentId << "_"
                            << ri << "_" << rj << " [label=\"" << (lit.negated ? "!" : "") << "\"];\n";
                    } else if (lit.ref.isSummary()) {
                        out << "    s" << summaryIds.at(lit.ref.summary) << " -> sr" << component.componentId
                            << "_" << ri << "_" << rj << " [label=\"" << (lit.negated ? "!" : "") << "\"];\n";
                    } else {
                        out << "    sc" << component.componentId << "_" << ri << "_" << rj << "_" << li
                            << " [shape=box,style=dashed,label=\"const\\n" << lit.ref.constantProbability
                            << "\"];\n";
                        out << "    sc" << component.componentId << "_" << ri << "_" << rj << "_" << li
                            << " -> sr" << component.componentId << "_" << ri << "_" << rj
                            << " [label=\"" << (lit.negated ? "!" : "") << "\"];\n";
                    }
                }
            }
        }
        out << "  }\n";
    }

    out << "}\n";
    out.close();
    return true;
}

}  // namespace souffle::problog::scbf

#ifdef SOUFFLE_SCBF_GLOBAL_FORMULA_SMOKE_MAIN
using namespace souffle::problog;
using namespace souffle::problog::scbf;

namespace {

void require(bool cond, const std::string& msg) {
    if (!cond) {
        throw std::runtime_error(msg);
    }
}

ScbfGlobalFormulaComponent* findComponentByRelation(
        ScbfGlobalFormula& global, const std::string& relationName) {
    for (auto& component : global.components) {
        if (component.target && component.target->getTuple().relation_name == relationName) {
            return &component;
        }
    }
    return nullptr;
}

}  // namespace

int main() {
    try {
        DerivationGraph graph;
        NodePtr f = graph.createNode(UntypedTuple{"f", {1}}, 1.0);
        f->isFact = true;
        NodePtr a = graph.createNode(UntypedTuple{"a", {1}}, 1.0);
        NodePtr b = graph.createNode(UntypedTuple{"b", {1}}, 1.0);
        NodePtr c = graph.createNode(UntypedTuple{"c", {1}}, 1.0);
        c->setQuery();

        graph.createHyperedge({f}, a);
        graph.createHyperedge({a}, b);
        graph.createHyperedge({b}, a);
        graph.createHyperedge({b}, c);

        ScbfProgram program = buildScbfProgram(graph);
        std::string err;
        require(validateScbfProgram(graph, program, &err), "invalid SCBF program: " + err);

        ScbfGlobalFormula global = buildScbfGlobalFormula(graph, program);
        require(validateScbfGlobalFormula(global, &err), "invalid global formula: " + err);
        std::filesystem::create_directories("/tmp/scbf-global-formula-dump");
        require(dumpScbfGlobalFormulaJson(global, "/tmp/scbf-global-formula-dump/smoke.json"),
                "failed to dump global formula json");
        require(dumpScbfGlobalFormulaDot(global, "/tmp/scbf-global-formula-dump/smoke.dot"),
                "failed to dump global formula dot");

        auto* compB = findComponentByRelation(global, "b");
        auto* compC = findComponentByRelation(global, "c");
        require(compB != nullptr && compC != nullptr, "failed to find expected components");
        require(!compC->rules.empty(), "component c should have at least one rule");
        require(!compC->rules.front()->bodyLiterals.empty(), "component c rule should have a body");
        require(compC->rules.front()->bodyLiterals.front().node == compB->targetNode,
                "component c should stitch directly to component b target");

        std::cout << summarizeScbfProgram(program) << "\n";
        std::cout << summarizeScbfGlobalFormula(global) << "\n";
        std::cout << "[scbf-global-formula-smoke] ok components=" << global.components.size() << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[scbf-global-formula-smoke] failed: " << ex.what() << "\n";
        return 1;
    }
}
#endif
