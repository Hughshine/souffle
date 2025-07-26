#ifndef DERIVATIONGRAPH_H
#define DERIVATIONGRAPH_H

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <queue>
#include "souffle/Derivation.h"
#include "souffle/problog/Rule.h"
#include "souffle/problog/RuleManager.h"
#include "souffle/RamTypes.h"
#include "souffle/SouffleInterface.h"

// example rule application sets
// rule 1: path(x,y) :- edge(x,y).
// rule 2: path(x,y) :- path(x,z), edge(z,y).
// facts: edge(1,2), edge(2,3)
// (derived) paths: path(1,2), path(2,3), path(1,3)

// Forward declarations
class Node;
class Hyperedge;
class DerivationGraph;
struct CycleDependencyGraph;

using NodePtr = std::shared_ptr<Node>;
using EdgePtr = std::shared_ptr<Hyperedge>;

// TODO: derivation graph now does not support negation...

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
        return tuple.toString();
//        std::stringstream ss;
//        ss << "Node(" << tuple.toString() << ")";
//        return ss.str();
    }
    bool isFact = false;
    bool pruned = false;
    bool needOutput = false;

private:
    explicit Node(const UntypedTuple& t, size_t nodeId, double prob = 1.0)
        : tuple(t), id(nodeId), probability(prob) {}

    UntypedTuple tuple;
    std::vector<EdgePtr> incomingEdges;
    std::vector<EdgePtr> outgoingEdges;
    size_t id;
    double probability;

    void addIncomingEdge(EdgePtr edge);
    void addOutgoingEdge(EdgePtr edge);
};

class Hyperedge {
public:
    friend class DerivationGraph;

    const std::vector<NodePtr>& getInputs() const { return inputs; }
    NodePtr getOutput() const { return output; }
    size_t getId() const { return id; }
    const Rule* getRule() const { return rule; }
    const std::vector<bool>& getBodyNegations() const { return bodyNegations; }

    double getProbability() const { return probability; }

    std::string toString() const {
//        if (rule == nullptr) {
//            return "Hyperedge(" + std::to_string(id) + ")";
//        } else {
//
//            return "Rule" + std::to_string(rule->getRuleId()) + "("+ std::to_string(id) + ")";
//        }
    // inputs to outputs
        std::stringstream ss;
        ss << "Hyperedge(" << id << ")[";
        ss << "rule" << ((rule)?rule->getRuleId():-1) << ",";
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
private:
    Hyperedge(const std::vector<NodePtr>& inputs, NodePtr output, size_t edgeId, RuleApplication ruleApp)
        : inputs(inputs), output(output), id(edgeId), rule(nullptr), ruleApp(ruleApp) {
        assert (false);
    }
    Hyperedge(const std::vector<NodePtr>& inputs, NodePtr output, size_t edgeId, const Rule* rule, std::vector<bool>& bodyNegations, RuleApplication ruleApp)
        : inputs(inputs), output(output), id(edgeId), rule(rule), ruleApp(ruleApp) {
        if (rule) {
            probability = rule->getProbability();
        }
        if (bodyNegations.size() > 0) {
            this->bodyNegations = bodyNegations;
        } else {
            this->bodyNegations = std::vector<bool>(rule->getBodyAtoms().size(), false);
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

    std::vector<EdgePtr> getIncomingEdges(NodePtr node) const;
    std::vector<EdgePtr> getOutgoingEdges(NodePtr node) const;
    std::vector<NodePtr> getInputs(EdgePtr edge) const;
    NodePtr getOutput(EdgePtr edge) const;
    std::vector<bool> getBodyNegations(EdgePtr edge) const;

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

NodePtr DerivationGraphViewInterface::getOutput(EdgePtr edge) const {
    NodePtr out = edge->getOutput();
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
    // deletion impacted
    // insertion impacted

    void dumpDotInc(const std::string& filename) const;
    void dumpStatisticsInc(std::ostream& out) const {
        out << "IncrementalDerivationGraph Statistics:" << std::endl;
        out << "  Number of nodes: " << getNodes().size() << std::endl;
        out << "  Number of edges: " << getEdges().size() << std::endl;
        out << "  Number of delta insert nodes: " << getDeltaInsertNodes().size() << std::endl;
        out << "  Number of delta insert edges: " << getDeltaInsertEdges().size() << std::endl;
        out << "  Number of delta delete nodes: " << getDeltaDeleteNodes().size() << std::endl;
        out << "  Number of delta delete edges: " << getDeltaDeleteEdges().size() << std::endl;
        for (const auto& [deletedFact, impactedFact]: getNodeImpactedByDeltaDelete()) {
            out << "  Node " << deletedFact->getId() << " impacted by delta delete: ";
            for (const auto& fact : impactedFact) {
                out << fact->toString() << " ";
            }
            out << std::endl;
        }
        for (const auto& [deletedEdge, impactedEdge]: getEdgeImpactedByDeltaDelete()) {
            out << "  Edge " << deletedEdge->getId() << " impacted by delta delete: ";
            for (const auto& edge : impactedEdge) {
                out << edge->getId() << " ";
            }
            out << std::endl;
        }
        for (const auto& [insertedFact, impactedFact]: getNodeImpactedByDeltaInsert()) {
            out << "  Node " << insertedFact->getId() << " impacted by delta insert: ";
            for (const auto& fact : impactedFact) {
                out << fact->toString() << " ";
            }
            out << std::endl;
        }
        for (const auto& [insertedEdge, impactedEdge]: getEdgeImpactedByDeltaInsert()) {
            out << "  Edge " << insertedEdge->getId() << " impacted by delta insert: ";
            for (const auto& edge : impactedEdge) {
                out << edge->getId() << " ";
            }
            out << std::endl;
        }
    }
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

    static DerivationGraph* createFrom(const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& ruleApps, const RuleManager& ruleManager, const std::unordered_map<UntypedTuple, double>& fact_prob = {})  {
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
        return graph;
    }

    SubgraphView prune(const std::vector<souffle::Relation*>& outputRelations) {
        std::unordered_set<std::string> outputRelationNames;
        for (const auto* rel : outputRelations) {
            outputRelationNames.insert(rel->getName());
            std::cout << "Output relation: " << rel->getName() << std::endl;
        }

        // 标记可达的节点和边
        std::unordered_set<NodePtr> reachableNodes;
        std::unordered_set<EdgePtr> reachableEdges;
        std::queue<NodePtr> workQueue;

        // 初始化：从所有输出 relation 的节点出发
        for (const auto& node : nodes) {
            if (outputRelationNames.count(node->getTuple().relation_name) > 0) {
                std::cout << "Found output node: " << node->getTuple().toString() << std::endl;
                reachableNodes.insert(node);
                workQueue.push(node);
                node->needOutput = true;
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
            if (reachableNodes.count(node)) {
                newNodes.insert(node);
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
    EdgePtr createHyperedge(const std::vector<NodePtr>& inputs, NodePtr output, const Rule* rule, std::vector<bool>& bodyNegations, RuleApplication ruleApp = naiveRuleApplication) {
        auto edge = std::shared_ptr<Hyperedge>(new Hyperedge(inputs, output, nextEdgeId++, rule, bodyNegations, ruleApp));

        for (const auto& input : inputs) {
            input->addOutgoingEdge(edge);
        }
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
    // 构造函数
    IncrementalDerivationGraph() : DerivationGraph() {}
    IncrementalDerivationGraph(const RuleManager* rm) : DerivationGraph(rm) {}

    IncSubgraphView prune(const std::vector<souffle::Relation*>& outputRelations);

    static IncrementalDerivationGraph* createFrom(const std::unordered_map<UntypedTuple, std::unordered_set<RuleApplication>*>& ruleApps, const RuleManager& ruleManager, const std::unordered_map<UntypedTuple, double>& fact_prob = {})  {
        FunctionTimer timer(" creating derivation graph ");

        auto graph = new IncrementalDerivationGraph(&ruleManager);
        for (const auto& [tuple, prob] : fact_prob) {
            auto node = graph->createNode(tuple);  // actually "find node" here
            node->setProbability(prob);
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
        {
//            std::ofstream os("./delta.txt");
//            os << "Applying delta inserts ..." << std::endl;
//            for (const auto& app : deltaInsertRuleApps) {
//                os << app.first.toString() << std::endl;
//                for (const auto& ruleApp : *(app.second)) {
//                    os << RuleApplication::toString(ruleApp) << std::endl;
//                }
//            }
//            os << "Applying delta deletes ..." << std::endl;
//            for (const auto& app : deltaDeleteRuleApps) {
//                os << app.first.toString() << std::endl;
//                for (const auto& ruleApp : *(app.second)) {
//                    os << RuleApplication::toString(ruleApp) << std::endl;
//                }
//            }
        }

        this->deltaInsertNodes.clear();
        this->deltaInsertEdges.clear();
        this->deltaDeleteNodes.clear();
        this->deltaDeleteEdges.clear();

        this->deletedFactImpactedNodes.clear();
        this->deletedFactImpactedEdges.clear();
        this->insertedFactImpactedNodes.clear();
        this->insertedFactImpactedEdges.clear();
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
            std::cout << "new fact inserted: " << node->toString() << std::endl;
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
                if (!outputNode->isFact)
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
                // TODO: possibly linked to input facts.
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
            if (!outputNode->isFact && outputNode->getIncomingEdges().empty()) {
                // 对于派生节点，如果没有入边，应该从图中删除
                // 如果这是一个新插入的节点被删除
                if (deltaInsertNodes.find(outputNode) != deltaInsertNodes.end()) {
                    // 从插入集合中移除
                    deltaInsertNodes.erase(outputNode);
                } else {
                    // 添加到删除集合中
                    deltaDeleteNodes.insert(outputNode);
                }

                // 将节点添加到待删除列表
                nodesToRemove.push_back(outputNode);
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
        if (node != nullptr) {
            // 从映射中移除
            tupleToNodeMap.erase(node->getTuple());
            deltaDeleteNodes.insert(node);
            // 从节点列表中移除
//            nodes.erase(std::remove(nodes.begin(), nodes.end(), node), nodes.end());
            nodes.erase(node);
            nodesToRemove.push_back(node);
        } else {
            std::cout << "deleted fact not found: " << tuple.toString() << std::endl;
            assert (false && "deleted fact not found");
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
    std::unordered_set<std::string> outputRelationNames;
    for (const auto* rel : outputRelations) {
        outputRelationNames.insert(rel->getName());
        std::cout << "Output relation: " << rel->getName() << std::endl;
    }

    // 标记可达的节点和边
    std::unordered_set<NodePtr> reachableNodes;
    std::unordered_set<EdgePtr> reachableEdges;
    std::queue<NodePtr> workQueue;

    // 初始化：从所有输出 relation 的节点出发
    for (const auto& node : nodes) {
        if (outputRelationNames.count(node->getTuple().relation_name) > 0) {
//            std::cout << "Found output node: " << node->getTuple().toString() << std::endl;
            reachableNodes.insert(node);
            workQueue.push(node);
            node->needOutput = true;
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
        if (reachableNodes.count(node)) {
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
        std::set<NodePtr> newImpactedNodes;
        for (const auto& impactedNode : impactedNodes) {
            if (reachableNodes.count(impactedNode)) {
                newImpactedNodes.insert(impactedNode);
            }
        }
        newInsertedFactImpactedNodes[fact] = std::move(newImpactedNodes);
    }
    for (const auto& [fact, impactedEdges] : insertedFactImpactedEdges) {
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
        std::set<NodePtr> newImpactedNodes;
        for (const auto& impactedNode : impactedNodes) {
            if (reachableNodes.count(impactedNode)) {
                newImpactedNodes.insert(impactedNode);
            }
        }
        newDeletedFactImpactedNodes[fact] = std::move(newImpactedNodes);
    }
    for (const auto& [fact, impactedEdges] : deletedFactImpactedEdges) {
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
            if (node->getProbability() < 1.0) {
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
        out << "  node" << node->getId() << " [label=\""
            << node->getTuple().toString();

        // 如果有概率信息，添加到标签中
        if (node->getProbability() < 1.0) {
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
        if (node->getProbability() < 1.0) {
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
    for (auto& [node, prob] : nodeProbabilities) {
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

#endif //DERIVATIONGRAPH_H
