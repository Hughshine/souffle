//
// Created by 17308 on 2025/6/9.
//

#ifndef SDDMANAGER_H
#define SDDMANAGER_H


#include <sdd++/sdd++.hpp>
#include <vector>
#include <string>
#include <sdd/sdd.h>
#include <iostream>
#include <cassert>
using NodeRef = SddNode*;
using Literal = int;
using weight = double;

using NodeRef = SddNode*;
using Literal = int;
using Weight = double;

class SddFormulaManager {
public:
    explicit SddFormulaManager(int var_count = 1000);
    ~SddFormulaManager();

    NodeRef createVar(int index);

    NodeRef makeAnd(const NodeRef& a, const NodeRef& b);
    NodeRef makeAnd(const std::vector<NodeRef>& nodes);
    NodeRef makeOr(const NodeRef& a, const NodeRef& b);
    NodeRef makeOr(const std::vector<NodeRef>& nodes);
    NodeRef makeNot(const NodeRef& a);

    NodeRef getTrue() const;
    NodeRef getFalse() const;

    bool isSame(const NodeRef& a, const NodeRef& b);

    void setVariableWeight(int varIndex, Weight posWeight, Weight negWeight);
    double computeWeightedModelCount(const NodeRef& node);
    void printInfo(const NodeRef& node, const std::string& name) const;
    void dumpProfilingStatistics() const;

private:
    SddManager* manager_;
    std::unordered_map<int, std::pair<Weight, Weight>> weight_map_;
};




SddFormulaManager::SddFormulaManager(int var_count) {
    manager_ = sdd_manager_create(var_count, 0);
}

SddFormulaManager::~SddFormulaManager() {
    sdd_manager_free(manager_);
}

NodeRef SddFormulaManager::createVar(int index) {
    assert(index > 0 && "SDD literals are 1-based");
    return sdd_manager_literal(index, manager_);
}

NodeRef SddFormulaManager::makeAnd(const NodeRef& a, const NodeRef& b) {
    return sdd_conjoin(a, b, manager_);
}

NodeRef SddFormulaManager::makeAnd(const std::vector<NodeRef>& nodes) {
    if (nodes.empty()) return getTrue();
    NodeRef result = nodes[0];
    for (size_t i = 1; i < nodes.size(); ++i) {
        result = sdd_conjoin(result, nodes[i], manager_);
    }
    return result;
}

NodeRef SddFormulaManager::makeOr(const NodeRef& a, const NodeRef& b) {
    return sdd_disjoin(a, b, manager_);
}

NodeRef SddFormulaManager::makeOr(const std::vector<NodeRef>& nodes) {
    if (nodes.empty()) return getFalse();
    NodeRef result = nodes[0];
    for (size_t i = 1; i < nodes.size(); ++i) {
        result = sdd_disjoin(result, nodes[i], manager_);
    }
    return result;
}

NodeRef SddFormulaManager::makeNot(const NodeRef& a) {
    return sdd_negate(a, manager_);
}

NodeRef SddFormulaManager::getTrue() const {
    return sdd_manager_true(manager_);
}

NodeRef SddFormulaManager::getFalse() const {
    return sdd_manager_false(manager_);
}

bool SddFormulaManager::isSame(const NodeRef& a, const NodeRef& b) {
    return a == b;
}



void SddFormulaManager::setVariableWeight(int varIndex, Weight posWeight, Weight negWeight) {
    weight_map_[varIndex] = std::make_pair(posWeight, negWeight);
}

double SddFormulaManager::computeWeightedModelCount(const NodeRef& node) {
    WmcManager* wmc = wmc_manager_new(node, /*log_mode*/ 0, manager_);

    for (const auto& [var, weights] : weight_map_) {
        wmc_set_literal_weight(var, weights.first, wmc);
        wmc_set_literal_weight(-var, weights.second, wmc);
    }

    double result = wmc_propagate(wmc);
    wmc_manager_free(wmc);
    return result;
}

void SddFormulaManager::printInfo(const NodeRef& node, const std::string& name) const {
    std::cout << "=== Info for: " << name << " ===\n";
    std::cout << "Size: " << sdd_size(node) << "\n";
    std::cout << "Model Count: " << sdd_model_count(node, manager_) << "\n";
}

void SddFormulaManager::dumpProfilingStatistics() const {
    std::cout<<"Current live nodes: "<< sdd_manager_live_size(manager_)<<std::endl;
    std::cout<<"Current dead nodes: "<< sdd_manager_dead_size(manager_)<<std::endl;
}







#endif //SDDMANAGER_H
