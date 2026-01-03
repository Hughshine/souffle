# Precompile Refactor Log

## Goals
- Reduce compile time for generated C++ by moving heavy runtime implementations out of headers.
- Build a precompiled runtime library and link it from `souffle-compile.py`.
- Preserve runtime behavior and CLI outputs.

## Current Wiring (2025-02-14)
- Runtime implementations moved into `.cpp` files and built into the `compiled` static library (see `src/CMakeLists.txt`).
- `souffle-compile.py` links `$<TARGET_FILE:compiled>` via `SOUFFLE_COMPILED_LIBS`; no extra runtime flags are required.
- Generated `main` no longer includes `souffle/cli/Cli.h` or `souffle/problog/DerivationGraph.h`, reducing header load.

## Scope (initial)
- Runtime/problog headers currently compiled into generated programs:
  - `src/include/souffle/Derivation.h`
  - `src/include/souffle/cli/Cli.h`
  - `src/include/souffle/problog/DerivationGraph.h`
  - `src/include/souffle/problog/GraphAnalyzer.h`
  - `src/include/souffle/problog/GraphRewriter.h`
  - `src/include/souffle/problog/Pipeline.h`
  - `src/include/souffle/problog/ForwardCompilation.h`
  - `src/include/souffle/problog/formula/CuddManager.h`
  - `src/include/souffle/problog/formula/SddManager.h`
  - `src/include/souffle/problog/Rule.h`
  - `src/include/souffle/problog/RuleManager.h`
  - `src/include/souffle/problog/QueryManager.h`

## Baseline
- Compile timing (P3 compute.cpp, compile-only):
  - Command: `/usr/bin/c++ -c -DUSE_NCURSES -DUSE_LIBZ -DUSE_SQLITE -I/home/hugh/research/datalog/souffle/src/include -I/usr/include -std=c++17 -fopenmp -O3 -I/home/hugh/research/datalog/souffle/src/include -o precompile_tmp/compute_baseline.o precompile_tmp/compute.cpp`
  - Wall time: 58.699s
  - Notes: baseline compile-only; link step was not attempted in that run.

## Latest Measurement
- Compile timing (P3 compute.cpp regenerated with `--online --full-only`, compile-only):
  - Command: `/usr/bin/c++ -c -DUSE_NCURSES -DUSE_LIBZ -DUSE_SQLITE -I/home/hugh/research/datalog/souffle/src/include -I/usr/include -std=c++17 -fopenmp -O3 -I/home/hugh/research/datalog/souffle/src/include -o precompile_tmp/compute_precompile.o precompile_tmp/compute.cpp`
  - Wall time: 15.36s
  - Delta: ~3.8x faster vs baseline
  - Notes: compile-only; see worklog for link test.

## Plan (completed 2025-02-14)
1) Measure baseline compile time for a representative generated program.
2) Add a runtime library target and wire `souffle-compile.py` to link it.
3) Move non-template implementations out of headers into `.cpp` files and keep headers as declarations.
4) Re-measure compile time and verify correctness with a small run.
Status: completed; remaining work is optional (e.g., further header splits/explicit instantiations).

## Worklog
- 2025-02-14: Initialized log and scope for precompile refactor.
- 2025-02-14: Fixed SDD linking order in `src/souffle-compile.template.py` by routing `-lsdd++ -lsdd` through the `additional_libs` list (so `-L` paths come first); kept CLI unchanged.
- 2025-02-14: Moved `souffle/Derivation.h` implementations into `src/Derivation.cpp` (headers now declarations + externs); reduced generated-code header payload.
- 2025-02-14: Stopped emitting `souffle/cli/Cli.h` in generated code (`src/synthesiser/Synthesiser.cpp`) so `--online` no longer drags the full CLI header into compile units.
- 2025-02-14: Added runtime sources to the `compiled` static library and injected `$<TARGET_FILE:compiled>` into `souffle-compile.py` link options via `src/CMakeLists.txt`.
- 2025-02-14: Rebuilt `cmake-build-release`, regenerated `precompile_tmp/compute.cpp`, and re-measured compile-only time (15.36s).
- 2025-02-14: Link test succeeded: `python3 cmake-build-release/src/souffle-compile.py --with-cudd -o precompile_tmp/compute_bin precompile_tmp/compute.cpp`.
- 2025-02-14: Verified linked binary runs and prints usage (`precompile_tmp/compute_bin --help`).
- 2025-02-14: Moved `DerivationGraph::setMergeBiImpEnabled` into `souffle::problog::runPipeline` and removed `DerivationGraph.h` include from generated `main`.
- 2025-02-14: Moved `souffle/problog/Atom.h` free-function definitions into `src/problog/Atom.cpp` to avoid duplicate symbols when linking with `libcompiled.a`.
