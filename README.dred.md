# README: Online DRed（Derivation Graph 增量）实现与性能笔记

## Scope / Context
- 仅覆盖在线增量路径 `--online`，这也是当前启用的实现。
- 旧的 `--inc` incremental backend 已弃用，本文件不再讨论。
- 关注 semi-naive + DRed-like deletion/rederive/insertion；rewrite/forward compilation 另见其它文档。

## Online DRed 流程（代码级概览）
1) CLI 解析 delta，写入 `$inc_delta_tuple_{insert,delete}_*`；删除不会直接从 base relation erase。
2) 生成代码的 `runFunctionInc()` 先调用 `inc_table_update`，再顺序执行各 `_inc` strata。
3) `inc_table_update`：清空 `@old_*` 后复制 base；清空 `@inc_tuple_overdelete_*` / `@inc_derv_overdelete_*`；拷贝 `$inc_delta_tuple_delete_*` → `$inc_delta_derv_delete_*` 作为 overdelete 种子。
4) Deletion fixpoint：记录 derivation 删除 → 产生 `@inc_*_overdelete`。
5) Rederive fixpoint：生成 `@inc_new_derv_rederive_*` + `@inc_delta_tuple_rederive_*`，从 overdelete 中扣除可重推导。
6) Insertion fixpoint：处理 `$inc_delta_tuple_insert_*`，递归层会产生 delta-delta 规则应用；非递归层通常走 purge+scan 重建。

## 1. 目标与背景

本模块实现了类似 DRed 的增量化 semi-naive evaluation，用于生成/维护 derivation graph（tuple 的推导来源、rule application 记录等）。当前发现：

- Deletion 更新时结果正确，但性能显著更慢（甚至比全量重算慢一个数量级）。
- 可能原因包括：over-deletion 的算法性额外代价 + C++ 代码生成/运行时实现低效（乃至内存泄漏、容器误用、关系未清空导致反复扫描等）。

**本 README 的用途**：归纳实现逻辑，并整理性能瓶颈定位路径，目标是解释 deletion 慢的原因并给出可验证的优化方向：
1) 定位 deletion 慢的主要瓶颈（CPU / 内存 / 容器 / 代码生成策略）。
2) 通过插桩与工具（perf/asan/lsan/heap profiler）将瓶颈定量化。
3) 基于源码提出/实施可验证的优化或修复（优先解决“数量级差”的根因，如泄漏与未清空）。

---

## 2. 相关术语（便于对齐）

- **Complete derivations**：某个 tuple 当前所有推导（rule applications）的完整集合。
- **Delta/DeltaDelta**：增量波次中产生的新增/删除推导集合，用于合并到 complete derivations。
- **Over-deletion**：DRed 中为了保证正确性，先“过度删除”再重推导（re-derive）。
- **Re-derive / re-insertion**：对可能仍有支持的 tuple 重新推导以恢复。
- **Stratum / Recursive stratum**：递归分层；递归层会放大增量传播成本。

---

## 2.1 Online 增量 relation 命名（关键前缀）
- `$inc_delta_derv_{insert,delete}_*`：derivation 变化的 tuple（规则应用变动）。
- `$inc_delta_tuple_{insert,delete}_*`：真实 tuple 插入/删除。
- `@inc_tuple_overdelete_*` / `@inc_derv_overdelete_*`：overdelete 集合。
- `@inc_new_derv_rederive_*` / `@inc_delta_tuple_rederive_*`：rederive 过程中的增量推导与“仍存活”的 tuple。
- `@old_*`：`inc_table_update` 中缓存旧结果；非递归 `_inc` 逻辑会用它重建 base。
- `@new_*` / `@delta_*`：semi-naive 生成的常规关系（在在线增量里仍存在）。

## 3. 代码入口与读码路线（必看）

> 主要分析对象是 online 翻译与 C++ 代码生成：`src/ast2ram/online/*` 与 `src/synthesiser/Synthesiser.cpp`。

按如下顺序阅读，基本覆盖 deletion 路径的关键热点：

### 3.0 Online pipeline 与命名规则
- `src/ast2ram/online/UnitTranslator.cpp`：生成 `inc_table_update` 与 `_inc` strata；创建 `@inc_*` / `$inc_*` 关系。
- `src/ast2ram/online/IncClauseTranslator.cpp`：rederive 版本 clause 与 RAM 生成。
- `src/ast2ram/utility/Utils.cpp`：增量关系命名前缀（overdelete/rederive 等）。
- `src/ast2ram/utility/TranslatorContext.cpp`：`--online` 选择 online 翻译策略。

### 3.1 `visit_(Clear)`：关系清空语义是否被正确实现
搜索关键函数：
- `visit_(type_identity<ram::Clear>, ...)`
- 以及注释 `// TODO: cannot purge real relations now, but incremental computation might need this`

重点关注：
- **仅 temp relation 才 purge**，non-temp relation 的 `Clear` 目前是 no-op（潜在导致增量辅助关系不被清空，越跑越慢）。

### 3.2 `visit_(RecordDerivation)`：记录推导（新增/删除）时的开销与策略
搜索关键函数：
- `visit_(type_identity<ram::RecordDerivation>, ...)`

重点关注 deletion 分支：
- 是否存在“运行时递归判断 + 扫描 complete derivations 并复制”的逻辑。
- 是否对每次 deletion 都会扫描完整 `ruleSetComplete`，导致 O(|complete|) 放大。

### 3.3 `visit_(DeltaUnion)`：合并 delta 推导集合到 complete（以及可能的内存泄漏）
搜索关键函数：
- `visit_(type_identity<ram::DeltaUnion>, ...)`

重点关注：
- `untypedTuple2DeltaDeltaInsertRuleApplications`
- `untypedTuple2DeltaDeltaDeleteRuleApplications`
- 合并后是否仅 `map.clear()`，但 value 是裸指针（`new unordered_set`）导致泄漏。
- 删除分支的 `erase` 是否成本过高（hash/eq 可能涉及 vector）。

### 3.4 Relation API / erase 限制（影响 deletion 策略）
- `src/include/souffle/SouffleInterface.h`：Relation 接口只有 `insert/contains/purge`，没有 tuple erase。
- `src/synthesiser/Relation.cpp`：只有 `RelationRepresentation::BTREE_DELETE` 会生成 `erase()`。
- `src/ast2ram/online/UnitTranslator.cpp`：base relation 默认用 `BTREE_DELETE`，但所有以 `@`/`$` 开头的辅助关系会降级为 `DEFAULT`；仅 `@inc_tuple_overdelete_*` 例外保留 `BTREE_DELETE`。
- 结果：多数增量辅助关系只能 purge/rebuild，删除路径更依赖扫描与重建。

### 3.5 生成 C++ 证据（P12 compute.cpp）
- 关系类型命名 `t_btree_{hasErase}{hasAux}{hasProv}_...`；`100` 代表 `btree_delete_set`（有 `erase()`），`000` 为普通 `btree_set`。
- P12 中 base relation 与 `@inc_tuple_overdelete_*` 是 `t_btree_100...`；多数 `$inc_delta_*` / `@old_*` / `@inc_derv_overdelete_*` / `@inc_*_rederive_*` 为 `t_btree_000...`（无 `erase()`）。
- `inc_table_update`：`purge(@old_*)` 后复制 base；`ExactClear(@inc_tuple_overdelete_*)` / `ExactClear(@inc_derv_overdelete_*)`；`$inc_delta_tuple_delete_*` → `$inc_delta_derv_delete_*`。
- 非递归 `_inc` strata 采用 purge+scan 重建 base（`@old_*` minus delete + insert delta），即便 base 本身支持 `erase()`。

---

## 4. 高优先级“可疑点清单”（按可能造成数量级差排序）

> 建议逐条“证伪或证实”，并输出数据支撑（log/trace/profile）。

### S1. Clear 对 non-temp relation 不 purge（可能导致 delta/辅助关系越积越大）
现象匹配：
- deletion 需要多轮传播/回收/重推导，每轮都会扫描 delta/中间关系；
- 如果这些关系未被清空，会反复扫描旧数据，造成指数/数量级放大。

验证动作：
1) 找出 deletion 流程中会被 `Clear` 的 relation 名称集合。
2) 运行时打印：`relationName`, `isTemp`, `size before`, `size after`.
3) 检查“应当清空的 delta 关系”是否被标为 non-temp（导致 no-op）。

可能修复方向（任选一种）：
- 让增量辅助关系在 RAM/元信息中标为 `temp`
- 或者将对应 `Clear` 替换为 `ExactClear`
- 或者在 codegen 中对特定前缀（如 `@delta_`, `$inc_delta_`, `@inc_`）允许 purge（需非常谨慎避免误清空 base/output）

---

### S2. DeltaUnion / DerivationManager 使用裸指针 set，clear 但不 delete：高概率内存泄漏
现象匹配：
- deletion 往往产生更多 delta（overdelete/rederive），更容易触发大规模 set 分配；
- 泄漏会导致内存膨胀、cache miss、allocator 压力，最终性能断崖。

验证动作（必须做）：
1) 用 AddressSanitizer + LeakSanitizer 跑包含 deletion 的最小 repro：
   - `-fsanitize=address,leak -fno-omit-frame-pointer`
2) 或使用 heap profiler（heaptrack/valgrind massif）观察是否随迭代增长。

定位要点：
- `RecordDerivation` 中是否 `new std::unordered_set<RuleApplication>()`
- `DeltaUnion` 末尾是否只有 `map.clear()`（不释放 set）

修复建议（优先级最高）：
- 将 map value 改为 `std::unique_ptr<std::unordered_set<RuleApplication>>`
- 或在 `clear()` 前显式遍历 delete（注意“接管 set 指针”的分支，避免 double-free）

交付要求：
- 修复后，LSan 报告必须显著减少/消失；
- deletion 性能应有可观改善（通常立竿见影）。

---

### S3. deletion 分支存在 “扫描 complete derivations 并复制”的 O(|complete|) 热点
现象匹配：
- 某些 tuple 的 derivation 数量很大时，每次 deletion 触发全扫描；
- 在递归层传播时触发次数会很高，导致比全量重算更慢。

验证动作：
1) 插桩统计：
   - 每次 deletion 扫描 `ruleSetComplete` 的次数、扫描元素总数；
   - `max/avg complete derivations per tuple`；
   - 触发扫描的 clause/ruleId 分布。
2) perf top / hotspot 确认时间是否耗在迭代 set/vector/hash 上。

优化方向：
- 将递归性判断尽量变为编译期常量（避免运行时 `isInRecursiveStratum(ruleId)`）
- 仅在“support count 归零/进入 overdelete 临界点”等必要时刻扫描一次
- 或维护 recursive-only 索引，避免扫描 complete 全量

---

### S4. `unordered_set.erase(RuleApplication)` 的 hash/eq 可能很重（RuleApplication 携带 vector）
现象匹配：
- deletion 路径大量 erase；
- 若 RuleApplication 的 key 里包含 `std::vector`（varValues），hash/eq 都会遍历，erase 显著变慢。

验证动作：
1) 查看 `RuleApplication` 结构与 hash/== 实现；
2) perf 看热点是否在 hash/equality 以及 vector 访问。

可能优化：
- `varValues.reserve(N)`（N 是编译期常量）减少小 vector 分配/扩容
- 用轻量 key（如 interned id / 指纹 hash）替代“vector 作为 key”或为 vector 做更高效哈希

---

### S5. `std::unordered_map::operator[]` 隐式插入空 entry 导致多余开销
现象：
- deletion 中用 `operator[]` 读取 map，key 不存在也会插入；
- 可能造成 rehash/增长与额外分配。

验证动作：
- 搜索 `[...]` 访问模式；
- 改成 `find()` 并在不存在时跳过/断言。

---

## 5. 推荐的调试与性能定位工作流

> 建议按顺序优先处理：先证实 S1/S2（最可能数量级差），再看 S3/S4/S5。

### Step A — 建立最小可复现（Repro）与基线数据
1) 选择一个能稳定复现“deletion 比全量慢数量级”的 workload：
   - 固定数据规模、固定 update 模式（删除多少 tuple/事实）
   - 固定线程数与编译模式（建议 `RelWithDebInfo`）
2) 记录 baseline：
   - 全量重算时间（T_full）
   - 增量 deletion 时间（T_del）
   - 内存峰值（RSS peak）
   - 迭代轮次数（若有）

输出格式（建议写入 `bench_results.md`）：
- dataset: ...
- update: ...
- T_full: ...
- T_del: ...
- RSS_peak: ...
- iterations: ...
- commit hash: ...

### Step B — 插桩（最低侵入）
添加可开关宏，例如 `#ifdef INC_DEBUG`：
- Clear 时输出：relationName/isTemp/size before/size after
- RecordDerivation/DeltaUnion 时输出：
  - 新分配 set 次数、set size 分布
  - 扫描 complete derivations 的次数与扫描元素总数
- 输出到 stderr 或独立 log 文件（避免影响主输出）

### Step C — Leak/Heap 检查（必须做）
1) ASan/LSan 跑最小 repro：
   - 若有泄漏：定位 call stack，优先修复 S2
2) heap profiler 跑较大 repro：
   - 观察 set/vector/tuple 分配是否随迭代增长

### Step D — CPU Profiling（perf）
1) `perf record` / `perf report`：
   - 看热点是否落在：
     - unordered_set hash/eq
     - vector 分配/复制
     - map rehash
     - purge/scan loops
2) 输出火焰图或热点函数列表，写入 `profile_notes.md`

### Step E — 针对性修复与验证
按 S1 -> S2 -> S3 -> S4 -> S5 的顺序修复，每个修复后：
- 重新跑 Step A 基线，比较：
  - T_del 是否下降
  - RSS_peak 是否下降
  - 与 T_full 的比值是否改善
- 确保正确性不回归（已有测试/或对比输出）

---

## 6. 建议的交付物

1) `analysis_report.md`
   - 问题复现方式与基线数据
   - 主要瓶颈的证据（log/profile/asan）
   - 修复策略与理由
2) Patch / PR
   - 至少包含 S1 或 S2 中的一项可验证修复（优先 S2 泄漏）
   - 插桩宏（可选，但建议保留）
3) `bench_results.md`
   - 修复前后对比数据（至少 3 次重复取均值/方差）

---

## 7. 常见陷阱与注意事项

- 插桩日志本身会影响性能；对“数量级差”的定位通常足够，但最终基准测试请关闭 debug 宏。
- 如果发现某些 relation 不 purge（S1），不要粗暴对所有 non-temp purge；必须做白名单或从上游标记为 temp / 使用 ExactClear。
- 修复 S2 时注意指针接管：有些 delta set 被“直接赋给 complete set”，不能再 delete。
  - 推荐使用 `unique_ptr` + move 语义避免双重释放。
- 递归层的 deletion 往往比 insertion 更复杂：先保证内存与 Clear 语义正确，再做算法层优化。

---

## 8. 需要的源码文件列表（若缺失，请补齐）

为了彻底定位 S2/S4，还需要查看：
- `DerivationManager` 定义（map 类型、value 是否裸指针）
- `RuleApplication`、`UntypedTuple` 结构以及 hash/== 实现
- RAM 生成阶段对增量辅助 relation 的 temp 标记逻辑（或 ExactClear 的生成逻辑）

---

## 9. 完成标准（Definition of Done）

- 正确性：deletion 增量结果与全量重算一致（对齐现有正确性断言/回归用例）
- 性能：在同一 workload 下，T_del 不再比 T_full 慢一个数量级
- 内存：LSan 不再报告明显泄漏；RSS 不随迭代异常增长
- 文档：analysis_report + bench_results 完整可复现
