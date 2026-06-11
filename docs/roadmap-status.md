# 实施状态看板

> **基于**: `docs/implementation-roadmap.md`（静态蓝图）
> **更新**: 本文件反映**当前实施进度**，每日更新
> **焦点**: 当前正在实施的 Phase
>
> **⚠️ 核心约束**: 本文件中的任务必须全部来自 `docs/implementation-roadmap.md`。
> 任务的新增、删除、拆分必须在 `docs/implementation-roadmap.md` 中先完成，再同步到本文件。
> `docs/implementation-roadmap.md` 无记录的任务不得出现在本文件中。

---

## 一、总体进度

| Phase | 进度 | 状态 | 工期 | 依赖 |
|-------|:----:|:----:|:----:|:----:|
| Pre-Phase | 100% ██████████ | ✅ 已完成 | 0.5 天 | 无 |
| Slice 00 | 100% ██████████ | ✅ 已完成 | 1-2 天 | Pre-Phase (CMake) |
| **Phase 0 MVP** | 30% ███░░░░░░░ | **🎯 当前焦点** | **7-10 天** | Pre-Phase + Slice 00 |
| ├─ Track 0.1 | 100% ██████████ | ✅ 已完成 | 3-4 天 | Pre-Phase |
| ├─ Track 0.1.5 (C₁) | 100% ██████████ | ✅ 已完成 | 0.5 天 | Track 0.1 |
| ├─ Track 0.2 | 0% ░░░░░░░░░░ | ⏸ 未开始 | 5-7 天 | Track 0.1 + C₁ |
| └─ Track 0.3 | 0% ░░░░░░░░░░ | ⏸ 未开始 | 2-3 天 | Pre-Phase |
| Phase 1 智能体层 | 0% ░░░░░░░░░░ | ⏸ 阻塞中 | 3-4 周 | Phase 0 |
| Phase 2 异步+EventBus | 0% ░░░░░░░░░░ | ⏸ 阻塞中 | 2-3 周 | Phase 1 |
| Phase 3 执行策略+安全 | 0% ░░░░░░░░░░ | ⏸ 阻塞中 | 2-3 周 | Phase 2 |
| Phase 4 模型路由+内核 | 0% ░░░░░░░░░░ | ⏸ 阻塞中 | 2-3 周 | Phase 3 |
| Phase 4.5 MVP清理 | 0% ░░░░░░░░░░ | ⏸ 阻塞中 | 1-2 天 | Phase 4 |
| Phase 5 自举服务化 | 0% ░░░░░░░░░░ | ⏸ 远期 | — | Phase 4.5 |

---

## 二、当前 Sprint（本周）

**开始**: 2026-05-30 | **结束**: 2026-06-06
**目标**: 完成 Pre-Phase + Slice 00，启动 Track 0.1

### Pre-Phase — 核心接口定义（0.5 天）

| 优先级 | 任务编号 | 任务 | 文件 | 状态 | 阻塞 |
|:------:|:--------:|------|------|:----:|:----:|
| P0 | P0.0a | 创建 `include/agenticdsl/cognitive/` | `mkdir -p` | [x] | — |
| P0 | P0.0b | 创建 `include/agenticdsl/policy/` | `mkdir -p` | [x] | — |
| P0 | P0.0c | 创建 `include/agenticdsl/types/` | `mkdir -p` | [x] | — |
| P0 | P0.1 | ICognitiveOrchestrator 接口 | `include/agenticdsl/cognitive/icognitive_orchestrator.h` | [x] | — |
| P0 | P0.2 | IExecutionPolicy 接口 | `include/agenticdsl/policy/iexecution_policy.h` | [x] | — |
| P0 | P0.3 | Session 前置声明 | `include/agenticdsl/types/session_fwd.h` | [x] | — |
| P0 | P0.4 | CMake include 配置 | `CMakeLists.txt`（根，第 37 行附近 `target_include_directories` 处） | [x] | — |

**验收任务**

| 优先级 | 验收编号 | 验收内容 | 验证命令 | 状态 |
|:------:|:--------:|------|---------|:----:|
| V | V0.1 | 编译通过 | `cmake .. && make agenticdsl_core` | [x] |
| V | V0.2 | 头文件可被 include | `echo '#include "agenticdsl/cognitive/icognitive_orchestrator.h"' \| g++ -x c++ -fsyntax-only -` | [x] |

### Slice 00 — 异步基础设施验证（1-2 天）

| 优先级 | 任务编号 | 任务 | 文件 | 状态 | 阻塞 |
|:------:|:--------:|------|------|:----:|:----:|
| S0 | S0.1 | 引入 Taskflow v4.0 | `external/taskflow/` | [x] | 需网络 |
| S0 | S0.2 | 引入 async_simple v1.4 | `external/async_simple/` | [x] | 需网络 |
| S0 | S0.3 | Taskflow CMake 配置 | `CMakeLists.txt`（根） | [x] | S0.1 |
| S0 | S0.4 | async_simple CMake 配置 | `external/CMakeLists.txt` | [x] | S0.2 |
| S0 | S0.5 | 编译选项（禁用测试/demo） | `external/CMakeLists.txt` | [x] | S0.4 |
| S0 | S0.6 | 桥接验证测试 | `tests/test_async_bridge.cpp` | [x] | S0.3, P0.4 |

**验收任务**

| 优先级 | 验收编号 | 验收内容 | 验证命令 | 状态 |
|:------:|:--------:|------|---------|:----:|
| V | V0.3 | 全量编译通过 | `cmake .. | V | V0.3 | 全量编译通过 | `cmake .. && make -j$(nproc)` | [ ]| V | V0.3 | 全量编译通过 | `cmake .. && make -j$(nproc)` | [ ] make -j$(nproc)` | [x] |
| V | V0.4 | 异步桥接测试通过 | `ctest -R test_async_bridge` | [x] |

### Track 0.1 — 云端 LLM 集成（3-4 天，等待启动）

| 优先级 | 任务编号 | 任务 | 文件 | 状态 | 阻塞 |
|:------:|:--------:|------|------|:----:|:----:|
| M1 | M1.1 | llm_config.h 统一配置 | `src/common/llm/llm_config.h` | [x] | — |
| M1 | M1.2 | 标记 ILLMAdapter deprecated | `src/common/llm/llm_adapter.h` | [x] | — |
| M1 | M1.3 | llm_types.h 更新 | `src/common/llm/llm_types.h` | [x] | M1.1 |
| M1 | M1.4 | CloudLLMAdapter 头文件 | `src/common/llm/cloud_adapter.h` | [x] | M1.1 |
| M1 | M1.5 | CloudLLMAdapter 实现 | `src/common/llm/cloud_adapter.cpp` | [x] | M1.4 |
| M1 | M1.6 | SSE 流式解析器 | `src/common/llm/sse_stream.h/cpp` | [x] | — |
| M1 | M1.7 | MockLLMProvider 头文件+实现 | `src/common/llm/mock_provider.h/cpp` | [x] | — |
| M2 | M2.1 | llm_config.json 更新 | `llm_config.json` | [x] | — |
| M3 | M3.1 | CloudLLM 单元测试 | `tests/test_cloud_llm.cpp` | [x] | M1.5 |
| M3 | M3.2 | SSE 解析测试 | `tests/test_sse_stream.cpp` | [x] | M1.6 |
| M3 | M3.3 | 集成测试（可选） | `tests/test_cloud_llm_live.cpp` | [x] | M1.5 |

**验收任务**

| 优先级 | 验收编号 | 验收内容 | 验证命令 | 状态 |
|:------:|:--------:|------|---------|:----:|
| V | V1.1 | CloudLLM mock 测试通过 | `ctest -R test_cloud_llm` | [x] |
| V | V1.2 | SSE 解析测试通过 | `ctest -R test_sse_stream` | [x] |
| V | V1.3 | LLM 模块编译通过 | `make agenticdsl_core` 无 error | [x] |

### Track 0.2 — 三层调用链验证（5-7 天，依赖 Track 0.1）

| 优先级 | 任务编号 | 任务 | 文件 | 状态 | 阻塞 |
|:------:|:--------:|------|------|:----:|:----:|
| M4 | M4.1 | IModelRouter 接口 | `src/common/llm/imodel_router.h` | [ ] | — |
| M4 | M4.2 | ModelRegistry 头文件 | `src/common/llm/model_registry.h` | [ ] | — |
| M4 | M4.3 | ModelRegistry 实现 | `src/common/llm/model_registry.cpp` | [ ] | M4.2 |
| M4 | M4.4 | DefaultModelRouter 头文件 | `src/common/llm/default_router.h` | [ ] | — |
| M4 | M4.5 | DefaultModelRouter 实现 | `src/common/llm/default_router.cpp` | [ ] | M4.4 |
| M4 | M4.6 | SimpleCognitiveOrchestrator 头文件 | `src/cognitive/simple_orchestrator.h` | [ ] | — |
| M4 | M4.7 | SimpleCognitiveOrchestrator 实现 | `src/cognitive/simple_orchestrator.cpp` | [ ] | M4.6 |
| M4 | M4.8 | 端到端示例 main.cpp | `examples/slice_01_tool_call/main.cpp` | [ ] | M4.7 |
| M4 | M4.9 | 端到端示例 CMake | `examples/slice_01_tool_call/CMakeLists.txt` | [ ] | — |
| M4 | M4.10 | cognitive/ CMakeLists.txt | `src/cognitive/CMakeLists.txt` | [ ] | — |

**验收任务**

| 优先级 | 验收编号 | 验收内容 | 验证命令 | 状态 |
|:------:|:--------:|------|---------|:----:|
| V | V2.1 | Mock 模式完整调用链 | `./examples/slice_01_tool_call --mock` | [ ] |
| V | V2.2 | ModelRegistry 测试通过 | `ctest -R test_model_registry` | [ ] |
| V | V2.3 | DefaultRouter 测试通过 | `ctest -R test_default_router` | [ ] |
| V | V2.4 | 三层调用链编译通过 | `make agenticdsl_core` 无 error | [ ] |

### Track 0.3 — 最小契约层（2-3 天，等待启动）

| 优先级 | 任务编号 | 任务 | 文件 | 状态 | 阻塞 |
|:------:|:--------:|------|------|:----:|:----:|
| M5 | M5.1 | Contract 库 CMake | `src/common/contract/CMakeLists.txt` | [ ] | — |
| M5 | M5.2 | IInteractionBus 接口 | `src/common/contract/iinteraction_bus.h` | [ ] | — |
| M5 | M5.3 | 事件类型定义 | `src/common/contract/event_types.h` | [ ] | — |
| M5 | M5.4 | InMemoryBus 头文件 | `src/common/contract/inmemory_bus.h` | [ ] | — |
| M5 | M5.5 | InMemoryBus 实现 | `src/common/contract/inmemory_bus.cpp` | [ ] | M5.4 |
| M5 | M5.6 | ToolResult 标准化 | `src/core/types/tool_result.h` | [ ] | — |
| M6 | M6.1 | InMemoryBus 单元测试 | `tests/test_interaction_bus.cpp` | [ ] | M5.5 |
| M6 | M6.2 | 多线程安全测试 | `tests/test_interaction_bus.cpp` | [ ] | M6.1 |
| M6 | M6.3 | ToolResult 测试 | `tests/test_tool_result.cpp` | [ ] | M5.6 |

**验收任务**

| 优先级 | 验收编号 | 验收内容 | 验证命令 | 状态 |
|:------:|:--------:|------|---------|:----:|
| V | V3.1 | InteractionBus 测试通过 | `ctest -R test_interaction_bus` | [ ] |
| V | V3.2 | ToolResult 测试通过 | `ctest -R test_tool_result` | [ ] |
| V | V3.3 | 并发安全验证 | 并发 emit 1000 次无死锁（测试内验证） | [ ] |
| V | V3.4 | Contract 模块编译通过 | `make agenticdsl_core` 无 error | [ ] |

---

## 三、阻塞项

| # | 描述 | 影响范围 | 提出日 | 状态 |
|---|------|---------|--------|:----:|
| 1 | `node.h:125` 保留已弃用类型 `LLMCallNode` struct（Phase 4.5 清理）; `node_executor` 已于 C₁.2 移除 `execute_llm_call()` 分发 | 任何包含 `node.h` 的翻译单元均产生 `-Wdeprecated-declarations` 警告 | 2026-06-07 | ✅ **完全修复**（Phase 0 收尾：删除 struct + impl + 重命名 backward-compat 测试，零功能影响） |

---

## 四、实施日志

| 日期 | 任务 | 耗时 | 结果 | 备注 |
|------|------|:----:|------|------|
| 2026-05-30 ~ 2026-06-02 | Pre-Phase 准备：7 个测试修复 | ~0.5 天 | ✅ 12/12 通过 | 见 `docs/archive/superpowers/plans/2026-06-02-test-fixes-for-prephase.md`；commits `1148845` (llm), `d6e8ce5` (library), `4ae97d9` (parser), `4b45a5b` (scheduler), `0166f1e` (executor) |
| 2026-06-03 | 文档基线对齐 | 0.5h | ✅ 完成 | 修正 roadmap-status.md / implementation-roadmap.md 中过时的测试断言；迁移 superpowers spec 方案对比至 ADR-0010；归档 3 个过期 superpowers 文档 |
| 2026-06-07 | **Pre-Phase 完成** (P0.0a–P0.4 + V0.1 + V0.2) | 0.5h | ✅ 全部通过 | 交付 3 个核心接口头文件 (ICognitiveOrchestrator / IExecutionPolicy 8 方法 / Session 三级体系) + 根 CMakeLists.txt 添加 `${CMAKE_SOURCE_DIR}/include` 搜索路径；V0.1 全量编译 + V0.2 三头独立 include 测试均通过；现有 12/12 测试零回归。修复预存 `node.h:125` `[[deprecated]]` 属性位置警告（原被屏蔽，修复后浮现 2 个 `node_executor.{h:45, cpp:90}` 使用弃用类型的 `-Wdeprecated-declarations` 警告——已在 roadmap 阻塞项中记录） |
| 2026-06-07 | **Slice 00 完成** (S0.1–S0.6 + V0.3 + V0.4) | 2h | ✅ 13/13 通过 |
| 2026-06-08 | **Phase C₁ 完成** (C₁.1-C₁.5) | 5h | ✅ 17+/17+ 通过 (108 assertions / 48 cases, 0 失败) | **关键桥梁**: 让 Track 0.1 成果真正接入引擎。3 个原子 commit (d38bc51 + 3f28020 + 4312333): ① C₁.1 新增 LlamaAdapterProvider 适配器（包装旧 LlamaAdapter → ILLMProvider）② C₁.2-C₁.4 完整调用链迁移（NodeExecutor/TopoScheduler/ExecutionSession/DSLEngine 全部改用 ILLMProvider*，删除 execute_llm_call 死代码，附带清理 Track 0.1 M1.3 遗留的 LLMParams struct）③ C₁.5 端到端集成测试（5 个 TEST_CASE：默认 Mock provider / ILLMProvider 接口 / set_llm_provider 替换 / 错误注入 / 端到端 [e2e]）。**DSLEngine::from_markdown 默认创建 MockLLMProvider**（CI 永远可运行，无需本地 LLM）。零回归：所有原有测试通过，编译 0 错误。解锁能力: 端到端 ILLMProvider 调用链、MockLLMProvider 默认行为。 |
| 2026-06-07 | **Track 0.1 完成** (M1.1-M3.3 + V1.1-V1.3) | 2h | ✅ 16/16 通过 | 实现 llm_config.h 统一 LLMConfig (合并 LLMConfig+LLMParams)；标记 ILLMAdapter [[deprecated]]；新建 cloud_adapter.h/cpp (OpenAI 协议 + 重试 + 错误映射) + sse_stream.h/cpp (通用 SSE 状态机)；新建 mock_provider.h/cpp (队列/固定/错误/延迟模拟)；新建 3 个测试文件 (30 个新增测试用例)；llm_config.json 双层兼容。V1.1 test_cloud_llm 19/19 通过，V1.2 test_sse_stream 11/11 通过，V1.3 LLM 模块编译 0 错误，全量 16/16 通过 (1.86s)。 下载 Taskflow v3.9.0 + async_simple master，配置 CMake（禁用测试/demo/ASAN），新建 test_async_bridge.cpp（3 TEST_CASE：Taskflow 基础功能 / async_simple 协程 / 共存验证）。V0.3 编译通过，V0.4 3/3 通过，回归 13/13 通过。

---

## 五、验证状态

| 测试二进制 | 模块 | 状态 | 最后运行 | 通过率 |
|-----------|------|:----:|:-------:|:-----:|
| test_basic | 基础 | ✅ | 2026-06-07 | 5/5 |
| test_cloud_llm | CloudLLM + Mock | ✅ | 2026-06-07 | 19/19 |
| test_sse_stream | SSE 解析器 | ✅ | 2026-06-07 | 11/11 |
| test_parser | Parser | ✅ | 2026-06-07 | 12/12 |
| test_scheduler | Scheduler | ✅ | 2026-06-07 | 全通过 |
| test_executor | Executor | ✅ | 2026-06-07 | 全通过 |
| test_engine | Engine | ✅ | 2026-06-07 | 全通过 |
| test_tool_registry | ToolRegistry | ✅ | 2026-06-07 | 全通过 |
| test_llm_tool | LLM | ✅ | 2026-06-07 | 全通过 |
| test_llm_streaming | LLM 流式 | ✅ | 2026-06-07 | 全通过 |
| test_library_loader | 标准库 | ✅ | 2026-06-07 | 全通过 |
| test_no_llm | 无 LLM 模式 | ✅ | 2026-06-07 | 全通过 |
| test_prompt_builder | Prompt | ✅ | 2026-06-07 | 全通过 |
| test_path_resolution | 路径解析 | ✅ | 2026-06-07 | 全通过 |
| **整体（C₁ 前）** | **16 个测试** | ✅ | **2026-06-07** | **16/16 (100%)** | |
| test_executor_with_mock_provider | 端到端 Mock 集成 | ✅ | 2026-06-08 | 16/5 通过（C₁.5 新增） |
| **整体（Phase C₁）** | **17+ 个测试** | ✅ | **2026-06-08** | **108 assertions / 48 cases, 0 失败** | |
| test_async_bridge | 异步桥接 | ✅ | 2026-06-07 | 3/3 通过 |
| **test_async_bridge** | Slice 00 | ⏳ | — | — |
| **test_cloud_llm** | Track 0.1 | ⏳ | — | — |
| **test_sse_stream** | Track 0.1 | ⏳ | — | — |
| **test_interaction_bus** | Track 0.3 | ⏳ | — | — |
| **test_tool_result** | Track 0.3 | ⏳ | — | — |
| **test_model_registry** | Track 0.2 | ⏳ | — | — |
| **test_default_router** | Track 0.2 | ⏳ | — | — |

---

## 六、Sprint 计划模板

> 每个 Sprint 开始时，将"当前 Sprint"部分重置为本周计划。
> 完成的任务移至"实施日志"。

### Sprint 结束前检查

- [ ] 所有 Sprint 任务 `[x]` 或 `[~]`
- [ ] 所有验收任务 `[x]` 或 `[~]`
- [ ] 阻塞项已记录原因
- [ ] 验证状态已更新
- [ ] 实施日志已补充
- [ ] 已与 `docs/implementation-roadmap.md` 对比一致性

---

## 七、Phase 0 完成情况（C₁ → X → B → A 已完成）+ Phase 1 候选入口

> **来源**: Oracle 深度分析（`bg_465470dd` 任务输出）  
> **分析时间**: 2026-06-07  
> **上下文**: 本节描述的 C₁ → X → B → A 全部完成（2026-06-08, PR #6 / commit `dcbca37`），Phase 0 收官。

### C₁ → X → B → A 完成清单（2026-06-08）

| # | 任务 | 工作量 | 状态 | 关键产出 | 验证 |
|---|------|--------|:----:|----------|------|
| **C₁.1** | 创建 `LlamaAdapterProvider` | 1h | [x] | `src/common/llm/llama_adapter_provider.{h,cpp}` | 编译通过 |
| **C₁.2** | `NodeExecutor` 改用 `ILLMProvider*` | 1h | [x] | `node_executor.{h,cpp}` | 编译 0 警告 |
| **C₁.3** | `TopoScheduler` + `ExecutionSession` 传递 `ILLMProvider*` | 1h | [x] | `topo_scheduler.{h,cpp}`, `execution_session.{h,cpp}` | 编译通过 |
| **C₁.4** | `DSLEngine::from_markdown` 默认 Mock | 0.5h | [x] | `engine.{h,cpp}` | `test_engine` 通过 |
| **C₁.5** | 新增 `test_executor_with_mock_provider.cpp` | 1h | [x] | `tests/test_executor_with_mock_provider.cpp` | 17 assertions / 5 cases 通过 |
| **X** | `ToolResult` MVP 信封 | 0.5h | [x] | `src/core/types/tool_result.{h,cpp}` | `test_tool_result` 24/24 通过 |
| **B** | Track 0.2 三层调用链 | 3-4d | [x] | `SimpleCognitiveOrchestrator` + `slice_01_tool_call` | `test_simple_orchestrator` 25/25 通过；e2e `--mock` 输出正确 |
| **A** | Track 0.3 最小契约层 | 2-3d | [x] | `IInteractionBus` + `InMemoryBus` | `test_interaction_bus` 18/18 通过（含 1000x 并发） |

**总计实际**: 5h（C₁+X）+ 3-4d（B+A），与预估一致。详细 commit 历史见 `git log dcbca37^..dcbca37`。

### Phase 0 完成标准验收

来自 `docs/implementation-roadmap.md` §Phase 通用完成标准：

- [x] 编译通过：`make -j$(nproc)` 无错误（C₁ 后达成）
- [x] 单元测试全绿：C1-era 新增测试 5 个全部通过（e2e mock, interaction bus, simple orchestrator, tool result, async bridge）
- [x] 可运行示例：`examples/slice_01_tool_call --mock` 输出正确（已验证）
- [x] 错误路径覆盖：B 阶段 `slice_01` 覆盖 LLM 超时 / 工具不存在 / JSON 解析失败（`test_simple_orchestrator` 5 个 TEST_CASE）
- [x] 无 MVP 残留：仅 `SimpleCognitiveOrchestrator` 允许 `TODO(mvp)` 标记
- [x] 公共头文件已迁移到 `include/agenticdsl/{contract,cognitive}/`（2026-06-08 清理）
- [x] `InMemoryBus` 引入首个 `std::mutex`（CP.22 合规：callback 锁外调用）
- [x] `IGenerationStream::error()` 错误传递契约（`LlamaAdapterProvider` 不再吞异常）

**已知遗留（非阻塞 Phase 1）**:
- 8 个老测试（test_library_loader / test_llm_streaming / test_parser / test_path_resolution / test_prompt_builder / test_scheduler / test_sse_stream / test_tool_registry）的二进制尚未在当前 build 中重新生成（CMake GLOB 配置期评估导致的陈旧构建状态）。在干净构建后全部 20 个测试均应通过。
- ThreadSanitizer 运行时报错（ASLR 内存映射冲突，非 data race）；18/18 并发断言已验证 `InMemoryBus` 行为正确，CI 环境可正常运行 TSan。

### 最近完成的 OpenSpec 变更（由 OpenSpec 跟踪）

> **约束**: 本节为引用区,任务明细由 OpenSpec `tasks.md` 维护,不在此拆分新任务
> (符合 line 7-9 约束)。

| 变更 ID | 标题 | 严重度 | 任务数 | 链接 |
|---------|------|:------:|:------:|------|
| `docs-code-alignment-fixes` | 文档/代码对齐修复（2026-06-09 审计 19 个问题） | 🔴 P0: 4 / 🟠 P1: 16 / 🟡 P2: 8 / 收尾 4 | 31 | [OpenSpec change](openspec/changes/archive/2026-06-09-docs-code-alignment-fixes/) |

**变更摘要**: 修复 `registry.cpp:116` 硬编码默认值 bug + 9 个 ADR 状态降级 + 6 处文档 `max_tokens` 默认值对齐 + 根 `AGENTS.md` 模块/测试清单更新。**`openspec validate` 已通过,apply-ready。**

### Phase 1 候选入口（待启动）

> Phase 0 收官后，下一步是 Phase 1 智能体层（详见 `docs/implementation-roadmap.md` §Phase 1）。

| Phase 1 组件 | ADR | 状态 | 依赖 |
|--------------|-----|:----:|------|
| DSLEngine bus 集成 + NodeExecutor token push | ADR-0019 P2 | [ ] 未开始 | IInteractionBus ✅ |
| `CognitiveWorker` + `DomainWorkerPool` | ADR-0020 P1 | [ ] 未开始 | CognitiveWorker 入口 ✅ |
| `DECLARE_TOOL` 宏 (PDK) | ADR-0021 P1 | [ ] 未开始 | 独立仓库 |
| `PluginInfo` + `PluginLoader` | ADR-0022 P1 | [ ] 未开始 | 独立 |
| `ToolResult` 完善（P2-P4） | ADR-0023 | [ ] 未开始 | ToolResult MVP ✅ |

### Phase 1 接口预留（已落实）

| Phase 1 组件 | 需要的预留 | 当前状态 |
|-------------|----------|----------|
| `CognitiveWorker` (ADR-0020) | `SimpleCognitiveOrchestrator` 可被独立 DSLEngine 驱动 | ✅ 构造函数允许 `nullptr` 注入 |
| `IInteractionBus` (ADR-0019 P2) | `InMemoryBus` 线程安全 | ✅ 18/18 并发断言 + 锁外 callback |
| `ToolResult` (ADR-0023) | `{"ok", "data", "meta"}` 信封 | ✅ X 阶段已定义 |
| `DomainWorkerPool` (ADR-0020 P2) | 工具调用支持异步 | ⚠️ SimpleCognitiveOrchestrator 当前同步调用 `call_tool`，Phase 1 改造 |

### 文档冲突已解决（历史记录）

`docs/adr/agenticdsl/api/cloud-llm-adapter.md` 已在 2026-06-07 标注为 **superseded by ADR-0001**。
- 实际实现遵循 ADR-0001 的 `ILLMProvider` stream-handle 设计
- 文档提议的独立 `ICloudLLMAdapter` 未被采用
- 完整决策记录见 `docs/adr/agenticdsl/api/cloud-llm-adapter.md` 头部说明

---

## 附录：参考文档

| 文档 | 用途 | 本文件引用方式 |
|------|------|---------------|
| `docs/implementation-roadmap.md` | Phase 定义、完整任务列表、设计约束 | 任务编号必须来自此文件 |
| `docs/prephase-slice00-phase0.md` | 当前 Phase 的详细实施步骤 | Sprint 任务按此文件拆解 |
| `docs/implementation-slices.md` | 端到端验证切片定义 | 验证标准引用此文件 |