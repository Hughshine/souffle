#include "weighted_appmc.h"

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

namespace {

const char* kToyWeightedCNF = R"(p cnf 2 1
c t wpmc
c p show 1 2 0
1 2 0
c p weight 1 0.9 0
c p weight 2 0.5 0
)";

void printUsage(const char* argv0) {
    std::cerr << "Usage: " << argv0
              << " [--input weighted.cnf] [--prec N] [--seed S] [--epsilon E]"
                 " [--delta D] [--tilt-max X] [--fail-on-tilt] [--no-preprocess]"
                 " [--verbosity V]\n";
}

void loadWeightedInput(const approxmc_demo::WeightedCNFInput& in, approxmc_demo::WeightedAppMC& appmc) {
    appmc.new_vars(in.numVars);

    for (const auto& clause : in.clauses) {
        std::vector<CMSat::Lit> lits;
        lits.reserve(clause.size());
        for (int dimacsLit : clause) {
            const uint32_t var = static_cast<uint32_t>(std::abs(dimacsLit) - 1);
            const bool sign = dimacsLit < 0;
            lits.emplace_back(var, sign);
        }
        appmc.add_clause(lits);
    }

    std::vector<uint32_t> sampl0;
    sampl0.reserve(in.samplingSet.size());
    for (uint32_t v : in.samplingSet) {
        sampl0.push_back(v - 1);
    }
    appmc.set_sampl_vars(sampl0);

    for (const auto& kv : in.litWeights) {
        const int dimacsLit = kv.first;
        const uint32_t var = static_cast<uint32_t>(std::abs(dimacsLit) - 1);
        const bool sign = dimacsLit < 0;
        appmc.set_lit_weight(CMSat::Lit(var, sign), kv.second);
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::string inputPath;
    uint32_t precision = 7;
    uint32_t seed = 1;
    double epsilon = 0.2;
    double delta = 0.05;
    uint32_t verbosity = 0;
    long double tiltMax = 0.0L;
    bool failOnTilt = false;
    bool preprocess = true;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--input" && i + 1 < argc) {
            inputPath = argv[++i];
        } else if (arg == "--prec" && i + 1 < argc) {
            precision = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--seed" && i + 1 < argc) {
            seed = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--epsilon" && i + 1 < argc) {
            epsilon = std::stod(argv[++i]);
        } else if (arg == "--delta" && i + 1 < argc) {
            delta = std::stod(argv[++i]);
        } else if (arg == "--verbosity" && i + 1 < argc) {
            verbosity = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--tilt-max" && i + 1 < argc) {
            tiltMax = std::stold(argv[++i]);
        } else if (arg == "--fail-on-tilt") {
            failOnTilt = true;
        } else if (arg == "--no-preprocess") {
            preprocess = false;
        } else if (arg == "--help" || arg == "-h") {
            printUsage(argv[0]);
            return 0;
        } else {
            printUsage(argv[0]);
            std::cerr << "Unknown or incomplete argument: " << arg << "\n";
            return 2;
        }
    }

    approxmc_demo::WeightedCNFInput weighted;
    try {
        if (inputPath.empty()) {
            weighted = approxmc_demo::parseWeightedDimacsString(kToyWeightedCNF);
        } else {
            std::ifstream in(inputPath);
            if (!in) {
                std::cerr << "Cannot open input file: " << inputPath << "\n";
                return 2;
            }
            weighted = approxmc_demo::parseWeightedDimacs(in);
        }
    } catch (const std::exception& ex) {
        std::cerr << "Failed to parse weighted CNF: " << ex.what() << "\n";
        return 2;
    }

    approxmc_demo::WeightedAppMC appmc;
    appmc.set_precision(precision);
    appmc.set_seed(seed);
    appmc.set_epsilon(epsilon);
    appmc.set_delta(delta);
    appmc.set_verbosity(verbosity);
    appmc.set_preprocess(preprocess);
    appmc.set_tilt_max(tiltMax);
    appmc.set_fail_on_tilt(failOnTilt);
    loadWeightedInput(weighted, appmc);

    const auto result = appmc.count();
    std::cout << std::setprecision(20);
    std::cout << "valid=" << (result.valid ? "true" : "false") << "\n";
    std::cout << "unsat=" << (result.unsat ? "true" : "false") << "\n";
    std::cout << "message=" << result.message << "\n";
    std::cout << "hashCount=" << result.hashCount << "\n";
    std::cout << "cellSolCount=" << result.cellSolCount << "\n";
    std::cout << "unweightedEstimate=" << result.unweightedEstimate << "\n";
    std::cout << "weightedEstimate=" << result.weightedEstimate << "\n";
    std::cout << "multiplier=" << result.multiplier << "\n";
    std::cout << "divideExp=" << result.divideExp << "\n";
    std::cout << "tilt=" << result.tilt << " (violated=" << (result.tiltViolated ? "true" : "false") << ")\n";
    std::cout << "forcedAssignments=" << result.forcedAssignments << "\n";
    std::cout << "addedVars=" << result.addedVars << "\n";
    std::cout << "addedClauses=" << result.addedClauses << "\n";
    std::cout << "maxQuantAbsError=" << result.maxQuantAbsError << "\n";
    std::cout << "maxQuantRelError=" << result.maxQuantRelError << "\n";

    return result.valid ? 0 : 1;
}
