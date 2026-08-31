# Stage 4 Round 1: Metis 7-plan 综合审计报告

> **生成时间**: 2026-09-01
> **来源**: Metis session `ses_fa656d8b1ffe2ZY9pcTfsJYD3X`
> **范围**: 7 个实施计划 (T1-T7) 的元层面 + 跨 plan 一致性 + ship 可行性
> **状态**: Stage 4 Round 1 — Oracle T7 Round 1 (bg_3c39ebb1) 已完成 + 其他 6 Oracle 因 kimi-code 配额失败, Metis 综合审计替代

---

## 1. 跨 plan 一致性矩阵

| 跨 plan 冲突 | 严重度 | 涉及 plan | 修正 |
|--------------|--------|-----------|------|
| **🔴 MCTSWorkflowSearch ctor overload 重复定义** — T6 Step 3 与 T2 Step 3 均加同一 ctor `(evaluator, governor, regression_gate, SearchConfig, IInteractionBus*, IBudgetController*)` | 🔴 | T2/T6 | 二选一: T6 加 ctor, T2 用同一 ctor 不重新声明; 或 T2 加, T6 改用变体 |
| **🔴 Tool 命名契约断裂 T5↔T1** — T5 注册 `evolution::reflect/search/compile`, T1 hardcode `cognitive::*`; pipeline (materialize→execute) 运行时报 tool not found | 🔴 | T1/T5 | 统一命名: T1 改 `evolution::*` (与 T5 主路径对齐), 或 T5 改 `cognitive::*` 主路径 (架构组裁决) |
| **🔴 ADR-0068 版本号与 ship 顺序不一致** — current Appendix A v1.7; T3 (ship 1st) 声称 v1.9+; T6 (ship 2nd) 声称 v2.0+; T2 (ship 3rd) 声称 v1.8 — out of order | 🔴 | T1/T2/T3/T6 | 改用 change-name tag 而非 numeric version, 或 ship 时按顺序分配 |
| **🟠 Include 路径系统错误** (5+ plans) — T3/T6 测试用 `agenticdsl/contract/ibudget_controller.h` (不存在, 真实 `budget/budget_controller.h`) | 🟠 | T3/T6/T1/T4 | 改用真实路径, 配套 ship-gate grep |
| **🟠 ctest baseline 数字 204 错误** — 实际 build/tests add_test = 348, TEST_CASE repo = 1281; 7 个 plan 用 204→235 链 arithmetic 一致但 base 错 | 🟠 | 全部 7 | 删除硬编码数字, 改用 `ctest --test-dir build -N | grep "Total Tests"` 动态读 |
| **🟠 tests/CMakeLists.txt 编辑不必要且有风险** — tests 用 `file(GLOB test_*.cpp)` 自动注册 (line 170), 6 个 code plan 在 commit 列表里加 `tests/CMakeLists.txt` 无必要 | 🟠 | T3/T6/T2/T5/T1/T4 | 删除 commit 列表中 `tests/CMakeLists.txt`, 新 test file 自动注册 |
| **🟠 T2 MutationContext 虚构字段** — T2 用 `.subject_version/.parent_version/.resource_cost`, 实际 struct 只有 mutation_id/source_id/mutation_kind/subject_ref/proposed_change/parent_ref/version_id/mode/evaluation_refs | 🟠 | T2 | 改为 `subject_ref/version_id/proposed_change` |

## 2. T7-specific 已确认问题 (Oracle Round 1 bg_3c39ebb1)

| 问题 | 严重度 | 状态 |
|------|--------|------|
| §十八 已在 v1.5 ship (doc line 904+); T7 plan 是 "从零新增" 写 | 🔴 | T7 plan Stage 4 应改为 "verification/lock-in" 而非 "implementation"; Step 1 Red test 实际会立即 PASS |
| Plan §九 命令 #18-#22 与 doc 实际 §九 #18-#22 编号冲突 (doc 已存在 v1.4/v1.5 的 #18-#22) | 🔴 | T7 plan §九 改 "验证已存在 #18-#22 锁定" |
| Plan §18.1 表格 4 列 vs doc 实际 5 列 (模式/性质/原语锚点(含文件:行)/应用代号/落地状态) | 🔴 | T7 plan §18.1 改为引用 doc 实际 §18.1, 不重写 |
| proposal.md "零 ctest 影响: 不触碰 include/src/tests/examples 任何代码" (line 116) vs plan Step 4 新增 examples/cognitive_meta_demo/main.cpp + tests/test_cognitive_meta_demo.cpp | 🔴 | proposal 需要 scope amendment (Oracle condition #3 要求 example, 与零代码 invariant 矛盾); plan 应注明 proposal amendment |
| main.cpp hello-world 仅打印字符串, 不演示 5 模式任何之一 (README 撒谎) | 🟠 | T7 main.cpp 应写真实调用 (与 T5 cognitive_tools 集成), 或诚实降级 README 为 "编译 smoke" |
| example 命名 `cognitive_meta_demo` 含 "meta" 与 ADR-0085 §决策 5 "V1 不实施 Meta-Agent" 联想风险 | 🟡 | T7 plan 改名 `cognitive_coordination_demo` |
| `std::getenv(...) ?: "build"` C++ 语法错 (`?:` 是 PHP 运算符) | 🔴 | ✅ 已修 (commit 9b0a7b0) |

## 3. 实施可行性评估

| 阶段 | 估时 | 累计 |
|------|------|------|
| T7 (纯文档) | 0.5 sprint | 0.5 |
| T3 (N1 修复) | 0.25 sprint | 0.75 |
| T6 (T3 接入方) | 0.5 sprint | 1.25 |
| T2 (Axis6 + commit) | 1.0 sprint | 2.25 |
| T5 (cognitive tools) | 0.5 sprint | 2.75 |
| T1 (Materializer) | 1.0 sprint | 3.75 |
| T4 (sigval) | 0.5 sprint | **4.25 sprint ≈ 85h solo-dev** |

**总容量** ≈ 85h solo-dev → **当前会话无法完整实施**; 建议拆 Sprint 24 W2-W3 各 0.5-1 sprint 增量, 或优先 ship N1 Blocker (T3+T6) 闭环.

## 4. Stage 5 串行 ship 关键路径决策表

| 触发 | 决策 |
|------|------|
| T3 ship 后 adr_lint 报 ADR-0068 v1.9+ 警告 (version out of order) | 接受 warning; T3 实际是 v1.8 (ship 顺序), 不强行 v1.9+; commit 时改用 change-name tag |
| T6 ctest pass 但 MCTS V1 mock evaluator dead code | ship with documented limitation (fail-closed gate exercised via unit MockBudget); V2 real evaluator upgrade follow-up |
| T2 ship 时发现 v1.0 test_mcts_workflow_search 17 cases 失败 (axis6 字段插入位置) | append LAST (default None), aggregate init backward compat; 不 rollback; 修正 plan 不动 struct 顺序 |
| T5 ship 时架构组裁决 cognitive::* 主路径 | 同步改 T1 materializer `axis6_to_tool()` switch + test case assertion; 1 commit rename |

## 5. 用户最关键应知道的 3 件事

1. **🔴 7 个 plan 跨 plan 一致性 3 个硬冲突**: MCTSWorkflowSearch ctor overload 重复定义 (T2 vs T6) + Tool 命名契约断裂 (T1 vs T5) + ADR-0068 版本号与 ship 顺序矛盾 (T3 vs T6 vs T2) — 这些必须在 ship 任何 1 个前先解决
2. **🟠 Plan 数字基础错**: ctest 204 baseline 实际 348; include 路径 5+ plan 错; 7 plan 总估时 85h solo-dev 当前会话不可完整实施
3. **🟢 可立即 ship**: T7 (纯文档, 已 ship, plan verification 角色) + T3 (N1 修复基础设施, 16 tasks, 0.25 sprint) + T6 (T3 接入方, 24 tasks, 0.5 sprint) = N1 Blocker 闭环 ~0.75 sprint; T2/T5/T1/T4 推迟 Sprint 24+ waves

---

**审计 commit**: 本报告附在 Stage 4 Round 1 元审查中, Oracle T7 Round 1 (bg_3c39ebb1) 提供 §十八 已 ship 与 proposal 零代码不变量冲突的关键发现; 其他 6 Oracle 因 kimi-code 配额失败, 由 Metis 综合审计替代.

**commit 时间**: 2026-09-01
**commit ID**: 见 `.rddf/plans/_stage-4-metis-cross-plan-audit.md`
