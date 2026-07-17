# PDK Chat Demo 架构设计 v2.0

**日期**: 2026-07-16
**状态**: 🟡 设计中（基于已 ship 的 ADR-0052 ~ ADR-0065）
**作者**: Architecture Working Group
**前置文档**: `docs/superpowers/specs/2026-07-16-pdk-chat-demo-design.md` v1
**关联 ADR**: 0052, 0053, 0054, 0055, 0056, 0057, 0058, 0059, 0060, 0061, 0062, 0063, 0064, 0065

---

## 一、设计目标

`examples/pdk_chat_demo` 是 HydraForge "Agent-as-Plugin" 架构的端到端验证 demo，演示：

1. **HydraForge = AgenticOS**——只提供基础设施，业务逻辑由 Agent Plugin 实现
2. **万物皆 Agent，Agent 皆 Plugin**——每个 Agent 是独立 .so/.dll/.wasm/.agent.md
3. **Agent 四形态可互换**——SKILL.md / DSL / C++ / Wasm 同一契约
4. **应用 = Agent 组合**——Chat 应用由 Chat/Loop/Provider/Session/Budget 等 Agent 编排
5. **完整 Pipeline 验证**——Manifest / Capability / Lifecycle / Schema / OTel / Conformance

---

## 二、架构全景

```
┌────────── 应用层 (Application = Agent 组合) ──────────────────────┐
│                                                                   │
│  Chat Agent (examples/pdk_chat_demo/)                             │
│  ├─ main.cpp: 加载所有 Agent Plugin + 编排交互循环                │
│  ├─ ChatSession: 多轮会话状态（基于 ADR-0033）                   │
│  ├─ 订阅 IInteractionBus 事件 → 终端输出 + OTel 导出            │
│  └─ 通过 IToolRegistry/IInteractionBus 统一调用                   │
│                                                                   │
│  ┌─ Loop Agent ────────────────────────────────────────────────┐  │
│  │ 形态: .agent.md (ReactLoop / PlanExecuteLoop / ForkJoin)    │  │
│  │ 契约: call_tool("loop/run", {prompt, ctx, tools})           │  │
│  ├─ Loop → emit events: loop.turn.*, llm.token, tool.exec.*    │  │
│  ├─ Session Agent ─────────────────────────────────────────────┤  │
│  │ 形态: C++ (libChatSession.so)                              │  │
│  │ 契约: session/history, branch, compact, persist            │  │
│  ├─ Provider Agent ────────────────────────────────────────────┤  │
│  │ 形态: C++ (libProviderAgent.so, 扩展 model_router)        │  │
│  │ 契约: provider/resolve, register, list, health             │  │
│  ├─ Budget Agent ──────────────────────────────────────────────┤  │
│  │ 形态: C++ (libBudgetAgent.so)                              │  │
│  │ 契约: budget/query, set_limit, alerts, cost_breakdown       │  │
│  ├─ Code Review Agent ──────────────────────────────────────────┤  │
│  │ 形态: SKILL.md (skills/code-review/SKILL.md)                │  │
│  │ 契约: code_review/run (隔离执行)                            │  │
│  ├─ FS / Shell Tools ────────────────────────────────────────────┤  │
│  │ 形态: C++ (libFSTools.so, libShellTools.so)                 │  │
│  │ 契约: fs/read, fs/write, shell/exec                         │  │
│  └─────────────────────────────────────────────────────────────┘  │
│                                                                   │
├────────── AgenticOS (HydraForge 基础设施) ───────────────────────┤
│                                                                   │
│  L1 Services:                                                     │
│  ├─ IToolRegistry (ADR-0004 V2 + ADR-0058 schema)                │
│  ├─ IInteractionBus (ADR-0019, InMemoryBus Sprint 12)           │
│  ├─ IBudgetController (ADR-0033 atoms)                           │
│  ├─ ILLMProvider + LLMProviderFactory (ADR-0042 v2)              │
│  ├─ IExecutionPolicy + ToolCoordinator (ADR-0031)               │
│  ├─ CapabilityRegistry (ADR-0054)                                │
│  ├─ ManifestRegistry (ADR-0052)                                  │
│  ├─ AgentLifecycle (ADR-0057: load/init/register/active)         │
│  ├─ SkillInterpreter (ADR-0055: 进程+seccomp 隔离)               │
│  ├─ WasmRuntime (ADR-0056: WAMR + capability host functions)     │
│  ├─ RemoteAgentAdapter (ADR-0059: MCP 协议映射)                 │
│  └─ OpenTelemetryExporter (ADR-0063: W3C trace context)         │
│                                                                   │
│  L0 Runtime:                                                      │
│  ├─ DSLEngine (4 阶段编译执行)                                   │
│  ├─ TopoScheduler / NodeExecutor                                  │
│  ├─ ContextEngine (5-层结构化上下文)                              │
│  └─ UserSession/TaskSession/SubtaskSession (ADR-0033)            │
│                                                                   │
└───────────────────────────────────────────────────────────────────┘
```

---

## 三、文件结构

```
examples/pdk_chat_demo/
├── CMakeLists.txt                # 独立编译目标，链接 AgenticOS + PDK
├── README.md                     # 使用说明
├── config.json                   # 应用配置（agents/编排）
├── main.cpp                      # 入口
├── chat_session.h / .cpp         # ChatSession（编排）
├── event_handler.h / .cpp        # 事件订阅 + 终端渲染 + OTel 导出
└── tests/
    ├── test_chat_session.cpp     # ChatSession 单元测试
    └── test_e2e_mock.cpp         # --mock 模式端到端测试

# Loop Agent DSL 定义（应用层，非 PDK）
lib/loop/
├── react.agent.md                # ReactLoop Agent
├── plan_execute.agent.md         # PlanExecuteLoop Agent（预留）
└── fork_join.agent.md            # ForkJoinLoop Agent（预留）

# 各 Agent Plugin（独立 PDK 项目）
pdk/loop_agent/                   # Loop Agent（.agent.md）  ← 与 lib/loop/ 配合
├── CMakeLists.txt
├── pdk_manifest.json
├── include/
└── src/pdk_entry.cpp             # 调用 lib/loop/react.agent.md

pdk/provider_agent/               # Provider Agent（model_router 扩展）
├── CMakeLists.txt
├── pdk_manifest.json
├── include/provider_agent.h
└── src/
    ├── pdk_entry.cpp
    ├── credential_store.cpp
    └── provider_resolve.cpp

pdk/session_agent/                # Session Agent（C++）
├── CMakeLists.txt
├── pdk_manifest.json
└── src/
    ├── pdk_entry.cpp
    └── session_store.cpp

pdk/budget_agent/                 # Budget Agent（C++）
├── CMakeLists.txt
├── pdk_manifest.json
└── src/pdk_entry.cpp

pdk/fs_tools/                     # FS Tools（C++）
pdk/shell_tools/                  # Shell Tools（C++）

skills/code_review/               # Code Review Skill（SKILL.md）
├── SKILL.md
└── scripts/
```

---

## 四、JSON 配置 (`config.json`)

```json
{
  "schema_version": "1.0",
  "app_id": "pdk_chat_demo",
  
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
      "models": { "test": { "model": "mock-llm-v1" } }
    }
  },

  "agent": {
    "loop_type": "react",
    "provider": "mock",
    "model": "test",
    "system_prompt": "You are a helpful coding assistant with access to tools.",
    "tools": ["fs/read", "fs/write", "shell/exec", "code_review/run"],
    "max_steps": 50,
    "timeout_ms": 300000,
    "budget_limit_usd": 1.0
  },

  "plugins": [
    {
      "id": "chat.loop",
      "path": "build/pdk/loop_agent/libLoopAgent.so",
      "lifecycle": "lazy",
      "activation_events": ["onTool:loop/run"]
    },
    {
      "id": "infra.provider",
      "path": "build/pdk/provider_agent/libProviderAgent.so"
    },
    {
      "id": "infra.session",
      "path": "build/pdk/session_agent/libSessionAgent.so"
    },
    {
      "id": "infra.budget",
      "path": "build/pdk/budget_agent/libBudgetAgent.so"
    },
    {
      "id": "tool.fs",
      "path": "build/pdk/fs_tools/libFSTools.so"
    },
    {
      "id": "tool.shell",
      "path": "build/pdk/shell_tools/libShellTools.so"
    },
    {
      "id": "skill.code_review",
      "path": "skills/code-review/SKILL.md",
      "type": "skill",
      "requires_isolation": true
    }
  ],

  "orchestration": {
    "model": "event_driven",
    "entry": "user_input",
    "event_topics": [
      "user.input",
      "loop.turn.start",
      "loop.turn.end",
      "llm.token",
      "tool.execution.start",
      "tool.execution.end",
      "loop.done",
      "loop.error",
      "budget.check.ok",
      "budget.check.failed"
    ]
  },

  "observability": {
    "otel_enabled": true,
    "endpoint": "http://localhost:4318",
    "sample_rate": 1.0,
    "export_format": "otlp+http"
  },

  "session": {
    "persist_dir": "~/.hydraforge/sessions/",
    "compact_threshold_tokens": 8000,
    "branch_on_user_request": true
  },

  "safety": {
    "default_trust_level": "medium",
    "high_risk_tools_require_approval": true,
    "budget_alerts": [
      {"threshold": 0.5, "severity": "warning"},
      {"threshold": 0.9, "severity": "critical"}
    ]
  }
}
```

---

## 五、Agent 详细设计

### 5.1 Chat Agent (编排器, `main.cpp`)

**职责**：
1. 解析 `config.json`，加载所有 Plugin
2. 初始化 AgenticOS（DSLEngine + ToolRegistry + Bus + Budget + OTel）
3. 进入交互循环（stdin → 事件 → terminal + OTel）

**关键代码骨架**：
```cpp
int main(int argc, char* argv[]) {
    bool mock_mode = (argc > 1 && std::string(argv[1]) == "--mock");
    
    // 1. 解析配置
    auto config = ChatConfig::from_json("config.json");
    if (mock_mode) config.override_provider("mock", "test");
    
    // 2. 初始化 AgenticOS
    auto engine = std::make_unique<DSLEngine>();
    auto tool_registry = std::make_unique<ToolRegistry>();
    auto bus = std::make_shared<InMemoryBus>();
    auto budget = std::make_unique<BudgetController>(config.agent.budget_limit_usd);
    auto otel = std::make_unique<OpenTelemetryExporter>(config.observability);
    
    engine->set_tool_registry(tool_registry.get());
    engine->set_interaction_bus(bus);
    
    // 3. 加载所有 Plugin
    PluginLoader loader;
    for (auto& plugin_cfg : config.plugins) {
        loader.load_so(plugin_cfg.path);
        // 自动调用 pdk_register_tools + pdk_register_agent
    }
    
    // 4. Provider Agent 注册 provider configs
    tool_registry->call_tool("provider/register", config.providers);
    
    // 5. 解析默认 provider → 设置 LLM
    auto llm_cfg_json = tool_registry->call_tool("provider/resolve", {
        {"provider_id", config.agent.provider},
        {"model_id", config.agent.model}
    });
    auto llm = LLMProviderFactory::create(LLMConfig::from_json(llm_cfg_json));
    engine->set_llm_provider(std::move(llm));
    
    // 6. 订阅事件 → 终端输出 + OTel
    EventHandler handler(bus, otel.get(), &std::cout);
    
    // 7. ChatSession 初始化
    ChatSession session(engine.get(), bus, tool_registry.get(), 
                        config.agent, config.session);
    
    // 8. 交互循环
    std::string input;
    while (std::getline(std::cin, input)) {
        auto result = session.chat(input);
        std::cout << result.response << std::endl;
    }
    
    // 9. 优雅退出
    loader.unload_all();
    otel->flush();
    return 0;
}
```

### 5.2 Loop Agent (`.agent.md`)

**形态**：DSL（首选）
**文件**：`lib/loop/react.agent.md`

```markdown
# ReactLoop Agent

## metadata
- version: 0.1
- loop_type: react
- max_steps: 50
- budget_inheritance: strict

## nodes

### start
- type: start
- next: [/loop/think]

### think
- type: generate
- prompt: |
    {{system_prompt}}
    
    Conversation:
    {{history}}
    
    User: {{user_input}}
- tools: {{active_tools}}
- output: llm_response
- next: [/loop/decide]

### decide
- type: condition
- condition: "{{llm_response.tool_calls.length}} > 0"
- true: /loop/tool_call
- false: /loop/respond

### tool_call
- type: tool
- name: "{{llm_response.tool_calls[0].name}}"
- args: "{{llm_response.tool_calls[0].args}}"
- output: tool_result
- next: [/loop/observe]

### observe
- type: assign
- history: "{{history}}\nTool: {{tool_result}}"
- next: /loop/think

### respond
- type: assign
- response: "{{llm_response.content}}"
- next: /loop/end

### end
- type: end
- output: "{{response}}"
```

**Plugin entry**（`pdk/loop_agent/src/pdk_entry.cpp`）：
```cpp
extern "C" const PluginInfo pdk_plugin_info = {
    .name = "loop_agent",
    .version = "0.1.0",
    .abi_version = 2
};

extern "C" void pdk_register_tools(IToolRegistry& registry) {
    registry.register_tool_function(
        "loop/run",
        ToolMetadata{
            .category = ToolCategory::Execute,
            .allowed_layers = {LayerProfile::Workflow},
            .input_schema = {{"type", "object"}, ...},
            .output_schema = {{"type", "object"}, ...}
        },
        [](const nlohmann::json& args) -> nlohmann::json {
            auto loop_type = args.value("loop_type", "react");
            auto agent_file = "lib/loop/" + loop_type + ".agent.md";
            auto sub_engine = DSLEngine::from_markdown_file(agent_file);
            auto ctx = LayeredContext::from_json(args);
            auto result = sub_engine->run(ctx);
            return result.to_json();
        }
    );
}

extern "C" void pdk_register_agent(AgentDescriptor& desc) {
    desc.id = "chat.loop";
    desc.forms = {AgentForm::DSL};
    desc.entry_tool = "loop/run";
    desc.provided_tools = {"loop/run", "loop/plan_execute", "loop/fork_join"};
    desc.capabilities = {"react_loop", "plan_execute_loop", "fork_join_loop"};
    desc.interface_versions = {"IAgentV1"};
}
```

### 5.3 Provider Agent (C++ 扩展 model_router)

**形态**：C++（凭据管理需安全内存）
**关键能力**：
- `provider/register(config_json)` — 注册 provider configs
- `provider/resolve(provider_id, model_id)` — 解析 + 查凭据 → LLMConfig
- `provider/list()` — 列出所有 provider
- `provider/health()` — 健康检查

**Plugin entry** 草图：
```cpp
extern "C" void pdk_register_tools(IToolRegistry& registry) {
    registry.register_tool_function(
        "provider/resolve",
        ToolMetadata{
            .category = ToolCategory::ReadOnly,
            .allowed_layers = {LayerProfile::Workflow}
        },
        [](const nlohmann::json& args) -> nlohmann::json {
            auto provider_id = args.at("provider_id").get<std::string>();
            auto model_id = args.at("model_id").get<std::string>();
            return credential_store::resolve(provider_id, model_id).to_json();
        }
    );
    // ... register, list, health
}
```

### 5.4 Session Agent (C++, 混合架构)

**形态**：Hybrid（C++ 持久化 + DSL compact）
**关键能力**：
- `session/history(session_id)` — 获取历史
- `session/compact(session_id, max_tokens)` — LLM 压缩
- `session/branch(session_id, msg_id)` — Fork
- `session/persist(session_id, path)` — 持久化到 JSONL
- `session/search(session_id, query)` — 历史搜索

### 5.5 Budget Agent (C++)

**形态**：C++（预算策略 + 跨 session 聚合）
**关键能力**：
- `budget/query()` — 当前余额
- `budget/set_limit(amount_usd)` — 修改限额
- `budget/alerts(threshold)` — 告警订阅
- `budget/cost_breakdown(session_id)` — 成本拆解

### 5.6 Code Review Skill (SKILL.md)

**形态**：SKILL.md（隔离执行）
**文件**：`skills/code-review/SKILL.md`

```markdown
---
name: code-review
description: 审查代码的安全漏洞、逻辑错误、可维护性问题
category: axis3-review
capabilities: [code_review, static_analysis]
input_schema:
  type: object
  properties:
    code: {type: string}
    language: {type: string, enum: [cpp, python, rust]}
    severity: {type: string, default: medium}
  required: [code, language]
output_schema:
  type: object
  properties:
    issues: {type: array}
    summary: {type: string}
requires_isolation: true
timeout_ms: 30000
budget_limit_usd: 0.05
trust_level: high
---

# Code Review Agent

## Process
1. 通读代码理解整体结构
2. 按以下维度检查：
   - 安全风险（注入、XSS、缓冲区溢出）
   - 逻辑错误（空指针、越界、竞态）
   - 可维护性（命名、复杂度、重复）
3. 输出 JSON 审查报告

## Output
每个 issue 包含 line/severity/message/suggestion。

## Hard Gate
- 必须返回非空 issues 列表
- 严重度必须分级
```

---

## 六、事件流

```
用户输入
   │
   ▼
Chat Agent (main.cpp)
   │
   ├─ emit "user.input" + span("user.input")
   │
   ▼
Loop Agent 触发（via call_tool "loop/run"）
   │
   ├─ emit "loop.turn.start" (turn=1, step=1)
   ├─ emit "llm.request"
   │   └─ Provider Agent 触发 (call_tool "provider/resolve")
   │       └─ emit "llm.response" (含 tokens_used)
   ├─ emit "loop.decision" (decide: tool_call)
   ├─ emit "tool.execution.start" (fs/read)
   ├─ (FS Agent 执行) emit "tool.execution.end" (ok=true, duration_ms=12)
   ├─ emit "loop.turn.end" (decision="observe")
   │
   ├─ (loop back to think) ... [迭代 N 步]
   │
   ├─ emit "loop.done" (response, total_steps=3, total_tokens=1500)
   │
   ▼
Chat Agent 接收结果
   │
   ├─ Session Agent 保存历史 (call_tool "session/persist")
   ├─ Budget Agent 累计消耗 (call_tool "budget/query")
   │
   ▼
Terminal 输出 + OTel export
```

---

## 七、错误处理

| 场景 | 处理策略 |
|------|---------|
| Provider Agent `provider/resolve` 失败 | 启动时 fail-fast，打印已注册 provider 列表 |
| LLM API 超时 | `ILLMProvider::generate()` 返回 Timeout → Loop Agent 重试（max 3）|
| 工具执行失败 | `ToolResult::error(...)` → Loop Agent observe 错误，继续 think |
| 工具审批拒绝 | `ApprovalHandler` 返回 false → emit `tool.audit.denied`，Loop 观察拒绝理由 |
| 步数超限（>50） | Loop Agent 强制终止，返回 `{success: false, message: "step limit exceeded"}` |
| 预算耗尽 | Loop Agent 立即终止，emit `loop.done.error`，UI 提示 |
| Skill 隔离失败（SKILL.md） | SkillInterpreter fork 子进程 → seccomp + rlimit → SIGKILL 超时 |
| JSON 配置语法错误 | `ChatConfig::from_json()` validate + 启动时 fail-fast |
| Wasm Plugin 加载失败 | PluginLoader 检查 ABI version + capability，失败时 fallback |

---

## 八、测试策略

### 8.1 测试矩阵

| 层 | 测试 | 模式 | 工具 |
|----|------|------|------|
| **Unit** | ChatConfig JSON 解析 | --mock | Catch2 |
| **Unit** | ChatSession 状态机 | --mock | Catch2 |
| **Unit** | EventHandler 订阅 | --mock | Catch2 |
| **Unit** | 各 Agent Plugin 单独 | --mock | Catch2 |
| **Integration** | Chat Agent → Loop → Tools | --mock | Catch2 + e2e |
| **Integration** | Skill 隔离（SKILL.md） | --mock | Catch2 + seccomp |
| **E2E** | 完整 chat 交互 | --mock | ctest |
| **Real LLM** | end-to-end 真实 API | manual | (CI skip) |
| **Conformance** | 6 个 Plugin 通过 Level 1+2 | --mock | hf conformance check |
| **OTel** | trace 导出到 mock collector | --mock | OpenTelemetry SDK |

### 8.2 Mock 模式 CI

```bash
cmake --preset tests && ctest -R pdk_chat
# 期望：所有测试在 --mock 模式下通过
```

### 8.3 行为指纹测试

```cpp
TEST_CASE("chat demo 行为指纹") {
    auto cases = load_test_cases("config/test_cases.json");
    auto traces_a = run_e2e(cases, /*cold_start=*/true);
    auto traces_b = run_e2e(cases, /*cold_start=*/true);
    
    // 三值 verdict (ADR-0061-02)
    auto verdict = agent_assay::evaluate(traces_a, traces_b);
    REQUIRE(verdict == Verdict::Pass);
}
```

---

## 九、可观测性

### 9.1 OTel Spans

| Span | 何时 | 关键属性 |
|------|------|---------|
| `chat.run` | ChatSession::chat() | session_id, agent_id, loop_type |
| `agent.run` | 每个 Agent 执行 | agent_id, form, version |
| `tool.call` | call_tool | tool_name, args_keys, duration_ms, ok |
| `llm.generate` | ILLMProvider | model, prompt_tokens, completion_tokens, cached |
| `bus.emit` | IInteractionBus::emit | topic, subscriber_count |
| `budget.consume` | IBudgetController | amount, remaining_usd, scope |

### 9.2 终端输出

```
$ ./pdk_chat_demo --mock

[10:23:45] user.input: "Write a hello world in C++"
[10:23:45] loop.turn.start: turn=1, step=1
[10:23:45] llm.request: model=mock-llm-v1, prompt_tokens=42
[10:23:46] llm.response: completion_tokens=85, duration_ms=210
[10:23:46] loop.decision: tool_call (shell/exec)
[10:23:46] tool.execution.start: shell/exec (args_keys=[command])
[10:23:47] tool.execution.end: ok=true, duration_ms=890
[10:23:47] loop.turn.end: decision=observe
[10:23:47] loop.turn.start: turn=2, step=2
[10:23:48] llm.response: completion_tokens=42
[10:23:48] loop.decision: respond
[10:23:48] loop.done: response="Here's the C++ code...", total_steps=2, total_tokens=127

Assistant: Here's the C++ code...

```cpp
#include <iostream>
int main() { std::cout << "Hello, World!" << std::endl; return 0; }
```

[10:23:48] session.persist: ok=true, path=~/.hydraforge/sessions/sess_abc.jsonl
[10:23:48] budget.query: remaining_usd=0.999, total_spent=0.001

User> 
```

### 9.3 OTel Export

```bash
# 启动 OTel collector
docker run -p 4318:4318 otel/opentelemetry-collector-contrib

# pdk_chat_demo 自动导出
$ ./pdk_chat_demo --mock
# trace 自动发到 http://localhost:4318/v1/traces
```

---

## 十、与 ADR 的完整对应关系

| 架构元素 | ADR |
|---------|-----|
| Plugin 加载 + pdk_register_tools | ADR-0021, 0022, 0051 |
| pdk_manifest.json | **ADR-0052** |
| AgentDescriptor + pdk_register_agent | **ADR-0053** |
| Capability-based discovery | **ADR-0054** |
| Skill 隔离（SKILL.md） | **ADR-0055** |
| Wasm runtime（v2 扩展） | **ADR-0056** |
| Plugin lifecycle（lazy/activation_events） | **ADR-0057** |
| Tool schema 校验 | **ADR-0058** |
| 跨进程协议（v2 扩展） | **ADR-0059** |
| 6 种协作模式 + 透明路由 | **ADR-0060** |
| Skill 进化（SKILL.md 对齐 Anthropic） | **ADR-0061** |
| Marketplace 包（v2） | **ADR-0062** |
| OTel Trace（v2 集成） | **ADR-0063** |
| Conformance 测试 | **ADR-0064** |
| Python Wasm Plugin（v2） | **ADR-0065** |

---

## 十一、v1 实施范围与 v2 扩展

### v1（必做）
- ✅ Chat Agent + Loop Agent + Provider Agent + Session Agent + Budget Agent
- ✅ Code Review Skill（SKILL.md）
- ✅ FS / Shell Tools
- ✅ MockLLMProvider + 端到端 --mock 模式
- ✅ Manifest + Lifecycle + Capability + Schema 校验
- ✅ 事件流 + 终端输出
- ✅ Session 持久化（JSONL）

### v2 扩展
- 🔵 Marketplace + .hfpkg 打包（ADR-0062）
- 🔵 Conformance Test Suite（ADR-0064）
- 🔵 OTel 集成（ADR-0063）
- 🔵 Wasm 编译的 Code Review Agent（ADR-0061-05）
- 🔵 Python Code Review Plugin（ADR-0065）
- 🔵 PlanExecuteLoop / ForkJoinLoop DSL（ADR-0021 §3.2）
- 🔵 真实 LLM 集成测试（手工触发）

### Phase B/C（远期）
- 🔮 Agent Marketplace 上线
- 🔮 Lambda 一致性检查（ADR-0061-10）
- 🔮 PASTE-style 推测执行（ADR-0061-07）

---

## 十二、与 pi-mono / tau 的对应

| pi-mono | tau | HydraForge（pdk_chat_demo） |
|---------|-----|---------------------------|
| `pi-ai` (Provider) | `tau_ai` | **Provider Agent**（agenticdsl C++）|
| `pi-agent-core` (Loop) | `tau_agent` | **Loop Agent**（skill `.agent.md`）|
| `pi-coding-agent` (App) | `tau_coding` (`AgentHarness`) | **Chat Agent**（编排器，main.cpp）|
| (none) | (none) | **Session / Budget Agent**（HydraForge 特有）|

---

## 十三、关键文件大小估计

| 文件 | 估计行数 |
|------|---------|
| `main.cpp` | ~120 |
| `chat_session.h/cpp` | ~200 |
| `event_handler.h/cpp` | ~80 |
| `config.json` | ~80 |
| `lib/loop/react.agent.md` | ~70 |
| `pdk/loop_agent/src/pdk_entry.cpp` | ~60 |
| `pdk/provider_agent/src/*` | ~250 |
| `pdk/session_agent/src/*` | ~200 |
| `pdk/budget_agent/src/*` | ~150 |
| `skills/code-review/SKILL.md` | ~40 |
| `pdk/fs_tools/src/*` | ~120 |
| `pdk/shell_tools/src/*` | ~150 |
| **总计** | **~1520 行** |

---

## 十四、与现有 examples/ 目录的关系

```
examples/
├── agent_basic/                 # 现有：纯手工，无脚手架
│   ├── main.cpp                  # 64 行
│   └── workflow.agent.md
├── agent_simple/                # 现有：MockLLMProvider 单轮
├── agent_loop/                   # 现有：MockLLMProvider 多轮
├── phase1_model_router_plugin/   # 现有：PluginLoader 演示
├── phase1_plugin_demo/           # 现有：PluginLoader 演示
└── pdk_chat_demo/                # 【新】完整 Agent-as-Plugin 演示
    └── ...                       # (本设计)
```

**pdk_chat_demo 不替代现有示例**，而是基于它们的最佳实践的综合验证。

---

## 十五、验证检查清单（v1 完成定义）

- [ ] `cmake --preset tests && ctest -R pdk_chat` 全绿（包含 mock 模式）
- [ ] 6 个 Plugin 全部加载成功，工具可调用
- [ ] 事件流 + 终端输出符合 §9.2 示例
- [ ] MockLLMProvider + 至少 3 个测试用例（"hello world" / "file read" / "shell exec"）
- [ ] SKILL.md Code Review Agent 通过 `requires_isolation` 验证
- [ ] Session 持久化到 `~/.hydraforge/sessions/` 成功
- [ ] Budget 累计 + 告警正确触发
- [ ] Schema 校验拒绝错误输入（test case）
- [ ] Lifecycle 状态转换正确（active/inactive/loaded）
- [ ] Conformance Level 1 通过（manifest 校验）

---

**下一步**：基于本设计开始 `examples/pdk_chat_demo` 的实施，或先讨论具体的实施细节（如 SkillCompiler 集成、Loop Agent 测试用例、Provider Agent 凭据管理等）？