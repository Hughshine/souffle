# Souffle AST and RAM Transformation Notes

This document inventories the AST (rule-level) and RAM (execution-level)
transformation pipelines wired in `src/MainDriver.cpp`, and explains every
transformation in those pipelines. For each pass, it states whether the pass
is correct under **query/output/evidence-relative probabilistic semantics**.

Definition of the semantic scope used below:
- We only require derivations that can affect **probabilistic queries, output
  relations, or evidence**. Derivations outside this slice are considered
  irrelevant.
- A "derivation" is a rule application with a concrete substitution for all
  rule variables.
- A "complete derivation graph" means *all* derivations within the scope
  above (not necessarily all rules in the whole program).

IMPORTANT implementation caveat:
- Several AST transforms rewrite relations in clauses but **do not update
  ProbQuery/Evidence nodes**, which are stored separately in the AST program.
  If such a transform removes/renames a relation that is referenced by a
  ProbQuery/Evidence, the program can become inconsistent. This is a concrete
  code-level reason some transforms are UNSAFE unless extended to include
  evidence/query relations in their analysis and renaming logic.

Legend:
- SAFE: preserves derivations within the query/output/evidence slice.
- UNSAFE: removes or merges derivations that are in-scope.
- DEPENDS: safe only under additional conditions noted.

Source of truth:
- `astTransformationPipeline()` in `src/MainDriver.cpp`
- `ramTransformerSequence()` in `src/MainDriver.cpp`

------------------------------------------------------------------------
## AST pipeline (rule-level)

### Current order (as wired in MainDriver)
Main pipeline order (top-level):
1. ComponentChecker
2. EvidenceSemanticChecker
3. ComponentInstantiationTransformer
4. ProbQueryChecker
5. DebugDeltaRelationTransformer
6. IODefaultsTransformer
7. ResolveAliasesTransformer
8. RemoveBooleanConstraintsTransformer
9. ResolveAliasesTransformer
10. MinimiseProgramTransformer
11. GroundedTermsChecker
12. ResolveAliasesTransformer
13. SimplifyConstantBinaryConstraintsTransformer
14. RemoveBooleanConstraintsTransformer
15. RemoveRedundantRelationsTransformer
16. RemoveRelationCopiesTransformer
17. RemoveEmptyRelationsTransformer
18. Fixpoint( ReduceExistentialsTransformer -> RemoveRedundantRelationsTransformer )
19. RemoveRelationCopiesTransformer
20. equivalencePipeline (see below)
21. RemoveRelationCopiesTransformer
22. magicPipeline (see below)
23. RemoveEmptyRelationsTransformer
24. IOAttributesTransformer
25. ConstantNormalizationTransformer

Equivalence pipeline (used twice in the main pipeline):
- NameUnnamedVariablesTransformer
- Fixpoint( MinimiseProgramTransformer )
- RemoveRelationCopiesTransformer
- RemoveEmptyRelationsTransformer
- RemoveRedundantRelationsTransformer

Magic pipeline (used once in the main pipeline):
- Conditional( ExpandEqrelsTransformer ) when `--magic-transform` is set
- MagicSetTransformer
- ResolveAliasesTransformer
- RemoveRelationCopiesTransformer
- RemoveEmptyRelationsTransformer
- RemoveRedundantRelationsTransformer
- equivalencePipeline

### AST transformations: detailed notes and probabilistic impact

ComponentChecker (ast/transform/ComponentChecker.cpp)
- Purpose: semantic checks for component references, inheritance, overrides,
  name clashes, and cycles.
- Behavior: validates program structure; no rewrites.
- Prob semantics: SAFE (no rule changes).

EvidenceSemanticChecker (ast/transform/EvidenceChecker.cpp)
- Purpose: validate evidence atoms.
- Behavior: checks relation existence and arity.
- Prob semantics: SAFE (no rewrites).

ComponentInstantiationTransformer (ast/transform/ComponentInstantiation.cpp)
- Purpose: instantiate component templates into concrete relations/clauses.
- Behavior: clones relations/types/directives/clauses with bound parameters.
- Prob semantics: SAFE/REQUIRED. This is semantic expansion, not optimization.

ProbQueryChecker (ast/transform/ProbQueryChecker.cpp)
- Purpose: validate probabilistic query atoms.
- Behavior: checks relation existence and arity.
- Prob semantics: SAFE (no rewrites).

DebugDeltaRelationTransformer (ast/transform/DebugDeltaRelation.cpp)
- Purpose: adjust delta-debug relations.
- Behavior: copies attribute list from the original relation and adds an
  "<iteration>" attribute.
- Prob semantics: DEPENDS. Only relevant when delta-debug is enabled; it
  changes schema and thus derivation representation for those relations.

IODefaultsTransformer (ast/transform/IODefaults.h)
- Purpose: fill default IO parameters.
- Behavior: sets IO type/name/operation, attaches fact/output dirs.
- Prob semantics: SAFE (directive-only changes).

ResolveAliasesTransformer (ast/transform/ResolveAliases.cpp)
- Purpose: canonicalize variable aliasing and normalize terms.
- Behavior: resolves alias equalities, removes trivial equalities, and
  extracts complex terms into fresh variables with constraints.
- Prob semantics: SAFE for derivation sets (substitution space preserved).
  If you treat constraint nodes as part of the derivation graph, the
  structure changes but derivations (substitutions) are equivalent.

RemoveBooleanConstraintsTransformer (ast/transform/RemoveBooleanConstraints.cpp)
- Purpose: simplify boolean literals.
- Behavior: removes `true` literals; drops clauses with `false` literals.
- Prob semantics: SAFE under query/evidence semantics because `false` clauses
  contribute no derivations and `true` literals add no information. Unsafe
  only if you want to preserve unsatisfiable derivation structure.

MinimiseProgramTransformer (ast/transform/MinimiseProgram.cpp)
- Purpose: remove equivalent or subsumed clauses/relations.
- Behavior: deletes redundant rules/relations and renames to canonical reps.
- Prob semantics: UNSAFE. It can remove in-scope derivations even if query
  results are unchanged. Also `renameAtoms` only updates clauses, not
  ProbQuery/Evidence nodes, so queries/evidence can become inconsistent.

GroundedTermsChecker (ast/transform/GroundedTermsChecker.cpp)
- Purpose: validate that rule heads and records are grounded.
- Behavior: reports errors; no rewrites.
- Prob semantics: SAFE.

SimplifyConstantBinaryConstraintsTransformer
(ast/transform/SimplifyConstantBinaryConstraints.cpp)
- Purpose: constant-fold simple EQ/NE constraints.
- Behavior: replaces constant EQ/NE with BooleanConstraint true/false.
- Prob semantics: SAFE under query/evidence semantics. It can enable
  RemoveBooleanConstraints to drop impossible derivations, which are out of
  scope anyway.

RemoveRedundantRelationsTransformer (ast/transform/RemoveRedundantRelations.cpp)
- Purpose: remove relations not needed for outputs (per analysis).
- Behavior: deletes relations not reachable from **output** relations in the
  precedence graph. Analysis ignores ProbQuery and Evidence lists.
- Prob semantics: DEPENDS.
  - SAFE **only if** ProbQuery/Evidence relations are treated as outputs in
    the analysis (currently they are not).
  - Otherwise UNSAFE: it may delete relations that matter for evidence/query.

RemoveRelationCopiesTransformer (ast/transform/RemoveRelationCopies.cpp)
- Purpose: eliminate pure-copy relations `r(x) :- s(x)`.
- Behavior: removes the copy relation and renames atoms in clauses.
- Prob semantics: UNSAFE.
  - It merges a rule layer, removing derivations that should be in scope.
  - It does not update ProbQuery/Evidence nodes (only clauses), so queries or
    evidence pointing to the removed relation can break.

RemoveEmptyRelationsTransformer (ast/transform/RemoveEmptyRelations.cpp)
- Purpose: prune relations that are empty by construction.
- Behavior: removes empty non-input relations; removes clauses using empty
  relations; drops negations of empty relations.
- Prob semantics: DEPENDS.
  - SAFE if empty relations are not referenced by ProbQuery/Evidence.
  - UNSAFE otherwise, because the transform does not consider evidence/query
    targets when deciding removals.

ReduceExistentialsTransformer (ast/transform/ReduceExistentials.cpp)
- Purpose: reduce relations used only existentially.
- Behavior: rewrites relations into `+?exists_*` forms and removes recursive
  clauses, collapsing witness variables.
- Prob semantics: UNSAFE. It merges multiple witness derivations into an
  existential check, which changes derivation multiplicity.

NameUnnamedVariablesTransformer (ast/transform/NameUnnamedVariables.cpp)
- Purpose: replace `_` with fresh variable names.
- Behavior: each unnamed variable occurrence becomes a distinct variable.
- Prob semantics: SAFE/REQUIRED. It preserves substitution space and makes
  witnesses explicit.

ExpandEqrelsTransformer (ast/transform/ExpandEqrels.cpp)
- Purpose: expand EQREL relations into explicit rules.
- Behavior: adds transitivity/symmetry/reflexivity rules and switches
  representation to BTREE.
- Prob semantics: DEPENDS.
  - SAFE if your probabilistic semantics **define** EQREL by those rules.
  - UNSAFE if EQREL is treated as a builtin equivalence relation elsewhere,
    because expansion adds extra derivations.

MagicSetTransformer (ast/transform/MagicSet.cpp)
- Purpose: query-driven rewriting (demand-driven evaluation).
- Behavior: adorns and rewrites rules so only derivations relevant to
  output/prob-query relations are produced.
- Prob semantics: DEPENDS.
  - SAFE for **query/output** semantics. The implementation explicitly
    includes ProbQuery relations when normalising outputs.
  - UNSAFE if evidence relations are not included in the slice (the current
    implementation does **not** consider Evidence objects). Evidence-related
    derivations could be pruned.
  - Also note: magic predicates are introduced; if you need derivation graphs
    in original rule language, you must map proofs back.

IOAttributesTransformer (ast/transform/IOAttributes.h)
- Purpose: populate IO directive metadata.
- Behavior: inserts attribute names/types/params JSON into IO directives.
- Prob semantics: SAFE (directive-only changes).

ConstantNormalizationTransformer
(ast/transform/ConstantNormalizationTransformer.cpp)
- Purpose: normalize constants into variables plus equality constraints.
- Behavior: replaces constants in terms with fresh variables and adds
  `X = const` constraints in the body.
- Prob semantics: SAFE for derivation sets (substitution space preserved).

Meta/control transformers (Pipeline/Fixpoint/Conditional)
- Purpose: control only.
- Prob semantics: SAFE.

### AST-focused safety summary (query/output/evidence semantics)

UNSAFE by construction (remove/merge in-scope derivations):
- MinimiseProgramTransformer
- RemoveRelationCopiesTransformer
- ReduceExistentialsTransformer

UNSAFE unless modified to include evidence/query targets:
- RemoveRedundantRelationsTransformer (outputs-only analysis)
- RemoveEmptyRelationsTransformer (does not check evidence/query targets)
- MagicSetTransformer (safe for outputs/ProbQuery, unsafe for Evidence)

DEPENDS on EQREL semantics:
- ExpandEqrelsTransformer

------------------------------------------------------------------------
## RAM pipeline (RAM-level)

### Current order (as wired in MainDriver)
1. Loop( ExpandFilter -> HoistConditions -> MakeIndex )
2. IfConversionTransformer
3. IfExistsConversionTransformer
4. CollapseFiltersTransformer
5. TupleIdTransformer
6. Loop( HoistAggregateTransformer -> TupleIdTransformer )
7. ExpandFilterTransformer
8. HoistConditionsTransformer
9. CollapseFiltersTransformer
10. EliminateDuplicatesTransformer
11. ReorderConditionsTransformer
12. Loop( ReorderFilterBreak )
13. Conditional( ParallelTransformer ) when jobs != 1
14. ReportIndexTransformer

### RAM transformations: detailed notes and probabilistic impact

ExpandFilterTransformer (ram/transform/ExpandFilter.cpp)
- Purpose: split filter conjunctions into nested filters.
- Prob semantics: SAFE for derivations (logical conditions preserved).

HoistConditionsTransformer (ram/transform/HoistConditions.cpp)
- Purpose: hoist filters outward when safe.
- Prob semantics: SAFE for derivations; only evaluation order changes.

MakeIndexTransformer (ram/transform/MakeIndex.cpp)
- Purpose: rewrite constraints to enable index scans.
- Prob semantics: SAFE (logical equivalence preserved).

IfConversionTransformer (ram/transform/IfConversion.cpp)
- Purpose: replace IndexScan with existence checks when tuple values unused.
- Prob semantics: UNSAFE if you build derivation graphs at RAM level,
  because enumeration of witnesses is collapsed into a single existence test.

IfExistsConversionTransformer (ram/transform/IfExistsConversion.cpp)
- Purpose: replace Scan/IndexScan + Filter with IfExists/IndexIfExists.
- Prob semantics: UNSAFE for RAM-level derivation graphs (same reason).

CollapseFiltersTransformer (ram/transform/CollapseFilters.cpp)
- Purpose: merge consecutive filters.
- Prob semantics: SAFE.

TupleIdTransformer (ram/transform/TupleId.cpp)
- Purpose: renumber tuple ids.
- Prob semantics: SAFE.

HoistAggregateTransformer (ram/transform/HoistAggregate.cpp)
- Purpose: hoist aggregates outward where dependencies allow.
- Prob semantics: SAFE for logical derivations; only evaluation order changes.

EliminateDuplicatesTransformer (ram/transform/EliminateDuplicates.cpp)
- Purpose: remove duplicate conditions.
- Prob semantics: SAFE.

ReorderConditionsTransformer (ram/transform/ReorderConditions.cpp)
- Purpose: reorder conditions by estimated complexity.
- Prob semantics: SAFE.

ReorderFilterBreak (ram/transform/ReorderFilterBreak.cpp)
- Purpose: swap filter/break nesting for efficiency.
- Prob semantics: SAFE.

ParallelTransformer (ram/transform/Parallel.cpp)
- Purpose: parallelize outermost loops when safe.
- Prob semantics: DEPENDS. Results are the same, but derivation ordering is
  nondeterministic; if derivation IDs depend on order, disable or use jobs=1.

ReportIndexTransformer (ram/transform/ReportIndex.h)
- Purpose: analysis/report only.
- Prob semantics: SAFE.

Meta/control transformers (Sequence/Loop/Conditional)
- Prob semantics: SAFE.

------------------------------------------------------------------------
## Consolidated "no-souffle-opt" suggestion (query/output/evidence semantics)

If `--no-souffle-opt` aims to preserve complete derivations **within the
query/output/evidence slice**, consider disabling at least:

AST layer:
- MinimiseProgramTransformer
- RemoveRelationCopiesTransformer
- ReduceExistentialsTransformer

Additionally, disable or adjust these unless you include Evidence/ProbQuery
relations in their analyses/renaming:
- RemoveRedundantRelationsTransformer
- RemoveEmptyRelationsTransformer
- MagicSetTransformer (safe only for outputs/ProbQuery; evidence not handled)

RAM layer (only if derivation graph is built at RAM level):
- IfConversionTransformer
- IfExistsConversionTransformer
- ParallelTransformer (or force `--jobs 1`)

------------------------------------------------------------------------
## TODOs (per current probabilistic semantics requirements)

Pipeline removals (currently unused/unsupported):
- Remove DebugDeltaRelationTransformer from AST pipeline.
- Remove ReduceExistentialsTransformer from AST pipeline.
- Remove ExpandEqrelsTransformer from the magic-set pipeline.
- Remove ParallelTransformer from RAM pipeline.

Evidence-aware slicing (query/output/evidence semantics):
- Define the relevant relation slice as the **predecessor closure** of
  (output relations + ProbQuery relations + Evidence relations).
- Restrict redundancy-related transforms to **outside** this slice:
  - MinimiseProgramTransformer
  - RemoveRelationCopiesTransformer
  - RemoveRedundantRelationsTransformer
  - RemoveEmptyRelationsTransformer
- Ensure any relation renaming/removal updates **ProbQuery** and **Evidence**
  nodes (currently only clauses are renamed).

Magic set:
- Disable MagicSetTransformer when evidence is present, until evidence
  relations are included in the magic-set slice and renaming logic.
