# ProbLog Extension Stack (Driver -> Pipeline)

This document is the implementation-level map for this fork's ProbLog extension,
from front-end parsing to runtime probability/incremental execution.

## Scope
- Focus: fork-specific probabilistic + incremental path, not generic upstream Souffle internals.
- Audience: maintainers changing driver/parser/AST/AST2RAM/RAM/synthesiser/runtime behaviors.
- Goal: explain where probabilistic semantics live, how data flows across layers, and what invariants must hold.

## End-to-End Flow
1. `src/MainDriver.cpp`: parse CLI, apply AST transforms, select online translator.
2. `src/parser/parser.yy` + `src/parser/ParserDriver.cpp`: parse `query`, `evidence`, probabilistic facts/heads into AST.
3. `src/ast/*`: store probabilistic metadata in `ast::Program`; run semantic checks.
4. `src/ast2ram/online/*`: generate full + incremental RAM programs and extended relation families.
5. `src/ram/*`: hold RAM-level probabilistic metadata and incremental operators (`RecordDerivation`, `DeltaUnion`).
6. `src/synthesiser/Synthesiser.cpp`: generate runtime C++ glue, build `RuleManager`/`QueryManager`, wire `runPipeline(...)`.
7. `src/problog/Pipeline.cpp`: build/prune/rewrite derivation graph, run FC/WMC (BDD/SDD), optionally launch incremental CLI.
8. `src/include/souffle/cli/Cli.h`: interactive/batch incremental updates (`insert/delete/commit`, `setmode`, dump toggles).

## 1) Driver Layer (`MainDriver.cpp`)

### 1.1 Defaults that shape this fork
- If no compile/generate option is provided, driver defaults to compile-only:
  `-o <basename>` (`dl-program`).
- If `--online` is not provided, driver sets it by default.
- `getUnitTranslator(...)` always chooses `ast2ram::online::TranslationStrategy`.

This means the fork defaults to the online compilation/runtime path.

### 1.2 AST transform pipeline (fork-relevant parts)
`astTransformationPipeline(...)` includes fork-specific checks:
- `EvidenceSemanticChecker` (implemented in `EvidenceChecker.h/.cpp`)
- `ProbQueryChecker`

Magic-set integration includes:
- `MagicSetTransformer`, whose internal pipeline includes `ProbQueryConstraintLiftingTransformer`.

### 1.3 Runtime option surface entry
- Driver exposes flags such as `--online`, `--full-only`, `--setmode`, `--rewrite`, `--split-mode`.
- Generated binaries parse runtime options in `CmdOptions` (`src/include/souffle/CompiledOptions.h`).

## 2) Parser Layer (`parser.yy`, `ParserDriver.cpp`)

### 2.1 New probabilistic syntax
- Probabilistic query:
  - `query(atom).`
  - zero-arity form: `query RelName.`
- Evidence:
  - `evidence(atom, true).`
  - `evidence(atom, false).`
- Probabilistic facts:
  - `0.7::a(1,2).`
  - zero-arity fact: `0.7::a.`
- Probabilistic rule heads:
  - `0.6::h(X) :- b(X).`
  - disjunctive heads can carry probabilities per head atom.

### 2.2 Parser -> AST handoff
- `ParserDriver::addProbQuery(...)` appends to `ast::Program::queries`.
- `ParserDriver::addEvidence(...)` appends to `ast::Program::evidences`.
- For `FLOAT::atom`, parser sets probability both on head atom and clause (`setProbability(...)`).

## 3) AST Layer (`ast/*`, `ast/transform/*`)

### 3.1 Program data model
`ast::Program` stores:
- `VecOwn<ProbQuery> queries`
- `VecOwn<Evidence> evidences`

### 3.2 Probabilistic nodes
- `ast::ProbQuery`: wraps an atom and exposes relation/arguments.
- `ast::Evidence`: wraps an atom + boolean truth value.

### 3.3 Semantic checks
- `ProbQueryChecker`:
  - relation must exist
  - arity must match
- `EvidenceSemanticChecker`:
  - relation must exist
  - arity must match

Both report through `TranslationUnit::ErrorReport`.

### 3.4 Magic-set query lifting hook
`ProbQueryConstraintLiftingTransformer` (inside `MagicSetTransformer`) currently:
- handles unary-constant probabilistic queries,
- can inject `var = constant` constraints into defining clauses,
- skips recursive/non-leaf SCC cases,
- skips conflicting multi-constant queries for same relation.

This is a targeted optimization/constraint propagation pass; behavior is intentionally conservative.

## 4) AST2RAM Online Lowering (`ast2ram/online/*`, `ast2ram/utility/Utils.*`)

### 4.1 Translator behavior
`online::UnitTranslator::translateUnit(...)` emits:
- full RAM program (`generateProgram(...)`)
- incremental RAM program (`generateProgramInc(...)`) unless `--full-only`
- shared subroutines per SCC and incremental phases

It also transfers probabilistic metadata:
- AST `ProbQuery` -> RAM `ProbQuery`
- AST `Evidence` -> RAM `Evidence`

Note: `translateProbQuery(...)`/`translateEvidence(...)` emit empty statements; they are metadata carriers for later synthesis/runtime.

### 4.2 Relation family naming conventions
Defined in `ast2ram/utility/Utils.cpp`:

| Purpose | Prefix / example |
| --- | --- |
| Main relation | `R` |
| Old snapshot | `@old_R` |
| Semi-naive delta/new | `@delta_R`, `@new_R` |
| Subsumption helper | `@reject_R`, `@delete_R`, `@lub_R` |
| Incremental derivation delta | `$inc_delta_derv_insert_R`, `$inc_delta_derv_delete_R` |
| Incremental tuple delta | `$inc_delta_tuple_insert_R`, `$inc_delta_tuple_delete_R` |
| Recursive inc tuple/derivation split | `@delta_tuple_insert_R`, `@delta_tuple_delete_R`, `@new_derv_insert_R`, `@new_derv_delete_R` |
| DRed overdelete/rederive | `@inc_tuple_overdelete_R`, `@inc_derv_overdelete_R`, `@inc_new_derv_rederive_R`, `@inc_delta_tuple_rederive_R` |

Any prefix change must stay consistent with base-name stripping logic in:
- `ast2ram/utility/Utils.cpp:getBaseRelationName(...)`
- `synthesiser/Synthesiser.cpp:getBaseRelationName(...)`

## 5) RAM Layer Extensions (`ram/*`)

### 5.1 `ram::Program` extensions
RAM program stores:
- `VecOwn<ProbQuery> probQueries`
- `VecOwn<Evidence> evidences`

### 5.2 `ram::RecordDerivation`
Represents derivation recording for tuple insert/delete events, including:
- clause ID/string context,
- variable bindings,
- mode flags (`insert/delete`, `complete/delta`, `recursive`, `rederive`).

Synthesiser lowers it into updates of:
- complete derivation map: `DerivationManager::untypedTuple2RuleApplications`
- delta maps: insert/delete and delta-delta variants
- profiling counters/timing buckets.

### 5.3 `ram::DeltaUnion`
Represents tuple-level delta consolidation from derivation-level deltas:
- merge rule-app deltas into complete sets,
- derive real tuple insert/delete deltas,
- apply to target relation (`newRel`) with insert/erase,
- special deterministic shortcut path when `det-opt` marks relation deterministic.

## 6) Synthesiser/Runtime Glue (`Synthesiser.cpp`)

### 6.1 Generated runtime initialization
Generated `main(...)`:
- creates `CmdOptions`,
- parses runtime arguments,
- sets global flags (`detOptEnabled`, `detForceEnabled`, profile toggles, etc.),
- constructs compiled program object and sets knowledge backend (BDD/SDD).

### 6.2 Rule/query/evidence construction
- Builds `RuleManager` from AST clauses (head/body symbolic atoms, probability, recursion flags).
- Builds `QueryManager` from AST probabilistic queries.
- Builds runtime `std::vector<std::pair<UntypedTuple,bool>> evidences` from RAM evidences.

### 6.3 Fact probability loading
Runtime hook loads `<relation>.facts` and `<relation>.prob`:
- parses tuples from `.facts`,
- aligns probability lines from `.prob`,
- defaults missing/invalid probability to `1.0`,
- fills global `fact_prob`.

With det-opt behavior enabled (default on; disable with `--no-det-opt`), synthesiser emits a deterministic prepass that:
- computes relation-level probabilistic seeds (`relationHasProbFact` + probabilistic rules),
- propagates through SCC dependencies,
- writes `relationIsDet`,
- dumps diagnostics: `det-relations.txt`, `det-scc.txt`.

### 6.4 Pipeline invocation contract
Generated code calls:
- `souffle::problog::setFullOnlyMode(...)`
- `souffle::problog::runPipeline(opt, obj, ruleManager, queryManager, fact_prob, evidences, enableOnlineCli)`

`enableOnlineCli` is only true when `--online` and not `--full-only`.

## 7) Shared Runtime State (`Derivation.h` / `Derivation.cpp`)

Key globals:
- `inputFactSet`
- `fact_prob`
- `relationHasProbFact`
- `relationIsDet`
- `initialInputRelations`

Key incremental derivation maps in `DerivationManager`:
- complete derivations
- delta insert/delete derivations
- delta-delta insert/delete derivations

These maps are the data bridge among generated RAM execution, graph update, and incremental CLI.

## 8) Probabilistic Pipeline (`Pipeline.h/.cpp`)

### 8.1 `runPipeline(...)` sequence
1. Configure dump/profiling switches and graph-level flags.
2. `det-force` short-circuit:
   - skip graph/FC/WMC,
   - emit output probabilities as `1.0`,
   - disable incremental CLI.
3. Enforce evidence guard:
   - evidence is rejected unless full-only mode is enabled.
4. Build `IncrementalDerivationGraph::createFrom(...)`.
5. Prune graph to output-focused subgraph.
6. Optional rewrite (`--rewrite`, with split mode).
7. Choose backend:
   - BDD path (`runBddPipeline`)
   - SDD path (`runSddPipeline`)
8. Optional incremental CLI launch.

### 8.2 Rewrite + online CLI interaction
- If rewrite runs in full mode, pipeline marks `rewritePerformed`.
- Online incremental CLI is then skipped in that same run (`allowOnlineCli = false`).

### 8.3 BDD/SDD behavior highlights
Both paths:
- build formulas from graph,
- apply evidences by node resolution in graph,
- compute output node probabilities,
- dump probabilities,
- can pass DD manager + formulas to CLI for incremental updates.

BDD path additionally has hybrid fast paths (single-randvar/conjunctive components) and profiling breakdowns.

## 9) Incremental CLI (`cli/Cli.h`)

### 9.1 User-visible commands
- `insert [prob::]rel(args) [prob]`
- `delete/remove rel(args)`
- `list`
- `commit`
- `setmode ...`
- `set/unset dumpjson|dumpdot|dumpstat`

### 9.2 Mode aliases
- `inc`, `incremental`, `incr`, `inc-naive` -> `INC_NAIVE`
- `inc-regional`, `regional` -> `INC_REGIONAL`
- `full`, `full-hard` -> `FULL_HARD`
- `full-soft` -> `FULL_SOFT`
- `elastic` -> `ELASTIC`

### 9.3 Commit lifecycle (non-ground path)
1. Materialize pending ops into incremental delta relations.
2. Execute `runAllInc(...)` (seminaive incremental phase).
3. Build incremental delta payloads (`factProbInc`, deleted facts, derivation delta maps).
4. Apply `graph->applyDelta(...)`, then prune.
5. Recompute affected output probabilities via DD manager (with evidence-aware logic).
6. Dump per-iteration probability file:
   - `fact-iter<N>-inc-naive.prob` or `fact-iter<N>-inc-regional.prob`.

Full-mode commits rebuild via full run (`runAll(...)`), re-create/prune graph, and dump `fact-iter<N>-full.prob`.

## 10) Cross-Layer Invariants (Must Keep)
- Parser grammar additions must stay aligned with AST node model and checkers.
- `Program` probabilistic metadata must be forwarded AST -> RAM -> synthesiser -> runtime pipeline.
- Relation prefix naming must stay consistent across:
  - AST2RAM name generation,
  - base-name stripping,
  - synthesiser/runtime handling.
- Evidence semantics currently require full-only mode (enforced in pipeline).
- Rewrite full-run and online incremental CLI are mutually exclusive in one run.
- Deterministic optimization (`det-opt`) has two consistent components:
  - synthesiser-generated pre-analysis (`relationIsDet`)
  - runtime checks in `RecordDerivation`/`DeltaUnion`/CLI paths.

## Source references
- [src/MainDriver.cpp](src/MainDriver.cpp)
- [src/parser/parser.yy](src/parser/parser.yy)
- [src/parser/ParserDriver.cpp](src/parser/ParserDriver.cpp)
- [src/ast/Program.h](src/ast/Program.h)
- [src/ast/ProbQuery.h](src/ast/ProbQuery.h)
- [src/ast/Evidence.h](src/ast/Evidence.h)
- [src/ast/transform/ProbQueryChecker.cpp](src/ast/transform/ProbQueryChecker.cpp)
- [src/ast/transform/EvidenceChecker.cpp](src/ast/transform/EvidenceChecker.cpp)
- [src/ast/transform/ProbQueryConstraintLifting.cpp](src/ast/transform/ProbQueryConstraintLifting.cpp)
- [src/ast/transform/MagicSet.h](src/ast/transform/MagicSet.h)
- [src/ast2ram/online/UnitTranslator.cpp](src/ast2ram/online/UnitTranslator.cpp)
- [src/ast2ram/utility/Utils.cpp](src/ast2ram/utility/Utils.cpp)
- [src/ram/Program.h](src/ram/Program.h)
- [src/ram/RecordDerivation.h](src/ram/RecordDerivation.h)
- [src/ram/DeltaUnion.h](src/ram/DeltaUnion.h)
- [src/synthesiser/Synthesiser.cpp](src/synthesiser/Synthesiser.cpp)
- [src/include/souffle/Derivation.h](src/include/souffle/Derivation.h)
- [src/problog/Pipeline.cpp](src/problog/Pipeline.cpp)
- [src/include/souffle/problog/Pipeline.h](src/include/souffle/problog/Pipeline.h)
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)

## Related commits
- `UNCOMMITTED` — docs(project): add full ProbLog extension stack document from driver to runtime pipeline
- `668298ef8` — fix(inc-region): update regional WMC routing and profiling
- `812ea4081` — docs(repo): refine README narratives
