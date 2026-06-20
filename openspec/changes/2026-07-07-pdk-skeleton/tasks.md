# Tasks: PDK Skeleton (Sprint 4)

> **变更类型**: 真实实现 (6 sub-tasks per plan §Sprint 4, T1 → T2 → T3 → T4a → T4b → T5)
> **关联 plan**: `.omo/plans/phase1-execution.md` §Sprint 4
> **关联 ADR**: docs/adr/adr-0021-pdk-design.md (🔍 Proposed → 🟡 Partial Sprint 4 ship)
> **关联 change**: `openspec/changes/2026-07-07-pdk-skeleton/`
> **创建日期**: 2026-06-16 (placeholder) → 2026-06-19 (filled)
> **修订说明**: W1D2.5 启动前置 → Sprint 4 启动时填充, 与 plan §Sprint 4 任务列表对齐

## Sprint 4 子任务 (5 commits, T4b 异步)

- [ ] S4.T1 — feat(pdk): add DECLARE_TOOL macro + ToolSpec metadata
- [ ] S4.T2 — feat(pdk): add DEFINE_AGENT template (React loop MVP)
- [ ] S4.T3 — feat(pdk): add SafeExec wrapper (timeout + exception MVP)
- [ ] S4.T4a — build(pdk): create monorepo pdk/ subdir + INTERFACE library
- [ ] S4.T4b — [Deferred/External] push to hydraforge-pdk GitHub repo
- [ ] S4.T5 — test(pdk): add 5 test cases for PDK macros (36/36 ctest)

## S4.T1: DECLARE_TOOL 宏 + ToolSpec 元数据

- [ ] **S4.T1.1** 新建 `include/agenticdsl/pdk/tool_macros.h` (~120 行)
  - 含 `hydraforge::pdk` 命名空间
  - 含 `ToolSpec` struct (name, description, params, permissions)
  - 含 `ToolParam` struct (name, type, required)
  - 含 `ToolPermissions` struct (readonly_paths, write_paths, network)
  - 含 `DECLARE_TOOL(name, description, ...)` 宏
  - 宏展开为 inline `tool_spec_##name` (ToolSpec) + inline `tool_handler_##name` (try-catch 包装)
  - 头文件仅 include `agenticdsl/contract/itool_registry.h` + nlohmann_json + 标准库

- [ ] **S4.T1.2** 文件头注释完整
  - 功能描述: DECLARE_TOOL 宏 — 工具注册脚手架 (ADR-0021 §3.1)
  - 设计依据: ADR-0021 §3.1 + ADR-0004 ToolRegistry 安全模型
  - 作者: AgenticDSL Phase 1 Sprint 4
  - 最后修改日期: 2026-06-19

- [ ] **S4.T1.3** 新建 `include/agenticdsl/pdk/CMakeLists.txt`
  - `add_library(agenticdsl_hdr_pdk INTERFACE)`
  - `target_include_directories` 指向 `pdk/include/`
  - `target_compile_features cxx_std_20`
  - `target_link_libraries agenticdsl_contract`

- [ ] **S4.T1.4** 头文件独立编译验证
  - DECLARE_TOOL 示例代码 (5 行领域逻辑) 编译通过
  - 静态分析: `grep -r "core/\|modules/\|common/" pdk/include/` 返回空 (P3 验证)
  - 无 lsp_diagnostics error

**T1 验收**:
- [ ] `tool_macros.h` 存在
- [ ] `CMakeLists.txt` (pdk/) 存在
- [ ] DECLARE_TOOL 示例编译通过

## S4.T2: DEFINE_AGENT 模板 — React 循环 MVP

- [ ] **S4.T2.1** 新建 `include/agenticdsl/pdk/agent_macros.h` (~80 行)
  - 含 `AgentLoopType` enum (React, PlanExecute, ForkJoin)
  - 含 `DEFINE_AGENT(name, loop_type)` 宏
  - MVP 仅支持 React, PlanExecute/ForkJoin 通过 static_assert 编译失败
  - 宏展开为 `class XXXAgent` 含构造 (DSLEngine + IInteractionBus) + run(prompt)
  - run() 委托 SimpleCognitiveOrchestrator 单轮 ReAct

- [ ] **S4.T2.2** 文件头注释完整
  - 功能描述: DEFINE_AGENT 宏 — Agent 循环脚手架 (ADR-0021 §3.2)
  - MVP 限制: 仅 React loop, PlanExecute/ForkJoin 留 Phase 2

- [ ] **S4.T2.3** 头文件独立编译验证
  - `coding_assistantAgent` 编译通过 (React 实例化)
  - `PlanExecute` 实例化编译失败 + static_assert 错误信息明确
  - 无 lsp_diagnostics error

**T2 验收**:
- [ ] `agent_macros.h` 存在
- [ ] DEFINE_AGENT(React) 示例编译通过
- [ ] DEFINE_AGENT(PlanExecute) 编译失败 + 明确错误

## S4.T3: SafeExec 封装 — 超时 + 异常 (MVP)

- [ ] **S4.T3.1** 新建 `include/agenticdsl/pdk/safe_exec.h` (~70 行)
  - 含 `SafeExec` class
  - 链式配置: `with_timeout(milliseconds)` + `with_layer_profile(int)` (MVP no-op)
  - 模板方法 `run(F&& fn) -> std::invoke_result_t<F>`
  - MVP 实现: `std::async` + `wait_for(timeout)` + 异常传播
  - 超时抛 `std::runtime_error("SafeExec: tool execution timed out after Nms")`

- [ ] **S4.T3.2** 文件头注释完整
  - 功能描述: SafeExec 沙箱执行封装 (ADR-0021 §3.3)
  - MVP 限制: 仅超时 + 异常, fork/cgroups/seccomp 留 Phase 3

- [ ] **S4.T3.3** 头文件独立编译验证
  - SafeExec 实例化编译通过
  - 超时测试: 10ms timeout + 100ms sleep → 抛 runtime_error
  - 异常测试: handler 抛 std::runtime_error → SafeExec 传播原异常
  - 正常路径: SafeExec.run([]{ return 42; }) 返回 42

**T3 验收**:
- [ ] `safe_exec.h` 存在
- [ ] SafeExec 三种路径 (超时/异常/正常) 编译 + 单元测试通过

## S4.T4a: monorepo `pdk/` 子目录 + INTERFACE 库

- [ ] **S4.T4a.1** 新建 `pdk/` 子目录结构
  ```
  pdk/
  ├── CMakeLists.txt           # INTERFACE 库配置
  └── include/
      └── hydraforge/
          └── pdk/
              ├── pdk.h        # 统一入口
              ├── tool_macros.h
              ├── agent_macros.h
              └── safe_exec.h
  ```

- [ ] **S4.T4a.2** 新建 `pdk/CMakeLists.txt` (~25 行)
  - `add_library(hydraforge_pdk INTERFACE)`
  - `target_compile_features cxx_std_20`
  - `target_include_directories` 指向 `pdk/include/`
  - `target_link_libraries agenticdsl_contract` (P3 契约解耦)

- [ ] **S4.T4a.3** 新建 `pdk/include/hydraforge/pdk/pdk.h` (~10 行)
  - 引用 3 个子头: tool_macros.h + agent_macros.h + safe_exec.h
  - 统一入口

- [ ] **S4.T4a.4** 根 `CMakeLists.txt` 修改 (单行 add_subdirectory)
  - 添加 `add_subdirectory(pdk)` (位置: 现有 add_subdirectory 之后)
  - 不影响现有构建

- [ ] **S4.T4a.5** 构建验证
  - `cmake --build build` 编译通过
  - `target_link_libraries(test_pdk_macros PRIVATE hydraforge_pdk)` 链接成功
  - `nm` 验证 PDK 头文件不引入 Runtime 内部符号 (Sprint 5 E2E)

**T4a 验收**:
- [ ] `pdk/` 子目录结构完整
- [ ] `pdk/CMakeLists.txt` 配置正确
- [ ] 根 CMakeLists.txt `add_subdirectory(pdk)` 通过
- [ ] INTERFACE 库编译 + 链接成功

## S4.T4b: 推到独立 `hydraforge-pdk` GitHub 仓库 [Deferred/External]

- [ ] **S4.T4b.1** 外部阻塞检查
  - 验证 GitHub 组织 `hydraforge` 存在
  - 验证仓库 `hydraforge-pdk` 存在
  - 若不存在: T4b 推迟至外部阻塞解除 (不阻塞 Sprint 4 ship 与 Sprint 5 启动)

- [ ] **S4.T4b.2** 添加 remote
  - `git remote add hydraforge-pdk git@github.com:hydraforge/hydraforge-pdk.git`

- [ ] **S4.T4b.3** 推送 monorepo `pdk/` 子目录
  - `git push hydraforge-pdk main`
  - 仅推送 `pdk/` 子目录相关提交 (sparse checkout 或 subtree)

- [ ] **S4.T4b.4** hydraforge-pdk 仓库独立编译验证
  - `git clone git@github.com:hydraforge/hydraforge-pdk.git`
  - `cmake --build .` 成功

**T4b 验收** (外部阻塞解除后):
- [ ] hydraforge-pdk GitHub 仓库创建成功
- [ ] monorepo `pdk/` 子目录推送至独立仓库
- [ ] 独立仓库独立编译通过

**注**: T4b Sprint 4 ship 后异步执行, 不阻塞 Sprint 5 启动

## S4.T5: PDK 单元测试 (5 test cases)

- [ ] **S4.T5.1** 新建 `tests/test_pdk_macros.cpp` (~250 行)
  - 测试 1: DECLARE_TOOL 展开 — `tool_spec_echo_tool.name == "echo_tool"`, handler 调用返回 json
  - 测试 2: DEFINE_AGENT 模板实例化 — `coding_assistantAgent` 编译 + 构造 + run() 调用
  - 测试 3: SafeExec 超时处理 — `with_timeout(10ms)` + 100ms sleep → 抛 runtime_error
  - 测试 4: SafeExec 异常捕获 — handler 抛 std::runtime_error → SafeExec 传播原异常
  - 测试 5: PDK 头文件无 Runtime 内部依赖 — 静态扫描 + 编译验证

- [ ] **S4.T5.2** 文件头注释完整
  - 功能描述: PDK 单元测试 (Phase 1 Sprint 4)
  - 5 个 TEST_CASE 覆盖: macro/template/safe-exec/runtime-decoupling

- [ ] **S4.T5.3** 测试基础设施
  - 使用 Catch2 (沿用现有)
  - MockLLMProvider + DSLEngine mock (同 test_cognitive_worker.cpp)
  - 静态扫描: `grep -r "core/\|modules/\|common/" pdk/include/` 返回空

- [ ] **S4.T5.4** 测试编译 + 运行
  - 5/5 test case pass
  - 36/36 ctest pass (31 baseline + 5 new)
  - 零回归

**T5 验收**:
- [ ] `test_pdk_macros.cpp` 存在
- [ ] 5/5 test case pass
- [ ] 36/36 ctest pass (31 baseline + 5 new)

## 提交策略 (5 commits, per plan §Sprint 4)

```
S4.T1 → feat(pdk): add DECLARE_TOOL macro + ToolSpec metadata
S4.T2 → feat(pdk): add DEFINE_AGENT template (React loop MVP)
S4.T3 → feat(pdk): add SafeExec wrapper (timeout + exception MVP)
S4.T4a → build(pdk): create monorepo pdk/ subdir + INTERFACE library
S4.T5 → test(pdk): add 5 test cases for PDK macros (36/36 ctest)
S4.T4b → [Deferred] chore(repo): push pdk/ to hydraforge-pdk standalone repo
```

## 依赖与阻塞

- **Block by**: Sprint 0/1a/1b/P1/CognitiveWorker/DomainWorkerPool (✅ 全部 ship)
- **Block**: Sprint 5 PluginLoader (2026-07-14 ~ 2026-07-16, W5)
- **External Block** (S4.T4b): GitHub 组织 `hydraforge` + 仓库 `hydraforge-pdk` 存在性

## Sprint 4 收官验收

- [ ] 36/36 ctest PASS (31 baseline + 5 new test_pdk_macros)
- [ ] 5 commits (T1 → T2 → T3 → T4a → T5) 已 commit
- [ ] `openspec validate 2026-07-07-pdk-skeleton` exit 0
- [ ] `tools/adr_lint.py docs/adr/` exit 0 (ADR-0021 状态更新)
- [ ] ADR-0021 状态: 🔍 Proposed → 🟡 Partial (Sprint 4 ship)
- [ ] 零回归 (Sprint 1a/1b/P1/CognitiveWorker/DomainWorkerPool 全部 31 测试不变)
- [ ] PDK 头文件无 Runtime 内部依赖 (P3 静态链接验证)
- [ ] monorepo `pdk/` 子目录可独立拆分至 hydraforge-pdk 仓库 (Phase 2 T4b 准备)

## Phase 2/3 后续范围

- [ ] DEFINE_AGENT 完整 ReAct 循环 + PlanExecute + ForkJoin
- [ ] FakeStateStore / StubLLM / MockSandbox 测试替身 (P6)
- [ ] PluginLifecycle 类
- [ ] SafeExec 完整 fork/cgroups/seccomp 实现
- [ ] `hydraforge-pdk` 独立仓库发布 (S4.T4b 异步)
- [ ] CMake 生成器 (`cmake_init()` / `project_template()`)
- [ ] Sprint 5 E2E demo (`examples/phase1_plugin_demo` + PDK 工具)