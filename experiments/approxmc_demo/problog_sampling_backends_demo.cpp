#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <optional>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

constexpr std::size_t kMaxVars = 10;

enum class Backend {
    All,
    World,
    Formula,
    Rejection,
    Likelihood,
    Importance,
    Particle,
    Mh,
    Dnf,
};

enum class ExampleId {
    All,
    Easy,
    Medium,
    RareEvidence,
    DenseOverlap,
};

struct Options {
    Backend backend = Backend::All;
    ExampleId example = ExampleId::All;
    uint64_t samples = 0;
    uint64_t acceptedSamples = 0;
    uint64_t particles = 0;
    uint64_t mhIterations = 0;
    uint64_t mhBurnin = 0;
    uint64_t mhThin = 0;
    double tolerance = -1.0;
    uint64_t seed = 7;
};

struct VariableInfo {
    const char* name = "";
    long double probTrue = 0.5L;
    long double proposalTrue = 0.5L;
};

struct Lit {
    std::size_t var = 0;
    bool value = false;
};

using Term = std::vector<Lit>;
using Assignment = std::array<uint8_t, kMaxVars>;

bool operator==(const Lit& lhs, const Lit& rhs) {
    return lhs.var == rhs.var && lhs.value == rhs.value;
}

bool operator<(const Lit& lhs, const Lit& rhs) {
    if (lhs.var != rhs.var) return lhs.var < rhs.var;
    return lhs.value < rhs.value;
}

struct ExampleBudget {
    uint64_t samples = 0;
    uint64_t acceptedSamples = 0;
    uint64_t particles = 0;
    uint64_t mhIterations = 0;
    uint64_t mhBurnin = 0;
    uint64_t mhThin = 1;
    double tolerance = 0.03;
};

struct DemoModel {
    std::string key;
    std::string title;
    std::string queryText;
    std::string evidenceText;
    std::size_t activeVars = 0;
    std::array<VariableInfo, kMaxVars> vars{};
    std::vector<Term> queryDnf;
    std::vector<Term> evidenceDnf;
    ExampleBudget budget;
};

struct ExactStats {
    long double queryProb = 0.0L;
    long double evidenceProb = 0.0L;
    long double jointProb = 0.0L;
    long double conditionalProb = 0.0L;
};

struct RunResult {
    std::string backend;
    long double estimate = 0.0L;
    long double absError = 0.0L;
    double runtimeSec = 0.0;
    std::string note;
};

struct Particle {
    Assignment asg{};
    long double weight = 1.0L;
};

Backend parseBackend(const std::string& text) {
    if (text == "all") return Backend::All;
    if (text == "world") return Backend::World;
    if (text == "formula") return Backend::Formula;
    if (text == "rejection") return Backend::Rejection;
    if (text == "likelihood") return Backend::Likelihood;
    if (text == "importance") return Backend::Importance;
    if (text == "particle") return Backend::Particle;
    if (text == "mh") return Backend::Mh;
    if (text == "dnf") return Backend::Dnf;
    throw std::runtime_error(
            "Invalid backend: " + text +
            " (expected all|world|formula|rejection|likelihood|importance|particle|mh|dnf)");
}

ExampleId parseExample(const std::string& text) {
    if (text == "all") return ExampleId::All;
    if (text == "easy") return ExampleId::Easy;
    if (text == "medium") return ExampleId::Medium;
    if (text == "rare") return ExampleId::RareEvidence;
    if (text == "dense") return ExampleId::DenseOverlap;
    throw std::runtime_error("Invalid example: " + text + " (expected all|easy|medium|rare|dense)");
}

Options parseArgs(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto needValue = [&](const char* flag) -> std::string {
            if (i + 1 >= argc) {
                throw std::runtime_error(std::string("Missing value after ") + flag);
            }
            return argv[++i];
        };
        if (arg == "--backend") {
            opt.backend = parseBackend(needValue("--backend"));
        } else if (arg == "--example") {
            opt.example = parseExample(needValue("--example"));
        } else if (arg == "--samples") {
            opt.samples = std::stoull(needValue("--samples"));
        } else if (arg == "--accepted-samples") {
            opt.acceptedSamples = std::stoull(needValue("--accepted-samples"));
        } else if (arg == "--particles") {
            opt.particles = std::stoull(needValue("--particles"));
        } else if (arg == "--mh-iterations") {
            opt.mhIterations = std::stoull(needValue("--mh-iterations"));
        } else if (arg == "--mh-burnin") {
            opt.mhBurnin = std::stoull(needValue("--mh-burnin"));
        } else if (arg == "--mh-thin") {
            opt.mhThin = std::stoull(needValue("--mh-thin"));
        } else if (arg == "--seed") {
            opt.seed = std::stoull(needValue("--seed"));
        } else if (arg == "--tolerance") {
            opt.tolerance = std::stod(needValue("--tolerance"));
        } else if (arg == "-h" || arg == "--help") {
            std::cout
                    << "Usage: " << argv[0]
                    << " [--example all|easy|medium|rare|dense]"
                    << " [--backend all|world|formula|rejection|likelihood|importance|particle|mh|dnf]"
                    << " [--samples N] [--accepted-samples N] [--particles N]"
                    << " [--mh-iterations N] [--mh-burnin N] [--mh-thin N]"
                    << " [--seed N] [--tolerance X]\n";
            std::exit(0);
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }
    if (opt.samples == 0 && opt.acceptedSamples != 0) {
        throw std::runtime_error("Invalid option combination");
    }
    return opt;
}

double elapsedSec(const Clock::time_point t0) {
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

std::optional<Term> normalizeTerm(Term term) {
    std::sort(term.begin(), term.end());
    Term out;
    out.reserve(term.size());
    for (const auto& lit : term) {
        if (!out.empty() && out.back().var == lit.var) {
            if (out.back().value != lit.value) {
                return std::nullopt;
            }
            continue;
        }
        out.push_back(lit);
    }
    return out;
}

bool evalTerm(const Term& term, const Assignment& asg) {
    for (const auto& lit : term) {
        if (static_cast<bool>(asg[lit.var]) != lit.value) {
            return false;
        }
    }
    return true;
}

bool evalDnf(const std::vector<Term>& dnf, const Assignment& asg) {
    for (const auto& term : dnf) {
        if (evalTerm(term, asg)) {
            return true;
        }
    }
    return false;
}

bool isSubsetTerm(const Term& small, const Term& big) {
    if (small.size() > big.size()) return false;
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < small.size() && j < big.size()) {
        if (small[i] == big[j]) {
            ++i;
            ++j;
        } else if (small[i] < big[j]) {
            return false;
        } else {
            ++j;
        }
    }
    return i == small.size();
}

std::vector<Term> simplifyDnf(const std::vector<Term>& input) {
    std::vector<Term> normalized;
    normalized.reserve(input.size());
    for (auto term : input) {
        auto norm = normalizeTerm(std::move(term));
        if (norm) {
            normalized.push_back(std::move(*norm));
        }
    }
    std::sort(normalized.begin(), normalized.end(), [](const Term& lhs, const Term& rhs) {
        if (lhs.size() != rhs.size()) return lhs.size() < rhs.size();
        return lhs < rhs;
    });
    normalized.erase(std::unique(normalized.begin(), normalized.end()), normalized.end());

    std::vector<Term> out;
    for (const auto& term : normalized) {
        bool subsumed = false;
        for (const auto& keep : out) {
            if (isSubsetTerm(keep, term)) {
                subsumed = true;
                break;
            }
        }
        if (!subsumed) {
            out.push_back(term);
        }
    }
    return out;
}

std::vector<Term> conjoinDnf(const std::vector<Term>& lhs, const std::vector<Term>& rhs) {
    std::vector<Term> out;
    out.reserve(lhs.size() * rhs.size());
    for (const auto& a : lhs) {
        for (const auto& b : rhs) {
            Term merged = a;
            merged.insert(merged.end(), b.begin(), b.end());
            auto norm = normalizeTerm(std::move(merged));
            if (norm) {
                out.push_back(std::move(*norm));
            }
        }
    }
    return simplifyDnf(out);
}

long double assignmentProb(const DemoModel& model, const Assignment& asg) {
    long double out = 1.0L;
    for (std::size_t i = 0; i < model.activeVars; ++i) {
        const long double p = model.vars[i].probTrue;
        out *= asg[i] ? p : (1.0L - p);
    }
    return out;
}

std::vector<std::size_t> activeVariables(const DemoModel& model) {
    std::vector<std::size_t> vars;
    vars.reserve(model.activeVars);
    for (std::size_t i = 0; i < model.activeVars; ++i) {
        vars.push_back(i);
    }
    return vars;
}

std::vector<std::size_t> formulaSupport(const DemoModel& model, const std::vector<Term>& lhs, const std::vector<Term>& rhs) {
    std::vector<bool> used(model.activeVars, false);
    for (const auto& dnf : {lhs, rhs}) {
        for (const auto& term : dnf) {
            for (const auto& lit : term) {
                used[lit.var] = true;
            }
        }
    }
    std::vector<std::size_t> vars;
    for (std::size_t i = 0; i < model.activeVars; ++i) {
        if (used[i]) vars.push_back(i);
    }
    return vars;
}

std::optional<bool> observedValueFor(const DemoModel& model, std::size_t var) {
    if (model.evidenceDnf.size() != 1) {
        return std::nullopt;
    }
    for (const auto& lit : model.evidenceDnf.front()) {
        if (lit.var == var) return lit.value;
    }
    return std::nullopt;
}

Assignment sampleAssignment(
        const DemoModel& model, std::mt19937_64& rng, const std::vector<std::size_t>& vars, bool useProposal) {
    Assignment asg{};
    asg.fill(0U);
    for (std::size_t var : vars) {
        const long double p = useProposal ? model.vars[var].proposalTrue : model.vars[var].probTrue;
        std::bernoulli_distribution dist(static_cast<double>(p));
        asg[var] = static_cast<uint8_t>(dist(rng));
    }
    return asg;
}

ExactStats computeExactStats(const DemoModel& model) {
    ExactStats stats;
    Assignment asg{};
    const uint64_t total = 1ULL << model.activeVars;
    for (uint64_t mask = 0; mask < total; ++mask) {
        for (std::size_t i = 0; i < model.activeVars; ++i) {
            asg[i] = static_cast<uint8_t>((mask >> i) & 1ULL);
        }
        const long double p = assignmentProb(model, asg);
        const bool q = evalDnf(model.queryDnf, asg);
        const bool e = evalDnf(model.evidenceDnf, asg);
        if (q) stats.queryProb += p;
        if (e) stats.evidenceProb += p;
        if (q && e) stats.jointProb += p;
    }
    if (stats.evidenceProb > 0.0L) {
        stats.conditionalProb = stats.jointProb / stats.evidenceProb;
    }
    return stats;
}

long double dnfMassUpperBound(const DemoModel& model, const std::vector<Term>& dnf) {
    long double sum = 0.0L;
    for (const auto& term : simplifyDnf(dnf)) {
        long double p = 1.0L;
        for (const auto& lit : term) {
            const long double base = model.vars[lit.var].probTrue;
            p *= lit.value ? base : (1.0L - base);
        }
        sum += p;
    }
    return sum;
}

DemoModel makeEasyExample() {
    DemoModel model;
    model.key = "easy";
    model.title = "Easy overlap, one evidence literal";
    model.queryText = "(a & b) | (a & d) | (c & d)";
    model.evidenceText = "c";
    model.activeVars = 5;
    model.vars = {{
            {"a", 0.55L, 0.72L},
            {"b", 0.35L, 0.50L},
            {"c", 0.25L, 0.25L},
            {"d", 0.20L, 0.33L},
            {"noise", 0.10L, 0.10L},
            {},
            {},
            {},
            {},
            {},
    }};
    model.queryDnf = {
            {{0, true}, {1, true}},
            {{0, true}, {3, true}},
            {{2, true}, {3, true}},
    };
    model.evidenceDnf = {
            {{2, true}},
    };
    model.budget = {200000, 100000, 30000, 500000, 20000, 5, 0.03};
    return model;
}

DemoModel makeMediumExample() {
    DemoModel model;
    model.key = "medium";
    model.title = "More overlap, two evidence literals";
    model.queryText = "(a & b) | (a & d) | (b & e) | (c & d & g) | (c & f) | (a & c & e)";
    model.evidenceText = "c & g";
    model.activeVars = 7;
    model.vars = {{
            {"a", 0.58L, 0.72L},
            {"b", 0.33L, 0.48L},
            {"c", 0.40L, 0.40L},
            {"d", 0.22L, 0.34L},
            {"e", 0.27L, 0.39L},
            {"f", 0.18L, 0.28L},
            {"g", 0.45L, 0.45L},
            {},
            {},
            {},
    }};
    model.queryDnf = {
            {{0, true}, {1, true}},
            {{0, true}, {3, true}},
            {{1, true}, {4, true}},
            {{2, true}, {3, true}, {6, true}},
            {{2, true}, {5, true}},
            {{0, true}, {2, true}, {4, true}},
    };
    model.evidenceDnf = {
            {{2, true}, {6, true}},
    };
    model.budget = {250000, 80000, 40000, 700000, 25000, 5, 0.035};
    return model;
}

DemoModel makeRareEvidenceExample() {
    DemoModel model;
    model.key = "rare";
    model.title = "Rarer evidence, still manageable within seconds";
    model.queryText = "(a & b & c) | (a & d & g) | (b & e & h) | (c & f) | (d & e & g) | (a & h)";
    model.evidenceText = "c & g & h";
    model.activeVars = 8;
    model.vars = {{
            {"a", 0.52L, 0.68L},
            {"b", 0.37L, 0.52L},
            {"c", 0.45L, 0.45L},
            {"d", 0.21L, 0.33L},
            {"e", 0.24L, 0.36L},
            {"f", 0.17L, 0.26L},
            {"g", 0.40L, 0.40L},
            {"h", 0.35L, 0.35L},
            {},
            {},
    }};
    model.queryDnf = {
            {{0, true}, {1, true}, {2, true}},
            {{0, true}, {3, true}, {6, true}},
            {{1, true}, {4, true}, {7, true}},
            {{2, true}, {5, true}},
            {{3, true}, {4, true}, {6, true}},
            {{0, true}, {7, true}},
    };
    model.evidenceDnf = {
            {{2, true}, {6, true}, {7, true}},
    };
    model.budget = {400000, 25000, 45000, 900000, 30000, 5, 0.045};
    return model;
}

DemoModel makeDenseOverlapExample() {
    DemoModel model;
    model.key = "dense";
    model.title = "Dense proof overlap, larger support";
    model.queryText =
            "(a & b) | (a & d) | (a & e) | (b & c & f) | (c & d & g) | (e & f & h) | (a & f & h) | (b & d & i) | (c & e & i)";
    model.evidenceText = "c & h";
    model.activeVars = 9;
    model.vars = {{
            {"a", 0.51L, 0.69L},
            {"b", 0.34L, 0.50L},
            {"c", 0.42L, 0.42L},
            {"d", 0.26L, 0.38L},
            {"e", 0.29L, 0.41L},
            {"f", 0.23L, 0.31L},
            {"g", 0.18L, 0.27L},
            {"h", 0.33L, 0.33L},
            {"i", 0.21L, 0.30L},
            {},
    }};
    model.queryDnf = {
            {{0, true}, {1, true}},
            {{0, true}, {3, true}},
            {{0, true}, {4, true}},
            {{1, true}, {2, true}, {5, true}},
            {{2, true}, {3, true}, {6, true}},
            {{4, true}, {5, true}, {7, true}},
            {{0, true}, {5, true}, {7, true}},
            {{1, true}, {3, true}, {8, true}},
            {{2, true}, {4, true}, {8, true}},
    };
    model.evidenceDnf = {
            {{2, true}, {7, true}},
    };
    model.budget = {500000, 70000, 50000, 1200000, 40000, 5, 0.05};
    return model;
}

std::vector<DemoModel> allExamples() {
    return {
            makeEasyExample(),
            makeMediumExample(),
            makeRareEvidenceExample(),
            makeDenseOverlapExample(),
    };
}

Options resolveOptionsForExample(const Options& user, const DemoModel& model, std::size_t exampleIndex) {
    Options out = user;
    out.samples = user.samples ? user.samples : model.budget.samples;
    out.acceptedSamples = user.acceptedSamples ? user.acceptedSamples : model.budget.acceptedSamples;
    out.particles = user.particles ? user.particles : model.budget.particles;
    out.mhIterations = user.mhIterations ? user.mhIterations : model.budget.mhIterations;
    out.mhBurnin = user.mhBurnin ? user.mhBurnin : model.budget.mhBurnin;
    out.mhThin = user.mhThin ? user.mhThin : model.budget.mhThin;
    out.tolerance = user.tolerance >= 0.0 ? user.tolerance : model.budget.tolerance;
    out.seed = user.seed + static_cast<uint64_t>(1000 * exampleIndex);
    return out;
}

RunResult runWorldSampling(const DemoModel& model, const ExactStats& exact, const Options& opt) {
    RunResult out;
    out.backend = "world";
    std::mt19937_64 rng(opt.seed + 11);
    const auto start = Clock::now();
    uint64_t evidenceHits = 0;
    uint64_t jointHits = 0;
    const auto allVars = activeVariables(model);
    for (uint64_t i = 0; i < opt.samples; ++i) {
        Assignment asg = sampleAssignment(model, rng, allVars, false);
        const bool e = evalDnf(model.evidenceDnf, asg);
        if (e) {
            ++evidenceHits;
            if (evalDnf(model.queryDnf, asg)) {
                ++jointHits;
            }
        }
    }
    out.runtimeSec = elapsedSec(start);
    out.estimate = evidenceHits ? static_cast<long double>(jointHits) / static_cast<long double>(evidenceHits) : 0.0L;
    out.absError = std::fabs(out.estimate - exact.conditionalProb);
    std::ostringstream note;
    note << "samples=" << opt.samples << " accepted=" << evidenceHits << " sampled_vars=" << allVars.size();
    out.note = note.str();
    return out;
}

RunResult runFormulaSampling(const DemoModel& model, const ExactStats& exact, const Options& opt) {
    RunResult out;
    out.backend = "formula";
    std::mt19937_64 rng(opt.seed + 23);
    const auto start = Clock::now();
    uint64_t evidenceHits = 0;
    uint64_t jointHits = 0;
    const auto support = formulaSupport(model, model.queryDnf, model.evidenceDnf);
    for (uint64_t i = 0; i < opt.samples; ++i) {
        Assignment asg = sampleAssignment(model, rng, support, false);
        const bool e = evalDnf(model.evidenceDnf, asg);
        if (e) {
            ++evidenceHits;
            if (evalDnf(model.queryDnf, asg)) {
                ++jointHits;
            }
        }
    }
    out.runtimeSec = elapsedSec(start);
    out.estimate = evidenceHits ? static_cast<long double>(jointHits) / static_cast<long double>(evidenceHits) : 0.0L;
    out.absError = std::fabs(out.estimate - exact.conditionalProb);
    std::ostringstream note;
    note << "samples=" << opt.samples << " accepted=" << evidenceHits << " sampled_vars=" << support.size();
    out.note = note.str();
    return out;
}

RunResult runRejectionSampling(const DemoModel& model, const ExactStats& exact, const Options& opt) {
    RunResult out;
    out.backend = "rejection";
    std::mt19937_64 rng(opt.seed + 37);
    const auto start = Clock::now();
    uint64_t attempts = 0;
    uint64_t accepted = 0;
    uint64_t queryHits = 0;
    const auto allVars = activeVariables(model);
    while (accepted < opt.acceptedSamples) {
        ++attempts;
        Assignment asg = sampleAssignment(model, rng, allVars, false);
        if (!evalDnf(model.evidenceDnf, asg)) continue;
        ++accepted;
        if (evalDnf(model.queryDnf, asg)) {
            ++queryHits;
        }
    }
    out.runtimeSec = elapsedSec(start);
    out.estimate = static_cast<long double>(queryHits) / static_cast<long double>(accepted);
    out.absError = std::fabs(out.estimate - exact.conditionalProb);
    std::ostringstream note;
    note << "accepted=" << accepted << " attempts=" << attempts;
    out.note = note.str();
    return out;
}

RunResult runLikelihoodWeighting(const DemoModel& model, const ExactStats& exact, const Options& opt) {
    RunResult out;
    out.backend = "likelihood";
    std::mt19937_64 rng(opt.seed + 41);
    const auto start = Clock::now();
    long double weightSum = 0.0L;
    long double queryWeight = 0.0L;
    Assignment asg{};
    asg.fill(0U);
    const auto support = activeVariables(model);
    const auto observed = model.evidenceDnf.front();
    for (uint64_t i = 0; i < opt.samples; ++i) {
        asg.fill(0U);
        long double w = 1.0L;
        for (std::size_t var : support) {
            auto obs = observedValueFor(model, var);
            if (obs.has_value()) {
                asg[var] = static_cast<uint8_t>(*obs);
                const long double p = model.vars[var].probTrue;
                w *= *obs ? p : (1.0L - p);
            } else {
                std::bernoulli_distribution dist(static_cast<double>(model.vars[var].probTrue));
                asg[var] = static_cast<uint8_t>(dist(rng));
            }
        }
        weightSum += w;
        if (evalDnf(model.queryDnf, asg)) {
            queryWeight += w;
        }
    }
    out.runtimeSec = elapsedSec(start);
    out.estimate = weightSum > 0.0L ? queryWeight / weightSum : 0.0L;
    out.absError = std::fabs(out.estimate - exact.conditionalProb);
    std::ostringstream note;
    note << "samples=" << opt.samples << " observed_lits=" << observed.size();
    out.note = note.str();
    return out;
}

RunResult runImportanceSampling(const DemoModel& model, const ExactStats& exact, const Options& opt) {
    RunResult out;
    out.backend = "importance";
    std::mt19937_64 rng(opt.seed + 53);
    const auto start = Clock::now();
    long double numerator = 0.0L;
    long double denominator = 0.0L;
    const auto support = activeVariables(model);
    Assignment asg{};
    asg.fill(0U);
    for (uint64_t i = 0; i < opt.samples; ++i) {
        asg.fill(0U);
        long double weight = 1.0L;
        for (std::size_t var : support) {
            auto obs = observedValueFor(model, var);
            if (obs.has_value()) {
                asg[var] = static_cast<uint8_t>(*obs);
                const long double p = model.vars[var].probTrue;
                weight *= *obs ? p : (1.0L - p);
                continue;
            }
            const long double q = model.vars[var].proposalTrue;
            std::bernoulli_distribution dist(static_cast<double>(q));
            asg[var] = static_cast<uint8_t>(dist(rng));
            const long double p = model.vars[var].probTrue;
            const long double target = asg[var] ? p : (1.0L - p);
            const long double proposal = asg[var] ? q : (1.0L - q);
            weight *= target / proposal;
        }
        denominator += weight;
        if (evalDnf(model.queryDnf, asg)) {
            numerator += weight;
        }
    }
    out.runtimeSec = elapsedSec(start);
    out.estimate = denominator > 0.0L ? numerator / denominator : 0.0L;
    out.absError = std::fabs(out.estimate - exact.conditionalProb);
    std::ostringstream note;
    note << "samples=" << opt.samples << " proposal-biased";
    out.note = note.str();
    return out;
}

long double effectiveSampleSize(const std::vector<Particle>& particles) {
    long double sumSq = 0.0L;
    for (const auto& particle : particles) {
        sumSq += particle.weight * particle.weight;
    }
    return sumSq > 0.0L ? 1.0L / sumSq : 0.0L;
}

void normalizeWeights(std::vector<Particle>& particles) {
    long double sum = 0.0L;
    for (const auto& particle : particles) {
        sum += particle.weight;
    }
    if (sum == 0.0L) {
        const long double uniform = 1.0L / static_cast<long double>(particles.size());
        for (auto& particle : particles) {
            particle.weight = uniform;
        }
        return;
    }
    for (auto& particle : particles) {
        particle.weight /= sum;
    }
}

void systematicResample(std::vector<Particle>& particles, std::mt19937_64& rng) {
    normalizeWeights(particles);
    std::vector<long double> cdf(particles.size(), 0.0L);
    long double acc = 0.0L;
    for (std::size_t i = 0; i < particles.size(); ++i) {
        acc += particles[i].weight;
        cdf[i] = acc;
    }
    std::uniform_real_distribution<long double> offsetDist(
            0.0L, 1.0L / static_cast<long double>(particles.size()));
    long double u = offsetDist(rng);
    std::vector<Particle> resampled;
    resampled.reserve(particles.size());
    std::size_t idx = 0;
    const long double step = 1.0L / static_cast<long double>(particles.size());
    for (std::size_t m = 0; m < particles.size(); ++m) {
        while (idx + 1 < cdf.size() && u > cdf[idx]) {
            ++idx;
        }
        resampled.push_back(particles[idx]);
        resampled.back().weight = step;
        u += step;
    }
    particles.swap(resampled);
}

RunResult runParticleSampling(const DemoModel& model, const ExactStats& exact, const Options& opt) {
    RunResult out;
    out.backend = "particle";
    std::mt19937_64 rng(opt.seed + 67);
    const auto start = Clock::now();
    std::vector<Particle> particles(opt.particles);
    const auto order = activeVariables(model);
    uint64_t resamples = 0;
    for (std::size_t var : order) {
        for (auto& particle : particles) {
            auto obs = observedValueFor(model, var);
            if (obs.has_value()) {
                particle.asg[var] = static_cast<uint8_t>(*obs);
                const long double p = model.vars[var].probTrue;
                particle.weight *= *obs ? p : (1.0L - p);
                continue;
            }
            const long double q = model.vars[var].proposalTrue;
            std::bernoulli_distribution dist(static_cast<double>(q));
            particle.asg[var] = static_cast<uint8_t>(dist(rng));
            const long double p = model.vars[var].probTrue;
            const long double target = particle.asg[var] ? p : (1.0L - p);
            const long double proposal = particle.asg[var] ? q : (1.0L - q);
            particle.weight *= target / proposal;
        }
        normalizeWeights(particles);
        if (effectiveSampleSize(particles) < static_cast<long double>(opt.particles) * 0.55L) {
            systematicResample(particles, rng);
            ++resamples;
        }
    }
    normalizeWeights(particles);
    long double estimate = 0.0L;
    for (const auto& particle : particles) {
        if (evalDnf(model.queryDnf, particle.asg)) {
            estimate += particle.weight;
        }
    }
    out.runtimeSec = elapsedSec(start);
    out.estimate = estimate;
    out.absError = std::fabs(out.estimate - exact.conditionalProb);
    std::ostringstream note;
    note << "particles=" << opt.particles << " resamples=" << resamples;
    out.note = note.str();
    return out;
}

long double unnormalizedPosteriorProb(const DemoModel& model, const Assignment& asg) {
    if (!evalDnf(model.evidenceDnf, asg)) {
        return 0.0L;
    }
    return assignmentProb(model, asg);
}

RunResult runMetropolisHastings(const DemoModel& model, const ExactStats& exact, const Options& opt) {
    RunResult out;
    out.backend = "mh";
    std::mt19937_64 rng(opt.seed + 79);
    std::uniform_real_distribution<long double> unit(0.0L, 1.0L);
    const auto start = Clock::now();

    Assignment state{};
    state.fill(0U);
    std::vector<std::size_t> mutableVars;
    for (std::size_t var = 0; var < model.activeVars; ++var) {
        auto obs = observedValueFor(model, var);
        if (obs.has_value()) {
            state[var] = static_cast<uint8_t>(*obs);
        } else {
            std::bernoulli_distribution dist(static_cast<double>(model.vars[var].probTrue));
            state[var] = static_cast<uint8_t>(dist(rng));
            mutableVars.push_back(var);
        }
    }

    std::uniform_int_distribution<std::size_t> chooseVar(0, mutableVars.size() - 1);
    long double currentProb = unnormalizedPosteriorProb(model, state);
    uint64_t accepted = 0;
    uint64_t kept = 0;
    uint64_t success = 0;

    for (uint64_t iter = 0; iter < opt.mhIterations; ++iter) {
        Assignment proposal = state;
        const std::size_t flip = mutableVars[chooseVar(rng)];
        proposal[flip] = static_cast<uint8_t>(1U - proposal[flip]);
        const long double proposalProb = unnormalizedPosteriorProb(model, proposal);
        const long double ratio = currentProb == 0.0L ? 1.0L : std::min(1.0L, proposalProb / currentProb);
        if (unit(rng) < ratio) {
            state = proposal;
            currentProb = proposalProb;
            ++accepted;
        }
        if (iter >= opt.mhBurnin && ((iter - opt.mhBurnin) % opt.mhThin == 0)) {
            ++kept;
            if (evalDnf(model.queryDnf, state)) {
                ++success;
            }
        }
    }

    out.runtimeSec = elapsedSec(start);
    out.estimate = kept ? static_cast<long double>(success) / static_cast<long double>(kept) : 0.0L;
    out.absError = std::fabs(out.estimate - exact.conditionalProb);
    std::ostringstream note;
    note << "iters=" << opt.mhIterations << " kept=" << kept << " accept_rate=" << std::fixed
         << std::setprecision(3)
         << (static_cast<double>(accepted) / static_cast<double>(opt.mhIterations));
    out.note = note.str();
    return out;
}

long double runDnfProbabilityEstimate(
        const DemoModel& model, const std::vector<Term>& dnf, uint64_t samples, uint64_t seed, std::string* noteOut) {
    std::mt19937_64 rng(seed);
    const std::vector<Term> simplified = simplifyDnf(dnf);
    std::vector<long double> termProbs;
    termProbs.reserve(simplified.size());
    for (const auto& term : simplified) {
        long double p = 1.0L;
        for (const auto& lit : term) {
            const long double base = model.vars[lit.var].probTrue;
            p *= lit.value ? base : (1.0L - base);
        }
        termProbs.push_back(p);
    }
    const long double sum = std::accumulate(termProbs.begin(), termProbs.end(), 0.0L);
    std::vector<double> discreteWeights(termProbs.begin(), termProbs.end());
    std::discrete_distribution<std::size_t> choose(discreteWeights.begin(), discreteWeights.end());
    uint64_t accepts = 0;
    const auto allVars = activeVariables(model);
    for (uint64_t i = 0; i < samples; ++i) {
        const std::size_t chosen = choose(rng);
        Assignment asg{};
        asg.fill(0U);
        std::vector<bool> fixed(model.activeVars, false);
        for (const auto& lit : simplified[chosen]) {
            asg[lit.var] = static_cast<uint8_t>(lit.value);
            fixed[lit.var] = true;
        }
        for (std::size_t var : allVars) {
            if (fixed[var]) continue;
            std::bernoulli_distribution dist(static_cast<double>(model.vars[var].probTrue));
            asg[var] = static_cast<uint8_t>(dist(rng));
        }
        std::size_t firstTrue = simplified.size();
        for (std::size_t idx = 0; idx < simplified.size(); ++idx) {
            if (evalTerm(simplified[idx], asg)) {
                firstTrue = idx;
                break;
            }
        }
        if (firstTrue == chosen) {
            ++accepts;
        }
    }
    if (noteOut) {
        std::ostringstream note;
        note << "terms=" << simplified.size() << " S(F)=" << std::fixed << std::setprecision(6)
             << static_cast<double>(sum);
        *noteOut = note.str();
    }
    return sum * static_cast<long double>(accepts) / static_cast<long double>(samples);
}

RunResult runDnfSampling(const DemoModel& model, const ExactStats& exact, const Options& opt) {
    RunResult out;
    out.backend = "dnf";
    const auto start = Clock::now();
    const std::vector<Term> joint = conjoinDnf(model.queryDnf, model.evidenceDnf);
    std::string numNote;
    std::string denNote;
    const long double numerator = runDnfProbabilityEstimate(model, joint, opt.samples, opt.seed + 97, &numNote);
    const long double denominator =
            runDnfProbabilityEstimate(model, model.evidenceDnf, opt.samples, opt.seed + 101, &denNote);
    out.runtimeSec = elapsedSec(start);
    out.estimate = denominator > 0.0L ? numerator / denominator : 0.0L;
    out.absError = std::fabs(out.estimate - exact.conditionalProb);
    out.note = numNote + " | " + denNote;
    return out;
}

std::vector<RunResult> runBackends(const DemoModel& model, const ExactStats& exact, const Options& opt) {
    std::vector<RunResult> out;
    auto maybeRun = [&](Backend which, auto&& fn) {
        if (opt.backend == Backend::All || opt.backend == which) {
            out.push_back(fn());
        }
    };
    maybeRun(Backend::World, [&] { return runWorldSampling(model, exact, opt); });
    maybeRun(Backend::Formula, [&] { return runFormulaSampling(model, exact, opt); });
    maybeRun(Backend::Rejection, [&] { return runRejectionSampling(model, exact, opt); });
    maybeRun(Backend::Likelihood, [&] { return runLikelihoodWeighting(model, exact, opt); });
    maybeRun(Backend::Importance, [&] { return runImportanceSampling(model, exact, opt); });
    maybeRun(Backend::Particle, [&] { return runParticleSampling(model, exact, opt); });
    maybeRun(Backend::Mh, [&] { return runMetropolisHastings(model, exact, opt); });
    maybeRun(Backend::Dnf, [&] { return runDnfSampling(model, exact, opt); });
    return out;
}

void printExampleHeader(const DemoModel& model, const ExactStats& exact, const Options& opt) {
    std::cout << "\nexample [" << model.key << "]: " << model.title << "\n";
    for (std::size_t i = 0; i < model.activeVars; ++i) {
        std::cout << "  " << model.vars[i].name << " ~ Bernoulli(" << std::fixed << std::setprecision(2)
                  << static_cast<double>(model.vars[i].probTrue) << ")\n";
    }
    const auto support = formulaSupport(model, model.queryDnf, model.evidenceDnf);
    std::cout << "  query Q = " << model.queryText << "\n";
    std::cout << "  evidence E = " << model.evidenceText << "\n";
    std::cout << "  active_vars=" << model.activeVars << " support_vars=" << support.size()
              << " query_terms=" << simplifyDnf(model.queryDnf).size()
              << " overlap_ratio=" << std::setprecision(6)
              << static_cast<double>(dnfMassUpperBound(model, model.queryDnf) / exact.queryProb) << "\n";
    std::cout << "  budget(samples=" << opt.samples << ", accepted=" << opt.acceptedSamples
              << ", particles=" << opt.particles << ", mh_iters=" << opt.mhIterations << ")\n";
    std::cout << std::setprecision(9)
              << "  exact P(Q)     = " << static_cast<double>(exact.queryProb) << "\n"
              << "  exact P(E)     = " << static_cast<double>(exact.evidenceProb) << "\n"
              << "  exact P(Q & E) = " << static_cast<double>(exact.jointProb) << "\n"
              << "  exact P(Q | E) = " << static_cast<double>(exact.conditionalProb) << "\n";
}

void printResults(const std::vector<RunResult>& results) {
    std::cout << "  backend results:\n";
    std::cout << "  " << std::left << std::setw(12) << "backend" << std::right << std::setw(14) << "estimate"
              << std::setw(14) << "abs_error" << std::setw(12) << "runtime_s" << "  note\n";
    for (const auto& res : results) {
        std::cout << "  " << std::left << std::setw(12) << res.backend << std::right << std::setw(14)
                  << std::setprecision(9) << static_cast<double>(res.estimate) << std::setw(14)
                  << static_cast<double>(res.absError) << std::setw(12) << std::setprecision(6) << res.runtimeSec
                  << "  " << res.note << "\n";
    }
}

std::vector<DemoModel> selectExamples(const Options& opt) {
    auto all = allExamples();
    if (opt.example == ExampleId::All) return all;

    std::vector<DemoModel> out;
    for (auto& example : all) {
        if ((opt.example == ExampleId::Easy && example.key == "easy") ||
                (opt.example == ExampleId::Medium && example.key == "medium") ||
                (opt.example == ExampleId::RareEvidence && example.key == "rare") ||
                (opt.example == ExampleId::DenseOverlap && example.key == "dense")) {
            out.push_back(example);
        }
    }
    return out;
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options userOpt = parseArgs(argc, argv);
        const auto examples = selectExamples(userOpt);
        bool allOk = true;
        for (std::size_t i = 0; i < examples.size(); ++i) {
            const auto& model = examples[i];
            const auto exact = computeExactStats(model);
            const auto opt = resolveOptionsForExample(userOpt, model, i);
            printExampleHeader(model, exact, opt);
            const auto results = runBackends(model, exact, opt);
            printResults(results);
            for (const auto& res : results) {
                if (res.absError > opt.tolerance || res.runtimeSec > 10.0) {
                    std::cerr << "\n[fail] example " << model.key << ", backend " << res.backend
                              << " exceeded limit: abs_error=" << static_cast<double>(res.absError)
                              << " tolerance=" << opt.tolerance << " runtime_s=" << res.runtimeSec << "\n";
                    allOk = false;
                }
            }
            if (allOk) {
                std::cout << "  [ok] all selected backends stayed within tolerance " << opt.tolerance
                          << " and 10s budget\n";
            }
        }
        return allOk ? 0 : 2;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }
}
