# provider-dynamic-discovery Specification

## Purpose

定义 AgenticDSL 运行时 provider 动态发现、注册、刷新和切换契约。该能力扩展 `LLMProviderFactory` 的构造时路由，使运行中的引擎可以安全管理 provider catalog，同时通过 `IToolRegistry` 和 `ToolCoordinator` 暴露受 ADR-0031 §决策 5、ADR-0004 V2 约束的 provider 工具。该 spec 不实现 slash 命令、TUI、CLI 参数或多 provider 并行调度。

## ADDED Requirements

### Requirement: provider-factory-thread-safe

`LLMProviderFactory` MUST 支持 `register_dynamic(name, factory_fn)`，并使用线程安全状态保护动态 provider 定义、provider catalog 和默认 provider。读取、创建、注册和默认切换 MUST 不观察到半成品状态。显式 `LLMConfig::provider` 的既有构造时路由 MUST 保持兼容。

#### Scenario: 运行时注册后立即创建
- **GIVEN** 一个正在运行的 `LLMProviderFactory`
- **WHEN** 调用 `register_dynamic("runtime-provider", factory_fn)` 并随后调用 `create()`
- **THEN** `create()` 可以解析并创建 `runtime-provider`
- **AND** 不需要重启引擎或重新读取 `llm_config.json`

#### Scenario: 并发读写不产生半成品
- **GIVEN** 多个 CognitiveWorker 并发调用 `create()`、provider 查询和动态注册
- **WHEN** 一个线程注册或替换 provider 定义，其他线程同时读取 catalog
- **THEN** 每次读取看到完整的旧定义或完整的新定义
- **AND** 不发生数据竞争、崩溃或空 callback 调用

#### Scenario: 构造时配置保持兼容
- **GIVEN** `LLMConfig::provider` 为既有 `mock`、cloud provider 或 `local`
- **WHEN** 调用 `LLMProviderFactory::create(config)`
- **THEN** 路由行为与动态发现前一致
- **AND** 未知 provider 仍遵循既有安全兜底行为

### Requirement: provider-refresh-tool

系统 MUST 提供名为 `provider/refresh` 的 ToolCoordinator 工具，从指定 provider 的上游 API 拉取并校验模型目录。成功刷新 MUST 原子提交新目录，新增模型可见，下线模型 MUST 以可诊断状态标记。工具 MUST 使用 ADR-0004 V2 `ToolMetadata` 注册，并不得在结果或日志中泄露 API key。

#### Scenario: 上游可达且目录有效
- **GIVEN** provider API 返回包含有效模型列表的响应
- **WHEN** Workflow layer 调用 `provider/refresh`
- **THEN** provider catalog 替换为最新目录
- **AND** 新增模型可通过 provider/list 或 resolve 查询
- **AND** 响应包含 added、removed、model_count 和 last_refresh 信息

#### Scenario: 下线模型被标记
- **GIVEN** 旧目录包含 model-a 和 model-b，上游新目录只包含 model-a
- **WHEN** `provider/refresh` 成功提交
- **THEN** model-b 不再作为可用模型返回
- **AND** 响应或 catalog 元数据明确标记 model-b 为 removed/stale

#### Scenario: 非 Workflow layer 被治理拒绝
- **GIVEN** 当前 layer 为 Cognitive 或 Thinking
- **WHEN** 调用 `provider/refresh`
- **THEN** ToolCoordinator 返回 `PermissionDenied`
- **AND** 上游 API 不被调用
- **AND** provider catalog 不发生变化

### Requirement: provider-register-dynamic-tool

系统 MUST 提供名为 `provider/register_dynamic` 的工具，将经过 schema 校验的 provider 定义注册到 `LLMProviderFactory` 和 provider catalog。合法注册 MUST 在当前进程内立即可解析，重复名称、空名称、非法 backend 或不完整定义 MUST 返回结构化错误且不得修改已有 provider。

#### Scenario: 合法定义运行时注册
- **GIVEN** 工具参数包含非空 provider name、合法 backend、API URL 和至少一个模型定义
- **WHEN** Workflow layer 调用 `provider/register_dynamic`
- **THEN** factory 注册新的 provider callback
- **AND** 后续 `create()` 与 `provider/list` 立即可以看到该 provider
- **AND** 不需要重启或修改原始配置文件

#### Scenario: 非法定义被拒绝
- **GIVEN** provider name 为空，或模型目录为空，或 backend 不受支持
- **WHEN** 调用 `provider/register_dynamic`
- **THEN** 返回 `ok=false` 和稳定 validation error_code
- **AND** factory、catalog 和当前默认 provider 均保持不变

#### Scenario: 重复注册不覆盖旧定义
- **GIVEN** provider `runtime-provider` 已经注册
- **WHEN** 再次以同名 provider 调用 `provider/register_dynamic`
- **THEN** 返回 duplicate-provider 错误
- **AND** 旧 callback、模型目录和默认状态保持不变

### Requirement: provider-switch-tool

系统 MUST 提供名为 `provider/switch` 的 ToolCoordinator 工具。工具 MUST 只允许切换到已注册 provider，并以原子操作更新默认 provider。成功后后续未指定 provider 的 LLM 创建请求 MUST 路由到目标 provider，`provider/list` MUST 反映当前默认。

#### Scenario: 切换到已注册 provider
- **GIVEN** provider-a 和 provider-b 均已注册，当前默认是 provider-a
- **WHEN** Workflow layer 调用 `provider/switch`，参数为 provider-b
- **THEN** 返回 `ok=true` 且 current_default 为 provider-b
- **AND** 后续默认路由的 LLM 创建请求使用 provider-b
- **AND** `provider/list` 标记 provider-b 为当前默认

#### Scenario: 切换到未知 provider
- **GIVEN** provider-missing 未注册
- **WHEN** 调用 `provider/switch`，参数为 provider-missing
- **THEN** 返回 `ok=false` 和 unknown-provider error_code
- **AND** 当前默认 provider 不发生变化

#### Scenario: switch 经过 ToolCoordinator 治理
- **GIVEN** 当前 layer 为 Cognitive 或 Thinking
- **WHEN** 调用 `provider/switch`
- **THEN** ToolCoordinator 返回 `PermissionDenied`
- **AND** factory 默认 provider 不发生变化
- **AND** 不执行任何 LLM 调用

### Requirement: refresh-failure-preserves-catalog

`provider/refresh` 在网络错误、上游超时、非法 JSON、schema 校验失败或空目录时 MUST 保留最近一次成功的 provider 定义和模型目录。失败结果 MUST 提供 warning 或稳定 error_code，且 MUST NOT 清空可用 catalog、覆盖默认 provider 或中断其他 worker 的正常调用。

#### Scenario: 网络失败保留旧目录
- **GIVEN** provider catalog 最近一次成功包含 model-a，当前默认 provider 可用
- **WHEN** 上游 API 在 refresh 时超时或返回网络错误
- **THEN** refresh 返回 `ok=false`、warning 或 retryable error_code
- **AND** model-a 仍可被 list、resolve 和后续 LLM 调用使用
- **AND** current_default 不发生变化

#### Scenario: 非法响应不提交部分结果
- **GIVEN** 上游响应包含新增 model-b 但不满足 schema 校验
- **WHEN** `provider/refresh` 处理该响应
- **THEN** refresh 返回 validation error
- **AND** model-b 不会以半成品写入 catalog
- **AND** 之前成功的目录和 provider 定义保持不变

#### Scenario: 失败刷新与切换并发
- **GIVEN** refresh 正在处理失败的上游响应，同时另一个 worker 请求切换到已注册 provider-b
- **WHEN** 两个操作完成
- **THEN** refresh 不会清空 provider-b 或覆盖其目录
- **AND** switch 的成功结果保持可见
- **AND** 其他 worker 不观察到空 catalog 或部分更新
