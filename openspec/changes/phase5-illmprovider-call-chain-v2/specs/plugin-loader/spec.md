# Spec: plugin-loader (Phase 5 增量 — ILLMProvider Plugin 化扩展)

## ADDED Requirements

### Requirement: pdk-create-llm-provider-symbol (REQ-PL-IPD-001)

`PluginLoader::load_so()` MUST 查找并调用 `pdk_create_llm_provider` 符号,从 plugin 返回 `shared_ptr<ILLMProvider>` 实例。

#### Scenario: load_so 查找 pdk_create_llm_provider

- **WHEN** 调用 `loader.load_so(path, registry, strict_version=true)`
- **THEN** MUST 在 `dlopen` 后顺序查找以下 5 个符号:
  - `pdk_plugin_info` (必选)
  - `pdk_register_tools` (必选)
  - `pdk_create_llm_provider` (可选)
  - `pdk_plugin_init` (可选)
  - `pdk_plugin_fini` (可选)
- **AND** 若 `pdk_create_llm_provider` 存在,MUST 缓存到 `loaded_` 列表的 `llm_provider_factory_` 字段
- **AND** 若 `pdk_create_llm_provider` 不存在,MUST NOT 报错(plugin 可仅提供工具,不提供 LLM)

#### Scenario: pdk_create_llm_provider 调用语义

- **WHEN** 调用 `loader.create_llm_provider(plugin_name, config)`
- **THEN** MUST 在 `loaded_` 列表按 name 查找
- **AND** 若找到,MUST 调用 `pdk_create_llm_provider(&config)` 返回 `shared_ptr<ILLMProvider>`
- **AND** 若 plugin 未实现 `pdk_create_llm_provider`,MUST 返回 `nullptr`
- **AND** 若 plugin 已卸载(handle 已 dlclose),MUST throw `std::runtime_error("plugin <name> unloaded")`

#### Scenario: plugin 卸载时释放 shared_ptr

- **WHEN** `loader.unload_plugin(name)` 被调用
- **THEN** MUST 释放该 plugin 的所有 `shared_ptr<ILLMProvider>` 实例引用(若仍持有)
- **AND** MUST `dlclose(handle)`(在所有 shared_ptr 释放后,per Sprint 17 C7 destruction order bug 修复模式)
- **AND** MUST NOT 释放由 caller 持有的 `shared_ptr`(caller 自行 RAII 管理)

### Requirement: pdk-plugin-init-fini-hooks (REQ-PL-IPD-002)

`PluginLoader` MUST 在 `load_so` 后调用 `pdk_plugin_init()`,在 `unload_plugin` 前调用 `pdk_plugin_fini()`,per ADR-0041 §1。

#### Scenario: load_so 调用 pdk_plugin_init

- **WHEN** 调用 `loader.load_so(path, registry)`
- **THEN** MUST 在 `dlsym("pdk_plugin_info")` + ABI 检查 + `dlsym("pdk_register_tools")` + `register_tools(reg)` 之后
- **AND** 若 `pdk_plugin_init` 存在,MUST 调用并检查返回值
- **AND** 若 `pdk_plugin_init` 返回 `false`,MUST 拒绝加载(回滚 `register_tools` 注册的工具 + dlclose)
- **AND** 若 `pdk_plugin_init` 不存在,MUST 跳过(向后兼容 abi_version=1 plugin)

#### Scenario: unload_plugin 调用 pdk_plugin_fini

- **WHEN** 调用 `loader.unload_plugin(name)`
- **THEN** MUST 在释放 `shared_ptr<ILLMProvider>` 之后、`dlclose(handle)` 之前
- **AND** MUST 调用 `pdk_plugin_fini()`(若存在)
- **AND** MUST NOT 抛出异常(plugin fini 失败仅记录 ERROR 日志)

#### Scenario: lifecycle 顺序保证

- **WHEN** PluginLoader 加载 plugin
- **THEN** 调用顺序 MUST 为:
  1. `dlopen(path)`
  2. `dlsym("pdk_plugin_info")` + ABI 检查
  3. `dlsym("pdk_register_tools")` + `register_tools(reg)`
  4. `dlsym("pdk_plugin_init")` + 调用(若存在)
  5. 加入 `loaded_` 列表
- **WHEN** PluginLoader 卸载 plugin
- **THEN** 调用顺序 MUST 为:
  1. 释放所有 plugin 持有的 `shared_ptr<ILLMProvider>`
  2. `dlsym("pdk_plugin_fini")` + 调用(若存在)
  3. 从 `loaded_` 列表移除
  4. `dlclose(handle)`

### Requirement: plugin-info-v2-abi-version (REQ-PL-IPD-003)

`PluginInfo` MUST 升级到 v2 ABI,新增 `dependencies[256]` 字段,per ADR-0041 §1.5。

#### Scenario: PluginInfo v2 字段

- **WHEN** 检查 `include/agenticdsl/plugin/plugin_info.h` 中 `PluginInfo` struct
- **THEN** MUST 含 v2 新增字段:
  - `dependencies[256]` — 逗号分隔的依赖 plugin name 列表(如 `"model_router,cost_tracker"`)
- **AND** MUST 提供 `inline constexpr uint32_t CURRENT_ABI_VERSION = 2`
- **AND** MUST 保持 POD 类型(无构造/析构/虚函数)
- **AND** MUST 保持 Linux x86_64 ABI 兼容的内存布局

#### Scenario: abi_version=1 向后兼容

- **WHEN** PluginLoader 加载 abi_version=1 plugin(Sprint 5 旧 plugin)
- **THEN** MUST 接受(abi_version=1 仍合法)
- **AND** MUST 视为空依赖(`dependencies` 字段不存在)
- **AND** MUST NOT 报错(strict_version=true 仅检查 abi_version,非字段差异)

#### Scenario: 拓扑加载依赖

- **WHEN** 调用 `loader.load_all(registry)` 且多个 plugin 互相依赖
- **THEN** MUST 按 `dependencies` 字段拓扑排序加载顺序
- **AND** 若 A plugin 依赖 B plugin 但 B 未加载,MUST 拒绝加载 A + 记录 ERROR
- **AND** 若存在循环依赖(A 依赖 B, B 依赖 A),MUST 检测并报错

### Requirement: tool-registry-injection (REQ-PL-IPD-004)

`PluginLoader::load_so()` MUST 接收 `IToolRegistry&` 参数并注入到 `pdk_register_tools`,确保 plugin 注册的工具立即可用于 DSL workflow。

#### Scenario: register_tools 参数签名

- **WHEN** PluginLoader 调用 `pdk_register_tools(reg)`
- **THEN** MUST 传入 `IToolRegistry&` 引用(**保持现有签名 `void (*)(IToolRegistry&)`,与 `pdk/model_router/` / `pdk/llama_engine/` 已有 plugin 一致**)
- **AND** plugin 注册的工具 MUST 立即可用于 DSL workflow(无需额外步骤)

#### Scenario: 多 plugin 工具命名冲突

- **WHEN** plugin A 和 plugin B 都注册同名工具
- **THEN** `IToolRegistry` MUST 按 ADR-0043 slash 命名规则验证(只允许 `inference/*` 前缀)
- **AND** 若冲突,MUST 拒绝第二个 plugin 加载 + 记录 ERROR
- **AND** Phase 5 推理工具(`inference/*`)+ 编排工具(`orchestration/*`)+ 第三方工具命名空间互不冲突

### Requirement: cloud-plugin-loader (REQ-PL-IPD-005)

Cloud plugin 作为 first-party plugin MUST 被 `PluginLoader` 通过 `load_all()` 或显式 `load_so()` 加载,符号约定与第三方 plugin 一致(per REQ-CLP-001)。

#### Scenario: cloud plugin 通过 load_all 自动加载

- **WHEN** 调用 `loader.load_all(registry)`
- **THEN** MUST 扫描 `./plugins/pdk_cloud/libhydraforge_pdk_cloud.so`
- **AND** MUST `dlopen` + 验证 `abi_version = 2`
- **AND** MUST 调用 `pdk_register_tools` (cloud plugin 实现为空)
- **AND** MUST 调用 `pdk_plugin_init`(初始化 httplib 连接池)
- **AND** 加入 `loaded_` 列表,`llm_provider_factory_` 字段缓存 `pdk_create_llm_provider` 符号

#### Scenario: cloud plugin 创建 ILLMProvider

- **WHEN** DSLEngine 构造时调 `CloudPluginLoader::load_provider(config)`
- **THEN** MUST 通过 `PluginLoader::create_llm_provider("pdk_cloud", config)` 获取 `shared_ptr<ILLMProvider>`
- **AND** MUST 在 LLMProviderFactory cloud 路由(6 个 provider 字符串)中复用同一 plugin 实例

#### Scenario: cloud plugin abi_version 不匹配

- **GIVEN** cloud plugin 编译时 `CURRENT_ABI_VERSION = 2`,Runtime 端 `CURRENT_ABI_VERSION = 3`
- **WHEN** PluginLoader 加载 cloud plugin
- **THEN** MUST 拒绝加载(strict_version=true)
- **AND** MUST 记录 ERROR:`abi version mismatch: plugin=2, runtime=3`
- **AND** MUST 降级到 MockLLMProvider(永不返回 nullptr)

---

### Requirement: plugin-loader-dedup (REQ-PL-IPD-006)

Cloud PluginLoader 与 DSLEngine::plugin_loader_ MUST 共享同一 dlopen handle,避免重复加载 .so 导致全局状态损坏。

#### Scenario: dlopen 句柄共享

- **WHEN** DSLEngine 构造时同时需要 cloud plugin (通过 CloudPluginLoader)和 llama_engine plugin (通过 LlamaEnginePluginLoader)
- **THEN** `PluginLoader::load_so()` MUST 检查 `loaded_` 列表,若 .so 已由 DSLEngine::plugin_loader_ 加载,MUST 复用句柄而非重新 dlopen
- **AND** `pdk_plugin_init()` MUST NOT 被重复调用(已调用则跳过)
- **AND** `pdk_create_llm_provider` 可通过任意 PluginLoader 引用调用

#### Scenario: 非 PluginLoader 的独立 factory loader

- **WHEN** `CloudPluginLoader::instance()` 是独立单例(非 PluginLoader 子类)
- **THEN** MUST 实现与 DSLEngine PluginLoader 兼容的句柄共享机制(通过全局 `loaded_handles_` map keyed by .so path)
- **AND** dlopen 后 MUST 缓存 handle 到全局 map,PluginLoader::load_so 先查全局 map 再 dlopen