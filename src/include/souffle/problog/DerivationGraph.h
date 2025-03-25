//
// Created by lxy10 on 2/25/2025.
//

#ifndef DERIVATIONGRAPH_H
#define DERIVATIONGRAPH_H

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <fstream>
#include <sstream>
#include "souffle/Derivation.h"
#include "souffle/problog/Rule.h"
#include "souffle/problog/RuleManager.h"
#include "souffle/RamTypes.h"

// example rule application sets
// rule 1: path(x,y) :- edge(x,y).
// rule 2: path(x,y) :- path(x,z), edge(z,y).
// facts: edge(1,2), edge(2,3)
// (derived) paths: path(1,2), path(2,3), path(1,3)
//inline Clauses rules


//inline UntypedTuple tuple1{"edge", {1, 2}};
//inline UntypedTuple tuple2{"edge", {2, 3}};
//inline UntypedTuple tuple3{"path", {1, 2}};
//inline UntypedTuple tuple4{"path", {2, 3}};
//inline UntypedTuple tuple5{"path", {1, 3}};
//
//inline std::map<UntypedTuple, std::set<RuleApplication>*> exampleRuleApps{
//        {tuple3, new std::set<RuleApplication>{
//                RuleApplication{1, {{"x", 1}, {"y", 2}}}
//        }},
//        {tuple4, new std::set<RuleApplication>{
//                RuleApplication{1, {{"x", 2}, {"y", 3}}}
//        }},
//        {tuple5, new std::set<RuleApplication>{
//                RuleApplication{2, {{"x", 1}, {"y", 3}, {"z", 2}}}
//        }}
//};


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

    const UntypedTuple& getTuple() const { return tuple; }
    const std::vector<EdgePtr>& getIncomingEdges() const { return incomingEdges; }
    const std::vector<EdgePtr>& getOutgoingEdges() const { return outgoingEdges; }
    size_t getId() const { return id; }
    double getProbability() const { return probability; }
    std::string toString() const {
        return tuple.toString();
//        std::stringstream ss;
//        ss << "Node(" << tuple.toString() << ")";
//        return ss.str();
    }
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
    std::string toString() const {
//        if (rule == nullptr) {
//            return "Hyperedge(" + std::to_string(id) + ")";
//        } else {
//
//            return "Rule" + std::to_string(rule->getRuleId()) + "("+ std::to_string(id) + ")";
//        }
    // inputs to outputs
        std::stringstream ss;
        ss << "Hyperedge(" << id << "): ";
        for (const auto& input : inputs) {
            ss << input->getTuple().toString() << " ";
        }
        ss << " -> " << output->getTuple().toString();

        return ss.str();
    }

private:
    Hyperedge(const std::vector<NodePtr>& inputs, NodePtr output, size_t edgeId)
        : inputs(inputs), output(output), id(edgeId), rule(nullptr) {}
    Hyperedge(const std::vector<NodePtr>& inputs, NodePtr output, size_t edgeId, const Rule* rule, std::vector<bool>& bodyNegations)
        : inputs(inputs), output(output), id(edgeId), rule(rule) {
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
};

class DerivationGraph {
public:
    DerivationGraph() : nextNodeId(0), nextEdgeId(0) {}
    DerivationGraph(const RuleManager* rm) : nextNodeId(0), nextEdgeId(0), ruleManager(rm) {}

    NodePtr createNode(const UntypedTuple& tuple, const double weight = 1.0) {
        // 先查找是否已存在
        NodePtr existingNode = findNode(tuple);
        if (existingNode) {
            return existingNode;
        }

        // 不存在则创建新节点
        auto node = std::shared_ptr<Node>(new Node(tuple, nextNodeId++));
        nodes.push_back(node);
        node->probability = weight;

        // 添加到映射中
        tupleToNodeMap[tuple] = node;
        return node;
    }


    EdgePtr createHyperedgeFromRuleApp(const RuleApplication& ruleApp, const RuleManager& rm) {
        // 先查找是否存在对应的边
        EdgePtr existingEdge = findHyperedgeFromRuleApp(ruleApp);
        if (existingEdge) {
            return existingEdge;
        }

        const Rule* rule = rm.getRule(ruleApp.ruleId);
        assert(rule != nullptr && "Rule not found");

        // 根据规则头部和变量值创建输出元组
        UntypedTuple headTuple{rule->getHead().getRelation(), rule->getHead().instantiatedFields(ruleApp.varValues)};
        auto headNode = createNode(headTuple);

        // 创建输入节点
        std::vector<NodePtr> bodyNodes;
        std::vector<bool> bodyNegations;
        for (const auto& bodyAtom : rule->getBodyAtoms()) {
            UntypedTuple bodyTuple{bodyAtom.getRelation(), bodyAtom.instantiatedFields(ruleApp.varValues)};
            auto bodyNode = createNode(bodyTuple);
            bodyNodes.push_back(bodyNode);
            bodyNegations.push_back(bodyAtom.isNegatedAtom());
        }

        auto newEdge = createHyperedge(bodyNodes, headNode, rule, bodyNegations);

        // 将新边添加到映射中
        std::string key = createEdgeKey(ruleApp.ruleId, ruleApp.varValues);
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
                         const std::map<std::string, souffle::RamDomain>& varValues) const {
        std::string key = createEdgeKey(ruleId, varValues);
        auto it = edgeKeyToEdgeMap.find(key);
        if (it != edgeKeyToEdgeMap.end()) {
            return it->second;
        }
        return nullptr;
    }

    EdgePtr findHyperedgeFromRuleApp(const RuleApplication& ruleApp) const {
        return findHyperedge(ruleApp.ruleId, ruleApp.varValues);
    }

    const std::vector<NodePtr>& getNodes() const { return nodes; }
    const std::vector<EdgePtr>& getEdges() const { return edges; }

    static DerivationGraph* createFrom(const std::map<UntypedTuple, std::set<RuleApplication>*>& ruleApps, const RuleManager& ruleManager, const std::map<UntypedTuple, double>& fact_prob = {})  {
        FunctionTimer timer(" creating derivation graph ");

        auto graph = new DerivationGraph(&ruleManager);
        for (const auto& [tuple, ruleAppSet] : ruleApps) {
            auto node = graph->createNode(tuple);
            for (const auto& ruleApp : *ruleAppSet) {
				auto edge = graph->createHyperedgeFromRuleApp(ruleApp, ruleManager);
            }
        }
        for (const auto& [tuple, prob] : fact_prob) {
            auto node = graph->createNode(tuple);  // actually "find node" here
            node->probability = prob;
        }
        return graph;
    }

    void dumpDot(const std::string& filename) const {
        std::ofstream out(filename);
        if (!out.is_open()) {
            throw std::runtime_error("Cannot open file: " + filename);
        }

        out << "digraph DerivationGraph {\n";
        out << "  rankdir=LR;\n";

        out << "  node [shape=box, style=filled, fillcolor=lightblue];\n";

        for (const auto& node : nodes) {
            out << "  node" << node->getId() << " [label=\""
                << node->getTuple().toString()
                << "\"];\n";
        }

        out << "  node [shape=point, fillcolor=red, width=0.2];\n";

        for (const auto& edge : edges) {
            out << "  edge" << edge->getId() << " [label=\"\"];\n";

            for (const auto& input : edge->getInputs()) {
                out << "  node" << input->getId()
                    << " -> edge" << edge->getId() << ";\n";
            }

            out << "  edge" << edge->getId()
                << " -> node" << edge->getOutput()->getId() << ";\n";
        }

        out << "}\n";
        out.close();
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

private:
    std::vector<NodePtr> nodes;
    std::vector<EdgePtr> edges;
    size_t nextNodeId;
    size_t nextEdgeId;
    const RuleManager* ruleManager;

    // 添加元组到节点的映射
    std::map<UntypedTuple, NodePtr> tupleToNodeMap;

    // 添加边键到边的映射
    std::map<std::string, EdgePtr> edgeKeyToEdgeMap;

    // 创建边的唯一键
    std::string createEdgeKey(souffle::RamDomain ruleId,
                             const std::map<std::string, souffle::RamDomain>& varValues) const {
        std::stringstream ss;
        ss << ruleId << "_";

        // 按照变量名排序，确保键的一致性
        std::vector<std::string> varNames;
        for (const auto& [name, _] : varValues) {
            varNames.push_back(name);
        }
        std::sort(varNames.begin(), varNames.end());

        for (const auto& name : varNames) {
            ss << name << ":" << varValues.at(name) << ";";
        }

        return ss.str();
    }

    // this one does not check if the edge already exists
    EdgePtr createHyperedge(const std::vector<NodePtr>& inputs, NodePtr output, const Rule* rule, std::vector<bool>& bodyNegations) {
        auto edge = std::shared_ptr<Hyperedge>(new Hyperedge(inputs, output, nextEdgeId++, rule, bodyNegations));

        for (const auto& input : inputs) {
            input->addOutgoingEdge(edge);
        }
        output->addIncomingEdge(edge);

        edges.push_back(edge);
        return edge;
    }

    // Just for test; this one does not check if the edge already exists
    EdgePtr createHyperedge(const std::vector<NodePtr>& inputs, NodePtr output) {
        auto edge = std::shared_ptr<Hyperedge>(new Hyperedge(inputs, output, nextEdgeId++));

        for (const auto& input : inputs) {
            input->addOutgoingEdge(edge);
        }
        output->addIncomingEdge(edge);

        edges.push_back(edge);
        return edge;
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

//class IncrementalDerivationGraph : public DerivationGraph {
//public:
//    // 继承构造函数
//    IncrementalDerivationGraph() : DerivationGraph() {}
//    IncrementalDerivationGraph(const RuleManager* rm) : DerivationGraph(rm) {}
//
//    // 从现有的派生图创建增量图
//    void applyDelta(
//        const std::map<UntypedTuple, std::set<RuleApplication>*>& deltaInsertRuleApps,
//        const std::map<UntypedTuple, std::set<RuleApplication>*>& deltaDeleteRuleApps,
//        const RuleManager& ruleManager
//    );
//
//    // 应用增量插入
//    void applyDeltaInserts(
//        const std::map<UntypedTuple, std::set<RuleApplication>*>& deltaInsertRuleApps,
//        const RuleManager& ruleManager
//    );
//
//    // 应用增量删除
//    void applyDeltaDeletes(
//        const std::map<UntypedTuple, std::set<RuleApplication>*>& deltaDeleteRuleApps,
//        const RuleManager& ruleManager
//    );
//
//    void dumpDotInc(const std::string& filename) const;
//
//private:
//    // 追踪增量变化的节点和边
//    std::set<NodePtr> deltaInsertNodes;
//    std::set<EdgePtr> deltaInsertEdges;
//    std::set<NodePtr> deltaDeleteNodes;
//    std::set<EdgePtr> deltaDeleteEdges;
//};

//void IncrementalDerivationGraph::applyDelta(
//    const std::map<UntypedTuple, std::set<RuleApplication>*>& deltaInsertRuleApps,
//    const std::map<UntypedTuple, std::set<RuleApplication>*>& deltaDeleteRuleApps,
//    const RuleManager& ruleManager
//) {
//    applyDeltaInserts(deltaInsertRuleApps, ruleManager);
//    applyDeltaDeletes(deltaDeleteRuleApps, ruleManager);
//}
//
//void IncrementalDerivationGraph::applyDeltaInserts(
//    const std::map<UntypedTuple, std::set<RuleApplication>*>& deltaInsertRuleApps,
//    const RuleManager& ruleManager
//) {
//    FunctionTimer timer("applying delta inserts");
//
//    for (const auto& [tuple, ruleAppSet] : deltaInsertRuleApps) {
//        auto outNode = createNode(tuple);
//        deltaInsertNodes.insert(outNode);
//
//        for (const auto& ruleApp : *ruleAppSet) {
//            auto edge = createHyperedgeFromRuleApp(ruleApp, ruleManager);
//            deltaInsertEdges.insert(edge);
//        }
//    }
//}

//void IncrementalDerivationGraph::applyDeltaDeletes(
//    const std::map<UntypedTuple, std::set<RuleApplication>*>& deltaDeleteRuleApps,
//    const RuleManager& ruleManager
//) {
//    FunctionTimer timer("applying delta deletes");
//
//    // 第一步：标记要删除的边
//    std::vector<EdgePtr> edgesToRemove;
//
//    for (const auto& [tuple, ruleAppSet] : deltaDeleteRuleApps) {
//        // 找到匹配元组的节点
//        NodePtr targetNode = nullptr;
//        for (const auto& node : getNodes()) {
//            if (node->getTuple() == tuple) {
//                targetNode = node;
//                break;
//            }
//        }
//
//        if (!targetNode) {
//            std::cout << "Node not found for tuple: " << tuple.toString() << std::endl;
//            continue;
//        } // 如果找不到节点，跳过
//
//        // 对于每个规则应用，找到并标记对应的边
//        for (const auto& ruleApp : *ruleAppSet) {
//            for (const auto& edge : targetNode->getIncomingEdges()) {
//                // 这里需要一个比较逻辑来确定边是否由特定规则应用创建
//                // 简化版本：假设我们可以通过规则ID和节点来匹配
//                if (edge->getRule() && edge->getRule()->getRuleId() == ruleApp.ruleId) {
//                    edgesToRemove.push_back(edge);
//                    deltaDeleteEdges.insert(edge);
//
//                    // 标记输出节点为delta删除
//                    deltaDeleteNodes.insert(edge->getOutput());
//                }
//            }
//        }
//    }
//
//    // 第二步：标记那些可能因为没有来源而被删除的节点
//    std::set<NodePtr> potentialOrphanNodes;
//    for (const auto& edge : edgesToRemove) {
//        potentialOrphanNodes.insert(edge->getOutput());
//    }
//
//    // 第三步：检查哪些节点变成了孤立节点（没有入边）
//    for (const auto& node : potentialOrphanNodes) {
//        bool hasRemainingEdges = false;
//        for (const auto& edge : node->getIncomingEdges()) {
//            if (std::find(edgesToRemove.begin(), edgesToRemove.end(), edge) == edgesToRemove.end()) {
//                hasRemainingEdges = true;
//                break;
//            }
//        }
//
//        if (!hasRemainingEdges) {
//            deltaDeleteNodes.insert(node);
//        }
//    }
//
//    // 注意：这个方法不会实际从图中删除节点和边，
//    // 只是标记它们。完整实现应该包括实际删除的逻辑。
//}

#endif //DERIVATIONGRAPH_H
