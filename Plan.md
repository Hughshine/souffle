# Plan

## Current questions
- Forward compilation still takes ~4s for tens of thousands of BDD nodes; find root cause.
- Keep README updates in sync with recent experiments and results.

## Status
### Done
- [x] After splitting components, prune isolated evidence-only components; maintain evidence + SCC.
- [x] Split respects evidence; if a candidate is evidence-affected, skip split.
- [x] Reviewed CycleDependencyGraph maintenance during rewrite; confirm cache validity.
- [x] Rewrite can emit results directly for is-fact outputs.

### In progress / next
- [ ] Prune still slow; optimize.

## Workstreams
### Full pipeline (full mode)
- [ ] FC fast path for all-conjunction components; avoid BDD independence overhead.
- [ ] Prune still slow (see above).
- [ ] Reordering tuning.
- [ ] Evaluation scripts / prompts:
  - side-channel
  - side-channel plus
  - data race +

### Incremental
- [ ] DRed fix.
- [ ] Data completion and cleanup.

## Evaluation plan (full compilation)
- Use `--full-only` to keep reordering consistent (sift).

1) Show derivation-graph generation is faster than ProbLog; separate `--derv-only` / `--ground` stage.
   - Run on side-channel benchmark.
   - (Optional) show ProbLog backward reasoning is poor on certain cases; compare `problog` vs `souffle --derv-only`.
2) Compare ProbLog vs Souffle (no rewrite vs rewrite): time + size (node count); highlight cases that previously failed (memory).
3) Explain WMC complexity scales with node count.
4) Show end-to-end time improvements across stages.

## Evaluation plan (incremental compilation)
- Precompute output facts to avoid traversing all nodes; only traverse output facts.

## Docs / README updates
- Add CUDD and SDD setup instructions in README (use Dockerfile snippet as reference):
  ```bash
  # CUDD (new org) with autoreconf
  WORKDIR /opt
  RUN git clone --depth 1 https://github.com/cuddorg/cudd.git /opt/cudd \
   && cd /opt/cudd \
   && autoreconf -fiv \
   && ./configure --enable-shared --prefix=/usr/local CFLAGS="-O3 -fPIC" CXXFLAGS="-O3 -fPIC" \
   && make -j"$(nproc)" && make install && ldconfig

  # SDD++ (bundles SDD 2.0)
  RUN git clone --depth 1 https://github.com/black-sat/sddpp.git /opt/sddpp \
   && cmake -S /opt/sddpp -B /opt/sddpp/build \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
   && cmake --build /opt/sddpp/build -j"$(nproc)" \
   && cmake --install /opt/sddpp/build && ldconfig
  ```
- Add a running example folder (simple case, compile+run script, explanation of options and outputs).
- Include a `.gitignore` for example outputs.

## TODO (carry-over)
1) Souffle compile is not only `souffle -o compute`; check `side_channel_inc.py compile` command. Add to README if it can be used outside the script.
2) Inc-regional stage 3 still slower than full/inc-naive in old results; profile rebuild and analyze sub-stages, including CUDD reordering time.
3) DRed delete is slow; summarize current DRed implementation (mostly in synthesised C++) and assess optimization space.

- Rebuild and run after changes; check consistency and report timings.
- Confirm results remain consistent.
- Full mode has two variants: full-hard (default) vs full-soft; `setmode full` uses full-hard; hard uses `resetHard`, soft uses `reset` with different options.
- Run full-hard, inc-naive, inc-regional on P12-P15; extract per-turn stage timings (see `README.profile.md`).

## Profiling snapshot
- [inc-analyze] timing(ms): leastParents=14.331027 scopes=8.799561 reach=2.357730 initRegion=2.527037 classify=1.346152 expand=0.041705 upstreamClose=2.195190 reclassify=0.000000 reexpand=0.000000 deltaReach=8.253815 intersect=0.273988 mergeable=0.000189 total=40.126394

## Historical notes (archived, not current)
The notes below are preserved as historical brainstorming. They do not reflect
current behavior and are not actionable without revalidation.

### SISO rewrite questions
- Is the complexity of rewriting SISO related to subgraph size or full graph size?
- For P13, can we still guarantee consistency between facts.prob, no rewrite, and rewrite?

### SISO detection adjustments
- [x] Define the simplest SISO (two nodes, one edge) with stricter constraints: si has no other outgoing edges and so has no other incoming edges.
- [ ] Detection should control target SISO size/type.
- [ ] Detection should mark other special (trivial/etc.) SISOs for fast paths.
- [ ] Single-conj SISO + input facts combinations (verify current handling).

### Analyzer fast paths (historical request)
- Archive the old analyzer logic without changing the interface exposed to the rewriter.
- Implement a fast path that only detects SISOs with at most two edges:
  - One-edge SISOs: pure two-node or general single hyperedge.
  - Two-edge SISOs: linear (a/SI -> b -> c/SO) or parallel (a -> b / a -> b).
  - Also consider a1 -> b / a2 -> b where a1/a2 are input facts (all facts as input).
- Add region kinds for these fast paths and a detector that avoids the heavy logic.

### Linear two-edge rewrite note
- Replace two edges and the middle node with a single edge; middle node must satisfy SISO constraints.
- New edge probability is the product of the previous two edges' probabilities.

### Rewrite optimizations (historical)
- Lazy-initialize DD manager only when needed (already done).
- Consider on-the-fly analyze + rewrite.
- Explore simplification that absorbs lone leaf nodes into edges.
- Optimize all-AND subgraphs.

### Negation handling (historical)
- Edge semantics depend on body negations; ensure rewrite always preserves negation.
- If the edge connected to SI is negated in linear two-edge, the new edge should mark SI as negated; if the second edge is negated, skip fast path.
- Ensure all rewrite logic handles body negations consistently.

### Evaluation notes (historical)
- Add output for variable reordering time in BDD build timing.
- Unify how debug mode is enabled.
- Run P5/P9/P12; compare rewrite vs no-rewrite results, stage timings, and final BDD node counts.

### Engineering ideas (historical)
- When graphs are small, reduce BDD manager initialization memory.
- Avoid final forward compilation by directly computing query node formulas.
- Consider detect + rewrite on the fly.
