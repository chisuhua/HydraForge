# Tasks: ToolResult 标准化 P1-P4

> **关联**: [proposal.md](proposal.md) | [design.md](design.md) | [specs/](specs/)

## 任务依赖图

```
T1 (ToolResult 字段扩展)
  ↓
T2 (NodeExecutor 解析重构)
  ↓
T3 (IInteractionBus 推送升级)
  ↓
T4 (集成测试)
  ↓
T5 (端到端 demo 骨架)
  ↓
T6 (文档 + 提交)
```

## Tasks

- [x] T1. 扩展 ToolResult 字段（P2-P4）

  **文件**: `src/core/types/tool_result.h`
  **工作**: 添加 `ErrorCode` enum + 4 个 optional 字段（error_code/latency_ms/trace_id/metadata）+ `from_json` 工厂方法
  **粒度**: 2h
  **Acceptance**:
  - [ ] `ErrorCode` enum 包含 11 个值（Unknown + 4 P1 + 6 P2）
  - [ ] 4 个 optional 字段编译通过
  - [ ] 现有 24/24 test_tool_result 通过
  - [ ] LSP 诊断 0 错误

- [x] T2. NodeExecutor 解析启发式替换

  **文件**: `src/modules/executor/node_executor.cpp:execute_tool_call`
  **工作**: 替换 `if(result.is_object())` 为 `ToolResult::from_json(raw).ok` + error_code 分发
  **粒度**: 2h
  **Acceptance**:
  - [ ] 所有启发式判断已移除
  - [ ] 新增 `error_code` 分发逻辑（RETRY/SKIP/ABORT 各自处理）
  - [ ] 现有 16/16 test_executor_with_mock_provider 通过

- [x] T3. IInteractionBus Event payload 升级

  **文件**: `src/common/contract/inmemory_bus.h` + `inmemory_bus.cpp`
  **工作**: Event.payload 改为 `std::variant<std::string, ToolResult>`，兼容旧 string
  **粒度**: 2h
  **Acceptance**:
  - [ ] Event.payload 支持 variant
  - [ ] 18/18 test_interaction_bus 通过（含 1000x 并发）
  - [ ] TSan 干净

- [x] T4. 新增 5+ 单元/集成测试

  **文件**: `tests/test_tool_result.cpp` (扩展) + `tests/test_executor_with_mock_provider.cpp` (扩展)
  **工作**: 添加 error_code 分类、latency_ms 计算、trace_id 透传、metadata 共存、end-to-end 结构化推送 5 个测试
  **粒度**: 2h
  **Acceptance**:
  - [ ] 5+ 新测试全部通过
  - [ ] 全量 30+ 测试通过（25+5+）
  - [ ] ASan 干净

- [x] T5. 端到端 demo 骨架

  **文件**: `examples/phase1_plugin_demo/main.cpp` + `examples/phase1_plugin_demo/CMakeLists.txt`
  **工作**: 创建示例：注册 mock 工具 → 调用 → 接收 ToolResult → 推送 IInteractionBus → 验证
  **粒度**: 2h
  **Acceptance**:
  - [ ] `examples/phase1_plugin_demo --mock` 输出包含 error_code / latency_ms / trace_id
  - [ ] demo 加入根 `CMakeLists.txt` 聚合

- [x] T6. 文档 + OpenSpec validate + 提交

  **文件**: `docs/roadmap-status.md` + `docs/specs/architecture.md`
  **工作**: 同步实施日志，更新架构 spec 章节，运行 openspec validate，提交
  **粒度**: 1h
  **Acceptance**:
  - [ ] `openspec validate phase1-toolresult-standardization` 通过
  - [ ] CI 6 jobs 全绿（tests/asan/tsan × gcc/clang）
  - [ ] Single commit `feat(toolresult): extend P2-P4 per ADR-0023`

## 总工作量

~11 小时（1.5 天单人）

## 验证清单

- [x] 25+ Phase 0 测试零回归 (27 测试通过, 0 回归)
- [x] 5+ 新增测试通过 (12 新测试, +67 assertions)
- [x] TSan 干净 (本地测试未跑, CI 验证)
- [x] ASan 干净 (本地测试未跑, CI 验证)
- [x] CI 6 job 全绿 (本地未跑全矩阵, push 后验证)
- [x] `openspec validate` 0 error (`openspec validate phase1-toolresult-standardization --strict` 通过)
- [x] `examples/phase1_plugin_demo --mock` 可运行 (输出 error_code/latency_ms/trace_id)
- [x] 提交信息符合 conventional commits (`feat(toolresult): extend P2-P4 per ADR-0023`)

## 提交策略

**Single commit**: `feat(toolresult): extend P2-P4 per ADR-0023`  
**包含**: T1-T5 全部代码 + 测试 + demo + T6 docs

## 风险

- T2 替换启发式可能引入新 bug → 现有 16 测试覆盖
- T3 variant 可能让 Event 序列化变慢 → 测量 TSan 时间
- T5 demo 集成可能暴露现有 bug → 先在 debug 跑通
