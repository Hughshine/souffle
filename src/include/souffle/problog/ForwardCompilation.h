#ifndef FORWARDCOMPILATION_H
#define FORWARDCOMPILATION_H

#include <iostream>
#include "souffle/Derivation.h"
#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/RuleManager.h"
#include "souffle/problog/ConstAnalysis.h"
#include "souffle/problog/formula/FormulaManager.h"
#include "souffle/problog/formula/LogicFormulaManager.h"
#include <queue>
#include "souffle/problog/formula/CuddManager.h"
#include <queue>
#include <set>
#include <map>
#include <memory>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <climits>
#include <type_traits>
#include <utility>
#include <fstream>
#include "souffle/problog/debug/Debugger.h"
#include "souffle/problog/RegionalIncremental.h"


Debugger& debugger = Debugger::getInstance();

inline void assertProbabilityInRange(double p, const std::string& ctx) {
//    std::cout << "[ForwardCompilation] probability check " << p << " at " << ctx << std::endl;
    if (p < 0.0 || p > 1.0) {
        std::cerr << "[ForwardCompilation] invalid probability " << p << " at " << ctx << std::endl;
        assert(false && "probability out of [0,1]");
    }
}

// Currently support CuddManager only; not optimized version
// 1. no stratum-by-stratum and cycle-by-cycle processing
// 2. no special designed logic formula manager - mainly for formula's semantic equivalence checking
template<typename FormulaNodeRef>
void buildFormulas(
    const SubgraphView& view,
    FormulaManager<FormulaNodeRef>& formulaManager,
    std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
    std::map<EdgePtr, FormulaNodeRef>& edgeFormulas
) {
    FunctionTimer timer(" forward compilation, building formulas ");
    const bool useConst = DerivationGraph::isConstFoldEnabled();
    const bool dumpConst = DerivationGraph::isConstDumpEnabled();
    ConstAnalysisResult constInfo;
    const ConstAnalysisResult* constInfoPtr = nullptr;
    if (useConst || dumpConst) {
        auto constStart = std::chrono::steady_clock::now();
        constInfo = analyzeConstants(view, true);
        auto constMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - constStart).count();
        if (useConst) {
            constInfoPtr = &constInfo;
        }
        std::cout << "[const-pre] tag=full-worklist took " << constMs << " ms" << std::endl;
        logConstAnalysis(constInfo, view, "full-worklist");
    }
    ConstFormulaAccess<FormulaNodeRef> constAccess{constInfoPtr, formulaManager};

    // Initialize formulas for input facts (nodes)
    std::map<NodePtr, FormulaNodeRef> baseNodeFormulas;
    std::map<EdgePtr, FormulaNodeRef> baseEdgeFormulas;
    for (const auto& node : view.getNodes()) {
        // Create a variable using the node's unique ID
        if (node->isFact) {
            int idx = formulaManager.getVarIndex(*node);
            if (node->getProbability() == 1.0) {
                nodeFormulas[node] = formulaManager.getTrue();
            } else {
                nodeFormulas[node] = formulaManager.createVar(idx, *node);
                formulaManager.setVariableWeight(idx, node->getProbability(), 1 - node->getProbability());
            }
            baseNodeFormulas.insert({node, nodeFormulas[node]});
        }
    }

    // Initialize formulas for rule instantiations (hyperedges)
    for (const auto& edge : view.getEdges()) {
        // Create a variable using the edge's unique ID; need to plus the size of graph.getNodes() to avoid conflict with node's id
        if (edge->isDeterministic()) {
            auto baseEdgeFormula = formulaManager.getTrue();
            baseEdgeFormulas.insert({edge, baseEdgeFormula});
        } else {
            int idx = formulaManager.getVarIndex(*edge);
            auto baseEdgeFormula = formulaManager.createVar(idx, *edge);
            formulaManager.setVariableWeight(idx, edge->getProbability(), 1 - edge->getProbability());
            baseEdgeFormulas.insert({edge, baseEdgeFormula});
        }
    }

    // Worklist algorithm
    std::queue<EdgePtr> worklist;
    std::set<EdgePtr> inWorklist; // Track edges in the worklist to avoid duplicates

    // Initialize worklist with all edges
    for (const auto& edge : view.getEdges()) {
        worklist.push(edge);
        inWorklist.insert(edge);
    }

    long long iteration = 0;
    while (!worklist.empty()) {
//        std::cout << "Iteration: " << iteration << std::endl;
//        std::cout << "Worklist size: " << worklist.size() << std::endl;
//        formulaManager.dumpProfilingStatistics();
        iteration++;
        auto edge = worklist.front();
        worklist.pop();
        inWorklist.erase(edge);
//        std::cout << "Processing edge " << edge->getId() << " " << edge->toString() << std::endl;
        // Store the old edge formula to check if it changes
        FormulaNodeRef oldEdgeFormula = edgeFormulas[edge];
//        std::cout << "Old edge formula: " << formulaManager.toString(oldEdgeFormula) << std::endl;
        // Compute new formula for the edge (conjunction of input node formulas and rule formula)
        FormulaNodeRef newEdgeFormula;
        bool edgeIsConst = constAccess.edgeFormula(edge, newEdgeFormula);
        bool allInputsAvailable = true;
        if (!edgeIsConst) {
            std::vector<FormulaNodeRef> inputFormulas;
            // Add the rule formula (the edge's base formula)
            inputFormulas.push_back(baseEdgeFormulas[edge]);

            // Add the input node formulas
            assert (view.getInputs(edge).size() == view.getBodyNegations(edge).size());
            assert (view.getInputs(edge).size() == edge->getInputs().size());
            for (size_t i = 0; i < view.getInputs(edge).size(); i++) {
                auto input = view.getInputs(edge)[i];
                auto isNegated = view.getBodyNegations(edge)[i];
                FormulaNodeRef lit;
                if (!constAccess.inputLiteral(nodeFormulas, input, isNegated, lit)) {
                    // Input node formula not available yet, skip this edge for now
                    // cannot skip, since there is cycle
                    allInputsAvailable = false;
//                std::cout << "Input node formula not available yet: " << input->getTuple().toString() << std::endl;
                    break;
                }
                inputFormulas.push_back(lit);
            }

            if (!allInputsAvailable) {
                // Put the edge back in the worklist for later processing
                worklist.push(edge);
                inWorklist.insert(edge);
                continue;
            }

            // Compute the conjunction of all input formulas
            if (inputFormulas.size() == 1) {
                newEdgeFormula = inputFormulas[0];
            } else {
                newEdgeFormula = formulaManager.makeAnd(inputFormulas);
            }
        }

        // Check if the edge formula actually changed
        bool edgeFormulaChanged = !formulaManager.isSame(oldEdgeFormula, newEdgeFormula);

        if (edgeFormulaChanged) {
            // Update the edge formula
            edgeFormulas[edge] = newEdgeFormula;
            // Update the output node formula
            auto output = view.getOutput(edge);
            // Store the old node formula to check if it changes
            FormulaNodeRef oldNodeFormula;
            bool nodeHasFormula = nodeFormulas.find(output) != nodeFormulas.end();
            if (nodeHasFormula) {
                oldNodeFormula = nodeFormulas[output];
            }

            // Compute the disjunction of all incoming edge formulas
            FormulaNodeRef newNodeFormula;
            bool hasNewNodeFormula = constAccess.nodeFormula(output, newNodeFormula);
            if (!hasNewNodeFormula) {
                // Collect formulas from all incoming edges
                std::vector<FormulaNodeRef> incomingFormulas;
                for (const auto& inEdge : view.getIncomingEdges(output)) {
                    auto it = edgeFormulas.find(inEdge);
                    if (it != edgeFormulas.end()) {
                        if (it->second.get()) {
                            // TODO: don't know why it can be NULL
                            incomingFormulas.push_back(it->second);
                        }
                    }
                }

                if (incomingFormulas.empty()) {
                    assert (false && "No incoming edge formula found for the output node");
                } else if (incomingFormulas.size() == 1) {
                    newNodeFormula = incomingFormulas[0];
                    hasNewNodeFormula = true;
                } else {
                    newNodeFormula = formulaManager.makeOr(incomingFormulas);
                    hasNewNodeFormula = true;
                }
            }

            if (!hasNewNodeFormula) {
                continue;
            }

            // Check if the node formula actually changed
            bool nodeFormulaChanged = !nodeHasFormula ||
                                     !formulaManager.isSame(oldNodeFormula, newNodeFormula);

            if (nodeFormulaChanged) {
                // Update the node formula
                nodeFormulas[output] = newNodeFormula;
                // Add outgoing edges to the worklist
                for (const auto& outEdge : view.getOutgoingEdges(output)) {
                    if (inWorklist.find(outEdge) == inWorklist.end()) {
                        worklist.push(outEdge);
                        inWorklist.insert(outEdge);
                    }
                }
            }
        }
    }
    std::cout << "Successfully build formulas" << std::endl;
}

struct PrioritizedEdge {
    EdgePtr edge;
    size_t priority;
    int sequence_id;
    bool operator<(const PrioritizedEdge& other) const {
        if (priority != other.priority)
            return priority > other.priority;  // Smaller value first
        return sequence_id > other.sequence_id;  // Smaller sequence id first
    }
};

template<typename FormulaNodeRef>
void buildFormulasCyclewise(
    SubgraphView& view,
    FormulaManager<FormulaNodeRef>& formulaManager,
    std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
    std::map<EdgePtr, FormulaNodeRef>& edgeFormulas,
    const std::unordered_set<NodePtr>& seedTrueNodes = {},
    std::vector<double>* roundTimingsMs = nullptr,
    bool allowConst = true,
    bool allowDumpConst = true
) {
     FunctionTimer timer("Build Formulas Cyclewise using DAG + Depth");
     auto toMs = [](auto d) {
         return std::chrono::duration<double, std::milli>(d).count();
     };
     auto overallStart = std::chrono::steady_clock::now();
     const bool useConst = allowConst && DerivationGraph::isConstFoldEnabled();
     const bool dumpConst = allowDumpConst && DerivationGraph::isConstDumpEnabled();
     ConstAnalysisResult constInfo;
     const ConstAnalysisResult* constInfoPtr = nullptr;
     if (useConst || dumpConst) {
         auto constStart = std::chrono::steady_clock::now();
         constInfo = analyzeConstants(view, true);
         auto constMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::steady_clock::now() - constStart).count();
         if (useConst) {
             constInfoPtr = &constInfo;
         }
         if (seedTrueNodes.empty()) {
             std::cout << "[const-pre] tag=full-cyclewise took " << constMs << " ms" << std::endl;
             logConstAnalysis(constInfo, view, "full-cyclewise");
         }
     }
     ConstFormulaAccess<FormulaNodeRef> constAccess{constInfoPtr, formulaManager};

     auto preStart = std::chrono::steady_clock::now();
     formulaManager.preConfig(view);
     auto preConfigMs = toMs(std::chrono::steady_clock::now() - preStart);
    debugger.logMessage(Level::INFO, "Variable ordering takes " + std::to_string(preConfigMs) + " ms");

    auto depStart = std::chrono::steady_clock::now();
    auto& depGraph = view.getCycleDependencyGraph();
//    depGraph.dumpCycles(std::cout);
    depGraph.dumpDot("scc.dot");
    auto depMs = toMs(std::chrono::steady_clock::now() - depStart);

    auto baseStart = std::chrono::steady_clock::now();
    std::map<NodePtr, FormulaNodeRef> baseNodeFormulas;
    std::map<EdgePtr, FormulaNodeRef> baseEdgeFormulas;
    size_t round = 0;
    auto assertProb = [](double p, const std::string& ctx) {
//        std::cout << "[ForwardCompilation] probability check " << p << " at " << ctx << std::endl;
        if (p < 0.0 || p > 1.0) {
            std::cerr << "[ForwardCompilation] invalid probability " << p << " at " << ctx << std::endl;
            assert(false && "probability out of [0,1]");
        }
    };

    // 1. Initialize formulas
    for (const auto& node : view.getNodes()) {
        if (seedTrueNodes.count(node)) {
            FormulaNodeRef var = formulaManager.getTrue();
            nodeFormulas[node] = var;
            baseNodeFormulas[node] = var;
        } else if (node->isFact) {
            int idx = formulaManager.getVarIndex(*node);
            assertProb(node->getProbability(), "fact init " + node->toString());
            FormulaNodeRef var = (node->getProbability() == 1.0)
                ? formulaManager.getTrue()
                : formulaManager.createVar(idx, *node);
            assertProbabilityInRange(node->getProbability(), "fact init " + node->toString());
            formulaManager.setVariableWeight(idx, node->getProbability(), 1 - node->getProbability());
            nodeFormulas[node] = var;
            baseNodeFormulas[node] = var;
        }

    }

    for (const auto& edge : view.getEdges()) {
        int idx = edge->isDeterministic() ? -1 : formulaManager.getVarIndex(*edge);
        FormulaNodeRef f = edge->isDeterministic()
            ? formulaManager.getTrue()
            : formulaManager.createVar(idx, *edge);
        if (!edge->isDeterministic()) {
            assertProb(edge->getProbability(), "edge init " + edge->toString());
            formulaManager.setVariableWeight(idx, edge->getProbability(), 1 - edge->getProbability());
        }
        baseEdgeFormulas[edge] = f;
    }
    auto baseInitMs = toMs(std::chrono::steady_clock::now() - baseStart);

    // 2. Schedule SCCs
    auto cycleTotalStart = std::chrono::steady_clock::now();
    std::vector<size_t> remainingInDegrees = depGraph.inDegrees;
    std::vector<bool> visited(depGraph.nodeCycles.size(), false);
    std::queue<size_t> ready;
    for (size_t i = 0; i < remainingInDegrees.size(); ++i) {
        if (remainingInDegrees[i] == 0)
            ready.push(i);
    }

    while (!ready.empty()) {
        auto cycleStart = std::chrono::steady_clock::now();
        size_t cid = ready.front(); ready.pop();
        if (visited[cid]) continue;
        visited[cid] = true;

        const auto& cycleEdges = depGraph.edgeCycles[cid];
        std::priority_queue<PrioritizedEdge> worklist;
        std::set<EdgePtr> inWorklist;
//        std::cout << "Processing cycle " << cid << ", edges: ";

        int _seqId = 0;
        for (auto edge : cycleEdges) {
            worklist.push({edge, depGraph.edgeDepthsGlobal.at(edge), _seqId++});
//            std::cout << "Adding edge " << edge->getId() << " " << edge->toString()
//                      << " with depth " << depGraph.edgeDepthsGlobal.at(edge) << " to worklist.\n";
            inWorklist.insert(edge);
        }

        // Track repeated stalls to help diagnose infinite loops.
        std::map<EdgePtr, size_t> stallCount;
        constexpr size_t kMaxStall = 100000;  // defensive cap to avoid infinite requeue
        std::set<EdgePtr> loggedFirstStall;

        while (!worklist.empty()) {
            auto roundStart = std::chrono::steady_clock::now();
            round++;
//            std::cout << "Round: " << round << std::endl;
//            std::cout << "Processing cycle " << cid << ", worklist size: " << worklist.size() << std::endl;
            formulaManager.dumpProfilingStatistics();

            EdgePtr edge = worklist.top().edge;
            size_t depth = worklist.top().priority;

//            std::cout << edge->toString() << " with depth " << depth << std::endl;
            worklist.pop();
            inWorklist.erase(edge);
//            std::cout << "Processing edge " << edge->getId() << " " << edge->toString() << std::endl;
//            std::cout << "Edge depth: " << depth << std::endl;
            FormulaNodeRef newEdgeF;
            bool edgeIsConst = constAccess.edgeFormula(edge, newEdgeF);
            bool allAvailable = true;
            if (!edgeIsConst) {
                std::vector<FormulaNodeRef> inputs = { baseEdgeFormulas[edge] };
                for (size_t i = 0; i < view.getInputs(edge).size(); ++i) {
                    NodePtr input = view.getInputs(edge)[i];
                    FormulaNodeRef lit;
                    if (!constAccess.inputLiteral(nodeFormulas, input, view.getBodyNegations(edge)[i], lit)) {
//                    std::cout << "Input node formula not available: " << input->getId() << " " << input->toString() << std::endl;
                        allAvailable = false;
                        auto& sc = stallCount[edge];
                        sc++;
                        if (loggedFirstStall.insert(edge).second || sc == 100 || sc == 1000) {
                            std::cout << "[buildFormulasCyclewise] stall edge " << edge->toString()
                                      << " missing input formula for node " << input->toString()
                                      << " (stall #" << sc << ")" << std::endl;
                        }
                        if (sc > kMaxStall) {
                            std::cout << "[buildFormulasCyclewise] giving up on edge " << edge->toString()
                                      << " after " << sc << " stalls; setting formula to False to continue."
                                      << std::endl;
                            edgeFormulas[edge] = formulaManager.getFalse();
                            allAvailable = true;  // allow propagation of False to break the cycle
                        }
                        break;
                    }
                    inputs.push_back(lit);
                }

                if (!allAvailable) {
//                std::cout << "Not all inputs available for edge " << edge->getId() << ", re-adding to worklist.\n";
                    worklist.push({edge, depGraph.edgeDepthsGlobal.at(edge), _seqId++});
                    inWorklist.insert(edge);
                    continue;
                }

                newEdgeF = (inputs.size() == 1) ? inputs[0] : formulaManager.makeAnd(inputs);
            }
            if (!formulaManager.isSame(edgeFormulas[edge], newEdgeF)) {
                edgeFormulas[edge] = newEdgeF;
                NodePtr out = view.getOutput(edge);

                FormulaNodeRef newNodeF;
                bool hasNewNodeF = constAccess.nodeFormula(out, newNodeF);
                if (!hasNewNodeF) {
                    std::vector<FormulaNodeRef> inFs;
                    for (auto& inEdge : view.getIncomingEdges(out)) {
                        auto it = edgeFormulas.find(inEdge);
                        if (it != edgeFormulas.end() && it->second.get()) {
                            inFs.push_back(it->second);
                        }
                    }

                    if (!inFs.empty()) {
                        newNodeF = (inFs.size() == 1) ? inFs[0] : formulaManager.makeOr(inFs);
                        hasNewNodeF = true;
                    }
                }

                if (hasNewNodeF) {
                    if (!nodeFormulas.count(out) || !formulaManager.isSame(nodeFormulas[out], newNodeF)) {
                        nodeFormulas[out] = newNodeF;

                        for (auto& outEdge : view.getOutgoingEdges(out)) {
                            auto it = depGraph.edgeToCycleIndex.find(outEdge);
                            if (it != depGraph.edgeToCycleIndex.end() && it->second == cid && !inWorklist.count(outEdge)) {
                                worklist.push({outEdge, depGraph.edgeDepthsGlobal.at(outEdge), _seqId++});
                                inWorklist.insert(outEdge);
                            }
                        }
                    }
                }
            }

            if (roundTimingsMs != nullptr) {
                auto roundEnd = std::chrono::steady_clock::now();
                double roundMs = std::chrono::duration<double, std::milli>(roundEnd - roundStart).count();
                roundTimingsMs->push_back(roundMs);
            }
        }

        for (auto succ : depGraph.reverseDependencies[cid]) {
            if (--remainingInDegrees[succ] == 0) {
                ready.push(succ);
            }
        }

        (void)cycleStart;  // silence unused warning if roundTimingsMs is null
    }
    auto cycleMs = toMs(std::chrono::steady_clock::now() - cycleTotalStart);

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - baseStart).count();
    formulaManager.dumpProfilingStatistics();
    for (auto& [key, value]: formulaManager.getProfilingStatistics()) {
        debugger.addInfo(key, value);
    }
    debugger.logMessage(Level::INFO, "Total rounds: " + std::to_string(round));
    debugger.logMessage(Level::INFO, "Insertion time: " + std::to_string(duration) + " ms");
    double overallMs = toMs(std::chrono::steady_clock::now() - overallStart);
    std::cout << "[buildFormulasCyclewise] timings(ms): total=" << overallMs
              << " preConfig=" << preConfigMs
              << " depGraph=" << depMs
              << " baseInit=" << baseInitMs
              << " cycles=" << cycleMs
              << " rounds=" << round
              << std::endl;

//    std::cout << "✅ buildFormulasCyclewiseNew completed using global depth info.\n";
}

struct ComponentSubgraph {
    size_t id;
    std::unordered_set<NodePtr> nodes;
    std::unordered_set<EdgePtr> edges;
};

inline std::size_t countComponentRandomVars(const ComponentSubgraph& comp) {
    std::size_t count = 0;
    for (const auto& node : comp.nodes) {
        if (node->isFact && node->getProbability() != 1.0) {
            ++count;
        }
    }
    for (const auto& edge : comp.edges) {
        if (!edge->isDeterministic()) {
            ++count;
        }
    }
    return count;
}

inline std::vector<ComponentSubgraph> buildComponentSubgraphs(const DerivationGraphViewInterface& view) {
    auto& depGraph = view.getCycleDependencyGraph();
    const size_t componentCount = depGraph.getComponentCount();
    std::vector<std::unordered_set<NodePtr>> nodesByComponent(componentCount);
    std::vector<std::unordered_set<EdgePtr>> edgesByComponent(componentCount);

    for (const auto& node : view.getNodes()) {
        size_t cid = depGraph.getComponentId(node);
        nodesByComponent[cid].insert(node);
    }
    for (const auto& edge : view.getEdges()) {
        NodePtr out = view.getOutput(edge);
        if (!out) {
            continue;
        }
        size_t cid = depGraph.getComponentId(out);
        edgesByComponent[cid].insert(edge);
    }

    std::vector<ComponentSubgraph> components;
    components.reserve(componentCount);
    for (size_t cid = 0; cid < componentCount; ++cid) {
        if (nodesByComponent[cid].empty() && edgesByComponent[cid].empty()) {
            continue;
        }
        components.push_back(ComponentSubgraph{cid, std::move(nodesByComponent[cid]),
                std::move(edgesByComponent[cid])});
    }
    return components;
}

struct SingleRandVarInfo {
    NodePtr node;
    EdgePtr edge;
    double probability = 1.0;
};

inline bool findSingleRandVar(const ComponentSubgraph& comp, SingleRandVarInfo& out) {
    std::size_t count = 0;
    out = SingleRandVarInfo{};

    for (const auto& node : comp.nodes) {
        if (node->isFact && node->getProbability() != 1.0) {
            ++count;
            if (count > 1) {
                return false;
            }
            out.node = node;
            out.edge.reset();
            out.probability = node->getProbability();
        }
    }
    for (const auto& edge : comp.edges) {
        if (!edge->isDeterministic()) {
            ++count;
            if (count > 1) {
                return false;
            }
            out.node.reset();
            out.edge = edge;
            out.probability = edge->getProbability();
        }
    }
    return count == 1;
}

struct BoolNodeRef {
    bool value = false;
    bool valid = false;
    void* get() const { return valid ? const_cast<BoolNodeRef*>(this) : nullptr; }
};

class BoolFormulaManager final : public FormulaManager<BoolNodeRef> {
public:
    BoolFormulaManager(NodePtr targetNode, EdgePtr targetEdge, bool varValue)
            : targetNode(std::move(targetNode)), targetEdge(std::move(targetEdge)), varValue(varValue) {}

    BoolNodeRef createVar(int) override {
        return markVar(nullptr, nullptr);
    }
    BoolNodeRef createVar(int, const Node& node) override {
        return markVar(&node, nullptr);
    }
    BoolNodeRef createVar(int, const Hyperedge& edge) override {
        return markVar(nullptr, &edge);
    }

    BoolNodeRef makeAnd(const BoolNodeRef& a, const BoolNodeRef& b) override {
        return BoolNodeRef{a.value && b.value, true};
    }
    BoolNodeRef makeAnd(const std::vector<BoolNodeRef>& nodes) override {
        bool value = true;
        for (const auto& node : nodes) {
            value = value && node.value;
            if (!value) break;
        }
        return BoolNodeRef{value, true};
    }
    BoolNodeRef makeOr(const BoolNodeRef& a, const BoolNodeRef& b) override {
        return BoolNodeRef{a.value || b.value, true};
    }
    BoolNodeRef makeOr(const std::vector<BoolNodeRef>& nodes) override {
        bool value = false;
        for (const auto& node : nodes) {
            value = value || node.value;
            if (value) break;
        }
        return BoolNodeRef{value, true};
    }
    BoolNodeRef makeNot(const BoolNodeRef& a) override {
        return BoolNodeRef{!a.value, true};
    }
    BoolNodeRef makeCondition(const BoolNodeRef& f, const std::vector<int>&,
            const std::vector<int>&) override {
        return f;
    }
    BoolNodeRef getTrue() override {
        return BoolNodeRef{true, true};
    }
    BoolNodeRef getFalse() override {
        return BoolNodeRef{false, true};
    }
    bool isSame(const BoolNodeRef& a, const BoolNodeRef& b) override {
        return a.valid == b.valid && a.value == b.value;
    }
    std::string toString(const BoolNodeRef& node) override {
        return node.value ? "true" : "false";
    }
    void setVariableWeight(int, double, double) override {}
    double computeWeightedModelCount(const BoolNodeRef& node) override {
        return node.value ? 1.0 : 0.0;
    }
    int getVarIndex(const Node&) override { return 0; }
    int getVarIndex(const Hyperedge&) override { return 0; }
    void printInfo(const BoolNodeRef&, const std::string&) override {}
    void dumpProfilingStatistics() override {}

    bool isValid() const { return !invalid && sawVar; }

private:
    BoolNodeRef markVar(const Node* node, const Hyperedge* edge) {
        if (sawVar) {
            invalid = true;
            return BoolNodeRef{varValue, true};
        }
        if (node != nullptr) {
            if (!targetNode || targetNode.get() != node) {
                invalid = true;
            }
        } else if (edge != nullptr) {
            if (!targetEdge || targetEdge.get() != edge) {
                invalid = true;
            }
        } else {
            invalid = true;
        }
        sawVar = true;
        return BoolNodeRef{varValue, true};
    }

    NodePtr targetNode;
    EdgePtr targetEdge;
    bool varValue = false;
    bool sawVar = false;
    bool invalid = false;
};

inline bool evaluateSingleRandComponent(
        const ComponentSubgraph& comp,
        const SingleRandVarInfo& var,
        bool varValue,
        std::unordered_map<NodePtr, bool>& nodeValues,
        long long* evalMs = nullptr) {
    BoolFormulaManager manager(var.node, var.edge, varValue);
    SubgraphView subview(comp.nodes, comp.edges);
    std::map<NodePtr, BoolNodeRef> nodeFormulas;
    std::map<EdgePtr, BoolNodeRef> edgeFormulas;
    auto start = std::chrono::steady_clock::now();
    buildFormulasCyclewise(subview, manager, nodeFormulas, edgeFormulas, {}, nullptr, true, false);
    auto end = std::chrono::steady_clock::now();
    if (evalMs) {
        *evalMs = static_cast<long long>(
                std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    }
    if (!manager.isValid()) {
        return false;
    }
    nodeValues.clear();
    nodeValues.reserve(comp.nodes.size());
    for (const auto& node : comp.nodes) {
        auto it = nodeFormulas.find(node);
        bool value = (it != nodeFormulas.end()) ? it->second.value : false;
        nodeValues.emplace(node, value);
    }
    return true;
}

template <typename ManagerT, typename FormulaRef>
struct ComponentFormulaBundle {
    size_t id;
    std::unique_ptr<ManagerT> manager;
    std::map<NodePtr, FormulaRef> nodeFormulas;
};

template <typename ManagerT, typename FormulaRef, typename ManagerFactory>
inline std::vector<ComponentFormulaBundle<ManagerT, FormulaRef>>
buildFormulasCyclewiseByComponentList(
        std::vector<ComponentSubgraph> components,
        ManagerFactory&& makeManager,
        long long* initMsTotal = nullptr,
        long long* initMsMax = nullptr) {
    std::vector<ComponentFormulaBundle<ManagerT, FormulaRef>> bundles;
    bundles.reserve(components.size());
    if (initMsTotal) *initMsTotal = 0;
    if (initMsMax) *initMsMax = 0;

    for (auto& comp : components) {
        auto compId = comp.id;
        auto nodeCount = comp.nodes.size();
        auto edgeCount = comp.edges.size();
        auto randVars = countComponentRandomVars(comp);

        SubgraphView subview(std::move(comp.nodes), std::move(comp.edges));
        auto initStart = std::chrono::steady_clock::now();
        auto manager = makeManager(subview);
        long long initMs = static_cast<long long>(
                std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::steady_clock::now() - initStart)
                        .count());
        if (initMsTotal) *initMsTotal += initMs;
        if (initMsMax) *initMsMax = std::max(*initMsMax, initMs);

        auto buildStart = std::chrono::steady_clock::now();
        std::map<NodePtr, FormulaRef> nodeFormulas;
        std::map<EdgePtr, FormulaRef> edgeFormulas;
        buildFormulasCyclewise(subview, *manager, nodeFormulas, edgeFormulas);
        auto buildMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - buildStart)
                               .count();

        std::cout << "[fc-component] id=" << compId
                  << " nodes=" << nodeCount
                  << " edges=" << edgeCount
                  << " rand_vars=" << randVars
                  << " init_ms=" << initMs
                  << " build_ms=" << buildMs
                  << " total_ms=" << (initMs + buildMs)
                  << std::endl;

        bundles.push_back(ComponentFormulaBundle<ManagerT, FormulaRef>{
                compId, std::move(manager), std::move(nodeFormulas)});
    }
    return bundles;
}

template <typename ManagerT, typename FormulaRef, typename ManagerFactory>
inline std::vector<ComponentFormulaBundle<ManagerT, FormulaRef>>
buildFormulasCyclewiseByComponent(
        const DerivationGraphViewInterface& view,
        ManagerFactory&& makeManager,
        long long* initMsTotal = nullptr,
        long long* initMsMax = nullptr) {
    return buildFormulasCyclewiseByComponentList<ManagerT, FormulaRef>(
            buildComponentSubgraphs(view), std::forward<ManagerFactory>(makeManager), initMsTotal,
            initMsMax);
}

template<typename FormulaNodeRef>
void buildFormulasInc(
    const IncrementalDerivationGraphViewInterface& view,
    FormulaManager<FormulaNodeRef>& formulaManager,
    std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
    std::map<EdgePtr, FormulaNodeRef>& edgeFormulas
) {
    auto* stage = debugger.startStage(StageKind::FORWARD_COMPILATION_INC);
//    FunctionTimer timer("forward compilation, incremental update");
    auto& deltaInsertedEdges = view.getDeltaInsertEdges();
    auto& deltaDeletedEdges = view.getDeltaDeleteEdges();
    auto& deltaInsertedNodes = view.getDeltaInsertNodes();
    auto& deltaDeletedNodes = view.getDeltaDeleteNodes();

    if (deltaInsertedEdges.empty() && deltaDeletedEdges.empty()) {
        stage->logMessage(Level::INFO, "No changes to apply, skipping incremental update\n");
        return;
    }
    const bool useConst = DerivationGraph::isConstFoldEnabled();
    const bool dumpConst = DerivationGraph::isConstDumpEnabled();
    ConstAnalysisResult constInfo;
    const ConstAnalysisResult* constInfoPtr = nullptr;
    if (useConst || dumpConst) {
        auto constStart = std::chrono::steady_clock::now();
        constInfo = analyzeConstants(view, true);
        auto constMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - constStart).count();
        if (useConst) {
            constInfoPtr = &constInfo;
        }
        std::cout << "[const-pre] tag=inc-worklist took " << constMs << " ms" << std::endl;
        logConstAnalysis(constInfo, view, "inc-worklist");
    }
    ConstFormulaAccess<FormulaNodeRef> constAccess{constInfoPtr, formulaManager};

    // deletion
    if (deltaDeletedEdges.empty()) {
        stage->logMessage(Level::INFO, "No deleted edges, skipping deletion phase\n");
    } else {
        stage->logMessage(Level::INFO, "Processing deleted edges");
        std::deque<EdgePtr> worklist;
        stage->logMessage(Level::INFO, "Change all impacted formulas to False");
        // over-delete all formulas that are impacted by the deleted edges
        {
            std::deque<EdgePtr> que(deltaDeletedEdges.begin(), deltaDeletedEdges.end());
            std::set<NodePtr> nodes;
            while (!que.empty()) {
                auto edge = que.front();
                que.pop_front();
                edgeFormulas[edge] = formulaManager.getFalse();
                if (!(deltaDeletedEdges.count(edge))) {
                    worklist.push_back(edge);  // worklist only contains impacted but not deleted edges
                }
                auto outNode = view.getOutput(edge);
                if (outNode == nullptr) continue;  // is not deleted
                nodes.insert(outNode);
                nodeFormulas[outNode] = formulaManager.getFalse();
                for (auto outEdge: view.getOutgoingEdges(outNode)) {
                    if (edgeFormulas.count(outEdge) == 0 || formulaManager.isSame(edgeFormulas[outEdge], formulaManager.getFalse())) {
                        continue;
                    }
                    que.push_back(outEdge);
                }
            }
            // for all impacted but not deleted nodes, update their formulas
            for (auto node : nodes) {
                FormulaNodeRef newNodeFormula;
                bool hasNewNodeFormula = constAccess.nodeFormula(node, newNodeFormula);
                if (!hasNewNodeFormula) {
                    std::vector<FormulaNodeRef> incoming;
                    for (auto e : view.getIncomingEdges(node)) {
                        auto it = edgeFormulas.find(e);
                        if (it != edgeFormulas.end() && it->second.get()) {
                            incoming.push_back(it->second);
                        }
                    }
                    assert (!incoming.empty());
                    newNodeFormula = formulaManager.makeOr(incoming);
                }
                nodeFormulas[node] = newNodeFormula;
            }
        }

        for (auto node : view.getDeltaDeleteNodes()) {
            nodeFormulas.erase(node);
        }
        for (auto edge : view.getDeltaDeleteEdges()) {
            edgeFormulas.erase(edge);
        }

        // TODO: can be optimized - not over-delete to False in previous step
        // TODO: re-derivation phrase
        while (!worklist.empty()) {
            auto* iteration = debugger.startIteration();
//            formulaManager.dumpProfilingStatistics();

            EdgePtr edge = worklist.front();
            worklist.pop_front();
            iteration->setInfo("edge_id", std::to_string(edge->getId()));
            iteration->setInfo("edge", edge->toString());

            // if its a deleted edge
            if (deltaDeletedEdges.count(edge)) {
                assert (false && "Deleted edge should not be in the worklist");
            }
            // a propagated edge / an unknown normal edge
            // now the edge should have its old formula (not deleted)
            FormulaNodeRef oldEdgeFormula = edgeFormulas[edge];
            // calculate the new formula
            FormulaNodeRef newEdgeFormula;
            bool edgeIsConst = constAccess.edgeFormula(edge, newEdgeFormula);
            bool allInputsAvailable = true;
            if (!edgeIsConst) {
                FormulaNodeRef baseFormula = edge->isDeterministic()
                    ? formulaManager.getTrue()
                    : formulaManager.createVar(formulaManager.getVarIndex(*edge), *edge);
                std::vector<FormulaNodeRef> inputFormulas = {baseFormula};
                const auto& inputs = view.getInputs(edge);
                const auto& negs = view.getBodyNegations(edge);

                for (size_t i = 0; i < inputs.size(); ++i) {
                    FormulaNodeRef lit;
                    if (!constAccess.inputLiteral(nodeFormulas, inputs[i], negs[i], lit)) {
                        worklist.push_back(edge);
                        allInputsAvailable = false;
                        break;
                    }
                    inputFormulas.push_back(lit);
                }
                if (!allInputsAvailable) {
                    // put the edge back in the worklist for later processing
                    iteration->logMessage(Level::INFO, "Input node formula not available yet, re-adding edge to worklist");
                    continue;
                }

                newEdgeFormula = formulaManager.makeAnd(inputFormulas);
            }
            // check if the edge formula actually changed
            if (formulaManager.isSame(oldEdgeFormula, newEdgeFormula)) {
                iteration->logMessage(Level::INFO, "Edge formula did not change, skipping edge");
                continue;
            }
            edgeFormulas[edge] = newEdgeFormula;

            NodePtr output = edge->getOutput();
            assert (output->isFact == false);
            assert (deltaDeletedNodes.count(output) == 0);

            FormulaNodeRef oldNode = nodeFormulas[output];

            FormulaNodeRef newNode;
            bool hasNewNode = constAccess.nodeFormula(output, newNode);
            if (!hasNewNode) {
                std::vector<FormulaNodeRef> incoming;
                for (auto e : view.getIncomingEdges(output)) {
                    auto it = edgeFormulas.find(e);
                    if (it != edgeFormulas.end() && it->second.get()) {
                        incoming.push_back(it->second);
                    }
                }
                assert (!incoming.empty());
                newNode = formulaManager.makeOr(incoming);
                assert (!formulaManager.isSame(newNode, formulaManager.getFalse()));
                hasNewNode = true;
            }

            if (hasNewNode && !formulaManager.isSame(oldNode, newNode)) {
                iteration->logMessage(Level::INFO, "Node formula changed, updating node");
                nodeFormulas[output] = newNode;
                for (auto outEdge : view.getOutgoingEdges(output)) {
                    if (!view.getDeltaDeleteEdges().count(outEdge)) {
                        worklist.push_back(outEdge);
                    }
                }
            } else {
                iteration->logMessage(Level::INFO, "Node formula did not change, skipping node");
            }
            debugger.endIteration();
        }
    }
    // === Insertion phase ===
    if (deltaInsertedEdges.empty()) {
        stage->logMessage(Level::INFO, "No inserted edges, skipping insertion phase\n");
        return;
    }
    stage->logMessage(Level::INFO, "Processing inserted edges\n");
    std::deque<EdgePtr> worklist;
    worklist.assign(deltaInsertedEdges.begin(), deltaInsertedEdges.end());
    for (auto node : deltaInsertedNodes) {
        if (node->isFact) {
            nodeFormulas[node] = (node->getProbability() == 1.0)
                ? formulaManager.getTrue()
//                ? formulaManager.createVar(formulaManager.getVarIndex(*node), *node)
                : formulaManager.createVar(formulaManager.getVarIndex(*node), *node);
            if (node->getProbability() != 1.0)
                assertProbabilityInRange(node->getProbability(), "inc delta fact init " + node->toString());
                assertProbabilityInRange(node->getProbability(), "inc inserted fact " + node->toString());
                formulaManager.setVariableWeight(formulaManager.getVarIndex(*node), node->getProbability(), 1 - node->getProbability());
        } else {
            nodeFormulas[node] = formulaManager.getFalse();
        }
    }
    for (auto edge : deltaInsertedEdges) {
        if (edge->isDeterministic()) {
            edgeFormulas[edge] = formulaManager.getFalse();
        } else {
            edgeFormulas[edge] = formulaManager.getFalse();
            formulaManager.createVar(formulaManager.getVarIndex(*edge), *edge);
            assertProbabilityInRange(edge->getProbability(), "inc delta edge init " + edge->toString());
            formulaManager.setVariableWeight(formulaManager.getVarIndex(*edge), edge->getProbability(), 1 - edge->getProbability());
        }
    }

    debugger.logMessage(Level::INFO, "Starting incremental update for inserted edges");

    while (!worklist.empty()) {
//        std::cout << "  Iteration: " << ++iteration
//                  << ", Worklist size: " << worklist.size() << std::endl;
        auto* iteration = debugger.startIteration();
//        formulaManager.dumpProfilingStatistics();

        EdgePtr edge = worklist.front();
        worklist.pop_front();
        FormulaNodeRef oldEdge = edgeFormulas[edge];
        // calculate the new formula
        FormulaNodeRef newEdge;
        bool edgeIsConst = constAccess.edgeFormula(edge, newEdge);
        bool allInputsAvailable = true;
        if (!edgeIsConst) {
            FormulaNodeRef baseFormula = edge->isDeterministic()
                ? formulaManager.getTrue()
                : formulaManager.createVar(formulaManager.getVarIndex(*edge), *edge);
            std::vector<FormulaNodeRef> inputFormulas = {baseFormula};
            const auto& inputs = view.getInputs(edge);
            const auto& negs = view.getBodyNegations(edge);
            for (size_t i = 0; i < inputs.size(); ++i) {
                FormulaNodeRef lit;
                if (!constAccess.inputLiteral(nodeFormulas, inputs[i], negs[i], lit)) {
//                std::cout << "Input node formula not available yet: " << inputs[i]->getTuple().toString() << std::endl;
                    allInputsAvailable = false;
                    worklist.push_back(edge);
                    break;
                }
                inputFormulas.push_back(lit);
            }
            if (!allInputsAvailable) {
                // put the edge back in the worklist for later processing
                continue;
            }
            newEdge = formulaManager.makeAnd(inputFormulas);
        }

        if (!formulaManager.isSame(oldEdge, newEdge)) {
//            std::cout << "oldEdge: " << formulaManager.toString(oldEdge) << std::endl;
//            std::cout << "newEdge: " << formulaManager.toString(newEdge) << std::endl;
            edgeFormulas[edge] = newEdge;
            NodePtr output = view.getOutput(edge);
            assert (output->isFact == false);
//            assert (deltaInsertedNodes.count(output) == 0);
            FormulaNodeRef oldNode = nodeFormulas[output];
            FormulaNodeRef newNode;
            bool hasNewNode = constAccess.nodeFormula(output, newNode);
            if (!hasNewNode) {
                std::vector<FormulaNodeRef> incoming;
                for (auto e : view.getIncomingEdges(output)) {
                    auto it = edgeFormulas.find(e);
                    if (it != edgeFormulas.end() && it->second.get()) {
                        incoming.push_back(it->second);
                    }
                }
                if (!incoming.empty()) {
                    newNode = formulaManager.makeOr(incoming);
                    hasNewNode = true;
                }
            }

            if (hasNewNode && !formulaManager.isSame(oldNode, newNode)) {
                nodeFormulas[output] = newNode;
                for (auto outEdge : view.getOutgoingEdges(output)) {
                    worklist.push_back(outEdge);
                }
            }
        }
    }
    debugger.endStage();
}

// TODO: Worklists are not depth-ordered yet.
template<typename FormulaNodeRef>
void buildFormulasIncCyclewise(
    IncrementalDerivationGraphViewInterface& view,
    FormulaManager<FormulaNodeRef>& formulaManager,
    std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
    std::map<EdgePtr, FormulaNodeRef>& edgeFormulas,
    std::set<NodePtr>& changedNodes
) {
//    formulaManager.stopDynamicOptimization();
//    FunctionTimer timer("forward compilation, incremental update (cyclewise)");
//    auto* stage = debugger.startStage(StageKind::FORWARD_COMPILATION_INC);
    using namespace std::chrono;
    static int turn = 1;
    auto start = high_resolution_clock::now();
    auto& depGraph = view.getCycleDependencyGraph();  // Includes computeSCCs, computeDependencies, computeDepths
    depGraph.dumpDot("scc" + std::to_string(turn++) + ".dot");


    const auto& deltaInsertedEdges = view.getDeltaInsertEdges();
    const auto& deltaDeletedEdges = view.getDeltaDeleteEdges();
    const auto& deltaInsertedNodes = view.getDeltaInsertNodes();
    const auto& deltaDeletedNodes = view.getDeltaDeleteNodes();
    debugger.logMessage(Level::INFO, "[inc-naive] delta counts: insNodes=" +
        std::to_string(deltaInsertedNodes.size()) + " insEdges=" +
        std::to_string(deltaInsertedEdges.size()) + " delNodes=" +
        std::to_string(deltaDeletedNodes.size()) + " delEdges=" +
        std::to_string(deltaDeletedEdges.size()));
    auto end = high_resolution_clock::now();
    debugger.logMessage(Level::INFO, "Finished building dependency graph and preparation. Time: " +
        std::to_string(duration_cast<milliseconds>(end - start).count()) + " milliseconds");
    if (deltaInsertedEdges.empty() && deltaDeletedEdges.empty()) {
        debugger.logMessage(Level::INFO, "No changes to apply, skipping incremental update");
        formulaManager.dumpProfilingStatistics();
        for (auto& [key, value]: formulaManager.getProfilingStatistics()) {
            debugger.addInfo(key, value);
        }
        return;
    }
    const bool useConst = DerivationGraph::isConstFoldEnabled();
    const bool dumpConst = DerivationGraph::isConstDumpEnabled();
    ConstAnalysisResult constInfo;
    const ConstAnalysisResult* constInfoPtr = nullptr;
    if (useConst || dumpConst) {
        auto constStart = std::chrono::steady_clock::now();
        constInfo = analyzeConstants(view, true);
        auto constMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - constStart).count();
        if (useConst) {
            constInfoPtr = &constInfo;
        }
        std::cout << "[const-pre] tag=inc-cyclewise took " << constMs << " ms" << std::endl;
        logConstAnalysis(constInfo, view, "inc-cyclewise");
    }
    ConstFormulaAccess<FormulaNodeRef> constAccess{constInfoPtr, formulaManager};
    debugger.logMessage(Level::INFO, "Starting incremental update for deleted edges");

    start = high_resolution_clock::now();
    std::map<size_t, std::priority_queue<PrioritizedEdge> > cycleWorklists;
    std::map<size_t, std::set<EdgePtr> > cycleInWorklists; // initial worklist
//    std::set<EdgePtr> inWorklist;

    debugger.logMessage(Level::INFO, "Performing deletion");
    {
        // for each deleted node, apply its neg to all its reachable edges and nodes
        // however, since we still want the optimizations for deterministic facts, we sperate them
        std::set<NodePtr> deletedFacts = view.getDeletedFacts();
        std::set<NodePtr> deletedDeterminsticFacts = view.getDeletedDeterminsticFacts();
        std::set<NodePtr> deletedNonDeterminsticFacts = view.getDeletedNonDeterministicFacts();
        for (auto deletedFact: deletedFacts) {
            assertProbabilityInRange(0.0, "deleted fact weight");
            formulaManager.setVariableWeight(formulaManager.getVarIndex(*deletedFact), 0.0, 1.0);
        }
        for (auto node : deltaDeletedNodes) {
            nodeFormulas.erase(node);
            changedNodes.insert(node);
        }

        for (auto edge: deltaDeletedEdges) {
            edgeFormulas.erase(edge);
        }

        for (auto it = nodeFormulas.begin(); it != nodeFormulas.end(); ) {
            if (view.getValidNodes().find(it->first) == view.getValidNodes().end()) {
                it = nodeFormulas.erase(it);  // remove invalid nodes
            } else {
                ++it;  // move to the next element
            }
        }

        for (auto it = edgeFormulas.begin(); it != edgeFormulas.end(); ) {
            if (view.getValidEdges().find(it->first) == view.getValidEdges().end()) {
                it = edgeFormulas.erase(it);  // remove invalid edges
            } else {
                ++it;  // move to the next element
            }
        }


        auto& nodeImpactedByDeltaDelete = view.getNodeImpactedByDeltaDelete();
        auto& edgeImpactedByDeltaDelete = view.getEdgeImpactedByDeltaDelete();

        // since we've optimized deterministic facts (which will reduce the size of formulas by a large constant factor) during full compilation,
        // we cannot simply conditioning formulas on the deleted deterministic facts since they are not in the formulas
        // we have to over-delete the formulas to False and then re-derive them
        // how to: change all impacted nodes' formulas to False first, put them into worklists, then rederive their formulas using the worklist algorithm
        start = high_resolution_clock::now();
        for (auto& deletedDeterminsticFact: deletedDeterminsticFacts) {
            // update nodes
            if (nodeImpactedByDeltaDelete.find(deletedDeterminsticFact) == nodeImpactedByDeltaDelete.end()) {
                continue;
            }
            if (edgeImpactedByDeltaDelete.find(deletedDeterminsticFact) == edgeImpactedByDeltaDelete.end()) {
                continue;
            }
            auto& impactedNodes = nodeImpactedByDeltaDelete.at(deletedDeterminsticFact);
            for (auto node: impactedNodes) {
                if (deltaDeletedNodes.count(node)) {
                    continue;  // skip deleted nodes
                }
                if (nodeFormulas.count(node) == 0 || formulaManager.isSame(nodeFormulas[node], formulaManager.getFalse())) {
                    continue;  // no need to update
                }
                if (node->isFact) {
                    continue;
                }
                nodeFormulas[node] = formulaManager.getFalse();
                changedNodes.insert(node);
            }
            auto& impactedEdges = edgeImpactedByDeltaDelete.at(deletedDeterminsticFact);
            for (auto edge: impactedEdges) {
                if (deltaDeletedEdges.count(edge)) {
                    continue;  // skip deleted edges
                }
                if (view.getOutput(edge)->isFact) {
                    continue;
                }
                edgeFormulas[edge] = formulaManager.getFalse();
                auto& worklist = cycleWorklists[depGraph.edgeToCycleIndex.at(edge)];
                auto& inWorklist = cycleInWorklists[depGraph.edgeToCycleIndex.at(edge)];
                if (inWorklist.count(edge)) {
                    continue;
                }
                worklist.push({edge, depGraph.edgeDepthsGlobal.at(edge), (int)depGraph.edgeDepthsGlobal.at(edge)});  // use depth as priority
                inWorklist.insert(edge);
            }
        }

        // update the impacted nodes' formula to the newest
        for (auto& impactedNode: changedNodes) {
            FormulaNodeRef newNode;
            bool hasNewNode = constAccess.nodeFormula(impactedNode, newNode);
            if (!hasNewNode) {
                std::vector<FormulaNodeRef> incoming;
                for (EdgePtr e : view.getIncomingEdges(impactedNode)) {
                    if (edgeFormulas.count(e) && edgeFormulas[e].get()) {
                        incoming.push_back(edgeFormulas[e]);
                    }
                }
                newNode = formulaManager.makeOr(incoming);
            }
            if (!formulaManager.isSame(nodeFormulas[impactedNode], newNode)) {
                nodeFormulas[impactedNode] = newNode;
            }
        }
        end = high_resolution_clock::now();
        debugger.logMessage(Level::INFO, "Finished over-deleting impacted formulas. Time: " +
            std::to_string(duration_cast<milliseconds>(end - start).count()) + " milliseconds");


        /** deal with deleted determinstic facts first */
        for (auto& deletedNonDeterminsticFact: deletedNonDeterminsticFacts) {
//            std::cout << "Processing deleted non-deterministic fact: " << deletedNonDeterminsticFact->toString() << std::endl;
            if (nodeImpactedByDeltaDelete.find(deletedNonDeterminsticFact) == nodeImpactedByDeltaDelete.end()) {
                continue;
            }
            if (edgeImpactedByDeltaDelete.find(deletedNonDeterminsticFact) == edgeImpactedByDeltaDelete.end()) {
                continue;
            }
            auto& impactedNodes = nodeImpactedByDeltaDelete.at(deletedNonDeterminsticFact);
            for (auto node: impactedNodes) {
                if (deltaDeletedNodes.count(node)) {
                    continue;  // skip deleted nodes
                }
                if (nodeFormulas.count(node) == 0 || formulaManager.isSame(nodeFormulas[node], formulaManager.getFalse())) {
                    continue;  // no need to update
                }
                if (node->isFact) { continue; }
                auto newNodeFormula = formulaManager.makeCondition(nodeFormulas[node], {}, {formulaManager.getVarIndex(*deletedNonDeterminsticFact)});
                if (!formulaManager.isSame(nodeFormulas[node], newNodeFormula)) {
                    nodeFormulas[node] = newNodeFormula;
                    changedNodes.insert(node);
                }
            }

            auto& impactedEdges = edgeImpactedByDeltaDelete.at(deletedNonDeterminsticFact);
            for (auto edge: impactedEdges) {
                if (deltaDeletedEdges.count(edge)) {
                    continue;  // skip deleted edges
                }
                edgeFormulas[edge] = formulaManager.makeCondition(edgeFormulas[edge], {}, {formulaManager.getVarIndex(*deletedNonDeterminsticFact)});
            }
        }
        end = high_resolution_clock::now();
        debugger.logMessage(Level::INFO, "Finished conditioning on deleted non-deterministic facts. Time: " +
            std::to_string(duration_cast<milliseconds>(end - start).count()) + " milliseconds");

        start = high_resolution_clock::now();
        // update the variable ordering for deleted non-deterministic facts
        std::set<int> deletedVarsIndex;
        for (auto node: deletedNonDeterminsticFacts) {
            auto index = formulaManager.getVarIndex(*node);
            deletedVarsIndex.insert(index);
        }
        for (auto edge: deltaDeletedEdges) {
            if (edge->isDeterministic()) continue;
            auto index = formulaManager.getVarIndex(*edge);
            deletedVarsIndex.insert(index);
        }
        debugger.logMessage(Level::INFO, "Deletion deletedVarsIndex size: " +
            std::to_string(deletedVarsIndex.size()));

        formulaManager.dumpProfilingStatistics();

        if (!deletedVarsIndex.empty()) {
            formulaManager.postprocessUselessVariables(deletedVarsIndex);
            formulaManager.dumpProfilingStatistics();
        }
        end = high_resolution_clock::now();
        debugger.logMessage(Level::INFO, "Finished updating variable ordering after deletion (non-deterministic). Time: " +
            std::to_string(duration_cast<milliseconds>(end - start).count()) + " milliseconds");



        start = high_resolution_clock::now();
        // try to rederive the formulas
        std::queue<size_t> ready;  // cycles with in-degree 0
        std::vector<bool> scheduled(depGraph.nodeCycles.size(), false);  // whether the cycle has been scheduled for insertion phase
        std::vector<size_t> inDegree = depGraph.inDegrees;
        for (size_t cid = 0; cid < depGraph.nodeCycles.size(); ++cid)
            if (inDegree[cid] == 0) {ready.push(cid); scheduled[cid] = true;}  // schedule cycles with in-degree 0

        while (!ready.empty()) {
            size_t cid = ready.front(); ready.pop();
            scheduled[cid] = true;
            auto& worklist = cycleWorklists[cid];
            int round = 0;
            int _seqId = 0;
            while (!worklist.empty()) {
                auto* iteration = debugger.startIteration();
                EdgePtr edge = worklist.top().edge; worklist.pop();
                cycleInWorklists[cid].erase(edge);
                round++;
                FormulaNodeRef newEdge;
                bool edgeIsConst = constAccess.edgeFormula(edge, newEdge);
                if (!edgeIsConst) {
                    FormulaNodeRef baseFormula = edge->isDeterministic()
                        ? formulaManager.getTrue()
                        : formulaManager.createVar(formulaManager.getVarIndex(*edge), *edge);
                    std::vector<FormulaNodeRef> inputFormulas{baseFormula};

                    const auto& inputs = view.getInputs(edge);
                    const auto& negs = view.getBodyNegations(edge);
                    bool allAvailable = true;
                    for (size_t i = 0; i < inputs.size(); ++i) {
                        FormulaNodeRef lit;
                        if (!constAccess.inputLiteral(nodeFormulas, inputs[i], negs[i], lit)) {
    //                    std::cout << "    [WAIT] Missing input: " << inputs[i]->toString() << std::endl;
                            allAvailable = false;
                            break;
                        }
                        inputFormulas.push_back(lit);
                    }

                    if (!allAvailable) {
                        worklist.push({edge, depGraph.edgeDepthsGlobal.at(edge), _seqId++});
                        cycleInWorklists[cid].insert(edge);
                        debugger.endIteration();
                        continue;
                    }

                    newEdge = formulaManager.makeAnd(inputFormulas);
                }
                if (formulaManager.isSame(edgeFormulas[edge], newEdge)) {
//                    std::cout << "    [SKIP] No change\n";
                    debugger.endIteration();
                    continue;
                }

//                std::cout << "    [CHANGE] Edge formula changed\n";
                edgeFormulas[edge] = newEdge;

                NodePtr output = view.getOutput(edge);
                if (!output || output->isFact) {
//                    std::cout << "    [SKIP] Output is null or a fact\n";
                    debugger.endIteration();
                    continue;
                }

                FormulaNodeRef newNode;
                bool hasNewNode = constAccess.nodeFormula(output, newNode);
                if (!hasNewNode) {
                    std::vector<FormulaNodeRef> incoming;
                    for (EdgePtr e : view.getIncomingEdges(output)) {
                        if (edgeFormulas.count(e) && edgeFormulas[e].get()) {
                            incoming.push_back(edgeFormulas[e]);
                        }
                    }
                    newNode = formulaManager.makeOr(incoming);
                }
                if (!formulaManager.isSame(nodeFormulas[output], newNode)) {
                    std::cout << "    [UPDATE] Node formula changed: " << output->toString() << std::endl;
                    nodeFormulas[output] = newNode;
                    changedNodes.insert(output);
                    for (EdgePtr outEdge : view.getOutgoingEdges(output)) {
                        assert (depGraph.edgeToCycleIndex.count(outEdge));
                        auto it = depGraph.edgeToCycleIndex.find(outEdge);
                        if (it->second == cid && !cycleInWorklists[cid].count(outEdge)) {
                            cycleWorklists[cid].push({outEdge, depGraph.edgeDepthsGlobal.at(outEdge), _seqId++});
                            cycleInWorklists[cid].insert(outEdge);
                        }
                    }
                }
                std::cout << "    [DONE] Edge processed\n";
                debugger.endIteration();
            }

            for (size_t succ : depGraph.reverseDependencies[cid]) {
                if (--inDegree[succ] == 0 && !scheduled[succ]) {
                    ready.push(succ);
                }
            }
        }
    }
    end = high_resolution_clock::now();
    debugger.logMessage(Level::INFO, "rederive time: " + std::to_string(duration_cast<milliseconds>(end - start).count()) + " milliseconds");
    // should reset variable ordering like information after deletion
    // should change to a light weight version?

    size_t round = 0;
    if (deltaInsertedNodes.empty() && deltaInsertedEdges.empty()) {
        start = high_resolution_clock::now();
        debugger.logMessage(Level::INFO, "No inserted edges, skipping insertion phase");
    } else {
        start = high_resolution_clock::now();
        formulaManager.preConfig(view);
        end = high_resolution_clock::now();
        debugger.logMessage(Level::INFO, "Variable reordering takes: " + std::to_string(duration_cast<milliseconds>(end - start).count()) + " milliseconds");
        start = high_resolution_clock::now();
        std::vector<size_t> inDegree = depGraph.inDegrees;
        std::queue<size_t> ready;  // cycles with in-degree 0
        std::vector<bool> scheduled(depGraph.nodeCycles.size(), false);  // whether the cycle has been scheduled for insertion phase
        size_t insertion_impacted_node_count = 0;

        // === Insertion phase ===
        std::cout << "Processing inserted edges" << std::endl;
        debugger.logMessage(Level::INFO, "Processing inserted edges");
        // initialized formulas for newly inserted nodes and edges
        for (auto node : deltaInsertedNodes) {
            if (node->isFact) {
                nodeFormulas[node] = node->getProbability() == 1.0
                    ? formulaManager.getTrue()
                    : formulaManager.createVar(formulaManager.getVarIndex(*node), *node);
                formulaManager.setVariableWeight(formulaManager.getVarIndex(*node), node->getProbability(), 1 - node->getProbability());
            } else {
                nodeFormulas[node] = formulaManager.getFalse();
            }
            changedNodes.insert(node);
        }
        // TODO
        end = high_resolution_clock::now();
        debugger.logMessage(Level::INFO, "Initialize inserted node formulas. Time: " +
            std::to_string(duration_cast<milliseconds>(end - start).count()) + " milliseconds");

        for (auto edge : deltaInsertedEdges) {
            edgeFormulas[edge] = formulaManager.getFalse();
            if (!edge->isDeterministic()) {
                formulaManager.createVar(formulaManager.getVarIndex(*edge), *edge);
                assertProbabilityInRange(edge->getProbability(), "inc inserted edge " + edge->toString());
                formulaManager.setVariableWeight(formulaManager.getVarIndex(*edge), edge->getProbability(), 1 - edge->getProbability());
            }
            assert (depGraph.edgeToCycleIndex.count(edge));
            size_t cid = depGraph.edgeToCycleIndex.at(edge);
            cycleWorklists[cid].push({edge, depGraph.edgeDepthsGlobal.at(edge), 0});
            cycleInWorklists[cid].insert(edge);
        }
        // TODO
        end = high_resolution_clock::now();
        debugger.logMessage(Level::INFO, "Initialize inserted edge formulas and worklists. Time: " +
            std::to_string(duration_cast<milliseconds>(end - start).count()) + " milliseconds");
        inDegree = depGraph.inDegrees;
        std::fill(scheduled.begin(), scheduled.end(), false);

        assert (ready.empty());  // should be empty after deletion phase

        for (size_t cid = 0; cid < depGraph.nodeCycles.size(); ++cid)
            if (inDegree[cid] == 0) {ready.push(cid); scheduled[cid] = true;}  // schedule cycles with in-degree 0
        end = high_resolution_clock::now();
        debugger.logMessage(Level::INFO, "Preparation for insertion phase. Time: " +
            std::to_string(duration_cast<milliseconds>(end - start).count()) + " milliseconds");
        while (!ready.empty()) {
            size_t cid = ready.front(); ready.pop();
            scheduled[cid] = true;
            auto& worklist = cycleWorklists[cid];
            auto& inWorklist = cycleInWorklists[cid];
            // note that this worklist only contains impacted nodes; however,
    //        std::cout << "[CYCLE " << cid << "] Begin Insertion Phase, worklist size: " << worklist.size() << std::endl;
            int _seqId = 1;
//            auto start_cycle = high_resolution_clock::now();
            while (!worklist.empty()) {
//                auto* iteration = debugger.startIteration();
                EdgePtr edge = worklist.top().edge;
                size_t depth = worklist.top().priority;
                worklist.pop();
                inWorklist.erase(edge);
                round++;
                std::cout << "  [INSERTION ROUND " << round << "] Cycle " << cid
                          << ", Worklist size: " << worklist.size() << std::endl;
//                std::cout << edge->toString() << " with depth " << depth << std::endl;

                const auto& inputs = view.getInputs(edge);
                const auto& negs = view.getBodyNegations(edge);
                FormulaNodeRef newEdge;
                bool edgeIsConst = constAccess.edgeFormula(edge, newEdge);
                bool allAvailable = true;
                if (!edgeIsConst) {
                    FormulaNodeRef baseFormula = edge->isDeterministic()
                        ? formulaManager.getTrue()
                        : formulaManager.createVar(formulaManager.getVarIndex(*edge), *edge);
                    std::vector<FormulaNodeRef> inputFormulas{baseFormula};
                    for (size_t i = 0; i < inputs.size(); ++i) {
                        FormulaNodeRef lit;
                        if (!constAccess.inputLiteral(nodeFormulas, inputs[i], negs[i], lit)) {
    //                    std::cout << "    [WAIT] Missing input: " << inputs[i]->toString() << std::endl;
                            allAvailable = false;
                            break;
                        }
                        inputFormulas.push_back(lit);
                    }

                    if (!allAvailable) {
                        worklist.push({edge, depGraph.edgeDepthsGlobal.at(edge), _seqId++});
                        inWorklist.insert(edge);
                        std::cout << "    [REQUEUE] Missing input for edge " << edge->toString()
                                  << ", back to worklist (cycle " << cid
                                  << ", new worklist size=" << worklist.size() << ")\n";
//                    debugger.endIteration();
                        continue;
                    }

                    newEdge = formulaManager.makeAnd(inputFormulas);
                }
                if (!edgeFormulas[edge].get()) {
                    std::cout << "    [EDGE-OLD-NULL] " << edge->toString() << " old formula is null\n";
                }
                if (!newEdge.get()) {
                    std::cout << "    [EDGE-NEW-NULL] " << edge->toString() << " new formula is null\n";
                }
                if (formulaManager.isSame(edgeFormulas[edge], newEdge)) {
                    std::cout << "    [SKIP] Edge formula unchanged for " << edge->toString()
                              << " (cycle " << cid << ")\n";
    //                std::cout << "    [SKIP] No change\n";
//                    debugger.endIteration();
                    continue;
                }

    //            std::cout << "    [CHANGE] Edge formula changed\n";
                std::cout << "    [EDGE-UPDATE] Formula changed for " << edge->toString()
                          << " (cycle " << cid << ")"
                          << " var=" << formulaManager.getVarIndex(*edge);
                std::cout << " inputs{";
                for (size_t i = 0; i < inputs.size(); ++i) {
                    if (i) std::cout << ",";
                    std::cout << formulaManager.getVarIndex(*inputs[i]);
                }
                std::cout << "} newPtr=" << reinterpret_cast<const void*>(newEdge.get()) << "\n";
                edgeFormulas[edge] = newEdge;
                if (formulaManager.isSame(newEdge, formulaManager.getFalse())) {
                    std::cout << "    [EDGE-FALSE] " << edge->toString() << " became False\n";
                } else if (formulaManager.isSame(newEdge, formulaManager.getTrue())) {
                    std::cout << "    [EDGE-TRUE] " << edge->toString() << " became True\n";
                }

                NodePtr output = view.getOutput(edge);
                if (!output || output->isFact) {
                    std::cout << "    [IGNORE] Output missing or fact for " << edge->toString()
                              << " (cycle " << cid << ")\n";
//                    debugger.endIteration();
                    continue;
                }

                FormulaNodeRef newNode;
                bool hasNewNode = constAccess.nodeFormula(output, newNode);
                if (!hasNewNode) {
                    std::vector<FormulaNodeRef> incoming;
                    for (EdgePtr e : view.getIncomingEdges(output)) {
                        if (edgeFormulas.count(e) && edgeFormulas[e].get()) {
                            incoming.push_back(edgeFormulas[e]);
                        }
                    }
                    newNode = formulaManager.makeOr(incoming);
                }
                if (!nodeFormulas[output].get()) {
                    std::cout << "    [NODE-OLD-NULL] " << output->toString() << " old formula is null\n";
                }
                if (!newNode.get()) {
                    std::cout << "    [NODE-NEW-NULL] " << output->toString() << " new formula is null\n";
                }
                if (!formulaManager.isSame(nodeFormulas[output], newNode)) {
    //                std::cout << "    [UPDATE] Node formula changed: " << output->toString() << std::endl;
                    nodeFormulas[output] = newNode;
                    std::cout << "    [NODE-UPDATE] " << output->toString()
                              << " newPtr=" << reinterpret_cast<const void*>(newNode.get()) << "\n";
                    if (formulaManager.isSame(newNode, formulaManager.getFalse())) {
                        std::cout << "    [NODE-FALSE] " << output->toString() << " became False\n";
                    } else if (formulaManager.isSame(newNode, formulaManager.getTrue())) {
                        std::cout << "    [NODE-TRUE] " << output->toString() << " became True\n";
                    }
                    changedNodes.insert(output);
                    for (EdgePtr outEdge : view.getOutgoingEdges(output)) {
                        auto it = depGraph.edgeToCycleIndex.find(outEdge);
                        assert (depGraph.edgeToCycleIndex.count(outEdge));
                        auto outCid = it->second;
                        if (!cycleInWorklists[outCid].count(outEdge)) {
                            cycleWorklists[outCid].push({outEdge, depGraph.edgeDepthsGlobal.at(outEdge), _seqId++});
                            cycleInWorklists[outCid].insert(outEdge);
                            std::cout << "    [ENQUEUE] Node formula changed for "
                                      << output->toString()
                                      << " → enqueue outgoing edge " << outEdge->toString()
                                      << " (cycle " << outCid
                                      << ", new worklist size=" << cycleWorklists[outCid].size()
                                      << ")\n";
                        } else {
                            std::cout << "    [ENQUEUE-SKIP] Outgoing edge already queued: "
                                      << outEdge->toString()
                                      << " (cycle " << outCid
                                      << ", worklist size=" << cycleWorklists[outCid].size()
                                      << ")\n";
                        }
                    }
                } else {
                    std::cout << "    [NODE-UNCHANGED] " << output->toString()
                              << " formula unchanged (cycle " << cid
                              << ", outgoing edges=" << view.getOutgoingEdges(output).size()
                              << ")\n";
                }
            }

            for (size_t succ : depGraph.reverseDependencies[cid]) {
                if (--inDegree[succ] == 0 && !scheduled[succ]) {
                    ready.push(succ);
                }
            }
//            auto end_cycle = high_resolution_clock::now();
//            debugger.logMessage(Level::INFO, "[CYCLE " + std::to_string(cid) + "] Insertion phase completed. Time: " +
//                std::to_string(duration_cast<microseconds>(end_cycle - start_cycle).count()) + " microseconds");
        }
    }
    end = high_resolution_clock::now();
    debugger.logMessage(Level::INFO, "Insertion time: " + std::to_string(duration_cast<milliseconds>(end - start).count()) + " milliseconds");
    debugger.logMessage(Level::INFO, "Iteration rounds: " + std::to_string(round));
    formulaManager.dumpProfilingStatistics();
    for (auto& [key, value]: formulaManager.getProfilingStatistics()) {
        debugger.addInfo(key, value);
    }
    debugger.addInfo("changed_node_count", std::to_string(changedNodes.size()));


}


template<typename FormulaNodeRef>
void buildFormulasCyclewiseOnDemand(
    SubgraphView& view,
    FormulaManager<FormulaNodeRef>& formulaManager,
    std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
    std::map<EdgePtr, FormulaNodeRef>& edgeFormulas
) {
     FunctionTimer timer("Build Formulas Cyclewise using DAG + Depth");
     auto start = std::chrono::high_resolution_clock::now();
     const bool useConst = DerivationGraph::isConstFoldEnabled();
     const bool dumpConst = DerivationGraph::isConstDumpEnabled();
     ConstAnalysisResult constInfo;
     const ConstAnalysisResult* constInfoPtr = nullptr;
     if (useConst || dumpConst) {
         auto constStart = std::chrono::steady_clock::now();
         constInfo = analyzeConstants(view, true);
         auto constMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::steady_clock::now() - constStart).count();
         if (useConst) {
             constInfoPtr = &constInfo;
         }
         std::cout << "[const-pre] tag=full-ondemand took " << constMs << " ms" << std::endl;
         logConstAnalysis(constInfo, view, "full-ondemand");
     }
     ConstFormulaAccess<FormulaNodeRef> constAccess{constInfoPtr, formulaManager};
     formulaManager.preConfig(view);
     auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    debugger.logMessage(Level::INFO, "Variable ordering takes " + std::to_string(duration) + " ms");

    for (auto node : view.getNodes()) {
        node->tmpRefCount = node->currentRefCount;
    }
    for (auto edge : view.getEdges()) {
        edge->tmpRefCount = edge->currentRefCount;
    }

    auto& depGraph = view.getCycleDependencyGraph();
//    depGraph.dumpCycles(std::cout);
    depGraph.dumpDot("scc.dot");

    start = std::chrono::high_resolution_clock::now();
    std::map<NodePtr, FormulaNodeRef> baseNodeFormulas;
    std::map<EdgePtr, FormulaNodeRef> baseEdgeFormulas;
    size_t round = 0;
    // 1. Initialize formulas
    for (const auto& node : view.getNodes()) {
        if (node->isFact) {
            FormulaNodeRef var = (node->getProbability() == 1.0)
                ? formulaManager.getTrue()
//                ? formulaManager.createVar(formulaManager.getVarIndex(*node), *node)
                : formulaManager.createVar(formulaManager.getVarIndex(*node), *node);
            assertProbabilityInRange(node->getProbability(), "inc cycle node " + node->toString());
            formulaManager.setVariableWeight(formulaManager.getVarIndex(*node), node->getProbability(), 1 - node->getProbability());
            nodeFormulas[node] = var;
            baseNodeFormulas[node] = var;
        }
    }

    for (const auto& edge : view.getEdges()) {
        FormulaNodeRef f = edge->isDeterministic()
            ? formulaManager.getTrue()
            : formulaManager.createVar(formulaManager.getVarIndex(*edge), *edge);
        if (!edge->isDeterministic()) {
            assertProbabilityInRange(edge->getProbability(), "inc cycle edge " + edge->toString());
            formulaManager.setVariableWeight(formulaManager.getVarIndex(*edge), edge->getProbability(), 1 - edge->getProbability());
        }
        baseEdgeFormulas[edge] = f;
    }

    // 2. Schedule SCCs
    std::vector<size_t> remainingInDegrees = depGraph.inDegrees;
    std::vector<bool> visited(depGraph.nodeCycles.size(), false);
    std::queue<size_t> ready;
    for (size_t i = 0; i < remainingInDegrees.size(); ++i) {
        if (remainingInDegrees[i] == 0)
            ready.push(i);
    }

    while (!ready.empty()) {
        size_t cid = ready.front(); ready.pop();
        if (visited[cid]) continue;
        visited[cid] = true;

        const auto& cycleEdges = depGraph.edgeCycles[cid];
        std::priority_queue<PrioritizedEdge> worklist;
        std::set<EdgePtr> inWorklist;
//        std::cout << "Processing cycle " << cid << ", edges: ";

        int _seqId = 0;
        for (auto edge : cycleEdges) {
            worklist.push({edge, depGraph.edgeDepthsGlobal.at(edge), _seqId++});
//            std::cout << "Adding edge " << edge->getId() << " " << edge->toString()
//                      << " with depth " << depGraph.edgeDepthsGlobal.at(edge) << " to worklist.\n";
            inWorklist.insert(edge);
        }

        while (!worklist.empty()) {
            round++;
//            std::cout << "Round: " << round << std::endl;
//            std::cout << "Processing cycle " << cid << ", worklist size: " << worklist.size() << std::endl;

//            formulaManager.dumpProfilingStatistics();

            EdgePtr edge = worklist.top().edge;
            size_t depth = worklist.top().priority;

//            std::cout << edge->toString() << " with depth " << depth << std::endl;
            worklist.pop();
            inWorklist.erase(edge);
//            std::cout << "Processing edge " << edge->getId() << " " << edge->toString() << std::endl;
//            std::cout << "Edge depth: " << depth << std::endl;
            FormulaNodeRef newEdgeF;
            bool edgeIsConst = constAccess.edgeFormula(edge, newEdgeF);
            bool allAvailable = true;
            if (!edgeIsConst) {
                std::vector<FormulaNodeRef> inputs = { baseEdgeFormulas[edge] };

                for (size_t i = 0; i < view.getInputs(edge).size(); ++i) {
                    NodePtr input = view.getInputs(edge)[i];
                    FormulaNodeRef lit;
                    if (!constAccess.inputLiteral(nodeFormulas, input, view.getBodyNegations(edge)[i], lit)) {
//                    std::cout << "Input node formula not available: " << input->getId() << " " << input->toString() << std::endl;
                        allAvailable = false;
                        break;
                    }
                    inputs.push_back(lit);
                }

                if (!allAvailable) {
//                std::cout << "Not all inputs available for edge " << edge->getId() << ", re-adding to worklist.\n";
                    worklist.push({edge, depGraph.edgeDepthsGlobal.at(edge), _seqId++});
                    inWorklist.insert(edge);
                    continue;
                }

                newEdgeF = (inputs.size() == 1) ? inputs[0] : formulaManager.makeAnd(inputs);
            }
            if (!formulaManager.isSame(edgeFormulas[edge], newEdgeF)) {
                edgeFormulas[edge] = newEdgeF;
                NodePtr out = view.getOutput(edge);

                FormulaNodeRef newNodeF;
                bool hasNewNodeF = constAccess.nodeFormula(out, newNodeF);
                if (!hasNewNodeF) {
                    std::vector<FormulaNodeRef> inFs;
                    for (auto& inEdge : view.getIncomingEdges(out)) {
                        auto it = edgeFormulas.find(inEdge);
                        if (it != edgeFormulas.end() && it->second.get()) {
                            inFs.push_back(it->second);
                        }
                    }

                    if (!inFs.empty()) {
                        newNodeF = (inFs.size() == 1) ? inFs[0] : formulaManager.makeOr(inFs);
                        hasNewNodeF = true;
                    }
                }

                if (hasNewNodeF) {
                    if (!nodeFormulas.count(out) || !formulaManager.isSame(nodeFormulas[out], newNodeF)) {
                        nodeFormulas[out] = newNodeF;

                        for (auto& outEdge : view.getOutgoingEdges(out)) {
                            auto it = depGraph.edgeToCycleIndex.find(outEdge);
                            if (it != depGraph.edgeToCycleIndex.end() && it->second == cid && !inWorklist.count(outEdge)) {
                                worklist.push({outEdge, depGraph.edgeDepthsGlobal.at(outEdge), _seqId++});
                                inWorklist.insert(outEdge);
                            }
                        }
                    }
                }
            }
        }
        // reduce reference counts for every output node of the cycle
        // reduce useless formulas
        std::set<NodePtr> outputNodes;
        for (auto node : depGraph.nodeCycles[cid]) {
            if (node->needOutput) {
                outputNodes.insert(node);
            }
        }
        for (auto node : outputNodes) {
            std::queue<NodePtr> q;
            std::unordered_set<NodePtr> visited;
            auto prob = formulaManager.computeWeightedModelCount(nodeFormulas[node]);
            probResult[node] = prob;
            q.push(node);
            while (!q.empty()) {
                auto qnode = q.front(); q.pop(); visited.insert(qnode);
                if (qnode->isFact) continue;
                qnode->tmpRefCount -= 1;
                if (qnode->tmpRefCount <= 0) {
                    nodeFormulas.erase(qnode);
//                    std::cout << "Reduced node formula for node " << qnode->toString() << std::endl;
                }
                for (auto inEdge : view.getIncomingEdges(qnode)) {
                    inEdge->tmpRefCount -= 1;
                    if (inEdge->tmpRefCount <= 0) {
                        edgeFormulas.erase(inEdge);
                    }
//                    std::cout << "Reduced edge formula for edge " << inEdge->toString() << std::endl;
                    for (auto inNode: view.getInputs(inEdge)) {
                        if (visited.count(inNode) == 0 && qnode->tmpRefCount > 0) {
                            q.push(inNode);
                        }
                    }
                }
            }
        }
        formulaManager.tryGarbageCollection();

        for (auto succ : depGraph.reverseDependencies[cid]) {
            if (--remainingInDegrees[succ] == 0) {
                ready.push(succ);
            }
        }
    }
    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    formulaManager.dumpProfilingStatistics();
    for (auto& [key, value]: formulaManager.getProfilingStatistics()) {
        debugger.addInfo(key, value);
    }
    debugger.logMessage(Level::INFO, "Total rounds: " + std::to_string(round));
    debugger.logMessage(Level::INFO, "Insertion time: " + std::to_string(duration) + " ms");

//    for (const auto& [node, bdd] : nodeFormulas) {
//        auto prob = formulaManager.computeWeightedModelCount(bdd);
//        probResult[node] = prob;
//    }
}

// Placeholder for the regional incremental pipeline. Currently delegates to the
// existing incremental cyclewise implementation to keep behavior unchanged.
template<typename FormulaNodeRef>
void buildFormulasIncRegionalCyclewise(
    IncrementalDerivationGraphViewInterface& view,
    FormulaManager<FormulaNodeRef>& formulaManager,
    std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
    std::map<EdgePtr, FormulaNodeRef>& edgeFormulas,
    std::set<NodePtr>& changedNodes
) {
    debugger.logMessage(Level::INFO, "[inc-regional] start pipeline");
    using namespace std::chrono;
    const auto& deltaInsertedEdges = view.getDeltaInsertEdges();
    const auto& deltaDeletedEdges = view.getDeltaDeleteEdges();
    const auto& deltaInsertedNodes = view.getDeltaInsertNodes();
    const auto& deltaDeletedNodes = view.getDeltaDeleteNodes();
    debugger.logMessage(Level::INFO, "[inc-regional] delta counts: insNodes=" +
        std::to_string(deltaInsertedNodes.size()) + " insEdges=" +
        std::to_string(deltaInsertedEdges.size()) + " delNodes=" +
        std::to_string(deltaDeletedNodes.size()) + " delEdges=" +
        std::to_string(deltaDeletedEdges.size()));

    if (deltaInsertedEdges.empty() && deltaInsertedNodes.empty() &&
            deltaDeletedEdges.empty() && deltaDeletedNodes.empty()) {
        debugger.logMessage(Level::INFO, "No changes to apply, skipping incremental update");
        formulaManager.dumpProfilingStatistics();
        for (auto& [key, value]: formulaManager.getProfilingStatistics()) {
            debugger.addInfo(key, value);
        }
        debugger.logMessage(Level::INFO, "[inc-regional] pipeline finished; usedFallback=false");
        return;
    }
    const bool useConst = DerivationGraph::isConstFoldEnabled();
    const bool dumpConst = DerivationGraph::isConstDumpEnabled();
    ConstAnalysisResult constInfo;
    const ConstAnalysisResult* constInfoPtr = nullptr;
    if (useConst || dumpConst) {
        auto constStart = std::chrono::steady_clock::now();
        constInfo = analyzeConstants(view, true);
        auto constMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - constStart).count();
        if (useConst) {
            constInfoPtr = &constInfo;
        }
        std::cout << "[const-pre] tag=inc-regional took " << constMs << " ms" << std::endl;
        logConstAnalysis(constInfo, view, "inc-regional");
    }
    ConstFormulaAccess<FormulaNodeRef> constAccess{constInfoPtr, formulaManager};

    if (!deltaDeletedEdges.empty() || !deltaDeletedNodes.empty()) {
        auto start = high_resolution_clock::now();
        auto& depGraph = view.getCycleDependencyGraph();  // includes SCC/dependencies/depths
        auto end = high_resolution_clock::now();
        debugger.logMessage(Level::INFO, "Finished building dependency graph and preparation. Time: " +
            std::to_string(duration_cast<milliseconds>(end - start).count()) + " milliseconds");

        debugger.logMessage(Level::INFO, "[inc-regional] Performing deletion");
        start = high_resolution_clock::now();
        std::map<size_t, std::priority_queue<PrioritizedEdge> > cycleWorklists;
        std::map<size_t, std::set<EdgePtr> > cycleInWorklists;

        std::set<NodePtr> deletedFacts = view.getDeletedFacts();
        std::set<NodePtr> deletedDeterminsticFacts = view.getDeletedDeterminsticFacts();
        std::set<NodePtr> deletedNonDeterminsticFacts = view.getDeletedNonDeterministicFacts();
        for (auto deletedFact: deletedFacts) {
            assertProbabilityInRange(0.0, "deleted fact weight");
            formulaManager.setVariableWeight(formulaManager.getVarIndex(*deletedFact), 0.0, 1.0);
        }
        for (auto node : deltaDeletedNodes) {
            nodeFormulas.erase(node);
            changedNodes.insert(node);
        }

        for (auto edge: deltaDeletedEdges) {
            edgeFormulas.erase(edge);
        }

        for (auto it = nodeFormulas.begin(); it != nodeFormulas.end(); ) {
            if (view.getValidNodes().find(it->first) == view.getValidNodes().end()) {
                it = nodeFormulas.erase(it);
            } else {
                ++it;
            }
        }

        for (auto it = edgeFormulas.begin(); it != edgeFormulas.end(); ) {
            if (view.getValidEdges().find(it->first) == view.getValidEdges().end()) {
                it = edgeFormulas.erase(it);
            } else {
                ++it;
            }
        }

        auto& nodeImpactedByDeltaDelete = view.getNodeImpactedByDeltaDelete();
        auto& edgeImpactedByDeltaDelete = view.getEdgeImpactedByDeltaDelete();

        start = high_resolution_clock::now();
        for (auto& deletedDeterminsticFact: deletedDeterminsticFacts) {
            if (nodeImpactedByDeltaDelete.find(deletedDeterminsticFact) == nodeImpactedByDeltaDelete.end()) {
                continue;
            }
            if (edgeImpactedByDeltaDelete.find(deletedDeterminsticFact) == edgeImpactedByDeltaDelete.end()) {
                continue;
            }
            auto& impactedNodes = nodeImpactedByDeltaDelete.at(deletedDeterminsticFact);
            for (auto node: impactedNodes) {
                if (deltaDeletedNodes.count(node)) {
                    continue;
                }
                if (nodeFormulas.count(node) == 0 || formulaManager.isSame(nodeFormulas[node], formulaManager.getFalse())) {
                    continue;
                }
                if (node->isFact) {
                    continue;
                }
                nodeFormulas[node] = formulaManager.getFalse();
                changedNodes.insert(node);
            }
            auto& impactedEdges = edgeImpactedByDeltaDelete.at(deletedDeterminsticFact);
            for (auto edge: impactedEdges) {
                if (deltaDeletedEdges.count(edge)) {
                    continue;
                }
                if (view.getOutput(edge)->isFact) {
                    continue;
                }
                edgeFormulas[edge] = formulaManager.getFalse();
                auto& worklist = cycleWorklists[depGraph.edgeToCycleIndex.at(edge)];
                auto& inWorklist = cycleInWorklists[depGraph.edgeToCycleIndex.at(edge)];
                if (inWorklist.count(edge)) {
                    continue;
                }
                worklist.push({edge, depGraph.edgeDepthsGlobal.at(edge), (int)depGraph.edgeDepthsGlobal.at(edge)});
                inWorklist.insert(edge);
            }
        }

        for (auto& impactedNode: changedNodes) {
            FormulaNodeRef newNode;
            bool hasNewNode = constAccess.nodeFormula(impactedNode, newNode);
            if (!hasNewNode) {
                std::vector<FormulaNodeRef> incoming;
                for (EdgePtr e : view.getIncomingEdges(impactedNode)) {
                    if (edgeFormulas.count(e) && edgeFormulas[e].get()) {
                        incoming.push_back(edgeFormulas[e]);
                    }
                }
                assert (!incoming.empty());
                newNode = formulaManager.makeOr(incoming);
                hasNewNode = true;
            }
            if (hasNewNode && !formulaManager.isSame(nodeFormulas[impactedNode], newNode)) {
                nodeFormulas[impactedNode] = newNode;
            }
        }
        end = high_resolution_clock::now();
        debugger.logMessage(Level::INFO, "Finished over-deleting impacted formulas. Time: " +
            std::to_string(duration_cast<milliseconds>(end - start).count()) + " milliseconds");

        for (auto& deletedNonDeterminsticFact: deletedNonDeterminsticFacts) {
            if (nodeImpactedByDeltaDelete.find(deletedNonDeterminsticFact) == nodeImpactedByDeltaDelete.end()) {
                continue;
            }
            if (edgeImpactedByDeltaDelete.find(deletedNonDeterminsticFact) == edgeImpactedByDeltaDelete.end()) {
                continue;
            }
            auto& impactedNodes = nodeImpactedByDeltaDelete.at(deletedNonDeterminsticFact);
            for (auto node: impactedNodes) {
                if (deltaDeletedNodes.count(node)) {
                    continue;
                }
                if (nodeFormulas.count(node) == 0 || formulaManager.isSame(nodeFormulas[node], formulaManager.getFalse())) {
                    continue;
                }
                if (node->isFact) { continue; }
                auto newNodeFormula = formulaManager.makeCondition(nodeFormulas[node], {}, {formulaManager.getVarIndex(*deletedNonDeterminsticFact)});
                if (!formulaManager.isSame(nodeFormulas[node], newNodeFormula)) {
                    nodeFormulas[node] = newNodeFormula;
                    changedNodes.insert(node);
                }
            }

            auto& impactedEdges = edgeImpactedByDeltaDelete.at(deletedNonDeterminsticFact);
            for (auto edge: impactedEdges) {
                if (deltaDeletedEdges.count(edge)) {
                    continue;
                }
                edgeFormulas[edge] = formulaManager.makeCondition(edgeFormulas[edge], {}, {formulaManager.getVarIndex(*deletedNonDeterminsticFact)});
            }
        }
        end = high_resolution_clock::now();
        debugger.logMessage(Level::INFO, "Finished conditioning on deleted non-deterministic facts. Time: " +
            std::to_string(duration_cast<milliseconds>(end - start).count()) + " milliseconds");

        start = high_resolution_clock::now();
        std::set<int> deletedVarsIndex;
        for (auto node: deletedNonDeterminsticFacts) {
            auto index = formulaManager.getVarIndex(*node);
            deletedVarsIndex.insert(index);
        }
        for (auto edge: deltaDeletedEdges) {
            if (edge->isDeterministic()) continue;
            auto index = formulaManager.getVarIndex(*edge);
            deletedVarsIndex.insert(index);
        }
        debugger.logMessage(Level::INFO, "Deletion deletedVarsIndex size: " +
            std::to_string(deletedVarsIndex.size()));
        formulaManager.dumpProfilingStatistics();
        if (!deletedVarsIndex.empty()) {
            formulaManager.postprocessUselessVariables(deletedVarsIndex);
            formulaManager.dumpProfilingStatistics();
        }
        end = high_resolution_clock::now();
        debugger.logMessage(Level::INFO, "Finished updating variable ordering after deletion (non-deterministic). Time: " +
            std::to_string(duration_cast<milliseconds>(end - start).count()) + " milliseconds");

        start = high_resolution_clock::now();
        std::queue<size_t> ready;
        std::vector<bool> scheduled(depGraph.nodeCycles.size(), false);
        std::vector<size_t> inDegree = depGraph.inDegrees;
        for (size_t cid = 0; cid < depGraph.nodeCycles.size(); ++cid)
            if (inDegree[cid] == 0) {ready.push(cid); scheduled[cid] = true;}

        while (!ready.empty()) {
            size_t cid = ready.front(); ready.pop();
            scheduled[cid] = true;
            auto& worklist = cycleWorklists[cid];
            int round = 0;
            int _seqId = 0;
            while (!worklist.empty()) {
                auto* iteration = debugger.startIteration();
                EdgePtr edge = worklist.top().edge; worklist.pop();
                cycleInWorklists[cid].erase(edge);
                round++;
                FormulaNodeRef newEdge;
                bool edgeIsConst = constAccess.edgeFormula(edge, newEdge);
                bool allAvailable = true;
                if (!edgeIsConst) {
                    FormulaNodeRef baseFormula = edge->isDeterministic()
                        ? formulaManager.getTrue()
                        : formulaManager.createVar(formulaManager.getVarIndex(*edge), *edge);
                    std::vector<FormulaNodeRef> inputFormulas{baseFormula};

                    const auto& inputs = view.getInputs(edge);
                    const auto& negs = view.getBodyNegations(edge);
                    for (size_t i = 0; i < inputs.size(); ++i) {
                        FormulaNodeRef lit;
                        if (!constAccess.inputLiteral(nodeFormulas, inputs[i], negs[i], lit)) {
                            allAvailable = false;
                            break;
                        }
                        inputFormulas.push_back(lit);
                    }

                    if (!allAvailable) {
                        worklist.push({edge, depGraph.edgeDepthsGlobal.at(edge), _seqId++});
                        cycleInWorklists[cid].insert(edge);
                        debugger.endIteration();
                        continue;
                    }

                    newEdge = formulaManager.makeAnd(inputFormulas);
                }
                if (formulaManager.isSame(edgeFormulas[edge], newEdge)) {
                    debugger.endIteration();
                    continue;
                }

                edgeFormulas[edge] = newEdge;

                NodePtr output = view.getOutput(edge);
                if (!output || output->isFact) {
                    debugger.endIteration();
                    continue;
                }

                FormulaNodeRef newNode;
                bool hasNewNode = constAccess.nodeFormula(output, newNode);
                if (!hasNewNode) {
                    std::vector<FormulaNodeRef> incoming;
                    for (EdgePtr e : view.getIncomingEdges(output)) {
                        if (edgeFormulas.count(e) && edgeFormulas[e].get()) {
                            incoming.push_back(edgeFormulas[e]);
                        }
                    }
                    if (!incoming.empty()) {
                        newNode = formulaManager.makeOr(incoming);
                        hasNewNode = true;
                    }
                }
                if (hasNewNode && !formulaManager.isSame(nodeFormulas[output], newNode)) {
                    nodeFormulas[output] = newNode;
                    changedNodes.insert(output);
                    for (EdgePtr outEdge : view.getOutgoingEdges(output)) {
                        assert (depGraph.edgeToCycleIndex.count(outEdge));
                        auto it = depGraph.edgeToCycleIndex.find(outEdge);
                        if (it->second == cid && !cycleInWorklists[cid].count(outEdge)) {
                            cycleWorklists[cid].push({outEdge, depGraph.edgeDepthsGlobal.at(outEdge), _seqId++});
                            cycleInWorklists[cid].insert(outEdge);
                        }
                    }
                }
                debugger.endIteration();
            }

            for (size_t succ : depGraph.reverseDependencies[cid]) {
                if (--inDegree[succ] == 0 && !scheduled[succ]) {
                    ready.push(succ);
                }
            }
        }
        end = high_resolution_clock::now();
        debugger.logMessage(Level::INFO, "rederive time: " + std::to_string(duration_cast<milliseconds>(end - start).count()) + " milliseconds");
    } else {
        debugger.logMessage(Level::INFO, "[inc-regional] No deletions; skipping deletion phase");
    }

    if (deltaInsertedEdges.empty() && deltaInsertedNodes.empty()) {
        debugger.logMessage(Level::INFO, "[inc-regional] No inserted edges/nodes, skipping insertion");
        formulaManager.dumpProfilingStatistics();
        for (auto& [key, value]: formulaManager.getProfilingStatistics()) {
            debugger.addInfo(key, value);
        }
        debugger.logMessage(Level::INFO, "[inc-regional] pipeline finished; usedFallback=false");
        return;
    }

    using FMType = std::remove_reference_t<decltype(formulaManager)>;
    RegionalIncrementalForwardCompilation<FMType, FormulaNodeRef> orchestrator;
    orchestrator.applyUpdate(view, formulaManager, nodeFormulas, edgeFormulas, changedNodes, constInfoPtr);
    formulaManager.dumpProfilingStatistics();
    for (auto& [key, value]: formulaManager.getProfilingStatistics()) {
        debugger.addInfo(key, value);
    }
    debugger.logMessage(Level::INFO, "[inc-regional] pipeline finished; usedFallback="
        + std::string(orchestrator.getStats().usedFallback ? "true" : "false"));
}
#endif //FORWARDCOMPILATION_H
