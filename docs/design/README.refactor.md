# Refactor Notes (Online-Only Fork)

## Source references
- [src/problog/Pipeline.cpp](src/problog/Pipeline.cpp)
- [src/include/souffle/problog/DerivationGraph.h](src/include/souffle/problog/DerivationGraph.h)


This file captures refactor opportunities after re-reading the core online pipeline and problog code. It is not a plan or a commitment; it is a prioritized list of likely improvements.

## Status
- Active refactor note; not a commitment to implement.

## Scope (files reviewed)
- `src/MainDriver.cpp`
- `src/include/souffle/cli/Cli.h`
- `src/include/souffle/problog/DerivationGraph.h`
- `src/include/souffle/problog/ForwardCompilation.h`
- `src/include/souffle/problog/Pipeline.h`
- `src/include/souffle/Derivation.h`
- `src/include/souffle/problog/RuleManager.h`
- `src/include/souffle/problog/QueryManager.h`
- `src/include/souffle/problog/Rule.h`
- `src/ast2ram/online/UnitTranslator.cpp`
- `src/ast2ram/utility/TranslatorContext.cpp`
- `src/include/souffle/CompiledOptions.h`

## Status Update (2025-02-14)
- Implemented a precompiled runtime library (`compiled`) and moved non-template runtime code into `.cpp` files.
- `souffle-compile.py` now links `$<TARGET_FILE:compiled>` via `SOUFFLE_COMPILED_LIBS`.
- Generated `main` no longer includes `souffle/cli/Cli.h` or `souffle/problog/DerivationGraph.h`, reducing header load.

## High-Impact Refactor Candidates

1) Remove header-level global state and make it explicit in a context object.
- Examples: `src/include/souffle/problog/DerivationGraph.h` (`nextFormulaNodeId`, `nodeIdMap`, `edgeIdMap`, `probResult`), `src/include/souffle/problog/ForwardCompilation.h` (`Debugger& debugger`), `src/include/souffle/Derivation.h` (`DerivationManager` static maps).
- Why: header globals create hidden coupling, ODR risk, and make repeated runs (or multiple graphs) fragile. They also complicate testing and thread safety.
- Suggestion: introduce a `PipelineContext`/`EvaluationContext` that owns ID mapping, probability results, and derivation maps; pass it to CLI and pipeline helpers.

2) Break shared_ptr cycles in the derivation graph ownership model.
- Current: `Node` holds `EdgePtr`, and `Hyperedge` holds `NodePtr` (both `shared_ptr`), which forms cycles.
- Risk: graph resets or rebuilds can leak memory because reference cycles do not break.
- Suggestion: keep ownership in a single container (`DerivationGraph` owns nodes/edges via `unique_ptr` or `shared_ptr`), but store non-owning pointers (`Node*`, `Edge*`) or `weak_ptr` in adjacency lists. Add a small `GraphStorage` layer to control lifetime and stable IDs.

3) Split the CLI into parser, state, and execution layers.
- `src/include/souffle/cli/Cli.h` mixes command parsing, queueing, graph mutation, formula building, WMC, dumping, and interactive I/O in a single class.
- Suggestion: extract `CommandParser` (string -> command struct), `CommandQueue`, and `IncrementalExecutor` (mode dispatch for inc/full/regional). Keep readline/interactive I/O thin. This will simplify testing and allow reuse for non-interactive runs.

4) Consolidate online execution steps shared by ground vs non-ground paths.
- `IncrementalCLI::commit()` has parallel logic for ground and non-ground, duplicating applyDelta, prune, FC, WMC, and dump steps.
- Suggestion: factor shared stages into a pipeline function that accepts a minimal interface (graph view + formula manager + mode + output config). This reduces error-prone drift between the two branches.

5) Move heavy inline implementations out of headers to reduce build time.
- Many non-template implementations live in headers: `src/include/souffle/problog/DerivationGraph.h`, `src/include/souffle/problog/ForwardCompilation.h`, `src/include/souffle/problog/Pipeline.h`, `src/include/souffle/problog/RuleManager.h`, `src/include/souffle/problog/QueryManager.h`, `src/include/souffle/problog/Rule.h`, `src/include/souffle/Derivation.h`.
- Status (2025-02-14): partially implemented. Runtime code moved into `src/*.cpp` (Derivation, Atom, Rule/RuleManager/QueryManager, Pipeline, Debugger) and linked via the `compiled` static library; generated code no longer includes `Cli.h` or `DerivationGraph.h`.
- Remaining heavy headers: `DerivationGraph.h`, `ForwardCompilation.h`, `GraphAnalyzer.h`, `GraphRewriter.h`.
- Suggestion: keep only declarations and small inline helpers in headers; move large function bodies to `.cpp`. Keep templates in headers where necessary.

6) Centralize output naming and output-dir handling.
- `outputPath` and output naming logic is duplicated in `src/include/souffle/cli/Cli.h` and `src/include/souffle/problog/Pipeline.h`, and `dumpProbabilities` in `src/include/souffle/problog/DerivationGraph.h` embeds defaults.
- Suggestion: create a small `OutputPaths` helper (or extend `CmdOptions`) for output dir, prefixing, and per-iteration filename conventions. This also makes naming changes safer.

7) Replace string-based mode plumbing with a shared enum and parser.
- `CmdOptions` stores `incMode` as a string and `IncrementalCLI::setCmdOptions()` re-parses it.
- Suggestion: introduce `enum class IncMode` in `src/include/souffle/CompiledOptions.h` (or a shared header), parse once, and reuse across CLI and pipeline.

8) Make derivation store ownership explicit and RAII-friendly.
- `DerivationManager` uses `unordered_map<UntypedTuple, unordered_set<RuleApplication>*>` with manual allocation and unclear ownership.
- Suggestion: store values directly (`unordered_set<RuleApplication>`) or use `std::unique_ptr` + explicit lifecycle hooks. Add helpers for "clear delta, keep base" semantics to avoid scattered manual cleanup.

9) Remove test/demo data from production headers.
- `src/include/souffle/Derivation.h` and `src/include/souffle/problog/RuleManager.h` include inline test objects and `ExampleRuleComponents`.
- Suggestion: move these to a test-only translation unit or guard with `#ifdef` so they do not inflate compile time or leak into runtime builds.

10) Main driver decomposition for clarity and testability.
- `src/MainDriver.cpp` is large and mixes option parsing, default policy, translation, and codegen/runtime execution.
- Suggestion: split into `DriverOptions` (parsing/defaulting), `CompileDriver` (AST->RAM->C++), and `RunDriver` (compile+execute). This makes policy changes easier and reduces regressions when defaults change (e.g., online default).

## Lower-Impact Cleanups (Optional)
- Remove unused members: `FunctionTimer` in `src/include/souffle/Derivation.h` stores a `Debugger&` but never uses it.
- Replace repeated `std::cout` logging with a central logger (so debug output can be toggled or redirected consistently).
- Reduce compile dependencies by replacing heavy includes with forward declarations where possible (e.g., `Pipeline.h` including `Cli.h`).
- Avoid `using namespace` in headers (e.g., `src/include/souffle/datastructure/Brie.h`, `src/include/souffle/problog/ForwardCompilation.h`) to prevent namespace pollution for generated programs.

## Include/ Scan Highlights (Additional)
- `src/include/souffle/problog/formula/CuddManager.h` defines mutable global state at header scope (`currentReorderingType`, `_cudd_gc_count`, `_cudd_reordering_count`, etc.). This should move into `WeightedBDDManager` state or a `.cpp` translation unit to avoid ODR issues and shared-state bugs.
- `src/include/souffle/Derivation.h` still carries inline test fixtures (`testVarValues`, `testRuleApplicationSet*`), which should be moved out of headers or guarded with `#ifdef`.

## Suggested Order (If You Decide to Implement)
1) Header globals -> context objects (ODR + correctness).
2) Ownership cycles in `DerivationGraph` (memory safety).
3) CLI layering split (testability).
4) Output naming + mode enum consolidation (low risk, high clarity).
5) Move heavy inline implementations into `.cpp` to improve build time.

## Precompiled Runtime Library (Detailed Notes)
Status (2025-02-14):
- Implemented using the `compiled` static library in `src/CMakeLists.txt`.
- CMake injects `$<TARGET_FILE:compiled>` into `SOUFFLE_COMPILED_LIBS`, and `souffle-compile.py` links it via `link_options`.
- The proposal below is historical context; current wiring does not add custom JSON fields.

If header implementations are split into `.cpp`, a precompiled runtime library is the safest path. The generated program must link against it; otherwise you will see unresolved symbols at link time.

### Recommended shape
- Build a dedicated runtime library (e.g., `libsouffle_runtime` or `libsouffle_problog`) that contains the moved implementations.
- Keep template/`inline` code in headers unless you add explicit instantiations.
- Keep generated `compute.cpp` compilation cheap: compile only the generated file and link to the runtime library.

### souffle-compile integration (expected changes)
- Inject runtime library paths and names from CMake into `JSON_DATA_TEXT`.
- Extend `souffle-compile.template.py` to append:
  - `-L<runtime_lib_dir>`
  - `-l<runtime_lib_name>`
  - `-Wl,-rpath,<runtime_lib_dir>` (if shared)
- Prefer CMake-provided paths over guessing based on source locations.

### Concrete wiring proposal (CMake + JSON fields)
Define explicit JSON fields so the compile script can link the runtime without guessing:
- `runtime_lib_dirs`: path-delimited list of directories containing the runtime library.
- `runtime_libs`: path-delimited list of library basenames (e.g., `souffle_runtime`).
- `runtime_rpaths`: path-delimited list of rpaths (shared libs only).
- `runtime_link_options`: extra link flags if needed (optional).

Example CMake injection sketch (in `src/CMakeLists.txt` where `SOUFFLE_COMPILE_PY` is generated):
```cmake
# Build the runtime library (static or shared).
add_library(souffle_runtime STATIC
  ${SOUFFLE_RUNTIME_SOURCES}
)
target_include_directories(souffle_runtime PUBLIC
  ${PROJECT_SOURCE_DIR}/src/include
)
target_link_libraries(souffle_runtime PUBLIC
  ${SOUFFLE_THIRD_PARTY_LIBS}
)

# Inject into JSON_DATA_TEXT.
set(SOUFFLE_RUNTIME_LIB_DIRS "${CMAKE_CURRENT_BINARY_DIR}")
set(SOUFFLE_RUNTIME_LIBS "souffle_runtime")
set(SOUFFLE_RUNTIME_RPATHS "${CMAKE_CURRENT_BINARY_DIR}")
set(SOUFFLE_RUNTIME_LINK_OPTIONS "")

# In the JSON literal:
# "runtime_lib_dirs": "${SOUFFLE_RUNTIME_LIB_DIRS}",
# "runtime_libs": "${SOUFFLE_RUNTIME_LIBS}",
# "runtime_rpaths": "${SOUFFLE_RUNTIME_RPATHS}",
# "runtime_link_options": "${SOUFFLE_RUNTIME_LINK_OPTIONS}",
```

### souffle-compile.template.py (pseudo patch)
Minimal parsing and append logic:
```python
runtime_lib_dirs = conf.get("runtime_lib_dirs", "")
runtime_libs = conf.get("runtime_libs", "")
runtime_rpaths = conf.get("runtime_rpaths", "")
runtime_link_options = conf.get("runtime_link_options", "")

runtime_lib_dirs = [p for p in runtime_lib_dirs.split(PATH_DELIMITER) if p]
runtime_libs = [l for l in runtime_libs.split(PATH_DELIMITER) if l]
runtime_rpaths = [p for p in runtime_rpaths.split(PATH_DELIMITER) if p]

# Add to link line (after objects; before third-party libs if static).
cmd.extend([LIBDIR_FMT.format(p) for p in runtime_lib_dirs])
cmd.extend([LIBNAME_FMT.format(l) for l in runtime_libs])
cmd.extend([RPATH_FMT.format(p) for p in runtime_rpaths])
if runtime_link_options:
    cmd.append(runtime_link_options)
```

### Ordering and static-vs-shared notes
- If the runtime library is static, place it *after* the generated objects and before other static deps that it requires.
- If the runtime library is shared, order is less strict, but `rpath` should include its directory for runs from build trees.
- Keep a fallback: if the JSON fields are empty, preserve the current header-only behavior.

### Build tree vs install tree
- Build tree: runtime library lives under the build dir (`cmake-build-*/src` or similar).
- Install tree: runtime library lives under `CMAKE_INSTALL_LIBDIR`.
- `souffle-compile.py` should consume exact paths from CMake so both layouts work.

### ABI and build-type concerns
- Generated code must link with a runtime library built with compatible C++ ABI and flags.
- If `souffle-compile.py -g` is used, decide whether it links to a debug build of the runtime or always to release.
- Consider a single canonical build type (e.g., `RelWithDebInfo`) to reduce mismatch risk.

### Template instantiation plan
- For template-heavy code (e.g., formula managers), keep templates in headers or add explicit instantiations for the concrete types in the runtime library.
- If you add explicit instantiation, expose `extern template` declarations in headers to avoid duplicate instantiations.

#### Practical guidance for this repo
- `FormulaManager` itself is an interface template; it can stay header-only with minimal cost.
- The heavy template sites are in `ForwardCompilation.h`, `RegionalIncremental.h`, and `cli/Cli.h` (templated on `NodeRef`).
- Concrete managers (`WeightedBDDManager` in `CuddManager.h`, `SddManager` in `SddManager.h`, `LogicFormulaManager`) are **not** templates; those are good early candidates to move into the runtime library `.cpp` files.
- If you want to move templated algorithms out of headers, you must explicitly instantiate them for the concrete `NodeRef` types used by generated code:
  - `BddNodeRef` (BDD path)
  - `SddNodeRef` (SDD path)
  - `LogicNodeRef` (if still used for formula transforms)
- I would **not** start by instantiating everything; begin with non-template code, then explicitly instantiate a narrow set of hot functions once you confirm the only `NodeRef` types in use.

#### Example explicit instantiation list (when ready)
For `ForwardCompilation.h` / `RegionalIncremental.h` / `Cli.h`:
- `buildFormulasCyclewise<NodeRef>`
- `buildFormulasIncCyclewise<NodeRef>`
- `buildFormulasIncRegionalCyclewise<NodeRef>`
- `IncrementalCLI<NodeRef>`

Then add in a `.cpp` within the runtime library:
```cpp
template class IncrementalCLI<BddNodeRef>;
template class IncrementalCLI<SddNodeRef>;
template void buildFormulasCyclewise<BddNodeRef>(...);
template void buildFormulasCyclewise<SddNodeRef>(...);
// ...and so on for the required functions.
```

And in headers:
```cpp
extern template class IncrementalCLI<BddNodeRef>;
extern template class IncrementalCLI<SddNodeRef>;
```

This reduces generated code compile time only after the template definitions are no longer needed in headers.

### Dependency surface
- Ensure the runtime library links the same third-party deps as today (`readline`, `CUDD`, `SDD`, `sqlite`, `zlib`, etc.).
- Generated programs should not have to re-specify those deps beyond `-l<runtime>`.

### Rollout strategy
1) Move non-template, high-impact headers (`DerivationGraph`, `ForwardCompilation`, `Pipeline`, `cli/Cli`) into `.cpp`.
2) Build runtime library and link it in `souffle-compile.py`.
3) Only then consider template-heavy areas, with explicit instantiation or header retention.

### Where to place the split .cpp files
Keep public headers under `src/include/souffle/...` and mirror that layout under `src/` for the new implementations. This keeps include paths stable and makes it easy to find a `.cpp` for each header.

Suggested mapping (examples):
- `src/include/souffle/cli/Cli.h` -> `src/cli/Cli.cpp`
- `src/include/souffle/problog/DerivationGraph.h` -> `src/problog/DerivationGraph.cpp`
- `src/include/souffle/problog/ForwardCompilation.h` -> `src/problog/ForwardCompilation.cpp`
- `src/include/souffle/problog/Pipeline.h` -> `src/problog/Pipeline.cpp`
- `src/include/souffle/problog/RuleManager.h` -> `src/problog/RuleManager.cpp`
- `src/include/souffle/problog/QueryManager.h` -> `src/problog/QueryManager.cpp`
- `src/include/souffle/problog/Rule.h` -> `src/problog/Rule.cpp`
- `src/include/souffle/Derivation.h` -> `src/Derivation.cpp`

If you want a clearer boundary, create a `src/runtime/` subtree and mirror the include layout there instead (e.g., `src/runtime/cli/Cli.cpp`, `src/runtime/problog/DerivationGraph.cpp`). The key is consistency: a predictable mapping helps CMake lists and future maintenance.

## Related commits
- `812ea4081` — docs(repo): refine README narratives
- `86b6c2379` — docs(readme): update precompile and quickstart notes
- `efb25a3a4` — Add refactor guidance for runtime library split
