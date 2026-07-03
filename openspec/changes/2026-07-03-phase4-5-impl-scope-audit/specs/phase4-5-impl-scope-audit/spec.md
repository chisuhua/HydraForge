# phase4-5-impl-scope-audit Specification

> **Purpose**: 追踪 Phase 4.5 → Phase 5 过渡前的 ADR 实施范围审计
> **STATUS: ACTIVE** 🟡
> **关联 proposal**: `../proposal.md`
> **最后更新**: 2026-07-03

## ADDED Requirements

### Requirement: phase4-5-adr-audit-complete

11 个 ADR (0001/0003/0004/0005/0007/0008/0019/0020/0022/0023/0033) MUST 各自创建一个 `*-impl-scope.md` 子文档。

#### Scenario: 11 个 impl-scope 文档存在

- **WHEN** 检查 `docs/adr/adr-XXXX-*-impl-scope.md` 文件列表
- **THEN** 必须存在 11 个文件 (11 个 ADR 各 1 个)
- **AND** 每个文件包含 "原始描述 vs 实际实施" 表格

#### Scenario: 每个缺失类被分类

- **WHEN** 阅读每个 `*-impl-scope.md` 的核心表格
- **THEN** 每个 ADR drift 报告的缺失类 MUST 被分类为 Shipped / Evolved / Deferred 之一
- **AND** 决策行 MUST 给出 ADR 状态调整建议

---

### Requirement: phase4-5-drift-resolved

`tools/docs_drift_audit.py` MUST 输出 `0 DRIFT items`。

#### Scenario: drift 工具验证

- **WHEN** 运行 `python3 tools/docs_drift_audit.py`
- **THEN** SUMMARY 行 MUST 显示 `0 DRIFT items, 0 WARNING items detected`
- **AND** Scenario 4 (ADR 声称实现 vs 代码 grep) MUST 显示 0 drift

---

### Requirement: phase4-5-adr-status-aligned

`docs/README.md` ADR 状态表格 MUST 与 `adr-relationships.md` 和各 ADR 主文档的 `## 状态` 字段一致。

#### Scenario: 状态一致

- **WHEN** 比较 `docs/README.md` ADR 表格 vs `docs/adr-management/relationships.md` §一状态总览
- **THEN** 每个 ADR 的状态标签 MUST 一致
- **AND** 状态变更 MUST 在 commit message 中记录

---

### Requirement: phase4-5-roadmap-synced

`docs/roadmap-status.md` MUST 包含 Phase 4.5 → Phase 5 过渡说明。

#### Scenario: roadmap 同步

- **WHEN** 读取 `docs/roadmap-status.md` §一
- **THEN** Phase 4.5 行 MUST 包含 "✅ 已完成 (C8, 2026-07-03)" + "C9 audit ship 标记"
- **AND** §四 实施日志 MUST 追加 C9 ship 行 (格式: `| 2026-07-03 | **C9 Sprint 19 ship (Phase 4.5 → Phase 5 过渡 audit)** | ...`)
- **AND** AGENTS.md § Recent Changes MUST 追加 C9 ship 记录

---

## 备注

本 change 完成后, Phase 5 启动的前置条件 (C9 关闭 ADR drift) 完成。
下一步: 写新 master plan `2026-07-XX-phase5-self-bootstrapping.md` + Oracle 咨询 Phase 5 阶段 1 切分。
