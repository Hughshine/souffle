#include "souffle/problog/DerivationGraph.h"

#include "weighted_conversion.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

constexpr long double kProbEps = 1e-15L;
constexpr int kTrueLit = std::numeric_limits<int>::max();
constexpr int kFalseLit = std::numeric_limits<int>::min();

bool isZero(long double x) {
    return std::fabs(x) <= kProbEps;
}

bool isOne(long double x) {
    return std::fabs(x - 1.0L) <= kProbEps;
}

struct RandomVarInfo {
    uint32_t id = 0;            // 1-based
    long double probTrue = 0.0; // P(var=true)
    std::string label;
};

struct Options {
    std::string approxmcBin;
    double epsilon = 0.1;
    double delta = 0.05;
    uint32_t seed = 1;
};

std::string defaultApproxmcBin() {
    const char* envBin = std::getenv("APPROXMC_BIN");
    if (envBin && *envBin) {
        return envBin;
    }
    return "/tmp/approxmc-bin/approxmc";
}

Options parseArgs(int argc, char** argv) {
    Options opt;
    opt.approxmcBin = defaultApproxmcBin();
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--approxmc-bin") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value after --approxmc-bin");
            }
            opt.approxmcBin = argv[++i];
        } else if (arg == "--epsilon") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value after --epsilon");
            }
            opt.epsilon = std::stod(argv[++i]);
        } else if (arg == "--delta") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value after --delta");
            }
            opt.delta = std::stod(argv[++i]);
        } else if (arg == "--seed") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value after --seed");
            }
            opt.seed = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "-h" || arg == "--help") {
            std::cout
                    << "Usage: " << argv[0]
                    << " [--approxmc-bin /path/to/approxmc] [--epsilon 0.1] [--delta 0.05] [--seed 1]\n";
            std::exit(0);
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }
    if (opt.epsilon <= 0.0 || opt.delta <= 0.0 || opt.delta >= 1.0) {
        throw std::runtime_error("Invalid epsilon/delta");
    }
    return opt;
}

bool fileExists(const std::string& path) {
    std::ifstream in(path);
    return in.good();
}

int negateLit(int lit) {
    if (lit == kTrueLit) {
        return kFalseLit;
    }
    if (lit == kFalseLit) {
        return kTrueLit;
    }
    return -lit;
}

class ExprArena {
public:
    enum class Kind {
        Const,
        Var,
        Not,
        And,
        Or,
    };

    struct Expr {
        Kind kind = Kind::Const;
        bool constValue = false;
        uint32_t var = 0;
        std::vector<int> args;
    };

    ExprArena() {
        trueId_ = addConst(true);
        falseId_ = addConst(false);
    }

    int makeConst(bool value) const {
        return value ? trueId_ : falseId_;
    }

    int makeVar(uint32_t var) {
        Expr e;
        e.kind = Kind::Var;
        e.var = var;
        exprs_.push_back(std::move(e));
        return static_cast<int>(exprs_.size() - 1);
    }

    int makeNot(int child) {
        if (child == trueId_) {
            return falseId_;
        }
        if (child == falseId_) {
            return trueId_;
        }
        const Expr& c = exprs_.at(static_cast<size_t>(child));
        if (c.kind == Kind::Not && !c.args.empty()) {
            return c.args.front();
        }
        Expr e;
        e.kind = Kind::Not;
        e.args.push_back(child);
        exprs_.push_back(std::move(e));
        return static_cast<int>(exprs_.size() - 1);
    }

    int makeAnd(const std::vector<int>& terms) {
        std::vector<int> normalized;
        normalized.reserve(terms.size());
        for (int t : terms) {
            if (t == falseId_) {
                return falseId_;
            }
            if (t == trueId_) {
                continue;
            }
            const Expr& e = exprs_.at(static_cast<size_t>(t));
            if (e.kind == Kind::And) {
                normalized.insert(normalized.end(), e.args.begin(), e.args.end());
            } else {
                normalized.push_back(t);
            }
        }
        if (normalized.empty()) {
            return trueId_;
        }
        if (normalized.size() == 1) {
            return normalized.front();
        }
        Expr out;
        out.kind = Kind::And;
        out.args = std::move(normalized);
        exprs_.push_back(std::move(out));
        return static_cast<int>(exprs_.size() - 1);
    }

    int makeOr(const std::vector<int>& terms) {
        std::vector<int> normalized;
        normalized.reserve(terms.size());
        for (int t : terms) {
            if (t == trueId_) {
                return trueId_;
            }
            if (t == falseId_) {
                continue;
            }
            const Expr& e = exprs_.at(static_cast<size_t>(t));
            if (e.kind == Kind::Or) {
                normalized.insert(normalized.end(), e.args.begin(), e.args.end());
            } else {
                normalized.push_back(t);
            }
        }
        if (normalized.empty()) {
            return falseId_;
        }
        if (normalized.size() == 1) {
            return normalized.front();
        }
        Expr out;
        out.kind = Kind::Or;
        out.args = std::move(normalized);
        exprs_.push_back(std::move(out));
        return static_cast<int>(exprs_.size() - 1);
    }

    bool eval(int exprId, const std::vector<uint8_t>& assignment) const {
        const Expr& e = exprs_.at(static_cast<size_t>(exprId));
        switch (e.kind) {
            case Kind::Const:
                return e.constValue;
            case Kind::Var:
                return assignment.at(e.var) != 0U;
            case Kind::Not:
                return !eval(e.args.front(), assignment);
            case Kind::And:
                for (int arg : e.args) {
                    if (!eval(arg, assignment)) {
                        return false;
                    }
                }
                return true;
            case Kind::Or:
                for (int arg : e.args) {
                    if (eval(arg, assignment)) {
                        return true;
                    }
                }
                return false;
        }
        return false;
    }

    std::string toString(int exprId) const {
        const Expr& e = exprs_.at(static_cast<size_t>(exprId));
        switch (e.kind) {
            case Kind::Const:
                return e.constValue ? "true" : "false";
            case Kind::Var: {
                std::ostringstream oss;
                oss << "v" << e.var;
                return oss.str();
            }
            case Kind::Not:
                return "(!" + toString(e.args.front()) + ")";
            case Kind::And:
                return join(e.args, " & ");
            case Kind::Or:
                return join(e.args, " | ");
        }
        return "<unknown>";
    }

    const Expr& getExpr(int exprId) const {
        return exprs_.at(static_cast<size_t>(exprId));
    }

    int trueId() const {
        return trueId_;
    }

    int falseId() const {
        return falseId_;
    }

private:
    int addConst(bool value) {
        Expr e;
        e.kind = Kind::Const;
        e.constValue = value;
        exprs_.push_back(std::move(e));
        return static_cast<int>(exprs_.size() - 1);
    }

    std::string join(const std::vector<int>& args, const char* sep) const {
        std::ostringstream oss;
        oss << "(";
        for (size_t i = 0; i < args.size(); ++i) {
            if (i != 0) {
                oss << sep;
            }
            oss << toString(args[i]);
        }
        oss << ")";
        return oss.str();
    }

    std::vector<Expr> exprs_;
    int trueId_ = -1;
    int falseId_ = -1;
};

class GraphFormulaBuilder {
public:
    explicit GraphFormulaBuilder(const DerivationGraph& graph, ExprArena& arena)
            : graph_(graph), arena_(arena) {
        randomVars_.push_back(RandomVarInfo{});  // index 0 unused
    }

    int buildNodeFormula(const NodePtr& node) {
        auto memoIt = nodeFormula_.find(node);
        if (memoIt != nodeFormula_.end()) {
            return memoIt->second;
        }
        if (!nodeStack_.insert(node).second) {
            // Least-model style cycle cut: repeated node on the current recursion path
            // contributes no support for this derivation branch.
            return arena_.makeConst(false);
        }

        int formula = arena_.makeConst(false);
        if (node->isFact) {
            const long double p = node->getProbability();
            if (isZero(p)) {
                formula = arena_.makeConst(false);
            } else if (isOne(p)) {
                formula = arena_.makeConst(true);
            } else {
                formula = arena_.makeVar(ensureFactVar(node, p));
            }
        } else {
            std::vector<int> disj;
            disj.reserve(node->getIncomingEdges().size());
            for (const auto& edge : node->getIncomingEdges()) {
                disj.push_back(buildEdgeFormula(edge));
            }
            formula = arena_.makeOr(disj);
        }

        nodeStack_.erase(node);
        nodeFormula_[node] = formula;
        return formula;
    }

    int buildEdgeFormula(const EdgePtr& edge) {
        auto memoIt = edgeFormula_.find(edge);
        if (memoIt != edgeFormula_.end()) {
            return memoIt->second;
        }
        if (!edgeStack_.insert(edge).second) {
            // Same cycle cut for repeated edges on current recursion path.
            return arena_.makeConst(false);
        }

        int formula = arena_.makeConst(false);
        const long double p = edge->getProbability();
        if (isZero(p)) {
            formula = arena_.makeConst(false);
        } else {
            std::vector<int> conj;
            if (!isOne(p)) {
                conj.push_back(arena_.makeVar(ensureEdgeVar(edge, p)));
            }
            const auto& inputs = edge->getInputs();
            const auto& negs = edge->getBodyNegations();
            for (size_t i = 0; i < inputs.size(); ++i) {
                int lit = buildNodeFormula(inputs[i]);
                if (i < negs.size() && negs[i]) {
                    lit = arena_.makeNot(lit);
                }
                conj.push_back(lit);
            }
            formula = arena_.makeAnd(conj);
        }

        edgeStack_.erase(edge);
        edgeFormula_[edge] = formula;
        return formula;
    }

    const std::vector<RandomVarInfo>& randomVars() const {
        return randomVars_;
    }

    uint32_t getFactVarId(const NodePtr& node) const {
        auto it = factVar_.find(node);
        return it == factVar_.end() ? 0 : it->second;
    }

    uint32_t getEdgeVarId(const EdgePtr& edge) const {
        auto it = edgeVar_.find(edge);
        return it == edgeVar_.end() ? 0 : it->second;
    }

private:
    uint32_t ensureFactVar(const NodePtr& node, long double p) {
        auto it = factVar_.find(node);
        if (it != factVar_.end()) {
            return it->second;
        }
        const uint32_t id = addRandomVar("fact:" + node->toString(), p);
        factVar_[node] = id;
        return id;
    }

    uint32_t ensureEdgeVar(const EdgePtr& edge, long double p) {
        auto it = edgeVar_.find(edge);
        if (it != edgeVar_.end()) {
            return it->second;
        }
        const uint32_t id = addRandomVar("edge:" + edge->toString(), p);
        edgeVar_[edge] = id;
        return id;
    }

    uint32_t addRandomVar(const std::string& label, long double p) {
        if (p < -kProbEps || p > 1.0L + kProbEps) {
            throw std::runtime_error("Probability out of range for random variable: " + label);
        }
        const uint32_t id = static_cast<uint32_t>(randomVars_.size());
        randomVars_.push_back(RandomVarInfo{id, std::min(1.0L, std::max(0.0L, p)), label});
        return id;
    }

    const DerivationGraph& graph_;
    ExprArena& arena_;

    std::vector<RandomVarInfo> randomVars_;
    std::unordered_map<NodePtr, uint32_t> factVar_;
    std::unordered_map<EdgePtr, uint32_t> edgeVar_;
    std::unordered_map<NodePtr, int> nodeFormula_;
    std::unordered_map<EdgePtr, int> edgeFormula_;
    std::unordered_set<NodePtr> nodeStack_;
    std::unordered_set<EdgePtr> edgeStack_;
};

class GraphSemanticsEvaluator {
public:
    explicit GraphSemanticsEvaluator(const GraphFormulaBuilder& builder) : builder_(builder) {}

    bool evalNode(const NodePtr& node, const std::vector<uint8_t>& assignment) const {
        std::unordered_map<NodePtr, bool> nodeMemo;
        std::unordered_map<EdgePtr, bool> edgeMemo;
        std::unordered_set<NodePtr> nodeStack;
        std::unordered_set<EdgePtr> edgeStack;
        return evalNodeRec(node, assignment, nodeMemo, edgeMemo, nodeStack, edgeStack);
    }

private:
    bool evalNodeRec(const NodePtr& node, const std::vector<uint8_t>& assignment,
            std::unordered_map<NodePtr, bool>& nodeMemo, std::unordered_map<EdgePtr, bool>& edgeMemo,
            std::unordered_set<NodePtr>& nodeStack, std::unordered_set<EdgePtr>& edgeStack) const {
        auto itMemo = nodeMemo.find(node);
        if (itMemo != nodeMemo.end()) {
            return itMemo->second;
        }
        if (!nodeStack.insert(node).second) {
            // Mirror formula extraction semantics: cycles on current path evaluate to false.
            return false;
        }

        bool value = false;
        if (node->isFact) {
            const long double p = node->getProbability();
            if (isZero(p)) {
                value = false;
            } else if (isOne(p)) {
                value = true;
            } else {
                const uint32_t varId = builder_.getFactVarId(node);
                if (varId == 0) {
                    throw std::runtime_error("Missing random variable mapping for fact: " + node->toString());
                }
                value = assignment.at(varId) != 0U;
            }
        } else {
            value = false;
            for (const auto& edge : node->getIncomingEdges()) {
                if (evalEdgeRec(edge, assignment, nodeMemo, edgeMemo, nodeStack, edgeStack)) {
                    value = true;
                    break;
                }
            }
        }

        nodeStack.erase(node);
        nodeMemo[node] = value;
        return value;
    }

    bool evalEdgeRec(const EdgePtr& edge, const std::vector<uint8_t>& assignment,
            std::unordered_map<NodePtr, bool>& nodeMemo, std::unordered_map<EdgePtr, bool>& edgeMemo,
            std::unordered_set<NodePtr>& nodeStack, std::unordered_set<EdgePtr>& edgeStack) const {
        auto itMemo = edgeMemo.find(edge);
        if (itMemo != edgeMemo.end()) {
            return itMemo->second;
        }
        if (!edgeStack.insert(edge).second) {
            return false;
        }

        bool value = true;
        const long double p = edge->getProbability();
        if (isZero(p)) {
            value = false;
        } else if (!isOne(p)) {
            const uint32_t varId = builder_.getEdgeVarId(edge);
            if (varId == 0) {
                throw std::runtime_error("Missing random variable mapping for edge: " + edge->toString());
            }
            value = assignment.at(varId) != 0U;
        }

        if (value) {
            const auto& inputs = edge->getInputs();
            const auto& negs = edge->getBodyNegations();
            for (size_t i = 0; i < inputs.size(); ++i) {
                bool lit = evalNodeRec(inputs[i], assignment, nodeMemo, edgeMemo, nodeStack, edgeStack);
                if (i < negs.size() && negs[i]) {
                    lit = !lit;
                }
                if (!lit) {
                    value = false;
                    break;
                }
            }
        }

        edgeStack.erase(edge);
        edgeMemo[edge] = value;
        return value;
    }

    const GraphFormulaBuilder& builder_;
};

template <typename PredicateT>
long double weightedEnumerate(const std::vector<RandomVarInfo>& randomVars, PredicateT&& predicate) {
    if (randomVars.size() <= 1U) {
        return predicate(std::vector<uint8_t>{}) ? 1.0L : 0.0L;
    }

    std::vector<uint8_t> assignment(randomVars.size(), 0);
    long double result = 0.0L;
    std::function<void(uint32_t, long double)> dfs = [&](uint32_t varId, long double weight) {
        if (varId >= randomVars.size()) {
            if (predicate(assignment)) {
                result += weight;
            }
            return;
        }
        const long double p = randomVars[varId].probTrue;
        assignment[varId] = 0;
        dfs(varId + 1U, weight * (1.0L - p));
        assignment[varId] = 1;
        dfs(varId + 1U, weight * p);
    };
    dfs(1U, 1.0L);
    return result;
}

struct CnfBuildResult {
    approxmc_demo::WeightedCNFInput weighted;
    bool unsat = false;
};

class TseitinCnfEncoder {
public:
    TseitinCnfEncoder(const ExprArena& arena, const std::vector<RandomVarInfo>& randomVars)
            : arena_(arena), randomVars_(randomVars) {
        const uint32_t baseVars = randomVars_.empty() ? 0U : static_cast<uint32_t>(randomVars_.size() - 1U);
        nextVar_ = baseVars + 1U;
    }

    CnfBuildResult encode(int rootExpr) {
        const int rootLit = encodeExpr(rootExpr);
        if (rootLit == kFalseLit) {
            unsat_ = true;
        } else if (rootLit != kTrueLit) {
            addClause({rootLit});
        }

        CnfBuildResult out;
        out.unsat = unsat_;
        out.weighted.numVars = nextVar_ > 0 ? (nextVar_ - 1U) : 0U;
        out.weighted.clauses = clauses_;
        out.weighted.multiplier = 1.0L;

        const uint32_t randomCount = randomVars_.empty() ? 0U : static_cast<uint32_t>(randomVars_.size() - 1U);
        out.weighted.samplingSet.reserve(randomCount);
        for (uint32_t v = 1; v <= randomCount; ++v) {
            out.weighted.samplingSet.push_back(v);
            const long double p = randomVars_[v].probTrue;
            out.weighted.litWeights[static_cast<int>(v)] = p;
            out.weighted.litWeights[-static_cast<int>(v)] = 1.0L - p;
        }
        return out;
    }

private:
    int encodeExpr(int exprId) {
        auto it = exprLitMemo_.find(exprId);
        if (it != exprLitMemo_.end()) {
            return it->second;
        }

        const auto& expr = arena_.getExpr(exprId);
        int lit = kFalseLit;
        switch (expr.kind) {
            case ExprArena::Kind::Const:
                lit = expr.constValue ? kTrueLit : kFalseLit;
                break;
            case ExprArena::Kind::Var:
                lit = static_cast<int>(expr.var);
                break;
            case ExprArena::Kind::Not:
                lit = negateLit(encodeExpr(expr.args.front()));
                break;
            case ExprArena::Kind::And:
                lit = encodeAnd(expr.args);
                break;
            case ExprArena::Kind::Or:
                lit = encodeOr(expr.args);
                break;
        }
        exprLitMemo_[exprId] = lit;
        return lit;
    }

    int encodeAnd(const std::vector<int>& args) {
        std::vector<int> lits;
        lits.reserve(args.size());
        for (int arg : args) {
            const int lit = encodeExpr(arg);
            if (lit == kFalseLit) {
                return kFalseLit;
            }
            if (lit == kTrueLit) {
                continue;
            }
            lits.push_back(lit);
        }
        if (lits.empty()) {
            return kTrueLit;
        }
        if (lits.size() == 1) {
            return lits.front();
        }

        const int z = static_cast<int>(newAuxVar());
        for (int li : lits) {
            addClause({-z, li});
        }
        std::vector<int> back;
        back.reserve(lits.size() + 1U);
        back.push_back(z);
        for (int li : lits) {
            back.push_back(-li);
        }
        addClause(back);
        return z;
    }

    int encodeOr(const std::vector<int>& args) {
        std::vector<int> lits;
        lits.reserve(args.size());
        for (int arg : args) {
            const int lit = encodeExpr(arg);
            if (lit == kTrueLit) {
                return kTrueLit;
            }
            if (lit == kFalseLit) {
                continue;
            }
            lits.push_back(lit);
        }
        if (lits.empty()) {
            return kFalseLit;
        }
        if (lits.size() == 1) {
            return lits.front();
        }

        const int z = static_cast<int>(newAuxVar());
        for (int li : lits) {
            addClause({-li, z});
        }
        std::vector<int> back;
        back.reserve(lits.size() + 1U);
        back.push_back(-z);
        for (int li : lits) {
            back.push_back(li);
        }
        addClause(back);
        return z;
    }

    uint32_t newAuxVar() {
        return nextVar_++;
    }

    void addClause(const std::vector<int>& rawClause) {
        if (unsat_) {
            return;
        }
        std::vector<int> clause;
        clause.reserve(rawClause.size());
        std::unordered_set<int> seen;
        for (int lit : rawClause) {
            if (lit == kTrueLit) {
                return;  // tautology
            }
            if (lit == kFalseLit) {
                continue;
            }
            if (seen.count(-lit)) {
                return;  // tautology
            }
            if (seen.insert(lit).second) {
                clause.push_back(lit);
            }
        }
        if (clause.empty()) {
            unsat_ = true;
        }
        clauses_.push_back(std::move(clause));
    }

    const ExprArena& arena_;
    const std::vector<RandomVarInfo>& randomVars_;
    uint32_t nextVar_ = 1;
    bool unsat_ = false;
    std::vector<std::vector<int>> clauses_;
    std::unordered_map<int, int> exprLitMemo_;
};

std::vector<NodePtr> getQueryNodes(const DerivationGraph& graph) {
    std::vector<NodePtr> queries;
    for (const auto& node : graph.getNodes()) {
        if (node && node->isQueryNode()) {
            queries.push_back(node);
        }
    }
    std::sort(queries.begin(), queries.end(), [](const NodePtr& a, const NodePtr& b) {
        return a->getTuple() < b->getTuple();
    });
    return queries;
}

DerivationGraph buildToyGraph() {
    DerivationGraph graph;

    auto factA = graph.createNode(UntypedTuple{"a", {1}}, 0.7);
    auto factB = graph.createNode(UntypedTuple{"b", {1}}, 0.4);
    auto factI1 = graph.createNode(UntypedTuple{"i1", {1}}, 0.3);
    auto factI2 = graph.createNode(UntypedTuple{"i2", {1}}, 0.6);
    factA->isFact = true;
    factB->isFact = true;
    factI1->isFact = true;
    factI2->isFact = true;

    auto q = graph.createNode(UntypedTuple{"q", {1}}, 1.0);
    auto r = graph.createNode(UntypedTuple{"r", {1}}, 1.0);
    auto s = graph.createNode(UntypedTuple{"s", {1}}, 1.0);
    auto ca = graph.createNode(UntypedTuple{"ca", {1}}, 1.0);
    auto cb = graph.createNode(UntypedTuple{"cb", {1}}, 1.0);
    q->setQuery();
    r->setQuery();
    s->setQuery();
    ca->setQuery();
    cb->setQuery();

    auto e1 = graph.createHyperedge({factA}, q, nullptr, {false}, RuleApplication{1, {}});
    auto e2 = graph.createHyperedge({factB}, q, nullptr, {false}, RuleApplication{2, {}});
    auto e3 = graph.createHyperedge({q, factB}, r, nullptr, {false, true}, RuleApplication{3, {}});
    auto e4 = graph.createHyperedge({s}, s, nullptr, {false}, RuleApplication{4, {}});
    auto e5 = graph.createHyperedge({factA}, s, nullptr, {false}, RuleApplication{5, {}});
    auto e6 = graph.createHyperedge({cb}, ca, nullptr, {false}, RuleApplication{6, {}});
    auto e7 = graph.createHyperedge({ca}, cb, nullptr, {false}, RuleApplication{7, {}});
    auto e8 = graph.createHyperedge({factI1}, ca, nullptr, {false}, RuleApplication{8, {}});
    auto e9 = graph.createHyperedge({factI2}, cb, nullptr, {false}, RuleApplication{9, {}});
    if (!e1 || !e2 || !e3 || !e4 || !e5 || !e6 || !e7 || !e8 || !e9) {
        throw std::runtime_error("Failed to create toy graph hyperedges");
    }

    e1->setProbability(1.0);
    e2->setProbability(0.5);
    e3->setProbability(1.0);
    e4->setProbability(1.0);
    e5->setProbability(1.0);
    e6->setProbability(1.0);
    e7->setProbability(1.0);
    e8->setProbability(1.0);
    e9->setProbability(1.0);

    return graph;
}

long double expectedProbability(const NodePtr& node) {
    const auto& rel = node->getTuple().relation_name;
    if (rel == "q") {
        // q := a OR (x_edge2 AND b)
        return 0.76L;
    }
    if (rel == "r") {
        // r := q AND (!b), which simplifies to a AND (!b) in this toy graph
        return 0.42L;
    }
    if (rel == "s") {
        // s := s OR a ; with cycle-cut least-model unfolding this reduces to a
        return 0.7L;
    }
    if (rel == "ca" || rel == "cb") {
        // ca := cb OR i1, cb := ca OR i2
        // Least fixed point is ca = cb = i1 OR i2.
        return 0.72L;
    }
    return -1.0L;
}

void writeApproxmcCnf(const approxmc_demo::UnweightedCNFResult& cnf, const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open CNF output file: " + path);
    }
    out << "p cnf " << cnf.numVars << " " << cnf.clauses.size() << "\n";
    out << "c p show";
    for (uint32_t v : cnf.samplingSet) {
        out << " " << v;
    }
    out << " 0\n";
    for (const auto& clause : cnf.clauses) {
        if (clause.empty()) {
            out << "0\n";
            continue;
        }
        for (int lit : clause) {
            out << lit << " ";
        }
        out << "0\n";
    }
}

struct ApproxmcRunResult {
    bool ok = false;
    long double unweightedEstimate = 0.0L;
    std::string rawOutput;
    std::string message;
};

ApproxmcRunResult runApproxmc(
        const std::string& approxmcBin, const std::string& cnfPath, const Options& opt) {
    ApproxmcRunResult res;
    std::ostringstream cmd;
    cmd << approxmcBin
        << " --verb 0"
        << " --seed " << opt.seed
        << " --epsilon " << std::setprecision(17) << opt.epsilon
        << " --delta " << std::setprecision(17) << opt.delta
        << " " << cnfPath
        << " 2>&1";

    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        res.message = "Failed to start approxmc process";
        return res;
    }

    char buffer[4096];
    std::string output;
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output.append(buffer);
    }
    const int rc = pclose(pipe);
    res.rawOutput = output;

    if (output.find("PARSE ERROR") != std::string::npos) {
        res.message = "ApproxMC parse error";
        return res;
    }

    std::istringstream iss(output);
    std::string line;
    bool foundCount = false;
    while (std::getline(iss, line)) {
        constexpr const char* kMcPrefix = "s mc ";
        if (line.rfind(kMcPrefix, 0) == 0) {
            std::string num = line.substr(std::strlen(kMcPrefix));
            while (!num.empty() && std::isspace(static_cast<unsigned char>(num.back()))) {
                num.pop_back();
            }
            if (num.empty()) {
                continue;
            }
            try {
                res.unweightedEstimate = std::stold(num);
                foundCount = true;
            } catch (const std::exception&) {
                res.message = "Failed to parse 's mc' value: " + num;
                return res;
            }
        }
    }
    if (!foundCount) {
        res.message = "ApproxMC output did not contain 's mc <count>'";
        return res;
    }
    if (rc != 0) {
        res.message = "ApproxMC exited with code " + std::to_string(rc);
        return res;
    }
    res.ok = true;
    return res;
}

std::string sanitizeTuple(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        if (std::isalnum(static_cast<unsigned char>(c))) {
            out.push_back(c);
        } else {
            out.push_back('_');
        }
    }
    return out;
}

long double applyMultiplier(const approxmc_demo::UnweightedCNFResult& converted, long double unweightedEstimate) {
    const long double denom = std::ldexp(1.0L, static_cast<int>(converted.divideExp));
    return (unweightedEstimate * converted.multiplier) / denom;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options opt = parseArgs(argc, argv);
        if (!fileExists(opt.approxmcBin)) {
            std::cerr << "ApproxMC binary not found: " << opt.approxmcBin << "\n";
            std::cerr << "Set APPROXMC_BIN or pass --approxmc-bin.\n";
            return 1;
        }

        const DerivationGraph graph = buildToyGraph();
        const auto queries = getQueryNodes(graph);
        if (queries.empty()) {
            std::cerr << "No query nodes found in toy graph.\n";
            return 1;
        }

        std::cout << "=== DerivationGraph -> Formula -> CNF -> Weighted AMC Demo ===\n";
        std::cout << "approxmc_bin=" << opt.approxmcBin << "\n";
        std::cout << "epsilon=" << opt.epsilon << " delta=" << opt.delta << " seed=" << opt.seed << "\n";
        std::cout << "nodes=" << graph.getNodes().size() << " edges=" << graph.getEdges().size()
                  << " queries=" << queries.size() << "\n\n";

        bool allGood = true;
        for (const auto& query : queries) {
            // Build query-local formula/mapping to avoid cross-query cache artifacts on cycles.
            ExprArena arena;
            GraphFormulaBuilder builder(graph, arena);
            GraphSemanticsEvaluator evaluator(builder);

            const int queryFormula = builder.buildNodeFormula(query);
            const auto& randomVars = builder.randomVars();

            // Exact baseline by weighted enumeration over random variables.
            const long double viaFormula = weightedEnumerate(randomVars, [&](const std::vector<uint8_t>& assignment) {
                return arena.eval(queryFormula, assignment);
            });
            const long double viaGraph = weightedEnumerate(randomVars, [&](const std::vector<uint8_t>& assignment) {
                return evaluator.evalNode(query, assignment);
            });
            const long double expected = expectedProbability(query);

            // AMC path: formula -> weighted CNF -> unweighted CNF -> approxmc
            TseitinCnfEncoder encoder(arena, randomVars);
            CnfBuildResult weightedCnf = encoder.encode(queryFormula);

            approxmc_demo::ConversionConfig cfg;
            cfg.precision = 12;
            cfg.preprocess = true;
            cfg.verbosity = 0;
            cfg.tiltMax = 0.0L;
            cfg.failOnTilt = false;
            const auto converted = approxmc_demo::convertWeightedToUnweighted(std::move(weightedCnf.weighted), cfg);
            if (!converted.report.valid) {
                std::cerr << "Conversion invalid for query " << query->toString()
                          << ": " << converted.report.message << "\n";
                return 1;
            }

            long double weightedByApproxmc = 0.0L;
            long double unweightedByApproxmc = 0.0L;
            std::string approxDiag;
            if (converted.report.unsat || weightedCnf.unsat) {
                weightedByApproxmc = 0.0L;
                unweightedByApproxmc = 0.0L;
                approxDiag = "unsat shortcut";
            } else {
                const std::string cnfPath =
                        "/tmp/souffle_amc_demo_" + sanitizeTuple(query->toString()) + ".cnf";
                writeApproxmcCnf(converted, cnfPath);
                const auto approxRes = runApproxmc(opt.approxmcBin, cnfPath, opt);
                std::remove(cnfPath.c_str());
                if (!approxRes.ok) {
                    std::cerr << "ApproxMC failed for query " << query->toString()
                              << ": " << approxRes.message << "\n";
                    std::cerr << approxRes.rawOutput << "\n";
                    return 1;
                }
                unweightedByApproxmc = approxRes.unweightedEstimate;
                weightedByApproxmc = applyMultiplier(converted, unweightedByApproxmc);
                std::ostringstream oss;
                oss << "unweighted=" << unweightedByApproxmc
                    << " multiplier=" << converted.multiplier
                    << " divideExp=" << converted.divideExp;
                approxDiag = oss.str();
            }

            std::cout << "[query] " << query->toString() << "\n";
            std::cout << "  formula                  : " << arena.toString(queryFormula) << "\n";
            std::cout << "  P(formula exact enum)    : " << std::setprecision(10) << viaFormula << "\n";
            std::cout << "  P(graph semantics enum)  : " << std::setprecision(10) << viaGraph << "\n";
            std::cout << "  P(weighted AMC approxmc) : " << std::setprecision(10) << weightedByApproxmc << "\n";
            std::cout << "  approxmc detail          : " << approxDiag << "\n";

            if (expected >= 0.0L) {
                std::cout << "  P(manual expected)       : " << std::setprecision(10) << expected << "\n";
                const long double diffExpected = std::fabs(weightedByApproxmc - expected);
                std::cout << "  |amc-expected|           : " << std::setprecision(3) << std::scientific
                          << diffExpected << std::defaultfloat << "\n";
            }
            const long double diffExact = std::fabs(weightedByApproxmc - viaFormula);
            const long double approxTolerance =
                    std::max(1e-9L, static_cast<long double>(opt.epsilon) * std::max(std::fabs(viaFormula), 1e-12L));
            std::cout << "  |amc-exact|              : " << std::setprecision(3) << std::scientific
                      << diffExact << std::defaultfloat << "\n";
            std::cout << "  approx tolerance         : " << std::setprecision(3) << std::scientific
                      << approxTolerance << std::defaultfloat << "\n";
            const long double diffPipeline = std::fabs(viaFormula - viaGraph);
            std::cout << "  |formula-graph|          : " << std::setprecision(3) << std::scientific
                      << diffPipeline << std::defaultfloat << "\n\n";

            if (diffExact > approxTolerance || diffPipeline > 1e-12L) {
                allGood = false;
            }
            std::cout << "  random vars for query    : " << (randomVars.size() - 1U) << "\n";
            for (size_t i = 1; i < randomVars.size(); ++i) {
                std::cout << "    v" << randomVars[i].id << "  p=" << std::setprecision(6)
                          << randomVars[i].probTrue << "  " << randomVars[i].label << "\n";
            }
            std::cout << "\n";
        }

        if (!allGood) {
            std::cerr << "\nAMC pipeline check failed: mismatch beyond tolerance.\n";
            return 1;
        }

        std::cout << "\nAMC pipeline check passed.\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }
}
