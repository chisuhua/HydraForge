# ADR-0075 Env Backend Local Docker 实施范围审计

> **生成时间**: 2026-08-31 (docs_drift_audit Scenario 4 修正)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0075-env-backend-local-docker.md](adr-0075-env-backend-local-docker.md)
> **状态**: ✅ Approved (Phase 6c C11-C13 ship)

## 状态

**📋 Audit** (impl-scope-audit 文档)

✅ Approved (Phase 6c C11-C13 ✅ Shipped 2026-08-18 via `from-roadmap-phase-6c-execution-envbackend`)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved (D1+D2+D3+D5 全 ship), 但 1/9 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `IEnvBackend` | ✅ Shipped | `include/agenticdsl/contract/env_backend.h` | D1 ship |
| `LocalBackend` | ✅ Shipped | `src/modules/env/local_backend.cpp` | D2 ship |
| `DockerBackend` | ✅ Shipped | `src/modules/env/docker_backend.cpp` | D3 ship |
| `EnvValidationHook` | 🟡 Partial | ADR-0075 D5 实装, 但作为 Hook 集成未单独提取为 IEnvValidationHook 接口类 | 8h 集成, 当前通过 EnvBackend::validate() inline |

## 分类详情

### ✅ Shipped

ADR-0075 D1 + D2 + D3 + D5 全 ship, OpenSpec change `from-roadmap-phase-6c-execution-envbackend` archived 2026-08-18。

### 📅 Deferred — Phase 6a

`EnvValidationHook` 接口抽象待 Phase 6a。当前实装为 inline validation, 复用 ADR-0081 (IAgentHookRegistry) L2 拦截。
