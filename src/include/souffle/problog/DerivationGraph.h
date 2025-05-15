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
        for (const auto& input : inputs) {
            ss << input->getTuple().toString();
        }
        ss << "->" << output->getTuple().toString() << "]";

        return ss.str();
    }
    RuleApplication getRuleApp() const { return ruleApp; }
    bool pruned = false;
private:
    Hyperedge(const std::vector<NodePtr>& inputs, NodePtr output, size_t edgeId, RuleApplication ruleApp)
        : inputs(inputs), output(output), id(edgeId), rule(nullptr), ruleApp(ruleApp) {}
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
    void dumpDotInc(const std::string& filename) const;
    void dumpStatisticsInc(std::ostream& out) const {
        out << "IncrementalDerivationGraph Statistics:" << std::endl;
        out << "  Number of nodes: " << getNodes().size() << std::endl;
        out << "  Number of edges: " << getEdges().size() << std::endl;
        out << "  Number of delta insert nodes: " << getDeltaInsertNodes().size() << std::endl;
        out << "  Number of delta insert edges: " << getDeltaInsertEdges().size() << std::endl;
        out << "  Number of delta delete nodes: " << getDeltaDeleteNodes().size() << std::endl;
        out << "  Number of delta delete edges: " << getDeltaDeleteEdges().size() << std::endl;
    }
};

class IncSubgraphView : public SubgraphView, public virtual IncrementalDerivationGraphViewInterface {
public:
    IncSubgraphView(const std::unordered_set<NodePtr>& nodes,
                    const std::unordered_set<EdgePtr>& edges,
                    const std::set<NodePtr>& deltaInsertNodes,
                    const std::set<EdgePtr>& deltaInsertEdges,
                    const std::set<NodePtr>& deltaDeleteNodes,
                    const std::set<EdgePtr>& deltaDeleteEdges)
            : SubgraphView(nodes, edges),
              deltaInsertNodes_(deltaInsertNodes),
              deltaInsertEdges_(deltaInsertEdges),
              deltaDeleteNodes_(deltaDeleteNodes),
              deltaDeleteEdges_(deltaDeleteEdges) {};

    // getNodes() / getEdges() from DerivationGraphView
    const std::unordered_set<NodePtr>& getNodes() const override {return nodes_; };
    const std::unordered_set<EdgePtr>& getEdges() const override {return edges_; };

    const std::set<NodePtr>& getDeltaInsertNodes() const override {return deltaInsertNodes_; };
    const std::set<EdgePtr>& getDeltaInsertEdges() const override {return deltaInsertEdges_; };
    const std::set<NodePtr>& getDeltaDeleteNodes() const override {return deltaDeleteNodes_; };
    const std::set<EdgePtr>& getDeltaDeleteEdges() const override {return deltaDeleteEdges_; };

    // 可选：dumpDot for incremental view

protected:
    std::set<NodePtr> deltaInsertNodes_;
    std::set<EdgePtr> deltaInsertEdges_;
    std::set<NodePtr> deltaDeleteNodes_;
    std::set<EdgePtr> deltaDeleteEdges_;
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
        for (const auto& app : deltaInsertRuleApps) {
            std::cout << app.first.toString() << std::endl;
            for (const auto& ruleApp : *(app.second)) {
                std::cout << RuleApplication::toString(ruleApp) << std::endl;
            }
        }
        for (const auto& app : deltaDeleteRuleApps) {
            std::cout << app.first.toString() << std::endl;
            for (const auto& ruleApp : *(app.second)) {
                std::cout << RuleApplication::toString(ruleApp) << std::endl;
            }
        }

        this->deltaInsertNodes.clear();
        this->deltaInsertEdges.clear();
        this->deltaDeleteNodes.clear();
        this->deltaDeleteEdges.clear();
        // 先应用删除，再应用插入
        applyDeltaDeletes(deltaDeleteRuleApps, ruleManager, deletedFacts);
        applyDeltaInserts(deltaInsertRuleApps, ruleManager, fact_prob);
    }

    // 用于跟踪增量变化的节点和边的集合
    std::set<NodePtr> deltaInsertNodes;
    std::set<EdgePtr> deltaInsertEdges;
    std::set<NodePtr> deltaDeleteNodes;
    std::set<EdgePtr> deltaDeleteEdges;

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
                deltaInsertNodes.insert(outputNode);
            }
        }
    }
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
            if (outputNode->getIncomingEdges().empty()) {
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
        std::cout << "removing node " << nodeToRemove->toString() << std::endl;
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
            std::cout << "Found output node: " << node->getTuple().toString() << std::endl;
            reachableNodes.insert(node);
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
            reachableEdges.insert(edge);
            for (const auto& inputNode : edge->getInputs()) {
                if (reachableNodes.insert(inputNode).second) {
                    workQueue.push(inputNode);
                }
            }
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

    std::unordered_set<NodePtr> newNodes;
    for (const auto& node : nodes) {
        if (reachableNodes.count(node)) {
            newNodes.insert(node);
        } else {
            node->pruned = true;
        }
    }

    std::unordered_set<EdgePtr> newEdges;
    for (const auto& edge : edges) {
        if (reachableEdges.count(edge)) {
            newEdges.insert(edge);
        } else {
            edge->pruned = true;
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



//        nodes = std::move(newNodes);
//        edges = std::move(newEdges);

    auto view = IncSubgraphView(std::move(newNodes), std::move(newEdges),
                           std::move(newDeltaInsertedNodes),
                           std::move(newDeltaInsertedEdges),
                           std::move(newDeltaDeletedNodes),
                           std::move(newDeltaDeletedEdges));
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
    std::unordered_map<NodePtr, double>& nodeProbabilities, const std::string& outputDir = "./output/") {
    std::ofstream outputFile(
        outputDir + "/" + "facts" + ".prob"
    );

    for (auto& [node, prob] : nodeProbabilities) {
        outputFile << node->getTuple().toString() << " : " << prob << std::endl;
    }

}

void computeSCCOrderedCyclesWithDepth(
    const SubgraphView& view,
    std::vector<std::unordered_set<NodePtr>>& nodeCycles,
    std::vector<std::unordered_set<EdgePtr>>& edgeCycles,
    std::unordered_map<NodePtr, size_t>& nodeToCycleIndex,
    std::unordered_map<EdgePtr, size_t>& edgeToCycleIndex,
    std::unordered_map<NodePtr, size_t>& nodeDepths,
    std::unordered_map<EdgePtr, size_t>& edgeDepths
) {
    FunctionTimer timer("Computing SCC Ordered Cycles with Depth");
    const auto& nodes = view.getNodes();
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

        for (const auto& edge : view.getOutgoingEdges(v)) {
            NodePtr w = view.getOutput(edge);
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
    for (const auto& edge : view.getEdges()) {
        size_t outCycle = rawNodeToCycle[view.getOutput(edge)];
        for (const auto& input : view.getInputs(edge)) {
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
    for (const auto& edge : view.getEdges()) {
        NodePtr out = view.getOutput(edge);
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
            for (auto& inEdge : view.getIncomingEdges(node)) {
                bool allOutOfCycle = true;
                for (auto& inNode : view.getInputs(inEdge)) {
                    if (nodeToCycleIndex[inNode] == cid) {
                        allOutOfCycle = false;
                        break;
                    }
                }
                if (allOutOfCycle) {
                    isEntry = true;
                    break;
                }
            }
            if (node->isFact || view.getIncomingEdges(node).empty()) isEntry = true;
            if (isEntry) {
                nodeDepths[node] = 0;
                q.push(node);
            }
        }
        while (!q.empty()) {
            NodePtr curr = q.front(); q.pop();
            size_t currDepth = nodeDepths[curr];
            for (auto& outEdge : view.getOutgoingEdges(curr)) {
                NodePtr out = view.getOutput(outEdge);
                if (nodeToCycleIndex[out] != cid) continue;
                if (!nodeDepths.count(out) || nodeDepths[out] > currDepth + 1) {
                    nodeDepths[out] = currDepth + 1;
                    q.push(out);
                }
            }
        }
        for (auto edge : cycleEdges) {
            size_t d = 0;
            for (auto in : view.getInputs(edge)) {
                if (nodeToCycleIndex[in] == cid && nodeDepths.count(in)) {
                    // Edge depth = max(input depth + 1), so inner-cycle edges start from 1, cross-cycle = 0
                    d = std::max(d, nodeDepths[in] + 1);
                }
            }
            edgeDepths[edge] = d;
        }
    }

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
}

#endif //DERIVATIONGRAPH_H
