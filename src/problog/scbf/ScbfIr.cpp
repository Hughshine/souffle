#include "souffle/problog/scbf/ScbfIr.h"

#include <algorithm>
#include <queue>
#include <set>
#include <sstream>
#include <unordered_set>

namespace souffle::problog::scbf {
namespace {

std::string nodeSortKey(const NodePtr& node) {
    return node ? node->getTuple().toString() + "#" + std::to_string(node->getId()) : "<null>";
}

std::vector<NodePtr> sortNodes(const std::unordered_set<NodePtr>& nodes) {
    std::vector<NodePtr> result(nodes.begin(), nodes.end());
    std::sort(result.begin(), result.end(), [](const NodePtr& a, const NodePtr& b) {
        return nodeSortKey(a) < nodeSortKey(b);
    });
    return result;
}

std::vector<NodePtr> sortNodes(const std::set<NodePtr>& nodes) {
    std::vector<NodePtr> result(nodes.begin(), nodes.end());
    std::sort(result.begin(), result.end(), [](const NodePtr& a, const NodePtr& b) {
        return nodeSortKey(a) < nodeSortKey(b);
    });
    return result;
}

std::vector<EdgePtr> sortEdges(const std::unordered_set<EdgePtr>& edges) {
    std::vector<EdgePtr> result(edges.begin(), edges.end());
    std::sort(result.begin(), result.end(), [](const EdgePtr& a, const EdgePtr& b) {
        const std::size_t aid = a ? a->getId() : 0;
        const std::size_t bid = b ? b->getId() : 0;
        if (aid != bid) {
            return aid < bid;
        }
        return (a && b) ? (a->toString() < b->toString()) : (a.get() < b.get());
    });
    return result;
}

std::vector<std::size_t> sortIds(const std::unordered_set<std::size_t>& ids) {
    std::vector<std::size_t> result(ids.begin(), ids.end());
    std::sort(result.begin(), result.end());
    return result;
}

}  // namespace

ScbfProgram buildScbfProgram(const DerivationGraphViewInterface& view) {
    ScbfProgram program;
    auto& depGraph = view.getCycleDependencyGraph();
    const std::size_t cycleCount = depGraph.nodeCycles.size();

    std::vector<std::size_t> inDegrees = depGraph.inDegrees;
    std::vector<bool> visited(cycleCount, false);
    std::queue<std::size_t> ready;
    for (std::size_t cid = 0; cid < cycleCount; ++cid) {
        if (inDegrees[cid] == 0) {
            ready.push(cid);
        }
    }

    while (!ready.empty()) {
        const std::size_t cid = ready.front();
        ready.pop();
        if (visited[cid]) {
            continue;
        }
        visited[cid] = true;
        program.topoOrderCycleIds.push_back(cid);
        for (const auto succ : depGraph.reverseDependencies[cid]) {
            if (--inDegrees[succ] == 0) {
                ready.push(succ);
            }
        }
    }

    // Defensive completion in case of malformed SCC metadata.
    for (std::size_t cid = 0; cid < cycleCount; ++cid) {
        if (!visited[cid]) {
            program.topoOrderCycleIds.push_back(cid);
        }
    }

    program.strata.reserve(program.topoOrderCycleIds.size());
    for (std::size_t topoIndex = 0; topoIndex < program.topoOrderCycleIds.size(); ++topoIndex) {
        const std::size_t cid = program.topoOrderCycleIds[topoIndex];
        ScbfStratum stratum;
        stratum.cycleId = cid;
        stratum.topoIndex = topoIndex;
        stratum.localNodes = sortNodes(depGraph.nodeCycles[cid]);
        stratum.localEdges = sortEdges(depGraph.edgeCycles[cid]);
        stratum.dependencies = sortIds(depGraph.dependencies[cid]);
        stratum.reverseDependencies = sortIds(depGraph.reverseDependencies[cid]);

        std::set<NodePtr> boundaryIn;
        std::set<NodePtr> boundaryOut;
        std::set<NodePtr> outputs;
        for (const auto& node : stratum.localNodes) {
            if (node && node->needOutput) {
                outputs.insert(node);
            }
        }
        for (const auto& edge : stratum.localEdges) {
            if (!edge) {
                continue;
            }
            const auto inputs = view.getInputs(edge);
            for (const auto& input : inputs) {
                auto it = depGraph.nodeToCycleIndex.find(input);
                if (it != depGraph.nodeToCycleIndex.end() && it->second != cid) {
                    boundaryIn.insert(input);
                }
            }
        }
        for (const auto& node : stratum.localNodes) {
            if (!node) {
                continue;
            }
            const auto& outgoing = view.getOutgoingEdges(node);
            for (const auto& edge : outgoing) {
                NodePtr out = view.getOutput(edge);
                auto it = depGraph.nodeToCycleIndex.find(out);
                if (it != depGraph.nodeToCycleIndex.end() && it->second != cid) {
                    boundaryOut.insert(node);
                    break;
                }
            }
        }

        stratum.boundaryInNodes = sortNodes(boundaryIn);
        stratum.boundaryOutNodes = sortNodes(boundaryOut);
        stratum.outputNodes = sortNodes(outputs);

        program.cycleIdToTopoIndex[cid] = topoIndex;
        for (const auto& node : stratum.localNodes) {
            program.nodeToCycleId[node] = cid;
        }
        for (const auto& edge : stratum.localEdges) {
            program.edgeToCycleId[edge] = cid;
        }
        program.strata.push_back(std::move(stratum));
    }

    return program;
}

ScbfFormulaArenaPlan buildScbfFormulaArenaPlan(
        const DerivationGraphViewInterface& view,
        const ScbfProgram& program,
        std::size_t cycleId) {
    ScbfFormulaArenaPlan plan;
    plan.cycleId = cycleId;
    auto itTopo = program.cycleIdToTopoIndex.find(cycleId);
    if (itTopo == program.cycleIdToTopoIndex.end()) {
        return plan;
    }
    const auto& stratum = program.strata[itTopo->second];

    std::set<NodePtr> targetSet;
    targetSet.insert(stratum.outputNodes.begin(), stratum.outputNodes.end());
    targetSet.insert(stratum.boundaryOutNodes.begin(), stratum.boundaryOutNodes.end());

    for (const auto& target : targetSet) {
        if (!target) {
            continue;
        }
        ScbfTargetPlan targetPlan;
        targetPlan.target = target;

        const auto& incoming = view.getIncomingEdges(target);
        std::set<std::pair<std::size_t, std::string>> importKeys;
        for (const auto& edge : incoming) {
            auto edgeCycleIt = program.edgeToCycleId.find(edge);
            if (edgeCycleIt == program.edgeToCycleId.end() || edgeCycleIt->second != cycleId) {
                continue;
            }
            targetPlan.localIncomingEdges.push_back(edge);
            const auto inputs = view.getInputs(edge);
            for (const auto& input : inputs) {
                auto nodeCycleIt = program.nodeToCycleId.find(input);
                if (nodeCycleIt == program.nodeToCycleId.end() || nodeCycleIt->second == cycleId) {
                    continue;
                }
                const auto key = std::make_pair(nodeCycleIt->second, nodeSortKey(input));
                if (importKeys.insert(key).second) {
                    targetPlan.imports.push_back(ScbfExportRef{nodeCycleIt->second, input});
                }
            }
        }

        std::sort(targetPlan.localIncomingEdges.begin(), targetPlan.localIncomingEdges.end(),
                [](const EdgePtr& a, const EdgePtr& b) { return a->getId() < b->getId(); });
        std::sort(targetPlan.imports.begin(), targetPlan.imports.end(),
                [](const ScbfExportRef& a, const ScbfExportRef& b) {
                    if (a.producerCycleId != b.producerCycleId) {
                        return a.producerCycleId < b.producerCycleId;
                    }
                    return nodeSortKey(a.node) < nodeSortKey(b.node);
                });
        plan.targets.push_back(std::move(targetPlan));
    }

    std::sort(plan.targets.begin(), plan.targets.end(), [](const ScbfTargetPlan& a, const ScbfTargetPlan& b) {
        return nodeSortKey(a.target) < nodeSortKey(b.target);
    });
    return plan;
}

bool validateScbfProgram(const DerivationGraphViewInterface& view, const ScbfProgram& program,
        std::string* errorMessage) {
    auto fail = [&](const std::string& msg) {
        if (errorMessage != nullptr) {
            *errorMessage = msg;
        }
        return false;
    };

    if (program.topoOrderCycleIds.size() != program.strata.size()) {
        return fail("topoOrderCycleIds.size != strata.size");
    }
    for (const auto& stratum : program.strata) {
        auto itTopo = program.cycleIdToTopoIndex.find(stratum.cycleId);
        if (itTopo == program.cycleIdToTopoIndex.end()) {
            return fail("missing cycleIdToTopoIndex entry");
        }
        if (itTopo->second != stratum.topoIndex) {
            return fail("inconsistent topoIndex mapping");
        }
        for (const auto& node : stratum.localNodes) {
            auto itNode = program.nodeToCycleId.find(node);
            if (itNode == program.nodeToCycleId.end() || itNode->second != stratum.cycleId) {
                return fail("nodeToCycleId mismatch");
            }
        }
        for (const auto& edge : stratum.localEdges) {
            auto itEdge = program.edgeToCycleId.find(edge);
            if (itEdge == program.edgeToCycleId.end() || itEdge->second != stratum.cycleId) {
                return fail("edgeToCycleId mismatch");
            }
            NodePtr out = view.getOutput(edge);
            auto itOut = program.nodeToCycleId.find(out);
            if (itOut == program.nodeToCycleId.end() || itOut->second != stratum.cycleId) {
                return fail("edge output assigned to different stratum");
            }
        }
        for (const auto dep : stratum.dependencies) {
            auto itDep = program.cycleIdToTopoIndex.find(dep);
            if (itDep == program.cycleIdToTopoIndex.end()) {
                return fail("dependency missing topo index");
            }
            if (itDep->second >= stratum.topoIndex) {
                return fail("dependency violates topological order");
            }
        }
        for (const auto& node : stratum.boundaryInNodes) {
            auto it = program.nodeToCycleId.find(node);
            if (it == program.nodeToCycleId.end()) {
                return fail("boundary-in node missing cycle assignment");
            }
            if (it->second == stratum.cycleId) {
                return fail("boundary-in node belongs to local stratum");
            }
        }
        for (const auto& node : stratum.boundaryOutNodes) {
            auto it = program.nodeToCycleId.find(node);
            if (it == program.nodeToCycleId.end() || it->second != stratum.cycleId) {
                return fail("boundary-out node not local");
            }
        }
    }
    return true;
}

std::string summarizeScbfProgram(const ScbfProgram& program) {
    std::size_t totalNodes = 0;
    std::size_t totalEdges = 0;
    std::size_t totalBoundaryIn = 0;
    std::size_t totalBoundaryOut = 0;
    std::size_t totalOutputs = 0;
    for (const auto& stratum : program.strata) {
        totalNodes += stratum.localNodes.size();
        totalEdges += stratum.localEdges.size();
        totalBoundaryIn += stratum.boundaryInNodes.size();
        totalBoundaryOut += stratum.boundaryOutNodes.size();
        totalOutputs += stratum.outputNodes.size();
    }
    std::ostringstream oss;
    oss << "[scbf-ir] strata=" << program.strata.size()
        << " nodes=" << totalNodes
        << " edges=" << totalEdges
        << " boundary_in=" << totalBoundaryIn
        << " boundary_out=" << totalBoundaryOut
        << " outputs=" << totalOutputs;
    return oss.str();
}

}  // namespace souffle::problog::scbf

#ifdef SOUFFLE_SCBF_IR_SMOKE_MAIN
using namespace souffle::problog;
using namespace souffle::problog::scbf;

namespace {

void require(bool cond, const std::string& msg) {
    if (!cond) {
        throw std::runtime_error(msg);
    }
}

NodePtr findNodeByRelation(const ScbfProgram& program, const std::string& relation) {
    for (const auto& [node, _] : program.nodeToCycleId) {
        if (node && node->getTuple().relation_name == relation) {
            return node;
        }
    }
    return nullptr;
}

}  // namespace

int main() {
    try {
        DerivationGraph graph;
        NodePtr f = graph.createNode(UntypedTuple{"f", {1}}, 1.0);
        f->isFact = true;
        NodePtr a = graph.createNode(UntypedTuple{"a", {1}}, 1.0);
        NodePtr b = graph.createNode(UntypedTuple{"b", {1}}, 1.0);
        NodePtr c = graph.createNode(UntypedTuple{"c", {1}}, 1.0);
        c->setQuery();

        graph.createHyperedge({f}, a);
        graph.createHyperedge({a}, b);
        graph.createHyperedge({b}, a);
        graph.createHyperedge({b}, c);

        ScbfProgram program = buildScbfProgram(graph);
        std::string err;
        require(validateScbfProgram(graph, program, &err), "validateScbfProgram failed: " + err);
        require(!program.strata.empty(), "program has no strata");
        require(program.strata.size() >= 2, "expected multiple strata");

        NodePtr nodeA = findNodeByRelation(program, "a");
        NodePtr nodeC = findNodeByRelation(program, "c");
        require(nodeA != nullptr && nodeC != nullptr, "failed to find test nodes");
        std::size_t cycleA = program.nodeToCycleId.at(nodeA);
        std::size_t cycleC = program.nodeToCycleId.at(nodeC);
        require(cycleA != cycleC, "expected a and c in different strata");

        auto planC = buildScbfFormulaArenaPlan(graph, program, cycleC);
        require(!planC.allowCrossTargetSharing, "cross-target sharing must be disabled");
        require(!planC.targets.empty(), "cycleC must expose at least one target");

        bool cHasImport = false;
        for (const auto& targetPlan : planC.targets) {
            if (!targetPlan.target || targetPlan.target->getTuple().relation_name != "c") {
                continue;
            }
            cHasImport = !targetPlan.imports.empty();
            break;
        }
        require(cHasImport, "target c should import prior-stratum exports");

        std::cout << summarizeScbfProgram(program) << "\n";
        std::cout << "[scbf-ir-smoke] ok strata=" << program.strata.size() << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "[scbf-ir-smoke] failed: " << ex.what() << "\n";
        return 1;
    }
}
#endif
