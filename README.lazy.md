# Lazy decision-diagram plan

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

## 更细的实施拆解
- **Symbolic DAG 扩展 (src/include/souffle/problog/formula/LogicFormulaManager.h 新增/改动)**  
  - 增加常量节点、`true/false` 单例；在 `makeAnd/makeOr` 中做扁平化、去重、排序（按 varId + hash），并缓存 hash 以供 `isSame` O(1)。  
  - 添加 `LazyNodeRef` 包装，兼容现有 `get()`/bool 检查，并暴露 `kind()/operands()` 便于 materialize。  
  - 维护 `varId -> Node/Edge` 的弱映射（仅在需要打印/权重时用）。

- **LazyManager 实现 (新文件例如 src/include/souffle/problog/formula/LazyManager.h)**  
  - 继承 `DDManager<LazyNodeRef>`；内部持有 `SymbolicArena` + 权重表 + 可选 `preConfig` 结果。  
  - `makeCondition`/`postprocessUselessVariables` 直接在符号层实现；`setVariableWeight` 更新权重并标记 materialization 缓存失效。  
  - 提供 `ensureMaterialized(const LazyNodeRef&)`、`dropMaterialized()`、`strongIsSame(a,b)`（触发 backend 比较）等工具。

- **Backend Bridge (同文件或单独 adapter)**  
  - 模板 `LazyBackend<DDManagerType, NodeRefType>`，第一次 materialize 时创建真实 backend，执行保存的 `preConfig`，按访问到的 varId 创建变量并回放权重。  
  - 利用 `transform(symbolicRoot, symbolicMgr, backendMgr)` 转成 DD；加 `std::unordered_map<LazyNodeRef, NodeRefType>` 做 compiled 缓存。  
  - 失效策略：修改权重/调用 `dropMaterialized` 时清缓存，保留符号 DAG。

- **具体管理器与管线接入**  
  - 定义别名/类 `CuddLazyManager`、`SddLazyManager`，构造时注入对应 backend。  
  - `ForwardCompilation` 里添加类型别名 `using DefaultFormulaManager = LazyOrEager...`，通过 CLI flag `--dd-backend` 选择。默认走 lazy-cudd。  
  - `Pipeline` 和生成的 `compute.cpp` 里用统一工厂创建 manager，避免多处条件编译。

- **测试与验证**  
  - 单测：符号等价、常量折叠、条件化、权重传播、缓存失效/重建。  
  - 回归：对小图同时跑 eager/lazy，校验 WMC 相等；在 smokers/eqrel 记录内存/时间差。  
  - 可选：在 debug 模式下 dump 符号公式 + materialized size，帮助对比。

## 数据结构设计草案
- **LazyNode (内部节点定义)**  
  - 字段：`enum Kind { CONST_TRUE, CONST_FALSE, VAR, AND, OR, NOT } kind; int varId; std::vector<LazyNodeRef> ops; std::optional<const Node*> node; std::optional<const Hyperedge*> edge; size_t hash;`  
  - 不变量：AND/OR 的 `ops` 已扁平化且按 `(kind, varId, hash)` 排序、去重；NOT 仅 1 个子节点；VAR 仅 varId/node/edge；常量节点无子节点。  
  - hash：构造时计算并缓存，AND/OR 采用 commutative hash（如 FNV/xxh 组合），用于快速查重。

- **LazyNodeRef (句柄)**  
  - 字段：`std::shared_ptr<LazyNode> ptr;` 提供 `get()`/bool，`kind()/operands()/varId()` 访问器。  
  - 比较：指针相等；哈希：指针地址。  
  - 工厂只通过 `SymbolicArena` 创建，保证同构节点复用。

- **SymbolicArena (去重工厂)**  
  - 状态：`unordered_set<std::shared_ptr<LazyNode>, LazyNodeHash, LazyNodeEq> pool; LazyNodeRef trueRef/falseRef;`  
  - API：`makeConst(bool)`, `makeVar(varId,node*,edge*)`, `makeAnd(vector<LazyNodeRef>)`, `makeOr(...)`, `makeNot(...)`，内部完成扁平化、排序、常量折叠、去重。  
  - 记录：保留 `unordered_map<int,const Node*> nodeMap` / `edgeMap` 供打印和权重绑定。

- **LazyManager (继承 DDManager<LazyNodeRef>)**  
  - 状态：`SymbolicArena arena; unordered_map<int, WeightPair> weights; optional<PreconfigData> preconfig; bool dirty=true; unique_ptr<BackendAdapter> backend;`  
  - API：`createVar`, `makeAnd/Or/Not`, `makeCondition`（符号化），`setVariableWeight`（置 dirty），`computeWeightedModelCount`（ensureMaterialized），`dropMaterialized`，`strongIsSame`。  
  - Profiling：返回符号节点数/unique var 数；materialized 后合并 backend 数据。

- **BackendAdapter (模板)**  
  - 状态：`std::unique_ptr<RealMgr> mgr; unordered_map<int, NodeRef> varCache; unordered_map<LazyNodeRef, NodeRef> compiled;`  
  - API：`materialize(root, arena, weights, preconfig) -> NodeRef`；内部：构建 mgr、配置顺序、createVar+weights、调用 `transform`。  
  - 失效：`clearCompiled()` 清空 `compiled`/`varCache`；`mgr` 可复用或重建（取决于权重/顺序变化策略）。

- **预处理/顺序数据 (PreconfigData)**  
  - 包含 `std::vector<int> order`（来自 `heuristics.compute`）和任何需要回放给 backend 的参数，LazyManager `preConfig` 填充，materialize 时使用。

## Risks and mitigations
- **Symbolic blow-up**: Very wide OR/AND may make the DAG large; mitigate with dedup, hashing, and optional chunked materialisation per stratum.  
- **Ordering sensitivity**: Without DD reordering, `isSame` relies on canonical operand ordering; enforce sorting and constant-folding.  
- **Weight drift**: Ensure weights are frozen before materialisation or guard `setVariableWeight` to invalidate compiled caches.  
- **Heuristic reuse**: If heuristics depend on live DD stats, they may differ in lazy mode; start with static ordering and only enable dynamic reordering after materialisation.
