# Design: ADR-0034 — IModelRouter 模型路由 Plugin

> **STATUS: ACTIVE** 🟢 — 5 Decisions + Oracle Q1-Q3 决策落地
> **Oracle 审查**: 2026-07-02 (锁定 Q1-Q3 决策)
> **前置依赖**: 无硬依赖 (PDK + PluginLoader + ToolRegistry V2 已 ship)

## Decision 1: 接口位置 — PDK Plugin 头 (`include/agenticdsl/pdk/model_router.h`), 不放在 `src/common/llm/`

### 问题

IModelRouter 接口应放在何处? ADR-0034 原始设计 (§决策 2) 将其定位在 `src/common/llm/model_router.h` (引擎内部基座层)。但 C7 定位为 PDK Plugin (非引擎内建), 需重新评估。

### 选项

- **A**: 引擎内部 `src/common/llm/model_router.h` — 引擎 base 层
- **B**: PDK 头 `include/agenticdsl/pdk/model_router.h` — Plugin 视角
- **C**: 双份: 引擎侧抽象 + Plugin 侧实现接口分离

### 决策

**选择 B** — PDK Plugin 头 `include/agenticdsl/pdk/model_router.h`。

理由:
1. Plugin 开发者视角: `#include <agenticdsl/pdk/model_router.h>` 应只拉 PDK 公共 API, 不依赖 `#include "common/llm/llm_types.h"` 等引擎内部头
2. 引擎最小化: 引擎不 import `IModelRouter` (C7 零 engine.h 变更), 路由决策通过 `call_tool("model_router/cost", ...)` 委托到 Plugin
3. 类型独立: PDK `ModelCapability`(struct) 与 `ILLMProvider::ModelCapability`(enum) 分离 — Plugin 开发者不接触 enum 定义
4. 与 ADR-0021 PDK 设计一致: `IEexecutionPolicy` / `IToolRegistry` 均在 `include/agenticdsl/` 下, 不放在 `src/common/`

### 实现

```cpp
// include/agenticdsl/pdk/model_router.h
namespace agenticdsl::pdk {

struct RoutingContext { /* ... */ };
struct ModelCapability { /* ... */ };   // PDK 侧完整 struct (含 cost/latency/tags)
class IModelRouter { /* ... */ };
class ModelRoutingError : public std::runtime_error { /* ... */ };

} // namespace agenticdsl::pdk
```

### 命名空间隔离

- `agenticdsl::pdk::ModelCapability` (struct with `per_token_cost`/`avg_latency_ms`/`tags`) — PDK 侧路由决策使用
- `agenticdsl::ILLMProvider::ModelCapability` (enum Chat/Completion/Embedding/ToolUse/Vision) — 引擎侧, 保持不变
- `agenticdsl::ILLMProvider::ModelInfo` (struct with `name`/`capabilities`/`context_window`/`provider`) — 引擎侧, 保持不变
- 桥接: Plugin 内部 `from_model_info(ModelInfo)` → `pdk::ModelCapability` (纯数据转换, 无引擎依赖)

## Decision 2: Plugin 入口签名 — `pdk_register_tools(IToolRegistry&)` + 分层工具名 `model_router/<strategy>`

### 问题

Oracle Q1 决策: 3 个独立 `.so` Plugin, 每个策略 ship 为一个 Plugin。如何在已有 PluginLoader 框架内注册?

### 选项

- **A**: 新入口函数 `pdk_register_router(IModelRouter*)` — 扩展 PluginLoader 加载协议
- **B**: 复用 `pdk_register_tools(IToolRegistry&)` — 每个 Plugin 注册一个 tool, 返回 opaque router handle
- **C**: IModelRouter 直接作为 Plugin 对象 — 需要 PluginLoader 支持 `IModelRouter` 动态转换

### 决策

**选择 B** — 复用 `pdk_register_tools(IToolRegistry&)`。

理由:
1. 已有模式: Sprint 4 PDK 定义 `pdk_register_tools` 为 Plugin 入口, PluginLoader 已有此调用路径
2. 零框架改动: 不需要修改 `IPluginLoader` 或 Plugin 发现协议
3. 分层工具名: `model_router/cost` / `model_router/quality` / `model_router/latency` — `/` 前缀约定避免命名冲突, 与现有 `code::edit_file` 风格一致
4. 状态管理: Router 实例由 tool handler lambda 捕获 `shared_ptr<IModelRouter>`, 生命周期绑定 Plugin `.so` 加载
5. Option A/C 需修改 PluginLoader 接口 — 违反 C7 "零引擎改动" 约束

### 实现

```cpp
// pdk/model_router/cost_strategy/cost_router.cpp
extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
  auto router = std::make_shared<CostModelRouterPolicy>();

  ::agenticdsl::ToolMetadata meta{
    "model_router/cost",
    "cost-based model routing strategy",
    "model_router",
    ::agenticdsl::ToolCategory::ReadOnly,
    ::agenticdsl::LayerProfile::Workflow,
    ::agenticdsl::ApprovalPolicy{false, false, true, false}  // yolo 不审批
  };

  registry.register_tool_function(
    "model_router/cost", meta,
    [router](const auto& args) -> nlohmann::json {
      auto ctx = parse_routing_context(args);
      auto candidates = extract_candidates_from_args(args);
      auto result = router->route(ctx, candidates);
      return {{"model_id", result}, {"router", router->name()}};
    }
  );
}
```

### 引擎调用侧

引擎侧需要路由决策时:
```cpp
// engine 内部调用 Plugin 注册的工具
auto result = tool_registry_->call_tool("model_router/cost", {
  {"task_type", "code_generation"},
  {"budget_remaining", "0.50"},
  {"required_tags", "fast"}
});
std::string selected_model = result["model_id"];
```

## Decision 3: 状态模型 — Stateless 策略 per Plugin load, 模型注册表缓存在引擎层

### 问题

Plugin 路由策略需要缓存模型列表吗? 若 Plugin 自身缓存, Plugin reload 会丢失数据。

### 选项

- **A**: Plugin 内部缓存 — `IModelRouter` 内部持有 `vector<ModelCapability>` 副本
- **B**: Stateless 策略 — 引擎每次传入 `candidates` 列表, 路由决策为纯函数
- **C**: Plugin 缓存 + 引擎推送更新 — 引擎通过 EventBus 推送模型变更事件

### 决策

**选择 B** — Stateless 策略 (Oracle Q1 锁定)。

理由:
1. 纯函数可测试性: `route(RoutingContext, vector<ModelCapability>) → string` 无副作用, 测试无需 mock 状态
2. Plugin reload 安全: 无内部缓存, 重新加载 .so 不丢失数据 (引擎层持有 Provider 查询结果)
3. 数据新鲜度: 引擎每次调用 `available_models()` 获取最新列表, 再传给 `route()`
4. 与 Oracle Q1 一致: "策略 MUST 是 stateless 或使用外部状态 (context layer)"

### 数据流

```
Engine (DSLEngine)
  ├─ ILLMProvider::available_models() → vector<ILLMProvider::ModelInfo>
  ├─ convert_to_pdk_capabilities()     → vector<pdk::ModelCapability>
  └─ call_tool("model_router/cost", {ctx, candidates})
       └─→ Plugin .so: IModelRouter::route(ctx, candidates) → model_id
```

### 引擎层缓存策略

- 引擎缓存最近一次 `available_models()` 结果 (TTL 可选, Sprint 17 MVP 无 TTL)
- 每个 `DSLEngine::run()` 调用周期内复用缓存
- 不在 Plugin 内做 I/O 或 Provider 查询 (Plugin 只管决策)

## Decision 4: 错误模型 — `ModelRoutingError` 异常 + 3 种错误码

### 问题

路由失败时如何通知调用方? 抛出异常? 返回 error json? 静默 fallback?

### 选项

- **A**: 返回 `optional<string>` — 空 = 无可用模型
- **B**: 返回 `Result<string, ModelRoutingError>` — Result 模式
- **C**: 抛出 `ModelRoutingError` 异常 (带错误码) — 与 ToolResult::ErrorCode 一致

### 决策

**选择 C** — 抛出 `ModelRoutingError` (继承 `std::runtime_error`) + 3 种错误码。

理由:
1. 与 ADR-0004 V2 (ToolRegistry) + ADR-0031 (IExecutionPolicy) 的 error handling 一致 — 均用 enum ErrorCode + throw
2. 3 种错误码:
   - `NoViableModel`: 无模型满足 constraints (budget / tags / latency) — throw, orchestrator 可 fallback
   - `ProviderUnavailable`: 配置的 provider 未加载 — throw, 需人工干预
   - `AmbiguousCapability`: 多个模型 tie — 返回第一个 + log warning 到 `tool.audit.warning` topic
3. DECLARE_TOOL 宏自动 catch `std::exception` → 包装为 `json{{"error", e.what()}}`, `ModelRoutingError` 的 `what()` 含错误码前缀

### 实现

```cpp
class ModelRoutingError : public std::runtime_error {
public:
  enum class Code { NoViableModel, ProviderUnavailable, AmbiguousCapability };

  Code code;

  ModelRoutingError(Code c, const std::string& msg)
    : std::runtime_error(msg), code(c) {}

  // what() 自动包含错误码前缀
  static std::string make_message(Code c, const std::string& msg) {
    switch (c) {
      case Code::NoViableModel:       return "[NoViableModel] " + msg;
      case Code::ProviderUnavailable:  return "[ProviderUnavailable] " + msg;
      case Code::AmbiguousCapability: return "[AmbiguousCapability] " + msg;
    }
    return msg;
  }
};
```

### `AmbiguousCapability` 特殊处理

多个模型 tie 时不 throw, 而是:
1. 返回第一个匹配模型
2. 通过 `IInteractionBus` emit `tool.audit.warning` 事件 (若 bus 已注入)
3. 若 bus 未注入 (Plugin standalone test), 写入 stderr

## Decision 5: 测试策略 — MockLLMProvider + canned ModelCapability

### 问题

如何测试纯路由逻辑而不依赖真实 LLM Provider?

### 选项

- **A**: 测试直接构造 `vector<ModelCapability>` 传入 `IModelRouter::route()`
- **B**: Mock ILLMProvider 注入 `available_models()` → 测试通过 Engine 调用链
- **C**: 集成测试 + 真实 llama.cpp 模型

### 决策

**选择 B — MockLLMProvider + canned ModelCapability** (Oracle Q4 锁定)。

理由:
1. 路由逻辑是纯函数: `IModelRouter::route(ctx, candidates)` 可以直接测试 (Option A), 但需要验证 Engine→Plugin 调用链时用 Option B
2. MockLLMProvider 已有 `available_models()` 返回静态列表 (Sprint 0 stub), 扩展 `set_available_models(vector<ModelInfo>)` 测试 hook
3. 零真实 LLM 调用: 所有 3 策略测试仅依赖静态数据, 无网络/GPU 依赖
4. 测试 fixture 覆盖 3 策略各 4-5 scenario:

### 测试 scenario 设计

**cost 策略** (4 scenarios):
| # | Scenario | 输入 | 期望输出 |
|---|----------|------|---------|
| 1 | cheapest-viable | 2 模型 (gpt-4 $0.03, gpt-3.5 $0.002), tag=["general"] | gpt-3.5-turbo |
| 2 | budget-exceeded | budget=$0.001, 最便宜 $0.002 | throw NoViableModel |
| 3 | capability-mismatch | tag=["vision"], 模型均无 vision | throw NoViableModel |
| 4 | single-model | 1 模型 | 返回该模型 (即使非最优) |

**quality 策略** (4 scenarios):
| # | Scenario | 输入 | 期望输出 |
|---|----------|------|---------|
| 1 | tag-match | tag=["code","reasoning"], gpt-4 tag 包含两者, gpt-3.5 仅 general | gpt-4 |
| 2 | partial-match | tag=["code"], gpt-4 (code) vs gpt-3.5 (general) | gpt-4 |
| 3 | no-tag-match | tag=["vision"], 所有模型无 vision | fallback 返回第一个模型 |
| 4 | empty-tag | tag=[] | 返回 quality 最高模型 (按 n_ctx+max_tokens 总分) |

**latency 策略** (4 scenarios):
| # | Scenario | 输入 | 期望输出 |
|---|----------|------|---------|
| 1 | lowest-latency | gpt-4 500ms vs gpt-3.5 200ms | gpt-3.5-turbo |
| 2 | latency-budget | max_latency=300ms, gpt-4 500ms | 跳过 gpt-4, 选次优 |
| 3 | all-exceed-budget | max_latency=100ms, 全 > 100ms | throw NoViableModel |
| 4 | capability-constraint | tag=["vision"] + latency | 优先 tag match, 其次 latency |

**registry 工具** (3 scenarios):
| # | Scenario | 输入 | 期望输出 |
|---|----------|------|---------|
| 1 | list-all | 无 filter | 返回全部已注册模型 |
| 2 | filter-by-tag | tag="fast" | 仅返回含 "fast" tag 的模型 |
| 3 | no-match | tag="quantum" | 返回空列表 |

### 测试 fixture 设计

```cpp
// tests/test_model_router_plugins.cpp

// Setup helper: 构造 3 模型 candidate 列表
std::vector<ModelCapability> make_test_candidates() {
  return {
    {"gpt-4", "GPT-4", 8192, 4096, true,  true,  0.03, 500, {"general", "reasoning", "vision", "code"}},
    {"gpt-3.5-turbo", "GPT-3.5", 4096, 4096, true, false, 0.002, 200, {"general", "fast"}},
    {"claude-3", "Claude 3", 16384, 4096, true, true, 0.015, 350, {"general", "reasoning", "code"}}
  };
}

TEST_CASE("CostModelRouter returns cheapest Chat-capable model", "[model_router][cost]") {
  auto router = CostModelRouterPolicy{};
  auto candidates = make_test_candidates();
  RoutingContext ctx;
  ctx.task_type = "completion";
  ctx.required_tags = {"general"};

  auto model_id = router.route(ctx, candidates);
  REQUIRE(model_id == "gpt-3.5-turbo");
}
```

## 引用

- `docs/adr/plugin/adr-0034-model-router.md` — ADR-0034 设计意图
- `include/agenticdsl/pdk/pdk.h` — PDK 统一入口 (v0.1.0)
- `include/agenticdsl/pdk/tool_macros.h` — DECLARE_TOOL 宏 (C6 升级, 4 参数)
- `include/agenticdsl/contract/itool_registry.h` — IToolRegistry 接口 (V2)
- `src/common/llm/llm_types.h` — ILLMProvider::ModelCapability (enum) + ModelInfo (struct)
- `src/common/policy/execution_policy.h` — ToolMetadata + ApprovalPolicy + LayerProfile (C6 ship)
- `scripts/sync-pdk.sh` — Dual-Repo 同步 (Sprint 4 T4b)
- `examples/phase1_model_router_plugin/main.cpp` — Sprint 0 stub (将被 C7 取代)
- `openspec/changes/2026-06-26-adr-0034-model-router-plugin/proposal.md` — C7 proposal