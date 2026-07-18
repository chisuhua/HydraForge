# pdk_chat_demo 运行时修复 — 端到端可工作

**STATUS**: 🔍 Proposed
**日期**: 2026-07-18
**关联**: `openspec/changes/2026-07-17-pdk-chat-demo-buildable/`（已归档）
**关联**: `examples/pdk_chat_demo/BUILD_VERIFICATION_REPORT.md`（Phase A 报告）

## Why

`pdk_chat_demo v1 buildable` 已 ship 并归档（80/80 ctest PASS）。但运行时实测暴露 3 个端到端 gap，demo 实际**部分可用**：

### Gap 1: 3/6 plugins 加载失败（🔴 HIGH）

实测运行日志（`build/examples/pdk_chat_demo/pdk_chat_demo --mock`）：
```
✅ chat.loop v0.1.0
✅ tool.fs v0.1.0
✅ tool.shell v0.1.0
❌ infra.provider: ToolRegistry: tool 'provider/register' has dangerous category but no plan or agent approval
❌ infra.session:  ToolRegistry: tool 'session/branch' has dangerous category but no plan or agent approval
❌ infra.session:  ToolRegistry: tool 'session/compact' has dangerous category but no plan or agent approval
❌ infra.session:  ToolRegistry: tool 'session/persist' has dangerous category but no plan or agent approval
❌ infra.budget:  ToolRegistry: tool 'budget/alerts' has dangerous category but no plan or agent approval
```

**根因**：ToolRegistry V2 校验（`src/common/tools/registry.cpp:102-106`）拒绝 `category ∈ {Execute, Network, StateModify}` 但 `requires_approval_in_plan == false && requires_approval_in_agent == false` 的组合。5 个工具的 ApprovalPolicy 全部设为 `{F, F, F, F}`，违反 ADR-0004 §7 安全底线。

### Gap 2: Loop Agent 是 stub（🟠 MED）

`pdk/loop_agent/src/pdk_entry.cpp:92-128` 的 `loop/run` handler 返回 canned response：

```cpp
output["response"] = "[loop_agent/" + loop_type + "] Processed: ..."
output["steps"] = 1;
output["tokens_used"] = 42;
output["cost_usd"] = 0.001;
```

注释明确说 "This is a mock response... For the demo --mock mode, we return a canned response to avoid the architectural limitation that DSLEngine::from_markdown creates an isolated sub-engine whose LLM provider cannot inherit configuration from the parent engine (ADR-0019 follow-up)."

**问题**：`loop_agent` 名义上调用 `lib/loop/react.agent.md` DSL，但实际从未读取该文件、未实例化 `DSLEngine::from_markdown`、未调用 `run(LayeredContext)`。`load_agent_file()` 等 helper（`pdk_entry.cpp:48-77`）是死代码。

### Gap 3: SKILL.md 永远 mock（🟡 LOW）

`main.cpp:107` 处理 SKILL.md 类型 plugin 时只打印 "Skill registered (mock)"。SkillInterpreter（ADR-0055）未实现，SKILL.md 不会被实际加载执行。

### 用户需求

用户配置了云端 LLM API（`~/.config/opencode/opencode.jsonc`）：
- Provider: `baiduqianfancodingplan`
- BaseURL: `https://qianfan.baidubce.com/v2/coding`
- API Key: `QIANFAN_API_KEY` 环境变量
- 模型: `deepseek-v4-pro` / `glm-5` / `minimax-m2.5`

希望 demo 能用真实 LLM 替代 MockLLMProvider 端到端运行。

## What Changes

### 修复 1: 5 个 ToolMetadata ApprovalPolicy（3 个 plugin）

为每个 dangerous-category 工具正确设置 `ApprovalPolicy{...}`：

| 文件 | 工具 | Category | ApprovalPolicy |
|---|---|---|---|
| `pdk/provider_agent/src/pdk_entry.cpp` | provider/register | StateModify | `{plan=T, agent=T, yolo=F, force=T}` |
| `pdk/session_agent/src/pdk_entry.cpp` | session/branch | Execute | `{plan=T, agent=T, yolo=F, force=F}` |
| `pdk/session_agent/src/pdk_entry.cpp` | session/compact | Execute | `{plan=T, agent=T, yolo=F, force=T}` |
| `pdk/session_agent/src/pdk_entry.cpp` | session/persist | Execute | `{plan=T, agent=T, yolo=F, force=T}` |
| `pdk/budget_agent/src/pdk_entry.cpp` | budget/alerts | Execute | `{plan=T, agent=T, yolo=F, force=F}` |

参考 `budget/set_limit`（`pdk/budget_agent/src/pdk_entry.cpp:79-86`）的正确实现作为模板。

### 修复 2: Loop Agent 真实 DSL 执行

采用**简化版 Option B**：在 handler 内部用 `DSLEngine::from_markdown` 创建 sub-engine 并执行，但**接受 sub-engine 无独立 LLM provider 限制**——DSL nodes（assign/condition/tool_call）可执行，仅 LLM 调用节点会被 fallback 处理。

```cpp
// pdk/loop_agent/src/pdk_entry.cpp: 新版 handler
[](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
    // 1. 选择 + 读取 lib/loop/<type>.agent.md
    // 2. DSLEngine::from_markdown(content)
    // 3. 构造 LayeredContext with system_prompt/user_input/history
    // 4. sub_engine->run(ctx)
    // 5. 从 ExecutionResult.final_context 提取 response/steps/tokens
    // 6. 返回真实输出
}
```

### 修复 3: main.cpp 接入云端 LLM API

在 `examples/pdk_chat_demo/main.cpp` 的非 mock 分支，构造 `LLMConfig` 直接调用 `LLMProviderFactory::create()`：

```cpp
LLMConfig llm_cfg;
llm_cfg.provider    = "custom";                            // 路由到 CloudLLMAdapter
llm_cfg.api_url     = "https://qianfan.baidubce.com/v2/coding";
llm_cfg.api_key_env = "QIANFAN_API_KEY";
llm_cfg.api_endpoint = "/chat/completions";
llm_cfg.model       = config.agent.model;                  // e.g. "deepseek-v4-pro"
llm_cfg.max_tokens  = 4096;
llm_cfg.temperature = 0.7;
auto factory = agenticdsl::llm::create_provider_factory();
llm_provider = factory->create(llm_cfg);
```

更新 `config.json`：
- 添加 `providers/baidu-deepseek` 配置（api_key_env + api_url + api_endpoint + models）
- `agent.provider = "baidu-deepseek"`, `agent.model = "deepseek-v4-pro"`

### 修复 4: SKILL.md 行为诚实化（可选）

修改 `main.cpp:107` 行为：对 SKILL.md 类型 plugin 显式标注为 `requires SkillInterpreter (ADR-0055), not yet implemented; mock-only in v1`，不再误标 "registered"。

## Capabilities

### New Capabilities
无

### Modified Capabilities
- `pdk-chat-demo-v1-buildable`: 添加 3 个新 requirements（plugin approval policy compliance / loop agent real DSL execution / cloud LLM integration）

## Impact

### Affected Code (4 files)

| 文件 | 变更类型 | 说明 |
|---|---|---|
| `pdk/provider_agent/src/pdk_entry.cpp` | 修改 | line 57-64 ApprovalPolicy 修复 |
| `pdk/session_agent/src/pdk_entry.cpp` | 修改 | line 108-115 / 136-142 / 173-179 三个 ApprovalPolicy 修复 |
| `pdk/budget_agent/src/pdk_entry.cpp` | 修改 | line 106-112 ApprovalPolicy 修复 |
| `pdk/loop_agent/src/pdk_entry.cpp` | 修改 | line 92-128 handler 重写为真实 DSL 执行 |
| `examples/pdk_chat_demo/main.cpp` | 修改 | line 132-144 LLM 注入路径 + line 107 SKILL.md 标注 |
| `examples/pdk_chat_demo/config.json` | 修改 | 添加 baidu-deepseek provider 配置 |

### Affected ADRs
- ADR-0004 §7 (ToolMetadata 安全模型): 现有规则被严格执行，修复使工具注册符合规范
- ADR-0019 §1.4 (engine.h decoupled): Loop Agent sub-engine 限制已知并文档化
- ADR-0055 (SkillInterpreter): 未实现，SKILL.md 标注 mock-only

### Affected Dependencies
无新增依赖（`CloudLLMAdapter` 已存在并支持 OpenAI 兼容协议）

## Non-Goals
- 不修改 `src/common/tools/registry.cpp` 的校验规则（规则本身合理）
- 不实现 SkillInterpreter（ADR-0055，独立 change 范围）
- 不为 Loop Agent sub-engine 解决 LLM provider 继承问题（ADR-0019 follow-up，独立 change）
- 不修改 ctest 测试套件（本 change 仅运行时验证）

## Ship Gate
- [ ] 6 PDK plugins 全部加载成功（运行 `--mock` 看到 6 个 "Loaded plugin" 行）
- [ ] Loop Agent 真实输出（response 来自 react.agent.md execution，非 canned 文本）
- [ ] 云端 LLM 模式可用（设置 `QIANFAN_API_KEY` 后真实 API 调用成功）
- [ ] SKILL.md 行为诚实（明确标注 mock-only + ADR-0055 依赖）
- [ ] `ctest -j$(nproc)` 80/80 仍 PASS（零回归）
- [ ] `openspec validate --strict` 通过
