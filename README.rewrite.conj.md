技术说明：SISO + Fact-Prefix + 纯合取区域的 DD 分解
0. 背景与目标

现有 pipeline 的核心目标是：在构造 DD（BDD/SDD）时，对 derivation graph 做结构分解，减少需要同时考虑的随机变量数，从而让 forward compilation 可扩展。

目前已经有两条“结构化分解”的路径：

SISO 区域（entry→exit）

GraphAnalyzer 在 derivation graph 上识别 SISO 子图；

GraphRewriter 在局部子图上用 forward compilation + WMC 求出 Pr(exit | entry)，将整个区域压缩成一条 summary hyperedge entry -> exit。

Fact-Prefix 区域（从 input facts 到某个中间节点 b 的锥）

对某个节点 b，构造它的 backward reachable 子图直到 fact frontier；

在这块子图上做局部 forward compilation + WMC 求 Pr(b)，把 b 重写为有概率的 fact，并删除 cone 内其它节点/边。

这份文档要补的是第三条 fast path：

纯合取（pure-conjunctive） backward 区域

对某个节点 b，如果它的 backward reachable 区域是“无环 + 无 disjunction + 公式结构是纯合取”的，则

不使用 DD，只通过一次 backward DFS/收集随机变量 + 简单乘积就能算出 Pr(b)；

然后重用 Fact-Prefix 的重写逻辑，把 b 变成 fact。

目标是：在保持语义正确的前提下，避免在简单结构上反复构造 BDD，减少 CUDD 开销。

1. 概念与语义：什么是“纯合取 backward 区域”？

对象：给定当前 view（IncSubgraphView）中的一个节点 b。

考虑 b 在 view 中的 backward reachable 子图 R(b)，即：

从 b 出发沿 getIncomingEdges 反向走，

一直走到 boundary（通常是 input facts：isFact=true && in-degree=0）为止，

收集经过的节点和边得到的子图。

我们把下面满足条件的 R(b) 定义为“纯合取 backward 区域”：

无 disjunction（按节点）
对 R(b) 中的每个节点 v，在当前 view 上：

|incomingEdges(v)| ≤ 1

即：无 “同一结论有多条规则推导”的情形。

在你当前构图里，input facts 不会有 incoming edges，因此不会出现 “fact + rule 同时定义同一 node”的混合 disj。

在正依赖图上无环（对 region 本身）

对 R(b) 中的每个节点 v，全局 SCC 信息显示：

sccSize[v] == 1 且没有 self-loop；

换句话说：R(b) 在“正依赖图”里是一个 DAG，不包含任何非平凡 SCC 节点。

外部其它地方（前一个 stratum 等）可以有环，只要这些节点不在 R(b) 内。

公式结构是纯合取，不出现 OR

每条规则边 e: inputs -> out 的语义仍然是：
F_e = X_e ∧ (∧_{u∈inputs} F_u)（X_e 是边上的 coin）；

对于 v，因为 |In(v)| ≤ 1，要么没有规则（fact），要么仅有一条规则：
F_v = F_e；

在整个 R(b) 无环的前提下递归展开，最终 F_b 的结构是
F_b ≡ ∧_i L_i
其中每个 L_i 是某个 primitive 随机变量的正字面 X 或负字面 ¬X（见 negation 限制）。

negation 限制：只对 primitive fact 取反

对规则体中的 not A，只允许 A 是 fact 且在 R(b) 内不再有自己的规则推导（即 A 在 region 内是“原子的”）；

在这种情况下，not A 的公式直接是 ¬X_A，仍然是一个 literal；

一旦发现 not 作用在某个非 fact 的中间节点上（该节点有自己的子公式），这块 region 就不能被认为是纯合取，需要 fallback 到 BDD。

在这些条件下，F_b 真正是一个“一堆 literal 的 conjuction”：

F_b = ∧_{v∈P} X_v ∧ ∧_{u∈N} ¬X_u

其中：

P：正向出现的 primitive 随机变量集合（fact coin + edge coin）；

N：负向出现的 primitive 随机变量集合。

如果某个变量同时在 P 和 N 中出现，就有 X ∧ ¬X ≡ False，于是 Pr(F_b)=0。

在随机变量两两独立的假设下（你目前的模型假设），可以直接计算：

Pr(F_b) = ∏_{v∈P} p_v × ∏_{u∈N} (1 - p_u)

这里 p_v 是对应 fact 或 edge coin 的概率。

2. 全局预备：SCC 信息与“trivially acyclic”判定

为了检测“region 内无环”，建议复用已有的 SCC 基础设施：

在构建 derivation graph / CycleDependencyGraph 时，全局做一次 SCC 分析（你已经在 forward compilation 前做过类似事情）。

对每个 node 记录：

sccId[node]：所在 SCC 的 id；

sccSize[sccId]：该 SCC 的节点数；

hasSelfLoop[node]：是否存在 self-loop edge。

对 GraphRewriter 暴露一个轻量接口概念（不必严格按照这里的函数名来）：

bool isTriviallyAcyclic(NodePtr v)

语义：sccSize[v] == 1 && !hasSelfLoop[v]；

在 pure-conj 检测中，我们对 region 内每个节点都要求 isTriviallyAcyclic(v) 为真。

要点：
“无环”约束的是 region 自身，而不是全图或当前 stratum；
其它 stratum 或图的其他部分可以有环，只要不被纳入本次要使用 pure-conj 的 region。

3. 检测纯合取 backward 区域：构造 R(b)

在 GraphRewriter 中，为某个节点 b 尝试 pure-conj fast path 时，需要先构造它的 backward 区域 R(b) 并进行结构检查。建议流程：

3.1 backward BFS/DFS 构造

从 b 出发，在当前 view（IncSubgraphView）上做一次 BFS/DFS：

数据结构：

worklist：节点队列或栈；

regionNodes：集合（NodePtr 集）；

regionEdges：集合（EdgePtr 集）。

伪流程（概念）：

初始化：

把 b 放进 worklist 和 regionNodes；

如果 !isTriviallyAcyclic(b)，则直接放弃 pure-conj fast path。

循环：
从 worklist 取出一个 v：

检查 isTriviallyAcyclic(v)：

若否：说明 v 所在 SCC 有环，这个 region 不适合 pure-conj，整个构造 abort（返回“非纯合取”）；

查询它在当前 view 上的 incoming edges：

如果 |incomingEdges(v)| > 1：说明存在 disjunction（多个规则推出同一结论），abort；

若 |incomingEdges(v)| == 0：

v 是 boundary：

在你当前系统中，这意味着是一个 input fact（isFact=true，in-degree=0）；

在 pure-conj 场景下，这很好：到此为止，不再往前扩展；

若 |incomingEdges(v)| == 1：设 e 为唯一入边：

将 e 加入 regionEdges；

对 e->getInputs() 中每个源 node u：

加入 regionNodes；

如果 u 不是 fact，则放入 worklist 继续 backward；

如果 u 是 fact（isFact=true 且无入边），就当作 boundary，不继续。

限制条件：
在构建过程中可以加规模阈值防止 region 过大，如：

regionNodes.size() <= SOUFFLE_PURE_CONJ_MAX_NODES；

regionEdges.size() <= SOUFFLE_PURE_CONJ_MAX_EDGES；
超过就 abort，退回 BDD 流程。

最终，如果整个过程未早期 abort，则得到：

regionNodes：R(b) 内所有节点；

regionEdges：R(b) 内所有边；
同时我们已经确保了：

region 内无环（所有 v trivially acyclic）；

region 内任何 v 的 in-degree ≤ 1（在 view 上无 disj）；

boundary 节点都是 facts（不会继续向前扩展）；

region 的拓扑结构本质上就是一个从 b 流向 facts 的 DAG。

3.2 negation 的额外约束（在下一步 probability 阶段处理）

构造 R(b) 时暂时不处理 negation，只保证结构无环、无 disj、boundary 为 fact。

在后面的“概率计算”阶段，如果发现：

某条边的 body negation 作用在一个非 fact 节点上；

或者 negation 涉及的 fact 在 region 中还有规则继续推导（不是 primitive），

则认为这个 region 在公式级别不再是“纯 literal 的合取”，需要 fallback 到 BDD。

4. 在纯合取 region 上计算 Pr(b)：literal 收集与乘积

在 R(b) 上，假设满足上述结构条件，现在要计算 Pr(b)，不通过 DD，而是直接 closed form。

4.1 primitive 随机变量与 VarId

需要一个对 primitive 随机事件的统一标识：

每个 probabilistic fact（0<p<1）对应一个 VarId；

每条 probabilistic edge（0<p<1）对应一个 VarId；

可以用适当的整数 id 或 std::pair<kind,id>，具体由现有实现决定。

并提供：

double getProbability(VarId v)：返回该随机事件的 p；

VarId varOfFact(NodePtr fact)；

VarId varOfEdge(EdgePtr edge)。

4.2 literal 集合：正负集合 + 矛盾检查

在 R(b) 上构造公式：

F_b = ∧_i L_i
L_i ∈ {X, ¬X}

我们维护两个集合：

posVars：出现为 X 的 VarId 集合；

negVars：出现为 ¬X 的 VarId 集合。

规则：

遍历 region 内所有 edge：

对每条边 e：

若 0 < prob(e) < 1：

取 ve = varOfEdge(e)；

插入到 posVars；

如果 ve 已在 negVars 中，则 X ∧ ¬X ≡ False，直接判定 Pr(b)=0，结束 fast path。

处理 body negations：

对每个 not A：

要求 A 是 fact 且在 region 中没有进一步 incoming edges（之前构造中已经保证 A 是 boundary fact）；

取 va = varOfFact(A)，插入 negVars；

若 va 已在 posVars 中，同样说明 X ∧ ¬X，Pr(b)=0。

遍历 region 内所有 fact 节点：

对每个 fact f：

若 0 < prob(f) < 1：

取 vf = varOfFact(f)，插入 posVars；

若 vf 已在 negVars，则 Pr(b)=0。

如果在上述步骤中没有出现正负矛盾，则得到一组 self-consistent 的正/负 literal。

4.3 概率乘积

在变量独立假设下：

Pr(F_b) = ∏_{v∈posVars} p_v × ∏_{u∈negVars} (1 - p_u)

实现上：

遍历 posVars，累乘 p_v；

遍历 negVars，累乘 (1 - p_v)；

若中途有任何 p_v 是 0 或 1，按正常乘法处理（等价于一些 literal 实际上是常量）。

如果已经判定 X ∧ ¬X 情况，直接返回 0。

4.4 fast path vs fallback

如果在 literal 收集过程中发现以下情况之一：

negation 作用在非 fact 上（或在 region 内 fact 又有规则推导）；

不能为某个节点/edge 分配明确的 VarId；

或者其它实现上无法处理的复杂情况（比如未来扩展出的新构造）；

则应当放弃纯合取 fast path，回退到原来的 BDD-based computeRegionMarginalProbability，保持语义正确。

5. 与 GraphRewriter / Pipeline 的集成建议
5.1 集成点：fact-prefix pass

目前 GraphRewriter 大致有两类 rewrite：

Fact-prefix 区域重写：

构造从 exit=b backward 到 fact frontier 的 region；

用 BDD 在 region 上算 Pr(b)；

把 b 设成 fact(prob=Pr(b))，删除 region 内其它节点/边。

entry→exit SISO 重写：

对 GraphAnalyzer 找出的 SISO region，算 Pr(exit|entry)，插入 summary edge。

新的 pure-conj fast path 自然放在 fact-prefix 重写逻辑内部，作为一个“先试 cheap 路径，失败再跑 BDD”的分支：

对每个 candidate exit 节点 b：

先用上一节的 pure-conj 检测构造 R(b)（regionNodes/regionEdges）：

若构造失败（发现 disj、环、节点数太大等），则跳过 pure-conj，直接走 BDD；

在 R(b) 上尝试纯合取概率计算：

若成功（包括 Pr(b)=0 的情况）：

用现有 fact-prefix rewrite 逻辑把 b 变成 fact，删掉 cone 内其它节点/边；

不再对这个 b 做 BDD；

若纯合取检测/计算失败（遇到 negation on non-fact 等），则 fallback 到原有 BDD 流程。

5.2 与 SISO 重写的顺序

建议顺序保持为：

一个外层迭代 loop（直到本轮没有任何 rewrite 为止）；

每轮中：

先做 fact-prefix + pure-conj fast path；

再做 fact-prefix + BDD-based 重写（针对未被 pure-conj 处理的节点）；

最后做 entry→exit SISO 重写；

若某一轮中三种 rewrite 都没做任何事情，视为达到 fixpoint。

这样：

纯合取 fast path 是最 cheap 的，优先消除图中的 100% conjunctive 区域；

BDD-based fact-prefix 承接剩余的“从 facts 到 b 的 cone”；

SISO 则针对一般的“内部结构复杂、对 entry→exit 可分解”的区域处理。

5.3 环与 strata 的关系

实现时不需要显式关心“当前 stratum”和“前一 stratum”的边界，只需要：

对 region 内每个节点 v 调用“isTriviallyAcyclic(v)”（基于全局 SCC）；

若任何 v 参与非平凡 SCC（不管它本身属于哪个 stratum），这个 region 都不能用 pure-conj fast path；

你当前的构图设置（input facts 无 incoming edges，被 prune 成 boundary）意味着：

backward cone 通常只包含当前 stratum 内的节点和事实；

前一 stratum 内的有环结构不会出现在 pure-conj 区域中（否则会有规则延伸到这边）。

6. 运行开关与调参建议

为了便于实验与调试，建议为 pure-conj fast path 增加一个可配置开关和阈值：

环境变量或选项：

SOUFFLE_PURE_CONJ_REWRITE（true/false，默认开启）；

SOUFFLE_PURE_CONJ_MAX_NODES（默认等于或小于 fact-prefix 的 MAX_NODES）；

SOUFFLE_PURE_CONJ_MAX_EDGES；

SOUFFLE_PURE_CONJ_MAX_RANDOM_VARS（可选：控制 region 内随机变量数的上限，过大时仍交给 BDD）。

这些配置的读取可以参考现有的 SOUFFLE_SISO_MAX_EDGES、SOUFFLE_FACT_PREFIX_MAX_* 风格。

在 rewrite.log 或控制台输出中增加一些统计字段，例如：

pureConjRegions：使用了 pure-conj fast path 的区域数；

pureConjZeroProbRegions：fast path 算出的 Pr(b)=0 的区域数；

平均/最大 pureConjRegionNodes/Edges 等。

7. 测试与验证建议
7.1 单元测试

构造若干小 derivation graph 的例子，覆盖：

纯合取 DAG（无 disj、无环）：

例如：a,b 为 facts，c :- a (coin), d :- c,b (coin)，求 Pr(d)。

对照：直接用 BDD pipeline vs pure-conj fast path，检查结果一致。

有 disj：

d :- a, d :- b；

buildPureConjRegionFrom 应返回“非纯合取”，走 BDD；

用 fast path 的话应该被禁止。

有环：

a :- b, b :- a；

任何包含 a 或 b 的 region 都应被 isTriviallyAcyclic 拦截，不能用 pure-conj fast path。

negation on fact：

p 是 fact，q :- not p；

backward region 是 {q, p}，结构无 disj 无环；

pure-conj 应能识别 literal 集合 {¬X_p}，Pr(q) = 1 - p(p)。

正负同时出现：

构造一个 region 包含 p 和 not p 在同一路径上（例如不同 rule 链合到一个节点），

literal 收集时应 detect 到 X 和 ¬X 同时出现，返回 Pr=0。

negation on非fact：

q :- p, r :- not q；

在构造 R(r) 时遇到 not q，且 q 不是 fact；

此时 tryComputePureConjProbability 应返回“不能处理”（fallback BDD），检查结果与纯 BDD pipeline 一致。

7.2 集成测试

在你已有的 benchmark 上（P9/P10 等）分场景跑：

baseline：--rewrite 关闭；

rewrite without pure-conj：只用现有 fact-prefix + SISO；

rewrite with pure-conj：新 fast path 开启。

比较：

最终 query 的概率结果是否一致；

总运行时间、BDD 节点数、memory；

pureConjRegions 的数量及平均 region 大小。

8. 总结

对 Codex 来说，这个改造可以分为三个主要步骤：

接线：

从现有 CycleDependencyGraph / SCC 结果暴露一个 isTriviallyAcyclic(node) 的接口；

在 GraphRewriter 中增加 pure-conj fast path 调用点（优先于 BDD 的 fact-prefix 重写）。

结构检测：

在 view 上，从 exit 节点 backward BFS/DFS 构造 region R(b)；

过程中检查：无环（trivial SCC）、无 disj（每节点 in-degree ≤ 1）、boundary 为 fact、规模不超阈值。

概率计算：

遍历 region edges + facts，收集 primitive 随机变量的正负 literal；

若发现同一 VarId 同时正负出现，直接 Pr=0；

否则按 ∏ p × ∏ (1-p) 乘出 Pr(b)，重用现有 fact-prefix rewrite 把 b 变成 fact。

所有地方在实现时都应有良好 fallback：任何检测阶段失败，立即退回到已有的 BDD-based 逻辑，从而保证 correctness 优先、优化次之。