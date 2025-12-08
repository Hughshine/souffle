## 背景 / 用法
- 核心代码：`src/include/souffle/problog/GraphAnalyzer.h`（SISO 检测），`GraphRewriter.h`（重写与后处理），`Pipeline.h`（管线计时），`Plan.md` 记录待办。
- 编译：`cmake --build cmake-build-release --target souffle -j4`
- 运行示例（带 profiling）：`./compute_new -p run_profile.log -F ./input -D ./output`；开启重写加 `-r`；fast-path 调试输出加 `SOUFFLE_SISO_FAST_DEBUG=1`。

## 目前实现状态（2025-12-07）
### Fast-path SISO 检测
- 支持的 RegionKind：`SingleHyperedge`（非 fact SI + ≥1 fact 输入，fact 输出单一边），`AllFactsToSO`（输入全 fact 且只连这条边），`LinearTwoEdge`（entry→mid→exit，mid 不可 query/evidence），`ParallelEdge`（>=2 单输入平行边，占位未重写），`General` 未启用。
- 过滤：fact 输入需非 evidence/needOutput，all-facts 输入必须只有这一条出边；single-hyperedge 跳过单输入“朴素”边；linear mid 需唯一出边且非 query/evidence。
- 重叠：仍做非重叠筛选。

### 重写逻辑
- All-facts：折叠为 fact 输出，概率乘以所有 fact 输入；删除孤立 fact 输入及原边。
- Single-hyperedge：生成 SI→SO 新边，概率乘以 fact 输入，删除旧边及孤立 fact 输入。
- Linear two-edge：两边概率相乘生成 entry→exit 新边，删除 mid 与两条旧边。
- Parallel two-edge：仍占位（跳过）。
- 每轮结束后执行“边收缩”：对所有边吸收非 evidence/query 的 fact 输入并更新概率，删除孤立 fact。
- BDD manager 延迟初始化；dump dot、检测计时已保留。

### 输出/日志
- `[pipeline]` 行列出 create/pruning/rewrite/build/WMC 等耗时，rewrite 行包含迭代数、region 数、RV 变化、节点/边增删。
- `Current live nodes` 为最终 forward compilation 的 BDD 节点数。
- `SOUFFLE_SISO_FAST_DEBUG=1` 时打印 fast-path 过滤原因。
- per-iteration 计时（GraphRewriter）：`total`、`countBefore`、`detect`、`loop`（含 `loopCond/loopRewritten/loopSkip`）、`edgeList`、`compact`、`countAfter`、`dumpRegions`（debug 时 dumpAllRegionsAsDot）、`dumpDot(before/after)`、`preLog/log`，`other` 为剩余杂项。dumpAllRegions/dumpDot 在 debug 下可能是主要开销（例如 P13 首轮 dumpRegions≈18ms, dumpDot(before)≈7ms, after≈2ms）。
- fast-path debug 文本输出已注释以降低日志开销；事实概率 rewrite/no-rewrite 已确认一致。

## 最近基准（facts.prob 均一致）
- P13：no-rw ≈4550 ms，BDD 97k；rw ≈2446 ms，BDD 10.9k。
- P14：no-rw ≈10285 ms，BDD 199k；rw ≈4916 ms，BDD 17.5k。
- P15：no-rw ≈44767 ms，BDD 820k；rw ≈27037 ms，BDD 45k。
- P16：no-rw ≈141729 ms，BDD 2.08M；rw ≈74557 ms，BDD 97k。
- P17 无重写 300s 超时（未完成）；重写未跑。P18/P19 未跑。

## 可能的下一步
- 完成 ParallelTwoEdge 重写（或禁用）。
- 解决 P17/P18/P19 超时/大规模输出：可只跑重写或放宽超时。
- 复核 edge compaction 对大图的收益/正确性；考虑跳过 query/evidence 相关边。
- 清理未跟踪的实验产物（大量 dot/log）。

## 基准图结构小结（side_channel_full）
- 主力 SISO 形态：`AllFactsToSO` 与 `SingleHyperedge` 最多（大量 fact 前置的一条超边），其次是少量 `LinearTwoEdge`；`ParallelEdge` 目前基本无匹配。
- 常见模式：多输入 fact 聚合到一个中间结点，再指向一个 RAND/KEY 节点；经过重写后可将中间结点变为 fact，进一步触发新一轮 all-facts 折叠。
- 罕见/缺失：真实的多输入并行单边（Parallel）几乎没有；general/复杂 SISO 未启用。
- 影响性能的阶段：rewrite 开销几乎全部是 fast-path SISO detection（首轮最慢，后续递减）；在 `-p` 下，若开启 debug，会有 `dumpAllRegionsAsDot`/`dumpDot` 的 I/O 开销（单次可达 10–20 ms，P13 观测）。
- BDD 构建：重写后 BDD 节点数大幅下降（P14~P17 中 rewrite BDD 比 no-rewrite 小 10–50×），构建时间与节点数成正比。

## 最新端到端对比（P14–P17，使用最新 compute_new）
粗略总耗时为 create+prune+rewrite+BDD build+per-node+prob dump 求和（来自 `run_rewrite_prof_new.stdout` 与 `run_no_rewrite_prof_new.stdout`）：
- P14：rw ≈ 1.4s vs no-rw ≈ 13.0s（≈ **9.3×** 提速）
- P15：rw ≈ 9.5s vs no-rw ≈ 44.7s（≈ **4.7×**）
- P16：rw ≈ 16.5s vs no-rw ≈ 138.3s（≈ **8.4×**）
- P17：rw ≈ 31.9s vs no-rw ≈ 307.2s（≈ **9.6×**）
分解看：rewrite 侧的 BDD build/per-node 大幅降低；重写阶段本身已压到 0.2–1.9s 范围，检测也在数 ms–百 ms 范围。

### 各阶段计算量差异（rw vs no-rw）
- RandomVars：rw 结束后约为 no-rw 的 24%（P14–P17 的 randomVarsRatio ≈0.24，去除 3/4 变量）。
- BDD 节点（对应 BDD build / per-node 时间）：rw 的节点数与时间约为 no-rw 的 1/6–1/50（同一案例内，BDD build 与 per-node 均随节点数线性降低）。
- Rewrite 迭代次数与 region 数：P14–P17 重写迭代 11–12 轮，regions 数 13k–63k，`nodesRemoved/edgesRemoved` ≈ `randomVarsRemoved` 量级，对最终规模压缩明显。
- 无 rewrite 情况：SISO detection（legacy）耗时巨大（旧日志首轮可达数十秒到数百秒），在 rw 路径下 fast-path detection 已压到毫秒级。

### rewrite 子阶段 profiling 特征
- per-iteration 计时输出（GraphRewriter）：`total`、`countBefore`、`detect`（含 fast-path 各类别细分）、`loop`（cond/rewritten/skip）、`edgeList`、`compact`（边收缩）、`countAfter`、`dumpRegions`、`dumpDot(before/after)`、`preLog/log`、`remainder`。
- 量化耗时（P14–P17，`-p`，未开 debug）：单轮 `total` ≈0.1–0.2s（P14）到 ≈1.9s（P17）；其中 `detect` 首轮 5–90 ms，后续降至 1–20 ms；`compact`/`edgeList`/`count*` 常在 1–10 ms；`dumpRegions`/`dumpDot` 在未开 debug 时 ~0 ms。
- 开启 debug（`-p` 默认开启，dot 输出受环境控制）时的 I/O：`dumpAllRegionsAsDot`/`dumpDot` 可占单次 10–20 ms（P13 观测），会在 per-iter `dumpRegions/dumpDot` 字段体现。
- BDD manager 延迟初始化，仅在需要 BDD 时创建；每轮结束的边收缩吸收 fact 输入并删除孤立 fact。
- Fast-path detection 细分统计：`singleHyperedgeMs/Count`、`allFactsToSOMs/Count`、`linearTwoEdgeMs/Count`、`parallelEdgeMs/Count`；调试过滤原因可用 `SOUFFLE_SISO_FAST_DEBUG=1`。

### 近期案例中的 SISO 分布（累计，rw 模式）
- P14：single≈554，linear≈6.3k，parallel=0，all-facts≈3.9k；首轮以 all-facts 为主，后续转为 linear。
- P15：single≈1.1k，linear≈13.4k，parallel=0，all-facts≈7.4k。
- P16：single≈1.8k，linear≈22.6k，parallel=0，all-facts≈13.4k。
- P17：single≈2.4k，linear≈30.7k，parallel=0，all-facts≈18.1k。
- 规律：linear 与 all-facts 占比最高；parallel 未命中；single 量级次之。

## 近期实验命令（P14–P17）
- 生成可执行（带 profile 与 full-only）：  
  `souffle_bin=/home/hugh/research/datalog/souffle/cmake-build-release/src/souffle`  
  `"$souffle_bin" --full-only --profile=/dev/null --online -F ./input -D ./output compute.souffle.dl -o compute_new > rebuild.log 2>&1`
- 跑 rewrite（含迭代/检测日志，输出到 `run_rewrite_prof_new.*`）：  
  `./compute_new -r -p run_rewrite_prof_new.log -F ./input -D ./output_rewrite_prof > run_rewrite_prof_new.stdout 2>&1`
- 跑 no-rewrite 对照（输出到 `run_no_rewrite_prof_new.*`，需先 `mkdir -p output_no_rewrite_prof`）：  
  `./compute_new -p run_no_rewrite_prof_new.log -F ./input -D ./output_no_rewrite_prof > run_no_rewrite_prof_new.stdout 2>&1`
- 适用目录：`experiments/side_channel_full/P14`～`P17`（在各目录下执行上述命令）。
