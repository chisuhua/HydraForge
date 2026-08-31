# ADR-0064 PDK Conformance Test Suite 实施范围审计

> **生成时间**: 2026-08-31 (docs_drift_audit Scenario 4 修正)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0064-pdk-conformance-test-suite.md](adr-0064-pdk-conformance-test-suite.md)
> **状态**: ✅ Approved (Phase 6a direction ADR)

## 状态

**📋 Audit** (impl-scope-audit 文档)

✅ Approved (Phase 6a 评审 2026-07-16 通过)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved, 但 2/2 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `ConformanceSuite` | 📅 Phase 6a Wave 2 待实施 | — | PDK 一致性测试套件 |
| `MarketplaceUpload` | 📅 Phase 6a Wave 2 待实施 | — | Marketplace 上传工具 (依赖 ADR-0062) |

## 分类详情

### 📅 Deferred — Phase 6a Wave 2

ADR-0064 依赖 ADR-0062 Marketplace ship。当前 cap-map §一 已 ship #27-#31 自进化基础, Conformance suite 是 Phase 6a Wave 2-3 范围, 依赖 Marketplace 决策。
