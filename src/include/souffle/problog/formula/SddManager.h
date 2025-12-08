//
// Created by 17308 on 2025/6/9.
//
#ifndef SDDMANAGER_H
#define SDDMANAGER_H

#include <sdd/sdd.h>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <optional>
#include <cstdint>

#include "souffle/problog/formula/FormulaManager.h"
#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/debug/Debugger.h"

class SddNodeRef {
    friend class SddFormulaManager;
public:
    SddNodeRef() : ptr_(nullptr), mgr_(nullptr) {}

    SddNodeRef(SddManager* m, SddNode* n) : ptr_(n), mgr_(m) {
        if (ptr_) sdd_ref(ptr_, mgr_);
    }

    SddNodeRef(SddManager* m, SddNode* n, const Node& node)
        : ptr_(n), mgr_(m), node(&node) {
        if (ptr_) sdd_ref(ptr_, mgr_);
    }

    SddNodeRef(SddManager* m, SddNode* n, const Hyperedge& edge)
        : ptr_(n), mgr_(m), edge(&edge) {
        if (ptr_) sdd_ref(ptr_, mgr_);
    }

    SddNodeRef(const SddNodeRef& other)
        : ptr_(other.ptr_), mgr_(other.mgr_), node(other.node), edge(other.edge) {
        if (ptr_) sdd_ref(ptr_, mgr_);
    }

    SddNodeRef& operator=(const SddNodeRef& other) {
        if (this != &other) {
            if (ptr_ && mgr_) sdd_deref(ptr_, mgr_);
            ptr_ = other.ptr_;
            mgr_ = other.mgr_;
            node = other.node;
            edge = other.edge;
            if (ptr_) sdd_ref(ptr_, mgr_);
        }
        return *this;
    }

    SddNodeRef(SddNodeRef&& other) noexcept
        : ptr_(other.ptr_), mgr_(other.mgr_),
          node(other.node), edge(other.edge) {
        other.ptr_ = nullptr;
    }

    SddNodeRef& operator=(SddNodeRef&& other) noexcept {
        if (this != &other) {
            if (ptr_ && mgr_) sdd_deref(ptr_, mgr_);
            ptr_ = other.ptr_;
            mgr_ = other.mgr_;
            node = other.node;
            edge = other.edge;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    ~SddNodeRef() {
        if (ptr_ && mgr_) sdd_deref(ptr_, mgr_);
    }

    SddNode* get() const { return ptr_; }

private:
    SddNode* ptr_;
    SddManager* mgr_;
    std::optional<const Node*> node;
    std::optional<const Hyperedge*> edge;
};

struct SddVariableWeight {
    double posWeight;  // Weight when variable is true
    double negWeight;  // Weight when variable is false
};

// PySDD 包装了这个 先留个位置 以后对vtree操作可能要用
class VtreeWrapper {
private:
    Vtree* vtree_;
    bool is_ref_;
public:
    explicit VtreeWrapper(const std::string& filename) : is_ref_(false) {
        vtree_ = sdd_vtree_read(filename.c_str());
        if (!vtree_) {
            throw std::runtime_error("sdd_vtree_read failed: " + filename);
        }
    }

    VtreeWrapper(
            SddLiteral var_count,
            const std::vector<SddLiteral>& var_order = {},
            const std::string& type = "balanced",
            const std::vector<SddLiteral>& is_X_var = {}
    ) : is_ref_(false) {
        std::vector<SddLiteral> order = var_order;
        if (order.empty()) {
            order.resize(var_count);
            for (SddLiteral i = 1; i <= var_count; ++i) {
                order[i - 1] = i;
            }
        }

        if (!is_X_var.empty()) {
            vtree_ = sdd_vtree_new_X_constrained(
                    var_count,
                    const_cast<SddLiteral*>(is_X_var.data()),
                    type.c_str());
        } else {
            vtree_ = sdd_vtree_new_with_var_order(
                    var_count,
                    order.data(),
                    type.c_str());
        }

        if (!vtree_) {
            throw std::runtime_error(
                    "Failed to construct vtree with var_count:" +
                    std::to_string(var_count));
        }
    }

    VtreeWrapper(VtreeWrapper&& other) noexcept
        : vtree_(other.vtree_), is_ref_(other.is_ref_) {
        other.vtree_ = nullptr;
        other.is_ref_ = false;
    }

    VtreeWrapper& operator=(VtreeWrapper&& other) noexcept {
        if (this != &other) {
            if (vtree_ && !is_ref_) sdd_vtree_free(vtree_);
            vtree_ = other.vtree_;
            is_ref_ = other.is_ref_;
            other.vtree_ = nullptr;
            other.is_ref_ = false;
        }
        return *this;
    }

    ~VtreeWrapper() {
        if (vtree_ && !is_ref_) sdd_vtree_free(vtree_);
    }

    // For subtree reference case (internal use)
    VtreeWrapper(::Vtree* raw_ptr, bool is_ref)
        : vtree_(raw_ptr), is_ref_(is_ref) {}

    SddLiteral var() const { return sdd_vtree_var(vtree_); }
    bool is_leaf() const { return sdd_vtree_is_leaf(vtree_); }

    VtreeWrapper left() const {
        return VtreeWrapper(sdd_vtree_left(vtree_), true);
    }

    VtreeWrapper right() const {
        return VtreeWrapper(sdd_vtree_right(vtree_), true);
    }

    Vtree* getVtree() const { return vtree_; }
};


class SddFormulaManager : public DDManager<SddNodeRef> {
public:
    SddFormulaManager(
            SddLiteral var_count = 0,
            bool auto_gc_and_minimize = false
            );
    ~SddFormulaManager() override;


    SddNodeRef createVar(int index) override;
    SddNodeRef createVar(int index, const Node& node) override;
    SddNodeRef createVar(int index, const Hyperedge& edge) override;
    SddNodeRef makeAnd(const SddNodeRef& a, const SddNodeRef& b) override;
    SddNodeRef makeAnd(const std::vector<SddNodeRef>& nodes) override;
    SddNodeRef makeOr(const SddNodeRef& a, const SddNodeRef& b) override;
    SddNodeRef makeOr(const std::vector<SddNodeRef>& nodes) override;
    SddNodeRef makeNot(const SddNodeRef& a) override;
    SddNodeRef makeCondition(
            const SddNodeRef& f,
            const std::vector<int>& trueIndexes,
            const std::vector<int>& falseIndexes) override;

    SddNodeRef getTrue() override;
    SddNodeRef getFalse() override;

    bool isSame(const SddNodeRef& a, const SddNodeRef& b) override;

    std::vector<double> buildWeightArray() const;
    void setLiteralWeightsFromArray(
            const std::vector<double>& weights,
            WmcManager* wmc_);
    void setVariableWeight(
            int varIndex,
            double posWeight,
            double negWeight) override;
    double computeWeightedModelCount(const SddNodeRef& node) override;

    void printInfo(const SddNodeRef& node, const std::string& name) override;
    void dumpProfilingStatistics() override;

    SddNodeRef createWeightedExample() override {
        return getTrue(); // Stub example
    }

    std::string toString(const SddNodeRef& node) override {
        return "[SDD node@" +
               std::to_string(
                       reinterpret_cast<std::uintptr_t>(node.get())) + "]";
    }

    int getVarIndex(const Node& node) override;
    int getVarIndex(const Hyperedge& edge) override;

private:
    SddManager* manager_;
    bool auto_gc_;
    std::shared_ptr<VtreeWrapper> vtree_;
    Debugger& debugger_;

    // rawId -> internalId 映射
    std::unordered_map<int,int> rawToInternal;
    std::unordered_map<const Node*, int> nodeToRawIndex_;
    std::unordered_map<const Hyperedge*, int> edgeToRawIndex_;
    // 当前 manager 中已经分配的 internal 变量个数 (1..nextInternalVar)
    int nextInternalVar = 0;

    // 权重：internalId -> weight
    std::unordered_map<int, SddVariableWeight> weight_map_;

    // internalId -> SddNodeRef（literal 缓存）
    std::unordered_map<int, SddNodeRef> variableRegistry;
    std::unordered_map<int, SddLiteral> atom2var;
    std::unordered_map<int, SddLiteral> var2atom;
    // 对应 ProbLog weights[0]：True 的全局权重
    bool hasGlobalTrueWeight_ = false;
    double globalTrueWeight_ = 1.0;
};


// ======================= 构造 / 析构 =======================

inline SddFormulaManager::SddFormulaManager(
        SddLiteral var_count,
        bool auto_gc_and_minimize
        )
    : manager_(nullptr),
      auto_gc_(auto_gc_and_minimize),
      debugger_(Debugger::getInstance()) {

    if (var_count <= 0) {var_count = 1;}
    vtree_ = nullptr;
    std::cout << "using vtree: " << vtree_ << std::endl;
    manager_ = sdd_manager_create(var_count, 0);

        std::cout << "[SDD][Init] var_count=" << var_count
                  << " auto_gc=" << auto_gc_and_minimize
                  << " vtree=" << (vtree_ ? "YES" : "NO")
                  << std::endl;

        std::cout << "[SDD][Init] manager var_count = "
                  << sdd_manager_var_count(manager_) << std::endl;


        nextInternalVar = static_cast<int>(sdd_manager_var_count(manager_));
    }

inline SddFormulaManager::~SddFormulaManager() {
    if (manager_) {
        sdd_manager_free(manager_);
        manager_ = nullptr;
    }
}


// =============== rawIndex -> internalIndex 统一映射 ===============

inline SddNodeRef SddFormulaManager::createVar(int rawIndex) {
    if (rawIndex <= 0) {
        throw std::runtime_error("SDD variable index must be >= 1");
    }

    int internal;
    auto it = rawToInternal.find(rawIndex);
    if (it == rawToInternal.end()) {
        internal = ++nextInternalVar;
        rawToInternal[rawIndex] = internal;

        int varCount = sdd_manager_var_count(manager_);
        if (varCount < internal) {
            sdd_manager_add_var_after_last(manager_);
        }
    } else {
        internal = it->second;
    }

    auto litIt = variableRegistry.find(internal);
    if (litIt != variableRegistry.end()) {
        return litIt->second;
    }

    SddNode* var = sdd_manager_literal(internal, manager_);
    if (!var) {
        throw std::runtime_error("Failed to create SDD literal");
    }

    SddNodeRef ref(manager_, var);
    variableRegistry[internal] = ref;
    return ref;
}

inline SddNodeRef SddFormulaManager::createVar(int rawIndex, const Node& node) {
    if (rawIndex <= 0) {
        throw std::runtime_error("SDD variable index must be >= 1");
    }

    int internal;
    auto it = rawToInternal.find(rawIndex);
    if (it == rawToInternal.end()) {
        internal = ++nextInternalVar;
        rawToInternal[rawIndex] = internal;

        int varCount = sdd_manager_var_count(manager_);
        if (varCount < internal) {
            sdd_manager_add_var_after_last(manager_);
        }
    } else {
        internal = it->second;
    }

    nodeToRawIndex_[&node] = rawIndex;

    auto rit = variableRegistry.find(internal);
    if (rit != variableRegistry.end()) {
        return rit->second;
    }

    SddNode* lit = sdd_manager_literal(internal, manager_);
    if (!lit) {
        throw std::runtime_error("Failed to create SDD literal");
    }

    SddNodeRef ref(manager_, lit, node);
    variableRegistry[internal] = ref;


    return ref;
}

inline SddNodeRef SddFormulaManager::createVar(int rawIndex, const Hyperedge& edge) {
    if (rawIndex <= 0) {
        throw std::runtime_error("SDD variable index must be >= 1");
    }

    int internal;
    auto it = rawToInternal.find(rawIndex);
    if (it == rawToInternal.end()) {
        internal = ++nextInternalVar;
        rawToInternal[rawIndex] = internal;

        int varCount = sdd_manager_var_count(manager_);
        if (varCount < internal) {
            sdd_manager_add_var_after_last(manager_);
        }
    } else {
        internal = it->second;
    }

    edgeToRawIndex_[&edge] = rawIndex;

    auto rit = variableRegistry.find(internal);
    if (rit != variableRegistry.end()) {
        return rit->second;
    }

    SddNode* lit = sdd_manager_literal(internal, manager_);
    if (!lit) {
        throw std::runtime_error("Failed to create SDD literal");
    }

    SddNodeRef ref(manager_, lit, edge);
    variableRegistry[internal] = ref;


    return ref;
}

inline int SddFormulaManager::getVarIndex(const Node& node) {
    auto it = nodeToRawIndex_.find(&node);
    if (it == nodeToRawIndex_.end()) {
        throw std::runtime_error("Node not registered in SddFormulaManager::getVarIndex");
    }
    return it->second;  // 返回 rawIndex
}

inline int SddFormulaManager::getVarIndex(const Hyperedge& edge) {
    auto it = edgeToRawIndex_.find(&edge);
    if (it == edgeToRawIndex_.end()) {
        throw std::runtime_error("Hyperedge not registered in SddFormulaManager::getVarIndex");
    }
    return it->second;  // 返回 rawIndex
}

// ========================= 布尔操作 =========================

inline SddNodeRef SddFormulaManager::makeAnd(
        const SddNodeRef& a, const SddNodeRef& b) {
    if (!a.get() || !b.get()) {
        throw std::runtime_error("makeAnd received null SDD node");
    }
    SddNode* res = sdd_conjoin(a.get(), b.get(), manager_);
    if (!res) {
        throw std::runtime_error("sdd_conjoin failed");
    }
    return SddNodeRef(manager_, res);
}

inline SddNodeRef SddFormulaManager::makeAnd(
        const std::vector<SddNodeRef>& nodes) {
    if (nodes.empty()) return getTrue();
    SddNodeRef result = nodes[0];
    for (size_t i = 1; i < nodes.size(); ++i) {
        result = makeAnd(result, nodes[i]);
    }
    return result;
}

inline SddNodeRef SddFormulaManager::makeOr(
        const SddNodeRef& a, const SddNodeRef& b) {
    if (!a.get() || !b.get()) {
        throw std::runtime_error("makeOr received null SDD node");
    }
    return SddNodeRef(manager_, sdd_disjoin(a.get(), b.get(), manager_));
}

inline SddNodeRef SddFormulaManager::makeOr(
        const std::vector<SddNodeRef>& nodes) {
    if (nodes.empty()) return getFalse();
    SddNodeRef result = nodes[0];
    for (size_t i = 1; i < nodes.size(); ++i) {
        result = makeOr(result, nodes[i]);
    }
    return result;
}

inline SddNodeRef SddFormulaManager::makeNot(const SddNodeRef& a) {
    return SddNodeRef(manager_, sdd_negate(a.get(), manager_));
}

inline SddNodeRef SddFormulaManager::getTrue() {
    return SddNodeRef(manager_, sdd_manager_true(manager_));
}

inline SddNodeRef SddFormulaManager::getFalse() {
    return SddNodeRef(manager_, sdd_manager_false(manager_));
}


// ========================= 条件化 =========================

inline SddNodeRef SddFormulaManager::makeCondition(
        const SddNodeRef& f,
        const std::vector<int>& trueIndexes,
        const std::vector<int>& falseIndexes) {
    if (!f.get()) return f;

    SddNodeRef result = f;

    // 正 literal
    for (int rawIdx : trueIndexes) {
        if (rawIdx <= 0) {
            throw std::runtime_error("SDD condition index must be >= 1");
        }

        int internal;
        auto it = rawToInternal.find(rawIdx);
        if (it == rawToInternal.end()) {
            internal = ++nextInternalVar;
            rawToInternal[rawIdx] = internal;

            int varCount = sdd_manager_var_count(manager_);
            while (varCount < internal) {
                sdd_manager_add_var_after_last(manager_);
                varCount = sdd_manager_var_count(manager_);
            }
        } else {
            internal = it->second;
        }

        SddNode* conditioned =
                sdd_condition(static_cast<SddLiteral>(internal),
                              result.get(),
                              manager_);
        result = SddNodeRef(manager_, conditioned);
    }

    // 负 literal
    for (int rawIdx : falseIndexes) {
        if (rawIdx <= 0) {
            throw std::runtime_error("SDD condition index must be >= 1");
        }

        int internal;
        auto it = rawToInternal.find(rawIdx);
        if (it == rawToInternal.end()) {
            internal = ++nextInternalVar;
            rawToInternal[rawIdx] = internal;

            int varCount = sdd_manager_var_count(manager_);
            while (varCount < internal) {
                sdd_manager_add_var_after_last(manager_);
                varCount = sdd_manager_var_count(manager_);
            }
        } else {
            internal = it->second;
        }

        SddNode* conditioned =
                sdd_condition(static_cast<SddLiteral>(-internal),
                              result.get(),
                              manager_);
        result = SddNodeRef(manager_, conditioned);
    }

    return result;
}


// ========================= 权重 & WMC =========================

inline bool SddFormulaManager::isSame(
        const SddNodeRef& a, const SddNodeRef& b) {
    return a.get() == b.get();
}

// rawIndex：0 特殊表示 True 权重，其余映射到 internalId
inline void SddFormulaManager::setVariableWeight(
        int rawIndex, double posWeight, double negWeight) {
    // 0 -> 全局 True 权重，只用正项
    if (rawIndex == 0) {
        hasGlobalTrueWeight_ = true;
        globalTrueWeight_ = posWeight;
        return;
    }

    if (rawIndex < 0) {
        throw std::runtime_error("SDD weight index must be >= 0");
    }
    if (rawIndex == 0) {
        // 已在上面处理，这里只是防御
        return;
    }

    int internal;
    auto it = rawToInternal.find(rawIndex);
    if (it == rawToInternal.end()) {
        internal = ++nextInternalVar;
        rawToInternal[rawIndex] = internal;

        int varCount = sdd_manager_var_count(manager_);
        while (varCount < internal) {
            sdd_manager_add_var_after_last(manager_);
            varCount = sdd_manager_var_count(manager_);
        }
    } else {
        internal = it->second;
    }

    weight_map_[internal] = {posWeight, negWeight};
}

inline std::vector<double> SddFormulaManager::buildWeightArray() const {
    int varCount = sdd_manager_var_count(manager_);

    // layout:
    //   weights[i]           = negWeight for var = i+1
    //   weights[i+varCount]  = posWeight for var = i+1
    std::vector<double> weights(varCount * 2, 0.0);

    for (int i = 0; i < varCount; ++i) {
        int internal = i + 1;

        auto it = weight_map_.find(internal);
        if (it != weight_map_.end()) {
            weights[i] = it->second.negWeight;            // -internal
            weights[i + varCount] = it->second.posWeight; // +internal
        } else {
            // 默认：neg = 0, pos = 1
            weights[i] = 0.0;
            weights[i + varCount] = 1.0;
        }
    }
    return weights;
}

inline void SddFormulaManager::setLiteralWeightsFromArray(
        const std::vector<double>& weights,
        WmcManager* wmc_) {

    int varCount = sdd_manager_var_count(manager_);
    if (weights.size() != static_cast<size_t>(varCount * 2)) {
        throw std::runtime_error(
                "Weight array size mismatch: expected 2*varCount.");
    }

    for (int i = 0; i < varCount; ++i) {
        int var = i + 1;
        double neg = weights[i];
        double pos = weights[i + varCount];
        wmc_set_literal_weight(-var, neg, wmc_);
        wmc_set_literal_weight( var, pos, wmc_);
    }
}

inline double SddFormulaManager::computeWeightedModelCount(
        const SddNodeRef& node) {
    if (!node.get()) {
        return 0.0;
    }
    WmcManager* wmc = wmc_manager_new(node.get(), 0, manager_);
    auto weights = buildWeightArray();
    setLiteralWeightsFromArray(weights, wmc);
    double result = wmc_propagate(wmc);
    wmc_manager_free(wmc);

    // 对应 ProbLog: 如果 weights[0] 存在，就乘上 True 的权重
    if (hasGlobalTrueWeight_) {
        result *= globalTrueWeight_;
    }

    return result;
}


// ========================= Debug 输出 =========================

inline void SddFormulaManager::printInfo(
        const SddNodeRef& node, const std::string& name) {
    std::cout << "=== Info for: " << name << " ===\n";
    std::cout << "Size: " << sdd_size(node.get()) << "\n";
    std::cout << "Model Count: " << sdd_model_count(node.get(), manager_) << "\n";
}

inline void SddFormulaManager::dumpProfilingStatistics() {

    auto n = sdd_manager_var_count(manager_);
    auto approx = 2LL * n - 1;

    auto v = sdd_manager_vtree(manager_);
    std::cout << "[SDD][Chk] var_count=" << n
          << " vtree_count=" << sdd_vtree_count(v)
          << " expected~=" << approx
          << "\n";
    std::cout << "Vtree size:        " << sdd_vtree_size(v) << "\n";
    std::cout << "Vtree live size:   " << sdd_vtree_live_size(v) << "\n";
    std::cout << "Vtree dead size:   " << sdd_vtree_dead_size(v) << "\n";
    std::cout << "Vtree count:       " << sdd_vtree_count(v) << "\n";
    std::cout << "Vtree live count:  " << sdd_vtree_live_count(v) << "\n";
    std::cout << "Vtree dead count:  " << sdd_vtree_dead_count(v) << "\n";

    std::cout << "Live nodes: " << sdd_manager_live_size(manager_) << "\n";
    std::cout << "Dead nodes: " << sdd_manager_dead_size(manager_) << "\n";
}

#endif // SDDMANAGER_H
