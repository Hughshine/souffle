# Elastic Mode Notes (Design)

## Status
- Design note only; `--setmode elastic` is accepted and currently falls back to
  `inc-naive`. A true elastic scheduler is still not implemented.

## Source references
- [src/include/souffle/cli/Cli.h](src/include/souffle/cli/Cli.h)
- [src/include/souffle/problog/DerivationGraph.h](src/include/souffle/problog/DerivationGraph.h)

下面我先把 **graph generation（完整 derivation graph 维护）阶段**里“全量 vs 增量”的切换思路整理成一套可直接落地的 elastic 方案，并把它和 Zhao 等 PPDP’21 的 **Bootstrap–Update** 框架一一对齐。你的直觉是对的：从确定性 Datalog 到概率版（Problog/ProbLog 风格）本质上只是把“计算状态”从 *IDB 元组/计数* 扩展成 *完整 provenance/derivation graph*；切换逻辑仍然是同一类问题。

---

## 1) Graph generation 阶段：两种策略的定义（对齐论文的 Bootstrap vs Update）

Zhao 论文把弹性增量化抽象成两种策略：

* **Bootstrap（Pb）**：类似 batch-mode 的“重算并重建计算状态”，用于高影响更新；
* **Update（Pu）**：增量评估，用上一次的计算状态处理本轮 diff。论文明确强调“完全非增量”和“完全增量”都可能在某些场景无效，因此需要这种 hybrid/elastic。 

把它映射到你的 **derivation graph generation/maintenance**：

### G-Bootstrap（全量重建 derivation graph）

输入：当前 EDB（含 probabilistic facts 的存在性）+ 规则程序
输出：

* 完整 derivation graph（nodes/edges/索引：tuple→node，edgeKey→edge 等）
* 以及后续增量所需的“计算状态”（例如 rule instantiation 计数、或你实现中可用于快速删插的索引/缓存）

这相当于论文里的“从头生成 σk”。

### G-Update（增量维护 derivation graph）

输入：上轮计算状态 σk−1 + 本轮 diff（先 delete 后 insert）
输出：更新后的 derivation graph 与新状态 σk

实现上通常对应：

* 逻辑层面：DRed / Backward-Forward / DRedc 等增量求值家族；
* 图层面：按 delta rule apps / tuple changes 去 insert/delete 图节点边、维护索引。

论文也点名了 DRed 的核心弱点：**over-deletion**，需要 re-derive 把误删的再推回来。

---

## 2) 为什么 delete 超多会让 G-Update 输给 G-Bootstrap（与你观察一致）

论文用静态分析的例子解释了“重更新”下增量退化的根因：当结构变化很大时，增量几乎做了“两遍工作”（删旧 + 算新），还叠加增量化额外开销，因此 heavyweight 更新更适合接近 batch-mode 的策略。

你的场景里，当 delete 超过 90% 时，G-Update 的成本很容易接近（甚至超过）：

* 图层面：大量 node/edge 从 adjacency、map、set 中删除（高常数因子、cache miss、rehash/erase 成本）；
* 语义层面：DRed 的 over-delete 可能触发大范围 re-derive（相当于又做了一次大插入/重推导）。

因此，“delete 很多时直接重建”是非常合理的弹性策略入口。

---

## 3) 论文的自动切换策略是什么（核心 heuristic）

论文给了一个非常直接、工程化的切换 heuristic：

> **当 incremental update 的运行时间超过“上一轮 bootstrap 运行时间”的某个比例（switching parameter）时，重新跑 bootstrap。**

更具体地，论文在方法描述中强调：

* **先尝试 Update**；
* 如果 Update **超时**（超时阈值设为“上一轮 Bootstrap runtime × switching parameter”），就 **丢弃 Update 的 partial state**，改用 Bootstrap 从头产生输出和状态；并指出这个 timeout 需要按应用调优。

他们在实验中用过一个经验值：**20% switching parameter**——即 update 时间超过上次 bootstrap 的 20% 就触发重启。

同时，论文也承认固定阈值会误判：在某些 workload（例如 CRDT）里，elastic 策略频繁触发 20% 阈值，但如果让 Update 跑完反而会更快。
这意味着：切换策略要结合你的 workload 特征（例如 delete-heavy）做分段/自适应，而不是盲用 0.2。

---

## 4) 把论文切换策略“移植”到 probabilistic derivation graph 维护：要点与差异

你说“从确定性到概率”，我建议这样理解差异，避免把问题复杂化：

### 4.1 结构更新 vs 权重更新要分流

* **结构更新**（fact 插入/删除、规则触发变化）才会改变 derivation graph 拓扑；这是 G-Update 与 G-Bootstrap 的主要成本来源。
* **权重更新**（同一 probabilistic fact 仍存在，只是概率参数变化）通常不需要改 derivation graph 结构，只要更新叶子权重即可；这类更新在 graph generation 阶段可以固定走轻量路径（无需参与 elastic 决策）。

这一点能显著降低“误触发重建”的概率。

### 4.2 “计算状态 σ”从 IDB 扩展为 derivation graph + 索引

论文里的 σ 是为了后续增量而维护的轻量状态；在你这里，σ 至少包含：

* derivation graph 本体（nodes/edges + adjacency）
* tuple→node、edgeKey→edge 等索引
* 以及你在 DRed/增量求值中需要的辅助状态（例如支持 re-derive 的计数/证明信息）

因此你做弹性切换时的“丢弃 partial state”必须落到工程机制上：**要么能回滚，要么能在 shadow state 里尝试增量**（见第 6 节建议）。

---

## 5) 我建议的“graph generation 阶段”切换方案：预判 + watchdog 的两级策略

仅靠 runtime watchdog 会有一个现实问题：如果你无法中途 abort/回滚增量更新，那么“跑到超时才切换”救不了单轮 latency。
因此我建议两级：

### Level-0：极便宜的预判（避免对明显大删做增量尝试）

当出现你说的典型退化情形时，直接走 G-Bootstrap：

* `delete_ratio` 的定义建议优先用**结构性工作量**，例如：

  * `|deletedFacts| / |currentFacts|`（EDB 视角）
  * 或（更贴近 graph generation）`|deltaDeleteEdges| / |edges|`、`|deltaDeleteNodes| / |nodes|`（DG 视角）
* 如果 `delete_ratio ≥ θ_hard`（你观察到 0.9 是危险区间，那就从 0.85/0.9 起步），直接 G-Bootstrap。

这一步本质上就是把你观测到的“delete>90% 常输”固化成策略。

### Level-1：Zhao 风格 runtime switching（watchdog + restart）

对不那么极端的更新，再用论文的 heuristic：

* 维护 `T_boot_last`（上一轮 G-Bootstrap 的耗时；或 EWMA 平滑后的基准）
* 设 switching parameter `ρ`（fraction）
* 尝试 G-Update，但给它一个预算：

  * `budget = ρ * T_boot_last`
* 若 G-Update 在执行中达到 budget：

  * **终止/丢弃增量执行**并改跑 G-Bootstrap（论文用“discard partial state and produce from scratch”描述这一点）

> 论文实验中 `ρ=0.2` 的含义就是：update 超过上次 bootstrap 20% 就重启。

### ρ 怎么设（给你一个和 delete 相关的建议）

考虑到你是 online 单轮响应，且 delete-heavy 更易退化，我建议让 ρ 随 delete_ratio 分段：

* `delete_ratio >= 0.9`：不尝试增量，直接 bootstrap
* `0.6–0.9`：ρ 取 0.1–0.2（更激进止损）
* `0.2–0.6`：ρ 取 0.2–0.4
* `<0.2`：ρ 取 0.4–0.8（避免误触发；论文也指出固定 20% 可能过于保守导致本来 Update 更快却被截断）

---

## 6) 工程落地的关键：如何做到“超预算就切换到重建”

论文的切换是假设你能“丢弃 partial state”。

在你的 derivation graph 维护里，要做到这一点，有三种工程路径（按推荐程度排序）：

### A) Shadow-state 尝试（推荐第一优先，最简单可靠）

* 每轮更新开始时，为 G-Update 建一个“写时复制/增量副本”或直接复制关键索引结构（不一定要全拷贝所有 node/edge 对象，关键是能隔离变更）。
* G-Update 成功且未超预算：commit 到主状态；
* 超预算：直接丢掉 shadow，走 bootstrap 重建主状态。

优点：逻辑最干净，不需要复杂回滚；缺点：要设计轻量复制（避免复制整张大图）。

### B) Undo-log 回滚（次推荐）

* 在执行 applyDeltaDeletes / applyDeltaInserts 时，把每一步对容器的修改记录到 log（删除了哪些 edge、从哪些 adjacency vector 移除了什么、map erase 了什么等）。
* 超预算：按 log 逆操作回滚。

优点：不用复制全状态；缺点：实现复杂，且 vector erase/rehash 等回滚成本不低。

### C) “本轮不回退、下轮强制 bootstrap”（只适合离线，不适合你 online）

* 如果增量跑慢，本轮只能硬着头皮跑完；然后下轮强制 bootstrap。
* 这不满足你“单轮 online latency”目标，通常不建议。

---

## 7) 额外提醒：不要指望“update size / impact size”永远能预测 runtime

论文在实验里明确指出：即便 update size 或 impact size 相近，runtime 仍可能非常不可预测；甚至出现“高 impact 的 update 反而更快、低 impact 反而更慢”的反例，因此 size/impact 不总能作为可靠预测器。

这对你意味着两点：

1. **预判规则只能做“明显大删/大更”这种 hard case**；
2. **真正兜底仍要靠 watchdog**（Zhao 的 heuristic 本质也是 runtime watchdog）。

---

## 8) 一句话总结（你要的“切换思路”）

在 graph generation 阶段，建议采用与 Zhao 等一致的 **Bootstrap–Update 弹性框架**：

* 把“完整 derivation graph + 索引/计数信息”视为计算状态 σ；
* 同时实现 **G-Bootstrap（全量重建 σ）** 与 **G-Update（DRed 风格增量维护 σ）**；
* 使用 runtime-based heuristic：**当 Update 耗时超过上次 Bootstrap 耗时的某个比例（switching parameter）就丢弃 partial state 并重启 Bootstrap**；
* 结合你 delete-heavy 的退化现象，在 watchdog 之前加一条 **delete_ratio ≥ 0.9 直接 bootstrap** 的 hard gate。

---

## 9) 在本 repo 可落地的 ELASTIC 方案（EDB-only + FULL-SOFT）

本 repo 已有 `IncMode::ELASTIC`（`src/include/souffle/cli/Cli.h`），但分支目前仍是 `assert(false)`。如果你希望“不要太复杂”，最贴近 Zhao 等 PPDP’21 论文的落地方式其实就是 **runtime watchdog**：

- 记录上一次 **FULL-SOFT（Bootstrap）** 的耗时 `T_bootstrap`；
- 对每一轮 **delete turn**，先尝试增量（Update / DRed），但设置一个超时预算 `budget = ρ · T_bootstrap`；
- 一旦 delete 更新耗时超过预算，就 **中止当前增量 delete**，直接回退到 FULL-SOFT 重算；
- **insert turn 永远走增量**（不触发 FULL），因为它本质上是单调扩张的半朴素增量闭包。

下面假设更新只针对 EDB，且回退采用 FULL-SOFT（内存重算、无需磁盘 reload）。

### 9.1 记录哪个时间、用哪个阈值（最小参数化）

沿用论文 heuristic 的“可解释参数”就够了：

- `T_bootstrap`：上一次 FULL-SOFT 跑完整轮（从 EDB 快照重算到 fixpoint）的 wall time。
- `ρ`：switching parameter（论文里实验用过 0.2，但你的 workload 可能需要更大；先从 0.2–0.5 试）。
- `budget_ms = ρ * T_bootstrap_ms`：本轮 delete 增量允许的最大时间。

这比“估算影响/扇出/BFS”等 cost model 简单很多，也更贴近你想要的“按上一轮时间做切换”。

### 9.2 delete-only watchdog：timer callback + cooperative abort

关键是把“超时”变成一个 **不侵入语义** 的 abort 信号：

1) **CLI/Driver 侧启动定时器**
   - delete turn 开始时：启动一个 timer thread（或基于 `std::condition_variable` 的 wait_for），在 `budget_ms` 到时后把 `std::atomic<bool> elastic_timeout` 置为 `true`。
   - delete turn 正常结束或发生回退时：关闭 timer（并把 flag 清回 `false`）。

2) **DRed delete 路径里插入检查点**
   - 需要在“可能跑很久”的循环里定期检查 `elastic_timeout`，一旦为真就抛出/返回一个 `ElasticAbort` 信号。
   - 对应在线增量翻译里最自然的检查点是：
     - over-delete fixpoint（`generateRecursiveStratumInc(..., /*isDelete=*/true)` 里的 `loop_counter1` 循环）
     - re-derive fixpoint（`generateStratumRederive(...)` 里的 `loop_counter_rederive` 循环）
   - insert 路径（`/*isDelete=*/false`，`loop_counter2`）不插检查点，从机制上保证“insert 不触发 FULL”。

3) **捕获 abort 并回退 FULL-SOFT**
   - 一旦 delete 增量被中止：不尝试回滚 partial state，而是直接执行 FULL-SOFT 清空并重算（见 9.5），相当于论文里的“丢弃 Update 的 partial σ，重启 Bootstrap”。

> 直觉：这个 watchdog 是“时间上止损”，而不是“预测哪个更快”；它把 delete 的最坏情况从“可能跑到天荒地老”变成“最多跑 budget，然后 FULL-SOFT”。

### 9.3 deletion / insertion 的进一步解耦（保证 insert 永远增量）

为了满足你“insert 永远不用回到 full”的要求，建议在 **turn 语义** 上做强制解耦：

- **理想情况（你的 benchmark 已经这样做）**：delete facts → `commit` → insert facts → `commit`。  
  这样每次 `commit` 只有一种 diff，自然满足：只有 delete turn 需要 watchdog / 可能回退 FULL。

- **如果用户在同一个 `commit` 里混了 delete+insert**：ELASTIC 模式可以在 CLI 内部把它拆成两个子 turn：
  1) 先应用 delete diff，运行 delete turn（带 watchdog；超时就 FULL-SOFT）。
  2) 再应用 insert diff，运行 insert turn（始终增量；不设 watchdog）。

实现上就是：
- delete turn：只填 `$inc_delta_tuple_delete_*`（insert delta 为空）
- insert turn：只填 `$inc_delta_tuple_insert_*`（delete delta 为空）

这样做的好处是：watchdog 只包住 delete 的那一次 `runAllInc`，不会因为 insert 边际增量而误触发回退。

### 9.4 最小化落地点（不改太多结构）

在本 repo 里，最小可行落地点就是补全 `IncMode::ELASTIC` 分支，并复用现有 FULL-SOFT 路径：

```text
commit(ELASTIC):
  if hasDeletes:
    startTimer(budget = ρ * T_bootstrap)
    try: run delete-turn incrementally
    catch ElasticAbort: run FULL-SOFT (bootstrap) on post-delete EDB snapshot
    stopTimer()

  if hasInserts:
    run insert-turn incrementally (no timer)

  update T_bootstrap when FULL-SOFT ran
```

其中 “post-delete EDB snapshot” 在当前实现里可以直接落在 `initialInputRelations`：delete-turn 先更新它，FULL-SOFT 时再 `loadInitialInputRelations()` 装载回 input relations。

### 9.5 FULL-SOFT 的语义边界（保证正确性）

为了让 “INC↔FULL” 的切换不引入隐式状态污染，FULL-SOFT 回退必须遵守：

- **EDB 真值快照**：以 `initialInputRelations` 作为“当前 EDB”，FULL 时用它重新装载 input relations。
- **彻底清空派生/辅助状态**：清空所有 IDB/临时/增量辅助关系（`@old_* @delta_* @new_* @inc_* $inc_delta_*`），以及 `DerivationManager` 的 complete/delta maps。
- **DDManager 只做 soft reset**：保留必要的内存池/缓存（对应 FULL-SOFT 的意图），但不要保留会导致语义不一致的派生结果。

建议在日志里打印一次决策输入与原因，便于调参：
- `[elastic] delete budget_ms=... T_bootstrap_ms=... exceeded=... -> FULL-SOFT`

### 9.6 验证与调参建议

- 用 `docs/topics/evaluation/README.eval.inc.md` 里的 side-channel benchmark 作为调参闭环：记录同一 workload 下 `INC(delete)` 与 `FULL-SOFT` 的 wall time，以及实际触发回退的次数。
- 目标不是“永远选对”，而是：
  - delete-heavy case 不再出现 “INC 比 FULL 慢一个数量级”；
  - 其余 case 仍尽量吃到增量收益。

## Related commits
- `619e52197` — fix(inc): track explicit deletes and log deltas
