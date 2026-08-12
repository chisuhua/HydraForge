# adr-doc-alignment-hotfix Specification

## Purpose

ADR 文档对齐 hotfix 规范: 形式化 4 类 hotfix 范围约束 (状态同步、引用修正、step 编号、ADR 编号重定义), 锁定本 hotfix 边界, 明确"不修改 PDK 代码 / 不修改 lib/inference / 不修改 C16 proposal"的 Non-goals。本 spec 是 Change B (命名政策) 与 Change C (P2 清理) 的前置契约。

## Requirements

### Requirement: adr-status-glossary-active-list-consistency

`docs/adr-management/STATUS-GLOSSARY.md` 状态表 MUST 仅包含 `docs/adr/adr-NNNN-*.md` 活跃 ADR 节点 (排除 `docs/archive/adr/` 归档文件 + `docs/adr/plugin/` 候选 ADR 单独管理)。活跃 ADR 状态值 MUST 与该 ADR 文件 `## 状态` 字段首行 emoji 标签一致。

#### Scenario: 状态值匹配活跃 ADR

- **WHEN** 解析 STATUS-GLOSSARY.md "使用场景" 列引用的活跃 ADR 编号
- **THEN** STATUS-GLOSSARY 表格中标注的状态值 MUST 与 ADR 文件 `## 状态` 字段一致
- **AND** ADR-0021/0022/0023 MUST 标 ✅ Approved (2026-06-24)
- **AND** ADR-0030 MUST 标 🔍 Proposed (2026-06-26)
- **AND** ADR-0034 MUST 标 ✅ Approved (2026-07-02)

#### Scenario: 归档 ADR 状态标注

- **WHEN** ADR 文件 `git mv` 至 `docs/archive/adr/`
- **THEN** STATUS-GLOSSARY 活跃表 MUST 移除该 ADR
- **AND** STATUS-GLOSSARY "已废弃" 段 MUST 列出该 ADR, 标 `⛔ Superseded` + 引用替代 ADR 编号
- **AND** ADR-0036 MUST 标 `⛔ Superseded` + 引用 ADR-0045

#### Scenario: 同步方向约束

- **WHEN** ADR 文件 `## 状态` 字段修订
- **THEN** STATUS-GLOSSARY 状态表 MUST 在同一次 commit 中同步 (单向同步, ADR → STATUS-GLOSSARY)
- **AND** 维护规则 #2 MUST 追加 "**同步方向**: From `## 状态` 字段 → STATUS-GLOSSARY 状态表 (单向)"

### Requirement: cross-document-filename-accuracy

`docs/adversarial-reviews/README.md` 等文档的 markdown 链接 MUST 准确指向现行文件路径, 不允许拼写错误。`tools/adr_lint.py` (若存在) MUST 检测拼写错误并报告 warning。

#### Scenario: 拼写错误检测

- **WHEN** `docs/adversarial-reviews/README.md` line 84 引用 `ref-1-b2-openspec-arch.md`
- **THEN** 该行 MUST 不含 `oopenspec` 双 "o" 拼写
- **AND** 链接目标文件 MUST 存在于 `docs/adversarial-reviews/ref-1-b2-openspec-arch.md`
- **AND** `git grep "oopenspec" -- 'docs/'` 输出仅命中 `docs/archive/` 或 git 历史 commit (非活跃文档)

#### Scenario: 重命名文件引用同步

- **WHEN** 文件被 `git mv` 至新路径 (如 ADR-0036 归档)
- **THEN** 引用该文件的所有活跃文档 MUST 在同一次 commit 中更新引用路径或删除引用
- **AND** `git grep "adr-0036-three-layer-service-protocol" -- 'docs/' 'openspec/'` 输出 MUST 仅含 `docs/archive/adr/` 路径

### Requirement: decisions-file-step-numbering

`docs/adversarial-reviews/decisions-*.md` 文件的 "实施步骤" 列表 MUST 编号连续无重复, 每步任务边界清晰, 签字状态标注与文件头 "已定稿" 状态保持一致。

#### Scenario: step 编号连续性

- **WHEN** `decisions-2026-07-07.md` D5 实施步骤列表
- **THEN** step 编号 MUST 从 1 开始连续递增
- **AND** 同一 step 标题或内容重复 MUST 报告为 lint error
- **AND** D5 step 列表 MUST 包含 5 步: 删除默认注入 / 新增 API / 添加单测 / 迁移示例 / 文档更新

#### Scenario: 签字状态透明性

- **WHEN** decisions 文件头部标注 "✅ 已定稿"
- **THEN** 文件内每个决策项 MUST 标注 `✅ 已签字 (YYYY-MM-DD by [signer])` 或 `🟡 待签字 (YYYY-MM-DD)`
- **AND** "✅ 已定稿" 与 "🟡 待签字" 混用 MUST 报告为 lint warning (允许, 但提示)

#### Scenario: D5 签字状态查询

- **WHEN** 修订 D5 签字状态标注
- **THEN** 实施者 MUST 跑 `git log --follow docs/adversarial-reviews/decisions-2026-07-07.md` 找 author
- **AND** 若 author 存在 + 有 D5 决策 commit → 标 `✅ 已签字 (2026-07-08 by [author])`
- **AND** 否则 → 标 `🟡 待签字 (2026-07-08)` (默认)

### Requirement: adr-numbering-archive-coherence

ADR 编号 MUST 全局唯一。Renumber 决策 MUST 同步归档旧编号文件至 `docs/archive/adr/`, 在归档文件头部添加 DEPRECATED 横幅, 并同步所有活跃文档的旧引用。

#### Scenario: ADR-0036 软归档

- **WHEN** `git mv docs/adr/adr-0036-three-layer-service-protocol.md docs/archive/adr/`
- **THEN** 归档文件头部 MUST 添加 `> **⛔ DEPRECATED (2026-07-08)**` 横幅 + 引用 ADR-0045
- **AND** `docs/README.md` MUST 删除 ADR-0036 行
- **AND** `docs/adr/plugin/README.md` MUST 追加 ADR-0036 renumber 注记
- **AND** `docs/adr/adr-0030-async-runtime-v2.md:318` 旧链接 MUST 更新或删除
- **AND** `docs/handoff/2026-07-06-architecture-completion.md:51` 旧引用 MUST 更新

#### Scenario: 归档 ADR 排除 lint 误报

- **WHEN** `tools/adr_lint.py` 扫描 ADR 节点
- **THEN** `docs/archive/adr/` 目录文件 MUST 排除在 lint 扫描外
- **AND** 仅 `docs/adr/adr-NNNN-*.md` + `docs/adr/plugin/adr-NNNN-*.md` 参与活跃 ADR 统计

#### Scenario: 编号引用一致性

- **WHEN** 活跃文档引用 ADR 编号 (如 ADR-0036)
- **THEN** 该 ADR 文件 MUST 存在于 `docs/adr/` 或 `docs/adr/plugin/` 目录
- **AND** 若 ADR 已被 renumber 归档, 引用 MUST 更新到新编号 (如 ADR-0036 → ADR-0045)
