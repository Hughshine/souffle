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

inline bool edgeHasNegation(const DerivationGraphViewInterface& view, EdgePtr edge) {
    const auto negs = view.getBodyNegations(edge);
    for (bool neg : negs) {
        if (neg) {
            return true;
        }
    }
    return false;
}

inline ConstAnalysisResult analyzeConstants(const DerivationGraphViewInterface& view, bool computeFalse = true) {
    ConstAnalysisResult result;
    std::queue<NodePtr> trueWork;
    std::queue<NodePtr> falseWork;
    std::unordered_map<EdgePtr, size_t> remainingTrueInputs;
    std::unordered_map<NodePtr, size_t> remainingNonFalseIncoming;
    std::unordered_set<NodePtr> hasIneligibleIncoming;
    std::vector<EdgePtr> zeroProbEdges;

    for (const auto& node : view.getNodes()) {
        if (!node || !node->isFact) {
            continue;
        }
        if (node->getProbability() == 1.0) {
            if (result.trueNodes.insert(node).second) {
                trueWork.push(node);
            }
        } else if (computeFalse && node->getProbability() == 0.0) {
            if (result.falseNodes.insert(node).second) {
                falseWork.push(node);
            }
        }
    }

    for (const auto& edge : view.getEdges()) {
        if (!edge) {
            continue;
        }
        if (edgeHasNegation(view, edge)) {
            result.ignoredEdges++;
            NodePtr out = view.getOutput(edge);
            if (out) {
                hasIneligibleIncoming.insert(out);
            }
            continue;
        }
        result.eligibleEdges++;
        NodePtr out = view.getOutput(edge);
        if (computeFalse && out) {
            remainingNonFalseIncoming[out] += 1;
        }
        if (edge->isDeterministic()) {
            size_t remaining = 0;
            for (const auto& in : view.getInputs(edge)) {
                if (!result.trueNodes.count(in)) {
                    remaining++;
                }
            }
            remainingTrueInputs.emplace(edge, remaining);
        }
        if (computeFalse && edge->getProbability() == 0.0) {
            zeroProbEdges.push_back(edge);
        }
    }

    for (const auto& [edge, remaining] : remainingTrueInputs) {
        if (remaining == 0) {
            result.trueEdges.insert(edge);
            NodePtr out = view.getOutput(edge);
            if (out && result.trueNodes.insert(out).second) {
                trueWork.push(out);
            }
        }
    }

    while (!trueWork.empty()) {
        NodePtr node = trueWork.front();
        trueWork.pop();
        for (const auto& edge : view.getOutgoingEdges(node)) {
            auto it = remainingTrueInputs.find(edge);
            if (it == remainingTrueInputs.end()) {
                continue;
            }
            if (it->second == 0) {
                continue;
            }
            it->second -= 1;
            if (it->second == 0) {
                result.trueEdges.insert(edge);
                NodePtr out = view.getOutput(edge);
                if (out && result.trueNodes.insert(out).second) {
                    trueWork.push(out);
                }
            }
        }
    }

    if (computeFalse) {
        auto maybeMarkNodeFalse = [&](NodePtr node) {
            if (!node || result.falseNodes.count(node) || result.trueNodes.count(node)) {
                return;
            }
            if (node->isFact && node->getProbability() > 0.0) {
                return;
            }
            if (hasIneligibleIncoming.count(node)) {
                return;
            }
            auto it = remainingNonFalseIncoming.find(node);
            if (it == remainingNonFalseIncoming.end() || it->second != 0) {
                return;
            }
            result.falseNodes.insert(node);
            falseWork.push(node);
        };

        auto markEdgeFalse = [&](EdgePtr edge) {
            if (!edge || result.falseEdges.count(edge)) {
                return;
            }
            if (edgeHasNegation(view, edge)) {
                return;
            }
            result.falseEdges.insert(edge);
            NodePtr out = view.getOutput(edge);
            if (!out) {
                return;
            }
            auto it = remainingNonFalseIncoming.find(out);
            if (it != remainingNonFalseIncoming.end()) {
                if (it->second > 0) {
                    it->second -= 1;
                }
                if (it->second == 0) {
                    maybeMarkNodeFalse(out);
                }
            }
        };

        for (const auto& edge : zeroProbEdges) {
            markEdgeFalse(edge);
        }

        while (!falseWork.empty()) {
            NodePtr node = falseWork.front();
            falseWork.pop();
            for (const auto& edge : view.getOutgoingEdges(node)) {
                markEdgeFalse(edge);
            }
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
