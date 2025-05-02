#include "souffle/problog/DerivationGraph.h"
#include <iostream>
#include <unordered_map>
#include <unordered_set>
void computeSCCOrderedCyclesWithDepth(
    const DerivationGraph& graph,
    std::vector<std::unordered_set<NodePtr>>& nodeCycles,
    std::vector<std::unordered_set<EdgePtr>>& edgeCycles,
    std::unordered_map<NodePtr, size_t>& nodeToCycleIndex,
    std::unordered_map<EdgePtr, size_t>& edgeToCycleIndex,
    std::unordered_map<NodePtr, size_t>& nodeDepths,
    std::unordered_map<EdgePtr, size_t>& edgeDepths
) {
    const auto& nodes = graph.getNodes();
    size_t index = 0, currentSCC = 0;
    std::unordered_map<NodePtr, size_t> indices, lowlinks;
    std::stack<NodePtr> stack;
    std::unordered_set<NodePtr> onStack;
    std::vector<std::unordered_set<NodePtr>> rawNodeCycles;
    std::unordered_map<NodePtr, size_t> rawNodeToCycle;

    std::function<void(NodePtr)> strongconnect = [&](NodePtr v) {
        indices[v] = lowlinks[v] = index++;
        stack.push(v);
        onStack.insert(v);

        for (const auto& edge : v->getOutgoingEdges()) {
            NodePtr w = edge->getOutput();
            if (indices.find(w) == indices.end()) {
                strongconnect(w);
                lowlinks[v] = std::min(lowlinks[v], lowlinks[w]);
            } else if (onStack.count(w)) {
                lowlinks[v] = std::min(lowlinks[v], indices[w]);
            }
        }

        if (lowlinks[v] == indices[v]) {
            std::unordered_set<NodePtr> scc;
            NodePtr w;
            do {
                w = stack.top(); stack.pop();
                onStack.erase(w);
                scc.insert(w);
                rawNodeToCycle[w] = currentSCC;
            } while (w != v);
            rawNodeCycles.push_back(std::move(scc));
            ++currentSCC;
        }
    };

    for (const auto& node : nodes) {
        if (indices.find(node) == indices.end()) {
            strongconnect(node);
        }
    }

    // Build dependency graph of SCCs (revised: use all inputs of each edge)
    std::vector<std::unordered_set<size_t>> sccGraph(currentSCC);
    std::vector<size_t> indegree(currentSCC, 0);
    for (const auto& edge : graph.getEdges()) {
        size_t outCycle = rawNodeToCycle[edge->getOutput()];
        for (const auto& input : edge->getInputs()) {
            size_t inCycle = rawNodeToCycle[input];
            if (inCycle != outCycle && !sccGraph[inCycle].count(outCycle)) {
                sccGraph[inCycle].insert(outCycle);
                indegree[outCycle]++;
            }
        }
    }

    // Kahn's algorithm for topological sort
    std::queue<size_t> q;
    for (size_t i = 0; i < indegree.size(); ++i) {
        if (indegree[i] == 0) q.push(i);
    }
    std::vector<size_t> topoOrder;
    while (!q.empty()) {
        size_t cid = q.front(); q.pop();
        topoOrder.push_back(cid);
        for (size_t succ : sccGraph[cid]) {
            if (--indegree[succ] == 0) q.push(succ);
        }
    }

    // Rebuild nodeCycles and edgeCycles with new topo order
    nodeCycles.clear();
    edgeCycles.clear();
    nodeToCycleIndex.clear();
    edgeToCycleIndex.clear();

    std::unordered_map<size_t, size_t> oldToNewCycleId;
    for (size_t newId = 0; newId < topoOrder.size(); ++newId) {
        size_t oldId = topoOrder[newId];
        oldToNewCycleId[oldId] = newId;
        nodeCycles.push_back(rawNodeCycles[oldId]);
        edgeCycles.emplace_back();
        for (auto node : rawNodeCycles[oldId]) {
            nodeToCycleIndex[node] = newId;
        }
    }
    for (const auto& edge : graph.getEdges()) {
        NodePtr out = edge->getOutput();
        if (nodeToCycleIndex.count(out)) {
            size_t cid = nodeToCycleIndex[out];
            edgeCycles[cid].insert(edge);
            edgeToCycleIndex[edge] = cid;
        }
    }

    // Compute depth within each cycle
    for (size_t cid = 0; cid < nodeCycles.size(); ++cid) {
        const auto& cycleNodes = nodeCycles[cid];
        const auto& cycleEdges = edgeCycles[cid];
        std::queue<NodePtr> q;
        for (auto node : cycleNodes) {
            bool isEntry = false;
            for (auto& inEdge : node->getIncomingEdges()) {
                for (auto& inNode : inEdge->getInputs()) {
                    if (nodeToCycleIndex[inNode] != cid) {
                        isEntry = true;
                        break;
                    }
                }
                if (isEntry) break;
            }
            if (node->isFact || node->getIncomingEdges().empty()) isEntry = true;
            if (isEntry) {
                nodeDepths[node] = 0;
                q.push(node);
            }
        }
        while (!q.empty()) {
            NodePtr curr = q.front(); q.pop();
            size_t currDepth = nodeDepths[curr];
            for (auto& outEdge : curr->getOutgoingEdges()) {
                NodePtr out = outEdge->getOutput();
                if (nodeToCycleIndex[out] != cid) continue;
                if (!nodeDepths.count(out) || nodeDepths[out] > currDepth + 1) {
                    nodeDepths[out] = currDepth + 1;
                    q.push(out);
                }
            }
        }
        for (auto edge : cycleEdges) {
            size_t d = 0;
            for (auto in : edge->getInputs()) {
                if (nodeToCycleIndex[in] == cid && nodeDepths.count(in)) {
                    d = std::max(d, nodeDepths[in] + 1);
                }
            }
            edgeDepths[edge] = d;
        }
    }
}


int main() {
    DerivationGraph graph;

    // Create nodes
    auto Z = graph.createNode(UntypedTuple{"n", {999}});
    Z->isFact = true;
    auto A = graph.createNode(UntypedTuple{"n", {1}});
    auto B = graph.createNode(UntypedTuple{"n", {2}});
    auto C = graph.createNode(UntypedTuple{"n", {3}});
    auto D = graph.createNode(UntypedTuple{"n", {4}});
    auto E = graph.createNode(UntypedTuple{"n", {5}});

    auto e0 = graph.createHyperedge({Z}, A);
    // Create edges: A → B → C → D → E → A (cycle)
    auto e1 = graph.createHyperedge({A}, B);
    auto e2 = graph.createHyperedge({B}, C);
    auto e3 = graph.createHyperedge({C}, D);
    auto e4 = graph.createHyperedge({D}, E);
    auto e5 = graph.createHyperedge({E}, A);

    // Add another edge F → G (acyclic)
    auto F = graph.createNode(UntypedTuple{"n", {6}});
    auto G = graph.createNode(UntypedTuple{"n", {7}});
    auto e6 = graph.createHyperedge({F}, G);
    auto e7 = graph.createHyperedge({G}, F);
    auto e8 = graph.createHyperedge({A}, F);

    // Call SCC + depth analysis
    std::vector<std::unordered_set<NodePtr>> nodeCycles;
    std::vector<std::unordered_set<EdgePtr>> edgeCycles;
    std::unordered_map<NodePtr, size_t> nodeToCycleIndex;
    std::unordered_map<EdgePtr, size_t> edgeToCycleIndex;
    std::unordered_map<NodePtr, size_t> nodeDepths;
    std::unordered_map<EdgePtr, size_t> edgeDepths;

    computeSCCOrderedCyclesWithDepth(graph, nodeCycles, edgeCycles,
                                     nodeToCycleIndex, edgeToCycleIndex,
                                     nodeDepths, edgeDepths);

    std::cout << "Number of SCCs: " << nodeCycles.size() << "\n";
    for (size_t i = 0; i < nodeCycles.size(); ++i) {
        std::cout << "Cycle " << i << " nodes:\n";
        for (auto n : nodeCycles[i]) {
            std::cout << "  " << n->toString() << " (depth=" << nodeDepths[n] << ")\n";
        }
        std::cout << "Cycle " << i << " edges:\n";
        for (auto e : edgeCycles[i]) {
            std::cout << "  " << e->toString() << " (depth=" << edgeDepths[e] << ")\n";
        }
        std::cout << "----\n";
    }

    return 0;
}