#include "weighted_conversion.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

namespace {

const char* kToyWeightedCNF = R"(p cnf 2 1
c t wpmc
c p show 1 2 0
1 2 0
c p weight 1 0.9 0
c p weight 2 0.5 0
)";

void printUsage(const char* argv0) {
    std::cerr
            << "Usage: " << argv0
            << " [--input weighted.cnf] [--output-unweighted out.cnf] [--print-cnf]"
               " [--prec N] [--tilt-max X] [--fail-on-tilt] [--no-preprocess]\n";
}

}  // namespace

int main(int argc, char** argv) {
    std::string inputPath;
    std::string outputPath;
    bool printCnf = false;

    approxmc_demo::ConversionConfig config;
    config.precision = 7;
    config.preprocess = true;
    config.tiltMax = 0.0L;
    config.failOnTilt = false;
    config.verbosity = 0;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--input" && i + 1 < argc) {
            inputPath = argv[++i];
        } else if (arg == "--output-unweighted" && i + 1 < argc) {
            outputPath = argv[++i];
        } else if (arg == "--prec" && i + 1 < argc) {
            config.precision = static_cast<uint32_t>(std::stoul(argv[++i]));
        } else if (arg == "--tilt-max" && i + 1 < argc) {
            config.tiltMax = std::stold(argv[++i]);
        } else if (arg == "--fail-on-tilt") {
            config.failOnTilt = true;
        } else if (arg == "--no-preprocess") {
            config.preprocess = false;
        } else if (arg == "--print-cnf") {
            printCnf = true;
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

    const auto converted = approxmc_demo::convertWeightedToUnweighted(weighted, config);
    const auto& rep = converted.report;

    std::cout << std::setprecision(20);
    std::cout << "valid=" << (rep.valid ? "true" : "false") << "\n";
    std::cout << "unsat=" << (rep.unsat ? "true" : "false") << "\n";
    std::cout << "message=" << rep.message << "\n";
    std::cout << "input vars/clauses=" << rep.inputVars << "/" << rep.inputClauses << "\n";
    std::cout << "output vars/clauses=" << rep.outputVars << "/" << rep.outputClauses << "\n";
    std::cout << "added vars/clauses=" << rep.addedVars << "/" << rep.addedClauses << "\n";
    std::cout << "forced assignments=" << rep.forcedAssignments << "\n";
    std::cout << "weighted vars before/after=" << rep.weightedVarsBefore << "/" << rep.weightedVarsAfter << "\n";
    std::cout << "tilt=" << rep.tilt << " (violated=" << (rep.tiltViolated ? "true" : "false") << ")\n";
    std::cout << "multiplier=" << rep.multiplier << "\n";
    std::cout << "divideExp=" << rep.divideExp << "\n";
    std::cout << "maxQuantAbsError=" << rep.maxQuantAbsError << "\n";
    std::cout << "maxQuantRelError=" << rep.maxQuantRelError << "\n";

    if (!rep.valid) {
        return 1;
    }

    if (!outputPath.empty()) {
        std::ofstream out(outputPath);
        if (!out) {
            std::cerr << "Cannot open output file: " << outputPath << "\n";
            return 2;
        }
        out << approxmc_demo::toDimacs(converted);
    }

    if (printCnf) {
        std::cout << "----- converted cnf begin -----\n";
        std::cout << approxmc_demo::toDimacs(converted);
        std::cout << "----- converted cnf end -----\n";
    }

    return 0;
}
