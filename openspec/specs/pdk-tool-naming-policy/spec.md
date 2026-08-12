# pdk-tool-naming-policy Specification

## Purpose

PDK 工具命名强制规范: 明确定义 SLASH 为唯一合法分隔符、DOT 仅允许 C++ method 调用、3 段式命名空间约定 (`inference/{component}/{action}`)。本 spec 是 `adr-doc-alignment-hotfix` 的扩展，增加命名一致性强制要求。

## Requirements

### Requirement: pdk-tool-slash-only-separator

所有 PDK 工具注册名 MUST 使用 SLASH (`/`) 作为命名空间分隔符。DOT (`.`) 仅允许在 C++ method name 上下文中使用。

#### Scenario: 工具注册名验证

- **WHEN** 解析 `lib/inference/*.md` DSL schema 中的 `tool:` 字段
- **THEN** 工具名 MUST 匹配 `{namespace}/{component}/{action?}` 格式 (SLASH 分隔)
- **AND** 工具名 MUST NOT 包含 DOT (如 `prefix_cache.configure`)
- **AND** `grep -E 'tool: [a-z_]+\.[a-z]+' lib/inference/*.md` 输出 MUST 为空

#### Scenario: PDK 代码工具名注册

- **WHEN** 读取 `pdk/llama_engine/src/*.cpp` 中的 `register_tool_function("...", ...)` 调用
- **THEN** 第一参数 (工具名) MUST 使用 SLASH 分隔
- **AND** `grep 'register_tool_function.*\.[a-z]' pdk/llama_engine/src/*.cpp` 输出 MUST 为空

#### Scenario: 测试断言工具名一致性

- **WHEN** 测试文件中的 `registry.call_tool("...", ...)` 调用字符串
- **THEN** 工具名 MUST 与对应的 DSL schema + PDK 注册名一致
- **AND** `grep 'call_tool\|has_tool\|tool_metas.at' tests/test_llama_engine_plugin.cpp | grep -E '\.[a-z]+'` 输出 MUST 为空 (排除 C++ method 调用)

### Requirement: inference-namespace-3-segment-convention

`inference/` 命名空间下的所有工具 MUST 使用 3 段式命名：`inference/{component}/{action}`。

#### Scenario: 架构工具 3 段命名

- **WHEN** 工具属于推理引擎架构层 (prefix_cache, kv_cache, decoding, cloud_engine, batching)
- **THEN** 工具名 MUST 为 `inference/{component}/{action}` 格式
- **AND** prefix_cache → `inference/prefix_cache/configure`
- **AND** kv_cache → `inference/kv_cache/configure`
- **AND** decoding → `inference/decoding/configure`
- **AND** cloud_engine → `inference/cloud_engine/configure`
- **AND** batching → `inference/batching/submit_and_wait`

#### Scenario: 与 engine/model 命名保持一致

- **WHEN** 比较架构工具与 engine/model 工具命名
- **THEN** 架构工具 MUST 使用与 `inference/engine/init` 一致的 `inference/` 前缀
- **AND** 架构工具 MUST NOT 使用不含 `inference/` 前缀的 2 段式命名 (如 `prefix_cache/configure`)

### Requirement: decisions-d3-slash-uniformity

`docs/adversarial-reviews/decisions-2026-07-07.md` D3 章 MUST 反映 SLASH 统一决策，不保留 "C13 架构工具命名边界" 反向小节。

#### Scenario: D3 不再排除架构工具

- **WHEN** 读取 D3 章 "C13 架构工具命名边界" 小节
- **THEN** 该小节 MUST 不存在（已删除）
- **AND** D3 映射表 MUST 包含架构工具的 SLASH 名称
- **AND** 架构工具命名规则 MUST 明确为 `inference/{component}/{action}`

#### Scenario: D3 决策影响段更新

- **WHEN** 读取 D3 "影响" 段
- **THEN** "C13 schema 文件: 4 处架构工具命名（不应用 D3 重写）" MUST 改为 "C13 schema 文件: 4 处架构工具命名应用 D3 SLASH 重写"
