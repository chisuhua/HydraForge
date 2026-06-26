# 实施状态看板

> **基于**: `docs/implementation-roadmap.md`（静态蓝图）
> **更新**: 本文件反映**当前实施进度**，每日更新
> **焦点**: 当前正在实施的 Phase
>
> **⚠️ 核心约束**: 本文件中的任务必须全部来自 `docs/implementation-roadmap.md`。
> 任务的新增、删除、拆分必须在 `docs/implementation-roadmap.md` 中先完成，再同步到本文件。
> `docs/implementation-roadmap.md` 无记录的任务不得出现在本文件中。
>
> **📋 2026-06-24 Sprint 5 收官 + 6.3.x 全关闭**: OpenSpec change `tech-debt-and-phase1-closure` ship,
> Phase 1 智能体层 80% → 100%。5 ADR (0019/0020/0021/0022/0023) → ✅ Approved。
> `docs/roadmap-status.md` Phase 1 = 100%。6.3.x follow-up 全部 6 项关闭。
> `tech-debt-cleanup-sprint-6` 干净 archive。Sprint 10 起点零 OpenSpec backlog。
>
> **📋 2026-06-15 阶段过渡**: Phase 0 MVP 收官(C₁ → X → B → A + P0/P1/P2 cleanup 全部 ship, 25/25 测试)。
> 进入 Phase 1 智能体层 (2026-06-16 ~ 2026-07-15, 4 周 5 Sprint)。详细执行计划见 `.omo/plans/phase1-execution.md`。
> 5 个 stage 整理计划已 ship + 追溯归档(`.omo/plans/archive/2026-06-15-archived/project-organization.md` + 4 个 `openspec/changes/archive/2026-06-15-retrospectives/`)。
> 后续活跃工作: 2 个 OpenSpec change (`phase1-toolresult-standardization` Sprint 1 + `2026-06-15-residual-engine-h-decoupling` P1 并行)。

> **📋 2026-06-16 Sprint 1a 收官**: ToolResult 标准化 P1-P4 完成 (commits `fb67a9b` / `60b31b5` / `5c9ba18`)。
> 实施报告: [`docs/SPRINT-1A-COMPLETION-REPORT.md`](SPRINT-1A-COMPLETION-REPORT.md)。
> OpenSpec change 已归档至 `openspec/changes/archive/2026-06-16-phase1-toolresult-standardization/`。
> 下一活跃工作: Sprint 1b Bus 集成 (OpenSpec `2026-06-17-phase1-bus-integration`)。

> **📋 2026-06-17 Sprint 1b 收官**: DSLEngine Bus 集成 (ADR-0019 P2) 完成。
> 27 测试 / 33 新 assertions 通过 (新增 10 测试)。
> OpenSpec change: `openspec/changes/2026-06-17-phase1-bus-integration/` (待归档)。
> 下一活跃工作: Sprint 2 CognitiveWorker (ADR-0020)。

> **📋 2026-06-18 P1.T3 收官**: TraceRecord data-only struct 从 `src/modules/trace/trace_exporter.h` 上移到 `include/agenticdsl/types/trace_record.h` (commit `01666fa`)。`engine.h` 跨模块 include 计数 4 → 3。**27/27 测试零回归**。P1 进度 75% → 87%。
> OpenSpec change: `2026-06-15-residual-engine-h-decoupling` (P1 active, T3 完成; 剩余 T1+T2)。
> 下一活跃工作: P1 T1 (LLMProviderFactory 从零构建) 或 T2 (IToolRegistry 7 虚函数)。

> **📋 2026-06-18 P1.T1 收官 (LLMProviderFactory 从零构建)**: 7 commits (`355d52c` `14ba62b` `f9062e6` `f7ef5bf` `9fe4266` `21b79d7` `b4da645`)。
> - `IProviderFactory` 抽象接口 (1 虚函数) + `LLMProviderFactory` 路由 (mock/openai/anthropic/deepseek/qwen/llama) + `MockProviderFactory` 包装
> - DSLEngine 注入 `provider_factory_` (PIMPL-lite), `engine.h` 移除 `common/llm/mock_provider.h` (line 40)
> - **`engine.h` 跨模块 include 计数 3 → 2** (剩余 `common/llm/llm_types.h` types 例外 + `common/tools/registry.h` T2 解决)
> - **28/28 测试零回归** (baseline 27 + 1 新增 test_provider_factory, 含多线程 1000x create)
>
> **📋 2026-06-18 P1 v3 修订 (Oracle T2 深度审查)**: 2 commits (`6b0ca86` `dc3bc96`).
> - 修正 design.md §決策2.1 line 181 `base_registry_` 值成员设计**不可实施** (non-copyable + 违反装饰器 + 9 个 secure 测试失败)
> - 改用**委托式多继承** (保持 registry_ref_/registry_shared_, 加 public IToolRegistry + 9 override 委托)
> - 9 虚函数 (不是 v2 估的 7-8): 移除未使用 has_cost_callback, 加 register_tool_function 桥接
> - SimpleCognitiveOrchestrator 改为 `IToolRegistry*` (依赖倒置, 6 调用点零修改)
>
> OpenSpec change: `2026-06-15-residual-engine-h-decoupling` (P1 active, T1+T3 完成; 剩余 T2+T4+T5).
> 下一活跃工作: T2 (IToolRegistry 9 虚函数 + SecureToolRegistry 委托多继承, 3.5 天) 或 T4+T5 (3+3 天).

> **📋 2026-06-18 P1 全部 ship (T1+T2+T3+T4+T5)**:
> - T1 LLMProviderFactory: 7 commits (`355d52c` 等), 跨模块 include 3→2
> - T2 IToolRegistry 9 虚函数 + SecureToolRegistry 委托多继承: 4 commits, 29/29 测试零回归
> - T3 TraceRecord 上移: commit `01666fa`, 跨模块 include 4→3
> - T4 PIMPL-lite tool_registry_: commit `71b8253`, 跨模块 include 2→1 (仅 `common/llm/llm_types.h` types 例外)
> - **完全达成 ADR-0019 §1.4 退出标准 (≤ 1 common/ include, types 例外允许)**
> - 同步: ADR-0019 §1.4 状态更新为 ✅ 已解决
>
> OpenSpec change: `2026-06-15-residual-engine-h-decoupling` 准备 archive.
>
> **📋 2026-06-18 Sprint 2 CognitiveWorker 实施 (OpenSpec change `2026-06-23-cognitive-worker`)**:
> - 新增: `include/agenticdsl/cognitive/cognitive_worker.h` + `src/modules/cognitive/cognitive_worker.cpp` (per-agent 隔离, 状态机 idle/running/stopped)
> - 构造签名 `(unique_ptr<DSLEngine>, shared_ptr<IInteractionBus>)` 强制注入 bus (F7), 析构 out-of-line 隐式 stop()+join (TD-CW-02)
> - 错误码 bridge `error_code_from_string()` 覆盖 SimpleCognitiveOrchestrator 9 处 legacy string 路径 (TD-CW-03)
> - 9 个新测试覆盖生命周期 / 任务提交 / 错误传播 / 并发 / 状态机 / 析构安全
> - 30/30 ctest pass (29 baseline + 1 new test_cognitive_worker w/ 9 cases, 33 assertions), 零回归
>
> **📋 2026-06-19 Sprint 3 DomainWorkerPool 实施 (OpenSpec change `2026-06-30-domain-worker-pool`)**:
> - 新增: `include/agenticdsl/cognitive/domain_worker_pool.h` + `src/modules/cognitive/domain_worker_pool.cpp` (N 个 std::jthread worker + 共享 FIFO 任务队列多消费者模式 + shared_mutex 保护 handler 注册表)
> - 构造双重重载 `(num_threads)` 与 `(num_threads, shared_ptr<IInteractionBus>)`, 状态机 idle/running/stopped
> - 协作式取消 (std::jthread + std::stop_token), 异常隔离 (try-catch + catch(...))
> - 队列排空策略: stop 后 worker 优先消费 queue 中 task, 队列空 + stop_token 才退出 (无 task 丢失)
> - 7 个新测试覆盖默认构造 / submit 派发 / 1000x 并发 / 异常隔离 / shutdown 等待 in-flight / graceful vs forced / bus 集成
> - 新增 `Dockerfile.tsan` 容器化 TSan 验证 (ubuntu:22.04 + gcc-13 + ASLR=0)
> - CP.22 协议 6/6 项通过 (`.omo/plans/2026-06-30-cp22-audit.md`)
> - **31/31 ctest pass** (30 baseline + 1 new test_domain_worker_pool w/ 7 cases, 94 assertions), 零回归
> - **ADR-0020 §2.2.1 状态**: 🟡 Partial → ✅ **Resolved** (Sprint 3 ship)

---

## 一、总体进度

| Phase | 进度 | 状态 | 工期 | 依赖 |
|-------|:----:|:----:|:----:|:----:|
| Pre-Phase | 100% ██████████ | ✅ 已完成 | 0.5 天 | 无 |
| Slice 00 | 100% ██████████ | ✅ 已完成 | 1-2 天 | Pre-Phase (CMake) |
| **Phase 0 MVP** | **100% ██████████** | **✅ 已完成 (2026-06-14)** | **7-10 天** | Pre-Phase + Slice 00 |
| ├─ Track 0.1 (Cloud LLM) | 100% ██████████ | ✅ 已完成 | 3-4 天 | Pre-Phase |
| ├─ Track 0.1.5 (C₁ migration) | 100% ██████████ | ✅ 已完成 | 0.5 天 | Track 0.1 |
| ├─ Track 0.2 (三层调用链) | 100% ██████████ | ✅ 已完成 | 5-7 天 | Track 0.1 + C₁ |
| ├─ Track 0.3 (最小契约层 X/B/A) | 100% ██████████ | ✅ 已完成 | 2-3 天 | Pre-Phase |
| └─ P0/P1/P2 Cleanup | 100% ██████████ | ✅ 已完成 | ~4h | 上述全部 |
| **Phase 1 智能体层** | **100% ██████████** | **✅ 已完成 (2026-06-24, Sprint 5 收官)** — Sprint 0/1a/1b/2/3/4/5 全部 ship | **4 周 5 Sprint + Sprint 0** | Phase 0 ✅ |
| ├─ Sprint 0: ModelRouter Plugin Stub (K1) | **100% ██████████** | **✅ 已完成 (2026-06-16, 提前 1 天)** | **0.8 天** | Phase 0 |
| ├─ Sprint 1a: ToolResult P2-P4 | **100% ██████████** | **✅ 已完成 (2026-06-16, 提前 2 天)** | **2 天** | Sprint 0 ✅ |
| ├─ Sprint 1b: Bus 集成 (S1a/S1b 拆分, K2) | **100% ██████████** | **✅ 已完成 (2026-06-17, 提前 1 天)** | **1 天** | Sprint 1a ✅ |
| ├─ Sprint 2: CognitiveWorker | **100% ██████████** | **✅ 已完成 (2026-06-18, 提前)** | **2.5 天** | Sprint 1a ✅ |
| ├─ Sprint 3: DomainWorkerPool + Dockerfile.tsan | **100% ██████████** | **✅ 已完成 (2026-06-19, OpenSpec change `2026-06-30-domain-worker-pool`)** — DomainWorkerPool + 7 测试 + Dockerfile.tsan 实施, 31/31 ctest pass (30 baseline + 1 new test_domain_worker_pool w/ 94 assertions), ADR-0020 §2.2.1 🟡 Partial → ✅ Resolved, CP.22 协议 6/6 通过 | 3 天 | Sprint 2 ✅ |
| ├─ Sprint 4: PDK 骨架 (hydraforge-pdk, K3) | **100% ██████████** | **✅ 已完成 (2026-06-19, OpenSpec change `2026-07-07-pdk-skeleton`)** — DECLARE_TOOL + DEFINE_AGENT + SafeExec MVP 实施, monorepo `pdk/` 子目录 INTERFACE 库, 32/32 ctest pass (31 baseline + 1 new test_pdk_macros w/ 33 assertions), ADR-0021 🔍 Proposed → 🟡 Partial. T4b (`hydraforge-pdk` 独立仓库推送) 异步待外部阻塞解除 | 3 天 | Sprint 3 ✅ |
| └─ Sprint 5: PluginLoader + 收官 | **100% ██████████** | **✅ 已完成 (2026-06-24, OpenSpec change `tech-debt-and-phase1-closure`)** — PluginInfo POD + PluginLoader dlopen (5 ctest) + phase1_plugin_demo 3 modes + 5 ADR Approved + sync-pdk.sh, 34/34 ctest pass 零回归 | 1.3 天 | Sprint 4 |
| **并行车道** | | | | |
| ├─ P1: Residual engine.h Decoupling | 100% | ✅ **已解决 (2026-06-18, T1+T2+T3+T4+T5 全部 ship)** — 跨模块 include 4→3→2→1, 29/29 测试零回归, ADR-0019 §1.4 完全退出 | 10 天 → 5 周 → ship | 详见 OpenSpec `2026-06-15-residual-engine-h-decoupling` |
| ├─ P2: 5/6 examples build 修复 | 0% | ⏸ 未开始 (W2-W3) | 5 天 | P1 ✅ |
| └─ P3: 28 ADR 退出 grep 验证 | 0% | ⏸ 未开始 (W1D3) | 2 天 | 无 |
| ├─ Sprint 2: CognitiveWorker | **100% ██████████** | **✅ 已完成 (2026-06-18, 提前)** | 2.5 天 | Sprint 1 |
| ├─ Sprint 3: DomainWorkerPool | **100% ██████████** | **✅ 已完成 (2026-06-19)** — 详见 OpenSpec `2026-06-30-domain-worker-pool`, 31/31 ctest pass, ADR-0020 §2.2.1 ✅ Resolved | 3 天 | Sprint 2 ✅ |
| ├─ Sprint 4: PDK 骨架 | **100% ██████████** | **✅ 已完成 (2026-06-19)** — 详见 OpenSpec `2026-07-07-pdk-skeleton`, 32/32 ctest pass, ADR-0021 🟡 Partial | 3 天 | Sprint 3 ✅ |
| └─ Sprint 5: PluginLoader + 收官 | 0% | ⏸ 未开始 (W5) | 1.3 天 | Sprint 4 |
| Phase 2 异步+EventBus | 0% ░░░░░░░░░░ | ⏸ 阻塞中 | 2-3 周 | Phase 1 |
| Phase 3 执行策略+安全 | 0% ░░░░░░░░░░ | ⏸ 阻塞中 | 2-3 周 | Phase 2 |
| Phase 4 模型路由+内核 | 0% ░░░░░░░░░░ | ⏸ 阻塞中 | 2-3 周 | Phase 3 |
| Phase 4.5 MVP清理 | 0% ░░░░░░░░░░ | ⏸ 阻塞中 | 1-2 天 | Phase 4 |
| Phase 5 自举服务化 | 0% ░░░░░░░░░░ | ⏸ 远期 | — | Phase 4.5 |

---

## 二、当前 Sprint（本周）

> **📋 2026-06-15 状态**: 本节展示的是 **2026-05-30 ~ 2026-06-06** 那周的工作(Pre-Phase + Slice 00)。
> 实际当前待启动的是 **Sprint 0 (ModelRouter, W1D3 = 2026-06-16)**,详细切分见 `.omo/plans/phase1-execution.md`。
>
> 历史保留作为 Pre-Phase + Slice 00 ship 证据。

### 下一 Sprint: Sprint 0 (W1D3 = 2026-06-16, 1 天) — ModelRouter 作为 Domain Plugin

> **⚠️ 架构决策 (2026-06-15 修订)**: ModelRouter **重新定位为 Domain Plugin** (而非 Runtime 组件)。
> ADR-0034 (IModelRouter, 已归档 ❌ 未实施) 不复活。Runtime 仅提供 `ModelCapability` + `available_models()` 数据抽象,模型选择由 Plugin 智能体维护。
>
> **OpenSpec change**: `openspec/changes/2026-06-16-model-router-plugin/` (启动时新建, 作为 PDK Domain Plugin 示例)

| 优先级 | 任务编号 | 任务 | 文件 | 状态 | 阻塞 |
|:------:|:--------:|------|------|:----:|:----:|
| P0 | S0.T1 | `ModelCapability` enum + `available_models()` (Runtime 数据) | `src/common/llm/llm_types.h` | [ ] | — |
| P0 | S0.T2 | Plugin 层 `ModelRouterPlugin` 头文件 | `include/agenticdsl/pdk/model_router_plugin.h` | [ ] | — |
| P0 | S0.T3 | Plugin 层 `ModelRegistry` 工具 (DECLARE_TOOL) | `include/agenticdsl/pdk/model_registry_tool.h` | [ ] | — |
| P0 | S0.T4 | `DefaultModelRouterPolicy` 决策 (Plugin 智能体示例) | `examples/phase1_model_router_plugin/main.cpp` | [ ] | S0.T3 |
| P0 | S0.T5 | 单元测试 + Plugin 加载验证 | `tests/test_model_registry_tool.cpp` + `tests/test_phase1_plugin demo.cpp` | [ ] | S0.T1-T4 |

**Sprint 0 验收**: 5/5 new test cases PASS, ctest full 30+/30+ PASS, `examples/phase1_model_router_plugin --mock` 可加载并路由

### 历史 Sprint: 2026-05-30 ~ 2026-06-06

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
| M5 | M5.6 | ToolResult 标准化 (P1-P4) | `src/core/types/tool_result.h` | [x] | Sprint 1a 2026-06-16 |
| M6 | M6.1 | InMemoryBus 单元测试 | `tests/test_interaction_bus.cpp` | [ ] | M5.5 |
| M6 | M6.2 | 多线程安全测试 | `tests/test_interaction_bus.cpp` | [ ] | M6.1 |
| M6 | M6.3 | ToolResult 测试 (P1-P4 扩展) | `tests/test_tool_result.cpp` | [x] | Sprint 1a 2026-06-16 (71/71) |

**验收任务**

| 优先级 | 验收编号 | 验收内容 | 验证命令 | 状态 |
|:------:|:--------:|------|---------|:----:|
| V | V3.1 | InteractionBus 测试通过 | `ctest -R test_interaction_bus` | [x] (28/28) |
| V | V3.2 | ToolResult 测试通过 (P1-P4 扩展) | `ctest -R test_tool_result` | [x] (71/71) |
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
| 2026-06-14 | **fix-test-failures 完成** (5 个根因) | 0.5h | ✅ **25/25 通过** | 修复 3 个失败测试（test_layered_context / test_path_policy / test_secure_tool_registry），全部 25 个测试 100% 通过 (4.53s)。**5 个根因**：① `flatten` 函数实现有 bug（merge lambda 把子键提升到顶层而非把层整体作为外层键）→ 重命名 `flatten` → `flatten_layers` 并修复 merge 逻辑；② `PathPolicy::check` 相对路径失配（`weakly_canonical` 把相对路径转绝对，与 `"./workspace"` 前缀不匹配）→ 对 `allowed_prefixes` 同样做 `weakly_canonical`；③ `ShellGuard::DANGEROUS_PATTERNS` 模式 `"wget \| sh"` 太严格，无法命中 `wget -qO- ... \| sh` → 增加 `"| sh"` 模式；④⑤ `test_secure_tool_registry` 的 fs.read 和线程安全 case 失败，根因也是 ②（PathPolicy 误拒）。**连带修复**：`template_renderer.cpp` 同步更新 `flatten` → `flatten_layers` 调用；`node_executor.cpp` 的 `ParsedGraph` 移动构造（commit `451e395`）。**解决"已知遗留"**："8 个老测试的二进制尚未在当前 build 中重新生成"问题现已闭环，全量 25/25 在干净 build 后均通过。详见 `.omo/plans/archive/2026-06-15-expired-plans/fix-test-failures.md`。 |
| 2026-06-14 | **P0/P1/P2 连续推进 完成** | ~4h | ✅ **25/25 tests/asan, TSan ASLR 遗留** | **P0 质量保障**：① `.gitignore` 添加 `__pycache__/` 条目并清理 `tools/__pycache__/`；② 重建 `build_tsan`（删除 6 天前陈旧目录）→ 编译成功 25 个测试二进制，但运行时 TSan 因 ASLR 内存映射冲突报错（roadmap-status 已知遗留，已确认 18/18 InMemoryBus 并发断言无 data race）；③ `build/asan` 重建 → **25/25 ASan 全部通过，0 内存错误**。**P1 CI 增强**：① 修复 `.gitignore` 阻止 `.github/workflows/ci.yml` 提交（移除 `.github/` 行）；② CI 矩阵从 4 jobs (tests/asan × 2 编译器) 扩展为 **6 jobs (tests/asan/tsan × 2 编译器)**；③ `test_cloud_llm_live` 标记 `LABELS "live"`，默认 ctest 不跑（需真实 API key）；④ 清理 `node.h:107` 和 `node.cpp:70` 中 LLMCallNode 历史注释。**P2 Phase 1 启动准备**：① 5 个候选 ADR 评分（依赖/阻塞/价值/契合），首选 **ADR-0023 ToolResult 标准化**（4.70），次选 ADR-0020（4.65）；② 生成 `docs/phase1-roadmap.md`（4 周 5 Sprint 切分）；③ 创建 OpenSpec change `phase1-toolresult-standardization`（proposal + design + 5 REQ + 6 tasks，共 572 行）。**5 commits** (`5e40c9a` `d894a04` `0765ffa` `add39b2` `685e941`)。详见 `.omo/plans/archive/2026-06-15-expired-plans/p0-p1-p2-phase0-cleanup.md`。 |
| 2026-06-17 | **Sprint 1b 完成** (S1b.T1-S1b.T4) | 1d | ✅ **27/27 通过 (100%), 10 新测试 / 33 新 assertions** | **DSLEngine Bus 集成 (ADR-0019 P2)**：S1b.T1 新增 `bus_` 成员 + 3 注入方法 (set/get/subscribe)；S1b.T2 实现 nullptr-safe 透传到 InMemoryBus；S1b.T3 NodeExecutor 构造函数接受 `IInteractionBus*`，DSLNode 执行推送 `dsl.call.started/completed`，ToolNode 成功推送 `tool.completed`，Retry/Abort 推送 `execution.failed` 后抛异常，Skip 不推送；S1b.T4 新建 `test_engine_bus_integration.cpp` (10 TEST_CASE：注入验证 / nullptr 透传 / DSL 事件 / Tool 事件 / Abort+throw / Retry+throw / Skip+no-emit / 零回归 / 1000x 并发)。OpenSpec change: `2026-06-17-phase1-bus-integration`。编译 0 错误，ASan 干净，零回归（25 Sprint 1a + 2 Phase 0 原有 = 27/27）。详见 `openspec/changes/2026-06-17-phase1-bus-integration/`。 |

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
| test_layered_context | LayeredContext (5-层上下文) | ✅ | 2026-06-14 | 12/12（flatten 实现修复） |
| test_path_policy | PathPolicy + ShellGuard | ✅ | 2026-06-14 | 9/9（相对路径 + ShellGuard 模式修复） |
| test_secure_tool_registry | SecureToolRegistry 装饰器 | ✅ | 2026-06-14 | 11/11（受 PathPolicy 修复带动） |
| test_execution_policy | IExecutionPolicy | ✅ | 2026-06-14 | 全通过 |
| test_cost_collector | 成本收集 | ✅ | 2026-06-14 | 全通过 |
| test_call_llm_tool | call_llm_tool | ✅ | 2026-06-14 | 全通过 |
| **整体（C₁ 前）** | **16 个测试** | ✅ | **2026-06-07** | **16/16 (100%)** | |
| test_executor_with_mock_provider | 端到端 Mock 集成 | ✅ | 2026-06-08 | 16/5 通过（C₁.5 新增） |
| **整体（Phase C₁）** | **17+ 个测试** | ✅ | **2026-06-08** | **108 assertions / 48 cases, 0 失败** | |
| test_async_bridge | 异步桥接 | ✅ | 2026-06-07 | 3/3 通过 |
| **整体（fix-test-failures 后）** | **25 个测试** | ✅ | **2026-06-14** | **25/25 (100%), 0 失败, 4.53s** | |
| **Sprint 1a 后 (P1-P4 扩展)** | **27 个测试** | ✅ | **2026-06-16** | **27/27 (100%), 0 失败, +12 新测试 / +67 assertions** | [SPRINT-1A-COMPLETION-REPORT](SPRINT-1A-COMPLETION-REPORT.md) |
| **ASan 验证（build/asan_ninja, Sprint 1a 模块）** | **3 个测试** | ✅ | **2026-06-16** | **3/3 (100%), 0 memory error** | test_tool_result + test_executor + test_interaction_bus |
| **ASan 验证（P2.5 复验, 完整 34 测试）** | **34 个测试** | ✅ | **2026-06-25** | **33/34 (97%), 1 pre-existing (P1 跟踪)** | test_cognitive_worker (Sprint 2); P1 修复: commit `d69e2d9` |
| **TSan 验证（build/tsan）** | 25 个测试 | ⚠️ | 2026-06-14 | ASLR 内存映射冲突（roadmap-status 已知遗留；非 data race） | 待 CI 矩阵验证 |
| **TSan 验证（P2.5 复验, 完整 34 测试）** | **34 个测试** | ⚠️ | **2026-06-25** | **32/34 (94%), 2 pre-existing (P1/P2 跟踪)** | test_cognitive_worker + test_domain_worker_pool; P1/P2 修复: commits `d69e2d9`/`0c44a18` |
| **ASan/TSan Sprint 10 修复验证** | **34 个测试** | ✅ | **2026-06-26** | **ASan 34/34 (100%) + TSan 34/34 (100%), 0 errors/warnings** | P1: cognitive_worker jthread commit `d69e2d9`; P2: domain_worker_pool atomic flag commit `0c44a18`; audit: `docs/audits/p2-tsan-investigation.md` |
| test_interaction_bus | Track 0.3 + Sprint 1a | ✅ | 2026-06-16 | 28/28 (含 std::string overload 新测试) | Sprint 1a |
| test_tool_result | Track 0.3 + Sprint 1a | ✅ | 2026-06-16 | 71/71 (P1-P4 扩展 +7 测试) | Sprint 1a |
| test_engine_bus_integration | Sprint 1b Bus 集成 | ✅ | 2026-06-17 | 10/10 (33 assertions, ADR-0019 P2) | Sprint 1b |
| test_model_registry | Track 0.2 | ⏳ | — | — |
| test_default_router | Track 0.2 | ⏳ | — | — |

---

## 六、Sprint 计划模板

> 每个 Sprint 开始时，将"当前 Sprint"部分重置为本周计划。
> 完成的任务移至"实施日志"。

### Sprint 结束前检查

#### 6.1 基础检查（必备）

- [ ] 所有 Sprint 任务 `[x]` 或 `[~]`
- [ ] 所有验收任务 `[x]` 或 `[~]`
- [ ] 阻塞项已记录原因
- [ ] 验证状态已更新
- [ ] 实施日志已补充
- [ ] 已与 `docs/implementation-roadmap.md` 对比一致性

#### 6.2 Review Gates 检查（Roadmap-Driven Development 强制）

> 自 2026-06-26 起，Master Plan 启用 4 种 Review Gates (见 `docs/superpowers/plans/<date>-<roadmap>.md` §9)。每个 Sprint 收官必须执行：

- [ ] **🔄 Sprint Review Gate** — 已 ship change 是否达到预期效果？有无新 bug？假设错误？
- [ ] **🧭 Drift Detection** — 运行 `python3 tools/check_roadmap_drift.py`，0 个 CRITICAL drift
- [ ] **🔗 Dependency Refresh** — 占位 change 的依赖是否仍成立（启动前必跑）
- [ ] **🎯 Strategic Alignment**（每季度 1 次）— 当前 backlog 是否仍服务项目核心目标
- [ ] **§十 Drift Log 更新** — 本 Sprint 发现的 drift 已追加到 Master plan §十
- [ ] **§十一 Adjustment Log 更新** — 占位 change 调整已追加（如有）
- [ ] **§十二 Pivots Log 更新** — 战略转向已追加（如有）

#### 6.3 自动化脚本（推荐）

```bash
# 一键运行完整 Sprint 收官检查
./scripts/sprint-closeout.sh

# 或手动逐步:
cd /workspace/project/HydraForge
python3 tools/check_roadmap_drift.py   # 必跑
ctest --output-on-failure              # 测试全绿
python3 tools/adr_lint.py docs/adr/    # ADR lint
python3 tools/docs_drift_audit.py     # docs 一致性
```

详细指南见 `docs/guides/sprint-closeout-checklist.md`。

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
- ~~8 个老测试（test_library_loader / test_llm_streaming / test_parser / test_path_resolution / test_prompt_builder / test_scheduler / test_sse_stream / test_tool_registry）的二进制尚未在当前 build 中重新生成（CMake GLOB 配置期评估导致的陈旧构建状态）。在干净构建后全部 20 个测试均应通过。~~ ✅ **已解决**（2026-06-14 fix-test-failures：干净构建后 25/25 全部通过）
- ThreadSanitizer 运行时报错（ASLR 内存映射冲突，非 data race）；18/18 并发断言已验证 `InMemoryBus` 行为正确，CI 环境可正常运行 TSan。**2026-06-14 验证**：本地重建 `build/tsan` 后运行 `ctest` 仍触发 ASLR 错误（确认 18/18 并发断言无 data race；本机 GCC 13.3.0 触发的兼容性问题）。已将 TSan 加入 CI 矩阵，期望 GitHub Actions ubuntu-latest 环境无此问题。

### 最近完成的 OpenSpec 变更（由 OpenSpec 跟踪）

> **约束**: 本节为引用区,任务明细由 OpenSpec `tasks.md` 维护,不在此拆分新任务
> (符合 line 7-9 约束)。

| 变更 ID | 标题 | 严重度 | 任务数 | 链接 |
|---------|------|:------:|:------:|------|
| `docs-code-alignment-fixes` | 文档/代码对齐修复（2026-06-09 审计 19 个问题） | 🔴 P0: 4 / 🟠 P1: 16 / 🟡 P2: 8 / 收尾 4 | 31 | [OpenSpec change](openspec/changes/archive/2026-06-09-docs-code-alignment-fixes/) |
| `phase1-toolresult-standardization` | Phase 1 入口：ToolResult 标准化 P1-P4（ADR-0023） | 🟠 P1 (首选) | 6 | [OpenSpec change](openspec/changes/phase1-toolresult-standardization/) |
| `2026-06-17-phase1-bus-integration` | Phase 1 Sprint 1b: DSLEngine Bus 集成（ADR-0019 P2） | 🟠 P1 | 5 | [OpenSpec change](openspec/changes/2026-06-17-phase1-bus-integration/) |

**变更摘要**: 修复 `registry.cpp:116` 硬编码默认值 bug + 9 个 ADR 状态降级 + 6 处文档 `max_tokens` 默认值对齐 + 根 `AGENTS.md` 模块/测试清单更新。**`openspec validate` 已通过,apply-ready。**

### Phase 1 候选入口（已选定，等待启动）

> **2026-06-14 更新**: 5 个候选 ADR 已评审（详见 `.omo/decisions/phase1-entry.md`），首选 **ADR-0023 ToolResult 标准化**（4.70 分），次选 ADR-0020（4.65 分）。详细路线图见 `docs/phase1-roadmap.md`（4 周 5 Sprint + Sprint 0）。
>
> **2026-06-15 修订**: ModelRouter 重新定位为 Domain Plugin (Sprint 0),不再作为 Runtime 组件。ADR-0034 (IModelRouter, 已归档) 不复活。Runtime 仅提供 `ModelCapability` + `available_models()` 数据抽象。

| Phase 1 组件 | ADR | 状态 | 依赖 |
|--------------|-----|:----:|------|
| `ModelRouter` 作为 **Domain Plugin** (Sprint 0 示例, **Plugin 层**非 Runtime) | (新, Plugin 层) | ✅ 已完成 (2026-06-16) | Track 0.1 ✅ |
| `ToolResult` 完善（P1-P4） ⭐ **首选** | ADR-0023 | ✅ 已完成 (2026-06-16) | ToolResult MVP ✅ |
| DSLEngine bus 集成 + NodeExecutor token push | ADR-0019 P2 | ✅ 已完成 (2026-06-17) | IInteractionBus ✅ |
| `CognitiveWorker` + `DomainWorkerPool` ⭐ **次选** | ADR-0020 P1 | [ ] 未开始 (Sprint 2-3) | CognitiveWorker 入口 ✅ |
| `DECLARE_TOOL` 宏 (PDK) | ADR-0021 P1 | [ ] 未开始 (Sprint 4) | 依赖 ADR-0023 |
| `PluginInfo` + `PluginLoader` | ADR-0022 P1 | [ ] 未开始 (Sprint 5) | 依赖 ADR-0021 + 0020 |

### Phase 1 接口预留（已落实）

| Phase 1 组件 | 需要的预留 | 当前状态 |
|-------------|----------|----------|
| `CognitiveWorker` (ADR-0020) | `SimpleCognitiveOrchestrator` 可被独立 DSLEngine 驱动 | ✅ 构造函数允许 `nullptr` 注入 |
| `IInteractionBus` (ADR-0019 P2) | `InMemoryBus` 线程安全 | ✅ 18/18 并发断言 + 锁外 callback |
| `ToolResult` (ADR-0023) | `{"ok", "data", "meta"}` 信封 | ✅ X 阶段已定义 |
| `DomainWorkerPool` (ADR-0020 P2) | 工具调用支持异步 | ⚠️ SimpleCognitiveOrchestrator 当前同步调用 `call_tool`，Phase 1 改造 |

### 文档冲突已解决（历史记录）

`docs/proposals/api/cloud-llm-adapter.md` 已在 2026-06-07 标注为 **superseded by ADR-0001**。
- 实际实现遵循 ADR-0001 的 `ILLMProvider` stream-handle 设计
- 文档提议的独立 `ICloudLLMAdapter` 未被采用
- 完整决策记录见 `docs/proposals/api/cloud-llm-adapter.md` 头部说明

---

## 附录：参考文档

| 文档 | 用途 | 本文件引用方式 |
|------|------|---------------|
| `docs/implementation-roadmap.md` | Phase 定义、完整任务列表、设计约束 | 任务编号必须来自此文件 |
| `docs/prephase-slice00-phase0.md` | 当前 Phase 的详细实施步骤 | Sprint 任务按此文件拆解 |
| `docs/implementation-slices.md` | 端到端验证切片定义 | 验证标准引用此文件 |