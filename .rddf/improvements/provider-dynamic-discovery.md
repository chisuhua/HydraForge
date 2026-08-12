# provider-dynamic-discovery

**优先级**: P2 | **来源**: layer-based-missing-capabilities-analysis.md §五 L1-5 + §六 L2-1（工厂运行时注册 + provider_agent 动态工具）
**阶段**: wave-3 | **分类**: core-impl
**类型**: feature

## 架构依据
- `LLMProviderFactory` 仅支持构造时配置，无 `register_dynamic()`——pi-agent `pi.registerProvider()` 借鉴路径（§十）的前置依赖。
- `pdk/provider_agent/` 已注册 4 个工具但缺 `provider/refresh`（拉取模型目录）、`provider/register_dynamic`、`provider/switch`。
- `/model` 运行中切换（chat-async-io-steering 提案）依赖本提案的 switch 能力。

## 范围
- **In Scope**: `LLMProviderFactory::register_dynamic()`（运行时新增 provider 定义）；`provider/refresh`（从 provider API 拉取最新模型目录并更新注册表）；`provider/register_dynamic` 工具；`provider/switch`（切换默认模型，会话级生效）。
- **Out Scope**: thinking_level 抽象（依赖 provider 支持）；多 provider 负载均衡（ADR-0034 model_router 领域）；Cloud provider 新接入（独立任务）。

## 关键场景
- GIVEN 运行中的引擎，WHEN 调用 `provider/register_dynamic`（合法定义），THEN 新 provider 立即可被 factory 解析，无需重启。
- GIVEN provider API 可达，WHEN `provider/refresh`，THEN 模型目录更新（新增模型可见，下线模型标记）。
- GIVEN 多 provider 注册，WHEN `provider/switch <name>`，THEN 后续 LLM 调用路由到目标 provider，`provider/list` 反映当前默认。

## 技术约束
- MUST 运行时注册线程安全（factory 可能被多 CognitiveWorker 并发访问）。
- MUST `provider/switch` 走 IToolRegistry 注册的工具路径（L2 原子能力），编排逻辑留在 L4。
- MUST NOT 破坏既有构造时配置路径（llm_config.json 行为等价）。
- SHOULD refresh 失败保留旧目录 + 告警（不中断服务）。

## 验收标准
- 3 个新工具注册与调用测试通过（含并发注册）。
- 既有 llm_config.json 加载路径零回归；ctest 全量零回归。
