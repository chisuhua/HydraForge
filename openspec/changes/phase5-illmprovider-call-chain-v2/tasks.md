## 1. Decorator 抽象 + CostTracking 装饰器(P0 budget hole 修复)

- [ ] 1.1 创建 `include/agenticdsl/contract/i_llm_provider_decorator.h` 声明 `ILLMProviderDecorator` 抽象接口(继承 `ILLMProvider`,持有 `unique_ptr<ILLMProvider> inner_`,3 个 `decorate_*` 钩子方法)
- [ ] 1.2 实现 `src/common/llm/illmprovider_decorator.cpp` 基类 generate/generate_stream 转发逻辑
- [ ] 1.3 创建 `src/common/llm/cost_tracking_decorator.{h,cpp}` 实现 `CostTrackingDecorator`(注入 `shared_ptr<IBudgetController>`,generate 成功时 record_llm_call)
- [ ] 1.4 CostTrackingDecorator 处理流式:包装 `IGenerationStream` 在 next() 返回 nullopt 时统计累积 token,累积 token 数 MUST 精确匹配 `GenerationResult.prompt_tokens + completion_tokens`(零误差,非估算)
- [ ] 1.4.1 新建 `tests/test_decorator_chain_depth.cpp` 含 1 TEST_CASE 验证深度限制(REQ-IPD-001 Scenario 2):构造 4 层嵌套装饰器(CostTracking → Compliance → RateLimit → inner_) + 调用 `generate()` MUST throw `std::runtime_error("decorator chain too deep")` + 验证 max_depth=3 边界(3 层 OK,4 层 throw)
- [ ] 1.4.2 新建 `tests/test_cost_tracking_streaming_precision.cpp` 含 1 TEST_CASE 验证流式累积精度:mock provider 发送 N 个 chunk(每个 chunk 含已知 token 数)+ CostTracking 包装 + 流结束 + 断言 `budget_->get_total_cost_usd() > 0`(使用现有 IBudgetController API,无需扩展接口)且累积 token 数 MUST 严格 == sum(chunk tokens)(无 ±1 误差)
- [ ] 1.5 CostTrackingDecorator 处理错误:Result::failure 不计费(避免失败请求被计费)
- [ ] 1.6 修改 `src/core/engine.cpp` 构造器,在 `provider_factory_->create()` 后包装 CostTrackingDecorator
- [ ] 1.7 验证 NodeExecutor::execute_generate_subgraph / execute_yield / SimpleCognitiveOrchestrator 全部经过装饰器链(grep 验证所有 `llm_provider_->generate()` 调用点)
- [ ] 1.8 新建 `tests/test_cost_tracking_decorator.cpp` 含 4 个 TEST_CASE(同步计费 / 流式计费 / 错误不计费 / 集成 budget hole)
- [ ] 1.9 跑现有 61 个 ctest,验证零回归
- [ ] 1.10 lsp_diagnostics 验证新文件无 warning/error

## 2. Compliance + RateLimit 装饰器(可选,P0 集成)

- [ ] 2.1 创建 `src/common/llm/compliance_decorator.{h,cpp}` 实现 `ComplianceDecorator`(MVP 仅日志,emit `compliance.log` event 含 prompt/completion hash,不存原始文本)
- [ ] 2.2 创建 `src/common/llm/rate_limit_decorator.{h,cpp}` 实现 `RateLimitDecorator`(token-bucket 配额,默认未启用,opt-in)
- [ ] 2.3 DSLEngine 构造器添加 opt-in flag(`enable_compliance` / `enable_rate_limit`)
- [ ] 2.4 新建 `tests/test_compliance_decorator.cpp` 含 3 个 TEST_CASE(hash 一致 / 不存原文 / 默认禁用)
- [ ] 2.5 新建 `tests/test_rate_limit_decorator.cpp` 含 3 个 TEST_CASE(配额充足 / 配额不足 / opt-in 启用)

## 3. `available_models()` 改 pure virtual(REQ-ICC-004)

> **估时修正**:原"5 个实现 override <1 天"低估,实际工作量 3.5-4 天(`llm_types.h` 接口修改 + 4 处现有实现 override + 新推理 Plugin override + 编译失败兜底测试 + Router 静默失败防护测试 + 5 处 ctest 验证零回归)。本组任务按 3.5-4 天排期。

- [ ] 3.1 修改 `src/common/llm/llm_types.h:157` 将 `available_models()` 从 `virtual { return {}; }` 改为 `= 0` pure virtual
- [ ] 3.2 修改 `src/common/llm/mock_provider.{h,cpp}` 显式 override 返回 `mock-llm-v1`(`capabilities={Chat, ToolUse}`, `context_window=4096`, `provider="mock"`)
- [ ] 3.3 修改 `src/common/llm/cloud_adapter.{h,cpp}` 显式 override 返回 `config.model` 对应 `ModelInfo`(per spec REQ-CLP-006)
- [ ] 3.4 修改 `src/common/llm/llama_adapter_provider.{h,cpp}` 显式 override 返回 `llama_config.model` 对应 `ModelInfo`
- [ ] 3.5 新建 `tests/test_available_models_pure_virtual.cpp` 含 4 个 TEST_CASE(5 个实现都 override / 未 override 编译失败断言 / Router 静默失败抛 NoViableModel / override 但返回空 vector 抛 NoViableModel)
- [ ] 3.6 跑 ctest 验证零回归

## 4. Dual Consumer Model + OrchestrationILLMProvider 直连(REQ-ICC-001/002/003/006)

- [ ] 4.1 修订 `docs/adr/adr-0045-orchestration-plugin-spec.md` §2: `OrchestrationILLMProvider::generate()` 内部从 `call_tool` 改为直连 `inference_provider_->generate()`
- [ ] 4.2 修订 ADR-0045 §6: 删除"双 IToolRegistry"架构(internal + external bypass),改为单一 external registry
- [ ] 4.3a 创建 `include/agenticdsl/pdk/agent_loops/orchestration_illm_provider.h`(类骨架,per ADR-0045 §2):声明 `class OrchestrationILLMProvider : public ILLMProvider`,持有 `std::shared_ptr<ILLMProvider> inference_provider_`、`std::shared_ptr<IModelRouter> router_`、`std::shared_ptr<IInteractionBus> bus_` 三个成员
- [ ] 4.3b 创建 `src/common/llm/orchestration_illm_provider.cpp` 实现 generate/generate_stream/available_models 三个方法
- [ ] 4.3c 修改 `orchestration_illm_provider.h` 删除原 `IToolRegistry internal_registry_` 成员(若已存在),确认无 orphan 字段
- [ ] 4.4 修改 `src/common/llm/orchestration_illm_provider.cpp::generate()` 直连调用 `inference_provider_->generate(tuned, token)`,并发安全由 `shared_ptr<ILLMProvider>` 引用计数保证(无显式 mutex,per ADR-0045 §决策)
- [ ] 4.5 修改 `OrchestrationILLMProvider::generate_stream()` 保留 `OrchestrationStream` 流式聚合(细粒度/聚合/粗粒度 per ADR-0045 §2.3)
- [ ] 4.6 验证 Agent 循环路径:`PlanExecuteLoop::plan_phase/verify_phase` 调 `engine_->get_llm_provider()->generate()` 返回 raw ILLMProvider*(**非** OrchestrationILLMProvider 包装),断言:Agent 循环路径 MUST NOT 经过 OrchestrationILLMProvider::generate()(grep 验证代码路径)
- [ ] 4.7 验证 `SimpleCognitiveOrchestrator::react_once()` 调 `llm_->generate()` 直连推理
- [ ] 4.8 新建 `tests/test_orchestration_dual_consumer.cpp` 含 5 个 TEST_CASE(直连 generate / 流式聚合保留 / Agent 循环 raw / stop_token 传播 / 单一 registry)
- [ ] 4.9 跑 ctest 验证零回归(61+ 测试)
- [ ] 4.10 新建 `tests/test_plan_execute_restart.cpp` 含 3 个 TEST_CASE 覆盖 verify_phase 重启行为:Scenario A `verify_phase 返回 success → PlanExecuteLoop MUST 终止循环并 return success` / Scenario B `verify_phase 返回 retryable failure → PlanExecuteLoop MUST 重启主循环从 plan_phase()`(MUST NOT throw exception)/ Scenario C `verify_phase 返回 non-retryable failure → PlanExecuteLoop MUST 终止循环并 return failure`

## 5. Cloud Plugin 化(REQ-CLP-001 ~ 006)

- [ ] 5.1 创建 `pdk/cloud/` 目录结构(参照 `pdk/model_router/`):CMakeLists.txt + include/ + src/
- [ ] 5.2 创建 `pdk/cloud/CMakeLists.txt` 定义 INTERFACE 库 `hydraforge_pdk_cloud`(参照 `pdk/CMakeLists.txt` 模式)
- [ ] 5.3 修改根 `CMakeLists.txt` 添加 `add_subdirectory(pdk/cloud)`
- [ ] 5.4 移动 `src/common/llm/cloud_adapter.{h,cpp}` 至 `pdk/cloud/src/`
- [ ] 5.5 创建 `pdk/cloud/src/cloud_plugin.cpp` 导出 5 符号(`pdk_plugin_info` / `pdk_register_tools` / `pdk_create_llm_provider` / `pdk_plugin_init` / `pdk_plugin_fini`)
- [ ] 5.6 在 cloud plugin 内部根据 `config.provider` 字符串分发:`openai/deepseek/qwen/moonshot/custom` → OpenAI 兼容 / `anthropic` → Anthropic 协议
- [ ] 5.7 Anthropic 协议实现 `/v1/messages` endpoint + `x-api-key` header + SSE 解析(`event: content_block_delta` 格式)
- [ ] 5.8 实现 `pdk_plugin_init()`: 初始化 httplib 连接池 + API key 缓存
- [ ] 5.9 实现 `pdk_plugin_fini()`: 释放 httplib 资源 + 清 API key 缓存
- [ ] 5.10 修改 `src/common/llm/llm_provider_factory.cpp` cloud 路由(6 个 provider 字符串)改为 `CloudPluginLoader::load_provider(config)`
- [ ] 5.11 实现 `src/common/llm/cloud_plugin_loader.{h,cpp}`: 首次调用 dlopen `libhydraforge_pdk_cloud.so`,缓存 handle,失败兜底到 MockLLMProvider
- [ ] 5.12a 创建 `src/common/llm/inference_plugin_loader.{h,cpp}` 实现 `InferencePluginLoader`(与 CloudPluginLoader 对称,首次调用 dlopen `libhydraforge_pdk_inference_engine.so`,缓存 handle,失败兜底到 MockLLMProvider)— **先创建 stub,Phase 2+ 实际接入推理 plugin**
- [ ] 5.12b 修改 `src/common/llm/llm_provider_factory.cpp` "local" / "llama" 路由 remap 到 `InferencePluginLoader::load_provider(config)`(per ADR-0042 §2 修订)
- [ ] 5.12c **Phase 2 实际接入点**:待推理 Plugin `pdk/inference_engine/` ship 后,`InferencePluginLoader` 自动 dlopen 新 .so 并切换路由(无需改 factory 路由表,设计目标)
- [ ] 5.13 新建 `tests/test_cloud_llm_plugin.cpp` 含 5 个 TEST_CASE(5 符号导出 / OpenAI compat 路由 / Anthropic 协议 / dlopen 兜底 / Lifecycle)
- [ ] 5.14 新建 `examples/cloud_llm_plugin_demo/main.cpp` 演示加载 cloud plugin + `--mock` 模式共存

## 6. PluginLoader 扩展(5 符号查找 + lifecycle + ABI v2)

> **依赖关系**: Task 6 全部子任务 MUST 在 Task 5(Cloud plugin 创建 + 5 符号导出)完成后执行。Task 6.2-6.4 是 PluginLoader 加载 Task 5.4-5.5 导出符号的"消费侧"。Task 7(ADR 文档)可与 Task 5/6 并行,不依赖实施。

- [ ] 6.1 修改 `include/agenticdsl/plugin/plugin_info.h` `PluginInfo` 加 `dependencies[256]` 字段,`CURRENT_ABI_VERSION = 2`
- [ ] 6.2 修改 `src/modules/plugin/plugin_loader.cpp` `load_so()` 添加 `pdk_create_llm_provider` / `pdk_plugin_init` / `pdk_plugin_fini` 三个符号查找
- [ ] 6.3 修改 `unload_plugin()` 按 lifecycle 顺序释放 shared_ptr → pdk_plugin_fini → dlclose
- [ ] 6.4 在 `include/agenticdsl/plugin/plugin_loader.h` 添加 `virtual std::shared_ptr<::agenticdsl::ILLMProvider> create_llm_provider(const std::string& plugin_name, const PdkProviderConfig& config) = 0;` 抽象方法,然后在 `src/modules/plugin/plugin_loader.cpp` 实现:从 `loaded_` 列表按 name 查找 → 若 plugin 已卸载 throw `std::runtime_error("plugin <name> unloaded")` → 调用 `pdk_create_llm_provider(&config)` 符号返回 `shared_ptr<ILLMProvider>` → 若 plugin 未实现该符号返回 `nullptr`(非 throw)
- [ ] 6.5 实现 plugin 依赖拓扑排序(读取 `dependencies[256]`,扫描 `loaded_` 列表,循环依赖检测)
- [ ] 6.6 新建 `tests/test_plugin_loader_v2.cpp` 含 5 个 TEST_CASE(5 符号查找 / lifecycle 顺序 / 拓扑排序 / 循环依赖检测 / abi_version=1 向后兼容)
- [ ] 6.7 跑 ctest 验证现有 plugin-loader 测试零回归

## 7. ADR 文档同步修订

- [ ] 7.1 修订 `docs/adr/adr-0001-illm-provider-streaming-interface.md` §状态行加:`available_models() pure virtual 决议 (2026-07-06)` + `is_available()/name() deferred indefinitely` + `Result<T,E> → std::expected 待 C++23 基线`
- [ ] 7.2 修订 `docs/adr/adr-0035-inference-engine-plugin-spec.md` §1.1 三层消费链图重画(Dual Consumer Model)
- [ ] 7.3 修订 ADR-0035 §2 工具命名表:删除 `llama_engine/` 残留引用,统一 `inference.*`
- [ ] 7.4 修订 ADR-0035 §4 加"SamplerStrategy 接口 deferred 到 Phase 6+"
- [ ] 7.5 修订 `docs/adr/adr-0038-dynamic-config-interface.md` §状态行加"BatchingQueue 接口 deferred 到第二个推理 backend"
- [ ] 7.6 修订 `docs/adr/adr-0042-illmprovider-evolution-path.md` §2: Phase 2 措施改"remap `local` 到 InferencePlugin"(非删除),Phase 3 trigger 改"2 release cycles",deprecate 范围加 `LlamaAdapter`
- [ ] 7.7 修订 ADR-0042 §4: 推翻"cloud 留核心"决议,改为 cloud plugin 化
- [ ] 7.8 修订 `docs/adr/adr-0045-orchestration-plugin-spec.md` §2 + §6(per task 4.1-4.2)
- [ ] 7.9 同步修订 `openspec/changes/phase5-llama-engine-plugin/proposal.md` 引用新 ADR-0035 §2 命名(`inference.*`)
- [ ] 7.10 修订 `docs/adr/adr-0005-llm-backend-config-factory.md` §3 加注释:cloud plugin 路径与 factory 路由共存

## 8. `[[deprecated]]` 标注 + 退役路径执行

- [ ] 8.1 修改 `src/common/llm/llama_adapter_provider.h` 添加 `[[deprecated("Use pdk/inference_engine/ plugin instead, see ADR-0042 §2")]]`
- [ ] 8.2 修改 `src/common/llm/llama_adapter.h` 添加 `[[deprecated("LlamaAdapter is deprecated; use pdk/inference_engine/ plugin, see ADR-0042 §2")]]`
- [ ] 8.3 验证 `LLMProviderFactory::create()` "local_legacy" alias 路由到 `LlamaAdapterProvider` + emit WARNING 日志
- [ ] 8.4 跑 ctest 验证 deprecation warning 不阻塞测试

## 9. 架构合规性 + 集成验证

- [ ] 9.1 跑 `cmake --preset tests` + `ctest --output-on-failure`,验证全部测试通过(61+ baseline + 5 new test_cost_tracking_decorator + 3 test_compliance_decorator + 3 test_rate_limit_decorator + 3 test_available_models_pure_virtual + 5 test_orchestration_dual_consumer + 5 test_cloud_llm_plugin + 5 test_plugin_loader_v2 = 90 测试)
- [ ] 9.2 跑 ASan preset,验证零内存泄漏 + 零 use-after-scope
- [ ] 9.3 跑 TSan preset,验证零 data race
- [ ] 9.4 lsp_diagnostics 验证所有修改文件零 warning/error
- [ ] 9.5 验证 3 处直调 LLM 路径全部经过装饰器链(budget hole 修复,IBudgetController::total_cost_usd > 0)
- [ ] 9.6 验证 `LLMProviderFactory::create(config.provider = "local")` 返回新推理 Plugin ILLMProvider(用户配置零改动)
- [ ] 9.7 验证 `LLMProviderFactory::create(config.provider = "anthropic")` 走 Anthropic 协议(不再路由到 OpenAI 兼容)
- [ ] 9.8 跑 `openspec validate phase5-illmprovider-call-chain-v2` exit 0
- [ ] 9.9 跑 `./scripts/sprint-closeout.sh` 7 步全绿
- [ ] 9.10 更新 `docs/active-status.md` Phase 5 B2 进度,ADR-0001/0035/0042/0045 状态从 🔍 Proposed → ✅ Approved

## 10. Ship Gate + Archive 准备

- [ ] 10.1 git diff --stat 概览,确认所有任务文件已修改
- [ ] 10.2 git add 全部修改文件(ADR + src + tests + examples)
- [ ] 10.3 git commit "feat(phase5-call-chain): ILLMProvider v2 — Dual Consumer + Decorator + Cloud Plugin"
- [ ] 10.4 git push origin phase5-inference-orchestration
- [ ] 10.5 验证 CI 6 jobs 全绿(Linux x 2 compilers + ASan + TSan + 2 docker)
- [ ] 10.6 准备 `openspec archive phase5-illmprovider-call-chain-v2` 后的 spec 上移至 `openspec/specs/`
- [ ] 10.7 写 `docs/handoff/2026-07-06-illmprovider-call-chain-v2-ship.md` 记录 ship 状态 + 下一会话入口(Phase 5 B2 Week 2 启动路径 D PoC)