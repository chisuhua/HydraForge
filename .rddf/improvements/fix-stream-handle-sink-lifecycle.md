# fix-stream-handle-sink-lifecycle

**优先级**: P2 | **来源**: dual-agent 调研 (2026-10-14) — Oracle C2 sink pointer UAF 风险
**阶段**: post-6c | **分类**: execution-runtime
**类型**: bugfix
**主题**: NodeExecutor 持有 stream_sink_ raw pointer, sink owner 未明确定义, 可能 UAF

## 架构依据

per 2026-10-14 dual-agent 调研 (`ses_ec5f38a92ffeLuZb4aIXmev0j3`):
- **V1 ship (2026-09-04)**: `node_executor.h:82,108` `set_stream_sink()` 注入点 (raw pointer, 与 `set_tool_coordinator` 模式一致)
- **Oracle C2 风险**: sink 的 owner 谁负责销毁?
  - 若 sink 注入方 (caller) 析构 sink 后 NodeExecutor 仍持 dangling pointer → 下次执行 UAF
  - 与 ADR-0001 / 现有 ToolCoordinator raw pointer 模式同问题, 但**未文档化 owner 契约**
- **提案 D5 验收**: "handle 析构 RAII 顺序" 只覆盖 handle 自身, 不覆盖 NodeExecutor 持有的 sink 指针

## Why

### 当前 ship 行为 (无 owner 契约)

```cpp
// node_executor.h:82
void set_stream_sink(IStreamHandle* sink);  // raw pointer, no owner transfer

// node_executor.cpp (V1 ship)
class NodeExecutor {
  IStreamHandle* stream_sink_ = nullptr;  // raw pointer, lifetime unmanaged

public:
  void execute(...) {
    if (stream_sink_ != nullptr) {
      // use stream_sink_  <-- 若 caller 已析构 sink, UAF
    }
  }
};
```

```cpp
// caller (per 提案示例, 假设)
{
  auto sink = make_unique<MyStreamSink>();
  executor.set_stream_sink(sink.get());  // raw pointer 注入
  executor.execute(...);  // OK
  // sink 析构
  executor.execute(...);  // ⚠️ stream_sink_ 仍指向已析构 sink → UAF
}
```

### 真实风险场景

1. **Test fixture 提前析构**: test 在 fixture 析构后调 executor → UAF
2. **多 executor 共享 sink**: sink 生命周期短于 executor → UAF
3. **异常路径**: `executor.execute()` 抛异常, sink 提前释放, 下次 execute UAF
4. **shutdown 顺序**: executor 先析构但 sink 仍存活 → sink 调用已析构 executor → use-after-free

## What Changes (proposal scope)

- **修复 1**: 文档化 sink 契约: "sink 生命周期 ≥ executor 生命周期, 否则 UAF"
  - 同 ADR-0001 ToolCoordinator raw pointer 模式的同问题先例
- **修复 2 (推荐)**: 改 `IStreamHandle*` → `shared_ptr<IStreamHandle>` 或 `weak_ptr<IStreamHandle>`, 让 RAII 自动管理
  - 接口 breaking change, 但消除 UAF 风险
- **修复 3 (备选)**: 新增 `clear_stream_sink()` 方法, executor 析构前显式 clear
  - 不改接口, 责任在 caller
- **修复 4**: 新增 RAII guard `StreamSinkGuard(executor, sink)`, 析构时自动 clear
  - 库级解决方案, 不改 executor 接口

## Out of Scope

- ToolCoordinator 同步问题 (per ADR-0001 已知, 本 fix 仅针对 stream_sink_)
- V2 完整流式 (per `adr-0072-d1-stream-v2-true-streaming.md`)
- IStreamHandle 内部状态管理 (已 RAII 安全)

## Dependencies

- **前置 ship**: `adr-0072-d1-stream-runtime-semantics` 已 ship (V1, 2026-09-04)

## Acceptance Criteria (建议)

- AC-1: 文档化 sink 契约 (修复 1): "lifetime ≥ executor lifetime" 注释 + dsl.md spec
- AC-2: 选修复 2 或修复 3 + 4, 在 `node_executor.h` 实施
- AC-3: 单元测试覆盖 sink 提前析构场景 (用 weak_ptr 验证不 crash, 或 set+clear+execute)
- AC-4: 既有 `test_stream_runtime.cpp` 19 cases 零回归
- AC-5: ASan 全量 0 新增 error (尤其 UAF detection)

## Estimated Effort

- 修复 1: 0.5h (注释)
- 修复 2 (推荐): 4-6h (接口改 + 所有调用方适配 + 测试)
- 修复 3 (备选): 1-2h (新方法)
- 修复 4 (备选): 1-2h (RAII guard)
- 总: 0.5h (仅修 1) / 0.5-1d (修 2-4 组合)

## Reference

- `src/modules/executor/node_executor.h:82,108` (V1 sink 注入)
- `src/common/tools/tool_coordinator.h` (类似 raw pointer 模式, 先例)
- ADR-0001 (raw pointer 治理)
- Dual-agent 调研: `.rddf/audits/2026-10-14-p0-proposals-dual-agent-review.md`

## Status

📋 **登记但未立项** (per 2026-10-14 dual-agent 调研, Oracle C2 UAF 风险发现). 待独立 OpenSpec change 立项实施.
