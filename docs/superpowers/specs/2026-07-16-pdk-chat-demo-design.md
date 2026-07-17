# PDK Chat Demo 设计文档

**日期**: 2026-07-16
**状态**: ✅ Reviewed（Oracle 分析完成，OS/Agent 边界已确定）
**参考**:
- pi-mono (badlogic/pi-mono, MIT, 71.6K ⭐) — 架构范式参考
- tau (huggingface/tau, Apache 2.0) — 3A 架构参考
- HydraForge ADR-0033 (session-hierarchy), ADR-0031 (ToolCoordinator), ADR-0042 (decorator chain)
- Oracle session `ses_095b7dddaffe2EGykbeTXBxEzs` (2026-07-16) — OS/Agent 边界启发式

---

## 1. 总览

`examples/pdk_chat_demo` 演示两个核心理念：

1. **HydraForge 是 AgenticOS**——只提供基础设施，不包含业务逻辑
2. **Agent 可以通过 skill 或 agenticdsl 灵活实现**——应用层不关心实现形式

应用由 Agent 组成，Agent 有**两种实现路径**：

| 实现路径 | 形式 | 适用场景 | 示例 |
|---------|------|---------|------|
| **skill** | `.agent.md` DSL 工作流 | 逻辑驱动、需要 LLM 编排的工作流 | Loop Agent (react.agent.md) |
| **agenticdsl** | PDK C++ plugin (`.so`) | 性能关键、需要 C++ 原生能力的组件 | Provider Agent, 备选 Loop Agent |

应用层（Chat Agent）通过 `ToolRegistry` 统一调用，不需要区分 Agent 是 skill 还是 agenticdsl 实现。

```
┌─── 应用层 (Application = Agent 组合) ───────────────────────────┐
│                                                                  │
│  Chat Agent (examples/pdk_chat_demo/)                            │
│  ├─ 编排 Loop Agent + Provider Agent + Session Agent + Budget Agent │
│  ├─ Prompt 构建 + 事件监听                                       │
│  └─ 通过 ToolRegistry/IInteractionBus 统一调用                   │
│                                                                  │
│  ┌─ Loop Agent ──────────────────────────────────────────────┐  │
│  │  skill: lib/loop/react.agent.md | agenticdsl: .so 备选    │  │
│  │  契约: call_tool("loop/run", {prompt, ctx, tools})        │  │
│  ├─ Session Agent ──────────────────────────────────────────┤  │
│  │  agenticdsl: pdk/chat_session/libChatSession.so            │  │
│  │  契约: session/history, branch, compact, persist          │  │
│  ├─ Provider Agent ─────────────────────────────────────────┤  │
│  │  agenticdsl: pdk/provider_agent/libProviderAgent.so       │  │
│  │  契约: provider/resolve, register, list, health           │  │
│  ├─ Budget Agent ───────────────────────────────────────────┤  │
│  │  agenticdsl: pdk/budget_agent/libBudgetAgent.so            │  │
│  │  契约: budget/query, set_limit, alerts, cost_breakdown    │  │
│  └───────────────────────────────────────────────────────────┘  │
│                                                                  │
├─── AgenticOS (HydraForge 基础设施，同步/确定性/不可降级) ───────┤
│  DSLEngine │ ToolRegistry │ ILLMProvider                         │
│  IInteractionBus (InMemoryBus) │ IBudgetController (atoms)       │
│  IExecutionPolicy │ ToolCoordinator │ ApprovalHandler            │
│  UserSession/TaskSession/SubtaskSession (ADR-0033)               │
│  TopoScheduler │ CostTrackingDecorator                           │
└──────────────────────────────────────────────────────────────────┘
```

**OS/Agent 边界启发式（4 测试法，Oracle 提供）**：

| 测试 | YES → Agent | NO → OS |
|------|------------|---------|
| 需要 LLM 推理？ | Session compact, Budget 策略 | try_consume_node（纯原子） |
| 可优雅降级？ | Provider failover, Budget 告警 | 安全审批（失败必须 fail-stop） |
| 有 domain 知识？ | Chat session 格式, 成本分摊规则 | DAG 调度, 节点执行 |
| 被多组件传递依赖？ | — | Bus, Scheduler, Budget atoms |

**4 个测试全 YES → Agent。任一 NO → OS。**

**与 pi-mono / tau 的对应**：

| pi-mono | tau | HydraForge（新范式） |
|---------|-----|---------------------|
| `pi-ai` (Provider 层) | `tau_ai` | AgenticOS: ILLMProvider + LLMProviderFactory |
| `pi-agent-core` (Loop) | `tau_agent` (`run_agent_loop`) | **Loop Agent**（skill: `.agent.md` 或 agenticdsl: `.so`） |
| `pi-coding-agent` (App) | `tau_coding` (`AgentHarness`) | **Chat Agent**（编排器）+ **Provider Agent**（agenticdsl） |

---

## 2. 文件结构

```
examples/pdk_chat_demo/
├── CMakeLists.txt                # 独立编译目标
├── config.json                   # Agent 配置
├── main.cpp                      # 入口
├── chat_session.h / .cpp         # ChatSession（待 Oracle 确定边界）
├── tools/                        # 业务工具
│   ├── fs_tools.cpp              #   fs.read / fs.write
│   └── shell_tools.cpp           #   shell.exec
└── lib/loop/                     # Loop Agent DSL 定义
    ├── react.agent.md            #   ReactLoop Agent
    └── plan_execute.agent.md     #   PlanExecuteLoop Agent（预留）

pdk/provider_agent/               # Provider Agent（扩展 model_router）
├── CMakeLists.txt
├── src/
│   ├── provider_registry.cpp     #   注册 provider/config
│   ├── provider_resolve.cpp      #   resolve → LLMConfig JSON
│   ├── credential_store.cpp      #   凭据管理
│   └── pdk_entry.cpp             #   pdk_register_tools + plugin_info
└── include/
    └── provider_agent.h          #   ProviderInfo 结构体
```

---

## 3. JSON Schema (`config.json`)

```json
{
  "schema_version": "1.0",

  "providers": {
    "openai": {
      "api_key_env": "OPENAI_API_KEY",
      "api_url": "https://api.openai.com/v1",
      "models": {
        "gpt-4o":      { "model": "gpt-4o",      "max_tokens": 4096, "temperature": 0.7 },
        "gpt-4o-mini": { "model": "gpt-4o-mini", "max_tokens": 2048 }
      }
    },
    "anthropic": {
      "api_key_env": "ANTHROPIC_API_KEY",
      "api_url": "https://api.anthropic.com/v1",
      "models": {
        "claude-sonnet": { "model": "claude-sonnet-4-20250514", "max_tokens": 4096 }
      }
    },
    "mock": {
      "models": {
        "test": { "model": "mock-llm-v1" }
      }
    }
  },

  "agent": {
    "loop_type": "react",
    "provider": "mock",
    "model": "test",
    "system_prompt": "You are a helpful coding assistant...",
    "tools": ["fs.read", "fs.write", "shell.exec"],
    "max_steps": 50,
    "timeout_ms": 300000
  }
}
```

**解析规则**:
- `providers.{name}.models.{model}` → Provider Agent 的注册单元
- `agent.provider + "/" + agent.model` → Provider Agent resolve 的 key
- `--mock` CLI flag → 强制 `provider="mock"`, `model="test"`
- `schema_version` → 前向兼容检查

---

## 4. Provider Agent 设计

### 4.1 扩展策略

**选择 Option A**（扩展 model_router，而非替换）：

| model_router 现有 | Provider Agent 新增 |
|---|---|
| `model_router/cost` | `provider/register` — 注册 provider config |
| `model_router/quality` | `provider/resolve` — 路由 + 查凭据 → LLMConfig |
| `model_router/latency` | `provider/list` — 列出所有 provider |
| `model_router/registry` | 内部: credential_store — 凭据管理 |

### 4.2 新增数据结构

```cpp
// provider_agent.h — 扩展自模型元数据
struct ProviderInfo {
    std::string id;                       // "openai", "anthropic", "mock"
    std::string api_url;                  // base URL
    std::string api_key_env;              // 环境变量名（延迟解析）
    std::map<std::string, LLMConfig> models;  // model_name → LLMConfig
};

// 工具注册:
//   provider/register(config_json) → {ok, count}
//   provider/resolve(provider_id, model_id) → LLMConfig JSON
//   provider/list() → [{id, api_url, models: [...]}]
```

### 4.3 provider/resolve 流程

```
Chat Agent
  call_tool("provider/resolve", {provider:"openai", model:"gpt-4o"})
    │
    ▼
Provider Agent (PDK plugin)
  ├─ 查 credential_store[provider_id]
  ├─ 查 models[model_id]
  ├─ resolve_api_key()    // 延迟解析: env > file > direct
  ├─ 拼装 LLMConfig JSON
  └─ 返回 LLMConfig
    │
    ▼
Chat Agent
  auto cfg = LLMConfig::from_json(result);
  auto provider = LLMProviderFactory::create(cfg);
  engine->set_llm_provider(std::move(provider));
```

---

## 5. Agent 实现形式的灵活性

每个 Agent 可以选择 **skill**（`.agent.md` DSL）或 **agenticdsl**（PDK `.so` plugin）实现。应用层（Chat Agent）不关心实现形式，只关心契约。

| 维度 | skill (DSL) | agenticdsl (PDK) |
|------|------------|-------------------|
| 文件 | `.agent.md` | `.so` plugin |
| 加载 | `DSLEngine::from_file()` | `PluginLoader::load_so()` |
| 调用 | `DSLEngine::run(ctx)` | `ToolRegistry::call_tool(...)` |
| 适用 | 逻辑驱动、LLM 编排 | 性能关键、C++ 原生 |
| 灵活性 | 无需重新编译即可改逻辑 | 需要重新编译 |
| 示例 | Loop Agent (react.agent.md) | Provider Agent (libProviderAgent.so) |

**为什么 Loop Agent 首选 skill**：
- Loop 逻辑本质是 think→decide→act→observe 的状态机
- DSL 可以直接被 LLM 理解/生成/修改（未来自举）
- 可以热更新（无需重新编译 HydraForge）

**为什么 Provider Agent 首选 agenticdsl**：
- 凭据管理需要安全内存操作（env var 解析、文件读取）
- `LLMProviderFactory` 在 C++ 侧
- model_router 已有成熟的 .so 基础设施

---

## 6. Loop Agent 设计（skill 为主，agenticdsl 备选）

### 6.1 契约

无论 skill 还是 agenticdsl 实现，对外契约相同：

```
call_tool("loop/run", {
  "loop_type": "react",
  "prompt": "...",
  "system_prompt": "...",
  "history": [...],
  "tools": ["fs.read", "fs.write", "shell.exec"],
  "ctx": {...}
})
→ { "response": "...", "steps": 3, "tokens_used": 1500 }
```

### 6.2 skill 实现：`lib/loop/react.agent.md`

```markdown
# ReactLoop Agent

## metadata
- version: 0.1
- loop_type: react

## nodes

### think
- type: generate
- prompt: "{{system_prompt}}\n\nConversation:\n{{history}}\n\nUser: {{user_input}}"
- tools: {{active_tools}}
- output: llm_response

### decide
- type: condition
- condition: "{{llm_response.tool_calls.length}} > 0"
- true: tool_call
- false: respond

### tool_call
- type: tool
- name: "{{llm_response.tool_calls[0].name}}"
- args: "{{llm_response.tool_calls[0].args}}"
- output: tool_result
- on_error: observe_error
- on_step_limit: respond_error

### observe
- type: assign
- history: "{{history}}\nTool result: {{tool_result}}"
- goto: think

### observe_error
- type: assign
- history: "{{history}}\nTool error: {{error}}"
- goto: think

### respond
- type: assign
- response: "{{llm_response.content}}"
- type: end

### respond_error
- type: assign
- response: "Step limit exceeded"
- type: end
```

### 6.3 agenticdsl 备选（PDK .so）

当 HydraForge DSL 的 goto/while 循环支持尚未成熟时，Loop Agent 退化为 PDK `.so` 实现。

```cpp
// pdk/react_loop/src/pdk_entry.cpp
DECLARE_TOOL(loop/react/run, "Execute a React loop", ReadOnly, "plan",
  auto prompt = __pdk_args["prompt"];
  auto ctx = LayeredContext::from_json(__pdk_args["ctx"]);
  
  ReactLoop loop(engine, bus);
  auto result = loop.run(prompt, ctx);
  
  return nlohmann::json{
    {"response", result.message},
    {"steps", result.total_steps}
  };
)
```

Chat Agent 无需区分——`call_tool("loop/run", args)` 对两种实现透明。

---

## 7. Chat Agent 设计

### 7.1 入口 (`main.cpp`)

```cpp
int main(int argc, char* argv[]) {
    bool mock_mode = (argc > 1 && std::string(argv[1]) == "--mock");

    // 1. 解析配置
    auto config = ChatConfig::from_json("config.json");
    if (mock_mode) config.override_provider("mock", "test");

    // 2. 初始化 AgenticOS
    auto engine   = DSLEngine::from_markdown("...");  // chat workflow DAG
    auto bus      = std::make_shared<InMemoryBus>();
    engine->set_interaction_bus(bus);

    // 3. 加载 Provider Agent + 注册 provider config
    PluginLoader loader;
    loader.load_so("pdk/provider_agent/libProviderAgent.so");
    auto& registry = *engine->get_tool_registry();
    registry.call_tool("provider/register", config.to_provider_json());

    // 4. 解析 provider → 设置 LLM
    std::string key = config.agent.provider + "/" + config.agent.model;
    auto llm_cfg_json = registry.call_tool("provider/resolve",
        {{"provider_id", config.agent.provider}, {"model_id", config.agent.model}});
    auto llm_cfg = LLMConfig::from_json(llm_cfg_json);
    auto provider = llm::create_provider_factory()->create(llm_cfg);
    engine->set_llm_provider(std::move(provider));

    // 5. 注册业务工具
    register_fs_tools(registry);
    register_shell_tools(registry);

    // 6. 创建 ChatSession + 启动交互循环
    ChatSession session(engine.get(), bus, config.agent);
    session.subscribe();  // 监听 loop/tool 事件 → 打印到终端

    std::string input;
    while (std::getline(std::cin, input)) {
        auto result = session.chat(input);
        std::cout << result.response << std::endl;
    }
}
```

### 7.2 Session 管理（混合架构：OS 类型 + Session Agent）

**OS 层**保留 ADR-0033 三层类型（`UserSession/TaskSession/SubtaskSession`）作为执行作用域。ChatSession **不定义新的 Session 类**——它通过 Session Agent 管理持久化、压缩、分支。

**Session Agent**（`pdk/chat_session/`）提供 5 个工具：

| 工具 | 输入 | 输出 | 说明 |
|------|------|------|------|
| `session/history` | `session_id` | `[{role, content, timestamp}]` | 获取完整历史 |
| `session/compact` | `session_id, max_tokens` | `compacted_history` | LLM 压缩（可选） |
| `session/branch` | `session_id, parent_message_id` | `new_session_id` | 从指定消息 fork |
| `session/persist` | `session_id, path` | `ok` | JSONL 持久化到文件 |
| `session/search` | `session_id, query` | `[{message_id, score}]` | 历史搜索 |

**ChatSession 精简**（不再管理持久化）：

```cpp
class ChatSession {
public:
    ChatSession(DSLEngine*, shared_ptr<IInteractionBus>, const AgentConfig&);
    
    ChatResult chat(const std::string& user_input);
    void subscribe();

private:
    DSLEngine* engine_;
    shared_ptr<IInteractionBus> bus_;
    AgentConfig config_;
    std::vector<Message> history_;           // 内存中的当前分支（由 Session Agent 提供）
    std::unique_ptr<DSLEngine> loop_engine_;
    std::string session_id_;                 // 持久化标识（传给 Session Agent）

    std::string build_prompt(const std::string& user_input);
    LayeredContext build_loop_context();
    
    // 委托给 Session Agent
    void save_to_session_agent();
    void load_from_session_agent();
};
```

**关键约束**：
- OS 的 `TaskSession.messages` 是 ground truth，不依赖 Session Agent
- Session Agent 崩溃不影响内存中的历史（只丢失未持久化部分）
- fork 路径在 `topo_scheduler.cpp` 同步创建 SubtaskSession，不经过 Session Agent

---

## 8. 事件流

Chat Agent 通过 `IInteractionBus` 订阅 Loop Agent 执行期间的事件：

| 主题 | 发出者 | 载荷 |
|------|--------|------|
| `loop.turn.start` | think 节点 | `{turn, step}` |
| `llm.request` | think 节点 | `{model, prompt_preview}` |
| `llm.response` | think 节点 | `{model, tokens_used, truncated}` |
| `tool.execution.start` | tool_call 节点 | `{name, args_keys}`（不含 args 值） |
| `tool.execution.end` | tool_call 节点 | `{name, duration_ms, ok}` |
| `loop.turn.end` | observe 节点 | `{turn, decision}` |
| `loop.done` | respond 节点 | `{response, total_steps, total_tokens}` |
| `loop.error` | 任意节点 | `{error, step}` |

---

## 9. 错误处理

| 场景 | 策略 |
|------|------|
| Provider Agent `provider/resolve` 失败 | 启动时 fail-fast，打印已注册 provider 列表 |
| LLM API 超时 | `ILLMProvider::generate()` 返回 `Timeout` → Loop Agent 重试（max 3） |
| 工具执行失败 | `ToolResult::error(...)` → Loop Agent observe 错误，继续 think |
| 工具审批拒绝 | `ApprovalHandler` 返回 false → emit `tool.audit.denied`，Loop 观察拒绝理由 |
| 步数超限（>50） | Loop Agent 强制终止，返回 `{success: false, message: "step limit exceeded"}` |
| JSON 配置语法错误 | `ChatConfig::from_json()` validate + 启动时 fail-fast |

---

## 10. 测试策略

| 层 | 测试 | 模式 |
|----|------|------|
| Provider Agent | register/resolve/list 正确性 | 单元测试，Mock ToolRegistry |
| ChatConfig | JSON 解析 + --mock 覆盖 | 单元测试 |
| ChatSession | build_prompt / build_loop_context | 单元测试 |
| Loop Agent | think→decide→tool_call→observe 流程 | 集成测试，`--mock` + MockLLMProvider |
| E2E | 完整 chat 交互 | 集成测试，`--mock` + 固定响应 |
| 真实 LLM | end-to-end 真实 API | 手动触发，CI 不跑 |

**CI**: `cmake --preset tests && ctest -R pdk_chat` 仅跑 mock 模式。

---

## 11. OS/Agent 边界决策（Oracle 分析完成）

| 组件 | 决策 | 理由 |
|------|------|------|
| **Session 管理** | 混合架构 | OS 保留 ADR-0033 类型（fork/join 与 Scheduler 同步耦合）；Session Agent 负责持久化/压缩/分支 |
| **Budget/Cost** | **提升为 Budget Agent** | OS 保留原子 `IBudgetController`（safety gate）；Agent 负责策略/告警/跨 session 聚合 |
| **Approval/Security** | 保留 OS | 安全边界不可绕过；`IExecutionPolicy` 是纯决策函数；`ToolRegistry V2` 注册时验证 |
| **Event Bus** | 保留 OS | Plumbing 组件，多组件传递依赖；Agent-ify 会产生循环依赖 |
| **Provider 管理** | Provider Agent（独立 plugin） | 确认方案 B——新建 `pdk/provider_agent/`，内部调用 model_router 路由 |

### 边界启发式（4 测试法）

| 测试 | YES → Agent | NO → OS |
|------|------------|---------|
| 需要 LLM 推理？ | Session compact, Budget 策略 | try_consume_node（纯原子） |
| 可优雅降级？ | Provider failover, Budget 告警 | 安全审批（失败必须 fail-stop） |
| 有 domain 知识？ | Chat session 格式, 成本分摊规则 | DAG 调度, 节点执行 |
| 被多组件传递依赖？ | — | Bus, Scheduler, Budget atoms |

**4 个测试全 YES → Agent。任一 NO → OS。**

### 新增 Agent 清单（本 demo 涉及的 PDK 插件）

| Agent | 路径 | 实现方式 | 优先级 |
|-------|------|---------|--------|
| Loop Agent | `lib/loop/react.agent.md`（skill）或 `pdk/react_loop/`（agenticdsl 备选） | skill 优先 | P0 |
| Provider Agent | `pdk/provider_agent/`（扩展 model_router） | agenticdsl | P0 |
| Session Agent | `pdk/chat_session/` | agenticdsl | P1（可先内联，后提取） |
| Budget Agent | `pdk/budget_agent/` | agenticdsl | P2（demo 可先跳过） |

### 与 pi-mono / tau 边界对比

| 组件 | pi-mono | tau | HydraForge |
|------|---------|-----|------------|
| Provider | `pi-ai` (library) | `tau_ai` (library) | **Provider Agent** |
| Loop | `pi-agent-core` (library) | `tau_agent` (library) | **Loop Agent** (skill) |
| Session | `AgentSession` (library) | `AgentHarness` (library) | OS types + **Session Agent** |
| Budget | CostConfig (per-call) | — (none) | **Budget Agent** + OS atoms |
| Approval | beforeToolCall hook | beforeToolCall hook | **OS middleware** (不可绕过) |
| Approval 策略 | per-Agent callback | per-Agent callback | `IExecutionPolicy` (Agent 可选) |
