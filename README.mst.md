# Magic Set Transformation (MST) Notes

## Source references
- [src/MainDriver.cpp](src/MainDriver.cpp)
- [src/ast/transform/MagicSet.h](src/ast/transform/MagicSet.h)
- [src/ast/transform/MagicSet.cpp](src/ast/transform/MagicSet.cpp)
- [src/ast2ram/online/ClauseTranslator.cpp](src/ast2ram/online/ClauseTranslator.cpp)
- [src/ast2ram/online/IncClauseTranslator.cpp](src/ast2ram/online/IncClauseTranslator.cpp)
- [src/parser/parser.yy](src/parser/parser.yy)
- [src/synthesiser/Synthesiser.cpp](src/synthesiser/Synthesiser.cpp)
- [src/include/souffle/problog/DerivationGraph.h](src/include/souffle/problog/DerivationGraph.h)

This note describes MST-specific behavior in the AST pipeline and how it affects
the online translator and Synthesiser rule emission used by ProbLog features.

## When MST runs
- The MagicSetTransformer runs only when `--magic-transform` is set or when a
  relation uses the `magic` qualifier.
- `--magic-transform-exclude` and the `no_magic` qualifier add relations to the
  ignored set (they override inclusion for those relations).
- `--magic-transform='*'` means all relations are candidates for MST, except
  those in the ignored set.

## Translator bindings for MST normalization
- The online ClauseTranslator/IncClauseTranslator recognize simple constant
  bindings from equality constraints of the form:
  - `<var> = <const>`
  - `<const> = <var>`
- These bindings populate `varExprMap` so rule emission can still build a full
  variable vector even when constants are represented via normalization
  constraints.

## Synthesiser rule emission impact
- MST normalization can introduce additional variables via equality constraints.
- `Synthesiser::emitRules` skips `ast::Constraint` body literals, so the
  translator-side `varExprMap` binding is required to avoid "variable not
  grounded" cases in emitted rules.

## Intermediate relations and derivation graph
- MagicSet normalization can introduce intermediate relations with prefixes:
  - `@split_in`
  - `@interm_in`
  - `@interm_out`
- These appear as additional nodes/edges in the derivation graph. A dedicated
  MST graph-fuse step is still TODO.

## Build / Run
- Use the standard build steps from [docs/USAGE.md](docs/USAGE.md), then invoke:
  - `./build/src/souffle --magic-transform='RelA,RelB' program.dl`
  - `./build/src/souffle --magic-transform='*' --magic-transform-exclude='RelC' program.dl`

## Example
```souffle
.decl b(X:number, Z:number)
.input b

.decl c(Y:number, Z:number)
.input c

.decl a(X:number, Y:number)

.decl res(X:number)
.output res

a(X, Y) :- b(X, Z), c(Y, Z).
res(Y) :- a(1, Y).

// Or:
// a(X, Y) :- b(X, Z), c(Y, Z).
// res(X) :- a(X, Y).
// query(res(1)).
```

## Related commits
- `e64f37aba` — docs(repo): add MST docs
- `c16d5349b` — added ConstantNormalizationTranslator, modified ClauseTranslators, added README
