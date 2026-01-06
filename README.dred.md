# README: Online DRed (Derivation Graph Incremental) Implementation and Performance Notes

- Covers only the online incremental path (default; `--online` optional), which is the only enabled implementation.
- The old `--inc` incremental backend is removed; this file does not discuss it.
- Focuses on semi-naive + DRed-like deletion/rederive/insertion; rewrite/forward compilation are covered in other docs.

## Status
- Active implementation and performance note for online DRed.

## Online DRed flow (code-level overview)
1) CLI parses deltas and writes to `$inc_delta_tuple_{insert,delete}_*`; deletes do not directly erase from base relations.
2) Generated `runFunctionInc()` calls `inc_table_update` first, then executes each `_inc` stratum in order.
3) `inc_table_update`: clear `@old_*` then copy base; clear `@inc_tuple_overdelete_*` / `@inc_derv_overdelete_*`; copy `$inc_delta_tuple_delete_*` -> `$inc_delta_derv_delete_*` as overdelete seeds.
4) Deletion fixpoint: record derivation deletions -> produce `@inc_*_overdelete`.
5) Rederive fixpoint: generate `@inc_new_derv_rederive_*` + `@inc_delta_tuple_rederive_*`, subtract re-derivable tuples from overdelete.
6) Insertion fixpoint: process `$inc_delta_tuple_insert_*`, recursive strata will generate delta-delta rule applications; non-recursive strata typically use purge+scan rebuild.

## 1. Goals and background

This module implements DRed-like incremental semi-naive evaluation to generate/maintain the derivation graph (tuple derivations, rule application records, etc.). Current observations:

- Deletion updates are correct, but significantly slower (even an order of magnitude slower than full recomputation).
- Possible reasons include: algorithmic overhead from over-deletion + inefficient C++ codegen/runtime (memory leaks, container misuse, relations not cleared causing repeated scans, etc.).

**Purpose of this README**: summarize implementation logic and organize a path to locate performance bottlenecks, aiming to explain why deletion is slow and provide verifiable optimization directions:
1) Identify the main bottlenecks of deletion slowness (CPU / memory / containers / codegen strategy).
2) Use instrumentation and tools (perf/asan/lsan/heap profiler) to quantify bottlenecks.
3) Propose/implement verifiable optimizations or fixes (prioritize root causes of order-of-magnitude slowdowns, such as leaks and missing clears).

## 2. Terms (for alignment)

- **Complete derivations**: the full set of rule applications deriving a tuple.
- **Delta/DeltaDelta**: newly added/removed derivations in an incremental round, merged into complete derivations.
- **Over-deletion**: in DRed, delete too much first to ensure correctness, then re-derive.
- **Re-derive / re-insertion**: re-derive tuples that may still have support.
- **Stratum / Recursive stratum**: recursion layers; recursive strata amplify incremental propagation cost.

## 2.1 Online incremental relation names (key prefixes)
- `$inc_delta_derv_{insert,delete}_*`: tuples for derivation changes (rule application changes).
- `$inc_delta_tuple_{insert,delete}_*`: actual tuple insert/delete.
- `@inc_tuple_overdelete_*` / `@inc_derv_overdelete_*`: overdelete sets.
- `@inc_new_derv_rederive_*` / `@inc_delta_tuple_rederive_*`: incremental derivations during rederive and tuples that still survive.
- `@old_*`: cached old results in `inc_table_update`; non-recursive `_inc` logic uses it to rebuild base.
- `@new_*` / `@delta_*`: standard semi-naive relations (still present in online incremental).

## 3. Code entry points and reading guide (must-read)

> Main analysis targets are online translation and C++ code generation: `src/ast2ram/online/*` and `src/synthesiser/Synthesiser.cpp`.

Read in the following order to cover key hotspots on the deletion path:

### 3.0 Online pipeline and naming rules
- `src/ast2ram/online/UnitTranslator.cpp`: generates `inc_table_update` and `_inc` strata; creates `@inc_*` / `$inc_*` relations.
- `src/ast2ram/online/IncClauseTranslator.cpp`: rederive clause and RAM generation.
- `src/ast2ram/utility/Utils.cpp`: incremental relation name prefixes (overdelete/rederive, etc.).
- `src/ast2ram/utility/TranslatorContext.cpp`: online translation strategy is hard-wired (no flag gating).

### 3.1 `visit_(Clear)`: whether relation clearing semantics are correctly implemented
Search for key functions:

- and the comment `// TODO: cannot purge real relations now, but incremental computation might need this`

Focus on:
- **Only temp relations are purged**; `Clear` on non-temp relations is currently a no-op (potentially causing incremental auxiliary relations to not be cleared, slowing down over time).

### 3.2 `visit_(RecordDerivation)`: cost and strategy for recording derivations (insert/delete)
Search for key functions:

Focus on deletion branch:
- Whether there is runtime recursion checks + scanning complete derivations and copying logic.
- Whether each deletion scans the entire `ruleSetComplete`, causing O(|complete|) blow-up.

### 3.3 `visit_(DeltaUnion)`: merge delta derivations into complete (and possible memory leaks)
Search for key functions:

Focus on:
- After merging, only `map.clear()` is called while values are raw pointers (`new unordered_set`), causing leaks.
- Erase cost in deletion branch may be high (hash/eq may involve vectors).

### 3.4 Relation API / erase limitations (affect deletion strategy)
- `src/include/souffle/SouffleInterface.h`: Relation interface only has `insert/contains/purge`, no tuple erase.
- `src/synthesiser/Relation.cpp`: only `RelationRepresentation::BTREE_DELETE` generates `erase()`.
- `src/ast2ram/online/UnitTranslator.cpp`: base relation defaults to `BTREE_DELETE`, but all auxiliary relations prefixed with `@`/`$` are downgraded to `DEFAULT`; only `@inc_tuple_overdelete_*` retains `BTREE_DELETE`.
- Result: most incremental auxiliary relations can only purge/rebuild, making deletion path rely more on scans and rebuilds.

### 3.5 Generated C++ evidence (P12 compute.cpp)
- Relation type naming `t_btree_{hasErase}{hasAux}{hasProv}_...`; `100` means `btree_delete_set` (has `erase()`), `000` is normal `btree_set`.
- In P12, base relations and `@inc_tuple_overdelete_*` are `t_btree_100...`; most `$inc_delta_*` / `@old_*` / `@inc_derv_overdelete_*` / `@inc_*_rederive_*` are `t_btree_000...` (no `erase()`).
- `inc_table_update`: `purge(@old_*)` then copy base; `ExactClear(@inc_tuple_overdelete_*)` / `ExactClear(@inc_derv_overdelete_*)`; `$inc_delta_tuple_delete_*` -> `$inc_delta_derv_delete_*`.
- Non-recursive `_inc` strata use purge+scan rebuild for base (`@old_*` minus delete + insert delta), even if base itself supports `erase()`.

## 4. High-priority suspicious list (ordered by likelihood of order-of-magnitude impact)

> Recommend "disprove or confirm" each item and produce data (log/trace/profile).

### S1. Clear on non-temp relations does not purge (delta/aux relations may grow unbounded)
Matching symptoms:
- Deletion requires multiple rounds of propagation/recovery/rederive, scanning delta/intermediate relations each round;
- If these relations are not cleared, old data is scanned repeatedly, causing exponential/order-of-magnitude blow-up.

Validation actions:
1) Find the set of relation names that get `Clear` in the deletion flow.
2) Print at runtime: `relationName`, `isTemp`, `size before`, `size after`.
3) Check whether delta relations that should be cleared are marked non-temp (causing no-op).

Possible fixes (choose one):
- Mark incremental auxiliary relations as `temp` in RAM/meta
- Or replace corresponding `Clear` with `ExactClear`
- Or allow purge in codegen for specific prefixes (e.g., `@delta_`, `$inc_delta_`, `@inc_`) with extreme caution to avoid clearing base/output by mistake

### S2. DeltaUnion / DerivationManager uses raw pointer sets, clear without delete: likely memory leak
Matching symptoms:
- Deletion produces more deltas (overdelete/rederive), more set allocations;
- Leaks cause memory growth, cache misses, allocator pressure, and eventual performance cliff.

Validation actions (must do):
1) Run minimal repro with deletion under AddressSanitizer + LeakSanitizer:
2) Or use heap profiler (heaptrack/valgrind massif) to observe growth across iterations.

Key points to locate:
- Does `RecordDerivation` do `new std::unordered_set<RuleApplication>()`?
- Does `DeltaUnion` end with only `map.clear()` (no freeing of sets)?

Fix suggestions (highest priority):
- Change map value to `std::unique_ptr<std::unordered_set<RuleApplication>>`
- Or explicitly traverse and delete before `clear()` (watch "handoff" branches to avoid double-free)

Deliverable requirements:
- After the fix, LSan reports should significantly drop/disappear;
- Deletion performance should improve materially (often immediately).

### S3. Deletion branch has O(|complete|) hotspots scanning complete derivations and copying
Matching symptoms:
- For tuples with large derivation counts, each deletion triggers full scans;
- Recursive propagation multiplies this, making it slower than full recompute.

Validation actions:
1) Instrument:
   - Count of scans of `ruleSetComplete` per deletion, total elements scanned;
   - Clause/ruleId distribution that triggers scans.
2) Use perf top/hotspot to confirm time spent iterating set/vector/hash.

Optimization directions:
- Make recursion checks compile-time constants where possible (avoid runtime `isInRecursiveStratum(ruleId)`)
- Scan only at necessary points (e.g., support count drops to zero / entering overdelete)
- Maintain recursive-only index to avoid scanning full complete set

### S4. `unordered_set.erase(RuleApplication)` hash/eq may be heavy (RuleApplication carries vector)
Matching symptoms:
- Deletion path uses many erase operations;
- If RuleApplication keys include `std::vector` (varValues), hash/eq iterate and become slow.

Validation actions:
1) Inspect `RuleApplication` structure and hash/== implementation;
2) Use perf to see hotspots in hash/equality and vector access.

Possible optimizations:
- `varValues.reserve(N)` (N is compile-time constant) to reduce small vector allocations/growth
- Use a lighter key (interned id / fingerprint hash) instead of "vector as key", or implement faster vector hashing

### S5. `std::unordered_map::operator[]` implicitly inserts empty entries, causing overhead
Symptoms:
- Deletion uses `operator[]` to read maps, which inserts if key missing;
- Can cause rehash/growth and extra allocations.

Validation actions:
- Search for `[...]` access patterns;
- Replace with `find()` and skip/assert on missing keys.

## 5. Recommended debugging and performance workflow

> Suggested order: confirm S1/S2 first (most likely order-of-magnitude causes), then S3/S4/S5.

### Step A -- Establish minimal repro and baseline data
1) Choose a workload that reliably reproduces "deletion is an order of magnitude slower than full":
   - Fixed data size and update pattern (how many tuples/facts deleted)
   - Fixed thread count and build mode (recommend `RelWithDebInfo`)
2) Record baseline:
   - Full recompute time (T_full)
   - Incremental deletion time (T_del)
   - Memory peak (RSS peak)
   - Number of iterations (if any)

Output format (recommended to write to `bench_results.md`):

### Step B -- Instrumentation (minimal intrusion)
Add togglable macros, e.g., `#ifdef INC_DEBUG`:
- On Clear: print relationName/isTemp/size before/size after
- On RecordDerivation/DeltaUnion: print
  - count of newly allocated sets, set size distribution
  - count of scans of complete derivations and total elements scanned
- Output to stderr or separate log file (to avoid affecting main output)

### Step C -- Leak/heap checks (must do)
1) Run minimal repro with ASan/LSan:
   - If leaks exist: locate call stack, fix S2 first
2) Run larger repro with heap profiler:
   - Observe whether set/vector/tuple allocations grow across iterations

### Step D -- perf/hotspot profiling
1) Use perf/pprof/hotspot to identify CPU hotspots:
   - See whether hotspots land in:
     - vector allocations/copies
     - hash/eq
     - map/set scans
2) Output flame graph or hotspot function list to `profile_notes.md`

### Step E -- Targeted fixes and verification
Fix in the order S1 -> S2 -> S3 -> S4 -> S5; after each fix:
- Re-run Step A baseline and compare:
  - whether T_del drops
  - whether RSS_peak drops
  - whether T_del / T_full ratio improves
- Ensure correctness does not regress (existing tests or output diff)

## 6. Suggested deliverables

- Repro steps and baseline data
- Evidence for main bottlenecks (log/profile/asan)
- Fix strategy and rationale
- At least one verifiable fix from S1 or S2 (prefer S2 leak)
- Instrumentation macros (optional, but recommended)
- Before/after comparison data (at least 3 repeats with mean/variance)

## 7. Common pitfalls and notes

- Instrumentation logs affect performance; they are sufficient for order-of-magnitude diagnosis, but disable debug macros for final benchmarks.
- If you find relations not purged (S1), do not blindly purge all non-temp relations; use a whitelist or mark temp upstream / use ExactClear.
- When fixing S2, watch pointer ownership: some delta sets are assigned directly to complete sets, so you cannot delete them.
  - Prefer `unique_ptr` + move semantics to avoid double-free.
- Deletion in recursive strata is often more complex than insertion; fix memory and Clear semantics first, then algorithmic optimizations.

## 8. Required source file list (add if missing)

To fully diagnose S2/S4, also inspect:
- `DerivationManager` definition (map type, whether value is raw pointer)
- `RuleApplication`, `UntypedTuple` structures and hash/== implementations
- RAM generation logic for temp markers on incremental auxiliary relations (or ExactClear generation)

## 9. Definition of Done

- Correctness: deletion incremental results match full recompute (align with existing assertions/regression tests)
- Performance: for the same workload, T_del is no longer an order of magnitude slower than T_full
- Memory: LSan reports no obvious leaks; RSS does not grow abnormally across iterations
- Documentation: analysis_report + bench_results are complete and reproducible
