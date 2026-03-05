#pragma once

#include <approxmc/approxmc.h>
#include <cryptominisat5/cryptominisat.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "weighted_conversion.h"

namespace approxmc_demo {

struct WeightedSolCount {
    bool valid = false;
    bool unsat = false;
    std::string message;

    uint32_t hashCount = 0;
    uint64_t cellSolCount = 0;

    long double unweightedEstimate = 0.0L;
    long double weightedEstimate = 0.0L;

    long double multiplier = 1.0L;
    uint32_t divideExp = 0;

    long double tilt = 1.0L;
    bool tiltViolated = false;

    uint32_t forcedAssignments = 0;
    int64_t addedVars = 0;
    int64_t addedClauses = 0;
    long double maxQuantAbsError = 0.0L;
    long double maxQuantRelError = 0.0L;
};

class WeightedAppMC {
public:
    WeightedAppMC() = default;

    // Match AppMC-like controls
    void set_verbosity(uint32_t verbosity) { verbosity_ = verbosity; }
    void set_seed(uint32_t seed) { seed_ = seed; }
    void set_epsilon(double epsilon) { epsilon_ = epsilon; }
    void set_delta(double delta) { delta_ = delta; }

    // Weighted-specific controls
    void set_precision(uint32_t precision) { precision_ = precision; }
    void set_tilt_max(long double tiltMax) { tiltMax_ = tiltMax; }
    void set_fail_on_tilt(bool failOnTilt) { failOnTilt_ = failOnTilt; }
    void set_preprocess(bool preprocess) { preprocess_ = preprocess; }

    // Formula API
    void new_var();
    void new_vars(uint32_t num);
    uint32_t nVars() const { return numVars_; }
    bool add_clause(const std::vector<CMSat::Lit>& lits);
    void set_sampl_vars(const std::vector<uint32_t>& vars);
    void set_lit_weight(const CMSat::Lit& lit, long double weight);

    WeightedSolCount count();
    const ConversionReport& last_conversion_report() const { return lastReport_; }

private:
    WeightedCNFInput build_input() const;

    uint32_t verbosity_ = 0;
    uint32_t seed_ = 1;
    double epsilon_ = 0.8;
    double delta_ = 0.2;
    uint32_t precision_ = 7;
    long double tiltMax_ = 0.0L;
    bool failOnTilt_ = false;
    bool preprocess_ = true;

    uint32_t numVars_ = 0;
    std::vector<std::vector<int>> clausesDimacs_;
    std::vector<uint32_t> samplVars0Based_;
    std::unordered_map<int, long double> litWeights_;

    ConversionReport lastReport_;
};

}  // namespace approxmc_demo
