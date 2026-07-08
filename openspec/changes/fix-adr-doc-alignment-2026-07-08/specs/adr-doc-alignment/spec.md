# adr-doc-alignment Specification

## Purpose

ADR 文档对齐规范: 形式化命名约定、状态同步规则、编号一致性、引用准确性 4 类约束,确保 `docs/adr/`、`docs/adr-management/`、`docs/adr/plugin/`、`docs/archive/adr/` 四目录与下游文档 (`lib/`、`openspec/changes/`) 保持对齐。本 spec 是 B2 实施 (C13/C14/C15) + 后续所有 Phase 5 变更的文档前置契约。

## ADDED Requirements

### Requirement: pdk-tool-naming-slash-convention

所有 PDK 工具 MUST 使用 SLASH (`/`) 作为分层分隔符, 格式 `{plugin_namespace}/{component}/{action?}`。DOT (`.`) 仅允许 C++ method names 与 std::string 字段名, 不允许作为 PDK 工具名分隔符。

#### Scenario: 注册 slash 风格工具名

- **WHEN** plugin 开发者通过 `registry.register_tool_function(name, ...)` 注册工具
- **THEN** `name` 参数 MUST 匹配正则 `^[a-z][a-z0-9_]+/[a-z][a-z0-9_]+(/[a-z][a-z0-9_]+)?$`
- **AND** `plugin_namespace` 段 MUST ≥3 字符 snake_case
- **AND** `component` 段 MUST ≥2 字符 snake_case
- **AND** 不可包含双 `/` 或尾随 `/`

#### Scenario: 拒绝 dot 风格工具名

- **WHEN** plugin 开发者注册工具名包含 `.` (如 `inference.engine.init`)
- **THEN** `IToolRegistry` MUST 拒绝注册, 抛出 `std::invalid_argument` 异常, 错误信息 MUST 包含 "slash (`/`) only, see ADR-0034 §命名约定"

#### Scenario: lib/inference/*.md 工具名引用

- **WHEN** `lib/inference/*.md` 文件 (B2 schema 定义) 在 `tool:` 字段引用工具
- **THEN** 工具名 MUST 使用 SLASH 格式
- **AND** 与对应 `pdk/<plugin>/src/*.cpp` 注册的工具名保持一致 (string match)

#### Scenario: 跨文档工具名一致性

- **WHEN** `openspec/changes/phase5-*` proposal + `docs/adr/adr-00*.md` + `lib/inference/*.md` 三类文档引用同一 PDK 工具
- **THEN** 三处工具名 MUST 完全一致 (string match)
- **AND** `tools/adr_lint.py` MUST 检测并报告不一致

### Requirement: adr-status-glossary-sync-rule

`docs/adr-management/STATUS-GLOSSARY.md` 状态表 MUST 与 `docs/adr/**/adr-*.md` 各 ADR 文件 `## 状态` 字段保持一致, 同步方向为 ADR → STATUS-GLOSSARY (单向)。任何 ADR 状态变更时, STATUS-GLOSSARY MUST 在同一次 commit 中同步。

#### Scenario: 状态值匹配

- **WHEN** 解析 ADR 文件 `## 状态` 字段 (首个 emoji 标签)
- **THEN** STATUS-GLOSSARY 状态表 "使用场景" 列引用该 ADR 时, 标注的状态值 MUST 与 ADR 文件实际状态一致
- **AND** 6 个标准状态标签 (✅ Approved / 🟡 Partial / ❌ Not Implemented / ⛔ Superseded / 🔍 Proposed / 📋 Reserved/Audit) 之外的状态 MUST 报告为 lint error

#### Scenario: STATUS-GLOSSARY 状态值修订

- **WHEN** STATUS-GLOSSARY.md 修订状态示例行
- **THEN** 修订前 MUST 验证 ADR 文件实际状态 (`grep -A 2 "^## 状态" docs/adr/adr-NNNN-*.md`)
- **AND** STATUS-GLOSSARY 维护规则 #2 追加 "From ADR ## 状态 → STATUS-GLOSSARY 单向同步" 说明

#### Scenario: CI lint 失败处理

- **WHEN** `tools/adr_lint.py` 检测到 STATUS-GLOSSARY 状态表与 ADR 实际状态不一致
- **THEN** CI MUST 失败, exit code 非 0
- **AND** 错误信息 MUST 列出冲突的 ADR 编号 + STATUS-GLOSSARY 当前值 + ADR 实际值

### Requirement: adr-numbering-uniqueness

ADR 编号 MUST 全局唯一。Renumber 决策 MUST 同步归档旧编号文件至 `docs/archive/adr/`, 在归档文件头部添加 DEPRECATED 横幅。

#### Scenario: Renumber 决策执行

- **WHEN** 新决策将 ADR 编号从 X 重编号到 Y (如 ADR-0036 → ADR-0045)
- **THEN** 旧编号 X 对应文件 MUST 在同一 change 中 `git mv` 至 `docs/archive/adr/`
- **AND** 归档文件头部 MUST 添加 `> **⛔ DEPRECATED (YYYY-MM-DD)**` 横幅, 引用新编号 Y
- **AND** 引用旧编号 X 的活跃文档 MUST 更新到引用新编号 Y
- **AND** `docs/adr/plugin/README.md` MUST 追加 renumber 注记

#### Scenario: ADR 编号冲突检测

- **WHEN** `tools/adr_relationships.py` 扫描 `docs/adr/` + `docs/adr/plugin/` + `docs/archive/adr/` 三目录
- **THEN** 同一编号在多目录出现 MUST 报告为 lint error
- **AND** 归档目录文件 MUST 排除在 `relationships.md` 节点统计外

#### Scenario: 编号跳跃合法性

- **WHEN** 新 ADR 使用编号 X, X 不在 0001-0099 范围
- **THEN** MUST 报告为 lint error, 提示"未授权编号范围"

### Requirement: cross-document-reference-accuracy

`docs/adversarial-reviews/`, `docs/handoff/`, `openspec/changes/*/proposal.md` 任何跨文档引用 MUST 准确指向现行文件路径, 不允许拼写错误、路径错误、或引用已删除/重命名文件。

#### Scenario: 文件名拼写验证

- **WHEN** 文档使用 markdown 链接 `[text](path/to/file.md)` 引用其他文档
- **THEN** `tools/adr_lint.py` MUST 验证链接目标文件存在
- **AND** 拼写错误 (如 `oopenspec` vs `openspec`) MUST 报告为 lint warning

#### Scenario: 重命名文件引用更新

- **WHEN** 文件被 `git mv` 至新路径
- **THEN** 引用该文件的所有文档 MUST 在同一 change 中更新引用路径
- **AND** 工具 MUST 扫描所有 markdown 链接检测 stale 引用

#### Scenario: Session ID 引用

- **WHEN** 文档引用 session ID (如 `ses_0cb1027ccffeN7BmCaOQTpQl1Y`)
- **THEN** MUST 标注 session 来源 (Oracle/Metis/Momus/explore/librarian) + 关联主题
- **AND** session ID 格式 MUST 匹配 `^ses_[0-9a-f]{20,}$`

### Requirement: adr-management-audit-semantics

STATUS-GLOSSARY.md 的 📋 标签 MUST 包含 "Reserved" (编号预留) 与 "Audit" (impl-scope-audit 文档) 两种语义, README.md 表格与 STATUS-GLOSSARY 用法保持一致。

#### Scenario: 状态标签识别

- **WHEN** `tools/adr_lint.py` 解析 ADR `## 状态` 字段
- **THEN** 识别 emoji 标签 MUST 包含: ✅ / 🟡 / ❌ / ⛔ / 🔍 / 📋
- **AND** 📋 标签的语义根据上下文判定:
  - **Reserved**: ADR-0024-0028 占位编号, 编号预留无内容
  - **Audit**: `adr-*-impl-scope.md` 文档, 实施范围审计

#### Scenario: 审计补充文档标记一致性

- **WHEN** `adr-0001-illm-provider-streaming-interface-impl-scope.md` 等 12 个 impl-scope 文档
- **THEN** `## 状态` 字段 MUST 使用 📋 Audit
- **AND** `docs/README.md` 表格状态列 MUST 标注 "📋 审计补充"

#### Scenario: 维护规则 #3 例外条款

- **WHEN** STATUS-GLOSSARY 维护规则 #3 禁止 "创建新状态标签"
- **THEN** 规则 MUST 追加例外条款: "现有标签的子语义扩展允许 (如 📋 双语义)"
- **AND** 新子语义引入 MUST 在 STATUS-GLOSSARY 表格中明确定义

### Requirement: decisions-file-step-numbering

`docs/adversarial-reviews/decisions-*.md` 实施步骤 MUST 编号连续无重复, 每步任务边界清晰, 签字状态与文件头 "已定稿" 标注保持一致。

#### Scenario: 步骤编号连续性

- **WHEN** decisions 文件包含 "实施步骤" 列表
- **THEN** step 编号 MUST 从 1 开始连续递增
- **AND** 同一 step 标题或内容重复 MUST 报告为 lint error
- **AND** 缺失 step 编号 MUST 报告为 lint warning

#### Scenario: 签字状态一致性

- **WHEN** decisions 文件头部标注 "✅ 已定稿"
- **THEN** 文件内每个决策项 MUST 标注 "✅ 已签字 (YYYY-MM-DD by [signer])" 或 "🟡 待签字"
- **AND** "✅ 已定稿" 与 "🟡 待签字" 混用 MUST 报告为 lint warning

#### Scenario: 决策项编号

- **WHEN** decisions 文件包含 D1/D2/D3/... 决策项
- **THEN** 编号 MUST 连续
- **AND** 每个决策项 MUST 包含 "决策" + "描述" + "影响" 3 个必需段

### Requirement: relationships-md-adr-coverage

`docs/adr-management/relationships.md` MUST 包含 `docs/adr/**/adr-*.md` (含 `plugin/` + `archive/` 排除项) 所有活跃 ADR 节点, 状态统计 MUST 与 STATUS-GLOSSARY 一致。

#### Scenario: ADR 节点覆盖

- **WHEN** `tools/adr_relationships.py` 生成 `relationships.md`
- **THEN** 输出 MUST 包含 `docs/adr/adr-NNNN-*.md` + `docs/adr/plugin/adr-NNNN-*.md` 所有活跃 ADR 节点
- **AND** 归档目录 (`docs/archive/adr/`) 文件 MUST 排除
- **AND** `docs/adr/README.md` (非 ADR 文件) MUST 排除
- **AND** 节点总数 MUST 与 STATUS-GLOSSARY 状态示例行 ADR 引用数匹配

#### Scenario: 状态统计表一致

- **WHEN** `relationships.md` "按状态统计" 表格更新
- **THEN** 各项数量之和 MUST 等于活跃 ADR 节点总数
- **AND** 状态值 MUST 在 6 个标准标签内
- **AND** 状态数量 MUST 与 STATUS-GLOSSARY "使用场景" 列 ADR 引用一致

#### Scenario: 替代/依赖边检测

- **WHEN** ADR 文件包含 "supersedes" / "替代" 关键词
- **THEN** `relationships.md` mermaid 图 MUST 包含对应的替代边 (`-.->|supersedes|`)
- **AND** "depends on" / "依赖" 关键词 MUST 包含依赖边 (`-->`)
