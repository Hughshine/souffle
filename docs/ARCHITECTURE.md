# Architecture

This artifact evaluates probabilistic Datalog programs under online updates.
The compiler emits an interactive runtime. The runtime computes a baseline
derivation graph, accepts insert/delete turns, and writes output tuple
probabilities after each commit.

The AE comparison is `full` versus `inc-naive` or `inc-regional`.
`full` is the exact recomputation oracle; incremental modes must produce
matching tuple keys and probabilities on the same delta stream.

## Source References

- [../src/MainDriver.cpp:636](../src/MainDriver.cpp#L636): compiler-facing incremental options.
- [../src/synthesiser/Synthesiser.cpp:673](../src/synthesiser/Synthesiser.cpp#L673): generated runtime pipeline entry.
- [../src/synthesiser/Synthesiser.cpp:4533](../src/synthesiser/Synthesiser.cpp#L4533): baked runtime defaults.
- [../src/problog/Pipeline.cpp:923](../src/problog/Pipeline.cpp#L923): baseline pipeline and online CLI handoff.
- [../src/include/souffle/cli/Cli.h:679](../src/include/souffle/cli/Cli.h#L679): interactive command surface.
- [../src/include/souffle/cli/Executor.h:234](../src/include/souffle/cli/Executor.h#L234): incremental commit graph update.
- [../src/include/souffle/problog/ForwardCompilation.h:517](../src/include/souffle/problog/ForwardCompilation.h#L517): naive incremental forward compilation.
- [../src/include/souffle/problog/ForwardCompilation.h:1967](../src/include/souffle/problog/ForwardCompilation.h#L1967): regional incremental forward compilation.
- [../src/include/souffle/problog/formula/CuddManager.h:568](../src/include/souffle/problog/formula/CuddManager.h#L568): CUDD adaptive reordering.

## 1. Compile

The normal Souffle frontend parses and transforms the program. This branch
selects the online AST-to-RAM translator and emits a binary wired to the
probabilistic incremental pipeline.

Compile-time AE parameters set runtime defaults only. They do not expose
backend, determinism, variable-index reuse, or reordering switches.

## 2. Baseline

The generated binary reads `<relation>.facts` and optional `<relation>.prob`
files, runs the compiled semi-naive evaluator, records rule applications, and
builds the baseline derivation graph. The baseline initializes BDD formulas and
writes the initial probability output.

## 3. Commit

Each `commit` applies queued insertions and deletions. The runtime:

1. stages tuple operations from the CLI;
2. reruns the generated incremental RAM relations for the delta;
3. applies graph deletes before inserts;
4. prunes the graph to output-relevant state;
5. runs the selected forward-compilation mode;
6. writes per-turn probabilities.

`@post_delete_*` snapshots are part of the generated RAM protocol for exact
mixed insert/delete semantics in non-recursive upper strata.

## 4. Forward Compilation

`inc-naive` updates formulas on the delta-reachable scope. It handles deletion,
rederive, insertion, CUDD variable reuse, and incremental weighted model
counting without exposing those internal choices as flags.

`inc-regional` analyzes the delta-reachable region, chooses boundaries, reuses
unaffected formulas outside the region, and calibrates boundary weights. If a
regional turn would consume unsafe persistent state, the runtime falls back on
the forward-compilation side while preserving semantic mode.

## 5. Outputs

Default probability outputs are always written. Additional material is opt-in:
graph DOT/JSON and statistics use `--dump`, and timing or diagnostic streams
use `--profile-stage`. The `stat` dump is a broad diagnostic selector: it emits
graph counters, `graph-*.json`, SEM/DRed summaries, regional scope diagnostics
for `inc-regional`, and startup deterministic-relation files when enabled
before baseline graph construction.
