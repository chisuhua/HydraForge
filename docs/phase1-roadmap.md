# Phase 1 实施路线图：智能体层

> **基于**: `.omo/decisions/phase1-entry.md` (Phase 1 入口决策)
> **关联**: `docs/implementation-roadmap.md` §Phase 1
> **时间窗口**: 2026-06-16 ~ 2026-07-15 (4 周 + Sprint 0 1 天)
> **目标**: 把 Phase 0 引擎基础 + 契约层扩展为完整多智能体插件运行时

---

## 一、目标

### 1.1 核心目标

实现 **CognitiveWorker + DomainWorkerPool + PDK + PluginLoading** 四件套，使 HydraForge 支持：
- 单 DSLEngine 驱动多个并发 worker
- 工具/Agent 插件以 `.so` 形式动态加载
- 工具调用结果结构化、可追踪、可重试

### 1.2 解锁能力

| 能力 | 之前 | 之后 |
|------|------|------|
| 多智能体并发 | ❌ 单线程 | ✅ CognitiveWorker + DomainWorkerPool |
| 工具结果格式 | 🟡 启发式分支 | ✅ ToolResult 标准化 |
| 插件加载 | ❌ 无 | ✅ .so dlopen + 注册 |
| 第三方扩展 | ❌ 需修改源码 | ✅ PDK + 独立仓库 |

### 1.3 验收指标

- 25/25 Phase 0 测试零回归
- +30+ Phase 1 新增测试（多线程 + 插件）
- TSan 干净（解决 ASLR 已知遗留）
- 端到端 demo: `examples/phase1_plugin_demo --mock`

---

## 二、组件清单

> **⚠️ 架构决策 (2026-06-15 修订)**: ModelRouter **重新定位为 Domain Plugin 层** (而非 Runtime 层)。
> ADR-0034 (IModelRouter, 已归档) 不复活,Runtime 不内置路由,模型选择由 Plugin 智能体通过 `DECLARE_AGENT` + `ModelRouterPolicy` 维护。

| Phase | 组件 | ADR | 状态起点 | 状态终点 | 预计工作量 |
|-------|------|-----|---------|---------|-----------|
| **0** | ModelRouter 作为 **Domain Plugin** (Sprint 0 示例) | (新, Plugin 层) | ❌ 无 | ✅ Plugin 可加载 + 路由 | 2.2 天 |
| **1.0** | ToolResult 扩展 | ADR-0023 | 🟡 P1 | ✅ P1-P4 | 1-2 周 |
| **1.0+** | DSLEngine bus 集成 (4 子任务) | ADR-0019 P2 | 🟡 P1 | ✅ P2 bus 推送 | 1 周 (与 1.0 并行) |
| **1.1** | CognitiveWorker | ADR-0020 | ❌ 无 | ✅ 可注入 | 1 周 |
| **1.2** | DomainWorkerPool | ADR-0020 | ❌ 无 | ✅ N 并发 | 1 周 |
| **1.3** | PDK 骨架 | ADR-0021 | 🔍 Proposed | ✅ DECLARE_TOOL | 1 周 |
| **1.4** | PluginLoader | ADR-0022 | 🔍 Proposed | ✅ .so 加载 | 1 周 |

---

## 三、依赖图

```
Sprint 0 (ModelRouter Plugin) ←── 独立 (1 天, Plugin 层, 2026-06-16)
   ↓
Phase 1.0 (ToolResult) ←── 依赖 Sprint 0 (最高优先)
    │
    ├──→ Phase 1.1 (CognitiveWorker) ←── 依赖 1.0
    │       │
    │       └──→ Phase 1.2 (DomainWorkerPool) ←── 依赖 1.1
    │               │
    │               └──→ Phase 1.3 (PDK)
    │                       │
    │                       └──→ Phase 1.4 (PluginLoader)
    │
    ├──→ (并行) Bus 集成 (ADR-0019 P2) ←── 依赖 1.0 (ToolResult 完成)
    │
    └──→ (隐含) ModelRouterPlugin ←── 作为 Sprint 0 Plugin 示例, 是 Plugin 1.3/1.4 的前置
```

---

## 四、Sprint 切分

### Sprint 0 (2026-06-16, 1 天) — ModelRouter 作为 Domain Plugin (Sprint 0 示例)

> **⚠️ 架构决策 (2026-06-15 修订)**: ModelRouter **重新定位为 Domain Plugin** 而非 Runtime 组件。
> ADR-0034 (IModelRouter, 已归档) 不复活。Runtime 仅提供 `ILLMProvider` + `available_models()` 数据抽象,模型选择由 Plugin 智能体通过 `DECLARE_AGENT` 宏 + `ModelRouterPolicy` 实现。
>
> **OpenSpec change**: `openspec/changes/2026-06-16-model-router-plugin/` (新建,作为 PDK Domain Plugin 示例)
> **依赖**: 无
> **风险**: 低 (5 个子任务, 重新定位在 Plugin 层)

| 任务 | 优先级 | 文件 | 工作量 |
|------|--------|------|--------|
| S0.T1 `ModelCapability` enum + `available_models()` | P0 | `src/common/llm/llm_types.h` (修改) | 0.2d |
| S0.T2 Plugin 层 `ModelRouterPlugin` 头文件 | P0 | `include/agenticdsl/pdk/model_router_plugin.h` (新建) | 0.5d |
| S0.T3 Plugin 层 `ModelRegistry` 工具 (DECLARE_TOOL) | P0 | `include/agenticdsl/pdk/model_registry_tool.h` (新建) | 0.5d |
| S0.T4 `DefaultModelRouterPolicy` 决策 (Plugin 智能体示例) | P0 | `examples/phase1_model_router_plugin/main.cpp` (新建) | 0.5d |
| S0.T5 单元测试 + Plugin 加载验证 | P0 | `tests/test_model_registry_tool.cpp` + `tests/test_phase1_plugin_demo.cpp` (新建) | 0.5d |

**Sprint 验收**: 5/5 new test cases PASS, ctest full 30+/30+, `examples/phase1_model_router_plugin --mock` 可加载并路由

### Sprint 1 (2026-06-17 ~ 2026-06-22, 4 天) — ToolResult 标准化 + Bus 集成

> **依赖**: Sprint 0 完成 (ModelRouter 移交, 2026-06-16)

| 任务 | 优先级 | 文件 | 工作量 |
|------|--------|------|--------|
| T1.1 扩展 ToolResult enum | P0 | `src/core/types/tool_result.h` | 0.5d |
| T1.2 扩展 ToolResult 字段 | P0 | `src/core/types/tool_result.h` | 0.5d |
| T1.3 NodeExecutor 移除启发式分支 | P0 | `src/modules/executor/node_executor.cpp` | 1d |
| T1.4 端到端集成测试 | P0 | `tests/test_executor_with_mock_provider.cpp` | 1d |
| T1.5 IInteractionBus 推送结构化结果 | P1 | `src/common/contract/inmemory_bus.cpp` | 0.5d |
| T1.6 ADR-0019 P2.1: DSLEngine bus 注入 | P0 | `src/core/engine.h` | 0.5h |
| T1.7 ADR-0019 P2.2: DSLEngine 3 方法 | P0 | `src/core/engine.cpp` | 2h |
| T1.8 ADR-0019 P2.3: NodeExecutor bus 注入 | P0 | `src/modules/executor/node_executor.h` | 0.5h |
| T1.9 ADR-0019 P2.4: NodeExecutor 逐 token 推送 | P0 | `src/modules/executor/node_executor.cpp` | 2h |
| T1.10 端到端 Bus 集成测试 | P1 | `tests/test_engine_bus_integration.cpp` | 0.5d |
| T1.11 端到端 demo 骨架 (Sprint 5 扩展基础) | P0 | `examples/phase1_plugin_demo/main.cpp` + `CMakeLists.txt` | 0.5d |

**Sprint 验收**: 30+ 测试通过（含新增），TSan 干净，端到端 demo 骨架可 build

### Sprint 2 (2026-06-23 ~ 2026-06-29) — CognitiveWorker

| 任务 | 优先级 | 文件 | 工作量 |
|------|--------|------|--------|
| T2.1 CognitiveWorker 头文件 | P0 | `include/agenticdsl/cognitive/cognitive_worker.h` | 0.5d |
| T2.2 CognitiveWorker 实现 | P0 | `src/modules/cognitive/cognitive_worker.cpp` | 1d |
| T2.3 注入 DSLEngine 验证 | P0 | `tests/test_cognitive_worker.cpp` | 0.5d |
| T2.4 与 IInteractionBus 集成 | P0 | `src/common/contract/inmemory_bus.cpp` | 0.5d |

**Sprint 验收**: CognitiveWorker 单元测试 8/8 pass

### Sprint 3 (2026-06-30 ~ 2026-07-06) — DomainWorkerPool

| 任务 | 优先级 | 文件 | 工作量 |
|------|--------|------|--------|
| T3.1 DomainWorkerPool 头文件 | P0 | `include/agenticdsl/cognitive/domain_worker_pool.h` | 0.5d |
| T3.2 std::jthread 队列实现 | P0 | `src/modules/cognitive/domain_worker_pool.cpp` | 1d |
| T3.3 多线程集成测试 | P0 | `tests/test_domain_worker_pool.cpp` | 1d |
| T3.4 CP.22 协议审查 | P0 | (audit) | 0.5d |

**Sprint 验收**: 1000x 并发无 data race，TSan 干净

### Sprint 4 (2026-07-07 ~ 2026-07-13) — PDK 骨架

| 任务 | 优先级 | 文件 | 工作量 |
|------|--------|------|--------|
| T4.1 DECLARE_TOOL 宏 | P1 | `include/agenticdsl/pdk/tool_macros.h` | 0.5d |
| T4.2 DEFINE_AGENT 模板 | P1 | `include/agenticdsl/pdk/agent_macros.h` | 1d |
| T4.3 SafeExec 封装 | P1 | `include/agenticdsl/pdk/safe_exec.h` | 0.5d |
| T4.4 独立 PDK 仓库骨架 | P1 | (github.com/.../agenticdsl-pdk) | 0.5d |
| T4.5 PDK 单元测试 | P1 | `tests/test_pdk_macros.cpp` | 0.5d |

**Sprint 验收**: 独立 PDK 仓库可编译 + 单元测试通过

### Sprint 5 (2026-07-14 ~ 2026-07-15) — PluginLoader + 收官

| 任务 | 优先级 | 文件 | 工作量 |
|------|--------|------|--------|
| T5.1 PluginLoader 头文件 | P0 | `include/agenticdsl/plugin/plugin_loader.h` | 0.3d |
| T5.2 dlopen/dlsym 实现 | P0 | `src/modules/plugin/plugin_loader.cpp` | 0.5d |
| T5.3 端到端 demo | P0 | `examples/phase1_plugin_demo/main.cpp` | 0.3d |
| T5.4 Phase 1 收官验证 | P0 | (CI + TSan + ASan) | 0.2d |

**Sprint 验收**: 端到端 demo 可运行，全部 ADR 状态变更为 ✅

---

## 五、依赖与阻塞

### Phase 0 已交付（无依赖）

- ✅ `IInteractionBus` + `InMemoryBus` (18/18 并发断言通过)
- ✅ `SimpleCognitiveOrchestrator` (25/25 测试通过)
- ✅ `ToolResult` MVP (24/24 测试通过)
- ✅ `IParser` + `IScheduler` 抽象接口
- ✅ `MockLLMProvider` 默认行为

### 外部依赖

- **独立 PDK 仓库**: github.com/{org}/agenticdsl-pdk (新建)
- **Taskflow v3.9.0** (已包含)
- **async_simple** (已包含)

### 风险

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| TSan ASLR 内存冲突（已知遗留） | 中 | Sprint 3 验证失败 | 升级 GCC 或在 docker 中跑 |
| PDK 独立仓库治理 | 中 | Sprint 4 延迟 | 内部 monorepo 先跑通，再拆分 |
| 多智能体死锁 | 高 | Sprint 3 重做 | CP.22 协议强制 + TSan CI |

---

## 六、验证标准

### Sprint 结束前检查

- [ ] 所有 Sprint 任务 `[x]`
- [ ] 全部 25+ 测试通过（含新增）
- [ ] TSan 干净
- [ ] ASan 干净
- [ ] 实施日志更新到 `docs/roadmap-status.md`

### Phase 1 收官验收

- [ ] 50+ 测试通过（Phase 0 + Phase 1）
- [ ] CI 矩阵全绿
- [ ] 端到端 demo 可运行
- [ ] 5 个候选 ADR 状态 ✅
- [ ] Phase 2（异步+EventBus）前置条件就绪

---

## 七、参考文档

| 文档 | 用途 |
|------|------|
| `docs/implementation-roadmap.md` | 蓝图参考 |
| `docs/adr/adr-0020-thread-model-isolation.md` | CognitiveWorker + Pool 设计 |
| `docs/adr/adr-0023-tool-result-standard.md` | ToolResult 标准化 |
| `docs/adr/adr-0021-pdk-design.md` | PDK 设计 |
| `docs/adr/adr-0022-plugin-loading.md` | 插件加载设计 |
| `.omo/decisions/phase1-entry.md` | Phase 1 入口决策 |
| `docs/roadmap-status.md` | 实施状态追踪 |

---

*最后更新: 2026-06-14*
