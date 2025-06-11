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
using Weight = double;

class SddNodeRef {
friend class SddFormulaManager;

public:
    // Default constructor
    SddNodeRef() : manager_(nullptr), node_(nullptr) {}

    // Primary constructor
    SddNodeRef(std::shared_ptr<SddManager> m, SddNode* n) : manager_(std::move(m)), node_(n) {
        if (node_) sdd_ref(node_, manager_.get());
    }

    // Copy constructor
    SddNodeRef(const SddNodeRef& other)
        : manager_(other.manager_), node_(other.node_) {
        if (node_) sdd_ref(node_, manager_.get());
    }

    // Copy assignment operator
    SddNodeRef& operator=(const SddNodeRef& other) {
        if (this != &other) {
            if (node_ && manager_) sdd_deref(node_, manager_.get());
            manager_ = other.manager_;
            node_ = other.node_;
            if (node_) sdd_ref(node_, manager_.get());
        }
        return *this;
    }

    // Move constructor
    SddNodeRef(SddNodeRef&& other) noexcept
        : manager_(std::move(other.manager_)), node_(other.node_) {
        other.node_ = nullptr;
    }

    // Move assignment operator
    SddNodeRef& operator=(SddNodeRef&& other) noexcept {
        if (this != &other) {
            if (node_ && manager_) sdd_deref(node_, manager_.get());
            manager_ = std::move(other.manager_);
            node_ = other.node_;
            other.node_ = nullptr;
        }
        return *this;
    }

    // Destructor
    ~SddNodeRef() {
        if (node_ && manager_) sdd_deref(node_, manager_.get());
    }

    // Getter for the SDD node
    SddNode* get() const { return node_; }
    operator bool() const { return node_ != nullptr; }
    bool operator==(const SddNodeRef& other) const { return node_ == other.node_; }
    bool operator!=(const SddNodeRef& other) const { return node_ != other.node_; }

private:
    std::shared_ptr<SddManager> manager_;
    SddNode* node_;
};


class SddFormulaManager {
public:
    explicit SddFormulaManager(int var_count = 1000);
    ~SddFormulaManager() = default;

    SddNodeRef createVar(int index);

    SddNodeRef makeAnd(const SddNodeRef& a, const SddNodeRef& b);
    SddNodeRef makeAnd(const std::vector<SddNodeRef>& nodes);
    SddNodeRef makeOr(const SddNodeRef& a, const SddNodeRef& b);
    SddNodeRef makeOr(const std::vector<SddNodeRef>& nodes);
    SddNodeRef makeNot(const SddNodeRef& a);

    SddNodeRef getTrue() const;
    SddNodeRef getFalse() const;

    bool isSame(const SddNodeRef& a, const SddNodeRef& b);

    void setVariableWeight(int varIndex, Weight posWeight, Weight negWeight);
    double computeWeightedModelCount(const SddNodeRef& node);
    void printInfo(const SddNodeRef& node, const std::string& name) const;
    void dumpProfilingStatistics() const;

private:
    std::shared_ptr<SddManager> manager_;
    std::unordered_map<int, std::pair<Weight, Weight>> weight_map_;
};

SddFormulaManager::SddFormulaManager(int var_count) {
    SddManager* raw_mgr = sdd_manager_create(var_count, 0);
    manager_ = std::shared_ptr<SddManager>(raw_mgr, sdd_manager_free);
}

SddNodeRef SddFormulaManager::createVar(int index) {
    assert(index > 0 && "SDD literals are 1-based");
    return SddNodeRef(manager_, sdd_manager_literal(index, manager_.get()));
}

SddNodeRef SddFormulaManager::makeAnd(const SddNodeRef& a, const SddNodeRef& b) {
    return SddNodeRef(manager_, sdd_conjoin(a.get(), b.get(), manager_.get()));
}

SddNodeRef SddFormulaManager::makeAnd(const std::vector<SddNodeRef>& nodes) {
    if (nodes.empty()) return getTrue();
    SddNodeRef result = nodes[0];
    for (size_t i = 1; i < nodes.size(); ++i) {
        result = makeAnd(result, nodes[i]);
    }
    return result;
}

SddNodeRef SddFormulaManager::makeOr(const SddNodeRef& a, const SddNodeRef& b) {
    return SddNodeRef(manager_, sdd_disjoin(a.get(), b.get(), manager_.get()));
}

SddNodeRef SddFormulaManager::makeOr(const std::vector<SddNodeRef>& nodes) {
    if (nodes.empty()) return getFalse();
    SddNodeRef result = nodes[0];
    for (size_t i = 1; i < nodes.size(); ++i) {
        result = makeOr(result, nodes[i]);
    }
    return result;
}

SddNodeRef SddFormulaManager::makeNot(const SddNodeRef& a) {
    return SddNodeRef(manager_, sdd_negate(a.get(), manager_.get()));
}

SddNodeRef SddFormulaManager::getTrue() const {
    return SddNodeRef(manager_, sdd_manager_true(manager_.get()));
}

SddNodeRef SddFormulaManager::getFalse() const {
    return SddNodeRef(manager_, sdd_manager_false(manager_.get()));
}

bool SddFormulaManager::isSame(const SddNodeRef& a, const SddNodeRef& b) {
    return a.get() == b.get();
}

void SddFormulaManager::setVariableWeight(int varIndex, Weight posWeight, Weight negWeight) {
    weight_map_[varIndex] = { posWeight, negWeight };
}

double SddFormulaManager::computeWeightedModelCount(const SddNodeRef& node) {
    WmcManager* wmc = wmc_manager_new(node.get(), /*log_mode*/ 0, manager_.get());
    for (const auto& [var, weights] : weight_map_) {
        wmc_set_literal_weight(var, weights.first, wmc);
        wmc_set_literal_weight(-var, weights.second, wmc);
    }
    double result = wmc_propagate(wmc);
    wmc_manager_free(wmc);
    return result;
}

void SddFormulaManager::printInfo(const SddNodeRef& node, const std::string& name) const {
    std::cout << "=== Info for: " << name << " ===\n";
    std::cout << "Size: " << sdd_size(node.get()) << "\n";
    std::cout << "Model Count: " << sdd_model_count(node.get(), manager_.get()) << "\n";
}

void SddFormulaManager::dumpProfilingStatistics() const {
    std::cout << "Live nodes: " << sdd_manager_live_size(manager_.get()) << "\n";
    std::cout << "Dead nodes: " << sdd_manager_dead_size(manager_.get()) << "\n";
}






#endif //SDDMANAGER_H
