# Forward-Compilation Profiling Notes

## Scope
- Focus: forward compilation (FC) in full and incremental modes.
- Output: all FC profiling is printed to stdout when `--fc-profile` is enabled.
- Intended for diagnosing FC slowdowns (pre-config, delete/rederive, insert loop).

## Enable
Runtime flag (no compile-time toggle):
```
./compute -F input -D output --setmode inc --fc-profile
```

Optional flags commonly used in experiments:
```
./compute -F input -D output --setmode inc --det-opt --fc-profile
```
To enable post-delete variable postprocess (off by default):
```
./compute -F input -D output --setmode inc --fc-profile --post-del
```

## Output Lines
### Full FC
`[fc-profile] stage=FORWARD_COMPILATION_FULL ...`
- `total_ms`, `const_ms`, `preConfig_ms`, `depGraph_ms`, `baseInit_ms`, `cycles_ms`
- `nodes`, `edges`, `fact_nodes`, `det_edges`, `nondet_edges`
- `edge_processed`, `edge_requeued`, `edge_updated`, `edge_const`, `edge_nonconst`
- `node_recomputed`, `node_updated`, `node_const`
- `make_and_calls/ms`, `make_or_calls/ms`, `make_condition_calls/ms`
- `input_literal_calls/missing/ms`

### Incremental FC (summary)
`[fc-profile] stage=FORWARD_COMPILATION_INC ...`
- `total_ms`, `dep_ms`, `const_ms`
- `view_nodes`, `view_edges`
- `del_nodes`, `del_edges`, `ins_nodes`, `ins_edges`
- `det_fact_del`, `nondet_fact_del`
- `det_imp_nodes/edges`, `nondet_imp_nodes/edges`, `nondet_only_nodes/edges`
- `del_nondet_vars`

### Incremental FC (phases)
`[fc-profile] stage=FORWARD_COMPILATION_INC phase=...`
- `delete_condition`: conditioning on deleted non-deterministic facts  
  `ms`, `make_condition_calls/ms`, `node_updated`, `edge_updated`
- `delete_overdelete`: set det-impacted formulas to False  
  `ms`, `det_imp_nodes`, `det_imp_edges`
- `delete_varorder`: variable ordering cleanup  
  `ms`, `collect_ms`, `postprocess_ms`, `dump_ms`, `deleted_vars`
- `rederive`: re-derive impacted formulas  
  `ms`, edge/node counters, `make_and/or/condition` + `input_literal` stats
- `insert_init`: insert preparation  
  `preConfig_ms`, `init_nodes_ms`, `init_edges_ms`, `insert_fact_vars`, `insert_edge_vars`
- `insert_init_nodes`: finer node init breakdown  
  `fact_true_ms`, `fact_var_ms`, `fact_weight_ms`, `nonfact_ms`, plus counts
- `insert_init_edges`: finer edge init breakdown  
  `setfalse_ms`, `var_ms`, `weight_ms`, `enqueue_ms`, `det_edges`, `nondet_edges`
- `insert_loop`: incremental propagation  
  `ms`, edge/node counters, `make_and/or/condition` + `input_literal` stats

### CUDD preConfig
`[fc-profile] stage=CUDD_PRECONFIG tag=...`
- `tag`: `full_cyclewise`, `inc_insert`, or `full_ondemand`
- `total_ms`: total preConfig wall time
- `cache_ms`: `wmcCache_.clear()`
- `fact_loop_ms`, `edge_loop_ms`: full loop time
- `fact_create_ms`, `edge_create_ms`: time spent inside `createVar(...)`
- `reorder_ms`: adaptive reordering time
- `old_var_size`, `new_var_size`: CUDD manager size before/after
- `fact_vars`, `edge_vars`: probabilistic variable counts scanned

### CUDD createVar
`[fc-profile] stage=CUDD_CREATEVAR tag=...`
- Emitted once per `createVar(...)` call during preConfig (tagged `full_cyclewise`,
  `inc_insert`, or `full_ondemand`).
- Fields:
  - `kind` (`fact`/`edge`/`index`), `index`
  - `hit`: `1` for cached var lookup, `0` for new var creation
  - `lookup_ms`: unordered_map lookup time
  - `stats_before_ms`, `stats_after_ms`, `stats_ms`: time spent reading CUDD stats
  - `copy_ms`: cached-return time (hit only)
  - `ith_ms`, `wrap_ms`, `insert_ms` (miss only)
  - `total_ms`: total time for the call

### CUDD createVar stats (GC/reorder/resize)
`[fc-profile] stage=CUDD_CREATEVAR_STATS tag=...`
- Emitted alongside `CUDD_CREATEVAR` for `hit=0` to capture pre/post CUDD stats
  around `Cudd_bddIthVar(...)`.
- Fields include: `gc_*`, `reorder_*`, `swap_*`, `node_*`, `dead_*`,
  `slots_*`, `used_slots_*`, `keys_*` with `_before/_after/_delta`.
- Interpretation:
  - `gc_delta>0` indicates a GC occurred during the call.
  - `reorder_delta>0` or `swap_delta>0` indicates reordering/swap work.
  - `slots_delta>0`/`used_slots_delta>0` suggests unique table resize/rehash.

## Interpretation Notes
- `insert_preconfig_ms` almost entirely comes from CUDD `createVar(...)` loops; the
  `reorder_ms` and `cache_ms` components are typically negligible.
- `delete_condition_ms` is expected to be near zero when
  `nondet_imp_nodes/edges` and `nondet_only_nodes/edges` are zero. In that case,
  conditioning does not run on any node/edge.
- `insert_loop_ms` is commonly dominated by `make_and_ms` (OR is often zero in these
  cases), which points to AND-chain construction as the hot path.

## Known Performance Implication
- The CUDD preConfig step is a full scan over probabilistic facts and edges. If
  variable indices are not stable across turns, `createVar` can repeatedly expand
  the CUDD manager (visible as `new_var_size >> old_var_size`), which drives most
  of `insert_preconfig_ms`.
