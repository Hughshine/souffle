//
// Created by 17308 on 2025/6/9.
//
#ifndef SDDMANAGER_H
#define SDDMANAGER_H

#include <vector>
#include <string>
#include <sdd/sdd.h>
#include <iostream>
#include <cassert>
#include <unordered_map>
#include <fstream>
#include <algorithm>
#include "FormulaManager.h"
#include "souffle/problog/formula/GraphHeuristics.h"

using NodeRef = SddNode*;
using Literal = int;
using Weight = double;

class SddNodeRef {
    friend class SddFormulaManager;

public:
    SddNodeRef() : manager_(nullptr), node_(nullptr) {}

    SddNodeRef(std::shared_ptr<SddManager> m, SddNode* n)
        : manager_(std::move(m)), node_(n) {
        if (node_) sdd_ref(node_, manager_.get());
    }

    SddNodeRef(const SddNodeRef& other)
        : manager_(other.manager_), node_(other.node_) {
        if (node_) sdd_ref(node_, manager_.get());
    }

    SddNodeRef& operator=(const SddNodeRef& other) {
        if (this != &other) {
            if (node_ && manager_) sdd_deref(node_, manager_.get());
            manager_ = other.manager_;
            node_ = other.node_;
            if (node_) sdd_ref(node_, manager_.get());
        }
        return *this;
    }

    SddNodeRef(SddNodeRef&& other) noexcept
        : manager_(std::move(other.manager_)), node_(other.node_) {
        other.node_ = nullptr;
    }

    SddNodeRef& operator=(SddNodeRef&& other) noexcept {
        if (this != &other) {
            if (node_ && manager_) sdd_deref(node_, manager_.get());
            manager_ = std::move(other.manager_);
            node_ = other.node_;
            other.node_ = nullptr;
        }
        return *this;
    }

    ~SddNodeRef() {
        if (node_ && manager_) sdd_deref(node_, manager_.get());
    }

    SddNode* get() const { return node_; }
    operator bool() const { return node_ != nullptr; }
    bool operator==(const SddNodeRef& other) const { return node_ == other.node_; }
    bool operator!=(const SddNodeRef& other) const { return node_ != other.node_; }

private:
    std::shared_ptr<SddManager> manager_;
    SddNode* node_;
};

class SddFormulaManager : public DDManager<SddNodeRef> {
public:
    explicit SddFormulaManager(int var_count = 1000 );
    ~SddFormulaManager() override = default;
    void preConfig(DerivationGraphViewInterface& view) override;

    SddNodeRef createVar(int index) override;
    SddNodeRef createVar(int index, const Node& node) override;
    SddNodeRef createVar(int index, const Hyperedge& edge) override;

    SddNodeRef makeAnd(const SddNodeRef& a, const SddNodeRef& b) override;
    SddNodeRef makeAnd(const std::vector<SddNodeRef>& nodes) override;
    SddNodeRef makeOr(const SddNodeRef& a, const SddNodeRef& b) override;
    SddNodeRef makeOr(const std::vector<SddNodeRef>& nodes) override;
    SddNodeRef makeNot(const SddNodeRef& a) override;
    SddNodeRef makeCondition(const SddNodeRef& f,
        const std::vector<int>& trueIndexes, const std::vector<int>& falseIndexes) override;

    SddNodeRef getTrue() override;
    SddNodeRef getFalse() override;

    bool isSame(const SddNodeRef& a, const SddNodeRef& b) override;

    void setVariableWeight(int varIndex, Weight posWeight, Weight negWeight) override;
    double computeWeightedModelCount(const SddNodeRef& node) override;

    void printInfo(const SddNodeRef& node, const std::string& name) override;
    void dumpProfilingStatistics() override;

    SddNodeRef createWeightedExample() override {
        return getTrue(); // Stub example
    }

    std::string toString(const SddNodeRef& node) override {
        return "[SDD node@" + std::to_string(reinterpret_cast<std::uintptr_t>(node.get())) + "]";
    }

private:
    std::shared_ptr<SddManager> manager_;
    std::unordered_map<int, std::pair<Weight, Weight>> weight_map_;
    std::unordered_map<int, const Node*> node_map_;
    std::unordered_map<int, const Hyperedge*> edge_map_;
    BDDForceHeuristics heuristics_;
    int var_count_;

    void configureManagerLimits(SddManager* raw_mgr);
    void rebuildManagerWithOrder(const std::vector<SddLiteral>& order);
    std::vector<SddLiteral> buildLiteralOrder(const std::vector<int>& heuristicOrder) const;

    inline int mapNodeId(int rawId) {
        return rawId + 1;
    }
};

inline SddFormulaManager::SddFormulaManager(int var_count) {
    var_count_ = std::max(1, var_count);
    Vtree* vtree = sdd_vtree_new(var_count_, "balanced");
    SddManager* raw_mgr = sdd_manager_new(vtree);
    configureManagerLimits(raw_mgr);
    manager_ = std::shared_ptr<SddManager>(raw_mgr, sdd_manager_free);
    sdd_manager_garbage_collect(manager_.get());
    sdd_manager_minimize_limited(manager_.get());
}

inline void SddFormulaManager::configureManagerLimits(SddManager* raw_mgr) {
    sdd_manager_set_vtree_apply_time_limit(5.0, raw_mgr);
    sdd_manager_set_vtree_cartesian_product_limit(1000000, raw_mgr);
    sdd_manager_set_vtree_operation_memory_limit(500, raw_mgr);
    sdd_manager_set_vtree_search_time_limit(2, raw_mgr);
    sdd_manager_auto_gc_and_minimize_on(raw_mgr);
}

inline std::vector<SddLiteral> SddFormulaManager::buildLiteralOrder(
        const std::vector<int>& heuristicOrder) const {
    std::vector<SddLiteral> order;
    order.reserve(var_count_);
    std::vector<char> seen(var_count_, 0);

    for (int var : heuristicOrder) {
        int lit = var + 1;
        if (lit <= 0 || lit > var_count_) continue;
        if (seen[lit - 1]) continue;
        order.push_back(lit);
        seen[lit - 1] = 1;
    }

    for (int lit = 1; lit <= var_count_; ++lit) {
        if (!seen[lit - 1]) order.push_back(lit);
    }

    return order;
}

inline void SddFormulaManager::rebuildManagerWithOrder(const std::vector<SddLiteral>& order) {
    Vtree* vtree = nullptr;
    if (!order.empty()) {
        std::vector<SddLiteral> tmp(order);
        vtree = sdd_vtree_new_with_var_order(var_count_, tmp.data(), "balanced");
    } else {
        vtree = sdd_vtree_new(var_count_, "balanced");
    }

    SddManager* raw_mgr = sdd_manager_new(vtree);
    configureManagerLimits(raw_mgr);
    manager_.reset(raw_mgr, sdd_manager_free);
    sdd_manager_garbage_collect(manager_.get());
    sdd_manager_minimize_limited(manager_.get());
}

inline void SddFormulaManager::preConfig(DerivationGraphViewInterface& view) {
    heuristics_.compute(view);
    auto literalOrder = buildLiteralOrder(heuristics_.getOrder());
    rebuildManagerWithOrder(literalOrder);
}



SddNodeRef SddFormulaManager::createVar(int rawId) {
    int index = mapNodeId(rawId);
    assert(index > 0 && "SDD literals are 1-based");
    return SddNodeRef(manager_, sdd_manager_literal(index, manager_.get()));
}

SddNodeRef SddFormulaManager::createVar(int index, const Node& node) {
    node_map_[index] = &node;
    return createVar(index);
}
SddNodeRef SddFormulaManager::createVar(int index, const Hyperedge& edge) {
    edge_map_[index] = &edge;
    return createVar(index);
}

inline SddNodeRef SddFormulaManager::makeAnd(const SddNodeRef& a, const SddNodeRef& b) {
    return SddNodeRef(manager_, sdd_conjoin(a.get(), b.get(), manager_.get()));
}

inline SddNodeRef SddFormulaManager::makeAnd(const std::vector<SddNodeRef>& nodes) {
    if (nodes.empty()) return getTrue();
    SddNodeRef result = nodes[0];
    for (size_t i = 1; i < nodes.size(); ++i) {
        result = makeAnd(result, nodes[i]);
    }
    return result;
}

inline SddNodeRef SddFormulaManager::makeOr(const SddNodeRef& a, const SddNodeRef& b) {
    return SddNodeRef(manager_, sdd_disjoin(a.get(), b.get(), manager_.get()));
}

inline SddNodeRef SddFormulaManager::makeOr(const std::vector<SddNodeRef>& nodes) {
    if (nodes.empty()) return getFalse();
    SddNodeRef result = nodes[0];
    for (size_t i = 1; i < nodes.size(); ++i) {
        result = makeOr(result, nodes[i]);
    }
    return result;
}

inline SddNodeRef SddFormulaManager::makeNot(const SddNodeRef& a) {
    return SddNodeRef(manager_, sdd_negate(a.get(), manager_.get()));
}

inline SddNodeRef SddFormulaManager::getTrue() {
    return SddNodeRef(manager_, sdd_manager_true(manager_.get()));
}

inline SddNodeRef SddFormulaManager::getFalse() {
    return SddNodeRef(manager_, sdd_manager_false(manager_.get()));
}

inline SddNodeRef SddFormulaManager::makeCondition(const SddNodeRef& f,
        const std::vector<int>& trueIndexes, const std::vector<int>& falseIndexes) {
    SddNodeRef result;
    for (int index : trueIndexes) {
        result = SddNodeRef(manager_, sdd_condition(index, f.get(), manager_.get()));
    }
    for (int index : falseIndexes) {
        result = SddNodeRef(manager_, sdd_condition(-index, f.get(), manager_.get()));
    }
    return result;
}

inline bool SddFormulaManager::isSame(const SddNodeRef& a, const SddNodeRef& b) {
    return a.get() == b.get();
}

void SddFormulaManager::setVariableWeight(int rawId, Weight posWeight, Weight negWeight) {
    int index = mapNodeId(rawId);
    weight_map_[index] = { posWeight, negWeight };
}

inline double SddFormulaManager::computeWeightedModelCount(const SddNodeRef& node) {
    WmcManager* wmc = wmc_manager_new(node.get(), 0, manager_.get());
    for (const auto& [var, weights] : weight_map_) {
        wmc_set_literal_weight(var, weights.first, wmc);
        wmc_set_literal_weight(-var, weights.second, wmc);
    }
    int var_count = sdd_manager_var_count(manager_.get());
    for (int i = 1; i <= var_count; ++i) {
        if (weight_map_.find(i) == weight_map_.end()) {
            weight_map_[i] = {0.5, 0.5};
        }
    }
    double result = wmc_propagate(wmc);
    if (!std::isfinite(result)) {
        auto describeVar = [&](int literal) {
            auto itNode = node_map_.find(literal);
            if (itNode != node_map_.end() && itNode->second) {
                std::cerr << "    literal " << literal << " -> node tuple="
                          << itNode->second->getTuple().toString()
                          << ", prob=" << itNode->second->getProbability() << "\n";
                return;
            }
            auto itEdge = edge_map_.find(literal);
            if (itEdge != edge_map_.end() && itEdge->second) {
                std::cerr << "    literal " << literal << " -> edge " << itEdge->second->toString()
                          << ", prob=" << itEdge->second->getProbability() << "\n";
                return;
            }
            std::cerr << "    literal " << literal << " -> (unknown origin)" << "\n";
        };

        std::cerr << "[Error] WMC result is not finite: " << result << "\n";
        for (const auto& [var, weights] : weight_map_) {
            bool invalid = (weights.first < 0 || weights.second < 0 ||
                            weights.first > 1 || weights.second > 1);
            double sum = weights.first + weights.second;
            if (invalid || std::fabs(sum - 1.0) > 1e-9) {
                std::cerr << "  Var " << var << ": +=" << weights.first
                          << ", -=" << weights.second
                          << " (sum=" << sum << ")\n";
                describeVar(var);
            }
        }
        assert(false && "WMC result is NaN or Inf");
    }
    wmc_manager_free(wmc);
    return result;
}

inline void SddFormulaManager::printInfo(const SddNodeRef& node, const std::string& name) {
    std::cout << "=== Info for: " << name << " ===\n";
    std::cout << "Size: " << sdd_size(node.get()) << "\n";
    std::cout << "Model Count: " << sdd_model_count(node.get(), manager_.get()) << "\n";
}

inline void SddFormulaManager::dumpProfilingStatistics() {
    std::cout << "Live nodes: " << sdd_manager_live_size(manager_.get()) << "\n";
    std::cout << "Dead nodes: " << sdd_manager_dead_size(manager_.get()) << "\n";
}

#endif // SDDMANAGER_H
