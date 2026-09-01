# cognitive-specialists-as-tools Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实施 cognitive specialist tool 注册 — `GEPALoop::reflect_and_commit` + `MCTSWorkflowSearch::search` + `SkillCompiler::compile` 注册为 `cognitive::*` tool (经 ToolRegistry V2 + ToolMetadata V2 + ADR-0004 安全矩阵 + ADR-0031 审批)。Oracle 评审 🟡 Conditional-Go (`session ses_facbd3ffbffeUjlJgZsgMWFiM4`), commit `06ddd13` 已修 W1 (ErrorCode::InvalidParams) + W2 (register_tool_function_json API)。**未解决 Oracle P0**: 命名争议 (`cognitive::*` vs `evolution::*`), 需架构组裁决, 默认走 `evolution::*` 备选方案但保留 cognitive::* 主路径 (本 plan 提供双方案)。

**Architecture:** 新建 `src/modules/cognitive/cognitive_tools.h/.cpp` 含 3 个 tool 注册函数 (`register_cognitive_tools(tool_registry, gepa_loop, mcts_searcher, skill_compiler)`)。每个 tool 配 ToolMetadata V2 (category + approval_policy + allowed_layers + cost_estimate + timeout_ms)。走 ADR-0004 ToolRegistry + ToolCoordinator 4 步校验链。LayerProfile 限制: Workflow + Thinking 层可调用 (Cognitive L4 禁止)。

**Tech Stack:** C++20, std::function, IInteractionBus, ADR-0004 ToolRegistry V2, ADR-0073 ToolMetadata (input_schema/output_schema nlohmann), DECLARE_TOOL 宏 (commit `06ddd13` W2 已用 register_tool_function_json API)。

---

## Scope Adjustments vs proposal

**Adopted scope** (Oracle Conditional-Go, commit `06ddd13`):
- 3 个 tool 注册: `evolution::reflect` (GEPALoop) / `evolution::search` (MCTSWorkflowSearch) / `evolution::compile` (SkillCompiler)
- ToolMetadata V2 完整字段 (commit `06ddd13` W2 register_tool_function_json(name, metadata, lambda))
- `examples/pdk_chat_demo/` 集成示例 (可选, opt-in flag)
- ≥4 测试 case: 注册成功 / 3 个 specialist 调用测试 / null 兜底 / 审批策略测试
- **备选**: `cognitive::*` 主路径 (Oracle P0 命名争议未裁决, 如架构组决定 rename, 本 plan 阶段 4 切换)

**Deferred to follow-up**:
- LayerProfile::Cognitive 调用支持 (L4 当前禁止 tool_call, ADR-0004 §8 矩阵)
- `evolution::*` 命名实际应用 (待架构组裁决)
- input_schema/output_schema 完整 JSON Schema 定义 (依赖 ADR-0073 schema generation, 当前依赖手动构造)
- Per-specialist 实时 cost calibration (当前 cost_estimate 保守估计)

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `src/modules/cognitive/cognitive_tools.h` (new) | `register_cognitive_tools()` 函数签名 + 3 个 specialist class instance 参数 |
| `src/modules/cognitive/cognitive_tools.cpp` (new) | ToolMetadata V2 + register_tool_function_json 实际调用 |

### Tests

| File | Responsibility |
|---|---|
| `tests/test_cognitive_specialists_tools.cpp` (new, ≥4 cases) | 注册测试 + 调用测试 + null 兜底 + 审批矩阵 |

### Example

| File | Responsibility |
|---|---|
| `examples/cognitive_specialists_demo/main.cpp` (new, opt-in) | `chat_session.cpp` 集成示例 (启动时 `register_cognitive_tools(tool_registry, ...)`) |

---

## TDD 5-Step Execution

### Step 1: Write failing test

**File**: `tests/test_cognitive_specialists_tools.cpp` (new, ~70 LOC)

```cpp
#include <agenticdsl/cognitive/cognitive_tools.h>
#include <agenticdsl/common/tools/registry.h>
#include <agenticdsl/common/tools/tool_metadata.h>
#include <catch2/catch_test_macros.hpp>
#include <memory>

TEST_CASE("register_cognitive_tools registers 3 specialists (reflect/search/compile)", "[tools][cognitive]") {
    ToolRegistry registry;
    auto gepa = std::make_shared<MockGEPALoop>();
    auto mcts = std::make_shared<MockMCTSWorkflowSearch>();
    auto sc = std::make_shared<MockSkillCompiler>();
    register_cognitive_tools(registry, gepa, mcts, sc);
    REQUIRE(registry.is_registered("evolution::reflect"));
    REQUIRE(registry.is_registered("evolution::search"));
    REQUIRE(registry.is_registered("evolution::compile"));
    REQUIRE(registry.size() == 3);
}

TEST_CASE("evolution::reflect tool calls GEPALoop::reflect_and_commit (mock LLM)", "[tools][cognitive][gepa]") {
    ToolRegistry registry;
    auto gepa = std::make_shared<MockGEPALoop>();
    gepa->mock_response = "improved_prompt_v2";
    register_cognitive_tools(registry, gepa, nullptr, nullptr);  // only reflect
    
    ToolCallContext ctx;
    ctx.args["failed_trace"] = "trace_xyz";
    auto result = registry.call_tool("evolution::reflect", ctx);
    REQUIRE(result.ok);
    REQUIRE(result.data["new_prompt"] == "improved_prompt_v2");
    REQUIRE(gepa->call_count == 1);
}

TEST_CASE("register_cognitive_tools with nullptr specialists is safe (no-op)", "[tools][cognitive]") {
    ToolRegistry registry;
    REQUIRE_NOTHROW(register_cognitive_tools(registry, nullptr, nullptr, nullptr));
    REQUIRE(registry.size() == 0);  // 不注册任何 tool
}

TEST_CASE("evolution::* tools require approval (ADR-0031 ExecutionPolicy)", "[tools][cognitive][approval]") {
    ToolRegistry registry;
    StrictApprovalHandler approval;
    registry.set_approval_handler(&approval);
    auto gepa = std::make_shared<MockGEPALoop>();
    register_cognitive_tools(registry, gepa, nullptr, nullptr);
    
    // plan mode: evolution::reflect 需要审批 (高影响 skill 修改)
    ToolCallContext ctx;
    ctx.execution_policy = ExecutionPolicy::Plan;
    REQUIRE_FALSE(registry.call_tool("evolution::reflect", ctx).ok);
    REQUIRE(approval.last_request_denied);
    
    // yolo mode: 放行
    ctx.execution_policy = ExecutionPolicy::Yolo;
    REQUIRE(registry.call_tool("evolution::reflect", ctx).ok);
}
```

**Verification**:
```bash
cmake --build build --target test_cognitive_specialists_tools
ctest -R "cognitive_specialists_tools" --output-on-failure
# Expected: FAIL (cognitive_tools 未实装)
```

---

### Step 2: Implement `cognitive_tools.h` declarations

**File**: `src/modules/cognitive/cognitive_tools.h`

```cpp
#pragma once
#include <agenticdsl/common/tools/registry.h>
#include <memory>

namespace agenticdsl::cognitive {

class GEPALoop;
class MCTSWorkflowSearch;
class SkillCompiler;

void register_cognitive_tools(
    ToolRegistry& registry,
    std::shared_ptr<GEPALoop> gepa_loop,
    std::shared_ptr<MCTSWorkflowSearch> mcts_searcher,
    std::shared_ptr<SkillCompiler> skill_compiler);

}  // namespace
```

---

### Step 3: Implement `cognitive_tools.cpp` with ToolMetadata V2

**File**: `src/modules/cognitive/cognitive_tools.cpp`

```cpp
#include "agenticdsl/cognitive/cognitive_tools.h"
#include "agenticdsl/cognitive/gepa_loop.h"
#include "agenticdsl/cognitive/mcts_workflow_search.h"
#include "agenticdsl/cognitive/skill_compiler.h"
#include "agenticdsl/common/tools/tool_metadata.h"
#include "agenticdsl/common/tools/registry.h"
#include <nlohmann/json.hpp>

namespace agenticdsl::cognitive {

using json = nlohmann::json;

void register_cognitive_tools(
    ToolRegistry& registry,
    std::shared_ptr<GEPALoop> gepa_loop,
    std::shared_ptr<MCTSWorkflowSearch> mcts_searcher,
    std::shared_ptr<SkillCompiler> skill_compiler) {
    
    // 1. evolution::reflect (GEPALoop reflect_and_commit)
    if (gepa_loop) {
        ToolMetadata meta{
            .name = "evolution::reflect",
            .category = ToolCategory::Execute,  // Oracle P0 待裁决: Execute vs StateModify vs 新增 Evolution
            .approval_policy = {
                .requires_approval_in_plan = true,
                .requires_approval_in_agent = true,
                .requires_approval_in_yolo = false  // YOLO 模式放行
            },
            .allowed_layers = {LayerProfile::Workflow, LayerProfile::Thinking},  // L4 Cognitive 禁止
            .cost_estimate = 0.05,  // 单次 LLM reflect 估算
            .timeout_ms = 30000,
            .input_schema = json{{"type", "object"}, {"properties", {{"failed_trace", {{"type", "string"}}}}}},
            .output_schema = json{{"type", "object"}, {"properties", {{"new_prompt", {{"type", "string"}}}}}}
        };
        registry.register_tool_function_json(
            "evolution::reflect", meta,
            [gepa_loop](const json& args) -> ToolResult {
                ExecutionTrace trace = args["failed_trace"];
                auto result = gepa_loop->reflect_and_commit(trace);
                return ToolResult{true, {{"new_prompt", result.candidate_skills[0]}}, {}, 42};
            });
    }
    
    // 2. evolution::search (MCTSWorkflowSearch search)
    if (mcts_searcher) {
        ToolMetadata meta{
            .name = "evolution::search",
            .category = ToolCategory::Execute,
            .approval_policy = {
                .requires_approval_in_plan = true,
                .requires_approval_in_agent = true,
                .requires_approval_in_yolo = false
            },
            .allowed_layers = {LayerProfile::Workflow, LayerProfile::Thinking},
            .cost_estimate = 0.10,  // 完整 MCTS search (含嵌套预算约束 per T2 decision 5)
            .timeout_ms = 60000,
            .input_schema = json{{"type", "object"}, {"properties", {{"task_spec", {{"type", "object"}}}}}},
            .output_schema = json{{"type", "object"}, {"properties", {{"best_workflow", {{"type", "object"}}}}}}
        };
        registry.register_tool_function_json(
            "evolution::search", meta,
            [mcts_searcher](const json& args) -> ToolResult {
                TaskSpec spec = args["task_spec"];
                auto result = mcts_searcher->search(spec);
                return ToolResult{true, {{"best_workflow", result.best_workflow}}, {}, result.iterations_used};
            });
    }
    
    // 3. evolution::compile (SkillCompiler compile)
    if (skill_compiler) {
        ToolMetadata meta{
            .name = "evolution::compile",
            .category = ToolCategory::StateModify,  // 修改 SKILL.md 内容, 视为 state mutation
            .approval_policy = {
                .requires_approval_in_plan = true,
                .requires_approval_in_agent = true,
                .requires_approval_in_yolo = false
            },
            .allowed_layers = {LayerProfile::Workflow, LayerProfile::Thinking},
            .cost_estimate = 0.02,  // 本地编译, 无 LLM
            .timeout_ms = 10000,
            .input_schema = json{{"type", "object"}, {"properties", {{"skill_md_path", {{"type", "string"}}}}}},
            .output_schema = json{{"type", "object"}, {"properties", {{"compiled_skill_path", {{"type", "string"}}}}}}
        };
        registry.register_tool_function_json(
            "evolution::compile", meta,
            [skill_compiler](const json& args) -> ToolResult {
                auto path = args["skill_md_path"].get<std::string>();
                auto compiled = skill_compiler->compile(path);
                return ToolResult{true, {{"compiled_skill_path", compiled}}, {}, 100};
            });
    }
    
    // Oracle P0 命名争议: 如架构组决定 cognitive::* 主路径, 替换 prefix 'evolution::' → 'cognitive::'
    // 见 §五 后续 — 需架构组 self-review 决议
}

}  // namespace
```

---

### Step 4: Add to pdk_chat_demo integration (opt-in)

**File**: `examples/pdk_chat_demo/chat_session.cpp`

新增 (opt-in flag `AGENTICDSL_BUILD_EXAMPLES=ON`):
```cpp
#include "agenticdsl/cognitive/cognitive_tools.h"
void ChatSession::init_cognitive_tools() {
    register_cognitive_tools(tool_registry_, gepa_loop_, mcts_searcher_, skill_compiler_);
}
```

---

### Step 5: Commit

```bash
git add src/modules/cognitive/cognitive_tools.h \
        src/modules/cognitive/cognitive_tools.cpp \
        tests/test_cognitive_specialists_tools.cpp \
        tests/CMakeLists.txt
git commit -m "feat(cognitive-tools): 3 specialist tool 注册 (evolution::reflect/search/compile)"
```

---

## Ship Gate Validation

```bash
# 1. openspec validate (T2 必须先 ship, 提供 axis6 节点)
openspec validate 2026-08-31-cognitive-specialists-as-tools --strict

# 2. compile + tests
cmake --build build && ctest -R "cognitive_specialists_tools" --output-on-failure
# Expected: PASS (4 cases)

# 3. Baseline regression (T3 + T6 + T2 已 ship, 220 + 4 = 224)
ctest --output-on-failure  # 224

# 4. ADR lint + drift
python3 tools/adr_lint.py  # ✓
python3 tools/docs_drift_audit.py  # 0 DRIFT

# 5. Tool 注册验证
grep -c "register_tool_function_json" src/modules/cognitive/cognitive_tools.cpp  # 3

# 6. ToolMetadata 字段验证 (commit 06ddd13 W2 已要求)
grep -c "requires_approval_in_plan" src/modules/cognitive/cognitive_tools.cpp  # ≥ 3
```

---

## Risk Assessment

| 风险 | 缓解 |
|------|------|
| Oracle P0 命名争议 (`cognitive::*` vs `evolution::*`) 未裁决 | Step 3 已 `evolution::*` 占位, 提供 `cognitive::*` 备选; ship 后由架构组 self-review issue 决议, 改名通过 1 commit |
| ToolCategory::Execute 语义错误 (Oracle P0) | 决策: reflect/search 用 Execute (LLM 调用), compile 用 StateModify (修改 SKILL.md); 架构组裁决如有歧义, 新增 ToolCategory::Evolution |
| input_schema/output_schema 仅注释 (Oracle P1) | Step 3 已用 nlohmann::json 实际 schema, 而非注释; 待 ADR-0073 schema generation 自动化 |
| cost_estimate 偏低 (Oracle P1) | 当前 0.05/0.10/0.02 是保守初值; per-specialist 实时 calibration V2 |
| LayerProfile::Cognitive L4 禁止 tool_call (Oracle P0) | Step 3 显式 `allowed_layers = {Workflow, Thinking}` 排除 Cognitive; L4 cognitive 调用 specialists 必须经 L3 Agent (CompositionPattern) |
| Specialist 未实装 (T2 前提) | T2 ship 触发 (commit 0f19997 amendment flip); T5 ship 依赖 T2 |
| SkillCompiler.compile() 内部 LLM 调用 (V2 风险) | 当前 compile 本地化, 无 LLM; V2 follow-up |

---

## 后续触发条件

| 触发条件 | 后续 OpenSpec change |
|----------|---------------------|
| T5 ship 立即 | T1 workflow-materializer-v1 启动 (axis6 节点引用 cognitive::* tools) |
| 架构组命名裁决 | cognitive::* vs evolution::* 切换 commit (1 commit) |
| ToolCategory 扩展需求 | 新增 ToolCategory::Evolution (1 change) |
| per-specialist 实时 calibration | specialist-cost-calibration 子 change (V2) |
| ADR-0073 schema generation 完整 ship | input_schema/output_schema 自动生成 (替代手动 json) |