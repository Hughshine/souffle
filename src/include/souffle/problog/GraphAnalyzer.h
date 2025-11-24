#ifndef GRAPHANALYZER_H
#define GRAPHANALYZER_H

#include "souffle/problog/DerivationGraph.h"

#include <algorithm>
#include <climits>
#include <fstream>
#include <iostream>
#include <queue>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

struct SISORegionInfo {
    std::vector<NodePtr> internalNodes;
    std::vector<EdgePtr> internalEdges;
    NodePtr entry = nullptr;
    EdgePtr entryEdge = nullptr;
    std::vector<NodePtr> entryPreds;
    NodePtr exit = nullptr;
    EdgePtr exitEdge = nullptr;
    NodePtr exitSucc = nullptr;
    bool valid = false;
    bool prefixAllFactsRequired = false;
};

class GraphAnalyzer {
private:
    using NodeSet = std::unordered_set<NodePtr>;
    using EdgeSet = std::unordered_set<EdgePtr>;
    using SupportMap = std::unordered_map<NodePtr, NodeSet>;
    using EdgeDomInputs = std::unordered_map<EdgePtr, std::vector<NodePtr>>;

    struct DomGraph {
        std::vector<NodePtr> nodes;                      // nodes in dom-graph
        std::unordered_map<NodePtr, int> indexOf;        // node -> index
        std::vector<std::vector<int>> preds;             // preds[i] = predecessor indices
        int entryIndex = -1;                             // chosen root for dominance
    };

    struct DomInfo {
        std::vector<std::unordered_set<int>> domSets;    // domSets[i] = set of dominators of i
        std::vector<int> idom;                           // immediate dominator of i
    };

    struct PrefixStructure {
        SupportMap support;
        EdgeDomInputs domInputs;
        DomGraph dg;
        DomInfo dom;
    };

    struct Region {
        NodeSet nodes;
        EdgeSet edges;
    };

    struct Candidate {
        NodePtr si = nullptr;
        NodePtr so = nullptr;
        EdgePtr exitEdge = nullptr;
        NodePtr exitSucc = nullptr;
    };

    // 找所有support input facts 所有backward能达到节点n的facts集合
    static NodeSet backwardCollectFacts(
        const DerivationGraphViewInterface& g,
        NodePtr start)
    {
        NodeSet facts;
        std::queue<NodePtr> q;
        std::unordered_set<NodePtr> vis;

        if (!start) return facts;
        q.push(start);
        vis.insert(start);

        while (!q.empty()) {
            NodePtr u = q.front();
            q.pop();

            auto inEdges = g.getIncomingEdges(u);
            if (inEdges.empty() || (u && u->isFact)) {
                facts.insert(u);
                continue;
            }

            for (EdgePtr e : inEdges) {
                if (!e) continue;
                for (NodePtr v : g.getInputs(e)) {
                    if (!v) continue;
                    if (vis.insert(v).second) q.push(v);
                }
            }
        }
        return facts;
    }

    static SupportMap buildSupportMap(const DerivationGraphViewInterface& g) {
        SupportMap mp;
        for (NodePtr n : g.getNodes()) {
            if (!n) continue;
            mp[n] = backwardCollectFacts(g, n);
        }
        return mp;
    }

    //计算超边的dominant input group 这里可能有问题？ 也有可能是后面合流点的计算问题 合流点全部是ai写的 他将图降维了不知道会有什么影响
    static EdgeDomInputs computeEdgeDomInputs (const DerivationGraphViewInterface& g, const SupportMap& sup) {
        EdgeDomInputs dpi;

        //调试
        static const std::string LOGF = "siso_edge_dom_inputs.log";
        std::ofstream log(LOGF, std::ios::app);
        if (!log.is_open()) {
            std::cerr << "ERROR: cannot open log file: " << LOGF << "\n";
            return dpi;
        }

        auto overlap = [](const NodeSet& a, const NodeSet& b) {
            if (a.size() < b.size()) {
                for (auto x : a) if (b.count(x)) return true;
            } else {
                for (auto x : b) if (a.count(x)) return true;
            }
            return false;
        };

        for (NodePtr n : g.getNodes()) {
            if (!n) continue;

            for (EdgePtr e : g.getOutgoingEdges(n)) {
                if (!e) continue;
                auto inputs = g.getInputs(e);
                if (inputs.empty()) continue;

                struct Group {
                    std::vector<NodePtr> members;
                    NodeSet supUnion;
                };
                std::vector<Group> groups;

                log << "\n===========================================\n";
                log << "Hyperedge e=" << e->getId()
                    << " → " << g.getOutput(e)->toString()
                    << "(id=" << g.getOutput(e)->getId() << ")\n";

                log << "Inputs:\n";
                for (NodePtr v : inputs) {
                    log << "  - " << v->toString()
                        << "(id=" << v->getId() << ")\n";
                }

                log << "\nSupport sets:\n";
                for (NodePtr v : inputs) {
                    const NodeSet& sv = sup.at(v);
                    log << "  Input " << v->toString()
                        << "(id=" << v->getId() << ") supports: { ";

                    for (NodePtr f : sv) {
                        log << f->toString()
                            << "(id=" << f->getId() << ") ";
                    }
                    log << "}\n";
                }

                for (NodePtr v : inputs) {
                    const NodeSet& sv = sup.at(v);

                    int grp = -1;
                    for (int i = 0; i < (int)groups.size(); i++) {
                        if (overlap(sv, groups[i].supUnion)) {
                            grp = i;
                            break;
                        }
                    }

                    if (grp < 0) {
                        groups.push_back(Group{{v}, NodeSet(sv.begin(), sv.end())});
                    } else {
                        groups[grp].members.push_back(v);
                        groups[grp].supUnion.insert(sv.begin(), sv.end());
                    }
                }


                log << "\nGroups formed:\n";
                for (int gi = 0; gi < (int)groups.size(); gi++) {
                    log << "  Group " << gi << " members: ";
                    for (NodePtr m : groups[gi].members) {
                        log << m->toString() << "(id=" << m->getId() << ") ";
                    }
                    log << "\n    supUnion: { ";
                    for (NodePtr f : groups[gi].supUnion) {
                        log << f->toString() << "(id=" << f->getId() << ") ";
                    }
                    log << "}\n";
                }

                if (groups.empty()) continue;


                int best = 0;
                size_t bestSize = groups[0].supUnion.size();
                for (int i = 1; i < (int)groups.size(); i++) {
                    if (groups[i].supUnion.size() > bestSize) {
                        best = i;
                        bestSize = groups[i].supUnion.size();
                    }
                }


                log << "\nDominant group = " << best
                    << " (supUnion size=" << bestSize << ")\n";
                log << "Dominant members: ";
                for (NodePtr m : groups[best].members) {
                    log << m->toString() << "(id=" << m->getId() << ") ";
                }
                log << "\n===========================================\n\n";

                dpi[e] = groups[best].members;
            }
        }

        return dpi;
}

    // ai说的将超边降维为普通边 只保留 dominant input -> output 暂且这样用 后面再说
    static DomGraph buildDomGraph(const DerivationGraphViewInterface& g, const EdgeDomInputs& dpi) {
        DomGraph dg;

        for (NodePtr n : g.getNodes()) {
            if (!n) continue;
            dg.indexOf[n] = (int)dg.nodes.size();
            dg.nodes.push_back(n);
        }

        int N = dg.nodes.size();
        dg.preds.assign(N, {});

        for (auto& kv : dpi) {
            EdgePtr e = kv.first;
            if (!e) continue;
            NodePtr out = g.getOutput(e);
            if (!out) continue;

            auto itOut = dg.indexOf.find(out);
            if (itOut == dg.indexOf.end()) continue;
            int outId = itOut->second;

            for (NodePtr v : kv.second) {
                auto itV = dg.indexOf.find(v);
                if (itV == dg.indexOf.end()) continue;
                dg.preds[outId].push_back(itV->second);
            }
        }

        dg.entryIndex = -1;
        for (int i = 0; i < N; i++) {
            if (dg.preds[i].empty()) {
                dg.entryIndex = i;
                break;
            }
        }
        if (dg.entryIndex < 0 && N > 0) dg.entryIndex = 0;

        return dg;
    }

    static DomInfo computeDominators(const DomGraph& dg) {
        DomInfo df;
        int N = dg.nodes.size();
        df.domSets.assign(N, {});
        df.idom.assign(N, -1);

        if (N == 0 || dg.entryIndex < 0) return df;

        for (int i = 0; i < N; i++) {
            if (i == dg.entryIndex) {
                df.domSets[i].insert(i);
            } else {
                for (int j = 0; j < N; j++) df.domSets[i].insert(j);
            }
        }

        bool changed = true;
        while (changed) {
            changed = false;

            for (int n = 0; n < N; n++) {
                if (n == dg.entryIndex) continue;
                const auto& ps = dg.preds[n];

                if (ps.empty()) {
                    std::unordered_set<int> newS = {n};
                    if (newS != df.domSets[n]) {
                        df.domSets[n] = std::move(newS);
                        changed = true;
                    }
                    continue;
                }

                std::unordered_set<int> newS = df.domSets[ps[0]];
                for (int k = 1; k < (int)ps.size(); k++) {
                    std::unordered_set<int> tmp;
                    for (int x : newS) if (df.domSets[ps[k]].count(x)) tmp.insert(x);
                    newS.swap(tmp);
                }

                newS.insert(n);

                if (newS != df.domSets[n]) {
                    df.domSets[n] = std::move(newS);
                    changed = true;
                }
            }
        }

        for (int n = 0; n < N; n++) {
            if (n == dg.entryIndex) {
                df.idom[n] = -1;
                continue;
            }
            auto& ds = df.domSets[n];

            std::vector<int> cand;
            for (int d : ds) if (d != n) cand.push_back(d);
            if (cand.empty()) {
                df.idom[n] = -1;
                continue;
            }

            int best = -1;
            for (int c : cand) {
                bool dominatedByOther = false;
                for (int o : cand) {
                    if (o == c) continue;
                    if (df.domSets[o].count(c)) {
                        dominatedByOther = true;
                        break;
                    }
                }
                if (!dominatedByOther) {
                    best = c;
                    break;
                }
            }
            if (best < 0) best = cand[0];
            df.idom[n] = best;
        }

        return df;
    }

    static PrefixStructure buildPrefixStructure(const DerivationGraphViewInterface& g) {
        PrefixStructure pre;
        pre.support = buildSupportMap(g);
        pre.domInputs = computeEdgeDomInputs(g, pre.support);
        pre.dg = buildDomGraph(g, pre.domInputs);
        pre.dom = computeDominators(pre.dg);
        return pre;
    }

    static int lcaInDomTree(int a, int b, const std::vector<int>& idom) {
        std::unordered_set<int> path;
        int x = a;
        while (x != -1) {
            path.insert(x);
            x = idom[x];
        }
        int y = b;
        while (y != -1) {
            if (path.count(y)) return y;
            y = idom[y];
        }
        return -1;
    }

    static int confluencePoint(const std::vector<int>& nodes,
                               const std::vector<int>& idom) {
        if (nodes.empty()) return -1;
        int ans = nodes[0];
        for (int i = 1; i < (int)nodes.size(); i++) {
            ans = lcaInDomTree(ans, nodes[i], idom);
            if (ans < 0) return -1;
        }
        return ans;
    }

    static Candidate findCandidate(
        const DerivationGraphViewInterface& g,
        EdgePtr exitEdge,
        const PrefixStructure& pre)
    {
        Candidate cand;
        if (!exitEdge) return cand;

        // exitSucc：exitEdge 的输出点
        NodePtr exitSucc = g.getOutput(exitEdge);
        if (!exitSucc) return cand;

        // 在 exitEdge 的所有 inputs 里找 SO：
        //    唯一 outgoing edge 且就是 exitEdge 的那个点
        // 这列没考虑exitNode同时有非exitEdge的outgoing edge到区域内部 这可能就是成环情况了？
        std::vector<NodePtr> soCandidates;
        auto inputs = g.getInputs(exitEdge);
        for (NodePtr n : inputs) {
            if (!n) continue;

            bool ok = true;
            for (EdgePtr oe : g.getOutgoingEdges(n)) {
                if (!oe) continue;

                if (oe == exitEdge) continue;

                NodePtr out = g.getOutput(oe);
                if (!out) {ok = false; break;}

                if (out == exitSucc) continue;
                if (out == n) continue;

                ok = false;
                break;
            }

            if (ok) {
                soCandidates.push_back(n);
            }
        }

        // 必须有且仅有一个 SO
        if (soCandidates.size() != 1) return cand;
        NodePtr exitNode = soCandidates[0];

        // 在 dom-graph 里找 exitNode 的 index
        auto itExitIdx = pre.dg.indexOf.find(exitNode);
        if (itExitIdx == pre.dg.indexOf.end()) return cand;
        int exitIdx = itExitIdx->second;

        // 收集 exitNode 所有 incoming hyperedge 的「dominant inputs」，
        //    在支配树上求合流点 → entryNode
        std::vector<int> srcIdx;

        for (EdgePtr eIn : g.getIncomingEdges(exitNode)) {
            if (!eIn) continue;

            const std::vector<NodePtr>* domSet = nullptr;
            std::vector<NodePtr> tmp;

            auto it = pre.domInputs.find(eIn);
            if (it != pre.domInputs.end()) {
                domSet = &it->second;           // 已经选过的 dominant input group
            } else {
                tmp = g.getInputs(eIn);         // 没有分组信息，就全部当 dominant
                domSet = &tmp;
            }

            for (NodePtr v : *domSet) {
                if (!v) continue;
                auto itV = pre.dg.indexOf.find(v);
                if (itV == pre.dg.indexOf.end()) continue;
                srcIdx.push_back(itV->second);
            }
        }

        // 如果 exitNode 没有 incoming edge，就把它自己当 source
        if (srcIdx.empty()) {
            srcIdx.push_back(exitIdx);
        }

        int entryIdx = confluencePoint(srcIdx, pre.dom.idom);
        if (entryIdx < 0 || entryIdx == exitIdx) return cand;

        NodePtr entryNode = pre.dg.nodes[entryIdx];
        if (!entryNode) return cand;

        cand.si = entryNode;
        cand.so = exitNode;
        cand.exitEdge = exitEdge;
        cand.exitSucc = exitSucc;
        return cand;
    }

    static Region buildStrictRegion(
        const DerivationGraphViewInterface& g,
        NodePtr entryNode,
        NodePtr exitNode,
        const EdgeDomInputs& domInputs)
    {
        Region reg;
        if (!entryNode || !exitNode) return reg;

        std::queue<NodePtr> q;
        q.push(exitNode);
        reg.nodes.insert(exitNode);

        bool reachedEntry = (exitNode == entryNode);

        while (!q.empty()) {
            NodePtr u = q.front();
            q.pop();

            if (u == entryNode) {
                reachedEntry = true;
                continue;
            }

            auto inEdges = g.getIncomingEdges(u);
            for (EdgePtr e : inEdges) {
                if (!e) continue;
                reg.edges.insert(e);

                auto it = domInputs.find(e);
                if (it != domInputs.end()) {
                    for (NodePtr v : it->second) {
                        if (!v) continue;
                        if (reg.nodes.insert(v).second) {
                            q.push(v);
                        }
                    }
                } else {
                    for (NodePtr v : g.getInputs(e)) {
                        if (!v) continue;
                        if (reg.nodes.insert(v).second) {
                            q.push(v);
                        }
                    }
                }
            }
        }

        if (!reachedEntry) {
            reg.nodes.clear();
            reg.edges.clear();
        }
        return reg;
    }

    static Region buildFullRegion(
        const DerivationGraphViewInterface& g,
        NodePtr entryNode,
        NodePtr exitNode)
    {
        Region reg;
        if (!entryNode || !exitNode) return reg;

        std::queue<NodePtr> q;
        q.push(exitNode);
        reg.nodes.insert(exitNode);

        bool reachedEntry = (exitNode == entryNode);

        while (!q.empty()) {
            NodePtr u = q.front();
            q.pop();

            if (u == entryNode) {
                reachedEntry = true;
                continue;
            }

            auto inEdges = g.getIncomingEdges(u);
            for (EdgePtr e : inEdges) {
                if (!e) continue;
                reg.edges.insert(e);

                for (NodePtr v : g.getInputs(e)) {
                    if (!v) continue;
                    if (reg.nodes.insert(v).second) {
                        q.push(v);
                    }
                }
            }
        }

        if (!reachedEntry) {
            reg.nodes.clear();
            reg.edges.clear();
        }
        return reg;
    }

    static bool selectEntryEdgeAndPreds(
    const DerivationGraphViewInterface& g,
    const Region& fullRegion,
    NodePtr entryNode,
    EdgePtr& entryEdge,
    std::vector<NodePtr>& entryPreds)
    {
        if (!entryNode) return false;

        entryPreds.clear();
        entryEdge = nullptr;

        // 用 set 去重：同一个外部前驱可能通过多条边进来
        std::unordered_set<NodePtr> predSet;

        for (EdgePtr eIn : g.getIncomingEdges(entryNode)) {
            if (!eIn) continue;
            NodePtr out = g.getOutput(eIn);
            if (out != entryNode) continue;

            bool hasOutsidePred = false;

            for (NodePtr p : g.getInputs(eIn)) {
                if (!p) continue;

                // 只关心“从 region 外进来的”前驱
                if (!fullRegion.nodes.count(p)) {
                    hasOutsidePred = true;

                    if (predSet.insert(p).second) {
                        entryPreds.push_back(p);
                    }
                }
            }

            // 记录一条真正跨 region 的 entryEdge（随便选一条，方便 dump/debug）
            if (hasOutsidePred && !entryEdge) {
                entryEdge = eIn;
            }
        }

        // 至少得有一个外部前驱，否则不算 SISO（纯 facts 前缀的特殊情况你以后要放开再改这里）
        return !entryPreds.empty();
    }

    static bool checkNoEscape(
    const DerivationGraphViewInterface& g,
    const Region& fullRegion,
    NodePtr exitNode,
    EdgePtr exitEdge,
    NodePtr exitSucc)
    {
        for (NodePtr n : fullRegion.nodes) {
            if (!n) continue;

            for (EdgePtr e : g.getOutgoingEdges(n)) {
                if (!e) continue;
                NodePtr out = g.getOutput(e);

                // 唯一允许离开区域的边：exitEdge
                // 不管是从哪个内部节点触发，都视为同一个出口
                if (e == exitEdge) {
                    continue;
                }

                bool outInside = (out && fullRegion.nodes.count(out));
                bool edgeInside = fullRegion.edges.count(e);

                // 任何指向区域外、或者不在 region edges 内的 outgoing edge，视为 escape
                if (!outInside || !edgeInside) {
                    return false;
                }
            }
        }
        return true;
    }

    static SISORegionInfo assembleSISO(
        const Region& strictRegion,
        const Region& fullRegion,
        NodePtr entryNode,
        EdgePtr entryEdge,
        const std::vector<NodePtr>& entryPreds,
        NodePtr exitNode,
        EdgePtr exitEdge,
        NodePtr exitSucc)
    {
        SISORegionInfo info;

        if (fullRegion.nodes.size() < 3) {
            return info;
        }

        info.entry = entryNode;
        info.entryEdge = entryEdge;
        info.entryPreds = entryPreds;
        info.exit = exitNode;
        info.exitEdge = exitEdge;
        info.exitSucc = exitSucc;

        info.internalNodes.assign(fullRegion.nodes.begin(), fullRegion.nodes.end());
        info.internalEdges.assign(fullRegion.edges.begin(), fullRegion.edges.end());

        // exitEdge 本身没有出现在 backward 收集里，需要手动加
        if (!fullRegion.edges.count(exitEdge)) {
            info.internalEdges.push_back(exitEdge);
        }

        info.valid = true;
        // prefixAllFactsRequired 暂时还是 false，看你后续是否要利用 support 做更细分判断
        info.prefixAllFactsRequired = false;
        return info;
    }

    static SISORegionInfo detectSISOStrictFromExitWithPrefix(
        const DerivationGraphViewInterface& g,
        EdgePtr exitEdge,
        const PrefixStructure& pre)
    {
        SISORegionInfo info;
        if (!exitEdge) return info;

        Candidate cand = findCandidate(g, exitEdge, pre);
        if (!cand.si || !cand.so || !cand.exitEdge || !cand.exitSucc) return info;

        // strict region 仅沿 dominant input 回溯
        Region strictR = buildStrictRegion(g, cand.si, cand.so, pre.domInputs);
        if (strictR.nodes.empty()) return info;

        // full region 沿所有 inputs 回溯，用来做 escape 检查
        Region fullR = buildFullRegion(g, cand.si, cand.so);
        if (fullR.nodes.empty()) return info;

        EdgePtr entryEdge = nullptr;
        std::vector<NodePtr> entryPreds;
        if (!selectEntryEdgeAndPreds(g, fullR, cand.si, entryEdge, entryPreds)) {
            return info;
        }

        if (!checkNoEscape(g, fullR, cand.so, cand.exitEdge, cand.exitSucc)) {
            return info;
        }

        // TODO: 在这里加 internalNodes evidence/query 过滤：

        info = assembleSISO(strictR, fullR, cand.si, entryEdge, entryPreds,
                            cand.so, cand.exitEdge, cand.exitSucc);
        return info;
    }

public:
    static inline SISORegionInfo detectSISOStrictFromExit(
        const DerivationGraphViewInterface& g,
        EdgePtr exitEdge)
    {
        PrefixStructure pre = buildPrefixStructure(g);
        return detectSISOStrictFromExitWithPrefix(g, exitEdge, pre);
    }

    static inline std::vector<SISORegionInfo> detectAllSISOStrictFromExit(
        const DerivationGraphViewInterface& g)
    {
        PrefixStructure pre = buildPrefixStructure(g);

        std::vector<SISORegionInfo> all;
        std::unordered_set<EdgePtr> seenEdges;

        for (NodePtr n : g.getNodes()) {
            if (!n) continue;
            for (EdgePtr e : g.getOutgoingEdges(n)) {
                if (!e) continue;
                if (!seenEdges.insert(e).second) continue;

                SISORegionInfo r = detectSISOStrictFromExitWithPrefix(g, e, pre);
                if (!r.valid) continue;
                all.push_back(std::move(r));
            }
        }

        if (all.empty()) return all;

        std::sort(all.begin(), all.end(),
                  [](const SISORegionInfo& a, const SISORegionInfo& b) {
                      return a.internalNodes.size() < b.internalNodes.size();
                  });

        std::vector<SISORegionInfo> result;
        NodeSet usedNodes;
        for (auto& r : all) {
            bool overlap = false;
            for (NodePtr n : r.internalNodes) {
                if (!n) continue;
                if (usedNodes.count(n)) {
                    overlap = true;
                    break;
                }
            }
            if (overlap) continue;

            for (NodePtr n : r.internalNodes) {
                if (!n) continue;
                usedNodes.insert(n);
            }
            result.push_back(std::move(r));
        }

        return result;
    }

    static inline void dumpRegionAsDot(const DerivationGraphViewInterface& g, const SISORegionInfo& r, const std::string& filename) {

    }

    static inline void printSISOInfo(const DerivationGraphViewInterface& g, const SISORegionInfo& r) {
        (void)g; // g 目前没用到，留着以防以后想打印更多信息
        std::ofstream out("siso_info.csv", std::ios::app);
        if (!out.is_open()) {
            std::cout << "cannot open siso_info.csv\n";
            return;
        }

        if (!r.valid) {
            out << "invalid\n";
            return;
        }

        out << "entry_node,entry_edge,entry_preds,exit_node,exit_edge,exit_succ,internal_nodes,internal_edges\n";

        // entry node
        out << "\"";
        if (r.entry) out << r.entry->toString() << "(id=" << r.entry->getId() << ")";
        else out << "null";
        out << "\",";

        // entry edge
        out << "\"";
        if (r.entryEdge) out << "edge" << r.entryEdge->getId();
        else out << "null";
        out << "\",";

        // entry preds
        out << "\"";
        if (r.entryPreds.empty()) {
            out << "none";
        } else {
            for (auto p : r.entryPreds) {
                if (!p) continue;
                out << p->toString() << "(id=" << p->getId() << ") ";
            }
        }
        out << "\",";

        // exit node
        out << "\"";
        if (r.exit) out << r.exit->toString() << "(id=" << r.exit->getId() << ")";
        else out << "null";
        out << "\",";

        // exit edge
        out << "\"";
        if (r.exitEdge) out << "edge" << r.exitEdge->getId();
        else out << "null";
        out << "\",";

        // exit succ
        out << "\"";
        if (r.exitSucc) out << r.exitSucc->toString() << "(id=" << r.exitSucc->getId() << ")";
        else out << "null";
        out << "\",";

        // internal nodes
        out << "\"";
        for (auto n : r.internalNodes) {
            if (!n) continue;
            out << n->toString() << "(id=" << n->getId() << ") ";
        }
        out << "\",";

        // internal edges
        out << "\"";
        for (auto e : r.internalEdges) {
            if (!e) continue;
            out << "edge" << e->getId() << " ";
        }
        out << "\"\n";
    }
};

#endif // GRAPHANALYZER_H
