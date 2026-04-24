# Regional Incremental Forward Compilation

`inc-regional` is the regional forward-compilation mode used by the incremental
AE branch. It reuses formulas outside the affected region and recomputes only
the selected region plus boundary calibration work.

## Source References

- [../../../src/include/souffle/CompiledOptions.h:191](../../../src/include/souffle/CompiledOptions.h#L191): public mode syntax.
- [../../../src/include/souffle/cli/Executor.h:276](../../../src/include/souffle/cli/Executor.h#L276): runtime dispatch to `inc-regional`.
- [../../../src/include/souffle/problog/ForwardCompilation.h:1960](../../../src/include/souffle/problog/ForwardCompilation.h#L1960): regional FC entry.
- [../../../src/include/souffle/problog/ForwardCompilation.h:2467](../../../src/include/souffle/problog/ForwardCompilation.h#L2467): regional orchestrator call.
- [../../../src/include/souffle/problog/RegionalIncremental.h:1201](../../../src/include/souffle/problog/RegionalIncremental.h#L1201): regional FC class.
- [../../../src/include/souffle/problog/RegionalIncremental.h:1535](../../../src/include/souffle/problog/RegionalIncremental.h#L1535): delta-reach dependency graph diagnostics.
- [../../../src/include/souffle/problog/formula/CuddManager.h:565](../../../src/include/souffle/problog/formula/CuddManager.h#L565): CUDD adaptive reordering initialization.

## Control Flow

After graph pruning, `inc-regional` receives an incremental view containing
delta-insert and delta-delete sets. The forward-compilation path:

1. handles deletion and rederive on the current graph view;
2. releases deleted probabilistic variable indices for reuse;
3. analyzes a delta-reachable regional scope for inserted formulas;
4. builds or reuses a dependency graph for that scope;
5. rebuilds region formulas;
6. calibrates boundary/output weights;
7. runs incremental weighted model counting.

## State Safety

Regional formulas are a persistent state. A later turn may request a consumer
that cannot safely read the currently regionalized state. The CLI tracks the
state class and falls back on the forward-compilation side when needed. The
semantic mode of the turn remains the requested one.

## Reordering

CUDD adaptive reordering is an implementation default. There is no public
reordering flag. Incremental turns create variables only for delta inserts when
possible, but adaptive reordering remains enabled through the CUDD manager.

## Diagnostics

Default runs only write probabilities. Use canonical selectors when collecting
extra AE material:

```bash
./compute -F input -D output --setmode inc-regional \
  --dump=dot,json,stat --profile-stage=inc,fc,wmc,inc-regional
```

`dot` and `json` expose the pruned graph, `stat` prints graph counters, `fc`
prints formula construction timing, `wmc` prints weighted-model-counting timing,
and `inc-regional` prints regional analysis and calibration diagnostics.
