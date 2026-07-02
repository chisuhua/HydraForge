# Proposal: ADR-0034 — IModelRouter 模型路由 Plugin

> **STATUS: ACTIVE** 🟢 — Oracle Q1-Q3 决策落地, proposal/design/spec/tasks 全部填充完成
> **Oracle 审查**: 2026-07-02 (锁定决策: 3 独立 Plugin + 全策略 ship + Dual-Repo)
> **预估工时**: 2.5 人天 (Sprint 17 主体)
> **关联 ADR**: docs/adr/plugin/adr-0034-model-router.md (🔍 Proposed, plugin-candidate)
> **追溯范围**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C7
> **前置依赖**: 无硬依赖 (PDK Sprint 4 ship + PluginLoader Sprint 5 ship + ToolRegistry V2 Sprint 16 C6 ship)

## Why

ADR-0034 IModelRouter 当前状态 🔍 Proposed (plugin-candidate), 文档位于 `docs/adr/plugin/`. 设计目标: 通过 Plugin SDK 加载第三方模型路由策略, 引擎侧仅保留接口契约。

当前代码库状态:
- `examples/phase1_model_router_plugin/main.cpp` — Sprint 0 stub (硬编码 `ModelRouterPolicy::route`, 仅第一个 Chat-capable 模型)
- `src/common/llm/llm_types.h` — `ILLMProvider::ModelCapability` enum + `ModelInfo` struct + `available_models()` 默认空实现
- `include/agenticdsl/pdk/tool_macros.h` — DECLARE_TOOL 宏 (C6 升级后支持 4 参数)
- `include/agenticdsl/pdk/pdk.h` — PDK 统一入口 (v0.1.0)

存在以下 gap:
1. **无 IModelRouter 运行时抽象**: 模型路由决策无法通过 Plugin 扩展, 每新增路由策略需改引擎代码
2. **Sprint 0 stub 硬编码**: `ModelRouterPolicy::route()` 仅 `find_if(Chat)`, 无成本/质量/延迟感知
3. **ModelCapability 是 enum**: 当前 `ILLMProvider::ModelCapability` 是带 5 值的 enum (Chat/Completion/Embedding/ToolUse/Vision), 缺少 `per_token_cost`/`avg_latency_ms`/`tags` 等路由决策所需字段 — 需扩展为 struct 兼容形式
4. **ILLMProvider 默认空实现**: `available_models()` 返回 `{}` — 各 Provider 未 override

不解决此问题: (a) ADR-0034 长期 🔍 Proposed; (b) 模型路由无 Plugin 生态起点; (c) 成本/质量/延迟路由无标准实现; (d) 第三方 Plugin 开发者缺参考实现

## What Changes (基于 Oracle Q1-Q3 决策)

### 1. IModelRouter 接口 (PDK 插件头, 非 `src/common/llm/`)

**Oracle Q1 决策 — Option B: 三个独立 Plugin**, 每个路由策略 ship 为独立 `.so` Plugin。

PDK 接口头文件: `include/agenticdsl/pdk/model_router.h` (新建)

```cpp
namespace agenticdsl::pdk {

struct RoutingContext {
  std::string task_type;                    // "completion" / "code_generation" / "reasoning"
  std::string session_id;
  std::optional<int> max_tokens;
  std::optional<double> budget_remaining;   // 美元
  std::vector<std::string> required_tags;   // 能力标签 ("fast" / "code" / "reasoning")
  std::string preferred_model;              // 用户偏好 (可忽略)
  bool is_fleet_mode = false;
};

struct ModelCapability {
  std::string model_id;
  std::string model_name;
  int n_ctx = 4096;
  int max_tokens = 4096;
  bool supports_streaming = true;
  bool supports_function_call = false;
  double per_token_cost = 0.0;              // 美元
  int avg_latency_ms = 500;                 // 平均延迟
  std::vector<std::string> tags;            // ["fast", "vision", "code"]
};

class IModelRouter {
public:
  virtual ~IModelRouter() = default;
  virtual std::string route(const RoutingContext& ctx,
                            const std::vector<ModelCapability>& candidates) = 0;
  virtual std::string name() const = 0;
};

// 路由异常
class ModelRoutingError : public std::runtime_error {
public:
  enum class Code { NoViableModel, ProviderUnavailable, AmbiguousCapability };
  Code code;
  ModelRoutingError(Code c, const std::string& msg);
};

} // namespace agenticdsl::pdk
```

**关键设计点**:
- PDK 命名空间独立于 `agenticdsl::ILLMProvider::ModelCapability`(enum), 避免类型冲突
- `ModelCapability` 是 `pdk::` struct 而非 enum — 含 `per_token_cost`/`avg_latency_ms`/`tags` 等路由字段
- `IModelRouter::route()` 接收 `candidates` 参数 (不持有 registry), 保持纯函数可测试
- `ModelRoutingError` 3 种错误码匹配 `ToolResult::ErrorCode` 模式

### 2. 三个独立 Plugin `.so` 文件

**Oracle Q1 决策 — Option B: 每个策略独立 Plugin**

Plugin 入口: `pdk_register_tools(IToolRegistry&)` (遵循 Sprint 4 PDK 已有模式, 非 Factory)

| Plugin | .so 名 | 注册工具名 | 路由策略 |
|--------|--------|-----------|---------|
| **成本路由** | `hydraforge_model_router_cost` | `model_router/cost` | 最低 `per_token_cost` + capability tag 匹配 |
| **质量路由** | `hydraforge_model_router_quality` | `model_router/quality` | 最佳 `tags` 匹配 (reasoning/code 等) |
| **延迟路由** | `hydraforge_model_router_latency` | `model_router/latency` | 最低 `avg_latency_ms` |

**Plugin 结构** (以 cost 为例):

```
pdk/model_router/cost_strategy/
├── CMakeLists.txt          # SHARED library, 链接 hydraforge_pdk
├── cost_router.cpp         # pdk_register_tools 入口
└── cost_router.h           # CostModelRouterPolicy 类
```

`cost_router.cpp` 入口:
```cpp
extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
  auto router = std::make_shared<CostModelRouterPolicy>();
  registry.register_tool_function(
    "model_router/cost",
    make_router_metadata("cost", "cost-based model routing"),
    [router](const auto& args) -> nlohmann::json {
      // 从 context layer 获取 candidates, 执行 route()
      auto ctx = parse_routing_context(args);
      auto candidates = get_candidates_from_context(args);
      return {{"model_id", router->route(ctx, candidates)}, {"router", router->name()}};
    }
  );
}
```

**分层工具名**: `model_router/cost` / `model_router/quality` / `model_router/latency` (避免名称冲突)

**状态模型** (Oracle Q1 决策): 策略本身 stateless — 路由决策 = 纯函数 `(RoutingContext, List<ModelCapability>) → model_id`。模型注册表缓存在引擎层 (DSLEngine), 不存 Plugin 内。Plugin reload 不丢失缓存数据。

### 3. ModelRegistry 工具 (DECLARE_TOOL `model_router/registry`)

一个独立的 DECLARE_TOOL 提供模型查询能力 (Plugin 内 or 独立工具):

```cpp
DECLARE_TOOL(model_router/registry,
  "查询可用模型列表, 可按 capability tag 筛选",
  ReadOnly, "agent",
  // 从 context layer 获取 available_models() 列表, 返回匹配结果
  auto models = get_models_from_provider();
  auto filtered = filter_by_tag(models, __pdk_args.value("tag", ""));
  return nlohmann::json(filtered);
)
```

**Oracle Q2 决策**: 3 种路由策略全部 ship — orthogonal 维度:
- **cost**: `examples/agent_basic/workflow.agent.md` (max_llm_calls: 1) + `tests/test_cost_collector.cpp` (per-token tracking)
- **latency**: `examples/agent_loop/` 的 tight ReAct loop + `GenerationRequest::stop_token` timeout
- **quality**: ADR-0034 `ModelCapability.tags` 字段 (capability-aware routing)

### 4. Dual-Repo 同步

**Oracle Q3 决策 — PDK Sample (Vendored + Standalone)**

PDK 插件目录 vendored in monorepo at `pdk/model_router/<strategy>/`, standalone 通过 `scripts/sync-pdk.sh` 同步到 `github.com/chisuhua/hydraforge-pdk`。

**Sync 脚本需零改动**: sync-pdk.sh 已按 `pdk/` 目录全量同步, `pdk/model_router/` 子目录自动纳入。触发时机: Sprint 17 ship 时执行一次。

### 5. 测试覆盖

**Oracle Q4 决策 — MockLLMProvider + canned ModelCapability**

所有路由逻辑为纯决策函数, 测试注入 fake ILLMProvider 返回静态 `vector<ModelCapability>`:
- `tests/test_model_router_plugins.cpp` — 3 策略各 4-5 scenario (cost / quality / latency / registry)
- MockLLMProvider 新增 `set_available_models(vector<ModelCapability>)` 测试 hook
- 零真实 LLM 调用

## Capabilities

### ADDED Requirements (详细场景见 spec.md)

| Requirement ID | 描述 |
|----------------|------|
| `model-router-interface` | IModelRouter 接口契约: `route(RoutingContext, vector<ModelCapability>) → string` + `name()` |
| `model-router-plugin-entry` | `pdk_register_tools` 契约: 每个 Plugin 精确注册 1 个 `model_router/<strategy>` 工具, 返回 router handle |
| `cost-strategy-end-to-end` | 成本路由: 返回 `per_token_cost` 最低的 capability-matching 模型, 预算超标时 skip |
| `quality-strategy-end-to-end` | 质量路由: 按 `tags` 匹配度排序, 无匹配 tag 时 fallback default |
| `latency-strategy-end-to-end` | 延迟路由: 返回 `avg_latency_ms` 最低的模型, 全模型超 latency budget 时 throw `NoViableModel` |
| `model-registry-tool` | `model_router/registry` 工具: 返回 Provider 可用模型列表, 支持 tag 筛选 |

## Impact

### 文件修改/创建清单

| 文件 | 操作 | 说明 |
|------|------|------|
| `include/agenticdsl/pdk/model_router.h` | 新建 | IModelRouter + RoutingContext + ModelCapability + ModelRoutingError |
| `pdk/model_router/cost_strategy/cost_router.h` | 新建 | CostModelRouterPolicy 声明 |
| `pdk/model_router/cost_strategy/cost_router.cpp` | 新建 | pdk_register_tools + route 实现 |
| `pdk/model_router/cost_strategy/CMakeLists.txt` | 新建 | SHARED library target |
| `pdk/model_router/quality_strategy/quality_router.h` | 新建 | QualityModelRouterPolicy 声明 |
| `pdk/model_router/quality_strategy/quality_router.cpp` | 新建 | pdk_register_tools + route 实现 |
| `pdk/model_router/quality_strategy/CMakeLists.txt` | 新建 | SHARED library target |
| `pdk/model_router/latency_strategy/latency_router.h` | 新建 | LatencyModelRouterPolicy 声明 |
| `pdk/model_router/latency_strategy/latency_router.cpp` | 新建 | pdk_register_tools + route 实现 |
| `pdk/model_router/latency_strategy/CMakeLists.txt` | 新建 | SHARED library target |
| `pdk/model_router/model_registry.cpp` | 新建 | DECLARE_TOOL model_router/registry |
| `pdk/model_router/model_registry_cmake.txt` | 新建 | CMake for registry tool |
| `pdk/CMakeLists.txt` | 修改 | +4 add_subdirectory(model_router/...) 行 |
| `src/common/llm/mock_provider.h` | 修改 | 新增 `set_available_models()` 测试 hook |
| `examples/phase1_model_router_plugin/main.cpp` | 修改 | 升级: 演示 3 Plugin 加载 + routing |
| `examples/phase1_model_router_plugin/CMakeLists.txt` | 修改 | 链接新的 plugin .so targets |
| `tests/test_model_router_plugins.cpp` | 新建 | 3 策略 × 场景测试 |
| `include/agenticdsl/pdk/pdk.h` | 修改 | +1 行: `#include <agenticdsl/pdk/model_router.h>` |
| `AGENTS.md` | 修改 | § Recent Changes 追加 C7 ship |

### API 兼容性

**非 breaking** — C7 完全增量:
- `ILLMProvider::available_models()` 默认空实现不变 (现有 Provider 不受影响)
- `ILLMProvider::ModelCapability`(enum) 保持不变 — PDK 的 `pdk::ModelCapability`(struct) 是独立类型
- `ILLMProvider::ModelInfo` struct 保持不变 — routing 通过 `available_models()` 收集数据后转换为 `pdk::ModelCapability`
- MockLLMProvider 新增 `set_available_models()` 测试 hook — 不影响生产代码
- 无需修改 `engine.h` / `engine.cpp` / `NodeExecutor` / `TopoScheduler` / `ExecutionSession`

**示例迁移**:
`examples/phase1_model_router_plugin/main.cpp` 从 Sprint 0 stub 升级为 Plugin 加载演示

## Non-goals

- **不实现** Plugin 自身动态加载 (复用 Sprint 5 PluginLoader, 无新加载机制)
- **不实现** IModelRouter hot-reload (Plugin reload 不支持, 策略变更需重新加载 .so)
- **不实现** Multi-router 仲裁 (多个 Router 并行决策 → 单一决策者模型, 超出 C7 范围)
- **不实现** Fleet 模式批量路由 (16 路并行依赖 C2 ADR-0030 V2, 留 Phase 3+)
- **不修改** engine.h/engine.cpp (C7 是纯 PDK Plugin 工作, 引擎零变更)
- **不修改** NodeExecutor / execute_dsl_node() (路由决策在 Plugin 侧, 引擎调用 `call_tool("model_router/cost", ...)`)
- **不修改** ILLMProvider 接口 (不新增 virtual method, `available_models()` 已存在)
- **不处理** Plugin .so 预编译与 CI 集成 (.so 构建留后续 CI 任务, C7 仅 CMake + 手动验证)

## Estimated Effort

| Phase | 内容 | 人天 |
|-------|------|:----:|
| Day 1-2 | IModelRouter 接口 + RoutingContext + ModelCapability + ModelRoutingError (pdk/model_router.h) | 0.5 |
| Day 3-5 | 3 个 Plugin .so (cost/quality/latency) + CMake | 1.0 |
| Day 6-7 | MockLLMProvider 扩展 + 示例升级 (phase1_model_router_plugin) | 0.5 |
| Day 8-10 | 测试 (test_model_router_plugins.cpp + ASan/TSan) | 0.25 |
| Day 11 | Ship gate (ctest + openspec validate + ADR 状态 + sync-pdk.sh) | 0.25 |
| **总计** | | **2.5 人天** |

## 详细制定 TODO (C7 fill-in 任务)

- [x] 1. 业务确认: 3 种路由策略全部 ship (Oracle Q2 决策, 2026-07-02)
- [x] 2. 决策: 3 个独立 Plugin (.so), 分层工具名 `model_router/<strategy>` (Oracle Q1 决策 — Option B)
- [x] 3. 评估: Dual-Repo 同步机制 — sync-pdk.sh 零改动 (Oracle Q3 决策, 2026-07-02)
- [x] 4. 写本 change proposal.md (What Changes 完整 detail, 基于 Oracle Q1-Q3)
- [x] 5. 写 design.md (5 个 Decision: 接口位置 / Plugin 入口 / 状态模型 / 错误模型 / 测试策略)
- [x] 6. 写 tasks.md (40-50 tasks, 12 sections)
- [x] 7. 写 specs/model-router-plugin/spec.md (6 ADDED Requirements, 各 2-3 Scenario)
- [ ] 8. `openspec validate 2026-06-26-adr-0034-model-router-plugin` exit 0
- [ ] 9. 更新 master plan C7 行: ⚪ 占位 → 🟡 active