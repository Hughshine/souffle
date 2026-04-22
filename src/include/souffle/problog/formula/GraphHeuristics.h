#ifndef GRAPHHEURISTICS_H
#define GRAPHHEURISTICS_H

#include <algorithm>
#include <cassert>
#include <cmath>
#include <iostream>
#include <iterator>
#include <limits>
#include <numeric>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <map>
#include <queue>
#include <vector>
#include <string>

#include "souffle/problog/DerivationGraph.h"

/**
 * BDDForceHeuristics builds a static variable ordering for Soufflé's probabilistic
 * derivation graph. Only probabilistic inputs (facts) and probabilistic rules
 * participate as variables; deterministic nodes merely propagate their dependency
 * sets.
 *
 * The pipeline has three stages:
 *   1. Collect the variable universe (probabilistic facts + rules) with stable IDs.
 *   2. Run a fixpoint over the derivation graph to compute, for every node/edge,
 *      the full set of probabilistic variables reachable from it.
 *   3. Emit attractive/repulsive events (overall supports, pairwise correlations)
 *      and apply a FORCE-style relaxation to obtain the final order.
 */
class BDDForceHeuristics {
public:
    struct Params {
        double w_and;            ///< weight for rule-level (AND) events
        double w_or;             ///< weight for node-level (OR) events
        double w_corr_anchor;    ///< weight for shared intersection anchors
        double w_corr_pull;      ///< weight for pulls (intersection + side)
        double w_corr_group;     ///< weight for full trio grouping
        double w_anti;           ///< weight for repulsive anti-events
        int    force_iters;      ///< FORCE iterations
        double repel_push;       ///< repulsive push strength
        int    block_support_min_size; ///< min support size to consider for blocks
        int    block_support_max_size; ///< max support size used for block clustering
        int    block_max_emit_size;    ///< maximum vars emitted per block
        bool   verbose;

        Params()
                : w_and(1.0),
                  w_or(0.35),
                  w_corr_anchor(0.45),
                  w_corr_pull(0.85),
                  w_corr_group(0.25),
                  w_anti(0.4),
                  force_iters(15),
                  repel_push(0.45),
                  block_support_min_size(3),
                  block_support_max_size(6),
                  block_max_emit_size(2),
                  verbose(true) {}
    };

    explicit BDDForceHeuristics(Params p = Params()) : P_(p), view_(nullptr) {}

    void setAnchorOrder(const std::vector<int>&) {}
    void clearAnchor() {}

    void compute(const DerivationGraphViewInterface& view) {
        view_ = &view;
        vars_.clear();
        idx_of_.clear();
        fact_var_to_tuple_.clear();
        rule_var_to_key_.clear();
        events_.clear();
        order_.clear();
        final_order_.clear();
        node_support_.clear();
        edge_support_.clear();
        node_scc_.clear();
        node_id_map_.clear();
        edge_id_map_.clear();
        next_var_id_ = 0;
        buildVariables();
        computeSCCs();
        computeSupports();
        buildBlocks();
        if (P_.verbose) {
            std::cout << "[Final] order size = " << final_order_.size() << '\n';
        }
    }

    const std::vector<int>& getOrder() const { return final_order_; }

private:
    using CanonicalKey = std::vector<std::variant<UntypedTuple, EdgeKey>>;

    struct Var {
        int var_id;
        enum Kind { FACT, RULE } kind;
        CanonicalKey stable_key;
    };

    struct Event {
        enum class Kind { Attractive, Repulsive };
        std::vector<int> var_ids;
        double weight;
        CanonicalKey stable_key;
        Kind kind;
    };

    struct ForceAccum {
        double attr_w = 0.0;
        double attr_sum = 0.0;
        double rep_w = 0.0;
        double rep_sum = 0.0;
    };

    Params P_;
    const DerivationGraphViewInterface* view_;

    std::vector<Var> vars_;
    std::unordered_map<int, int> idx_of_;

    std::unordered_map<int, UntypedTuple> fact_var_to_tuple_;
    std::unordered_map<int, EdgeKey>      rule_var_to_key_;

    std::vector<Event> events_;
    std::vector<int>   order_;
    std::vector<int>   final_order_;

    // support sets (sorted unique var_ids) for every node and edge
    std::unordered_map<size_t, std::vector<int>> node_support_;
    std::unordered_map<size_t, std::vector<int>> edge_support_;
    std::unordered_map<size_t, int> node_scc_;

    mutable std::unordered_map<size_t, int> node_id_map_;
    mutable std::unordered_map<size_t, int> edge_id_map_;
    mutable int next_var_id_ = 0;

    int stableNodeId(size_t rawId) const {
        auto it = node_id_map_.find(rawId);
        if (it != node_id_map_.end()) return it->second;
        int id = next_var_id_++;
        node_id_map_[rawId] = id;
        return id;
    }

    int stableEdgeId(size_t rawId) const {
        auto it = edge_id_map_.find(rawId);
        if (it != edge_id_map_.end()) return it->second;
        int id = next_var_id_++;
        edge_id_map_[rawId] = id;
        return id;
    }

    void buildBlocks() {
        final_order_.clear();
        if (vars_.empty()) return;

        std::vector<int> baseline;
        baseline.reserve(vars_.size());
        for (const auto& v : vars_) baseline.push_back(v.var_id);
        std::sort(baseline.begin(), baseline.end(), [this](int a, int b) { return stableLess(a, b); });

        std::unordered_map<int, size_t> stable_pos;
        stable_pos.reserve(baseline.size());
        for (size_t i = 0; i < baseline.size(); ++i) stable_pos[baseline[i]] = i;

        auto adjacency = buildSupportAdjacency();
        if (adjacency.empty()) {
            final_order_ = baseline;
            if (P_.verbose) {
                std::cout << "[StaticOrder] adjacency graph empty, fallback to stable order size="
                          << final_order_.size() << '\n';
            }
            return;
        }

        std::unordered_set<int> visited;
        visited.reserve(vars_.size());
        final_order_.reserve(vars_.size());

        auto degreeOf = [&](int v) -> size_t {
            auto it = adjacency.find(v);
            return (it == adjacency.end()) ? 0 : it->second.size();
        };

        auto cmpStable = [&](int a, int b) {
            auto ia = stable_pos.find(a);
            auto ib = stable_pos.find(b);
            size_t pa = (ia == stable_pos.end()) ? std::numeric_limits<size_t>::max() : ia->second;
            size_t pb = (ib == stable_pos.end()) ? std::numeric_limits<size_t>::max() : ib->second;
            return pa < pb;
        };

        auto enqueueNeighbors = [&](int v, std::queue<int>& q) {
            auto it = adjacency.find(v);
            if (it == adjacency.end()) return;
            auto neighbors = it->second;
            std::sort(neighbors.begin(), neighbors.end(), cmpStable);
            for (int u : neighbors) {
                if (visited.insert(u).second) q.push(u);
            }
        };

        while (visited.size() < vars_.size()) {
            int start = -1;
            size_t bestDegree = 0;
            for (int v : baseline) {
                if (visited.count(v)) continue;
                size_t deg = degreeOf(v);
                if (start == -1 || deg > bestDegree || (deg == bestDegree && cmpStable(v, start))) {
                    start = v;
                    bestDegree = deg;
                }
            }
            if (start == -1) break;
            std::queue<int> bfs;
            bfs.push(start);
            visited.insert(start);

            while (!bfs.empty()) {
                int v = bfs.front();
                bfs.pop();
                final_order_.push_back(v);
                enqueueNeighbors(v, bfs);
            }
        }

        for (int v : baseline) {
            if (!visited.count(v)) final_order_.push_back(v);
        }

        if (P_.verbose) {
            std::cout << "[StaticOrder] order size=" << final_order_.size() << '\n';
            std::cout << "Pure variable ordering: ";
            for (int v : final_order_) {
                std::cout << describeVar(v) << ' ';
            }
            std::cout << '\n';
        }
    }

    std::unordered_map<int, std::vector<int>> buildSupportAdjacency() const {
        std::unordered_map<int, std::set<int>> adj_sets;
        if (!view_) return {};
        for (const auto& node : view_->getNodes()) {
            if (!node || node->pruned) continue;
            std::vector<int> clique;
            const auto incoming = view_->getIncomingEdgesStable(node);
            for (const auto& edge : incoming) {
                if (!edge || edge->pruned) continue;
                auto vars = gatherImmediateProbabilisticInputs(edge);
                clique.insert(clique.end(), vars.begin(), vars.end());
            }
            std::sort(clique.begin(), clique.end());
            clique.erase(std::unique(clique.begin(), clique.end()), clique.end());
            if (clique.size() < 2) continue;
            for (size_t i = 0; i < clique.size(); ++i) {
                for (size_t j = i + 1; j < clique.size(); ++j) {
                    adj_sets[clique[i]].insert(clique[j]);
                    adj_sets[clique[j]].insert(clique[i]);
                }
            }
        }
        std::unordered_map<int, std::vector<int>> adj;
        adj.reserve(adj_sets.size());
        for (auto& [var, neigh] : adj_sets) {
            adj[var] = std::vector<int>(neigh.begin(), neigh.end());
        }
        return adj;
    }

    std::vector<int> gatherImmediateProbabilisticInputs(const EdgePtr& e) const {
        std::vector<int> vars;
        if (!e || !view_) return vars;
        const auto inputs = view_->getInputsStable(e);
        const auto negs = view_->getBodyNegationsStable(e);
        for (size_t i = 0; i < inputs.size(); ++i) {
            const NodePtr b = inputs[i];
            const bool neg = (i < negs.size()) ? negs[i] : false;
            if (!b || neg) continue;
            if (b->isFact && !b->pruned && b->getProbability() < 1.0) {
                vars.push_back(stableNodeId(b->getId()));
            }
            const auto parents = view_->getIncomingEdgesStable(b);
            for (const auto& parent : parents) {
                if (!parent || parent->pruned) continue;
                if (parent->getProbability() < 1.0) {
                    vars.push_back(stableEdgeId(parent->getId()));
                }
            }
        }
        std::sort(vars.begin(), vars.end());
        vars.erase(std::unique(vars.begin(), vars.end()), vars.end());
        return vars;
    }

private:
    static bool isNearlyEqual(double a, double b, double eps = 1e-12) {
        return std::fabs(a - b) < eps;
    }

    void dprintln(const std::string& s) const {
        if (P_.verbose) std::cout << s << '\n';
    }

    std::string describeVar(int var_id) const {
        auto fit = fact_var_to_tuple_.find(var_id);
        if (fit != fact_var_to_tuple_.end()) {
            return "FACT(" + fit->second.toString() + ")";
        }
        auto rit = rule_var_to_key_.find(var_id);
        if (rit != rule_var_to_key_.end()) {
            return "RULE(" + std::get<1>(rit->second).toString() + ")";
        }
        return "UNKNOWN(" + std::to_string(var_id) + ")";
    }

    void buildVariables() {
        vars_.clear();
        idx_of_.clear();
        fact_var_to_tuple_.clear();
        rule_var_to_key_.clear();

        std::vector<NodePtr> nodes;
        for (const auto& n : view_->getNodes()) {
            if (n) nodes.push_back(n);
        }
        std::sort(nodes.begin(), nodes.end(), [](const NodePtr& a, const NodePtr& b) {
            return a->getTuple() < b->getTuple();
        });

        for (const auto& n : nodes) {
            if (!n->isFact) continue;
            if (n->pruned) continue;
            if (!(n->getProbability() < 1.0)) continue;
            const int vid = stableNodeId(n->getId());
            if (!idx_of_.count(vid)) {
                idx_of_[vid] = static_cast<int>(vars_.size());
                vars_.push_back(Var{vid, Var::FACT, {n->getTuple()}});
                fact_var_to_tuple_[vid] = n->getTuple();
            }
        }

        std::vector<EdgePtr> edges;
        for (const auto& e : view_->getEdges()) {
            if (e) edges.push_back(e);
        }
        std::sort(edges.begin(), edges.end(), [](const EdgePtr& a, const EdgePtr& b) {
            return a->getEdgeKey() < b->getEdgeKey();
        });

        for (const auto& e : edges) {
            if (e->pruned) continue;
            if (!(e->getProbability() < 1.0)) continue;
            const int vid = stableEdgeId(e->getId());
            if (!idx_of_.count(vid)) {
                idx_of_[vid] = static_cast<int>(vars_.size());
                vars_.push_back(Var{vid, Var::RULE, {e->getEdgeKey()}});
                rule_var_to_key_[vid] = e->getEdgeKey();
            }
        }

        std::sort(vars_.begin(), vars_.end(), [](const Var& a, const Var& b) {
            return a.stable_key < b.stable_key;
        });

        dprintln("[Universe] |vars|=" + std::to_string(vars_.size()));
    }

    static std::vector<int> unite(const std::vector<int>& a, const std::vector<int>& b) {
        if (a.empty()) return b;
        if (b.empty()) return a;
        std::vector<int> out;
        out.reserve(a.size() + b.size());
        std::merge(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(out));
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return out;
    }

    static bool mergeInto(std::vector<int>& base, const std::vector<int>& addition) {
        if (addition.empty()) return false;
        std::vector<int> merged;
        merged.reserve(base.size() + addition.size());
        std::merge(base.begin(), base.end(), addition.begin(), addition.end(), std::back_inserter(merged));
        merged.erase(std::unique(merged.begin(), merged.end()), merged.end());
        if (merged == base) return false;
        base.swap(merged);
        return true;
    }

    void computeSCCs() {
        node_scc_.clear();
        std::unordered_map<size_t, int> index, lowlink;
        std::vector<size_t> stack;
        std::unordered_map<size_t, bool> onstack;
        int idx = 0;
        int comp = 0;

        std::function<void(const NodePtr&)> strongconnect = [&](const NodePtr& n) {
            if (!n) return;
            const size_t id = n->getId();
            index[id] = idx;
            lowlink[id] = idx;
            ++idx;
            stack.push_back(id);
            onstack[id] = true;

            const auto outgoing = view_->getOutgoingEdges(n);
            for (const auto& e : outgoing) {
                if (!e || e->pruned) continue;
                const NodePtr out = view_->getOutput(e);
                if (!out) continue;
                const size_t oid = out->getId();
                if (!index.count(oid)) {
                    strongconnect(out);
                    lowlink[id] = std::min(lowlink[id], lowlink[oid]);
                } else if (onstack[oid]) {
                    lowlink[id] = std::min(lowlink[id], lowlink[oid]);
                }
            }

            if (lowlink[id] == index[id]) {
                while (true) {
                    size_t w = stack.back();
                    stack.pop_back();
                    onstack[w] = false;
                    node_scc_[w] = comp;
                    if (w == id) break;
                }
                ++comp;
            }
        };

        for (const auto& n : view_->getNodes()) {
            if (!n) continue;
            if (!index.count(n->getId())) strongconnect(n);
        }
    }

    void computeSupports() {
        node_support_.clear();
        edge_support_.clear();

        for (const auto& n : view_->getNodes()) {
            if (!n) continue;
            std::vector<int> supp;
            if (n->isFact && !n->pruned && n->getProbability() < 1.0) {
                const int vid = stableNodeId(n->getId());
                if (idx_of_.count(vid)) supp.push_back(vid);
            }
            node_support_[n->getId()] = std::move(supp);
        }

        for (const auto& e : view_->getEdges()) {
            if (!e) continue;
            std::vector<int> supp;
            if (!e->pruned && e->getProbability() < 1.0) {
                const int vid = stableEdgeId(e->getId());
                if (idx_of_.count(vid)) supp.push_back(vid);
            }
            edge_support_[e->getId()] = std::move(supp);
        }

        bool changed = true;
        size_t iter = 0;
        while (changed) {
            changed = false;
            ++iter;
            for (const auto& e : view_->getEdges()) {
                if (!e || e->pruned) continue;
                auto& supp = edge_support_[e->getId()];
                const auto inputs = view_->getInputsStable(e);
                const auto negs   = view_->getBodyNegationsStable(e);
                for (size_t i = 0; i < inputs.size(); ++i) {
                    const NodePtr b = inputs[i];
                    const bool neg  = (i < negs.size()) ? negs[i] : false;
                    if (!b || neg) continue;
                    const NodePtr out = view_->getOutput(e);
                    if (out) {
                        auto ita = node_scc_.find(out->getId());
                        auto itb = node_scc_.find(b->getId());
                        if (ita != node_scc_.end() && itb != node_scc_.end() && ita->second == itb->second) {
                            continue;
                        }
                    }
                    changed |= mergeInto(supp, node_support_[b->getId()]);
                }
            }

            for (const auto& n : view_->getNodes()) {
                if (!n) continue;
                auto& supp = node_support_[n->getId()];
                const auto incoming = view_->getIncomingEdgesStable(n);
                for (const auto& e : incoming) {
                    if (!e || e->pruned) continue;
                    const NodePtr out = view_->getOutput(e);
                    if (out && out->getId() == n->getId()) {
                        // rule whose output is this node: only merge if edge is not recursive (at least one input outside SCC)
                        bool hasExternalInput = false;
                        const auto inputs = view_->getInputsStable(e);
                        const auto negs   = view_->getBodyNegationsStable(e);
                        for (size_t i = 0; i < inputs.size(); ++i) {
                            const NodePtr b = inputs[i];
                            const bool neg  = (i < negs.size()) ? negs[i] : false;
                            if (!b || neg) continue;
                            if (b->getId() != n->getId()) {
                                hasExternalInput = true;
                                break;
                            }
                        }
                        if (!hasExternalInput) continue;
                    }
                    changed |= mergeInto(supp, edge_support_[e->getId()]);
                }
            }

            if (iter > 2000) {
                dprintln("[WARN] support fixpoint iterations exceeded 2000, forcing stop");
                break;
            }
        }

        if (P_.verbose) {
            std::cout << "[Support] iterations=" << iter << '\n';
            std::cout << "[Support] Node dependency sets:\n";
            for (const auto& n : view_->getNodes()) {
                if (!n) continue;
                const auto& supp = node_support_.at(n->getId());
                std::cout << "  NODE " << n->getTuple().toString() << " ->";
                for (int v : supp) std::cout << ' ' << describeVar(v);
                std::cout << '\n';
            }
            std::cout << "[Support] Edge dependency sets:\n";
            for (const auto& e : view_->getEdges()) {
                if (!e) continue;
                const auto& supp = edge_support_.at(e->getId());
                std::cout << "  EDGE#" << e->getId() << " ->";
                for (int v : supp) std::cout << ' ' << describeVar(v);
                std::cout << '\n';
            }
        }
    }

    const std::vector<int>& supportOfNode(const NodePtr& n) const {
        static const std::vector<int> empty;
        if (!n) return empty;
        auto it = node_support_.find(n->getId());
        return (it != node_support_.end()) ? it->second : empty;
    }

    const std::vector<int>& supportOfEdge(const EdgePtr& e) const {
        static const std::vector<int> empty;
        if (!e) return empty;
        auto it = edge_support_.find(e->getId());
        return (it != edge_support_.end()) ? it->second : empty;
    }

    std::variant<UntypedTuple, EdgeKey> stableKeyFor(int var_id) const {
        if (auto it = fact_var_to_tuple_.find(var_id); it != fact_var_to_tuple_.end()) return it->second;
        if (auto it = rule_var_to_key_.find(var_id); it != rule_var_to_key_.end()) return it->second;
        assert(false && "Unknown var_id");
        return EdgeKey{};
    }

    bool stableLess(int a, int b) const {
        return stableKeyFor(a) < stableKeyFor(b);
    }

    void pushEvent(const std::vector<int>& raw, double weight, Event::Kind kind) {
        if (weight <= 0.0) return;
        std::vector<int> filtered;
        filtered.reserve(raw.size());
        for (int v : raw) {
            if (idx_of_.count(v)) filtered.push_back(v);
        }
        if (filtered.size() < 2) return;
        std::sort(filtered.begin(), filtered.end(), [this](int lhs, int rhs) {
            return stableLess(lhs, rhs);
        });
        filtered.erase(std::unique(filtered.begin(), filtered.end()), filtered.end());
        if (filtered.size() < 2) return;

        CanonicalKey key;
        key.reserve(filtered.size());
        for (int v : filtered) key.push_back(stableKeyFor(v));
        events_.push_back(Event{std::move(filtered), weight, std::move(key), kind});
    }

    static std::vector<int> intersection(const std::vector<int>& a, const std::vector<int>& b) {
        std::vector<int> out;
        std::set_intersection(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(out));
        return out;
    }

    static std::vector<int> difference(const std::vector<int>& a, const std::vector<int>& b) {
        std::vector<int> out;
        std::set_difference(a.begin(), a.end(), b.begin(), b.end(), std::back_inserter(out));
        return out;
    }

    void emitCorrelationEvents(const std::vector<const std::vector<int>*>& blocks) {
        if (blocks.size() < 2) return;
        for (size_t i = 0; i < blocks.size(); ++i) {
            const auto* A = blocks[i];
            if (!A || A->size() < 2) continue;
            for (size_t j = i + 1; j < blocks.size(); ++j) {
                const auto* B = blocks[j];
                if (!B || B->size() < 2) continue;

                auto inter = intersection(*A, *B);
                auto a_only = difference(*A, inter);
                auto b_only = difference(*B, inter);

                if (!inter.empty()) {
                    pushEvent(inter, P_.w_corr_anchor, Event::Kind::Attractive);
                    pushEvent(unite(a_only, inter), P_.w_corr_pull, Event::Kind::Attractive);
                    pushEvent(unite(inter, b_only), P_.w_corr_pull, Event::Kind::Attractive);
                    pushEvent(unite(unite(a_only, inter), b_only), P_.w_corr_group, Event::Kind::Attractive);
                }

                auto repel = unite(a_only, b_only);
                if (repel.size() >= 2) {
                    pushEvent(repel, P_.w_anti, Event::Kind::Repulsive);
                }
            }
        }
    }

    void buildEvents() {
        events_.clear();
        if (vars_.empty()) return;

        // Node-level OR events
        std::map<int, std::vector<int>> scc_to_vars;
        for (const auto& n : view_->getNodes()) {
            if (!n) continue;
            const auto& supp = node_support_[n->getId()];
            pushEvent(supp, P_.w_or, Event::Kind::Attractive);

            auto itscc = node_scc_.find(n->getId());
            if (itscc != node_scc_.end()) {
                scc_to_vars[itscc->second].insert(scc_to_vars[itscc->second].end(), supp.begin(), supp.end());
            }
        }

        for (auto& kv : scc_to_vars) {
            auto& vec = kv.second;
            std::sort(vec.begin(), vec.end());
            vec.erase(std::unique(vec.begin(), vec.end()), vec.end());
            if (vec.size() >= 2) pushEvent(vec, P_.w_or * 0.5, Event::Kind::Repulsive);
        }

        // Edge-level AND + correlations
        for (const auto& e : view_->getEdges()) {
            if (!e || e->pruned) continue;
            const auto& supp = edge_support_[e->getId()];
            pushEvent(supp, P_.w_and, Event::Kind::Attractive);

            std::vector<const std::vector<int>*> blocks;
            const auto inputs = view_->getInputsStable(e);
            const auto negs   = view_->getBodyNegationsStable(e);
            for (size_t i = 0; i < inputs.size(); ++i) {
                const NodePtr b = inputs[i];
                const bool neg  = (i < negs.size()) ? negs[i] : false;
                if (!b || neg) continue;
                const auto& block = node_support_[b->getId()];
                if (!block.empty()) blocks.push_back(&block);
            }
            emitCorrelationEvents(blocks);
        }

        std::sort(events_.begin(), events_.end(), [](const Event& a, const Event& b) {
            return a.stable_key < b.stable_key;
        });

        if (P_.verbose) {
            std::cout << "[Events] total=" << events_.size() << '\n';
            for (const auto& ev : events_) {
                std::cout << "  " << (ev.kind == Event::Kind::Attractive ? "ATTR" : "REPEL")
                          << " w=" << ev.weight << " vars=";
                for (int v : ev.var_ids) std::cout << ' ' << describeVar(v);
                std::cout << '\n';
            }
        }
    }

    double totalSpan(const std::vector<int>& order) const {
        if (events_.empty() || order.empty()) return 0.0;
        std::unordered_map<int, int> pos;
        for (int i = 0; i < static_cast<int>(order.size()); ++i) pos[order[i]] = i;

        double span = 0.0;
        for (const auto& e : events_) {
            int mn = std::numeric_limits<int>::max();
            int mx = std::numeric_limits<int>::min();
            for (int v : e.var_ids) {
                auto it = pos.find(v);
                if (it == pos.end()) continue;
                mn = std::min(mn, it->second);
                mx = std::max(mx, it->second);
            }
            if (mx < mn) continue;
            double contrib = (mx - mn) * e.weight;
            if (e.kind == Event::Kind::Repulsive) contrib = -contrib;
            span += contrib;
        }
        return span;
    }

    void forceFree() {
        order_.clear();
        order_.reserve(vars_.size());
        for (const auto& v : vars_) order_.push_back(v.var_id);
        if (order_.size() <= 1 || events_.empty()) return;

        if (P_.verbose) {
            std::cout << "[FORCE] initial span=" << totalSpan(order_) << '\n';
        }

        for (int it = 0; it < P_.force_iters; ++it) {
            std::unordered_map<int, int> pos;
            for (int i = 0; i < static_cast<int>(order_.size()); ++i) pos[order_[i]] = i;

            std::unordered_map<int, ForceAccum> accum;
            accum.reserve(order_.size());
            std::vector<double> centers(events_.size(), 0.0);
            for (size_t ei = 0; ei < events_.size(); ++ei) {
                double s = 0.0;
                int count = 0;
                for (int v : events_[ei].var_ids) {
                    auto itp = pos.find(v);
                    if (itp == pos.end()) continue;
                    s += itp->second;
                    ++count;
                }
                centers[ei] = (count > 0) ? (s / count) : 0.0;
            }

            for (size_t ei = 0; ei < events_.size(); ++ei) {
                const auto& ev = events_[ei];
                for (int v : ev.var_ids) {
                    auto& ac = accum[v];
                    if (ev.kind == Event::Kind::Attractive) {
                        ac.attr_w += ev.weight;
                        ac.attr_sum += ev.weight * centers[ei];
                    } else {
                        ac.rep_w += ev.weight;
                        ac.rep_sum += ev.weight * centers[ei];
                    }
                }
            }

            auto target = [&](int v) {
                auto itAcc = accum.find(v);
                if (itAcc == accum.end() || itAcc->second.attr_w <= 0.0) return static_cast<double>(pos[v]);
                double attr_center = itAcc->second.attr_sum / itAcc->second.attr_w;
                if (itAcc->second.rep_w > 0.0) {
                    double rep_center = itAcc->second.rep_sum / itAcc->second.rep_w;
                    attr_center += P_.repel_push * (attr_center - rep_center);
                }
                return attr_center;
            };

            std::stable_sort(order_.begin(), order_.end(), [&](int a, int b) {
                double ta = target(a);
                double tb = target(b);
                if (isNearlyEqual(ta, tb)) return stableLess(a, b);
                return ta < tb;
            });

            if (P_.verbose) {
                std::cout << "[FORCE] iter=" << (it + 1) << " span=" << totalSpan(order_) << '\n';
            }
        }
    }
};

#endif
