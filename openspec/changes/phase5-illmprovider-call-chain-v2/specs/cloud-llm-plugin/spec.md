# cloud-llm-plugin Specification

## Purpose

`CloudLLMAdapter` 从核心代码 `src/common/llm/` 移至 `pdk/cloud/` 作为 first-party PDK plugin — 经 Oracle 长期演进分析决议(2026-07-06,推翻 ADR-0042 §4 原"cloud 留核心"决议),所有 backend(cloud + local)统一走 PDK plugin 机制。`LLMProviderFactory` 改为薄路由,通过 `pdk_create_llm_provider()` 工厂符号 dlopen cloud plugin。

## ADDED Requirements

### Requirement: cloud-plugin-export-symbols (REQ-CLP-001)

Cloud plugin MUST 导出 5 个 PDK 符号(per ADR-0041),含 2 个必选 + 3 个可选。

#### Scenario: 5 符号完整导出

- **WHEN** 编译 `pdk/cloud/src/cloud_plugin.cpp`
- **THEN** MUST 导出以下 5 个 `extern "C"` 符号:
  - `extern "C" const PluginInfo pdk_plugin_info` — **必选**(数据符号,非函数),包含 ABI 版本 + 元数据
  - `void pdk_register_tools(IToolRegistry& reg)` — **必选**,MVP 实现为空(cloud plugin 无 DSL 工具)
  - `std::shared_ptr<::agenticdsl::ILLMProvider> pdk_create_llm_provider(const PdkProviderConfig* cfg)` — **可选**,返回 `CloudLLMAdapter` 实例 (**注意**: `PdkProviderConfig` 为纯 POD struct,跨 .so ABI 安全,定义见 [PdkProviderConfig 定义](#pdkproviderconfig-定义))
  - `bool pdk_plugin_init()` — **可选**,初始化 httplib 连接池 + API key 缓存
  - `void pdk_plugin_fini()` — **可选**,释放 httplib 资源 + 清 API key 缓存

#### Scenario: PluginInfo 元数据

- **WHEN** 检查 `pdk_plugin_info` 数据符号指向的 `PluginInfo`
- **THEN** MUST 含:
  - `abi_version = 2`(per ADR-0041 §1.5 v2)
  - `name = "pdk_cloud"`
  - `major_version = 1`, `minor_version = 0`, `patch_version = 0`
  - `description = "First-party cloud LLM plugin (OpenAI/Anthropic/DeepSeek/Qwen/Moonshot compatible)"`
  - `capabilities = "illmprovider,cloud,http"`
  - `dependencies = ""`(无依赖,作为根 plugin)

#### Scenario: namespace 统一

- **WHEN** `pdk_create_llm_provider()` 返回类型
- **THEN** MUST 是 `std::shared_ptr<::agenticdsl::ILLMProvider>`(per ADR-0042 §1 namespace 统一)
- **AND** MUST NOT 是 `hydraforge::ILLMProvider`(per AGENTS.md 命名空间约定)

### Requirement: anthropic-protocol-branch (REQ-CLP-002)

Cloud plugin MUST 在 `pdk_create_llm_provider()` 内部根据 `config.provider` 字符串分发到 4 个协议实现,修复当前"anthropic 路由到不存在的实现"bug。

#### Scenario: provider 字符串路由

- **WHEN** 调用 `pdk_create_llm_provider(config)`
- **THEN** MUST 按 `config.provider` 字符串路由:
  - `"openai"` / `"deepseek"` / `"qwen"` / `"moonshot"` / `"custom"` → OpenAI 兼容协议(共享 `OpenAICompatAdapter` 内部类)
  - `"anthropic"` → Anthropic 协议(独立 `AnthropicAdapter` 内部类,支持 `/v1/messages` endpoint + `x-api-key` header)
- **AND** MVP 阶段 Anthropic 协议 MUST 仅实现核心 4 个方法:`generate` / `generate_stream` / `available_models` + 错误映射

#### Scenario: Anthropic SSE 流式

- **WHEN** `AnthropicAdapter::generate_stream(req, token)` 被调用
- **THEN** MUST 构造 Anthropic 协议 `POST /v1/messages` 请求,`stream: true` header
- **AND** MUST 解析 Anthropic SSE 格式:`event: content_block_delta\ndata: {"type":"content_block_delta","index":0,"delta":{"type":"text_delta","text":"..."}}`
- **AND** MUST 在 `next(token)` 中返回 `delta.text` 内容
- **AND** MUST 处理 Anthropic `message_stop` 事件作为流结束信号

#### Scenario: 错误映射

- **WHEN** cloud plugin 接收到 4xx/5xx HTTP response
- **THEN** MUST 映射到 `LLMError::Code`:
  - 401/403 → `LLMError::Code::AuthenticationError`
  - 429 → `LLMError::Code::RateLimited` (含 `retry_after`)
  - 5xx → `LLMError::Code::ServerError`
  - 网络错误 → `LLMError::Code::NetworkError`
  - 请求格式错误 → `LLMError::Code::InvalidRequest`

### Requirement: factory-routing-dlopen (REQ-CLP-003)

`LLMProviderFactory::create()` MUST 改为薄路由:cloud provider 字符串委托到 `CloudPluginLoader`(内置 dlopen cache),不再直接 `new CloudLLMAdapter`。`PdkProviderConfig` 为纯 POD struct,跨 .so 边界安全传递配置信息。

#### Scenario: cloud 路由 dlopen

- **WHEN** 调用 `LLMProviderFactory::create(config)` 且 `config.provider` ∈ {"openai", "anthropic", "deepseek", "qwen", "moonshot", "custom"}
- **THEN** MUST 调用 `CloudPluginLoader::instance().load_provider(config)`
- **AND** `CloudPluginLoader` 首次调用时 MUST dlopen `libhydraforge_pdk_cloud.so`(路径:`./plugins/pdk_cloud/` 或 `$HYDRAFORGE_PLUGIN_PATH`)
- **AND** `dlsym(handle, "pdk_create_llm_provider")` 查找符号
- **AND** 调用符号返回 `shared_ptr<ILLMProvider>`(由 plugin 内部 new `CloudLLMAdapter`)

#### Scenario: 缓存 dlopen handle

- **WHEN** 第二次调用 `LLMProviderFactory::create()` 同一 cloud provider
- **THEN** MUST 复用第一次 dlopen 的 handle,不重新 dlopen
- **AND** 调用 `pdk_create_llm_provider` 每次返回新 `shared_ptr<ILLMProvider>` 实例(每个 config 独立实例)

#### Scenario: dlopen 失败兜底

- **WHEN** `dlopen("libhydraforge_pdk_cloud.so")` 失败(.so 找不到或 ABI 不匹配)
- **THEN** MUST 记录 ERROR 日志(`hydraforge_pdk_cloud.so load failed: <reason>`)
- **AND** MUST 返回 `MockLLMProvider` 实例(兜底,永不返回 nullptr)
- **AND** MUST NOT throw 异常(保证 caller 永不崩溃)
- **AND** 首次 dlopen 失败后,MUST 缓存失败状态,后续调用同一 provider MUST NOT 重试 dlopen(避免性能退化);用户修复 .so 后需重启进程 或调用 `CloudPluginLoader::reset()`

### Requirement: cloud-plugin-lifecycle (REQ-CLP-004)

Cloud plugin MUST 实现 `pdk_plugin_init()` / `pdk_plugin_fini()` lifecycle hooks,per ADR-0041 §1。

#### Scenario: pdk_plugin_init

- **WHEN** PluginLoader 加载 cloud plugin 后调用 `pdk_plugin_init()`
- **THEN** MUST 返回 `true`(成功)
- **AND** MUST 初始化 httplib::Client 连接池(`std::vector<httplib::Client>`)
- **AND** MUST 预解析所有已知 API key(env > file > direct)
- **AND** 任何初始化失败 MUST 返回 `false`,PluginLoader MUST 拒绝加载

#### Scenario: pdk_plugin_fini

- **WHEN** PluginLoader 卸载 cloud plugin 前调用 `pdk_plugin_fini()`
- **THEN** MUST 清空 httplib 连接池
- **AND** MUST 清空 API key 缓存
- **AND** MUST NOT 释放仍持有的 `shared_ptr<ILLMProvider>` 实例(由 caller RAII 管理)

#### Scenario: lifecycle 调用顺序

- **WHEN** PluginLoader::load_so("libhydraforge_pdk_cloud.so") 加载
- **THEN** 调用顺序 MUST 为:
  1. `dlopen` 加载 .so
  2. `dlsym("pdk_plugin_info")` + ABI 检查
  3. `dlsym("pdk_plugin_init")` + 调用
  4. `dlsym("pdk_register_tools")` + 调用
  5. plugin 加入 `loaded_` 列表
- **AND** `pdk_create_llm_provider` 仅在 caller 需要 provider 时调用(非 init 时)

### Requirement: factory-local-remap (REQ-CLP-005)

`LLMProviderFactory::create()` MUST 将 `"local"` / `"llama"` provider 字符串 remap 到 C14 推理 Plugin(`pdk/llama_engine/`,已 ship),per ADR-0042 §2 修订。

#### Scenario: local remap 到推理 Plugin

- **WHEN** 调用 `LLMProviderFactory::create(config)` 且 `config.provider ∈ {"local", "llama"}`
- **THEN** MUST 调用 `LlamaEnginePluginLoader::instance().load_provider(config)`
- **AND** `LlamaEnginePluginLoader` MUST dlopen `libhydraforge_pdk_llama_engine.so`
- **AND** 调用 `pdk_create_llm_provider` 返回推理 Plugin ILLMProvider 实例
- **AND** 用户配置 `provider: "local"` 无需改动即可从 LlamaAdapterProvider 迁移到推理 Plugin

#### Scenario: legacy alias 临时回退

- **WHEN** 配置 `provider: "local_legacy"` (隐藏 alias, Phase 3 release 前提供)
- **THEN** MUST 路由到原 `LlamaAdapterProvider`(`src/common/llm/llama_adapter_provider.cpp`)
- **AND** MUST emit WARNING 日志(`local_legacy is deprecated, use local instead`)
- **AND** Phase 3 release 后 MUST NOT 提供此 alias(完全删除 `LlamaAdapterProvider`)

### Requirement: available-models-override (REQ-CLP-006)

Cloud plugin 的 `CloudLLMAdapter::available_models()` MUST 显式 override,返回当前 config 注册的模型列表(per REQ-ICC-004 pure virtual)。

#### Scenario: 单模型返回

- **GIVEN** `config.provider = "openai"`, `config.model = "gpt-4o"`
- **WHEN** 调用 `cloud_provider->available_models()`
- **THEN** MUST 返回 1 个 `ModelInfo`:
  - `name = "gpt-4o"`
  - `capabilities = {Chat, ToolUse}`(OpenAI 支持 tool call)
  - `context_window = 128000`(gpt-4o 默认)
  - `provider = "openai"`

#### Scenario: anthropic 模型

- **GIVEN** `config.provider = "anthropic"`, `config.model = "claude-3-5-sonnet-20240620"`
- **WHEN** 调用 `cloud_provider->available_models()`
- **THEN** MUST 返回 1 个 `ModelInfo`:
  - `name = "claude-3-5-sonnet-20240620"`
  - `capabilities = {Chat, ToolUse, Vision}`(Anthropic 支持 vision)
  - `context_window = 200000`
  - `provider = "anthropic"`

#### Scenario: 编译时强制 override

- **WHEN** 第三方 cloud ILLMProvider 实现未 override `available_models()`
- **THEN** MUST 编译失败(纯虚方法未实现,per REQ-ICC-004)

---

## PdkProviderConfig 定义

> **设计约束**: `pdk_create_llm_provider()` 跨越 .so 边界,参数必须为纯 POD struct,避免 `std::string`/`std::optional` 等非平凡类型导致的 ABI 问题。

```cpp
// include/agenticdsl/plugin/pdk_provider_config.h
struct LLMConfig;  // 前向声明(在 src/common/llm/llm_types.h)

// 纯 POD,跨 .so ABI 安全
struct PdkProviderConfig {
  const char* provider;         // "openai" / "anthropic" / "deepseek" / "qwen" / "moonshot" / "custom"
  const char* model;            // 模型名 (如 "gpt-4o")
  const char* api_key;          // API key (可为 nullptr,plugin 自行解析 env)
  const char* base_url;         // 自定义 endpoint (可为 nullptr,用默认值)
  const char* api_version;      // API 版本 (可为 nullptr)
  const char* organization;     // org id (可为 nullptr)
  uint32_t max_retries;         // 最大重试次数 (默认 3)
  uint32_t timeout_ms;          // 请求超时(毫秒,默认 30000)
  float temperature;            // 采样温度 (默认 0.7, -1.0 表示用 provider 默认值)
  float top_p;                  // nucleus sampling (默认 1.0)
};

// plugin 侧将 PdkProviderConfig 转换为内部 LLMConfig
// LLMConfig from_pdk_config(const PdkProviderConfig* cfg);
```

**生命周期职责**:
- `pdk_create_llm_provider()` 接收 `const PdkProviderConfig*`，内部深拷贝为 `LLMConfig`
- caller 负责保证 `PdkProviderConfig` 在调用期间有效
- plugin 内部 `cloud_adapter.cpp` 通过 `from_pdk_config()` 转换后,原 `PdkProviderConfig` 可安全释放