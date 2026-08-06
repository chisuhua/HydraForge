## 1. LLMProviderFactory 动态注册与并发状态

- [ ] 1.1 扩展 `src/common/llm/llm_provider_factory.h`，定义动态 factory callback、provider catalog 条目、默认 provider 查询与 `register_dynamic()` API。
- [ ] 1.2 在 `llm_provider_factory.cpp` 增加 `std::shared_mutex` 保护动态注册表、默认 provider 和目录元数据，读取使用 shared lock，写入使用 unique lock。
- [ ] 1.3 保持 `llm_config.json` 构造时路由优先级、内置 mock/cloud/llama 路径和未知 provider mock 兜底行为不变。
- [ ] 1.4 实现动态 provider 名称校验、空 callback 拒绝、重复注册错误和已注册 provider 查询，禁止锁内执行 provider 构造。
- [ ] 1.5 实现 `switch_default()` 的原子更新和 `current_default()` 查询，明确显式 `LLMConfig::provider` 与默认 provider 的优先级。
- [ ] 1.6 为已有 factory 测试补充构造时配置回归、动态注册后立即 create、未知 provider和重复注册断言。
- [ ] 1.7 提交：`git commit -m "feat(provider): add thread-safe dynamic provider registration"`

## 2. provider/refresh 工具与模型目录提交

- [ ] 2.1 扩展 `pdk/provider_agent/include/provider_agent.h` 的目录状态接口，增加 refresh 请求结果、last refresh 状态和下线模型标记所需的只读字段。
- [ ] 2.2 在 `pdk/provider_agent/src/provider_resolve.cpp` 或独立 refresh 实现中加入上游模型目录请求，复用现有 API URL、endpoint、API key 延迟解析和 JSON 依赖。
- [ ] 2.3 实现两阶段刷新：锁外请求与 schema 校验，成功后一次性替换目录；失败、空目录或非法响应保留旧 catalog 并返回 warning。
- [ ] 2.4 注册 `provider/refresh` 工具，使用 ADR-0004 V2 `ToolMetadata` 标注 Workflow layer、approval policy、domain 和 allowed layers。
- [ ] 2.5 确认响应不回显 API key，返回 provider、added、removed、model_count、last_refresh 和 warning/error_code 等稳定字段。
- [ ] 2.6 添加可注入 HTTP/mock transport，覆盖可达上游、新模型、下线模型、超时、非法 JSON 和旧目录保留。
- [ ] 2.7 提交：`git commit -m "feat(provider-agent): add refresh tool with stale-catalog protection"`

## 3. provider/register_dynamic 工具

- [ ] 3.1 定义工具输入 schema，至少校验 provider name、backend kind、API URL、模型定义和可选 credential environment name。
- [ ] 3.2 将合法 JSON 定义转换为 `LLMConfig` 模板和 factory callback，callback 捕获值或安全共享所有权，不捕获裸指针或敏感日志文本。
- [ ] 3.3 注册 `provider/register_dynamic` 到 `IToolRegistry`，重复 provider、非法 backend、缺少 endpoint 或空模型目录返回结构化错误。
- [ ] 3.4 验证工具调用后新 provider 无需重启即可被 `LLMProviderFactory::create()` 解析，并让 `provider/list` 立即可见。
- [ ] 3.5 增加 ToolCoordinator layer、approval、审计和错误传播测试，确认 Cognitive/Thinking 非法调用不会改变 catalog。
- [ ] 3.6 提交：`git commit -m "feat(provider-agent): register providers at runtime"`

## 4. provider/switch 与 provider/list 状态查询

- [ ] 4.1 实现 `provider/switch` 工具，校验目标 provider 已注册后原子更新 factory 默认选择，失败不改变当前默认。
- [ ] 4.2 为 `provider/switch` 配置 ADR-0004 V2 ToolMetadata，并强制经过 `IToolRegistry` 和 `ToolCoordinator`，禁止 chat 层直调绕过治理。
- [ ] 4.3 检查现有 `provider/list` 工具：若已存在则扩展当前默认、模型目录版本和 stale warning 字段，否则按现有 provider agent 注册模式补齐。
- [ ] 4.4 添加切换成功、未知 provider、并发 switch、switch 与 refresh 交错以及后续 create 路由验证。
- [ ] 4.5 提交：`git commit -m "feat(provider-agent): add governed provider switching and listing"`

## 5. 并发 fixture、集成验证与文档契约

- [ ] 5.1 新增 `tests/test_provider_factory_concurrent.cpp`，使用多个线程并发 register、create、list、refresh 和 switch，验证无数据竞争、崩溃或半成品状态。
- [ ] 5.2 增加 `provider/register_dynamic`、`provider/refresh`、`provider/switch` 的工具调用测试，覆盖 Workflow 准入和 Cognitive/Thinking 拒绝。
- [ ] 5.3 增加动态 callback 生命周期测试，确认 callback 不依赖临时栈对象，失败路径不泄漏 provider 或覆盖旧配置。
- [ ] 5.4 执行 `cmake --build build --target test_provider_factory test_provider_agent` 或项目对应测试目标，并记录缺失目标时的实际 target 名称。
- [ ] 5.5 执行 `ctest --output-on-failure`，确认既有 `llm_config.json` 路径零回归；若存在 pre-existing failure，按项目约定记录而不删除测试。
- [ ] 5.6 执行 `openspec validate provider-dynamic-discovery --json`，确认 passed=true 且 failed=0。
- [ ] 5.7 提交：`git commit -m "test(provider): cover dynamic discovery concurrency and governance"`
