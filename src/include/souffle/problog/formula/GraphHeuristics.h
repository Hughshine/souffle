#ifndef GRAPHHEURISTICS_H
#define GRAPHHEURISTICS_H

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <algorithm>
#include <numeric>
#include <iostream>
#include <cassert>
#include <cmath>
#include <climits>
#include "souffle/problog/DerivationGraph.h"

class BDDForceHeuristics {
public:
    struct Params {
        // Frontier folding for derived facts appearing in rule bodies (for AND/BRIDGE construction)
        int  frontier_depth;                // 0 = no folding (default); >0 = expand through derived facts up to this depth
        int  and_size_budget;               // cap the total variables collected while folding (guards explosion)

        // Event weights
        double w_and;                       // weight for AND-events
        double w_or;                        // weight for OR-events
        double w_bridge;                    // weight for BRIDGE-events

        // Optional bridge variant: shared input fact across multiple rules (default off)
        bool enable_bridge_shared_input;

        // FORCE parameters
        int  force_iters;                   // max iterations

        // Debugging
        bool verbose;                       // print debug info

        Params()
        : frontier_depth(4),  // how to set this parameter for different domain is an art. TODO
          and_size_budget(12),
          w_and(1.0),
          w_or(0.30),
          w_bridge(0.15),
          enable_bridge_shared_input(true),
          force_iters(20),
          verbose(true) {}
    };

    explicit BDDForceHeuristics(Params p = Params())
    : P_(p), view_(nullptr) {}

    // Supply a prior (remembered) variable order as "anchor".
    // On the next compute(), variables present in anchor are kept in the same relative order.
    // New variables (not present in anchor) are re-ordered and inserted between/around anchors.
    void setAnchorOrder(const std::vector<int>& prev_order) {
        pending_anchor_ = prev_order;       // filtered later after we know current universe
    }

    void clearAnchor() {
        pending_anchor_.clear();
        anchor_order_.clear();
        anchor_rank_.clear();
    }

    // Run the full pipeline up to FORCE (no clustering/refinement in this version)
    void compute(const DerivationGraphViewInterface& view) {
        view_ = &view;

        buildVariables();       // universe: only probabilistic input facts & probabilistic rules (p<1, not pruned)
        installAnchor();        // filter anchor to current universe, build anchor ranks
        buildEvents();          // AND/OR/BRIDGE events (respecting frontier_depth / budgets)
        if (!anchor_order_.empty()) forceAnchored();
        else                        forceFree();

        // Final order is the FORCE output (no clustering stages in this version)
        final_order_ = order_;

        if (P_.verbose) {
            std::cout << "[Final] order size = " << final_order_.size() << "\n";
        }
    }

    const std::vector<int>& getOrder() const { return final_order_; }

private:
    // ---------- Types ----------
    struct Var {
        int  var_id;     // formula variable id (from mapNodeId/mapEdgeId)
        enum Kind { FACT, RULE } kind;
    };
    struct Event {
        std::vector<int> var_ids;  // participating variables (IDs)
        double weight;             // event weight
    };

    // ---------- Data ----------
    Params P_;
    const DerivationGraphViewInterface* view_;

    std::vector<Var> vars_;                        // all variables in this round (universe)
    std::unordered_map<int,int> idx_of_;           // var_id -> index (0..vars_.size()-1)

    std::vector<Event> events_;                    // hyperedges/events

    // Anchor support
    std::vector<int> pending_anchor_;              // external prior order (to be filtered)
    std::vector<int> anchor_order_;                // filtered anchors (present in current universe)
    std::unordered_map<int,int> anchor_rank_;      // var_id -> rank in anchor_order_

    // Orders
    std::vector<int> order_;                       // FORCE result
    std::vector<int> final_order_;                 // equals order_ in this version

    // ---------- Small utilities ----------

    static inline bool isNearlyEqual(double a, double b, double eps = 1e-12) {
        return std::fabs(a - b) < eps;
    }

    void dprintln(const std::string& s) const {
        if (P_.verbose) std::cout << s << "\n";
    }

    // ---------- Universe construction ----------
    void buildVariables() {
        vars_.clear();
        idx_of_.clear();

        // Input facts (isFact == true), probability < 1.0, not pruned
        for (const NodePtr& n : view_->getNodes()) {
            if (!n) continue;
            if (!n->isFact) continue;                  // exclude derived facts
            if (n->pruned) continue;                   // excluded by caller
            const double p = n->getProbability();
            if (!(p < 1.0)) {                          // drop p==1
                if (P_.verbose) {
                    std::cout << "Drop FACT node#" << n->getId() << " p=" << p << "\n";
                }
                continue;
            }
            const int vid = mapNodeId(n->getId());
            if (!idx_of_.count(vid)) {
                idx_of_[vid] = (int)vars_.size();
                vars_.push_back(Var{vid, Var::FACT});
                if (P_.verbose) {
                    std::cout << "Keep var FACT var_id=" << vid << " (node#" << n->getId() << ")\n";
                }
            }
        }

        // Rules (edges), probability < 1.0, not pruned
        for (const EdgePtr& e : view_->getEdges()) {
            if (!e) continue;
            if (e->pruned) continue;
            const double p = e->getProbability();
            if (!(p < 1.0)) {                          // drop p==1
                if (P_.verbose) {
                    std::cout << "Drop RULE edge#" << e->getId() << " p=" << p << "\n";
                }
                continue;
            }
            const int vid = mapEdgeId(e->getId());
            if (!idx_of_.count(vid)) {
                idx_of_[vid] = (int)vars_.size();
                vars_.push_back(Var{vid, Var::RULE});
                if (P_.verbose) {
                    std::cout << "Keep var RULE var_id=" << vid << " (edge#" << e->getId() << ")\n";
                }
            }
        }

        // sanity: var_id unique
        {
            std::unordered_set<int> seen;
            seen.reserve(vars_.size());
            for (const auto& v : vars_) {
                assert(!seen.count(v.var_id) && "duplicate var_id in vars_");
                seen.insert(v.var_id);
            }
        }
        if (P_.verbose) {
            std::cout << "[Universe] |vars|=" << vars_.size() << "\n";
        }
    }

    void installAnchor() {
        anchor_order_.clear();
        anchor_rank_.clear();
        if (pending_anchor_.empty()) return;

        std::unordered_set<int> uni;
        uni.reserve(idx_of_.size());
        for (const auto& kv : idx_of_) uni.insert(kv.first);

        std::unordered_set<int> used;
        used.reserve(pending_anchor_.size());
        for (int v : pending_anchor_) {
            if (uni.count(v) && !used.count(v)) {
                anchor_rank_[v] = (int)anchor_order_.size();
                anchor_order_.push_back(v);
                used.insert(v);
            }
        }
        if (P_.verbose) {
            std::cout << "[Anchor] kept " << anchor_order_.size()
                      << " of " << pending_anchor_.size() << " variables\n";
        }
    }

    // ---------- Frontier folding ----------
    // Collect probabilistic frontier starting from a (possibly derived) fact,
    // limited by depth and a size budget. Avoid cycles via seen_nodes.
    void frontierCollect(const NodePtr& fact,
                         int depth_left,
                         int budget_left,
                         std::unordered_set<int>& out_vars,
                         std::unordered_set<size_t>& seen_nodes) {
        if (!fact || depth_left < 0 || budget_left <= 0) return;

        if (fact->isFact) {
            if (!fact->pruned && fact->getProbability() < 1.0) {
                const int vid = mapNodeId(fact->getId());
                if (idx_of_.count(vid)) out_vars.insert(vid);
            }
            return;
        }

        // derived fact
        if (!seen_nodes.insert(fact->getId()).second) return; // already visited

        // For each incoming edge: include its (probabilistic) rule variable; and, if depth_left>0, expand into its inputs.
        const auto inE = view_->getIncomingEdges(fact);
        for (const EdgePtr& e : inE) {
            if (!e || e->pruned) continue;
            if (e->getProbability() < 1.0) {
                const int rvid = mapEdgeId(e->getId());
                if (idx_of_.count(rvid)) out_vars.insert(rvid);
            }
            if (depth_left <= 0) continue;

            const auto inputs = view_->getInputs(e);
            const auto negs   = view_->getBodyNegations(e);
            for (size_t i=0; i<inputs.size(); ++i) {
                if (budget_left <= 0) return;
                const NodePtr b = inputs[i];
                const bool neg  = (i < negs.size()) ? negs[i] : false;
                if (!b || neg) continue;
                frontierCollect(b, depth_left - 1, budget_left - 1, out_vars, seen_nodes);
                if ((int)out_vars.size() >= P_.and_size_budget) return;
            }
        }
    }

    // ---------- Event construction ----------
    void buildEvents() {
        events_.clear();
        events_.reserve(view_->getEdges().size() * 2 + view_->getNodes().size());

        auto push_event = [&](std::vector<int>& s, double w) {
            // filter to current universe, unique, need >=2
            std::vector<int> t;
            t.reserve(s.size());
            for (int v : s) if (idx_of_.count(v)) t.push_back(v);
            std::sort(t.begin(), t.end());
            t.erase(std::unique(t.begin(), t.end()), t.end());
            if (t.size() >= 2) events_.push_back(Event{std::move(t), w});
        };

        // OR-events: for each derived head, collect flips of incoming rules with p<1
        {
            std::unordered_map<size_t, std::vector<int>> head_to_flips; // head nodeId -> rule var_ids
            head_to_flips.reserve(view_->getEdges().size());
            for (const EdgePtr& e : view_->getEdges()) {
                if (!e || e->pruned) continue;
                if (e->getProbability() < 1.0) {
                    const NodePtr h = view_->getOutput(e);
                    if (h) {
                        const int rvid = mapEdgeId(e->getId());
                        if (idx_of_.count(rvid)) head_to_flips[h->getId()].push_back(rvid);
                    }
                }
            }
            for (auto& kv : head_to_flips) {
                auto& flips = kv.second;
                if (flips.size() >= 2) {
                    if (P_.verbose) std::cout << "OR-event head#" << kv.first
                                              << " size=" << flips.size() << "\n";
                    push_event(flips, P_.w_or);
                }
            }
        }

        // AND-events: for each rule, include its flip (if p<1) + body's probabilistic frontier (subject to frontier_depth)
        for (const EdgePtr& e : view_->getEdges()) {
            if (!e || e->pruned) continue;

            std::vector<int> vars;
            if (e->getProbability() < 1.0) {
                const int rvid = mapEdgeId(e->getId());
                if (idx_of_.count(rvid)) vars.push_back(rvid);
            }

            const auto inputs = view_->getInputs(e);
            const auto negs   = view_->getBodyNegations(e);
            for (size_t i=0; i<inputs.size(); ++i) {
                const NodePtr b = inputs[i];
                const bool neg  = (i < negs.size()) ? negs[i] : false;
                if (!b || neg) continue;
                if (b->isFact) {
                    if (!b->pruned && b->getProbability() < 1.0) {
                        const int vid = mapNodeId(b->getId());
                        if (idx_of_.count(vid)) vars.push_back(vid);
                    }
                } else if (P_.frontier_depth > 0) {
                    std::unordered_set<int> out;
                    std::unordered_set<size_t> seen;
                    frontierCollect(b, P_.frontier_depth, P_.and_size_budget, out, seen);
                    vars.insert(vars.end(), out.begin(), out.end());
                }
            }
            if (vars.size() >= 2) push_event(vars, P_.w_and);
        }

        // BRIDGE (type 1): body contains a derived fact B that has multiple alternative incoming rule flips.
        for (const EdgePtr& e : view_->getEdges()) {
            if (!e || e->pruned) continue;

            int myFlip = -1;
            if (e->getProbability() < 1.0) {
                const int rvid = mapEdgeId(e->getId());
                if (idx_of_.count(rvid)) myFlip = rvid;
            }

            const auto inputs = view_->getInputs(e);
            const auto negs   = view_->getBodyNegations(e);
            for (size_t i=0; i<inputs.size(); ++i) {
                const NodePtr b = inputs[i];
                const bool neg  = (i < negs.size()) ? negs[i] : false;
                if (!b || neg || b->isFact) continue; // derived only
                std::vector<int> bridge;
                if (myFlip >= 0) bridge.push_back(myFlip);
                const auto inE = view_->getIncomingEdges(b);
                for (const EdgePtr& pe : inE) {
                    if (!pe || pe->pruned) continue;
                    if (pe->getProbability() < 1.0) {
                        const int pvid = mapEdgeId(pe->getId());
                        if (idx_of_.count(pvid)) bridge.push_back(pvid);
                    }
                }
                if (bridge.size() >= 2) push_event(bridge, P_.w_bridge);
            }
        }

        // BRIDGE (type 2, optional): shared probabilistic input fact across multiple rules
        if (P_.enable_bridge_shared_input) {
            std::unordered_map<size_t, std::vector<int>> fact_to_ruleflips; // input fact id -> flips
            for (const EdgePtr& e : view_->getEdges()) {
                if (!e || e->pruned) continue;
                int rvid = -1;
                if (e->getProbability() < 1.0) {
                    const int t = mapEdgeId(e->getId());
                    if (idx_of_.count(t)) rvid = t;
                }
                const auto inputs = view_->getInputs(e);
                const auto negs   = view_->getBodyNegations(e);
                for (size_t i=0; i<inputs.size(); ++i) {
                    const NodePtr b = inputs[i];
                    const bool neg  = (i < negs.size()) ? negs[i] : false;
                    if (!b || neg || !b->isFact) continue;
                    if (!b->pruned && b->getProbability() < 1.0 && rvid >= 0) {
                        fact_to_ruleflips[b->getId()].push_back(rvid);
                    }
                }
            }
            for (auto& kv : fact_to_ruleflips) {
                auto& flips = kv.second;
                if (flips.size() >= 2) {
                    std::vector<int> bridge;
                    const int avid = mapNodeId(kv.first);
                    if (idx_of_.count(avid)) bridge.push_back(avid);
                    bridge.insert(bridge.end(), flips.begin(), flips.end());
                    if (bridge.size() >= 2) push_event(bridge, P_.w_bridge);
                }
            }
        }

        if (P_.verbose) {
            std::cout << "[Events] total=" << events_.size() << "\n";
        }
    }

    // ---------- Objective: total weighted span ----------
    double totalSpan(const std::vector<int>& order) const {
        if (events_.empty() || order.empty()) return 0.0;
        std::unordered_map<int,int> pos;
        pos.reserve(order.size());
        for (int i = 0; i < (int)order.size(); ++i) pos[order[i]] = i;

        double S = 0.0;
        for (const auto& e : events_) {
            int mn = std::numeric_limits<int>::max();
            int mx = std::numeric_limits<int>::min();
            for (int v : e.var_ids) {
                auto it = pos.find(v);
                if (it == pos.end()) continue; // shouldn't happen
                mn = std::min(mn, it->second);
                mx = std::max(mx, it->second);
            }
            if (mx >= mn) S += (mx - mn) * e.weight;
        }
        return S;
    }

    // ---------- FORCE without anchor ----------
    void forceFree() {
        order_.clear(); order_.reserve(vars_.size());
        for (const auto& v : vars_) order_.push_back(v.var_id);
        if (events_.empty() || order_.size() <= 1) return;

        for (int it = 0; it < P_.force_iters; ++it) {
            // current positions
            std::unordered_map<int,int> pos;
            pos.reserve(order_.size());
            for (int i = 0; i < (int)order_.size(); ++i) pos[order_[i]] = i;

            // event centers
            std::vector<double> center(events_.size(), 0.0);
            for (size_t ei = 0; ei < events_.size(); ++ei) {
                double s = 0.0; int c = 0;
                for (int v : events_[ei].var_ids) {
                    auto itp = pos.find(v);
                    if (itp != pos.end()) { s += itp->second; ++c; }
                }
                center[ei] = (c > 0 ? s / (double)c : 0.0);
            }

            // variable targets (weighted avg of centers)
            std::unordered_map<int,double> sumW, sumWC;
            sumW.reserve(order_.size()); sumWC.reserve(order_.size());
            for (size_t ei = 0; ei < events_.size(); ++ei) {
                const auto& ev = events_[ei];
                for (int v : ev.var_ids) {
                    sumW[v]  += ev.weight;
                    sumWC[v] += ev.weight * center[ei];
                }
            }
            std::vector<std::pair<int,double>> tgt;
            tgt.reserve(order_.size());
            for (int v : order_) {
                double t = (sumW[v] > 0 ? sumWC[v] / sumW[v] : (double)pos[v]);
                tgt.emplace_back(v, t);
            }

            // stable sort by target
            std::stable_sort(order_.begin(), order_.end(),
                [&](int a, int b) {
                    double ta = sumW[a] > 0 ? sumWC[a]/sumW[a] : pos[a];
                    double tb = sumW[b] > 0 ? sumWC[b]/sumW[b] : pos[b];
                    if (isNearlyEqual(ta, tb)) return a < b;
                    return ta < tb;
                });

            if (P_.verbose) {
                std::cout << "[FORCE] iter " << (it+1)
                          << " span = " << totalSpan(order_) << "\n";
            }
        }
    }

    // ---------- anchored merge: insert new variables around/between anchors by their (continuous) targets ----------
    static std::vector<int> anchoredMerge(const std::vector<std::pair<int,double>>& new_vars,
                                          const std::vector<int>& anchor_order) {
        const int k = (int)anchor_order.size();
        std::vector<std::pair<int,double>> nv = new_vars;
        std::stable_sort(nv.begin(), nv.end(),
            [](const auto& a, const auto& b){
                if (std::fabs(a.second - b.second) < 1e-12) return a.first < b.first;
                return a.second < b.second;
            });

        std::vector<int> out; out.reserve(k + nv.size());
        size_t p = 0;

        // (<0) at front
        while (p < nv.size() && nv[p].second < 0.0) { out.push_back(nv[p].first); ++p; }

        // between anchors i and i+1 on integer lattice [i, i+1)
        for (int i = 0; i < k; ++i) {
            out.push_back(anchor_order[i]); // place anchor i
            while (p < nv.size() && nv[p].second >= i && nv[p].second < (i+1)) {
                out.push_back(nv[p].first); ++p;
            }
        }
        // (>=k) at tail
        while (p < nv.size()) { out.push_back(nv[p].first); ++p; }

        return out;
    }

    // ---------- FORCE with anchor (anchors fixed; only new variables move) ----------
    void forceAnchored() {
        // split universe into anchors vs free
        std::unordered_set<int> A(anchor_order_.begin(), anchor_order_.end());
        std::vector<int> free_vars; free_vars.reserve(vars_.size());
        for (const auto& v : vars_) if (!A.count(v.var_id)) free_vars.push_back(v.var_id);

        if (free_vars.empty()) { order_ = anchor_order_; return; }
        if (anchor_order_.empty()) { forceFree(); return; }

        // Start with an initial merge based on anchor-only centers
        std::unordered_map<int,int> anchor_pos;
        for (int i = 0; i < (int)anchor_order_.size(); ++i) anchor_pos[anchor_order_[i]] = i;

        auto compute_targets_given_pos =
            [&](const std::unordered_map<int,int>& pos_map,
                const std::vector<int>& movable) -> std::vector<std::pair<int,double>> {
                std::vector<double> center(events_.size(), 0.0);
                for (size_t ei = 0; ei < events_.size(); ++ei) {
                    double s = 0.0; int c = 0;
                    for (int v : events_[ei].var_ids) {
                        auto itp = pos_map.find(v);
                        if (itp != pos_map.end()) { s += itp->second; ++c; }
                    }
                    center[ei] = (c>0 ? s/(double)c : 0.0);
                }
                std::unordered_map<int,double> sumW, sumWC;
                sumW.reserve(movable.size()); sumWC.reserve(movable.size());
                for (size_t ei = 0; ei < events_.size(); ++ei) {
                    const auto& ev = events_[ei];
                    for (int v : ev.var_ids) {
                        // only accumulate for movable vars
                        // (anchors' targets are irrelevant—they don't move)
                        if (!A.count(v)) {
                            sumW[v]  += ev.weight;
                            sumWC[v] += ev.weight * center[ei];
                        }
                    }
                }
                std::vector<std::pair<int,double>> tgt;
                tgt.reserve(movable.size());
                for (int v : movable) {
                    double t = (sumW[v] > 0 ? sumWC[v]/sumW[v] : 0.0);
                    tgt.emplace_back(v, t);
                }
                return tgt;
            };

        // Initial merge using anchor positions only
        std::vector<std::pair<int,double>> init_targets = compute_targets_given_pos(anchor_pos, free_vars);
        order_ = anchoredMerge(init_targets, anchor_order_);

        double best = totalSpan(order_);
        if (P_.verbose) {
            std::cout << "[FORCE-anchored] init span = " << best << "\n";
        }

        // Iteratively refine: use last merged order to compute positions; anchors never change relative order
        for (int it = 0; it < P_.force_iters; ++it) {
            // build pos from current order_
            std::unordered_map<int,int> pos;
            pos.reserve(order_.size());
            for (int i = 0; i < (int)order_.size(); ++i) pos[order_[i]] = i;

            // targets for free vars
            std::vector<std::pair<int,double>> targets = compute_targets_given_pos(pos, free_vars);
            std::vector<int> merged = anchoredMerge(targets, anchor_order_);
            double s = totalSpan(merged);
            if (P_.verbose) {
                std::cout << "[FORCE-anchored] iter " << (it+1) << " span = " << s << "\n";
            }
            if (s + 1e-12 < best) {
                best = s;
                order_.swap(merged);
            } else {
                break; // no improvement
            }
        }
    }
};



// ================================================================
// BDDSpanHeuristics (header-only, C++17)
//  - Consumes a DerivationGraphViewInterface
//  - Builds AND / OR / BRIDGE hyperedges using FrontierProb folding
//  - Applies weighted FORCE ordering
//  - Cuts clusters by crossing density (zero-only by default)
//  - Refines intra-cluster order (fanin level + co-occur + micro-keys)
//  - Stitches clusters by causal DAG
//  - Returns final variable ordering (std::vector<int> formula var ids)
// Notes:
//   * Variables include ONLY probabilistic input facts (node->isFact && p<1)
//     and probabilistic rules (edge->getProbability()<1).
//   * Derived facts, and any prob=1 nodes/edges are excluded everywhere.
// ================================================================

/*
BDDSpanHeuristics::Params p;
p.verbose = true;                 // 打开调试
// p.cut_near_zero = true;        // 可选更细切簇
// p.cut_local_valley = true;     // 可选局部谷值切割
// p.max_cluster_size = 40;       // 可选最大簇大小

BDDSpanHeuristics h(view, p);
const std::vector<int>& varOrder = h.getOrdering();

// 或者快速自测打印：
BDDSpanHeuristics::SelfTest(view);
*/

class BDDSpanHeuristics {
public:
    // Parameters (tweak as needed)
    struct Params {
        int frontier_depth;          // FrontierProb fold depth
        int and_size_budget;         // cap FrontierProb set size
        double w_and;                // weight: AND
        double w_or;                 // weight: OR
        double w_bridge;             // weight: BRIDGE
        int force_iters;             // FORCE iterations
        bool cut_zero_only;          // cut clusters only at density==0 (default)
        bool cut_near_zero;          // optional: allow near-zero (<=1) as cuts
        bool cut_local_valley;       // optional: local minima as cuts
        int max_cluster_size;        // optional: split clusters if > this (0 = off)
        bool verbose;                // debug prints

        Params()
            : frontier_depth(4),
              and_size_budget(12),
              w_and(1.0),
              w_or(0.30),
              w_bridge(0.15),
              force_iters(20),
              cut_zero_only(false),
              cut_near_zero(false),
              cut_local_valley(true),
              max_cluster_size(0),
              verbose(true) {}
    };

    explicit BDDSpanHeuristics(const DerivationGraphViewInterface& g, Params p = Params())
        : g_(g), P(p) {
        // output timing every phrases
        using namespace std::chrono;

        if (P.verbose) std::cout << "[BDDSpanHeuristics] Timing function calls:\n";

        auto start = high_resolution_clock::now();
        buildVariables(); // collect probabilistic variables only
        auto end = high_resolution_clock::now();
        if (P.verbose) std::cout << "  buildVariables: " << duration_cast<milliseconds>(end - start).count() << " ms\n";

        start = high_resolution_clock::now();
        buildEvents(); // AND/OR/BRIDGE hypered ges
        end = high_resolution_clock::now();
        if (P.verbose) std::cout << "  buildEvents: " << duration_cast<milliseconds>(end - start).count() << " ms\n";

        start = high_resolution_clock::now();
        forceOrder(); // initial FORCE ordering
        end = high_resolution_clock::now();
        if (P.verbose) std::cout << "  forceOrder: " << duration_cast<milliseconds>(end - start).count() << " ms\n";

        start = high_resolution_clock::now();
        clusterCut(); // cut into clusters
        end = high_resolution_clock::now();
        if (P.verbose) std::cout << "  clusterCut: " << duration_cast<milliseconds>(end - start).count() << " ms\n";

        start = high_resolution_clock::now();
        intraClusterRefine(); // sort within clusters
        end = high_resolution_clock::now();
        if (P.verbose) std::cout << "  intraClusterRefine: " << duration_cast<milliseconds>(end - start).count() << " ms\n";

        start = high_resolution_clock::now();
        interClusterStitch(); // topo-stitch clusters
        end = high_resolution_clock::now();
        if (P.verbose) std::cout << "  interClusterStitch: " << duration_cast<milliseconds>(end - start).count() << " ms\n";

    }

    // Final linear ordering of formula variables (ids from mapNodeId/mapEdgeId)
    const std::vector<int>& getOrdering() const { return final_order_; }

    // For debugging/inspection
    void printSummary(std::ostream& os = std::cout) const {
        os << "[BDDSpanHeuristics] |V|=" << vars_.size()
           << "  |E|=" << edges_.size()
           << "  (AND=" << countEdges("AND")
           << " OR=" << countEdges("OR")
           << " BRIDGE=" << countEdges("BRIDGE") << ")\n";
    }


    // Simple self-test helper: run heuristics on a provided view and print the final order.
    static void SelfTest(const DerivationGraphViewInterface& view) {
        Params p;
        p.verbose = true;
        BDDSpanHeuristics H(view, p);
        H.printSummary();
        const auto& ord = H.getOrdering();
        std::cout << "[SelfTest] Final variable order ids: ";
        for (size_t i=0;i<ord.size();++i) {
            if (i) std::cout << ", ";
            std::cout << ord[i];
        }
        std::cout << std::endl;
    }
private:
    enum class VType { Fact, Rule };

    struct Var {
        VType t;
        int var_id;            // formula variable id (from mapNodeId/mapEdgeId)
        NodePtr node;          // if t==Fact, non-null
        EdgePtr edge;          // if t==Rule, non-null
        std::string desc;      // for debug
    };

    struct Hyperedge {
        std::string tag;       // "AND" | "OR" | "BRIDGE"
        double weight;
        std::vector<int> var_ids; // formula var ids (a subset of universe)
    };

    const DerivationGraphViewInterface& g_;
    Params P;

    // Universe of variables to order (prob<1 input facts, prob<1 rules)
    std::vector<Var> vars_;
    // quick membership: var_id -> index in vars_
    std::unordered_map<int,size_t> idx_of_;

    // Hyperedges
    std::vector<Hyperedge> edges_;

    // FORCE order (var ids)
    std::vector<int> order_;        // working order after FORCE
    // Clusters (each is a list of var ids)
    std::vector<std::vector<int>> clusters_;
    // Final order
    std::vector<int> final_order_;

    // ------------------------------------------------------------
    // Utilities
    // ------------------------------------------------------------
    bool isProbabilisticInputFact(const NodePtr& n) const {
        if (!n) return false;
        if (!n->isFact) return false;               // derived facts are excluded
        double p = n->getProbability();
        return (p < 1.0) && !n->pruned;
    }
    bool isProbabilisticRule(const EdgePtr& e) const {
        if (!e) return false;
        double p = e->getProbability();
        return (p < 1.0) && !e->pruned;
    }
    bool varExists(int vid) const {
        return idx_of_.find(vid) != idx_of_.end();
    }
    size_t countEdges(const std::string& tag) const {
        size_t c=0; for (auto& he: edges_) if (he.tag==tag) ++c; return c;
    }
    void ensureDistinct(const std::vector<int>& xs, const char* msg) const {
        std::unordered_set<int> s;
        for (int v: xs) {
            assert(s.insert(v).second && msg);
        }
    }
    // Make a readable name for a var id
    std::string varName(int vid) const {
        auto it = idx_of_.find(vid);
        if (it==idx_of_.end()) return std::string("UNKNOWN(")+std::to_string(vid)+")";
        const Var& v = vars_[it->second];
        return v.desc + " [id=" + std::to_string(vid) + "]";
    }

    // ------------------------------------------------------------
    // 1) Build universe variables (prob<1 input facts, prob<1 rules)
    // ------------------------------------------------------------
    void buildVariables() {
        vars_.clear(); idx_of_.clear();
        if (P.verbose) std::cout << "[buildVariables] scanning graph...\n";

        // deterministic filter stats
        size_t fact_all=0, fact_kept=0, rule_all=0, rule_kept=0;

        // facts
        for (const auto& n : g_.getNodes()) {
            ++fact_all;
            if (isProbabilisticInputFact(n)) {
                int vid = mapNodeId(n->getId());
                Var v{VType::Fact, vid, n, nullptr, std::string("Fact: ")+n->toString()};
                if (idx_of_.count(vid)) {
                    // should never happen because mapNodeId is global unique mapping
                    assert(false && "duplicate formula var id for fact");
                }
                idx_of_[vid] = vars_.size();
                vars_.push_back(std::move(v));
                ++fact_kept;
                if (P.verbose) std::cout << "  + keep FACT  " << n->toString()
                                         << " -> var " << vid << "\n";
            } else {
                if (P.verbose) {
                    std::cout << "  - drop FACT  " << n->toString()
                              << " (isFact=" << n->isFact
                              << ", p=" << n->getProbability() << ", pruned=" << n->pruned << ")\n";
                }
            }
        }

        // rules
        for (const auto& e : g_.getEdges()) {
            ++rule_all;
            if (isProbabilisticRule(e)) {
                int vid = mapEdgeId(e->getId());
                Var v{VType::Rule, vid, nullptr, e, ruleEdgeName(e)};
                if (idx_of_.count(vid)) {
                    assert(false && "duplicate formula var id for rule");
                }
                idx_of_[vid] = vars_.size();
                vars_.push_back(std::move(v));
                ++rule_kept;
                if (P.verbose) std::cout << "  + keep RULE  " << v.desc
                                         << " -> var " << vid << "\n";
            } else {
                if (P.verbose) {
                    std::cout << "  - drop RULE  " << ruleEdgeName(e)
                              << " (p=" << e->getProbability() << ", pruned=" << e->pruned << ")\n";
                }
            }
        }

        if (P.verbose) {
            std::cout << "[buildVariables] kept " << (fact_kept+rule_kept)
                      << " vars (facts kept " << fact_kept << "/" << fact_all
                      << ", rules kept " << rule_kept << "/" << rule_all << ")\n";
        }

        // sanity: unique ids
        std::vector<int> vids; vids.reserve(vars_.size());
        for (auto& v: vars_) vids.push_back(v.var_id);
        ensureDistinct(vids, "duplicate var ids in vars_");
        if (P.verbose) std::cout << "[buildVariables] done.\n\n";
    }

    std::string ruleEdgeName(const EdgePtr& e) const {
        NodePtr out = g_.getOutput(e);
        std::string head = out ? out->toString() : std::string("<?>");
        return std::string("Rule->") + head + " (edge #" + std::to_string(e->getId()) + ")";
    }

    // ------------------------------------------------------------
    // FrontierProb folding (depth-limited)
    // returns a set of formula var ids (subset of universe) that co-occur
    // ------------------------------------------------------------
    void frontierProb_collectFromNode(NodePtr atom, int depth,
                                      std::unordered_set<int>& acc) const {
        if (!atom) return;
        if (atom->isFact) {
            if (atom->getProbability() < 1.0 && !atom->pruned) {
                int vid = mapNodeId(atom->getId());
                if (varExists(vid)) acc.insert(vid);
            }
            return;
        }
        // derived fact
        auto in = g_.getIncomingEdges(atom); // rules producing this fact
        // stop at OR (>=2 in-edges)
        if (in.size() != 1) return;
        EdgePtr e = in[0];
        if (e && e->getProbability() < 1.0 && !e->pruned) {
            int rvid = mapEdgeId(e->getId());
            if (varExists(rvid)) acc.insert(rvid);
        }
        if (depth >= P.frontier_depth) return;
        // fold into its body
        auto body = g_.getInputs(e);
        auto negs = g_.getBodyNegations(e);
        for (size_t i=0;i<body.size();++i) {
            if (i<negs.size() && negs[i]) continue; // skip negated
            frontierProb_collectFromNode(body[i], depth+1, acc);
            if ((int)acc.size() >= P.and_size_budget) return;
        }
    }

    // ------------------------------------------------------------
    // 2) Build hyperedges: AND / OR / BRIDGE
    // ------------------------------------------------------------
    void buildEvents() {
        edges_.clear();
        if (P.verbose) std::cout << "[buildEvents] building AND/OR/BRIDGE...\n";

        // index: head fact -> producing rule flips (var ids)
        std::unordered_map<NodePtr, std::vector<int>> prod;
        prod.reserve(g_.getNodes().size());
        for (const auto& e : g_.getEdges()) {
            if (!isProbabilisticRule(e)) continue;
            NodePtr h = g_.getOutput(e);
            int rvid = mapEdgeId(e->getId());
            if (!varExists(rvid)) continue; // safety
            prod[h].push_back(rvid);
        }

        // OR hyperedges: flips that produce the same head (>=2)
        for (auto& kv : prod) {
            const std::vector<int>& flips = kv.second;
            if (flips.size() >= 2) {
                Hyperedge he; he.tag="OR"; he.weight=P.w_or; he.var_ids=flips;
                dedup_and_filter(he.var_ids);
                if (he.var_ids.size()>=2) {
                    if (P.verbose) printEdge(he);
                    edges_.push_back(std::move(he));
                }
            }
        }

        // AND + BRIDGE
        for (const auto& e : g_.getEdges()) {
            if (!isProbabilisticRule(e)) continue;
            int rvid = mapEdgeId(e->getId());
            if (!varExists(rvid)) continue;
            // AND: rule flip + frontier of each (non-negated) body atom
            Hyperedge andE; andE.tag="AND"; andE.weight=P.w_and;
            andE.var_ids.push_back(rvid);
            auto body = g_.getInputs(e);
            auto negs = g_.getBodyNegations(e);
            for (size_t i=0;i<body.size();++i) {
                if (i<negs.size() && negs[i]) continue;
                std::unordered_set<int> front;
                frontierProb_collectFromNode(body[i], 0, front);
                for (int vid: front) andE.var_ids.push_back(vid);
            }
            dedup_and_filter(andE.var_ids);
            if (andE.var_ids.size()>=2) {
                if (P.verbose) printEdge(andE);
                edges_.push_back(std::move(andE));
            }

            // BRIDGE: for any derived body B with >=2 producers: {flip_e} ∪ flips(B)
            for (size_t i=0;i<body.size();++i) {
                if (i<negs.size() && negs[i]) continue;
                NodePtr B = body[i];
                if (!B || B->isFact) continue; // derived only
                auto alts = g_.getIncomingEdges(B);
                std::vector<int> flips;
                for (const auto& e2 : alts) {
                    if (!isProbabilisticRule(e2)) continue;
                    int fvid = mapEdgeId(e2->getId());
                    if (varExists(fvid)) flips.push_back(fvid);
                }
                if (flips.size()>=2) {
                    Hyperedge br; br.tag="BRIDGE"; br.weight=P.w_bridge;
                    br.var_ids = flips;
                    br.var_ids.push_back(rvid);
                    dedup_and_filter(br.var_ids);
                    if (br.var_ids.size()>=2) {
                        if (P.verbose) printEdge(br);
                        edges_.push_back(std::move(br));
                    }
                }
            }
        }

        if (P.verbose) {
            std::cout << "[buildEvents] total hyperedges: " << edges_.size()
                      << " (AND="<<countEdges("AND")
                      << ", OR="<<countEdges("OR")
                      << ", BRIDGE="<<countEdges("BRIDGE")<<")\n\n";
        }
    }

    void dedup_and_filter(std::vector<int>& vids) const {
        // remove unknown ids & duplicates
        std::vector<int> f;
        f.reserve(vids.size());
        for (int v: vids) if (varExists(v)) f.push_back(v);
        std::sort(f.begin(), f.end());
        f.erase(std::unique(f.begin(), f.end()), f.end());
        vids.swap(f);
    }

    void printEdge(const Hyperedge& he) const {
        std::cout << "  ["<<he.tag<<"] w="<<he.weight<<" { ";
        for (size_t i=0;i<he.var_ids.size();++i) {
            if (i) std::cout << ", ";
            std::cout << varName(he.var_ids[i]);
        }
        std::cout << " }\n";
    }

    // ------------------------------------------------------------
    // 3) FORCE ordering (minimize weighted hyperedge span)
    // ------------------------------------------------------------
    double totalSpan(const std::vector<int>& ord) const {
        // ord is a sequence of var ids
        std::unordered_map<int,int> pos; pos.reserve(ord.size());
        for (size_t i=0;i<ord.size();++i) pos[ord[i]] = (int)i;
        double sum = 0.0;
        for (auto& e: edges_) {
            int lo=INT_MAX, hi=INT_MIN;
            for (int v: e.var_ids) {
                auto it = pos.find(v); assert(it!=pos.end());
                lo = std::min(lo, it->second);
                hi = std::max(hi, it->second);
            }
            sum += e.weight * (hi - lo);
        }
        return sum;
    }

    void forceOrder() {
        order_.clear();
        order_.reserve(vars_.size());
        for (auto& v: vars_) order_.push_back(v.var_id);

        if (P.verbose) {
            std::cout << "[FORCE] start: |V|="<<order_.size()<<" |E|="<<edges_.size()<<"\n";
            std::cout << "  initial span: "<< totalSpan(order_) << "\n";
        }

        if (order_.empty() || edges_.empty()) return;

        for (int it=0; it<P.force_iters; ++it) {
            // compute hyperedge centers
            std::unordered_map<int,int> pos; pos.reserve(order_.size());
            for (size_t i=0;i<order_.size();++i) pos[order_[i]]=(int)i;

            std::vector<double> centers; centers.reserve(edges_.size());
            for (auto& e: edges_) {
                double s=0.0; for (int v: e.var_ids) s += pos[v];
                centers.push_back(s / std::max<size_t>(1, e.var_ids.size()));
            }

            // target positions
            std::unordered_map<int,double> target;
            for (size_t ei=0; ei<edges_.size(); ++ei) {
                const auto& e = edges_[ei];
                double c = centers[ei];
                for (int v: e.var_ids) {
                    target[v] += e.weight * c;
                }
            }
            std::unordered_map<int,double> wsum;
            for (const auto& e: edges_) {
                for (int v: e.var_ids) wsum[v] += e.weight;
            }
            std::vector<int> new_order = order_;
            std::stable_sort(new_order.begin(), new_order.end(),
                [&](int a, int b){
                    double ta = (wsum[a]>0 ? target[a]/wsum[a] : (double)std::distance(order_.begin(), std::find(order_.begin(), order_.end(), a)));
                    double tb = (wsum[b]>0 ? target[b]/wsum[b] : (double)std::distance(order_.begin(), std::find(order_.begin(), order_.end(), b)));
                    if (std::fabs(ta-tb) < 1e-9) return a<b;
                    return ta<tb;
                });

            double oldS = totalSpan(order_);
            double newS = totalSpan(new_order);
            if (P.verbose) std::cout << "  iter "<<it+1<<" span "<<oldS<<" -> "<<newS<<"\n";
            if (newS <= oldS - 1e-9) {
                order_.swap(new_order);
            } else {
                break; // no improvement
            }
        }
        if (P.verbose) std::cout << "[FORCE] done. span="<< totalSpan(order_) << "\n\n";
    }

    // ------------------------------------------------------------
    // 4) Cut clusters (crossing density valleys)
    // ------------------------------------------------------------
    std::vector<double> crossingDensity(const std::vector<int>& ord) const {
        std::unordered_map<int,int> pos; pos.reserve(ord.size());
        for (size_t i=0;i<ord.size();++i) pos[ord[i]]=(int)i;
        std::vector<double> dens(ord.size()? ord.size()-1:0, 0.0);
        for (auto& e: edges_) {
            int lo=INT_MAX, hi=INT_MIN;
            for (int v: e.var_ids) {
                auto it=pos.find(v); assert(it!=pos.end());
                lo = std::min(lo, it->second);
                hi = std::max(hi, it->second);
            }
            for (int i=lo;i<hi;i++) dens[i] += e.weight;
        }
        return dens;
    }

    void clusterCut() {
        clusters_.clear();
        if (order_.empty()) return;
        auto dens = crossingDensity(order_);
        std::vector<size_t> cuts;

        for (size_t i=0;i<dens.size();++i) {
            bool cut=false;
            if (dens[i]==0.0 && P.cut_zero_only) cut=true;
            if (P.cut_near_zero && dens[i]<=1.0) cut=true;
            if (P.cut_local_valley) {
                double prev = (i>0? dens[i-1] : 1e9);
                double next = (i+1<dens.size()? dens[i+1] : 1e9);
                if (dens[i] < prev && dens[i] < next) cut=true;
            }
            if (cut) cuts.push_back(i);
        }
        size_t s=0;
        for (size_t c: cuts) {
            if (c+1 > s) {
                clusters_.push_back(slice(order_, s, c+1));
                s = c+1;
            }
        }
        if (s < order_.size()) clusters_.push_back(slice(order_, s, order_.size()));

        // enforce max cluster size (optional)
        if (P.max_cluster_size>0) {
            std::vector<std::vector<int>> resized;
            for (auto& C: clusters_) {
                if ((int)C.size() <= P.max_cluster_size) { resized.push_back(C); continue; }
                for (size_t i=0;i<C.size(); i += (size_t)P.max_cluster_size) {
                    size_t j = std::min(C.size(), i+(size_t)P.max_cluster_size);
                    resized.push_back(slice(C, i, j));
                }
            }
            clusters_.swap(resized);
        }

        if (P.verbose) {
            std::cout << "[Cut] clusters="<<clusters_.size()<<"\n";
            for (size_t i=0;i<clusters_.size();++i) {
                std::cout << "  C"<<i<<" size="<<clusters_[i].size()<<": ";
                printList(clusters_[i]); std::cout << "\n";
            }
            std::cout << "\n";
        }
    }

    static std::vector<int> slice(const std::vector<int>& v, size_t i, size_t j) {
        return std::vector<int>(v.begin()+i, v.begin()+j);
    }
    void printList(const std::vector<int>& v) const {
        std::cout << "{ ";
        for (size_t k=0;k<v.size();++k) {
            if (k) std::cout << ", ";
            std::cout << varName(v[k]);
        }
        std::cout << " }";
    }

    // ------------------------------------------------------------
    // 5) Intra-cluster refinement
    // ------------------------------------------------------------
    void intraClusterRefine() {
        for (auto& C : clusters_) {
            // compute fanin levels within this cluster
            std::unordered_map<int,int> lvl = computeLevels(C);
            // local co-occurrence score
            std::unordered_map<int,double> co = localCooccur(C);
            // micro-key: shared premise (0), rule flip (1), unique premise (2)
            std::unordered_map<int,int> micro = microKeys(C);

            std::stable_sort(C.begin(), C.end(), [&](int a, int b){
                int la = lvl.count(a)?lvl[a]:INT_MAX;
                int lb = lvl.count(b)?lvl[b]:INT_MAX;
                if (la!=lb) return la<lb;
                double ca = co.count(a)?co[a]:0.0;
                double cb = co.count(b)?co[b]:0.0;
                if (std::fabs(ca-cb)>1e-9) return ca>cb;
                int ma = micro.count(a)?micro[a]:1;
                int mb = micro.count(b)?micro[b]:1;
                if (ma!=mb) return ma<mb;
                return a<b;
            });
        }
    }

    std::unordered_map<int,int> computeLevels(const std::vector<int>& C) const {
        std::unordered_set<int> S(C.begin(), C.end());
        std::unordered_map<int,int> level;
        bool changed=true;
        int rounds=0;
        while (changed && rounds++<64) {
            changed=false;
            // rules: level = 1 + max(level(prereqs))
            for (const auto& v: vars_) {
                if (S.count(v.var_id)==0) continue;
                if (v.t == VType::Rule) {
                    int r = 0;
                    EdgePtr e = v.edge;
                    // prerequisites = FrontierProb from each body atom
                    auto body = g_.getInputs(e);
                    auto negs = g_.getBodyNegations(e);
                    std::unordered_set<int> deps;
                    for (size_t i=0;i<body.size();++i) {
                        if (i<negs.size() && negs[i]) continue;
                        frontierProb_collectFromNode(body[i], 0, deps);
                    }
                    int best = -1;
                    for (int d: deps) if (S.count(d)) best = std::max(best, level.count(d)?level[d]:0);
                    int newL = (best<0?1:best+1);
                    if (!level.count(v.var_id) || level[v.var_id]!=newL) { level[v.var_id]=newL; changed=true; }
                } else {
                    // facts stay low
                    if (!level.count(v.var_id)) { level[v.var_id]=0; changed=true; }
                }
            }
        }
        return level;
    }

    std::unordered_map<int,double> localCooccur(const std::vector<int>& C) const {
        std::unordered_set<int> S(C.begin(), C.end());
        std::unordered_map<int,double> deg;
        for (auto& e: edges_) {
            int cnt=0; for (int v: e.var_ids) if (S.count(v)) ++cnt;
            if (cnt>=2) {
                for (int v: e.var_ids) if (S.count(v)) deg[v] += e.weight*(cnt-1);
            }
        }
        return deg;
    }

    std::unordered_map<int,int> microKeys(const std::vector<int>& C) const {
        std::unordered_set<int> S(C.begin(), C.end());
        // count how many rules (within cluster) use each fact-variable as premise
        std::unordered_map<int,int> countUse;
        for (const auto& v: vars_) {
            if (S.count(v.var_id)==0) continue;
            if (v.t != VType::Rule) continue;
            auto body = g_.getInputs(v.edge);
            auto negs = g_.getBodyNegations(v.edge);
            std::unordered_set<int> deps;
            for (size_t i=0;i<body.size();++i) {
                if (i<negs.size() && negs[i]) continue;
                frontierProb_collectFromNode(body[i], 0, deps);
            }
            for (int d: deps) if (S.count(d)) countUse[d]++;
        }
        std::unordered_map<int,int> key;
        for (int vid : C) {
            auto it = idx_of_.find(vid);
            assert(it!=idx_of_.end());
            const Var& v = vars_[it->second];
            if (v.t == VType::Fact) {
                int uses = countUse[vid];
                key[vid] = (uses>=2?0:2);  // shared premise first (0), unique later (2)
            } else {
                key[vid] = 1;              // rule flip in the middle
            }
        }
        return key;
    }

    // ------------------------------------------------------------
    // 6) Inter-cluster stitching (cluster DAG)
    // ------------------------------------------------------------
    void interClusterStitch() {
        // Map var id -> cluster index
        std::unordered_map<int,int> which;
        for (size_t i=0;i<clusters_.size();++i) for (int v: clusters_[i]) which[v]=(int)i;
        // Build cluster-level edges
        std::vector<std::vector<int>> adj(clusters_.size());
        std::vector<int> indeg(clusters_.size(), 0);

        for (const auto& v : vars_) {
            if (v.t != VType::Rule) continue;
            int rvid = v.var_id;
            auto body = g_.getInputs(v.edge);
            auto negs = g_.getBodyNegations(v.edge);
            std::unordered_set<int> deps;
            for (size_t i=0;i<body.size();++i) {
                if (i<negs.size() && negs[i]) continue;
                frontierProb_collectFromNode(body[i], 0, deps);
            }
            for (int d: deps) {
                auto itA = which.find(d), itB = which.find(rvid);
                if (itA==which.end() || itB==which.end()) continue;
                if (itA->second != itB->second) {
                    adj[itA->second].push_back(itB->second);
                }
            }
        }
        // dedup and compute indeg
        for (auto& lst: adj) {
            std::sort(lst.begin(), lst.end());
            lst.erase(std::unique(lst.begin(), lst.end()), lst.end());
        }
        for (size_t i=0;i<adj.size();++i) for (int v: adj[i]) indeg[v]++;

        // topo sort
        std::vector<int> q;
        for (size_t i=0;i<indeg.size();++i) if (indeg[i]==0) q.push_back((int)i);
        std::vector<int> topo;
        while (!q.empty()) {
            int u = q.back(); q.pop_back(); topo.push_back(u);
            for (int v: adj[u]) if (--indeg[v]==0) q.push_back(v);
        }
        if (topo.size()!=clusters_.size()) {
            // cycle -> fallback to original cluster order
            topo.clear();
            for (size_t i=0;i<clusters_.size();++i) topo.push_back((int)i);
        }

        // concatenate clusters in topo order
        final_order_.clear();
        for (int ci: topo) {
            final_order_.insert(final_order_.end(), clusters_[ci].begin(), clusters_[ci].end());
        }

        if (P.verbose) {
            std::cout << "[Stitch] cluster topo:";
            for (size_t i=0;i<topo.size();++i) std::cout << " " << topo[i];
            std::cout << "\n[Final] order ("<<final_order_.size()<<"):\n  ";
            printList(final_order_); std::cout << "\n\n";
        }

        // Sanity: every var in final_order_ must exist in universe
        for (int v: final_order_) assert(varExists(v) && "final order contains unknown var id");
    }
};

#endif // GRAPHHEURISTICS_H
