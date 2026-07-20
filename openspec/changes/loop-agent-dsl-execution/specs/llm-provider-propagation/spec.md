## ADDED Requirements

### Requirement: DSLEngine::from_markdown MUST accept parent LLM provider

`DSLEngine` MUST 提供一个方法，使通过 `from_markdown()` 创建的子引擎 SHALL 使用父引擎的 LLM provider，而非默认的 MockLLMProvider。

#### Scenario: 显式 provider 重载的 from_markdown
- **WHEN** 调用 `DSLEngine::from_markdown(dsl_content, parent_provider_ref)` 
- **THEN** 返回的 DSLEngine 实例使用 `parent_provider_ref` 作为其 LLM provider（而非默认 Mock）
- **THEN** 现有单参数 `from_markdown(content)` 行为保持不变（仍使用默认 MockLLMProvider）

#### Scenario: Provider 生命周期语义
- **WHEN** 子引擎通过 provider 引用执行 LLM 调用
- **THEN** provider 引用在子引擎执行期间有效（父引擎拥有 provider 所有权）
- **THEN** 子引擎不拥有 provider 所有权（不销毁、不转移）

#### Scenario: 向后兼容
- **WHEN** 现有代码调用 `DSLEngine::from_markdown(content)`（单参数）
- **THEN** 行为无变化——子引擎仍然使用默认 MockLLMProvider
- **THEN** 无需修改现有调用点

### Requirement: Decorator chain MUST be preserved (no re-wrap)

子引擎继承的 MUST 是父引擎 `decorate_provider()` 处理后的已包裹 provider（CostTracking → Compliance → RateLimit → inner），而非未装饰的 raw provider。子引擎 SHALL NOT 重新装饰。

#### Scenario: 继承已装饰 provider
- **WHEN** 父引擎已通过 `set_llm_provider(unique_ptr)` 注册了经过 CostTrackingDecorator 包裹的 provider
- **AND** 子引擎通过 `from_markdown(content, *parent.get_llm_provider())` 创建
- **THEN** 子引擎的 LLM 调用经过父引擎 CostTrackingDecorator，cost 计入 parent budget
- **AND** 子引擎内部没有再次调用 `decorate_provider()`（无二次包裹）

#### Scenario: 禁止 set_llm_provider 在新 overload 内部调用
- **WHEN** 实现 `from_markdown(content, ILLMProvider&)` 新重载
- **THEN** 不得在实现体内调用 `set_llm_provider(unique_ptr)` 方法
- **THEN** 必须直接使用 `borrowed_provider_` 字段存储引用
- **THEN** 必须释放 `owned_provider_` 中的默认 MockLLMProvider

#### Scenario: 双字段存储不变式
- **WHEN** 子引擎通过新 overload 创建后
- **THEN** `owned_provider_` 为 nullptr（Mock 已释放）
- **AND** `borrowed_provider_` 指向父引擎 provider
- **AND** `get_llm_provider()` 返回 `borrowed_provider_` 指向的值

### Requirement: Provider pointer SHALL be thread-local

`loop/set_parent_provider` 工具存储的 provider 指针 MUST 是线程级别隔离的（SHALL use `thread_local`），不同线程的 DSLEngine 实例互不干扰。

#### Scenario: thread_local 隔离
- **WHEN** 线程 A 调用 `loop/set_parent_provider` 设置 provider_A
- **AND** 线程 B 调用 `loop/set_parent_provider` 设置 provider_B
- **THEN** 线程 A 的 `loop/run` 使用 provider_A
- **THEN** 线程 B 的 `loop/run` 使用 provider_B
- **THEN** 两线程互不干扰

#### Scenario: 同一线程覆盖行为
- **WHEN** 同一线程先后两次调用 `loop/set_parent_provider` 设置不同的 provider
- **THEN** 第二次调用覆盖第一次（last-write-wins）
- **THEN** 覆盖发生时输出 warning 日志
- **THEN** 不抛出异常

### Requirement: Test isolation MUST reset thread_local

Catch2 同一线程上先后运行的 TEST_CASE SHALL NOT 因 `thread_local` 变量残留而互相影响。每个 TEST_CASE MUST 在 Setup 阶段显式 reset provider 指针。

#### Scenario: 跨测试重置
- **WHEN** TEST_CASE 使用了 loop_agent 的 `loop/set_parent_provider` 设置 provider
- **AND** 下一 TEST_CASE 在相同线程运行
- **THEN** 每个 TEST_CASE 在 Setup 阶段显式重置 `tls_parent_provider` 为 nullptr
- **THEN** 不依赖上一个 TEST_CASE 设置的值