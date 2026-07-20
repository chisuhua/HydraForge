## Context

`DSLEngine::from_markdown()` 当前实现（`src/core/engine.cpp:40-60`）使用 `std::make_unique<DSLEngine>(std::move(graphs))` 创建子引擎，DSLEngine 构造器内部使用 `LLMProviderFactory::create(mock_config)` 初始化一个默认的 MockLLMProvider。父引擎无法将其 LLM provider（可能是真实 provider、经过 Decorator 链包装的 provider）传递给子引擎。

`pdk/loop_agent/src/pdk_entry.cpp` 中 `loop/run` 工具的 lambda 本应调用 `DSLEngine::from_markdown(content)` 执行 `lib/loop/react.agent.md`，但因子引擎无真实 LLM provider 而退回 mock 响应。

### 现有架构

```
parent_engine (configured with LLM provider)
  │
  ├── from_markdown(dsl_content)
  │     └── new DSLEngine(graphs)
  │           └── llm_provider_ = MockLLMProvider  ← 无法继承
  │
  └── set_llm_provider(real_provider)  ← 只影响父引擎自己
```

## Goals / Non-Goals

**Goals:**
- `DSLEngine::from_markdown()` 创建的子引擎能继承父引擎的 LLM provider（包括 Decorator 链包装后的 provider）
- `pdk/loop_agent` 的 `loop/run` 工具能真实调用 `from_markdown` 并执行 DSL 中的 LLM 节点
- 向后兼容：现有调用 `from_markdown(content)` 的代码不受影响
- 生命周期安全：父引擎析构后子引擎不应持有 dangling provider 指针

**Non-Goals:**
- 不修改 `ILLMProvider` 接口或 `LLMProviderFactory`
- 不传播其他 DSLEngine 内部状态（如 budget、session registry、tool registry）
- 不解决多层嵌套的 provider 传播（仅解决 1 层 parent→child）
- 不修改 PluginLoader 的加载接口

## Decisions

### Decision 1 — 传播方式：显式参数重载 + 双字段存储

**方案选择**：新增 `from_markdown(content, ILLMProvider&)` 静态重载。

```cpp
// engine.h
static std::unique_ptr<DSLEngine> from_markdown(
    const std::string& markdown_content,
    ILLMProvider& parent_provider);  // 新重载
```

**理由**：
- 显式参数语义清晰，调用点明确知道正在传播 provider
- 无需生命周期管理（引用不转移所有权，父引擎仍拥有 provider）
- 对比 `set_llm_provider()` 后调方案（方案 B），重载更安全——子引擎构造完成时 provider 已就位，不会出现「构造了但未设 provider」的时间窗口
- 对比 `shared_ptr<ILLMProvider>` 方案（方案 C），引用更轻量且不引入共享可变状态

**⚠️ 实现关键 — 存储矛盾化解**：
`llm_provider_` 当前声明为 `unique_ptr<ILLMProvider>`（engine.h:196），无法直接存储 `ILLMProvider&`。新 overload 不得调用 `set_llm_provider(unique_ptr)`（会触发 `decorate_provider()` 二次包裹）。解决方案：**双字段存储**。

```cpp
// engine.h — 新增字段
std::unique_ptr<ILLMProvider> owned_provider_;   // 持有所有权（set_llm_provider 路径）
ILLMProvider* borrowed_provider_ = nullptr;       // 非拥有借用（from_markdown + ILLMProvider& 路径）

// getter 统一返回激活的 provider
ILLMProvider* get_llm_provider() {
    return borrowed_provider_ ? borrowed_provider_ : owned_provider_.get();
}
```

**实现**：
```cpp
std::unique_ptr<DSLEngine> DSLEngine::from_markdown(
    const std::string& content,
    ILLMProvider& parent_provider)
{
    auto engine = from_markdown(content);                    // 复用基础逻辑（默认 MockLLMProvider）
    engine->owned_provider_.reset();                          // 释放 MockLLMProvider
    engine->borrowed_provider_ = &parent_provider;            // 借用父引擎 provider
    // 注意：此处不调用 set_llm_provider() — 避免触发 decorate_provider() 二次包裹
    return engine;
}
```

**不变式**：
- `owned_provider_` 和 `borrowed_provider_` 互斥：任何时候至多一个非 null
- `get_llm_provider()` 先检查 `borrowed_provider_`，再 fallback `owned_provider_`

### Decision 2 — 传递已装饰的 provider（非 raw）+ 生命周期契约

**方案：传递 `parent.llm_provider_`（已装饰链），子引擎以非拥有引用持有**

```cpp
// 子引擎持有的是 parent 已包裹 CostTrackingDecorator 的 provider
// 调用方：from_markdown(content, *parent.get_llm_provider())
```

**理由**：
- `ILLMProvider` 是虚基类接口，拷贝构造函数不可用
- Factory 重建需要知道具体 provider 类型和配置，当前 `ILLMProvider` 接口没有提供 `clone()` 或 `config()` 方法
- **装饰器链语义**：子引擎继承的是 `decorate_provider()` 处理后的已包裹链（CostTracking → Compliance → RateLimit → inner）。这意味着：
  - CostTrackingDecorator 从 parent engine 捕获 child engine 的 LLM 调用并正确归入 parent budget
  - 子引擎不再调用 `decorate_provider()` 重新包裹（避免 CostTracking 双重计费）
  - **不变式**：子引擎 LLM 调用的 budget 归属 parent engine（当前 spec 边界，非未来目标）
- 生命周期契约：Plugin 场景中，父引擎（`pdk_chat_demo/main.cpp` 中的 `engine`）在 Plugin 卸载前不会销毁
- 使用 `ILLMProvider*` raw ptr 通过 `borrowed_provider_` 字段存储（非 owning），子引擎不负责销毁

**风险评估**：如果未来出现父引擎先于子 engine 销毁的场景，需要升级为 `shared_ptr`。当此场景确认时再改，不现在过度设计。

### Decision 3 — Plugin 侧桥接方案：`thread_local` 全局 + `loop/set_parent_provider` 工具

`pdk/loop_agent/src/pdk_entry.cpp` 中 `pdk_register_tools` 签名为 `void(IToolRegistry&)`，无法接受父引擎引用。需扩展 plugin 初始化机制。

当前 PluginLoader 不支持初始化上下文传递（ADR-0022 §决策约束），采用**轻量方案**：

**⚠️ 不采用普通 static 全局**（Metis 审查发现：进程级 static 在多 DSLEngine 场景下会互相覆盖）。改用 `thread_local` 实现线程级隔离：

```cpp
// pdk_entry.cpp 中 loop/run lambda
static thread_local ILLMProvider* tls_parent_provider = nullptr;

// 新增 loop/set_parent_provider 工具
// ⚠️ 必须使用 DECLARE_TOOL V2 宏（Sprint 6+）声明安全元数据：
//   category = SystemConfig
//   approval_policy = force_approval_always
//   allowed_layers = {Workflow}
registry.register_tool_function("loop/set_parent_provider", 
    ToolMetadata{
        .name = "loop/set_parent_provider",
        .description = "Set parent engine LLM provider for DSL execution",
        .domain = "loop",
        .category = ToolCategory::SystemConfig,       // 系统配置类目
        .min_layer = LayerProfile::Workflow,
        .approval = {false, true, false, true},       // force_approval_always=true
        .allowed_layers = {LayerProfile::Workflow}
    },
    [](const std::unordered_map<std::string, std::string>& args) -> json {
        // F2: 校验 nullptr
        auto it = args.find("provider_ptr");
        if (it == args.end() || it->second.empty()) {
            return {{"success", false}, {"error", "provider_ptr required"}};
        }
        // F4/6: overwrite 时打 warning
        if (tls_parent_provider) {
            // 只有真正替换时才 log warning（而非首次设置）
            // log: "Overwriting existing parent provider"
        }
        tls_parent_provider = reinterpret_cast<ILLMProvider*>(std::stoull(it->second));
        return {{"success", true}};
    });

// loop/run 使用 tls_parent_provider 调用 from_markdown
// F3: 存在时真实执行，不存在时返回 mock（但打 error log）
[](const std::unordered_map<std::string, std::string>& args) -> json {
    if (!tls_parent_provider) {
        // log error: "parent provider not set, falling back to mock"
        // 返回 mock 响应（与当前实现一致）
    }
    auto agent_content = load_agent_file(loop_type);
    auto engine = DSLEngine::from_markdown(agent_content, *tls_parent_provider);
    auto result = engine->run(ctx);
    return result;
};
```

**为什么不扩展 pdk_register_tools 签名**：
- 这是最干净的方案（传 `PluginInitContext&`），但涉及 ABI 版本变更
- 留作 ADR-0052 候选（Phase 6+），当前 change 不解决

## Risks / Trade-offs

| 风险 | 缓解措施 | 状态 |
|------|---------|------|
| 父引擎先于子 engine 销毁导致 dangling 引用 | 当前不存在此场景（子 engine 在 loop/run 调用内创建并同步执行，执行完即销毁）。如果未来异步化，改 `shared_ptr`。`borrowed_provider_` 生命周期契约已文档化 | ✅ 已覆盖 |
| Plugin 的 `thread_local` 变量不是进程安全（仅线程级隔离） | `thread_local` 比普通 static 更安全：多 DSLEngine 在不同线程运行时隔离。同一线程多 engine 场景仍可能互相覆盖，用 overwrite warning 缓解 | 🟡 部分缓解 |
| 新 overload 两阶段初始化（`from_markdown` 基础构造 + 释放 Mock + 设 `borrowed_provider_`）可能导致构造后状态不一致 | 使用 RAII 封装：新 overload 保证 `from_markdown` 基础构造在前，`owned_provider_.reset()` + `borrowed_provider_ = &` 的原子性。不变式：任何时候至多一个非 null | ✅ 已覆盖 |
| 真实 LLM provider 的 Decorator 链（CostTrackingDecorator 等）在子 engine 中不生效 | ✅ **修复**：子 engine 直接继承 parent 的 `llm_provider_`（已包裹链），**不再**重新 decorate。cost tracking 计入 parent budget。详见 Decision 2 | ✅ 已覆盖 |
| CostTrackingDecorator 双重包裹 | **不会发生**：新 overload **禁止**调用 `set_llm_provider()`，直接写 `borrowed_provider_` 字段绕过装饰器。代码审查时 grep 验证 | ✅ 已覆盖 |
| `loop/set_parent_provider` 多工具注册调用导致覆盖 | `thread_local` + overwrite warning log。该工具只应被 `main.cpp` 在初始化阶段调用 | 🟡 部分缓解 |
| 测试间 `thread_local` 状态泄漏 | Catch2 不同 TEST_CASE 在同线程执行时共享 `thread_local`。要求每个 TEST_CASE 开头显式重置为 nullptr | ✅ 测试纪律 |
| `loop/set_parent_provider` 工具缺少安全元数据 | 使用 DECLARE_TOOL V2 宏声明 `category=SystemConfig`, `force_approval_always=true`, `allowed_layers={Workflow}` | ✅ 已覆盖 |
| PlanExecute / ForkJoin loop_type 未覆盖 provider 透传 | 设计上两种模式同样依赖 `tls_parent_provider`。spec 和 tasks 已覆盖 | ✅ 新覆盖 |
| child engine 的 LLM 调用 budget 不可单独观测 | 当前设计有意将 child budget 归入 parent（非 bug）。如果未来需要独立 child budget，需扩展 `ILLMProvider` 接口或引入 `IBudgetController` 分叉 | ⚠️ 已知限制 |
| Plugin ABI 不支持上下文传递（ADR-0022 §决策） | ADR-0052 候选（Phase 6+）— 扩展 `pdk_register_tools` 签名 | 🔮 远期缓解 |

## 开放问题（实施前需澄清）

> **2026-07-19**: 以下 7 个歧义点已与用户确认，决议如下。实施时直接按决议执行。

| ID | 问题 | 决议 | 影响范围 |
|----|------|------|---------|
| Q1 | `from_markdown(content, ILLMProvider&)` 的第二参数是 parent 的 raw provider 还是已装饰的 `llm_provider_`？ | **✅ 已装饰的**。调用方传 `*parent.get_llm_provider()` | spec + 测试 |
| Q2 | `thread_local` 同一线程多 engine 覆盖时，覆盖行为是 throw / last-write-wins / first-engine 锁定？ | **✅ last-write-wins** + overwrite warning log | pdk_entry.cpp 实现 |
| Q3 | 旧 `from_markdown(content)` 单参数签名是否保留？子引擎没拿到 provider 时的行为？ | **✅ 保留**。无 provider → MockLLMProvider（100% 向后兼容） | 无影响 |
| Q4 | child DSL 执行错误如何传播回 parent context？ | **✅ return 传播**。`loop/run` 返回 json 中的 error 字段 | pdk_entry.cpp |
| Q5 | `loop/set_parent_provider` 的访问控制（category/approval_policy/allowed_layers） | **✅ SystemConfig / force_approval_always / {Workflow}** | pdk_entry.cpp |
| Q6 | child engine 的 budget 归 parent 是否预期？需要独立 budget 吗？ | **✅ 预期**（当前范围外，已知限制） | spec + 测试 |
| Q7 | 既有 mock 测试 case 是否一次性迁移到真实执行路径？ | **✅ 无既有测试**，从零编写全量测试矩阵（17+ TEST_CASE） | 测试策略 |