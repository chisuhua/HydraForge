# ADR-0056 WASM Runtime 实施范围审计

> **生成时间**: 2026-08-31 (docs_drift_audit Scenario 4 修正)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0056-wasm-runtime.md](adr-0056-wasm-runtime.md)
> **状态**: ✅ Approved (Phase 6a direction ADR)

## 状态

**📋 Audit** (impl-scope-audit 文档)

✅ Approved (Phase 6a 评审 2026-07-16 通过)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved, 但 2/3 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `WasmRuntime` | 📅 Phase 6a Wave 2 待实施 | — | wasi-sdk 集成 (ADR-0061-05 ✅ Approved P1 ⚠ 无代码) 是前置 |
| `AgentCapability` | 📅 Phase 6a Wave 2 待实施 | — | 与 ADR-0054 共享 |
| `WasmModule` | 📅 Phase 6a Wave 2 待实施 | — | wasi-sdk 加载/实例化封装 |

## 分类详情

### 📅 Deferred — Phase 6a Wave 2

ADR-0056 依赖 ADR-0061-05 (`cpp-wasm-toolchain`, ✅ Approved 但 ⚠ 无代码, 当前 0/0 tasks)。两个 ADR 都需要 wasi-sdk 集成 + C++→Wasm CI 基础设施, 由 ADR-0061-05 子 Change 立项后, ADR-0056 子 Change 才能开工。
