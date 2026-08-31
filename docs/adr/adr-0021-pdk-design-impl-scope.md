# ADR-0021 PDK Design 实施范围审计

> **生成时间**: 2026-08-31 (docs_drift_audit Scenario 4 修正)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0021-pdk-design.md](adr-0021-pdk-design.md)
> **状态**: ✅ Approved (audit 后保持)

## 状态

**📋 Audit** (impl-scope-audit 文档)

✅ Approved (audit 后保持 — Sprint 5 2026-06-24 ship, 主契约已实现)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved (2026-06-24, Sprint 5 ship), 但 3/7 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `DECLARE_TOOL` (宏) | ✅ Shipped | `include/agenticdsl/pdk/tool_macros.h` | PDK 工具注册宏 |
| `DEFINE_AGENT` (宏) | ✅ Shipped | `include/agenticdsl/pdk/agent_macros.h` | PDK Agent 注册宏 |
| `SafeExec` | ✅ Shipped | `include/agenticdsl/pdk/safe_exec.h` | PDK 沙箱封装 |
| `FakeStateStore` | 📅 Deferred (测试辅助) | — | 仅在 unit test mock 中使用, 未提取为生产类 |
| `MockSandbox` | 📅 Deferred (测试辅助) | — | 同上 |
| `StubLLM` | 📅 Deferred (测试辅助) | — | pdk_chat_demo `mock_blocking_provider.h` 提供等价能力, 未抽象为 PDK 通用类 |
| `IFeature` | 📅 Deferred | — | Phase 6 后续 |

## 分类详情

### 📅 Deferred — 测试辅助类

ADR-0021 描述了 `FakeStateStore` / `MockSandbox` / `StubLLM` 作为 PDK 集成测试 mock 工具。实际实现中:
- `StubLLM` 等价能力由 `examples/pdk_chat_demo/mock_blocking_provider.h` 提供 (T19 GEPA Phase 2 测试基线)
- `FakeStateStore` 等价能力由各 test fixture 内联提供 (避免 PDK 引入过多间接层)
- `MockSandbox` 等价能力由 `pdk_chat_demo/tests/test_skill_interpreter_safe_exec.cpp` 提供

设计为 V2 提取候选, Phase 1 暂不实施。
