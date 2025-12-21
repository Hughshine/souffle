下面给出一个“Codex 级别”的实现计划：按模块拆分、逐个落地、每个模块配套 unit tests，最后再给出一个统合模块（可直接替换 buildFormulasIncCyclewise 的 insertion 部分，deletion 复用旧逻辑）。

为对齐你给的 pdf（inc_problog_data），整体算法目标是：

只对 region 内节点重建 BDD/DD，并在 Θorig 下算出 region（含 boundary）节点新概率；

inc_problog_data (4)

对每个 boundary 节点 v，选一个“可 merge 的旧入边”作为 anchor，通过一次 symbolic-WMC 解出 gate 的新概率 
𝑝
\*
p
\*
，从而外部节点可以继续复用旧 BDD；

inc_problog_data (4)

外部节点不重建 DD，只靠更新后的参数（包含这些 calibrated gates）重新 WMC（按必要性）。

inc_problog_data (4)

同时你强调两点约束我会在设计里硬编码：

selector 必须直接复用 analyzer 的“合并性判定结果”，避免重复计算；

删除逻辑直接 copy/paste 现有 deletion 阶段（同一次 update 仍然先删后插），regional 只替换 insertion。

总体落地策略

我们把“regional insertion”拆成 4 个可独立测试的核心模块 + 1 个统合 orchestrator：

AnalyzerResultCache（增强 IncRegionAnalyzer）
输出 region、boundary 分类、以及 每个 boundary 节点的 mergeable old incoming edges 候选列表（selector 只读它，不再自行判定）。

RegionalInsertPlan（计划/选择层）
基于 analyzer 的缓存结果构建一个 insertion plan（region nodes、boundary nodes、每个 boundary 的候选 anchor 列表），并做必要的健壮性检查与 fallback 决策。

RegionalDDRebuilder（只重建 region 内的 node/edge formulas）
替换 buildFormulasIncCyclewise 的 insertion rebuild 逻辑：只更新 region 内节点（以及 head 在 region 的 edges），外部公式不动。

BoundaryGateCalibrator（symbolic WMC 校准）
对每个 boundary 节点 v：

用新 BDD+ 算 Pr_new[v]；

inc_problog_data (4)

用旧 BDDold(v) 计算 α、β，并解 
𝑝
\*
p
\*
；

inc_problog_data (4)

写回公式管理器的变量权重（setVariableWeight）。

RegionalIncrementalForwardCompilation（统合模块）
对外提供一个入口函数/类：

先跑 deletion（复用旧代码）

再跑 insertion：Analyzer → Plan → Rebuild → Calibrate

若 plan 或 calibrate 失败，fallback 到旧 insertion（buildFormulasIncCyclewise 的原 insertion 部分）

每个模块都配套 unit tests。下面我按“实现顺序”给出详细计划与测试点。

模块 0：测试基础设施（先做，后面模块都复用）
0.1 新增一个可枚举 WMC 的 ToyFormulaManager（仅测试用）

原因：你真实系统里 CUDD/SDD manager 很重，单测容易变成集成测试且难控。我们要“模块级别可重复、可小规模枚举验证”的测试环境。

目标接口：覆盖 ForwardCompilation 里用到的最小集合：

makeTrue/makeFalse/makeVar/makeAnd/makeOr/makeNot

setVariableWeight(var, p, 1-p)

computeWeightedModelCount(formula)（用枚举赋值做 exact WMC）

isSame(a,b)（结构同一性即可；更强可选“逻辑等价”通过枚举验证）

文件建议

tests/toy/ToyFormulaManager.h

tests/toy/ToyFormulaManager.cpp（可 header-only）

单测

ToyFormulaManager_WMC_Sanity：构造小公式 (x ∨ y) ∧ ¬z，给定权重，枚举验证 WMC。

ToyFormulaManager_Weights_Update：多次 setVariableWeight 后 WMC 变化正确。

这一层完成后，后续模块的测试都可以在“完全不依赖 BDD 库”的情况下精确对比概率。

模块 1：增强 IncRegionAnalyzer，输出可复用的 mergeability 判定结果（避免 selector 重算）

你现有 IncRegionAnalyzer 已经能做 region expansion & boundary 分类；pdf 对 boundary 定义与分类也很清晰：boundary 是 region 内但有出边指向 region 外的 tail 节点 

inc_problog_data (4)

，并分 out-induced / scope-induced / residual。

inc_problog_data (4)

你还实现了 mergeableEdgeAtHead_（用于 mergeableHead_），其核心判定目前是：

必须是 old edge（不在 delta_insert_edges_cache_）

respects scopes

non-subsumed（在 head 处不被其它 old 输入体覆盖）

Holtzen 等 - 2020 - Scaling exac…

pdf 里还有一个额外条件 (C3-2)：anchor old edge 必须“持有 rule variable”（可调 gate），否则无法做 
𝑝
\*
p
\*
 校准。

inc_problog_data (4)


建议在 analyzer 的缓存候选里就加入这个过滤，避免 calibrator/selector 再做二次过滤。

1.1 新增 Analyzer 输出结构体

在 IncRegionAnalyzer.h 中新增：

struct IncRegionAnalysis {
  Region region;
  Boundaries boundaries;

  // 关键：对 boundary node v，缓存其所有“mergeable old incoming edges”候选
  // selector 只读它，不再调用 mergeableEdgeAtHead_/edgeRespectsScopes_/edgeNonSubsumed_。
  std::unordered_map<NodePtr, std::vector<EdgePtr>> mergeableAnchorsByHead;
};


并提供 getter：

const IncRegionAnalysis& getLastAnalysis() const;

const std::vector<EdgePtr>& getMergeableAnchors(NodePtr v) const;（不存在返回空 vector）

1.2 在 analyze() 完成后一次性填充 mergeableAnchorsByHead

实现思路：

boundary_nodes = boundaries.all

对每个 boundary 节点 v：

遍历 view_.getIncomingEdges(v)

若 mergeableEdgeAtHead_(e, v) 为真，且 e->isProbabilistic()（或 “holds rule variable”）为真，则 push 到候选列表

存入 last_analysis_.mergeableAnchorsByHead[v]

注意：这一步就是“合并性判定结果的唯一计算点”。之后 selector/calibrator 都不再碰 mergeable 判定逻辑。

1.3 单测（IncRegionAnalyzer_CachedMergeableAnchors）

用你 header 里已有的 ExampleIncView 或 IncSubgraphView 构造一个小图：

设置 delta insert edges / delta out nodes

跑 analyzer

断言：

analysis.boundaries.all 非空时，每个 boundary v 的 getMergeableAnchors(v) 非空（如果 region expansion 已保证 merge-ready）

候选边不在 delta_insert_edges

候选边 isProbabilistic == true

重复调用 getMergeableAnchors 不触发任何 recompute（通过设计保证：getter 只返回缓存容器）

模块 2：RegionalInsertPlan（只做“计划”，不做重建/校准）
2.1 计划结构体

新建 RegionalInsertPlan.h：

struct RegionalInsertPlan {
  std::unordered_set<NodePtr> regionNodes;
  std::unordered_set<NodePtr> boundaryNodes;

  // 从 analyzer 直接拷贝过来：selector 不做 mergeability 重算
  std::unordered_map<NodePtr, std::vector<EdgePtr>> anchorCandidates;

  bool mergeReady;     // boundary 全部有候选
  bool regionClosed;   // boundary 为空
};

2.2 PlanBuilder

RegionalInsertPlanBuilder::build(view, analysis)：

regionNodes = analysis.region.nodes

boundaryNodes = analysis.boundaries.all

anchorCandidates = analysis.mergeableAnchorsByHead（直接用缓存）

regionClosed = boundaryNodes.empty()

mergeReady = regionClosed || ∀v∈boundaryNodes: !anchorCandidates[v].empty()

若 mergeReady == false：

返回 plan，但标记需要 fallback（不在这里扩 region；扩 region 属于 analyzer）

2.3 单测（RegionalInsertPlanBuilder_Basic）

构造 analyzer 输出（或直接跑 analyzer）

验证 plan 的 region/boundary 与 analyzer 一致

验证 mergeReady 标志正确（候选缺失时为 false）

模块 3：RegionalDDRebuilder（替换 insertion 的 rebuild 部分，只更新 region 内公式）

这一块是你要求“源码级别设计”的核心：我们要替换 buildFormulasIncCyclewise insertion 阶段中“worklist 传播 + SCC cyclewise rebuild”的部分，但 scope 限制在 regionNodes。

你现有 buildFormulasIncCyclewise insertion 起手会：

对 delta insert fact 节点生成 var 并设置 weight；

ForwardCompilation

对 delta insert edges 生成 base var（或 true）并设置 weight；

ForwardCompilation


这段可以保留，RegionalDDRebuilder 直接复用。

3.1 Rebuilder 的 I/O

新建 RegionalDDRebuilder.h：

template <typename FormulaManagerT, typename FormulaNodeRef>
class RegionalDDRebuilder {
public:
  struct Snapshot {
    // boundary 节点的旧公式：给 calibrator 用
    std::unordered_map<NodePtr, FormulaNodeRef> oldBoundaryNodeFormulas;
  };

  struct Result {
    Snapshot snapshot;
    std::set<NodePtr> changedNodes;           // 输出给后续 WMC/外部流程
    std::unordered_set<EdgePtr> rebuiltEdges; // 可选：用于调试/统计
  };

  static Result rebuildInsertRegion(
      IncrementalDerivationGraphViewInterface& view,
      FormulaManagerT& fm,
      std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
      std::map<EdgePtr, FormulaNodeRef>& edgeFormulas,
      const RegionalInsertPlan& plan);
};

3.2 必须保存的 snapshot

在任何对 nodeFormulas[v] 写新公式之前，对所有 v ∈ plan.boundaryNodes：

snapshot.oldBoundaryNodeFormulas[v] = nodeFormulas[v]（若不存在则记录为 fm.makeFalse() 或不记录并在 calibrator 失败时 fallback）

这样 calibrator 能拿到 BDDold(v)，外部节点旧公式也仍然引用旧子图（因为旧 edge formulas 持有旧引用，不会因 map 覆盖而变化）。

3.3 region 内 rebuild 的“边界”

我们只重建：

所有 head 在 regionNodes 的 edge formulas

所有 regionNodes 的 node formulas

对于 head 不在 regionNodes 的边（即 boundary outgoing edges / 外部边），我们不动（外部 BDD 复用）。

3.4 rebuild 算法（复用 buildFormulasIncCyclewise 的结构，但加 region filter）

建议实现为“按 SCC 分层 + 每 SCC 内 worklist”的方式，理由：

你的系统已有 cyclewise 经验（CycleDependencyGraph、buildFormulasCyclewise）。

region 很可能仍有递归（SCC），不能简单拓扑。

具体步骤

先做 insertion 初始化（复用旧逻辑）：

inserted facts：nodeFormulas[n] = fm.makeVar(var(n))，setVariableWeight(var(n), p, 1-p) 

ForwardCompilation

inserted edges：edgeFormulas[e] = fm.makeVar(var(e)) 或 true，setVariableWeight(var(e), p, 1-p) 

ForwardCompilation

构建 depGraph（可直接复用现有 CycleDependencyGraph 对整个 view 做 SCC），然后只取：

regionCycles = { cycleId | cycle.nodes ∩ regionNodes != ∅ }

形成 “regionCycles 的拓扑处理队列”：

对 regionCycles 的依赖边只保留指向 regionCycles 的部分（外部 cycles 认为已稳定，不计入 indegree）

Kahn queue：indegree==0 的 regionCycle 先处理

SCC 内部 worklist：

初始 worklist 只放 满足 shouldRebuildEdge(e) 的边，且 head(e) ∈ regionNodes
shouldRebuildEdge(e) 推荐：

e ∈ deltaInsertEdges → true

否则，存在 input u ∈ regionNodes → true

否则 false（直接沿用旧 edgeFormula）

弹出 edge e：

计算新的 edge formula：newE = baseVar(e) ∧ ∧inputs lit(u)（含 negations）

若 newE 与 edgeFormulas[e] 不同：写回，并把 head = output(e) 加入 dirtyHeads 集合

对 dirty head 节点：重算 node formula（OR 所有 incoming edge formulas），若变化：

写回 nodeFormulas[head]

changedNodes.insert(head)

将该 head 的 outgoing edges 中，满足 head(outEdge) ∈ regionNodes && shouldRebuildEdge(outEdge) 的 outEdge 入队

完成 SCC 后继续下一个 SCC。

这个过程本质上是把 buildFormulasIncCyclewise 的 insertion 传播限制在 regionNodes。

3.5 单测（RegionalDDRebuilder_EquivalenceOnRegion）

用 ToyFormulaManager：

构造一个小 derivation graph（无 evidence），先 full compile 得到 “旧公式”

添加 delta insert edges / nodes，构造 view（valid edges 包含 inserted）

跑 analyzer → plan → rebuilder

对 regionNodes 中每个节点 u：

用 full rebuild（buildFormulasCyclewise 在更新后图上全量重建）得到 baseline formula

比较 WMC(u) 是否一致（不要只比结构，结构可能不同但 WMC 应一致）

断言 changedNodes ⊆ regionNodes

再加一个包含 SCC 的测试（2 节点互相递归 + 新插入边），确保 worklist 不死循环、结果概率与 baseline 一致。

模块 4：BoundaryGateCalibrator（按 pdf 做 symbolic WMC 校准）

pdf 给的公式非常明确：对 boundary v，选 mergeable old incoming edge e_old，算：

α = WMC(BDDold(v) | X_eold := 0 ; Θorig)

β = WMC(BDDold(v) | X_eold := 1 ; Θorig)

𝑓
𝑣
(
𝑝
)
=
𝛼
(
1
−
𝑝
)
+
𝛽
𝑝
f
v
	​

(p)=α(1−p)+βp，解

𝑝
\*
=
(
𝑃
𝑟
𝑛
𝑒
𝑤
[
𝑣
]
−
𝛼
)
/
(
𝛽
−
𝛼
)
p
\*
=(Prnew[v]−α)/(β−α)，并 clip 到 [0,1]；若 β=α 则该边不可用。

inc_problog_data (4)

4.1 Calibrator 的 I/O

新建 BoundaryGateCalibrator.h：

template <typename FormulaManagerT, typename FormulaNodeRef>
class BoundaryGateCalibrator {
public:
  struct CalibrationRecord {
    NodePtr v;
    EdgePtr anchor;
    double alpha;
    double beta;
    double pStar;
  };

  struct Result {
    std::vector<CalibrationRecord> applied;
    std::unordered_set<NodePtr> failedNodes; // 没找到可用 anchor 的 boundary
  };

  static Result calibrate(
      IncrementalDerivationGraphViewInterface& view,
      FormulaManagerT& fm,
      const RegionalInsertPlan& plan,
      const typename RegionalDDRebuilder<FormulaManagerT,FormulaNodeRef>::Snapshot& snap,
      const std::map<NodePtr, FormulaNodeRef>& nodeFormulas);
};

4.2 关键实现细节（避免破坏权重）

实现时必须小心“临时改权重再恢复”，否则尝试候选 anchor 时会污染全局 Θ。

因此我建议对 FormulaManager 增加一个“读当前权重”的能力：

std::pair<double,double> getVariableWeight(int varIdx)
如果已有类似接口就直接用；如果没有，这个改动非常值得做，因为增量更新多轮会反复覆盖/恢复。

然后 calibrator 逻辑是：

对每个 boundary v：

target = WMC(nodeFormulas.at(v))（这是 BDD+(v) 的 WMC，对应 Prnew[v]）

inc_problog_data (4)

遍历 plan.anchorCandidates[v]（直接来自 analyzer 缓存；不重算 mergeability）：

var = fm.getVarIndex(*e_old)

oldW = fm.getVariableWeight(var)

setWeight(var, 0,1) → α = WMC(snap.oldBoundaryNodeFormulas[v])

setWeight(var, 1,0) → β = WMC(snap.oldBoundaryNodeFormulas[v])

setWeight(var, oldW) → 恢复

若 |β-α| < eps：continue（该 anchor 不可用）

inc_problog_data (4)

pStar = (target-α)/(β-α)，clip[0,1]，写回 setWeight(var, pStar, 1-pStar) 

inc_problog_data (4)

记录 CalibrationRecord，break

若所有候选失败 → failedNodes.insert(v)

4.3 单测（BoundaryGateCalibrator_SolvesPStar）

用 ToyFormulaManager 做一个最小可验证例子：

旧时 boundary 节点 v：v = (X_old ∧ a)（a 是某个外部 stable 事件）

插入后 v 新增一条 derivation：v_new = v ∨ (X_new ∧ b)

对比：

先 full rebuild 得到 Pr_new[v] baseline

再用 calibrator 算 pStar，写回 X_old 的权重

用旧公式 BDDold(v) 在新权重下算 WMC，应等于 baseline Pr_new[v]

再加一个“候选 1 无效、候选 2 有效”的测试：让候选 1 对 v 的旧公式不敏感（导致 α=β），验证 calibrator 会跳过并选候选 2。

模块 5：统合模块 RegionalIncrementalForwardCompilation（替换 insertion，deletion 复用旧逻辑）

这是你要的“最终统合模块”：对外一个入口，内部按阶段调用，且对失败可 fallback。

5.1 对外 API 设计

建议在 ForwardCompilation.h 附近新增一个类（你说“设计为类”，更好管理状态/统计/调试输出）：

template <typename FormulaManagerT, typename FormulaNodeRef>
class RegionalIncrementalForwardCompilation {
public:
  struct Options {
    bool enableFallbackToClassicInsertion = true;
    double eps = 1e-12;
  };

  struct Stats {
    size_t regionNodeCount = 0;
    size_t boundaryNodeCount = 0;
    size_t calibratedCount = 0;
    bool usedFallback = false;
  };

  RegionalIncrementalForwardCompilation(Options opt = {});

  void applyUpdate(
      IncrementalDerivationGraphViewInterface& view,
      FormulaManagerT& fm,
      std::map<NodePtr, FormulaNodeRef>& nodeFormulas,
      std::map<EdgePtr, FormulaNodeRef>& edgeFormulas,
      std::set<NodePtr>& changedNodes);

  const Stats& getStats() const;

private:
  Options opt_;
  Stats stats_;

  // 复用旧 deletion（直接 copy/paste 旧代码进来，或抽成 helper）
  void applyDeletionsClassic_(...);

  // 新的 regional insertion
  void applyInsertionsRegional_(...);

  // classic insertion（用于 fallback）
  void applyInsertionsClassic_(...);
};

5.2 applyUpdate 的执行流程（严格“先删后插”）

applyDeletionsClassic_

完整复用 buildFormulasIncCyclewise 的 deletion 阶段（你要求 copy paste 即可）

applyInsertionsRegional_

若 view.getDeltaInsertEdges() 和 view.getDeltaInsertNodes() 都为空：return

构造 analyzer 输入：

delta_input_facts = { n ∈ deltaInsertNodes | n->isFact() }

delta_edges = view.getDeltaInsertEdges()

delta_out_nodes = view.getDeltaOutNodes()（analyzer 需要这个）

跑 analyzer 得到 analysis（含缓存的 anchorCandidates）

planBuilder.build → plan

若 !plan.mergeReady：

若允许 fallback → applyInsertionsClassic_，stats.usedFallback=true，return

否则直接报错/抛异常（你可以决定）

rebuilder.rebuildInsertRegion → (snapshot, changedNodes)

calibrator.calibrate → 若存在 failed boundary：

允许 fallback → 直接走 classic insertion（更保守）；或

二次策略：扩大 region（但那需要回到 analyzer 扩张逻辑，不建议在这层做）

写 stats

5.3 统合单测（RegionalIncrementalForwardCompilation_EndToEnd）

端到端对比 baseline（全量重建）：

构造一个更新前图 G_old：

跑 full compile，得到 nodeFormulas_old / edgeFormulas_old

构造更新后 view（带 delta insert edges/nodes）

用 RegionalIncrementalForwardCompilation::applyUpdate 在旧公式上做增量更新，得到 nodeFormulas_inc + 权重更新

同时对更新后图做 baseline full rebuild（buildFormulasCyclewise）

选择若干 query nodes（含 region 内、boundary、region 外），对比：

WMC(nodeFormulas_inc[q]) 与 WMC(nodeFormulas_full[q]) 相等（在同一 Θnew 下：full rebuild 不需要 calibrated gate，但它的 Pr 应该与 inc 一致）

特别覆盖：query 在 region 外（验证“外部复用 + gate 校准”确实生效）

再加一个“delete+insert 同时发生”的测试：

deltaDeleteEdges 非空，deltaInsertEdges 非空

applyUpdate 应先删再插

概率对比 baseline（在更新后图上全量重建）

你关心的两条硬约束如何落实
约束 1：selector 复用 analyzer 的合并性判定结果，避免重算

通过设计强制实现：

analyzer 在 analysis.mergeableAnchorsByHead[v] 里缓存候选（mergeable 判定只发生一次）

plan 直接拷贝该 map

calibrator 只遍历候选列表，不再调用 mergeableEdgeAtHead_ 或任何 scopes/subsumption 判定

也就是说：selector 根本不具备“重新计算 mergeability”的依赖入口，自然不会重算。

约束 2：删除逻辑直接复用旧逻辑

统合模块的 applyUpdate 里 deletion 独立成 applyDeletionsClassic_，内容就是把 buildFormulasIncCyclewise deletion 阶段复制过来，不改语义；regional 只替换 insertion。

建议的实现顺序（Codex 逐步落地）

ToyFormulaManager + 基础单测（保证后面都有可用的“真·精确 oracle”）

IncRegionAnalyzer 增强输出缓存 + 单测

RegionalInsertPlan + 单测

RegionalDDRebuilder + 单测（先无 SCC，后加 SCC case）

BoundaryGateCalibrator + 单测（含 α=β 的候选跳过）

RegionalIncrementalForwardCompilation 统合 + 端到端单测

接入现有 pipeline/CLI：提供一个 flag（例如 --inc-regional），默认可先不打开；对比性能后再切默认

----

下面我按你列的 5 个点逐一给出“可直接喂给 codex 的源码级别决策 + 实现落点 + 单测设计”。我会尽量把每个点都落到具体的类/接口/伪码与测试用例上，且满足你之前的要求：selector 不重新计算 mergeable，而是直接复用 analyzer 的判定结果。

1) SCC 交叉策略：region 与外部同一 SCC 时怎么安全处理？
结论（建议给 codex 的决策）

默认策略：强制把 region 扩张到 SCC 闭合（SCC-closed）。
原因：如果 region 切开一个 SCC（同一个 SCC 同时含 region 内节点与 region 外节点），那么 region 内节点的公式/概率更新与 region 外节点互相递归依赖，你把 region 外节点当作“旧值常量”复用会破坏 SCC 的最小不动点语义；仅靠“过滤边 + Kahn on regionCycles”无法保证正确性（你实际上在 SCC 内做了不完整的方程组求解）。

你项目里已经有 CycleDependencyGraph（Tarjan SCC）可以直接复用：CycleDependencyGraph depGraph(view) 会给你 nodeToCycleIndex、nodeCycles、edgeCycles 等结构。

DerivationGraph

“SCC-closed”具体定义（实现时可写成断言）

对当前用于插入阶段的图 G+（建议用 validNodes/validEdges 构造的 SubgraphView），region R.nodes 必须满足：

对任意 SCC C，若 C ∩ R.nodes ≠ ∅，则 C ⊆ R.nodes。

这样你在 RegionalDDRebuilder 内就可以安全地把 region 外节点当作已知输入（它们不在同一 SCC 中，不会形成互相递归的闭环）。

落地实现：新增一个 SCC 闭合扩张模块

建议新增模块（header-only 也可以）：

struct RegionSccClosure {
  // returns true if region expanded
  static bool closeToScc(
      Region& R,
      const Region& reachFilter,         // 可选：只在 deltaReachable 内闭合，避免无关扩张
      const CycleDependencyGraph& scc);
};


伪码（关键点：扩张时一次性加入整个 SCC）：

bool RegionSccClosure::closeToScc(Region& R, const Region& rf, const CycleDependencyGraph& scc) {
  bool changed = false;
  std::unordered_set<size_t> touched;
  for (auto n : R.nodes) touched.insert(scc.nodeToCycleIndex.at(n));

  for (auto cid : touched) {
    for (auto n : scc.nodeCycles[cid]) {
      if (!rf.nodes.empty() && !rf.nodes.count(n)) continue; // 可选
      changed |= R.nodes.insert(n).second;
    }
    // edges：建议把 “output 在 SCC 内” 的边也纳入（便于 region 内重编译）
    for (auto e : scc.edgeCycles[cid]) {
      if (!rf.edges.empty() && !rf.edges.count(e)) continue; // 可选
      changed |= R.edges.insert(e).second;
    }
  }
  return changed;
}


你们的 CycleDependencyGraph 明确维护 nodeCycles/edgeCycles/nodeToCycleIndex 等映射。

Holtzen 等 - 2020 - Scaling exac…

如何与 analyzer 的 region fixpoint 结合（避免反复重算 mergeable）

推荐把 SCC 闭合作为 analyzer 输出后的一个 后处理 fixpoint：

IncRegionAnalyzer 先跑原本的 scope-based + boundary mergeability fixpoint。

得到 R 后，再跑 RegionSccClosure::closeToScc；若 expanded：

只重新计算 boundary 集合（不重新计算 mergeable 判定），然后继续 boundary-mergeable fixpoint；直到同时满足：

boundary 要么为空，要么 boundary node 都有 mergeable 入边

region SCC-closed

mergeable 判定复用方案见第 5 点（analyzer 内做 memo + 输出 anchor candidates）。

单测（无 gtest）建议

Test_SccClosure_ExpandsWholeScc：

构造 A<->B 成环（同一 SCC），B->C。

初始 region 只包含 A。

跑 closeToScc 后应包含 A,B（以及 SCC 内边）。

Test_SccClosure_NoChangeWhenClosed：

region 已含 A,B；closeToScc 不应再扩张。

构图可以用 DerivationGraph::createNode/createHyperedge，不需要 RuleApp。

Holtzen 等 - 2020 - Scaling exac…

2) 权重读取接口：只有 setVariableWeight 时，如何加 getVariableWeight？
你当前 CUDD 实现的事实（作为接口语义依据）

WeightedBDDManager 内部有 std::unordered_map<int, VariableWeight> weights;

CuddManager


WMC 递归时若没找到权重，会用 默认 pos=1, neg=0：

CuddManager


这意味着“未设置权重的变量”在 WMC 中等价于强制取 True。

建议新增接口（签名/行为）

给 DDManager（或 FormulaManager）补齐：

struct VariableWeight { double posWeight; double negWeight; };

virtual VariableWeight getVariableWeight(int varIndex) const = 0;
virtual bool hasVariableWeight(int varIndex) const = 0; // 可选但强烈建议


语义：

若存在显式设置，返回设置值；

若不存在，返回 {1.0, 0.0}，与当前 WMC fallback 保持一致。

CuddManager

CuddManager.h 内部实现（直接可落代码）
VariableWeight WeightedBDDManager::getVariableWeight(int varIndex) const {
  auto it = weights.find(varIndex);
  if (it == weights.end()) return VariableWeight{1.0, 0.0};
  return it->second;
}
bool WeightedBDDManager::hasVariableWeight(int varIndex) const {
  return weights.find(varIndex) != weights.end();
}

Calibrator 权重覆写/恢复（你要的伪码）

即使你最后可能不需要恢复（因为校准后要保留新权重），建议提供一个 RAII 工具，单测也更方便：

struct WeightOverrideGuard {
  DDManager& mgr;
  int var;
  VariableWeight old;
  bool hadOld;

  WeightOverrideGuard(DDManager& m, int v) : mgr(m), var(v) {
    hadOld = mgr.hasVariableWeight(var);
    old = mgr.getVariableWeight(var); // 若不存在则得到默认 {1,0}
  }

  void set(double pos, double neg) { mgr.setVariableWeight(var, pos, neg); }

  ~WeightOverrideGuard() {
    // 恢复到 old；若你未来支持 unset，可在 hadOld=false 时 erase
    mgr.setVariableWeight(var, old.posWeight, old.negWeight);
  }
};

单测建议

Test_GetVariableWeight_DefaultIsOneZero：

新建 manager，随机挑一个 varIndex，不 setWeight，getVariableWeight 应返回 {1,0}。

Test_GetVariableWeight_RoundTrip：

setWeight(p,1-p) 后 get 应相同。

3) 锚边存在性：boundary 的老入边若 deterministic/折成 True（无 gate）怎么办？
关键事实：你当前编译对 deterministic edge 会直接用 True

在 buildFormulasCyclewiseOnDemand 里，edge 若 deterministic，会直接 getTrue()，并且 idx 甚至被置为 -1（表示没有 gate var）。

DerivationGraph


同样逻辑在 forward compilation 的初始化也出现：deterministic edge 不 createVar，不 setWeight。

ForwardCompilation

而你给的 pdf 里，mergeable 条件明确要求锚边要“持有 rule variable”（C3-2）。

inc_problog_data (4)


并且 deterministic rule 在文中是 X_e ≡ 1 的情形（本质无可校准 gate）。

inc_problog_data (4)

设计决策

analyzer 的 mergeable 判定必须把 deterministic edge 排除，否则 selector 会选到“没有 gate”的锚边，calibrator 必然失败（β=α 或根本拿不到 varIndex）。

若 boundary node 的所有老入边都 deterministic（或者都不满足 non-subsumed / scope 条件），则该 boundary node 不可校准，regional update 不能安全停止在该边界。

代码落点：修改 IncRegionAnalyzer 的 mergeableEdgeAtHead_

你现在的 mergeableEdgeAtHead_ 只检查：

不是 delta insert edge

respects scope

non-subsumed

3729334

建议在最前面加 deterministic 检查：

if (e->isDeterministic()) return false;


这样 mergeableHead_（其本质是“存在至少一条 mergeable 入边”）的判定才能和 calibrator 的锚边需求一致。你现在的 mergeableHead_ 确实就是在 incoming edges 上找一条满足 mergeableEdgeAtHead_ 的边。

3729334

Calibrator 的 fallback 策略（必须“保正确性”）

我建议把 fallback 做成显式枚举，不要 silent degrade：

优先：扩 region（继续 scope 扩张/或直接扩到 deltaReachable 全闭包），直到 boundary 为空（case (i)），此时无需 calibrator。

若扩张达到上限（例如已经等于 reachFilter 但 boundary 仍非空，这理论上不该发生），则：

判失败并回退到全量 insertion 编译（调用你已有的 insertion 逻辑，如 buildFormulasIncCyclewise 一套）。

这是最安全的兜底。

单测建议

Test_Analyzer_MergeableRejectsDeterministicEdge：

boundary node v 的某条老入边 e_old prob=1。

确保 edgeMergeable(e_old)==false，并且 v 不会被误判 mergeable。

Test_Calibrator_FailsWhenNoAnchor：

构造 boundary node 只有 deterministic 入边；

calibrator 应返回明确失败码；

orchestrator 应走 fallback 分支（可在测试里用计数器/标志验证）。

4) delta_out_nodes 缺口：没有 getDeltaOutNodes() 怎么办？是否还需要 delete 的 out？
结论

插入阶段只需要 “delta inserted edges 的 head 集合” 即可。
这与你 pdf 的定义一致：OutNodes(ΔE) := { head(e) | e ∈ ΔE }。

inc_problog_data (4)


也与你当前 analyzer 的实现一致：它就是这么从 deltaInsertEdges 取 output node 的。

IncRegionAnalyzer

因此不需要额外加 getDeltaOutNodes() API；在 regional 插入模块内现算即可。

是否需要 delta 删除的 out？

你已经明确“删除复用旧逻辑，并且增量即使同时删+插也分阶段先删再插”，这和 buildFormulasInc 的结构一致（deletion phase 在前）。

ForwardCompilation


因此：

regional insertion 分析/编译阶段 无需把 deleted out nodes 纳入输入；

如果未来你要把 deletion 也改成 regional，那么再讨论 deleted out nodes（那时可能需要把 delete 的 head 作为 seeds）。

单测建议

Test_DeltaOutNodes_EqualsInsertEdgeHeads：构造两条 delta insert edge，检查计算出的 out_nodes 集合正确。

5) 测试建议：无 gtest 的最小测试框架 + ToyFormulaManager/小图用例
无 gtest 的最小测试框架

建议加一个 tests/TestMain.cpp：

#include <iostream>
#include <vector>
#include <functional>

#define CHECK(cond) do { if(!(cond)) { \
  std::cerr << "CHECK failed: " << #cond << " @ " << __FILE__ << ":" << __LINE__ << "\n"; \
  return false; } } while(0)

using TestFn = std::function<bool()>;

int main() {
  std::vector<std::pair<const char*, TestFn>> tests = {
    {"Test_SccClosure_ExpandsWholeScc", Test_SccClosure_ExpandsWholeScc},
    {"Test_GetVariableWeight_DefaultIsOneZero", Test_GetVariableWeight_DefaultIsOneZero},
    // ...
  };

  int fail = 0;
  for (auto& [name, fn] : tests) {
    bool ok = fn();
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << "\n";
    if (!ok) fail++;
  }
  return fail ? 1 : 0;
}

ToyFormulaManager 的选择

你其实不一定要 stub 一个“玩具公式系统”，因为：

region/SCC/anchor 的大部分测试只依赖 DerivationGraph + analyzer/closure；

calibrator 的正确性最好用真实的 WMC（即 CuddManager）做端到端小算例。

CuddManager 里已经有：

setVariableWeight

computeWeightedModelCount

makeCondition（通过 restrict 实现）

CuddManager


足够 calibrator 测试用。

推荐的模块级单测清单（对齐你“每个模块都带 UT”的诉求）

按你未来模块拆分，我建议至少这些 UT：

A. RegionSccClosure（新模块）

Test_SccClosure_ExpandsWholeScc

Test_SccClosure_NoChangeWhenClosed

B. Mergeable 判定缓存与输出（analyzer 修改）

你要满足“selector 直接复用 analyzer 的 mergeable 结果，避免重新计算”。做法建议：

analyzer 内维护 memo：

unordered_map<EdgePtr,bool> mergeableCache;

unordered_map<NodePtr, vector<EdgePtr>> mergeableInEdgesByHead;（或只存一个 preferred anchor）

analyzer 在第一次需要判定某个 head 时填充并缓存；

selector 只读这个 map，不再扫 incoming edges。

对应 UT：

Test_MergeableMemo_NoRecompute：用一个计数器包裹 edgeNonSubsumed_ 或 edgeRespectsScopes_（可在测试版 analyzer 注入 lambda）验证不会重复调用。

Test_MergeableRejectsDeterministic（见第 3 点）。

C. Calibrator（新模块）

用一个极小公式验证校准公式：

Test_Calibrator_ComputesPStar_MatchesTargetProb

构造 old formula F = X_anchor ∧ A ∨ B（A、B 为一些独立 var 或常量）

用 Θorig 计算：

α = WMC(F | X=0)

β = WMC(F | X=1)

给一个 target P_new，算 p* = (P_new - α)/(β - α)（并裁剪到 [0,1]）

setWeight(X_anchor)=p*，验证 WMC(F) 等于 target（允许浮点误差）。

Test_Calibrator_FailsWhenAlphaEqBeta

让 F 不依赖 X_anchor，则 β=α，calibrator 应显式失败并不改权重。pdf 里也强调 β=α 时 gate irrelevant，校准无意义。

inc_problog_data (4)

D. CuddManager getVariableWeight（接口补齐）

Test_GetVariableWeight_DefaultIsOneZero（语义对齐 WMC fallback）

CuddManager

Test_GetVariableWeight_RoundTrip

给 codex 的“整合性提示”：这些点如何回写到你的 regional insertion pipeline

你现在的 buildFormulasInc 已经把 deletion phase 写好了并且明确分阶段执行。

ForwardCompilation


因此 regional insertion 替换点就是 deletion 之后、insertion 之前 的那段逻辑。

插入阶段建议 orchestrator 顺序：

IncRegionAnalyzer 得到 region + boundaries +（缓存的）mergeable anchors（必须排除 deterministic）。

RegionSccClosure 做 SCC-closed 扩张（必要时与 boundary-fixpoint 交替迭代）。

RegionalDDRebuilder 只对 region 内节点重编译 DD（对 SCC 内用 cyclewise 方式）。

Calibrator 对 boundary nodes 做锚边校准（若 boundary 为空直接跳过；若无锚边触发 fallback）。

用更新后的 Θnew 对“需要的外部节点”做 WMC 重新求值（你可以先做最保守版：把 deltaReachable 之外但 query 相关的都算；后续再做按需）。

如果你愿意，我下一步可以直接把“给 codex 的实现计划”写成一个按 commit 切分的任务列表（每个 commit：改哪些文件、加哪些类、加哪些测试、编译通过的最小闭环），并且把你提到的 RegionalDDRebuilder/Calibrator 的接口都对齐你现有 ForwardCompilation.h 里 nodeFormulas/edgeFormulas 的数据结构风格。