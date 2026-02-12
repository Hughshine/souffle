# Lazy decision-diagram plan

## Source references
- [src/include/souffle/problog/formula/FormulaManager.h](src/include/souffle/problog/formula/FormulaManager.h)
- [src/include/souffle/problog/ForwardCompilation.h](src/include/souffle/problog/ForwardCompilation.h)


## Status
- Design/roadmap only; not implemented in code.
- Independent of online incremental DRed (applies to formula managers).

## Why
- CuddManager and SddManager eagerly build DDs for every `makeAnd`/`makeOr`, so intermediate nodes blow up even though only equivalence checks and weighted model counting actually need canonical DDs.
- A lazy layer that keeps formulas symbolic until required should cut DD construction, reduce reordering churn, and make it cheaper to try alternative orderings/backends.

## Feasibility
- `FormulaManager` already abstracts formula operations; `LogicFormulaManager` proves we can keep a DAG of `VAR/AND/OR/NOT` without touching a DD backend.
- DD backends can be built on demand with `transform(src, srcMgr, dstMgr)`; weights and variable metadata are already tracked separately.
- Equality checks inside forward compilation only need structural stability, not full BDD equivalence, so canonical symbolic DAGs suffice for fixpoint detection.
- WMC and cross-manager equivalence are rare and can pay the one-time compilation cost.

## Proposed design
### Layers
1) **Symbolic core**: A `LazyNodeRef` (wrapper over a shared `LogicNode`-like DAG) that canonicalises commutative ops (flatten, sort, dedup, constant-fold `true/false`). Provides `get()`/bool to stay compatible with existing users.  
2) **LazyManager**: Implements `FormulaManager<LazyNodeRef>`, stores weights and optional node/edge metadata. `isSame` uses pointer identity on the canonical DAG; an optional `strongIsSame` can call the backend when needed.  
3) **Backend adapter**: Templated `LazyBackend<DDManager>` that materialises a symbolic root into CUDD/SDD using `transform`, caches the mapping `LazyNodeRef -> DDRef`, and reuses a single backend instance (created at first materialisation).  
4) **Concrete types**: `CuddLazyManager` and `SddLazyManager` plug in `WeightedBDDManager` or `SddFormulaManager`. They proxy `computeWeightedModelCount`/`setVariableWeight` to the backend after ensuring materialisation.

### Materialisation path
- Defer allocating DD variables; keep a list of referenced var ids and their Node/Edge attachments.
- On first call to `computeWeightedModelCount`, `strongIsSame`, or an explicit `materialize()`, build the backend manager, replay `preConfig` heuristics (variable ordering), create vars, push weights, then run `transform` to get a DD node. Cache results so repeated WMC does not recompile.
- Allow backend eviction: keep symbolic DAG as source of truth; `dropMaterialized()` frees DD caches to control memory.

### Behavioural details
- `makeAnd`/`makeOr` should flatten nested same-kind nodes, sort operands by var-id/hash to keep structural equality meaningful, and short-circuit on `true/false`. That preserves the fixpoint logic that relies on `isSame`.
- `makeCondition` operates symbolically; when materialising, emit the same conditioning via backend `makeCondition`.
- `postprocessUselessVariables` can be a no-op symbolically; if materialised, forward to backend and invalidate caches if it reordered vars.
- `getProfilingStatistics` returns symbolic counts (node/edge) until materialised; after materialisation, merge backend stats.
- Keep a small debug helper to dump the symbolic formula so existing logging keeps working without requiring DD strings.

## Integration plan
1) **Symbolic DAG refresh**: Reuse `LogicNode` but extend with constant nodes, operand sorting, and cached hashes for fast `isSame`. Add a thin `LazyNodeRef` that exposes `get()` for compatibility.  
2) **LazyManager implementation**: Derive from `DDManager<LazyNodeRef>`; implement all formula ops; store weights and metadata maps; add hooks `ensureMaterialized()` and `materialized(const LazyNodeRef&)`.  
3) **Backend bridge**: Add a small adapter that owns the real `WeightedBDDManager`/`SddFormulaManager`, populates variables/weights, and calls `transform`. Cache compiled nodes and allow optional eviction.  
4) **Concrete managers**: Provide `CuddLazyManager`/`SddLazyManager` typedefs and keep existing eager managers intact for fallback and benchmarking.  
5) **Pipeline switch**: Introduce a CLI/env toggle (e.g., `--dd-backend lazy-cudd|lazy-sdd|cudd|sdd`); default to lazy for experiments. Wire `ForwardCompilation`/`Pipeline` to the new types via a single alias.  
6) **Validation**:  
   - Unit tests: symbolic equality (ordering/flattening), conditioning, weight propagation, cache eviction.  
   - Cross-check WMC/equivalence vs eager managers on small graphs.  
   - Benchmark memory/time on existing smokers/eqrel experiments (toggle lazy vs eager).

## More detailed implementation breakdown
- **Symbolic DAG extensions (add/modify in src/include/souffle/problog/formula/LogicFormulaManager.h)**  
  - Add constant nodes and `true/false` singletons; in `makeAnd/makeOr`, flatten, dedup, and sort (by varId + hash), and cache hashes for O(1) `isSame`.  
  - Add a `LazyNodeRef` wrapper to keep existing `get()`/bool checks, and expose `kind()/operands()` for materialization.  
  - Maintain a weak mapping `varId -> Node/Edge` (only used when printing/weighting).

- **LazyManager implementation (new file e.g. src/include/souffle/problog/formula/LazyManager.h)**  
  - Derive from `DDManager<LazyNodeRef>`; internally hold `SymbolicArena` + weights + optional `preConfig` results.  
  - Implement `makeCondition`/`postprocessUselessVariables` symbolically; `setVariableWeight` updates weights and marks materialization caches dirty.  
  - Provide `ensureMaterialized(const LazyNodeRef&)`, `dropMaterialized()`, `strongIsSame(a,b)` (triggers backend comparison), and related helpers.

- **Backend bridge (same file or separate adapter)**  
  - Template `LazyBackend<DDManagerType, NodeRefType>`; on first materialization create the real backend, apply saved `preConfig`, create variables for accessed varIds, and replay weights.  
  - Use `transform(symbolicRoot, symbolicMgr, backendMgr)` to convert to a DD; add `std::unordered_map<LazyNodeRef, NodeRefType>` as a compiled cache.  
  - Invalidation policy: clear caches on weight changes or `dropMaterialized`, while keeping the symbolic DAG.

- **Concrete managers and pipeline integration**  
  - Define aliases/classes `CuddLazyManager`, `SddLazyManager`, injecting the backend in the constructor.  
  - In `ForwardCompilation`, add a type alias like `using DefaultFormulaManager = LazyOrEager...`, and select via CLI flag `--dd-backend`. Default to lazy-cudd.  
  - Use a single factory in `Pipeline` and the generated `compute.cpp` to create managers and avoid scattered conditionals.

- **Testing and validation**  
  - Unit tests: symbolic equivalence, constant folding, conditioning, weight propagation, cache invalidation/rebuild.  
  - Regression: run eager/lazy on small graphs, check WMC equality; record memory/time differences on smokers/eqrel.  
  - Optional: in debug, dump symbolic formulas + materialized size for comparison.

## Draft data-structure design
- **LazyNode (internal node definition)**  
  - Fields: `enum Kind { CONST_TRUE, CONST_FALSE, VAR, AND, OR, NOT } kind; int varId; std::vector<LazyNodeRef> ops; std::optional<const Node*> node; std::optional<const Hyperedge*> edge; size_t hash;`  
  - Invariants: AND/OR `ops` are flattened and sorted by `(kind, varId, hash)` with dedup; NOT has exactly one child; VAR only carries varId/node/edge; constants have no children.  
  - Hash: computed and cached at construction; AND/OR use a commutative hash (e.g., FNV/xxh combo) for fast dedup.

- **LazyNodeRef (handle)**  
  - Field: `std::shared_ptr<LazyNode> ptr;` provides `get()`/bool, `kind()/operands()/varId()` accessors.  
  - Equality: pointer equality; hash: pointer address.  
  - Factory only creates via `SymbolicArena`, ensuring isomorphic nodes are reused.

- **SymbolicArena (dedup factory)**  
  - State: `unordered_set<std::shared_ptr<LazyNode>, LazyNodeHash, LazyNodeEq> pool; LazyNodeRef trueRef/falseRef;`  
  - API: `makeConst(bool)`, `makeVar(varId,node*,edge*)`, `makeAnd(vector<LazyNodeRef>)`, `makeOr(...)`, `makeNot(...)`, internally performing flattening, sorting, constant folding, and dedup.  
  - Record: keep `unordered_map<int,const Node*> nodeMap` / `edgeMap` for printing and weight binding.

- **LazyManager (extends `DDManager<LazyNodeRef>`)**  
  - State: `SymbolicArena arena; unordered_map<int, WeightPair> weights; optional<PreconfigData> preconfig; bool dirty=true; unique_ptr<BackendAdapter> backend;`  
  - API: `createVar`, `makeAnd/Or/Not`, `makeCondition` (symbolic), `setVariableWeight` (marks dirty), `computeWeightedModelCount` (ensureMaterialized), `dropMaterialized`, `strongIsSame`.  
  - Profiling: return symbolic node/unique var counts; after materialization merge backend stats.

- **BackendAdapter (template)**  
  - State: `std::unique_ptr<RealMgr> mgr; unordered_map<int, NodeRef> varCache; unordered_map<LazyNodeRef, NodeRef> compiled;`  
  - API: `materialize(root, arena, weights, preconfig) -> NodeRef`; internally build manager, configure ordering, create vars+weights, call `transform`.  
  - Invalidation: `clearCompiled()` empties `compiled`/`varCache`; `mgr` can be reused or rebuilt (depending on weight/ordering changes).

- **Preprocessing/ordering data (PreconfigData)**  
  - Contains `std::vector<int> order` (from `heuristics.compute`) and any parameters that need to be replayed to the backend; `LazyManager` fills `preConfig` and uses it during materialization.

## Risks and mitigations
- **Symbolic blow-up**: Very wide OR/AND may make the DAG large; mitigate with dedup, hashing, and optional chunked materialisation per stratum.  
- **Ordering sensitivity**: Without DD reordering, `isSame` relies on canonical operand ordering; enforce sorting and constant-folding.  
- **Weight drift**: Ensure weights are frozen before materialisation or guard `setVariableWeight` to invalidate compiled caches.  
- **Heuristic reuse**: If heuristics depend on live DD stats, they may differ in lazy mode; start with static ordering and only enable dynamic reordering after materialisation.

## Related commits
- `4dd403de4` — Translate Chinese comments and docs to English
- `4c4bd26b2` — docs(readme): restructure online incremental docs
- `fab2b08cf` — upd rewriter basic
