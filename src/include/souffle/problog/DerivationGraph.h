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
        if (auto it = std::find_if(nodes.begin(), nodes.end(),
                 [&tuple](const NodePtr& node) { return node->getTuple() == tuple;}); it != nodes.end()) {
            return *it;
        }
        auto node = std::shared_ptr<Node>(new Node(tuple, nextNodeId++));
        nodes.push_back(node);
        node.get()->probability = weight;
        return node;
    }

    EdgePtr createHyperedge(const std::vector<NodePtr>& inputs, NodePtr output) {
        auto edge = std::shared_ptr<Hyperedge>(new Hyperedge(inputs, output, nextEdgeId++));

        for (const auto& input : inputs) {
            input->addOutgoingEdge(edge);
        }
        output->addIncomingEdge(edge);

        edges.push_back(edge);
        return edge;
    }

    EdgePtr createHyperedge(const std::vector<NodePtr>& inputs, NodePtr output, const Rule* rule, std::vector<bool>& bodyNegations) {
        auto edge = std::shared_ptr<Hyperedge>(new Hyperedge(inputs, output, nextEdgeId++, rule, bodyNegations));

        for (const auto& input : inputs) {
            input->addOutgoingEdge(edge);
        }
        output->addIncomingEdge(edge);

        edges.push_back(edge);
        return edge;
    }

    EdgePtr createHyperedgeFromRuleApp(const RuleApplication& ruleApp, const RuleManager& rm) {
		const Rule* rule = rm.getRule(ruleApp.ruleId);
        assert(rule != nullptr && "Rule not found");
    	UntypedTuple headTuple{rule->getHead().getRelation(), rule->getHead().instantiatedFields(ruleApp.varValues)};
        auto headNode = createNode(headTuple);
        std::vector<NodePtr> bodyNodes;
        std::vector<bool> bodyNegations;
        for (const auto& bodyAtom : rule->getBodyAtoms()) {
            UntypedTuple bodyTuple{bodyAtom.getRelation(), bodyAtom.instantiatedFields(ruleApp.varValues)};
            auto bodyNode = createNode(bodyTuple);
            bodyNodes.push_back(bodyNode);
            bodyNegations.push_back(bodyAtom.isNegatedAtom());
        }
        return createHyperedge(bodyNodes, headNode, rule, bodyNegations);
    }

    const std::vector<NodePtr>& getNodes() const { return nodes; }
    const std::vector<EdgePtr>& getEdges() const { return edges; }

    static DerivationGraph* createFrom(const std::map<UntypedTuple, std::set<RuleApplication>*>& ruleApps, const RuleManager& ruleManager, const std::map<UntypedTuple, double>& fact_prob)  {
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

#endif //DERIVATIONGRAPH_H
