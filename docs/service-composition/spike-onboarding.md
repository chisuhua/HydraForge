# Spike Onboarding — G2/G4/G5 Team Kickoff Guide

> **⚠️ RED BANNER**: Spike 代码是 **tension-maximizing MVP**, **非** G2/G4/G5 的生产参考代码。本 Spike 刻意暴露 awkward patterns (30 行 handler 约束 / string→json 类型退化 / 无 shared contract header) 以收集 Layer 1 + Layer 3 观察数据。G2/G4/G5 应从 Spike 的 **设计缺陷中学习**,而非复制其代码模式。

> **受众**: G2/G4/G5 团队 (1.5 eng each) + 平台团队
> **阅读时间**: ~15 分钟
> **关联**: [ADR-0051 Phase 6 PDK Composition Spike](../../docs/adr/adr-0051-phase6-pdk-composition-spike.md)
> **OpenSpec**: `openspec/changes/phase6-service-ification-v1/`

---

## §1: What Spike IS

Phase 6 PDK Composition Spike 验证 **in-process agent-to-agent 工具调用**的可行性。

### 核心机制

| 维度 | 值 |
|------|-----|
| **注册模式** | `IToolRegistry::register_tool_function("name/verb", metadata, lambda)` — 与 `pdk/llama_engine/` 一致 |
| **工具命名** | ADR-0043 slash-only: `knowledge_base/query`, `coding_assistant/review` |
| **Args 合约** | `std::unordered_map<std::string, std::string>` → `nlohmann::json result` |
| **新宏** | **None** — 不使用 DECLARE_TOOL (宏 token-pasting 不支持 `/`), 不引入 DECLARE_SERVICE |
| **Transport** | 进程内 `IToolRegistry::call_tool()` (非网络 / 非 MCP / 非 OpenAI API) |
| **LLM 模拟** | MockLLMProvider (单线程 test stub, 不要求并发) |
| **核心代码变更** | 仅 ToolCoordinator RAII guard (nesting depth + cycle detection + thread_local) |

### 已有 Agent

| Agent | 工具名 | ToolCategory | 循环 | 功能 |
|-------|--------|-------------|------|------|
| **G3 Knowledge Base** | `knowledge_base/query` | `Execute` / `{Workflow}` | N/A (被动工具) | 3-5 文档检索 + MockLLM 生成 + session store |
| **G1 Coding Assistant** | `coding_assistant/review` | `Execute` / `{Workflow}` | ReAct (2-step) | 调 G3 检索 → MockLLM 综合 → 返回 review comment |

### Agent 间交互

```
User: "review my code"
  └─ G1: step1 → call_tool("knowledge_base/query", {query, session_id})
                   └─ G3: 检索文档 → MockLLM 生成 → return {success, answer}
  └─ G1: step2 → synthesize G3 answer → return review comment
```

---

## §2: What Spike IS NOT

| 不是 | 说明 |
|------|------|
| **Not networked** | Agent 间通过 in-process function call, 非 gRPC/MCP/REST |
| **Not async/streaming** | 同步调用, 无 stream/yield/future。G1 等待 G3 完成后再继续 |
| **Not multi-tenant** | 单个 DSLEngine 实例, 无 tenant isolation / namespace / quota |
| **Not Candidate B v1** | 不兑现 ADR-0050 "外部 MCP/OpenAI API" 目标。Phase 6 Candidate B v1 正式启动仍需满足 ADR-0050 §启动条件 5 项 |
| **Not production-grade** | MockLLMProvider / 魔术字符串 / 无 error recovery / 单线程。见 RED BANNER |
| **Not G2/G4/G5 template** | G2 (streaming) / G4 (cache) / G5 (browser) 的需求超出 Spike 验证范围 |

---

## §3: Spike Contract (Normative Spec)

### 3.1 工具命名

遵循 ADR-0043 slash-only: `{domain}/{verb}`
- 注册: `registry.register_tool_function("knowledge_base/query", meta, handler)`
- 调用: `registry.call_tool("knowledge_base/query", args)`

### 3.2 Args Schema

```cpp
std::unordered_map<std::string, std::string> args;
args["query"] = "What is AgenticDSL?";
args["session_id"] = "ses_abc123";

nlohmann::json result = registry.call_tool("knowledge_base/query", args);
```

所有值均为字符串。复杂结构 (arrays, nested objects) JSON 编码到 string value 中。

### 3.3 Return Schema

```json
// 成功
{"success": true, "answer": "AgenticDSL is a DSL execution engine..."}

// 失败
{"success": false, "error": "No documents matched query 'xyz'"}
```

**必填字段**: `success` (bool)。`answer` 和 `error` 互斥。

### 3.4 Error Handling

| 场景 | 响应 |
|------|------|
| 工具不存在 | `ToolRegistry` 抛出 `std::runtime_error` (caller 职责: try-catch) |
| LLM 回调未设置 | G3 返回 `{success: false, error: "LLM callback not configured"}` |
| Session ID 无效 | G3 创建新 session (幂等, 非错误) |
| 嵌套深度 > 2 | HARD KILL (ToolCoordinator RAII throw) |
| 环检测 | HARD KILL + `cycle_detected_log` audit event |

### 3.5 ToolMetadata (ADR-0004 V2)

```cpp
ToolMetadata meta;
meta.category = ToolCategory::Execute;
meta.allowed_layers = {LayerProfile::Workflow};
meta.cost_estimate = 0.0;  // MockLLM = free
meta.timeout_ms = 30000;   // 30s default

ApprovalPolicy policy = make_approval("agent");  // plan+agent, no force-approve, no yolo
```

---

## §4: Does Your Agent Fit Spike?

回答以下 4 个问题决策是否适合在 Spike v1 中构建 Agent:

```
Q1: 你的 Agent 是无状态的吗?
  ├─ YES → 适合 Spike v1 (纯 compute: transform/validate/query)
  └─ NO  → Q2

Q2: 你的 Agent 需要跨请求的 session 状态?
  ├─ YES → 适合 Spike v1 (per-session store within plugin, 如 G3)
  └─ NO  → Q3 (无状态)

Q3: 你的 Agent 需要流式输出 (streaming/yield)?
  ├─ YES → 🔴 NOT suitable for Spike v1 (G2 需要 streaming infrastructure)
  └─ NO  → Q4

Q4: 你的 Agent 需要调用其他 Agent (cross-agent composition)?
  ├─ YES → 适合 Spike v1 (G1→G3 模式), 但注意:
  │        - 嵌套深度 ≤ 2 (ToolCoordinator HARD KILL at depth > 2)
  │        - cycle detection 仅同线程有效 (跨线程 cycle 不可检测)
  └─ NO  → 适合 Spike v1 (简单 standalone agent)
```

### 按角色分类

| 你的 Agent 类型 | 适合? | 参考 |
|----------------|:-----:|------|
| Compute/Transform (纯计算) | ✅ | G3 pattern (注册一个 tool) |
| Orchestrator (编排多个 tool) | ✅ | G1 pattern (DEFINE_AGENT + ReAct loop) |
| Streaming (LLM streaming) | ❌ | 需要 G2 infrastructure (不在 Spike scope) |
| Cache (key-value store) | ⚠️ | G4 需要 shared cache layer (不在 Spike scope, 可用 in-plugin store 替代) |
| Browser (web automation) | ❌ | G5 需要 playwright/selenium (不在 Spike scope) |

---

## §5: Trigger Thresholds for DECLARE_SERVICE Push

Spike v1 使用 `register_tool_function` 模式。DECLARE_SERVICE 宏的形式化触发条件 (per ADR-0051 §触发条件 T-5):

| 条件 | 当前状态 (2026-07-15) | 触发 |
|------|----------------------|:----:|
| 2+ different awkward pattern categories | **4/5 类别** (Contract Drift, Lifecycle Coupling, Error Propagation, Resource Lifetime) | ✅ |
| Layer 1 reviewer agreement | **6 AGREE / 0 CONFLICT** | ✅ |
| Layer 3 convergence (primary + reviewer) | **6 一致 / 2 P0 / 4 互补 / 0 冲突** | ✅ |

**当前建议**: 🔴 YES — 满足形式化触发条件。应在 Spike ship 时同步创建 ADR-0052 draft。

**为什么不立即兑现**: 样本仅 G1+G3 (2 agents)。G2/G4/G5 可能揭示新 pattern 类别 (Streaming/Cache/Browser-specific)。推迟到 ≥3 个不同 agent type 后。

---

## §6: Reference Implementations

### G3 Knowledge Base Plugin

| 项目 | 路径 |
|------|------|
| 源码 | `pdk/g3_knowledge_base/src/g3_query.cpp` |
| 入口 | `pdk/g3_knowledge_base/src/g3_entry.cpp` |
| 状态 | `pdk/g3_knowledge_base/src/g3_state.h` |
| 测试 | `tests/test_g3_knowledge_base.cpp` (5 tests) |

**关键模式**:
- `register_tool_function("knowledge_base/query", meta, handler)` (非 DECLARE_TOOL)
- Handler ≤ 30 行 (ADR-0051 Ship Gate)
- Error schema: `{success, error}`
- `std::shared_mutex` 保护 session store

### G1 Coding Assistant Plugin

| 项目 | 路径 |
|------|------|
| 源码 | `pdk/g1_coding_assistant/src/g1_agent.cpp` |
| 入口 | `pdk/g1_coding_assistant/src/g1_entry.cpp` |
| 状态 | `pdk/g1_coding_assistant/src/g1_state.h` |
| 测试 | `tests/test_g1_coding_assistant.cpp` (3 tests) |

**关键模式**:
- `DEFINE_AGENT(CodingAssistant, AgentLoopType::React)` (2-parameter macro)
- 2-step ReAct: invoke G3 → synthesize comment
- Tool manifest: discover via `IToolRegistry::has_tool()`

### E2E Integration

| 项目 | 路径 |
|------|------|
| 测试 | `tests/test_service_v1.cpp` (3 tests: multi-turn / isolation / error propagation) |
| Escalation | `tests/test_escalation_triggers.cpp` (6 tests) |

---

## Quick Start for New Agent

```bash
# 1. 复制 G3 骨架
cp -r pdk/g3_knowledge_base pdk/gx_your_agent

# 2. 修改 CMakeLists.txt 中的 target name
# 3. 修改 gx_entry.cpp 中的 pdk_register_tools() — 注册你的工具
# 4. 修改 gx_query.cpp 中的 handler — 实现业务逻辑
# 5. 添加 pdk/gx_your_agent 到根 CMakeLists.txt
# 6. 添加测试到 tests/test_gx_your_agent.cpp

# 7. 构建并测试
cd build && cmake .. && make -j$(nproc)
ctest -R test_gx_your_agent --output-on-failure
```

---

**最后更新**: 2026-07-15 (per tasks.md §8.1-§8.7)
**维护者**: HydraForge 平台团队
**关联 ADR**: [ADR-0051 Phase 6 PDK Composition Spike](../../docs/adr/adr-0051-phase6-pdk-composition-spike.md)
