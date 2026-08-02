# yaml-fenced-block-detection Specification

## Purpose

定义 `MarkdownParser`/`DslValidator` 对 DSL 文件的双格式（fenced yaml 代码块与 Markdown bold）支持契约。fenced yaml 路径必须识别 `# --- BEGIN AgenticDSL ---` 标记，复用现有 schema 校验逻辑；非法 yaml 必须产生与 bold 格式等价的 line-level 错误；bold 格式保持完全向后兼容，确保现有回归网持续有效。

## ADDED Requirements

### Requirement: yaml-fenced-block-parsing

`MarkdownParser` MUST 解析含 `# --- BEGIN AgenticDSL ---` 标记的 fenced yaml 代码块（` ```yaml ... ``` `），并将块内键值对转换为与 bold 格式等价的规范化 metadata map；解析失败时必须抛出含行号信息的错误。

#### Scenario: 合法 fenced yaml 块被解析为 metadata
- **GIVEN** 一个 `.agent.md` 文件首行为 `### AgenticDSL \`/main\``，紧跟 ` ```yaml ... ``` ` 块，块首行为 `# --- BEGIN AgenticDSL ---`，包含 `name: react`、`approval_policy: always`、`category: thinking` 字段
- **WHEN** `MarkdownParser` 解析该文件
- **THEN** 返回的 metadata map 包含 `name=react`、`approval_policy=always`、`category=thinking`
- **AND** 所有键名被规范化（小写、下划线替换连字符、trim）

#### Scenario: 非法 yaml 产生 line-level 错误
- **GIVEN** 一个 fenced yaml 块文件包含语法错误（如缺少冒号、缩进错误）
- **WHEN** `MarkdownParser` 尝试解析
- **THEN** 抛出/返回 `ValidationError`，包含出错行号与列号
- **AND** 错误信息不暴露内部 yaml-cpp 栈

### Requirement: dual-format-auto-detection

`DslValidator` MUST 自动检测输入文件格式：扫描文本中是否存在 fenced yaml 块且块首行含 `# --- BEGIN AgenticDSL ---` 标记，是则走 yaml 解析路径，否则走原有 bold 解析路径；检测逻辑对调用方透明，无需额外配置参数。

#### Scenario: yaml 文件被自动识别
- **GIVEN** 文件首行为 `### AgenticDSL \`/main\`` + 含 BEGIN 标记的 fenced yaml 块
- **WHEN** 调用 `DslValidator::validate()`
- **THEN** 进入 yaml 解析路径，不走 bold 行扫描

#### Scenario: bold 文件被自动识别
- **GIVEN** 文件首行为 `# Title` 或 `**name**: value` 的 bold 格式 `.agent.md`
- **WHEN** 调用 `DslValidator::validate()`
- **THEN** 进入原有 bold 解析路径，行为与现状完全一致

#### Scenario: 普通 yaml 示例不被误识别
- **GIVEN** 文件包含 ` ```yaml ... ``` ` 块但块首行不含 `# --- BEGIN AgenticDSL ---` 标记（普通 yaml 示例）
- **WHEN** 调用 `DslValidator::validate()`
- **THEN** 不走 yaml 路径，进入 bold 解析路径（或返回无 metadata）

### Requirement: schema-validation-reuse

两种格式解析后的 metadata map MUST 接入同一 schema 校验函数；禁止为 yaml 路径复制或重写校验规则（字段类型、审批策略、Layer 矩阵、必需字段等）。

#### Scenario: yaml 非法配置触发 schema 错误
- **GIVEN** 一个 fenced yaml 文件缺少 schema 必需字段 `name`
- **WHEN** `DslValidator::validate()` 执行
- **THEN** 返回与 bold 格式缺少 `name` 时完全相同的 `ValidationError` 类型与错误信息

#### Scenario: yaml 审批策略冲突被检测
- **GIVEN** 一个 yaml 文件设置 `category: dangerous` 且 `approval_policy: plan=false, agent=false`
- **WHEN** 执行 schema 校验
- **THEN** 校验失败，返回与 bold 格式等价的冲突错误

### Requirement: bold-format-backward-compatibility

原有 bold 格式（`**key**: value`）MUST 保持 100% 向后兼容，现有 bold 格式测试 fixture 在新增 yaml 路径后仍需全部通过；公开 API 签名 MUST NOT 改变。

#### Scenario: 现有 bold fixture 零回归
- **GIVEN** 现有 bold 格式测试 fixture
- **WHEN** 运行 `tests/test_dsl_validator.cpp`
- **THEN** 全部 bold fixture 校验通过，行为与 change 前一致

#### Scenario: API 签名不变
- **GIVEN** 现有代码调用 `DslValidator::validate(content)` 或 `MarkdownParser::parse_metadata(content)`
- **WHEN** 重新编译
- **THEN** 无需修改任何调用点
- **AND** 返回值/异常语义不变

### Requirement: invalid-yaml-line-level-errors

对于 yaml 语法错误、类型错误、或 schema 校验失败，`DslValidator` MUST 提供 line-level 错误信息，指向 DSL 文件中的具体位置，而非泛化的"yaml 无效"提示。

#### Scenario: yaml 语法错误含行号
- **GIVEN** fenced yaml 块第 3 行存在缩进错误
- **WHEN** 校验失败
- **THEN** 错误信息包含 `line 3` 或等价的行号定位

#### Scenario: schema 非法字段含字段名
- **GIVEN** yaml 包含未知字段 `unkown_field: value`
- **WHEN** 校验失败
- **THEN** 错误信息明确指出 `unknown_field` 非法

### Requirement: validation-skip-branch-removal

`examples/pdk_chat_demo/main.cpp` 中检测到 YAML 即跳过校验的分支 MUST 删除；yaml 文件应与 bold 文件一样接受完整 schema 校验，不再出现 stderr 警告或静默放行。

#### Scenario: yaml 文件不再被跳过
- **GIVEN** pdk_chat_demo 加载 `lib/loop/*.agent.md` yaml 文件
- **WHEN** 程序运行到 main.cpp 原 `is_yaml` 检测位置
- **THEN** 不再执行跳过校验的 `return`
- **AND** 文件继续进入 `DslValidator::validate()`

#### Scenario: 无 stderr 跳过警告
- **GIVEN** 成功构建 pdk_chat_demo
- **WHEN** 运行并加载 yaml agent 文件
- **THEN** `stderr` 不出现包含"跳过"或"skip validation"字样的警告
- **AND** `grep -n "跳过\|skip validation" examples/pdk_chat_demo/main.cpp` 返回 0