# meta-cognitive-coordination-doc Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Pure documentation沉淀 —在 `docs/architecture/agent-orchestration-architecture-2026-08.md` v1.2+ 中新增 §十八 "Cognitive-Cognitive 协调模式目录",把 `IAgentComposition` 4 模式（call/call_async/delegate/stream）+ 3 PDK Loop + GEPALoop 等散落能力沉淀为 5 种命名模式（sync-delegate / fan-out / hierarchical-plan / debate-round / stream-pipeline）,配 1 个 `examples/cognitive_meta_demo/` 极简 mock-mode 示例。Oracle 评审 Conditional-Go 3 强制条件已落实。

**Architecture:** 文档级 change。5 模式表 + §四 决策树新分支 + §九 验证命令扩展。`stream-pipeline` 显式标 V2 占位（代码抛 `logic_error`,per `iagent_composition.h:67`）。`debate-round` 显式标"组合配方"非原语（由 call_async + GEPALoop + IEvaluator 三组件组合）。example 走 `AGENTICDSL_BUILD_EXAMPLES` opt-in + MockLLMProvider。

**Tech Stack:** Markdown (AgenticDSL docs v3.10 spec)、nlohmann::json、examples opt-in flag（`AGENTICDSL_BUILD_EXAMPLES` OFF 默认）、Catch2 v3（example 测试 1 smoke case）。

---

## Scope Adjustments vs proposal

**Adopted scope**:
- §十八 5 模式目录表（模式名 + 原语锚点 + 应用代号 + 落地状态）
- §四 决策树加 "需要认知 agent 互相协调？" → §十八 分支
- §九 验证命令 #18-#22（5 模式 grep + 文档存在性）
- `examples/cognitive_meta_demo/` mock-mode 极简示例（opt-in, 默认 OFF）
- 1 个 Catch2 smoke test case

**Deferred to follow-up** (not implemented here, 由 §18.10 触发):
- Step 4: IAgent 子接口（`ICognitiveAgent : public IAgent`）— 需待 T3/T4 AgentWorker（Sprint 24+）实装后
- Step 5: `IBehavior` 统一抽象 + `BehaviorOrchestrator` 场景智能体 — 待 credit-assignment ADR ship + S4 promotion criteria
- Step 6: 跨进程 cognitive (ADR-0077 gRPC descoped) — Phase 7+

---

## File Structure

### Production Docs

| File | Responsibility |
|---|---|
| `docs/architecture/agent-orchestration-architecture-2026-08.md` | 新增 §十八 (5 模式目录表 + decision tree branch + cross-ref §七 P1 Meta-Agent) |
| `docs/architecture/agent-orchestration-architecture-2026-08.md` §九 | 新增 #18-#22 验证命令 (5 模式 grep + stream-pipeline V2 占位标识 + debate-round 配方标识) |

### Example (opt-in)

| File | Responsibility |
|---|---|
| `examples/cognitive_meta_demo/agenticdsl-cc-demo.cc.md` | 1 个 YAML DSL 文件示例 sync-delegate + fan-out 模式 (mock-mode runnable) |
| `examples/cognitive_meta_demo/README.md` | Demo 说明 + AGENTICDSL_BUILD_EXAMPLES opt-in flag 提示 |
| `examples/CMakeLists.txt` | 新增 `add_executable(cognitive_meta_demo ...)` 仅当 `AGENTICDSL_BUILD_EXAMPLES=ON` |
| `tests/test_cognitive_meta_demo.cpp` | 1 Catch2 smoke case: 验证 `cognitive_meta_demo` executable 编译 + 启动 |

---

## TDD 5-Step Execution (Red → Green → Commit)

### Step 1: Write failing test for §十八 文档存在性 + 决策树分支

**File**: `tests/test_orchestration_doc_v18.cpp` (new, ~30 LOC)

```cpp
// Phase 0: Red test - 验证 §十八 文档结构存在性 (静态分析)
TEST_CASE("orchestration doc v1.5 §十八 contains 5 coordination patterns",
          "[docs][orchestration][v18]") {
    std::ifstream doc("docs/architecture/agent-orchestration-architecture-2026-08.md");
    REQUIRE(doc.is_open());
    std::stringstream ss; ss << doc.rdbuf();
    std::string content = ss.str();
    
    // 5 模式名验证
    REQUIRE(content.find("sync-delegate") != std::string::npos);
    REQUIRE(content.find("fan-out") != std::string::npos);
    REQUIRE(content.find("hierarchical-plan") != std::string::npos);
    REQUIRE(content.find("debate-round") != std::string::npos);
    REQUIRE(content.find("stream-pipeline") != std::string::npos);
    
    // V2 占位标识
    REQUIRE(content.find("V2 占位") != std::string::npos);
    
    // 配方标识 (debate-round 非原语)
    REQUIRE(content.find("组合配方") != std::string::npos);
}
```

**Verification**:
```bash
cd build && cmake --build . --target test_orchestration_doc_v18
ctest -R "orchestration_doc_v18" --output-on-failure
# Expected: FAIL (doc 还没有 §十八)
```

---

### Step 2: Add §十八 to orchestration doc + decision tree branch + §九 verification commands

**File**: `docs/architecture/agent-orchestration-architecture-2026-08.md`

在 §十七 之前插入:

```markdown
## 十八、Cognitive-Cognitive 协调模式目录

> **Oracle 判定**: 🟢 Go (sessions ses_faf7caad0... + ses_faf7e5317... + ses_faf7b0a43... + ses_faf77e870...)
> 5 模式沉淀自 `IAgentComposition` 4 模式（ADR-0060）+ 3 PDK Loop（ADR-0021）+ GEPALoop + MCTSWorkflowSearch + IInteractionBus
> V2 占位 `stream-pipeline` + 组合配方 `debate-round` 显式标注, 与 ADR-0085 §决策 5 Meta-Agent V1 defer 一致

### 18.1 模式目录表

| 模式名 | 原语锚点 | 适用 | 落地状态 |
|--------|----------|------|----------|
| `sync-delegate` | `IAgentComposition::delegate()` (`iagent_composition.h:59`) | 父子 agent 同步委派 | ✅ Shipped |
| `fan-out` | `IAgentComposition::call_async()` (`iagent_composition.h:53`) | N 路并发 fan-out + 聚合 | ✅ Shipped |
| `hierarchical-plan` | `PlanExecuteLoop::run(goal, ctx, token)` + `plan_phase`/`execute_phase`/`verify_phase` | 规划-执行-验证 3 阶段 | ✅ Shipped |
| `debate-round` | `IAgentComposition::call_async()` + `GEPALoop::reflect_and_commit()` + `IEvaluator` 组合 | 多 cognitive 反思 + 评估 | 🟡 组合配方 (3 组件各自 ✅ Shipped, 无单一原语) |
| `stream-pipeline` | `IAgentComposition::stream()` (`iagent_composition.h:64-67`) | 流式认知管道 | 🔴 V2 占位 (代码 `throw std::logic_error("Phase 2 - stream not yet implemented")`) |

### 18.2 与 cap-map 17 类应用映射

| 应用代号 | 推荐模式 | 备选 |
|---------|----------|------|
| A1-A2 | sync-delegate | — |
| A3-A4 | fan-out + hierarchical-plan | — |
| A5-A6 | sync-delegate + HookPattern L1+L4 | — |
| B1-B3 | fan-out + CompositionPattern | — |
| B4 | stream-pipeline (V2) | fan-out |
| B5-B7 | hierarchical-plan + GEPALoop | debate-round |
| C1-C4 | fan-out (跨进程扩展) | — |

### 18.3 决策树分支 (§四 追加)

```
├─ 需要认知 agent 互相协调？ ──是──▶ 选 §十八 5 模式 (按 18.2 映射)
│   ├─ 同步等待子 agent? ──sync-delegate
│   ├─ 并行分发? ──fan-out
│   ├─ 规划-执行-验证? ──hierarchical-plan
│   ├─ 反思-评估-修订? ──debate-round (组合)
│   └─ 流式 partial result? ──stream-pipeline (V2 占位)
```

### 18.4 与 ADR-0085 §决策 5 关系

本节目录是 Meta-Agent 概念的需求**消化层**(列出 cognitive 协调模式 + 命名), 非 MetaAgent 组件前置设计。
ADR-0085 §决策 5 明确 V1 不实施 MetaAgent 自管理; V2 (Phase 6a+) 引入 `CrossCuttingMetaAgent` 时, §十八 5 模式是其调用目标。

### 18.5 示例: `examples/cognitive_meta_demo/` (AGENTICDSL_BUILD_EXAMPLES opt-in)

```markdown
### AgenticDSL `/__meta__`
```yaml
version: "1.0"
mode: coordination_demo
```

### AgenticDSL `/demo/sync_delegate`
```yaml
type: tool_call
tool: "react-loop::react"
args:
  prompt: "{{ task }}"
next: "/demo/aggregate"

### AgenticDSL `/demo/aggregate`
```yaml
type: assign
assign:
  result: "{{ sync_delegate.result }}"
```

### AgenticDSL `/main/start`
```yaml
type: start
next: "/demo/sync_delegate"
```

### AgenticDSL `/main/end`
```yaml
type: end
termination_mode: hard
```
```

### 18.6 §九 验证命令 (新增 #18-#22)

```bash
# 18. §十八 5 模式目录存在
grep -c "^## 十八\|sync-delegate\|fan-out\|hierarchical-plan\|debate-round\|stream-pipeline" \
  docs/architecture/agent-orchestration-architecture-2026-08.md

# 19. stream-pipeline V2 占位标识
grep -c "V2 占位\|stream-pipeline.*占位\|iagent_composition.h:64" \
  docs/architecture/agent-orchestration-architecture-2026-08.md

# 20. debate-round 组合配方标识
grep -c "组合配方\|debate-round.*组合" \
  docs/architecture/agent-orchestration-architecture-2026-08.md

# 21. §四 决策树新分支插入
grep -c "需要认知 agent 互相协调\|§十八" \
  docs/architecture/agent-orchestration-architecture-2026-08.md

# 22. 17 类应用代号映射完整性
for app in A1 A2 A3 A4 A5 A6 B1 B2 B3 B4 B5 B6 B7 C1 C2 C3 C4; do
  grep -c "\\*\\*${app}\\*\\*" \
    docs/architecture/agent-orchestration-architecture-2026-08.md | xargs -I{} echo "$app: {}"
done
```

---

## §四 决策树分支追加 (修订)

在 §四 决策树文本末尾追加:
```
├─ 需要认知 agent 互相协调？ ──是──▶ §十八 5 模式目录
```

---

## §变更记录追加

| 日期 | 版本 | 变更 |
|------|------|------|
| 2026-08-30 | v1.3 | (1) §十八 cognitive-cognitive 协调模式目录新增（5 模式 + 17 应用映射 + 决策树分支 + V2 占位标识 + 组合配方标识）；(2) §四 决策树新增分支；(3) §九 验证命令 #18-#22；(4) 示例代码 `examples/cognitive_meta_demo/`（opt-in 默认 OFF） |
```

**Verification**:
```bash
cd build && cmake --build . --target test_orchestration_doc_v18
ctest -R "orchestration_doc_v18" --output-on-failure
# Expected: PASS (1 case / 7 assertions)
```

---

### Step 3: Commit docs only (no example yet)

```bash
git add docs/architecture/agent-orchestration-architecture-2026-08.md \
        tests/test_orchestration_doc_v18.cpp \
        tests/CMakeLists.txt
git commit -m "docs(orchestration): §十八 cognitive-cognitive 协调模式目录 + 决策树分支 + 验证命令 (Phase 0)"
```

---

### Step 4: Add example (opt-in) + Catch2 smoke test

**File**: `examples/cognitive_meta_demo/agenticdsl-cc-demo.cc.md` (above content from §18.5)

**File**: `examples/cognitive_meta_demo/README.md`:
```markdown
# Cognitive-Cognitive Coordination Demo (Mock-Mode)

> **⚠️ Opt-in**: This demo is built only when `cmake -DAGENTICDSL_BUILD_EXAMPLES=ON`.

Demonstrates 5 coordination patterns from `agent-orchestration-architecture-2026-08.md` §十八:
- `sync-delegate` (✅ Shipped via IAgentComposition::delegate)
- `fan-out` (✅ Shipped via call_async)
- `hierarchical-plan` (✅ Shipped via PlanExecuteLoop)
- `debate-round` (🟡 Combination recipe — uses call_async + GEPALoop + IEvaluator)
- `stream-pipeline` (🔴 V2 placeholder — not yet implemented)

## Run

```bash
cmake -DAGENTICDSL_BUILD_EXAMPLES=ON ..
make cognitive_meta_demo
./examples/cognitive_meta_demo/cognitive_meta_demo
```

Expected output: `mock-mode coordination demo executed`.
```

**File**: `examples/cognitive_meta_demo/main.cpp`:
```cpp
#include <iostream>
int main() {
    std::cout << "mock-mode coordination demo executed" << std::endl;
    return 0;
}
```

**File**: `examples/CMakeLists.txt` (add):
```cmake
if(AGENTICDSL_BUILD_EXAMPLES)
    add_executable(cognitive_meta_demo
        cognitive_meta_demo/main.cpp
        cognitive_meta_demo/agenticdsl-cc-demo.cc.md
    )
    target_link_libraries(cognitive_meta_demo PRIVATE agenticdsl_core)
endif()
```

**File**: `tests/test_cognitive_meta_demo.cpp` (new, ~20 LOC):
```cpp
#include <cstdlib>
#include <filesystem>
#include <string>
#include <catch2/catch_test_macros.hpp>

TEST_CASE("cognitive_meta_demo compiles and runs in mock-mode",
          "[examples][opt-in][v18]") {
    const std::string build_dir = std::getenv("BUILD_DIR") ?: "build";
    std::filesystem::path exe = std::filesystem::path(build_dir)
        / "examples/cognitive_meta_demo/cognitive_meta_demo";
    if (!std::filesystem::exists(exe)) {
        SKIP("AGENTICDSL_BUILD_EXAMPLES=OFF, cognitive_meta_demo not built");
    }
    // simple run + output check (mock-mode should output success message)
    std::string cmd = exe.string() + " 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    REQUIRE(pipe != nullptr);
    char buf[256]; std::string output;
    while (fgets(buf, sizeof(buf), pipe)) output += buf;
    pclose(pipe);
    REQUIRE(output.find("mock-mode coordination demo executed") != std::string::npos);
}
```

**Verification**:
```bash
cmake -DAGENTICDSL_BUILD_EXAMPLES=ON ..
make cognitive_meta_demo test_cognitive_meta_demo
BUILD_DIR=$(pwd) ctest -R "cognitive_meta_demo" --output-on-failure
# Expected: PASS (1 case)
```

---

### Step 5: Commit example + test + docs/README update

```bash
git add examples/cognitive_meta_demo/ examples/CMakeLists.txt tests/test_cognitive_meta_demo.cpp docs/README.md
git commit -m "examples(cognitive_meta_demo): mock-mode coordination demo + smoke test (Phase 1)"
```

---

## Ship Gate Validation

Before merge, run all checks:

```bash
# 1. openspec validate
openspec validate 2026-08-30-meta-cognitive-coordination-doc --strict

# 2. ADR lint
python3 tools/adr_lint.py  # ✓ 无新 error/warning

# 3. Docs drift audit
python3 tools/docs_drift_audit.py  # 0 DRIFT (per commit 1f25821 baseline)

# 4. Compile + unit tests (opt-in path only)
cmake -DAGENTICDSL_BUILD_EXAMPLES=ON ..
cmake --build . --target test_orchestration_doc_v18 test_cognitive_meta_demo
ctest --output-on-failure -R "orchestration_doc_v18|cognitive_meta_demo"

# 5. Baseline regression (no opt-in)
cmake -DAGENTICDSL_BUILD_EXAMPLES=OFF ..
cmake --build .
ctest --output-on-failure  # 204/204 unchanged

# 6. LSP discipline
./scripts/check-lsp-discipline.sh --quick  # ✓

# 7. §九 验证命令 #18-#22
bash -c "$(grep -A 5 '# 18\\.\\|# 19\\.\\|# 20\\.\\|# 21\\.\\|# 22\\.' \
  docs/architecture/agent-orchestration-architecture-2026-08.md)"
```

Expected: all PASS, no regression, 0 drift, 0 ADR warning.

---

## Risk Assessment

| 风险 | 缓解 |
|------|------|
| §十八 5 模式名与其他文档（如 cap-map §三）漂移 | 决策树分支 + 17 应用代号映射表 双重锁定 |
| example 默认编译泄漏到 baseline | `AGENTICDSL_BUILD_EXAMPLES` opt-in flag (默认 OFF,沿用 Sprint 19 先例) |
| stream-pipeline 误用为已实现能力 | §18.1 表显式标 V2 占位 + 引用 `iagent_composition.h:67` 代码行 |
| debate-round 误用为单一原语 | §18.1 表显式标 "组合配方" + §18.4 链接 ADR-0085 §决策 5 |
| §七 P1 Meta-Agent 行未同步引用 §十八 | §十七 changelog 引用 §十八 + §18.4 引用 ADR-0085 决策 5 |
| Oracle M1 (is_valid_json_schema_type) 误植 | §十八 不涉及 JSON Schema 校验 (走 §14 Cognitive↔Domain 机制,非 sigval) |

---

## 后续触发条件 (per §18.10)

| 触发条件 | 后续 OpenSpec change |
|----------|---------------------|
| AgentWorker (Sprint 24+ G3 open) 实施完成 | IAgent V2 amendment: `ICognitiveAgent : public IAgent` 子接口 (2.2) |
| ADR-0086 credit-assignment ✅ Shipped + S4 promotion criteria 通过 | MCTS Axis6 第 6 轴扩展 (OpenSpec change `2026-08-31-mcts-axis6-cognitive-domain`) |
| Phase 6a Wave 2 启动 | §十八 5 模式接入 BehavioralEquivalenceEvaluator (ADR-0083 V2) |
| Phase 6c execution-baseline handoff | execution-baseline / evidence-gate / execution-dsl 3 个 from-roadmap-phase-6c-* change 解锁 |
