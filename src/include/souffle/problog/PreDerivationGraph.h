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

// Depends on DerivationGraph for materialize (header-only use is fine).
#include "souffle/Derivation.h"
#include "DerivationGraph.h"  // NodePtr/EdgePtr/DerivationGraph/UntypedTuple

using NodeId = std::size_t;
using EdgeId = std::size_t;

/** Node key: relation name + arguments (use RamDomain, consistent with UntypedTuple). */
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
            // Mixed hash
            h ^= (Hd(v) + 0x9e3779b97f4a7c15ULL + (h<<6) + (h>>2));
        }
        return h;
    }
};

/**
 * Preset graph (header-only)
 * - Node = ground atom
 * - Hyperedge = ground rule (multiple inputs, one output)
 * - Supports: repeated seed inputs, O(V+E) propagation, pruned view (no real edge/node deletion),
 *   dynamic edge add/remove, DOT export
 * - materialize(DerivationGraph&): project the current view into an existing DerivationGraph
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

    /** -- Pruned view: read-only wrapper -- */
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
    // -- Nodes --
    NodeId addNode(const AtomKey& k, double probability = 1.0) {
        auto it = nodeIdByKey_.find(k);
        if (it != nodeIdByKey_.end()) return it->second;
        NodeId id = nodes_.size();
        nodes_.push_back(k);
        nodeIdByKey_[k] = id;
        // Expand state storage
        present_base_.push_back(0);
        present_.push_back(0);
        outEdges_.emplace_back();
        node_probabilities_[id] = probability;
        return id;
    }

    std::unordered_map<NodeId, double> node_probabilities_;
    NodeId getOrAddNode(const AtomKey& k, const double prob = 1.0) { return addNode(k, prob); }
    bool   hasNode(const AtomKey& k) const { return nodeIdByKey_.count(k) > 0; }
    bool hasFact(const AtomKey& k) const {
        if (!hasNode(k)) return false;
        NodeId id = nodeId(k);
        return present_base_.at(id) > 0;
    }
    NodeId nodeId(const AtomKey& k) const {
        auto it = nodeIdByKey_.find(k);
        assert(it != nodeIdByKey_.end());
        return it->second;
    }
    const AtomKey& key(NodeId id) const { return nodes_.at(id); }

    // -- Hyperedges --
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
        edge_alive_.push_back(0); // Computed after recompute
        for (NodeId u : inputs) {
            if (u >= outEdges_.size()) outEdges_.resize(u + 1);
            outEdges_[u].push_back(id);
        }
        return id;
    }
    void enableEdge(EdgeId e, bool en) { edges_.at(e).enabled = en; }
    void removeEdge(EdgeId e) { edges_.at(e).enabled = false; }

    // -- Input seeds (facts) --
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

    // -- Propagation & pruning --
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

        // Alive edge: enabled && all inputs present && output present
        for (EdgeId e = 0; e < edges_.size(); ++e) {
            const auto& E = edges_[e];
            if (!E.enabled) { edge_alive_[e] = 0; continue; }
            bool body_ok = true;
            for (NodeId u : E.inputs) if (!present_[u]) { body_ok = false; break; }
            edge_alive_[e] = (body_ok && present_[E.output]) ? 1 : 0;
        }
    }

    // -- Read-only view --
    DerivationGraphView view() const { return DerivationGraphView(this, &present_, &edge_alive_); }

    // -- Export DOT (present nodes / edges with body satisfied = solid; otherwise dashed) --
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

    // -- Materialize into an existing DerivationGraph --
    /**
     * Project the "current pruned view" into DerivationGraph:
     * - Nodes with present_[u]==1 -> createNode(UntypedTuple{...})
     * - Edges with edge_alive_[e]==1 -> createHyperedge(inputs, output)
     * - For nodes with present_base_[u]==1, additionally set node->isFact = true
     */
    // void materialize(IncrementalDerivationGraph& out) const {
    //     // 1) Create/reuse DG nodes for all visible nodes
    //     std::vector<NodePtr> id2node(nodes_.size(), nullptr);
    //     for (NodeId u = 0; u < nodes_.size(); ++u) {
    //         if (!present_[u]) continue;
    //         const AtomKey& k = nodes_[u];
    //         UntypedTuple tup{k.rel, k.args};        // Match example: UntypedTuple{"rel",{args}}
    //         auto np = out.createNode(tup);                   // Use existing API to create/find node
    //         if (present_base_[u]) { np->isFact = true; }     // Mark fact for downstream (dumpJson uses it)
    //         // probability TODO
    //         np->setProbability(node_probabilities_.at(u));
    //         id2node[u] = np;
    //     }
    //
    //     // 2) Create hyperedges for all visible edges
    //     for (EdgeId e = 0; e < edges_.size(); ++e) {
    //         // negation
    //         // probability
    //         if (!edge_alive_[e]) continue;
    //         const auto& E = edges_[e];
    //         std::vector<NodePtr> inputs;
    //         inputs.reserve(E.inputs.size());
    //         for (NodeId u : E.inputs) {
    //             auto np = id2node[u];
    //             // If an input is not present (should not happen), skip the edge
    //             if (!np) { inputs.clear(); break; }
    //             inputs.push_back(np);
    //         }
    //         if (inputs.empty()) continue;
    //         auto outNode = id2node[E.output];
    //         if (!outNode) continue;
    //
    //         // Use the convenience overload without Rule (sufficient here)
    //         // TODO: should unify id type
    //         auto edge = out.createHyperedge(inputs, outNode, nullptr, E.bodyNegations, {static_cast<souffle::RamDomain>(e), {}});
    //         // TODO
    //         edge->setProbability(E.probability);
    //     }
    // }

    void materialize(IncrementalDerivationGraph& out) const {
        // --- Prep: clear previous incremental info and capture old state ---
        out.deltaInsertNodes.clear();
        out.deltaInsertEdges.clear();
        out.deltaDeleteNodes.clear();
        out.deltaDeleteEdges.clear();

        std::unordered_set<NodePtr> oldNodes = out.nodes; // Direct access
        std::unordered_set<EdgePtr> oldEdges = out.edges; // Direct access

        // --- Step 1: handle nodes (add/delete/update) ---
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

        // --- Step 2: handle edges (add/delete/update) ---
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
                if (inputs.empty()) {
                    assert (false);  // fact rules should be omitted already
                }
                ep = out.createHyperedge(inputs, outNode, nullptr, E.bodyNegations, {static_cast<souffle::RamDomain>(e), {}});
                out.deltaInsertEdges.insert(ep);
            } else {
                oldEdges.erase(ep);
            }
            ep->setProbability(E.probability);
        }
        out.deltaDeleteEdges.insert(oldEdges.begin(), oldEdges.end());

        // --- Step 3: perform actual deletions to keep graph consistent ---
        // 1. Delete edges
        for (const auto& edgeToDelete : out.deltaDeleteEdges) {
            edgeToDelete->getOutput()->getIncomingEdges().erase(
                std::remove(edgeToDelete->getOutput()->getIncomingEdges().begin(), edgeToDelete->getOutput()->getIncomingEdges().end(), edgeToDelete),
                edgeToDelete->getOutput()->getIncomingEdges().end());

            for (const auto& inputNode : edgeToDelete->getInputs()) {
                inputNode->getOutgoingEdges().erase(
                    std::remove(inputNode->getOutgoingEdges().begin(), inputNode->getOutgoingEdges().end(), edgeToDelete),
                    inputNode->getOutgoingEdges().end());
            }
            out.edges.erase(edgeToDelete); // Direct access to out.edges
            // Note: still cannot clean edgeKeyToEdgeMap because it needs Rule info to rebuild the key
        }

        // 2. Delete nodes
        for (const auto& nodeToDelete : out.deltaDeleteNodes) {
            out.tupleToNodeMap.erase(nodeToDelete->getTuple()); // Direct access
            out.nodes.erase(nodeToDelete); // Direct access
        }
    }


    // -- Stats/accessors --
    std::size_t numNodes() const { return nodes_.size(); }
    std::size_t numEdges() const { return edges_.size(); }

    const std::vector<AtomKey>& allNodes() const { return nodes_; }      // Full set (even if unconnected)
    const std::vector<Edge>&    allEdges() const { return edges_; }      // Full set (even if disabled)

private:
    void ensureNodeSize(NodeId id) {
        while (present_base_.size() <= id) {
            present_base_.push_back(0);
            present_.push_back(0);
            outEdges_.emplace_back();
        }
    }

private:
    // All nodes (including unconnected nodes, for fast edge linking later)
    std::vector<AtomKey> nodes_;
    std::unordered_map<AtomKey, NodeId, AtomKeyHash> nodeIdByKey_;

    // All edges
    std::vector<Edge> edges_;

    // Outgoing edge index: node -> [edges...]
    std::vector<std::vector<EdgeId>> outEdges_;

    // Runtime state
    std::vector<uint8_t> present_base_; // Input/explicit facts
    std::vector<uint8_t> present_;      // Closure reachable (base + derived)

    // Edge counts and pruning flags
    std::vector<uint32_t> edge_need_;
    std::vector<uint32_t> edge_have_;
    std::vector<uint8_t>  edge_alive_;
};

#endif //PREDERIVATIONGRAPH_H
