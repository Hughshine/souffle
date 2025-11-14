#ifndef DERIVATIONGRAPH_H
#define DERIVATIONGRAPH_H

#include <iostream>
#pragma once

#include "souffle/Derivation.h"
#include "souffle/RamTypes.h"
#include "souffle/SouffleInterface.h"
#include "souffle/problog/Rule.h"
#include "souffle/problog/RuleManager.h"
#include "souffle/problog/QueryManager.h"
#include <fstream>
#include <memory>
#include <queue>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>
#include <tuple>
#include <filesystem>
#include <algorithm>

int nextFormulaNodeId = 0;
std::unordered_map<size_t, int> nodeIdMap;
std::unordered_map<int, size_t> idNodeMap;
std::unordered_map<size_t, int> edgeIdMap;
std::unordered_map<int, size_t> idEdgeMap;
int mapNodeId(size_t id) {
    if (nodeIdMap.find(id) == nodeIdMap.end()) {
        nodeIdMap[id] = nextFormulaNodeId++;
        idNodeMap[nextFormulaNodeId - 1] = id;
    }
    return nodeIdMap[id];
}
int mapEdgeId(size_t id) {
    if (edgeIdMap.find(id) == edgeIdMap.end()) {
        edgeIdMap[id] = nextFormulaNodeId++;
        idEdgeMap[nextFormulaNodeId - 1] = id;
    }
    return edgeIdMap[id];
}

// example rule application sets
// rule 1: path(x,y) :- edge(x,y).
// rule 2: path(x,y) :- path(x,z), edge(z,y).
// facts: edge(1,2), edge(2,3)
// (derived) paths: path(1,2), path(2,3), path(1,3)
// evidence(path(1,3), true).

// Forward declarations
class Node;
class Hyperedge;
class DerivationGraph;
struct CycleDependencyGraph;

using NodePtr = std::shared_ptr<Node>;
using EdgePtr = std::shared_ptr<Hyperedge>;
// TODO: derivation graph now does not support negation...

/** class Evidence {
public:
    friend class DerivationGraph;
    friend class IncrementalDerivationGraph;
    Evidence(const UntypedTuple tuple, bool value) : tuple(tuple), value(value) {}

    const UntypedTuple& getTuple() const {return tuple;}
    bool getValue() const {return value;}

    std::string toString() const {
        std::stringstream ss;
        ss << "evidence(" << tuple.toString() << ", " << (value ? "true" : "false") << ")";
        return ss.str();
    }
private:
    UntypedTuple tuple;
    bool value;
}; **/

class Node {
public:
    friend class DerivationGraph;
    friend class IncrementalDerivationGraph;

    const UntypedTuple& getTuple() const { return tuple; }
    const std::vector<EdgePtr>& getIncomingEdges() const { return incomingEdges; }
    std::vector<EdgePtr>& getIncomingEdges() { return incomingEdges; }

    const std::vector<EdgePtr>& getOutgoingEdges() const { return outgoingEdges; }
    std::vector<EdgePtr>& getOutgoingEdges() { return outgoingEdges; }
    size_t getId() const { return id; }
    void setProbability(double prob) { probability = prob; }
    double getProbability() const { return probability; }
    std::string toString() const {
        std::stringstream ss;
        ss << tuple.toString();
        if (has_evidence) {  // 用 has_evidence 判断
            ss << "[E:" << (evidenceValue ? "true" : "false") << "]";  // 用 evidenceValue
        }
        return ss.str();
    }
    void setQuery() {
        isQuery = true;
    }
    bool isQueryNode() {
        return isQuery;
    }
    bool hasEvidence() const { return has_evidence; }
    bool getEvidenceValue() const { return evidenceValue; }
    void setEvidence(bool value) {
        evidenceValue = value;
        has_evidence = true;
    }

    bool isFact = false;
    bool pruned = false;
    bool needOutput = false;
    bool isQuery = false;

    size_t currentRefCount = 0;  // references by output nodes; set during pruning
    size_t tmpRefCount = 0;      // set during

private:
    explicit Node(const UntypedTuple& t, size_t nodeId, double prob = 1.0)
        : tuple(t), id(nodeId), probability(prob) {}

    UntypedTuple tuple;
    std::vector<EdgePtr> incomingEdges;
    std::vector<EdgePtr> outgoingEdges;
    size_t id;
    double probability;

    bool has_evidence = false;
    bool evidenceValue = false;

    void addIncomingEdge(EdgePtr edge);
    void addOutgoingEdge(EdgePtr edge);
};

using EdgeKey = std::tuple<
    unsigned long,                                     // Rule ID
    UntypedTuple,                             // Output Node's Tuple
    std::vector<std::pair<UntypedTuple, bool>>  // Vector of (Input Node Tuple, isNegated)
>;

class Hyperedge {
public:
    friend class DerivationGraph;

    const std::vector<NodePtr>& getInputs() const { return inputs; }

    mutable std::optional<std::vector<NodePtr>> cachedSortedInputs;
    mutable std::optional<std::vector<bool>> cachedSortedBodyNegations;

    const std::vector<NodePtr>& getInputsStable() const {
        if (cachedSortedInputs.has_value()) {
            return *cachedSortedInputs;
        }
        std::vector<NodePtr> sortedInputs = inputs;
        std::sort(sortedInputs.begin(), sortedInputs.end(), [](const NodePtr& a, const NodePtr& b) {
            return a->getTuple() < b->getTuple();
        });
        cachedSortedInputs.emplace(sortedInputs.begin(), sortedInputs.end());
        return *cachedSortedInputs;
    }
    NodePtr getOutput() const { return output; }
    size_t getId() const { return id; }
    const Rule* getRule() const { return rule; }
    const std::vector<bool>& getBodyNegations() const { return bodyNegations; }
    const std::vector<bool>& getBodyNegationsStable() const {
        if (cachedSortedBodyNegations.has_value()) {
            return *cachedSortedBodyNegations;
        }
        std::vector<std::pair<NodePtr, bool>> inputNegPairs;
        for (size_t i = 0; i < inputs.size(); ++i) {
            inputNegPairs.emplace_back(inputs[i], bodyNegations[i]);
        }
        std::sort(inputNegPairs.begin(), inputNegPairs.end(), [](const auto& a, const auto& b) {
            return a.first->getTuple() < b.first->getTuple();
        });
        std::vector<bool> sortedNegations;
        for (const auto& pair : inputNegPairs) {
            sortedNegations.push_back(pair.second);
        }
        cachedSortedBodyNegations.emplace(sortedNegations.begin(), sortedNegations.end());
        return *cachedSortedBodyNegations;
    }
    void setProbability(double probability) {
        if (rule) {
            this->probability = rule->getProbability();
        } else {
            this->probability = probability;
        }
    }

    double getProbability() const { return probability; }
    bool isDeterministic() const {return probability == 1.0;}

    std::string toString() const {
//        if (rule == nullptr) {
//            return "Hyperedge(" + std::to_string(id) + ")";
//        } else {
//
//            return "Rule" + std::to_string(rule->getRuleId()) + "("+ std::to_string(id) + ")";
//        }
    // inputs to outputs
        std::stringstream ss;
        // ss << "Hyperedge(" << id << ")[";
        ss << "Hyperedge[";
        ss << "rule" << ((rule)?std::to_string(rule->getRuleId()):" nil") << ",";
        for (size_t i = 0; i < bodyNegations.size(); ++i) {
            ss << (bodyNegations[i] ? "!" : "") << inputs[i]->getTuple().toString();
            if (i != bodyNegations.size() - 1) {
                ss << ",";
            }
        }
        ss << "->" << output->getTuple().toString() << "]";

        return ss.str();
    }
    RuleApplication getRuleApp() const { return ruleApp; }
    bool pruned = false;
    size_t currentRefCount = 0;  // references by output nodes; set during pruning
    size_t tmpRefCount = 0;      // set during



    mutable std::optional<EdgeKey> cachedEdgeKey;

    /**
     * @brief Generates a stable, comparable key representing the edge's content.
     * This key is composed of the rule ID, the output node's content, and an
     * ordered list of input nodes' content along with their negation status.
     * @return An EdgeKey tuple that can be used for lexicographical comparison.
     */
    EdgeKey getEdgeKey() {
        if (cachedEdgeKey.has_value()) {
            return *cachedEdgeKey;
        }
        // 1. Get the Rule ID from the RuleApplication.
        // This is the primary identifier for the rule being applied.
        size_t rule_id = ruleApp.ruleId;

        // 2. Get the output node's tuple.
        UntypedTuple output_tuple = output->getTuple();

        // 3. Build a vector of input tuples and their negation status.
        // The order is critical and is preserved by iterating from 0 to N.
        std::vector<std::pair<UntypedTuple, bool>> input_data;
        input_data.reserve(inputs.size());
        for (size_t i = 0; i < inputs.size(); ++i) {
            input_data.emplace_back(inputs[i]->getTuple(), bodyNegations[i]);
        }

        // 4. Combine these stable components into a single, comparable std::tuple.
        std::sort(input_data.begin(), input_data.end());

        cachedEdgeKey.emplace(std::make_tuple(rule_id, output_tuple, std::move(input_data)));
        return *cachedEdgeKey;
    }

private:
    Hyperedge(const std::vector<NodePtr>& inputs, NodePtr output, size_t edgeId, RuleApplication ruleApp)
        : inputs(inputs), output(output), id(edgeId), rule(nullptr), ruleApp(ruleApp) {
        assert (false);
    }
    Hyperedge(const std::vector<NodePtr>& inputs, NodePtr output, size_t edgeId, const Rule* rule, const std::vector<bool>& bodyNegations, RuleApplication ruleApp)
        : inputs(inputs), output(output), id(edgeId), rule(rule), ruleApp(ruleApp) {
        if (rule) {
            probability = rule->getProbability();
        }
        if (bodyNegations.size() > 0) {
            assert (bodyNegations.size() == inputs.size());
            this->bodyNegations = bodyNegations;
        } else {
            this->bodyNegations = std::vector<bool>(inputs.size(), false);
        }
//        for (size_t i = 0; i < inputs.size(); ++i) {
//            std::cout << "input: " << inputs[i]->toString() << std::endl;
//            std::cout << "isNegated: " << bodyNegations[i] << std::endl;
//        }
    }


    std::vector<NodePtr> inputs;
    std::vector<bool> bodyNegations;
    NodePtr output;
    size_t id;
    double probability;
    const Rule* rule;
    const RuleApplication ruleApp;
};

class DerivationGraphViewInterface {
public:
    // TODO: consider using unordered_map
    virtual const std::unordered_set<NodePtr>& getNodes() const = 0;
    virtual const std::unordered_set<EdgePtr>& getEdges() const = 0;
    void dumpDot(const std::string& filename) const;
    void dumpJson(const std::string& filename) const;
    void writeGraphStatsJson() const;

    std::vector<EdgePtr> getIncomingEdges(NodePtr node) const;
    std::vector<EdgePtr> getOutgoingEdges(NodePtr node) const;
    std::vector<NodePtr> getInputs(EdgePtr edge) const;
    std::vector<NodePtr> getInputsStable(EdgePtr edge) const;

    NodePtr getOutput(EdgePtr edge) const;
    std::vector<bool> getBodyNegations(EdgePtr edge) const;
    std::vector<bool> getBodyNegationsStable(EdgePtr edge) const;

    mutable std::optional<std::vector<EdgePtr>> cachedSortedIncomingEdges;
    std::vector<EdgePtr> getIncomingEdgesStable(NodePtr node) const;

    virtual ~DerivationGraphViewInterface() = default;
};

std::vector<EdgePtr> DerivationGraphViewInterface::getIncomingEdges(NodePtr node) const {
    std::vector<EdgePtr> result;
    for (const auto& edge : node->getIncomingEdges()) {
        if (getEdges().count(edge)) {
            result.push_back(edge);
        }
    }
    return result;
}

std::vector<EdgePtr> DerivationGraphViewInterface::getIncomingEdgesStable(NodePtr node) const {
    if (cachedSortedIncomingEdges.has_value()) {
        return *cachedSortedIncomingEdges;
    }
    std::vector<EdgePtr> sortedEdges = getIncomingEdges(node);
    std::sort(sortedEdges.begin(), sortedEdges.end(), [](const EdgePtr& a, const EdgePtr& b) {
        return a->getEdgeKey() < b->getEdgeKey();
    });
    cachedSortedIncomingEdges.emplace(sortedEdges.begin(), sortedEdges.end());
    return *cachedSortedIncomingEdges;
}


std::vector<EdgePtr> DerivationGraphViewInterface::getOutgoingEdges(NodePtr node) const {
    std::vector<EdgePtr> result;
    for (const auto& edge : node->getOutgoingEdges()) {
        if (getEdges().count(edge)) {
            result.push_back(edge);
        }
    }
    return result;
}

std::vector<NodePtr> DerivationGraphViewInterface::getInputs(EdgePtr edge) const {
    if (getEdges().count(edge) == 0) {
        return std::vector<NodePtr>();
    }
    // if edge is in the view, then all of its input nodes should be in the view
    return edge->getInputs();
//    std::vector<NodePtr> result;
//    for (const auto& input : edge->getInputs()) {
//        if (getNodes().count(input)) {
//            result.push_back(input);
//        }
//    }
//    return result;
}

std::vector<NodePtr> DerivationGraphViewInterface::getInputsStable(EdgePtr edge) const {
    if (getEdges().count(edge) == 0) {
        return std::vector<NodePtr>();
    }
    return edge->getInputsStable();
}

NodePtr DerivationGraphViewInterface::getOutput(EdgePtr edge) const {
    NodePtr out = edge->getOutput();
    assert (out != nullptr);
    if (getNodes().count(out) == 0) {
        std::cout << "Node not in view: " << out->toString() << std::endl;
        assert (getNodes().count(out) != 0);
    }
    return getNodes().count(out) ? out : nullptr;
}

std::vector<bool> DerivationGraphViewInterface::getBodyNegations(EdgePtr edge) const {
    if (getEdges().count(edge) == 0) {
        return std::vector<bool>();
    }
    // if edge is not pruned, then all of its input nodes should be in the view
    return edge->getBodyNegations();
//    std::vector<bool> result;
//    for (size_t i = 0; i < edge->getBodyNegations().size(); ++i) {
//        if (getNodes().count(edge->getInputs()[i])) {
//            result.push_back(edge->getBodyNegations()[i]);
//        }
//    }
//    return result;
}

std::vector<bool> DerivationGraphViewInterface::getBodyNegationsStable(EdgePtr edge) const {
    if (getEdges().count(edge) == 0) {
        return std::vector<bool>();
    }
    return edge->getBodyNegationsStable();
}

class SubgraphView : public virtual DerivationGraphViewInterface {
public:
    SubgraphView(std::unordered_set<NodePtr> nodes,
                 std::unordered_set<EdgePtr> edges)
        : nodes_(std::move(nodes)), edges_(std::move(edges)) {}

    const std::unordered_set<NodePtr>& getNodes() const override { return nodes_; }
    const std::unordered_set<EdgePtr>& getEdges() const override { return edges_; }
    void dumpStatistics(std::ostream& out) const {
        out << "DerivationGraph Statistics:" << std::endl;
        out << "  Number of nodes: " << nodes_.size() << std::endl;
        out << "  Number of edges: " << edges_.size() << std::endl;
    }
protected:
    std::unordered_set<NodePtr> nodes_;
    std::unordered_set<EdgePtr> edges_;
};

void DerivationGraphViewInterface::dumpJson(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    out << "{\n";

    // Facts section
    out << "  \"facts\": [\n";
    bool first = true;
    for (const auto& node : getNodes()) {
        if (node->isFact) {
            if (!first) {
                out << ",\n";
            }
            first = false;
            out << "    {\"name\": \"" << node->getTuple().toString() << "\",";
            out << "     \"probability\": " << node->getProbability() << "}";
        }
    }
    out << "\n  ],\n";

    // Rules section
    out << "  \"rules\": [\n";
    first = true;
    for (const auto& edge : getEdges()) {
        if (!first) {
            out << ",\n";
        }
        first = false;

        out << "    {\n";

        // Head
        NodePtr headNode = this->getOutput(edge);
        out << "      \"head\": \"" << headNode->getTuple().toString() << "\",\n";
        // Probability
        out << "      \"probability\": " << edge->getProbability() << ",\n";
        // Bodies
        out << "      \"bodies\": [\n";

        std::vector<NodePtr> inputs = this->getInputs(edge);
        std::vector<bool> negations = this->getBodyNegations(edge);

        for (size_t i = 0; i < inputs.size(); ++i) {
            if (i > 0) {
                out << ",\n";
            }
            out << "        {\n";

            // Handle negation - default to false if negations vector is too short
            bool isNegated = (i < negations.size()) ? negations[i] : false;
            out << "          \"negation\": " << (isNegated ? "true" : "false") << ",\n";
            out << "          \"name\": \"" << inputs[i]->getTuple().toString() << "\"\n";
            out << "        }";
        }

        out << "\n      ]\n";
        out << "    }";
    }

    out << "\n  ]\n";
    out << "}\n";
    out.close();
}

void DerivationGraphViewInterface::dumpDot(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    out << "digraph SubgraphView {\n";
    out << "  rankdir=LR;\n";

    // 节点样式
    out << "  node [shape=box, style=filled, fillcolor=lightblue];\n";
    for (const auto& node : getNodes()) {
        out << "  node" << node->getId() << " [label=\""
            << node->getTuple().toString() << "\"];\n";
    }

    // 边样式
    out << "  node [shape=point, fillcolor=red, width=0.2];\n";
    for (const auto& edge : getEdges()) {
        out << "  edge" << edge->getId() << ";\n";

        for (const auto& input : this->getInputs(edge)) {
            if (getNodes().count(input)) {  // TODO: seems redundant
                out << "  node" << input->getId()
                    << " -> edge" << edge->getId() << ";\n";
            }
        }

        NodePtr outNode = this->getOutput(edge);
        if (getNodes().count(outNode)) {
            out << "  edge" << edge->getId()
                << " -> node" << outNode->getId() << ";\n";
        }
    }

    out << "}\n";
    out.close();
}

inline void writeGraphStatsJson(const DerivationGraphViewInterface& g);

class IncrementalDerivationGraphViewInterface : virtual public DerivationGraphViewInterface {
public:
    virtual const std::set<NodePtr>& getDeltaInsertNodes() const = 0;
    virtual const std::set<EdgePtr>& getDeltaInsertEdges() const = 0;
    virtual const std::set<NodePtr>& getDeltaDeleteNodes() const = 0;
    virtual const std::set<EdgePtr>& getDeltaDeleteEdges() const = 0;
    virtual const std::unordered_map<NodePtr, std::set<NodePtr>>& getNodeImpactedByDeltaDelete() const = 0;
    virtual const std::unordered_map<NodePtr, std::set<EdgePtr>>& getEdgeImpactedByDeltaDelete() const = 0;
    virtual const std::unordered_map<NodePtr, std::set<NodePtr>>& getNodeImpactedByDeltaInsert() const = 0;
    virtual const std::unordered_map<NodePtr, std::set<EdgePtr>>& getEdgeImpactedByDeltaInsert() const = 0;
    const std::set<NodePtr>& getValidNodes() {
        if (validNodes_.size() > 0) {
            return validNodes_;
        }
        for (const auto& node : getNodes()) {
            if (getDeltaDeleteNodes().count(node) == 0 && node->pruned == false) {
                validNodes_.insert(node);
            }
        }
        return validNodes_;
     }
    const std::set<EdgePtr>& getValidEdges() {
        if (validEdges_.size() > 0) {
            return validEdges_;
        }
        for (const auto& edge : getEdges()) {
            if (getDeltaDeleteEdges().count(edge) == 0 && edge->pruned == false) {
                validEdges_.insert(edge);
            }
        }
        return validEdges_;
     }
    const std::set<NodePtr>& getDeletedFacts() {
        if (deletedFacts_.size() > 0) {
            return deletedFacts_;
        }
        for (const auto& kv : getNodeImpactedByDeltaDelete()) {
            deletedFacts_.insert(kv.first);
        }
        return deletedFacts_;
    }
    const std::set<NodePtr>& getDeletedDeterminsticFacts() {
        if (deletedDeterminsticFacts_.size() > 0) {
            return deletedDeterminsticFacts_;
        }
        for (const auto& deletedFact: getDeletedFacts()) {
            if (deletedFact->getProbability() == 1.0) {
                deletedDeterminsticFacts_.insert(deletedFact);
            }
        }
        return deletedDeterminsticFacts_;
    }
    const std::set<NodePtr>& getDeletedNonDeterministicFacts() {
        if (deletedNonDeterministicFacts_.size() > 0) {
            return deletedNonDeterministicFacts_;
        }
        for (const auto& deletedFact: getDeletedFacts()) {
            if (deletedFact->getProbability() < 1.0) {
                deletedNonDeterministicFacts_.insert(deletedFact);
            }
        }
        return deletedNonDeterministicFacts_;
    }

    // deletion impacted
    // insertion impacted

    void dumpDotInc(const std::string& filename) const;
    void dumpStatisticsInc(std::ostream& out) {
        out << "IncrementalDerivationGraph Statistics:" << std::endl;
        out << "  Number of nodes: " << getNodes().size() << std::endl;
        out << "  Number of edges: " << getEdges().size() << std::endl;
        out << "  Number of delta insert nodes: " << getDeltaInsertNodes().size() << std::endl;
        out << "  Number of delta insert edges: " << getDeltaInsertEdges().size() << std::endl;
        out << "  Number of delta delete nodes: " << getDeltaDeleteNodes().size() << std::endl;
        out << "  Number of delta delete edges: " << getDeltaDeleteEdges().size() << std::endl;
        std::set<NodePtr> impactedNodes;
        std::set<EdgePtr> impactedEdges;
        for (const auto& [deletedFact, impactedFact]: getNodeImpactedByDeltaDelete()) {
            out << "  Deleted fact " << deletedFact->getTuple().toString() + "_" + std::to_string(deletedFact->getId()) << " impacts visible nodes: ";
            for (const auto& fact : impactedFact) {
                out << fact->toString() << " ";
                impactedNodes.insert(fact);
            }
            out << std::endl;
        }
        for (const auto& [deletedEdge, impactedEdge]: getEdgeImpactedByDeltaDelete()) {
            out << "  Deleted fact " << deletedEdge->getTuple().toString() + "_" + std::to_string(deletedEdge->getId()) << " impacts visible edges: ";
            for (const auto& edge : impactedEdge) {
                out << edge->toString() << " ";
                impactedEdges.insert(edge);
            }
            out << std::endl;
        }
        for (const auto& [insertedFact, impactedFact]: getNodeImpactedByDeltaInsert()) {
            out << "  Node " << insertedFact->getId() << " impacted by delta insert: ";
            for (const auto& fact : impactedFact) {
                out << fact->toString() << " ";
                impactedNodes.insert(fact);
            }
            out << std::endl;
        }
        for (const auto& [insertedEdge, impactedEdge]: getEdgeImpactedByDeltaInsert()) {
            out << "  Edge " << insertedEdge->getId() << " impacted by delta insert: ";
            for (const auto& edge : impactedEdge) {
                out << edge->getId() << " ";
                impactedEdges.insert(edge);
            }
            out << std::endl;
        }
        for (const auto& node : getValidNodes()) {
            out << "  Valid node: " << node->toString() << "_" << node->getId() << "_" << mapNodeId(node->getId()) << std::endl;
        }
        for (const auto& node : getDeletedFacts()) {
            out << "  Deleted fact: " << node->toString() << "_" << node->getId() << "_" << mapNodeId(node->getId())  << std::endl;
        }
        for (const auto& node : getDeletedDeterminsticFacts()) {
            out << "  Deleted deterministic fact: " << node->toString() << "_" << node->getId() << std::endl;
        }
        for (const auto& node : getDeletedNonDeterministicFacts()) {
            out << "  Deleted non-deterministic fact: " << node->toString() << "_" << node->getId() << "_" << mapNodeId(node->getId()) << std::endl;
        }
        std::set<NodePtr> uselessValidNodes;
        for (const auto& node : getValidNodes()) {
            if (impactedNodes.count(node) == 0) {
                bool used = false;
                for (const auto& edge : getOutgoingEdges(node)) {
                    if (impactedEdges.count(edge) > 0) {
                        used = true;
                        break;
                    }
                }
                if (!used)
                    uselessValidNodes.insert(node);
            }
        }
        out << " percentage of useless valid nodes: " << (double)uselessValidNodes.size() / (double)getValidNodes().size() << std::endl;
         // ===== JSON 数字统计输出（新增，不影响原有日志） =====
        this->writeGraphStatsJson();
    }
protected:
    std::set<NodePtr> validNodes_;
    std::set<EdgePtr> validEdges_;
    std::set<NodePtr> deletedFacts_;
    std::set<NodePtr> deletedDeterminsticFacts_;
    std::set<NodePtr> deletedNonDeterministicFacts_;
};

class IncSubgraphView : public SubgraphView, public virtual IncrementalDerivationGraphViewInterface {
public:
    IncSubgraphView(const std::unordered_set<NodePtr>& nodes,
                    const std::unordered_set<EdgePtr>& edges,
                    const std::set<NodePtr>& deltaInsertNodes,
                    const std::set<EdgePtr>& deltaInsertEdges,
                    const std::set<NodePtr>& deltaDeleteNodes,
                    const std::set<EdgePtr>& deltaDeleteEdges,
            const std::unordered_map<NodePtr, std::set<NodePtr>>& nodeImpactedByDeltaDelete = {},
            const std::unordered_map<NodePtr, std::set<EdgePtr>>& edgeImpactedByDeltaDelete = {},
            const std::unordered_map<NodePtr, std::set<NodePtr>>& nodeImpactedByDeltaInsert = {},
            const std::unordered_map<NodePtr, std::set<EdgePtr>>& edgeImpactedByDeltaInsert = {})
            : SubgraphView(nodes, edges),
              deltaInsertNodes_(deltaInsertNodes),
              deltaInsertEdges_(deltaInsertEdges),
              deltaDeleteNodes_(deltaDeleteNodes),
              deltaDeleteEdges_(deltaDeleteEdges),
            nodeImpactedByDeltaDelete_(nodeImpactedByDeltaDelete),
            edgeImpactedByDeltaDelete_(edgeImpactedByDeltaDelete),
            nodeImpactedByDeltaInsert_(nodeImpactedByDeltaInsert),
            edgeImpactedByDeltaInsert_(edgeImpactedByDeltaInsert) {};

    // getNodes() / getEdges() from DerivationGraphView
    const std::unordered_set<NodePtr>& getNodes() const override {return nodes_; };
    const std::unordered_set<EdgePtr>& getEdges() const override {return edges_; };

    const std::set<NodePtr>& getDeltaInsertNodes() const override {return deltaInsertNodes_; };
    const std::set<EdgePtr>& getDeltaInsertEdges() const override {return deltaInsertEdges_; };
    const std::set<NodePtr>& getDeltaDeleteNodes() const override {return deltaDeleteNodes_; };
    const std::set<EdgePtr>& getDeltaDeleteEdges() const override {return deltaDeleteEdges_; };


    const std::unordered_map<NodePtr, std::set<NodePtr>>& getNodeImpactedByDeltaDelete() const override {
        return nodeImpactedByDeltaDelete_;
    }
    const std::unordered_map<NodePtr, std::set<EdgePtr>>& getEdgeImpactedByDeltaDelete() const override {
        return edgeImpactedByDeltaDelete_;
    }
    const std::unordered_map<NodePtr, std::set<NodePtr>>& getNodeImpactedByDeltaInsert() const override {
        return nodeImpactedByDeltaInsert_;
    }
    const std::unordered_map<NodePtr, std::set<EdgePtr>>& getEdgeImpactedByDeltaInsert() const override {
        return edgeImpactedByDeltaInsert_;
    }


    // 可选：dumpDot for incremental view

protected:
    std::set<NodePtr> deltaInsertNodes_;
    std::set<EdgePtr> deltaInsertEdges_;
    std::set<NodePtr> deltaDeleteNodes_;
    std::set<EdgePtr> deltaDeleteEdges_;
    std::unordered_map<NodePtr, std::set<NodePtr>> nodeImpactedByDeltaDelete_;
    std::unordered_map<NodePtr, std::set<EdgePtr>> edgeImpactedByDeltaDelete_;
    std::unordered_map<NodePtr, std::set<NodePtr>> nodeImpactedByDeltaInsert_;
    std::unordered_map<NodePtr, std::set<EdgePtr>> edgeImpactedByDeltaInsert_;
};

class DerivationGraph: virtual public DerivationGraphViewInterface {
public:
    DerivationGraph() : nextNodeId(0), nextEdgeId(0) {}
    DerivationGraph(const RuleManager* rm) : nextNodeId(0), nextEdgeId(0), ruleManager(rm) {}
    void dumpStatistics(std::ostream& out) const {
        out << "DerivationGraph Statistics:" << std::endl;
        out << "  Number of nodes: " << nodes.size() << std::endl;
        out << "  Number of edges: " << edges.size() << std::endl;
    }
    NodePtr createNode(const UntypedTuple& tuple, const double weight = 1.0) {
        // 先查找是否已存在
        NodePtr existingNode = findNode(tuple);
        if (existingNode) {
            return existingNode;
        }

        // 不存在则创建新节点
        auto node = std::shared_ptr<Node>(new Node(tuple, nextNodeId++));
        nodes.insert(node);
        node->setProbability(weight);

        // 添加到映射中
        tupleToNodeMap[tuple] = node;
        return node;
    }

    void createQuery(NodePtr node, const QueryManager& queryManager) {
        const auto& tuple = node->getTuple();
        const std::string& relation = tuple.relation_name;
        const auto& fields = tuple.fields;

        for (const auto& query : queryManager.getAllQuery()) {
            const std::string& queryRelation = query->getRelationName();
            const auto& queryVars = query->getBoundVariables();

            if (relation != queryRelation) continue;
            if (queryVars.size() != fields.size()) {
                std::cerr << "Length mismatch in relation " << relation
                          << ": queryVars=" << queryVars.size()
                          << ", fields=" << fields.size() << std::endl;
                continue;
            }

            bool match = true;
            for (size_t i = 0; i < fields.size(); i++) {
                if (queryVars[i] == "_") continue;
                if (std::to_string(fields[i]) != queryVars[i]) {
                    match = false;
                    break;
                }
            }

            if (match) {
                node->setQuery();
            }
        }
    }
    void attachEvidence(const std::vector<std::pair<UntypedTuple,bool>>& evidenceList) {
        for (const auto& [tuple, value] : evidenceList) {
            NodePtr node = findNode(tuple);
            if (node) {
                node ->setEvidence(value);
                std::cout << "[Info] Attached evidence (" << tuple.toString() << ","
                                      << (value ? "true" : "false")
                                      << ") to node " << node->toString() << std::endl;
            } else {
                std::cerr << "Evidence (" << tuple.toString() << ","
                                      << (value ? "true" : "false")
                                      << ") does not match any node" << std::endl;
            }
        }
    }


    EdgePtr createHyperedgeFromRuleApp(const RuleApplication& ruleApp, const RuleManager& rm) {
        // 先查找是否存在对应的边
        const Rule* rule = rm.getRule(ruleApp.ruleId);
        assert(rule != nullptr && "Rule not found");
        const std::vector<std::string>& vars = rule->getVars();

        EdgePtr existingEdge = findHyperedgeFromRuleApp(ruleApp, vars);
        if (existingEdge) {
            return existingEdge;
        }


        // 根据规则头部和变量值创建输出元组
        UntypedTuple headTuple{rule->getHead().getRelation(),
                rule->getHead().instantiatedFields(rule->getVars(), ruleApp.varValuesPure)};
        auto headNode = createNode(headTuple);

        // 创建输入节点
        std::vector<NodePtr> bodyNodes;
        std::vector<bool> bodyNegations;
        for (const auto& bodyAtom : rule->getBodyAtoms()) {
            UntypedTuple bodyTuple{bodyAtom.getRelation(), bodyAtom.instantiatedFields(rule->getVars(), ruleApp.varValuesPure)};
            auto bodyNode = createNode(bodyTuple);
            bodyNodes.push_back(bodyNode);
            bodyNegations.push_back(bodyAtom.isNegatedAtom());
        }
//        std::cout << "creating hyperedge from ruleApp: " << ruleApp.ruleId << std::endl;
//        for (size_t i = 0; i < bodyNodes.size(); ++i) {
//            std::cout << "bodyNode: " << bodyNodes[i]->toString() << std::endl;
//            std::cout << "isNegated: " << bodyNegations[i] << std::endl;
//        }
        auto newEdge = createHyperedge(bodyNodes, headNode, rule, bodyNegations, ruleApp);

        // 将新边添加到映射中
        std::string key = createEdgeKey(ruleApp.ruleId, vars, ruleApp.varValuesPure);
        edgeKeyToEdgeMap[key] = newEdge;

        return newEdge;
    }

    NodePtr findNode(const UntypedTuple& tuple) const {
        auto it = tupleToNodeMap.find(tuple);
        if (it != tupleToNodeMap.end()) {
            return it->second;
        }
        return nullptr;
    }

    EdgePtr findHyperedge(souffle::RamDomain ruleId,
                         const std::vector<std::string>& vars,
                            const std::vector<souffle::RamDomain>& values) const {
        std::string key = createEdgeKey(ruleId, vars, values);
        auto it = edgeKeyToEdgeMap.find(key);
        if (it != edgeKeyToEdgeMap.end()) {
            return it->second;
        }
        return nullptr;
    }

    EdgePtr findHyperedgeFromRuleApp(const RuleApplication& ruleApp, const std::vector<std::string>& vars) const {
        return findHyperedge(ruleApp.ruleId, vars, ruleApp.varValuesPure);
    }

    const std::unordered_set<NodePtr>& getNodes() const { return nodes; }
    const std::unordered_set<EdgePtr>& getEdges() const { return edges; }

    static DerivationGraph* createFrom(const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& ruleApps, const RuleManager& ruleManager, const QueryManager& queryManager, const std::unordered_map<UntypedTuple, double>& fact_prob = {}, const std::vector<std::pair<UntypedTuple,bool>>& evidences = {})  {
        std::cout << "[Debug] Enter DerivationGraph::createFrom()" << std::endl;
        FunctionTimer timer(" creating derivation graph ");
        auto graph = new DerivationGraph(&ruleManager);
        for (const auto& [tuple, prob] : fact_prob) {
            auto node = graph->createNode(tuple);  // actually "find node" here
            node->probability = prob;
            node->isFact = true;
        }
        for (const auto& [tuple, ruleAppSet] : ruleApps) {
            auto node = graph->createNode(tuple);
//            if (node->isFact) {
//                std::cout << "Found fact node: " << node->getTuple().toString() << std::endl;
//                continue;  // skip fact nodes currently
//            }
            for (const auto& ruleApp : *ruleAppSet) {
				auto edge = graph->createHyperedgeFromRuleApp(ruleApp, ruleManager);
            }
        }
        std::cout << "[Debug] Current nodes in graph:" << std::endl;
        graph->attachEvidence(evidences);
        for (const auto& node : graph->getNodes()) {
            std::cout << "  " << node->getTuple().toString() << std::endl;
        }

        return graph;
    }

    SubgraphView prune(const std::vector<souffle::Relation*>& outputRelations) {
        std::vector<std::string> outputRelationNames;
        for (const auto* rel : outputRelations) {
            outputRelationNames.push_back(rel->getName());
        }
        return prune(outputRelationNames);
    }

    SubgraphView prune(const std::vector<std::string>& outputRelations) {
        std::unordered_set<std::string> outputRelationNames(outputRelations.begin(), outputRelations.end());

        // 标记可达的节点和边
        std::unordered_set<NodePtr> reachableNodes;
        std::unordered_set<EdgePtr> reachableEdges;
        std::queue<NodePtr> workQueue;

        std::unordered_set<NodePtr> outputNodes;
        // 初始化：从所有输出 relation 的节点出发
        for (const auto& node : nodes) {
            if (outputRelationNames.count(node->getTuple().relation_name) > 0) {
//                std::cout << "Found output node: " << node->getTuple().toString() << std::endl;
                reachableNodes.insert(node);
                workQueue.push(node);
                node->needOutput = true;
                outputNodes.insert(node);
            }
            node->currentRefCount = 0;
        }

        for (const auto& edge : edges) {
            edge->currentRefCount = 0;
        }

        // register reference count
        for (const auto& node : outputNodes) {
            std::queue<NodePtr> q;
            std::unordered_set<NodePtr> visited;
            q.push(node);
            while (!q.empty()) {
                NodePtr n = q.front(); q.pop();
                visited.insert(n);
                n->currentRefCount += 1;
                for (const auto& edge : n->getIncomingEdges()) {
                    edge->currentRefCount += 1;
                    for (const auto& inputNode : edge->getInputs()) {
                        if (visited.count(inputNode) == 0) {
                            q.push(inputNode);
                        }
                    }
                }
            }
        }

        // 反向 BFS 遍历
        while (!workQueue.empty()) {
            NodePtr current = workQueue.front();
            workQueue.pop();
            if (current->isFact) {
                continue;  // skip input fact nodes
            }
            for (const auto& edge : current->getIncomingEdges()) {
                if (std::find(edge->getInputs().begin(), edge->getInputs().end(), current) != edge->getInputs().end()) {
                    continue;  // if the edge reports self-dependency, then it is redundant
                }
                reachableEdges.insert(edge);
                for (const auto& inputNode : edge->getInputs()) {
                    if (reachableNodes.insert(inputNode).second) {
                        workQueue.push(inputNode);
                    }
                }
            }
        }

        // 过滤节点和边
        std::unordered_set<NodePtr> newNodes;
        for (const auto& node : nodes) {
            if (reachableNodes.count(node)){
                newNodes.insert(node);
            if (!reachableNodes.count(node)) {
                node->needOutput = true;
            }
            }
        }

        std::unordered_set<EdgePtr> newEdges;
        for (const auto& edge : edges) {
            if (reachableEdges.count(edge)) {
                newEdges.insert(edge);
            }
        }

        // TODO: update nodes incoming and outgoing edges
        for (const auto& node : newNodes) {
            std::vector<EdgePtr> newIncomingEdges;
            std::vector<EdgePtr> newOutgoingEdges;
            for (const auto& edge : node->getIncomingEdges()) {
                if (reachableEdges.count(edge)) {
                    newIncomingEdges.push_back(edge);
                }
            }
            for (const auto& edge : node->getOutgoingEdges()) {
                if (reachableEdges.count(edge)) {
                    newOutgoingEdges.push_back(edge);
                }
            }
            node->incomingEdges = std::move(newIncomingEdges);
            node->outgoingEdges = std::move(newOutgoingEdges);
        }

//        nodes = std::move(newNodes);
//        edges = std::move(newEdges);
        return SubgraphView(std::move(newNodes), std::move(newEdges));
    }

    static DerivationGraph createExample() {
        DerivationGraph graph;

        auto edge1 = graph.createNode(UntypedTuple{"edge", {1, 2}});
        auto edge2 = graph.createNode(UntypedTuple{"edge", {2, 3}});

        auto path1 = graph.createNode(UntypedTuple{"path", {1, 2}});
        auto path2 = graph.createNode(UntypedTuple{"path", {2, 3}});

        auto path3 = graph.createNode(UntypedTuple{"path", {1, 3}});

        graph.createHyperedge({edge1}, path1);
        graph.createHyperedge({edge2}, path2);

        graph.createHyperedge({edge1, path2}, path3);

        return graph;
    }

    void setRuleManager(RuleManager* rm) {
        ruleManager = rm;
    }

    // this one does not check if the edge already exists
    EdgePtr createHyperedge(const std::vector<NodePtr>& inputs, NodePtr output, const Rule* rule, const std::vector<bool>& bodyNegations, RuleApplication ruleApp = naiveRuleApplication) {
        if (inputs.empty() || (rule != nullptr && rule->isFact())) {
            // we do not create edges with no inputs or edges for fact rules
            return nullptr;
        }
        auto edge = std::shared_ptr<Hyperedge>(new Hyperedge(inputs, output, nextEdgeId++, rule, bodyNegations, ruleApp));
        for (const auto& input : inputs) {
            assert (input != nullptr);
            input->addOutgoingEdge(edge);
        }
        assert (output != nullptr);
        output->addIncomingEdge(edge);

        edges.insert(edge);

        return edge;
    }

    // Just for test; this one does not check if the edge already exists
    EdgePtr createHyperedge(const std::vector<NodePtr>& inputs, NodePtr output, RuleApplication ruleApp = naiveRuleApplication) {
        auto edge = std::shared_ptr<Hyperedge>(new Hyperedge(inputs, output, nextEdgeId++, ruleApp));

        for (const auto& input : inputs) {
            input->addOutgoingEdge(edge);
        }
        output->addIncomingEdge(edge);

        edges.insert(edge);
        return edge;
    }

protected:
//    std::vector<NodePtr> nodes;
//    std::vector<EdgePtr> edges;
    std::unordered_set<NodePtr> nodes;
    std::unordered_set<EdgePtr> edges;
    size_t nextNodeId;
    size_t nextEdgeId;
    const RuleManager* ruleManager;

    // 添加元组到节点的映射
    std::map<UntypedTuple, NodePtr> tupleToNodeMap;

    // 添加边键到边的映射
    std::map<std::string, EdgePtr> edgeKeyToEdgeMap;

    // 创建边的唯一键
    std::string createEdgeKey(souffle::RamDomain ruleId,
                             const std::vector<std::string>& vars,
                             const std::vector<souffle::RamDomain>& values) const {
        std::stringstream ss;
        ss << ruleId << "_";

        // 按照变量名排序，确保键的一致性
        std::vector<std::string> varNames = vars;
//        for (const auto& [name, _] : varValues) {
//            varNames.push_back(name);
//        }
        std::sort(varNames.begin(), varNames.end());

        for (size_t i = 0; i < varNames.size(); ++i) {
            ss << varNames[i] << ":" << values[i] << ";";
        }
//        for (const auto& name : varNames) {
//            ss << name << ":" << varValues.at(name) << ";";
//        }

        return ss.str();
    }


};

void Node::addIncomingEdge(EdgePtr edge) {
    assert(edge->getOutput().get() == this);
    incomingEdges.push_back(edge);
}

void Node::addOutgoingEdge(EdgePtr edge) {
    const auto& inputs = edge->getInputs();
    assert(std::find_if(inputs.begin(), inputs.end(), [this](const NodePtr& node) {
        return node.get() == this;
    }) != inputs.end());
    outgoingEdges.push_back(edge);
}

class IncrementalDerivationGraph : public DerivationGraph, virtual public IncrementalDerivationGraphViewInterface {
public:
    friend class PreDerivationGraph;

    // 构造函数
    IncrementalDerivationGraph() : DerivationGraph() {}
    IncrementalDerivationGraph(const RuleManager* rm) : DerivationGraph(rm) {}
    IncSubgraphView prune(const std::vector<souffle::Relation*>& outputRelations);
    IncSubgraphView prune(const std::vector<std::string>& outputRelations);
  
    static IncrementalDerivationGraph* createFrom(const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& ruleApps, const RuleManager& ruleManager, const QueryManager& queryManager, const std::unordered_map<UntypedTuple, double>& fact_prob = {}, const std::vector<std::pair<UntypedTuple,bool>>& evidences = {}) {
        FunctionTimer timer(" creating derivation graph ");
        auto graph = new IncrementalDerivationGraph(&ruleManager);
        for (const auto& [tuple, prob] : fact_prob) {
            auto node = graph->createNode(tuple);  // actually "find node" here
            node->setProbability(prob);
            node->isFact = true;
        }
        for (const auto& [tuple, ruleAppSet] : ruleApps) {
            auto node = graph->createNode(tuple, 0.0);
//            if (node->isFact) {
//                std::cout << "Found fact node: " << node->getTuple().toString() << std::endl;
//                continue;  // skip fact nodes currently
//            }
            for (const auto& ruleApp : *ruleAppSet) {
                auto edge = graph->createHyperedgeFromRuleApp(ruleApp, ruleManager);
            }
        }

        for (auto& node : graph->getNodes()) {
            graph->createQuery(node, queryManager);
        }

        return graph;
    }

    // 应用增量插入
    void applyDeltaInserts(
        const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& deltaInsertRuleApps,
        const RuleManager& ruleManager,
        const std::unordered_map<UntypedTuple, double>& fact_prob = {}
    );

    // 应用增量删除
    void applyDeltaDeletes(
        const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& deltaDeleteRuleApps,
        const RuleManager& ruleManager,
        const std::vector<UntypedTuple>& deletedFacts
    );

    // 合并方法，同时应用插入和删除操作
    void applyDelta(
        const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& deltaInsertRuleApps,
        const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& deltaDeleteRuleApps,
        const RuleManager& ruleManager,
        const std::unordered_map<UntypedTuple, double>& fact_prob = {},
        const std::vector<UntypedTuple>& deletedFacts = {}
    ) {
        this->deltaInsertNodes.clear();
        this->deltaInsertEdges.clear();
        this->deltaDeleteNodes.clear();
        this->deltaDeleteEdges.clear();

        this->deletedFactImpactedNodes.clear();
        this->deletedFactImpactedEdges.clear();
        this->insertedFactImpactedNodes.clear();
        this->insertedFactImpactedEdges.clear();

//        for (auto& insertedRuleApp : deltaInsertRuleApps) {
//            std::cout << "For tuple: " << insertedRuleApp.first.toString() << std::endl;
//            for (const auto& ruleApp : *(insertedRuleApp.second)) {
//                std::cout << "  Inserted Rule application: " << RuleApplication::toString(ruleApp) << std::endl;
//            }
//        }
//        for (auto& deletedRuleApp : deltaDeleteRuleApps) {
//            std::cout << "For tuple: " << deletedRuleApp.first.toString() << std::endl;
//            for (const auto& ruleApp : *(deletedRuleApp.second)) {
//                std::cout << "  Deleted Rule application: " << RuleApplication::toString(ruleApp) << std::endl;
//            }
//        }
        // 先应用删除，再应用插入
        applyDeltaDeletes(deltaDeleteRuleApps, ruleManager, deletedFacts);
        applyDeltaInserts(deltaInsertRuleApps, ruleManager, fact_prob);
    }

    // 用于跟踪增量变化的节点和边的集合
    std::set<NodePtr> deltaInsertNodes;
    std::set<EdgePtr> deltaInsertEdges;
    std::set<NodePtr> deltaDeleteNodes;
    std::set<EdgePtr> deltaDeleteEdges;

    std::unordered_map<NodePtr, std::set<NodePtr>> deletedFactImpactedNodes;
    std::unordered_map<NodePtr, std::set<EdgePtr>> deletedFactImpactedEdges;
    std::unordered_map<NodePtr, std::set<NodePtr>> insertedFactImpactedNodes;
    std::unordered_map<NodePtr, std::set<EdgePtr>> insertedFactImpactedEdges;

    const std::set<NodePtr>& getDeltaInsertNodes() const {
        return deltaInsertNodes;
    }

    const std::set<EdgePtr>& getDeltaInsertEdges() const {
        return deltaInsertEdges;
    }

    const std::set<NodePtr>& getDeltaDeleteNodes() const {
        return deltaDeleteNodes;
    }

    const std::set<EdgePtr>& getDeltaDeleteEdges() const {
        return deltaDeleteEdges;
    }

    const std::unordered_map<NodePtr, std::set<NodePtr>>& getNodeImpactedByDeltaDelete() const {
        return deletedFactImpactedNodes;
    }

    const std::unordered_map<NodePtr, std::set<EdgePtr>>& getEdgeImpactedByDeltaDelete() const {
        return deletedFactImpactedEdges;
    }

    const std::unordered_map<NodePtr, std::set<NodePtr>>& getNodeImpactedByDeltaInsert() const {
        return insertedFactImpactedNodes;
    }

    const std::unordered_map<NodePtr, std::set<EdgePtr>>& getEdgeImpactedByDeltaInsert() const {
        return insertedFactImpactedEdges;
    }

//    std::set<NodePtr> validNodes_;
//    std::set<NodePtr> deletedFacts_;
//    std::set<NodePtr> deletedDeterminsticFacts_;
//    std::set<NodePtr> deletedNonDeterministicFacts_;
//    const std::set<NodePtr>& getValidNodes() {
//        if (validNodes_.size() > 0) {
//            return validNodes_;
//        }
//        for (const auto& node : getNodes()) {
//            if (getDeltaDeleteNodes().count(node) == 0 && node->pruned == false) {
//                validNodes_.insert(node);
//            }
//        }
//        return validNodes_;
//    }
//
//    const std::set<NodePtr>& getDeletedFacts() = 0; // nodes - deleted nodes
//    const std::set<NodePtr>& getDeletedDeterminsticFacts() = 0; // nodes - deleted nodes
//    const std::set<NodePtr>& getDeletedNonDeterminsticFacts() = 0; // nodes - deleted nodes
};

void IncrementalDerivationGraph::applyDeltaInserts(
    const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& deltaInsertRuleApps,
    const RuleManager& ruleManager,
    const std::unordered_map<UntypedTuple, double>& fact_prob
) {
    if (deltaInsertRuleApps.empty() && fact_prob.empty()) {
        return;
    }
    FunctionTimer timer("applying delta inserts");

    // all facts
    for (const auto& [tuple, prob] : fact_prob) {
        auto node = this->findNode(tuple);
        if (node == nullptr) {
            node = createNode(tuple);
            deltaInsertNodes.insert(node);
//            std::cout << "new fact inserted: " << node->toString() << std::endl;
        }
        node->setProbability(prob);
        node->isFact = true;
    }

    for (const auto& [tuple, ruleAppSet] : deltaInsertRuleApps) {
        // 处理与该元组关联的每个规则应用
        for (const auto& ruleApp : *ruleAppSet) {
            // 检查这个规则应用的边是否已经存在
            const Rule* rule = ruleManager.getRule(ruleApp.ruleId);
            EdgePtr existingEdge = findHyperedgeFromRuleApp(ruleApp, rule->getVars());
            if (existingEdge != nullptr) {
                std::cout << existingEdge->toString() << std::endl;
            }
            assert(existingEdge == nullptr && "Delta insert edge already exists in the graph");

            // 创建规则应用对应的超边
            EdgePtr newEdge = createHyperedgeFromRuleApp(ruleApp, ruleManager);

            // 将边标记为增量插入
            deltaInsertEdges.insert(newEdge);

            // 获取输出节点
            NodePtr outputNode = newEdge->getOutput();

            // 如果这是一个重新插入的节点（之前被标记为删除）
            if (deltaDeleteNodes.find(outputNode) != deltaDeleteNodes.end()) {
                // 从删除集合中移除
                deltaDeleteNodes.erase(outputNode);
            } else {
                // 否则，将其添加到插入集合中（如果尚未添加）
//                std::cout << "Inserting new node: " << outputNode->toString() << std::endl;
//                std::cout << "Incoming edge count: " << outputNode->getIncomingEdges().size() << std::endl;
                if (!outputNode->isFact && (outputNode->pruned || outputNode->getIncomingEdges().size() == 1))
                    deltaInsertNodes.insert(outputNode);
            }
        }
    }

    for (const auto& [insertedFact, _]: fact_prob) {
        auto node = this->findNode(insertedFact);
        assert (node != nullptr && "Inserted fact node not found in the graph");
        if (node != nullptr) {
            // 记录被插入的事实影响的节点和边
            std::queue<EdgePtr> impactedEdgesQueue;
            std::set<EdgePtr> impactedEdges;
            std::set<NodePtr> impactedNodes;
            impactedNodes.insert(node);
            for (const auto& edge : node->getOutgoingEdges()) {
                impactedEdges.insert(edge);
                impactedEdgesQueue.push(edge);
            }
            while (!impactedEdgesQueue.empty()) {
                EdgePtr edge = impactedEdgesQueue.front();
                impactedEdgesQueue.pop();
                NodePtr outputNode = edge->getOutput();
                if (impactedNodes.find(outputNode) == impactedNodes.end()) {
                    impactedNodes.insert(outputNode);
                    for (const auto& edge: outputNode->getOutgoingEdges()) {
                        impactedEdges.insert(edge);
                        impactedEdgesQueue.push(edge);
                    }
                }
            }
            for (const auto& edge : impactedEdges) {
                insertedFactImpactedEdges[node].insert(edge);
            }
            for (const auto& impactedNode : impactedNodes) {
                insertedFactImpactedNodes[node].insert(impactedNode);
            }
        }
    }

    // For debugging
//    for (const auto& [insertedFact, impactedNodes] : insertedFactImpactedNodes) {
//        std::cout << "Inserted fact: " << insertedFact->toString() << " impacts nodes: ";
//        for (const auto& impactedNode : impactedNodes) {
//            std::cout << impactedNode->toString() << ", ";
//        }
//        std::cout << std::endl;
//    }
}

void IncrementalDerivationGraph::applyDeltaDeletes(
    const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& deltaDeleteRuleApps,
    const RuleManager& ruleManager,
    const std::vector<UntypedTuple>& deletedFacts
) {
    // TODO: should delete corresponding inserting and deleting edges
    if (deltaDeleteRuleApps.empty() && deletedFacts.empty()) {
        return;
    }
    FunctionTimer timer("applying delta deletes");
    std::set<UntypedTuple> deletedFactsSet(deletedFacts.begin(), deletedFacts.end());
    {
        // update impact information
        for (auto deletedFact : deletedFacts) {
            auto node = this->findNode(deletedFact);
            assert (node != nullptr && "Deleted fact node not found in the graph");
            if (node != nullptr) {
                // 记录被删除的事实影响的节点和边
                std::queue<EdgePtr> impactedEdgesQueue;
                std::set<EdgePtr> impactedEdges;
                std::set<NodePtr> impactedNodes;
                impactedNodes.insert(node);
                for (const auto& edge : node->getOutgoingEdges()) {
                    impactedEdges.insert(edge);
                    impactedEdgesQueue.push(edge);
                }
                while (!impactedEdgesQueue.empty()) {
                    EdgePtr edge = impactedEdgesQueue.front();
                    impactedEdgesQueue.pop();
                    NodePtr outputNode = edge->getOutput();
                    if (impactedNodes.find(outputNode) == impactedNodes.end()) {
//                        std::cout << "Impacting node: " << outputNode->toString() << std::endl;
                        impactedNodes.insert(outputNode);
                        for (const auto& edge: outputNode->getOutgoingEdges()) {
                            impactedEdges.insert(edge);
                            impactedEdgesQueue.push(edge);
                        }
                    }
                }
                for (const auto& edge : impactedEdges) {
                    deletedFactImpactedEdges[node].insert(edge);
                }
                for (const auto& impactedNode : impactedNodes) {
                    deletedFactImpactedNodes[node].insert(impactedNode);
                }
            }
        }
    }

    // 跟踪需要删除的节点
    std::vector<NodePtr> nodesToRemove;

    for (const auto& [tuple, ruleAppSet] : deltaDeleteRuleApps) {
        for (const auto& ruleApp : *ruleAppSet) {
            // 查找要删除的边 - 直接使用map查找
            const Rule* rule = ruleManager.getRule(ruleApp.ruleId);
            const std::vector<std::string>& vars = rule->getVars();
            EdgePtr existingEdge = findHyperedgeFromRuleApp(ruleApp, vars);
            if (existingEdge == nullptr) {
                std::cout << "Did not find the edge to delete: "
                          << createEdgeKey(ruleApp.ruleId, vars, ruleApp.varValuesPure) << std::endl;
//                continue;
                assert(false && "Did not find the edge to delete");
            }

            // 将边标记为增量删除
            deltaDeleteEdges.insert(existingEdge);

            // 获取输出节点和输入节点
            NodePtr outputNode = existingEdge->getOutput();
            const std::vector<NodePtr>& inputNodes = existingEdge->getInputs();

            // 从输出节点的入边列表中移除这条边
            auto& inEdges = outputNode->getIncomingEdges();
            inEdges.erase(std::remove(inEdges.begin(), inEdges.end(), existingEdge), inEdges.end());

            // 从每个输入节点的出边列表中移除这条边
            for (const auto& inputNode : inputNodes) {
                // 检查节点是否还在图中 - 使用map直接查找
                if (tupleToNodeMap.find(inputNode->getTuple()) != tupleToNodeMap.end()) {
                    auto& outEdges = inputNode->getOutgoingEdges();
                    outEdges.erase(std::remove(outEdges.begin(), outEdges.end(), existingEdge), outEdges.end());
                }
            }

            // 从图的边列表中移除这条边
            edges.erase(existingEdge);
//            auto& allEdges = edges;
//            allEdges.erase(std::remove(allEdges.begin(), allEdges.end(), existingEdge), allEdges.end());

            // 从edgeKeyToEdgeMap中移除这条边
            std::string edgeKey = createEdgeKey(ruleApp.ruleId, vars, ruleApp.varValuesPure);
            edgeKeyToEdgeMap.erase(edgeKey);

            // 检查输出节点是否还有其他导出路径
            // check whether the output node is a deleted fact
//            std::cout << "Trying to delete output node: " << outputNode->toString() << std::endl;
            if ((!outputNode->isFact || deletedFactsSet.count(outputNode->getTuple())) && outputNode->getIncomingEdges().empty()) {
//                std::cout << "Removing output node with no incoming edges: " << outputNode->toString() << std::endl;
                // 对于派生节点，如果没有入边，应该从图中删除
                // 如果这是一个新插入的节点被删除
                if (deltaInsertNodes.find(outputNode) != deltaInsertNodes.end()) {
                    // 从插入集合中移除
                    deltaInsertNodes.erase(outputNode);
                } else {
                    // 添加到删除集合中
                    deltaDeleteNodes.insert(outputNode);
                    nodesToRemove.push_back(outputNode);
                }
            }

        }
    }

    // 最后，从图中删除没有入边的派生节点
    for (const auto& nodeToRemove : nodesToRemove) {
        // 从映射中移除
        tupleToNodeMap.erase(nodeToRemove->getTuple());
//        std::cout << "removing node " << nodeToRemove->toString() << std::endl;
        // 从节点列表中移除
//        nodes.erase(std::remove(nodes.begin(), nodes.end(), nodeToRemove), nodes.end());
        nodes.erase(nodeToRemove);
    }

    for (const auto& tuple : deletedFacts) {
        auto node = this->findNode(tuple);
        // it's possible that a deleted fact has deleted derivation...
        if (node != nullptr) {
            // 从映射中移除

//            node->isFact = false;
            tupleToNodeMap.erase(node->getTuple());
            deltaDeleteNodes.insert(node);
            if (!node->getIncomingEdges().empty()) {
                // the node changes from an input fact to a derived node
                node->isFact = false;
                node->pruned = true; // pretend to be pruned
                deltaInsertNodes.insert(node); // but still keep it in the graph
                for (auto edge: node->getIncomingEdges()) {
                    if (deltaDeleteEdges.find(edge) == deltaDeleteEdges.end()) {
                        deltaInsertEdges.insert(edge);
                    }
                }
            } else {
                nodes.erase(node);
                nodesToRemove.push_back(node);
            }
            // 从节点列表中移除

        } else {
            // It's possible that the deleted fact has been removed from the graph; but it has to be with deleted derivations
            std::cout << "deleted fact not found: " << tuple.toString() << std::endl;
            // assert (false && "deleted fact not found");
        }
    }

    for (auto& [fact, impactedEdges]: deletedFactImpactedEdges) {
        std::set<EdgePtr> newImpactedEdges;
        for (const auto& impactedEdge : impactedEdges) {
            // 如果这个边已经被标记为删除，则不需要再次添加
            if (deltaDeleteEdges.find(impactedEdge) != deltaDeleteEdges.end()) {
                continue;
            }
            // 否则，将其添加到新的影响边集合中
            newImpactedEdges.insert(impactedEdge);
        }
        // 更新影响边集合
        impactedEdges = std::move(newImpactedEdges);
    }
    for (auto& [fact, impactedNodes]: deletedFactImpactedNodes) {
        std::set<NodePtr> newImpactedNodes;
        for (const auto& impactedNode : impactedNodes) {
            // 如果这个节点已经被标记为删除，则不需要再次添加
            if (deltaDeleteNodes.find(impactedNode) != deltaDeleteNodes.end()) {
                continue;
            }
            // 否则，将其添加到新的影响节点集合中
            newImpactedNodes.insert(impactedNode);
//            std::cout << "Deleted fact: " << fact->toString() << " impacts node: " << impactedNode->toString() << std::endl;
        }
        // 更新影响节点集合
        impactedNodes = newImpactedNodes;
    }
//    for (const auto& [deletedFact, impactedNodes] : deletedFactImpactedNodes) {
//        std::cout << "Deleted fact: " << deletedFact->toString() << " impacts nodes: ";
//        for (const auto& impactedNode : impactedNodes) {
//            std::cout << impactedNode->toString() << ", ";
//        }
//        std::cout << std::endl;
//    }
}

IncSubgraphView IncrementalDerivationGraph::prune(const std::vector<souffle::Relation*>& outputRelations) {
    std::vector<std::string> outputRelationNames;
    for (const auto* rel : outputRelations) {
        outputRelationNames.push_back(rel->getName());
    }
    return prune(outputRelationNames);
}

// pruning is not incremental for now
IncSubgraphView IncrementalDerivationGraph::prune(const std::vector<std::string>& outputRelations) {
    std::unordered_set outputRelationNames(outputRelations.begin(), outputRelations.end());
    // 标记可达的节点和边
    std::unordered_set<NodePtr> reachableNodes;
    std::unordered_set<EdgePtr> reachableEdges;
    std::queue<NodePtr> workQueue;

    std::unordered_set<NodePtr> outputNodes;

    // 初始化：从所有输出 relation 的节点出发
    for (const auto& node : nodes) {
        if (outputRelationNames.count(node->getTuple().relation_name) > 0) {
            std::cout << "Found output node: " << node->getTuple().toString() << std::endl;
            reachableNodes.insert(node);
            workQueue.push(node);
            node->needOutput = true;
            outputNodes.insert(node);
        } else if (node -> isQueryNode()) {
            reachableNodes.insert(node);
            workQueue.push(node);
            node->needOutput = true;
            outputNodes.insert(node);
        }
        node->currentRefCount = 0;
    }

    for (const auto& edge : edges) {
        edge->currentRefCount = 0;
    }

    // register reference count
    for (const auto& node : outputNodes) {
        std::queue<NodePtr> q;
        std::unordered_set<NodePtr> visited;
        q.push(node);
        while (!q.empty()) {
            NodePtr n = q.front(); q.pop();
            visited.insert(n);
            n->currentRefCount += 1;
            for (const auto& edge : n->getIncomingEdges()) {
                edge->currentRefCount += 1;
                for (const auto& inputNode : edge->getInputs()) {
                    if (visited.count(inputNode) == 0) {
                        q.push(inputNode);
                    }
                }
            }
        }
    }

    // TODO
    for (const auto& node : nodes) {
        if (node->hasEvidence() && reachableNodes.insert(node).second) {
            std::cout << "Found evidence node: " << node->toString()
                      << " with value " << (node->getEvidenceValue() ? "true" : "false")
                      << std::endl;
            workQueue.push(node);
        }
    }

    // 反向 BFS 遍历
    while (!workQueue.empty()) {
        NodePtr current = workQueue.front();
        workQueue.pop();
        if (current->isFact) {
            continue;  // currently skip input facts
        }
        for (const auto& edge : current->getIncomingEdges()) {
            if (std::find(edge->getInputs().begin(), edge->getInputs().end(), current) != edge->getInputs().end()) {
                continue;  // if the edge reports self-dependency, then it is redundant
            }
            reachableEdges.insert(edge);
            for (const auto& inputNode : edge->getInputs()) {
                if (reachableNodes.insert(inputNode).second) {
                    workQueue.push(inputNode);
                }
            }
        }
    }

    std::unordered_set<NodePtr> newNodes;
    for (const auto& node : nodes) {
        if (reachableNodes.count(node)){
            newNodes.insert(node);
            if (node->pruned) {
//                std::cout << "reusing a pruned node: " << node->getTuple().toString() << std::endl;
                deltaInsertNodes.insert(node);
            }
            node->pruned = false;  // reset pruned flag
        } else {
            node->pruned = true;
        }
    }

    std::unordered_set<EdgePtr> newEdges;
    for (const auto& edge : edges) {
        if (reachableEdges.count(edge)) {
            newEdges.insert(edge);
            if (edge->pruned) {
//                std::cout << "reusing a pruned edge: " << edge->toString() << std::endl;
                deltaInsertEdges.insert(edge);
            }
            edge->pruned = false;  // reset pruned flag
        } else {
            edge->pruned = true;
        }
    }

    // 过滤节点和边
    std::set<NodePtr> newDeltaDeletedNodes;
    std::set<EdgePtr> newDeltaDeletedEdges;
    for (const auto& deletedNode : deltaDeleteNodes) {
        if (!deletedNode->pruned) {
            newDeltaDeletedNodes.insert(deletedNode);
        }
    }
    for (const auto& deletedEdge : deltaDeleteEdges) {
        if (!deletedEdge->pruned) {
            newDeltaDeletedEdges.insert(deletedEdge);
        }
    }




    // TODO: update nodes incoming and outgoing edges
//    for (const auto& node : newNodes) {
//        std::vector<EdgePtr> newIncomingEdges;
//        std::vector<EdgePtr> newOutgoingEdges;
//        for (const auto& edge : node->getIncomingEdges()) {
//            if (reachableEdges.count(edge)) {
//                newIncomingEdges.push_back(edge);
//            }
//        }
//        for (const auto& edge : node->getOutgoingEdges()) {
//            if (reachableEdges.count(edge)) {
//                newOutgoingEdges.push_back(edge);
//            }
//        }
//        node->incomingEdges = std::move(newIncomingEdges);
//        node->outgoingEdges = std::move(newOutgoingEdges);
//    }

    std::set<NodePtr> newDeltaInsertedNodes;
    std::set<EdgePtr> newDeltaInsertedEdges;

    for (const auto& insertedNode : deltaInsertNodes) {
        if (reachableNodes.count(insertedNode)) {
            newDeltaInsertedNodes.insert(insertedNode);
        }
    }
    for (const auto& insertedEdge : deltaInsertEdges) {
        if (reachableEdges.count(insertedEdge)) {
            newDeltaInsertedEdges.insert(insertedEdge);
        }
    }


    std::unordered_map<NodePtr, std::set<NodePtr>> newInsertedFactImpactedNodes;
    std::unordered_map<NodePtr, std::set<EdgePtr>> newInsertedFactImpactedEdges;
    for (const auto& [fact, impactedNodes] : insertedFactImpactedNodes) {
        if (!reachableNodes.count(fact)) {
            continue;
        }
        std::set<NodePtr> newImpactedNodes;
        for (const auto& impactedNode : impactedNodes) {
            if (reachableNodes.count(impactedNode)) {
                newImpactedNodes.insert(impactedNode);
            }
        }
        newInsertedFactImpactedNodes[fact] = std::move(newImpactedNodes);
    }
    for (const auto& [fact, impactedEdges] : insertedFactImpactedEdges) {
        if (!reachableNodes.count(fact)) {
            continue;
        }
        std::set<EdgePtr> newImpactedEdges;
        for (const auto& impactedEdge : impactedEdges) {
            if (reachableEdges.count(impactedEdge)) {
                newImpactedEdges.insert(impactedEdge);
            }
        }
        newInsertedFactImpactedEdges[fact] = std::move(newImpactedEdges);
    }
    std::unordered_map<NodePtr, std::set<NodePtr>> newDeletedFactImpactedNodes;
    std::unordered_map<NodePtr, std::set<EdgePtr>> newDeletedFactImpactedEdges;
    for (const auto& [fact, impactedNodes] : deletedFactImpactedNodes) {
        if (!reachableNodes.count(fact)) {
            continue;
        }
        std::set<NodePtr> newImpactedNodes;
        for (const auto& impactedNode : impactedNodes) {
            if (reachableNodes.count(impactedNode)) {
                newImpactedNodes.insert(impactedNode);
            }
        }
        newDeletedFactImpactedNodes[fact] = std::move(newImpactedNodes);
    }
    for (const auto& [fact, impactedEdges] : deletedFactImpactedEdges) {
        if (!reachableNodes.count(fact)) {
            continue;
        }
        std::set<EdgePtr> newImpactedEdges;
        for (const auto& impactedEdge : impactedEdges) {
            if (reachableEdges.count(impactedEdge)) {
                newImpactedEdges.insert(impactedEdge);
            }
        }
        newDeletedFactImpactedEdges[fact] = std::move(newImpactedEdges);
    }

    auto view = IncSubgraphView(std::move(newNodes), std::move(newEdges),
                           std::move(newDeltaInsertedNodes),
                           std::move(newDeltaInsertedEdges),
                           std::move(newDeltaDeletedNodes),
                           std::move(newDeltaDeletedEdges),
                            std::move(newDeletedFactImpactedNodes),
                            std::move(newDeletedFactImpactedEdges),
                            std::move(newInsertedFactImpactedNodes),
                            std::move(newInsertedFactImpactedEdges)
            );
    view.dumpStatisticsInc(std::cout);
    return view;
}

void IncrementalDerivationGraphViewInterface::dumpDotInc(const std::string& filename) const {
    std::ofstream out(filename);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    out << "digraph IncrementalDerivationGraph {\n";
    out << "  rankdir=LR;\n";

    // 添加图例
    out << "  subgraph cluster_legend {\n";
    out << "    label=\"Legend\";\n";
    out << "    style=filled;\n";
    out << "    color=lightgrey;\n";
    out << "    node [style=filled];\n";
    out << "    \"Normal Node\" [fillcolor=lightblue];\n";
    out << "    \"Inserted Node\" [fillcolor=lightgreen];\n";
    out << "    \"Deleted Node\" [fillcolor=pink, style=\"filled,dashed\"];\n";
    out << "    \"Normal Node\" -> \"Normal Edge\" [color=black];\n";
    out << "    \"Inserted Node\" -> \"Inserted Edge\" [color=green];\n";
    out << "    \"Deleted Node\" -> \"Deleted Edge\" [color=red, style=dashed];\n";
    out << "    \"Normal Edge\" [shape=point, fillcolor=black, width=0.2];\n";
    out << "    \"Inserted Edge\" [shape=point, fillcolor=green, width=0.2];\n";
    out << "    \"Deleted Edge\" [shape=point, fillcolor=red, width=0.2];\n";
    out << "  }\n\n";

    // 正常节点 - 蓝色填充
    out << "  // Regular nodes\n";
    out << "  node [shape=box, style=filled, fillcolor=lightblue];\n";

    for (const auto& node : getNodes()) {
        // 忽略插入和删除的节点，它们将单独处理
        if (getDeltaInsertNodes().find(node) == getDeltaInsertNodes().end() &&
            getDeltaDeleteNodes().find(node) == getDeltaDeleteNodes().end()) {
            out << "  node" << node->getId() << " [label=\""
                << node->getTuple().toString();

            // 如果有概率信息，添加到标签中
            if (node->isFact) {
                out << "\\nP=" << node->getProbability();
            }

            out << "\"];\n";
        }
    }

    std::cout << "Dumping " << getNodes().size() << " nodes, "
              << getEdges().size() << " edges, "
              << getDeltaInsertNodes().size() << " inserted nodes, "
              << getDeltaInsertEdges().size() << " inserted edges, "
              << getDeltaDeleteNodes().size() << " deleted nodes, "
              << getDeltaDeleteEdges().size() << " deleted edges." << std::endl;

    // 插入的节点 - 绿色填充
    out << "\n  // Inserted nodes\n";
    out << "  node [shape=box, style=filled, fillcolor=lightgreen];\n";

    for (const auto& node : getDeltaInsertNodes()) {
        std::cout << "DumpDotInc Inserting node: " << node->getTuple().toString() << std::endl;
        out << "  node" << node->getId() << " [label=\""
            << node->getTuple().toString();

        // 如果有概率信息，添加到标签中
        if (node->isFact) {
            out << "\\nP=" << node->getProbability();
        }

        out << "\"];\n";
    }

    // 删除的节点 - 红色虚线填充
    out << "\n  // Deleted nodes\n";
    out << "  node [shape=box, style=\"filled,dashed\", fillcolor=pink];\n";

    for (const auto& node : getDeltaDeleteNodes()) {
        out << "  node" << node->getId() << " [label=\""
            << node->getTuple().toString();

        // 如果有概率信息，添加到标签中
        if (node->isFact) {
            out << "\\nP=" << node->getProbability();
        }

        out << "\"];\n";
    }

    // 正常边 - 黑色点和线
    out << "\n  // Regular edges\n";
    out << "  node [shape=point, fillcolor=black, width=0.2];\n";

    for (const auto& edge : getEdges()) {
        // 忽略插入和删除的边，它们将单独处理
        if (getDeltaInsertEdges().find(edge) == getDeltaInsertEdges().end() &&
            getDeltaDeleteEdges().find(edge) == getDeltaDeleteEdges().end()) {

            // 添加规则ID到标签（如果有）
            std::string edgeLabel = "";
            if (edge->getRule() != nullptr) {
                edgeLabel = " [label=\"R" + std::to_string(edge->getRule()->getRuleId()) + "\"";

                // 如果概率不是1.0，添加概率信息
                if (edge->getProbability() < 1.0) {
                    edgeLabel += ", tooltip=\"P=" + std::to_string(edge->getProbability()) + "\"";
                }

                edgeLabel += "]";
            }
            out << "  edge" << edge->getId() << edgeLabel << ";\n";

            // 输入节点到边的连接
            for (const auto& input : this->getInputs(edge)) {
                // 检查是否是否定的体原子
                size_t inputIdx = std::distance(this->getInputs(edge).begin(),
                                 std::find(this->getInputs(edge).begin(), this->getInputs(edge).end(), input));
                bool isNegated = inputIdx < this->getBodyNegations(edge).size() ?
                                 this->getBodyNegations(edge)[inputIdx] : false;

                // 如果是否定的，使用虚线和不同的箭头样式
                std::string edgeStyle = isNegated ? " [style=dashed, arrowhead=odot]" : "";

                out << "  node" << input->getId()
                    << " -> edge" << edge->getId() << edgeStyle << ";\n";
            }

            // 边到输出节点的连接
            out << "  edge" << edge->getId()
                << " -> node" << this->getOutput(edge)->getId() << ";\n";
        }
    }

    // 插入的边 - 绿色点和线
    out << "\n  // Inserted edges\n";
    out << "  node [shape=point, fillcolor=green, width=0.2];\n";

    for (const auto& edge : getDeltaInsertEdges()) {
        // 添加规则ID到标签（如果有）
        std::string edgeLabel = "";
        if (edge->getRule() != nullptr) {
            edgeLabel = " [label=\"R" + std::to_string(edge->getRule()->getRuleId()) + "\"";

            // 如果概率不是1.0，添加概率信息
            if (edge->getProbability() < 1.0) {
                edgeLabel += ", tooltip=\"P=" + std::to_string(edge->getProbability()) + "\"";
            }

            edgeLabel += ", color=green]";
        } else {
            edgeLabel = " [color=green]";
        }
        out << "  edge" << edge->getId() << edgeLabel << ";\n";

        // 输入节点到边的连接 - 绿色
        for (const auto& input : this->getInputs(edge)) {
            // 检查是否是否定的体原子
            size_t inputIdx = std::distance(this->getInputs(edge).begin(),
                             std::find(this->getInputs(edge).begin(), this->getInputs(edge).end(), input));
            bool isNegated = inputIdx < this->getBodyNegations(edge).size() ?
                             this->getBodyNegations(edge)[inputIdx] : false;

            // 如果是否定的，使用虚线和不同的箭头样式，但仍然保持绿色
            std::string edgeStyle = isNegated ?
                " [color=green, style=dashed, arrowhead=odot]" : " [color=green]";

            out << "  node" << input->getId()
                << " -> edge" << edge->getId() << edgeStyle << ";\n";
        }

        // 边到输出节点的连接 - 绿色
        out << "  edge" << edge->getId()
            << " -> node" << this->getOutput(edge)->getId() << " [color=green];\n";
    }

    // 删除的边 - 红色虚线点和线
    out << "\n  // Deleted edges\n";
    out << "  node [shape=point, fillcolor=red, width=0.2];\n";

    for (const auto& edge : getDeltaDeleteEdges()) {
        // 添加规则ID到标签（如果有）
        std::string edgeLabel = "";
        if (edge->getRule() != nullptr) {
            edgeLabel = " [label=\"R" + std::to_string(edge->getRule()->getRuleId()) + "\"";

            // 如果概率不是1.0，添加概率信息
            if (edge->getProbability() < 1.0) {
                edgeLabel += ", tooltip=\"P=" + std::to_string(edge->getProbability()) + "\"";
            }

            edgeLabel += ", color=red, style=dashed]";
        } else {
            edgeLabel = " [color=red, style=dashed]";
        }

        out << "  edge" << edge->getId() << edgeLabel << ";\n";

        // 输入节点到边的连接 - 红色虚线
        for (const auto& input : edge->getInputs()) {
            // 检查是否是否定的体原子
            size_t inputIdx = std::distance(edge->getInputs().begin(),
                             std::find(edge->getInputs().begin(), edge->getInputs().end(), input));
            bool isNegated = inputIdx < edge->getBodyNegations().size() ?
                             edge->getBodyNegations()[inputIdx] : false;

            // 如果是否定的，使用双重虚线和不同的箭头样式，但仍然保持红色
            std::string edgeStyle = isNegated ?
                " [color=red, style=\"dashed,dotted\", arrowhead=odot]" : " [color=red, style=dashed]";

            out << "  node" << input->getId()
                << " -> edge" << edge->getId() << edgeStyle << ";\n";
        }

        // 边到输出节点的连接 - 红色虚线
        out << "  edge" << edge->getId()
            << " -> node" << edge->getOutput()->getId() << " [color=red, style=dashed];\n";
    }

    out << "}\n";
    out.close();
}

inline std::unordered_map<NodePtr, double> probResult;

void dumpProbabilities(
    std::unordered_map<NodePtr, double>& nodeProbabilities, const std::string& outputDir = "./output/",
          const std::string& fileName = "facts") {
    std::ofstream outputFile(
        outputDir + "/" + fileName + ".prob"
    );
    outputFile << std::setprecision(8);
    std::vector<NodePtr> sortedNodes;
    for (const auto& [node, prob] : nodeProbabilities) {
        sortedNodes.push_back(node);
    }
    std::sort(sortedNodes.begin(), sortedNodes.end(),
              [](const NodePtr& a, const NodePtr& b) {
                  return a->getTuple().toString() < b->getTuple().toString();
              });
    for (auto& node: sortedNodes) {
        auto prob = nodeProbabilities[node];
        if (node->needOutput) {
            outputFile << node->getTuple().toString() << " : " << prob << std::endl;
        }
    }

}

struct CycleDependencyGraph {
    const DerivationGraphViewInterface& graph;

    std::vector<std::unordered_set<NodePtr>> nodeCycles;
    std::vector<std::unordered_set<EdgePtr>> edgeCycles;

    std::unordered_map<NodePtr, size_t> nodeToCycleIndex;
    std::unordered_map<EdgePtr, size_t> edgeToCycleIndex;

    std::vector<std::unordered_set<size_t>> dependencies;  // edges: i -> j means i depends on j
    std::vector<std::unordered_set<size_t>> reverseDependencies;
    std::vector<size_t> inDegrees;

    std::unordered_map<NodePtr, size_t> nodeDepthsGlobal;
    std::unordered_map<EdgePtr, size_t> edgeDepthsGlobal;

    explicit CycleDependencyGraph(const DerivationGraphViewInterface& g) : graph(g) {
        computeSCCs();
        computeDependencies();
        computeDepths();
    }

    void dumpCycles(std::ostream& out = std::cout) const {
        out << "Number of SCCs: " << nodeCycles.size() << "\n";
        for (size_t i = 0; i < nodeCycles.size(); ++i) {
            out << "Cycle " << i << " nodes:\n";
            for (const auto& n : nodeCycles[i]) {
                out << "  " << n->toString() << "\n";
                if (nodeDepthsGlobal.count(n) > 0) {
                    out << "    Depth: " << nodeDepthsGlobal.at(n) << "\n";
                } else {
                    out << "    Depth: N/A\n";
                }
            }
            out << "Cycle " << i << " edges:\n";
            for (const auto& e : edgeCycles[i]) {
                out << "  " << e->toString() << "\n";
                if (edgeDepthsGlobal.count(e) > 0) {
                    out << "    Depth: " << edgeDepthsGlobal.at(e) << "\n";
                } else {
                    out << "    Depth: N/A\n";
                }
            }
            // depth

            out << "Depends on: ";
            for (auto d : dependencies[i]) {
                out << d << " ";
            }
            out << "\nReverse depends on: ";
            for (auto d : reverseDependencies[i]) {
                out << d << " ";
            }
            out << "\nIn-degree: " << inDegrees[i] << "\n";
            out << "\n----\n";
        }
    }


    void dumpDot(const std::string& filename) const {
        std::ofstream out(filename);
        if (!out.is_open()) {
            throw std::runtime_error("Cannot open file: " + filename);
        }

        out << "digraph CycleDependencyGraph {\n";
        out << "  rankdir=LR;\n";
        out << "  node [shape=box, style=filled, fillcolor=lightyellow];\n";

        // 输出每个 SCC 节点
        for (size_t i = 0; i < nodeCycles.size(); ++i) {
            out << "  C" << i << " [label=\"Cycle " << i << "\\n";
            size_t count = 0;
            for (const auto& n : nodeCycles[i]) {
                out << n->getTuple().toString();
                if (++count >= 3) { out << "\\n..."; break; }
                out << "\\n";
            }
            out << "\"];\n";
        }

        // 输出依赖边
        for (size_t i = 0; i < dependencies.size(); ++i) {
            for (auto dep : dependencies[i]) {
                out << "  C" << dep << " -> C" << i << ";\n";
            }
        }

        out << "}\n";
        out.close();
    }

private:
    void computeSCCs() {
        size_t index = 0, currentSCC = 0;
        std::unordered_map<NodePtr, size_t> indices, lowlinks;
        std::stack<NodePtr> stack;
        std::unordered_set<NodePtr> onStack;

        std::function<void(NodePtr)> strongconnect = [&](NodePtr v) {
            indices[v] = lowlinks[v] = index++;
            stack.push(v);
            onStack.insert(v);

            for (auto& edge : graph.getOutgoingEdges(v)) {
                NodePtr w = graph.getOutput(edge);
                if (!indices.count(w)) {
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
                    nodeToCycleIndex[w] = currentSCC;
                } while (w != v);
                nodeCycles.push_back(std::move(scc));
                edgeCycles.emplace_back();
                ++currentSCC;
            }
        };

        for (auto& node : graph.getNodes()) {
            if (!indices.count(node)) {
                strongconnect(node);
            }
        }

        for (auto& edge : graph.getEdges()) {
            NodePtr out = graph.getOutput(edge);
            if (nodeToCycleIndex.count(out)) {
                size_t cid = nodeToCycleIndex[out];
                edgeToCycleIndex[edge] = cid;
                edgeCycles[cid].insert(edge);
            }
        }
    }

    void computeDependencies() {
        size_t n = nodeCycles.size();
        dependencies.resize(n);
        reverseDependencies.resize(n);
        inDegrees.resize(n, 0);  // ensure all entries initialized

        for (auto& edge : graph.getEdges()) {
            size_t outCid = nodeToCycleIndex[graph.getOutput(edge)];
            for (auto& input : graph.getInputs(edge)) {
                size_t inCid = nodeToCycleIndex[input];
                if (inCid != outCid && !dependencies[outCid].count(inCid)) {
                    dependencies[outCid].insert(inCid);             // out depends on in
                    reverseDependencies[inCid].insert(outCid);      // in is depended on by out
                    inDegrees[outCid]++;
                }
            }
        }
    }
    void computeDepths() {
        nodeDepthsGlobal.clear();
        edgeDepthsGlobal.clear();

        for (size_t cid = 0; cid < nodeCycles.size(); ++cid) {
            const auto& nodes = nodeCycles[cid];
            const auto& edges = edgeCycles[cid];

            std::queue<NodePtr> q;
            for (NodePtr node : nodes) {
                bool isEntry = false;
                for (auto& inEdge : graph.getIncomingEdges(node)) {
                    for (auto& inNode : graph.getInputs(inEdge)) {
                        if (nodeToCycleIndex[inNode] != cid) {
                            isEntry = true;
                            break;
                        }
                    }
                }
                if (node->isFact || graph.getIncomingEdges(node).empty()) isEntry = true;
                if (isEntry) {
                    nodeDepthsGlobal[node] = 0;
                    q.push(node);
                }
            }

            while (!q.empty()) {
                NodePtr curr = q.front(); q.pop();
                size_t d = nodeDepthsGlobal[curr];
                for (auto& outEdge : graph.getOutgoingEdges(curr)) {
                    NodePtr out = graph.getOutput(outEdge);
                    if (nodeToCycleIndex[out] == cid) {
                        if (!nodeDepthsGlobal.count(out) || nodeDepthsGlobal[out] > d + 1) {
                            nodeDepthsGlobal[out] = d + 1;
                            q.push(out);
                        }
                    }
                }
            }

            for (auto edge : edges) {
                size_t d = 0;
                for (auto in : graph.getInputs(edge)) {
                    if (nodeToCycleIndex[in] == cid && nodeDepthsGlobal.count(in)) {
                        d = std::max(d, nodeDepthsGlobal[in] + 1);
                    }
                }
                edgeDepthsGlobal[edge] = d;
            }
        }
    }
};

void DerivationGraphViewInterface::writeGraphStatsJson() const {
    static size_t s_idx = 0;  // 控制输出文件 index
    const std::string dir = "output";
    const std::string path = dir + "/graph-" + std::to_string(s_idx++) + ".json";

#if __cplusplus >= 201703L
    // 如目录不存在则创建（C++17）
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
#endif

    std::ofstream out(path);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot open file: " + path);
    }

    const auto& nodes = getNodes();
    const auto& edges = getEdges();

    // 基本计数
    const size_t num_nodes = nodes.size();
    const size_t num_edges = edges.size();

    // #queries：基于 Node 的 isQuery 标志
    size_t num_queries = 0;

    // in-degree（排除 facts）
    size_t indeg_sum = 0, indeg_cnt = 0, indeg_max = 0;

    // out-degree（仅统计 outdeg>0 的节点）
    size_t outdeg_sum = 0, outdeg_cnt = 0, outdeg_max = 0;

    for (const auto& n : nodes) {
        const size_t indeg = getIncomingEdges(n).size();
        const size_t outdeg = getOutgoingEdges(n).size();

        // avg_in_degree: 排除 input facts
        if (!n->isFact) {
            indeg_sum += indeg;
            ++indeg_cnt;
            if (indeg > indeg_max) indeg_max = indeg;
        }

        // avg_out_degree: 排除 outdeg==0
        if (outdeg > 0) {
            outdeg_sum += outdeg;
            ++outdeg_cnt;
            if (outdeg > outdeg_max) outdeg_max = outdeg;
        }

        if (n->isQuery) ++num_queries;
    }

    const double avg_in_degree  = indeg_cnt  ? static_cast<double>(indeg_sum)  / indeg_cnt  : 0.0;
    const double avg_out_degree = outdeg_cnt ? static_cast<double>(outdeg_sum) / outdeg_cnt : 0.0;

    // 超边输入数（hyperedge arity）
    size_t inp_sum = 0, inp_cnt = 0, inp_max = 0;
    for (const auto& e : edges) {
        const size_t k = getInputs(e).size();
        inp_sum += k;
        ++inp_cnt;
        if (k > inp_max) inp_max = k;
    }
    const double avg_hyperedge_inputs = inp_cnt ? static_cast<double>(inp_sum) / inp_cnt : 0.0;

    // 环统计（基于 SCC；仅统计 |SCC|>=2 的非平凡环）
    CycleDependencyGraph cdg(*this);
    size_t cycles = 0, cyc_size_sum = 0, cyc_size_max = 0;
    for (const auto& scc : cdg.nodeCycles) {
        const size_t s = scc.size();
        if (s >= 2) {
            ++cycles;
            cyc_size_sum += s;
            if (s > cyc_size_max) cyc_size_max = s;
        }
    }
    const double avg_cycle_size = cycles ? static_cast<double>(cyc_size_sum) / cycles : 0.0;

    // 只输出数值（键是字符串，值全为数字）
    out.setf(std::ios::fixed);
    out << std::setprecision(6);
    out << "{\n"
        << "  \"nodes\": " << num_nodes << ",\n"
        << "  \"edges\": " << num_edges << ",\n"
        << "  \"queries\": " << num_queries << ",\n"
        << "  \"avg_in_degree\": " << avg_in_degree << ",\n"
        << "  \"max_in_degree\": " << indeg_max << ",\n"
        << "  \"avg_out_degree\": " << avg_out_degree << ",\n"
        << "  \"max_out_degree\": " << outdeg_max << ",\n"
        << "  \"avg_hyperedge_inputs\": " << avg_hyperedge_inputs << ",\n"
        << "  \"max_hyperedge_inputs\": " << inp_max << ",\n"
        << "  \"cycles\": " << cycles << ",\n"
        << "  \"avg_cycle_size\": " << avg_cycle_size << ",\n"
        << "  \"max_cycle_size\": " << cyc_size_max << "\n"
        << "}\n";
}


#endif //DERIVATIONGRAPH_H
