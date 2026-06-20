# Spec: PDK Skeleton (Sprint 4 增量规格)

## ADDED Requirements

### Requirement: declare-tool-macro

`DECLARE_TOOL` 宏 MUST 提供工具注册脚手架, 展开为 ToolSpec 元数据 + 错误处理包装的 handler 函数, 自动捕获 std::exception 并返回 nlohmann::json 错误对象.

#### Scenario: DECLARE_TOOL 展开生成 ToolSpec + handler

- **WHEN** 开发者使用 `DECLARE_TOOL(echo_tool, "回显工具") { ... }` 声明工具
- **THEN** 宏 MUST 展开为 `tool_spec_echo_tool` (ToolSpec 实例) 含 name="echo_tool", description="回显工具"
- **AND** 宏 MUST 展开为 `tool_handler_echo_tool(const nlohmann::json& args)` 函数
- **AND** handler 调用 MUST 通过 try-catch 包装, 异常时返回 `{{"error", e.what()}}` 而非传播至调用方
- **AND** 宏 MUST 在 `hydraforge::pdk` 命名空间下声明

#### Scenario: DECLARE_TOOL handler 异常隔离

- **WHEN** handler 函数体内 `__VA_ARGS__` 抛 std::runtime_error("disk full")
- **THEN** DECLARE_TOOL 包装 MUST 捕获异常
- **AND** MUST 返回 `nlohmann::json{{"error", "disk full"}}` (而非 std::runtime_error 传播)
- **AND** 调用方 MUST 收到合法 json 对象, 可安全访问 `.contains("error")` 判断失败

#### Scenario: DECLARE_TOOL 5 行领域逻辑示例

- **GIVEN** 开发者编写:
  ```cpp
  DECLARE_TOOL(edit_file, "编辑文件") {
      PARAM(path, string, required);
      PARAM(content, string, optional);
      auto p = GET_PARAM(path);
      // ... 领域逻辑 (5 行)
      RETURN_SUCCESS();
  }
  ```
- **THEN** 编译后 MUST 生成 tool_spec_edit_file + tool_handler_edit_file
- **AND** 编译时间增加 < 100ms (宏展开 O(1))
- **AND** 生成的 handler 调用者使用 `tool_handler_edit_file(json)` 即可

### Requirement: define-agent-template

`DEFINE_AGENT` 宏 MUST 提供 Agent 循环脚手架, MVP 仅实现 React (思考-行动-观察) 循环, Phase 2 扩展为 PlanExecute / ForkJoin.

#### Scenario: DEFINE_AGENT MVP React 循环

- **WHEN** 开发者使用 `DEFINE_AGENT(coding_assistant, REACT_LOOP_TEMPLATE) { ... }` 定义 Agent
- **THEN** 宏 MUST 展开为 `class coding_assistantAgent` 含构造方法 + `run(prompt)` 方法
- **AND** run() MUST 内部循环: 思考 → 调用 SimpleCognitiveOrchestrator → 观察结果 → 直到任务完成
- **AND** Agent class MUST 持有独立 DSLEngine + IInteractionBus 引用 (per-agent 隔离, ADR-0020 §2.2.1)
- **AND** 编译时间增加 < 200ms

#### Scenario: DEFINE_AGENT 模板实例化

- **WHEN** 编译器实例化 `coding_assistantAgent` class
- **THEN** 必须能成功编译 (无模板错误)
- **AND** `coding_assistantAgent().run("test prompt")` MUST 不崩溃, 返回 ToolResult
- **AND** 异常时 MUST 返回 ok=false ToolResult 含 error_code + error_message (ADR-0023 P2 标准化)

#### Scenario: DEFINE_AGENT MVP 仅 React, PlanExecute 留 TODO

- **WHEN** 开发者尝试 `DEFINE_AGENT(my_agent, PLAN_EXECUTE_TEMPLATE) { ... }`
- **THEN** MVP 实现 MUST 编译失败 (TODO 标记) 或 抛 static_assert
- **AND** 编译错误信息 MUST 明确指向 "PLAN_EXECUTE not implemented, see ADR-0021 Phase 2"

### Requirement: safe-exec-wrapper

`SafeExec` 类 MUST 提供声明式沙箱执行封装, MVP 实现仅支持超时控制 + 异常捕获, Phase 2/3 扩展 fork/cgroups/seccomp.

#### Scenario: SafeExec 超时控制

- **WHEN** `SafeExec` 配置 `with_timeout(10ms)`
- **AND** 调用 `exec.run([]{ std::this_thread::sleep_for(100ms); })`
- **THEN** run() MUST 在 10ms 后抛 `std::runtime_error("Tool execution timed out after 10ms")`
- **AND** 超时检测 MUST 通过 `std::async` + `wait_for(timeout)` 实现 (MVP 简化)

#### Scenario: SafeExec 异常传播

- **WHEN** 调用 `exec.run([]{ throw std::runtime_error("file not found"); })`
- **THEN** run() MUST 传播 std::runtime_error 至调用方 (不捕获)
- **AND** 调用方 MUST 收到原始异常 (而非包装)
- **AND** 测试 MUST 验证异常类型 + message 完全匹配

#### Scenario: SafeExec 链式配置

- **WHEN** 调用 `SafeExec().with_timeout(5s).with_layer_profile(LayerProfile::Workflow).run(fn)`
- **THEN** chain 配置 MUST 顺序应用 (timeout → layer_profile)
- **AND** run() MUST 反映最终配置 (timeout=5s)
- **AND** MVP layer_profile 可为 no-op (仅存元数据, Phase 2/3 实施)

#### Scenario: SafeExec 正常路径 (无超时无异常)

- **WHEN** 调用 `exec.run([]{ return 42; })` (timeout=1s, 函数立即返回)
- **THEN** run() MUST 返回 42 (与 invoke_result_t<F> 一致)
- **AND** 无异常无超时

### Requirement: pdk-monorepo-structure

`pdk/` monorepo 子目录 MUST 提供 PDK 的独立构建配置 (INTERFACE 库), 根 CMakeLists.txt MUST 通过 `add_subdirectory(pdk)` 集成, 便于 Phase 2 后拆分至独立 `hydraforge-pdk` 仓库 (K3 决策).

#### Scenario: pdk/ 子目录 + INTERFACE 库

- **WHEN** 根 CMakeLists.txt 添加 `add_subdirectory(pdk)`
- **THEN** CMake MUST 创建 `hydraforge_pdk` INTERFACE 库
- **AND** target_include_directories MUST 指向 `pdk/include/` (含 hydraforge/pdk/ 子目录)
- **AND** INTERFACE 库 MUST 设置 cxx_std_20 (PDK 使用 C++20 std::jthread 等)

#### Scenario: pdk 测试通过 add_subdirectory(pdk) 链接 PDK 头文件

- **WHEN** tests/test_pdk_macros.cpp 通过 `target_link_libraries(test_pdk_macros PRIVATE hydraforge_pdk)`
- **THEN** 编译 MUST 能找到 `<hydraforge/pdk/tool_macros.h>` 等头文件
- **AND** 测试 MUST 编译通过, 5/5 测试通过

#### Scenario: pdk/ 不依赖 Runtime 内部实现

- **WHEN** 检查 pdk/ 子目录下所有头文件的 #include 列表
- **THEN** MUST NOT 包含 `core/engine.h` 或 `modules/*/*.h` (Runtime 内部)
- **AND** MUST 仅包含 `agenticdsl/contract/*.h` (契约接口)
- **AND** Sprint 5 E2E 验证: `nm plugin.so | grep hydraforge_runtime` MUST 返回空 (P3 静态链接)

#### Scenario: monorepo `pdk/` 可独立拆分至 hydraforge-pdk 仓库

- **WHEN** Phase 2 决定拆分 (Sprint 4 ship 后异步 T4b)
- **THEN** 仅需 `git mv pdk/ ../hydraforge-pdk/` + 在 HydraForge 根 CMakeLists.txt 移除 `add_subdirectory(pdk)` + 添加 `find_package(hydraforge_pdk REQUIRED)`
- **AND** 拆分后 PDK 头文件路径保持不变 (`include/hydraforge/pdk/...`)
- **AND** 不需要修改任何 PDK 头文件代码 (接口已稳定, ADR-0021 P5)

### Requirement: pdk-standalone-repo

`hydraforge-pdk` 独立 GitHub 仓库推送 MUST 在 Sprint 4 ship 后异步执行 (T4b), 外部阻塞条件: GitHub 组织 `hydraforge` + 仓库 `hydraforge-pdk` 必须先存在.

#### Scenario: Sprint 4 ship 后 T4b 异步执行

- **WHEN** Sprint 4 monorepo `pdk/` 子目录 ship 完成 (commit `aa54605` 或后续)
- **AND** 用户决定执行 T4b (外部阻塞解除后)
- **THEN** `git remote add hydraforge-pdk git@github.com:hydraforge/hydraforge-pdk.git` MUST 执行
- **AND** `git push hydraforge-pdk main` MUST 将 monorepo `pdk/` 推送至独立仓库
- **AND** 推送后 hydraforge-pdk 仓库 MUST 可独立编译 (`cmake --build .` 成功)

#### Scenario: T4b 外部阻塞处理

- **WHEN** GitHub 组织 `hydraforge` 或仓库 `hydraforge-pdk` 不存在
- **THEN** T4b MUST 推迟至外部阻塞解除 (可异步执行,不阻塞 Sprint 5)
- **AND** monorepo `pdk/` 子目录 MUST 维持当前状态 (Phase 1 Sprint 4 验收基线)

### Requirement: pdk-sprint4-contract-lock

`pdk` Sprint 4 实施 MUST 锁定对外契约, Sprint 5+ 仅在以下范围内变更.

#### Scenario: 锁定对外 API

- **THEN** 头文件 `include/agenticdsl/pdk/tool_macros.h` MUST 导出 `DECLARE_TOOL` 宏 + `ToolSpec` struct
- **AND** 头文件 `include/agenticdsl/pdk/agent_macros.h` MUST 导出 `DEFINE_AGENT` 宏 + `AgentLoopType` enum
- **AND** 头文件 `include/agenticdsl/pdk/safe_exec.h` MUST 导出 `SafeExec` class
- **AND** 统一入口 `<agenticdsl/pdk/pdk.h>` MUST 引用 3 个子头
- **AND** 头文件 MUST NOT 引入 Runtime 内部 (`core/` `modules/`)

#### Scenario: Sprint 4 MVP 仅 React, Phase 2 扩展 PlanExecute

- **THEN** `AgentLoopType::React` MUST 实现完整循环 (MVP)
- **AND** `AgentLoopType::PlanExecute` / `AgentLoopType::ForkJoin` MAY 编译失败或 no-op (Phase 2 实施)
- **AND** Sprint 5 MAY 在不修改 PDK 头文件 API 前提下实现 PlanExecute
- **AND** Sprint 5 MAY 添加 `FakeStateStore` / `StubLLM` / `MockSandbox` (Phase 2 范围, P6 测试替身)

#### Scenario: Sprint 5 不破坏 PDK 接口

- **THEN** Sprint 5 PluginLoader MUST 通过 PDK 头文件加载 `.so` 插件
- **AND** Sprint 5 E2E demo (`examples/phase1_plugin_demo`) MUST 使用 PDK 工具 + DECLARE_TOOL 宏
- **AND** 任何 PDK 头文件 API 变更 MUST 通过新增 OpenSpec change (不允许静默修改)