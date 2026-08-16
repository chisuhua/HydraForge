# pdk-chat-demo-dsl-validator Specification

## Purpose

定义 `examples/pdk_chat_demo/dsl_validator.{h,cpp}` 在生产非 mock 启动路径上的 YAML DSL 校验契约，确保 `.agent.md` 的 YAML fenced 块被结构化解析为节点校验输入、错误信息携带稳定路径、且不修改核心 parser。

继承 `yaml-frontmatter-detection` spec 的解析/格式检测/错误定位基础，本 spec 专注于 **生产 fixture 对接** 与 **demo/core 边界管理**。

## ADDED Requirements

### Requirement: production-yaml-fixture-validation

`DslValidator` MUST 在生产非 mock 启动路径上对 `lib/loop/*.agent.md` 的 YAML fenced 块执行完整结构化校验，所有生产 fixture 必须通过 `valid=true` 检查；任一 fixture 校验失败必须返回非零退出码阻止应用启动。

#### Scenario: react_golden fixture 通过非 mock 校验
- **GIVEN** `lib/loop/react.agent.md` 含合法 YAML fenced 块（含 `/__meta__` + `/main` 双块）
- **WHEN** `pdk_chat_demo` 以非 mock 模式加载该文件并调用 `DslValidator::validate()`
- **THEN** 返回 `ValidationResult{valid=true, errors=[]}`
- **AND** 应用进入聊天循环，不打印任何 yaml 校验诊断

#### Scenario: plan_execute_golden fixture 通过非 mock 校验
- **GIVEN** `lib/loop/plan_execute.agent.md` 含合法 YAML fenced 块
- **WHEN** 非 mock 启动路径调用 `DslValidator::validate()`
- **THEN** 返回 `valid=true`

#### Scenario: fork_join_golden fixture 通过非 mock 校验
- **GIVEN** `lib/loop/fork_join.agent.md` 含合法 YAML fenced 块
- **WHEN** 非 mock 启动路径调用 `DslValidator::validate()`
- **THEN** 返回 `valid=true`

#### Scenario: 任一 golden fixture 校验失败阻止启动
- **GIVEN** `lib/loop/*.agent.md` 任一文件 YAML 结构被破坏（如缺 `name` 必填字段）
- **WHEN** 非 mock 启动路径调用 `DslValidator::validate()`
- **THEN** 返回 `valid=false`，错误信息含 `MISSING_REQUIRED_FIELD` + `path=frontmatter.<field>`
- **AND** `pdk_chat_demo` 进程退出码非零，不进入聊天循环

### Requirement: structured-yaml-parsing

YAML fenced 块解析 MUST 使用 `yaml-cpp` + `agenticdsl::yaml_to_json` 完成完整 YAML→JSON 转换，禁止依赖正则或子串截取；frontmatter 必填字段从结构化 JSON 中按 `contains()` 取值，nodes 列表从结构化 JSON 中按 `nodes` 字段取值。

#### Scenario: YAML 注释不污染解析结果
- **GIVEN** 一个 YAML 块含注释行（`# 注释`）穿插在 nodes 列表中
- **WHEN** `DslValidator` 解析
- **THEN** 返回的 nodes JSON 数组不含注释内容
- **AND** `valid=true`

#### Scenario: 多行缩进 frontmatter 字段被正确解析
- **GIVEN** 一个 YAML 块含嵌套 frontmatter（如 `execution_budget.max_nodes: 25`）
- **WHEN** `DslValidator` 提取 frontmatter 字段
- **THEN** `execution_budget.max_nodes` 路径可被访问
- **AND** 字段值类型正确（数字/字符串/布尔）

#### Scenario: nodes 字段非数组触发 INVALID_NODES_TYPE
- **GIVEN** YAML 块含 `nodes: foo`（标量而非列表）
- **WHEN** `DslValidator` 解析
- **THEN** 返回错误 `type=INVALID_NODES_TYPE`, `path=nodes`
- **AND** 不触发 `PARSE_ERROR`（消除 fail-fast 副作用）

### Requirement: stable-error-paths

校验错误路径 MUST 使用点分隔格式：`frontmatter.<field>` / `node[N].<field>` / `yaml_block[L:C]`；CI 脚本和用户诊断可直接按路径 grep 定位错误源。

#### Scenario: frontmatter 缺失字段携带字段名
- **GIVEN** YAML 块缺 `version` 必填字段
- **WHEN** `DslValidator::validate()` 执行
- **THEN** 返回 `ValidationError{type=MISSING_REQUIRED_FIELD, path="frontmatter.version", ...}`
- **AND** `message` 字段含 `version` 字样

#### Scenario: node 字段错误携带数组索引
- **GIVEN** YAML 块含 3 个节点，第 2 个节点缺 `id`
- **WHEN** `DslValidator::validate()` 执行
- **THEN** 返回 `ValidationError{type=MISSING_REQUIRED_FIELD, path="node[1].id", ...}`

#### Scenario: YAML 语法错误携带行号列号
- **GIVEN** YAML 块第 3 行含缩进错误
- **WHEN** yaml-cpp 解析失败抛出 `YAML::ParserException`
- **THEN** 返回 `ValidationError{type=INVALID_YAML, path="yaml_block[3:0]", ...}`
- **AND** `path` 中 `[L:C]` 格式可被正则提取

### Requirement: non-fail-fast-collection

校验过程 MUST 收集所有结构错误（含 frontmatter 缺失 + 节点字段缺失 + 非法类型 + 工具依赖 + YAML 语法错误）而非 fail-fast；单次 `validate()` 调用返回的 `errors` 数量 ≥ 1 时，应用启动失败并打印所有错误。

#### Scenario: 多错误一并返回
- **GIVEN** YAML 块同时缺 `name`、`version`，且第 1 个节点缺 `id`
- **WHEN** `DslValidator::validate()` 执行
- **THEN** `errors.size() >= 3`
- **AND** 3 条错误分别携带 `frontmatter.name` / `frontmatter.version` / `node[0].id` 路径

#### Scenario: 节点遍历无意义时仍可提前返回
- **GIVEN** `nodes` 字段不存在（不是非法类型）
- **WHEN** `DslValidator::validate()` 执行
- **THEN** 返回 `MISSING_SECTION` 错误后**提前返回**（不进入节点遍历）
- **AND** `errors.size() == 1`

### Requirement: line-ending-compatibility

同一合法 `.agent.md` 使用 LF 或 CRLF 行尾 MUST 产生等价校验结果；YAML fenced 块提取与 `yaml-cpp` 解析对两种行尾透明。

#### Scenario: LF fixture 校验通过
- **GIVEN** 一个生产 fixture 的 LF 版本（`\n` 行尾）
- **WHEN** `DslValidator::validate()` 执行
- **THEN** 返回 `valid=true`

#### Scenario: CRLF fixture 校验通过
- **GIVEN** 同一 fixture 的 CRLF 版本（`\r\n` 行尾）
- **WHEN** `DslValidator::validate()` 执行
- **THEN** 返回 `valid=true`
- **AND** 错误信息（若有）行号偏移与 LF 版本一致（不出现双重计数）

### Requirement: mock-mode-skip-contract

`pdk_chat_demo` 在 mock 启动模式下 MUST 跳过 `DslValidator::validate()` 调用，且跳过行为不打印 YAML 校验诊断；本提案不改变 mock 启动契约。

#### Scenario: mock 模式不调用 validator
- **GIVEN** `pdk_chat_demo` 以 `--mock` flag 启动
- **WHEN** 主程序加载 `.agent.md`
- **THEN** 不调用 `DslValidator::validate()`
- **AND** `stderr` 不出现 yaml 校验相关警告

#### Scenario: 5 个 mock fixture 全部启动成功
- **GIVEN** 5 个现有 mock 模式启动 fixture（含合法 + 含部分结构问题）
- **WHEN** `pdk_chat_demo --mock` 加载每个 fixture
- **THEN** 全部启动成功，进入聊天循环
- **AND** 行为与 change 前完全一致

### Requirement: core-parser-immutability

本提案 MUST NOT 修改 `src/modules/parser/` 下任何文件、不新增核心 parser API；所有改动局限于 `examples/pdk_chat_demo/dsl_validator.{h,cpp}` 与 `examples/pdk_chat_demo/tests/`。

#### Scenario: src/modules/parser/ 目录零改动
- **GIVEN** 本提案 ship 后的 git diff
- **WHEN** `git diff HEAD~1 -- src/modules/parser/` 执行
- **THEN** 返回空（0 行变更）

#### Scenario: 核心 MarkdownParser 公开 API 不变
- **GIVEN** 现有代码调用 `MarkdownParser::parse_metadata(content)` 或相关 API
- **WHEN** 重新编译 `agenticdsl_core`
- **THEN** 零编译错误，无需修改任何调用点

#### Scenario: demo/core 边界清晰
- **GIVEN** `examples/pdk_chat_demo/dsl_validator.cpp` 实现
- **WHEN** 检查头文件 include
- **THEN** 可引用 `src/common/utils/yaml_json.h`（公共工具）
- **AND** 不可引用 `src/modules/parser/`（核心 parser）

### Requirement: adr-0058-boundary

工具 input/output JSON Schema 校验（MUST 由 ADR-0058 负责）不在本提案范围；本提案仅校验 `.agent.md` 文档级与 DSL 图结构级错误，工具调用路径的 schema 校验由 ADR-0058 独立 change 处理。

#### Scenario: 工具 schema 校验不回归
- **GIVEN** `pdk_chat_demo` 调用 `call_tool` 工具
- **WHEN** 工具输入违反 ADR-0058 schema
- **THEN** 错误来源为 ADR-0058 validator，非本提案的 `DslValidator`
- **AND** 两条错误链路独立（DS L 校验错误 ≠ 工具 schema 错误）