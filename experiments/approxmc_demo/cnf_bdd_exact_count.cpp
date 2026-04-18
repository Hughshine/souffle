#include "souffle/problog/formula/CuddManager.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

struct CnfInput {
    int numVars = 0;
    std::vector<std::vector<int>> clauses;
    std::vector<int> supportVars;
};

struct Options {
    std::string cnfPath;
    bool printHelp = false;
};

double elapsedSec(const Clock::time_point t0) {
    return std::chrono::duration<double>(Clock::now() - t0).count();
}

Options parseArgs(int argc, char** argv) {
    Options opt;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--cnf") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value after --cnf");
            }
            opt.cnfPath = argv[++i];
        } else if (arg == "-h" || arg == "--help") {
            opt.printHelp = true;
        } else {
            throw std::runtime_error("Unknown argument: " + arg);
        }
    }
    if (!opt.printHelp && opt.cnfPath.empty()) {
        throw std::runtime_error("Missing --cnf path");
    }
    return opt;
}

void printUsage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " --cnf /path/to/input.cnf\n";
}

bool startsWith(const std::string& line, const std::string& prefix) {
    return line.size() >= prefix.size() && line.compare(0, prefix.size(), prefix) == 0;
}

CnfInput parseDimacs(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("Failed to open CNF: " + path);
    }

    CnfInput out;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        if (startsWith(line, "c p show")) {
            std::istringstream iss(line);
            std::string c, p, show;
            iss >> c >> p >> show;
            int lit = 0;
            while (iss >> lit) {
                if (lit == 0) break;
                out.supportVars.push_back(std::abs(lit));
            }
            continue;
        }
        if (startsWith(line, "c ind")) {
            std::istringstream iss(line);
            std::string c, ind;
            iss >> c >> ind;
            int lit = 0;
            while (iss >> lit) {
                if (lit == 0) break;
                out.supportVars.push_back(std::abs(lit));
            }
            continue;
        }
        if (line[0] == 'c') {
            continue;
        }
        if (startsWith(line, "p cnf")) {
            std::istringstream iss(line);
            std::string p, cnf;
            int numClauses = 0;
            iss >> p >> cnf >> out.numVars >> numClauses;
            (void)numClauses;
            continue;
        }

        std::istringstream iss(line);
        std::vector<int> clause;
        int lit = 0;
        while (iss >> lit) {
            if (lit == 0) break;
            clause.push_back(lit);
        }
        if (!clause.empty()) {
            out.clauses.push_back(std::move(clause));
        }
    }

    if (out.numVars <= 0) {
        throw std::runtime_error("Missing or invalid p cnf line in " + path);
    }
    if (out.supportVars.empty()) {
        out.supportVars.reserve(static_cast<size_t>(out.numVars));
        for (int v = 1; v <= out.numVars; ++v) {
            out.supportVars.push_back(v);
        }
    } else {
        std::sort(out.supportVars.begin(), out.supportVars.end());
        out.supportVars.erase(std::unique(out.supportVars.begin(), out.supportVars.end()), out.supportVars.end());
    }
    return out;
}

BddNodeRef buildClause(WeightedBDDManager& mgr, const std::vector<int>& clause) {
    std::vector<BddNodeRef> lits;
    lits.reserve(clause.size());
    for (int lit : clause) {
        const int varIndex = std::abs(lit) - 1;
        BddNodeRef var = mgr.createVar(varIndex);
        lits.push_back(lit > 0 ? var : mgr.makeNot(var));
    }
    return mgr.makeOr(lits);
}

BddNodeRef buildFormula(WeightedBDDManager& mgr, const CnfInput& cnf) {
    std::vector<BddNodeRef> clauses;
    clauses.reserve(cnf.clauses.size());
    for (const auto& clause : cnf.clauses) {
        clauses.push_back(buildClause(mgr, clause));
    }
    if (clauses.empty()) {
        return mgr.getTrue();
    }
    return mgr.makeAnd(clauses);
}

DdNode* buildPositiveCube(WeightedBDDManager& mgr, const std::vector<int>& varsOneBased) {
    BddNodeRef cube = mgr.getTrue();
    for (int var : varsOneBased) {
        cube = mgr.makeAnd(cube, mgr.createVar(var - 1));
    }
    DdNode* raw = cube.get();
    Cudd_Ref(raw);
    return raw;
}

long double exactProjectedCount(WeightedBDDManager& mgr, const CnfInput& cnf, const BddNodeRef& formula) {
    std::unordered_set<int> support(cnf.supportVars.begin(), cnf.supportVars.end());
    std::vector<int> hiddenVars;
    hiddenVars.reserve(static_cast<size_t>(std::max(0, cnf.numVars - static_cast<int>(cnf.supportVars.size()))));
    for (int v = 1; v <= cnf.numVars; ++v) {
        if (!support.count(v)) {
            hiddenVars.push_back(v);
        }
    }

    DdManager* dd = mgr.getManager();
    DdNode* root = formula.get();
    Cudd_Ref(root);

    if (!hiddenVars.empty()) {
        DdNode* cube = buildPositiveCube(mgr, hiddenVars);
        DdNode* abstracted = Cudd_bddExistAbstract(dd, root, cube);
        if (abstracted == nullptr) {
            Cudd_RecursiveDeref(dd, cube);
            Cudd_RecursiveDeref(dd, root);
            throw std::runtime_error("Cudd_bddExistAbstract failed");
        }
        Cudd_Ref(abstracted);
        Cudd_RecursiveDeref(dd, cube);
        Cudd_RecursiveDeref(dd, root);
        root = abstracted;
    }

    const double minterms = Cudd_CountMinterm(dd, root, static_cast<int>(cnf.supportVars.size()));
    Cudd_RecursiveDeref(dd, root);
    return static_cast<long double>(minterms);
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const Options opt = parseArgs(argc, argv);
        if (opt.printHelp) {
            printUsage(argv[0]);
            return 0;
        }

        const auto tParse = Clock::now();
        const CnfInput cnf = parseDimacs(opt.cnfPath);
        const double parseSec = elapsedSec(tParse);

        WeightedBDDManager::InitConfig cfg;
        cfg.numVars = static_cast<unsigned int>(std::max(128, cnf.numVars + 8));
        cfg.numSlots = 512;
        cfg.cacheSize = 1u << 20;
        cfg.maxMemory = 8UL * 1024 * 1024 * 1024;
        WeightedBDDManager mgr(cfg);

        const auto tBuild = Clock::now();
        BddNodeRef formula = buildFormula(mgr, cnf);
        const double buildSec = elapsedSec(tBuild);

        const auto tCount = Clock::now();
        const long double exactCount = exactProjectedCount(mgr, cnf, formula);
        const double countSec = elapsedSec(tCount);

        std::cout << std::setprecision(17);
        std::cout << "[cnf] path=" << opt.cnfPath
                  << " vars=" << cnf.numVars
                  << " clauses=" << cnf.clauses.size()
                  << " support_vars=" << cnf.supportVars.size()
                  << " parse_s=" << parseSec << "\n";
        std::cout << "[bdd] exact_count=" << exactCount
                  << " build_s=" << buildSec
                  << " count_s=" << countSec
                  << " total_s=" << (parseSec + buildSec + countSec)
                  << " live_nodes=" << mgr.getLiveNodeCount()
                  << "\n";
        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "error: " << ex.what() << "\n";
        return 1;
    }
}
