#pragma once

// Paper-formalization-based incremental region identification.
//
// This header provides a drop-in replacement for the previous least-parents/scopes
// implementation. The core is RegionAnalyzer, which implements the region selection
// from CAV'26 "Incremental Compilation" Section 6.1 (Region Identification), using
// Definitions 8–11 and Theorem 2.
//
// Design goals:
//   * Make the expensive part scale with |delta| and |affected| (∆fact), not the full graph.
//   * Preserve the public interface (Region / Boundaries / IncRegionAnalysis / RegionAnalyzer).
//   * Produce boundary anchors compatible with BoundaryGateCalibrator.

#include <algorithm>
#include <cassert>
#include <chrono>
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

#include "souffle/problog/DerivationGraph.h"

namespace incra {

using NodePtr = std::shared_ptr<Node>;
using EdgePtr = std::shared_ptr<Hyperedge>;
using DGView  = IncrementalDerivationGraphViewInterface;

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
    // The original implementation carried 3 categories; we preserve them to keep
    // downstream consumers unchanged. Under the paper formalization, these are
    // simply convenient tags.
    std::set<NodePtr> out_induced;   // boundary node is head of some inserted edge (R1 violation seed)
    std::set<NodePtr> scope_induced; // boundary node is an inserted node (delta insert node)
    std::set<NodePtr> residual;      // other boundary nodes
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
    long diff() const { return long(naive_recomputed) - long(optimized_recomputed); }
};

class RegionAnalyzer {
public:
    explicit RegionAnalyzer(DGView& view) : view_(view) {}

    // Main entry: run analysis and emit outputs.
    Stats analyze(const std::vector<NodePtr>& delta_input_facts,
                  const std::string& json_out_path = "",
                  const std::string& csv_out_path  = "") {
        using Clock = std::chrono::steady_clock;
        auto now = [] { return Clock::now(); };
        auto toMs = [](auto dur) { return std::chrono::duration<double, std::milli>(dur).count(); };

        auto t0 = now();

        // 0) Prepare adjacency and delta caches.
        prepareGraphStructures_();
        auto tPrep = now();

        // 1) Build source atoms S (Def. 9) and seed nodes (R1-violation + inserted nodes).
        computeSourcesAndSeeds_(delta_input_facts);
        auto tSources = now();

        // 2) Compute ∆fact (affected atoms) via DepAnalysis-style reachability (Step 1).
        computeDeltaClosure_();
        auto tDelta = now();

        // 3) Region identification (Step 2) using Def. 10–11 and Theorem 2.
        Region region = selectRegionByReusability_();
        auto tRegion = now();

        // 4) Identify boundary nodes (dom(Anchor) in Algorithm 2 is represented here
        //    as region nodes with edges crossing to reused nodes).
        Boundaries B = classifyBoundaries_(region);
        auto tBound = now();

        // 5) Anchor candidates per boundary node (to be calibrated downstream).
        auto anchors = computeAnchorCandidates_(B);
        auto tAnch = now();

        // 6) Delta-reachable for stats and downstream SCC-closure filter.
        Region dr = deltaReachable_(delta_input_facts);
        auto tDR = now();

        // Store last state for toDot() and downstream consumers.
        last_region_ = region;
        last_boundaries_ = B;
        dr_ = dr;
        last_analysis_.region = region;
        last_analysis_.boundaries = B;
        last_analysis_.mergeableAnchorsByHead = anchors;
        have_last_ = true;

        Stats stats;
        stats.dr_nodes = dr.nodes.size();
        stats.dr_edges = dr.edges.size();
        stats.region_nodes = region.nodes.size();
        stats.region_edges = region.edges.size();
        for (const auto& n : region.nodes) if (n && !n->isFact) stats.optimized_recomputed++;
        for (const auto& n : dr.nodes)     if (n && !n->isFact) stats.naive_recomputed++;

        auto tEnd = now();
        std::cout << "[inc-region] timing(ms):"
                  << " prep=" << toMs(tPrep - t0)
                  << " sources=" << toMs(tSources - tPrep)
                  << " deltaClosure=" << toMs(tDelta - tSources)
                  << " selectRegion=" << toMs(tRegion - tDelta)
                  << " boundaries=" << toMs(tBound - tRegion)
                  << " anchors=" << toMs(tAnch - tBound)
                  << " deltaReach=" << toMs(tDR - tAnch)
                  << " total=" << toMs(tEnd - t0)
                  << " |S|=" << sources_.size()
                  << " |SCC(S)|=" << source_sccs_.size()
                  << " |∆fact|=" << reach_filter_.nodes.size()
                  << " |R|=" << region.nodes.size()
                  << " |B|=" << (B.out_induced.size() + B.scope_induced.size() + B.residual.size())
                  << "\n";

        emitConsole_(stats, region);
        if (!json_out_path.empty()) emitJSON_(stats, region, json_out_path);
        if (!csv_out_path.empty())  emitCSV_(stats, region, csv_out_path);

        return stats;
    }

    // Visualize with dot: region nodes (blue), boundary nodes (orange), delta items (green),
    // and anchor candidate edges into boundary nodes (red).
    bool toDot(const std::string& path, bool mark_merge = true) {
        if (!have_last_) return false;
        std::ofstream out(path);
        if (!out) return false;
        out << "digraph G {\n";
        out << "  rankdir=LR;\n";

        auto inRegion = [&](const NodePtr& n){ return last_region_.nodes.count(n) > 0; };
        auto isB = [&](const NodePtr& n){
            return last_boundaries_.out_induced.count(n)
                || last_boundaries_.scope_induced.count(n)
                || last_boundaries_.residual.count(n);
        };
        auto isDeltaN = [&](const NodePtr& n){
            return view_.getDeltaInsertNodes().count(n) || view_.getDeltaDeleteNodes().count(n);
        };
        auto isDeltaE = [&](const EdgePtr& e){
            return view_.getDeltaInsertEdges().count(e) || view_.getDeltaDeleteEdges().count(e);
        };

        // Nodes
        for (auto& n : view_.getValidNodes()) {
            std::string shape = (n && n->isFact) ? "box" : "ellipse";
            std::string pen   = inRegion(n) ? "2" : "1";
            std::string color = inRegion(n) ? "#1f77b4" : "black";
            if (isB(n)) { color = "#ff7f0e"; pen = "2"; }
            if (isDeltaN(n)) { color = "#2ca02c"; pen = "2"; }
            out << "  \"" << node_id(n) << "\""
                << " [shape=" << shape << ", penwidth=" << pen << ", color=\"" << color << "\"]";
            out << ";\n";
        }

        // Hyperedges with intermediate edge nodes.
        for (auto& e : view_.getValidEdges()) {
            auto head = view_.getOutput(e);
            auto ins  = view_.getInputs(e);
            bool inRegEdge = last_region_.edges.count(e) > 0;
            bool onBHead   = isB(head);
            bool delta     = isDeltaE(e);
            bool canMerge  = mark_merge && onBHead && isAnchorCandidate_(head, e);

            std::ostringstream edgeNodeName;
            edgeNodeName << "edge_node_" << reinterpret_cast<uintptr_t>(e.get());
            std::string color = "black";
            std::string pen   = "1";
            if (inRegEdge) { color = "#1f77b4"; pen = "2"; }
            if (delta)     { color = "#2ca02c"; pen = "2"; }
            if (canMerge)  { color = "red"; pen = "2"; }

            out << "  " << quote_(edgeNodeName.str())
                << " [shape=point, width=0.2, height=0.2, label=\"\", style=filled, color=\"" << color
                << "\", fillcolor=\"" << color << "\", penwidth=" << pen << "];\n";
            for (auto& t : ins) {
                out << "  \"" << node_id(t) << "\" -> " << quote_(edgeNodeName.str())
                    << " [color=\"" << color << "\", penwidth=" << pen << "];\n";
            }
            out << "  " << quote_(edgeNodeName.str()) << " -> \"" << node_id(head) << "\""
                << " [color=\"" << color << "\", penwidth=" << pen << "];\n";
        }

        emitLegend_(out, mark_merge);
        out << "}\n";
        return true;
    }

    // Accessors for last run.
    const Region&     lastRegion() const { return last_region_; }
    const Boundaries& lastBoundaries() const { return last_boundaries_; }
    const Region&     lastDeltaReachable() const { return dr_; }
    const IncRegionAnalysis& getLastAnalysis() const { return last_analysis_; }
    const std::vector<EdgePtr>& getMergeableAnchors(NodePtr v) const {
        static const std::vector<EdgePtr> kEmpty;
        auto it = last_analysis_.mergeableAnchorsByHead.find(v);
        return it == last_analysis_.mergeableAnchorsByHead.end() ? kEmpty : it->second;
    }

private:
    // ---------------------------------------------------------------------
    // Core: SelectRegion via reusability (Def. 11) + summary nodes (Def. 10).
    // ---------------------------------------------------------------------

    struct ReachInfo {
        std::unordered_set<NodePtr> nodes;
        std::unordered_set<EdgePtr> edges;
    };

    // Build adjacency maps on the *valid* (post-delete, unpruned) view.
    void prepareGraphStructures_() {
        incoming_edges_map_.clear();
        outgoing_edges_map_.clear();
        flat_preds_cache_.clear();
        anchor_cache_.clear();
        summary_cache_.clear();
        scc_forward_cache_.clear();
        scc_ancestor_cache_.clear();

        delta_insert_edges_cache_.clear();
        for (auto& e : view_.getDeltaInsertEdges()) {
            delta_insert_edges_cache_.insert(e);
        }
        delta_insert_nodes_cache_.clear();
        for (auto& n : view_.getDeltaInsertNodes()) {
            delta_insert_nodes_cache_.insert(n);
        }

        for (auto& n : view_.getValidNodes()) {
            incoming_edges_map_[n];
            outgoing_edges_map_[n];
        }
        for (auto& e : view_.getValidEdges()) {
            auto head = view_.getOutput(e);
            if (!head) continue;
            incoming_edges_map_[head].push_back(e);
            auto ins = view_.getInputs(e);
            for (auto& t : ins) {
                if (!t) continue;
                outgoing_edges_map_[t].push_back(e);
            }
        }
    }

    // Compute sources S and seeds.
    void computeSourcesAndSeeds_(const std::vector<NodePtr>& delta_input_facts) {
        sources_.clear();
        seeds_.clear();
        inserted_edge_heads_.clear();

        // Treat explicit delta inputs as inserted nodes.
        for (auto& n : delta_input_facts) {
            if (!n) continue;
            sources_.insert(n);
            seeds_.insert(n);
        }

        // Inserted nodes are sources and seeds.
        for (auto& n : delta_insert_nodes_cache_) {
            if (!n) continue;
            sources_.insert(n);
            seeds_.insert(n);
        }

        // Inserted edges contribute both head and body atoms to S (Def. 9).
        for (auto& e : view_.getDeltaInsertEdges()) {
            if (!e) continue;
            NodePtr h = view_.getOutput(e);
            if (h) {
                sources_.insert(h);
                inserted_edge_heads_.insert(h);
                seeds_.insert(h); // R1-violation seed: new incoming derivation edge into h.
            }
            for (auto& b : view_.getInputs(e)) {
                if (b) sources_.insert(b);
            }
        }

        // Fallback: if delta inputs were empty, still keep something deterministic.
        if (sources_.empty()) {
            for (auto& n : view_.getDeltaInsertNodes()) {
                if (n) sources_.insert(n);
            }
            for (auto& e : view_.getDeltaInsertEdges()) {
                if (auto h = view_.getOutput(e)) sources_.insert(h);
            }
        }
    }

    // Step 1 (Algorithm 2): compute ∆fact = DepAnalysis(Πder, ∆der).
    // We use the view's precomputed delta-reachable closure when available.
    void computeDeltaClosure_() {
        reach_filter_.nodes.clear();
        reach_filter_.edges.clear();

        const auto& cacheNodes = view_.getDeltaInsertReachableNodes();
        const auto& cacheEdges = view_.getDeltaInsertReachableEdges();
        if (!cacheNodes.empty() || !cacheEdges.empty()) {
            reach_filter_.nodes.insert(cacheNodes.begin(), cacheNodes.end());
            reach_filter_.edges.insert(cacheEdges.begin(), cacheEdges.end());
        } else {
            // Conservative reachability on the flattened graph (body atom -> head atom).
            std::queue<NodePtr> q;
            for (const auto& s : sources_) {
                if (!s) continue;
                if (reach_filter_.nodes.insert(s).second) {
                    q.push(s);
                }
            }
            while (!q.empty()) {
                NodePtr cur = q.front();
                q.pop();
                auto it = outgoing_edges_map_.find(cur);
                if (it == outgoing_edges_map_.end()) continue;
                for (const auto& e : it->second) {
                    if (!e) continue;
                    reach_filter_.edges.insert(e);
                    NodePtr head = view_.getOutput(e);
                    if (!head) continue;
                    if (reach_filter_.nodes.insert(head).second) {
                        q.push(head);
                    }
                }
            }
        }

        // Ensure sources and delta items are included.
        for (const auto& s : sources_) if (s) reach_filter_.nodes.insert(s);
        for (const auto& e : view_.getDeltaInsertEdges()) {
            if (!e) continue;
            reach_filter_.edges.insert(e);
            if (auto h = view_.getOutput(e)) reach_filter_.nodes.insert(h);
            for (auto b : view_.getInputs(e)) if (b) reach_filter_.nodes.insert(b);
        }
        for (const auto& n : view_.getDeltaInsertNodes()) if (n) reach_filter_.nodes.insert(n);
    }

    // Build region R = { v ∈ ∆fact | v is not reusable } (Theorem 2).
    Region selectRegionByReusability_() {
        Region R;

        // Obtain SCC DAG for Dep(.) approximations and reachability grouping.
        scc_ = &view_.getCycleDependencyGraph();
        assert(scc_ && "CycleDependencyGraph unavailable");

        // Group sources by SCC id (sources in same SCC have identical reachability).
        source_sccs_.clear();
        for (const auto& s : sources_) {
            if (!s) continue;
            auto it = scc_->nodeToCycleIndex.find(s);
            if (it != scc_->nodeToCycleIndex.end()) {
                source_sccs_.insert(it->second);
            }
        }

        // Memoize summary candidates per (sourceScc, target).
        // We compute non-reusability sets per source SCC and union them.
        std::unordered_set<NodePtr> regionNodes;
        regionNodes.reserve(reach_filter_.nodes.size());

        for (size_t sid : source_sccs_) {
            // Forward closure from this source SCC.
            const auto& reachScc = forwardReachScc_(sid);

            // Worklist for non-reusable nodes w.r.t. this source SCC.
            std::queue<NodePtr> q;
            std::unordered_set<NodePtr> nonReuse;

            auto seedIfReachable = [&](const NodePtr& v) {
                if (!v) return;
                if (!reach_filter_.nodes.count(v)) return; // v ∉ ∆fact => trivial reusable
                auto it = scc_->nodeToCycleIndex.find(v);
                if (it == scc_->nodeToCycleIndex.end()) return;
                if (!reachScc.count(it->second)) return; // sid ∉ Dep(v) => trivial reusable
                if (nonReuse.insert(v).second) {
                    q.push(v);
                }
            };

            // R1 violation seeds (incoming inserted edge) + inserted nodes.
            for (const auto& v : seeds_) {
                seedIfReachable(v);
            }

            while (!q.empty()) {
                NodePtr cur = q.front();
                q.pop();

                auto outIt = outgoing_edges_map_.find(cur);
                if (outIt == outgoing_edges_map_.end()) continue;

                for (const auto& e : outIt->second) {
                    if (!e) continue;
                    NodePtr head = view_.getOutput(e);
                    if (!head) continue;
                    if (!reach_filter_.nodes.count(head)) continue;
                    if (nonReuse.count(head)) continue;

                    // Summary-node short-circuit for head w.r.t. this source SCC.
                    NodePtr summ = summaryPred_(sid, head);
                    if (summ && summ.get() == cur.get()) {
                        // R2 satisfied via summary; do not propagate non-reusability along this arc.
                        continue;
                    }
                    nonReuse.insert(head);
                    q.push(head);
                }
            }

            // Union into global region.
            regionNodes.insert(nonReuse.begin(), nonReuse.end());
        }

        // Region nodes are exactly the non-reusable nodes in ∆fact.
        R.nodes = std::move(regionNodes);

        // Region edges are not part of the paper definition; we collect a useful
        // superset for debugging and for matching the old interface.
        for (const auto& e : view_.getValidEdges()) {
            if (!e) continue;
            NodePtr head = view_.getOutput(e);
            if (head && R.nodes.count(head)) {
                R.edges.insert(e);
            }
        }
        // Also keep inserted edges to ease visualization.
        for (const auto& e : view_.getDeltaInsertEdges()) {
            if (e) R.edges.insert(e);
        }
        return R;
    }

    // ---------------------------------------------------------------------
    // Summary node oracle (Def. 10)
    // ---------------------------------------------------------------------

    // Return the (unique) summary predecessor t' for target t w.r.t. source SCC sid,
    // or nullptr if no summary exists. Cached per (sid,t).
    NodePtr summaryPred_(size_t sid, const NodePtr& t) {
        if (!t) return nullptr;
        auto& perSid = summary_cache_[sid];
        auto it = perSid.find(t);
        if (it != perSid.end()) {
            return it->second; // may be nullptr (meaning "computed: none")
        }
        NodePtr res = computeSummaryPred_(sid, t);
        perSid.emplace(t, res);
        return res;
    }

    NodePtr computeSummaryPred_(size_t sid, const NodePtr& t) {
        // If sid does not reach t, summary is irrelevant.
        const auto& reachScc = forwardReachScc_(sid);
        auto tit = scc_->nodeToCycleIndex.find(t);
        if (tit == scc_->nodeToCycleIndex.end()) return nullptr;
        if (!reachScc.count(tit->second)) return nullptr;

        const auto& preds = flatPreds_(t);
        if (preds.empty()) return nullptr;

        // (C2-first half): exactly one predecessor of t is reachable from sid.
        NodePtr candidate;
        for (const auto& p : preds) {
            if (!p) continue;
            auto pit = scc_->nodeToCycleIndex.find(p);
            if (pit == scc_->nodeToCycleIndex.end()) continue;
            if (reachScc.count(pit->second)) {
                if (!candidate) {
                    candidate = p;
                } else {
                    // Multiple reachable predecessors => no single-entry.
                    return nullptr;
                }
            }
        }
        if (!candidate) {
            // Should not happen if sid reaches t, but keep safe.
            return nullptr;
        }

        // (C2-second half): Dep(candidate) must be disjoint from Dep(p) for every other pred p.
        // We approximate Dep(.) using SCC-ancestor sets in the condensation DAG, which is exact
        // for reachability questions.
        auto cit = scc_->nodeToCycleIndex.find(candidate);
        if (cit == scc_->nodeToCycleIndex.end()) return nullptr;
        size_t candCid = cit->second;
        for (const auto& p : preds) {
            if (!p || p.get() == candidate.get()) continue;
            auto pit = scc_->nodeToCycleIndex.find(p);
            if (pit == scc_->nodeToCycleIndex.end()) continue;
            if (!depDisjointScc_(candCid, pit->second)) {
                return nullptr;
            }
        }

        // (C3): require at least one anchor edge into candidate.
        const auto& ac = anchorCandidates_(candidate);
        if (ac.empty()) {
            return nullptr;
        }
        return candidate;
    }

    // Flattened predecessors of t (Definition 8): u is a predecessor if u appears in the body
    // of some incoming derivation edge of t.
    const std::vector<NodePtr>& flatPreds_(const NodePtr& t) {
        static const std::vector<NodePtr> kEmpty;
        if (!t) return kEmpty;
        auto it = flat_preds_cache_.find(t);
        if (it != flat_preds_cache_.end()) return it->second;

        std::unordered_set<NodePtr> seen;
        std::vector<NodePtr> preds;
        auto inIt = incoming_edges_map_.find(t);
        if (inIt != incoming_edges_map_.end()) {
            for (const auto& e : inIt->second) {
                if (!e) continue;
                for (const auto& b : view_.getInputs(e)) {
                    if (!b) continue;
                    if (seen.insert(b).second) {
                        preds.push_back(b);
                    }
                }
            }
        }
        auto ins = flat_preds_cache_.emplace(t, std::move(preds));
        return ins.first->second;
    }

    // ---------------------------------------------------------------------
    // SCC closures for reachability and dependency intersection.
    // ---------------------------------------------------------------------

    const std::unordered_set<size_t>& forwardReachScc_(size_t cid) {
        auto it = scc_forward_cache_.find(cid);
        if (it != scc_forward_cache_.end()) return it->second;
        std::unordered_set<size_t> reach{cid};
        if (cid < scc_->reverseDependencies.size()) {
            for (auto succ : scc_->reverseDependencies[cid]) {
                const auto& r2 = forwardReachScc_(succ);
                reach.insert(r2.begin(), r2.end());
            }
        }
        auto ins = scc_forward_cache_.emplace(cid, std::move(reach));
        return ins.first->second;
    }

    const std::unordered_set<size_t>& ancestorScc_(size_t cid) {
        auto it = scc_ancestor_cache_.find(cid);
        if (it != scc_ancestor_cache_.end()) return it->second;
        std::unordered_set<size_t> anc{cid};
        if (cid < scc_->dependencies.size()) {
            for (auto pred : scc_->dependencies[cid]) {
                const auto& a2 = ancestorScc_(pred);
                anc.insert(a2.begin(), a2.end());
            }
        }
        auto ins = scc_ancestor_cache_.emplace(cid, std::move(anc));
        return ins.first->second;
    }

    bool depDisjointScc_(size_t a, size_t b) {
        const auto& A = ancestorScc_(a);
        const auto& B = ancestorScc_(b);
        if (A.empty() || B.empty()) return true;
        if (A.size() <= B.size()) {
            for (auto x : A) if (B.count(x)) return false;
        } else {
            for (auto x : B) if (A.count(x)) return false;
        }
        return true;
    }

    // ---------------------------------------------------------------------
    // Boundary identification + anchor candidates.
    // ---------------------------------------------------------------------

    Boundaries classifyBoundaries_(const Region& R) {
        Boundaries B;
        std::unordered_set<NodePtr> boundary_nodes;
        // boundary nodes are region nodes with at least one outgoing arc to a non-region head.
        for (const auto& e : view_.getValidEdges()) {
            if (!e) continue;
            NodePtr head = view_.getOutput(e);
            if (!head) continue;
            if (R.nodes.count(head)) continue;
            for (const auto& tail : view_.getInputs(e)) {
                if (!tail) continue;
                if (R.nodes.count(tail)) {
                    boundary_nodes.insert(tail);
                }
            }
        }

        for (const auto& n : boundary_nodes) {
            if (inserted_edge_heads_.count(n)) {
                B.out_induced.insert(n);
            } else if (delta_insert_nodes_cache_.count(n)) {
                B.scope_induced.insert(n);
            } else {
                B.residual.insert(n);
            }
        }
        return B;
    }

    std::unordered_map<NodePtr, std::vector<EdgePtr>> computeAnchorCandidates_(const Boundaries& B) {
        std::unordered_map<NodePtr, std::vector<EdgePtr>> out;
        std::set<NodePtr> boundary_nodes;
        boundary_nodes.insert(B.out_induced.begin(), B.out_induced.end());
        boundary_nodes.insert(B.scope_induced.begin(), B.scope_induced.end());
        boundary_nodes.insert(B.residual.begin(), B.residual.end());
        for (const auto& v : boundary_nodes) {
            out[v] = anchorCandidates_(v);
        }
        return out;
    }

    // Anchor candidate edges into v (Def. 10(C3) witness edges), suitable for calibration.
    // We approximate the "occurs in Πbdd(t)" test by selecting an old, non-deterministic,
    // non-subsumed incoming edge into v.
    const std::vector<EdgePtr>& anchorCandidates_(const NodePtr& v) {
        static const std::vector<EdgePtr> kEmpty;
        if (!v) return kEmpty;
        auto it = anchor_cache_.find(v);
        if (it != anchor_cache_.end()) return it->second;

        std::vector<EdgePtr> candidates;
        // Use stable ordering for determinism.
        auto inEdges = view_.getIncomingEdgesStable(v);
        // Filter to old incoming edges with a variable.
        std::vector<EdgePtr> oldIncoming;
        oldIncoming.reserve(inEdges.size());
        for (const auto& e : inEdges) {
            if (!e) continue;
            if (e->isDeterministic()) continue;                 // must have variable label
            if (delta_insert_edges_cache_.count(e)) continue;   // must be old edge (Πder)
            oldIncoming.push_back(e);
        }

        // Non-subsumption (C3): ensure e can be the unique satisfied old alternative.
        // We use a sufficient condition: no other old incoming edge has a body that is a subset
        // of e's body.
        auto tupleLess = [](const NodePtr& a, const NodePtr& b) {
            return a->getTuple() < b->getTuple();
        };

        // Cache stable body vectors.
        std::vector<std::vector<NodePtr>> bodies;
        bodies.reserve(oldIncoming.size());
        for (const auto& e : oldIncoming) {
            auto b = view_.getInputsStable(e);
            // Ensure sorted by tupleLess.
            std::sort(b.begin(), b.end(), tupleLess);
            bodies.push_back(std::move(b));
        }

        for (size_t i = 0; i < oldIncoming.size(); ++i) {
            bool subsumed = false;
            const auto& bodyI = bodies[i];
            for (size_t j = 0; j < oldIncoming.size(); ++j) {
                if (i == j) continue;
                const auto& bodyJ = bodies[j];
                if (bodyJ.empty()) continue;
                // If bodyJ ⊆ bodyI then i is subsumed by j.
                if (std::includes(bodyI.begin(), bodyI.end(), bodyJ.begin(), bodyJ.end(), tupleLess)) {
                    subsumed = true;
                    break;
                }
            }
            if (!subsumed) {
                candidates.push_back(oldIncoming[i]);
            }
        }

        auto ins = anchor_cache_.emplace(v, std::move(candidates));
        return ins.first->second;
    }

    bool isAnchorCandidate_(const NodePtr& head, const EdgePtr& e) {
        if (!head || !e) return false;
        const auto& cs = anchorCandidates_(head);
        for (const auto& x : cs) {
            if (x.get() == e.get()) return true;
        }
        return false;
    }

    // ---------------------------------------------------------------------
    // Delta-reachable region (for stats + downstream SCC-closure filter).
    // ---------------------------------------------------------------------

    Region deltaReachable_(const std::vector<NodePtr>& /*delta_input_facts*/) {
        Region DR;
        const auto& reachNodes = view_.getDeltaInsertReachableNodes();
        const auto& reachEdges = view_.getDeltaInsertReachableEdges();
        if (!reachNodes.empty() || !reachEdges.empty()) {
            DR.nodes.insert(reachNodes.begin(), reachNodes.end());
            DR.edges.insert(reachEdges.begin(), reachEdges.end());
        } else {
            // Fallback: reuse the closure computed in computeDeltaClosure_.
            DR.nodes.insert(reach_filter_.nodes.begin(), reach_filter_.nodes.end());
            DR.edges.insert(reach_filter_.edges.begin(), reach_filter_.edges.end());
        }
        // Ensure delta items are in.
        for (const auto& n : view_.getDeltaInsertNodes()) {
            if (n) DR.nodes.insert(n);
        }
        for (const auto& e : view_.getDeltaInsertEdges()) {
            if (!e) continue;
            DR.edges.insert(e);
            if (auto h = view_.getOutput(e)) DR.nodes.insert(h);
        }
        return DR;
    }

    // ---------------------------------------------------------------------
    // Emission helpers.
    // ---------------------------------------------------------------------

    void emitConsole_(const Stats& s, const Region& R) {
        std::cout << "=== Incremental Region Analysis (Paper formalization) ===\n";
        std::cout << "Region nodes: " << s.region_nodes << " / DR nodes: " << s.dr_nodes << "\n";
        std::cout << "Region edges: " << s.region_edges << " / DR edges: " << s.dr_edges << "\n";
        std::cout << "Recompute (optimized): " << s.optimized_recomputed
                  << " ; (naive): " << s.naive_recomputed
                  << " ; ratio: " << std::fixed << std::setprecision(3) << s.ratio()
                  << " ; diff: " << s.diff() << "\n";

        // Basic per-node print.
        for (auto& n : view_.getValidNodes()) {
            bool inR   = R.nodes.count(n);
            bool delta = view_.getDeltaInsertNodes().count(n) || view_.getDeltaDeleteNodes().count(n);
            std::cout << "node,\"" << node_id(n) << "\"," << (n && n->isFact ? 1 : 0)
                      << "," << (inR ? 1 : 0)
                      << "," << (delta ? 1 : 0)
                      << "\n";
        }
        for (auto& e : view_.getValidEdges()) {
            bool inR   = R.edges.count(e);
            bool delta = view_.getDeltaInsertEdges().count(e) || view_.getDeltaDeleteEdges().count(e);
            NodePtr head = view_.getOutput(e);
            bool anchor = head && isAnchorCandidate_(head, e);
            std::cout << "edge,\"" << edge_id(e, view_) << "\"," << 0
                      << "," << (inR ? 1 : 0)
                      << "," << (delta ? 1 : 0)
                      << "," << (anchor ? 1 : 0)
                      << "\n";
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
            out << "    {\"id\": " << quote_(node_id(n))
                << ", \"is_fact\": " << (n && n->isFact ? 1 : 0)
                << ", \"in_region\": " << (inR ? 1 : 0)
                << ", \"is_delta\": " << (delta ? 1 : 0)
                << "}";
        }
        out << "\n  ],\n  \"edges\": [\n";
        first = true;
        for (auto& e : view_.getValidEdges()) {
            if (!first) out << ",\n";
            first = false;
            bool inR   = R.edges.count(e);
            bool delta = view_.getDeltaInsertEdges().count(e) || view_.getDeltaDeleteEdges().count(e);
            NodePtr head = view_.getOutput(e);
            bool anchor = head && isAnchorCandidate_(head, e);
            out << "    {\"id\": " << quote_(edge_id(e, view_))
                << ", \"in_region\": " << (inR ? 1 : 0)
                << ", \"is_delta\": " << (delta ? 1 : 0)
                << ", \"anchor_candidate\": " << (anchor ? 1 : 0)
                << "}";
        }
        out << "\n  ]\n}\n";
    }

    void emitCSV_(const Stats& s, const Region& R, const std::string& path) {
        std::ofstream out(path);
        if (!out) return;
        out << "type,id,is_fact,in_region,is_delta,anchor_candidate\n";
        for (auto& n : view_.getValidNodes()) {
            bool inR   = R.nodes.count(n);
            bool delta = view_.getDeltaInsertNodes().count(n) || view_.getDeltaDeleteNodes().count(n);
            out << "node," << quote_(node_id(n)) << "," << (n && n->isFact ? 1 : 0)
                << "," << (inR ? 1 : 0)
                << "," << (delta ? 1 : 0)
                << ",0\n";
        }
        for (auto& e : view_.getValidEdges()) {
            bool inR   = R.edges.count(e);
            bool delta = view_.getDeltaInsertEdges().count(e) || view_.getDeltaDeleteEdges().count(e);
            NodePtr head = view_.getOutput(e);
            bool anchor = head && isAnchorCandidate_(head, e);
            out << "edge," << quote_(edge_id(e, view_)) << ",0"
                << "," << (inR ? 1 : 0)
                << "," << (delta ? 1 : 0)
                << "," << (anchor ? 1 : 0)
                << "\n";
        }
    }

    void emitLegend_(std::ofstream& out, bool includeMerge) const {
        out << "  subgraph cluster_legend {\n";
        out << "    label=\"Legend\";\n";
        out << "    color=gray;\n";
        out << "    legend_fact [shape=box, label=\"Fact node\"];\n";
        out << "    legend_derived [shape=ellipse, label=\"Derived node\"];\n";
        out << "    legend_region [shape=ellipse, penwidth=2, color=\"#1f77b4\", label=\"Region member\"];\n";
        out << "    legend_boundary [shape=ellipse, penwidth=2, color=\"#ff7f0e\", label=\"Boundary node\"];\n";
        out << "    legend_delta_node [shape=ellipse, penwidth=2, color=\"#2ca02c\", label=\"Delta node\"];\n";
        out << "    legend_hyper_region [shape=point, width=0.2, height=0.2, label=\"\", style=filled, color=\"#1f77b4\", fillcolor=\"#1f77b4\"];\n";
        out << "    legend_hyper_delta  [shape=point, width=0.2, height=0.2, label=\"\", style=filled, color=\"#2ca02c\", fillcolor=\"#2ca02c\"];\n";
        if (includeMerge) {
            out << "    legend_hyper_anchor [shape=point, width=0.2, height=0.2, label=\"\", style=filled, color=\"red\", fillcolor=\"red\"];\n";
        }
        out << "  }\n";
    }

    static std::string quote_(const std::string& s) {
        std::ostringstream oss;
        oss << "\"";
        for (char c : s) {
            if (c == '"') oss << "\\\"";
            else if (c == '\\') oss << "\\\\";
            else oss << c;
        }
        oss << "\"";
        return oss.str();
    }

private:
    DGView& view_;

    // Structural helpers (valid view only).
    std::unordered_map<NodePtr, std::vector<EdgePtr>> incoming_edges_map_;
    std::unordered_map<NodePtr, std::vector<EdgePtr>> outgoing_edges_map_;
    std::unordered_map<NodePtr, std::vector<NodePtr>> flat_preds_cache_;

    // Delta caches.
    std::unordered_set<EdgePtr> delta_insert_edges_cache_;
    std::unordered_set<NodePtr> delta_insert_nodes_cache_;

    // Paper concepts.
    std::unordered_set<NodePtr> sources_;            // S (Def. 9)
    std::unordered_set<NodePtr> seeds_;              // R1-violation heads + inserted nodes
    std::unordered_set<NodePtr> inserted_edge_heads_;
    ReachInfo reach_filter_;                         // ∆fact (Step 1)

    // SCC-level caches.
    const CycleDependencyGraph* scc_ = nullptr;
    std::unordered_set<size_t> source_sccs_;
    std::unordered_map<size_t, std::unordered_set<size_t>> scc_forward_cache_;
    std::unordered_map<size_t, std::unordered_set<size_t>> scc_ancestor_cache_;

    // Summary cache: per (sourceScc, target) -> summary predecessor (or nullptr).
    std::unordered_map<size_t, std::unordered_map<NodePtr, NodePtr>> summary_cache_;

    // Anchor candidates cache: node -> candidate incoming edges.
    std::unordered_map<NodePtr, std::vector<EdgePtr>> anchor_cache_;

    // Last run artifacts.
    Region last_region_;
    Boundaries last_boundaries_;
    Region dr_;
    IncRegionAnalysis last_analysis_;
    bool have_last_ = false;
};

} // namespace incra
