# Design: pdk_chat_demo 运行时修复

**关联**: `proposal.md`

## 1. 设计原则

### 1.1 零核心变更
本 change 不修改任何 HydraForge 核心代码（`src/core/`, `src/common/`, `include/`）。所有修复在 PDK plugin + demo 代码层面完成。

### 1.2 参考正确模式
5 个 ToolMetadata 修复参考 `budget/set_limit`（`pdk/budget_agent/src/pdk_entry.cpp:79-86`）—— 这是项目内已知正确的危险工具配置。

### 1.3 保持向后兼容
所有修改不改变 handler 签名、不改变插件对外契约、不改变 config.json 已有字段。

---

## 2. ToolMetadata ApprovalPolicy 修复模式

### 2.1 校验规则（参考）

**`src/common/tools/registry.cpp:102-106`**：
```cpp
bool no_plan_or_agent = !meta.approval.requires_approval_in_plan
                      && !meta.approval.requires_approval_in_agent;
bool dangerous = (meta.category == ToolCategory::Execute
               || meta.category == ToolCategory::Network
               || meta.category == ToolCategory::StateModify);
if (no_plan_or_agent && dangerous) {
    throw std::invalid_argument(
        "ToolRegistry: tool '" + name + "' has dangerous category but no plan or agent approval");
}
```

**危险类目**：`Execute` / `Network` / `StateModify`  
**安全类目**：`ReadOnly` / `WriteFile`（不需要审批）

### 2.2 修复矩阵

| 工具 | Category | 决策 | force_approval_always |
|---|---|---|:---:|
| provider/register | StateModify | plan=T, agent=T | ✅ T（敏感配置变更） |
| session/branch | Execute | plan=T, agent=T | ❌ F（非破坏性） |
| session/compact | Execute | plan=T, agent=T | ✅ T（不可逆消息丢失） |
| session/persist | Execute | plan=T, agent=T | ✅ T（磁盘写入） |
| budget/alerts | Execute | plan=T, agent=T | ❌ F（可逆订阅） |

### 2.3 BEFORE/AFTER 示例（provider/register）

```cpp
// BEFORE (pdk/provider_agent/src/pdk_entry.cpp:57-64)
.category = ::agenticdsl::ToolCategory::StateModify,
.approval = ::agenticdsl::ApprovalPolicy{
    .requires_approval_in_plan  = false,
    .requires_approval_in_agent = false,
    .requires_approval_in_yolo  = false,
    .force_approval_always      = false
},

// AFTER
.category = ::agenticdsl::ToolCategory::StateModify,
.approval = ::agenticdsl::ApprovalPolicy{
    .requires_approval_in_plan  = true,
    .requires_approval_in_agent = true,
    .requires_approval_in_yolo  = false,
    .force_approval_always      = true    // 敏感状态变更强制审批
},
```

---

## 3. Loop Agent 真实 DSL 执行

### 3.1 当前状态

`pdk/loop_agent/src/pdk_entry.cpp:92-128` 是 stub，注释明确：
> "This is a mock response... For the demo --mock mode, we return a canned response to avoid the architectural limitation that DSLEngine::from_markdown creates an isolated sub-engine whose LLM provider cannot inherit configuration from the parent engine."

### 3.2 架构选择

采用 **Option B（简化版）**：让 `loop/run` 真正调用 `DSLEngine::from_markdown` + `run(LayeredContext)`，但**接受 sub-engine 无独立 LLM provider 的限制**：
- ✅ 可执行 DSL 节点（assign / condition / tool_call / end）
- ❌ 无法调用 LLM provider（sub-engine 无 parent 继承）
- ✅ emit 事件可工作（如果用 main bus）

**为什么不选 Option A**：Option A 需要修改 main.cpp 全局 API（暴露 engine 指针给 plugin loader），这是 OpenSpec `2026-07-17-pdk-chat-demo-buildable` 已明确承诺的**零核心变更**的反例。

### 3.3 实现（pdk/loop_agent/src/pdk_entry.cpp）

```cpp
// 新版 handler（替换 line 92-128 的 stub）
[&capture](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
    std::string loop_type   = str_arg(args, "loop_type", "react");
    std::string user_prompt = str_arg(args, "prompt");
    std::string system_prompt = str_arg(args, "system_prompt", "");

    // 1. 选择 Loop agent DSL 文件
    std::string agent_file = HYDRAFORGE_LOOP_DIR + "/" + loop_type + ".agent.md";
    std::ifstream f(agent_file);
    if (!f.is_open()) {
        // 优雅降级到 mock 响应
        return fallback_mock_response(loop_type, user_prompt,
            "loop_agent: cannot open " + agent_file);
    }

    // 2. 读取 markdown 内容
    std::stringstream ss;
    ss << f.rdbuf();
    std::string markdown = ss.str();

    // 3. 创建 sub-engine (无 LLM provider——ADR-0019 限制)
    auto sub_engine = ::agenticdsl::DSLEngine::from_markdown(markdown);

    // 4. 构造 LayeredContext (Sprint 20 推荐入口)
    ::agenticdsl::LayeredContext ctx;
    if (!system_prompt.empty()) {
        ctx.system["prompt"] = system_prompt;
    }
    ctx.working["user_input"] = user_prompt;
    ctx.working["loop_type"]   = loop_type;

    // 透传 history
    try {
        ctx.working["history"] = nlohmann::json::parse(str_arg(args, "history", "[]"));
    } catch (...) {
        ctx.working["history"] = nlohmann::json::array();
    }

    // 5. 执行 DSL
    auto result = sub_engine->run(ctx);

    // 6. 提取输出 (ExecutionResult.final_context 是 flat Context)
    nlohmann::json output;
    output["success"]  = result.success;
    output["response"] = result.final_context.count("response")
        ? result.final_context.at("response").get<std::string>()
        : user_prompt;
    output["steps"]    = result.final_context.count("steps")
        ? result.final_context.at("steps").get<int>()
        : 1;
    output["tokens_used"] = result.final_context.count("tokens_used")
        ? result.final_context.at("tokens_used").get<int>()
        : 0;
    output["cost_usd"]    = 0.001;  // 估算（无真实 LLM 调用）
    output["error"]       = result.success ? "" : result.message;
    return output;
}
```

### 3.4 失败模式

| 场景 | 行为 |
|---|---|
| `lib/loop/react.agent.md` 不存在 | 返回 fallback mock 响应 + error message |
| DSL 解析失败 | 抛出 `std::runtime_error`（向上传递） |
| DSL 包含 LLM 调用节点 | sub-engine 无 LLM provider，跳过该节点 |

---

## 4. 云端 LLM 接入

### 4.1 现状

`LLMProviderFactory` 已支持 OpenAI 兼容协议（`src/common/llm/llm_provider_factory.cpp:45-66`）：

```cpp
if (backend == "openai" || backend == "anthropic" ||
    backend == "deepseek" || backend == "qwen" ||
    backend == "moonshot" || backend == "custom") {
    return cloud_factory->create(config);  // → CloudLLMAdapter
}
```

`CloudLLMAdapter` 已在 `src/common/llm/cloud_adapter.{h,cpp}` 实现（OpenAI 兼容 + Bearer 认证 + 指数退避重试）。

### 4.2 config.json 配置

```json
{
  "providers": {
    "baidu-deepseek": {
      "api_key_env": "QIANFAN_API_KEY",
      "api_url": "https://qianfan.baidubce.com/v2/coding",
      "api_endpoint": "/chat/completions",
      "models": {
        "deepseek-v4-pro": { "model": "deepseek-v4-pro", "max_tokens": 4096, "temperature": 0.7 },
        "glm-5":          { "model": "glm-5",          "max_tokens": 4096, "temperature": 0.7 },
        "minimax-m2.5":    { "model": "minimax-m2.5",    "max_tokens": 4096, "temperature": 0.7 }
      }
    }
  },
  "agent": {
    "provider": "baidu-deepseek",
    "model": "deepseek-v4-pro"
  }
}
```

### 4.3 main.cpp 修改（line 132-144）

```cpp
// BEFORE (依赖 provider/resolve 工具，已被配置驱动)
auto llm_cfg_json = tool_registry->call_tool("provider/resolve", {...});
auto llm_cfg = agenticdsl::LLMConfig::from_json(llm_cfg_json);
llm_provider = agenticdsl::LLMProviderFactory::create(llm_cfg);

// AFTER (直接构造 LLMConfig + 工厂)
::agenticdsl::LLMConfig llm_cfg;
llm_cfg.provider     = config.agent.provider;          // "baidu-deepseek"
llm_cfg.api_key_env  = "QIANFAN_API_KEY";
llm_cfg.api_url      = "https://qianfan.baidubce.com/v2/coding";
llm_cfg.api_endpoint = "/chat/completions";
llm_cfg.model        = config.agent.model;
llm_cfg.max_tokens   = 4096;
llm_cfg.temperature  = 0.7;

auto factory = ::agenticdsl::llm::create_provider_factory();
llm_provider = factory->create(llm_cfg);
```

### 4.4 端点格式验证

⚠️ **需实测验证**：百炼 v2/coding 端点（`POST /chat/completions`）是否完全 OpenAI 兼容。如果不兼容：
- 检查响应格式（应为 OpenAI Chat Completion 标准）
- 检查请求头（需 `Authorization: Bearer <api_key>`）
- 检查模型 ID（`deepseek-v4-pro` 在百炼侧的实际名称）

---

## 5. SKILL.md 行为诚实化

### 5.1 main.cpp 修改（line 105-108）

```cpp
// BEFORE
} else if (plugin_cfg.type == "skill") {
    std::cout << "[main] Skill registered (mock): " << plugin_cfg.id << std::endl;
}

// AFTER
} else if (plugin_cfg.type == "skill") {
    std::cout << "[main] Skill registered (mock-only, requires SkillInterpreter ADR-0055): "
              << plugin_cfg.id << std::endl;
}
```

### 5.2 config.json 注释

```json
{
  "plugins": [{
    "id": "skill.code_review",
    "path": "../skills/code-review/SKILL.md",
    "type": "skill",
    "_comment": "SKILL.md execution requires SkillInterpreter (ADR-0055, not yet implemented); registered as mock-only in v1"
  }]
}
```

---

## 6. 验证计划

### 6.1 构建验证

```bash
cd /workspace/project/HydraForge/build
cmake --build . --target LoopAgent ProviderAgent SessionAgent BudgetAgent FSTools ShellTools pdk_chat_demo 2>&1 | tail -10
# 期望：6 plugins + demo + tests 全部编译成功，零错误
```

### 6.2 Mock 模式端到端

```bash
cd /workspace/project/HydraForge/build/examples/pdk_chat_demo
echo "write a hello world in C++" | timeout 10 ./pdk_chat_demo --mock 2>&1 | grep -E "Loaded plugin|Failed|Assistant|loop\.done"

# 期望输出（6 个 Loaded，0 个 Failed，1 个 Assistant 响应，1 个 loop.done 事件）:
# [main] Loaded plugin: chat.loop from .../libLoopAgent.so
# [main] Loaded plugin: infra.provider from .../libProviderAgent.so
# [main] Loaded plugin: infra.session from .../libSessionAgent.so
# [main] Loaded plugin: infra.budget from .../libBudgetAgent.so
# [main] Loaded plugin: tool.fs from .../libFSTools.so
# [main] Loaded plugin: tool.shell from .../libShellTools.so
# Assistant: [loop_agent/react] Processed: "write a hello world in C++"
#   [steps=1, tokens=0, cost=$0.001]
# [loop.done: total_steps=1, total_tokens=0]
```

### 6.3 真实 LLM 模式

```bash
export QIANFAN_API_KEY=sk-xxxxxxx
cd /workspace/project/HydraForge/build/examples/pdk_chat_demo
echo "用 C++ 写一个 hello world" | timeout 30 ./pdk_chat_demo 2>&1 | tail -30

# 期望：6 plugins 加载 + 真实 LLM 响应（来自 deepseek-v4-pro）
```

### 6.4 零回归

```bash
cd /workspace/project/HydraForge/build
ctest -j$(nproc) --output-on-failure 2>&1 | tail -10
# 期望：80/80 PASS
```

---

## 7. 风险登记

| # | 风险 | 严重度 | 缓解 |
|:---:|---|:---:|---|
| **R1** | ToolMetadata 修复不彻底（漏改某工具） | 🟢 LOW | 5 个工具明确列表 + 编译失败即发现 |
| **R2** | Loop Agent sub-engine 行为不可预测（DSL 含 LLM 节点） | 🟠 MED | 失败时 fallback 到 mock 响应（不抛异常） |
| **R3** | 百炼端点与 CloudLLMAdapter 不完全兼容 | 🟠 MED | 失败时 fallback 到 MockLLMProvider + 错误日志 |
| **R4** | 修改影响其他 80 个 ctest 测试 | 🟢 LOW | 仅修改 plugin/demo 文件，核心不变 |
| **R5** | ApprovalPolicy 修改改变了运行时审批行为 | 🟡 LOW | 在 Plan/Agent 模式下第一次调用会被审批（但 demo --mock 不触发） |

---

## 8. 后续 Phase 6 工作

本 change 不解决但需留 OpenSpec 跟踪：
- ADR-0019 follow-up: sub-engine LLM provider 继承机制
- ADR-0055 SkillInterpreter 实施（fork+exec/seccomp）
- ADR-0058 Schema runtime validation（input_schema/output_schema 强制执行）
