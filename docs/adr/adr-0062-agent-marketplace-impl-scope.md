# ADR-0062 Agent Marketplace 实施范围审计

> **生成时间**: 2026-08-31 (docs_drift_audit Scenario 4 修正)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0062-agent-marketplace.md](adr-0062-agent-marketplace.md)
> **状态**: ✅ Approved (Phase 6a direction ADR)

## 状态

**📋 Audit** (impl-scope-audit 文档)

✅ Approved (Phase 6a 评审 2026-07-16 通过)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved, 但 2/2 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `PackageSandbox` | 📅 Phase 6a Wave 2 待实施 | — | Marketplace package 隔离沙箱 |
| `PackageVerifier` | 📅 Phase 6a Wave 2 待实施 | — | 包签名验证 |

## 分类详情

### 📅 Deferred — Phase 6a Wave 2

ADR-0062 Marketplace 是 Phase 6a 后期方向, 依赖 ADR-0059 跨进程协议 + ADR-0056 WASM Runtime。当前 Sprint 24 W2 仍聚焦 P0 闭环 (cap-map §一 #27-#31 自进化基础), Marketplace 子 Change 待 Wave 3+。
