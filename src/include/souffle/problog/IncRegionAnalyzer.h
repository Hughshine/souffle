
#pragma once
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <memory>
#include <queue>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include <climits>
#include <initializer_list>
#include <chrono>

// The user's request: include the graph API like this
#include "souffle/problog/DerivationGraph.h"

// Everything lives in one header, in namespace incra.
namespace incra {

// Shorthand aliases taken from the host DerivationGraph.h.
using NodePtr = std::shared_ptr<Node>;
using EdgePtr = std::shared_ptr<Hyperedge>;
using DGView  = IncrementalDerivationGraphViewInterface;

// A small helper to stringify a node and an edge consistently.
static inline std::string node_id(const NodePtr& n) {
    return n ? n->getTuple().toString() : std::string("<null>");
}
static inline std::string edge_id(const EdgePtr& e, DGView& view) {
    std::ostringstream oss;
    oss << "[";
    auto ins = view.getInputs(e);
    for (size_t i = 0; i < ins.size(); ++i) {
        if (i) oss << ", ";
        oss << node_id(ins[i]);
    }
    oss << " -> " << node_id(view.getOutput(e)) << "]";
    return oss.str();
}

struct Region {
    std::unordered_set<NodePtr> nodes;
    std::unordered_set<EdgePtr> edges;
};

struct Boundaries {
    std::set<NodePtr> out_induced;   // region nodes whose outgoing edges received new inputs
    std::set<NodePtr> scope_induced; // region nodes that are least parents of delta scopes
    std::set<NodePtr> residual;      // remaining boundary nodes
};

struct IncRegionAnalysis {
    Region region;
    Boundaries boundaries;
    std::unordered_map<NodePtr, std::vector<EdgePtr>> mergeableAnchorsByHead;
};

struct Stats {
    size_t optimized_recomputed = 0;  // #non-fact nodes to recompute under our region
    size_t naive_recomputed     = 0;  // #non-fact nodes in delta-reachable
    size_t dr_nodes             = 0;  // delta-reachable nodes
    size_t region_nodes         = 0;  // region nodes
    size_t dr_edges             = 0;  // delta-reachable edges
    size_t region_edges         = 0;  // region edges
    double ratio() const {
        return naive_recomputed == 0 ? 1.0 :
               double(optimized_recomputed) / double(naive_recomputed);
    }
    long   diff() const { return long(naive_recomputed) - long(optimized_recomputed); }
};

// The analyzer. All heavy lifting is here.
class RegionAnalyzer {
public:
    explicit RegionAnalyzer(DGView& view)
    : view_(view) {}

    // Main entry: run analysis and emit outputs.
    Stats analyze(const std::vector<NodePtr>& delta_input_facts,
                  const std::string& json_out_path = "",
                  const std::string& csv_out_path  = "") {
        auto now = [] { return std::chrono::steady_clock::now(); };
        auto toMs = [](auto dur) { return std::chrono::duration<double, std::milli>(dur).count(); };
        auto t0 = now();

        // Build "least-parents" and "scopes". Pre- and post- ignoring insert edges is supported,
        // but we focus on "now" for region computation.
        buildLeastParents_();
        debugPrintLeastParents_();
        auto t1 = now();
        computeScopes_();
        debugPrintNodeScopes_();
        debugPrintEdgeScopes_();
        auto t2 = now();

        last_delta_inputs_.clear();
        last_delta_inputs_.insert(delta_input_facts.begin(), delta_input_facts.end());
        last_delta_nodes_ = view_.getDeltaInsertNodes();
        last_delta_nodes_.insert(last_delta_inputs_.begin(), last_delta_inputs_.end());
        last_delta_edges_.clear();
        auto deltaEdgeSet = view_.getDeltaInsertEdges();
        last_delta_edges_.insert(deltaEdgeSet.begin(), deltaEdgeSet.end());
        current_delta_sources_ = last_delta_inputs_;
        if (current_delta_sources_.empty()) {
            current_delta_sources_ = last_delta_nodes_;
        }
        reach_filter_ = reachFromCache_(current_delta_sources_);
        auto t3 = now();

        // Initial region + boundaries + expansion
        Region region = initialRegion_(delta_input_facts);
        auto t4 = now();
        Boundaries B  = classifyBoundaries_(region);
        auto t5 = now();
        expandToFixpoint_(region, B);
        auto t6 = now();

        // Upstream closure: include ancestors (within reach_filter_) feeding region nodes,
        // then re-classify boundaries and re-run expansion to stabilize.
        double reclass_ms = 0.0;
        double reexpand_ms = 0.0;
        auto upstream_start = now();
        bool upstreamExpanded = upstreamClose_(region);
        auto upstream_end = now();
        double upstream_ms = toMs(upstream_end - t6);
        auto t7 = upstream_end;
        if (upstreamExpanded) {
            auto tr1 = now();
            B = classifyBoundaries_(region);
            auto tr2 = now();
            expandToFixpoint_(region, B);
            auto tr3 = now();
            reclass_ms = toMs(tr2 - tr1);
            reexpand_ms = toMs(tr3 - tr2);
            t7 = tr3;
        }

        // Final safeguard: region must be a subset of delta-reachable
        auto dr = deltaReachable_(delta_input_facts);
        auto t8 = now();
        intersectWithDeltaReachable_(region, dr);
        auto t9 = now();

        auto mergeAnchors = computeMergeableAnchors_(region, B);
        auto t10 = now();

        // Save last state for toDot()
        last_region_     = region;
        last_boundaries_ = B;
        dr_              = dr;
        last_analysis_.region = region;
        last_analysis_.boundaries = B;
        last_analysis_.mergeableAnchorsByHead = std::move(mergeAnchors);
        have_last_       = true;

        // Count stats
        Stats stats;
        stats.dr_nodes     = dr.nodes.size();
        stats.dr_edges     = dr.edges.size();
        stats.region_nodes = region.nodes.size();
        stats.region_edges = region.edges.size();
        for (const auto& n : region.nodes) if (!n->isFact) stats.optimized_recomputed++;
        for (const auto& n : dr.nodes)     if (!n->isFact) stats.naive_recomputed++;

        // Emit
        std::cout << "[inc-analyze] timing(ms): "
                  << "leastParents=" << toMs(t1 - t0)
                  << " scopes=" << toMs(t2 - t1)
                  << " reach=" << toMs(t3 - t2)
                  << " initRegion=" << toMs(t4 - t3)
                  << " classify=" << toMs(t5 - t4)
                  << " expand=" << toMs(t6 - t5)
                  << " upstreamClose=" << upstream_ms
                  << " reclassify=" << reclass_ms
                  << " reexpand=" << reexpand_ms
                  << " deltaReach=" << toMs(t8 - t7)
                  << " intersect=" << toMs(t9 - t8)
                  << " mergeable=" << toMs(t10 - t9)
                  << " total=" << toMs(t10 - t0)
                  << "\n";
        emitConsole_(stats, region);
        if (!json_out_path.empty()) emitJSON_(stats, region, json_out_path);
        if (!csv_out_path.empty())  emitCSV_(stats, region, csv_out_path);

        return stats;
    }

    // toDot with visual marks: region nodes (double border), boundary nodes (dashed, orange),
    // mergeable incoming edges to boundary heads (red), delta items (green).
    bool toDot(const std::string& path, bool mark_merge = true) {
        if (!have_last_) return false;
        std::ofstream out(path);
        if (!out) return false;
        out << "digraph G {\n";
        out << "  rankdir=LR;\n";

        auto inRegion = [&](const NodePtr& n){ return last_region_.nodes.count(n) > 0; };
        auto isOutB   = [&](const NodePtr& n){ return last_boundaries_.out_induced.count(n)   > 0; };
        auto isScopeB = [&](const NodePtr& n){ return last_boundaries_.scope_induced.count(n)> 0; };
        auto isResB   = [&](const NodePtr& n){ return last_boundaries_.residual.count(n)      > 0; };
        auto isDeltaN = [&](const NodePtr& n){
            return isIn_<NodePtr>(n, view_.getDeltaInsertNodes())
                || isIn_<NodePtr>(n, view_.getDeltaDeleteNodes());
        };
        auto isDeltaE = [&](const EdgePtr& e){
            return isIn_<EdgePtr>(e, view_.getDeltaInsertEdges())
                || isIn_<EdgePtr>(e, view_.getDeltaDeleteEdges());
        };

        // Nodes
        for (auto& n : view_.getValidNodes()) {
            // base vs derived
            std::string shape = n->isFact ? "box" : "ellipse";
            std::string pen   = inRegion(n) ? "2"  : "1";
            std::string color = inRegion(n) ? "#1f77b4" : "black"; // region nodes: blue border
            if (isOutB(n) || isScopeB(n) || isResB(n)) {
                color = "#ff7f0e"; // orange for boundary nodes
                pen   = "2";
                shape = n->isFact ? "box" : "ellipse";
            }
            if (isDeltaN(n)) {
                color = "#2ca02c"; // green for delta
                pen   = "2";
            }
            out << "  \"" << node_id(n) << "\""
                << " [shape=" << shape << ", penwidth=" << pen << ", color=\"" << color << "\"];\n";
        }

        // Hyperedges are rendered with intermediate edge nodes, similar to DerivationGraph::dumpDot.
        for (auto& e : view_.getValidEdges()) {
            auto head  = view_.getOutput(e);
            auto ins   = view_.getInputs(e);
            bool inRegEdge = last_region_.edges.count(e) > 0;
            bool onBHead   = isOutB(head) || isScopeB(head) || isResB(head);
            bool delta     = isDeltaE(e);
            bool canMerge  = mark_merge && edgeMergeable_(e);
            std::ostringstream edgeNodeName;
            edgeNodeName << "edge_node_" << reinterpret_cast<uintptr_t>(e.get());
            std::string color = "black";
            std::string pen   = "1";
            if (inRegEdge) { color = "#1f77b4"; pen = "2"; }          // in-region edges: blue
            if (onBHead && canMerge) { color = "red"; pen = "2"; }    // mergeable into boundary: red
            if (delta) { color = "#2ca02c"; pen = "2"; }              // delta edges: green
            out << "  " << quote_(edgeNodeName.str())
                << " [shape=point, width=0.2, height=0.2, label=\"\", style=filled, color=\"" << color
                << "\", fillcolor=\"" << color << "\", penwidth=" << pen << "];\n";
            for (auto& t : ins) {
                out << "  \"" << node_id(t) << "\" -> " << quote_(edgeNodeName.str())
                    << " [color=\"" << color << "\", penwidth=" << pen << ", style=solid];\n";
            }
            out << "  " << quote_(edgeNodeName.str()) << " -> \"" << node_id(head) << "\""
                << " [color=\"" << color << "\", penwidth=" << pen << ", style=solid];\n";
        }

        emitLegend_(out, mark_merge);
        out << "}\n";
        return true;
    }

    // Accessors for last run
    const Region&     lastRegion()    const { return last_region_; }
    const Boundaries& lastBoundaries()const { return last_boundaries_; }
    const Region&     lastDeltaReachable() const { return dr_; }
    const IncRegionAnalysis& getLastAnalysis() const { return last_analysis_; }
    const std::vector<EdgePtr>& getMergeableAnchors(NodePtr v) const {
        static const std::vector<EdgePtr> kEmpty;
        auto it = last_analysis_.mergeableAnchorsByHead.find(v);
        return it == last_analysis_.mergeableAnchorsByHead.end() ? kEmpty : it->second;
    }

private:
    // ---- helpers ----

    template <class T, class SetT>
    static bool isIn_(const T& x, const SetT& s) {
        return s.find(x) != s.end();
    }

    static bool isBase_(const NodePtr& n) { return n->isFact; }

    void prepareGraphStructures_() {
        incoming_edges_map_.clear();
        outgoing_edges_map_.clear();
        preds_.clear();
        succs_.clear();
        for (auto& n : view_.getValidNodes()) {
            incoming_edges_map_[n];
            outgoing_edges_map_[n];
            preds_[n];
            succs_[n];
        }
        for (auto& e : view_.getValidEdges()) {
            auto head = view_.getOutput(e);
            incoming_edges_map_[head].push_back(e);
            auto ins = view_.getInputs(e);
            for (auto& t : ins) {
                outgoing_edges_map_[t].push_back(e);
                preds_[head].insert(t);
                succs_[t].insert(head);
            }
        }
        delta_insert_edges_cache_.clear();
        for (auto& e : view_.getDeltaInsertEdges()) {
            delta_insert_edges_cache_.insert(e);
        }
    }

    // Build Least-Parents (LP) sets for nodes following the dominance-based definition.
    void buildLeastParents_() {
        using Clock = std::chrono::steady_clock;
        auto toMs = [](auto dur) { return std::chrono::duration<double, std::milli>(dur).count(); };
        auto t_total_start = Clock::now();
        auto t0 = Clock::now();
        prepareGraphStructures_();
        double t_prepare = toMs(Clock::now() - t0);
        lp_set_.clear();
        reachable_cache_.clear();
        t0 = Clock::now();
        auto& scc = view_.getCycleDependencyGraph();
        double t_scc = toMs(Clock::now() - t0);
        double t_scc_reach = 0.0;
        double t_scc_nodes = 0.0;
        double t_reachable = 0.0;
        double t_branch = 0.0;
        double t_dominators = 0.0;
        double t_select = 0.0;
        size_t sources_total = 0;
        size_t sources_used = 0;
        size_t sources_skipped = 0;
        size_t branch_edges = 0;
        size_t branch_nodes = 0;
        size_t reachable_nodes = 0;
        std::unordered_map<size_t, std::unordered_set<size_t>> scc_reach;
        std::unordered_map<size_t, std::set<NodePtr>> scc_reach_nodes;
        std::function<const std::unordered_set<size_t>&(size_t)> getReachSccs =
                [&](size_t cid) -> const std::unordered_set<size_t>& {
            auto it = scc_reach.find(cid);
            if (it != scc_reach.end()) return it->second;
            auto ts = Clock::now();
            std::unordered_set<size_t> reach{cid};
            if (cid < scc.reverseDependencies.size()) {
                for (auto succ : scc.reverseDependencies[cid]) {
                    const auto& succReach = getReachSccs(succ);
                    reach.insert(succReach.begin(), succReach.end());
                }
            }
            t_scc_reach += toMs(Clock::now() - ts);
            return scc_reach.emplace(cid, std::move(reach)).first->second;
        };
        auto getReachNodes = [&](size_t cid) -> const std::set<NodePtr>& {
            auto it = scc_reach_nodes.find(cid);
            if (it != scc_reach_nodes.end()) return it->second;
            auto ts = Clock::now();
            std::set<NodePtr> nodes;
            const auto& reachSccs = getReachSccs(cid);
            for (auto rid : reachSccs) {
                if (rid >= scc.nodeCycles.size()) continue;
                const auto& group = scc.nodeCycles[rid];
                nodes.insert(group.begin(), group.end());
            }
            t_scc_nodes += toMs(Clock::now() - ts);
            return scc_reach_nodes.emplace(cid, std::move(nodes)).first->second;
        };
        for (auto& source : view_.getValidNodes()) {
            sources_total++;
            auto outIt = outgoing_edges_map_.find(source);
            if (outIt == outgoing_edges_map_.end() || outIt->second.size() < 2) {
                lp_set_[source] = {};
                sources_skipped++;
                continue;
            }
            sources_used++;
            auto tr = Clock::now();
            auto cit = scc.nodeToCycleIndex.find(source);
            if (cit != scc.nodeToCycleIndex.end()) {
                const auto& reachable = getReachNodes(cit->second);
                reachable_cache_[source] = reachable;
            } else {
                reachable_cache_[source] = forwardReachable_(source);
            }
            const auto& reachable = reachable_cache_[source];
            t_reachable += toMs(Clock::now() - tr);
            reachable_nodes += reachable.size();
            std::unordered_map<NodePtr, size_t> branch_counts;
            for (auto& edge : outIt->second) {
                if (!edge) continue;
                auto child = view_.getOutput(edge);
                if (!child) continue;
                auto tb = Clock::now();
                auto branchReach = forwardReachableFromBranch_(child, source);
                t_branch += toMs(Clock::now() - tb);
                branch_edges++;
                branch_nodes += branchReach.size();
                for (auto& node : branchReach) {
                    if (node.get() == source.get()) continue;
                    branch_counts[node]++;
                }
            }
            std::set<NodePtr> merge_nodes;
            for (auto& [node, count] : branch_counts) {
                if (count >= 2 && reachable.count(node)) {
                    merge_nodes.insert(node);
                }
            }
            if (merge_nodes.empty()) {
                lp_set_[source] = {};
                continue;
            }
            auto td = Clock::now();
            auto dom = computeDominators_(source, reachable);
            t_dominators += toMs(Clock::now() - td);
            auto ts = Clock::now();
            std::set<NodePtr> least;
            for (auto& m : merge_nodes) {
                bool dominated = false;
                for (auto& other : merge_nodes) {
                    if (m.get() == other.get()) continue;
                    auto dit = dom.find(m);
                    if (dit != dom.end() && dit->second.count(other)) {
                        dominated = true;
                        break;
                    }
                }
                if (!dominated) least.insert(m);
            }
            t_select += toMs(Clock::now() - ts);
            lp_set_[source] = std::move(least);
        }
        double t_total = toMs(Clock::now() - t_total_start);
        std::cout << "[least-parents] timing(ms): prepare=" << t_prepare
                  << " scc=" << t_scc
                  << " sccReach=" << t_scc_reach
                  << " reachNodes=" << t_scc_nodes
                  << " reachable=" << t_reachable
                  << " branch=" << t_branch
                  << " dominators=" << t_dominators
                  << " select=" << t_select
                  << " total=" << t_total
                  << " sources=" << sources_total
                  << " used=" << sources_used
                  << " skipped=" << sources_skipped
                  << " branches=" << branch_edges
                  << " branchNodes=" << branch_nodes
                  << " reachNodes=" << reachable_nodes
                  << "\n";
    }

    void computeScopes_() {
        node_scope_.clear();
        edge_scope_by_source_.clear();
        edge_scope_index_.clear();
        for (auto& n : view_.getValidNodes()) {
            std::set<NodePtr> scope_nodes;
            std::set<EdgePtr> scope_edges;
            auto lp_it = lp_set_.find(n);
            auto reach_it = reachable_cache_.find(n);
            if (lp_it != lp_set_.end() && reach_it != reachable_cache_.end()) {
                for (auto& m : lp_it->second) {
                    collectScopeFrom_(m, reach_it->second, scope_nodes, scope_edges);
                }
            }
            node_scope_[n] = scope_nodes;
            edge_scope_by_source_[n] = scope_edges;
            for (auto& e : scope_edges) {
                edge_scope_index_[e].insert(n);
            }
        }
    }

    const std::set<NodePtr>& edgeScope_(const EdgePtr& e, bool ignore_insert_edges=false) {
        static const std::set<NodePtr> kEmpty;
        auto it = edge_scope_index_.find(e);
        return it == edge_scope_index_.end() ? kEmpty : it->second;
    }

    std::set<NodePtr> forwardReachable_(const NodePtr& source) {
        std::set<NodePtr> reachable;
        if (!source) return reachable;
        std::vector<NodePtr> stack;
        stack.push_back(source);
        while (!stack.empty()) {
            auto cur = stack.back();
            stack.pop_back();
            if (!reachable.insert(cur).second) continue;
            auto sit = succs_.find(cur);
            if (sit == succs_.end()) continue;
            for (auto& next : sit->second) {
                stack.push_back(next);
            }
        }
        return reachable;
    }

    std::unordered_map<NodePtr, std::set<NodePtr>> computeDominators_(const NodePtr& source,
            const std::set<NodePtr>& reachable) {
        std::unordered_map<NodePtr, std::set<NodePtr>> dom;
        if (!source) return dom;
        for (auto& node : reachable) {
            if (node.get() == source.get()) {
                dom[node] = { source };
            } else {
                dom[node] = reachable;
            }
        }
        bool changed = true;
        while (changed) {
            changed = false;
            for (auto& node : reachable) {
                if (node.get() == source.get()) continue;
                std::set<NodePtr> intersection;
                bool first = true;
                bool has_pred = false;
                auto pit = preds_.find(node);
                if (pit != preds_.end()) {
                    for (auto& pred : pit->second) {
                        if (!reachable.count(pred)) continue;
                        has_pred = true;
                        if (first) {
                            intersection = dom[pred];
                            first = false;
                        } else {
                            std::set<NodePtr> temp;
                            std::set_intersection(intersection.begin(), intersection.end(),
                                dom[pred].begin(), dom[pred].end(), std::inserter(temp, temp.begin()));
                            intersection.swap(temp);
                        }
                    }
                }
                std::set<NodePtr> new_dom;
                new_dom.insert(node);
                if (has_pred) {
                    new_dom.insert(intersection.begin(), intersection.end());
                }
                if (dom[node] != new_dom) {
                    dom[node] = std::move(new_dom);
                    changed = true;
                }
            }
        }
        return dom;
    }

    void collectScopeFrom_(const NodePtr& target, const std::set<NodePtr>& reachable,
            std::set<NodePtr>& scope_nodes, std::set<EdgePtr>& scope_edges) {
        if (!target) return;
        std::vector<NodePtr> stack;
        std::unordered_set<NodePtr> visited;
        stack.push_back(target);
        visited.insert(target);
        while (!stack.empty()) {
            auto cur = stack.back();
            stack.pop_back();
            if (!reachable.count(cur)) continue;
            scope_nodes.insert(cur);
            auto it = incoming_edges_map_.find(cur);
            if (it == incoming_edges_map_.end()) continue;
            for (auto& e : it->second) {
                auto ins = view_.getInputs(e);
                bool all_reachable = true;
                for (auto& tail : ins) {
                    if (!reachable.count(tail)) {
                        all_reachable = false;
                        break;
                    }
                }
                if (!all_reachable) continue;
                scope_edges.insert(e);
                for (auto& tail : ins) {
                    if (visited.insert(tail).second) {
                        stack.push_back(tail);
                    }
                }
            }
        }
    }

    struct ReachInfo {
        std::unordered_set<NodePtr> nodes;
        std::unordered_set<EdgePtr> edges;
    };

    ReachInfo reachFromCache_(const std::set<NodePtr>& sources) {
        ReachInfo info;
        const auto& cacheNodes = view_.getDeltaInsertReachableNodes();
        const auto& cacheEdges = view_.getDeltaInsertReachableEdges();
        if (!cacheNodes.empty() || !cacheEdges.empty()) {
            info.nodes.insert(cacheNodes.begin(), cacheNodes.end());
            info.edges.insert(cacheEdges.begin(), cacheEdges.end());
            return info;
        }
        std::vector<NodePtr> srcVec;
        srcVec.reserve(sources.size());
        for (const auto& n : sources) {
            srcVec.push_back(n);
        }
        Region dr = deltaReachable_(srcVec);
        info.nodes.insert(dr.nodes.begin(), dr.nodes.end());
        info.edges.insert(dr.edges.begin(), dr.edges.end());
        return info;
    }

    std::set<NodePtr> forwardReachableFromBranch_(const NodePtr& start, const NodePtr& source) {
        std::set<NodePtr> reachable;
        if (!start) return reachable;
        std::vector<NodePtr> stack;
        stack.push_back(start);
        while (!stack.empty()) {
            auto cur = stack.back();
            stack.pop_back();
            if (cur.get() == source.get()) continue;
            if (!reachable.insert(cur).second) continue;
            auto sit = succs_.find(cur);
            if (sit == succs_.end()) continue;
            for (auto& next : sit->second) {
                stack.push_back(next);
            }
        }
        return reachable;
    }

    bool mergeableEdgeAtHead_(const NodePtr& head, const EdgePtr& edge) {
        if (!head || !edge) return false;
        if (edge->isDeterministic()) return false;
        if (delta_insert_edges_cache_.count(edge)) return false;
        if (!edgeRespectsScopes_(head, edge)) return false;
        if (!edgeNonSubsumed_(head, edge)) return false;
        return true;
    }

    bool edgeRespectsScopes_(const NodePtr& head, const EdgePtr& edge) {
        auto it = edge_scope_index_.find(edge);
        if (it == edge_scope_index_.end()) return true;
        for (auto& source : it->second) {
            auto lp_it = lp_set_.find(source);
            if (lp_it == lp_set_.end()) return false;
            if (!lp_it->second.count(head)) return false;
        }
        return true;
    }

    bool edgeNonSubsumed_(const NodePtr& head, const EdgePtr& edge) {
        auto inputs = view_.getInputs(edge);
        std::set<NodePtr> candidate(inputs.begin(), inputs.end());
        auto inEdges = view_.getIncomingEdges(head);
        for (auto& other : inEdges) {
            if (other.get() == edge.get()) continue;
            auto otherInputs = view_.getInputs(other);
            std::set<NodePtr> otherSet(otherInputs.begin(), otherInputs.end());
            if (otherSet.empty()) continue;
            if (std::includes(candidate.begin(), candidate.end(), otherSet.begin(), otherSet.end())) {
                return false;
            }
        }
        return true;
    }

    // ---- region building ----

    Region initialRegion_(const std::vector<NodePtr>& delta_input_facts) {
        Region R;
        std::set<NodePtr> delta_nodes = view_.getDeltaInsertNodes();
        std::set<EdgePtr> delta_edges = view_.getDeltaInsertEdges();
        for (auto& n : delta_input_facts) delta_nodes.insert(n);

        std::unordered_set<NodePtr> out_nodes;
        for (auto& e : delta_edges) {
            out_nodes.insert(view_.getOutput(e));
        }

        std::unordered_set<NodePtr> scope_nodes_union;
        std::unordered_set<EdgePtr> scope_edges_union;
        for (auto& x : delta_nodes) {
            auto nit = node_scope_.find(x);
            if (nit != node_scope_.end()) {
                scope_nodes_union.insert(nit->second.begin(), nit->second.end());
            }
            auto eit = edge_scope_by_source_.find(x);
            if (eit != edge_scope_by_source_.end()) {
                scope_edges_union.insert(eit->second.begin(), eit->second.end());
            }
        }

        std::unordered_set<NodePtr> candidate_nodes(delta_nodes.begin(), delta_nodes.end());
        candidate_nodes.insert(out_nodes.begin(), out_nodes.end());
        candidate_nodes.insert(scope_nodes_union.begin(), scope_nodes_union.end());

        std::unordered_set<EdgePtr> candidate_edges(delta_edges.begin(), delta_edges.end());
        candidate_edges.insert(scope_edges_union.begin(), scope_edges_union.end());

        for (auto& n : candidate_nodes) {
            if (reach_filter_.nodes.count(n)) {
                R.nodes.insert(n);
            }
        }
        for (auto& e : candidate_edges) {
            if (reach_filter_.edges.count(e)) {
                R.edges.insert(e);
            }
        }
        return R;
    }

    Boundaries classifyBoundaries_(const Region& R) {
        Boundaries B;
        std::unordered_set<NodePtr> boundary_nodes;
        for (auto& e : view_.getValidEdges()) {
            auto head = view_.getOutput(e);
            auto ins = view_.getInputs(e);
            if (R.nodes.count(head)) continue;
            for (auto& tail : ins) {
                if (R.nodes.count(tail)) {
                    boundary_nodes.insert(tail);
                }
            }
        }

        std::unordered_set<NodePtr> out_nodes;
        for (auto& e : last_delta_edges_) {
            out_nodes.insert(view_.getOutput(e));
        }

        std::unordered_set<NodePtr> lp_union;
        for (auto& x : last_delta_nodes_) {
            auto it = lp_set_.find(x);
            if (it != lp_set_.end()) {
                lp_union.insert(it->second.begin(), it->second.end());
            }
        }

        for (auto& n : boundary_nodes) {
            if (out_nodes.count(n)) {
                B.out_induced.insert(n);
            } else if (lp_union.count(n)) {
                B.scope_induced.insert(n);
            } else {
                B.residual.insert(n);
            }
        }
        return B;
    }

    bool edgeMergeable_(const EdgePtr& e) {
        auto head = view_.getOutput(e);
        return mergeableEdgeAtHead_(head, e);
    }

    bool mergeableHead_(const NodePtr& h, const Region& R) {
        if (!h || !R.nodes.count(h)) return false;
        auto inEs = view_.getIncomingEdges(h);
        for (auto& e : inEs) {
            if (mergeableEdgeAtHead_(h, e)) {
                return true;
            }
        }
        return false;
    }

    std::unordered_map<NodePtr, std::vector<EdgePtr>> computeMergeableAnchors_(const Region& R, const Boundaries& B) {
        std::unordered_map<NodePtr, std::vector<EdgePtr>> anchors;
        std::set<NodePtr> boundary_nodes;
        boundary_nodes.insert(B.out_induced.begin(), B.out_induced.end());
        boundary_nodes.insert(B.scope_induced.begin(), B.scope_induced.end());
        boundary_nodes.insert(B.residual.begin(), B.residual.end());
        for (auto& head : boundary_nodes) {
            auto inEs = view_.getIncomingEdges(head);
            for (auto& e : inEs) {
                if (mergeableEdgeAtHead_(head, e)) {
                    anchors[head].push_back(e);
                }
            }
        }
        return anchors;
    }

    // Upstream closure: include ancestors (limited by reach_filter_) feeding any region node.
    bool upstreamClose_(Region& R) {
        bool changed = false;
        std::queue<NodePtr> q;
        for (auto n : R.nodes) q.push(n);
        std::unordered_set<NodePtr> seen(R.nodes.begin(), R.nodes.end());
        auto inFilterNode = [&](NodePtr n) {
            return reach_filter_.nodes.empty() || reach_filter_.nodes.count(n);
        };
        auto inFilterEdge = [&](EdgePtr e) {
            return reach_filter_.edges.empty() || reach_filter_.edges.count(e);
        };
        while (!q.empty()) {
            NodePtr cur = q.front();
            q.pop();
            for (auto e : view_.getIncomingEdges(cur)) {
                if (!inFilterEdge(e)) continue;
                if (R.edges.insert(e).second) changed = true;
                for (auto in : view_.getInputs(e)) {
                    if (!inFilterNode(in)) continue;
                    if (seen.insert(in).second) {
                        R.nodes.insert(in);
                        changed = true;
                        q.push(in);
                    }
                }
            }
        }
        return changed;
    }

    void expandToFixpoint_(Region& R, Boundaries& B) {
        int guard = 0;
        while (guard++ < 10000) {
            auto boundary_nodes = B.out_induced;
            boundary_nodes.insert(B.scope_induced.begin(), B.scope_induced.end());
            boundary_nodes.insert(B.residual.begin(), B.residual.end());
            std::cout << "[region] expand iter " << guard
                      << " |R_nodes|=" << R.nodes.size()
                      << " |R_edges|=" << R.edges.size()
                      << " |boundary|=" << boundary_nodes.size()
                      << std::endl;
            if (boundary_nodes.empty()) {
                std::cout << "[region] boundary empty, stopping expansion\n";
                break;
            }
            std::vector<NodePtr> blocking;
            for (auto& n : boundary_nodes) {
                if (!mergeableHead_(n, R)) {
                    blocking.push_back(n);
                }
            }
            std::cout << "[region] blocking boundary count=" << blocking.size() << std::endl;
            if (blocking.empty()) {
                std::cout << "[region] all boundary nodes mergeable, stopping expansion\n";
                break;
            }
            bool extended = false;
            for (auto& n : blocking) {
                auto sit = node_scope_.find(n);
                if (sit != node_scope_.end()) {
                    for (auto& sn : sit->second) {
                        if (reach_filter_.nodes.count(sn) && R.nodes.insert(sn).second) {
                            extended = true;
                        }
                    }
                }
                auto eit = edge_scope_by_source_.find(n);
                if (eit != edge_scope_by_source_.end()) {
                    for (auto& e : eit->second) {
                        if (reach_filter_.edges.count(e) && R.edges.insert(e).second) {
                            extended = true;
                        }
                    }
                }
            }
            if (!extended) {
                std::cout << "[region] scopes added no new items, stopping expansion\n";
                break;
            }
            B = classifyBoundaries_(R);
        }
        if (guard >= 10000) {
            std::cout << "[region] expand reached iteration guard limit" << std::endl;
        }
        std::cout << "[region] final region nodes=" << R.nodes.size()
                  << " edges=" << R.edges.size()
                  << " boundaries(out=" << B.out_induced.size()
                  << ", scope=" << B.scope_induced.size()
                  << ", residual=" << B.residual.size() << ")\n";
    }

    // Compute delta-reachable region using impacted maps rebuilt after pruning.
    // This is insertion-only and reuses the precomputed reachability for delta inserts.
    Region deltaReachable_(const std::vector<NodePtr>& /*delta_input_facts*/) {
        Region DR;
        const auto& reachNodes = view_.getDeltaInsertReachableNodes();
        const auto& reachEdges = view_.getDeltaInsertReachableEdges();
        if (!reachNodes.empty() || !reachEdges.empty()) {
            DR.nodes.insert(reachNodes.begin(), reachNodes.end());
            DR.edges.insert(reachEdges.begin(), reachEdges.end());
        } else {
            const auto& nodeImpacted = view_.getNodeImpactedByDeltaInsert();
            const auto& edgeImpacted = view_.getEdgeImpactedByDeltaInsert();
            if (!nodeImpacted.empty() || !edgeImpacted.empty()) {
                for (const auto& kv : nodeImpacted) {
                    DR.nodes.insert(kv.first);
                    DR.nodes.insert(kv.second.begin(), kv.second.end());
                }
                for (const auto& kv : edgeImpacted) {
                    DR.edges.insert(kv.second.begin(), kv.second.end());
                }
            } else {
                const auto& liveNodes = view_.getNodes();
                const auto& liveEdges = view_.getEdges();
                std::queue<NodePtr> q;
                auto seed = [&](const NodePtr& src) {
                    if (!src || !liveNodes.count(src)) {
                        return;
                    }
                    if (DR.nodes.insert(src).second) {
                        q.push(src);
                    }
                };
                for (const auto& n : view_.getDeltaInsertNodes()) {
                    seed(n);
                }
                for (const auto& e : view_.getDeltaInsertEdges()) {
                    if (auto h = view_.getOutput(e)) {
                        seed(h);
                    }
                }
                while (!q.empty()) {
                    NodePtr cur = q.front();
                    q.pop();
                    for (const auto& e : cur->getOutgoingEdges()) {
                        if (!liveEdges.count(e)) {
                            continue;
                        }
                        DR.edges.insert(e);
                        NodePtr nxt = e->getOutput();
                        if (nxt && liveNodes.count(nxt) && DR.nodes.insert(nxt).second) {
                            q.push(nxt);
                        }
                    }
                }
            }
        }
        for (const auto& n : view_.getDeltaInsertNodes()) {
            DR.nodes.insert(n);
        }
        for (const auto& e : view_.getDeltaInsertEdges()) {
            DR.edges.insert(e);
            if (auto h = view_.getOutput(e)) {
                DR.nodes.insert(h);
            }
        }
        return DR;
    }

    void intersectWithDeltaReachable_(Region& R, const Region& DR) {
        // filter nodes
        for (auto it = R.nodes.begin(); it != R.nodes.end(); ) {
            if (!DR.nodes.count(*it)) it = R.nodes.erase(it);
            else ++it;
        }
        for (auto it = R.edges.begin(); it != R.edges.end(); ) {
            if (!DR.edges.count(*it)) it = R.edges.erase(it);
            else ++it;
        }
    }

    // ---- emissions ----

    void emitConsole_(const Stats& s, const Region& R) {
        std::cout << "=== Incremental Region Analysis ===\n";
        std::cout << "Region nodes: " << s.region_nodes << " / DR nodes: " << s.dr_nodes << "\n";
        std::cout << "Region edges: " << s.region_edges << " / DR edges: " << s.dr_edges << "\n";
        std::cout << "Recompute (optimized): " << s.optimized_recomputed
                  << " ; (naive): " << s.naive_recomputed
                  << " ; ratio: " << std::fixed << std::setprecision(3) << s.ratio()
                  << " ; diff: " << s.diff() << "\n";

        // Per-node quick print
        for (auto& n : view_.getValidNodes()) {
            bool inR   = R.nodes.count(n);
            bool delta = view_.getDeltaInsertNodes().count(n) || view_.getDeltaDeleteNodes().count(n);
            std::cout << "node,\"" << node_id(n) << "\"," << (n->isFact?1:0) << "," << (inR?1:0) << "," << (delta?1:0)
                      << "," << 0 /*mergeable as node*/ << ",\"" << scopeStr_(node_scope_[n]) << "\"\n";
        }
        for (auto& e : view_.getValidEdges()) {
            bool inR   = R.edges.count(e);
            bool delta = view_.getDeltaInsertEdges().count(e) || view_.getDeltaDeleteEdges().count(e);
            std::cout << "edge,\"" << edge_id(e, view_) << "\"," << 0 << "," << (inR?1:0) << "," << (delta?1:0)
                      << "," << (edgeMergeable_(e)?1:0) << ",\"" << scopeStr_(edgeScope_(e)) << "\"\n";
        }
    }

    void emitJSON_(const Stats& s, const Region& R, const std::string& path) {
        std::ofstream out(path);
        if (!out) return;
        out << "{\n";
        out << "  \"stats\": {\"region_nodes\": " << s.region_nodes << ", \"dr_nodes\": " << s.dr_nodes
            << ", \"region_edges\": " << s.region_edges << ", \"dr_edges\": " << s.dr_edges
            << ", \"optimized\": " << s.optimized_recomputed << ", \"naive\": " << s.naive_recomputed
            << ", \"ratio\": " << std::fixed << std::setprecision(6) << s.ratio()
            << ", \"diff\": " << s.diff() << "},\n";
        out << "  \"nodes\": [\n";
        bool first = true;
        for (auto& n : view_.getValidNodes()) {
            if (!first) out << ",\n";
            first = false;
            bool inR   = R.nodes.count(n);
            bool delta = view_.getDeltaInsertNodes().count(n) || view_.getDeltaDeleteNodes().count(n);
            out << "    {\"id\": " << quote_(node_id(n)) << ", \"is_fact\": " << (n->isFact?1:0)
                << ", \"in_region\": " << (inR?1:0) << ", \"is_delta\": " << (delta?1:0)
                << ", \"scope\": " << quote_(scopeStr_(node_scope_[n])) << "}";
        }
        out << "\n  ],\n  \"edges\": [\n";
        first = true;
        for (auto& e : view_.getValidEdges()) {
            if (!first) out << ",\n";
            first = false;
            bool inR   = R.edges.count(e);
            bool delta = view_.getDeltaInsertEdges().count(e) || view_.getDeltaDeleteEdges().count(e);
            out << "    {\"id\": " << quote_(edge_id(e, view_)) << ", \"in_region\": " << (inR?1:0)
                << ", \"is_delta\": " << (delta?1:0) << ", \"mergeable\": " << (edgeMergeable_(e)?1:0)
                << ", \"scope\": " << quote_(scopeStr_(edgeScope_(e))) << "}";
        }
        out << "\n  ]\n}\n";
    }

    void emitCSV_(const Stats& s, const Region& R, const std::string& path) {
        std::ofstream out(path);
        if (!out) return;
        out << "type,id,is_fact,in_region,is_delta,mergeable,scope\n";
        for (auto& n : view_.getValidNodes()) {
            bool inR   = R.nodes.count(n);
            bool delta = view_.getDeltaInsertNodes().count(n) || view_.getDeltaDeleteNodes().count(n);
            out << "node," << quote_(node_id(n)) << "," << (n->isFact?1:0) << "," << (inR?1:0) << "," << (delta?1:0)
                << ",0," << quote_(scopeStr_(node_scope_[n])) << "\n";
        }
        for (auto& e : view_.getValidEdges()) {
            bool inR   = R.edges.count(e);
            bool delta = view_.getDeltaInsertEdges().count(e) || view_.getDeltaDeleteEdges().count(e);
            out << "edge," << quote_(edge_id(e, view_)) << ",0," << (inR?1:0) << "," << (delta?1:0)
                << "," << (edgeMergeable_(e)?1:0) << "," << quote_(scopeStr_(edgeScope_(e))) << "\n";
        }
    }

    void emitLegend_(std::ofstream& out, bool includeMerge) const {
        out << "  subgraph cluster_legend {\n";
        out << "    label=\"Legend\";\n";
        out << "    color=gray;\n";
        out << "    legend_fact [shape=box, label=\"Fact node\"];\n";
        out << "    legend_derived [shape=ellipse, label=\"Derived node\"];\n";
        out << "    legend_region [shape=ellipse, penwidth=2, color=\"#1f77b4\", label=\"Region member\"];\n";
        out << "    legend_boundary [shape=ellipse, penwidth=2, color=\"#ff7f0e\", label=\"Boundary head\"];\n";
        out << "    legend_delta_node [shape=ellipse, penwidth=2, color=\"#2ca02c\", label=\"Delta node\"];\n";
        out << "    legend_src_region [shape=box, label=\"Sample fact\"];\n";
        out << "    legend_dst_region [shape=ellipse, label=\"Sample head\"];\n";
        out << "    legend_hyper_region [shape=point, width=0.2, height=0.2, label=\"\", style=filled, color=\"#1f77b4\", fillcolor=\"#1f77b4\"];\n";
        out << "    legend_src_region -> legend_hyper_region [color=\"#1f77b4\", penwidth=2, label=\"region hyperedge\"];\n";
        out << "    legend_hyper_region -> legend_dst_region [color=\"#1f77b4\", penwidth=2];\n";
        out << "    legend_src_delta [shape=box, label=\"Sample fact\"];\n";
        out << "    legend_dst_delta [shape=ellipse, label=\"Sample head\"];\n";
        out << "    legend_hyper_delta [shape=point, width=0.2, height=0.2, label=\"\", style=filled, color=\"#2ca02c\", fillcolor=\"#2ca02c\"];\n";
        out << "    legend_src_delta -> legend_hyper_delta [color=\"#2ca02c\", penwidth=2, label=\"delta hyperedge\"];\n";
        out << "    legend_hyper_delta -> legend_dst_delta [color=\"#2ca02c\", penwidth=2];\n";
        if (includeMerge) {
            out << "    legend_src_merge [shape=box, label=\"Sample fact\"];\n";
            out << "    legend_dst_merge [shape=ellipse, label=\"Sample head\"];\n";
            out << "    legend_hyper_merge [shape=point, width=0.2, height=0.2, label=\"\", style=filled, color=\"red\", fillcolor=\"red\"];\n";
            out << "    legend_src_merge -> legend_hyper_merge [color=\"red\", penwidth=2, label=\"mergeable hyperedge\"];\n";
            out << "    legend_hyper_merge -> legend_dst_merge [color=\"red\", penwidth=2];\n";
        }
        out << "  }\n";
    }

    static std::string scopeStr_(const std::set<NodePtr>& ss) {
        std::ostringstream oss;
        oss << "{";
        bool first = true;
        for (auto& n : ss) {
            if (!first) oss << "; ";
            first = false;
            oss << node_id(n);
        }
        oss << "}";
        return oss.str();
    }

    static std::string quote_(const std::string& s) {
        std::ostringstream oss;
        oss << "\"";
        for (char c : s) {
            if (c=='"') oss << "\\\"";
            else if (c=='\\') oss << "\\\\";
            else oss << c;
        }
        oss << "\"";
        return oss.str();
    }

private:
    DGView& view_;

    // Derived artifacts
    std::unordered_map<NodePtr, std::set<NodePtr>> lp_set_;     // least-parents set (approx)
    std::unordered_map<NodePtr, std::set<NodePtr>> node_scope_; // node -> scope (base facts)

    // Scope caches keyed by source node
    std::unordered_map<NodePtr, std::set<EdgePtr>> edge_scope_by_source_;
    std::unordered_map<EdgePtr, std::set<NodePtr>> edge_scope_index_;
    std::unordered_map<NodePtr, std::set<NodePtr>> reachable_cache_;

    // Structural helpers
    std::unordered_map<NodePtr, std::vector<EdgePtr>> incoming_edges_map_;
    std::unordered_map<NodePtr, std::vector<EdgePtr>> outgoing_edges_map_;
    std::unordered_map<NodePtr, std::set<NodePtr>> preds_;
    std::unordered_map<NodePtr, std::set<NodePtr>> succs_;
    std::unordered_set<EdgePtr> delta_insert_edges_cache_;

    // Last run
    Region     last_region_;
    Boundaries last_boundaries_;
    Region     dr_;
    IncRegionAnalysis last_analysis_;
    bool       have_last_ = false;
    std::set<NodePtr> last_delta_nodes_;
    std::set<EdgePtr> last_delta_edges_;
    std::set<NodePtr> last_delta_inputs_;
    std::set<NodePtr> current_delta_sources_;
    ReachInfo reach_filter_;

    void debugPrintLeastParents_() {
        std::cout << "[lp] least parents per node:\n";
        for (auto& [node, parents] : lp_set_) {
            std::cout << "  " << node_id(node) << " <- {";
            bool first = true;
            for (auto& p : parents) {
                if (!first) std::cout << ", ";
                std::cout << node_id(p);
                first = false;
            }
            std::cout << "}\n";
        }
    }

    void debugPrintNodeScopes_() {
        std::cout << "[scope] node scopes:\n";
        for (auto& [node, scope] : node_scope_) {
            std::cout << "  " << node_id(node) << " scope=" << scopeStr_(scope) << "\n";
        }
    }

    void debugPrintEdgeScopes_() {
        std::cout << "[scope] edge scopes:\n";
        for (auto& e : view_.getValidEdges()) {
            std::cout << "  " << edge_id(e, view_) << " scope=" << scopeStr_(edgeScope_(e)) << "\n";
        }
    }
};

// ---------- A minimal in-header ExampleIncView for demo and tests ----------
// This is optional. If you use your repository view, you do not need this.
// We implement a small synthetic graph with relations edge(X,Y), path(X,Y).
// The delta is a small set of edges; path facts derived accordingly.
class ExampleIncView : public IncrementalDerivationGraphViewInterface {
public:
    ExampleIncView() : builder_(std::make_unique<DerivationGraph>()) {}

    // Build a small integer graph with inserted/deleted facts to illustrate the API.
    void build_demo() {
        clearAll_();

        // Base edge(X,Y) facts. We treat (1,3) and (3,5) as inserts, (2,4) as a deletion.
        auto e12 = makeNode_("edge", {1, 2}, true);
        auto e23 = makeNode_("edge", {2, 3}, true);
        auto e34 = makeNode_("edge", {3, 4}, true);
        auto e45 = makeNode_("edge", {4, 5}, true);
        auto e24 = makeNode_("edge", {2, 4}, true, DeltaTag::Delete);
        auto e13 = makeNode_("edge", {1, 3}, true, DeltaTag::Insert);
        auto e35 = makeNode_("edge", {3, 5}, true, DeltaTag::Insert);
        auto e56 = makeNode_("edge", {5, 6}, true);
        auto e67 = makeNode_("edge", {6, 7}, true);
        auto e78 = makeNode_("edge", {7, 8}, true);

        // Derived path(X,Y) nodes we care about.
        auto p12 = makeNode_("path", {1, 2}, false);
        auto p23 = makeNode_("path", {2, 3}, false);
        auto p34 = makeNode_("path", {3, 4}, false);
        auto p45 = makeNode_("path", {4, 5}, false);
        auto p24 = makeNode_("path", {2, 4}, false);
        auto p13 = makeNode_("path", {1, 3}, false);
        auto p35 = makeNode_("path", {3, 5}, false);
        auto p14 = makeNode_("path", {1, 4}, false);
        auto p15 = makeNode_("path", {1, 5}, false);
        auto p25 = makeNode_("path", {2, 5}, false);
        auto p56 = makeNode_("path", {5, 6}, false);
        auto p67 = makeNode_("path", {6, 7}, false);
        auto p78 = makeNode_("path", {7, 8}, false);
        auto p57 = makeNode_("path", {5, 7}, false);
        auto p58 = makeNode_("path", {5, 8}, false);
        auto p26 = makeNode_("path", {2, 6}, false);
        auto p27 = makeNode_("path", {2, 7}, false);
        auto p28 = makeNode_("path", {2, 8}, false);
        auto p16 = makeNode_("path", {1, 6}, false);
        auto p17 = makeNode_("path", {1, 7}, false);
        auto p18 = makeNode_("path", {1, 8}, false);
        auto p36 = makeNode_("path", {3, 6}, false);
        auto p37 = makeNode_("path", {3, 7}, false);
        auto p38 = makeNode_("path", {3, 8}, false);
        auto p68 = makeNode_("path", {6, 8}, false);

        // Direct rules: path(X,Y) :- edge(X,Y)
        auto edge_p12 = addEdge_({ e12 }, p12);
        auto edge_p23 = addEdge_({ e23 }, p23);
        auto edge_p34 = addEdge_({ e34 }, p34);
        auto edge_p45 = addEdge_({ e45 }, p45);
        auto edge_p24_fact = addEdge_({ e24 }, p24, DeltaTag::Delete);
        auto edge_p13_fact = addEdge_({ e13 }, p13, DeltaTag::Insert);
        auto edge_p35_fact = addEdge_({ e35 }, p35, DeltaTag::Insert);
        auto edge_p56 = addEdge_({ e56 }, p56);
        auto edge_p67 = addEdge_({ e67 }, p67);
        auto edge_p78 = addEdge_({ e78 }, p78);

        // Recursive rules: path(X,Z) :- path(X,Y), edge(Y,Z)
        auto edge_p13_rec = addEdge_({ p12, e23 }, p13);
        auto edge_p14_rec = addEdge_({ p13, e34 }, p14);
        auto edge_p15_chain = addEdge_({ p14, e45 }, p15);
        auto edge_p24_rec = addEdge_({ p23, e34 }, p24);
        auto edge_p25_via24 = addEdge_({ p24, e45 }, p25);
        auto edge_p25_delta = addEdge_({ p23, e35 }, p25, DeltaTag::Insert);
        auto edge_p35_rec = addEdge_({ p34, e45 }, p35);
        auto edge_p15_delta = addEdge_({ p13, e35 }, p15, DeltaTag::Insert);
        auto edge_p57 = addEdge_({ p56, e67 }, p57);
        auto edge_p58 = addEdge_({ p57, e78 }, p58);
        auto edge_p26 = addEdge_({ p25, e56 }, p26);
        auto edge_p27 = addEdge_({ p26, e67 }, p27);
        auto edge_p28 = addEdge_({ p27, e78 }, p28);
        auto edge_p16 = addEdge_({ p15, e56 }, p16);
        auto edge_p17 = addEdge_({ p16, e67 }, p17);
        auto edge_p18 = addEdge_({ p17, e78 }, p18);
        auto edge_p36 = addEdge_({ p35, e56 }, p36);
        auto edge_p37 = addEdge_({ p36, e67 }, p37);
        auto edge_p38 = addEdge_({ p37, e78 }, p38);
        auto edge_p68 = addEdge_({ p67, e78 }, p68);

        // Illustrative impact lookup tables (not used by the analyzer but nice for demos).
        recordNodeImpact_(e13, {p13, p14, p15, p16, p17, p18}, /*insert*/ true);
        recordNodeImpact_(e35, {p35, p15, p25, p36, p37, p38}, /*insert*/ true);
        recordNodeImpact_(e24, {p24, p25, p26, p27, p28}, /*insert*/ false);

        recordEdgeImpact_(e13, {edge_p13_fact, edge_p13_rec, edge_p14_rec, edge_p15_chain, edge_p15_delta,
                edge_p16, edge_p17, edge_p18}, /*insert*/ true);
        recordEdgeImpact_(e35, {edge_p35_fact, edge_p35_rec, edge_p25_delta, edge_p15_delta,
                edge_p36, edge_p37, edge_p38}, /*insert*/ true);
        recordEdgeImpact_(e24, {edge_p24_fact, edge_p25_via24, edge_p26, edge_p27, edge_p28}, /*insert*/ false);

        (void)edge_p12; (void)edge_p23; (void)edge_p34; (void)edge_p45; (void)edge_p24_rec; (void)edge_p56;
        (void)edge_p67; (void)edge_p78; (void)edge_p57; (void)edge_p58; (void)edge_p68;

        // Make all facts/edges probabilistic for demo to exercise calibration.
        for (auto& n : nodes_) {
            if (n->isFact) {
                n->setProbability(0.6);
            }
        }
        for (auto& e : edges_) {
            e->setProbability(0.6);
        }

        // Reset cached validity sets inherited from IncrementalDerivationGraphViewInterface.
        validNodes_.clear();
        validEdges_.clear();
    }

    // ---------- Interface expected by analyzer ----------

    const std::unordered_set<NodePtr>& getNodes() const override { return nodes_; }
    const std::unordered_set<EdgePtr>& getEdges() const override { return edges_; }

    const std::set<NodePtr>& getDeltaInsertNodes() const override { return delta_ins_nodes_; }
    const std::set<NodePtr>& getDeltaDeleteNodes() const override { return delta_del_nodes_; }
    const std::set<EdgePtr>& getDeltaInsertEdges() const override { return delta_ins_edges_; }
    const std::set<EdgePtr>& getDeltaDeleteEdges() const override { return delta_del_edges_; }

    const std::unordered_map<NodePtr, std::unordered_set<NodePtr>>& getNodeImpactedByDeltaDelete() const override {
        return node_impacted_by_del_;
    }
    const std::unordered_map<NodePtr, std::unordered_set<EdgePtr>>& getEdgeImpactedByDeltaDelete() const override {
        return edge_impacted_by_del_;
    }
    const std::unordered_map<NodePtr, std::unordered_set<NodePtr>>& getNodeImpactedByDeltaInsert() const override {
        return node_impacted_by_ins_;
    }
    const std::unordered_map<NodePtr, std::unordered_set<EdgePtr>>& getEdgeImpactedByDeltaInsert() const override {
        return edge_impacted_by_ins_;
    }
    const std::unordered_set<NodePtr>& getDeltaInsertReachableNodes() const override {
        return delta_ins_reachable_nodes_;
    }
    const std::unordered_set<EdgePtr>& getDeltaInsertReachableEdges() const override {
        return delta_ins_reachable_edges_;
    }

private:
    enum class DeltaTag { None, Insert, Delete };

    static UntypedTuple makeTuple_(const std::string& rel, std::initializer_list<int> fields) {
        UntypedTuple tuple;
        tuple.relation_name = rel;
        tuple.fields.reserve(fields.size());
        for (int v : fields) {
            tuple.fields.push_back(static_cast<souffle::RamDomain>(v));
        }
        return tuple;
    }

    NodePtr makeNode_(const std::string& rel, std::initializer_list<int> fields, bool isFact,
            DeltaTag delta = DeltaTag::None) {
        auto tuple = makeTuple_(rel, fields);
        auto node = builder_->createNode(tuple);
        node->isFact = isFact;
        if (isFact) {
            node->setProbability(1.0);
        }
        nodes_.insert(node);
        if (delta == DeltaTag::Insert) {
            delta_ins_nodes_.insert(node);
            delta_ins_reachable_nodes_.insert(node);
        } else if (delta == DeltaTag::Delete) {
            delta_del_nodes_.insert(node);
        }
        return node;
    }

    EdgePtr addEdge_(const std::vector<NodePtr>& ins, const NodePtr& out, DeltaTag delta = DeltaTag::None) {
        auto e = builder_->createHyperedge(ins, out);
        if (!e) return nullptr;
        edges_.insert(e);
        if (delta == DeltaTag::Insert) {
            delta_ins_edges_.insert(e);
            delta_ins_reachable_edges_.insert(e);
        } else if (delta == DeltaTag::Delete) {
            delta_del_edges_.insert(e);
        }
        return e;
    }

    void recordNodeImpact_(const NodePtr& deltaNode, std::initializer_list<NodePtr> impacted, bool insert) {
        auto& target = insert ? node_impacted_by_ins_ : node_impacted_by_del_;
        auto& slot = target[deltaNode];
        slot.insert(impacted.begin(), impacted.end());
        if (insert) {
            delta_ins_reachable_nodes_.insert(deltaNode);
            delta_ins_reachable_nodes_.insert(impacted.begin(), impacted.end());
        }
    }

    void recordEdgeImpact_(const NodePtr& deltaNode, std::initializer_list<EdgePtr> impacted, bool insert) {
        auto& target = insert ? edge_impacted_by_ins_ : edge_impacted_by_del_;
        auto& slot = target[deltaNode];
        slot.insert(impacted.begin(), impacted.end());
        if (insert) {
            delta_ins_reachable_edges_.insert(impacted.begin(), impacted.end());
        }
    }

    void clearAll_() {
        builder_ = std::make_unique<DerivationGraph>();
        nodes_.clear(); edges_.clear();
        delta_ins_nodes_.clear(); delta_del_nodes_.clear();
        delta_ins_edges_.clear(); delta_del_edges_.clear();
        node_impacted_by_del_.clear(); edge_impacted_by_del_.clear();
        node_impacted_by_ins_.clear(); edge_impacted_by_ins_.clear();
        delta_ins_reachable_nodes_.clear();
        delta_ins_reachable_edges_.clear();
        validNodes_.clear(); validEdges_.clear();
    }

    std::unique_ptr<DerivationGraph> builder_;
    std::unordered_set<NodePtr>      nodes_;
    std::unordered_set<EdgePtr>      edges_;
    std::set<NodePtr>                delta_ins_nodes_, delta_del_nodes_;
    std::set<EdgePtr>                delta_ins_edges_, delta_del_edges_;
    std::unordered_map<NodePtr, std::unordered_set<NodePtr>> node_impacted_by_del_;
    std::unordered_map<NodePtr, std::unordered_set<EdgePtr>> edge_impacted_by_del_;
    std::unordered_map<NodePtr, std::unordered_set<NodePtr>> node_impacted_by_ins_;
    std::unordered_map<NodePtr, std::unordered_set<EdgePtr>> edge_impacted_by_ins_;
    std::unordered_set<NodePtr> delta_ins_reachable_nodes_;
    std::unordered_set<EdgePtr> delta_ins_reachable_edges_;
};

} // namespace incra
