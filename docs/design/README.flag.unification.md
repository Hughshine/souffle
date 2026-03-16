# Unified Flag Surface Design

## Source references
- [src/MainDriver.cpp](src/MainDriver.cpp)
- [src/include/souffle/CompiledOptions.h](src/include/souffle/CompiledOptions.h)
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)
- [src/problog/Pipeline.cpp](src/problog/Pipeline.cpp)
- [src/problog_graph_query.cpp](src/problog_graph_query.cpp)
- [docs/USAGE.md](docs/USAGE.md)
- [docs/topics/runtime/README.flag.md](docs/topics/runtime/README.flag.md)
- [docs/design/README.refactor.scope.md](docs/design/README.refactor.scope.md)

This file is a design proposal for a single canonical flag model that covers the
fork's compiler defaults, compiled runtime flags, interactive online CLI mode
selection, and standalone offline graph-query tooling.

## Status
- Canonical flag parsing and shared mode/output helpers are now implemented
  across the compiler, compiled runtime, online CLI, and graph-query tooling.
- Current authoritative user-facing behavior remains [docs/USAGE.md](docs/USAGE.md)
  and [docs/topics/runtime/README.flag.md](docs/topics/runtime/README.flag.md).
- This document still serves as the design rationale and longer-term cleanup map
  for the implemented flag surface.

## Scope
- Fork-owned probabilistic, rewrite, incremental, profiling, dump, and
  experimental flags.
- Shared naming across:
  - `souffle` compiler defaults
  - generated `./compute` runtime flags
  - interactive online CLI configuration
  - standalone graph-query / approx tooling

## Non-Goals
- Do not redesign all upstream Souffle flags.
- Do not change current runtime behavior in this design.
- Do not move or rename `research/` or `docs/research/`.
- Do not force approximate backends into the main runtime before they are ready.

## Problem
The current flag surface has four overlapping problems:
- Same concept uses different names on different surfaces.
  - example: `--knowledge`, `-k`, backend-specific code branches, and standalone
    `--backend amc`
- One flag sometimes encodes multiple dimensions.
  - example: `--setmode` mixes semantic mode and FC strategy
- Related features are split across unrelated booleans.
  - example: rewrite mode, split mode, detection mode, and implicit-vs-legacy
    are all separate toggles
- Dump/profile flags are numerous and difficult to reason about as a family.

## Design Goal
Define one canonical configuration schema, then let each binary or shell expose
the relevant subset. Compatibility aliases remain accepted, but the schema has
only one preferred spelling for each concept.

## Canonical Model
The canonical model is organized into seven families.

### 1. Surface
- `surface`
  - values: `compiled-runtime`, `online-cli`, `graph-query`

### 2. Execution
- `sem-mode`
  - values: `full`, `inc`
- `fc-mode`
  - values: `full-hard`, `full-soft`, `inc-naive`, `inc-regional`, `elastic`
- `full-evaluator`
  - values: `exact`, `scbf`, `approx`
- `dd-backend`
  - values: `bdd`, `sdd`
- `approx-backend`
  - values: `none`, `amc`

### 3. Rewrite
- `rewrite-engine`
  - values: `off`, `legacy`, `implicit`, `implicit-iter`
- `rewrite-split`
  - values: `off`, `naive`, `complete`
- `rewrite-detect`
  - values: `dirty-frontier`, `complete`

### 4. Determinism / Graph Simplification
- `det-mode`
  - values: `auto`, `off`, `force`
- `fold-const`
  - values: `on`, `off`
- `merge-bi-imp`
  - values: `on`, `off`
- `prune-extra`
  - values: `on`, `off`

### 5. IO / Outputs
- `input-dir`
- `output-dir`
- `log-file`
- `dump`
  - repeatable or comma-separated values: `json`, `dot`, `stat`, `const`

### 6. Profiling / Tracing
- `profile-file`
- `profile-stage`
  - repeatable or comma-separated values:
    `dred`, `inc`, `fc`, `wmc`, `inc-delete`, `inc-regional`,
    `inc-regional-heavy`, `dep-graph`
- `trace-inc-regional`
  - tuple list

### 7. Workflow / Defaults
- `online`
  - values: `on`, `off`
- `derivation-only`
  - values: `on`, `off`

## Preferred Canonical Flags
These are the proposed preferred spellings.

### Shared Core
- `--input-dir=<DIR>`
- `--output-dir=<DIR>`
- `--log-file=<FILE>`
- `--profile-file=<FILE>`
- `--online`
- `--derivation-only`

### Execution
- `--sem-mode=<full|inc>`
- `--fc-mode=<full-hard|full-soft|inc-naive|inc-regional|elastic>`
- `--full-evaluator=<exact|scbf|approx>`
- `--dd-backend=<bdd|sdd>`
- `--approx-backend=<none|amc>`

### Rewrite
- `--rewrite-engine=<off|legacy|implicit|implicit-iter>`
- `--rewrite-split=<off|naive|complete>`
- `--rewrite-detect=<dirty-frontier|complete>`

### Determinism / Simplification
- `--det-mode=<auto|off|force>`
- `--fold-const`
- `--no-fold-const`
- `--merge-bi-imp`
- `--no-merge-bi-imp`
- `--prune-extra`
- `--no-prune-extra`

### Dumps / Profiling
- `--dump=<json,dot,stat,const>`
- `--profile-stage=<dred,inc,fc,wmc,inc-delete,inc-regional,inc-regional-heavy,dep-graph>`
- `--trace-inc-regional=<LIST>`

## Surface-by-Surface Contract

### A. Compiler (`souffle`)
Compiler flags should keep upstream meanings, but fork-owned runtime defaults
should be expressed through the same canonical vocabulary.

Compiler should accept:
- `--online`
- `--derivation-only`
- `--sem-mode`
- `--fc-mode`
- `--full-evaluator`
- `--dd-backend`
- `--rewrite-engine`
- `--rewrite-split`
- `--rewrite-detect`
- `--det-mode`
- `--fold-const`
- `--merge-bi-imp`
- `--prune-extra`
- `--dump`
- `--profile-stage`
- `--trace-inc-regional`

Compiler semantics:
- These flags set defaults baked into the generated binary.
- The generated binary may override them at runtime.

### B. Compiled Runtime (`./compute`)
This surface should expose the same canonical flags directly.

Runtime should accept:
- all execution, rewrite, determinism, dump, and profile families above
- `--input-dir`, `--output-dir`, `--log-file`, `--profile-file`

Runtime validation rules:
- `--full-evaluator=scbf` requires `--dd-backend=<bdd|sdd>`
- `--full-evaluator=approx` is invalid for the compiled runtime until an
  approximate runtime path exists
- `--rewrite-engine=off` ignores `--rewrite-split` and `--rewrite-detect`
- `--rewrite-engine=implicit|implicit-iter` implies rewrite is enabled
- `--det-mode=force` rejects combinations that require a derivation graph
- `--sem-mode=inc` plus `--fc-mode=full-*` remains valid
- `--sem-mode=full` plus `--fc-mode=inc-*` remains valid

### C. Interactive Online CLI
The interactive shell is not a flag parser, but it should use the same config
vocabulary.

Preferred interactive commands:
- `set sem-mode full`
- `set fc-mode inc-regional`
- `set rewrite-engine implicit`
- `set rewrite-split complete`
- `set rewrite-detect complete`
- `set det-mode off`
- `set dump json`
- `unset dump json`
- `show config`

Compatibility commands kept:
- `setmode <legacy-mode>`
- `set dumpjson|dumpdot|dumpstat`
- `unset dumpjson|dumpdot|dumpstat`

### D. Offline Graph Query / Approx Tooling
The standalone graph-query tool should use the same execution vocabulary where
possible, but keep its own required query-specific flags.

Preferred graph-query flags:
- `--full-evaluator=approx`
- `--approx-backend=amc`
- `--rewrite-engine=<off|legacy|implicit|implicit-iter>`
- `--rewrite-split=<off|naive|complete>`
- `--dd-backend=<bdd|sdd>` when exact query replay is used

Rule:
- `approx-backend` is only meaningful when `full-evaluator=approx`

## Compatibility Map
Legacy flags should remain accepted as aliases until a dedicated cleanup phase.

| Current flag | Canonical meaning |
|---|---|
| `--setmode=inc`, `--setmode=incr`, `--setmode=incremental` | `--sem-mode=inc --fc-mode=inc-naive` |
| `--setmode=inc-regional` | `--sem-mode=inc --fc-mode=inc-regional` |
| `--setmode=full`, `--setmode=full-hard` | `--sem-mode=full --fc-mode=full-hard` |
| `--setmode=full-soft` | `--sem-mode=full --fc-mode=full-soft` |
| `--rewrite` | `--rewrite-engine=legacy` |
| `--implicit-rewrite` | `--rewrite-engine=implicit` |
| `--implicit-iterate-split-rewrite` | `--rewrite-engine=implicit-iter` |
| `--split-mode=no-split` | `--rewrite-split=off` |
| `--split-mode=naive-split` | `--rewrite-split=naive` |
| `--split-mode=complete-split` | `--rewrite-split=complete` |
| `--force-complete-siso-detect` | `--rewrite-detect=complete` |
| `--det-opt` | `--det-mode=auto` |
| `--no-det-opt` | `--det-mode=off` |
| `--det-force` | `--det-mode=force` |
| `-k`, `--knowledge=bdd|sdd` | `--dd-backend=bdd|sdd` |
| `--scbf` | `--full-evaluator=scbf` |
| `--dumpjson` | `--dump=json` |
| `--dumpdot` | `--dump=dot` |
| `--dumpstat` | `--dump=stat` |
| `--dumpconst` | `--dump=const` |
| `--dred-profile` | `--profile-stage=dred` |
| `--inc-profile` | `--profile-stage=inc` |
| `--fc-profile` | `--profile-stage=fc` |
| `--profile-wmc` | `--profile-stage=wmc` |
| `--profile-inc-delete` | `--profile-stage=inc-delete` |
| `--profile-inc-regional` | `--profile-stage=inc-regional` |
| `--profile-inc-regional-heavy` | `--profile-stage=inc-regional-heavy` |
| `--profile-dep-graph` | `--profile-stage=dep-graph` |
| graph-query `--backend amc` | `--full-evaluator=approx --approx-backend=amc` |

## Naming Rules
- Use `backend` only for solver/data-structure choices.
  - `dd-backend`, `approx-backend`
- Use `engine` for algorithm families.
  - `rewrite-engine`
- Use `mode` for semantic / execution state.
  - `sem-mode`, `fc-mode`, `det-mode`
- Use `evaluator` for mutually exclusive top-level full evaluators.
  - `full-evaluator`
- Prefer one canonical long name, keep short aliases only where they already
  match upstream convention.

## Validation Rules
The new surface should reject invalid combinations early and consistently.

Examples:
- `--full-evaluator=approx --sem-mode=inc`
  - reject: approx is not an online incremental evaluator
- `--rewrite-engine=off --rewrite-split=complete`
  - reject or warn: split has no meaning without rewrite
- `--full-evaluator=scbf --approx-backend=amc`
  - reject: SCBF and AMC are different evaluator families
- `--dd-backend=sdd --full-evaluator=approx`
  - warn or ignore: DD backend is irrelevant for pure approx mode

## Help Text Policy
Help output should be grouped by family, not by historical growth order:
- IO
- Execution
- Rewrite
- Determinism / Simplification
- Dumps
- Profiling / Tracing
- Compatibility aliases

Each family should show:
- canonical flag
- accepted values
- default
- important incompatibilities

## Rollout Plan
1. Add canonical internal config objects first.
2. Keep all current flags as aliases.
3. Canonicalize to the new names at parse time.
4. Update `-h`, `docs/USAGE.md`, and runtime flag docs to present canonical names first.
5. Only after one stable cycle, start deprecating redundant aliases.

## Acceptance
- One canonical name per concept.
- Compiler, runtime, CLI, and graph-query all map into the same config schema.
- Help text and docs agree on defaults and valid values.
- Existing benchmark scripts and regression tests keep working via compatibility aliases.
- No semantic behavior changes are required for Phase 1 of the redesign.

## Why This Refactor Is Worth Doing
- It makes full exact, rewrite, implicit rewrite, SCBF, approx, and incremental
  execution modes describable in one language.
- It removes current naming drift between `MainDriver`, `CompiledOptions`,
  `Pipeline`, and standalone tools.
- It gives the refactor a stable public surface before deeper runtime surgery.

## Related commits
- `UNCOMMITTED` — docs(design): add unified flag surface proposal across compiler/runtime/CLI/tooling
