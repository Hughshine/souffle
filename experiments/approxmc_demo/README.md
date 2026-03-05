# ApproxMC Demo (C++ in-memory)

Standalone experiment directory for using ApproxMC from C++ without modifying Souffle internals.

## Files
- `weighted_conversion.h/.cpp`: in-memory weighted->unweighted converter with preprocessing.
- `WEIGHTED_TO_UNWEIGHTED.md`: conversion algorithm notes and reconstruction formula.
- `weighted_appmc.h/.cpp`: AppMC-like weighted wrapper API (`set_lit_weight`, `count`).
- `weighted_conversion_demo.cpp`: converter-only demo (no ApproxMC dependency).
- `weighted_appmc_demo.cpp`: weighted counting demo through `WeightedAppMC`.
- `unweighted_appmc_demo.cpp`: baseline unweighted AppMC demo.
- `toy_weighted.cnf`: minimal weighted toy input.
- `weighted_pipeline_demo.py`: legacy Python pipeline demo kept for comparison.

## API shape (AppMC-like)

`WeightedAppMC` intentionally mirrors unweighted `AppMC` usage:
- formula construction: `new_vars`, `add_clause`, `set_sampl_vars`
- base controls: `set_seed`, `set_epsilon`, `set_delta`, `set_verbosity`
- weighted additions:
  - `set_lit_weight(CMSat::Lit lit, long double w)`
  - `set_precision(uint32_t)`
  - `set_tilt_max(long double)` / `set_fail_on_tilt(bool)`
  - `set_preprocess(bool)`

`count()` returns `WeightedSolCount` with extra diagnostics:
- `weightedEstimate`, `unweightedEstimate`
- `multiplier`, `divideExp`
- `tilt`, `tiltViolated`
- `forcedAssignments`
- `addedVars`, `addedClauses`
- `maxQuantAbsError`, `maxQuantRelError`

Conversion details are documented in [`WEIGHTED_TO_UNWEIGHTED.md`](WEIGHTED_TO_UNWEIGHTED.md).

Minimal usage sketch:
```cpp
using CMSat::Lit;
approxmc_demo::WeightedAppMC wamc;
wamc.new_vars(2);
wamc.add_clause({Lit(0, false), Lit(1, false)}); // (x1 v x2)
wamc.set_sampl_vars({0, 1});                      // 0-based
wamc.set_lit_weight(Lit(0, false), 0.9L);         // w(x1)=0.9
wamc.set_lit_weight(Lit(1, false), 0.5L);         // w(x2)=0.5
wamc.set_precision(7);
wamc.set_tilt_max(100.0L);
auto res = wamc.count();
```

## Preprocessing and checks

The converter includes:
- clause canonicalization and tautology removal
- unit propagation
- zero-weight forced assignments
- weight completion (`w(-x)=1-w(x)` when only one side is provided)
- per-variable normalization with multiplier accumulation
- tilt computation and threshold check

## Build

### Converter-only (always available)
```bash
cd experiments/approxmc_demo
cmake -S . -B build
cmake --build build -j
./build/weighted_conversion_demo --input toy_weighted.cnf --print-cnf
```

### With ApproxMC installed
```bash
cd experiments/approxmc_demo
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/approxmc/install/prefix
cmake --build build -j
./build/unweighted_appmc_demo
./build/weighted_appmc_demo --input toy_weighted.cnf --tilt-max 100
```

If `approxmc` is not found, CMake still builds `weighted_conversion_demo` and skips ApproxMC-dependent targets.

## No-CMake quick run

Direct compile + test script:
```bash
cd experiments/approxmc_demo
./run_experiment.sh
```

This script:
- compiles `weighted_conversion_demo` with `g++`
- runs toy conversion checks
- runs tilt failure check
- runs a forced-assignment preprocessing check

Optional ApproxMC-backed run:
```bash
cd experiments/approxmc_demo
APPROXMC_PREFIX=/path/to/approxmc/install/prefix ./run_experiment.sh
```

## Souffle-generated C++ linking

If generated C++ uses ApproxMC symbols, link at final compile step:
```bash
APPROXMC_PREFIX=/path/to/approxmc/install/prefix
./build/src/souffle-compile.py generated.cpp -o generated \
  -I"${APPROXMC_PREFIX}/include" \
  -L"${APPROXMC_PREFIX}/lib" \
  -lapproxmc
```

`souffle-compile.template.py` already supports `-I`, `-L`, and `-l`.

## Upstream references
- ApproxMC: https://github.com/meelgroup/approxmc
- Weighted-to-unweighted algorithm/tool lineage: https://github.com/meelgroup/weighted-to-unweighted
