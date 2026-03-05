#pragma once

#include <cstdint>
#include <iosfwd>
#include <string>
#include <unordered_map>
#include <vector>

namespace approxmc_demo {

struct WeightedCNFInput {
    uint32_t numVars = 0;
    std::vector<std::vector<int>> clauses;
    std::vector<uint32_t> samplingSet;  // DIMACS-style, 1-based
    std::unordered_map<int, long double> litWeights;  // DIMACS literal -> weight
    long double multiplier = 1.0L;
};

struct ConversionConfig {
    uint32_t precision = 7;
    bool preprocess = true;
    uint32_t verbosity = 0;
    long double tiltMax = 0.0L;  // 0 means disabled
    bool failOnTilt = false;
};

struct ConversionReport {
    bool valid = true;
    bool unsat = false;
    std::string message;

    uint32_t inputVars = 0;
    uint32_t inputClauses = 0;
    uint32_t outputVars = 0;
    uint32_t outputClauses = 0;
    int64_t addedVars = 0;
    int64_t addedClauses = 0;

    uint32_t inputSamplingSize = 0;
    uint32_t outputSamplingSize = 0;

    uint32_t forcedAssignments = 0;
    uint32_t weightedVarsBefore = 0;
    uint32_t weightedVarsAfter = 0;
    uint32_t quantizedVars = 0;

    long double multiplier = 1.0L;
    uint32_t divideExp = 0;

    long double tilt = 1.0L;
    bool tiltViolated = false;
    long double maxQuantAbsError = 0.0L;
    long double maxQuantRelError = 0.0L;
};

struct UnweightedCNFResult {
    uint32_t numVars = 0;
    std::vector<std::vector<int>> clauses;
    std::vector<uint32_t> samplingSet;  // DIMACS-style, 1-based
    long double multiplier = 1.0L;
    uint32_t divideExp = 0;
    ConversionReport report;
};

WeightedCNFInput parseWeightedDimacs(std::istream& in);
WeightedCNFInput parseWeightedDimacsString(const std::string& text);
std::string toDimacs(const UnweightedCNFResult& result);

UnweightedCNFResult convertWeightedToUnweighted(
        WeightedCNFInput input, const ConversionConfig& config = {});

}  // namespace approxmc_demo
