# ADR-0034: IModelRouter 模型路由接口

## 状态

**提议中** (2026-05-28)

## 领域

基座 / LLM / 模型路由

## 关联

- ADR-0001（ILLMProvider 流式接口）— 现有 `generate_stream()` 基础
- ADR-0030（AsyncRuntime 双层协程）— Phase 2/3 依赖的异步运行时
- ADR-0002（EventBus）— Phase 3 事件集成
- ADR-0033（SessionHierarchy）— SessionContext 路由上下文

---

## 背景

### 当前代码库状态

| 组件 | 现状 | 评估 |
|------|------|------|
| **ILLMProvider** | ⚠️ 基础实现——仅有 `generate()` + `generate_stream()` | 需扩展模型查询能力 |
| **LlamaAdapter** | ~~⚠️ 同步实现——简单 `generate(prompt)`~~ ✅ **已修复 (C1.2, commit 3f28020)**：LlamaAdapter 已通过 `LlamaAdapterProvider` 适配为 ILLMProvider | 需支持多模型标识 |
| **HttpLLMAdapter** | ⚠️ HTTP 实现——使用 cpp-httplib | 需支持模型标识 |
| **ILLMTool** | ⚠️ 工具接口——独立于 Provider | 无路由感知 |
| **LLM 配置** | ⚠️ 运行时配置——`llm_config.json` | 无模型发现机制 |
| **NodeExecutor** | ~~⚠️ 直接调用——`execute_llm_call()` 无路由层~~ ✅ **已修复 (C1.2, commit 3f28020)**：`execute_llm_call` 已删除，引擎通过 `ILLMProvider` 路由 | 需解耦 |

### 问题

1. **硬编码模型选择**：当前 LLM 调用直接指定模型，无路由层抽象
2. **多模型支持缺失**：无法在同一会话中根据任务类型切换不同模型
3. **Provider 模型能力未知**：调用方无法查询 Provider 支持哪些模型
4. **预算感知路由缺失**：无法根据剩余预算选择合适模型
5. **Fleet Mode 路由缺失**：批量场景需要模型能力匹配

---

## 决策

### 1. ILLMProvider 扩展（非破坏性）

**关键决策**：在 `ILLMProvider` 中添加默认实现方法，不破坏现有实现。

```cpp
// ===== src/common/llm/llm_types.h =====

// 模型能力描述
struct ModelCapability {
    std::string model_id;
    std::string model_name;           // 人类可读名称
    int n_ctx = 4096;                 // 上下文窗口大小
    int max_tokens = 4096;            // 最大输出token数
    bool supports_streaming = true;   // 是否支持流式
    bool supports_function_call = false;
    float per_token_cost = 0.0f;      // 每token成本（USD）
    std::vector<std::string> tags;    // 能力标签：["fast", "vision", "code"]
};

class ILLMProvider {
public:
    virtual ~ILLMProvider() = default;

    virtual Result<GenerationResult, LLMError>
        generate(const GenerationRequest& req, std::stop_token token) = 0;

    virtual std::unique_ptr<IGenerationStream>
        generate_stream(const GenerationRequest& req, std::stop_token token) = 0;

    // ===== 新增：默认实现（非破坏性）=====

    // 返回该Provider支持的所有模型
    virtual std::vector<ModelCapability> available_models() const {
        return {};
    }

    // 检查指定模型是否可用
    virtual bool is_available(const std::string& model_id) const {
        return true;
    }
};
```

### 2. IModelRouter 接口

**关键决策**：路由接口位于 `src/common/llm/` 基座层，认知层可扩展但基座不依赖认知层。

```cpp
// ===== src/common/llm/model_router.h =====

namespace agenticdsl {

// 路由上下文——描述一次路由请求的特征
struct RoutingContext {
    std::string task_type;           // "completion", "code_generation", "reasoning"
    std::string session_id;
    std::optional<int> max_tokens;    // 如果明确要求最大输出
    std::optional<float> budget_remaining;  // 剩余预算
    std::vector<std::string> required_tags; // 必须具备的能力标签
    std::string preferred_model;      // 用户偏好（可忽略）
    bool is_fleet_mode = false;       // 是否舰队模式批量处理
};

// 模型路由器接口
class IModelRouter {
public:
    virtual ~IModelRouter() = default;

    // 核心方法：根据上下文选择最合适的模型ID
    virtual std::string route(const RoutingContext& ctx) = 0;

    // 返回路由器名称（用于日志和调试）
    virtual std::string name() const = 0;
};

}  // namespace agenticdsl
```

### 3. ModelRegistry 模型注册表

**关键决策**：集中管理模型ID到Provider的映射，支持运行时注册。

```cpp
// ===== src/common/llm/model_registry.h =====

namespace agenticdsl {

class ModelRegistry {
public:
    // 注册模型：模型ID + 能力描述 + 提供者实例
    void register_model(
        std::string model_id,
        ModelCapability cap,
        ILLMProvider* provider
    );

    // 获取模型对应的Provider
    ILLMProvider* get_provider(const std::string& model_id) const;

    // 获取模型能力描述
    const ModelCapability* get_model_info(const std::string& model_id) const;

    // 返回所有注册模型
    std::vector<ModelCapability> all_models() const;

    // 根据标签筛选可用模型
    std::vector<ModelCapability> models_with_tags(
        const std::vector<std::string>& required_tags
    ) const;

private:
    struct ModelEntry {
        ModelCapability capability;
        ILLMProvider* provider;
    };
    std::unordered_map<std::string, ModelEntry> models_;
};

}  // namespace agenticdsl
```

### 4. DefaultModelRouter 默认路由实现

**关键决策**：提供基于任务类型的简单路由实现，支持预算感知。

```cpp
// ===== src/common/llm/default_model_router.h =====

namespace agenticdsl {

// 任务类型到模型的默认映射配置
struct TaskModelMapping {
    std::string task_type;
    std::string default_model;
    std::vector<std::string> fallback_models;
};

// 默认路由器：基于任务类型 + 预算选择模型
class DefaultModelRouter : public IModelRouter {
public:
    explicit DefaultModelRouter(const ModelRegistry& registry);

    std::string route(const RoutingContext& ctx) override;
    std::string name() const override { return "default"; }

    // 配置任务类型映射
    void set_task_mapping(std::vector<TaskModelMapping> mappings);

private:
    // 按优先级筛选可用模型
    std::string select_by_priority(
        const std::vector<std::string>& candidates,
        const RoutingContext& ctx
    ) const;

    const ModelRegistry& registry_;
    std::vector<TaskModelMapping> task_mappings_;
};

}  // namespace agenticdsl
```

### 5. 使用模式（同步）

```cpp
// ===== 使用示例 =====

// 1. 创建注册表并注册模型
ModelRegistry registry;
registry.register_model(
    "gpt-4",
    ModelCapability{
        .model_id = "gpt-4",
        .model_name = "GPT-4",
        .n_ctx = 8192,
        .supports_streaming = true,
        .supports_function_call = true,
        .per_token_cost = 0.03f,
        .tags = {"general", "reasoning", "vision"}
    },
    &gpt4_provider
);

registry.register_model(
    "gpt-3.5-turbo",
    ModelCapability{
        .model_id = "gpt-3.5-turbo",
        .model_name = "GPT-3.5 Turbo",
        .n_ctx = 4096,
        .supports_streaming = true,
        .per_token_cost = 0.002f,
        .tags = {"fast", "general"}
    },
    &gpt35_provider
);

// 2. 创建路由器
DefaultModelRouter router(registry);
router.set_task_mapping({
    {"code_generation", "gpt-4", {"gpt-3.5-turbo"}},
    {"reasoning", "gpt-4", {}},
    {"completion", "gpt-3.5-turbo", {"gpt-4"}}
});

// 3. 路由调用
RoutingContext ctx;
ctx.task_type = "code_generation";
ctx.budget_remaining = 0.50f;  // 50美分预算
ctx.required_tags = {"fast"};

std::string selected_model = router.route(ctx);
ILLMProvider* provider = registry.get_provider(selected_model);

// 4. 执行生成
GenerationRequest request;
request.prompt = "Write a hello world function";
request.params.model = selected_model;

auto result = provider->generate(request, std::stop_token{});
if (result.has_value()) {
    std::cout << result.value().text << std::endl;
}
```

### 6. 演进路径

#### Phase 1：同步路由 + 同步调用（当前）

- [ ] 在 `llm_types.h` 添加 `ModelCapability` 结构
- [ ] 在 `ILLMProvider` 添加 `available_models()` + `is_available()` 默认实现
- [ ] 实现 `ModelRegistry` 类
- [ ] 实现 `IModelRouter` 接口
- [ ] 实现 `DefaultModelRouter` 类
- [x] 集成到 NodeExecutor：`execute_dsl_node()` 使用路由器 ✅ (C1.2, commit 3f28020)

#### Phase 2：异步/流式支持（ADR-0030 就绪后）

- [ ] 在 `IModelRouter` 添加异步 `route_async()` 方法
- [ ] 实现 `StreamingModelRouter` 支持流式路由
- [ ] 添加 `LLMCallCoordinator` 协调器（ADR-0030）
- [ ] 支持streaming响应聚合

#### Phase 3：批量/舰队模式（ADR-0002 就绪后）

- [ ] 添加批量路由 `route_batch()` 方法
- [ ] 集成 EventBus 事件（ADR-0002）
- [ ] 实现模型可用性健康检查
- [ ] 支持Fleet Mode自动故障切换

---

## 验证标准

### 功能验证

1. **路由正确性**：给定 `RoutingContext`，路由器返回 `ModelRegistry` 中存在的模型ID
2. **任务映射**：配置 `task_type` 映射后，相同任务类型始终路由到相同模型（无预算压力时）
3. **预算感知**：当 `budget_remaining` 低于高成本模型阈值时，自动切换到低成本模型
4. **标签筛选**：指定 `required_tags` 后，仅返回匹配标签的模型
5. **Fallback**：主模型不可用时，尝试 fallback 列表中的模型

### 非破坏性验证

6. **现有代码兼容**：现有 `LlamaAdapter` 现需通过 `LlamaAdapterProvider` 包装为 `ILLMProvider` 后注入引擎；`HttpLLMAdapter` 同理。C1 后 (commit 3f28020) 已完成适配器包装层。
7. **默认行为一致**：未配置路由器时，使用第一个注册的模型（向后兼容）

### 性能验证

8. **路由延迟**：单次 `route()` 调用不超过 1ms
9. **无锁设计**：路由决策为纯函数，无锁竞争

---

## 风险与缓解

| 风险 | 影响 | 缓解 |
|------|------|------|
| **Provider 未实现新方法** | 低——默认实现返回空列表/`true` | 仅影响模型发现功能，不影响生成 |
| **路由循环依赖** | 中——Registry持有Provider，Router持有Registry | 构造函数注入，避免循环 |
| **Phase 2 异步设计改动** | 低——Phase 1 为同步子集 | 抽象接口先行，延迟实现 |
| **多Provider同一模型ID冲突** | 中——后者覆盖前者 | `register_model()` 返回 `false` 并记录警告 |

---

## 参考

- [ADR-0001: ILLMProvider 流式接口](./adr-0001-illm-provider-streaming-interface.md)
- [ADR-0030: AsyncRuntime 双层协程](./adr-0030-async-runtime-dual-layer.md)
- [ADR-0002: EventBus 有界队列](./adr-0002-eventbus-bounded-queue.md)
- [ADR-0033: SessionHierarchy](./adr-0033-session-hierarchy.md)
- [llm_types.h 源文件](../../../src/common/llm/llm_types.h)
- [llama_adapter.h 源文件](../../../src/common/llm/llama_adapter.h)
- [http_adapter.h 源文件](../../../src/common/llm/http_adapter.h)