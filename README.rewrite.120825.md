## 背景与目标（2025-12-08）
- 核心组件：`src/include/souffle/problog/GraphAnalyzer.h`（SISO 检测）、`GraphRewriter.h`（重写与边收缩）、`Pipeline.h`（管线计时）、`Plan.md`（任务记录）。
- 目标：通过 SISO/局部重写和 fact-prefix/边收缩减少随机变量、降低 BDD 构建成本，并在简单结构上用更便宜的 fast path 替代 BDD。
- 当前 fast path 假设无 negation（除非明确处理）：默认只吸收正向 literal，`bodyNegations` 仅在部分路径使用（吸收 fact、单边吸收、线性两边入口极性、fan-out 收敛等）。

## 运行指令模板
- 编译 Souffle（release）：`cmake --build cmake-build-release --target souffle -j4`
- 编译每个基准（例：P13）：  
  `souffle_bin=/home/hugh/research/datalog/souffle/cmake-build-release/src/souffle`  
  `"$souffle_bin" --full-only --profile=/dev/null --online -F ./input -D ./output compute.souffle.dl -o compute_new > rebuild.log 2>&1`
- 运行 rewrite：`./compute_new -r -p run_rewrite_prof_new.log -F ./input -D ./output_rewrite_prof > run_rewrite_prof_new.stdout 2>&1`
- 运行 no-rewrite：`./compute_new -p run_no_rewrite_prof_new.log -F ./input -D ./output_no_rewrite_prof > run_no_rewrite_prof_new.stdout 2>&1`
- 对比输出：`diff output_no_rewrite_prof/facts.prob output_rewrite_prof/facts.prob`
- 清理 dot（示例 P13）：`rm -f rewrite_iter*.dot siso_regions_iter*.dot rewrite_final.dot`

### 多个基准快速跑法
- 先在 repo 根清理 dot：`find experiments/side_channel_full -maxdepth 2 -name "*.dot" -delete`
- 编译一次 compute（上面的 souffle 指令）。
- 逐个目录运行 rewrite：`for p in P13 P14 P15 P16 P17; do (cd experiments/side_channel_full/$p && ../../compute_new -r -p run_rewrite_prof_new.log -F ./input -D ./output_rewrite_prof > run_rewrite_prof_new.stdout 2>&1); done`
- 同理 no-rewrite：将 `-r` 去掉、输出改 `_no_rewrite_prof`。
- 跑完再统一 diff：`for p in P13 P14 P15 P16 P17; do diff experiments/side_channel_full/$p/output_no_rewrite_prof/facts.prob experiments/side_channel_full/$p/output_rewrite_prof/facts.prob || echo "$p differs"; done`

## 已实现的 SISO RegionKind（fast path）
- `SingleHyperedge`：一条超边，恰好 1 个 SI（非 fact），其余为可吸收 fact（primitive、唯一出边）。重写：吸收 fact 概率（考虑 negation 仅在吸收 fact 时 p/(1-p)），生成 SI→SO 新边。
- `AllFactsToSO`：单边，输入全是 fact（无入边、唯一出边、非 evidence/needOutput），SO 变 fact，概率为边 coin × ∏inputs（考虑 negation p/(1-p)）。
- `LinearTwoEdge`：entry→mid→exit，mid 非 query/evidence，mid 仅一条出边；允许入口边带 neg，出口边不得 neg。重写：概率相乘，保留入口极性，生成单边 entry→exit。
- `ParallelEdge`：仅检测并行单输入边（按输入、极性分桶），未做重写。
- `FanOutConverge`：SI（fact）扇出单输入边到 xi，xi 唯一出边收敛到同一 SO 的多输入正极性边；SI→xi 极性需一致，混合极性则删除子图。重写：吸收 SI coin（按极性）、fan 边 coin、收敛边 coin，SI 成 fact，插入 SI→SO 概率 1 边，删除 xi/相关边。
- `General`：未启用。

## Edge compaction（每轮 SISO 后）
- 遍历所有边，吸收输入中“唯一出边的 fact”概率，且尊重 negation：若该 fact 对应输入被标记 `bodyNegations[i]`，则乘 `(1-p)`；否则乘 `p`。只吸收 `isFact && !hasEvidence() && !needOutput && outs.size()==1 && outs[0]==edge`，保留其他输入。移除因吸收变孤立的 fact；重建新边，更新概率。无 negation 的其他输入不吸收。

## 重要约束与语义
- Fast path 默认不支持含 `not` 的区域，除特定处理（吸收 fact 时可按 negation 取 1-p，LinearTwoEdge 入口 neg，FanOutConverge 极性一致）。
- 吸收 fact 要求唯一出边，防止外部引用被破坏；SISO 检测中可吸收 fact 同样要求唯一出边。
- 线性两边出口边若带 neg 则跳过 fast path（回退）。
- ParallelEdge 仅检测，不重写；混合极性并行不会合并。
- FanOutConverge：要求收敛边体全正，fan 极性一致；混合极性删除子图（SO 保留）。

## 最近实验与结果
- P5：fan-out-converge 命中 1 次；rewrite/ no-rewrite `facts.prob` 一致；检测耗时 ~0 ms，debug dot 会额外 10–20 ms I/O。
- P13（最新代码）：rewrite 端到端 ~105 ms；BDD build 731 ms（no-rewrite 2705 ms）；facts.prob 一致；fan-out-converge 0 次；parallel 检测分桶但未重写。
- 旧测 P14–P17（未包含最新 compaction/negation 处理）：rewrite 相对 no-rewrite 端到端加速约 4.7×–9.6×，主要来自 BDD 节点下降；需在最新版本重新跑以更新统计。

## 性能与一致性（最新观测，BDD）
- P13：rewrite 163 ms；BDD build 13 ms（no-rewrite 2626 ms，per-node WMC 1642 ms）；RV 6142→650（ratio 0.106）；facts.prob 一致。debug dot 每轮 ~15–20 ms I/O。
- P14：rewrite 227 ms；build 236 ms（no-rewrite 3557 ms，per-node WMC 3850 ms）；RV 9017→1592（ratio 0.177）；facts.prob 一致。
- P15：rewrite 484 ms；build 1041 ms（no-rewrite 13532 ms，per-node WMC 17286 ms）；RV 18130→4233（ratio 0.233）；facts.prob 一致。
- P16：rewrite 980 ms；build 1978 ms（no-rewrite 53678 ms，per-node WMC 63277 ms）；RV 32011→7700（ratio 0.241）；facts.prob 一致。
- P17：rewrite 1484 ms；build 3587 ms（no-rewrite 96263 ms）；per-node WMC 2008 ms；RV 43529→10500（ratio 0.241）；facts.prob 一致。
- P18：rewrite 2589 ms；build 6724 ms；per-node WMC 3504 ms；RV 55051→13286（ratio 0.241）；只跑 rewrite。
- P19：rewrite 3092 ms；build 8577 ms；per-node WMC 5403 ms；RV 69446→16800（ratio 0.242）；只跑 rewrite。
- Fast-detect 耗时 ms 级；开启 debug（dot 输出）会引入 10–20 ms/轮 I/O。

## SDD 状态
- P13 SDD 路径 WMC 异常：`KEY_SENSITIVE(20)` 溢出到 `8.1854755e+127`（BDD 0.944704），初始权重正常，疑似 SddFormulaManager var 索引/权重映射或 var_count 不匹配；暂不使用 SDD。

## 调试/日志
- `-p` 运行输出 `[pipeline]` 计时；rewrite 行含迭代数、region 数、RV 变化等。
- Fast-path 检测日志（SOUFFLE_SISO_FAST_DEBUG=1）会打印跳过原因；默认关闭以减小开销。
- 计时字段（GraphRewriter per-iter）：`countBefore`、`detect`、`loop`（cond/rewritten/skip）、`edgeList`、`compact`、`countAfter`、`dumpRegions`、`dumpDot(before/after)`、`preLog/log`、`remainder`。

## 仍需注意/待办
- ParallelEdge 尚未实现重写；检测已按极性分桶。
- 若需继续改进，优先排查 SddFormulaManager 的 var 映射/var_count 以修正 SDD。
- negation 全面支持尚未完成；含 not 的区域应回退 BDD。
- 修改 SingleHyperedge 放宽 SI 为 fact 曾导致事实差异（外部 escape）；目前保持 SI 非 fact。
- 运行前清理旧产物（输出、dot），避免混用旧结果。

## 注意事项/风险
- negation 全局未彻底支持：除明确处理的场景外，含 `not` 的区域应回退 BDD。
- SingleHyperedge SI 目前要求非 fact（放宽曾导致概率差异）。
- SDD 目前结果不可信（P13 溢出），默认用 BDD。
- 运行前清理旧产物（输出、dot），避免混用旧结果。
