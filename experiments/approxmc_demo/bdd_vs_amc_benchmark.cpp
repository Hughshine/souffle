#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/ForwardCompilation.h"
#include "souffle/problog/formula/CuddManager.h"

#include "weighted_conversion.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <sys/resource.h>

namespace {

using Clock = std::chrono::steady_clock;

constexpr long double kProbEps = 1e-15L;
constexpr int kTrueLit = std::numeric_limits<int>::max();
constexpr int kFalseLit = std::numeric_limits<int>::min();

bool isZero(long double x) {
    return std::fabs(x) <= kProbEps;
}

bool isOne(long double x) {
    return std::fabs(x - 1.0L) <= kProbEps;
}

long peakRssKb() {
    struct rusage ru {};
    getrusage(RUSAGE_SELF, &ru);
    return static_cast<long>(ru.ru_maxrss);
}

double elapsedSec(const Clock::time_point t0) {
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

enum class BackendMode {
    Bdd,
    Amc,
    Both,
};

struct Options {
    BackendMode backend = BackendMode::Both;
    uint32_t nvars = 100;
    uint32_t seed = 1;
    double epsilon = 0.1;
    double delta = 0.05;
    std::string approxmcBin;
    bool printHelp = false;
};

std::string defaultApproxmcBin() {
    const char* env = std::getenv("APPROXMC_BIN");
    if (env && *env) {
        return env;
    }
    return "/tmp/approxmc-bin/approxmc";
}

BackendMode parseBackend(const std::string& s) {
    if (s == "bdd") return BackendMode::Bdd;
    if (s == "amc") return BackendMode::Amc;
    if (s == "both") return BackendMode::Both;
    throw std::runtime_error("Invalid backend: " + s + " (expected bdd|amc|both)");
}

Options parseArgs(int argc, char** argv) {
    Options opt;
    opt.approxmcBin = defaultApproxmcBin();
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--backend") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value after --backend");
            opt.backend = parseBackend(argv[++i]);
        } else if (arg == "--nvars") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value after --nvars");
            opt.nvars = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--seed") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value after --seed");
            opt.seed = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--epsilon") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value after --epsilon");
            opt.epsilon = std::stod(argv[++i]);
        } else if (arg == "--delta") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value after --delta");
            opt.delta = std::stod(argv[++i]);
        } else if (arg == "--approxmc-bin") {
            if (i + 1 >= argc) throw std::runtime_error("Missing value after --approxmc-bin");
            opt.approxmcBin = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            opt.printHelp = true;
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }
    if (opt.nvars < 2) {
        throw std::runtime_error("--nvars must be >= 2");
    }
    if (opt.epsilon <= 0.0 || opt.delta <= 0.0 || opt.delta >= 1.0) {
        throw std::runtime_error("Require epsilon>0 and 0<delta<1");
    }
    return opt;
}

bool fileExists(const std::string& path) {
    std::ifstream in(path);
    return in.good();
}

struct SyntheticGraph {
    DerivationGraph graph;
    NodePtr query;
    uint32_t randomFacts = 0;
};

long double factProb(uint32_t i) {
    static_cast<void>(i);
    // Keep weights dyadic to avoid conversion blow-up in this benchmark.
    return 0.5L;
}

SyntheticGraph buildSyntheticGraph(uint32_t nvars, uint32_t seed) {
    SyntheticGraph out;
    auto& graph = out.graph;

    std::vector<NodePtr> facts;
    facts.reserve(nvars);
    for (uint32_t i = 0; i < nvars; ++i) {
        NodePtr f = graph.createNode(UntypedTuple{"x", {static_cast<souffle::RamDomain>(i + 1)}}, factProb(i));
        if (!f) throw std::runtime_error("Failed creating fact node");
        f->isFact = true;
        facts.push_back(f);
    }
    out.randomFacts = nvars;

    uint32_t ruleId = 1000 + seed;  // consume seed in structure id stream for determinism
    std::vector<NodePtr> current = facts;
    uint32_t depth = 0;
    while (current.size() > 1) {
        std::vector<NodePtr> next;
        next.reserve((current.size() + 1U) / 2U);
        for (size_t i = 0; i < current.size(); i += 2) {
            if (i + 1 >= current.size()) {
                next.push_back(current[i]);
                continue;
            }
            NodePtr n = graph.createNode(UntypedTuple{"t", {static_cast<souffle::RamDomain>(depth),
                                                            static_cast<souffle::RamDomain>(i / 2 + 1)}},
                    1.0);
            if (!n) throw std::runtime_error("Failed creating internal node");
            const NodePtr a = current[i];
            const NodePtr b = current[i + 1];
            if ((depth % 2U) == 0U) {
                // AND layer
                auto e = graph.createHyperedge(
                        {a, b}, n, nullptr, {false, false},
                        RuleApplication{static_cast<souffle::RamDomain>(ruleId++), {}});
                if (!e) throw std::runtime_error("Failed creating AND edge");
                e->setProbability(1.0);
            } else {
                // OR layer
                auto e1 = graph.createHyperedge({a}, n, nullptr, {false},
                        RuleApplication{static_cast<souffle::RamDomain>(ruleId++), {}});
                auto e2 = graph.createHyperedge({b}, n, nullptr, {false},
                        RuleApplication{static_cast<souffle::RamDomain>(ruleId++), {}});
                if (!e1 || !e2) throw std::runtime_error("Failed creating OR edges");
                e1->setProbability(1.0);
                e2->setProbability(1.0);
            }
            next.push_back(n);
        }
        current = std::move(next);
        ++depth;
    }

    NodePtr q = graph.createNode(UntypedTuple{"qbig", {1}}, 1.0);
    if (!q) throw std::runtime_error("Failed creating query node");
    q->setQuery();
    auto e = graph.createHyperedge({current.front()}, q, nullptr, {false},
            RuleApplication{static_cast<souffle::RamDomain>(ruleId++), {}});
    if (!e) throw std::runtime_error("Failed creating query edge");
    e->setProbability(1.0);

    out.query = q;
    return out;
}

struct BddResult {
    long double probability = 0.0L;
    double fcSec = 0.0;
    double evalSec = 0.0;
    size_t liveNodes = 0;
};

BddResult runBddWmc(DerivationGraph& graph, const NodePtr& query) {
    BddResult out;
    SubgraphView view(graph.getNodes(), graph.getEdges());
    WeightedBDDManager::InitConfig initCfg;
    initCfg.numVars = static_cast<unsigned int>(std::max<size_t>(128, graph.getNodes().size() * 2));
    initCfg.numSlots = 512;
    initCfg.cacheSize = 1u << 18;      // keep benchmark memory modest
    initCfg.maxMemory = 1UL << 30;     // 1 GiB ceiling
    WeightedBDDManager manager(initCfg);
    std::map<NodePtr, BddNodeRef> nodeFormulas;
    std::map<EdgePtr, BddNodeRef> edgeFormulas;

    const auto tFc = Clock::now();
    buildFormulasCyclewise(view, manager, nodeFormulas, edgeFormulas);
    out.fcSec = elapsedSec(tFc);
    out.liveNodes = manager.getLiveNodeCount();

    const auto it = nodeFormulas.find(query);
    if (it == nodeFormulas.end()) {
        throw std::runtime_error("BDD formula not built for query");
    }

    const auto tEval = Clock::now();
    out.probability = static_cast<long double>(manager.computeWeightedModelCount(it->second));
    out.evalSec = elapsedSec(tEval);
    return out;
}

struct RandomVarInfo {
    uint32_t id = 0;
    long double probTrue = 0.0;
    std::string label;
};

int negateLit(int lit) {
    if (lit == kTrueLit) return kFalseLit;
    if (lit == kFalseLit) return kTrueLit;
    return -lit;
}

class ExprArena {
public:
    enum class Kind { Const, Var, Not, And, Or };
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

    int makeConst(bool v) const {
        return v ? trueId_ : falseId_;
    }

    int makeVar(uint32_t var) {
        Expr e;
        e.kind = Kind::Var;
        e.var = var;
        exprs_.push_back(std::move(e));
        return static_cast<int>(exprs_.size() - 1);
    }

    int makeNot(int child) {
        if (child == trueId_) return falseId_;
        if (child == falseId_) return trueId_;
        const Expr& c = exprs_.at(static_cast<size_t>(child));
        if (c.kind == Kind::Not && !c.args.empty()) return c.args.front();
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
            if (t == falseId_) return falseId_;
            if (t == trueId_) continue;
            const Expr& e = exprs_.at(static_cast<size_t>(t));
            if (e.kind == Kind::And) {
                normalized.insert(normalized.end(), e.args.begin(), e.args.end());
            } else {
                normalized.push_back(t);
            }
        }
        if (normalized.empty()) return trueId_;
        if (normalized.size() == 1) return normalized.front();
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
            if (t == trueId_) return trueId_;
            if (t == falseId_) continue;
            const Expr& e = exprs_.at(static_cast<size_t>(t));
            if (e.kind == Kind::Or) {
                normalized.insert(normalized.end(), e.args.begin(), e.args.end());
            } else {
                normalized.push_back(t);
            }
        }
        if (normalized.empty()) return falseId_;
        if (normalized.size() == 1) return normalized.front();
        Expr out;
        out.kind = Kind::Or;
        out.args = std::move(normalized);
        exprs_.push_back(std::move(out));
        return static_cast<int>(exprs_.size() - 1);
    }

    const Expr& getExpr(int id) const {
        return exprs_.at(static_cast<size_t>(id));
    }

    size_t size() const {
        return exprs_.size();
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

    std::vector<Expr> exprs_;
    int trueId_ = -1;
    int falseId_ = -1;
};

class GraphFormulaBuilder {
public:
    GraphFormulaBuilder(const DerivationGraph& graph, ExprArena& arena) : graph_(graph), arena_(arena) {
        randomVars_.push_back(RandomVarInfo{});
    }

    int buildNodeFormula(const NodePtr& node) {
        auto memoIt = nodeFormula_.find(node);
        if (memoIt != nodeFormula_.end()) return memoIt->second;
        if (!nodeStack_.insert(node).second) return arena_.makeConst(false);

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
        if (memoIt != edgeFormula_.end()) return memoIt->second;
        if (!edgeStack_.insert(edge).second) return arena_.makeConst(false);

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
                if (i < negs.size() && negs[i]) lit = arena_.makeNot(lit);
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

private:
    uint32_t ensureFactVar(const NodePtr& node, long double p) {
        auto it = factVar_.find(node);
        if (it != factVar_.end()) return it->second;
        const uint32_t id = addRandomVar("fact:" + node->toString(), p);
        factVar_[node] = id;
        return id;
    }

    uint32_t ensureEdgeVar(const EdgePtr& edge, long double p) {
        auto it = edgeVar_.find(edge);
        if (it != edgeVar_.end()) return it->second;
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
        if (it != exprLitMemo_.end()) return it->second;

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
            if (lit == kFalseLit) return kFalseLit;
            if (lit == kTrueLit) continue;
            lits.push_back(lit);
        }
        if (lits.empty()) return kTrueLit;
        if (lits.size() == 1) return lits.front();

        const int z = static_cast<int>(newAuxVar());
        for (int li : lits) addClause({-z, li});
        std::vector<int> back;
        back.reserve(lits.size() + 1U);
        back.push_back(z);
        for (int li : lits) back.push_back(-li);
        addClause(back);
        return z;
    }

    int encodeOr(const std::vector<int>& args) {
        std::vector<int> lits;
        lits.reserve(args.size());
        for (int arg : args) {
            const int lit = encodeExpr(arg);
            if (lit == kTrueLit) return kTrueLit;
            if (lit == kFalseLit) continue;
            lits.push_back(lit);
        }
        if (lits.empty()) return kFalseLit;
        if (lits.size() == 1) return lits.front();

        const int z = static_cast<int>(newAuxVar());
        for (int li : lits) addClause({-li, z});
        std::vector<int> back;
        back.reserve(lits.size() + 1U);
        back.push_back(-z);
        for (int li : lits) back.push_back(li);
        addClause(back);
        return z;
    }

    uint32_t newAuxVar() {
        return nextVar_++;
    }

    void addClause(const std::vector<int>& rawClause) {
        if (unsat_) return;
        std::vector<int> clause;
        clause.reserve(rawClause.size());
        std::unordered_set<int> seen;
        for (int lit : rawClause) {
            if (lit == kTrueLit) return;
            if (lit == kFalseLit) continue;
            if (seen.count(-lit)) return;
            if (seen.insert(lit).second) clause.push_back(lit);
        }
        if (clause.empty()) unsat_ = true;
        clauses_.push_back(std::move(clause));
    }

    const ExprArena& arena_;
    const std::vector<RandomVarInfo>& randomVars_;
    uint32_t nextVar_ = 1;
    bool unsat_ = false;
    std::vector<std::vector<int>> clauses_;
    std::unordered_map<int, int> exprLitMemo_;
};

void writeApproxmcCnf(const approxmc_demo::UnweightedCNFResult& cnf, const std::string& path) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Failed to open CNF output: " + path);
    out << "p cnf " << cnf.numVars << " " << cnf.clauses.size() << "\n";
    out << "c p show";
    for (uint32_t v : cnf.samplingSet) out << " " << v;
    out << " 0\n";
    for (const auto& clause : cnf.clauses) {
        for (int lit : clause) out << lit << " ";
        out << "0\n";
    }
}

struct ApproxmcRunResult {
    bool ok = false;
    long double unweightedEstimate = 0.0L;
    std::string rawOutput;
    std::string message;
    double runtimeSec = 0.0;
};

ApproxmcRunResult runApproxmc(
        const std::string& approxmcBin, const std::string& cnfPath, double epsilon, double delta, uint32_t seed) {
    ApproxmcRunResult res;
    std::ostringstream cmd;
    cmd << approxmcBin
        << " --verb 0"
        << " --seed " << seed
        << " --epsilon " << std::setprecision(17) << epsilon
        << " --delta " << std::setprecision(17) << delta
        << " " << cnfPath
        << " 2>&1";

    const auto t0 = Clock::now();
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        res.message = "Failed to start approxmc process";
        return res;
    }

    char buf[4096];
    std::string output;
    while (std::fgets(buf, sizeof(buf), pipe) != nullptr) {
        output.append(buf);
    }
    const int rc = pclose(pipe);
    res.runtimeSec = elapsedSec(t0);
    res.rawOutput = output;

    if (output.find("PARSE ERROR") != std::string::npos) {
        res.message = "ApproxMC parse error";
        return res;
    }

    std::istringstream iss(output);
    std::string line;
    bool found = false;
    while (std::getline(iss, line)) {
        constexpr const char* kPrefix = "s mc ";
        if (line.rfind(kPrefix, 0) == 0) {
            std::string num = line.substr(std::strlen(kPrefix));
            while (!num.empty() && std::isspace(static_cast<unsigned char>(num.back()))) num.pop_back();
            if (num.empty()) continue;
            try {
                res.unweightedEstimate = std::stold(num);
                found = true;
            } catch (const std::exception&) {
                res.message = "Failed to parse s mc";
                return res;
            }
        }
    }
    if (!found) {
        res.message = "ApproxMC output missing s mc";
        return res;
    }
    if (rc != 0) {
        res.message = "ApproxMC exited non-zero: " + std::to_string(rc);
        return res;
    }
    res.ok = true;
    return res;
}

long double applyMultiplier(const approxmc_demo::UnweightedCNFResult& converted, long double unweightedEstimate) {
    const long double denom = std::ldexp(1.0L, static_cast<int>(converted.divideExp));
    return (unweightedEstimate * converted.multiplier) / denom;
}

std::string sanitizeFileSuffix(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (char c : in) {
        if (std::isalnum(static_cast<unsigned char>(c))) out.push_back(c);
        else out.push_back('_');
    }
    return out;
}

struct AmcResult {
    long double probability = 0.0L;
    uint32_t randomVars = 0;
    uint32_t weightedVars = 0;
    uint32_t weightedClauses = 0;
    uint32_t unweightedVars = 0;
    uint32_t unweightedClauses = 0;
    double extractSec = 0.0;
    double encodeSec = 0.0;
    double convertSec = 0.0;
    double approxmcSec = 0.0;
    long double unweightedEstimate = 0.0L;
};

AmcResult runAmc(const DerivationGraph& graph, const NodePtr& query, const Options& opt) {
    AmcResult out;

    const auto tExtract = Clock::now();
    ExprArena arena;
    GraphFormulaBuilder builder(graph, arena);
    const int formula = builder.buildNodeFormula(query);
    const auto& randomVars = builder.randomVars();
    out.randomVars = randomVars.empty() ? 0U : static_cast<uint32_t>(randomVars.size() - 1U);
    out.extractSec = elapsedSec(tExtract);

    const auto tEncode = Clock::now();
    TseitinCnfEncoder encoder(arena, randomVars);
    CnfBuildResult weighted = encoder.encode(formula);
    out.weightedVars = weighted.weighted.numVars;
    out.weightedClauses = static_cast<uint32_t>(weighted.weighted.clauses.size());
    out.encodeSec = elapsedSec(tEncode);

    const auto tConv = Clock::now();
    approxmc_demo::ConversionConfig cfg;
    cfg.precision = 4;
    cfg.preprocess = true;
    cfg.verbosity = 0;
    cfg.tiltMax = 0.0L;
    cfg.failOnTilt = false;
    auto converted = approxmc_demo::convertWeightedToUnweighted(std::move(weighted.weighted), cfg);
    out.convertSec = elapsedSec(tConv);
    if (!converted.report.valid) {
        throw std::runtime_error("Weighted->unweighted conversion failed: " + converted.report.message);
    }

    out.unweightedVars = converted.numVars;
    out.unweightedClauses = static_cast<uint32_t>(converted.clauses.size());
    if (converted.report.unsat || weighted.unsat) {
        out.probability = 0.0L;
        return out;
    }

    const std::string cnfPath = "/tmp/souffle_bdd_amc_" + sanitizeFileSuffix(query->toString()) + ".cnf";
    writeApproxmcCnf(converted, cnfPath);
    const auto approx = runApproxmc(opt.approxmcBin, cnfPath, opt.epsilon, opt.delta, opt.seed);
    std::remove(cnfPath.c_str());
    if (!approx.ok) {
        throw std::runtime_error("ApproxMC failed: " + approx.message + "\n" + approx.rawOutput);
    }

    out.approxmcSec = approx.runtimeSec;
    out.unweightedEstimate = approx.unweightedEstimate;
    out.probability = applyMultiplier(converted, out.unweightedEstimate);
    return out;
}

void printUsage(const char* argv0) {
    std::cout << "Usage: " << argv0
              << " [--backend bdd|amc|both] [--nvars 100] [--seed 1]"
                 " [--epsilon 0.1] [--delta 0.05] [--approxmc-bin /path/to/approxmc]\n";
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options opt = parseArgs(argc, argv);
        if (opt.printHelp) {
            printUsage(argv[0]);
            return 0;
        }

        if ((opt.backend == BackendMode::Amc || opt.backend == BackendMode::Both) &&
                !fileExists(opt.approxmcBin)) {
            throw std::runtime_error(
                    "ApproxMC binary not found: " + opt.approxmcBin +
                    " (set APPROXMC_BIN or pass --approxmc-bin)");
        }

        const auto tBuild = Clock::now();
        SyntheticGraph bench = buildSyntheticGraph(opt.nvars, opt.seed);
        const double buildSec = elapsedSec(tBuild);

        std::cout << std::setprecision(17);
        std::cout << "[graph] nodes=" << bench.graph.getNodes().size()
                  << " edges=" << bench.graph.getEdges().size()
                  << " random_facts=" << bench.randomFacts
                  << " query=" << bench.query->toString()
                  << " build_s=" << buildSec << "\n";

        long double bddProb = -1.0L;
        long double amcProb = -1.0L;

        if (opt.backend == BackendMode::Bdd || opt.backend == BackendMode::Both) {
            const auto t0 = Clock::now();
            BddResult bdd = runBddWmc(bench.graph, bench.query);
            const double total = elapsedSec(t0);
            bddProb = bdd.probability;
            std::cout << "[bdd] prob=" << bdd.probability
                      << " fc_s=" << bdd.fcSec
                      << " eval_s=" << bdd.evalSec
                      << " total_s=" << total
                      << " live_nodes=" << bdd.liveNodes
                      << " peak_rss_kb=" << peakRssKb() << "\n";
        }

        if (opt.backend == BackendMode::Amc || opt.backend == BackendMode::Both) {
            const auto t0 = Clock::now();
            AmcResult amc = runAmc(bench.graph, bench.query, opt);
            const double total = elapsedSec(t0);
            amcProb = amc.probability;
            std::cout << "[amc] prob=" << amc.probability
                      << " random_vars=" << amc.randomVars
                      << " weighted_cnf_vars=" << amc.weightedVars
                      << " weighted_cnf_clauses=" << amc.weightedClauses
                      << " unweighted_cnf_vars=" << amc.unweightedVars
                      << " unweighted_cnf_clauses=" << amc.unweightedClauses
                      << " extract_s=" << amc.extractSec
                      << " encode_s=" << amc.encodeSec
                      << " convert_s=" << amc.convertSec
                      << " approxmc_s=" << amc.approxmcSec
                      << " total_s=" << total
                      << " peak_rss_kb=" << peakRssKb()
                      << "\n";
        }

        if (opt.backend == BackendMode::Both && bddProb >= 0.0L && amcProb >= 0.0L) {
            const long double absDiff = std::fabs(bddProb - amcProb);
            const long double relDiff = absDiff / std::max(std::fabs(bddProb), 1e-15L);
            std::cout << "[compare] abs_diff=" << absDiff
                      << " rel_diff=" << relDiff
                      << " epsilon=" << opt.epsilon
                      << " delta=" << opt.delta
                      << "\n";
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }
}
