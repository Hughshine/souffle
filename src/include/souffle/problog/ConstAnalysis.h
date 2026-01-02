#pragma once

#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/formula/FormulaManager.h"

struct ConstAnalysisResult {
    std::unordered_set<NodePtr> trueNodes;
    std::unordered_set<NodePtr> falseNodes;
    std::unordered_set<EdgePtr> trueEdges;
    std::unordered_set<EdgePtr> falseEdges;
    size_t eligibleEdges = 0;
    size_t ignoredEdges = 0;
};

enum class ConstTruth {
    Unknown,
    True,
    False,
};

inline ConstTruth negateTruth(ConstTruth value) {
    if (value == ConstTruth::True) {
        return ConstTruth::False;
    }
    if (value == ConstTruth::False) {
        return ConstTruth::True;
    }
    return ConstTruth::Unknown;
}

inline ConstTruth literalTruth(ConstTruth nodeTruth, bool negated) {
    return negated ? negateTruth(nodeTruth) : nodeTruth;
}

inline ConstAnalysisResult analyzeConstants(const DerivationGraphViewInterface& view, bool computeFalse = true) {
    ConstAnalysisResult result;
    std::queue<NodePtr> trueWork;
    std::queue<NodePtr> falseWork;
    std::unordered_map<NodePtr, ConstTruth> nodeTruth;
    std::unordered_map<NodePtr, bool> hasUnknownFact;
    std::unordered_map<NodePtr, size_t> remainingNonFalseIncoming;

    struct EdgeTruthInfo {
        ConstTruth state = ConstTruth::Unknown;
        size_t unresolvedTrueLits = 0;
        size_t falseLits = 0;
        bool baseTrue = false;
        bool baseFalse = false;
    };

    std::unordered_map<EdgePtr, EdgeTruthInfo> edgeInfo;

    for (const auto& node : view.getNodes()) {
        if (!node) {
            continue;
        }
        nodeTruth.emplace(node, ConstTruth::Unknown);
        if (!node->isFact) {
            continue;
        }
        if (node->getProbability() == 1.0) {
            nodeTruth[node] = ConstTruth::True;
            result.trueNodes.insert(node);
        } else if (node->getProbability() > 0.0) {
            hasUnknownFact[node] = true;
        }
    }

    auto hasNonDetFact = [&](NodePtr node) {
        auto it = hasUnknownFact.find(node);
        return it != hasUnknownFact.end() && it->second;
    };

    auto markNodeTrue = [&](NodePtr node) {
        if (!node) {
            return;
        }
        auto it = nodeTruth.find(node);
        if (it == nodeTruth.end() || it->second != ConstTruth::Unknown) {
            return;
        }
        it->second = ConstTruth::True;
        result.trueNodes.insert(node);
        trueWork.push(node);
    };

    auto markNodeFalse = [&](NodePtr node) {
        if (!computeFalse || !node) {
            return;
        }
        auto it = nodeTruth.find(node);
        if (it == nodeTruth.end() || it->second != ConstTruth::Unknown) {
            return;
        }
        if (hasNonDetFact(node)) {
            return;
        }
        it->second = ConstTruth::False;
        result.falseNodes.insert(node);
        falseWork.push(node);
    };

    auto maybeMarkNodeFalse = [&](NodePtr node) {
        if (!computeFalse || !node) {
            return;
        }
        auto it = nodeTruth.find(node);
        if (it == nodeTruth.end() || it->second != ConstTruth::Unknown) {
            return;
        }
        if (hasNonDetFact(node)) {
            return;
        }
        auto countIt = remainingNonFalseIncoming.find(node);
        if (countIt == remainingNonFalseIncoming.end() || countIt->second == 0) {
            markNodeFalse(node);
        }
    };

    auto resolveEdge = [&](EdgePtr edge, EdgeTruthInfo& info) {
        if (!edge || info.state != ConstTruth::Unknown) {
            return;
        }
        if (computeFalse && (info.baseFalse || info.falseLits > 0)) {
            info.state = ConstTruth::False;
            result.falseEdges.insert(edge);
            NodePtr out = view.getOutput(edge);
            if (out) {
                auto it = remainingNonFalseIncoming.find(out);
                if (it != remainingNonFalseIncoming.end() && it->second > 0) {
                    it->second -= 1;
                }
                if (it == remainingNonFalseIncoming.end()) {
                    remainingNonFalseIncoming[out] = 0;
                }
                maybeMarkNodeFalse(out);
            }
            return;
        }
        if (info.baseTrue && info.unresolvedTrueLits == 0) {
            info.state = ConstTruth::True;
            result.trueEdges.insert(edge);
            NodePtr out = view.getOutput(edge);
            if (out) {
                markNodeTrue(out);
            }
        }
    };

    for (const auto& edge : view.getEdges()) {
        if (!edge) {
            continue;
        }
        NodePtr out = view.getOutput(edge);
        if (!out) {
            result.ignoredEdges++;
            continue;
        }
        result.eligibleEdges++;
        if (computeFalse) {
            remainingNonFalseIncoming[out] += 1;
        }
        EdgeTruthInfo info;
        info.baseTrue = edge->isDeterministic();
        info.baseFalse = edge->getProbability() == 0.0;
        const auto& inputs = view.getInputs(edge);
        const auto& negs = view.getBodyNegations(edge);
        for (size_t i = 0; i < inputs.size(); ++i) {
            ConstTruth inputTruth = ConstTruth::Unknown;
            auto it = nodeTruth.find(inputs[i]);
            if (it != nodeTruth.end()) {
                inputTruth = it->second;
            }
            ConstTruth lit = literalTruth(inputTruth, negs[i]);
            if (lit != ConstTruth::True) {
                info.unresolvedTrueLits += 1;
            }
            if (lit == ConstTruth::False) {
                info.falseLits += 1;
            }
        }
        edgeInfo.emplace(edge, info);
    }

    for (auto& [edge, info] : edgeInfo) {
        resolveEdge(edge, info);
    }

    if (computeFalse) {
        for (const auto& node : view.getNodes()) {
            maybeMarkNodeFalse(node);
        }
    }

    auto updateEdgesForNode = [&](NodePtr node, ConstTruth newTruth) {
        ConstTruth oldTruth = ConstTruth::Unknown;
        for (const auto& edge : view.getOutgoingEdges(node)) {
            auto it = edgeInfo.find(edge);
            if (it == edgeInfo.end()) {
                continue;
            }
            EdgeTruthInfo& info = it->second;
            if (info.state != ConstTruth::Unknown) {
                continue;
            }
            const auto& inputs = view.getInputs(edge);
            const auto& negs = view.getBodyNegations(edge);
            for (size_t i = 0; i < inputs.size(); ++i) {
                if (inputs[i] != node) {
                    continue;
                }
                ConstTruth oldLit = literalTruth(oldTruth, negs[i]);
                ConstTruth newLit = literalTruth(newTruth, negs[i]);
                if (oldLit == newLit) {
                    continue;
                }
                if (oldLit == ConstTruth::True) {
                    info.unresolvedTrueLits += 1;
                } else if (oldLit == ConstTruth::False) {
                    if (info.falseLits > 0) {
                        info.falseLits -= 1;
                    }
                }
                if (newLit == ConstTruth::True) {
                    if (info.unresolvedTrueLits > 0) {
                        info.unresolvedTrueLits -= 1;
                    }
                } else if (newLit == ConstTruth::False) {
                    info.falseLits += 1;
                }
            }
            resolveEdge(edge, info);
        }
    };

    while (!trueWork.empty() || !falseWork.empty()) {
        if (!trueWork.empty()) {
            NodePtr node = trueWork.front();
            trueWork.pop();
            updateEdgesForNode(node, ConstTruth::True);
        } else {
            NodePtr node = falseWork.front();
            falseWork.pop();
            updateEdgesForNode(node, ConstTruth::False);
        }
    }

    return result;
}

inline void logConstAnalysis(
        const ConstAnalysisResult& result,
        const DerivationGraphViewInterface& view,
        const std::string& tag) {
    std::cout << "[const-pre] tag=" << tag
              << " trueNodes=" << result.trueNodes.size()
              << " falseNodes=" << result.falseNodes.size()
              << " trueEdges=" << result.trueEdges.size()
              << " falseEdges=" << result.falseEdges.size()
              << " eligibleEdges=" << result.eligibleEdges
              << " ignoredEdges=" << result.ignoredEdges
              << " nodes=" << view.getNodes().size()
              << " edges=" << view.getEdges().size()
              << std::endl;
    if (!DerivationGraph::isConstDumpEnabled()) {
        return;
    }
    std::string filename = "const-pre-" + tag + ".txt";
    const std::string path = DerivationGraphViewInterface::qualifyDumpPath(filename);
    std::ofstream out(path);
    if (!out.is_open()) {
        std::cerr << "[const-pre] failed to open " << path << std::endl;
        return;
    }
    out << "[const-pre] tag=" << tag << "\n";
    out << "nodes=" << view.getNodes().size()
        << " edges=" << view.getEdges().size()
        << " eligibleEdges=" << result.eligibleEdges
        << " ignoredEdges=" << result.ignoredEdges << "\n";
    out << "trueNodes=" << result.trueNodes.size()
        << " falseNodes=" << result.falseNodes.size()
        << " trueEdges=" << result.trueEdges.size()
        << " falseEdges=" << result.falseEdges.size() << "\n\n";

    auto dumpNodes = [&](const std::unordered_set<NodePtr>& nodes, const std::string& header) {
        std::vector<NodePtr> sorted(nodes.begin(), nodes.end());
        std::sort(sorted.begin(), sorted.end(),
                [](const NodePtr& a, const NodePtr& b) { return a->getId() < b->getId(); });
        out << header << " count=" << sorted.size() << "\n";
        for (const auto& node : sorted) {
            out << "node " << node->getId()
                << " fact=" << (node->isFact ? 1 : 0)
                << " p=" << node->getProbability()
                << " " << node->toString() << "\n";
        }
        out << "\n";
    };

    auto dumpEdges = [&](const std::unordered_set<EdgePtr>& edges, const std::string& header) {
        std::vector<EdgePtr> sorted(edges.begin(), edges.end());
        std::sort(sorted.begin(), sorted.end(),
                [](const EdgePtr& a, const EdgePtr& b) { return a->getId() < b->getId(); });
        out << header << " count=" << sorted.size() << "\n";
        for (const auto& edge : sorted) {
            out << "edge " << edge->getId()
                << " det=" << (edge->isDeterministic() ? 1 : 0)
                << " p=" << edge->getProbability()
                << " " << edge->toString() << "\n";
        }
        out << "\n";
    };

    dumpNodes(result.trueNodes, "[true-nodes]");
    dumpNodes(result.falseNodes, "[false-nodes]");
    dumpEdges(result.trueEdges, "[true-edges]");
    dumpEdges(result.falseEdges, "[false-edges]");
}

template<typename FormulaNodeRef>
struct ConstFormulaAccess {
    const ConstAnalysisResult* info;
    FormulaManager<FormulaNodeRef>& formulaManager;

    bool edgeFormula(const EdgePtr& edge, FormulaNodeRef& out) const {
        if (!info || !edge) {
            return false;
        }
        if (info->trueEdges.count(edge)) {
            out = formulaManager.getTrue();
            return true;
        }
        if (info->falseEdges.count(edge)) {
            out = formulaManager.getFalse();
            return true;
        }
        return false;
    }

    bool nodeFormula(const NodePtr& node, FormulaNodeRef& out) const {
        if (!info || !node) {
            return false;
        }
        if (info->trueNodes.count(node)) {
            out = formulaManager.getTrue();
            return true;
        }
        if (info->falseNodes.count(node)) {
            out = formulaManager.getFalse();
            return true;
        }
        return false;
    }

    bool nodeLiteral(const NodePtr& node, bool negated, FormulaNodeRef& out) const {
        if (!info || !node) {
            return false;
        }
        if (info->trueNodes.count(node)) {
            out = negated ? formulaManager.getFalse() : formulaManager.getTrue();
            return true;
        }
        if (info->falseNodes.count(node)) {
            out = negated ? formulaManager.getTrue() : formulaManager.getFalse();
            return true;
        }
        return false;
    }

    bool inputLiteral(const std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
            const NodePtr& node, bool negated, FormulaNodeRef& out) const {
        if (nodeLiteral(node, negated, out)) {
            return true;
        }
        auto it = nodeFormulas.find(node);
        if (it == nodeFormulas.end()) {
            return false;
        }
        out = negated ? formulaManager.makeNot(it->second) : it->second;
        return true;
    }
};
