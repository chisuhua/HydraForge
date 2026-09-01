# workflow-materializer-v1 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实施 `WorkflowMaterializer V1` — `WorkflowGraph` → DSL 文本 → 经 `DSLEngine::continue_with_generated_dsl()` 注册到调度器。Oracle 评审 🟡 Conditional-Go (`session ses_facbd3ffbffeUjlJgZsgMWFiM4`), commit `06ddd13` 已修 B3 (axis6 依赖声明) + commit `259b9d1` 已修 proposal L70 stale (`dsl_call /lib/cognitive/...` → `tool_call cognitive::*`)。**强依赖 T2 (axis6 字段) + T5 (cognitive::* tools) ship 后**。

**Architecture:** 新建 `src/modules/cognitive/workflow_materializer.{h,cpp}` 含 `Materializer::materialize_to_dsl(const WorkflowGraph&)` 函数。映射规则: WorkflowNode Axis1-Axis6 → DSL 节点类型 (start/assign/tool_call/fork/join/on_failure) + 连接边。输出为 `### AgenticDSL /dynamic/mcts/<task_id>` 格式 Markdown 文本 (单一事实源)。经 `DSLEngine::continue_with_generated_dsl(dsl_text)` 注册 (静态路径, 跳过 P0 GenerateSubGraph 断链)。emit `workflow.materialized` 事件到 IInteractionBus (lineage 追踪)。

**Tech Stack:** C++20, nlohmann::json, DSLEngine::continue_with_generated_dsl, IInteractionBus, ADR-0071 LLM-native DSL authoring (Materializer 是 agent-driven, 非 LLM-driven)。

---

## Scope Adjustments vs proposal

**Adopted scope** (Oracle Conditional-Go):
- `Materializer::materialize_to_dsl(WorkflowGraph) → std::optional<std::string>` (axis6=None 或未覆盖组合 → nullopt)
- WorkflowNode Axis1-Axis6 → DSL 节点映射表 (axis6=Reflect/Search/Compile → tool_call `cognitive::*`, per design.md §决策 5)
- axis1=Branching → fork/join 节点对 (分支内容映射规则待补完, 见风险表)
- 输出格式 `### AgenticDSL /dynamic/mcts/<task_id>` Markdown 文本
- `DSLEngine::continue_with_generated_dsl()` 复用 (engine.cpp:390)
- emit `workflow.materialized` 事件 (含 workflow_hash + output_path + lineage)
- ≥6 测试 case (commit `259b9d1` 修后 tasks.md 2.3)

**Deferred to follow-up**:
- axis1=Branching fork 分支内容映射完整规则 (当前 design.md §决策 2 部分空白, V2 补)
- lossless 转换保证 (lossless-fallback 路径)
- "端到端闭环" (需 G3 触发器 + G7 归因契约; 当前 "管线 demo" 级别)
- 多 WorkflowGraph → DSL 批量并行 (Phase 6a Wave 2)

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `include/agenticdsl/cognitive/workflow_materializer.h` (new) | `Materializer` class + `materialize_to_dsl()` 签名 |
| `src/modules/cognitive/workflow_materializer.cpp` (new) | 映射规则实装 + DSL 文本生成 + lineage event |

### Tests

| File | Responsibility |
|---|---|
| `tests/test_workflow_materializer.cpp` (new, ≥6 cases) | 6 个核心 case (per tasks.md 2.3) |

### Example

| File | Responsibility |
|---|---|
| `examples/mcts_materialize_demo/main.cpp` (new, opt-in) | 完整管线 demo: search → materialize → continue_with_generated_dsl → execute |

---

## TDD 5-Step Execution

### Step 1: Write failing test

**File**: `tests/test_workflow_materializer.cpp` (new, ~90 LOC)

```cpp
#include <agenticdsl/cognitive/workflow_materializer.h>
#include <agenticdsl/cognitive/mcts_workflow_search.h>
#include <agenticdsl/core/engine.h>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("empty WorkflowGraph returns nullopt", "[materializer][v1]") {
    Materializer m;
    WorkflowGraph empty;
    REQUIRE_FALSE(m.materialize_to_dsl(empty).has_value());
}

TEST_CASE("Linear axis1 + None axis6 produces linear DSL (start→node→end)", "[materializer][v1]") {
    Materializer m;
    WorkflowGraph g;
    g.nodes.push_back({"start", Axis1Template::Linear, Axis2Param::Temperature,
                       Axis3Tool::None, Axis4Control::Sequential, Axis5Error::Retry,
                       Axis6CognitiveDomain::None});
    g.nodes.push_back({"compute", Axis1Template::Linear, Axis2Param::Temperature,
                       Axis3Tool::Calculator, Axis4Control::Sequential, Axis5Error::Retry,
                       Axis6CognitiveDomain::None});
    g.nodes.push_back({"end", Axis1Template::Linear, Axis2Param::Temperature,
                       Axis3Tool::None, Axis4Control::Sequential, Axis5Error::Abort,
                       Axis6CognitiveDomain::None});
    auto dsl = m.materialize_to_dsl(g);
    REQUIRE(dsl.has_value());
    REQUIRE(dsl->find("type: start") != std::string::npos);
    REQUIRE(dsl->find("type: tool_call") != std::string::npos);
    REQUIRE(dsl->find("type: end") != std::string::npos);
}

TEST_CASE("axis6=Reflect produces cognitive::reflect tool_call node (per design §决策 5)",
          "[materializer][v1][axis6]") {
    Materializer m;
    WorkflowGraph g;
    g.nodes.push_back({"reflect_step", Axis1Template::Linear, Axis2Param::Temperature,
                       Axis3Tool::Custom, Axis4Control::Sequential, Axis5Error::Retry,
                       Axis6CognitiveDomain::Reflect});
    auto dsl = m.materialize_to_dsl(g);
    REQUIRE(dsl.has_value());
    REQUIRE(dsl->find("tool: \"cognitive::reflect\"") != std::string::npos
         || dsl->find("tool: \"evolution::reflect\"") != std::string::npos);  // T5 命名裁决后
}

TEST_CASE("axis1=Branching produces fork/join node pair", "[materializer][v1][branching]") {
    Materializer m;
    WorkflowGraph g;
    g.nodes.push_back({"branch_root", Axis1Template::Branching, Axis2Param::Temperature,
                       Axis3Tool::None, Axis4Control::Parallel, Axis5Error::Retry,
                       Axis6CognitiveDomain::None});
    g.nodes.push_back({"join_point", Axis1Template::Branching, Axis2Param::Temperature,
                       Axis3Tool::None, Axis4Control::Sequential, Axis5Error::Abort,
                       Axis6CognitiveDomain::None});
    auto dsl = m.materialize_to_dsl(g);
    REQUIRE(dsl.has_value());
    REQUIRE(dsl->find("type: fork") != std::string::npos);
    REQUIRE(dsl->find("type: join") != std::string::npos);
}

TEST_CASE("DSL text round-trips through MarkdownParser → ParsedGraph",
          "[materializer][v1][roundtrip]") {
    Materializer m;
    WorkflowGraph g;
    // ... 构造 graph
    auto dsl = m.materialize_to_dsl(g);
    REQUIRE(dsl.has_value());
    DSLEngine engine;
    bool ok = engine.continue_with_generated_dsl(*dsl);
    REQUIRE(ok);
    REQUIRE(engine.full_graphs().size() >= 1);  // 注册成功
}

TEST_CASE("lineage event contains workflow_hash + output_path",
          "[materializer][v1][bus][lineage]") {
    Materializer m;
    auto bus = std::make_shared<InMemoryBus>();
    m.set_bus(bus);
    WorkflowGraph g;
    g.task_id = "test_task_xyz";
    m.materialize_to_dsl(g);
    REQUIRE(bus->events.size() >= 1);
    auto& last = bus->events.back();
    REQUIRE(last.topic == "workflow.materialized");
    REQUIRE(last.data.contains("workflow_hash"));
    REQUIRE(last.data.contains("output_path"));
}
```

**Verification**:
```bash
cmake --build build --target test_workflow_materializer
ctest -R "workflow_materializer" --output-on-failure
# Expected: FAIL (Materializer 未实装)
```

---

### Step 2: Implement `Materializer::materialize_to_dsl()`

**File**: `include/agenticdsl/cognitive/workflow_materializer.h`

```cpp
#pragma once
#include <agenticdsl/cognitive/mcts_workflow_search.h>
#include <agenticdsl/contract/iinteraction_bus.h>
#include <memory>
#include <optional>
#include <string>

namespace agenticdsl::cognitive {

class Materializer {
public:
    std::optional<std::string> materialize_to_dsl(const WorkflowGraph& graph);
    void set_bus(std::shared_ptr<IInteractionBus> bus) { bus_ = bus; }
private:
    std::shared_ptr<IInteractionBus> bus_;
    std::string render_node(const WorkflowNode& node, int indent) const;
};

}  // namespace
```

**File**: `src/modules/cognitive/workflow_materializer.cpp`

```cpp
#include "agenticdsl/cognitive/workflow_materializer.h"
#include "agenticdsl/contract/bus_event.h"
#include <sstream>
#include <nlohmann/json.hpp>

namespace agenticdsl::cognitive {

using json = nlohmann::json;

static std::string axis6_to_tool(Axis6CognitiveDomain a6) {
    switch (a6) {
        case Axis6CognitiveDomain::Reflect:    return "cognitive::reflect";    // T5 命名裁决后: "evolution::reflect"
        case Axis6CognitiveDomain::Search:     return "cognitive::search";
        case Axis6CognitiveDomain::Compile:    return "cognitive::compile";
        case Axis6CognitiveDomain::Meta_Select: return "cognitive::meta_select";  // V2
        case Axis6CognitiveDomain::Reason:     return "cognitive::reason";         // V2
        case Axis6CognitiveDomain::None:       return "";  // 不生成 cognitive 节点
    }
    return "";
}

std::string Materializer::render_node(const WorkflowNode& node, int indent) const {
    std::ostringstream yaml;
    std::string pad(indent * 2, ' ');
    yaml << pad << "id: " << node.id << "\n";
    
    // axis6 → tool_call cognitive::* (V1 走 tool_call 路线, design §决策 5)
    std::string tool_name = axis6_to_tool(node.axis6);
    if (!tool_name.empty()) {
        yaml << pad << "type: tool_call\n";
        yaml << pad << "tool: \"" << tool_name << "\"\n";
        yaml << pad << "args:\n";
        yaml << pad << "  prompt: \"{{ task }}\"\n";
    } else if (node.axis3 == Axis3Tool::Calculator) {
        yaml << pad << "type: tool_call\n";
        yaml << pad << "tool: \"math::calculate\"\n";
        yaml << pad << "args:\n";
        yaml << pad << "  expr: \"{{ expr }}\"\n";
    } else if (node.axis1 == Axis1Template::Branching && node.axis4 == Axis4Control::Parallel) {
        yaml << pad << "type: fork\n";
        yaml << pad << "branches:\n";
        // 分支内容: 当前为空 (V2 补 axis1=Branching 分支内容映射完整规则)
        yaml << pad << "  - \"/dynamic/branch_a\"\n";
        yaml << pad << "  - \"/dynamic/branch_b\"\n";
    } else if (node.axis4 == Axis4Control::Parallel && node.axis1 == Axis1Template::Branching) {
        yaml << pad << "type: join\n";
        yaml << pad << "wait_for: [\"branch_a\", \"branch_b\"]\n";
    } else if (node.axis5 == Axis5Error::Abort) {
        yaml << pad << "type: end\n";
        yaml << pad << "termination_mode: hard\n";
    } else if (node.id == "start" || node.axis5 == Axis5Error::None) {
        yaml << pad << "type: start\n";
    } else {
        yaml << pad << "type: assign\n";
        yaml << pad << "assign:\n";
        yaml << pad << "  result: \"{{ input }}\"\n";
    }
    yaml << "\n";
    return yaml.str();
}

std::optional<std::string> Materializer::materialize_to_dsl(const WorkflowGraph& graph) {
    // 兜底: 空 graph 或所有节点 axis6=None (等同 v1.0)
    if (graph.nodes.empty()) return std::nullopt;
    
    std::ostringstream md;
    md << "### AgenticDSL `/dynamic/mcts/" << graph.task_id << "`\n";
    md << "```yaml\n";
    md << "# --- BEGIN AgenticDSL ---\n";
    for (const auto& node : graph.nodes) {
        md << "- ";
        md << render_node(node, 0);
    }
    md << "# --- END AgenticDSL ---\n";
    md << "```\n";
    
    std::string dsl_text = md.str();
    
    // emit lineage event (workflow_hash + output_path)
    if (bus_) {
        std::string workflow_hash = std::to_string(std::hash<std::string>{}(dsl_text));
        bus_->emit(BusEvent{"workflow.materialized", {
            {"workflow_hash", workflow_hash},
            {"output_path", "/dynamic/mcts/" + graph.task_id},
            {"task_id", graph.task_id}
        }});
    }
    return dsl_text;
}

}  // namespace
```

---

### Step 3: Register `workflow.materialized` in ADR-0068

**File**: `docs/adr/adr-0068-event-emission-contract.md` Appendix A

新增 (commit `06ddd13` W4 ADR-0068 v1.9+ 归口, T1 用 v1.9):
```
| `workflow.materialized` | Materializer 完成 WorkflowGraph→DSL 转换 | Materializer.materialize_to_dsl | ✅ Shipped (T1 2026-08-31) |
```

---

### Step 4: Pipeline demo example (opt-in)

**File**: `examples/mcts_materialize_demo/main.cpp` (new, opt-in)

```cpp
#include "agenticdsl/cognitive/mcts_workflow_search.h"
#include "agenticdsl/cognitive/workflow_materializer.h"
#include "agenticdsl/core/engine.h"
#include <iostream>

int main() {
    // 1. MCTS 搜索 (mock evaluator)
    auto evaluator = std::make_shared<MockEvaluator>();
    auto governor = std::make_shared<MockMutationGovernor>();
    auto regression = std::make_shared<MockRegressionGate>();
    agenticdsl::cognitive::MCTSWorkflowSearch searcher(evaluator, governor, regression);
    
    // 2. 搜索最佳 workflow
    auto best = searcher.search({});
    
    // 3. Materializer → DSL 文本
    agenticdsl::cognitive::Materializer m;
    auto dsl = m.materialize_to_dsl(best);
    
    // 4. DSLEngine → register
    DSLEngine engine;
    engine.continue_with_generated_dsl(*dsl);
    
    // 5. execute (管线 demo)
    std::cout << "pipeline demo executed" << std::endl;
    return 0;
}
```

---

### Step 5: Commit

```bash
git add include/agenticdsl/cognitive/workflow_materializer.h \
        src/modules/cognitive/workflow_materializer.cpp \
        tests/test_workflow_materializer.cpp \
        tests/CMakeLists.txt \
        docs/adr/adr-0068-event-emission-contract.md
git commit -m "feat(materializer): WorkflowGraph → DSL 文本 (ToolRegistry 静态路径)"
```

---

## Ship Gate Validation

```bash
# 1. openspec validate (T2 + T5 必须先 ship)
openspec validate 2026-08-31-workflow-materializer-v1 --strict

# 2. compile + tests (T2 + T5 依赖已 ship, axis6 字段 + cognitive::* tools 可用)
cmake --build build && ctest -R "workflow_materializer" --output-on-failure
# Expected: PASS (6 cases)

# 3. Baseline regression (T3 + T6 + T2 + T5 已 ship, 224 + 6 = 230)
ctest --output-on-failure  # 230

# 4. ADR lint + drift
python3 tools/adr_lint.py  # ✓
python3 tools/docs_drift_audit.py  # 0 DRIFT

# 5. axis6 字段依赖验证
grep -c "Axis6CognitiveDomain\|axis6" include/agenticdsl/cognitive/mcts_workflow_search.h  # ≥ 1

# 6. cognitive::* tools 依赖验证
grep -c "register_cognitive_tools\|cognitive::reflect" src/modules/cognitive/cognitive_tools.cpp  # ≥ 1
```

---

## Risk Assessment

| 风险 | 缓解 |
|------|------|
| T2/T5 未 ship (axis6 + cognitive::* tools 不可用) | ship 顺序 enforce: T3 → T6 → T2 → T5 → T1; ship gate grep T2 + T5 接口 |
| proposal L70 stale (commit `259b9d1` 已修) | 文档/代码已对齐 tool_call 路线 |
| axis1=Branching 分支内容映射空白 (Oracle P0) | Step 2 写明 `branches: ["/dynamic/branch_a", "/dynamic/branch_b"]` 占位; V2 补完整规则 |
| P0 GenerateSubGraph 断链 (Oracle 评估) | Materializer 走 `continue_with_generated_dsl` 静态路径 (engine.cpp:390), 不经过断点 |
| lineage event 字段 (workflow_hash + output_path) 验证 | Step 1 test_6 显式断言 |
| workflow.materialized 主题注册 (ADR-0068) | Step 3 同步注册; ship gate grep 验证 |
| "端到端闭环" vs "管线 demo" 措辞降级 (commit `06ddd13` W5) | 文档明确: 当前为 "管线 demo", 端到端闭环需 G3 触发 + G7 归因 |
| axis6=Meta_Select/Reason 节点 (Phase 1 预留) | Step 2 含 switch case, V2 实装 IPER 后启用 |

---

## 后续触发条件

| 触发条件 | 后续 OpenSpec change |
|----------|---------------------|
| T1 ship 立即 | T4 signature-validation-real-impl 启动 (P0 GenerateSubGraph 治理补全) |
| axis1=Branching 分支内容映射需求 | workflow-materializer-v2 (lossless + 分支内容) |
| IPER 实装 (ADR-0061-09 V2) | axis6 Meta_Select/Reason 节点启用 |
| 端到端触发 + 归因 (G3 + G7) | 端到端闭环 change |