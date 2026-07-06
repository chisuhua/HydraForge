# arch-refactor Specification

## Purpose
Sprint 17 全项目审计发现 4 项严重架构债 (`layered_context.h` 静态 `null_j` TSan 阳性、`execute_tool_call()` 138 行混合职责、`execute_single_branch()` 117 行自包含调度循环含 `HardEndException` 控制流、`topo_scheduler.h` 跨模块头文件传播);本 spec 锁定修复后的行为契约与并发测试覆盖率。
## Requirements
### Requirement: layered-context-thread-safety

`layered_context.h` 中的 `static nlohmann::json null_j` MUST 改为 `thread_local`，防止并发 `at()` 调用的 TSan 阳性。

#### Scenario: Concurrent reads are data-race-free

- **WHEN** 10 线程并发读取 `ctx.at("working.value")` × 100 次
- **THEN** 0 个 error，所有值正确 (42)
- **AND** TSan 验证 0 warnings

### Requirement: execute-tool-call-split

`NodeExecutor::execute_tool_call()` MUST split into 3 helper methods for readability.

#### Scenario: Tool call dispatch still works after refactor

- **WHEN** 运行现有 49 个 ctest
- **THEN** 100% PASS，行为完全保持
- **AND** `dispatch_to_tool()` + `handle_tool_errors()` + `process_output_keys()` 独立可测

### Requirement: execute-single-branch-split

`TopoScheduler::execute_single_branch()` MUST split into helper methods for readability.

#### Scenario: Branch simulation still works after refactor

- **WHEN** 运行 `test_execute_parallel` 和 `test_scheduler`
- **THEN** 所有分支模拟通过，HardEndException 行为保持

### Requirement: topo-scheduler-header-decouple

`topo_scheduler.h` MUST eliminate modules/ includes via PIMPL-lite化.

#### Scenario: ResourceManager PIMPL-lite eliminates include

- **WHEN** 检查 `topo_scheduler.h` includes
- **THEN** `modules/scheduler/resource_manager.h` 已移除 (仅在 .cpp 中 include)
- **AND** `ResourceManager` 通过 `unique_ptr<ResourceManager>` PIMPL-lite 化
- **AND** engine.h 仍保持 0 modules/include (ADR-0019 §1.4 compliant)

### Requirement: resource-node-no-circular-include

`src/core/types/resource.h` MUST NOT 直接或间接 `#include "node.h"`，打破 `node.h ↔ resource.h` 循环依赖。`resource.h` 应当仅 include `context.h` + 标准库头文件，前向声明或字符串别名解决其他依赖。

#### Scenario: 单向依赖链
- **WHEN** 编译 `src/core/types/resource.h`
- **THEN** 仅 include `context.h` 和标准库 (`<string>`, `<nlohmann/json.hpp>`)
- **AND** 不 include `node.h`
- **AND** `node.h` 仍可正常使用 `ResourceType` (因为 `node.h` 先 include `resource.h`)

#### Scenario: 头守卫仍生效
- **WHEN** 同一翻译单元同时 include `node.h` 和 `resource.h`
- **THEN** 头守卫防止无限递归
- **AND** 编译器不报 "main file cannot be included recursively" 错误

### Requirement: tool-result-error-enum-only

`ToolResult::error()` MUST 仅暴露接受 `ErrorCode` 枚举的重载。接受 `std::string` 错误码的 `@deprecated` 重载 MUST 被移除。

#### Scenario: 仅 ErrorCode 重载可调用
- **WHEN** 编译 `src/core/types/tool_result.h`
- **THEN** 公开 API 仅有 `static ToolResult error(ErrorCode code, std::string msg, ...)`
- **AND** `static ToolResult error(std::string code, std::string msg, ...)` 已移除
- **AND** 全代码库 (src/ + tests/ + include/) 调用点已迁移到 ErrorCode 重载

#### Scenario: 调用方迁移零残留
- **WHEN** 运行 `grep -rn 'ToolResult::error("' src/ tests/ include/`
- **THEN** 输出 0 行 (无字符串错误码调用残留)

### Requirement: library-loader-pimpl-lite

`src/modules/library/library_loader.h` MUST 使用 PIMPL-lite 模式解耦 `MarkdownParser` 直接依赖。前向声明 `MarkdownParser`，通过 `std::unique_ptr<MarkdownParser>` 间接持有。

#### Scenario: header 不再 include parser
- **WHEN** 编译 `src/modules/library/library_loader.h`
- **THEN** 不再 include `modules/parser/markdown_parser.h`
- **AND** `class MarkdownParser;` 前向声明存在
- **AND** 完整 include 移到 `src/modules/library/library_loader.cpp`

#### Scenario: 析构外置
- **WHEN** 持有 `unique_ptr<MarkdownParser>` 成员
- **THEN** 析构函数 `~LibraryLoader()` 必须 out-of-line 定义 (在 .cpp 中)
- **AND** 头文件中只有 `~LibraryLoader();` 声明

### Requirement: adr-0032-status-approved

`docs/archive/adr/adr-0032-cost-collector.md` 状态 MUST 从 `🟡 Partial` 同步到 `✅ Approved`，反映 `tests/test_cost_collector.cpp` (2026-06-14) 已 ship 的实际状态。

#### Scenario: ADR 状态字段正确
- **WHEN** 阅读 ADR-0032 头部元数据
- **THEN** 状态为 `✅ Approved`
- **AND** 包含 commit 引用: `tests/test_cost_collector.cpp`
- **AND** 包含 ship 日期: `2026-06-14`

#### Scenario: adr_lint 验证通过
- **WHEN** 运行 `python3 tools/adr_lint.py docs/archive/adr/adr-0032-cost-collector.md`
- **THEN** exit 0，无格式错误

### Requirement: agent-simple-compilable

`examples/agent_simple/simple.cpp` MUST 可编译并使用 MockLLMProvider 模式 (不依赖真实模型权重)。CMakeLists.txt MUST 存在并链接 `agenticdsl_core` + `agenticdsl_modules_cognitive`。

#### Scenario: 编译成功
- **WHEN** 运行 `cmake --preset debug -DAGENTICDSL_BUILD_EXAMPLES=ON && cmake --build build --target agent_simple`
- **THEN** 编译 exit 0
- **AND** `build/agent_simple` 二进制存在

#### Scenario: 启动运行
- **WHEN** 运行 `./build/agent_simple < examples/agent_simple/initial.md`
- **THEN** exit 0，输出包含 `[INFO] Engine ready` 或类似状态信息
- **AND** 零 LLM 真实调用 (MockLLMProvider 拦截)

#### Scenario: 无外部依赖
- **WHEN** 编译 `simple.cpp`
- **THEN** 不引用 `agenticdsl::LlamaAdapter` (已废弃)
- **AND** 不引用已删除的 `common/utils.h` (实际在 `common/utils/parser_utils.h`)

### Requirement: agent-loop-compilable

`examples/agent_loop/agent_loop.cpp` MUST 可编译并使用 MockLLMProvider 模式。CMakeLists.txt MUST 存在并链接必要模块。

#### Scenario: 编译成功
- **WHEN** 运行 `cmake --preset debug -DAGENTICDSL_BUILD_EXAMPLES=ON && cmake --build build --target agent_loop`
- **THEN** 编译 exit 0
- **AND** `build/agent_loop` 二进制存在

#### Scenario: 不引用已删除 API
- **WHEN** 编译 `agent_loop.cpp`
- **THEN** 不引用 `agenticdsl::PromptBuilder` (已删除)
- **AND** 不调用 `DSLEngine::get_llm_adapter()` (已删除，改 `get_llm_provider()`)

#### Scenario: 启动运行
- **WHEN** 运行 `./build/agent_loop`
- **THEN** exit 0
- **AND** 多轮循环演示可见 (info log 表明循环进度)

### Requirement: examples-build-target

根 `CMakeLists.txt` MUST 添加 `AGENTICDSL_BUILD_EXAMPLES` 选项 (默认 OFF)，启用时构建所有 examples/ 子目录。

#### Scenario: Examples 默认不构建
- **WHEN** 运行 `cmake --preset debug` (不带 `-DAGENTICDSL_BUILD_EXAMPLES=ON`)
- **THEN** 不构建 examples 二进制
- **AND** 不影响主 ctest 49/49 PASS

#### Scenario: Examples opt-in 构建
- **WHEN** 运行 `cmake --preset debug -DAGENTICDSL_BUILD_EXAMPLES=ON && cmake --build build`
- **THEN** 5+ 个 example 二进制构建 (agent_basic + agent_simple + agent_loop + slice_01_tool_call + phase1_*)
- **AND** 编译 exit 0

### Requirement: agents-md-debt-removed

`AGENTS.md` line 46 审计债注释 MUST 被移除 (since D-2 now resolved)。`examples/` 描述表 MUST 更新反映新启用的 2 个 examples。

#### Scenario: 审计债注释移除
- **WHEN** 阅读 `AGENTS.md` line 40-50
- **THEN** 不再包含 "examples/agent_simple/ 和 examples/agent_loop/ 的 DEPRECATED 注释基于错误删除假设撰写" 段落
- **AND** examples 表中 `agent_simple` 和 `agent_loop` 标记为 ✅ Active

#### Scenario: AGENTS.md adr_lint 通过
- **WHEN** 运行 `python3 tools/adr_lint.py`
- **THEN** exit 0 (如果存在该工具) 或手动验证 markdown 格式正确

### Requirement: context-layered-adapter

`include/agenticdsl/types/context_flatten.h` MUST 提供 `agenticdsl::to_context(const nlohmann::json&)` 和 `agenticdsl::from_context(const LayeredContext&)` 双向桥接函数。

#### Scenario: 适配函数存在
- **WHEN** 编译 `context_flatten.h`
- **THEN** `to_context(const nlohmann::json&)` 存在 (返回 LayeredContext)
- **AND** `from_context(const LayeredContext&)` 存在 (返回 nlohmann::json)
- **AND** 两者均 `inline` 0 开销

#### Scenario: 双向桥接无损
- **WHEN** 测试 `flatten(to_context(j)) == j` 对任意 j
- **THEN** 桥接无损 (round-trip 相等)
- **AND** `to_context(from_context(lc)) == lc` 对任意 lc (默认 L1-L2 空)

### Requirement: dslengine-run-layered-overload

`DSLEngine::run(const LayeredContext&)` MUST 存在作为推荐签名。`run(const Context&)` MUST 保留为 `[[deprecated]]` 桥接到新签名 (Sprint 20 期间)。

#### Scenario: 双 overload 存在
- **WHEN** 检查 `src/core/engine.h` 公开方法
- **THEN** `ExecutionResult run(const LayeredContext& ctx);` 存在
- **AND** `ExecutionResult run(const Context& context) [[deprecated]];` 存在

#### Scenario: 旧签名委托
- **WHEN** 调用 `engine.run(flat_context)` 旧路径
- **THEN** 自动 `to_context()` 转换
- **AND** 调用 `engine.run(layered_context)` 新路径
- **THEN** 行为完全等价
- **AND** 编译时 0 警告 (c++20 strict)

### Requirement: node-execute-layered-overload

`Node::execute()` 虚函数 MUST 支持 `LayeredContext&` 签名。旧 `Context&` 签名 MUST 保留为 `[[deprecated]]` (Sprint 20 期间)。

#### Scenario: 双签名存在
- **WHEN** 检查 `src/core/types/node.h` 虚函数
- **THEN** `virtual LayeredContext execute(LayeredContext& ctx) = 0;` 存在
- **AND** `virtual Context execute(Context& context) [[deprecated]];` 存在 (旧, 桥接到新)

#### Scenario: 6 个子类迁移
- **WHEN** 实施本 change
- **THEN** 6 个 Node 子类 (`StartNode` + `EndNode` + `LLMNode` + `ToolNode` + `ConditionalNode` + `ForkNode`) 实现新签名
- **AND** 旧签名通过 `flatten(execute(to_context(ctx)))` 桥接
- **AND** 行为完全保持 (22 测试 PASS)

### Requirement: test-fixtures-layered

22 个 Catch2 测试 fixture MUST 从 `Context = nlohmann::json` 构造改为 `LayeredContext` 构造 (Sprint 20 期间)。

#### Scenario: 测试 fixture 迁移
- **WHEN** 实施本 change
- **THEN** 22 个 `SECTION("...")` 中的 `Context` 字面量改为 `LayeredContext` 构造
- **AND** 通过 `to_context(...)` 或 `LayeredContext::from_json(...)` 工厂方法
- **AND** 22 个测试零行为变化 (assertion 全 PASS)

#### Scenario: 桥接测试覆盖
- **WHEN** 添加 5 个新测试到 `tests/test_context_adapter.cpp` (新文件)
- **THEN** `to_context` 5 个 case 覆盖: 空 / 嵌套 / 数组 / path 访问 / inja 模板
- **AND** 5 个测试全 PASS
- **AND** ctest 53/53 PASS (48 + 5 新测试)

### Requirement: plan-execute-loop-state-machine

`include/agenticdsl/pdk/agent_loops/plan_execute_loop.h` MUST 定义 `class PlanExecuteLoop` 含 3 阶段状态机: `Planning` → `Executing` → `Verifying` → `Done` / `Retry`。

#### Scenario: 状态机定义
- **WHEN** 编译 `plan_execute_loop.h`
- **THEN** `enum class State` 5 个值存在: `Planning` + `Executing` + `Verifying` + `Done` + `Retry`
- **AND** `LoopResult run(const std::string& goal, const LayeredContext& ctx)` 公开方法存在
- **AND** 3 私有阶段方法: `plan_phase()` + `execute_phase()` + `verify_phase()`

#### Scenario: 3 阶段顺序
- **WHEN** 调用 `loop.run(goal, ctx)` 
- **THEN** 状态转移: Planning → Executing → Verifying → Done
- **AND** Verify 失败 (最多 3 次) → 重新 Planning (Retry)
- **AND** 3 次 Retry 后仍失败 → 整体失败返回

#### Scenario: MockLLMProvider 集成
- **WHEN** 测试用 MockLLMProvider 预设 Plan + Verify response
- **THEN** Plan 阶段返回预设子图列表
- **AND** Verify 阶段返回 true (成功) 或 false (失败)
- **AND** DSLEngine 执行子图成功返回 ExecutionResult

### Requirement: fork-join-loop-state-machine

`include/agenticdsl/pdk/agent_loops/fork_join_loop.h` MUST 定义 `class ForkJoinLoop` 含 3 阶段状态机: `Forking` → `Executing` → `Joining` → `Done`。

#### Scenario: 状态机定义
- **WHEN** 编译 `fork_join_loop.h`
- **THEN** `enum class State` 4 个值存在: `Forking` + `Executing` + `Joining` + `Done`
- **AND** `LoopResult run(const std::vector<std::string>& branches, const LayeredContext& ctx)` 公开方法存在
- **AND** 2 私有阶段方法: `fork_phase()` + `join_phase()` (Execute 由 DomainWorkerPool)

#### Scenario: 并发执行
- **WHEN** 调用 `loop.run({"branch1", "branch2", "branch3"}, ctx)`
- **THEN** DomainWorkerPool 并发派发 3 个 branch 任务
- **AND** worker 内部执行 SimpleCognitiveOrchestrator.process()
- **AND** Join 阶段按 branch_id 顺序合并结果
- **AND** 1 个 branch 失败 → 整体失败返回

#### Scenario: 合并确定性
- **WHEN** 3 个 branch 完成后
- **THEN** Join 结果 LayeredContext.working 包含 3 个 branch 输出的并集
- **AND** 顺序确定 (branch_id 排序)
- **AND** 重复 key 后 branch 覆盖前 branch (确定)

### Requirement: agent-macros-3-loop-types

`include/agenticdsl/pdk/agent_macros.h` MUST 支持 3 种 `AgentLoopType` 编译通过: `React` (Sprint 4) + `PlanExecute` (本 change) + `ForkJoin` (本 change)。`static_assert` 限制 MUST 移除。

#### Scenario: 3 种 LoopType 编译
- **WHEN** 用户写 `DEFINE_AGENT(LoopType::React, "MyAgent", ...)`
- **THEN** 编译通过, 展开为 `class MyAgent` 内部使用 `ReactLoop`

#### Scenario: PlanExecute 编译
- **WHEN** 用户写 `DEFINE_AGENT(LoopType::PlanExecute, "PlannerAgent", ...)`
- **THEN** 编译通过, 展开为 `class PlannerAgent` 内部使用 `PlanExecuteLoop`

#### Scenario: ForkJoin 编译
- **WHEN** 用户写 `DEFINE_AGENT(LoopType::ForkJoin, "ForkerAgent", ...)`
- **THEN** 编译通过, 展开为 `class ForkerAgent` 内部使用 `ForkJoinLoop`

#### Scenario: 模板分发零开销
- **WHEN** 编译 `agent_macros.h`
- **THEN** `LoopDispatcher<LoopType>` 主模板 + 3 个 specialization 存在
- **AND** 编译期 `Type` 解析, 0 运行时开销
- **AND** `[[deprecated]]` 警告 (如使用旧 static_assert 路径) 0 个

### Requirement: pdk-loop-test-coverage

`tests/test_pdk_plan_execute.cpp` + `tests/test_pdk_fork_join.cpp` MUST 各包含 5 个 TEST_CASE, 覆盖关键场景。

#### Scenario: PlanExecute 测试 (5 个)
- **WHEN** 运行 `ctest -R test_pdk_plan_execute`
- **THEN** 5 个 TEST_CASE 全 PASS:
  - 规划成功 + 验证成功 → Done
  - 规划成功 + 验证失败 + 重试成功 → Done
  - 规划失败 → 整体失败
  - MockLLMProvider 空响应 → 失败
  - Retry 3 次后仍失败 → 整体失败

#### Scenario: ForkJoin 测试 (5 个)
- **WHEN** 运行 `ctest -R test_pdk_fork_join`
- **THEN** 5 个 TEST_CASE 全 PASS:
  - 3 branch 并发 + 全成功 → Done
  - 2 branch + 1 失败 → 整体失败
  - 1 branch (degenerate) → Done
  - 4 branch 并发 + 合并顺序正确
  - 异常隔离 (branch 抛异常) → worker 不挂

#### Scenario: 完整 ctest 58/58 PASS
- **WHEN** 运行 `cd build/tests && ctest`
- **THEN** 58/58 PASS (48 现有 + 5 PlanExecute + 5 ForkJoin)
- **AND** React 路径零回归

### Requirement: http-mock-server-helper

`tests/test_helpers/http_mock_server.h` MUST 定义 `class HttpMockServer` RAII helper，含构造启动 + 析构关闭 + 公开 `port()` 和 `server()` 方法。

#### Scenario: Helper 类定义
- **WHEN** 编译 `http_mock_server.h`
- **THEN** `class HttpMockServer` 存在
- **AND** 构造: 启动 `httplib::Server` + 绑定 `127.0.0.1` 随机端口 + 启动后台 thread + `sleep_for(100ms)`
- **AND** 析构: `server_.stop()` + `thread.join()` 自动调用
- **AND** 公开方法: `int port() const` + `httplib::Server& server()`
- **AND** 禁用拷贝 (`HttpMockServer(const HttpMockServer&) = delete`)

#### Scenario: 绑定失败抛异常
- **WHEN** 端口绑定失败 (e.g. httplib::Server 内部错误)
- **THEN** 构造抛 `std::runtime_error("failed to bind httplib mock server")`
- **AND** 析构不调用 (构造失败, 资源未分配)

#### Scenario: 自动清理
- **WHEN** `HttpMockServer mock;` 离开 scope
- **THEN** 析构自动调用 `server_.stop()`
- **AND** 后台 thread 自动 join
- **AND** 0 server thread 泄漏 (与当前手写模板行为等价)

### Requirement: test-http-adapter-helper-integration

`tests/test_http_adapter.cpp` 4 处手写 httplib server 模板 MUST 替换为 `HttpMockServer` helper 使用。

#### Scenario: 4 处替换
- **WHEN** 实施本 change
- **THEN** 4 个 TEST_CASE (200 OK / 404 / 500 / SSE chunk) 全部使用 `HttpMockServer mock;`
- **AND** 每个 case 净减少 ~6 行 (去除 bind_to_any_port + thread + sleep_for + stop + join 模板)
- **AND** 文件总行数 180 → 150 (-30 行)

#### Scenario: 行为完全保持
- **WHEN** 运行 `ctest -R test_http_adapter`
- **THEN** 7 TEST_CASE / 16 assertions 全 PASS
- **AND** 200 OK 测试返回 `Hello from mock`
- **AND** 404 / 500 测试返回正确 ErrorCode
- **AND** SSE chunk 测试 16 chars / 2 chunks 正确累积

#### Scenario: 零回归
- **WHEN** 运行 `cd build/tests && ctest`
- **THEN** 48/48 PASS (重构等价, 0 行为变化)

