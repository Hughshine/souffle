# Inc-Regional Incremental Pipeline (Current Design)

本文件描述当前代码实现的 inc-regional 增量 pipeline。内容以代码为准，不是规划文档。

## Scope / Assumptions
- deletion 先执行经典增量删除逻辑，再进入 regional insertion。
- 增量模式不执行 rewrite。
- 必须已有基线公式（`nodeFormulas`/`edgeFormulas` 非空），否则直接 `assert` 失败。
- 增量模式禁用 bi-imp merge（full merge 后不允许切换到 inc/inc-regional）。

## How to Enable
- CLI 参数：`--setmode inc-regional`（或交互 CLI 输入 `setmode inc-regional`）。
- `inc` / `incr` / `incremental` 现映射为 `inc-naive`。
- 输出文件按模式区分：
  - `fact-iter{N}-inc-naive.prob`
  - `fact-iter{N}-inc-regional.prob`
  - `fact-iter{N}-full.prob`

## Code Map
- `src/include/souffle/problog/RegionalIncremental.h`：主实现（Analyzer → Plan → Rebuild → Calibrate）。
- `src/include/souffle/problog/IncRegionAnalyzer.h`：region 分析、边界分类、mergeable anchors 缓存、分析 timing。
- `src/include/souffle/problog/DerivationGraph.h`：prune-inc 生成 impacted maps + deltaReach cache。
- `src/include/souffle/problog/ForwardCompilation.h`：`buildFormulasIncRegionalCyclewise` 入口。
- `src/include/souffle/cli/Cli.h` / `src/MainDriver.cpp` / `src/include/souffle/CompiledOptions.h`：mode 解析与迭代输出名。
- `src/include/souffle/problog/formula/*`：weight 读写与 reordering 时间统计。

## Pipeline Overview (Per Turn)
1) `applyDeltaDeletes`（旧逻辑）  
2) `applyDeltaInserts`（旧逻辑）  
3) `prune-inc`（构建子图 + impacted maps + deltaReach cache）  
4) Forward compilation：先跑 deletion（inc-naive 逻辑），再跑 inc-regional insertion  
5) WMC + 输出 iter 结果

inc-regional 只替换 insertion 的 forward compilation；deletion 复用 inc-naive 的 deletion 实现。

---

## Region Analysis (IncRegionAnalyzer)
输入：`IncrementalDerivationGraphViewInterface` + delta insert facts（若无则用 delta insert nodes）。  
输出：region、boundary 分类、mergeable anchors 缓存、deltaReachable 子图。

关键步骤（对应 `[inc-analyze] timing(ms)`）：
- `buildLeastParents_()` / `computeScopes_()`：范围与依赖结构。
- `reachFromSources_()`：从 delta sources 计算可达过滤器。
- `initialRegion_()`：初始 region。
- `classifyBoundaries_()` + `expandToFixpoint_()`：按边界规则扩展 region。
- `upstreamClose_()`：向上游补齐祖先（受 reach_filter 限制），必要时再分类/再扩展。
- `deltaReachable_()` + `intersectWithDeltaReachable_()`：保证 region 在 delta-reachable 内。
- `computeMergeableAnchors_()`：对 boundary 节点缓存可 merge 的旧入边候选。

边界分类：
- `out_induced` / `scope_induced` / `residual` 三类 boundary node。

Mergeable anchor 判定（`mergeableEdgeAtHead_`）：
- 不是 delta insert edge。
- 非确定性边（deterministic edge 被排除）。
- respects scopes。
- non-subsumed。

注意：Region 使用 `unordered_set`，遍历顺序不稳定。

---

## Impacted Maps & Delta Reach Cache (DerivationGraph)
`prune-inc` 会在子图上重建 impacted maps（插入/删除）：
- 从每个 delta insert fact 做 BFS（沿 outgoing edges）收集受影响 nodes/edges。
- 结果存入 `insertedFactImpactedNodes/Edges`（`unordered_set`）。
- 同时构建 **delta-insert reachable union cache**：
  - `deltaInsertReachableNodes`
  - `deltaInsertReachableEdges`
  该缓存通过 `getDeltaInsertReachableNodes/Edges()` 暴露给 analyzer。

`deltaReachable_()` 的优先级：
1) 使用 union cache（最快）  
2) 若为空，fallback 到 impacted maps  

`reach_filter_` 直接使用 delta-reachable cache（不再额外补丁）。

---

## Planning (RegionalInsertPlan)
`RegionalInsertPlanBuilder` 从 analyzer 缓存构建计划：
- `regionNodes`：region 内节点。
- `boundaryNodes`：三类 boundary 的并集。
- `anchorCandidates`：来自 analyzer 的 cached anchors。
- `mergeReady`：若 `boundaryNodes` 为空或每个 boundary 至少有一个 anchor，则为 true。
- `regionClosed`：`boundaryNodes` 为空。

---

## Rebuild (RegionalDDRebuilder)
目标：只重建 region 内节点/边，外部公式保持旧值。

步骤：
- **Snapshot**：保存 boundary 节点旧公式（用于后续校准）。
- **Init inserted nodes/edges**：为 delta insert nodes/edges 建立公式并设置权重。
- **SCC 处理**：构建 `CycleDependencyGraph`，取 region 涉及的 cycle 集合，计算 indegree。
- **重建循环**：
  - 只处理满足 `shouldRebuildEdge` 的边：
    - head 在 region 内，且（edge 是 delta-insert 或存在 region 内输入）。
  - worklist 按 edge depth 排序。
  - 若缺失输入来自 region 内，重新入队；若缺失输入仅来自 region 外，跳过。
  - 更新 edge formula 后重算 head node 的 OR 公式。
  - 需要时将受影响的 outgoing edges 再入队。

计时输出（`[inc-regional rebuild]`）：
`snapshot/initNodes/initEdges/depGraph/regionCycles/indegree/rebuildLoop/total/reorder`  
`reorder` 依赖 `FormulaManager::getReorderingTimeSeconds()`（CUDD 实现支持）。

---

## Calibration (BoundaryGateCalibrator)
目标：为 boundary 节点计算 gate 校准参数 `p*`（但当前不自动应用）。

流程（每个 boundary 节点 v）：
1) `target = Pr_new(v)`（用重建后的公式计算）。
2) 遍历 anchorCandidates[v]：
   - 用 snapshot 旧公式计算 `oldVal`。
   - 将 anchor 变量权重置 0 得 `alpha`，置 1 得 `beta`。
   - `p* = (target - alpha) / (beta - alpha)`。
3) 记录 `CalibrationRecord` 与 `weightOverrides`。

当前行为：
- `calibrate()` **只计算并返回 override**，不写回权重。
- `RegionalIncrementalForwardCompilation` 保存 `lastOverrides_` 供外部使用。

---

## Fallback & Guards
`RegionalIncrementalForwardCompilation::applyUpdate`：
- `mergeReady == false` → fallback 到经典 `buildFormulasIncCyclewise`。
- calibration 任一 boundary 失败 → fallback（可通过 `Options` 关闭）。
- `nodeFormulas`/`edgeFormulas` 为空 → `assert` 失败。

---

## Profiling / Logs
常见输出：
- `[inc-analyze] timing(ms): ...`
- `[inc-regional rebuild] timing(ms): ...`
- `[inc-regional] timing(ms): analyze sccClose plan rebuild calibrate total`
- `[prune-inc impact] ...`（prune-inc 重建 impacted maps）
调试输出默认关闭，可按需开启：
- `--dumpjson` / CLI `set dumpjson`：输出 JSON（prune 之后）。
- `--dumpdot` / CLI `set dumpdot`：输出 derivation graph 的 DOT。
- `--dumpstat` / CLI `set dumpstat`：输出 `dumpStatisticsInc` / `dumpStatistics`。

---

## Known Limitations / TODO
- Calibration 结果尚未自动 apply（仅计算并缓存）。
- Regional 仅覆盖 insertion；deletion 仍走旧逻辑。
- anchor 选择目前取第一个可行候选，没有评分/优化。
- Region 迭代顺序不稳定（`unordered_set`）；日志和输出顺序不保证稳定。

---

## Quick Run (Example)
```
./compute -F input -D output_run_inc_regional --setmode inc-regional < delta/inc10_1.txt
```
确认一致性：
```
diff output_run_inc_regional/facts.prob output_run_full/facts.prob
```
