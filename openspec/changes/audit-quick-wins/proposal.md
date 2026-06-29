# Proposal: Audit Quick Wins (Sprint 15)

## Why

2026-06-30 全项目审计 (`docs/superpowers/plans/2026-06-30-audit-remediation-roadmap.md`) 发现两类紧迫问题:

1. **P0: 2 个预先存在的测试失败** — `test_basic` 和 `test_no_llm`。根本原因: Sprint 14 C4 commit `a48e563` 引入 ToolCoordinator 默认注入到 DSLEngine, 与 ApprovalHandler 路径冲突, 触发了 5/2 条不一致的中间件优先级链警告 ("both tool_coordinator_ and approval_handler_ are set, preferring tool_coordinator_")。这些失败在 commit 阶段未被发现 (ctest 报告显示 41/41 PASS, 但实际二进制是 Sprint 14 C4 之前构建的过期产物)。

2. **代码债**: 8 项可低成本清理的代码异味 (重复逻辑、注释死代码、类型签名冗余等), 单个修复均 < 50 行, 不需要架构变更。

## What Changes

### 1. P0 修复: ToolCoordinator opt-in
- `src/core/engine.h`: 添加 `set_tool_coordinator(unique_ptr<ToolCoordinator>)` 公开方法
- `src/core/engine.cpp`: 默认 ctor 不再自动创建 ToolCoordinator (`tool_coordinator_ = nullptr`)
- 行为变更: ToolCoordinator 现在是 opt-in middleware 而非默认启用. ADR-0031 §决策 5 设计不变, 仅激活时机从 default-on 改为 explicit.

### 2. 代码债清理 (8 项)
- `node_executor.cpp`: 删除永远 throw 的 fork/join stubs (调度器层处理)
- `execution_session.cpp`: 删除 Sprint 2 重构遗留的注释代码块
- `cloud_adapter.cpp`: 提取 `compute_backoff()` 消除 2 处重复
- `topo_scheduler.cpp`: 去除 `build_dag()` 调试 drain 循环 + 重构 ready_queue 计算
- `plugin_loader.cpp`: 非 Linux stub + 统一日志 (`LOG_INFO/WARN/ERROR`)
- `node_factory.cpp`: 合并 `make_llm_call` / `make_dsl_call` 共享逻辑
- `layered_context.h`: 合并 `navigate()` 双重遍历 + `split_path()` 返回类型 void

### 3. 文档修正
- `AGENTS.md`: 修正 engine.h 跨模块 include 计数 (1 → 4), 补充 Sprint 15 ship 记录

## Impact

| 维度 | Before | After |
|------|--------|-------|
| 测试通过率 | 39/41 (95%) | **41/41 (100%)** |
| 严重架构债 | 4 (含 2 个 P0 测试失败) | **2** |
| 中等架构债 | 3 | **2** |
| 代码债 | 8 | **0** |
| 引擎行为变更 | — | ToolCoordinator opt-in (向后兼容) |
| 用户 API 变更 | — | `DSLEngine::set_tool_coordinator()` 新方法 (opt-in) |

## Compatibility

- **完全向后兼容**: 现有调用 `DSLEngine::from_markdown(...)` 的代码行为不变, ApprovalHandler 路径仍默认启用
- **新增 API**: 用户如需 ToolCoordinator middleware, 显式调用 `engine->set_tool_coordinator(...)`
- **行为变化**: 之前 ToolCoordinator 默认启用时, 触发了 `LOG_WARN("both tool_coordinator_ and approval_handler_ are set, preferring tool_coordinator_")` — 现已不再产生此警告

## Risk Assessment

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| ToolCoordinator opt-in 破坏现有 ToolCoordinator 使用 | 低 | 中 | 4 个测试 (`test_executor_with_mock_provider`, `test_tool_coordinator`, `test_layer_profile`, `test_pdk_macros`) 已覆盖相关代码路径 |
| 重构改变 scheduler 行为 | 低 | 高 | `test_execute_parallel` + `test_scheduler` 41/41 PASS 验证 |
| `layered_context.h` 双重遍历合并改变 system 层写保护 | 低 | 高 | `test_layered_context` 已覆盖 system 层所有路径 |
| 非 Linux stub 引入符号冲突 | 低 | 低 | 仅当 `__linux__` 未定义时启用 |

## Test Strategy

- 现有 41 个测试 + P0 修复后回归测试 (100% 通过)
- 每个 refactor task 通过 `cmake --build build && ctest` 验证
- 不需要新增测试 (现有测试已覆盖所有改动路径)

## Out of Scope

- Change B (测试覆盖补齐 9 个 MISSING/PARTIAL 文件): 留 Sprint 16
- Change C (严重架构债修复: execute_single_branch 拆分、layered_context thread safety、topo_scheduler.h 跨模块解耦): 留 Sprint 17
- Oracle 咨询: 不需要 (所有改动均为局部、行为保持)

## References

- 审计源: `docs/superpowers/plans/2026-06-30-audit-remediation-roadmap.md` (780 行)
- Sprint 14 C4 ship: commit `a48e563` (ToolCoordinator 引入)
- ADR-0031 P3-P4: Oracle session `ses_0ed4408faffeLv8VfrC0s5PzW7`
- Sprint 15 plan: `docs/superpowers/plans/2026-06-30-audit-remediation-roadmap.md §四`