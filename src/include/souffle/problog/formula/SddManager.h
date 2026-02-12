//
// Created by 17308 on 2025/6/9.
//
#ifndef SDDMANAGER_H
#define SDDMANAGER_H

#include <sdd/sdd.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <unordered_map>
#include <unordered_set>
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
    SddNodeRef() = default;

    SddNodeRef(std::shared_ptr<SddManager> m, SddNode* n) : ptr_(n), mgr_(std::move(m)) {
        if (ptr_ && mgr_) sdd_ref(ptr_, mgr_.get());
    }

    SddNodeRef(std::shared_ptr<SddManager> m, SddNode* n, const Node& node)
        : ptr_(n), mgr_(std::move(m)), node(&node) {
        if (ptr_ && mgr_) sdd_ref(ptr_, mgr_.get());
    }

    SddNodeRef(std::shared_ptr<SddManager> m, SddNode* n, const Hyperedge& edge)
        : ptr_(n), mgr_(std::move(m)), edge(&edge) {
        if (ptr_ && mgr_) sdd_ref(ptr_, mgr_.get());
    }

    SddNodeRef(const SddNodeRef& other)
        : ptr_(other.ptr_), mgr_(other.mgr_), node(other.node), edge(other.edge) {
        if (ptr_ && mgr_) sdd_ref(ptr_, mgr_.get());
    }

    SddNodeRef& operator=(const SddNodeRef& other) {
        if (this != &other) {
            if (ptr_ && mgr_) sdd_deref(ptr_, mgr_.get());
            ptr_ = other.ptr_;
            mgr_ = other.mgr_;
            node = other.node;
            edge = other.edge;
            if (ptr_ && mgr_) sdd_ref(ptr_, mgr_.get());
        }
        return *this;
    }

    SddNodeRef(SddNodeRef&& other) noexcept
        : ptr_(other.ptr_), mgr_(std::move(other.mgr_)),
          node(other.node), edge(other.edge) {
        other.ptr_ = nullptr;
    }

    SddNodeRef& operator=(SddNodeRef&& other) noexcept {
        if (this != &other) {
            if (ptr_ && mgr_) sdd_deref(ptr_, mgr_.get());
            ptr_ = other.ptr_;
            mgr_ = std::move(other.mgr_);
            node = other.node;
            edge = other.edge;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    ~SddNodeRef() {
        if (ptr_ && mgr_) sdd_deref(ptr_, mgr_.get());
    }

    SddNode* get() const { return ptr_; }

private:
    SddNode* ptr_ = nullptr;
    std::shared_ptr<SddManager> mgr_;
    std::optional<const Node*> node;
    std::optional<const Hyperedge*> edge;
};

struct SddVariableWeight {
    double posWeight;  // Weight when variable is true
    double negWeight;  // Weight when variable is false
};


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
    ~SddFormulaManager() override = default;


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
    FormulaManager<SddNodeRef>::VariableWeight getVariableWeight(int varIndex) const override {
        if (varIndex == 0) {
            return FormulaManager<SddNodeRef>::VariableWeight{globalTrueWeight_, 0.0};
        }
        if (varIndex < 0) {
            return FormulaManager<SddNodeRef>::VariableWeight{1.0, 0.0};
        }
        auto itRaw = rawToInternal.find(varIndex);
        if (itRaw != rawToInternal.end()) {
            auto it = weight_map_.find(itRaw->second);
            if (it == weight_map_.end()) {
                return FormulaManager<SddNodeRef>::VariableWeight{1.0, 0.0};
            }
            return FormulaManager<SddNodeRef>::VariableWeight{it->second.posWeight, it->second.negWeight};
        }
        auto itPending = weight_map_by_raw_.find(varIndex);
        if (itPending == weight_map_by_raw_.end()) {
            return FormulaManager<SddNodeRef>::VariableWeight{1.0, 0.0};
        }
        return FormulaManager<SddNodeRef>::VariableWeight{itPending->second.posWeight, itPending->second.negWeight};
    }
    bool hasVariableWeight(int varIndex) const override {
        if (varIndex == 0) {
            return hasGlobalTrueWeight_;
        }
        if (varIndex < 0) {
            return false;
        }
        auto itRaw = rawToInternal.find(varIndex);
        if (itRaw != rawToInternal.end()) {
            return weight_map_.find(itRaw->second) != weight_map_.end();
        }
        return weight_map_by_raw_.find(varIndex) != weight_map_by_raw_.end();
    }
    double computeWeightedModelCount(const SddNodeRef& node) override;

    void printInfo(const SddNodeRef& node, const std::string& name) override;
    void dumpProfilingStatistics() override;
    std::map<std::string, std::string> getProfilingStatistics() override;
    std::size_t getLiveNodeCount() const override;
    std::size_t getDeadNodeCount() const override;
    std::size_t getTotalNodeCount() const override;

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
    bool peekVarIndex(const Node& node, int& idx) const override;
    bool peekVarIndex(const Hyperedge& edge, int& idx) const override;
    void bindVarIndex(const Node& node, int idx) override;
    void bindVarIndex(const Hyperedge& edge, int idx) override;
    void releaseVarIndex(const Node& node) override;
    void releaseVarIndex(const Hyperedge& edge) override;
    void tryGarbageCollection() override;
    void reset() override;
    void resetHard() override;

private:
    std::shared_ptr<SddManager> manager_;
    bool auto_gc_;
    SddLiteral initialVarCount_ = 1;
    std::shared_ptr<VtreeWrapper> vtree_;
    Debugger& debugger_;

    // rawId -> internalId mapping
    std::unordered_map<int,int> rawToInternal;
    std::unordered_map<const Node*, int> nodeToRawIndex_;
    std::unordered_map<const Hyperedge*, int> edgeToRawIndex_;

    int nextRawVar = 1;
    int nextInternalVar = 0;


    std::unordered_map<int, SddVariableWeight> weight_map_;
    std::unordered_map<int, SddVariableWeight> weight_map_by_raw_;


    std::unordered_map<int, SddNodeRef> variableRegistry;
    std::unordered_map<int, SddLiteral> atom2var;
    std::unordered_map<int, SddLiteral> var2atom;

    std::unordered_set<int> freeIndices_;
    std::unordered_map<UntypedTuple, int> freeNodeIndexByTuple_;
    std::unordered_map<RuleApplication, int> freeEdgeIndexByRuleApp_;
    std::unordered_map<int, UntypedTuple> freeIndexToTuple_;
    std::unordered_map<int, RuleApplication> freeIndexToRuleApp_;

    bool hasGlobalTrueWeight_ = false;
    double globalTrueWeight_ = 1.0;
    bool didGcForReuse_ = false;

    void dropFreeIndex(int idx) {
        freeIndices_.erase(idx);
        auto itTuple = freeIndexToTuple_.find(idx);
        if (itTuple != freeIndexToTuple_.end()) {
            freeNodeIndexByTuple_.erase(itTuple->second);
            freeIndexToTuple_.erase(itTuple);
        }
        auto itRule = freeIndexToRuleApp_.find(idx);
        if (itRule != freeIndexToRuleApp_.end()) {
            freeEdgeIndexByRuleApp_.erase(itRule->second);
            freeIndexToRuleApp_.erase(itRule);
        }
    }

    void ensureGarbageCollectedForReuse() {
        if (didGcForReuse_ || !manager_) {
            return;
        }
        // sdd_manager_is_var_used() only reflects liveness after garbage collection.
        sdd_manager_garbage_collect(manager_.get());
        didGcForReuse_ = true;
    }
};



inline SddFormulaManager::SddFormulaManager(
        SddLiteral var_count,
        bool auto_gc_and_minimize
        )
    : manager_(nullptr),
      auto_gc_(auto_gc_and_minimize),
      debugger_(Debugger::getInstance()) {

    if (var_count <= 0) {
        var_count = 1;
    }
    initialVarCount_ = var_count;
    vtree_ = nullptr;
    std::cout << "using vtree: " << vtree_ << std::endl;
    manager_.reset(sdd_manager_create(var_count, auto_gc_and_minimize ? 1 : 0),
                   [](SddManager* mgr) { sdd_manager_free(mgr); });
    if (!manager_) {
        throw std::runtime_error("sdd_manager_create failed");
    }

        std::cout << "[SDD][Init] var_count=" << var_count
                  << " auto_gc=" << auto_gc_and_minimize
                  << " vtree=" << (vtree_ ? "YES" : "NO")
                  << std::endl;

        std::cout << "[SDD][Init] manager var_count = "
                  << sdd_manager_var_count(manager_.get()) << std::endl;


        nextInternalVar = 0;
        nextRawVar = 1;
    }

inline SddNodeRef SddFormulaManager::createVar(int rawIndex) {
    if (rawIndex <= 0) {
        throw std::runtime_error("SDD variable index must be >= 1");
    }

    int internal;
    auto it = rawToInternal.find(rawIndex);
    if (it == rawToInternal.end()) {
        internal = -1;
        if (reuseVarIndexEnabled && !freeIndices_.empty()) {
            internal = *freeIndices_.begin();
            dropFreeIndex(internal);
        }
        if (internal < 0) {
            internal = ++nextInternalVar;
            int varCount = sdd_manager_var_count(manager_.get());
            while (varCount < internal) {
                sdd_manager_add_var_after_last(manager_.get());
                varCount = sdd_manager_var_count(manager_.get());
            }
        }
        rawToInternal[rawIndex] = internal;
    } else {
        internal = it->second;
    }

    auto litIt = variableRegistry.find(internal);
    if (litIt != variableRegistry.end()) {
        return litIt->second;
    }

    auto rawWeightIt = weight_map_by_raw_.find(rawIndex);
    if (rawWeightIt != weight_map_by_raw_.end()) {
        weight_map_[internal] = rawWeightIt->second;
        weight_map_by_raw_.erase(rawWeightIt);
    }

    SddNode* var = sdd_manager_literal(internal, manager_.get());
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
        internal = -1;
        if (reuseVarIndexEnabled) {
            auto freeIt = freeNodeIndexByTuple_.find(node.getTuple());
            if (freeIt != freeNodeIndexByTuple_.end()) {
                internal = freeIt->second;
                dropFreeIndex(internal);
            } else if (!freeIndices_.empty()) {
                internal = *freeIndices_.begin();
                dropFreeIndex(internal);
            }
        }
        if (internal < 0) {
            internal = ++nextInternalVar;
            int varCount = sdd_manager_var_count(manager_.get());
            while (varCount < internal) {
                sdd_manager_add_var_after_last(manager_.get());
                varCount = sdd_manager_var_count(manager_.get());
            }
        }
        rawToInternal[rawIndex] = internal;
    } else {
        internal = it->second;
    }

    nodeToRawIndex_[&node] = rawIndex;

    auto rit = variableRegistry.find(internal);
    if (rit != variableRegistry.end()) {
        return rit->second;
    }

    auto rawWeightIt = weight_map_by_raw_.find(rawIndex);
    if (rawWeightIt != weight_map_by_raw_.end()) {
        weight_map_[internal] = rawWeightIt->second;
        weight_map_by_raw_.erase(rawWeightIt);
    }

    SddNode* lit = sdd_manager_literal(internal, manager_.get());
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
        internal = -1;
        if (reuseVarIndexEnabled) {
            auto freeIt = freeEdgeIndexByRuleApp_.find(edge.getRuleApp());
            if (freeIt != freeEdgeIndexByRuleApp_.end()) {
                internal = freeIt->second;
                dropFreeIndex(internal);
            } else if (!freeIndices_.empty()) {
                internal = *freeIndices_.begin();
                dropFreeIndex(internal);
            }
        }
        if (internal < 0) {
            internal = ++nextInternalVar;
            int varCount = sdd_manager_var_count(manager_.get());
            while (varCount < internal) {
                sdd_manager_add_var_after_last(manager_.get());
                varCount = sdd_manager_var_count(manager_.get());
            }
        }
        rawToInternal[rawIndex] = internal;
    } else {
        internal = it->second;
    }

    edgeToRawIndex_[&edge] = rawIndex;

    auto rit = variableRegistry.find(internal);
    if (rit != variableRegistry.end()) {
        return rit->second;
    }

    auto rawWeightIt = weight_map_by_raw_.find(rawIndex);
    if (rawWeightIt != weight_map_by_raw_.end()) {
        weight_map_[internal] = rawWeightIt->second;
        weight_map_by_raw_.erase(rawWeightIt);
    }

    SddNode* lit = sdd_manager_literal(internal, manager_.get());
    if (!lit) {
        throw std::runtime_error("Failed to create SDD literal");
    }

    SddNodeRef ref(manager_, lit, edge);
    variableRegistry[internal] = ref;


    return ref;
}

inline int SddFormulaManager::getVarIndex(const Node& node) {
    auto it = nodeToRawIndex_.find(&node);
    if (it != nodeToRawIndex_.end()) return it->second;
    int idx = nextRawVar++;
    nodeToRawIndex_[&node] = idx;
    return idx;
}

inline int SddFormulaManager::getVarIndex(const Hyperedge& edge) {
    auto it = edgeToRawIndex_.find(&edge);
    if (it != edgeToRawIndex_.end()) return it->second;
    int idx = nextRawVar++;
    edgeToRawIndex_[&edge] = idx;
    return idx;
}

inline bool SddFormulaManager::peekVarIndex(const Node& node, int& idx) const {
    auto it = nodeToRawIndex_.find(&node);
    if (it == nodeToRawIndex_.end()) {
        return false;
    }
    idx = it->second;
    return true;
}

inline bool SddFormulaManager::peekVarIndex(const Hyperedge& edge, int& idx) const {
    auto it = edgeToRawIndex_.find(&edge);
    if (it == edgeToRawIndex_.end()) {
        return false;
    }
    idx = it->second;
    return true;
}

inline void SddFormulaManager::bindVarIndex(const Node& node, int idx) {
    if (idx <= 0) {
        return;
    }
    auto it = nodeToRawIndex_.find(&node);
    if (it != nodeToRawIndex_.end() && it->second == idx) {
        return;
    }
    nodeToRawIndex_[&node] = idx;
    if (idx >= nextRawVar) {
        nextRawVar = idx + 1;
    }
}

inline void SddFormulaManager::bindVarIndex(const Hyperedge& edge, int idx) {
    if (idx <= 0) {
        return;
    }
    auto it = edgeToRawIndex_.find(&edge);
    if (it != edgeToRawIndex_.end() && it->second == idx) {
        return;
    }
    edgeToRawIndex_[&edge] = idx;
    if (idx >= nextRawVar) {
        nextRawVar = idx + 1;
    }
}


inline SddNodeRef SddFormulaManager::makeAnd(
        const SddNodeRef& a, const SddNodeRef& b) {
    if (!a.get() || !b.get()) {
        throw std::runtime_error("makeAnd received null SDD node");
    }
    SddNode* res = sdd_conjoin(a.get(), b.get(), manager_.get());
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
    return SddNodeRef(manager_, sdd_disjoin(a.get(), b.get(), manager_.get()));
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
    return SddNodeRef(manager_, sdd_negate(a.get(), manager_.get()));
}

inline SddNodeRef SddFormulaManager::getTrue() {
    return SddNodeRef(manager_, sdd_manager_true(manager_.get()));
}

inline SddNodeRef SddFormulaManager::getFalse() {
    return SddNodeRef(manager_, sdd_manager_false(manager_.get()));
}


inline SddNodeRef SddFormulaManager::makeCondition(
        const SddNodeRef& f,
        const std::vector<int>& trueIndexes,
        const std::vector<int>& falseIndexes) {
    if (!f.get()) return f;

    SddNodeRef result = f;

    // Positive literal
    for (int rawIdx : trueIndexes) {
        if (rawIdx <= 0) {
            throw std::runtime_error("SDD condition index must be >= 1");
        }

        int internal;
        auto it = rawToInternal.find(rawIdx);
        if (it == rawToInternal.end()) {
            internal = -1;
            if (reuseVarIndexEnabled && !freeIndices_.empty()) {
                internal = *freeIndices_.begin();
                dropFreeIndex(internal);
            }
            if (internal < 0) {
                internal = ++nextInternalVar;
                int varCount = sdd_manager_var_count(manager_.get());
                while (varCount < internal) {
                    sdd_manager_add_var_after_last(manager_.get());
                    varCount = sdd_manager_var_count(manager_.get());
                }
            }
            rawToInternal[rawIdx] = internal;

            auto rawWeightIt = weight_map_by_raw_.find(rawIdx);
            if (rawWeightIt != weight_map_by_raw_.end()) {
                weight_map_[internal] = rawWeightIt->second;
                weight_map_by_raw_.erase(rawWeightIt);
            }
        } else {
            internal = it->second;
        }

        SddNode* conditioned =
                sdd_condition(static_cast<SddLiteral>(internal),
                              result.get(),
                              manager_.get());
        result = SddNodeRef(manager_, conditioned);
    }

    for (int rawIdx : falseIndexes) {
        if (rawIdx <= 0) {
            throw std::runtime_error("SDD condition index must be >= 1");
        }

        int internal;
        auto it = rawToInternal.find(rawIdx);
        if (it == rawToInternal.end()) {
            internal = -1;
            if (reuseVarIndexEnabled && !freeIndices_.empty()) {
                internal = *freeIndices_.begin();
                dropFreeIndex(internal);
            }
            if (internal < 0) {
                internal = ++nextInternalVar;
                int varCount = sdd_manager_var_count(manager_.get());
                while (varCount < internal) {
                    sdd_manager_add_var_after_last(manager_.get());
                    varCount = sdd_manager_var_count(manager_.get());
                }
            }
            rawToInternal[rawIdx] = internal;

            auto rawWeightIt = weight_map_by_raw_.find(rawIdx);
            if (rawWeightIt != weight_map_by_raw_.end()) {
                weight_map_[internal] = rawWeightIt->second;
                weight_map_by_raw_.erase(rawWeightIt);
            }
        } else {
            internal = it->second;
        }

        SddNode* conditioned =
                sdd_condition(static_cast<SddLiteral>(-internal),
                              result.get(),
                              manager_.get());
        result = SddNodeRef(manager_, conditioned);
    }

    return result;
}



inline bool SddFormulaManager::isSame(
        const SddNodeRef& a, const SddNodeRef& b) {
    return a.get() == b.get();
}

inline void SddFormulaManager::setVariableWeight(
        int rawIndex, double posWeight, double negWeight) {

    if (rawIndex == 0) {
        hasGlobalTrueWeight_ = true;
        globalTrueWeight_ = posWeight;
        return;
    }

    if (rawIndex < 0) {
        throw std::runtime_error("SDD weight index must be >= 0");
    }

    int internal;
    auto it = rawToInternal.find(rawIndex);
    if (it == rawToInternal.end()) {
        // If the variable isn't mapped/used, don't force manager growth just to store a weight.
        return;
    }

    internal = it->second;
    weight_map_[internal] = {posWeight, negWeight};
    weight_map_by_raw_.erase(rawIndex);
}

inline std::vector<double> SddFormulaManager::buildWeightArray() const {
    int varCount = sdd_manager_var_count(manager_.get());

    std::vector<double> weights(varCount * 2, 0.0);

    for (int i = 0; i < varCount; ++i) {
        int internal = i + 1;

        auto it = weight_map_.find(internal);
        if (it != weight_map_.end()) {
            weights[i] = it->second.negWeight;
            weights[i + varCount] = it->second.posWeight;
        } else {
            // Default: neg = 0, pos = 1
            weights[i] = 0.0;
            weights[i + varCount] = 1.0;
        }
    }
    return weights;
}

inline void SddFormulaManager::setLiteralWeightsFromArray(
        const std::vector<double>& weights,
        WmcManager* wmc_) {

    int varCount = sdd_manager_var_count(manager_.get());
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
    WmcManager* wmc = wmc_manager_new(node.get(), 0, manager_.get());
    auto weights = buildWeightArray();
    setLiteralWeightsFromArray(weights, wmc);
    double result = wmc_propagate(wmc);
    wmc_manager_free(wmc);

    if (hasGlobalTrueWeight_) {
        result *= globalTrueWeight_;
    }

    return result;
}

inline void SddFormulaManager::printInfo(
        const SddNodeRef& node, const std::string& name) {
    std::cout << "=== Info for: " << name << " ===\n";
    std::cout << "Size: " << sdd_size(node.get()) << "\n";
    std::cout << "Model Count: " << sdd_model_count(node.get(), manager_.get()) << "\n";
}

inline void SddFormulaManager::dumpProfilingStatistics() {

    auto n = sdd_manager_var_count(manager_.get());
    auto approx = 2LL * n - 1;

    auto v = sdd_manager_vtree(manager_.get());
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

    std::cout << "Live nodes: " << sdd_manager_live_size(manager_.get()) << "\n";
    std::cout << "Dead nodes: " << sdd_manager_dead_size(manager_.get()) << "\n";
}

inline std::map<std::string, std::string> SddFormulaManager::getProfilingStatistics() {
    std::map<std::string, std::string> stats;
    const auto live_nodes = static_cast<std::size_t>(sdd_manager_live_size(manager_.get()));
    const auto dead_nodes = static_cast<std::size_t>(sdd_manager_dead_size(manager_.get()));
    stats["live_nodes"] = std::to_string(live_nodes);
    stats["dead_nodes"] = std::to_string(dead_nodes);
    stats["total_nodes"] = std::to_string(live_nodes + dead_nodes);
    return stats;
}

inline std::size_t SddFormulaManager::getLiveNodeCount() const {
    return static_cast<std::size_t>(sdd_manager_live_size(manager_.get()));
}

inline std::size_t SddFormulaManager::getDeadNodeCount() const {
    return static_cast<std::size_t>(sdd_manager_dead_size(manager_.get()));
}

inline std::size_t SddFormulaManager::getTotalNodeCount() const {
    return getLiveNodeCount() + getDeadNodeCount();
}

inline void SddFormulaManager::releaseVarIndex(const Node& node) {
    if (!reuseVarIndexEnabled) {
        return;
    }
    ensureGarbageCollectedForReuse();
    auto it = nodeToRawIndex_.find(&node);
    if (it == nodeToRawIndex_.end()) {
        return;
    }
    int raw = it->second;
    nodeToRawIndex_.erase(it);

    auto internalIt = rawToInternal.find(raw);
    if (internalIt == rawToInternal.end()) {
        weight_map_by_raw_.erase(raw);
        return;
    }
    int internal = internalIt->second;
    rawToInternal.erase(internalIt);
    weight_map_by_raw_.erase(raw);

    if (manager_ && sdd_manager_is_var_used(internal, manager_.get())) {
        return;
    }

    weight_map_.erase(internal);
    variableRegistry.erase(internal);
    dropFreeIndex(internal);
    freeIndices_.insert(internal);
    freeNodeIndexByTuple_[node.getTuple()] = internal;
    freeIndexToTuple_[internal] = node.getTuple();
}

inline void SddFormulaManager::releaseVarIndex(const Hyperedge& edge) {
    if (!reuseVarIndexEnabled) {
        return;
    }
    ensureGarbageCollectedForReuse();
    auto it = edgeToRawIndex_.find(&edge);
    if (it == edgeToRawIndex_.end()) {
        return;
    }
    int raw = it->second;
    edgeToRawIndex_.erase(it);

    auto internalIt = rawToInternal.find(raw);
    if (internalIt == rawToInternal.end()) {
        weight_map_by_raw_.erase(raw);
        return;
    }
    int internal = internalIt->second;
    rawToInternal.erase(internalIt);
    weight_map_by_raw_.erase(raw);

    if (manager_ && sdd_manager_is_var_used(internal, manager_.get())) {
        return;
    }

    weight_map_.erase(internal);
    variableRegistry.erase(internal);
    dropFreeIndex(internal);
    freeIndices_.insert(internal);
    freeEdgeIndexByRuleApp_[edge.getRuleApp()] = internal;
    freeIndexToRuleApp_[internal] = edge.getRuleApp();
}

inline void SddFormulaManager::tryGarbageCollection() {
    if (!manager_) {
        return;
    }
    // Avoid doing full GC every turn; let the library decide based on dead-node ratio.
    sdd_manager_garbage_collect_if(0.30f, manager_.get());
    didGcForReuse_ = false;
}

inline void SddFormulaManager::reset() {
    nodeToRawIndex_.clear();
    edgeToRawIndex_.clear();
    rawToInternal.clear();
    nextRawVar = 1;
    nextInternalVar = 0;
    weight_map_.clear();
    weight_map_by_raw_.clear();
    variableRegistry.clear();
    freeIndices_.clear();
    freeNodeIndexByTuple_.clear();
    freeEdgeIndexByRuleApp_.clear();
    freeIndexToTuple_.clear();
    freeIndexToRuleApp_.clear();
    hasGlobalTrueWeight_ = false;
    globalTrueWeight_ = 1.0;
    didGcForReuse_ = false;
    tryGarbageCollection();
}

inline void SddFormulaManager::resetHard() {
    reset();
    manager_.reset(sdd_manager_create(initialVarCount_, auto_gc_ ? 1 : 0),
                   [](SddManager* mgr) { sdd_manager_free(mgr); });
    if (!manager_) {
        throw std::runtime_error("sdd_manager_create failed");
    }
}

#endif // SDDMANAGER_H
