#ifndef CUDDMANAGER_H
#define CUDDMANAGER_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <optional>
#include "souffle/problog/formula/FormulaManager.h"

extern "C" {
#include <cudd.h>
}

// Cudd Version. Should rename.
class BddNodeRef {
friend class WeightedBDDManager;
public:
    // Default constructor
    BddNodeRef() : manager(nullptr), ddNode(nullptr) {}

    // Primary constructor
    BddNodeRef(std::shared_ptr<DdManager> m, DdNode* n) : manager(m), ddNode(n) {
        if (ddNode) Cudd_Ref(ddNode);
    }

    // Constructor with Node
    BddNodeRef(std::shared_ptr<DdManager> m, DdNode* n, const Node& node)
        : manager(m), ddNode(n), node(node) {
        if (ddNode) Cudd_Ref(ddNode);
    }

    // Constructor with Hyperedge
    BddNodeRef(std::shared_ptr<DdManager> m, DdNode* n, const Hyperedge& edge)
        : manager(m), ddNode(n), edge(edge) {
        if (ddNode) Cudd_Ref(ddNode);
    }

    // Copy constructor
    BddNodeRef(const BddNodeRef& other)
        : manager(other.manager), ddNode(other.ddNode),
          node(other.node), edge(other.edge) {
        if (ddNode) Cudd_Ref(ddNode);
    }

    // Copy assignment operator
    BddNodeRef& operator=(const BddNodeRef& other) {
        if (this != &other) {
            // Release existing resources
            if (ddNode && manager) Cudd_RecursiveDeref(manager.get(), ddNode);

            // Copy from other
            manager = other.manager;
            ddNode = other.ddNode;
            node = other.node;
            edge = other.edge;

            // Increment reference count for new node
            if (ddNode) Cudd_Ref(ddNode);
        }
        return *this;
    }

    // Move constructor
    BddNodeRef(BddNodeRef&& other) noexcept
        : manager(std::move(other.manager)),
          ddNode(other.ddNode),
          node(std::move(other.node)),
          edge(std::move(other.edge)) {
        other.ddNode = nullptr;
    }

    // Move assignment operator
    BddNodeRef& operator=(BddNodeRef&& other) noexcept {
        if (this != &other) {
            // Release existing resources
            if (ddNode && manager) Cudd_RecursiveDeref(manager.get(), ddNode);

            // Move from other
            manager = std::move(other.manager);
            ddNode = other.ddNode;
            node = std::move(other.node);
            edge = std::move(other.edge);

            // Reset other
            other.ddNode = nullptr;
        }
        return *this;
    }

    // Destructor
    ~BddNodeRef() {
        if (ddNode && manager) Cudd_RecursiveDeref(manager.get(), ddNode);
    }

    // Getter for the BDD node
    DdNode* get() const { return ddNode; }

    // TODO: static false true node
//    static BddNodeRef getTrue(std::shared_ptr<DdManager> manager) {
//        return BddNodeRef(manager, Cudd_ReadOne(manager.get()));
//    }
//    static BddNodeRef getFalse(std::shared_ptr<DdManager> manager) {
//        return BddNodeRef(manager, Cudd_ReadLogicZero(manager.get()));
//    }
private:
    std::shared_ptr<DdManager> manager;  // Shared ownership of manager
    DdNode* ddNode;
    std::optional<Node> node;
    std::optional<Hyperedge> edge;
};

struct VariableWeight {
    double posWeight;  // Weight when variable is true
    double negWeight;  // Weight when variable is false
};
class WeightedBDDManager: public DDManager<BddNodeRef> {
public:
    WeightedBDDManager();
    ~WeightedBDDManager() override = default;

    // Basic BDD operations
    BddNodeRef createVar(int index) override;
    BddNodeRef createVar(int index, const Node& node) override;
    BddNodeRef createVar(int index, const Hyperedge& edge) override;

    BddNodeRef makeAnd(const BddNodeRef& a, const BddNodeRef& b) override;
    BddNodeRef makeAnd(const std::vector<BddNodeRef>& nodes) override;
    BddNodeRef makeOr(const BddNodeRef& a, const BddNodeRef& b) override;
    BddNodeRef makeOr(const std::vector<BddNodeRef>& nodes) override;
    BddNodeRef makeNot(const BddNodeRef& a) override;
    bool isSame(const BddNodeRef& a, const BddNodeRef& b) override;

    BddNodeRef getTrue() {
        return BddNodeRef(manager, Cudd_ReadOne(manager.get()));
    }
    BddNodeRef getFalse() {
        return BddNodeRef(manager, Cudd_ReadLogicZero(manager.get()));
    }

    std::string toString(const BddNodeRef& nodeRef);

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

    std::shared_ptr<DdManager> manager;
    std::unordered_map<int, VariableWeight> weights;

    std::string toStringRecursive(DdNode* node,
                            std::unordered_map<DdNode*, std::string>& cache);
    std::string getVariableName(int varIndex);
    std::unordered_map<int, BddNodeRef> variableRegistry;


};

// Implementation

WeightedBDDManager::WeightedBDDManager() {
    DdManager* m = Cudd_Init(0, 0, CUDD_UNIQUE_SLOTS, CUDD_CACHE_SLOTS, 0);
//    DdManager* m = Cudd_Init(0, 0, 4096, 2*CUDD_CACHE_SLOTS, 0);
    if (m == nullptr) {
        throw std::runtime_error("Failed to initialize CUDD manager");
    }
    manager = std::shared_ptr<DdManager>(m, [](DdManager* m) {
        if (m) Cudd_Quit(m);
    });
}

BddNodeRef WeightedBDDManager::createVar(int index) {
    DdNode* var = Cudd_bddIthVar(manager.get(), index);
    BddNodeRef ref(manager, var);
    variableRegistry[index] = ref;
    return BddNodeRef(manager, var);
}

BddNodeRef WeightedBDDManager::createVar(int index, const Node& node) {
    DdNode* var = Cudd_bddIthVar(manager.get(), index);
    if (var == nullptr) {
        throw std::runtime_error("Failed to create BDD variable");
    }
    BddNodeRef ref(manager, var, node);
    variableRegistry[index] = ref;
    return BddNodeRef(manager, var, node);
}

BddNodeRef WeightedBDDManager::createVar(int index, const Hyperedge& edge) {
    DdNode* var = Cudd_bddIthVar(manager.get(), index);
    if (var == nullptr) {
        throw std::runtime_error("Failed to create BDD variable");
    }
    BddNodeRef ref(manager, var, edge);
    variableRegistry[index] = ref;
    return BddNodeRef(manager, var, edge);
}

BddNodeRef WeightedBDDManager::makeAnd(const BddNodeRef& a, const BddNodeRef& b) {
    DdNode* result = Cudd_bddAnd(manager.get(), a.get(), b.get());
    if (result == nullptr) {
        throw std::runtime_error("makeAnd failed");
    }
    return BddNodeRef(manager, result);
}

BddNodeRef WeightedBDDManager::makeAnd(const std::vector<BddNodeRef>& nodes) {
    if (nodes.empty()) {
        return BddNodeRef(manager, Cudd_ReadOne(manager.get()));
    }
    DdNode* result = nodes[0].get();
    for (size_t i = 1; i < nodes.size(); i++) {
        result = Cudd_bddAnd(manager.get(), result, nodes[i].get());
        if (result == nullptr) {
            throw std::runtime_error("makeAnd failed");
        }
    }
    return BddNodeRef(manager, result);
}

BddNodeRef WeightedBDDManager::makeOr(const BddNodeRef& a, const BddNodeRef& b) {
    DdNode* result = Cudd_bddOr(manager.get(), a.get(), b.get());
    if (result == nullptr) {
        throw std::runtime_error("makeOr failed");
    }
    return BddNodeRef(manager, result);
}

BddNodeRef WeightedBDDManager::makeOr(const std::vector<BddNodeRef>& nodes) {
    if (nodes.empty()) {
        return BddNodeRef(manager, Cudd_ReadZero(manager.get()));
    }
    DdNode* result = nodes[0].get();
    for (size_t i = 1; i < nodes.size(); i++) {
        result = Cudd_bddOr(manager.get(), result, nodes[i].get());
        if (result == nullptr) {
            throw std::runtime_error("makeOr failed");
        }
    }
    return BddNodeRef(manager, result);
}

BddNodeRef WeightedBDDManager::makeNot(const BddNodeRef& a) {
    DdNode* result = Cudd_Not(a.get());
    return BddNodeRef(manager, result);
}

bool WeightedBDDManager::isSame(const BddNodeRef& a, const BddNodeRef& b) {
    return a.get() == b.get();
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

std::string WeightedBDDManager::toString(const BddNodeRef& nodeRef) {
    if (nodeRef.get() == nullptr) {
        return "NULL";
    }

    static std::unordered_map<DdNode*, std::string> cache;
    return toStringRecursive(nodeRef.get(), cache);
}

std::string WeightedBDDManager::getVariableName(int varIndex) {
    auto it = variableRegistry.find(varIndex);
    if (it != variableRegistry.end()) {
        const BddNodeRef& ref = it->second;
        // Direct access to private members thanks to friendship
        if (ref.node.has_value()) {
            return ref.node.value().toString();
        } else if (ref.edge.has_value()) {
            return ref.edge.value().toString();
        }
    }
    // Fallback to default naming
    return "x" + std::to_string(varIndex);
}

std::string WeightedBDDManager::toStringRecursive(
    DdNode* node,
    std::unordered_map<DdNode*, std::string>& cache) {

    // Check cache first
    auto it = cache.find(node);
    if (it != cache.end()) {
        return it->second;
    }

    // Handle constant nodes
    if (Cudd_IsConstant(node)) {
        return Cudd_IsComplement(node) ? "0" : "1";
    }

    // Get the variable index for this node
    int index = Cudd_NodeReadIndex(Cudd_Regular(node));
    std::string varName = getVariableName(index);

    // Get then and else cofactors
    DdNode* tNode = Cudd_T(Cudd_Regular(node));
    DdNode* eNode = Cudd_E(Cudd_Regular(node));

    // Account for complement if needed
    if (Cudd_IsComplement(node)) {
        tNode = Cudd_Not(tNode);
        eNode = Cudd_Not(eNode);
    }

    // Recursively convert cofactors to formulas
    std::string tFormula = toStringRecursive(tNode, cache);
    std::string eFormula = toStringRecursive(eNode, cache);

    // Apply simplifications for more readable formulas
    std::string formula;

    if (tFormula == "1" && eFormula == "0") {
        formula = varName;
    }
    else if (tFormula == "0" && eFormula == "1") {
        formula = "¬" + varName;
    }
    else if (tFormula == eFormula) {
        formula = tFormula;
    }
    else if (tFormula == "1") {
        formula = varName + " ∨ (¬" + varName + " ∧ " + eFormula + ")";
    }
    else if (tFormula == "0") {
        formula = "¬" + varName + " ∧ " + eFormula;
    }
    else if (eFormula == "1") {
        formula = "¬" + varName + " ∨ (" + varName + " ∧ " + tFormula + ")";
    }
    else if (eFormula == "0") {
        formula = varName + " ∧ " + tFormula;
    }
    else {
        formula = "(" + varName + " ∧ " + tFormula + ") ∨ (¬" + varName + " ∧ " + eFormula + ")";
    }

    // Cache and return result
    cache[node] = formula;
    return formula;
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