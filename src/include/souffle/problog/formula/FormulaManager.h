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
    virtual NodeRef createVar(int index, const Node& tuple) = 0;  // TODO: may change to pointer
    virtual NodeRef createVar(int index, const Hyperedge& edge) = 0;
    virtual NodeRef makeAnd(const NodeRef& a, const NodeRef& b) = 0;
    virtual NodeRef makeAnd(const std::vector<NodeRef>& nodes) = 0;
    virtual NodeRef makeOr(const NodeRef& a, const NodeRef& b) = 0;
    virtual NodeRef makeOr(const std::vector<NodeRef>& nodes) = 0;
    virtual NodeRef makeNot(const NodeRef& a) = 0;
    virtual NodeRef makeCondition(const NodeRef& f,
        const std::vector<int>& trueIndexes, const std::vector<int>& falseIndexes);
    virtual void postprocessUselessVariables(const std::set<int>& condVars) {};
    virtual bool isSame(const NodeRef& a, const NodeRef& b) = 0;
    virtual NodeRef getTrue() = 0;
    virtual NodeRef getFalse() = 0;
    virtual std::string toString(const NodeRef& node) = 0;
    virtual void preConfig(DerivationGraphViewInterface& view) {};
    virtual void setVariableWeight(int varIndex, double posWeight, double negWeight) = 0;
    virtual double computeWeightedModelCount(const NodeRef& node) = 0;

    virtual void printInfo(const NodeRef& node, const std::string& name) = 0;
    virtual void dumpProfilingStatistics() = 0;
    virtual void stopDynamicOptimization() {};
    virtual std::map<std::string, std::string> getProfilingStatistics() {
        return {};
    }
};

template<typename NodeRef>
class DDManager : public FormulaManager<NodeRef> {
public:
    virtual ~DDManager() = default;

    virtual void setVariableWeight(int varIndex, double posWeight, double negWeight) = 0;
    virtual double computeWeightedModelCount(const NodeRef& node) = 0;
    virtual NodeRef createWeightedExample() = 0;
};

template<typename SrcNodeRef, typename DstNodeRef>
DstNodeRef transformFormula(
    const SrcNodeRef& src,
    FormulaManager<SrcNodeRef>& srcMgr,
    FormulaManager<DstNodeRef>& dstMgr) {

    // Check cache first
    // auto it = cache.find(&src);
    // if (it != cache.end()) {
    //     return it->second;
    // }

    // Get the result based on node type

    if ( src.isVar() ) {
        int varIndex = src.getVarIndex() ;
        DstNodeRef var = dstMgr.createVar(varIndex);
        // TODO: automatically passing weights
        return var;
    }
    else if ( src.isNot() ) {
        SrcNodeRef child = src.getOperands()[0];
        DstNodeRef transformedChild = transformFormula(child, srcMgr, dstMgr);
        return dstMgr.makeNot(transformedChild);
    }
    else if ( src.isAnd() ) {
        std::vector<SrcNodeRef> children = src.getOperands();
        std::vector<DstNodeRef> transformedChildren;
        transformedChildren.reserve(children.size());

        for (auto& child : children) {
            transformedChildren.push_back(
                transformFormula(child, srcMgr, dstMgr));
        }

        return dstMgr.makeAnd(transformedChildren);
    }
    else if ( src.isOr() ) {
        std::vector<SrcNodeRef> children = src.getOperands();
        std::vector<DstNodeRef> transformedChildren;
        transformedChildren.reserve(children.size());

        for (auto& child : children) {
            transformedChildren.push_back(
                transformFormula(child, srcMgr, dstMgr));
        }

        return  dstMgr.makeOr(transformedChildren);
    }
    else {
        assert (false && "Unknown node type");
    }

    // cache[src] = result;
}

template<typename SrcNodeRef, typename DstNodeRef>
DstNodeRef transform(
    const SrcNodeRef& src,
    FormulaManager<SrcNodeRef>& srcMgr,
    FormulaManager<DstNodeRef>& dstMgr) {
    // std::unordered_map<SrcNodeRef, DstNodeRef> cache;
    return transformFormula(src, srcMgr, dstMgr);
}



#endif //FORMULAMANAGER_H
