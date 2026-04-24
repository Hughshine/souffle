#ifndef FORMULAMANAGER_H
#define FORMULAMANAGER_H
#include <cassert>
#include <cstring>
#include "souffle/Derivation.h"
#include "souffle/problog/DerivationGraph.h"

template<typename NodeRef>
class FormulaManager {
public:
    virtual ~FormulaManager() = default;

    virtual NodeRef createVar(int index) = 0;
    virtual NodeRef createVar(int index, const Node& tuple) = 0;
    virtual NodeRef createVar(int index, const Hyperedge& edge) = 0;
    virtual NodeRef makeAnd(const NodeRef& a, const NodeRef& b) = 0;
    virtual NodeRef makeAnd(const std::vector<NodeRef>& nodes) = 0;
    virtual NodeRef makeOr(const NodeRef& a, const NodeRef& b) = 0;
    virtual NodeRef makeOr(const std::vector<NodeRef>& nodes) = 0;
    virtual NodeRef makeNot(const NodeRef& a) = 0;
    virtual NodeRef makeCondition(const NodeRef& f,
        const std::vector<int>& trueIndexes, const std::vector<int>& falseIndexes) = 0;
    virtual bool isSame(const NodeRef& a, const NodeRef& b) = 0;
    virtual NodeRef getTrue() = 0;
    virtual NodeRef getFalse() = 0;
    virtual std::string toString(const NodeRef& node) = 0;
    virtual void preConfig(DerivationGraphViewInterface& view) {};
    virtual void setVariableWeight(int varIndex, double posWeight, double negWeight) = 0;
    struct VariableWeight { double posWeight; double negWeight; };
    virtual VariableWeight getVariableWeight(int varIndex) const {
        (void)varIndex;
        return VariableWeight{1.0, 0.0};
    }
    virtual bool hasVariableWeight(int varIndex) const {
        (void)varIndex;
        return false;
    }
    virtual double computeWeightedModelCount(const NodeRef& node) = 0;
    virtual int getVarIndex(const Node& node) = 0;
    virtual int getVarIndex(const Hyperedge& edge) = 0;
    virtual bool peekVarIndex(const Node&, int&) const { return false; }
    virtual bool peekVarIndex(const Hyperedge&, int&) const { return false; }
    virtual void bindVarIndex(const Node&, int) {}
    virtual void bindVarIndex(const Hyperedge&, int) {}
    virtual void releaseVarIndex(const Node&) {}
    virtual void releaseVarIndex(const Hyperedge&) {}

    virtual void printInfo(const NodeRef& node, const std::string& name) = 0;
    virtual void dumpProfilingStatistics() = 0;
    virtual std::map<std::string, std::string> getProfilingStatistics() {
        return {};
    }
    virtual std::size_t getLiveNodeCount() const {
        return 0;
    }
    virtual std::size_t getDeadNodeCount() const {
        return 0;
    }
    virtual std::size_t getTotalNodeCount() const {
        return getLiveNodeCount() + getDeadNodeCount();
    }
    virtual void tryGarbageCollection() {};
    virtual void reset() {};
    virtual void resetHard() {};
};

template<typename NodeRef>
class DDManager : public FormulaManager<NodeRef> {
public:
    virtual ~DDManager() = default;

    virtual void setVariableWeight(int varIndex, double posWeight, double negWeight) = 0;
    virtual double computeWeightedModelCount(const NodeRef& node) = 0;
};

#endif //FORMULAMANAGER_H
