# adr-doc-alignment-p2-cleanup Specification

## Purpose

ADR 文档对齐 P2 清理规范: 形式化 5 项清理范围约束 (数字同步 / ADR 状态注记 / master plan 数字 / STATUS-GLOSSARY 双语义 / relationships.md 重跑), 锁定本 change 边界, 明确"不修复 ADR-0036 编号冲突 / 不修改 C16 proposal / 不解决 llama.h LSP 错误"的 Non-goals。本 spec 是 `adr-doc-alignment-hotfix` 与 `pdk-tool-naming-policy` 的最终完善 (6 Requirements → 11 Requirements)。

## MODIFIED Requirements

### Requirement: adr-status-glossary-active-list-consistency (扩展)

`docs/adr-management/STATUS-GLOSSARY.md` 状态表 MUST 仅包含 `docs/adr/adr-NNNN-*.md` 活跃 ADR 节点 (排除 `docs/archive/adr/` 归档文件 + `docs/adr/plugin/` 候选 ADR 单独管理)。活跃 ADR 状态值 MUST 与该 ADR 文件 `## 状态` 字段首行 emoji 标签一致。

#### Scenario: 状态值匹配活跃 ADR

- **WHEN** 解析 STATUS-GLOSSARY.md "使用场景" 列引用的活跃 ADR 编号
- **THEN** STATUS-GLOSSARY 表格中标注的状态值 MUST 与 ADR 文件 `## 状态` 字段一致
- **AND** 表格 MUST 包含 7 个标准标签 (Approved / Partial / Not Implemented / Superseded / Proposed / Reserved / Audit)
- **AND** 标题 MUST 标注 "7 个标准标签" (非 6 个)

#### Scenario: 归档 ADR 状态标注

- **WHEN** ADR 文件 `git mv` 至 `docs/archive/adr/`
- **THEN** STATUS-GLOSSARY 活跃表 MUST 移除该 ADR
- **AND** STATUS-GLOSSARY "已废弃" 段 MUST 列出该 ADR, 标 `⛔ Superseded` + 引用替代 ADR 编号
- **AND** ADR-0036 MUST 标 `⛔ Superseded` + 引用 ADR-0045

#### Scenario: 同步方向约束

- **WHEN** ADR 文件 `## 状态` 字段修订
- **THEN** STATUS-GLOSSARY 状态表 MUST 在同一次 commit 中同步 (单向同步, ADR → STATUS-GLOSSARY)
- **AND** 维护规则 #2 MUST 追加 "**同步方向**: From `## 状态` 字段 → STATUS-GLOSSARY 状态表 (单向)"

### Requirement: dual-semantic-reserved-vs-audit (新增)

STATUS-GLOSSARY.md `📋` 标签 MUST 拆分为两个独立行 (`📋 Reserved` + `📋 Audit`) 而非合并单行, 避免 audit 文档误用 reserved 语义。

#### Scenario: 双语义区分

- **WHEN** 解析 STATUS-GLOSSARY.md 状态表
- **THEN** `📋 Reserved` 行 MUST 仅用于"编号预留, 无内容"场景
- **AND** `📋 Audit` 行 MUST 用于 "审计补充文档" 场景 (impl-scope-audit 文档专用)
- **AND** 维护规则 #3 MUST 追加例外条款: 现有标签的子语义扩展允许 (如 `📋` 双语义), 但 MUST 同步更新 `tools/adr_lint.py` 的状态白名单 + regex 模式

#### Scenario: adr_lint 工具同步

- **WHEN** 修订 STATUS-GLOSSARY.md 新增/拆分/合并状态标签
- **THEN** `tools/adr_lint.py` 的 `VALID_STATUS` 集合 MUST 同步更新 (含 "audit" 等新标签)
- **AND** `STATUS_PATTERN` regex MUST 同步追加对应 emoji+name 模式
- **AND** 错误信息 MUST 列出更新后的全部合法标签

### Requirement: impl-scope-audit-marking (新增)

`docs/adr/*-impl-scope.md` 文档 `## 状态` 段 MUST 显式标记为 `📋 Audit` 语义, 以区别于正式 ADR 状态。

#### Scenario: impl-scope 文档结构

- **WHEN** 解析 `docs/adr/*-impl-scope.md` 的 `## 状态` 段
- **THEN** 第一非空行 MUST 包含 `📋 Audit` 标签
- **AND** 后续行 MUST 保留父 ADR 的实际状态 (✅ Approved / 🟡 Partial 等) 作为 audit 结论
- **AND** `adr_lint.py` MUST 接受此双行格式 (不报错)

#### Scenario: 例外 (历史创建文件)

- **WHEN** `docs/adr/adr-0002-impl-scope-audit.md` 或 `adr-0004-impl-scope-audit.md` 使用 `📋 Reserved (审计补充)` 格式
- **THEN** 此格式 MUST 被 `adr_lint.py` 接受 (兼容历史 2026-06-13 创建的 OpenSpec change `docs-code-drift-audit-2026-06` 产出)

### Requirement: master-plan-c13-ship-state-marker (新增)

`docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §7.5 (推理标准库 7 子图) MUST 在 7/7 schema 全部 ship 后将 checkbox 标 `[x]` + 列出 ship commit hash。

#### Scenario: 7/7 schema ship 状态验证

- **WHEN** C13 (4 schema: prefix_cache/kv_cache/decoding/cloud_engine) + C14 (3 schema: engine/model/session) + C15 (1 schema: batching) 全部 ship
- **THEN** §7.5 MUST 标注所有 7 个 schema 状态为 `[x]` + 关联 ship commit hash
- **AND** §7.6 line 567 checkbox MUST 标 `[x] 推理标准库 7/7 子图全部 ship`
- **AND** §8.1 line 580 checkbox MUST 标 `[x] 推理标准库 7/7 子图全部 ship (engine/model/session + prefix_cache/kv_cache/decoding/batching)`

#### Scenario: 验证方式

- **WHEN** 跑 `ctest --test-dir build/tests`
- **THEN** test_llama_engine_plugin MUST PASS (10 cases)
- **AND** 65/65 ctest MUST PASS (零回归)

### Requirement: adr-0021-samplerstrategy-deprecation-note (新增)

`docs/adr/adr-0021-pdk-design.md` `## 状态` 段 MUST 追加 "2026-07-08 update" 注记, 说明 §8 SamplerStrategy 接口被 `decisions-2026-07-07.md` D1 决策撤销。

#### Scenario: D1 决策注记

- **WHEN** 修订 `docs/adr/adr-0021-pdk-design.md` `## 状态` 段
- **THEN** MUST 追加 "> **2026-07-08 update**: §8 SamplerStrategy 接口被 `docs/adversarial-reviews/decisions-2026-07-07.md` D1 决策撤销 (B2 实施前对齐)"
- **AND** 验证: `grep "SamplerStrategy.*撤销" docs/adr/adr-0021-pdk-design.md` 命中 1 行

### Requirement: relationships-md-regenerate-coverage (新增)

`tools/adr_relationships.py` MUST 在重跑后覆盖 16+ 新 ADR 节点 (0035/0038-0046/0014/0029/0032 等), 状态统计 MUST 反映最新 ADR 状态。

#### Scenario: 重跑后节点覆盖

- **WHEN** 跑 `python3 tools/adr_relationships.py`
- **THEN** 输出 ADR 总数 MUST ≥ 38 (vs 原 22, 含 16 新节点)
- **AND** `docs/adr/plugin/adr-0034-model-router.md` MUST 包含
- **AND** `docs/archive/adr/` MUST 排除
- **AND** "按状态统计" 表格 MUST 自动更新 (✅ Approved ≥ 16)

#### Scenario: CI 模式

- **WHEN** 跑 `python3 tools/adr_relationships.py --check`
- **THEN** 若生成结果与已存在的 relationships.md 不一致 MUST exit 1
- **AND** 一致时 exit 0

## ADDED Requirements

无 (本 change 是 cleanup, 不新增 spec-level behavior)