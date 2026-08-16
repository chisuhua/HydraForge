# from-roadmap-phase-6b-platform

## Why

`examples/pdk_chat_demo/dsl_validator.{h,cpp}` 已承担 `.agent.md` DSL 图结构校验，`examples/pdk_chat_demo/main.cpp` 已在非 mock 启动路径调用它。生产 `.agent.md` 使用 YAML fenced DSL，但当前 YAML 节点内容未可靠转换为现有节点校验输入，导致生产校验链存在失效风险。本提案采用扩展现有 `DslValidator` 的方案，不把 demo 级契约下沉到核心 `src/modules/parser/`，并复用已有 YAML fenced 块识别及 LF/CRLF 兼容逻辑。

本提案与 ADR-0058 保持边界：ADR-0058 负责工具 input/output JSON Schema 校验，本提案只负责 `.agent.md` 文档和 DSL 图结构校验。

## What Changes

**In Scope**:

- 在现有 `DslValidator` 中将 YAML fenced DSL 解析为统一的结构化节点输入。
- 继续校验 frontmatter 必填字段、节点类型白名单、节点必填字段和可选的工具注册依赖。
- 保持非 mock 模式校验失败即打印诊断并阻止启动。
- 增加生产 YAML fixture、非法 YAML/节点字段、工具依赖和 LF/CRLF 回归测试。

### 关键场景

- GIVEN 一个包含合法 `AgenticDSL` YAML fenced 块的 `.agent.md` 文件
  WHEN 非 mock 的 `pdk_chat_demo` 加载该文件
  THEN `DslValidator` 能解析 YAML 节点并返回 `valid=true`，应用继续启动。

- GIVEN YAML fenced 块缺少 `name`、`version`、`agent_loop` 或节点 `id`、`type` 等必填字段
  WHEN validator 执行校验
  THEN 返回对应路径和错误类型的 `ValidationError`，非 mock 启动失败且不进入聊天循环。

- GIVEN 节点类型不在白名单，或 `call_tool` 引用未注册工具
  WHEN validator 搭配 `IToolRegistry` 校验
  THEN 返回明确的节点路径和依赖错误，不执行有问题的 DSL。

- GIVEN 同一合法 `.agent.md` 使用 LF 或 CRLF 行尾
  WHEN validator 解析 YAML fenced 块
  THEN 两种格式产生等价的校验结果。

- GIVEN mock 模式加载同一 `.agent.md`
  WHEN 应用启动
  THEN 保持现有跳过 DSL 校验的行为，不因本提案改变 mock 启动契约。

**Out of Scope**:

- 不改变 mock 模式当前跳过 DSL 校验的行为。
- 不迁移或重构核心 `MarkdownParser`，不新增核心 parser API。
- 不实现 ADR-0058 的工具 input/output JSON Schema 执行时校验。
- 不负责 DAG 循环检测、LLM 输出 schema 或 Plugin manifest schema。

## Capabilities

- MUST 复用现有 `DslValidator` 的 `ValidationResult` / `ValidationError` 错误模型和非 mock 启动失败路径。
- MUST 对 YAML fenced 内容做结构化解析，禁止将原始 YAML 列表直接当作 JSON 文本解析。
- MUST 保留 LF、CRLF 及现有 Markdown bold 兼容路径，避免回归已通过的 fixture。
- SHOULD 收集同一输入中的多个结构错误，并输出稳定的字段/节点路径，便于 CI 和用户诊断。

## Impact

- MUST NOT 修改 ADR-0058 工具 schema 校验边界或把 demo 校验逻辑扩散到核心 parser。

## Acceptance

- [ ] 生产 `lib/loop/*.agent.md` 的 YAML fenced 格式在非 mock 启动路径通过真实 `DslValidator` 校验。
- [ ] 缺少 frontmatter、节点字段、非法节点类型和未注册工具的测试均能稳定失败并返回明确错误。
- [ ] LF 与 CRLF fixture 均通过，现有 Markdown bold fixture 不回归。
- [ ] mock 模式仍显示跳过校验并保持现有测试行为。
- [ ] `ctest -R 'pdk_chat|dsl_validation' --output-on-failure` 通过，且相关目标构建成功。
- [ ] 代码和测试变更不引入核心 `src/modules/parser/` API 修改。

