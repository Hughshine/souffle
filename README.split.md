# Derivation Graph Split 优化设计文档（伪代码为主，Codex 可直接实现）

版本：v1.1（complete-split 合并语义修复，面向当前 “rewrite/split 暴露 SISO 结构” 的工程化优化）
范围：**split 机会发现 + split 变换本身 + 增量维护**（不依赖你现有源码结构，按可落地数据结构与伪代码描述）

---

## 0.1 当前实现状态（2026-01-04）

**实现与文档设想存在差异，先记录“已落地版本”的真实行为：**

* CLI 选项：`--split-mode={no-split|naive-split|complete-split}`（短名 `-P`），默认 `naive-split`。
* split 目前**只拆 fact 节点**；尚未对中间节点复制 incoming edges。
* complete-split 使用 **multi-source union-find** 对 outgoing 分组；**任何下游交汇都会合并分支**（不再用 `hasRVReach` 放过确定性汇合），以保证相关性不被破坏。`hasRVReach` 仅用于过滤候选 fact（下游无 RV 时不 split）。
* `hasRVReach` 目前每轮 split **全图重算**（从 RV 节点/边反向可达），尚未做增量维护。
* split 预算已接入：`splitMaxNewNodesPerPass`、`splitMaxNewEdgesPerPass`、`splitMaxGroupsPerNode`、`splitMinGroupEdges`。
* 调度已改为：**每次 rewrite 到 fixpoint 都尝试 split；若 split 发生则继续 rewrite 到 fixpoint，直到 split 也不再发生**（split 不再嵌入每轮 rewrite）。
* 日志：`[GraphRewriter] split(mode): nodes=.. edges=.. time=.. ms`；`siso_regions_iter*.dot` 会写入 output 目录（`--dumpdot`）。

## 0.2 近期实验结论（历史，P4–P19，三种 split 模式）

**结论摘要：**

* 语义更新（2026-01-04）：complete-split 已改为“任何交汇即合并”。以下表格为旧语义的历史结果，仅供参考；需按新语义重跑。P20 已验证修复后 rewrite 与 no-rewrite 一致。
* 输出一致：`facts.prob` 在所有 P4–P19 case 下三种 split 模式完全一致。
* naive-split 在多数大实例上几乎不触发（split=0），收益接近 no-split。
* complete-split 触发 split 更多：在启用“多轮 split”后，P17–P19 的 RV 比例显著下降、pipeline 总时间明显改善，但 rewrite 迭代/regions 成本上升。

**仅统计 pipeline 日志（`[pipeline] ... took X ms` 之和），不含 Souffle 规则求值时间：**

| case | no-split | naive-split | complete-split |
| --- | --- | --- | --- |
| P4  | 0 ms | 0 ms | 0 ms |
| P5  | 0 ms | 0 ms | 0 ms |
| P6  | 1 ms | 1 ms | 1 ms |
| P7  | 2 ms | 3 ms | 2 ms |
| P8  | 4 ms | 4 ms | 4 ms |
| P9  | 5 ms | 6 ms | 6 ms |
| P10 | 15 ms | 15 ms | 15 ms |
| P11 | 15 ms | 15 ms | 15 ms |
| P12 | 257 ms | 51 ms | 50 ms |
| P13 | 284 ms | 281 ms | 326 ms |
| P14 | 515 ms | 538 ms | 523 ms |
| P15 | 1121 ms | 1057 ms | 1054 ms |
| P16 | 2882 ms | 2696 ms | 2766 ms |
| P17 | 4252 ms | 4245 ms | 1807 ms |
| P18 | 7059 ms | 6908 ms | 2859 ms |
| P19 | 10211 ms | 11132 ms | 4420 ms |

> 注：P17–P19 为“多轮 split”调度下的最新结果；P4–P16 仍为上一次（单次 split）结果，需全量重跑以统一口径。

**split 本身代价（单次 split pass 的日志统计；no-split 不适用）：**

| case | naive-split (nodes/edges, ms) | complete-split (nodes/edges, ms) |
| --- | --- | --- |
| P4  | 0/0, 0.000 | 0/0, 0.002 |
| P5  | 0/0, 0.000 | 0/0, 0.002 |
| P6  | 4/4, 0.017 | 2/2, 0.019 |
| P7  | 3/3, 0.022 | 2/2, 0.029 |
| P8  | 3/3, 0.028 | 2/2, 0.039 |
| P9  | 11/11, 0.054 | 5/6, 0.064 |
| P10 | 0/0, 0.002 | 0/0, 0.006 |
| P11 | 0/0, 0.001 | 0/0, 0.005 |
| P12 | 50/50, 0.275 | 25/25, 0.363 |
| P13 | 100/100, 0.617 | 50/50, 0.931 |
| P14 | 52/52, 0.711 | 50/50, 1.238 |
| P15 | 0/0, 0.516 | 25/25, 2.951 |
| P16 | 0/0, 1.511 | 50/50, 7.444 |
| P17 | 0/0, 1.291 | 50/50, 8.286 |
| P18 | 2/2, 1.642 | 51/51, 11.544 |
| P19 | 0/0, 2.008 | 50/50, 15.096 |

## 0.3 Full rule set baseline（2026-01-04，det-opt + naive-split）
设置：
- 规则集 `full`，base dir 为 `experiments/side_channel_full_eval`。
- 编译 `--full-only`，运行 `--det-opt`，rewrite 时额外带 `--rewrite`。
- split 模式为默认 `naive-split`（rewrite only）。
- `rand_vars` = prune 后用于 FC 的概率事实数 + 概率边数（rewrite 后再统计）。

| case | variant | rand_vars | total_s | seminaive_s | create_s | prune_s | rewrite_s | fc_s | wmc_s | manager_init_ms | reorder_s | dd_live_nodes |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| P1 | norewrite | 2 | 0.022 | 0.013 | 0.000 | 0.000 | 0.000 | 0.008 | 0.000 | 7 | 0.000 | 3 |
| P1 | rewrite | 2 | 0.021 | 0.015 | 0.000 | 0.000 | 0.000 | 0.004 | 0.000 | 3 | 0.000 | 3 |
| P3 | norewrite | 2 | 0.018 | 0.012 | 0.000 | 0.000 | 0.000 | 0.005 | 0.000 | 4 | 0.000 | 3 |
| P3 | rewrite | 2 | 0.020 | 0.013 | 0.000 | 0.000 | 0.000 | 0.005 | 0.000 | 4 | 0.000 | 3 |
| P4 | norewrite | 1 | 0.008 | 0.002 | 0.000 | 0.000 | 0.000 | 0.005 | 0.000 | 4 | 0.000 | 2 |
| P4 | rewrite | 1 | 0.006 | 0.001 | 0.000 | 0.000 | 0.000 | 0.004 | 0.000 | 3 | 0.000 | 2 |
| P5 | norewrite | 12 | 0.006 | 0.002 | 0.000 | 0.000 | 0.000 | 0.004 | 0.000 | 3 | 0.000 | 33 |
| P5 | rewrite | 0 | 0.006 | 0.002 | 0.000 | 0.000 | 0.000 | 0.003 | 0.000 | 3 | 0.000 | 1 |
| P6 | norewrite | 39 | 0.009 | 0.003 | 0.000 | 0.000 | 0.000 | 0.004 | 0.000 | 3 | 0.000 | 117 |
| P6 | rewrite | 6 | 0.013 | 0.007 | 0.000 | 0.000 | 0.001 | 0.004 | 0.000 | 3 | 0.000 | 18 |
| P7 | norewrite | 56 | 0.012 | 0.004 | 0.001 | 0.001 | 0.000 | 0.004 | 0.000 | 3 | 0.000 | 194 |
| P7 | rewrite | 18 | 0.012 | 0.004 | 0.001 | 0.001 | 0.002 | 0.004 | 0.000 | 3 | 0.000 | 59 |
| P8 | norewrite | 53 | 0.017 | 0.008 | 0.001 | 0.001 | 0.000 | 0.005 | 0.000 | 3 | 0.000 | 187 |
| P8 | rewrite | 18 | 0.019 | 0.008 | 0.001 | 0.001 | 0.002 | 0.005 | 0.000 | 4 | 0.000 | 60 |
| P9 | norewrite | 113 | 0.016 | 0.006 | 0.001 | 0.001 | 0.000 | 0.005 | 0.000 | 3 | 0.000 | 432 |
| P9 | rewrite | 31 | 0.017 | 0.006 | 0.001 | 0.001 | 0.003 | 0.005 | 0.000 | 4 | 0.000 | 90 |
| P10 | norewrite | 32 | 0.047 | 0.024 | 0.013 | 0.001 | 0.000 | 0.005 | 0.000 | 4 | 0.000 | 49 |
| P10 | rewrite | 0 | 0.045 | 0.023 | 0.012 | 0.002 | 0.001 | 0.004 | 0.000 | 3 | 0.000 | 1 |
| P11 | norewrite | 16 | 0.043 | 0.021 | 0.013 | 0.001 | 0.000 | 0.004 | 0.000 | 3 | 0.000 | 17 |
| P11 | rewrite | 16 | 0.043 | 0.021 | 0.012 | 0.001 | 0.000 | 0.004 | 0.000 | 3 | 0.000 | 17 |
| P12 | norewrite | 466 | 0.132 | 0.063 | 0.012 | 0.010 | 0.000 | 0.035 | 0.000 | 25 | 0.000 | 1693 |
| P12 | rewrite | 0 | 0.111 | 0.060 | 0.012 | 0.010 | 0.020 | 0.003 | 0.000 | 3 | 0.000 | 1 |
| P13 | norewrite | 1143 | 0.494 | 0.109 | 0.026 | 0.026 | 0.000 | 0.305 | 0.001 | 65 | 0.210 | 4107 |
| P13 | rewrite | 0 | 0.234 | 0.114 | 0.028 | 0.026 | 0.052 | 0.003 | 0.000 | 2 | 0.000 | 1 |
| P14 | norewrite | 1643 | 0.928 | 0.166 | 0.042 | 0.046 | 0.000 | 0.629 | 0.003 | 52 | 0.530 | 7090 |
| P14 | rewrite | 449 | 0.366 | 0.162 | 0.043 | 0.044 | 0.084 | 0.017 | 0.000 | 9 | 0.000 | 1885 |
| P15 | norewrite | 3262 | 3.226 | 0.369 | 0.096 | 0.137 | 0.000 | 2.527 | 0.005 | 72 | 2.220 | 28140 |
| P15 | rewrite | 1225 | 1.448 | 0.378 | 0.104 | 0.133 | 0.142 | 0.653 | 0.001 | 60 | 0.560 | 5425 |
| P16 | norewrite | 5635 | 5.411 | 0.603 | 0.178 | 0.281 | 0.000 | 4.164 | 0.011 | 269 | 3.570 | 73245 |
| P16 | rewrite | 2210 | 2.698 | 0.588 | 0.186 | 0.276 | 0.275 | 1.312 | 0.002 | 80 | 1.130 | 12527 |
| P17 | norewrite | 7635 | 6.621 | 0.812 | 0.231 | 0.505 | 0.000 | 4.812 | 0.025 | 305 | 4.050 | 136607 |
| P17 | rewrite | 3010 | 4.801 | 0.788 | 0.283 | 0.501 | 0.431 | 2.710 | 0.003 | 86 | 2.400 | 22181 |
| P18 | norewrite | 9641 | 7.039 | 1.050 | 0.337 | 0.852 | 0.000 | 4.430 | 0.049 | 322 | 3.000 | 236563 |
| P18 | rewrite | 3806 | 5.354 | 1.035 | 0.348 | 0.855 | 0.786 | 2.210 | 0.005 | 85 | 1.850 | 39675 |
| P19 | norewrite | 12143 | 10.403 | 1.326 | 0.451 | 1.346 | 0.000 | 6.759 | 0.060 | 306 | 5.210 | 380873 |
| P19 | rewrite | 4810 | 8.666 | 1.313 | 0.460 | 1.286 | 1.147 | 4.313 | 0.008 | 322 | 3.780 | 60507 |
| P20 | norewrite | 17852 | 6.799 | 1.046 | 0.417 | 0.654 | 0.000 | 4.161 | 0.027 | 301 | 1.580 | 75871 |
| P20 | rewrite | 3632 | 4.412 | 0.999 | 0.406 | 0.867 | 1.547 | 0.446 | 0.002 | 50 | 0.180 | 9237 |

## 0. 背景与目标

你当前的 split 规则（对 input facts 节点：若某一条 outgoing 的后续推理与其它 outgoing 的后续推理在变量集合上**完全不相交**，则把该节点分裂出影子节点，概率保持一致）本质上是在做一种**结构化分解**：尽量把下游互不相关（在随机变量层面）的推理过程拆开，从而暴露更多 SISO 结构，利于后续 rewrite/编译/推理。

这类“结构/顺序对编译与推理性能高度敏感”的现象，在 KC/BDD/WMC 系统中非常典型：变量生成顺序、公式结构、编译顺序都会显著影响 BDD 紧凑性与性能，工程上往往必须提供结构控制与启发式优化。 以及 BDD/相关工具对“连接变量放近/结构分区”的强调也说明了结构优化的收益空间很大。

---

## 1. 问题陈述

### 1.1 现状

* 对某节点 `u`（目前多为 input fact）：

  * `u` 有多条 outgoing 边到 `v1..vk`
  * 若存在某个 `vi`，使得 `Vars(Support(vi))` 与 `Vars(Support(vj)) (j≠i)` 完全不相交
  * 则 split：复制 `u` 为影子 `u'`，把与 `vi` 对应的那条 outgoing 重定向到 `u'`，概率保持一致

### 1.2 当前瓶颈

* 你为了性能只在 outgoing 的 **50 个节点集合**里做变量交集判断；当 `outdegree(u) > 50` 就拒绝 split。
* 深图/大扇出节点会错失大量可 split 机会。

### 1.3 你提出的新方向

* 不仅 split input facts，也允许 split 中间节点：

  * split 时复制该节点的 incoming edges（input fact 的 incoming 是隐式的，形式差异不大）
* 担忧：如果对很多节点都尝试 split（尤其图近似线性或大图），重复计算会累积很贵；希望能“一次预处理/增量维护”解决。

---

## 2. 关键定义（把“local vs support”讲清楚）

### 2.1 图模型

* Derivation graph：有向图 `G=(V,E)`，边方向是推理方向：`inputs -> output node`。
* 每条边可能“承载随机变量”（RV）；记为 `rv(e)`，无则为 `⊥`。
* 你关心的 “共享变量” 指的是 **共享同一个 RV 身份**（同一随机选择/同一 probabilistic fact instance/同一 flip 实例等）。

### 2.2 支持集定义（建议采用）

你当前判定 “某 outgoing 子推理过程独立于其它” 更像 **support（向前可达的 RV 集）**，而不是只看局部几步（local）。

* `DownRV(n)`: 从节点 `n` 沿推理方向可达的所有随机变量集合（出边上的 rv + 后继的 DownRV）。
* `DownRV(e: u->v)`: 可以定义为 `rv(e) ∪ DownRV(v)`（若你不想把 “该 outgoing 边自身的 RV” 算进来，也可以只用 `DownRV(v)`；但工程上通常两者都可，取决于你 RV 挂载位置与语义）。

> split 判定（核心）
> 对 `u` 的 outgoing 边集合 `{e_i: u->v_i}`，若存在划分使得不同组的 `DownRV(e)` 互不相交，则可以把这些 outgoing 分配到不同 clone 上。

**实现注记（当前版本）**：complete-split 为保证语义安全，采用“下游结构交汇即合并”的保守判定（即只要两分支在图上相交，就认为相关），而不是显式计算 `DownRV` 交集。这会减少可拆分机会，但保证不破坏相关性。

---

## 3. 总体设计思路（回答你的核心诉求）

本设计给出三层优化，能同时解决：

1. `outdegree>50` 时不再“直接拒绝”；
2. 避免对线性/大图重复做昂贵集合计算；
3. 支持 split 中间节点，同时用代价模型控制“无意义 split”爆炸。

### 3.1 核心变化：把 “50 限制 + pairwise 交集” 换成 **线性时间分组算法**

现有常见实现会做 `O(k^2)` 的集合相交或抽样；我们改成 **worklist + union-find** 的线性分组：

* 思想：只要两个 outgoing 分支在下游某处**汇合到同一段**，就视为相关 ⇒ 属于同一组（complete-split 采用该严格语义）。
* 用一次多源遍历（multi-source traversal）在下游传播 “来自哪个 outgoing 分支”，发现冲突就 union。
* 复杂度近似 `O(|V_reachable| + |E_reachable|)`（每个节点最多经历 `NONE -> SINGLE -> MULTI` 两次状态变化）。

这能避免对 `k` 很大时的 `k*support_size` 或 `k^2` 相交；尤其当下游有大量共享子图时，优势更明显。

### 3.2 预处理/增量维护：`hasRVReach[n]` 用于候选过滤（而不是分组合并）

你担心“每个节点都重新算支持集很贵”。关键观察：

* 进行 **分组/拆分** 并不一定需要显式 `DownRV(n)` 全集合；
* 我们只需要一个布尔量：`hasRVReach[n]` = 从 `n` 往前是否能到达任何 RV。

* **当前实现采用更严格语义**：只要两个分支在下游任何节点发生交汇，就必须 union（不再用 `hasRVReach` 放过“确定性汇合”）。这样可保证 split 后的相关性不被破坏。
* `hasRVReach` 仍保留为**候选过滤**：若某个 fact 完全无法到达 RV，则不做 split（无收益）。

并且 `hasRVReach` 可以用 **计数器增量维护**（适配你的“rewrite 多为单调减少，split 会增加”）：

* 每个节点维护 `rvReachCount[n] = #outgoing edges that contribute RV reachability`
* `hasRVReach[n] = (rvReachCount[n] > 0)`

当 rewrite 删除/增加边、或后继 `hasRVReach` 翻转时，只需常数/线性于受影响入边的更新，无需全图重算。

### 3.3 支持 split 中间节点：用 **代价模型 + 预算** 控制爆炸

中间节点 split 的风险来自：

* 复制 incoming edges 造成结构膨胀
* 可能生成很多“形式上可 split 但无收益”的 clone

因此：

* 引入 `SplitCost(u)` 与 `SplitBenefit(u)` 估算；只做 “性价比足够高” 的 split
* 引入全局/每轮 rewrite 的 split budget：限制 clone 数、限制复制的 incoming edge 总数、限制新增节点数等

---

## 4. 数据结构设计（最小必要，Codex 可直接实现）

```pseudo
type NodeId = int
type EdgeId = int
type VarId  = int   // Random variable identity

struct Edge {
  NodeId src
  NodeId dst
  VarId  rv        // rv == -1 表示无随机变量
}

struct Node {
  list<EdgeId> outEdges
  list<EdgeId> inEdges
  bool isInputFact

  // --- incremental bookkeeping ---
  int rvReachCount          // >=0
  bool hasRVReach           // (rvReachCount>0)

  int outDegree             // cached (len(outEdges))
  int inDegree              // cached (len(inEdges))

  // optional: 用于避免重复入队
  int dirtyToken
}

struct Graph {
  array<Node> nodes
  array<Edge> edges
}
```

### 4.1 重要工程建议：把 “边承载 RV” 改为 “边引用 RV”

你提到 split 会“复制 incoming edges，分裂随机变量（边承载了随机变量）”。
如果你目前是 **Edge 自带 RV 实例**，复制边就等价“新建 RV”，确实会造成 RV 数量膨胀。

建议把 RV 做成独立对象/独立 ID，Edge 只持有 `VarId` 引用：

* 复制 edge 时默认复用相同 `VarId`（语义上更像“同一个随机选择被多个地方引用”）
* 如果你确实需要“把 RV 也分裂成独立副本”，再显式 `cloneVar(rv)` 生成新 `VarId`（可控、可选择）

这能把 split 的主要成本从 “变量数爆炸” 降到 “边数增加”，更利于后续 KC/BDD（变量数往往更敏感）。同类系统里变量顺序/结构对 BDD 有巨大影响，变量数量和相互耦合更关键。

---

## 5. 增量维护：`hasRVReach` 的初始化与更新

### 5.1 初始化（一次性）

```pseudo
function initHasRVReach(G):
  for n in G.nodes:
    n.rvReachCount = 0
    n.hasRVReach = false

  // 朴素版本：反复扫描直到不变（适用于有环）
  changed = true
  while changed:
    changed = false
    for each node u:
      newCount = 0
      for eId in u.outEdges:
        e = G.edges[eId]
        if e.rv != -1:
          newCount += 1
        else if G.nodes[e.dst].hasRVReach:
          newCount += 1
      if newCount != u.rvReachCount:
        u.rvReachCount = newCount
        u.hasRVReach = (newCount > 0)
        changed = true
```

> 若图是 DAG，可按逆拓扑一次性算完；但你没明确 DAG/是否有环，上面 fixpoint 版本更稳妥。

### 5.2 增量更新（推荐，用 worklist）

维护贡献函数：

```pseudo
function edgeContributesRV(G, eId):
  e = G.edges[eId]
  if e.rv != -1: return true
  return G.nodes[e.dst].hasRVReach
```

当发生局部修改（加边/删边/节点替换）时，更新 `rvReachCount` 并向前驱传播：

```pseudo
function recomputeNodeRVReachCount(G, u):
  cnt = 0
  for eId in u.outEdges:
    if edgeContributesRV(G, eId): cnt += 1
  old = u.rvReachCount
  u.rvReachCount = cnt
  u.hasRVReach = (cnt > 0)
  return (old != cnt)

function propagateHasRVReachChange(G, startNodes):
  queue = startNodes
  while queue not empty:
    u = pop(queue)
    oldHas = u.hasRVReach
    changed = recomputeNodeRVReachCount(G, u)
    if changed:
      // u.hasRVReach 可能翻转，影响 u 的所有前驱
      for inEId in u.inEdges:
        p = G.nodes[ G.edges[inEId].src ]
        push(queue, p)
```

这套机制在你的场景里很匹配：

* 大多数 rewrite 单调减少：删边导致 `hasRVReach` 可能从 true 变 false，沿前驱传播
* split 增加：加边/重定向导致 true 传播，仍然可用同一机制

---

## 6. Split 机会发现：用多源遍历把 outgoing 分组（替代 “最多 50 个 outgoing”）

### 6.1 目标

对节点 `u` 的 outgoing edges `E_out(u) = [e0..e(k-1)]`：

* 我们要得到一个分组 `groups: list<list<EdgeId>>`
* 满足：不同 group 的下游 RV support 不相交（近似等价：它们在“能到达 RV 的下游区域”没有交汇/共享）

### 6.2 关键算法：Multi-source ownership propagation + Union-Find

#### 状态定义（对 **下游节点**）

对每个节点 `x`（只在 `u` 的 forward cone 内使用临时状态）维护：

* `owner[x]` ∈ { -1 (NONE), -2 (MULTI), 0..k-1 (某个 outgoing index) }
* `rep[x]`：当 `owner[x]==MULTI` 时用于 union 的代表（保存第一次到达它的 origin）

状态变化单调：`NONE -> SINGLE(i) -> MULTI` 或 `NONE -> MULTI`
因此每个节点最多入队 2 次。

#### 伪代码

```pseudo
const NONE = -1
const MULTI = -2

struct UF { parent[], size[] } // 标准并查集

function partitionOutgoingByRVOverlap(G, u: NodeId) -> list<list<EdgeId>>:
  out = G.nodes[u].outEdges
  k = len(out)
  if k <= 1: return [out]  // 无需 split

  // 如果 u 下游根本没有 RV，就算分组也通常没收益（可直接返回）
  if not G.nodes[u].hasRVReach:
    return [out]

  // --- 临时数组：用 token 技术避免 O(|V|) 清零 ---
  token += 1
  // arrays: seenToken[x], owner[x], rep[x]
  // 只有 seenToken[x]==token 才认为该节点在本次调用中初始化过

  uf = UF.init(k)

  queue = emptyQueue()

  // helper: 赋值/合并 owner
  function touchNode(x):
    if seenToken[x] != token:
      seenToken[x] = token
      owner[x] = NONE
      rep[x] = NONE

  function assign(x: NodeId, incomingOwner: int, incomingRep: int):
    touchNode(x)

    if owner[x] == NONE:
      owner[x] = incomingOwner
      rep[x] = (incomingOwner == MULTI) ? incomingRep : incomingOwner
      push(queue, x)
      return

    if owner[x] == incomingOwner:
      return

    // owner[x] != incomingOwner 发生冲突，合并为 MULTI
    oldOwner = owner[x]
    oldRep   = rep[x]

    if owner[x] != MULTI:
      owner[x] = MULTI
      // rep[x] 保持 oldRep（第一个到达者代表）
      rep[x] = oldRep
      push(queue, x)

    // 任何交汇都表示相关，直接 union
    incRep = (incomingOwner == MULTI) ? incomingRep : incomingOwner
    uf.union(oldRep, incRep)

  // 初始化：从每条 outgoing 的 dst 作为源点开始传播
  for i in 0..k-1:
    e = G.edges[out[i]]
    v = e.dst
    assign(v, i, i)

  // BFS/Worklist 传播 ownership
  while queue not empty:
    x = pop(queue)
    ox = owner[x]
    rx = rep[x]   // MULTI 的代表

    for eId in G.nodes[x].outEdges:
      y = G.edges[eId].dst
      if ox == MULTI:
        assign(y, MULTI, rx)
      else:
        assign(y, ox, ox)

  // 形成 groups：按 uf.find(i) 聚合
  map<int, list<EdgeId>> buckets
  for i in 0..k-1:
    root = uf.find(i)
    buckets[root].append(out[i])

  groups = buckets.values()

  return groups
```

#### 解释与性质

* 当两个 outgoing 分支在下游某节点 `x` 汇合时，一律视为相关并 union（保证 split 语义不改变相关性）。
* 不需要枚举 `outgoing` 的 50 个子集；也不需要做 `k^2` 相交；扇出再大，也只是在 reachable cone 内传播一次。

> 这类“结构分组/把相关变量放近”的工程思想，在 BDD/SAT 里非常经典：结构与变量关联性是性能关键，且应避免为了优化本身消耗过多资源。

---

## 7. Split 决策：什么时候 split、split 成几份（避免无意义 split）

### 7.1 最小收益门槛

只有当 `groups.size >= 2` 才考虑 split。

此外建议增加若干启发式（可配置）：

* `MIN_GROUP_OUT_EDGES`：忽略太小 group（例如只有 1 条边且下游无 RV）
* `MAX_CLONES_PER_NODE`：每个节点一次最多拆成 N 份（例如 2 或 3），避免一次爆炸
* `MAX_IN_DEGREE_TO_SPLIT`：入度过大时默认不 split，除非收益显著

### 7.2 代价模型（建议）

```pseudo
cost(u, tGroups):
  // clone tGroups-1 个节点，每个 clone 复制 indeg(u) 条 incoming edges
  return (tGroups - 1) * indeg(u)

benefit(u, groups):
  // 最简单可用：groups.size - 1
  // 更精细：统计被拆开的 outgoing 的 downstream RV 数量、或后续 rewrite 的命中次数等
  return groups.size - 1

doSplit if benefit/cost >= THRESHOLD
```

`THRESHOLD` 可先设为很小的值（例如 0.01），再根据实验调参。

---

## 8. Split 变换：支持 input facts 与中间节点

### 8.1 语义与实现模式（必须明确）

你说“保持概率一致”，又说 split 会“分裂随机变量”。为了让工程可控，建议把 split 的 incoming edge 处理做成可选策略：

* `IncomingCloneMode = ALIAS_RV`（默认建议）：复制 incoming edges，但 **复用同一 VarId**（边引用 RV）

  * 语义更接近“同一随机事件被多个地方引用”，通常更符合 exact inference 的一致性
  * 也能显著抑制 RV 数量膨胀

* `IncomingCloneMode = FRESH_RV`（谨慎启用）：复制 incoming edges 并为每条复制边分配新的 VarId（概率相同）

  * 只有在你能严格证明这不会改变语义，或你本来就在做某种近似/等价变换时使用

### 8.2 ApplySplit 伪代码

```pseudo
enum IncomingCloneMode { ALIAS_RV, FRESH_RV }

function applySplit(G, u: NodeId, groups: list<list<EdgeId>>, mode: IncomingCloneMode):
  // groups[0] 留在原节点 u，其余 groups[i] 分配到 clone 节点 u_i
  if len(groups) <= 1: return

  originalIn = copy(G.nodes[u].inEdges)

  for gi in 1..len(groups)-1:
    u2 = newNodeLike(G, u)     // 复制节点属性：isInputFact 等
    // 复制 incoming edges
    for inEId in originalIn:
      e = G.edges[inEId]
      newRv = e.rv
      if mode == FRESH_RV and e.rv != -1:
        newRv = cloneVarId(e.rv)   // 新 VarId，概率参数同原 rv
      newInE = addEdge(G, e.src, u2, newRv)
      // 更新 adjacency: inEdges/outEdges, indegree/outdegree, 以及 hasRVReach 计数维护
      onEdgeAdded(G, newInE)

    // 将该 group 的 outgoing edges 从 u 挪到 u2
    for outEId in groups[gi]:
      // outEId 的 src 从 u 改为 u2
      rerouteEdgeSource(G, outEId, newSrc=u2)
      // adjacency 与 hasRVReach 计数需要维护
      onEdgeRerouted(G, outEId, oldSrc=u, newSrc=u2)

  // 最后：更新 u 自己的 outEdges（只保留 groups[0]）
  // 具体由 rerouteEdgeSource 实现时同步维护
```

> input fact 的 “incoming edge 隐式” 处理：
> 如果你把 input fact 的概率来源也建模为一个显式 RV（或一个 baseFactId），则 clone 节点共享该 base id（ALIAS）即可。

---

## 9. Rewrite 主循环：只在“候选节点”上做 split 检测，避免线性图重复计算

### 9.1 候选队列策略

你担心“遍历每个节点都尝试 split 会很贵”。建议改为事件驱动：

候选节点条件（默认）：

* `outDegree(u) >= 2`
* `hasRVReach[u] == true`（下游无 RV 时 split 无收益）

触发入队事件：

* rewrite 删除/新增/重定向了一条边 `e`：把 `src(e)` 入队
* `hasRVReach` 在某节点翻转：把该节点及其前驱入队（或至少该节点入队）

### 9.2 主循环伪代码

```pseudo
function optimizeSplits(G):
  initCandidateQueue(Q)
  for u in nodes:
    if G.nodes[u].outDegree >= 2 and G.nodes[u].hasRVReach:
      push(Q, u)

  splitBudgetEdges = MAX_NEW_IN_EDGES_PER_PASS
  splitBudgetNodes = MAX_NEW_NODES_PER_PASS

  while Q not empty:
    u = pop(Q)
    if G.nodes[u].outDegree < 2: continue
    if not G.nodes[u].hasRVReach: continue

    groups = partitionOutgoingByRVOverlap(G, u)

    if len(groups) <= 1: continue

    // 可选：限制一次拆分份数
    if len(groups) > MAX_CLONES_PER_NODE:
      groups = pickTopGroups(groups, MAX_CLONES_PER_NODE) // 例如保留最大/最有RV的组

    c = (len(groups)-1) * G.nodes[u].inDegree
    b = len(groups) - 1

    if not worthSplitting(u, c, b): continue
    if splitBudgetEdges < c or splitBudgetNodes < (len(groups)-1): continue

    applySplit(G, u, groups, mode=ALIAS_RV)

    splitBudgetEdges -= c
    splitBudgetNodes -= (len(groups)-1)

    // split 改变了局部结构：u、clones、以及它们的前驱/后继都可能成为新候选
    enqueueNeighborhood(Q, u)
```

---

## 10. 复杂度与性能预期

### 10.1 相比现有 50 限制的收益

* 你现状对 `outdegree>50` 直接拒绝，收益为 0；
* 本方案对大扇出节点：

  * 分组算法复杂度 ~ `O(|reachable cone|)`（与扇出 k 不再呈二次关系）
  * 能在深图中显著捕获 split 机会

### 10.2 线性图的担忧如何消解

* 线性图里几乎所有节点 `outDegree==1`，根本不会入候选队列；
* 所以不会出现“遍历每个节点重复做昂贵 split 检测”的累积成本。

---

## 11. 测试与正确性检查（建议你实现时必须带上）

### 11.1 结构不变量

* 所有边的 `src/dst` adjacency 一致（outEdges 与 inEdges 对称）
* `inDegree/outDegree` 缓存一致
* `hasRVReach` 与 `rvReachCount` 一致（可 debug 模式下抽查重算）

### 11.2 split 语义一致性（你强调“概率一致”）

建议写两类测试：

1. **图结构等价测试（不涉及概率）**
   split 前后，针对一组 query 节点，比较它们在图中的可达性/推理结构是否符合预期（比如 SISO 结构数量增加等）。

2. **概率一致性回归测试（强烈建议）**
   对小规模随机图/真实样例：

* 用你现有 exact 推理（或枚举）比较 split 前后 query 的概率是否一致
* 若 ALIAS_RV 模式一致而 FRESH_RV 不一致，则说明你之前“复制边即复制 RV”的语义会改变结果，需要谨慎

---

## 12. 可选增强：把“分组”当作一般 rewrite 的基础设施

一旦你有了 `partitionOutgoingByRVOverlap(u)`：

* 你不仅能做 split
* 还可以做更高层的 rewrite 规划：选择在更低 in-degree 的节点 split（更便宜），或选择拆分后再做其它单调减少 rewrite，使结构更接近“相关部分一起编译/相近变量聚集”的经验法则

---

## 13. 你可以直接采用的默认参数（起步值）

* `MAX_CLONES_PER_NODE = 2`（先只做二分，最稳）
* `MAX_NEW_IN_EDGES_PER_PASS = 50_000`（按你的图规模调）
* `MAX_NEW_NODES_PER_PASS = 5_000`
* `worthSplitting`: `benefit >= 1 AND cost <= 1000`（先用硬阈值，后面再换 benefit/cost）

---

## 14. 小结：本设计回答你的两个核心问题

1. **不再因为 outdegree>50 而拒绝 split**：
   用多源传播 + union-find，一次遍历下游 cone 即可分组，避免 `k^2`/抽样。

2. **信息能否一次预处理？rewrite/split 会不会破坏？**

* 我们只预处理/增量维护一个很便宜的 `hasRVReach`（计数器维护），用于过滤候选节点、避免在无 RV 下游的分支上浪费 split 开销；
* split 机会发现只在候选节点上触发，不会在线性图上累积；
* rewrite/split 引起的结构变更通过 worklist 局部传播更新，不需要全图重算。

如果你愿意，我也可以把上面的伪代码进一步“落成接口级设计”（例如 `Graph` 的 API、`rerouteEdgeSource` 需要维护哪些索引、token 数组如何组织以避免频繁分配），但即使按当前文本，Codex 已经可以直接实现一个可工作的版本。
