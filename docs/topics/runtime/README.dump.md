# Dump and Debug Outputs Reference (souffle + ./compute)

## Source references
- [src/MainDriver.cpp](src/MainDriver.cpp)
- [src/reports/DebugReport.cpp](src/reports/DebugReport.cpp)
- [src/ast/transform/DebugReporter.cpp](src/ast/transform/DebugReporter.cpp)
- [src/ram/transform/Transformer.cpp](src/ram/transform/Transformer.cpp)
- [src/include/souffle/CompiledOptions.h](src/include/souffle/CompiledOptions.h)
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)
- [src/include/souffle/problog/Pipeline.h](src/include/souffle/problog/Pipeline.h)
- [src/problog/Pipeline.cpp](src/problog/Pipeline.cpp)
- [src/include/souffle/problog/DerivationGraph.h](src/include/souffle/problog/DerivationGraph.h)
- [src/include/souffle/problog/GraphRewriter.h](src/include/souffle/problog/GraphRewriter.h)
- [src/include/souffle/problog/GraphAnalyzer.h](src/include/souffle/problog/GraphAnalyzer.h)
- [src/include/souffle/problog/IncRegionAnalyzer.h](src/include/souffle/problog/IncRegionAnalyzer.h)
- [src/include/souffle/problog/ForwardCompilation.h](src/include/souffle/problog/ForwardCompilation.h)
- [src/include/souffle/problog/ConstAnalysis.h](src/include/souffle/problog/ConstAnalysis.h)
- [src/problog/debug/Debugger.cpp](src/problog/debug/Debugger.cpp)
- [src/synthesiser/Synthesiser.cpp](src/synthesiser/Synthesiser.cpp)
- [src/synthesiser/GroundSynthesiser.cpp](src/synthesiser/GroundSynthesiser.cpp)
- [src/include/souffle/Derivation.h](src/include/souffle/Derivation.h)
- [src/Derivation.cpp](src/Derivation.cpp)
- [src/include/souffle/io/WriteStreamJSON.h](src/include/souffle/io/WriteStreamJSON.h)
- [src/include/souffle/io/ReadStreamJSON.h](src/include/souffle/io/ReadStreamJSON.h)
- [src/include/souffle/provenance/Explain.h](src/include/souffle/provenance/Explain.h)
- [docs/USAGE.md](docs/USAGE.md)

## Scope
This document lists supported dump/debug outputs for the compiler (`souffle`) and
compiled programs (`./compute`). Each item notes how to enable it, which file(s) are
produced, which pipeline(s) it belongs to, and how to interpret the output.

## Compiler outputs (`souffle`)

### `--show <mode>` (stdout)
Pipeline: compiler front-end + AST/RAM pipelines.

`--show` prints compiler-internal views to stdout. Modes can be combined unless noted.
No files are written.

Program/AST views:
- `initial-ast` (alias: `initial-datalog`): parsed Datalog before AST transforms.
- `transformed-ast` (alias: `transformed-datalog`): Datalog after AST transforms.

Graph views (HTML-wrapped DOT to stdout):
- `precedence-graph`: precedence graph.
- `scc-graph`: SCC graph.

Graph views (text):
- `precedence-graph-text`: plain-text precedence graph.
- `scc-graph-text`: plain-text SCC graph.

Analysis views:
- `type-analysis`: type analysis summary.

RAM views:
- `initial-ram`: RAM program before RAM transforms.
- `transformed-ram`: RAM program after RAM transforms.

Special case:
- `parse-errors`: prints parse errors and exits with the error count.
  If combined with other `--show` modes, it warns and suppresses them.

### `--debug-report <FILE>` (HTML report)
Pipeline: compiler front-end + AST/RAM pipelines.

Writes an HTML debug report to `<FILE>` when compilation finishes.
- File name: exactly `<FILE>` (caller chooses).
- Content: config snapshot, parsing time, AST transform diffs, RAM transform diffs.

Use: inspect full compilation pipeline changes in one report.

## Runtime outputs (`./compute`)

### Common outputs (full pipeline + incremental CLI)

#### Probability outputs (`dumpProbabilities`)
Pipeline: full + incremental; emitted when probabilities are computed.

Files:
- `facts.prob` (full pipeline + det-force shortcut)
- `fact-iter<iter>-inc-naive.prob` (incremental CLI)
- `fact-iter<iter>-inc-regional.prob` (incremental CLI)
- `fact-iter<iter>-full.prob` (incremental CLI full mode)

Format:
- One tuple per line: `Relation(a,b,...) : <prob>`.
- Parse by splitting on ` : `, left side is tuple string, right side is float.

#### Initial-input snapshot
Pipeline: full + incremental.

File:
- `initial-input-relations-iter<iter>.txt`

Format:
- Sections labeled `Relation: <name>`, followed by tuple lines indented by two spaces.

Note: path is built as `<output-dir>/initial-input-relations-iter...`; if output dir
is empty, the code yields an absolute path like `/initial-input-relations-iter...`.

#### Constant pre-analysis (`--dumpconst`)
Pipeline: full + incremental, when constant analysis runs.

File:
- `const-pre-<tag>.txt` where tag indicates the algorithm path:
  `full-worklist`, `full-cyclewise`, `full-ondemand`, `inc-worklist`,
  `inc-cyclewise`, `inc-regional`.

Format:
- Header summary with node/edge counts.
- Sections: `[true-nodes]`, `[false-nodes]`, `[true-edges]`, `[false-edges]`.
- Each line encodes id, fact/det flag, probability, and tuple string.

Use: diagnose constant propagation and pruning impact.

### Full pipeline outputs (non-CLI, `src/problog/Pipeline.cpp`)

#### Derivation graph snapshots (`--dumpdot`, `--dumpjson`)
Pipeline: full pipeline (non-CLI).

Files:
- `before_prune.dot`
- `after_prune.dot`
- `derivation.json`

Semantics:
- DOT: graph before/after prune, nodes are tuples, edges are rule instantiations.
- JSON (`derivation.json`): graph view after prune.

Parse (JSON):
- Top-level keys: `facts` and `rules`.
- `facts[]`: `{ "name": "Rel(...)" , "probability": <float> }`.
- `rules[]`: `{ "head": "Rel(...)", "probability": <float>, "bodies": [ {"negation": <bool>, "name": "Rel(...)"}, ... ] }`.

#### Rewrite outputs (SISO rewrite, `--rewrite` + `--dumpdot`)
Pipeline: rewrite stage inside full pipeline.

Files:
- `rewrite_iter<iter>_before.dot`
- `rewrite_iter<iter>_after.dot`
- `rewrite_final.dot`

Semantics:
- Per-iteration and final DOT snapshots of SISO rewrite.
- Useful for verifying region selection and rewrite effects.

#### SISO region overlay (helper, not called by default)
Pipeline: helper in full pipeline; not invoked unless wired in.

File:
- `siso_regions.dot`

Semantics:
- Full graph with detected SISO regions colored.

### Incremental CLI outputs (`src/include/souffle/cli/Cli.h`)

#### Derivation graph snapshots (`--dumpdot`)
Pipeline: incremental CLI.

Files:
- `derivation-inc-before-prune<iter>.dot`
- `derivation-inc-after-prune<iter>.dot`
- `derivation-full-before-prune<iter>.dot`
- `derivation-full-after-prune<iter>.dot`

Semantics:
- Incremental modes: before/after prune; includes delta annotation in DOT legend.
- Full mode (inside CLI): before/after prune for full recomputation.

#### Derivation graph snapshots (`--dumpjson`)
Pipeline: incremental CLI.

Files:
- `derivation-inc-after-prune<iter>-<timestamp>.json`
- `derivation-full-after-prune<iter>-<timestamp>.json`
  - Timestamp format: `YYYYMMDD-HHMMSS`.

Parse:
- Top-level keys: `facts`, `rules`, `delta`.
- `delta.insert` / `delta.delete` each contain `nodes`, `edges`, `facts` arrays.
- `edges[]` uses the same `{head, probability, bodies[]}` structure as `derivation.json`.

#### Graph statistics (`--dumpstat`)
Pipeline: incremental CLI.

File:
- `graph-<N>.json`

Parse:
- Numeric keys: `nodes`, `edges`, `queries`, `avg_in_degree`, `max_in_degree`,
  `avg_out_degree`, `max_out_degree`, `avg_hyperedge_inputs`, `max_hyperedge_inputs`.
- Optional cycle keys (only if heavy stats enabled): `cycles`, `avg_cycle_size`, `max_cycle_size`.

#### Interactive CLI commands
Pipeline: incremental CLI interactive shell.

- `set dumpjson|dumpdot|dumpstat`: enable runtime dumps.
- `unset dumpjson|dumpdot|dumpstat`: disable runtime dumps.
- `dump`:
  - Ground mode: writes `preDG_dump.dot` to CWD.
  - Non-ground: prints relation contents to stdout.

### Cyclewise/ondemand FC outputs (SCC dependency graphs)
Pipeline: cyclewise forward compilation paths.

Files (when `--dumpdot` enabled):
- `scc.dot`
- `scc<turn>.dot`

Semantics:
- SCC dependency graph over derivation graph cycles.
- DOT nodes are SCCs labeled with a few member tuples; edges are dependencies.

### Inc-regional diagnostics
Pipeline: inc-regional analyzer (`--setmode inc-regional`).

Files:
- `inc-region-<idx>.dot` (requires `--dumpdot`)
- `region-<idx>.txt` (requires `--profile-inc-regional`)

Semantics:
- DOT: region boundaries, delta nodes, and mergeability hints.
- TXT: textual region summary and stats (inc-regional profiling).

### Debugger JSON report (`-l/--logfile <NAME>`)
Pipeline: full + incremental.

File:
- `<logBase>_YYYYMMDHMS.json` where `logBase` is the basename of `<NAME>`.

Parse:
- `turns[]`: each has `index`, `mode`, `time_seconds`, `info`, `logs`, `stages`.
- `stages[]`: each has `name`, `time_seconds`, `peak_mem_kb`, `info`, `logs`.

Use: pipeline timing and metadata per turn/stage.

### Ground-only outputs (non-default path)
Pipeline: ground synthesiser (not used by the current MainDriver path).

File:
- `preDG.dot`

Semantics:
- DOT snapshot of the pre-derivation graph in ground mode.

## Other JSON outputs (not dump flags)
These are JSON files emitted by other subsystems; they are not part of `--dump*` flags.

- Relation IO (`IO=...` with JSON format): default `<relation>.json` in output dir.
  - Overridable via the `filename` IO parameter.
- Provenance explain: `format json` + `output <file>` writes JSON to the given filename.
- Derivation info helpers (not wired by default):
  - `<program>.json` or `<program>.<suffix>.json` in the given output dir.

## Output paths and directories
- `--output <DIR>` sets the base directory for most runtime dumps.
- `--dumpdot`/`--dumpjson` use the runtime output dir; if none is set, DOT/JSON are written
  to the current working directory.
- `graph-<N>.json` (dumpstat) uses output dir if set; otherwise a local `output/` directory.
- `preDG_dump.dot` and `preDG.dot` ignore the output dir and always write to the CWD.

## Related commits
- (no git history yet; uncommitted/new file)
