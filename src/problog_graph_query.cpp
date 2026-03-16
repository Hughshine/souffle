#include "souffle/problog/DerivationGraph.h"
#include "souffle/problog/ForwardCompilation.h"
#include "souffle/problog/GraphRewriter.h"
#include "souffle/problog/approx/WeightedConversion.h"
#include "souffle/problog/formula/CuddManager.h"
#include "souffle/CompiledOptions.h"
#ifdef SOUFFLE_STANDALONE_HAS_APPROXMC_LIB
#include <approxmc/approxmc.h>
#include <cryptominisat5/cryptominisat.h>
#endif
#ifdef SOUFFLE_STANDALONE_HAS_SDD
#include "souffle/problog/formula/SddManager.h"
#endif

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <limits>
#include <optional>
#include <queue>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

constexpr long double kProbEps = 1e-15L;
constexpr int kTrueLit = std::numeric_limits<int>::max();
constexpr int kFalseLit = std::numeric_limits<int>::min();

enum class Backend {
    Bdd,
    Sdd,
    Amc,
};

struct Options {
    std::string jsonPath;
    std::vector<std::string> querySpecs;
    std::vector<std::string> queryAllRelations;
    bool rewrite = false;
    bool fullGraph = false;
    bool listQueries = false;
    Backend backend = Backend::Bdd;
    souffle::FullEvaluator fullEvaluator = souffle::FullEvaluator::EXACT;
    std::string ddBackend = "bdd";
    souffle::ApproxBackend approxBackend = souffle::ApproxBackend::NONE;
    souffle::RewriteEngine rewriteEngine = souffle::RewriteEngine::OFF;
    souffle::RewriteSplitMode rewriteSplit = souffle::RewriteSplitMode::NAIVE;
    souffle::RewriteDetectMode rewriteDetect = souffle::RewriteDetectMode::DIRTY_FRONTIER;
    std::string approxmcBin;
    double epsilon = 0.1;
    double delta = 0.05;
    uint32_t seed = 1;
    uint32_t amcPrecision = 4;
    bool amcPreprocess = true;
    uint32_t amcVerb = 0;
    bool amcStreamOutput = false;
};

const char* backendChoices() {
#ifdef SOUFFLE_STANDALONE_HAS_SDD
    return "bdd|sdd|amc";
#else
    return "bdd|amc";
#endif
}

bool isZero(long double x) {
    return std::fabs(x) <= kProbEps;
}

bool isOne(long double x) {
    return std::fabs(x - 1.0L) <= kProbEps;
}

std::string defaultApproxmcBin() {
    const char* env = std::getenv("APPROXMC_BIN");
    if (env && *env) {
        return env;
    }
    return "/tmp/approxmc-bin/approxmc";
}

bool fileExists(const std::string& path) {
    std::ifstream in(path);
    return in.good();
}

[[noreturn]] void failUsage(const std::string& message, const char* argv0) {
    std::ostringstream oss;
    if (!message.empty()) {
        oss << message << "\n\n";
    }
    oss << "Usage:\n"
        << "  " << argv0
        << " --json <derivation.json> --query <Rel(a,b,...)> [--query ...]"
        << " [--backend " << backendChoices() << "] [--dd-backend " << souffle::ddBackendOptionSyntax()
        << "] [--full-evaluator " << souffle::fullEvaluatorOptionSyntax() << "]"
        << " [--rewrite|--rewrite-engine " << souffle::rewriteEngineOptionSyntax()
        << "] [--full-graph]\n"
        << "  " << argv0
        << " --json <derivation.json> --query-all <REL1/REL2/...>"
        << " [--backend " << backendChoices() << "] [--dd-backend " << souffle::ddBackendOptionSyntax()
        << "] [--full-evaluator " << souffle::fullEvaluatorOptionSyntax() << "]"
        << " [--rewrite|--rewrite-engine " << souffle::rewriteEngineOptionSyntax()
        << "] [--full-graph]\n"
        << "  " << argv0 << " --json <derivation.json> --list-queries\n\n"
        << "Options:\n"
        << "  --json <path>         Path to derivation JSON file\n"
        << "  --query <tuple>       Query tuple like path(1,3). Repeatable\n"
        << "  --query all <rels>    Query all tuples under relations in <rels> (separator: '/' or ',')\n"
        << "  --query-all <rels>    Same as above, e.g. KEY_SENSITIVE/KEY_IND\n"
        << "  --backend <" << backendChoices() << ">   Probability backend (default: bdd)\n"
        << "  --dd-backend <" << souffle::ddBackendOptionSyntax()
        << ">   Canonical exact DD backend selector\n"
        << "  --full-evaluator <" << souffle::fullEvaluatorOptionSyntax()
        << ">   Canonical evaluator selector\n"
        << "  --approx-backend <" << souffle::approxBackendOptionSyntax()
        << ">   Canonical approx backend selector\n"
        << "  --approxmc-bin <path> Path to ApproxMC-compatible CLI (for backend=amc)\n"
        << "  --epsilon <value>     AMC epsilon (> 0, default: 0.1)\n"
        << "  --delta <value>       AMC delta in (0,1) (default: 0.05)\n"
        << "  --seed <value>        AMC random seed (default: 1)\n"
        << "  --amc-precision <n>   Weight quantization precision (default: 4)\n"
        << "  --amc-verb <n>        Pass ApproxMC verbosity level through (default: 0)\n"
        << "  --amc-stream-output   Stream ApproxMC child output and AMC stage markers\n"
        << "  --amc-no-preprocess   Disable weighted->unweighted preprocessing\n"
        << "  --rewrite             Run optimize rewrite on the query subgraph before FC\n"
        << "  --rewrite-engine <" << souffle::rewriteEngineOptionSyntax()
        << ">   Canonical rewrite selector\n"
        << "  --rewrite-split <" << souffle::rewriteSplitOptionSyntax()
        << ">   Canonical rewrite split selector\n"
        << "  --rewrite-detect <" << souffle::rewriteDetectOptionSyntax()
        << ">   Canonical rewrite detect selector\n"
        << "  --full-graph          Compile entire graph (default: backward slice from query)\n"
        << "  --list-queries        Print all tuples present in graph and exit\n"
        << "  -h, --help            Show this message\n";
    throw std::runtime_error(oss.str());
}

std::string trim(const std::string& in) {
    std::size_t start = 0;
    while (start < in.size() && std::isspace(static_cast<unsigned char>(in[start]))) {
        ++start;
    }
    std::size_t end = in.size();
    while (end > start && std::isspace(static_cast<unsigned char>(in[end - 1]))) {
        --end;
    }
    return in.substr(start, end - start);
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return s;
}

Backend parseBackend(const std::string& raw) {
    const std::string val = toLower(trim(raw));
    if (val == "bdd") {
        return Backend::Bdd;
    }
    if (val == "amc") {
        return Backend::Amc;
    }
    if (val == "sdd") {
#ifdef SOUFFLE_STANDALONE_HAS_SDD
        return Backend::Sdd;
#else
        throw std::runtime_error("Unsupported backend 'sdd' (this build does not include SDD support)");
#endif
    }
    throw std::runtime_error("Unsupported backend '" + raw + "' (expected " + std::string(backendChoices()) + ")");
}

const char* backendName(Backend backend) {
    switch (backend) {
        case Backend::Bdd: return "bdd";
        case Backend::Sdd: return "sdd";
        case Backend::Amc: return "amc";
    }
    return "unknown";
}

std::vector<std::string> parseRelationList(const std::string& raw) {
    std::string normalized = raw;
    for (char& c : normalized) {
        if (c == '/' || c == ';') {
            c = ',';
        }
    }

    std::vector<std::string> rels;
    std::stringstream ss(normalized);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        tok = trim(tok);
        if (!tok.empty()) {
            rels.push_back(tok);
        }
    }
    if (rels.empty()) {
        throw std::runtime_error("Empty relation list in query-all: " + raw);
    }
    return rels;
}

void appendRelationList(std::vector<std::string>& out, const std::string& raw) {
    const auto rels = parseRelationList(raw);
    out.insert(out.end(), rels.begin(), rels.end());
}

UntypedTuple parseTuple(const std::string& spec) {
    const std::string s = trim(spec);
    if (s.empty()) {
        throw std::runtime_error("Empty tuple spec");
    }

    const auto lp = s.find('(');
    if (lp == std::string::npos) {
        return UntypedTuple{s, {}};
    }
    const auto rp = s.rfind(')');
    if (rp == std::string::npos || rp <= lp) {
        throw std::runtime_error("Bad tuple spec (missing ')'): " + s);
    }
    if (!trim(s.substr(rp + 1)).empty()) {
        throw std::runtime_error("Bad tuple spec (trailing content): " + s);
    }

    std::string relation = trim(s.substr(0, lp));
    if (relation.empty()) {
        throw std::runtime_error("Bad tuple spec (missing relation): " + s);
    }

    std::vector<souffle::RamDomain> fields;
    std::string inside = s.substr(lp + 1, rp - lp - 1);
    std::stringstream ss(inside);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        tok = trim(tok);
        if (tok.empty()) {
            continue;
        }
        long long value = std::stoll(tok);
        fields.push_back(static_cast<souffle::RamDomain>(value));
    }

    return UntypedTuple{std::move(relation), std::move(fields)};
}

Options parseArgs(int argc, char** argv) {
    Options opt;
    opt.approxmcBin = defaultApproxmcBin();
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--json") {
            if (i + 1 >= argc) {
                failUsage("Missing value after --json", argv[0]);
            }
            opt.jsonPath = argv[++i];
        } else if (arg == "--query") {
            if (i + 1 >= argc) {
                failUsage("Missing value after --query", argv[0]);
            }
            const std::string value = argv[++i];
            if (toLower(value) == "all") {
                if (i + 1 >= argc) {
                    failUsage("Missing relation list after '--query all'", argv[0]);
                }
                appendRelationList(opt.queryAllRelations, argv[++i]);
            } else {
                opt.querySpecs.push_back(value);
            }
        } else if (arg == "--query-all") {
            if (i + 1 >= argc) {
                failUsage("Missing value after --query-all", argv[0]);
            }
            appendRelationList(opt.queryAllRelations, argv[++i]);
        } else if (arg == "--backend") {
            if (i + 1 >= argc) {
                failUsage("Missing value after --backend", argv[0]);
            }
            opt.backend = parseBackend(argv[++i]);
            if (opt.backend == Backend::Amc) {
                opt.fullEvaluator = souffle::FullEvaluator::APPROX;
                opt.approxBackend = souffle::ApproxBackend::AMC;
            } else {
                opt.fullEvaluator = souffle::FullEvaluator::EXACT;
                opt.ddBackend = backendName(opt.backend);
            }
        } else if (arg == "--dd-backend") {
            if (i + 1 >= argc) {
                failUsage("Missing value after --dd-backend", argv[0]);
            }
            if (!souffle::parseDdBackendToken(argv[++i], opt.ddBackend)) {
                failUsage("Unsupported value for --dd-backend", argv[0]);
            }
            opt.fullEvaluator = souffle::FullEvaluator::EXACT;
        } else if (arg == "--full-evaluator") {
            if (i + 1 >= argc) {
                failUsage("Missing value after --full-evaluator", argv[0]);
            }
            if (!souffle::parseFullEvaluatorToken(argv[++i], opt.fullEvaluator)) {
                failUsage("Unsupported value for --full-evaluator", argv[0]);
            }
        } else if (arg == "--approx-backend") {
            if (i + 1 >= argc) {
                failUsage("Missing value after --approx-backend", argv[0]);
            }
            if (!souffle::parseApproxBackendToken(argv[++i], opt.approxBackend)) {
                failUsage("Unsupported value for --approx-backend", argv[0]);
            }
        } else if (arg == "--rewrite") {
            opt.rewrite = true;
            opt.rewriteEngine = souffle::RewriteEngine::LEGACY;
        } else if (arg == "--rewrite-engine") {
            if (i + 1 >= argc) {
                failUsage("Missing value after --rewrite-engine", argv[0]);
            }
            if (!souffle::parseRewriteEngineToken(argv[++i], opt.rewriteEngine)) {
                failUsage("Unsupported value for --rewrite-engine", argv[0]);
            }
            opt.rewrite = opt.rewriteEngine != souffle::RewriteEngine::OFF;
        } else if (arg == "--rewrite-split") {
            if (i + 1 >= argc) {
                failUsage("Missing value after --rewrite-split", argv[0]);
            }
            if (!souffle::parseRewriteSplitModeToken(argv[++i], opt.rewriteSplit)) {
                failUsage("Unsupported value for --rewrite-split", argv[0]);
            }
        } else if (arg == "--rewrite-detect") {
            if (i + 1 >= argc) {
                failUsage("Missing value after --rewrite-detect", argv[0]);
            }
            if (!souffle::parseRewriteDetectModeToken(argv[++i], opt.rewriteDetect)) {
                failUsage("Unsupported value for --rewrite-detect", argv[0]);
            }
        } else if (arg == "--full-graph") {
            opt.fullGraph = true;
        } else if (arg == "--list-queries") {
            opt.listQueries = true;
        } else if (arg == "--approxmc-bin") {
            if (i + 1 >= argc) {
                failUsage("Missing value after --approxmc-bin", argv[0]);
            }
            opt.approxmcBin = argv[++i];
        } else if (arg == "--epsilon") {
            if (i + 1 >= argc) {
                failUsage("Missing value after --epsilon", argv[0]);
            }
            opt.epsilon = std::stod(argv[++i]);
        } else if (arg == "--delta") {
            if (i + 1 >= argc) {
                failUsage("Missing value after --delta", argv[0]);
            }
            opt.delta = std::stod(argv[++i]);
        } else if (arg == "--seed") {
            if (i + 1 >= argc) {
                failUsage("Missing value after --seed", argv[0]);
            }
            opt.seed = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--amc-precision") {
            if (i + 1 >= argc) {
                failUsage("Missing value after --amc-precision", argv[0]);
            }
            opt.amcPrecision = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--amc-verb") {
            if (i + 1 >= argc) {
                failUsage("Missing value after --amc-verb", argv[0]);
            }
            opt.amcVerb = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--amc-stream-output") {
            opt.amcStreamOutput = true;
        } else if (arg == "--amc-no-preprocess") {
            opt.amcPreprocess = false;
        } else if (arg == "-h" || arg == "--help") {
            failUsage("", argv[0]);
        } else {
            failUsage("Unknown argument: " + arg, argv[0]);
        }
    }

    if (opt.jsonPath.empty()) {
        failUsage("--json is required", argv[0]);
    }
    if (!opt.listQueries && opt.querySpecs.empty() && opt.queryAllRelations.empty()) {
        failUsage("At least one query is required (use --query or --query-all)", argv[0]);
    }
    if (opt.epsilon <= 0.0 || opt.delta <= 0.0 || opt.delta >= 1.0) {
        failUsage("Require epsilon > 0 and 0 < delta < 1", argv[0]);
    }
    if (opt.amcPrecision == 0) {
        failUsage("--amc-precision must be >= 1", argv[0]);
    }

    if (opt.fullEvaluator == souffle::FullEvaluator::SCBF) {
        failUsage("graph-query does not support --full-evaluator=scbf", argv[0]);
    }
    if (opt.rewriteEngine == souffle::RewriteEngine::IMPLICIT ||
            opt.rewriteEngine == souffle::RewriteEngine::IMPLICIT_ITER) {
        failUsage("graph-query currently supports only legacy rewrite", argv[0]);
    }
    if (opt.fullEvaluator == souffle::FullEvaluator::APPROX) {
        if (opt.approxBackend == souffle::ApproxBackend::NONE) {
            opt.approxBackend = souffle::ApproxBackend::AMC;
        }
        if (opt.approxBackend != souffle::ApproxBackend::AMC) {
            failUsage("graph-query currently supports only --approx-backend=amc", argv[0]);
        }
        opt.backend = Backend::Amc;
    } else {
        if (opt.approxBackend != souffle::ApproxBackend::NONE) {
            failUsage("--approx-backend requires --full-evaluator=approx", argv[0]);
        }
        opt.backend = parseBackend(opt.ddBackend);
    }

    return opt;
}

std::vector<UntypedTuple> resolveQueryTuples(
        const IncrementalDerivationGraph& graph,
        const std::vector<std::string>& querySpecs,
        const std::vector<std::string>& queryAllRelations) {
    std::set<UntypedTuple> querySet;

    for (const auto& spec : querySpecs) {
        querySet.insert(parseTuple(spec));
    }

    if (!queryAllRelations.empty()) {
        std::unordered_set<std::string> wanted(queryAllRelations.begin(), queryAllRelations.end());
        std::unordered_set<std::string> seen;

        for (const auto& node : graph.getNodes()) {
            if (!node) {
                continue;
            }
            const auto& tuple = node->getTuple();
            if (wanted.count(tuple.relation_name)) {
                querySet.insert(tuple);
                seen.insert(tuple.relation_name);
            }
        }

        for (const auto& rel : wanted) {
            if (!seen.count(rel)) {
                std::cerr << "[warning] No tuple found for relation in query-all: " << rel
                          << "; ignored" << std::endl;
            }
        }
    }

    return std::vector<UntypedTuple>(querySet.begin(), querySet.end());
}

std::vector<NodePtr> resolveQueryNodes(
        IncrementalDerivationGraph& graph, const std::vector<UntypedTuple>& queries) {
    std::vector<NodePtr> result;
    result.reserve(queries.size());

    for (const auto& q : queries) {
        NodePtr node = graph.findNode(q);
        if (!node) {
            throw std::runtime_error("Query tuple not found in derivation graph: " + q.toString());
        }
        result.push_back(node);
    }

    return result;
}

SubgraphView buildBackwardSliceInView(
        const DerivationGraphViewInterface& view, const std::vector<NodePtr>& roots) {
    std::unordered_set<NodePtr> nodes;
    std::unordered_set<EdgePtr> edges;
    std::queue<NodePtr> work;

    for (const auto& root : roots) {
        if (!root) {
            continue;
        }
        if (nodes.insert(root).second) {
            work.push(root);
        }
    }

    while (!work.empty()) {
        NodePtr cur = work.front();
        work.pop();

        for (const auto& edge : view.getIncomingEdges(cur)) {
            edges.insert(edge);
            for (const auto& in : view.getInputs(edge)) {
                if (nodes.insert(in).second) {
                    work.push(in);
                }
            }
        }
    }

    return SubgraphView(std::move(nodes), std::move(edges));
}

SubgraphView buildBackwardSlice(const std::vector<NodePtr>& roots) {
    std::unordered_set<NodePtr> nodes;
    std::unordered_set<EdgePtr> edges;
    std::queue<NodePtr> work;

    for (const auto& root : roots) {
        if (!root) {
            continue;
        }
        if (nodes.insert(root).second) {
            work.push(root);
        }
    }

    while (!work.empty()) {
        NodePtr cur = work.front();
        work.pop();

        for (const auto& edge : cur->getIncomingEdges()) {
            edges.insert(edge);
            for (const auto& in : edge->getInputs()) {
                if (nodes.insert(in).second) {
                    work.push(in);
                }
            }
        }
    }

    return SubgraphView(std::move(nodes), std::move(edges));
}

NodePtr findByTupleInView(const SubgraphView& view, const UntypedTuple& tuple) {
    for (const auto& node : view.getNodes()) {
        if (node && node->getTuple() == tuple) {
            return node;
        }
    }
    return nullptr;
}

std::optional<double> findPrecomputedProbability(
        const IncrementalDerivationGraph& graph,
        const SubgraphView& activeView,
        const UntypedTuple& queryTuple) {
    if (NodePtr target = graph.findNode(queryTuple)) {
        auto it = precomputedProbResult.find(target);
        if (it != precomputedProbResult.end()) {
            return it->second;
        }
    }

    if (NodePtr inView = findByTupleInView(activeView, queryTuple)) {
        auto it = precomputedProbResult.find(inView);
        if (it != precomputedProbResult.end()) {
            return it->second;
        }
    }

    for (const auto& [node, prob] : precomputedProbResult) {
        if (node && node->getTuple() == queryTuple) {
            return prob;
        }
    }

    return std::nullopt;
}

double elapsedSeconds(const Clock::time_point start) {
    return std::chrono::duration<double>(Clock::now() - start).count();
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

struct RandomVarInfo {
    uint32_t id = 0;
    long double probTrue = 0.0L;
    std::string label;
};

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
        auto it = varIds_.find(var);
        if (it != varIds_.end()) {
            return it->second;
        }
        Expr e;
        e.kind = Kind::Var;
        e.var = var;
        exprs_.push_back(std::move(e));
        const int id = static_cast<int>(exprs_.size() - 1);
        varIds_[var] = id;
        return id;
    }

    int makeNot(int child) {
        if (child == trueId_) return falseId_;
        if (child == falseId_) return trueId_;
        const Expr& e = exprs_.at(static_cast<std::size_t>(child));
        if (e.kind == Kind::Not && !e.args.empty()) {
            return e.args.front();
        }
        auto it = notIds_.find(child);
        if (it != notIds_.end()) {
            return it->second;
        }
        Expr out;
        out.kind = Kind::Not;
        out.args.push_back(child);
        exprs_.push_back(std::move(out));
        const int id = static_cast<int>(exprs_.size() - 1);
        notIds_[child] = id;
        return id;
    }

    int makeAnd(const std::vector<int>& terms) {
        std::vector<int> normalized;
        normalized.reserve(terms.size());
        for (int t : terms) {
            if (t == falseId_) return falseId_;
            if (t == trueId_) continue;
            const Expr& e = exprs_.at(static_cast<std::size_t>(t));
            if (e.kind == Kind::And) {
                normalized.insert(normalized.end(), e.args.begin(), e.args.end());
            } else {
                normalized.push_back(t);
            }
        }
        if (normalized.empty()) return trueId_;
        std::sort(normalized.begin(), normalized.end());
        normalized.erase(std::unique(normalized.begin(), normalized.end()), normalized.end());
        for (int t : normalized) {
            if (containsComplement(normalized, t)) {
                return falseId_;
            }
        }
        if (normalized.empty()) return trueId_;
        if (normalized.size() == 1) return normalized.front();
        auto memoIt = andIds_.find(normalized);
        if (memoIt != andIds_.end()) {
            return memoIt->second;
        }
        Expr out;
        out.kind = Kind::And;
        out.args = std::move(normalized);
        exprs_.push_back(std::move(out));
        const int id = static_cast<int>(exprs_.size() - 1);
        andIds_[exprs_.back().args] = id;
        return id;
    }

    int makeOr(const std::vector<int>& terms) {
        std::vector<int> normalized;
        normalized.reserve(terms.size());
        for (int t : terms) {
            if (t == trueId_) return trueId_;
            if (t == falseId_) continue;
            const Expr& e = exprs_.at(static_cast<std::size_t>(t));
            if (e.kind == Kind::Or) {
                normalized.insert(normalized.end(), e.args.begin(), e.args.end());
            } else {
                normalized.push_back(t);
            }
        }
        if (normalized.empty()) return falseId_;
        std::sort(normalized.begin(), normalized.end());
        normalized.erase(std::unique(normalized.begin(), normalized.end()), normalized.end());
        for (int t : normalized) {
            if (containsComplement(normalized, t)) {
                return trueId_;
            }
        }
        if (normalized.empty()) return falseId_;
        if (normalized.size() == 1) return normalized.front();
        auto memoIt = orIds_.find(normalized);
        if (memoIt != orIds_.end()) {
            return memoIt->second;
        }
        Expr out;
        out.kind = Kind::Or;
        out.args = std::move(normalized);
        exprs_.push_back(std::move(out));
        const int id = static_cast<int>(exprs_.size() - 1);
        orIds_[exprs_.back().args] = id;
        return id;
    }

    const Expr& getExpr(int exprId) const {
        return exprs_.at(static_cast<std::size_t>(exprId));
    }

    bool eval(int exprId, const std::vector<uint8_t>& assignment) const {
        const Expr& e = exprs_.at(static_cast<std::size_t>(exprId));
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
        const Expr& e = exprs_.at(static_cast<std::size_t>(exprId));
        switch (e.kind) {
            case Kind::Const:
                return e.constValue ? "true" : "false";
            case Kind::Var:
                return "v" + std::to_string(e.var);
            case Kind::Not:
                return "!(" + toString(e.args.front()) + ")";
            case Kind::And: {
                std::ostringstream oss;
                oss << "(";
                for (std::size_t i = 0; i < e.args.size(); ++i) {
                    if (i != 0) {
                        oss << " & ";
                    }
                    oss << toString(e.args[i]);
                }
                oss << ")";
                return oss.str();
            }
            case Kind::Or: {
                std::ostringstream oss;
                oss << "(";
                for (std::size_t i = 0; i < e.args.size(); ++i) {
                    if (i != 0) {
                        oss << " | ";
                    }
                    oss << toString(e.args[i]);
                }
                oss << ")";
                return oss.str();
            }
        }
        return "<unknown>";
    }

    void collectVarIndexes(int exprId, std::unordered_set<int>& out) const {
        const Expr& e = exprs_.at(static_cast<std::size_t>(exprId));
        switch (e.kind) {
            case Kind::Const:
                return;
            case Kind::Var:
                out.insert(static_cast<int>(e.var));
                return;
            case Kind::Not:
                collectVarIndexes(e.args.front(), out);
                return;
            case Kind::And:
            case Kind::Or:
                for (int arg : e.args) {
                    collectVarIndexes(arg, out);
                }
                return;
        }
    }

    int trueId() const {
        return trueId_;
    }

    int falseId() const {
        return falseId_;
    }

private:
    struct VectorHash {
        std::size_t operator()(const std::vector<int>& values) const {
            std::size_t seed = 0;
            for (int value : values) {
                seed ^= std::hash<int>{}(value) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
            }
            return seed;
        }
    };

    bool containsComplement(const std::vector<int>& sortedTerms, int term) const {
        const Expr& expr = exprs_.at(static_cast<std::size_t>(term));
        if (expr.kind == Kind::Not && !expr.args.empty()) {
            return std::binary_search(sortedTerms.begin(), sortedTerms.end(), expr.args.front());
        }
        auto it = notIds_.find(term);
        return it != notIds_.end() &&
               std::binary_search(sortedTerms.begin(), sortedTerms.end(), it->second);
    }

    int addConst(bool value) {
        Expr e;
        e.kind = Kind::Const;
        e.constValue = value;
        exprs_.push_back(std::move(e));
        return static_cast<int>(exprs_.size() - 1);
    }

    std::vector<Expr> exprs_;
    std::unordered_map<uint32_t, int> varIds_;
    std::unordered_map<int, int> notIds_;
    std::unordered_map<std::vector<int>, int, VectorHash> andIds_;
    std::unordered_map<std::vector<int>, int, VectorHash> orIds_;
    int trueId_ = -1;
    int falseId_ = -1;
};

struct SymbolicNodeRef {
    int exprId = -1;
    const ExprArena* arena = nullptr;

    void* get() const {
        return (arena != nullptr && exprId >= 0) ? const_cast<SymbolicNodeRef*>(this) : nullptr;
    }

    bool isVar() const {
        return arena && arena->getExpr(exprId).kind == ExprArena::Kind::Var;
    }

    bool isNot() const {
        return arena && arena->getExpr(exprId).kind == ExprArena::Kind::Not;
    }

    bool isAnd() const {
        return arena && arena->getExpr(exprId).kind == ExprArena::Kind::And;
    }

    bool isOr() const {
        return arena && arena->getExpr(exprId).kind == ExprArena::Kind::Or;
    }

    int getVarIndex() const {
        return isVar() ? static_cast<int>(arena->getExpr(exprId).var) : -1;
    }

    std::vector<SymbolicNodeRef> getOperands() const {
        std::vector<SymbolicNodeRef> out;
        if (!arena || exprId < 0) {
            return out;
        }
        const auto& e = arena->getExpr(exprId);
        out.reserve(e.args.size());
        for (int arg : e.args) {
            out.push_back(SymbolicNodeRef{arg, arena});
        }
        return out;
    }
};

class SymbolicFormulaManager final : public FormulaManager<SymbolicNodeRef> {
public:
    using VariableWeight = FormulaManager<SymbolicNodeRef>::VariableWeight;

    SymbolicNodeRef createVar(int index) override {
        ensureWeightSlot(index);
        return SymbolicNodeRef{arena_.makeVar(static_cast<uint32_t>(index)), &arena_};
    }

    SymbolicNodeRef createVar(int index, const Node& node) override {
        ensureWeightSlot(index);
        labels_[index] = node.toString();
        return SymbolicNodeRef{arena_.makeVar(static_cast<uint32_t>(index)), &arena_};
    }

    SymbolicNodeRef createVar(int index, const Hyperedge& edge) override {
        ensureWeightSlot(index);
        labels_[index] = edge.toString();
        return SymbolicNodeRef{arena_.makeVar(static_cast<uint32_t>(index)), &arena_};
    }

    SymbolicNodeRef makeAnd(const SymbolicNodeRef& a, const SymbolicNodeRef& b) override {
        return makeAnd(std::vector<SymbolicNodeRef>{a, b});
    }

    SymbolicNodeRef makeAnd(const std::vector<SymbolicNodeRef>& nodes) override {
        std::vector<int> exprs;
        exprs.reserve(nodes.size());
        for (const auto& node : nodes) {
            exprs.push_back(node.exprId);
        }
        return SymbolicNodeRef{arena_.makeAnd(exprs), &arena_};
    }

    SymbolicNodeRef makeOr(const SymbolicNodeRef& a, const SymbolicNodeRef& b) override {
        return makeOr(std::vector<SymbolicNodeRef>{a, b});
    }

    SymbolicNodeRef makeOr(const std::vector<SymbolicNodeRef>& nodes) override {
        std::vector<int> exprs;
        exprs.reserve(nodes.size());
        for (const auto& node : nodes) {
            exprs.push_back(node.exprId);
        }
        return SymbolicNodeRef{arena_.makeOr(exprs), &arena_};
    }

    SymbolicNodeRef makeNot(const SymbolicNodeRef& a) override {
        return SymbolicNodeRef{arena_.makeNot(a.exprId), &arena_};
    }

    SymbolicNodeRef makeCondition(
            const SymbolicNodeRef& f, const std::vector<int>& trueIndexes, const std::vector<int>& falseIndexes) override {
        std::vector<SymbolicNodeRef> terms;
        terms.push_back(f);
        for (int idx : trueIndexes) {
            terms.push_back(createVar(idx));
        }
        for (int idx : falseIndexes) {
            terms.push_back(makeNot(createVar(idx)));
        }
        return makeAnd(terms);
    }

    bool isSame(const SymbolicNodeRef& a, const SymbolicNodeRef& b) override {
        return a.exprId == b.exprId && a.arena == b.arena;
    }

    SymbolicNodeRef getTrue() override {
        return SymbolicNodeRef{arena_.trueId(), &arena_};
    }

    SymbolicNodeRef getFalse() override {
        return SymbolicNodeRef{arena_.falseId(), &arena_};
    }

    std::string toString(const SymbolicNodeRef& node) override {
        if (!node.arena || node.exprId < 0) {
            return "<invalid>";
        }
        return arena_.toString(node.exprId);
    }

    void setVariableWeight(int varIndex, double posWeight, double negWeight) override {
        ensureWeightSlot(varIndex);
        weights_[varIndex] = VariableWeight{posWeight, negWeight};
    }

    VariableWeight getVariableWeight(int varIndex) const override {
        if (varIndex >= 0 && static_cast<std::size_t>(varIndex) < weights_.size()) {
            return weights_[varIndex];
        }
        return VariableWeight{1.0, 0.0};
    }

    bool hasVariableWeight(int varIndex) const override {
        return varIndex >= 0 && static_cast<std::size_t>(varIndex) < hasWeight_.size() && hasWeight_[varIndex];
    }

    double computeWeightedModelCount(const SymbolicNodeRef& node) override {
        if (!node.arena || node.exprId < 0) {
            return 0.0;
        }
        std::unordered_set<int> used;
        arena_.collectVarIndexes(node.exprId, used);
        if (used.empty()) {
            return arena_.eval(node.exprId, std::vector<uint8_t>{0}) ? 1.0 : 0.0;
        }
        if (used.size() > 24) {
            throw std::runtime_error(
                    "Symbolic exact WMC supports at most 24 vars in standalone AMC prep; got " +
                    std::to_string(used.size()));
        }
        std::vector<int> vars(used.begin(), used.end());
        std::sort(vars.begin(), vars.end());
        const int maxVar = vars.back();
        std::vector<uint8_t> assignment(static_cast<std::size_t>(maxVar + 1), 0);
        long double total = 0.0L;
        const std::size_t combinations = std::size_t{1} << vars.size();
        for (std::size_t mask = 0; mask < combinations; ++mask) {
            long double weight = 1.0L;
            for (std::size_t i = 0; i < vars.size(); ++i) {
                const int var = vars[i];
                const bool value = ((mask >> i) & 1U) != 0U;
                assignment[static_cast<std::size_t>(var)] = value ? 1U : 0U;
                const VariableWeight w = getVariableWeight(var);
                weight *= value ? w.posWeight : w.negWeight;
            }
            if (arena_.eval(node.exprId, assignment)) {
                total += weight;
            }
        }
        return static_cast<double>(total);
    }

    int getVarIndex(const Node& node) override {
        auto it = nodeIndex_.find(&node);
        if (it != nodeIndex_.end()) {
            return it->second;
        }
        const int idx = nextVarIndex_++;
        nodeIndex_[&node] = idx;
        ensureWeightSlot(idx);
        labels_[idx] = node.toString();
        return idx;
    }

    int getVarIndex(const Hyperedge& edge) override {
        auto it = edgeIndex_.find(&edge);
        if (it != edgeIndex_.end()) {
            return it->second;
        }
        const int idx = nextVarIndex_++;
        edgeIndex_[&edge] = idx;
        ensureWeightSlot(idx);
        labels_[idx] = edge.toString();
        return idx;
    }

    bool peekVarIndex(const Node& node, int& out) const override {
        auto it = nodeIndex_.find(&node);
        if (it == nodeIndex_.end()) {
            return false;
        }
        out = it->second;
        return true;
    }

    bool peekVarIndex(const Hyperedge& edge, int& out) const override {
        auto it = edgeIndex_.find(&edge);
        if (it == edgeIndex_.end()) {
            return false;
        }
        out = it->second;
        return true;
    }

    void bindVarIndex(const Node& node, int idx) override {
        nodeIndex_[&node] = idx;
        ensureWeightSlot(idx);
    }

    void bindVarIndex(const Hyperedge& edge, int idx) override {
        edgeIndex_[&edge] = idx;
        ensureWeightSlot(idx);
    }

    void printInfo(const SymbolicNodeRef&, const std::string&) override {}
    void dumpProfilingStatistics() override {}

    std::size_t getLiveNodeCount() const override {
        return labels_.empty() ? 0U : labels_.size() - 1U;
    }

    const ExprArena& arena() const {
        return arena_;
    }

    std::vector<RandomVarInfo> randomVars() const {
        std::vector<RandomVarInfo> out(static_cast<std::size_t>(nextVarIndex_));
        for (int idx = 1; idx < nextVarIndex_; ++idx) {
            VariableWeight w = getVariableWeight(idx);
            std::string label = (static_cast<std::size_t>(idx) < labels_.size()) ? labels_[idx] : ("v" + std::to_string(idx));
            out[static_cast<std::size_t>(idx)] =
                    RandomVarInfo{static_cast<uint32_t>(idx), w.posWeight, std::move(label)};
        }
        return out;
    }

private:
    void ensureWeightSlot(int idx) {
        if (idx < 0) {
            return;
        }
        const std::size_t need = static_cast<std::size_t>(idx + 1);
        if (weights_.size() < need) {
            weights_.resize(need, VariableWeight{1.0, 0.0});
            hasWeight_.resize(need, false);
            labels_.resize(need);
        }
        hasWeight_[static_cast<std::size_t>(idx)] = true;
    }

    ExprArena arena_;
    int nextVarIndex_ = 1;
    std::vector<VariableWeight> weights_;
    std::vector<bool> hasWeight_;
    std::vector<std::string> labels_;
    std::unordered_map<const Node*, int> nodeIndex_;
    std::unordered_map<const Hyperedge*, int> edgeIndex_;
};

class ViewFormulaBuilder {
public:
    ViewFormulaBuilder(const DerivationGraphViewInterface& view, ExprArena& arena)
            : view_(view), arena_(arena) {
        randomVars_.push_back(RandomVarInfo{});
    }

    int buildNodeFormula(const NodePtr& node) {
        auto memoIt = nodeFormula_.find(node);
        if (memoIt != nodeFormula_.end()) {
            return memoIt->second;
        }
        if (!node || !view_.getNodes().count(node)) {
            return arena_.makeConst(false);
        }
        if (!nodeStack_.insert(node).second) {
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
            const auto& incoming = view_.getIncomingEdges(node);
            disj.reserve(incoming.size());
            for (const auto& edge : incoming) {
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
        if (!edge || !view_.getEdges().count(edge)) {
            return arena_.makeConst(false);
        }
        if (!edgeStack_.insert(edge).second) {
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
            const auto inputs = view_.getInputs(edge);
            const auto negs = view_.getBodyNegations(edge);
            for (std::size_t i = 0; i < inputs.size(); ++i) {
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

private:
    struct SupportKeyHash {
        std::size_t operator()(const std::vector<SupportToken>& values) const {
            std::size_t seed = 0;
            for (SupportToken value : values) {
                seed ^= std::hash<SupportToken>{}(value) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
            }
            return seed;
        }
    };

    static long double clampProbability(long double p) {
        return std::min(1.0L, std::max(0.0L, p));
    }

    uint32_t ensureSupportVar(
            const std::vector<SupportToken>& support, const std::string& label, long double p) {
        auto it = supportVar_.find(support);
        if (it != supportVar_.end()) {
            const long double existing =
                    randomVars_.at(static_cast<std::size_t>(it->second)).probTrue;
            if (std::fabs(existing - clampProbability(p)) > kProbEps) {
                throw std::runtime_error(
                        "Inconsistent probability for shared probabilistic support in AMC extraction: " +
                        label);
            }
            return it->second;
        }
        const uint32_t id = addRandomVar(label, p);
        supportVar_.emplace(support, id);
        return id;
    }

    uint32_t ensureFactVar(const NodePtr& node, long double p) {
        const auto& support = node->getProbabilisticSupportTokens();
        if (!support.empty()) {
            return ensureSupportVar(support, "fact:" + node->toString(), p);
        }
        auto it = factSemanticVar_.find(node->getSemanticFactId());
        if (it != factSemanticVar_.end()) {
            return it->second;
        }
        const uint32_t id = addRandomVar("fact:" + node->toString(), p);
        factSemanticVar_[node->getSemanticFactId()] = id;
        return id;
    }

    uint32_t ensureEdgeVar(const EdgePtr& edge, long double p) {
        const auto& support = edge->getProbabilisticSupportTokens();
        if (!support.empty()) {
            return ensureSupportVar(support, "edge:" + edge->toString(), p);
        }
        auto it = edgeVarById_.find(edge->getId());
        if (it != edgeVarById_.end()) {
            return it->second;
        }
        const uint32_t id = addRandomVar("edge:" + edge->toString(), p);
        edgeVarById_[edge->getId()] = id;
        return id;
    }

    uint32_t addRandomVar(const std::string& label, long double p) {
        if (p < -kProbEps || p > 1.0L + kProbEps) {
            throw std::runtime_error("Probability out of range for random variable: " + label);
        }
        const uint32_t id = static_cast<uint32_t>(randomVars_.size());
        randomVars_.push_back(RandomVarInfo{id, clampProbability(p), label});
        return id;
    }

    const DerivationGraphViewInterface& view_;
    ExprArena& arena_;
    std::vector<RandomVarInfo> randomVars_;
    std::unordered_map<std::vector<SupportToken>, uint32_t, SupportKeyHash> supportVar_;
    std::unordered_map<std::size_t, uint32_t> factSemanticVar_;
    std::unordered_map<std::size_t, uint32_t> edgeVarById_;
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

void writeApproxmcCnf(const approxmc_demo::UnweightedCNFResult& cnf, const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        throw std::runtime_error("Failed to open CNF output: " + path);
    }
    out << "p cnf " << cnf.numVars << " " << cnf.clauses.size() << "\n";
    out << "c p show";
    for (uint32_t v : cnf.samplingSet) {
        out << " " << v;
    }
    out << " 0\n";
    for (const auto& clause : cnf.clauses) {
        for (int lit : clause) {
            out << lit << " ";
        }
        out << "0\n";
    }
}

struct ApproxmcRunResult {
    bool ok = false;
    long double unweightedEstimate = 0.0L;
    uint32_t hashCount = 0;
    uint64_t cellSolCount = 0;
    std::string rawOutput;
    std::string message;
    double runtimeSec = 0.0;
    const char* counterKind = "cli";
};

#ifdef SOUFFLE_STANDALONE_HAS_APPROXMC_LIB
ApproxmcRunResult runApproxmcLibrary(
        const approxmc_demo::UnweightedCNFResult& converted, double epsilon, double delta, uint32_t seed,
        uint32_t verb) {
    ApproxmcRunResult res;
    res.counterKind = "lib";
    try {
        std::unique_ptr<CMSat::FieldGen> fg = std::make_unique<CMSat::FGenDouble>();
        ApproxMC::AppMC appmc(fg);
        appmc.set_seed(seed);
        appmc.set_epsilon(epsilon);
        appmc.set_delta(delta);
        appmc.set_verbosity(verb);

        appmc.new_vars(converted.numVars);
        for (const auto& clause : converted.clauses) {
            std::vector<CMSat::Lit> lits;
            lits.reserve(clause.size());
            for (int dimacsLit : clause) {
                const uint32_t var = static_cast<uint32_t>(std::abs(dimacsLit) - 1);
                lits.emplace_back(var, dimacsLit < 0);
            }
            appmc.add_clause(lits);
        }

        std::vector<uint32_t> samplVars;
        samplVars.reserve(converted.samplingSet.size());
        for (uint32_t v : converted.samplingSet) {
            samplVars.push_back(v - 1);
        }
        appmc.set_sampl_vars(samplVars);

        const auto start = Clock::now();
        const ApproxMC::SolCount count = appmc.count();
        res.runtimeSec = elapsedSeconds(start);
        if (!count.valid) {
            res.message = "ApproxMC library returned invalid count";
            return res;
        }
        res.hashCount = count.hashCount;
        res.cellSolCount = count.cellSolCount;
        res.unweightedEstimate =
                std::ldexp(static_cast<long double>(count.cellSolCount), static_cast<int>(count.hashCount));
        res.ok = true;
        return res;
    } catch (const std::exception& ex) {
        res.message = ex.what();
        return res;
    }
}
#endif

ApproxmcRunResult runApproxmc(
        const std::string& approxmcBin,
        const std::string& cnfPath,
        double epsilon,
        double delta,
        uint32_t seed,
        uint32_t verb,
        bool streamOutput) {
    ApproxmcRunResult res;
    std::ostringstream cmd;
    cmd << "\"" << approxmcBin << "\""
        << " --verb " << verb
        << " --seed " << seed
        << " --epsilon " << std::setprecision(17) << epsilon
        << " --delta " << std::setprecision(17) << delta
        << " \"" << cnfPath << "\""
        << " 2>&1";

    const auto start = Clock::now();
    FILE* pipe = popen(cmd.str().c_str(), "r");
    if (!pipe) {
        res.message = "Failed to start approxmc process";
        return res;
    }

    char buf[4096];
    std::string output;
    while (std::fgets(buf, sizeof(buf), pipe) != nullptr) {
        output.append(buf);
        if (streamOutput) {
            std::cerr << "[approxmc] " << buf;
            std::cerr.flush();
        }
    }
    const int rc = pclose(pipe);
    res.runtimeSec = elapsedSeconds(start);
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
            while (!num.empty() && std::isspace(static_cast<unsigned char>(num.back()))) {
                num.pop_back();
            }
            if (num.empty()) {
                continue;
            }
            try {
                res.unweightedEstimate = std::stold(num);
                found = true;
            } catch (const std::exception&) {
                res.message = "Failed to parse s mc";
                return res;
            }
        }
        constexpr const char* kSolutions = "Number of solutions is:";
        const auto solPos = line.find(kSolutions);
        if (solPos != std::string::npos) {
            std::string rest = trim(line.substr(solPos + std::strlen(kSolutions)));
            const auto starPos = rest.find("*2**");
            if (starPos != std::string::npos) {
                try {
                    res.cellSolCount = static_cast<uint64_t>(std::stoull(rest.substr(0, starPos)));
                    res.hashCount = static_cast<uint32_t>(std::stoul(rest.substr(starPos + 4)));
                } catch (const std::exception&) {
                }
            }
        }
        constexpr const char* kShim = "c pyapproxmc done ";
        if (line.rfind(kShim, 0) == 0) {
            const auto cellPos = line.find("cell=");
            const auto hashPos = line.find("hashes=");
            if (cellPos != std::string::npos) {
                try {
                    res.cellSolCount = static_cast<uint64_t>(std::stoull(line.substr(cellPos + 5)));
                } catch (const std::exception&) {
                }
            }
            if (hashPos != std::string::npos) {
                try {
                    res.hashCount = static_cast<uint32_t>(std::stoul(line.substr(hashPos + 7)));
                } catch (const std::exception&) {
                }
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
        if (std::isalnum(static_cast<unsigned char>(c))) {
            out.push_back(c);
        } else {
            out.push_back('_');
        }
    }
    return out;
}

struct AmcQueryResult {
    double probability = 0.0;
    std::size_t randomVars = 0;
    uint32_t weightedVars = 0;
    uint32_t weightedClauses = 0;
    uint32_t unweightedVars = 0;
    uint32_t unweightedClauses = 0;
    uint32_t projectionVars = 0;
    uint32_t forcedAssignments = 0;
    uint32_t weightedVarsBefore = 0;
    uint32_t weightedVarsAfter = 0;
    uint32_t divideExp = 0;
    int64_t addedVars = 0;
    int64_t addedClauses = 0;
    long double multiplier = 1.0L;
    long double tilt = 1.0L;
    bool tiltViolated = false;
    long double maxQuantAbsError = 0.0L;
    long double maxQuantRelError = 0.0L;
    double extractSec = 0.0;
    double encodeSec = 0.0;
    double convertSec = 0.0;
    double approxmcSec = 0.0;
    uint32_t approxmcHashCount = 0;
    uint64_t approxmcCellSolCount = 0;
    const char* counterKind = "cli";
    bool usedApproxmc = false;
};

struct AmcSummary {
    std::size_t approxQueries = 0;
    std::size_t deterministicQueries = 0;
    std::size_t maxRandomVars = 0;
    uint32_t maxWeightedVars = 0;
    uint32_t maxWeightedClauses = 0;
    uint32_t maxUnweightedVars = 0;
    uint32_t maxUnweightedClauses = 0;
    uint32_t maxProjectionVars = 0;
    double approxmcSec = 0.0;
};

bool hasNonTrivialCycles(SubgraphView& view);

bool hasUnsupportedAmcCycles(SubgraphView& view) {
    if (hasNonTrivialCycles(view)) {
        return true;
    }
    for (const auto& edge : view.getEdges()) {
        if (!edge) {
            continue;
        }
        const NodePtr output = view.getOutput(edge);
        if (!output) {
            continue;
        }
        for (const auto& input : view.getInputs(edge)) {
            if (input == output) {
                return true;
            }
        }
    }
    return false;
}

AmcQueryResult evaluateQueryWithAmc(SubgraphView& view, const NodePtr& query, const Options& opt) {
    AmcQueryResult out;

    if (hasUnsupportedAmcCycles(view)) {
        throw std::runtime_error(
                "backend=amc currently requires an acyclic query slice after rewrite");
    }

    const auto extractStart = Clock::now();
    ExprArena arena;
    // AMC uses query-local backward extraction with path-local cycle cutting.
    // This avoids relying on buildFormulasCyclewise() convergence via semantic
    // equality, which holds for DD managers but not for the symbolic DAG used
    // here.
    ViewFormulaBuilder builder(view, arena);
    const int formula = builder.buildNodeFormula(query);
    const auto& randomVars = builder.randomVars();
    out.extractSec = elapsedSeconds(extractStart);
    out.randomVars = randomVars.empty() ? 0U : static_cast<std::size_t>(randomVars.size() - 1U);

    if (formula == arena.falseId()) {
        out.probability = 0.0;
        return out;
    }
    if (formula == arena.trueId()) {
        out.probability = 1.0;
        return out;
    }

    const auto encodeStart = Clock::now();
    TseitinCnfEncoder encoder(arena, randomVars);
    CnfBuildResult weighted = encoder.encode(formula);
    out.weightedVars = weighted.weighted.numVars;
    out.weightedClauses = static_cast<uint32_t>(weighted.weighted.clauses.size());
    out.encodeSec = elapsedSeconds(encodeStart);

    const auto convertStart = Clock::now();
    approxmc_demo::ConversionConfig cfg;
    cfg.precision = opt.amcPrecision;
    cfg.preprocess = opt.amcPreprocess;
    cfg.verbosity = 0;
    cfg.tiltMax = 0.0L;
    cfg.failOnTilt = false;
    auto converted = approxmc_demo::convertWeightedToUnweighted(std::move(weighted.weighted), cfg);
    out.convertSec = elapsedSeconds(convertStart);
    if (!converted.report.valid) {
        throw std::runtime_error(
                "Weighted->unweighted conversion failed for " + query->getTuple().toString() +
                ": " + converted.report.message);
    }

    out.unweightedVars = converted.numVars;
    out.unweightedClauses = static_cast<uint32_t>(converted.clauses.size());
    out.projectionVars = converted.report.outputSamplingSize;
    out.forcedAssignments = converted.report.forcedAssignments;
    out.weightedVarsBefore = converted.report.weightedVarsBefore;
    out.weightedVarsAfter = converted.report.weightedVarsAfter;
    out.divideExp = converted.report.divideExp;
    out.addedVars = converted.report.addedVars;
    out.addedClauses = converted.report.addedClauses;
    out.multiplier = converted.report.multiplier;
    out.tilt = converted.report.tilt;
    out.tiltViolated = converted.report.tiltViolated;
    out.maxQuantAbsError = converted.report.maxQuantAbsError;
    out.maxQuantRelError = converted.report.maxQuantRelError;
    if (weighted.unsat || converted.report.unsat) {
        out.probability = 0.0;
        return out;
    }

    if (opt.amcStreamOutput) {
        std::cerr << "[amc-run] query=" << query->getTuple().toString()
                  << " counter="
#ifdef SOUFFLE_STANDALONE_HAS_APPROXMC_LIB
                  << "lib"
#else
                  << "cli"
#endif
                  << " weighted_vars_before=" << out.weightedVarsBefore
                  << " weighted_vars_after=" << out.weightedVarsAfter
                  << " weighted_cnf_vars=" << out.weightedVars
                  << " weighted_cnf_clauses=" << out.weightedClauses
                  << " unweighted_cnf_vars=" << out.unweightedVars
                  << " unweighted_cnf_clauses=" << out.unweightedClauses
                  << " projection_vars=" << out.projectionVars
                  << " added_vars=" << out.addedVars
                  << " added_clauses=" << out.addedClauses
                  << " tilt=" << out.tilt
                  << " max_quant_rel_err=" << out.maxQuantRelError
                  << " precision=" << opt.amcPrecision
                  << " epsilon=" << opt.epsilon
                  << " delta=" << opt.delta
#ifndef SOUFFLE_STANDALONE_HAS_APPROXMC_LIB
                  << " cnf="
                  << "/tmp/souffle_graph_query_amc_" + sanitizeFileSuffix(query->getTuple().toString()) + ".cnf"
#endif
                  << "\n";
        std::cerr.flush();
    }
#ifdef SOUFFLE_STANDALONE_HAS_APPROXMC_LIB
    const auto approx = runApproxmcLibrary(converted, opt.epsilon, opt.delta, opt.seed, opt.amcVerb);
#else
    const std::string cnfPath =
            "/tmp/souffle_graph_query_amc_" + sanitizeFileSuffix(query->getTuple().toString()) + ".cnf";
    writeApproxmcCnf(converted, cnfPath);
    const auto approx =
            runApproxmc(opt.approxmcBin, cnfPath, opt.epsilon, opt.delta, opt.seed, opt.amcVerb, opt.amcStreamOutput);
    std::remove(cnfPath.c_str());
#endif
    if (!approx.ok) {
        throw std::runtime_error(
                "ApproxMC failed for " + query->getTuple().toString() + ": " + approx.message +
                "\n" + approx.rawOutput);
    }
    if (opt.amcStreamOutput) {
        std::cerr << "[amc-run] query=" << query->getTuple().toString()
                  << " counter=" << approx.counterKind
                  << " approxmc_runtime_s=" << approx.runtimeSec
                  << " cell=" << approx.cellSolCount
                  << " hashes=" << approx.hashCount
                  << " unweighted_estimate=" << std::setprecision(20) << approx.unweightedEstimate << "\n";
        std::cerr.flush();
    }

    out.usedApproxmc = true;
    out.approxmcSec = approx.runtimeSec;
    out.approxmcHashCount = approx.hashCount;
    out.approxmcCellSolCount = approx.cellSolCount;
    out.counterKind = approx.counterKind;
    out.probability = static_cast<double>(applyMultiplier(converted, approx.unweightedEstimate));
    return out;
}

WeightedBDDManager::InitConfig makeCuddInitConfig(std::size_t varCount) {
    WeightedBDDManager::InitConfig cfg;
    const auto maxVars = std::numeric_limits<unsigned int>::max();
    const auto doubledVars = varCount > maxVars / 2 ? maxVars : static_cast<unsigned int>(varCount * 2);
    cfg.numVars = doubledVars;
    cfg.numSlots = 512;
    // Match full pipeline sizing to keep CUDD behavior comparable.
    if (varCount <= 256) {
        cfg.cacheSize = 1u << 18;
        cfg.maxMemory = 1UL << 30;
    } else if (varCount <= 1024) {
        cfg.cacheSize = 1u << 20;
        cfg.maxMemory = 4UL << 30;
    } else if (varCount <= 4096) {
        cfg.cacheSize = 1u << 22;
        cfg.maxMemory = 8UL << 30;
    } else if (varCount > 10000) {
        cfg.cacheSize = 1u << 26;
    }
    return cfg;
}

struct ComponentAnalysisStandalone {
    ComponentSubgraph comp;
    SingleRandVarInfo singleRand;
    std::size_t randVars = 0;
    bool hasNegation = false;
    bool hasOr = false;
    bool hasCycle = false;
};

std::vector<ComponentAnalysisStandalone> analyzeComponentsStandalone(
        const DerivationGraphViewInterface& view, std::vector<ComponentSubgraph> components) {
    auto& depGraph = view.getCycleDependencyGraph();
    std::vector<ComponentAnalysisStandalone> analyses;
    analyses.reserve(components.size());

    for (auto& comp : components) {
        ComponentAnalysisStandalone analysis;
        analysis.comp = std::move(comp);
        std::unordered_map<NodePtr, std::size_t> incomingCounts;
        incomingCounts.reserve(analysis.comp.nodes.size());

        for (const auto& node : analysis.comp.nodes) {
            if (node->isFact && node->getProbability() != 1.0) {
                analysis.randVars++;
                if (analysis.randVars == 1) {
                    analysis.singleRand.node = node;
                    analysis.singleRand.edge.reset();
                    analysis.singleRand.probability = node->getProbability();
                }
            }
            auto it = depGraph.nodeToCycleIndex.find(node);
            if (it != depGraph.nodeToCycleIndex.end() && depGraph.nodeCycles[it->second].size() > 1) {
                analysis.hasCycle = true;
            }
        }

        for (const auto& edge : analysis.comp.edges) {
            if (!edge->isDeterministic()) {
                analysis.randVars++;
                if (analysis.randVars == 1) {
                    analysis.singleRand.node.reset();
                    analysis.singleRand.edge = edge;
                    analysis.singleRand.probability = edge->getProbability();
                }
            }

            const auto negs = view.getBodyNegations(edge);
            for (bool neg : negs) {
                if (neg) {
                    analysis.hasNegation = true;
                    break;
                }
            }

            NodePtr out = view.getOutput(edge);
            if (out) {
                incomingCounts[out]++;
                for (const auto& in : view.getInputs(edge)) {
                    if (in == out) {
                        analysis.hasCycle = true;
                        break;
                    }
                }
            }
        }

        for (const auto& node : analysis.comp.nodes) {
            auto it = incomingCounts.find(node);
            if (it != incomingCounts.end()) {
                if (it->second > 1) {
                    analysis.hasOr = true;
                }
                if (node->isFact && it->second > 0) {
                    analysis.hasOr = true;
                }
            }
        }

        analyses.push_back(std::move(analysis));
    }

    return analyses;
}

std::unordered_set<std::string> parseFeatureSet(const char* raw) {
    std::unordered_set<std::string> features;
    if (!raw || !*raw) {
        return features;
    }
    std::string text(raw);
    for (char& c : text) {
        if (c == ';' || c == '/') {
            c = ',';
        }
    }
    std::stringstream ss(text);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        tok = toLower(trim(tok));
        if (!tok.empty()) {
            features.insert(tok);
        }
    }
    return features;
}

bool envTruthy(const char* raw) {
    if (!raw) {
        return false;
    }
    const std::string val = toLower(trim(raw));
    return val == "1" || val == "true" || val == "yes" || val == "on";
}

bool standaloneFastPathEnabled() {
    return !envTruthy(std::getenv("SOUFFLE_STANDALONE_DISABLE_FASTPATH"));
}

bool hasNonTrivialCycles(SubgraphView& view) {
    const auto& cdg = view.getCycleDependencyGraph();
    for (const auto& cycle : cdg.nodeCycles) {
        if (cycle.size() > 1) {
            return true;
        }
    }
    return false;
}

souffle::problog::RewriteFeatureFlags buildStandaloneRewriteFlags(const Options& opt) {
    // Keep default behavior aligned with the main pipeline.
    // Env override still supports targeted feature scans.
    souffle::problog::RewriteFeatureFlags flags;

    const auto features = parseFeatureSet(std::getenv("SOUFFLE_STANDALONE_RW_FEATURES"));
    if (!features.empty()) {
        flags.splitMode = souffle::problog::SplitMode::None;
        flags.enableCompaction = false;
        flags.enableCleanupIsolated = false;
        flags.enableSingleHyperedge = false;
        flags.enableAllFactsToSO = false;
        flags.enableGeneral = false;
        flags.enableLinearTwoEdge = false;
        flags.enableParallelEdge = false;
        flags.enableFanOutConverge = false;
        const auto enabled = [&](const std::string& name) {
            return features.count("all") || features.count(name);
        };

        if (enabled("single")) flags.enableSingleHyperedge = true;
        if (enabled("allfacts")) flags.enableAllFactsToSO = true;
        if (enabled("linear")) flags.enableLinearTwoEdge = true;
        if (enabled("parallel")) flags.enableParallelEdge = true;
        if (enabled("fanout")) flags.enableFanOutConverge = true;
        if (enabled("general")) flags.enableGeneral = true;
        if (enabled("compaction")) flags.enableCompaction = true;
        if (enabled("cleanup")) flags.enableCleanupIsolated = true;
        if (enabled("force-complete-detect")) flags.forceCompleteSisoDetect = true;
        if (enabled("split-naive")) {
            flags.splitMode = souffle::problog::SplitMode::Naive;
        } else if (enabled("split-complete")) {
            flags.splitMode = souffle::problog::SplitMode::Complete;
        }
    }
    switch (opt.rewriteSplit) {
        case souffle::RewriteSplitMode::OFF:
            flags.splitMode = souffle::problog::SplitMode::None;
            break;
        case souffle::RewriteSplitMode::NAIVE:
            flags.splitMode = souffle::problog::SplitMode::Naive;
            break;
        case souffle::RewriteSplitMode::COMPLETE:
            flags.splitMode = souffle::problog::SplitMode::Complete;
            break;
    }
    flags.forceCompleteSisoDetect = opt.rewriteDetect == souffle::RewriteDetectMode::COMPLETE;
    return flags;
}

struct RewriteSafetyAdjustments {
    bool hasCycles = false;
    bool disabledSingle = false;
    bool disabledCompaction = false;
};

RewriteSafetyAdjustments enforceStandaloneRewriteSafety(
        souffle::problog::RewriteFeatureFlags& flags, SubgraphView& view) {
    RewriteSafetyAdjustments out;
    if (!flags.enableAllFactsToSO) {
        return out;
    }
    if (envTruthy(std::getenv("SOUFFLE_STANDALONE_ALLOW_RISKY_SINGLE_ALLFACTS")) ||
            envTruthy(std::getenv("SOUFFLE_STANDALONE_ALLOW_RISKY_FACT_ABSORB_IN_CYCLES"))) {
        return out;
    }

    out.hasCycles = hasNonTrivialCycles(view);
    if (!out.hasCycles) {
        return out;
    }

    // In standalone JSON replay, fact-absorption rewrites on cyclic graphs can
    // erase the anchoring structure FC relies on, which leads to repeated SCC
    // stalls and eventual "giving up on edge" fallbacks.
    if (flags.enableSingleHyperedge) {
        flags.enableSingleHyperedge = false;
        out.disabledSingle = true;
    }
    if (flags.enableCompaction) {
        flags.enableCompaction = false;
        out.disabledCompaction = true;
    }
    return out;
}

struct ViewSanitizeStats {
    std::size_t removedNullNodes = 0;
    std::size_t removedNullEdges = 0;
    std::size_t removedDanglingEdges = 0;
};

struct ViewAdjacencyConsistencyStats {
    std::size_t missingOutIncomingMembership = 0;
    std::size_t missingInputOutgoingMembership = 0;
    std::size_t malformedNodeIncomingMembership = 0;
    std::size_t malformedNodeOutgoingMembership = 0;
};

ViewSanitizeStats sanitizeView(SubgraphView& view) {
    ViewSanitizeStats stats;
    auto& nodes = view.mutableNodes();
    auto& edges = view.mutableEdges();

    for (auto it = nodes.begin(); it != nodes.end();) {
        if (!(*it)) {
            ++stats.removedNullNodes;
            it = nodes.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = edges.begin(); it != edges.end();) {
        const EdgePtr& edge = *it;
        if (!edge) {
            ++stats.removedNullEdges;
            it = edges.erase(it);
            continue;
        }

        NodePtr out = edge->getOutput();
        if (!out || !nodes.count(out)) {
            ++stats.removedDanglingEdges;
            it = edges.erase(it);
            continue;
        }

        bool valid = true;
        for (const auto& in : edge->getInputs()) {
            if (!in || !nodes.count(in)) {
                valid = false;
                break;
            }
        }
        if (!valid) {
            ++stats.removedDanglingEdges;
            it = edges.erase(it);
            continue;
        }

        ++it;
    }

    if (stats.removedNullNodes || stats.removedNullEdges || stats.removedDanglingEdges) {
        view.clearViewCaches();
    }
    return stats;
}

ViewAdjacencyConsistencyStats checkViewAdjacencyConsistency(
        const SubgraphView& view, std::size_t sampleLimit = 8) {
    ViewAdjacencyConsistencyStats stats;
    const auto& nodes = view.getNodes();
    const auto& edges = view.getEdges();

    auto containsEdgePtr = [](const std::vector<EdgePtr>& vec, const EdgePtr& edge) {
        return std::find(vec.begin(), vec.end(), edge) != vec.end();
    };

    std::size_t shown = 0;
    const auto maybePrintSample = [&](const std::string& msg) {
        if (shown < sampleLimit) {
            std::cout << "[diag] adjacency-mismatch " << msg << '\n';
            ++shown;
        }
    };

    // Forward membership: every active edge must appear in endpoint node lists.
    for (const auto& edge : edges) {
        if (!edge) {
            continue;
        }
        const NodePtr out = edge->getOutput();
        if (!out || !nodes.count(out) || !containsEdgePtr(out->getIncomingEdges(), edge)) {
            ++stats.missingOutIncomingMembership;
            maybePrintSample("edge missing from output.incoming: " + edge->toString());
        }

        for (const auto& in : edge->getInputs()) {
            if (!in || !nodes.count(in) || !containsEdgePtr(in->getOutgoingEdges(), edge)) {
                ++stats.missingInputOutgoingMembership;
                maybePrintSample("edge missing from input.outgoing: " + edge->toString());
            }
        }
    }

    // Reverse membership: every node-side edge in active view must reference back correctly.
    for (const auto& node : nodes) {
        if (!node) {
            continue;
        }
        for (const auto& inEdge : node->getIncomingEdges()) {
            if (!inEdge || !edges.count(inEdge)) {
                continue;
            }
            if (inEdge->getOutput() != node) {
                ++stats.malformedNodeIncomingMembership;
                maybePrintSample(
                        "node.incoming has wrong output: node=" + node->toString() +
                        " edge=" + inEdge->toString());
            }
        }
        for (const auto& outEdge : node->getOutgoingEdges()) {
            if (!outEdge || !edges.count(outEdge)) {
                continue;
            }
            const auto& ins = outEdge->getInputs();
            if (std::find(ins.begin(), ins.end(), node) == ins.end()) {
                ++stats.malformedNodeOutgoingMembership;
                maybePrintSample(
                        "node.outgoing missing node in inputs: node=" + node->toString() +
                        " edge=" + outEdge->toString());
            }
        }
    }

    return stats;
}

void reportProblematicSources(const SubgraphView& view, std::size_t limit = 30) {
    std::size_t count = 0;
    for (const auto& node : view.getNodes()) {
        if (!node || node->isFact) {
            continue;
        }
        const auto inEdges = view.getIncomingEdges(node);
        const auto outEdges = view.getOutgoingEdges(node);
        if (!inEdges.empty() || outEdges.empty()) {
            continue;
        }
        std::cout << "[diag] source-nonfact " << node->getTuple().toString()
                  << " in=0 out=" << outEdges.size()
                  << " prob=" << node->getProbability()
                  << '\n';
        std::size_t edgeShown = 0;
        for (const auto& out : outEdges) {
            if (!out || edgeShown >= 3) {
                break;
            }
            std::cout << "[diag]   out-edge " << out->toString() << '\n';
            ++edgeShown;
        }
        ++count;
        if (count >= limit) {
            break;
        }
    }
    if (count > 0) {
        std::cout << "[diag] source-nonfact-total=" << count << '\n';
    }
}

template <typename ManagerT, typename FormulaNodeRef>
void runBackend(
        const char* backend,
        SubgraphView& activeView,
        IncrementalDerivationGraph& graph,
        const std::vector<UntypedTuple>& queryTuples,
        bool enableDebugger,
        double& fcSec,
        double& evalSec,
        std::size_t* liveNodesOut = nullptr,
        double* reorderSecOut = nullptr,
        std::size_t* reorderCountOut = nullptr) {
    const auto fcStart = Clock::now();
    ManagerT manager;
    std::map<NodePtr, FormulaNodeRef> nodeFormulas;
    std::map<EdgePtr, FormulaNodeRef> edgeFormulas;
    if (enableDebugger) {
        debugger.startTurn("FULL");
        debugger.startStage(StageKind::FORWARD_COMPILATION_FULL);
    }
    buildFormulasCyclewise(activeView, manager, nodeFormulas, edgeFormulas);
    if (enableDebugger) {
        debugger.endStage();
        debugger.endTurn();
    }
    if (liveNodesOut) {
        *liveNodesOut = manager.getLiveNodeCount();
    }
    if (reorderSecOut || reorderCountOut) {
        if constexpr (std::is_same_v<ManagerT, WeightedBDDManager>) {
            DdManager* dd = manager.getManager();
            if (reorderSecOut) {
                *reorderSecOut = dd ? static_cast<double>(Cudd_ReadReorderingTime(dd)) / 1000.0 : 0.0;
            }
            if (reorderCountOut) {
                *reorderCountOut = dd ? static_cast<std::size_t>(Cudd_ReadReorderings(dd)) : 0;
            }
        } else {
            if (reorderSecOut) {
                *reorderSecOut = 0.0;
            }
            if (reorderCountOut) {
                *reorderCountOut = 0;
            }
        }
    }
    fcSec = elapsedSeconds(fcStart);

    const auto evalStart = Clock::now();
    std::cout << "[backend] " << backend << '\n';
    std::cout << std::setprecision(17);
    for (const auto& queryTuple : queryTuples) {
        NodePtr target = graph.findNode(queryTuple);
        auto it = nodeFormulas.find(target);
        if (it == nodeFormulas.end()) {
            if (auto precomputed = findPrecomputedProbability(graph, activeView, queryTuple)) {
                std::cout << "[result] " << queryTuple.toString() << " = " << *precomputed << '\n';
                continue;
            }
            target = findByTupleInView(activeView, queryTuple);
            if (!target) {
                throw std::runtime_error("Query tuple is outside active view: " + queryTuple.toString());
            }
            it = nodeFormulas.find(target);
            if (it == nodeFormulas.end()) {
                if (auto precomputed = findPrecomputedProbability(graph, activeView, queryTuple)) {
                    std::cout << "[result] " << queryTuple.toString() << " = " << *precomputed << '\n';
                    continue;
                }
                throw std::runtime_error("No formula built for query tuple: " + queryTuple.toString());
            }
        }

        const double prob = manager.computeWeightedModelCount(it->second);
        std::cout << "[result] " << queryTuple.toString() << " = " << prob << '\n';
    }
    evalSec = elapsedSeconds(evalStart);
}

void runBackendAmc(
        const char* backend,
        SubgraphView& activeView,
        IncrementalDerivationGraph& graph,
        const std::vector<UntypedTuple>& queryTuples,
        const Options& opt,
        double& fcSec,
        double& evalSec,
        AmcSummary* summaryOut = nullptr) {
    const auto totalStart = Clock::now();
    std::cout << "[backend] " << backend << '\n';
    std::cout << std::setprecision(17);

    AmcSummary summary;
    double approxSec = 0.0;

    for (const auto& queryTuple : queryTuples) {
        NodePtr target = graph.findNode(queryTuple);
        if (!target) {
            target = findByTupleInView(activeView, queryTuple);
        }
        if (!target) {
            if (auto precomputed = findPrecomputedProbability(graph, activeView, queryTuple)) {
                std::cout << "[result] " << queryTuple.toString() << " = " << *precomputed << '\n';
                ++summary.deterministicQueries;
                continue;
            }
            throw std::runtime_error("Query tuple is outside active view: " + queryTuple.toString());
        }
        if (auto precomputed = findPrecomputedProbability(graph, activeView, queryTuple)) {
            std::cout << "[result] " << queryTuple.toString() << " = " << *precomputed << '\n';
            ++summary.deterministicQueries;
            continue;
        }

        const auto localSliceStart = Clock::now();
        SubgraphView queryView = opt.fullGraph
                ? SubgraphView(activeView.getNodes(), activeView.getEdges())
                : buildBackwardSliceInView(activeView, {target});
        const double localSliceSec = elapsedSeconds(localSliceStart);
        AmcQueryResult res = evaluateQueryWithAmc(queryView, target, opt);

        fcSec += localSliceSec + res.extractSec + res.encodeSec + res.convertSec;
        evalSec += res.approxmcSec;
        approxSec += res.approxmcSec;
        if (res.usedApproxmc) {
            ++summary.approxQueries;
            summary.approxmcSec += res.approxmcSec;
        } else {
            ++summary.deterministicQueries;
        }
        summary.maxRandomVars = std::max(summary.maxRandomVars, res.randomVars);
        summary.maxWeightedVars = std::max(summary.maxWeightedVars, res.weightedVars);
        summary.maxWeightedClauses = std::max(summary.maxWeightedClauses, res.weightedClauses);
        summary.maxUnweightedVars = std::max(summary.maxUnweightedVars, res.unweightedVars);
        summary.maxUnweightedClauses = std::max(summary.maxUnweightedClauses, res.unweightedClauses);
        summary.maxProjectionVars = std::max(summary.maxProjectionVars, res.projectionVars);

        std::cout << "[amc-query] tuple=" << queryTuple.toString()
                  << " counter=" << res.counterKind
                  << " random_vars=" << res.randomVars
                  << " weighted_cnf_vars=" << res.weightedVars
                  << " weighted_cnf_clauses=" << res.weightedClauses
                  << " weighted_vars_before=" << res.weightedVarsBefore
                  << " weighted_vars_after=" << res.weightedVarsAfter
                  << " forced_assignments=" << res.forcedAssignments
                  << " unweighted_cnf_vars=" << res.unweightedVars
                  << " unweighted_cnf_clauses=" << res.unweightedClauses
                  << " projection_vars=" << res.projectionVars
                  << " added_vars=" << res.addedVars
                  << " added_clauses=" << res.addedClauses
                  << " tilt=" << res.tilt
                  << " tilt_violated=" << (res.tiltViolated ? 1 : 0)
                  << " max_quant_abs_err=" << res.maxQuantAbsError
                  << " max_quant_rel_err=" << res.maxQuantRelError
                  << " multiplier=" << res.multiplier
                  << " divide_exp=" << res.divideExp
                  << " cell=" << res.approxmcCellSolCount
                  << " hashes=" << res.approxmcHashCount
                  << " approxmc_s=" << res.approxmcSec
                  << '\n';
        std::cout << "[result] " << queryTuple.toString() << " = " << res.probability << '\n';
    }

    if (summaryOut) {
        *summaryOut = summary;
    }
    if (summary.approxQueries > 0 || summary.deterministicQueries > 0) {
        std::cout << std::fixed << std::setprecision(6)
                  << "[amc-stats] approx_queries=" << summary.approxQueries
                  << " deterministic_queries=" << summary.deterministicQueries
                  << " max_random_vars=" << summary.maxRandomVars
                  << " max_weighted_cnf_vars=" << summary.maxWeightedVars
                  << " max_weighted_cnf_clauses=" << summary.maxWeightedClauses
                  << " max_unweighted_cnf_vars=" << summary.maxUnweightedVars
                  << " max_unweighted_cnf_clauses=" << summary.maxUnweightedClauses
                  << " max_projection_vars=" << summary.maxProjectionVars
                  << " approxmc_runtime_s=" << approxSec
                  << '\n';
    }
    if (summary.approxQueries == 0 && summary.deterministicQueries == 0) {
        fcSec = elapsedSeconds(totalStart);
    }
}

void runBackendBddHybrid(
        const char* backend,
        SubgraphView& activeView,
        IncrementalDerivationGraph& graph,
        const std::vector<UntypedTuple>& queryTuples,
        bool enableDebugger,
        double& fcSec,
        double& evalSec,
        std::size_t& liveNodesOut,
        double* reorderSecOut = nullptr,
        std::size_t* reorderCountOut = nullptr) {
    struct SlowComponentEval {
        ComponentSubgraph comp;
        std::size_t randVars = 0;
    };

    const auto fcStart = Clock::now();
    if (enableDebugger) {
        debugger.startTurn("FULL");
        debugger.startStage(StageKind::FC_WMC_HYBRID_FULL);
    }

    auto components = buildComponentSubgraphs(activeView);
    auto analyses = analyzeComponentsStandalone(activeView, std::move(components));
    const bool enableFast = standaloneFastPathEnabled();

    std::map<NodePtr, double> probResult;
    std::vector<SlowComponentEval> slowEvals;
    slowEvals.reserve(analyses.size());
    std::size_t liveNodesSum = 0;

    for (auto& analysis : analyses) {
        const bool singleCandidate = analysis.randVars == 1;
        const bool conjCandidate =
                !singleCandidate && !analysis.hasNegation && !analysis.hasOr && !analysis.hasCycle;

        if (enableFast && singleCandidate) {
            std::unordered_map<NodePtr, bool> valuesTrue;
            std::unordered_map<NodePtr, bool> valuesFalse;
            if (evaluateSingleRandComponent(analysis.comp, analysis.singleRand, true, valuesTrue) &&
                    evaluateSingleRandComponent(analysis.comp, analysis.singleRand, false, valuesFalse)) {
                const double p = analysis.singleRand.probability;
                for (const auto& node : analysis.comp.nodes) {
                    const auto itTrue = valuesTrue.find(node);
                    const auto itFalse = valuesFalse.find(node);
                    const bool vTrue = itTrue != valuesTrue.end() && itTrue->second;
                    const bool vFalse = itFalse != valuesFalse.end() && itFalse->second;
                    if (!vTrue && !vFalse) {
                        continue;
                    }
                    probResult[node] = (vTrue ? p : 0.0) + (vFalse ? (1.0 - p) : 0.0);
                }
                std::cout << "[fc-component] id=" << analysis.comp.id
                          << " fast_path=single"
                          << " nodes=" << analysis.comp.nodes.size()
                          << " edges=" << analysis.comp.edges.size()
                          << " rand_vars=" << analysis.randVars
                          << '\n';
                continue;
            }
        }

        if (enableFast && conjCandidate) {
            std::unordered_map<NodePtr, double> nodeProbs;
            if (evaluateConjComponent(analysis.comp, nodeProbs)) {
                for (const auto& [node, prob] : nodeProbs) {
                    probResult[node] = prob;
                }
                std::cout << "[fc-component] id=" << analysis.comp.id
                          << " fast_path=conj"
                          << " nodes=" << analysis.comp.nodes.size()
                          << " edges=" << analysis.comp.edges.size()
                          << " rand_vars=" << analysis.randVars
                          << '\n';
                continue;
            }
        }

        std::cout << "[fc-component] id=" << analysis.comp.id
                  << " fast_path=0"
                  << " nodes=" << analysis.comp.nodes.size()
                  << " edges=" << analysis.comp.edges.size()
                  << " rand_vars=" << analysis.randVars
                  << '\n';
        slowEvals.push_back(SlowComponentEval{std::move(analysis.comp), analysis.randVars});
    }

    std::sort(slowEvals.begin(), slowEvals.end(),
            [](const SlowComponentEval& a, const SlowComponentEval& b) {
                return a.randVars > b.randVars;
            });

    if (!slowEvals.empty()) {
        std::size_t maxRandVars = 0;
        for (const auto& slow : slowEvals) {
            maxRandVars = std::max(maxRandVars, slow.randVars);
        }
        auto initConfig = makeCuddInitConfig(maxRandVars);
        WeightedBDDManager manager(initConfig);

        for (auto& slow : slowEvals) {
            manager.reset();
            std::map<NodePtr, BddNodeRef> compNodeFormulas;
            std::map<EdgePtr, BddNodeRef> compEdgeFormulas;
            SubgraphView subview(std::move(slow.comp.nodes), std::move(slow.comp.edges));
            buildFormulasCyclewise(subview, manager, compNodeFormulas, compEdgeFormulas);
            liveNodesSum += manager.getLiveNodeCount();
            for (const auto& node : subview.getNodes()) {
                auto it = compNodeFormulas.find(node);
                if (it == compNodeFormulas.end()) {
                    continue;
                }
                probResult[node] = manager.computeWeightedModelCount(it->second);
            }
        }

        DdManager* dd = manager.getManager();
        if (reorderSecOut) {
            *reorderSecOut = dd ? static_cast<double>(Cudd_ReadReorderingTime(dd)) / 1000.0 : 0.0;
        }
        if (reorderCountOut) {
            *reorderCountOut = dd ? static_cast<std::size_t>(Cudd_ReadReorderings(dd)) : 0;
        }
    } else {
        if (reorderSecOut) {
            *reorderSecOut = 0.0;
        }
        if (reorderCountOut) {
            *reorderCountOut = 0;
        }
    }

    for (const auto& [node, prob] : precomputedProbResult) {
        if (node) {
            probResult.emplace(node, prob);
        }
    }

    if (enableDebugger) {
        debugger.endStage();
        debugger.endTurn();
    }
    liveNodesOut = liveNodesSum;
    fcSec = elapsedSeconds(fcStart);

    const auto evalStart = Clock::now();
    std::cout << "[backend] " << backend << '\n';
    std::cout << std::setprecision(17);
    for (const auto& queryTuple : queryTuples) {
        NodePtr target = graph.findNode(queryTuple);
        auto it = probResult.find(target);
        if (it == probResult.end()) {
            if (auto precomputed = findPrecomputedProbability(graph, activeView, queryTuple)) {
                std::cout << "[result] " << queryTuple.toString() << " = " << *precomputed << '\n';
                continue;
            }
            target = findByTupleInView(activeView, queryTuple);
            if (!target) {
                throw std::runtime_error("Query tuple is outside active view: " + queryTuple.toString());
            }
            it = probResult.find(target);
            if (it == probResult.end()) {
                if (auto precomputed = findPrecomputedProbability(graph, activeView, queryTuple)) {
                    std::cout << "[result] " << queryTuple.toString() << " = " << *precomputed << '\n';
                    continue;
                }
                throw std::runtime_error("No probability computed for query tuple: " + queryTuple.toString());
            }
        }
        std::cout << "[result] " << queryTuple.toString() << " = " << it->second << '\n';
    }
    evalSec = elapsedSeconds(evalStart);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options opt = parseArgs(argc, argv);
#ifndef SOUFFLE_STANDALONE_HAS_APPROXMC_LIB
        if (opt.backend == Backend::Amc && !fileExists(opt.approxmcBin)) {
            throw std::runtime_error(
                    "ApproxMC binary not found: " + opt.approxmcBin +
                    " (set APPROXMC_BIN or pass --approxmc-bin)");
        }
#endif
        if (envTruthy(std::getenv("SOUFFLE_STANDALONE_DUMP_JSON"))) {
            DerivationGraphViewInterface::setDumpJsonEnabled(true);
        }
        if (envTruthy(std::getenv("SOUFFLE_STANDALONE_DUMP_DOT"))) {
            DerivationGraphViewInterface::setDumpDotEnabled(true);
        }
        if (envTruthy(std::getenv("SOUFFLE_STANDALONE_DUMP_STATS"))) {
            DerivationGraphViewInterface::setDumpStatsEnabled(true);
        }
        if (const char* dumpDir = std::getenv("SOUFFLE_STANDALONE_DUMP_DIR")) {
            const std::string dir = trim(dumpDir);
            if (!dir.empty()) {
                DerivationGraphViewInterface::setDumpOutputDir(dir);
            }
        }

        const auto totalStart = Clock::now();

        const auto loadStart = Clock::now();
        auto graph = std::unique_ptr<IncrementalDerivationGraph>(
                IncrementalDerivationGraph::loadFromJsonInc(opt.jsonPath));
        const double loadSec = elapsedSeconds(loadStart);

        if (opt.listQueries) {
            std::vector<std::string> allTuples;
            allTuples.reserve(graph->getNodes().size());
            for (const auto& node : graph->getNodes()) {
                if (node) {
                    allTuples.push_back(node->getTuple().toString());
                }
            }
            std::sort(allTuples.begin(), allTuples.end());
            for (const auto& tuple : allTuples) {
                std::cout << tuple << '\n';
            }
            return 0;
        }

        const auto expandStart = Clock::now();
        const std::vector<UntypedTuple> queryTuples = resolveQueryTuples(
                *graph, opt.querySpecs, opt.queryAllRelations);
        const double expandSec = elapsedSeconds(expandStart);

        if (queryTuples.empty()) {
            std::cerr << "[warning] No query tuple resolved; nothing to evaluate." << std::endl;
            std::cout << std::fixed << std::setprecision(6)
                      << "[summary] nodes=0 edges=0 queries=0" << std::endl;
            std::cout << std::fixed << std::setprecision(6)
                      << "[timing_s] load=" << loadSec
                      << " expand=" << expandSec
                      << " resolve=0.000000"
                      << " slice=0.000000"
                      << " rewrite=0.000000"
                      << " fc=0.000000"
                      << " eval=0.000000"
                      << " total=" << elapsedSeconds(totalStart)
                      << std::endl;
            return 0;
        }

        const auto resolveStart = Clock::now();
        const std::vector<NodePtr> queryNodes = resolveQueryNodes(*graph, queryTuples);
        const double resolveSec = elapsedSeconds(resolveStart);

        // Match pipeline semantics: protect query tuples from rewrite-side
        // simplification/removal and keep them in output accounting.
        for (const auto& node : queryNodes) {
            if (node) {
                node->setQuery();
            }
        }

        const auto sliceStart = Clock::now();
        SubgraphView queryView = opt.fullGraph
                ? SubgraphView(graph->getNodes(), graph->getEdges())
                : buildBackwardSlice(queryNodes);
        const double sliceSec = elapsedSeconds(sliceStart);
        if (DerivationGraphViewInterface::isDumpJsonEnabled()) {
            queryView.dumpJson("standalone_query_view_before_rewrite.json");
        }

        if (opt.rewrite && !envTruthy(std::getenv("SOUFFLE_STANDALONE_SKIP_MARK_ALL_OUTPUTS"))) {
            // JSON snapshots do not serialize pipeline output metadata.
            // Preserve all nodes in the active view as outputs so rewrite
            // cannot drop semantics needed by downstream queries.
            for (const auto& node : queryView.getNodes()) {
                if (!node) {
                    continue;
                }
                node->setQuery();
            }
        } else if (opt.rewrite) {
            std::cout << "[rewrite-warning] skip mark-all-outputs enabled; "
                         "rewrite now relies only on explicit query tuples as outputs."
                      << '\n';
        }

        std::unique_ptr<IncSubgraphView> rewrittenView;
        SubgraphView* activeView = &queryView;
        double rewriteSec = 0.0;

        if (opt.rewrite) {
            const auto rewriteStart = Clock::now();
            rewrittenView = std::make_unique<IncSubgraphView>(
                    queryView.getNodes(),
                    queryView.getEdges(),
                    std::set<NodePtr>{},
                    std::set<EdgePtr>{},
                    std::set<NodePtr>{},
                    std::set<EdgePtr>{});

            souffle::problog::GraphRewriter rewriter;
            auto rewriteFlags = buildStandaloneRewriteFlags(opt);
            const auto safety = enforceStandaloneRewriteSafety(rewriteFlags, queryView);
            if (safety.hasCycles && (safety.disabledSingle || safety.disabledCompaction)) {
                std::cout << "[rewrite-warning] detected non-trivial cycles with allfacts enabled; "
                             "disabled";
                if (safety.disabledSingle) {
                    std::cout << " single";
                }
                if (safety.disabledCompaction) {
                    if (safety.disabledSingle) {
                        std::cout << " and";
                    }
                    std::cout << " compaction";
                }
                std::cout << " to avoid unresolved SCC stalls in standalone JSON replay; "
                             "set SOUFFLE_STANDALONE_ALLOW_RISKY_FACT_ABSORB_IN_CYCLES=1 "
                             "(or SOUFFLE_STANDALONE_ALLOW_RISKY_SINGLE_ALLFACTS=1) to force."
                          << '\n';
            }
            const auto stats = rewriter.rewriteUntilFixpoint(*graph, *rewrittenView, false, rewriteFlags);
            rewriteSec = elapsedSeconds(rewriteStart);
            activeView = rewrittenView.get();

            std::cout << "[rewrite] iterations=" << stats.numIterations
                      << " regions=" << stats.numRegionsRewritten
                      << " nodes_removed=" << stats.numNodesRemoved
                      << " edges_removed=" << stats.numEdgesRemoved
                      << " edges_added=" << stats.numEdgesAdded
                      << '\n';
            if (DerivationGraphViewInterface::isDumpJsonEnabled()) {
                activeView->dumpJson("standalone_query_view_after_rewrite.json");
            }
        }

        const auto sanitizeStats = sanitizeView(*activeView);
        if (sanitizeStats.removedNullNodes || sanitizeStats.removedNullEdges ||
                sanitizeStats.removedDanglingEdges) {
            std::cout << "[sanitize] removed_null_nodes=" << sanitizeStats.removedNullNodes
                      << " removed_null_edges=" << sanitizeStats.removedNullEdges
                      << " removed_dangling_edges=" << sanitizeStats.removedDanglingEdges
                      << '\n';
        }
        const auto consistencyStats = checkViewAdjacencyConsistency(*activeView);
        if (consistencyStats.missingOutIncomingMembership ||
                consistencyStats.missingInputOutgoingMembership ||
                consistencyStats.malformedNodeIncomingMembership ||
                consistencyStats.malformedNodeOutgoingMembership) {
            std::cout << "[diag] adjacency-consistency"
                      << " missing_out_incoming=" << consistencyStats.missingOutIncomingMembership
                      << " missing_input_outgoing=" << consistencyStats.missingInputOutgoingMembership
                      << " malformed_node_incoming=" << consistencyStats.malformedNodeIncomingMembership
                      << " malformed_node_outgoing=" << consistencyStats.malformedNodeOutgoingMembership
                      << '\n';
        }
        if (opt.rewrite) {
            reportProblematicSources(*activeView);
        }

        double fcSec = 0.0;
        double evalSec = 0.0;
        std::size_t bddLiveNodes = 0;
        double bddReorderSec = 0.0;
        std::size_t bddReorderCount = 0;
        AmcSummary amcSummary;
        const bool enableDebugger = envTruthy(std::getenv("SOUFFLE_STANDALONE_USE_DEBUGGER"));
        if (opt.backend == Backend::Bdd) {
            if (opt.rewrite) {
                runBackendBddHybrid(
                        backendName(opt.backend), *activeView, *graph, queryTuples, enableDebugger, fcSec,
                        evalSec, bddLiveNodes, &bddReorderSec, &bddReorderCount);
            } else {
                runBackend<WeightedBDDManager, BddNodeRef>(
                        backendName(opt.backend), *activeView, *graph, queryTuples, enableDebugger, fcSec,
                        evalSec, &bddLiveNodes, &bddReorderSec, &bddReorderCount);
            }
        } else if (opt.backend == Backend::Amc) {
            runBackendAmc(
                    backendName(opt.backend), *activeView, *graph, queryTuples, opt, fcSec, evalSec,
                    &amcSummary);
        } else {
#ifdef SOUFFLE_STANDALONE_HAS_SDD
            runBackend<SddFormulaManager, SddNodeRef>(
                    backendName(opt.backend), *activeView, *graph, queryTuples, enableDebugger, fcSec, evalSec);
#else
            throw std::runtime_error("backend=sdd requested, but this binary was built without SDD support");
#endif
        }
        if (enableDebugger) {
            std::cout << "[debugger-report-begin]\n";
            debugger.printReport(std::cout);
            std::cout << "[debugger-report-end]\n";
        }

        std::cout << std::fixed << std::setprecision(6)
                  << "[summary] nodes=" << activeView->getNodes().size()
                  << " edges=" << activeView->getEdges().size()
                  << " queries=" << queryTuples.size()
                  << '\n';
        if (opt.backend == Backend::Bdd) {
            std::cout << "[bdd-stats] live_nodes=" << bddLiveNodes << '\n';
            std::cout << std::fixed << std::setprecision(6)
                      << "[bdd-stats] reordering_runtime_s=" << bddReorderSec << '\n';
            std::cout << "[bdd-stats] reorderings=" << bddReorderCount << '\n';
        } else if (opt.backend == Backend::Amc) {
            std::cout << std::fixed << std::setprecision(6)
                      << "[amc-config] counter="
#ifdef SOUFFLE_STANDALONE_HAS_APPROXMC_LIB
                      << "lib"
#else
                      << "cli"
#endif
                      << " "
                      << "epsilon=" << opt.epsilon
                      << " delta=" << opt.delta
                      << " precision=" << opt.amcPrecision
                      << " preprocess=" << (opt.amcPreprocess ? 1 : 0)
                      << " approx_queries=" << amcSummary.approxQueries
                      << " deterministic_queries=" << amcSummary.deterministicQueries
                      << '\n';
        }
        std::cout << std::fixed << std::setprecision(6)
                  << "[timing_s] load=" << loadSec
                  << " expand=" << expandSec
                  << " resolve=" << resolveSec
                  << " slice=" << sliceSec
                  << " rewrite=" << rewriteSec
                  << " fc=" << fcSec
                  << " eval=" << evalSec
                  << " total=" << elapsedSeconds(totalStart)
                  << '\n';

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << std::endl;
        return 1;
    }
}
