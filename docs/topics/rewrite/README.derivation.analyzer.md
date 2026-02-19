# Derivation Graph Analyzer (`sh/analyze_derivation_graph.py`)

## Source references
- [sh/analyze_derivation_graph.py](sh/analyze_derivation_graph.py)
- [docs/topics/runtime/README.flag.md](docs/topics/runtime/README.flag.md)
- [docs/topics/runtime/README.dump.md](docs/topics/runtime/README.dump.md)

This document describes the standalone derivation-graph analyzer script:
`sh/analyze_derivation_graph.py`.

It is intended for quick structural inspection of `--dumpjson` outputs, SISO
exploration, and graph-only rewrite simulation (without BDD/SDD probability
evaluation).

## Scope
- Inputs:
  - full derivation JSON from Souffle `--dumpjson` (with `facts` + `rules`)
  - lightweight graph summary JSON from `--dumpstat` (summary-only mode)
- Outputs:
  - console summary (graph stats, SCC, SISO/rewrite/explore summary)
  - optional report JSON (`--out-json`)
  - optional interactive HTML (`--interactive`, default on)

## Key capabilities
- Graph stats and SCC summary.
  - includes predicate-level dependency summary and strata metrics:
    - `strata_count`: predicate-SCC count in dependency graph
    - `negation_strata_count`: negation-stratification level count
- General SISO detection (`global` / `cone` mode).
- Souffle-supported fast-path SISO detection.
- Graph-only rewrite fixpoint simulation:
  - fast-path rewrite
  - optional split (`none|naive|complete`, default `naive`)
  - optional compaction pass (enabled by default)
- Random subgraph exploration samples.
- Interactive viewer with:
  - rewrite state stepping (`Rewrite +1`, `Rewrite to End`, `Undo`, `Reset`)
  - on-demand `Detect General SISO`
  - source switching (`full-graph`, `fast-path`, `general`, `explore`)
  - node search/locate in full graph (`Locate (Full Graph)`, `Next Match`)
  - `Explore Samples` follows the currently selected rewrite state (not only the original graph)
  - current full-graph header shows predicate/strata summary
  - current region metadata shows subgraph-level strata summary

## Important behavior notes
- This analyzer does **not** run weighted model counting.
- `--rewrite-fixpoint` is graph-only simulation (default: on).
- In interactive mode with rewrite states, `Rewrite to End` jumps to the last computed
  state (`iterations_run`); `max_iterations` is only an upper bound.
- Interactive mode defaults to full-graph source and writes `/tmp/explore.html`
  unless `--interactive-html` is provided.
- `general-siso` output excludes trivial one-edge regions with at most one
  non-fact driver.

## Quick start

### 1) Analyze a derivation dump and emit interactive HTML
```bash
python3 sh/analyze_derivation_graph.py \
  /path/to/derivation.json \
  --interactive \
  --interactive-html /tmp/explore.html
```

### 2) Also save a machine-readable report JSON
```bash
python3 sh/analyze_derivation_graph.py \
  /path/to/derivation.json \
  --out-json /tmp/analyze_report.json \
  --interactive-html /tmp/explore.html
```

### 3) Disable rewrite simulation
```bash
python3 sh/analyze_derivation_graph.py \
  /path/to/derivation.json \
  --no-rewrite-fixpoint \
  --interactive-html /tmp/explore_no_rewrite.html
```

### 4) Tweak rewrite split mode
```bash
python3 sh/analyze_derivation_graph.py \
  /path/to/derivation.json \
  --rewrite-split-mode complete \
  --interactive-html /tmp/explore_complete_split.html
```

## Input expectations
- For full analysis, input JSON must contain:
  - `facts` (list)
  - `rules` (list)
- For summary-only mode (`--dumpstat` JSON), the script prints summary data and
  skips interactive per-region rendering.

## Useful options
- General SISO:
  - `--general-siso-mode {off,auto,global,cone}`
  - `--max-dom-nodes`, `--max-candidates`, `--cone-depth`, `--cone-node-limit`
- Fast-path SISO:
  - `--supported-siso-mode {on,off}`
- Rewrite simulation:
  - `--rewrite-fixpoint / --no-rewrite-fixpoint`
  - `--rewrite-split-mode {none,naive,complete}`
  - `--rewrite-max-iterations`
  - `--rewrite-no-compaction`
- Interactive/explore:
  - `--interactive / --no-interactive`
  - `--interactive-html /tmp/explore.html`
  - `--explore-mode ...`
  - `--include-all-sources / --no-include-all-sources`

## Interactive usage notes
- Use `Rewrite to End` to jump directly to final rewrite state.
- `Detect General SISO` is explicit/on-demand per rewrite state.
- Node search is intended for full-graph inspection:
  - enter atom name (substring match supported)
  - `Locate (Full Graph)` switches to full graph and focuses first match
  - `Next Match` cycles matches

## Typical pipeline from Souffle runtime
1. Run Souffle with `--dumpjson` (and optionally `--derv-only`) to generate
   `derivation.json`.
2. Run this analyzer script on that JSON.
3. Open generated HTML (`/tmp/explore.html`) for stepwise visual inspection.

## Related commits
- `UNCOMMITTED` — docs(rewrite): add derivation analyzer usage guide
- `UNCOMMITTED` — feat(analyzer): interactive rewrite controls and node search
