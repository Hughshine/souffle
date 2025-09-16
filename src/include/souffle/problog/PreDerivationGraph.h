//
// Created by lxy10 on 9/1/2025.
//

#ifndef PREDERIVATIONGRAPH_H
#define PREDERIVATIONGRAPH_H

#pragma once
#include <vector>
#include <deque>
#include <string>
#include <sstream>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <algorithm>
#include <cassert>

// 依赖 DerivationGraph 做 materialize（只在 .h 中使用也没问题）
#include "souffle/Derivation.h"
#include "DerivationGraph.h"  // NodePtr/EdgePtr/DerivationGraph/UntypedTuple

using NodeId = std::size_t;
using EdgeId = std::size_t;

/** 节点键：关系名 + 实参（使用 RamDomain，与 UntypedTuple 保持一致） */
struct AtomKey {
    std::string rel;
    std::vector<souffle::RamDomain> args;

    bool operator==(const AtomKey& o) const noexcept {
        if (rel != o.rel || args.size() != o.args.size()) return false;
        for (std::size_t i = 0; i < args.size(); ++i) if (args[i] != o.args[i]) return false;
        return true;
    }

    std::string toString() const {
        std::ostringstream os;
        os << rel << "(";
        for (std::size_t i = 0; i < args.size(); ++i) {
            os << args[i];
            if (i + 1 < args.size()) os << ",";
        }
        os << ")";
        return os.str();
    }
};

struct AtomKeyHash {
    std::size_t operator()(const AtomKey& k) const noexcept {
        std::hash<std::string> Hs;
        std::hash<souffle::RamDomain> Hd;
        std::size_t h = Hs(k.rel);
        for (auto v : k.args) {
            // 混合哈希
            h ^= (Hd(v) + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2));
        }
        return h;
    }
};

/**
 * 预设图（header-only）
 * - Node = ground 原子
 * - Hyperedge = ground 规则（多输入一输出）
 * - 支持：多次 seed 输入、O(V+E) 传播、剪枝视图（不真正删除边/点）、动态增删边、导出 DOT
 * - materialize(DerivationGraph&): 将当前视图落到工程内已有 DerivationGraph
 */
class PreDerivationGraph {
public:
    size_t currentTurn_ = 0;

    struct Edge {
        std::vector<NodeId> inputs;
        NodeId output{};
        bool enabled{true};
        std::vector<bool> bodyNegations;
        double probability{1.0};
    };

    /** —— 剪枝视图：只读包装 —— */
    class DerivationGraphView {
    public:
        DerivationGraphView(const PreDerivationGraph* g,
                            const std::vector<uint8_t>* present,
                            const std::vector<uint8_t>* alive)
            : g_(g), present_(present), alive_(alive) {}

        std::vector<NodeId> nodes() const {
            std::vector<NodeId> v;
            v.reserve(g_->nodes_.size());
            for (NodeId i = 0; i < g_->nodes_.size(); ++i)
                if ((*present_)[i]) v.push_back(i);
            return v;
        }
        std::vector<EdgeId> edges() const {
            std::vector<EdgeId> eids;
            eids.reserve(g_->edges_.size());
            for (EdgeId e = 0; e < g_->edges_.size(); ++e)
                if ((*alive_)[e]) eids.push_back(e);
            return eids;
        }
        std::vector<EdgeId> outEdges(NodeId u) const {
            std::vector<EdgeId> eids;
            if (u >= g_->outEdges_.size()) return eids;
            for (EdgeId e : g_->outEdges_[u])
                if ((*alive_)[e]) eids.push_back(e);
            return eids;
        }

        const AtomKey& keyOf(NodeId u) const { return g_->nodes_.at(u); }
        const Edge&    edgeOf(EdgeId e) const { return g_->edges_.at(e); }

    private:
        const PreDerivationGraph* g_;
        const std::vector<uint8_t>* present_;
        const std::vector<uint8_t>* alive_;
    };

public:
    // —— 节点 —— //
    NodeId addNode(const AtomKey& k, double probability = 1.0) {
        auto it = nodeIdByKey_.find(k);
        if (it != nodeIdByKey_.end()) return it->second;
        NodeId id = nodes_.size();
        nodes_.push_back(k);
        nodeIdByKey_[k] = id;
        // 扩容状态
        present_base_.push_back(0);
        present_.push_back(0);
        outEdges_.emplace_back();
        node_probabilities_[id] = probability;
        return id;
    }

    std::unordered_map<NodeId, double> node_probabilities_;
    NodeId getOrAddNode(const AtomKey& k, const double prob = 1.0) { return addNode(k, prob); }
    bool   hasNode(const AtomKey& k) const { return nodeIdByKey_.count(k) > 0; }
    NodeId nodeId(const AtomKey& k) const {
        auto it = nodeIdByKey_.find(k);
        assert(it != nodeIdByKey_.end());
        return it->second;
    }
    const AtomKey& key(NodeId id) const { return nodes_.at(id); }

    // —— 超边 —— //
    EdgeId addEdge(const std::vector<NodeId>& inputs, NodeId output, double prob = 1.0, const std::vector<bool>& negs = {}) {
        EdgeId id = edges_.size();
        if (negs.size() > 0) {
            assert(negs.size() == inputs.size());
            edges_.push_back(Edge{inputs, output, true, negs, prob});
        } else {
            edges_.push_back(Edge{inputs, output, true, std::vector<bool>(inputs.size(), false), prob});
        }
        edge_need_.push_back(static_cast<uint32_t>(inputs.size()));
        edge_have_.push_back(0);
        edge_alive_.push_back(0); // recompute 后计算
        for (NodeId u : inputs) {
            if (u >= outEdges_.size()) outEdges_.resize(u + 1);
            outEdges_[u].push_back(id);
        }
        return id;
    }
    void enableEdge(EdgeId e, bool en) { edges_.at(e).enabled = en; }
    void removeEdge(EdgeId e) { edges_.at(e).enabled = false; }

    // —— 输入种子（facts） —— //
    enum class SeedMode { Accumulate, Replace };

    void seedFacts(const std::vector<NodeId>& facts, SeedMode mode = SeedMode::Accumulate) {
        if (mode == SeedMode::Replace) {
            std::fill(present_base_.begin(), present_base_.end(), 0);
        }
        for (NodeId u : facts) {
            ensureNodeSize(u);
            present_base_[u] = 1;
        }
    }
    void seedFactsByKey(const std::vector<AtomKey>& facts, SeedMode mode = SeedMode::Accumulate) {
        std::vector<NodeId> ids;
        ids.reserve(facts.size());
        for (auto& k : facts) ids.push_back(getOrAddNode(k));
        seedFacts(ids, mode);
    }
    void clearFacts() { std::fill(present_base_.begin(), present_base_.end(), 0); }
    void retractFacts(const std::vector<NodeId>& factIdsToRemove) {
        for (NodeId u : factIdsToRemove) {
            if (u < present_base_.size()) {
                present_base_[u] = 0;
            }
        }
    }

    void retractFactsByKey(const std::vector<AtomKey>& factsToRemove) {
        std::vector<NodeId> ids;
        ids.reserve(factsToRemove.size());
        for (const auto& k : factsToRemove) {
            auto it = nodeIdByKey_.find(k);
            if (it != nodeIdByKey_.end()) {
                ids.push_back(it->second);
            }
        }
        retractFacts(ids);
    }

    // —— 传播 & 剪枝 —— //
    void recompute() {
        std::fill(present_.begin(), present_.end(), 0);
        std::fill(edge_have_.begin(), edge_have_.end(), 0);
        std::fill(edge_alive_.begin(), edge_alive_.end(), 0);

        std::deque<NodeId> Q;
        for (NodeId u = 0; u < present_base_.size(); ++u) {
            if (present_base_[u]) { present_[u] = 1; Q.push_back(u); }
        }

        while (!Q.empty()) {
            NodeId u = Q.front(); Q.pop_front();
            if (u >= outEdges_.size()) continue;
            for (EdgeId e : outEdges_[u]) {
                const Edge& E = edges_[e];
                if (!E.enabled) continue;
                auto& have = edge_have_[e];
                if (have < edge_need_[e]) ++have;
                if (have == edge_need_[e]) {
                    NodeId v = E.output;
                    if (!present_[v]) { present_[v] = 1; Q.push_back(v); }
                }
            }
        }

        // alive 边：enabled && 所有 inputs present && output present
        for (EdgeId e = 0; e < edges_.size(); ++e) {
            const auto& E = edges_[e];
            if (!E.enabled) { edge_alive_[e] = 0; continue; }
            bool body_ok = true;
            for (NodeId u : E.inputs) if (!present_[u]) { body_ok = false; break; }
            edge_alive_[e] = (body_ok && present_[E.output]) ? 1 : 0;
        }
    }

    // —— 只读视图 —— //
    DerivationGraphView view() const { return DerivationGraphView(this, &present_, &edge_alive_); }

    // —— 导出 DOT（present 节点/满足体的边 = 实线；否则虚线） —— //
    void toDot(std::ostream& os) const {
        os << "digraph G{\n";
        for (NodeId u = 0; u < nodes_.size(); ++u) {
            bool present = (u < present_.size() ? present_[u] : 0);
            os << "  n" << u << " [label=\"" << nodes_[u].toString()
               << "\",style=" << (present ? "solid" : "dashed") << "];\n";
        }
        for (EdgeId e = 0; e < edges_.size(); ++e) {
            const auto& E = edges_[e];
            if (!E.enabled) continue;
            bool alive = (e < edge_alive_.size() ? edge_alive_[e] : 0);
            for (NodeId u : E.inputs) {
                os << "  n" << u << " -> n" << E.output
                   << " [color=black,style=" << (alive ? "solid" : "dashed")
                   << ",label=\"e" << e << "\"];\n";
            }
        }
        os << "}\n";
    }

    void toDot(const std::string& filename) const {
        std::ofstream os(filename);
        os << "digraph G{\n";
        for (NodeId u = 0; u < nodes_.size(); ++u) {
            bool present = (u < present_.size() ? present_[u] : 0);
            os << "  n" << u << " [label=\"" << nodes_[u].toString()
               << "\",style=" << (present ? "solid" : "dashed") << "];\n";
        }
        for (EdgeId e = 0; e < edges_.size(); ++e) {
            const auto& E = edges_[e];
            bool alive = (e < edge_alive_.size() ? edge_alive_[e] : 0);
            for (NodeId u : E.inputs) {
                os << "  n" << u << " -> n" << E.output
                   << " [color=black,style=" << (alive ? "solid" : "dashed")
                   << ",label=\"e" << e << "\"];\n";
            }
        }
        os << "}\n";
    }

    // —— 落到已有 DerivationGraph —— //
    /**
     * 将“当前剪枝视图”投影到 DerivationGraph：
     * - present_[u]==1 的节点 -> createNode(UntypedTuple{...})
     * - edge_alive_[e]==1 的边 -> createHyperedge(inputs, output)
     * - 对于 present_base_[u]==1 的节点，额外设置 node->isFact = true
     */
    // void materialize(IncrementalDerivationGraph& out) const {
    //     // 1) 为所有可见节点创建/复用 DG 节点
    //     std::vector<NodePtr> id2node(nodes_.size(), nullptr);
    //     for (NodeId u = 0; u < nodes_.size(); ++u) {
    //         if (!present_[u]) continue;
    //         const AtomKey& k = nodes_[u];
    //         UntypedTuple tup{k.rel, k.args};        // 与示例一致：UntypedTuple{"rel",{args}} :contentReference[oaicite:4]{index=4}
    //         auto np = out.createNode(tup);                   // 使用现成 API 创建/查找节点 :contentReference[oaicite:5]{index=5}
    //         if (present_base_[u]) { np->isFact = true; }     // 标注 fact，方便下游区分（dumpJson 会用到） :contentReference[oaicite:6]{index=6}
    //         // probability TODO
    //         np->setProbability(node_probabilities_.at(u));
    //         id2node[u] = np;
    //     }
    //
    //     // 2) 为所有可见边创建超边
    //     for (EdgeId e = 0; e < edges_.size(); ++e) {
    //         // negation
    //         // probability
    //         if (!edge_alive_[e]) continue;
    //         const auto& E = edges_[e];
    //         std::vector<NodePtr> inputs;
    //         inputs.reserve(E.inputs.size());
    //         for (NodeId u : E.inputs) {
    //             auto np = id2node[u];
    //             // 若输入未 present（理论上不会发生），跳过该边
    //             if (!np) { inputs.clear(); break; }
    //             inputs.push_back(np);
    //         }
    //         if (inputs.empty()) continue;
    //         auto outNode = id2node[E.output];
    //         if (!outNode) continue;
    //
    //         // 使用不带 Rule 的便捷重载创建超边（够用） :contentReference[oaicite:7]{index=7}
    //         // TODO: should unify id type
    //         auto edge = out.createHyperedge(inputs, outNode, nullptr, E.bodyNegations, {static_cast<souffle::RamDomain>(e), {}});
    //         // TODO
    //         edge->setProbability(E.probability);
    //     }
    // }

    void materialize(IncrementalDerivationGraph& out) const {
        // --- 准备阶段：清空上一轮的增量信息，并记录旧状态 ---
        out.deltaInsertNodes.clear();
        out.deltaInsertEdges.clear();
        out.deltaDeleteNodes.clear();
        out.deltaDeleteEdges.clear();

        std::unordered_set<NodePtr> oldNodes = out.nodes; // 直接访问
        std::unordered_set<EdgePtr> oldEdges = out.edges; // 直接访问

        // --- 第一步：处理节点（增、删、改） ---
        std::vector<NodePtr> id2node(nodes_.size(), nullptr);
        for (NodeId u = 0; u < nodes_.size(); ++u) {
            if (!present_[u]) continue;
            const AtomKey& k = nodes_[u];
            UntypedTuple tup{k.rel, k.args};
            NodePtr np = out.findNode(tup);
            if (np == nullptr) {
                np = out.createNode(tup);
                out.deltaInsertNodes.insert(np);
            } else {
                oldNodes.erase(np);
            }
            np->isFact = present_base_[u];
            np->setProbability(node_probabilities_.at(u));
            id2node[u] = np;
        }
        out.deltaDeleteNodes.insert(oldNodes.begin(), oldNodes.end());

        // --- 第二步：处理边（增、删、改） ---
        for (EdgeId e = 0; e < edges_.size(); ++e) {
            if (!edge_alive_[e]) continue;
            const auto& E = edges_[e];
            std::vector<NodePtr> inputs;
            inputs.reserve(E.inputs.size());
            bool inputs_valid = true;
            for (NodeId u : E.inputs) {
                if (!(id2node[u])) { inputs_valid = false; break; }
                inputs.push_back(id2node[u]);
            }
            if (!inputs_valid || !id2node[E.output]) continue;

            auto outNode = id2node[E.output];
            assert (outNode != nullptr);
            EdgePtr ep = nullptr;
            for (const auto& candidate_edge : outNode->getIncomingEdges()) {
                if (oldEdges.count(candidate_edge) && candidate_edge->getInputs().size() == inputs.size()) {
                    std::unordered_set<NodePtr> current_inputs(candidate_edge->getInputs().begin(), candidate_edge->getInputs().end());
                    std::unordered_set<NodePtr> target_inputs(inputs.begin(), inputs.end());
                    if (current_inputs == target_inputs) { ep = candidate_edge; break; }
                }
            }

            if (ep == nullptr) {
                ep = out.createHyperedge(inputs, outNode, nullptr, E.bodyNegations, {static_cast<souffle::RamDomain>(e), {}});
                out.deltaInsertEdges.insert(ep);
            } else {
                oldEdges.erase(ep);
            }
            ep->setProbability(E.probability);
        }
        out.deltaDeleteEdges.insert(oldEdges.begin(), oldEdges.end());

        // --- 第三步：执行实际的删除操作，使图结构一致 ---
        // 1. 删除边
        for (const auto& edgeToDelete : out.deltaDeleteEdges) {
            edgeToDelete->getOutput()->getIncomingEdges().erase(
                std::remove(edgeToDelete->getOutput()->getIncomingEdges().begin(), edgeToDelete->getOutput()->getIncomingEdges().end(), edgeToDelete),
                edgeToDelete->getOutput()->getIncomingEdges().end());

            for (const auto& inputNode : edgeToDelete->getInputs()) {
                inputNode->getOutgoingEdges().erase(
                    std::remove(inputNode->getOutgoingEdges().begin(), inputNode->getOutgoingEdges().end(), edgeToDelete),
                    inputNode->getOutgoingEdges().end());
            }
            out.edges.erase(edgeToDelete); // 直接访问 out.edges
            // 注意：仍然无法清理 edgeKeyToEdgeMap，因为它需要 Rule 信息来重建 key
        }

        // 2. 删除节点
        for (const auto& nodeToDelete : out.deltaDeleteNodes) {
            out.tupleToNodeMap.erase(nodeToDelete->getTuple()); // 直接访问
            out.nodes.erase(nodeToDelete); // 直接访问
        }
    }


    // —— 统计/访问器 —— //
    std::size_t numNodes() const { return nodes_.size(); }
    std::size_t numEdges() const { return edges_.size(); }

    const std::vector<AtomKey>& allNodes() const { return nodes_; }      // 全集（即使未接边）
    const std::vector<Edge>&    allEdges() const { return edges_; }      // 全集（即使被禁用）

private:
    void ensureNodeSize(NodeId id) {
        while (present_base_.size() <= id) {
            present_base_.push_back(0);
            present_.push_back(0);
            outEdges_.emplace_back();
        }
    }

private:
    // 节点全集（包含未接边节点，便于之后快速连边）
    std::vector<AtomKey> nodes_;
    std::unordered_map<AtomKey, NodeId, AtomKeyHash> nodeIdByKey_;

    // 边全集
    std::vector<Edge> edges_;

    // 出边索引：node -> [edges...]
    std::vector<std::vector<EdgeId>> outEdges_;

    // 运行期状态
    std::vector<uint8_t> present_base_; // 输入/显式事实
    std::vector<uint8_t> present_;      // 闭包可达（base + 推导）

    // 边计数与剪枝标记
    std::vector<uint32_t> edge_need_;
    std::vector<uint32_t> edge_have_;
    std::vector<uint8_t>  edge_alive_;
};

#endif //PREDERIVATIONGRAPH_H
