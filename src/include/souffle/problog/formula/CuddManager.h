#ifndef CUDDMANAGER_H
#define CUDDMANAGER_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include "souffle/problog/formula/FormulaManager.h"

extern "C" {
#include <cudd.h>
}

// Cudd Version. Should rename.

class BddNodeRef {
public:
    BddNodeRef(DdManager* m, DdNode* n) : manager(m), node(n) {
        if (node) Cudd_Ref(node);
    }
    
    BddNodeRef(BddNodeRef&& other) noexcept
        : manager(other.manager), node(other.node) {
        other.node = nullptr;
    }
    
    BddNodeRef& operator=(BddNodeRef&& other) noexcept {
        if (this != &other) {
            if (node) Cudd_RecursiveDeref(manager, node);
            manager = other.manager;
            node = other.node;
            other.node = nullptr;
        }
        return *this;
    }
    
    ~BddNodeRef() {
        if (node) Cudd_RecursiveDeref(manager, node);
    }
    
    BddNodeRef(const BddNodeRef&) = delete;
    BddNodeRef& operator=(const BddNodeRef&) = delete;
    
    DdNode* get() const { return node; }
    
private:
    DdManager* manager;
    DdNode* node;
};

struct VariableWeight {
    double posWeight;  // Weight when variable is true
    double negWeight;  // Weight when variable is false
};

class WeightedBDDManager: public DDManager<BddNodeRef> {
public:
    WeightedBDDManager();
    ~WeightedBDDManager () override;

    // Basic BDD operations
    BddNodeRef createVar(int index) override;
    BddNodeRef makeAnd(const BddNodeRef& a, const BddNodeRef& b) override;
    BddNodeRef makeAnd(const std::vector<BddNodeRef>& nodes) override;
    BddNodeRef makeOr(const BddNodeRef& a, const BddNodeRef& b) override;
    BddNodeRef makeOr(const std::vector<BddNodeRef>& nodes) override;
    BddNodeRef makeNot(const BddNodeRef& a) override;

    // Weight-related operations
    void setVariableWeight(int varIndex, double posWeight, double negWeight) override;
    double computeWeightedModelCount(const BddNodeRef& node) override;

    BddNodeRef createWeightedExample() override;

    // Utility functions
    void printInfo(const BddNodeRef& node, const std::string& name) override;
    DdManager* getManager() const { return manager.get(); }

private:
    double recursiveWeightedModelCount(DdNode* node, 
                                     std::unordered_map<DdNode*, double>& cache);

    struct ManagerDeleter {
        void operator()(DdManager* m) { if (m) Cudd_Quit(m); }
    };
    std::unique_ptr<DdManager, ManagerDeleter> manager;
    std::unordered_map<int, VariableWeight> weights;
};


WeightedBDDManager::WeightedBDDManager() {
    DdManager* m = Cudd_Init(0, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0);
    if (m == nullptr) {
        throw std::runtime_error("Failed to initialize CUDD manager");
    }
    manager.reset(m);
}

WeightedBDDManager::~WeightedBDDManager() = default;

BddNodeRef WeightedBDDManager::createVar(int index) {
    DdNode* var = Cudd_bddIthVar(manager.get(), index);
    return BddNodeRef(manager.get(), var);
}

BddNodeRef WeightedBDDManager::makeAnd(const BddNodeRef& a, const BddNodeRef& b) {
    DdNode* result = Cudd_bddAnd(manager.get(), a.get(), b.get());
    return BddNodeRef(manager.get(), result);
}

BddNodeRef WeightedBDDManager::makeAnd(const std::vector<BddNodeRef>& nodes) {
    if (nodes.empty()) {
        return BddNodeRef(manager.get(), Cudd_ReadOne(manager.get()));
    }
    DdNode* result = nodes[0].get();
    for (size_t i = 1; i < nodes.size(); i++) {
        result = Cudd_bddAnd(manager.get(), result, nodes[i].get());
    }
    return BddNodeRef(manager.get(), result);
}

BddNodeRef WeightedBDDManager::makeOr(const BddNodeRef& a, const BddNodeRef& b) {
    DdNode* result = Cudd_bddOr(manager.get(), a.get(), b.get());
    return BddNodeRef(manager.get(), result);
}

BddNodeRef WeightedBDDManager::makeOr(const std::vector<BddNodeRef>& nodes) {
    if (nodes.empty()) {
        return BddNodeRef(manager.get(), Cudd_ReadZero(manager.get()));
    }
    DdNode* result = nodes[0].get();
    for (size_t i = 1; i < nodes.size(); i++) {
        result = Cudd_bddOr(manager.get(), result, nodes[i].get());
    }
    return BddNodeRef(manager.get(), result);
}

BddNodeRef WeightedBDDManager::makeNot(const BddNodeRef& a) {
    DdNode* result = Cudd_Not(a.get());
    return BddNodeRef(manager.get(), result);
}

void WeightedBDDManager::setVariableWeight(int varIndex, double posWeight, double negWeight) {
    weights[varIndex] = VariableWeight{posWeight, negWeight};
}

double WeightedBDDManager::computeWeightedModelCount(const BddNodeRef& node) {
    std::unordered_map<DdNode*, double> cache;
    return recursiveWeightedModelCount(node.get(), cache);
}

double WeightedBDDManager::recursiveWeightedModelCount(
    DdNode* node,
    std::unordered_map<DdNode*, double>& cache) {

    // Check if node is constant
    if (Cudd_IsConstant(node)) {
        return Cudd_IsComplement(node) ? 0.0 : 1.0;
    }

    // Check cache
    auto it = cache.find(node);
    if (it != cache.end()) {
        return it->second;
    }

    // Get node's variable index
    int varIndex = Cudd_NodeReadIndex(node);

    // Get then and else cofactors
    DdNode* T = Cudd_T(node);
    DdNode* E = Cudd_E(node);

    // If node is complemented, adjust cofactors
    if (Cudd_IsComplement(node)) {
        T = Cudd_Not(T);
        E = Cudd_Not(E);
    }

    // Get weights for this variable
    auto weightIt = weights.find(varIndex);
    double posWeight = weightIt != weights.end() ? weightIt->second.posWeight : 1.0;
    double negWeight = weightIt != weights.end() ? weightIt->second.negWeight : 0.0;

    // Recursive computation
    double tWeight = recursiveWeightedModelCount(T, cache);
    double eWeight = recursiveWeightedModelCount(E, cache);

    // Combine results
    double result = posWeight * tWeight + negWeight * eWeight;

    // Cache and return result
    cache[node] = result;
    return result;
}

BddNodeRef WeightedBDDManager::createWeightedExample() {
    // Create variables a, b, c
    auto a = createVar(0);
    auto b = createVar(1);
    auto c = createVar(2);

    // Set weights
    setVariableWeight(0, 0.7, 0.3);  // a: P(true)=0.7, P(false)=0.3
    setVariableWeight(1, 0.6, 0.4);  // b: P(true)=0.6, P(false)=0.4
    setVariableWeight(2, 0.8, 0.2);  // c: P(true)=0.8, P(false)=0.2

    // Create (a AND b) OR (NOT c)
    auto ab = makeAnd(a, b);
    auto notC = makeNot(c);
    return makeOr(ab, notC);
}

void WeightedBDDManager::printInfo(const BddNodeRef& node, const std::string& name) {
    std::cout << "BDD Info for " << name << ":" << std::endl;
    std::cout << "Number of nodes: " << Cudd_DagSize(node.get()) << std::endl;
    std::cout << "Number of paths: " << Cudd_CountPath(node.get()) << std::endl;
    std::cout << "Weighted model count: " << computeWeightedModelCount(node) << std::endl;
}

#endif //CUDDMANAGER_H