#include "weighted_conversion.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <deque>
#include <iomanip>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace approxmc_demo {
namespace {

constexpr long double kEps = 1e-12L;

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.compare(0, prefix.size(), prefix) == 0;
}

std::string trim(const std::string& s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = s.find_last_not_of(" \t\r\n");
    return s.substr(first, last - first + 1);
}

long double parseWeightToken(const std::string& token) {
    const auto slash = token.find('/');
    if (slash == std::string::npos) {
        return std::stold(token);
    }
    const auto numerator = std::stold(token.substr(0, slash));
    const auto denominator = std::stold(token.substr(slash + 1));
    if (std::fabs(denominator) <= kEps) {
        throw std::runtime_error("Weight denominator cannot be zero");
    }
    return numerator / denominator;
}

std::vector<int> parseLiteralListFromLine(const std::string& line) {
    std::vector<int> lits;
    std::stringstream ss(line);
    int lit = 0;
    while (ss >> lit) {
        if (lit == 0) {
            break;
        }
        lits.push_back(lit);
    }
    return lits;
}

std::vector<int> canonicalizeClause(const std::vector<int>& clause, bool* tautology) {
    std::vector<int> out;
    out.reserve(clause.size());
    for (int lit : clause) {
        if (lit != 0) {
            out.push_back(lit);
        }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
    for (int lit : out) {
        if (std::binary_search(out.begin(), out.end(), -lit)) {
            if (tautology != nullptr) {
                *tautology = true;
            }
            return {};
        }
    }
    if (tautology != nullptr) {
        *tautology = false;
    }
    return out;
}

bool approxEq(long double a, long double b) {
    return std::fabs(a - b) <= kEps;
}

bool isZero(long double x) {
    return std::fabs(x) <= kEps;
}

bool isOne(long double x) {
    return approxEq(x, 1.0L);
}

long double pow2ld(uint32_t exp) {
    return std::ldexp(1.0L, static_cast<int>(exp));
}

std::string toBinaryNoLastBit(uint64_t value) {
    if (value == 0) {
        return {};
    }

    std::string bits;
    while (value > 0) {
        bits.push_back((value & 1ULL) ? '1' : '0');
        value >>= 1U;
    }
    std::reverse(bits.begin(), bits.end());
    bits.pop_back();  // drop the guaranteed trailing 1
    return bits;
}

std::vector<std::vector<int>> getChainCNF(int var, const std::string& bitStr, bool sign, uint32_t baseVars) {
    std::vector<std::vector<int>> cnfClauses;
    const auto bitLen = static_cast<uint32_t>(bitStr.size());

    cnfClauses.push_back({static_cast<int>(bitLen + 1 + baseVars)});
    for (uint32_t i = 0; i < bitLen; ++i) {
        int newVar = static_cast<int>(bitLen - i + baseVars);
        if (!sign) {
            newVar = -newVar;
        }

        const char bit = bitStr[bitLen - i - 1];
        if (bit == '0') {
            cnfClauses.push_back({newVar});
        } else {
            for (auto& clause : cnfClauses) {
                clause.push_back(newVar);
            }
        }
    }

    for (auto& clause : cnfClauses) {
        clause.push_back(var);
    }
    return cnfClauses;
}

long double effectiveLiteralWeight(const std::unordered_map<int, long double>& weights, int lit) {
    const auto it = weights.find(lit);
    if (it != weights.end()) {
        return it->second;
    }
    const auto opp = weights.find(-lit);
    if (opp != weights.end()) {
        return 1.0L - opp->second;
    }
    return 1.0L;
}

struct QuantizedWeight {
    uint64_t bitMultiplier = 0;
    uint32_t bitPrecision = 0;
    long double representedWeight = 0.0L;
};

QuantizedWeight quantizeWeight(long double weight, uint32_t precision) {
    if (precision < 2) {
        throw std::runtime_error("Precision must be >= 2");
    }
    if (weight < -kEps || weight > 1.0L + kEps) {
        throw std::runtime_error("Weight must be in [0,1]");
    }

    const long double scaled = weight * pow2ld(precision);
    const auto rounded = static_cast<long double>(std::nearbyintl(scaled));
    if (rounded < 0.0L) {
        throw std::runtime_error("Quantization produced negative integer");
    }

    uint64_t bitMult = static_cast<uint64_t>(rounded);
    uint32_t bitPrec = precision;
    while (bitPrec > 0 && bitMult % 2ULL == 0ULL) {
        bitMult >>= 1U;
        --bitPrec;
    }

    QuantizedWeight q;
    q.bitMultiplier = bitMult;
    q.bitPrecision = bitPrec;
    if (bitPrec == 0) {
        q.representedWeight = static_cast<long double>(bitMult);
    } else {
        q.representedWeight = static_cast<long double>(bitMult) / pow2ld(bitPrec);
    }
    return q;
}

}  // namespace

WeightedCNFInput parseWeightedDimacs(std::istream& in) {
    WeightedCNFInput result;
    bool foundHeader = false;
    bool foundSamplingSet = false;

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty()) {
            continue;
        }

        if (startsWith(line, "p ")) {
            std::stringstream ss(line);
            std::string p, cnf;
            uint32_t vars = 0;
            uint32_t clauses = 0;
            ss >> p >> cnf >> vars >> clauses;
            if (p != "p" || cnf != "cnf" || !ss) {
                throw std::runtime_error("Invalid header. Expected: p cnf <vars> <clauses>");
            }
            result.numVars = vars;
            foundHeader = true;
            continue;
        }

        if (startsWith(line, "c p show") || startsWith(line, "c ind")) {
            foundSamplingSet = true;
            std::stringstream ss(startsWith(line, "c p show") ? line.substr(8) : line.substr(5));
            int var = 0;
            while (ss >> var) {
                if (var == 0) {
                    break;
                }
                if (var < 0) {
                    throw std::runtime_error("Sampling variables must be positive");
                }
                result.samplingSet.push_back(static_cast<uint32_t>(var));
            }
            continue;
        }

        if (startsWith(line, "c p weight") || startsWith(line, "w ")) {
            std::stringstream ss(startsWith(line, "c p weight") ? line.substr(10) : line.substr(2));
            int lit = 0;
            std::string token;
            ss >> lit >> token;
            if (!ss || lit == 0 || token.empty()) {
                throw std::runtime_error("Invalid weight line: " + line);
            }
            result.litWeights[lit] = parseWeightToken(token);
            continue;
        }

        if (startsWith(line, "c MUST MULTIPLY BY")) {
            std::stringstream ss(line.substr(std::string("c MUST MULTIPLY BY").size()));
            std::string token;
            ss >> token;
            if (!token.empty()) {
                result.multiplier = parseWeightToken(token);
            }
            continue;
        }

        if (!line.empty() && line[0] == 'c') {
            continue;
        }

        if (!foundHeader) {
            throw std::runtime_error("Header must appear before clauses");
        }
        auto clause = parseLiteralListFromLine(line);
        result.clauses.push_back(std::move(clause));
    }

    if (!foundHeader) {
        throw std::runtime_error("Missing DIMACS header");
    }

    if (!foundSamplingSet) {
        result.samplingSet.reserve(result.numVars);
        for (uint32_t v = 1; v <= result.numVars; ++v) {
            result.samplingSet.push_back(v);
        }
    }

    return result;
}

WeightedCNFInput parseWeightedDimacsString(const std::string& text) {
    std::stringstream ss(text);
    return parseWeightedDimacs(ss);
}

std::string toDimacs(const UnweightedCNFResult& result) {
    std::ostringstream out;
    out << "p cnf " << result.numVars << " " << result.clauses.size() << "\n";
    out << "c p show ";
    for (uint32_t v : result.samplingSet) {
        out << v << " ";
    }
    out << "0\n";
    for (const auto& clause : result.clauses) {
        for (int lit : clause) {
            out << lit << " ";
        }
        out << "0\n";
    }
    out << std::setprecision(20);
    out << "c MUST MULTIPLY BY " << result.multiplier << " 0\n";
    out << "c DIVIDE BY 2**" << result.divideExp << "\n";
    return out.str();
}

UnweightedCNFResult convertWeightedToUnweighted(WeightedCNFInput input, const ConversionConfig& config) {
    UnweightedCNFResult out;
    out.report.inputVars = input.numVars;
    out.report.inputClauses = static_cast<uint32_t>(input.clauses.size());
    out.report.inputSamplingSize = static_cast<uint32_t>(input.samplingSet.size());

    try {
        if (input.numVars == 0) {
            throw std::runtime_error("Input has zero variables");
        }

        std::set<uint32_t> samplingSet;
        if (input.samplingSet.empty()) {
            for (uint32_t v = 1; v <= input.numVars; ++v) {
                samplingSet.insert(v);
            }
        } else {
            for (uint32_t v : input.samplingSet) {
                if (v == 0 || v > input.numVars) {
                    throw std::runtime_error("Sampling set contains an out-of-range variable");
                }
                samplingSet.insert(v);
            }
        }

        std::vector<std::vector<int>> clauses;
        clauses.reserve(input.clauses.size());
        for (const auto& c : input.clauses) {
            bool tautology = false;
            auto canonical = canonicalizeClause(c, &tautology);
            if (tautology) {
                continue;
            }
            if (canonical.empty()) {
                out.report.unsat = true;
                out.report.valid = true;
                out.report.message = "Found empty clause during canonicalization";
                out.multiplier = 0.0L;
                out.divideExp = 0;
                out.numVars = input.numVars;
                out.samplingSet.assign(samplingSet.begin(), samplingSet.end());
                out.report.outputVars = out.numVars;
                out.report.outputClauses = 1;
                return out;
            }
            for (int lit : canonical) {
                const uint32_t var = static_cast<uint32_t>(std::abs(lit));
                if (var == 0 || var > input.numVars) {
                    throw std::runtime_error("Clause contains literal outside header variable range");
                }
            }
            clauses.push_back(std::move(canonical));
        }

        std::unordered_map<int, long double> weights = input.litWeights;
        long double multiplier = input.multiplier;
        uint32_t forcedAssignments = 0;

        if (config.preprocess) {
            std::deque<int> unitQueue;

            for (const auto& clause : clauses) {
                if (clause.size() == 1) {
                    unitQueue.push_back(clause.front());
                }
            }

            std::set<uint32_t> weightedVars;
            for (const auto& kv : weights) {
                weightedVars.insert(static_cast<uint32_t>(std::abs(kv.first)));
            }

            for (uint32_t var : weightedVars) {
                const long double wPos = effectiveLiteralWeight(weights, static_cast<int>(var));
                const long double wNeg = effectiveLiteralWeight(weights, -static_cast<int>(var));
                if (isZero(wPos) && isZero(wNeg)) {
                    out.report.unsat = true;
                    out.report.valid = true;
                    out.report.message = "Both literal weights became 0 for a variable";
                    out.multiplier = 0.0L;
                    out.divideExp = 0;
                    out.numVars = input.numVars;
                    out.samplingSet.assign(samplingSet.begin(), samplingSet.end());
                    out.report.forcedAssignments = forcedAssignments;
                    out.report.outputVars = out.numVars;
                    out.report.outputClauses = static_cast<uint32_t>(clauses.size());
                    return out;
                }
                if (isZero(wPos)) {
                    unitQueue.push_back(-static_cast<int>(var));
                } else if (isZero(wNeg)) {
                    unitQueue.push_back(static_cast<int>(var));
                }
            }

            std::unordered_map<uint32_t, bool> assignment;
            while (!unitQueue.empty()) {
                const int lit = unitQueue.front();
                unitQueue.pop_front();

                const uint32_t var = static_cast<uint32_t>(std::abs(lit));
                const bool value = (lit > 0);
                const auto it = assignment.find(var);
                if (it != assignment.end()) {
                    if (it->second != value) {
                        out.report.unsat = true;
                        out.report.valid = true;
                        out.report.message = "Conflicting unit assignments during preprocessing";
                        out.multiplier = 0.0L;
                        out.divideExp = 0;
                        out.numVars = input.numVars;
                        out.samplingSet.assign(samplingSet.begin(), samplingSet.end());
                        out.report.forcedAssignments = forcedAssignments;
                        out.report.outputVars = out.numVars;
                        out.report.outputClauses = static_cast<uint32_t>(clauses.size());
                        return out;
                    }
                    continue;
                }

                const long double litWeight = effectiveLiteralWeight(weights, lit);
                if (litWeight < -kEps) {
                    throw std::runtime_error("Encountered negative literal weight during preprocessing");
                }
                multiplier *= litWeight;
                assignment[var] = value;
                ++forcedAssignments;
                samplingSet.erase(var);
                weights.erase(static_cast<int>(var));
                weights.erase(-static_cast<int>(var));

                std::vector<std::vector<int>> simplified;
                simplified.reserve(clauses.size());
                for (const auto& clause : clauses) {
                    bool satisfied = false;
                    std::vector<int> reduced;
                    reduced.reserve(clause.size());
                    for (int cLit : clause) {
                        if (cLit == lit) {
                            satisfied = true;
                            break;
                        }
                        if (cLit == -lit) {
                            continue;
                        }
                        reduced.push_back(cLit);
                    }

                    if (satisfied) {
                        continue;
                    }

                    bool tautology = false;
                    auto canonical = canonicalizeClause(reduced, &tautology);
                    if (tautology) {
                        continue;
                    }
                    if (canonical.empty()) {
                        out.report.unsat = true;
                        out.report.valid = true;
                        out.report.message = "Preprocessing produced empty clause";
                        out.multiplier = 0.0L;
                        out.divideExp = 0;
                        out.numVars = input.numVars;
                        out.samplingSet.assign(samplingSet.begin(), samplingSet.end());
                        out.report.forcedAssignments = forcedAssignments;
                        out.report.outputVars = out.numVars;
                        out.report.outputClauses = 1;
                        return out;
                    }
                    if (canonical.size() == 1) {
                        unitQueue.push_back(canonical.front());
                    }
                    simplified.push_back(std::move(canonical));
                }
                clauses.swap(simplified);
            }
        }

        // keep only weights that are relevant for the sampling set
        {
            std::unordered_map<int, long double> filtered;
            filtered.reserve(weights.size());
            for (const auto& kv : weights) {
                if (samplingSet.count(static_cast<uint32_t>(std::abs(kv.first))) > 0) {
                    filtered.emplace(kv.first, kv.second);
                }
            }
            weights.swap(filtered);
        }

        std::set<uint32_t> weightedVars;
        for (const auto& kv : weights) {
            weightedVars.insert(static_cast<uint32_t>(std::abs(kv.first)));
        }
        out.report.weightedVarsBefore = static_cast<uint32_t>(weightedVars.size());

        // ensure both polarities are present
        for (uint32_t var : weightedVars) {
            const int pos = static_cast<int>(var);
            const int neg = -pos;
            if (weights.find(pos) == weights.end() && weights.find(neg) != weights.end()) {
                weights[pos] = 1.0L - weights[neg];
            }
            if (weights.find(neg) == weights.end() && weights.find(pos) != weights.end()) {
                weights[neg] = 1.0L - weights[pos];
            }
        }

        // drop 1/1 pairs
        {
            std::vector<uint32_t> toErase;
            toErase.reserve(weightedVars.size());
            for (uint32_t var : weightedVars) {
                if (isOne(weights[static_cast<int>(var)]) && isOne(weights[-static_cast<int>(var)])) {
                    toErase.push_back(var);
                }
            }
            for (uint32_t var : toErase) {
                weights.erase(static_cast<int>(var));
                weights.erase(-static_cast<int>(var));
                weightedVars.erase(var);
            }
        }

        // normalize each pair to sum up to 1, and absorb totals into multiplier
        for (uint32_t var : weightedVars) {
            const int pos = static_cast<int>(var);
            const int neg = -pos;
            long double wPos = weights[pos];
            long double wNeg = weights[neg];
            if (wPos < -kEps || wNeg < -kEps) {
                throw std::runtime_error("Negative literal weight after preprocessing");
            }
            if (isZero(wPos) || isZero(wNeg)) {
                throw std::runtime_error(
                        "Zero literal weight remains after preprocessing; increase preprocessing strength");
            }

            const long double total = wPos + wNeg;
            if (total <= kEps) {
                out.report.unsat = true;
                out.report.valid = true;
                out.report.message = "Weight normalization produced zero total";
                out.multiplier = 0.0L;
                out.divideExp = 0;
                out.numVars = input.numVars;
                out.samplingSet.assign(samplingSet.begin(), samplingSet.end());
                out.report.forcedAssignments = forcedAssignments;
                out.report.outputVars = out.numVars;
                out.report.outputClauses = static_cast<uint32_t>(clauses.size());
                return out;
            }
            if (!approxEq(total, 1.0L)) {
                multiplier *= total;
                wPos /= total;
                wNeg /= total;
                weights[pos] = wPos;
                weights[neg] = wNeg;
            }
        }

        // drop 1/1 pairs again after normalization
        {
            std::vector<uint32_t> toErase;
            toErase.reserve(weightedVars.size());
            for (uint32_t var : weightedVars) {
                if (isOne(weights[static_cast<int>(var)]) && isOne(weights[-static_cast<int>(var)])) {
                    toErase.push_back(var);
                }
            }
            for (uint32_t var : toErase) {
                weights.erase(static_cast<int>(var));
                weights.erase(-static_cast<int>(var));
                weightedVars.erase(var);
            }
        }
        out.report.weightedVarsAfter = static_cast<uint32_t>(weightedVars.size());

        // compute tilt
        long double minWeight = std::numeric_limits<long double>::infinity();
        long double maxWeight = 0.0L;
        for (uint32_t var : weightedVars) {
            const long double wPos = weights[static_cast<int>(var)];
            const long double wNeg = weights[-static_cast<int>(var)];
            if (wPos > kEps) {
                minWeight = std::min(minWeight, wPos);
                maxWeight = std::max(maxWeight, wPos);
            }
            if (wNeg > kEps) {
                minWeight = std::min(minWeight, wNeg);
                maxWeight = std::max(maxWeight, wNeg);
            }
        }
        out.report.tilt = std::isfinite(minWeight) ? (maxWeight / minWeight) : 1.0L;
        if (config.tiltMax > 0.0L && out.report.tilt > config.tiltMax) {
            out.report.tiltViolated = true;
            if (config.failOnTilt) {
                out.report.valid = false;
                out.report.message = "Tilt exceeds configured threshold";
                out.numVars = input.numVars;
                out.clauses = clauses;
                out.samplingSet.assign(samplingSet.begin(), samplingSet.end());
                out.multiplier = multiplier;
                out.divideExp = 0;
                out.report.outputVars = out.numVars;
                out.report.outputClauses = static_cast<uint32_t>(out.clauses.size());
                out.report.outputSamplingSize = static_cast<uint32_t>(out.samplingSet.size());
                out.report.forcedAssignments = forcedAssignments;
                out.report.multiplier = multiplier;
                return out;
            }
        }

        uint32_t numVars = input.numVars;
        std::vector<std::vector<int>> convertedClauses = clauses;
        uint32_t divideExp = 0;
        long double maxAbsQuantErr = 0.0L;
        long double maxRelQuantErr = 0.0L;

        std::vector<uint32_t> varsToEncode(weightedVars.begin(), weightedVars.end());
        std::sort(varsToEncode.begin(), varsToEncode.end());
        for (uint32_t var : varsToEncode) {
            const long double weight = weights[static_cast<int>(var)];
            const auto q = quantizeWeight(weight, config.precision);
            ++out.report.quantizedVars;

            if (q.bitPrecision == 0) {
                out.report.valid = false;
                out.report.message =
                        "Quantization collapsed to precision 0. Likely forced/degenerate weight remained.";
                return out;
            }

            const long double absErr = std::fabs(q.representedWeight - weight);
            const long double relErr = absErr / std::max(weight, kEps);
            maxAbsQuantErr = std::max(maxAbsQuantErr, absErr);
            maxRelQuantErr = std::max(maxRelQuantErr, relErr);

            if (q.bitPrecision == 1 && q.bitMultiplier == 1) {
                divideExp += 1;
                continue;
            }

            std::string bits = toBinaryNoLastBit(q.bitMultiplier);
            const uint32_t targetLen = q.bitPrecision - 1;
            if (bits.size() > targetLen) {
                out.report.valid = false;
                out.report.message = "Internal error while preparing chain encoding bits";
                return out;
            }
            if (bits.size() < targetLen) {
                bits = std::string(targetLen - bits.size(), '0') + bits;
            }

            std::string complement(bits.size(), '0');
            for (size_t i = 0; i < bits.size(); ++i) {
                complement[i] = (bits[i] == '0') ? '1' : '0';
            }

            samplingSet.insert(numVars + 1);
            for (uint32_t i = 0; i < q.bitPrecision - 1; ++i) {
                samplingSet.insert(numVars + i + 2);
            }

            auto origChain = getChainCNF(-static_cast<int>(var), bits, true, numVars);
            auto compChain = getChainCNF(static_cast<int>(var), complement, false, numVars);
            const std::set<std::vector<int>> origSet(origChain.begin(), origChain.end());

            for (auto& clause : origChain) {
                convertedClauses.push_back(std::move(clause));
            }
            for (auto& clause : compChain) {
                if (origSet.count(clause) == 0) {
                    convertedClauses.push_back(std::move(clause));
                }
            }

            numVars += q.bitPrecision;
            divideExp += q.bitPrecision;
        }

        out.numVars = numVars;
        out.clauses = std::move(convertedClauses);
        out.samplingSet.assign(samplingSet.begin(), samplingSet.end());
        out.multiplier = multiplier;
        out.divideExp = divideExp;

        out.report.valid = true;
        out.report.unsat = false;
        out.report.multiplier = multiplier;
        out.report.divideExp = divideExp;
        out.report.forcedAssignments = forcedAssignments;
        out.report.maxQuantAbsError = maxAbsQuantErr;
        out.report.maxQuantRelError = maxRelQuantErr;
        out.report.outputVars = out.numVars;
        out.report.outputClauses = static_cast<uint32_t>(out.clauses.size());
        out.report.addedVars =
                static_cast<int64_t>(out.report.outputVars) - static_cast<int64_t>(out.report.inputVars);
        out.report.addedClauses =
                static_cast<int64_t>(out.report.outputClauses) - static_cast<int64_t>(out.report.inputClauses);
        out.report.outputSamplingSize = static_cast<uint32_t>(out.samplingSet.size());
        if (out.report.message.empty()) {
            out.report.message = "OK";
        }
        return out;

    } catch (const std::exception& ex) {
        out.report.valid = false;
        out.report.message = ex.what();
        return out;
    }
}

}  // namespace approxmc_demo
