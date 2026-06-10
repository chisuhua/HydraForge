# 技术债与文档清理修复 — 任务清单

> **关联审计**: [2026-06-09 综合审计报告](../../../../docs/audits/2026-06-09-tech-debt-and-doc-cleanup.md) (36 个问题)
> **关联 proposal**: `proposal.md`
> **关联 design**: `design.md`
> **关联 spec**: `specs/tech-debt-cleanup/spec.md`
> **执行人**: TBD | **创建日期**: 2026-06-09 | **状态**: 计划中
> **粒度**: 每任务约 1-2 小时,符合 `openspec/config.yaml` 规则

---

## 阶段 1: P0 一键修复 (30 分钟) — 提交 #1

- [x] 1.1 修复 7 处 `AgenticOS_*` 死链 (层0 + 层0-refactor + layer0-refactor.md 3 处 + layer0.md 4 处)
  - 验证: `grep -rn "AgenticOS_Layer0_Spec\|AgenticOS_Architecture\|AgenticOS_Layer0_RefactoringPlan" docs/` 0 命中
  - 关联: REQ-dead-link-fixes
  - `docs/specs/layer0-refactor.md:6,9` `AgenticOS_Layer0_Spec.md` → `layer0.md`
  - `docs/specs/layer0-refactor.md:1595` `AgenticOS_Architecture.md` → `architecture.md`
  - `docs/specs/layer0.md:672` `AgenticOS_Layer0_RefactoringPlan.md` → `layer0-refactor.md`
  - 验证: `grep -rn "AgenticOS_Layer0_Spec\|AgenticOS_Architecture\|AgenticOS_Layer0_RefactoringPlan" docs/` 返回 0 命中
  - 关联: REQ-dead-link-fixes
- [x] 1.2 修复 ADR-0001 日期字面量 `(YYYY-MM-DD)` → `2026-05-28`
  - 验证: `grep "YYYY-MM-DD" docs/adr/` 返回 0 命中 ✓
  - 关联: REQ-adr-marked-deprecated
- [x] 1.3 修复 ADR-0020 README 状态 `🔄 部分实施` → `❌ 未实施 (SimpleCognitiveOrchestrator 有,WorkerPool 无)`
  - 关联: 文档诚实标注
- [x] 1.4 删除 3 个死文件:
  - `src/modules/prompts.yaml` (24 KB)
  - `tests/test_prompt_builder.cpp` (8 行 stub)
  - `examples/agent_loop/tmp.md` (2.6 KB)
  - 验证: `ctest` 19/19 通过(20→19,因删除1个stub测试)
  - 关联: P1-1
- [x] 1.5 修复 4 处 OpenSpec 链接路径 (指向 archive/):
  - `docs/SPECS-ALIGNMENT.md:113`
  - `docs/roadmap-status.md:262`
  - `docs/adr/adr-0029.md:6`
  - `docs/adr/adr-0035.md:6`
  - 全部改为 `archive/2026-06-09-docs-code-alignment-fixes/`
  - 验证: 路径可解析 ✓
  - 关联: REQ-dead-link-fixes
- [x] 1.6 修复 ADR 相对路径错误 (`../adr-0030` → `./adr-0030`):
  - 仅1处: `adr-0015-iper-loop.md:89` (其他7处实际正确使用 `./adr-`)
  - 验证: `grep "../adr-0030[^/]" docs/adr/` 0 命中 ✓
  - 关联: REQ-dead-link-fixes

## 阶段 2: P0 代码重构 (1-2 天) — 提交 #2-3

- [ ] 2.1 新增 `src/common/log/log.h` 自实现日志门面:
  - `LOG_DEBUG/INFO/WARN/ERROR(level, fmt, ...)` 宏
  - 默认输出到 stderr (避免污染 stdout)
  - CMake `SPDLOG_ACTIVE_LEVEL` 风格条件编译: release 剥除 DEBUG
  - 关联: REQ-unified-logging-facade
- [ ] 2.2 替换 `topo_scheduler.cpp` 中 15 处 `std::cout << "[DEBUG]..."` → `LOG_DEBUG(...)`
  - 验证: `grep "std::cout" src/modules/scheduler/topo_scheduler.cpp` 0 命中
  - 关联: REQ-unified-logging-facade
- [ ] 2.3 替换其他 4 文件中的 std::cout/std::cerr:
  - `markdown_parser.cpp:187,422`
  - `node_executor.cpp:304`
  - `context_engine.cpp:151`
  - `engine.cpp:105`
  - 关联: REQ-unified-logging-facade
- [ ] 2.4 重写 `HttpLLMAdapter` 继承 `ILLMProvider`:
  - `src/common/llm/http_adapter.h/.cpp` 改为继承 `ILLMProvider` 而非 `ILLMAdapter`
  - 同步迁移 `llama_adapter.cpp:19` 实例化点
  - **BREAKING CHANGE**: 需在 commit message 显式标注
  - 验证: `grep "ILLMAdapter" src/` 0 命中(仅 `src/common/llm/llm_adapter.h` 自身)
  - 关联: REQ-no-deprecated-base-class
- [ ] 2.5 删除 `src/common/llm/llm_adapter.h`:
  - 验证: 全项目编译通过,`grep -rn "include.*llm_adapter.h" src/ tests/ examples/` 0 命中
  - 关联: REQ-no-deprecated-base-class

## 阶段 3: P1 ADR 批量废弃 (30 分钟) — 提交 #4

- [x] 3.1 ADR-0010 头部加 ⛔ 横幅:
  - 格式: `> ⛔ 已废弃 (2026-06-09) — 详见 OpenSpec change tech-debt-and-doc-cleanup`
  - 关联: REQ-adr-marked-deprecated
- [x] 3.2 同样处理 ADR-0011/0012/0013/0014/0015/0016/0017/0018 (8 个)
  - 验证: `grep "⛔ 已废弃" docs/adr/adr-001[0-8]*.md | wc -l` ≥ 9
- [x] 3.3 同样处理 ADR-0030/0032/0034/0036 (4 个)
  - 验证: `grep "⛔ 已废弃" docs/adr/adr-003[0246].md | wc -l` ≥ 4
- [x] 3.4 同步 11 个 ADR 文档"##状态"节内容与 README 表格
  - ADR-0007 状态节: `已批准` → `🟡 部分实施`
  - ADR-0010~0018 状态节: 改为"代码未实施 (规划中)"
  - 验证: `grep -l "## 状态" docs/adr/adr-001[0-8]*.md` 11 文件

## 阶段 4: P1 实施 (2-3 天) — 提交 #5-7

- [ ] 4.1 实施 `CostCollector` 集成到 `BudgetController`:
  - `src/modules/budget/budget_controller.h` 新增 `cost_tracker` 子结构
  - 字段: `total_cost_usd`, `tokens_consumed`, `last_call_cost_usd`
  - 方法: `record_llm_call(tokens, model)`, `get_total_cost_usd()`, `reset()`
  - 关联: REQ-cost-tracker-integration
- [ ] 4.2 在 `call_llm_tool` 路径累积 cost:
  - `src/common/tools/registry.cpp` `call_llm_tool` 成功后调用 `record_llm_call`
  - 关联: REQ-cost-tracker-integration
- [ ] 4.3 暴露 `engine.get_session_cost()` API:
  - `src/core/engine.h` 新增方法
  - 关联: REQ-cost-tracker-integration
- [ ] 4.4 新建 `tests/test_cost_collector.cpp`:
  - 至少 3 个 TEST_CASE: 单次调用累积 / 多次调用累加 / reset 后清零
  - 关联: REQ-cost-tracker-integration
- [ ] 4.5 实施 `PlanModePolicy`:
  - `src/modules/policy/plan_mode_policy.h/.cpp` 实现 `IExecutionPolicy` 8 个方法
  - 行为: `requires_approval=true`, `should_auto_execute=false`, 等
  - 关联: REQ-execution-policy-implementations
- [ ] 4.6 实施 `AgentModePolicy`:
  - 行为: 平衡模式, 关键操作需 approval
- [ ] 4.7 实施 `YoloModePolicy`:
  - 行为: 全自动, 几乎不需 approval
- [ ] 4.8 新建 `tests/test_execution_policy.cpp`:
  - 3 个 TEST_CASE (每个 policy 一组)
  - 关联: REQ-execution-policy-implementations
- [ ] 4.9 清理 6 个 lib/ 孤儿子图:
  - 删除 `lib/auth/verify_session.md`
  - 删除 `lib/human/clarify_input.md`
  - 删除 `lib/human/confirm_action.md`
  - 删除 `lib/inference/engine.md`
  - 删除 `lib/inference/model.md`
  - 保留 `lib/inference/session.md` (被 docs 引用)
  - 实施前 grep 验证无引用
  - 验证: 20/20 测试通过
  - 关联: REQ-lib-stdlib-orphan-cleanup

## 阶段 5: 测试补充 + 收尾 (1-2 天) — 提交 #8-9

- [ ] 5.1 新建 `tests/test_call_llm_tool.cpp`:
  - 验证 `kDefaults{}` sentinel 修复:用户显式传 512 真正生效
  - 验证: 用户省略 max_tokens 保留 default_params 值
  - 响应 commit 提示 "Untested: call_llm_tool"
  - 关联: REQ-call-llm-tool-test-coverage
- [x] 5.2 在 `req1.md` 5 处 `LLMCallNode` 引用加归档横幅:
  - line 2151, 2388, 2439, 2496, 2514
  - 格式: `<!-- ARCHIVED: superseded by DSLNode (v3.10) -->`
  - 验证: `grep -c "ARCHIVED: superseded by DSLNode" src/modules/exports/req1.md` ≥ 5
  - 关联: REQ-req1-md-llmcallnode-banner
- [x] 5.3 清理 5 处注释掉的死代码:
  - `src/modules/scheduler/execution_session.cpp:5` 注释掉的 `#include`
  - `src/modules/executor/node_executor.cpp:259,261` 注释掉的 `PromptBuilder` 调用
  - `src/common/utils/yaml_json.cpp:46,72,75` 注释掉的 `std::cerr`
  - `src/modules/library/library_loader.cpp:78` 注释掉的警告
  - 验证: `grep "// std::cerr\|// #include" src/` 0 命中(注释掉的)
- [x] 5.4 修复 `roadmap-status.md:255` 措辞:
  - `跨 Phase 活跃变更` → `最近完成的 OpenSpec 变更`
- [x] 5.5 修复 `app-dev-guide.md:6` 时间戳:
  - `2025年11月10日` → `2026-06-09`
  - 同时检查 `app-dev-guide.md:473` API key 示例 `xxx` → `<your-api-key>`
- [x] 5.6 同步 `docs/audits/` 目录到 git (已在 commit ac9e684 中完成)
  - `git add docs/audits/`
  - 关联: 持久化审计工件
- [x] 5.7 修复 5 处 `TBD` 占位符:
  - `docs/SPECS-ALIGNMENT.md:76-80` 5 处 `TBD` → `TBA` 或指派具体负责人

## 阶段 6: 验收 (收尾)

- [ ] 6.1 LSP 诊断: 修改的 .cpp/.h 文件无新增诊断(预存在错误不计)
- [ ] 6.2 全量测试: `ctest --output-on-failure` 至少 23/23 通过 (20 + 3 新增)
- [ ] 6.3 OpenSpec 状态: `openspec status --change tech-debt-and-doc-cleanup` 显示 `isComplete: true`
- [ ] 6.4 OpenSpec validate: `is valid`

---

## 执行顺序建议

```
Day 1:
  1.1-1.6 (一键) → 3.1-3.4 (ADR 批量废弃) → 5.4-5.7 (小修)

Day 2-3:
  2.1-2.5 (日志门面 + HttpLLMAdapter 迁移)

Day 4-5:
  4.1-4.9 (CostCollector + ExecutionPolicy + lib 清理)

Day 6:
  5.1-5.3 (测试补充 + 注释清理)

Day 7:
  6.1-6.4 (验收)
```

## 总计

- **6 大阶段 / 38 子任务**
- **P0**: 12 项 (1.x 一键 + 2.x 重构)
- **P1**: 13 项 (3.x 废弃 + 4.x 实施)
- **P2**: 9 项 (5.x 测试 + 注释 + 收尾)
- **验收**: 4 项 (6.x)
- **估时**: 5-7 工作日
- **新增测试**: 3 个 (test_call_llm_tool / test_cost_collector / test_execution_policy)
- **测试总数**: 20 → 23
- **删除文件**: 4 个 (prompts.yaml, test_prompt_builder.cpp, tmp.md, llm_adapter.h)
- **删除子图**: 5 个 lib/ 孤儿
- **废弃 ADR**: 13 个 (0010-0018, 0030/0032/0034/0036)
