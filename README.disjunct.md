# Disjunction / Strengthening Log

This file captures the strengthened side-channel dataset work and the
incremental correctness/segfault investigations for disjunction-heavy runs.

## Context
- Strengthening adds extra input facts to create more disjunctions for RAND.
- Target: make inc-regional/BDD updates observable beyond trivial node edits.
- Strengthened datasets live under `problog-benchmark/side_channel_inc_strengthen_*`.

## Recheck (2026-01-18): det-opt delta-union fix vs strengthened P12–P20
Rebuilt with the det-opt delta-union changes and reran inc1 (full vs inc-naive).

**Matches (full vs inc facts.prob):**
- P12, P13, P14: ✅ no mismatches.

**BDD failures (CUDD make*Balanced):**
- P15, P16, P18: abort in `Cudd_bddAnd` (`makeAndBalanced`).
- P17, P19: abort in `Cudd_bddOr` (`makeOrBalanced`).
- P20: run did not complete (no `facts.prob`).

**Size hints (BDD path):**
- P14 full: `CUDD vars created: facts=4752 edges=784 total=5536`, live_nodes ≈ 767,667.
- P15 full: `CUDD vars created: facts=10131 edges=1654 total=11785` before abort.
  (No JSON/log stage info due to abort.)

**SDD check (P15):**
- Full run with `--knowledge=sdd` completes without abort.
  This points to a CUDD/BDD path issue on larger strengthened cases.
  Suspect: BDD blow-up or manager sizing/initialization; no CUDD init change applied yet.

## Incremental FC correctness (non-det-opt)
- P12 strengthened runs initially produced severe inc/full delete mismatches
  (e.g., KEY_IND tuples flipped to 0 in inc but non-zero in full).
- Fixes applied in FC delete/rederive path:
  - Use valid nodes/edges for impact traversal.
  - Only treat explicit deleted facts as deleted facts (no impacted-node union).
  - Force node recompute during rederive if the node was overdeleted,
    even when the edge formula is unchanged.
  - Always record phase-changed nodes (even without --fc-profile) so rederive
    can see overdeleted nodes.
- Result: large 0 vs non-zero mismatches resolved.
- Remaining mismatch (P12 inc3): 9 KEY_IND tuples with tiny deltas
  (max |Δ| ~ 2.3e-3). This looks like a numerical/caching difference rather
  than a logical missing derivation; still needs a root-cause check.

## --det-opt segfault triage (P12)
Symptom:
- Incremental delete crashes with
  `Segmentation violation signal in rule: assign(FROM1,FROM2) :- binary_constant(FROM1,FROM2).`
- Baseline full/inc without deltas is OK.
- Commit-only (no delta) is OK.
- Deleting only probabilistic facts (RAND / KEY_SENSITIVE) is OK.
- Deleting deterministic facts triggers the crash.

Isolation results (P12, det-opt, delete-only):
- Crash reproduces when deleting any of:
  - binary_constant
  - load_assign
  - store_assign
- No crash when deleting:
  - share
  - andor_assign_right
  - xor_assign_left

Minimal repro (P12):
- `./compute -F input -D output_dbg --setmode inc --det-opt < /tmp/delta_det_only.txt`
- Where `/tmp/delta_det_only.txt` contains only deterministic deletes + `commit` + `q`.

Why the error message always points to binary_constant:
- In `compute.cpp`, the signal message is updated before each rule body.
- The crash happens after the `binary_constant` rule message is set,
  during the delta-union logic for `assign`, so the message points to
  that rule even when the actual failure is later in the function.

Hypothesis (most likely cause):
- `assign` is marked deterministic in det-opt (`det-relations.txt` shows det=1).
- In incremental delta-union for deterministic relations, rule-application
  sets are intentionally skipped.
- Later in the delta-union code, the generated C++ still dereferences
  `untypedTuple2DeltaDelta{Insert,Delete}RuleApplications[...]` without
  checking for null when `detRel == true`.
- This yields a null dereference once deterministic delta tuples exist.

Evidence:
- Generated `compute.cpp` shows:
  - `untypedDeltaDervTupleInsertRuleSet` is read from
    `untypedTuple2DeltaDeltaInsertRuleApplications` without detRel guard.
  - Subsequent code uses `->begin()` or iterates over the set.
- This code path runs only when deterministic deltas (e.g., load_assign
  deletes) populate `rel_inc_delta_derv_*_assign`.

Next step to fix:
- Adjust the synthesised delta-union logic for deterministic relations
  so it does not dereference null rule-app sets:
  - Either skip union/erase when `detRel` and treat the delta tuple as
    the sole change, or
  - Always allocate empty rule-app sets for detRel to satisfy the union code.

Fix applied (Synthesiser):
- Guard delta-union rule-app access behind `detRel == false`.
- Compute rule-app counts only when `detRel == false`; det relations now
  short-circuit to tuple insert/delete without touching rule-app sets.

Rerun result (P12, det-opt, inc3):
- Recompiled with `build/src/souffle` and `-o compute`.
- Incremental delete/insert completes without segfault.
- Full-vs-inc compare (delete/insert turns):
  - `iter1` (delete): 6 mismatches in `KEY_IND`, max |Δ| = 0.00229881.
  - `iter2` (insert): 0 mismatches.
  - Final `facts.prob`: 0 mismatches.

## Environment constraints
- `gdb` is blocked by ptrace restrictions in this environment, so stack
  traces were not available; triage relied on delta isolation and generated
  C++ inspection.

## det-opt inc3 timeout + missing input (P12)
Symptom:
- `--det-opt` incremental run timed out with an infinite requeue loop.
- Log shows repeated:
  `REQUEUE Missing input for edge Hyperedge[rule31,RAND(382),assign(1467,382)->RAND(1467)]`.

Root cause:
- For deterministic relations under `det-opt`, we skip rule-application sets.
- Seminaive still produces `@inc_delta_tuple_insert_*` tuples for deterministic relations,
  but `applyDeltaInserts()` ignores them unless they are in `fact_prob`.
- As a result, deterministic derived tuples (e.g., `assign`) are not inserted into the
  derivation graph’s delta node set, so their node formulas are missing.
- In insertion FC, edges that depend on those nodes requeue forever because inputs
  never become available.

Fix:
- In `IncrementalCLI::getFactProbInc()`, when `detOptEnabled`, scan
  `$inc_delta_tuple_insert_*` relations for deterministic base relations and
  add those tuples to `fact_prob` with probability `1.0`.
- Also ensure `getDeletedFacts()` includes deterministic delta deletes
  to keep delete handling symmetric (already applied).

Rerun result (P12, det-opt, inc3):
- `iter1` (delete): 0 mismatches.
- `iter2` (insert): 0 mismatches.
- Final `facts.prob`: 0 mismatches.

## P13 strengthened mismatch: delta-delete coverage vs mismatches (det-opt)
This section fixes the terminology confusion around “missing heads” and
records the P13 inc3 analysis in a precise, reproducible way.

**Definitions (all derived from dumpjson after-prune views):**
- **Removed rule-apps**: `baseline.rules − delete.rules` (same inc run, iter0 vs iter1).
- **Delta delete edges**: `delta.delete.edges` from dumpjson (direct deletions recorded by `applyDelta`,
  not a view diff).
- **Missing rule-apps**: `removed rule-apps − delta delete edges` (matched by head/body/prob).
- **Missing heads**: unique head tuples of missing rule-apps.
- **Missing heads still present**: missing heads that still appear in the delete view (i.e., head exists
  in iter1 rules).

**P13 inc3 (strengthened, det-opt) counts:**
- Removed rule-apps: **159** (RAND 150, KEY_IND 9).
- Delta delete edges: **48** (RAND 39, KEY_IND 9).
- Missing rule-apps: **117** (RAND 114, KEY_IND 3).
- Missing heads (unique): **61**.
- Missing heads still present: **2** (`RAND(3068)`, `RAND(5975)`).

**Mismatch set (iter1, delete turn):**
12 mismatched `KEY_IND` tuples:
`2798, 2796, 802, 800, 2790, 2788, 5319, 5322, 1381, 2692, 2695, 784`.

**Dependency closure results (baseline rule graph):**
- For 6 mismatches, the dependency closure **includes missing heads**:
  `KEY_IND(1381)`, `KEY_IND(2692)`, `KEY_IND(2695)`,
  `KEY_IND(5319)`, `KEY_IND(5322)`, `KEY_IND(784)`.
- For the remaining 6 mismatches, **no missing head appears in the dependency
  closure**:
  `KEY_IND(2798)`, `KEY_IND(2796)`, `KEY_IND(802)`,
  `KEY_IND(800)`, `KEY_IND(2790)`, `KEY_IND(2788)`.

**Implication:**
Mismatch sources are split:
1) A subset plausibly caused by missing delta rule-apps (the 6 above).
2) Another subset likely due to conditioning / impact update gaps rather than missing rule-apps.

**Artifacts (P13 output_missing2):**
- `missing-removed-vs-delta-delete.txt`
- `missing-removed-with-head-present.txt`
- `reachable-from-missing-heads.txt`
- `reachable-key-ind-from-missing-heads.txt`
- `reachable-key-ind-from-RAND3068-5975.txt`
- `reachable-vs-mismatch-all-missing-heads.txt`
- `reachable-vs-mismatch.txt`
- `mismatch-backward-missing-heads.txt`
