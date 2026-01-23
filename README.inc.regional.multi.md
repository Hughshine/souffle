# Multi-Turn inc-regional State Management (Insert/Delete)

## Source references
- [src/include/souffle/problog/RegionalIncremental.h](src/include/souffle/problog/RegionalIncremental.h)
- [src/include/souffle/problog/ForwardCompilation.h](src/include/souffle/problog/ForwardCompilation.h)
- [src/include/souffle/problog/IncRegionAnalyzer.h](src/include/souffle/problog/IncRegionAnalyzer.h)


Status: design notes / reasoning only (no code changes yet).

## Goal
Enable multiple incremental updates while keeping inc-regional correct and fast.
Key challenge: after a regional insert, some nodes keep old BDDs that were only
"corrected" via calibration during WMC. The next turn must account for those
stale formulas.

## Terms
- Region: nodes rebuilt this turn (BDD formulas updated).
- Boundary: nodes used to calibrate region/outside interaction.
- Delta-reachable (DR): nodes/edges reachable from delta changes.
- Stale formula: BDD does not reflect current derivations/structure.
  (Calibration fixes weights, not structure.)

## Invariants to Preserve
1. Any node whose derivation structure changed since its BDD was built must be
   rebuilt before it is trusted as a formula in later turns.
2. Calibration can adjust boundary probabilities but cannot add/remove missing
   derivations inside a BDD.
3. WMC for region-outside nodes is correct only if their formulas match the
   current derivation structure.

## Insert -> Insert (multi-turn)
Observation: successive inserts can be treated as cumulative delta, but only if
all nodes whose structure changed are rebuilt at least once.

### Strategy A: Stale-Structural Tracking (recommended baseline)
Maintain a set of stale nodes whose formulas are outdated.

- On each insert turn:
  1) Build `newDR` from this turn's delta.
  2) `initialRegion = staleStructural ∪ newDR`.
  3) Run analyze/expand to fixpoint within DR.
  4) Rebuild formulas for region.
  5) Calibrate boundaries; outside region uses old BDD + calibrated weights.
  6) Remove rebuilt nodes from `staleStructural`.

This allows old BDD reuse while ensuring structure-changed nodes are rebuilt.

### Strategy B: Accumulated Region (aggressive)
Keep `prevRegion` and include it in each new `initialRegion`.

Pros: simplifies correctness (previously rebuilt nodes are always included).
Cons: region grows quickly; can over-expand and lose the speedup.

### Strategy C: Threshold Fallback
If `|region| / |DR|` (or `|region| / |allNodes|`) exceeds a threshold (e.g. 0.95),
fall back to inc-naive (full DR rebuild) for this turn. This keeps performance
predictable when region becomes almost everything.

## Insert -> Delete
Delete invalidates formulas by removing derivations. Calibration cannot fix a
BDD that still contains removed derivations.

Guidelines:
- If delete DR intersects stale nodes or the current region, prefer a classic
  rebuild on delete DR (or inc-naive). This is the safe default.
- If delete DR is provably disjoint from prior stale regions (no dependency
  overlap), it might be handled as an isolated regional delete (still requires
  structural rebuild on its DR).
- After delete, reset any calibration state; do not carry weight overrides
  forward without revalidation.

## Relationship Between Delta Regions
Let `DR1` (previous stale region) and `DR2` (current delta region):
- If independent: regions can be processed separately, reuse old BDDs outside
  both regions, calibrate per turn.
- If overlapping or dependent: must merge and rebuild the union; otherwise
  formulas could depend on stale structure across regions.

Dependency here is structural: overlapping backward-reachable closure or shared
incoming influence into the same boundary scopes.

## When to Force Full Refresh
Full refresh (inc-naive or full) is appropriate when:
- Region nearly equals DR or full graph.
- Calibration cost rivals rebuild cost.
- Too many turns have accumulated and stale tracking is uncertain.

## Open Questions / Future Work
- Efficiently tracking structural changes (stale nodes) without over-approximating.
- Fast dependency/overlap detection between successive DRs.
- How to combine calibration across turns if we ever want persistent overrides.

## Summary
Multi-turn insert can be efficient if we track structural staleness and ensure
all structure-changed nodes are rebuilt at least once. Calibration alone is not
sufficient for structural updates, but it enables reuse of old BDDs outside the
region once structure correctness is restored. Delete turns require stricter
rebuild rules.

## Related commits
- (no git history yet; uncommitted/new file)
