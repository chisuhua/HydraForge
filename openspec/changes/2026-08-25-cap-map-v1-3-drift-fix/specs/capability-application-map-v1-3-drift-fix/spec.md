# capability-application-map v1.3 drift 修复 Specification

## Purpose

锁定 16 处 `docs/architecture/capability-application-map-2026-08.md`（v1.3）文档 drift 修复的验收条件，明确 Non-goals。本 spec 是 Sprint 24 启动前 Step 3 任务（per AGENTS.md Single-Developer Mode §Sprint 24 Pre-Launch Step 3）。

## ADDED Requirements

### Requirement: section-flow-restored-seven-before-eight

主要章节 MUST 按 §一→§二→§三→§四→§五→§六→§七→§八 顺序排列。

#### Scenario: 主要章节顺序正确

- **WHEN** 解析 `awk '/^## /{print NR":"$0}' docs/architecture/capability-application-map-2026-08.md` 前 8 行
- **THEN** MUST 依次匹配 `一、` `二、` `三、` `四、` `五、` `六、` `七、` `八、`
- **AND** `## 七` 的行号 MUST 小于 `## 八` 的行号

### Requirement: gap-status-vocabulary-closed-added

§二 状态词汇表 MUST 包含 `✅ Closed` 状态定义。

#### Scenario: 词汇表已补充 Closed 状态

- **WHEN** 解析 §二 line 88-92 词汇表段
- **THEN** MUST 至少出现一次 `Closed` 关键字
- **AND** §二 G15 行状态列 MUST 等于 `✅ Closed`

### Requirement: gap-section-title-updated

§二 章节标题 MUST NOT 含 "未 ship" 措辞。

#### Scenario: 标题反映 Closed 状态

- **WHEN** 解析 `^## 二、` 标题文本
- **THEN** 标题 MUST NOT 包含 `未 ship` 字符串
- **AND** 标题 MUST 含 `Closed` 或 `追踪` 等时效指示词

### Requirement: g15-loop1-cell-synced

§八 闭环 1 表格 G15 行（第 7 行 - 蒸馏输出格式）MUST 反映 ADR-0061-13 已 Approved。

#### Scenario: G15 单元格更新

- **WHEN** 解析 §八 闭环 1 表格的第 7 行
- **THEN** 第 3 列（契约状态） MUST 等于 `✅ Approved`
- **AND** 第 5 列（阻塞链） MUST NOT 含 `新 ADR 需求`

### Requirement: closed-loop-g10-g12-stale-rows-synced

闭环 1 + 闭环 2 表格中所有引用 G10/G12 的行 MUST 反映 ADR-0083 / ADR-0080 v1.2 Amendment Approved 状态。

#### Scenario: 闭环 1 G10 同步

- **WHEN** 解析 §八 闭环 1 表格的 row 3 (评估信号行)
- **THEN** 契约状态列 MUST 等于 `✅ Approved (2026-08-25)`
- **AND** 阻塞链列 MUST NOT 含 `新 ADR 需求`

#### Scenario: 闭环 2 G10 同步

- **WHEN** 解析 §八 闭环 2 表格的 row 3
- **THEN** 契约状态列 MUST 等于 `✅ Approved`
- **AND** 阻塞链列 MUST NOT 含 `新 ADR 需求`

### Requirement: section-eight-five-duplicate-deleted

§八.5 排期表 MUST 仅含评审通过后 Sprint 24/25/26 排期。

#### Scenario: 排期表无重复

- **WHEN** 解析 §八.5 全文
- **THEN** MUST NOT 含 `下个 Sprint` 字面字符串
- **AND** MUST NOT 含 `opportunistic` 字面字符串

### Requirement: adr-0071-0074-evaluation-passed-footer-synced

`docs/adr/adr-0071-llm-native-agenticdsl-architecture.md` 与 `docs/adr/adr-0074-prompt-evidence-gate.md` 文件 MUST 在 header line 5 与 footer 行同步 "Promotion 评审通过 2026-08-25" 措辞。

#### Scenario: ADR-0071 header + footer 已同步

- **WHEN** `grep -n "待架构组评审" docs/adr/adr-0071-llm-native-agenticdsl-architecture.md`
- **THEN** 输出行数 MUST 等于 `0`
- **WHEN** `grep -c "Promotion 评审通过 2026-08-25" docs/adr/adr-0071-llm-native-agenticdsl-architecture.md`
- **THEN** 输出 MUST ≥ `2`

#### Scenario: ADR-0074 header + footer 已同步

- **WHEN** `grep -n "待架构组评审" docs/adr/adr-0074-prompt-evidence-gate.md`
- **THEN** 输出行数 MUST 等于 `0`
- **WHEN** `grep -c "Promotion 评审通过 2026-08-25" docs/adr/adr-0074-prompt-evidence-gate.md`
- **THEN** 输出 MUST ≥ `2`

### Requirement: ctest-count-185-synced

§六 验证命令附录 MUST 反映当前 ctest Total Tests 数 185。

#### Scenario: §六.1.2 SLM ship gate ctest 计数

- **WHEN** 解析 §六.1.2 line 325 验证期望值
- **THEN** MUST 等于 `185/185` 而非 `184/184`

#### Scenario: §六.3 ctest 全量验证期望

- **WHEN** 解析 §六.3 line 385 验证期望值
- **THEN** MUST 含 `185/185 tests` 字面字符串

### Requirement: section-one-coverage-range-sprint23

§一 覆盖范围 + 标题 MUST 反映截至 Sprint 23 (含 T14/T16 后置增补) + 23 项。

#### Scenario: §一 覆盖范围含 Sprint 23

- **WHEN** 解析 §一 line 30-32 覆盖范围说明 + §一 line 28 标题
- **THEN** MUST 含 `Sprint 23` 字面字符串
- **AND** §一 标题 MUST 含 `23 项`

### Requirement: section-three-zero-engineering-count-23

§三 "零工程" 段 MUST 反映已 ship 能力总数 23。

#### Scenario: §三 A 类依赖能力行

- **WHEN** 解析 §三 line 124 + line 131
- **THEN** MUST 含 `23 项已 ship 能力` 而非 `22 项`

### Requirement: section-two-property-terminology-aligned

§二 line 91 词汇对齐 MUST 使用 `性质标记` 与表格列名一致。

#### Scenario: 词汇表与列名匹配

- **WHEN** `grep -n "分层标记" docs/architecture/capability-application-map-2026-08.md`
- **THEN** 输出行数 MUST 等于 `0`

### Requirement: section-one-l4-header-noted

§一 L4 表头 MUST 标注 `#23 T14 v1.2 后置增补` 注释。

#### Scenario: L4 表头含增补注释

- **WHEN** 解析 §一 L4 章节标题行
- **THEN** MUST 含 `#23 T14 v1.2 后置增补` 字面字符串

---

## Non-Goals Excluded Requirements

### Requirement: zero-code-changes

本 change MUST NOT 修改任何源代码文件。

#### Scenario: git diff 不含源码

- **WHEN** 跑 `git diff --stat HEAD -- 'src/*' 'include/*' 'pdk/*' 'tests/*' 'examples/*'`
- **THEN** 输出 MUST 等于空
- **AND** ctest 仍维持 185/185 PASS

### Requirement: zero-archive-touched

本 change MUST NOT 修改 `openspec/changes/archive/`。

#### Scenario: archive/ 未触碰

- **WHEN** 跑 `git diff --stat HEAD -- 'openspec/changes/archive/'`
- **THEN** 输出 MUST 等于空
