# MST Notes

This note describes MST-specific changes in the translator and how they affect
ProbLog emitRules and the derivation graph.

## Translator changes (ClauseTranslator)
- We adjust ClauseTranslator (and IncClauseTranslator) to recognize simple
  constant bindings from binary constraints of the form:
  - `<var> = <const>`
  - `<const> = <var>`
- These bindings are used to populate varExprMap so emitRules can still
  materialize a full varValues vector for RuleApplication, even when constants
  are represented via normalization constraints.

## ProbLog emitRules impact
- MST normalization may introduce additional variables via equality constraints.
- emitRules still skips constraints in the rule body, so the translator-side
  varExprMap binding is required to avoid "variable not grounded" issues.
- This preserves the original seminaive semantics while keeping ProbLog rule
  applications well-formed.

## New edges in derivation graph
- MST emitRules can introduce intermediate relations:
  - `@split_in`
  - `@interm_in`
  - `@interm_out`
- These create additional edges in the derivation graph. A later graph fuse can merge them to reduce overhead.

## Build / Run
- Compile command: 
- --magic-transform='Relations' enable mst changes on the given relations.
- --magic-transform='*' enable mst changes for all relations.
