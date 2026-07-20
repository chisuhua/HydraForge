## Why

pdk_chat_demo 的 Loop Agent (`pdk/loop_agent/`) 当前返回硬编码的 mock 响应，无法真实执行 `lib/loop/react.agent.md` DSL。根因是 `DSLEngine::from_markdown()` 创建的子引擎是一个独立实例，父引擎的 LLM provider（包括 ILLMProvider 实现、模型配置、API key 等）无法被子引擎继承，导致子引擎在 DSL 执行时无法进行 LLM 推理。

该限制被编码在 `pdk/loop_agent/src/pdk_entry.cpp:118`：
> "mock response ... architectural limitation that DSLEngine::from_markdown creates an isolated sub-engine whose LLM provider cannot inherit configuration from the parent engine"

解决此问题的价值：pdk_chat_demo 将端到端验证完整的 Agent-as-Plugin 管线——主引擎配好 LLM → Plugin 加载 → Plugin 内部调 `from_markdown` 执行 DSL → DSL 中的 LLM 调用使用父引擎的 LLM provider。这是 ADR-0060 (Agent Composition) 中 `call` 协作模式的关键基础设施。

> **2026-07-19 更新**: 经 Metis + Oracle 联合审查，设计已修复 3 个阻断问题：
> 1. `llm_provider_` (unique_ptr) ↔ ILLMProvider& 存储矛盾 → 双字段方案
> 2. 进程级 static 全局 → thread_local 隔离 + overwrite warning
> 3. 仅 React 覆盖 → PlanExecute/ForkJoin 全覆盖
> 详见 [design.md](./design.md) 和 [tasks.md](./tasks.md)

## What Changes

- **BREAKING**: `DSLEngine` 内部新增 `borrowed_provider_` 字段 + `set_borrowed_provider()` setter，支持非拥有引用语义
- **新能力**: `DSLEngine::from_markdown(content, ILLMProvider&)` 新重载 — 子引擎借用父引擎 LLM provider（已装饰链），不使用 MockLLMProvider
- **BREAKING**: `get_llm_provider()` 改为双字段路由（`borrowed_provider_` > `owned_provider_`）
- **不变式**: `owned_provider_` 和 `borrowed_provider_` 互斥，至多一个非 null
- `pdk/loop_agent/src/pdk_entry.cpp` 中 `loop/run` 工具的 lambda 实现：从 mock 响应改为真实调用 `DSLEngine::from_markdown()` + DSL 执行
- `pdk/loop_agent/src/pdk_entry.cpp` 新增 `loop/set_parent_provider` 工具（`thread_local` 级别存储，`category=SystemConfig`, `force_approval_always`）
- `pdk/loop_agent/src/pdk_entry.cpp` 增加 `loop_type` 合法性校验（仅 `react`/`plan_execute`/`fork_join`）
- 测试：新增 `test_llm_provider_propagation.cpp` + 扩充 `test_loop_agent_plugin`，覆盖装饰器链继承、双字段不变式、thread_local 隔离、cost 单次计费、3 种 loop_type

## Capabilities

### New Capabilities
- `llm-provider-propagation`: DSLEngine 子引擎（from_markdown 创建的引擎）能够从父引擎继承 LLM provider 配置（ILLMProvider、model config、API key 等），使得 DSL 子图中的 LLM 调用使用父引擎的 provider
- `loop-agent-real-execution`: Loop Agent Plugin (`chat.loop`) 的 `loop/run` 工具不再返回 mock 响应，而是真实加载 `lib/loop/*.agent.md` 并通过 DSLEngine 执行，支持 react/plan_execute/fork_join 三种循环模式

### Modified Capabilities
- 无（这是新引入的能力，不改变现有 spec 级行为）

## Impact

### 代码影响
- `src/core/engine.h` / `engine.cpp`：`from_markdown` 方法签名或内部实现修改
- `src/core/engine-factory.h`：可能影响 factory 创建子引擎的方式
- `pdk/loop_agent/src/pdk_entry.cpp`：`loop/run` 实现从 mock 改为真实执行
- `pdk/loop_agent/CMakeLists.txt`：可能需要链接 `agenticdsl_core` 以使用 DSLEngine
- `tests/test_loop_agent_dsl_execution.cpp`：新增测试文件

### 依赖
- 无外部依赖变更
- 需要理解 `DSLEngine` 内部 `llm_provider_` 成员的所有权和生命周期管理

### 不变量（Non-goals）
- 不修改 `ILLMProvider` 接口本身（ADR-0001 保持不变）
- 不修改 PluginLoader 的加载机制
- 不解决 Session Agent / Budget Agent 的跨引擎传播（范围外）
- 不涉及 SKILL.md 隔离执行（那是 skill-interpreter-real-loading 的范围）