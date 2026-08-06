# provider-dynamic-discovery

## Why

- `LLMProviderFactory` 仅支持构造时配置，无 `register_dynamic()`——pi-agent `pi.registerProvider()` 借鉴路径（§十）的前置依赖。
- `pdk/provider_agent/` 已注册 4 个工具但缺 `provider/refresh`（拉取模型目录）、`provider/register_dynamic`、`provider/switch`。
- `/model` 运行中切换（chat-async-io-steering 提案）依赖本提案的 switch 能力。

## What Changes

**In Scope**:

- (TBD)

### 关键场景

- GIVEN 运行中的引擎，WHEN 调用 `provider/register_dynamic`（合法定义），THEN 新 provider 立即可被 factory 解析，无需重启。
- GIVEN provider API 可达，WHEN `provider/refresh`，THEN 模型目录更新（新增模型可见，下线模型标记）。
- GIVEN 多 provider 注册，WHEN `provider/switch <name>`，THEN 后续 LLM 调用路由到目标 provider，`provider/list` 反映当前默认。

**Out of Scope**:

- (TBD)

## Capabilities

- MUST 运行时注册线程安全（factory 可能被多 CognitiveWorker 并发访问）。
- MUST `provider/switch` 走 IToolRegistry 注册的工具路径（L2 原子能力），编排逻辑留在 L4。
- MUST NOT 破坏既有构造时配置路径（llm_config.json 行为等价）。
- SHOULD refresh 失败保留旧目录 + 告警（不中断服务）。

## Impact

- MUST 运行时注册线程安全（factory 可能被多 CognitiveWorker 并发访问）。
- MUST `provider/switch` 走 IToolRegistry 注册的工具路径（L2 原子能力），编排逻辑留在 L4。
- MUST NOT 破坏既有构造时配置路径（llm_config.json 行为等价）。
- SHOULD refresh 失败保留旧目录 + 告警（不中断服务）。

## Acceptance

- 3 个新工具注册与调用测试通过（含并发注册）。
- 既有 llm_config.json 加载路径零回归；ctest 全量零回归。

