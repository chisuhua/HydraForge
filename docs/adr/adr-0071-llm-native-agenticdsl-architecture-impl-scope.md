# ADR-0071 LLM-native AgenticDSL Architecture 实施范围审计

> **生成时间**: 2026-08-31 (docs_drift_audit Scenario 4 修正)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0071-llm-native-agenticdsl-architecture.md](adr-0071-llm-native-agenticdsl-architecture.md)
> **状态**: ✅ Approved (顶层方向 ADR, 锚定 Phase 6+)

## 状态

**📋 Audit** (impl-scope-audit 文档)

✅ Approved (2026-08-25 Promotion 评审通过; 顶层方向 ADR; 派生子 ADR/Change 分 4 Wave)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved (顶层方向 ADR), 但 3/6 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `EnvBackend` | ✅ Shipped | `include/agenticdsl/contract/env_backend.h` (Phase 6c C11-C13 ship 2026-08-18) | ADR-0075 ✅ Approved + ship |
| `K8sBackend` | 📅 Phase 6a Wave 2 待实施 | — | K8s backend, 依赖 ADR-0076 MCP + ADR-0077 gRPC |
| `SSHBackend` | 📅 Phase 6a Wave 2 待实施 | — | SSH backend |
| `LocalBackend` | ✅ Shipped | `src/modules/env/local_backend.cpp` | ADR-0075 D2 ship |
| `DockerBackend` | ✅ Shipped | `src/modules/env/docker_backend.cpp` | ADR-0075 D3 ship |
| `EnvValidationHook` | 🟡 Partial | ADR-0075 D5 ✅ Shipped, hook 类名略不同 | 见 ADR-0075 |

## 分类详情

### ✅ Shipped (Phase 6c)

ADR-0075 ✅ Approved 2026-08-18 ship 实现了 IEnvBackend + LocalBackend + DockerBackend + EnvValidationHook (ADR-0075 D1+D2+D3+D5)。

### 📅 Deferred — Phase 6a Wave 2

K8sBackend + SSHBackend 是 ADR-0071 §D5/D8 蓝图方向, 依赖 ADR-0076 (MCP Server, 🔍 Proposed Wave 3 descoped) + ADR-0077 (gRPC Data Plane, 🔍 Proposed Wave 4 descoped docs-only)。
