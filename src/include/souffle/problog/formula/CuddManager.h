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
#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/formula/GraphHeuristics.h"
#include "souffle/problog/debug/Debugger.h"
extern "C" {
#include <cudd.h>
}


double getCacheHitRate(DdManager* manager);
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
        : manager(m), ddNode(n), node(&node) {
        if (ddNode) Cudd_Ref(ddNode);
    }

    // Constructor with Hyperedge
    BddNodeRef(std::shared_ptr<DdManager> m, DdNode* n, const Hyperedge& edge)
        : manager(m), ddNode(n), edge(&edge) {
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
    std::optional<const Node*> node;
    std::optional<const Hyperedge*> edge;
};

struct VariableWeight {
    double posWeight;  // Weight when variable is true
    double negWeight;  // Weight when variable is false
};
class WeightedBDDManager: public DDManager<BddNodeRef> {
public:
    WeightedBDDManager();
    ~WeightedBDDManager() override = default;

    void tryGarbageCollection() {
        Cudd_ReduceHeap(manager.get(), CUDD_REORDER_NONE, 0);
    }
    void stopDynamicOptimization() override {
        Cudd_AutodynDisable(manager.get());
    }

    BDDForceHeuristics heuristics;
    Debugger& debugger = Debugger::getInstance();
    void preConfig(DerivationGraphViewInterface& view) override {
        static size_t iteration = 0;
        using namespace std::chrono;
        auto start = high_resolution_clock::now();
        auto end = high_resolution_clock::now();
        auto duration = duration_cast<milliseconds>(end - start).count();

        auto oldCuddVarSize = Cudd_ReadSize(manager.get());
        size_t numVarsToAdd = 0;
        start = high_resolution_clock::now();
        for (const auto& node : view.getNodes()) {
            if (node->isFact && node->getProbability() < 1.0) {
                // Create a variable for each fact node if haven't been created yet
                createVar(mapNodeId(node->getId()), *node);
                numVarsToAdd ++;
            }
        }
        for (const auto& edge : view.getEdges()) {
            if (!edge->isDeterministic()) {
                // Create a variable for each non-deterministic edge
                createVar(mapEdgeId(edge->getId()), *edge);
                numVarsToAdd ++;
            }
        }
        end = high_resolution_clock::now();
        duration = duration_cast<milliseconds>(end - start).count();
        debugger.logMessage(Level::INFO, "CUDD nodes created in " + std::to_string(duration) + " ms");
        if (numVarsToAdd < 20) {
            debugger.logMessage(Level::INFO, "Number of variables to add is small (" + std::to_string(numVarsToAdd) + "), skip static ordering");
            return;
        }
        start = high_resolution_clock::now();
        heuristics.compute(view);
        std::vector<int> order = heuristics.getOrder();
        heuristics.setAnchorOrder(order);
        end = high_resolution_clock::now();
        duration = duration_cast<milliseconds>(end - start).count();
        debugger.logMessage(Level::INFO, "Heuristic ordering computed in " + std::to_string(duration) + " ms");

        start = high_resolution_clock::now();
        // for each variable, if it is not in the heuristic order, then put it at the beginning
        // since it must be deleted
        std::vector<int> newOrder;
        for (int i = 0; i < Cudd_ReadSize(manager.get()); ++i) {
            int index = Cudd_ReadPerm(manager.get(), i);
            auto it = std::find(order.begin(), order.end(), index);
            if (it == order.end()) {
                newOrder.push_back(index);
            }
        }
        // append order to newOrder
        newOrder.insert(newOrder.end(), order.begin(), order.end());
        Cudd_ShuffleHeap(manager.get(), newOrder.data());
        end = high_resolution_clock::now();
        duration = duration_cast<milliseconds>(end - start).count();
        debugger.logMessage(Level::INFO, "CUDD heap shuffled in " + std::to_string(duration) + " ms");
    }

    // Basic BDD operations
    BddNodeRef createVar(int index) override;
    BddNodeRef createVar(int index, const Node& node) override;
    BddNodeRef createVar(int index, const Hyperedge& edge) override;

    BddNodeRef makeAnd(const BddNodeRef& a, const BddNodeRef& b) override;
    BddNodeRef makeAnd(const std::vector<BddNodeRef>& nodes) override;
    BddNodeRef makeOr(const BddNodeRef& a, const BddNodeRef& b) override;
    BddNodeRef makeOr(const std::vector<BddNodeRef>& nodes) override;
    BddNodeRef makeNot(const BddNodeRef& a) override;
    BddNodeRef makeCondition(const BddNodeRef& f,
        const std::vector<int>& trueIndexes, const std::vector<int>& falseIndexes) override;
    bool isSame(const BddNodeRef& a, const BddNodeRef& b) override;
    void postprocessUselessVariables(const std::set<int>& condVars);
    BddNodeRef getTrue() override {
        return BddNodeRef(manager, Cudd_ReadOne(manager.get()));
    }
    BddNodeRef getFalse() override {
        return BddNodeRef(manager, Cudd_ReadLogicZero(manager.get()));
    }

    std::string toString(const BddNodeRef& nodeRef) override;

    // Weight-related operations
    void setVariableWeight(int varIndex, double posWeight, double negWeight) override;
    double computeWeightedModelCount(const BddNodeRef& node) override;

    BddNodeRef createWeightedExample() override;

    // Utility functions
    void printInfo(const BddNodeRef& node, const std::string& name) override;
    DdManager* getManager() const { return manager.get(); }
    void dumpProfilingStatistics() override {
            std::cout << "Current live nodes: " << Cudd_ReadNodeCount(manager.get()) << std::endl;
            std::cout << "Memory usage: " << Cudd_ReadMemoryInUse(manager.get()) / (1024.0 * 1024) << " MB" << std::endl;
            // current variable ordering
            std::cout << "Current variable ordering: ";
            for (int i = 0; i < Cudd_ReadSize(manager.get()); ++i) {
                int index = Cudd_ReadPerm(manager.get(), i);
                std::cout << getVariableName(index) << " ";
            }
            std::cout << std::endl;
//            Cudd_PrintInfo(manager.get(), stdout);
    };
    std::map<std::string, std::string> getProfilingStatistics() override {
        std::map<std::string, std::string> stats;
        stats["live_nodes"] = std::to_string(Cudd_ReadNodeCount(manager.get()));
        stats["memory_usage_mb"] = std::to_string(Cudd_ReadMemoryInUse(manager.get()) / (1024.0 * 1024));
        stats["cache_hits"] = std::to_string(Cudd_ReadCacheHits(manager.get()));
        stats["cache_lookups"] = std::to_string(Cudd_ReadCacheLookUps(manager.get()));
        stats["cache_hit_rate"] = std::to_string(getCacheHitRate(manager.get()) * 100.0) + "%";

        static long last_reordering_time = 0;
        long current_reordering_time = Cudd_ReadReorderingTime(manager.get());
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.3f", (current_reordering_time - last_reordering_time) / 1000.0);
        stats["reordering_runtime"] = std::string(buf);
        last_reordering_time = current_reordering_time;
        return stats;
    }


private:
    BddNodeRef makeAndBalanced(const std::vector<BddNodeRef>& nodes, size_t begin, size_t end);
    BddNodeRef makeOrBalanced(const std::vector<BddNodeRef>& nodes, size_t begin, size_t end);
    BddNodeRef makeAndSequential(const std::vector<BddNodeRef>& nodes);
    BddNodeRef makeOrSequential(const std::vector<BddNodeRef>& nodes);
    double recursiveWeightedModelCount(DdNode* node,
                                     std::unordered_map<DdNode*, double>& cache);

    std::shared_ptr<DdManager> manager;
    std::unordered_map<int, VariableWeight> weights;

    std::string toStringRecursive(DdNode* node,
                            std::unordered_map<DdNode*, std::string>& cache);
    std::string getVariableName(int varIndex);
    std::unordered_map<int, BddNodeRef> variableRegistry;


};

double getCacheHitRate(DdManager* manager) {
    double hits = static_cast<double>(Cudd_ReadCacheHits(manager));
    double lookups = static_cast<double>(Cudd_ReadCacheLookUps(manager));
    if (lookups == 0.0) return 0.0;
    return hits / lookups;
}




using Clock = std::chrono::steady_clock;
using Duration = std::chrono::duration<double>;
Cudd_ReorderingType currentReorderingType = CUDD_REORDER_SAME;
void adaptiveReorder(DdManager* manager) {
    size_t node_count = Cudd_ReadNodeCount(manager);
    Cudd_ReorderingType next = CUDD_REORDER_NONE;

    if (node_count < 10000) {next = CUDD_REORDER_SIFT_CONVERGE; }
//    else if (node_count < 30000) next = CUDD_REORDER_SIFT;
    else if (node_count < 50000) {next = CUDD_REORDER_SIFT; }
    else if (node_count < 100000) {next = CUDD_REORDER_WINDOW4_CONV; }
    else if (node_count < 300000) {next = CUDD_REORDER_WINDOW4;}
    else if (node_count < 1000000) {next = CUDD_REORDER_WINDOW2;}
    else if (node_count < 3000000) {next = CUDD_REORDER_WINDOW2;}
    else {next = CUDD_REORDER_NONE; Cudd_AutodynDisable(manager);}

    if (next != currentReorderingType) {
        Cudd_AutodynEnable(manager, next);
        currentReorderingType = next;
        std::cout << "Switched reordering to " << next << " at node count " << node_count << std::endl;
    }
}

void adaptiveReorder2(DdManager* manager) {
    size_t node_count = Cudd_ReadNodeCount(manager);
    Cudd_ReorderingType next = CUDD_REORDER_NONE;

    if (node_count < 50000) {next = CUDD_REORDER_NONE; }
    else if (node_count < 300000) {next = CUDD_REORDER_WINDOW4_CONV;}
    else if (node_count < 1000000) {next = CUDD_REORDER_WINDOW4;}
    else if (node_count < 3000000) {next = CUDD_REORDER_WINDOW2;}
    else {next = CUDD_REORDER_NONE; Cudd_AutodynDisable(manager);}


    if (next != currentReorderingType) {
        Cudd_AutodynEnable(manager, next);
        currentReorderingType = next;
        std::cout << "Switched reordering to " << next << " at node count " << node_count << std::endl;
    }
}


std::chrono::time_point<Clock> _cudd_gc_start_time;
std::chrono::time_point<Clock> _cudd_gc_end_time;
int _cudd_gc_count = 0;
bool gc_begin = true;
int myGCFunc(DdManager* dd, const char* str, void* data) {
    fprintf(stdout, "[GC] Dead = %u, Keys = %u, Mem = %zu\n",
            Cudd_ReadDead(dd), Cudd_ReadKeys(dd), Cudd_ReadMemoryInUse(dd));
    fprintf(stdout, "[GC] Recursive calls = %.2f, Cache Used = %.2f, Cache hits = %.0f, lookups = %.0f, hit rate = %.2f%%\n",
            Cudd_ReadRecursiveCalls(dd), Cudd_ReadUsedSlots(dd), Cudd_ReadCacheHits(dd), Cudd_ReadCacheLookUps(dd),
            getCacheHitRate(dd) * 100.0);
    if (gc_begin) {
        fprintf(stdout, "[GC] Starting GC: %d\n", ++_cudd_gc_count);
        _cudd_gc_start_time = Clock::now();
        gc_begin = false;
    } else {
        _cudd_gc_end_time = Clock::now();
        Duration duration = _cudd_gc_end_time - _cudd_gc_start_time;
        std::cout << "[GC] Time taken: " << duration.count() << " seconds" << std::endl;
        gc_begin = true;
//        if (Cudd_ReadNodeCount(dd) >= 50000) {
//            adaptiveReorder2(dd);
//        } else {
//            Cudd_AutodynDisable(dd);
//            std::cout << "[GC] Disabled reordering due to low node count." << std::endl;
//        }
    }
    return 1;
}


int _cudd_reordering_count = 0;
std::chrono::time_point<Clock> _cudd_reordering_start_time;
std::chrono::time_point<Clock> _cudd_reordering_end_time;
bool reordering_begin = true;
int myVRFunc(DdManager* dd, const char* str, void* data) {
    if (reordering_begin) {
        _cudd_reordering_start_time = Clock::now();
        reordering_begin = false;
        std::cout << "[VR] Starting reordering " << ++_cudd_reordering_count << "..." << std::endl;
    } else {
        _cudd_reordering_end_time = Clock::now();
        Duration duration = _cudd_reordering_end_time - _cudd_reordering_start_time;
        std::cout << "[VR] Time taken: " << duration.count() << " seconds" << std::endl;
        reordering_begin = true;
        // set next reordering threshold
        adaptiveReorder(dd);
    }
    return 1;
}
// Implementation
WeightedBDDManager::WeightedBDDManager() {
    // could make this static, TODO
//    DdManager* m = Cudd_Init(0, 0, 4096 * 2, 2048 * 2024, 32UL * 1024 * 1024 * 1024);
    // 这些参数对性能的影响很复杂。memory设置太大会减少gc=>reordering，reordering不频繁不好，太频繁也不好.
//    DdManager* m = Cudd_Init(0, 0, 4096, 1 << 24, 32UL * 1024 * 1024 * 1024);
//    Cudd_SetMaxCacheHard(m, 10000000);
    DdManager* m = Cudd_Init(0, 0, 4096, 1 << 24, 32UL * 1024 * 1024 * 1024);

//    Cudd_EnableGarbageCollection(m);
//    Cudd_DisableGarbageCollection(m);
//    Cudd_AutodynEnable(m, CUDD_REORDER_GROUP_SIFT);
//    currentReorderingType = CUDD_REORDER_WINDOW4;
//    currentReorderingType = CUDD_REORDER_NONE;
//    Cudd_AutodynEnable(m, currentReorderingType);
    Cudd_AutodynDisable(m);
//    Cudd_SetLooseUpTo(m, 4);  // Set loose up to 4
//    Cudd_SetNextReordering(m, 4);
//    Cudd_SetMaxCacheHard(m, 1 << 28);
//    adaptiveReorder(m);
//    Cudd_SetMaxLive(m, );
    Cudd_AddHook(m, myGCFunc, CUDD_PRE_GC_HOOK);
    Cudd_AddHook(m, myGCFunc, CUDD_POST_GC_HOOK);
    Cudd_AddHook(m, myVRFunc, CUDD_PRE_REORDERING_HOOK);
    Cudd_AddHook(m, myVRFunc, CUDD_POST_REORDERING_HOOK);

    if (m == nullptr) {
        throw std::runtime_error("Failed to initialize CUDD manager");
    }
    manager = std::shared_ptr<DdManager>(m, [](DdManager* m) {
        if (m) Cudd_Quit(m);
    });
}

BddNodeRef WeightedBDDManager::createVar(int index) {
    if (variableRegistry.find(index) != variableRegistry.end()) {
        return variableRegistry[index];
    }
    DdNode* var = Cudd_bddIthVar(manager.get(), index);
    BddNodeRef ref(manager, var);
    variableRegistry[index] = ref;
    return ref;
//    BddNodeRef(manager, var);
}

BddNodeRef WeightedBDDManager::createVar(int index, const Node& node) {
    if (variableRegistry.find(index) != variableRegistry.end()) {
        return variableRegistry[index];
    }
    DdNode* var = Cudd_bddIthVar(manager.get(), index);
    if (var == nullptr) {
        throw std::runtime_error("Failed to create BDD variable");
    }
    BddNodeRef ref(manager, var, node);
    variableRegistry[index] = ref;
    return ref;
}

BddNodeRef WeightedBDDManager::createVar(int index, const Hyperedge& edge) {
    if (variableRegistry.find(index) != variableRegistry.end()) {
        return variableRegistry[index];
    }
    DdNode* var = Cudd_bddIthVar(manager.get(), index);
    if (var == nullptr) {
        throw std::runtime_error("Failed to create BDD variable");
    }
    BddNodeRef ref(manager, var, edge);
    variableRegistry[index] = ref;
//    return BddNodeRef(manager, var, edge);
    return ref;
}

BddNodeRef WeightedBDDManager::makeAnd(const BddNodeRef& a, const BddNodeRef& b) {
    DdNode* result = Cudd_bddAnd(manager.get(), a.get(), b.get());
    if (result == nullptr) {
        std::cout << (Cudd_ReadErrorCode(manager.get())) << std::endl;
//        throw std::runtime_error("makeAnd failed");
        assert(false);
    }
    return BddNodeRef(manager, result);
}

BddNodeRef WeightedBDDManager::makeAnd(const std::vector<BddNodeRef>& nodes) {
//    return makeAndSequential(nodes);
    return makeAndBalanced(nodes, 0, nodes.size());
}

BddNodeRef WeightedBDDManager::makeAndSequential(const std::vector<BddNodeRef>& nodes) {
    if (nodes.empty()) {
        return BddNodeRef(manager, Cudd_ReadOne(manager.get()));
    }
    for (const auto& node : nodes) {
        if (node.get() == nullptr) {
            assert (false && "makeAnd received a null node");
        }
    }
    BddNodeRef result(manager, nodes[0].get());
    for (size_t i = 1; i < nodes.size(); i++) {
        assert (nodes[i].get() != nullptr && "makeAnd received a null node");
        assert (result.get() != nullptr && "makeAnd received a null result node");
        assert (manager.get() != nullptr && "makeAnd received a null manager");
        auto node = Cudd_bddAnd(manager.get(), result.get(), nodes[i].get());
        if (node == nullptr) {
                assert (node != nullptr && "Cudd_bddAnd failed");
        }
        result = BddNodeRef(manager, node);
    }
    return result;
}

BddNodeRef WeightedBDDManager::makeAndBalanced(const std::vector<BddNodeRef>& nodes, size_t begin, size_t end) {
    if (begin >= end) {
        return BddNodeRef(manager, Cudd_ReadOne(manager.get()));
    }
    if (end - begin == 1) {
        return nodes[begin];
    }

    size_t mid = begin + (end - begin) / 2;
    BddNodeRef left = makeAndBalanced(nodes, begin, mid);
    BddNodeRef right = makeAndBalanced(nodes, mid, end);

    if (left.get() == nullptr || right.get() == nullptr || manager.get() == nullptr) {
        throw std::runtime_error("makeAndBalanced received null input or manager");
    }

    DdNode* and_node = Cudd_bddAnd(manager.get(), left.get(), right.get());
    if (and_node == nullptr) {
        throw std::runtime_error("Cudd_bddAnd failed in makeAndBalanced");
    }
    return BddNodeRef(manager, and_node);
}

BddNodeRef WeightedBDDManager::makeOr(const BddNodeRef& a, const BddNodeRef& b) {
    DdNode* result = Cudd_bddOr(manager.get(), a.get(), b.get());
    if (result == nullptr) {
        throw std::runtime_error("makeOr failed");
    }
    return BddNodeRef(manager, result);
}

BddNodeRef WeightedBDDManager::makeOr(const std::vector<BddNodeRef>& nodes) {
//    return makeOrSequential(nodes);
    return makeOrBalanced(nodes, 0, nodes.size());
}

BddNodeRef WeightedBDDManager::makeOrSequential(const std::vector<BddNodeRef>& nodes) {
    if (nodes.empty()) {
        return BddNodeRef(manager, Cudd_ReadZero(manager.get()));
    }
    BddNodeRef result(manager, nodes[0].get());
    for (size_t i = 1; i < nodes.size(); i++) {
        result = BddNodeRef(manager, Cudd_bddOr(manager.get(), result.get(), nodes[i].get()));
        if (result.get() == nullptr) {
            throw std::runtime_error("makeOr failed");
        }
    }
    return result;
}

BddNodeRef WeightedBDDManager::makeOrBalanced(const std::vector<BddNodeRef>& nodes, size_t begin, size_t end) {
    if (begin >= end) {
        return BddNodeRef(manager, Cudd_ReadZero(manager.get()));
    }
    else if (end - begin == 1) {
        return nodes[begin];
    }
    size_t mid = begin + (end - begin) / 2;
    BddNodeRef left = makeOrBalanced(nodes, begin, mid);
    BddNodeRef right = makeOrBalanced(nodes, mid, end);
//    assert (left.get() != nullptr && "makeOrBalanced received null left node");
//    assert (right.get() != nullptr && "makeOrBalanced received null right node");
    DdNode* or_node = Cudd_bddOr(manager.get(), left.get(), right.get());
    if (or_node == nullptr) {
        std::cout << "error code: " << Cudd_ReadErrorCode(manager.get()) << std::endl;
        assert (false && "makeOrBalanced failed");
    }
    return BddNodeRef(manager, or_node);
}

BddNodeRef WeightedBDDManager::makeNot(const BddNodeRef& a) {
    DdNode* result = Cudd_Not(a.get());
    return BddNodeRef(manager, result);
}

BddNodeRef WeightedBDDManager::makeCondition(const BddNodeRef& f,
        const std::vector<int>& trueIndexes, const std::vector<int>& falseIndexes) {
    BddNodeRef cube = getTrue();
    for (int index : trueIndexes) {
        BddNodeRef x = createVar(index);  // find
        cube = makeAnd(cube, x);
    }
    for (int index : falseIndexes) {
        BddNodeRef x = createVar(index);  // find
        cube = makeAnd(cube, makeNot(x));
    }
    return BddNodeRef(manager, Cudd_bddRestrict(manager.get(), f.get(), cube.get()));
}

void WeightedBDDManager::postprocessUselessVariables(const std::set<int>& condVars) {
    DdManager* dd = manager.get();
    int n = Cudd_ReadSize(dd);  // 当前BDD变量个数
    assert(n > 0);
    // 获取当前的变量顺序（每一层对应的变量索引）
    std::vector<int> currentOrder(n);
    for (int level = 0; level < n; ++level) {
        int var = Cudd_ReadInvPerm(dd, level);
        // 检查是否越界或非法
        assert(var >= 0 && var < n);
        currentOrder[level] = var;
    }

    // 构造新的排列：将condVars中的变量移到前面，其他变量顺序不变
    std::vector<int> newOrder;
    newOrder.reserve(n);

    // 先添加需前置的变量（按它们在当前顺序中的出现顺序）
    for (int var : currentOrder) {
        if (condVars.count(var)) {
            assert(var >= 0 && var < n);
            newOrder.push_back(var);
        }
    }
    // 再添加其余变量
    for (int var : currentOrder) {
        if (!condVars.count(var)) {
            assert(var >= 0 && var < n);
            newOrder.push_back(var);
        }
    }

    // 最终长度必须等于n
    assert(static_cast<int>(newOrder.size()) == n);

    // 调用CUDD函数调整变量顺序
    int result = Cudd_ShuffleHeap(dd, newOrder.data());
    if (result != 1) {
        throw std::runtime_error("Cudd_ShuffleHeap failed to reorder variables");
    }
    std::cout << "6" << std::endl;
}

bool WeightedBDDManager::isSame(const BddNodeRef& a, const BddNodeRef& b) {
    return a.get() == b.get();
}

void WeightedBDDManager::setVariableWeight(int varIndex, double posWeight, double negWeight) {
    weights[varIndex] = VariableWeight{posWeight, negWeight};
}

double WeightedBDDManager::computeWeightedModelCount(const BddNodeRef& node) {
//    std::cout << "wmc..." << std::endl;
//    static int count = 0;
//    if (count++ == 0) {
//        for (auto& [index, weight] : weights) {
//            std::cout << "Variable " << index << ": posWeight = " << weight.posWeight
//                      << ", negWeight = " << weight.negWeight << std::endl;
//        }
//    }
    std::unordered_map<DdNode*, double> cache;
    return recursiveWeightedModelCount(node.get(), cache);
}

double WeightedBDDManager::recursiveWeightedModelCount(
    DdNode* node,
    std::unordered_map<DdNode*, double>& cache) {
//    std::cout << "wmc..." << std::endl;
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
            return ref.node.value()->toString();
        } else if (ref.edge.has_value()) {
            return ref.edge.value()->toString();
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