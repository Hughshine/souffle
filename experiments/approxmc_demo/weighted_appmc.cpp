#include "weighted_appmc.h"

#include <cstdlib>
#include <cmath>
#include <memory>
#include <utility>
#include <vector>

namespace approxmc_demo {

void WeightedAppMC::new_var() {
    ++numVars_;
}

void WeightedAppMC::new_vars(uint32_t num) {
    numVars_ += num;
}

bool WeightedAppMC::add_clause(const std::vector<CMSat::Lit>& lits) {
    std::vector<int> clause;
    clause.reserve(lits.size());
    for (const auto& lit : lits) {
        const int var = static_cast<int>(lit.var()) + 1;
        clause.push_back(lit.sign() ? -var : var);
    }
    clausesDimacs_.push_back(std::move(clause));
    return true;
}

void WeightedAppMC::set_sampl_vars(const std::vector<uint32_t>& vars) {
    samplVars0Based_ = vars;
}

void WeightedAppMC::set_lit_weight(const CMSat::Lit& lit, long double weight) {
    const int var = static_cast<int>(lit.var()) + 1;
    const int dimacsLit = lit.sign() ? -var : var;
    litWeights_[dimacsLit] = weight;
}

WeightedCNFInput WeightedAppMC::build_input() const {
    WeightedCNFInput input;
    input.numVars = numVars_;
    input.clauses = clausesDimacs_;
    input.litWeights = litWeights_;
    input.multiplier = 1.0L;

    if (samplVars0Based_.empty()) {
        input.samplingSet.reserve(numVars_);
        for (uint32_t v = 1; v <= numVars_; ++v) {
            input.samplingSet.push_back(v);
        }
    } else {
        input.samplingSet.reserve(samplVars0Based_.size());
        for (uint32_t v0 : samplVars0Based_) {
            input.samplingSet.push_back(v0 + 1);
        }
    }
    return input;
}

WeightedSolCount WeightedAppMC::count() {
    WeightedSolCount result;

    ConversionConfig config;
    config.precision = precision_;
    config.preprocess = preprocess_;
    config.verbosity = verbosity_;
    config.tiltMax = tiltMax_;
    config.failOnTilt = failOnTilt_;

    const auto converted = convertWeightedToUnweighted(build_input(), config);
    lastReport_ = converted.report;

    result.valid = converted.report.valid;
    result.unsat = converted.report.unsat;
    result.message = converted.report.message;
    result.multiplier = converted.report.multiplier;
    result.divideExp = converted.report.divideExp;
    result.tilt = converted.report.tilt;
    result.tiltViolated = converted.report.tiltViolated;
    result.forcedAssignments = converted.report.forcedAssignments;
    result.addedVars = converted.report.addedVars;
    result.addedClauses = converted.report.addedClauses;
    result.maxQuantAbsError = converted.report.maxQuantAbsError;
    result.maxQuantRelError = converted.report.maxQuantRelError;

    if (!converted.report.valid) {
        return result;
    }
    if (converted.report.unsat) {
        result.valid = true;
        result.unweightedEstimate = 0.0L;
        result.weightedEstimate = 0.0L;
        return result;
    }

    std::unique_ptr<CMSat::FieldGen> fg = std::make_unique<CMSat::FGenDouble>();
    ApproxMC::AppMC appmc(fg);
    appmc.set_seed(seed_);
    appmc.set_epsilon(epsilon_);
    appmc.set_delta(delta_);
    appmc.set_verbosity(verbosity_);

    appmc.new_vars(converted.numVars);
    for (const auto& clause : converted.clauses) {
        std::vector<CMSat::Lit> lits;
        lits.reserve(clause.size());
        for (int dimacsLit : clause) {
            const uint32_t var = static_cast<uint32_t>(std::abs(dimacsLit) - 1);
            const bool sign = (dimacsLit < 0);
            lits.emplace_back(var, sign);
        }
        appmc.add_clause(lits);
    }

    std::vector<uint32_t> samplVars;
    samplVars.reserve(converted.samplingSet.size());
    for (uint32_t v : converted.samplingSet) {
        samplVars.push_back(v - 1);
    }
    appmc.set_sampl_vars(samplVars);

    const ApproxMC::SolCount c = appmc.count();
    if (!c.valid) {
        result.valid = false;
        result.message = "ApproxMC returned invalid count";
        return result;
    }

    result.hashCount = c.hashCount;
    result.cellSolCount = c.cellSolCount;
    result.unweightedEstimate =
            std::ldexp(static_cast<long double>(c.cellSolCount), static_cast<int>(c.hashCount));
    result.weightedEstimate =
            (result.unweightedEstimate * converted.multiplier) / std::ldexp(1.0L, converted.divideExp);
    result.valid = true;
    return result;
}

}  // namespace approxmc_demo
