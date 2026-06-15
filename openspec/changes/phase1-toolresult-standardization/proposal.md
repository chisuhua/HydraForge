# Phase 1 入口：ToolResult 标准化（P1-P4）

> **关联**: `.omo/decisions/phase1-entry.md` (首选入口)
> **ADR**: [ADR-0023-tool-result-standard.md](../../docs/adr/adr-0023-tool-result-standard.md)
> **上游依赖**: Phase 0 收官 (C₁ → X → B → A) ✅

## Why

HydraForge 当前工具调用结果格式**全线不一致**：

| 组件 | 当前返回格式 | 问题 |
|------|------------|------|
| `ToolRegistry::call_tool()` | `{"result": 42}` 或 `{"error": "..."}` | 每个工具格式不同 |
| `NodeExecutor::execute_tool_call()` | `if(result.is_object())` 启发式分支 | 脆弱，无法判断成功/失败 |
| `registry.cpp` 默认工具 | `{"results": "..."}`, `{"result": 42}`, `{"location": "..."}` | 各用各的顶层 key |
| ADR-0021 `RETURN_SUCCESS` | 格式未定义 | PDK 无输出合约 |
| ADR-0019 `Event.content` | `std::string` | 结构化数据丢失 |

Phase 0 X 阶段已交付 ToolResult MVP（24/24 测试通过），但只覆盖 P1。本 change 扩展 P2-P4：
- **P2**: 工具错误码分类（RETRY/SKIP/ABORT）
- **P3**: 工具调用元数据（latency, trace_id, caller）
- **P4**: 与 IInteractionBus 结构化推送集成

不解决此问题，PDK (ADR-0021) 和 PluginLoading (ADR-0022) 都无法启动。

## What Changes

- **扩展 ToolResult 字段**：添加 `error_code` (enum), `latency_ms` (uint64), `trace_id` (string), `metadata` (json)
- **扩展 NodeExecutor 解析**：替换 `if(result.is_object())` 启发式为类型化分发
- **扩展 IInteractionBus 推送**：从 `std::string content` 升级为 `ToolResult payload`
- **新增 5+ 集成测试**：覆盖错误码识别、元数据透传、结构化推送

**Non-goals**:
- ❌ 不实现 PDK（独立 change）
- ❌ 不实现 PluginLoader（独立 change）
- ❌ 不修改 ToolResult MVP 现有 24 个测试

## Impact

- **Affected specs**:
  - `docs/specs/architecture.md` (ToolRegistry 章节需更新)
  - `docs/specs/layer0.md` (§21 NodeExecutor)
- **Affected code**:
  - `src/core/types/tool_result.{h,cpp}` — 扩展字段
  - `src/modules/executor/node_executor.{h,cpp}` — 替换启发式分支
  - `src/common/contract/inmemory_bus.{h,cpp}` — 推送类型升级
  - `tests/test_tool_result.cpp` — 新增 5+ 测试
  - `tests/test_executor_with_mock_provider.cpp` — 验证集成

## Success Criteria

- [ ] ToolResult P1-P4 字段全部实现
- [ ] NodeExecutor 移除所有 `if(result.is_object())` 启发式
- [ ] IInteractionBus 支持结构化 `ToolResult` 推送
- [ ] 全量 25 + 5+ = 30+ 测试通过
- [ ] TSan + ASan 干净
- [ ] CI 6 个 job 全绿
- [ ] 端到端 demo: `examples/phase1_plugin_demo` 骨架可运行
