## Context

当前 `LLMProviderFactory` 位于 `src/common/llm/llm_provider_factory.{h,cpp}`，以 `LLMConfig::provider` 为路由键，在构造时固定持有 mock、cloud、llama 三类内部 factory。`create()` 可以并发调用，但 provider 路由表本身是编译期分支，运行中没有 `register_dynamic()`、默认 provider 状态或可替换的动态定义。因此，配置只能通过启动时的 `llm_config.json` 生效，无法支持 chat 会话中的 provider 注册、刷新和切换。

`pdk/provider_agent/` 已提供一套相邻但独立的 provider 配置能力。`ProviderInfo` 描述 `id`、API URL、endpoint、延迟解析的 API key 环境变量和模型目录，`ProviderRegistry` 使用互斥锁保护 `register_providers()`、`list_providers()`、`resolve()` 和 `health()`。现有四个 provider agent 工具覆盖 provider 配置解析、健康检查以及 loop agent 的父 provider 绑定和执行路径，但尚未提供 `provider/refresh`、`provider/register_dynamic` 或 `provider/switch`。本 change 将复用现有 `ProviderRegistry` 的数据模型和工具注册方式，同时把可创建 provider 的动态定义接入 `LLMProviderFactory`。

Wave 2 的 `chat-streaming-slash-tui` 依赖统一的 provider 列表和 `/model` 运行时切换，其中 slash 命令最终应调用 `provider/switch`。Wave 3 的 `chat-async-io-steering` 继续依赖同一切换能力，将用户意图转换为会话级 provider 选择。因而本 change 提供稳定的 L2 工具和 factory 原子 API，但不实现 slash 命令、TUI 或异步编排本身。

运行时访问必须考虑多个 `CognitiveWorker` 并发调用 LLM。读取 provider 定义、创建 provider、刷新目录和切换默认 provider 可能交错发生，不能以无保护的普通 map 或裸指针共享状态。工具仍须经过 `IToolRegistry` 和 `ToolCoordinator` 的治理路径，遵循 ADR-0031 §决策 5 的 ToolCoordinator Option C，以及 ADR-0004 V2 的 `ToolMetadata`、layer 和 approval 约束。

## Goals / Non-Goals

**Goals:**

- 增加 `LLMProviderFactory::register_dynamic()`，允许运行中的引擎注册合法 provider 定义，并保证注册、解析、创建和默认 provider 切换线程安全。
- 增加 `provider/refresh` 工具，从 provider 上游 API 获取最新模型目录，更新注册表并让新增模型可见，同时对下线模型保留可诊断状态。
- 增加 `provider/register_dynamic` 工具，将受校验的 provider 定义转换为 factory 可创建的动态 provider，并在无需重启的情况下立即可解析。
- 增加 `provider/switch` 工具，通过原子状态变更切换默认 provider，使后续 LLM 调用路由到目标 provider，并让 `provider/list` 反映当前默认。
- 让上述工具通过 `IToolRegistry` 注册并由 `ToolCoordinator` 执行，使用 ADR-0004 V2 的 `ToolMetadata` 表达 Workflow layer、approval policy 和允许的 layer。
- 保持现有 `llm_config.json` 构造时配置路径和未知 provider 的兼容兜底行为，不要求既有调用方迁移。
- 刷新失败时保留最近一次成功的模型目录和 provider 定义，返回可观察的 warning，而不是中断正在运行的服务。

**Non-Goals:**

- provider 连接池、连接复用和请求级负载均衡，留给后续 runtime 或 model-router change。
- `--provider` 等 CLI 参数，属于 `cli-args-cxxopts` change。
- `/model` slash 命令和完整 TUI 交互。它们会消费本 change 的 `provider/switch`，但不在此实现。
- 多 provider 并行推理、fleet 调度和复杂路由策略，按 ADR-0030 V2 保持 defer。
- thinking_level 抽象、Cloud provider 新接入和 provider 专属高级参数协议。
- 持久化动态注册结果到配置文件。当前注册状态属于运行时，重启后仍由既有配置加载。

## Decisions

### Decision 1: 使用 `std::shared_mutex` 保护 factory 状态

**Rationale:**

`LLMProviderFactory` 的主要操作是高频读取，动态注册、刷新和默认切换是低频写入。将 provider 定义、factory 回调和默认 provider 放在同一受保护状态中，读取路径使用 `std::shared_lock`，写入路径使用 `std::unique_lock`，可以直接表达读多写少的访问模型。创建 provider 时先在锁内复制必要的定义或回调，再在锁外执行可能构造网络客户端、读取环境变量或分配资源的操作，避免长时间持有写锁。

**Alternatives Considered:**

- **`std::atomic` 整体状态交换**：适合不可变快照，但需要把所有 factory 回调、目录和默认状态包装成共享快照，并处理回调生命周期，改动明显大于当前需求。
- **copy-on-write**：读路径简单，但每次刷新都要复制完整模型目录和回调表，且容易把 ProviderRegistry 与 LLMProviderFactory 的所有权边界混在一起。当前规模不值得引入。
- **普通 `std::mutex`**：实现简单，但会让多个 CognitiveWorker 的并发解析和列表查询串行。保留为低复杂度 fallback，但不是首选。

### Decision 2: refresh 失败保留旧目录并返回 warning

**Rationale:**

模型目录刷新属于控制面操作，不应让暂时的上游网络错误破坏已经可用的 provider。刷新采用两阶段流程：先在锁外请求并解析完整的新目录，校验通过后再以一次短写锁替换目录和 `last_refresh` 元数据。请求失败、JSON schema 不合法或目录为空时，不提交部分结果，保留旧目录，返回 `ok=false`、稳定错误码和 warning。这样 `provider/list` 仍可服务，调用方能区分“刷新失败”与“没有 provider”。

**Alternatives Considered:**

- **fail-loud 并清空目录**：状态更显式，但会把短暂的 API 故障放大为后续调用全面失败，不符合不中断服务的能力约束。
- **静默忽略失败**：保持服务但失去诊断信息，无法支持 TUI 和运维观察，不采用。

### Decision 3: `provider/switch` 是 ToolCoordinator 工具，不是高层直调

**Rationale:**

provider 切换是一个原子状态变更能力，和工具注册、layer 校验、approval、审计保持一致。按照 ADR-0031 §决策 5，L2 负责工具原子能力，L4 负责编排。`provider/switch` 通过 `IToolRegistry` 注册，使用 `ToolCoordinator` 执行，工具内部只校验 provider 存在并原子更新默认选择。未来 `/model`、chat steering 或其他编排器都调用同一个受治理入口，不复制权限和审计逻辑。

**Alternatives Considered:**

- **直接公开 `factory.switch_default()` 给 chat 层**：容易绕过 ADR-0004 V2 的 layer 和 approval 约束，也会产生多个切换语义。
- **把切换塞进 L4 chat 编排器**：违反原子能力与编排分层，且无法让 DSL、TUI 和 slash 命令共享同一治理路径。

### Decision 4: 动态注册接收 `register_dynamic(name, factory_fn)`

**Rationale:**

factory 接收 provider 名称和可复制、可存储的 factory callback，调用时再以当前 `LLMConfig` 创建 `ILLMProvider`。这种 API 保持 factory 对具体 provider 实例的非拥有关系，避免把有状态、通常不可复制且可能含线程同步的 provider 对象在多个 worker 间共享。callback 可捕获不可变配置或共享资源，并在注册时完成名称、空回调和重复策略校验。`provider/register_dynamic` 工具只负责把 JSON 定义转换为 callback 所需的配置，不把原始 API key 写入日志或持久化。

**Alternatives Considered:**

- **`register_dynamic(name, provider_instance)`**：简单，但实例所有权、并发 generate 安全性、请求间状态隔离和销毁时机都变成 factory 的责任，不适合作为默认 API。
- **新增完整 `IProviderFactory` 子类注册 API**：类型边界更强，但工具层需要动态加载具体 C++ 类型，超出本 change，且会扩大 ABI 面。

## Risks / Trade-offs

- **并发 refresh 与 switch 竞争**：刷新写入目录时可能与切换同时发生。缓解方式是使用同一 `shared_mutex` 保护 catalog 和默认 provider，并要求每次操作在短临界区内完成。切换只允许已提交的 provider，绝不读取半成品目录。
- **上游 API 故障造成 catalog 过期**：保留旧目录保障可用性，但用户可能看不到最新模型。通过 `last_refresh`、warning 和健康状态暴露陈旧信息，后续可加入 TTL 和后台刷新，不在本 change 中隐式清除旧值。
- **动态 callback 捕获生命周期错误**：注册方可能捕获已销毁对象。API 文档和工具实现只允许捕获值或 `shared_ptr`，测试覆盖注册后销毁临时配置的场景；factory 不接受裸 owning 指针。
- **默认 provider 与构造时 provider 的语义差异**：既有 `create(config)` 仍优先使用显式 `config.provider`，只有未指定 provider 或会话选择要求默认路由时才读取动态默认值。这样保持 `llm_config.json` 行为等价，避免隐式改变旧调用方。
- **ToolMetadata 配置错误导致工具不可用**：按 ADR-0004 V2 为三个工具补齐 category、approval、allowed_layers，并增加 Cognitive、Thinking、Workflow 三类 layer 的拒绝和准入测试。

## Migration Plan

无数据迁移。启动时继续加载 `llm_config.json`，动态 provider 仅存在于当前进程。升级后新增工具由 `pdk/provider_agent` 注册，旧 DSL 和既有 provider 名称继续使用原有 factory 路由。回滚时移除动态工具注册和 factory 扩展即可，不需要转换配置文件。

验证顺序为：先跑 factory 单元和并发测试，再跑 provider agent 工具测试，最后执行完整 `ctest --output-on-failure` 和 `openspec validate provider-dynamic-discovery --json`。
