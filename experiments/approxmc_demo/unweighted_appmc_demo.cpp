#include <approxmc/approxmc.h>
#include <cryptominisat5/cryptominisat.h>

#include <cstdint>
#include <iostream>
#include <numeric>
#include <vector>

int main() {
    using ApproxMC::AppMC;
    using ApproxMC::SolCount;
    using CMSat::FGenDouble;
    using CMSat::Lit;

    constexpr uint32_t numVars = 10;
    constexpr uint64_t expected = 512;  // x3 <-> x4 over 10 vars

    std::unique_ptr<FieldGen> fg = std::make_unique<FGenDouble>();
    AppMC appmc(fg);
    appmc.set_seed(7);
    appmc.set_epsilon(0.2);
    appmc.set_delta(0.05);

    appmc.new_vars(numVars);

    // DIMACS: -3 4 0
    appmc.add_clause({Lit(2, true), Lit(3, false)});
    // DIMACS: 3 -4 0
    appmc.add_clause({Lit(2, false), Lit(3, true)});

    std::vector<uint32_t> samplVars(numVars);
    std::iota(samplVars.begin(), samplVars.end(), 0);
    appmc.set_sampl_vars(samplVars);

    const SolCount count = appmc.count();
    if (!count.valid) {
        std::cerr << "ApproxMC returned an invalid count." << std::endl;
        return 1;
    }

    const uint64_t estimate = count.cellSolCount << count.hashCount;
    std::cout << "ApproxMC estimate: " << count.cellSolCount << " * 2^" << count.hashCount << " = " << estimate
              << std::endl;
    std::cout << "Expected exact count for this toy CNF: " << expected << std::endl;

    if (estimate != expected) {
        std::cerr << "Estimate mismatch for this deterministic toy case." << std::endl;
        return 2;
    }

    return 0;
}
