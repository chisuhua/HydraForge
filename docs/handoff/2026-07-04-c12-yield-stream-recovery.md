# C12 YIELD/STREAM Node Implementation — Recovery Handoff

**Date**: 2026-07-04
**Branch**: main
**Status**: ⏸ Paused — recoverable state
**Change**: `openspec/changes/2026-07-03-phase5-stage1-step2-yield-stream/`

---

## TL;DR

7 commits shipped, 63/63 ctest zero regression, `openspec validate` exit 0. 基础设施问题:**3/3 deep agents timed out at 30min with zero output**. Tier 1-B + 1-C + 2-A implemented directly by orchestrator. §2 (parser), §5 (topo_scheduler), §6 (budget), §7 (tests), §8a (example), §8 (validation), §9 (archive) pending.

**Continue from here**: §2 markdown_parser (§2 5 lines, isolated, low risk — try delegating first or implement directly).

---

## ✅ Shipped (7 commits, all on main)

| Hash | Subject | Tasks |
|---|---|---|
| `2654229` | chore(c12): Oracle Q1-Q4 + Risk 8-12 consistency fixes | docs only |
| `75a00b5` | docs(c12): align Phase 5 master plan C12 row | docs only |
| `801c67c` | feat(c12): NodeType::YIELD + YieldMode enum + YieldNode polymorphic subclass | §1.1-1.5 |
| `72a4b29` | feat(c12): YieldState + pending_yield_ + BudgetExceededException + yield_mutex_ | §4.1-4.3 + §6.0 + §6b.1-2 |
| `c28a8dd` | feat(c12): YieldStreamBridge for IGenerationStream pull-based bridge | §6a.1-3 |
| `f97e2d7` | feat(c12): execute_yield() + dispatch switch YIELD case | §3.1-3.8 + §6a.4 |
| `971efdd` | chore(c12): mark §3 + §6a.1-4 complete in tasks.md | tasks checkbox |

**Files modified**:
- `src/core/types/node.h` (+45 行: NodeType::YIELD, YieldMode, YieldNode)
- `src/modules/scheduler/execution_session.h` (+37 行: YieldState, BudgetExceededException, pending_yield_, yield_mutex_, 3 accessors)
- `src/modules/scheduler/execution_session.cpp` (+17 行: 3 accessor 实现)
- `src/modules/executor/yield_stream_bridge.h` (NEW, 38 行)
- `src/modules/executor/yield_stream_bridge.cpp` (NEW, 47 行)
- `src/modules/executor/node_executor.h` (+2 行: execute_yield 声明)
- `src/modules/executor/node_executor.cpp` (+71 行: execute_yield 实现 + includes + dispatch case)
- `src/modules/executor/CMakeLists.txt` (+1 行: yield_stream_bridge.cpp)
- `openspec/changes/.../tasks.md` (任务 checkbox 标记)
- `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` (master plan 对齐)

---

## ⏳ Pending Work (in dependency order)

### Tier 1-A — §2 MarkdownParser (5 items)
- **File**: `src/modules/parser/markdown_parser.cpp/h`
- **Scope**: 解析 `{"type": "yield", "yield_value": "...", "mode": "next|continue|stop", "stop_path": "..."}` → YieldNode
- **Pattern**: Follow existing ASSIGN/TOOL_CALL case in `create_node_from_json`
- **Risk**: LOW (isolated, additive)
- **Suggestion**: Try `task(category="deep")` again OR implement directly (~30 lines)

### Tier 2-B — §5 TopoScheduler (5 items)
- **File**: `src/modules/scheduler/topo_scheduler.cpp` (705 lines)
- **Scope**:
  - 5.0: `enum class SchedulerState { RUNNING, YIELDED, COMPLETED, FAILED }`
  - 5.1: yield pause: 不跳出主 while 循环, 循环内检测 pending_yield_ 后挂起
  - 5.2: `resume_yield(session_id, token_value)` 公共方法
  - 5.3: DAG state 持久化: resume_context 保存 ready_queue + in_degree_table
- **Risk**: MEDIUM-HIGH (touches core scheduler loop, Sprint 18 simplify commit context)
- **Suggestion**: Read `topo_scheduler.h` first (esp. `resume_yield` related signatures), then implement carefully

### Tier 2-C — §6 Budget Integration (4 items)
- **File**: `src/modules/executor/node_executor.cpp` + `src/modules/scheduler/execution_session.cpp`
- **Scope**: Replace current `[]() { return true; }` no-op BudgetChecker with real budget check
- **Link cycle constraint**: executor can't call `session_->is_budget_exceeded()` (link cycle)
- **Solution pattern**:
  - ExecutionSession::execute_node wraps YIELD execution with proper BudgetChecker lambda capturing `this`
  - OR pass BudgetChecker via execute_yield parameter (API change)
- **Risk**: MEDIUM (link cycle workaround already documented in code)

### Tier 3 — §7 Tests (10 items)
- **File**: NEW `tests/test_yield_node.cpp`
- **Scope**: 8-10 Catch2 test cases covering NEXT/CONTINUE/STOP + ASan/TSan
- **Pattern**: Follow `tests/test_executor.cpp` (MockLLMProvider for streaming)
- **Risk**: LOW (additive test file)

### Tier 4 — §8a Example (5 items)
- **File**: NEW `examples/phase5_yield_token_generator/`
- **Scope**: E2E demo with `--mock` flag, N 次调用返回 N tokens
- **Pattern**: Follow `examples/slice_01_tool_call/` (most recent --mock example)
- **Risk**: LOW (new directory, isolated)

### Tier 5 — §8 Validation (6 items)
- **Scope**: ctest + ASan + TSan + adr_lint + docs_drift + openspec validate
- **Risk**: LOW (verification only)

### Tier 6 — §9 Archive (5 items)
- **Scope**: git push + `openspec archive` + master plan §十一 调整日志
- **Risk**: LOW (mechanical)

---

## 🚨 Infrastructure Issue: 3/3 Deep Agents Failed

| Agent | bg_id | Session ID | Duration | Output |
|---|---|---|---|---|
| Tier 1-A (§2 parser) | bg_23b611c4 | ses_0d24a515bffeQRlcLbGZnLTMn9 | 36m20s | ZERO |
| Tier 1-B (§4+§6.0+§6b) | bg_d43384b7 | ses_0d24a50d4ffeKh4tddDaYsO1V2 | 33m13s | ZERO (did extensive reading then timed out) |
| Tier 1-C (§6a bridge) | bg_01a9f29a | ses_0d24a4f8cffeZN1CC7ESwoVb6k | 30m19s | ZERO |

**Pattern**: All 3 agents timed out at 30min inactivity with no file changes. Model used: `openstarry/glm-5.2` via category `deep`.

**Root cause hypothesis**: Either model load issue, queue depth, or systemic infrastructure problem.

**Workaround used**: Orchestrator took over directly for Tier 1-B + 1-C + 2-A. Worked reliably but more token-expensive.

**Recommendation**: Try `task(category="unspecified-high")` instead of `deep` for future delegations in this session. Or continue direct implementation.

---

## 🔑 Critical Design Decisions Made (must preserve)

### 1. Executor→Scheduler Link Cycle Prevention
**Problem**: `agenticdsl_modules_executor` cannot link `agenticdsl_modules_scheduler` (scheduler already depends on executor → cycle).

**Solution** (committed in f97e2d7):
- `execute_yield()` does NOT call `session_->set_pending_yield()` / `clear_pending_yield()` / `is_budget_exceeded()`
- Instead returns context keys for downstream processing:
  - `__yield__`: pulled token(s) string
  - `__yield_mode__`: NEXT/CONTINUE/STOP
  - `__yield_node_path__`: NEXT mode anchor
  - `__yield_budget_exceeded__`: CONTINUE over-budget marker
  - `__yield_stop_path__`: STOP mode jump target
  - `__yield_error__`: stream open failure marker
- `ExecutionSession::execute_node` (in `execution_session.cpp:188`) intercepts YIELD results and updates `pending_yield_` (TODO §5 integration)

**Rationale**: Both executor and scheduler need to handle YIELD state, but at different layers. Keeping executor "pure" (no scheduler calls) maintains link direction.

### 2. NULL LLM Provider Backward Compatibility
**Location**: `node_executor.cpp` execute_yield() early-return when `llm_provider_ == nullptr`

**Rationale**: Existing 63 tests may exercise mock paths without LLM. Throwing would break them.

### 3. CONTINUE Budget Check is Currently No-Op
**Location**: `node_executor.cpp` execute_yield() CONTINUE branch uses `[]() { return true; }`

**Status**: Intentional deferral to Tier 2-C (§6 budget). The infrastructure (BudgetExceededException + BudgetChecker parameter) is in place; the wiring is pending.

### 4. YieldStreamBridge stop_token is Default
**Location**: `yield_stream_bridge.cpp` constructs with `{}` (default stop_token)

**Rationale**: No upstream cancel source yet (no jthread integration). Future §5 resume_yield may wire proper stop_source.

### 5. YieldState module_path Semantics
**Structure**: `std::string module_path; nlohmann::json resume_context;`

**module_path**: Identifies which module triggered yield (for downstream resume routing)
**resume_context**: Free-form JSON containing DAG state (ready_queue + in_degree, populated by §5)

---

## 🔍 Quick Resume Checklist

```bash
cd /workspace/project/HydraForge

# 1. Verify clean state
git status  # should be clean
git log --oneline origin/main..HEAD  # 7 commits

# 2. Verify build + tests
cd build && cmake --build . -j$(nproc)
ctest --output-on-failure  # expect 63/63 pass
cd ..

# 3. Verify OpenSpec change
openspec validate 2026-07-03-phase5-stage1-step2-yield-stream --strict
# expect: "Change ... is valid"

# 4. Next work item: §2 MarkdownParser
# Read: src/modules/parser/markdown_parser.cpp/h
# Find: existing ASSIGN/TOOL_CALL case in create_node_from_json
# Add: case "yield" parsing yield_value/mode/stop_path
```

---

## 📊 Metrics

- **Commits**: 7 ahead of origin/main
- **Files changed**: 10 (4 new + 6 modified)
- **Lines added**: ~270 (excluding tasks.md/docs)
- **ctest**: 63/63 PASS, zero regression
- **Build**: zero errors, only pre-existing deprecation warnings
- **OpenSpec validate**: exit 0

---

## 📝 Original Request Context

User said: "执行1, 3,后开始实施" (execute items 1, 3, then start implementation)

Items 1, 3 from my earlier readiness check:
1. ✅ Commit working tree modifications (4 files, Oracle Q1-Q4 + Risk 8-12 fixes) → commits 2654229, 75a00b5
3. ✅ Start task §1 (NodeType + YieldMode + YieldNode) → commit 801c67c

"后开始实施" (then start implementation):
- ✅ Continued with Tier 1-B (recovered from agent failure): commit 72a4b29
- ✅ Continued with Tier 1-C (recovered from agent failure): commit c28a8dd
- ✅ Continued with Tier 2-A (§3 + §6a.4): commit f97e2d7
- ⏸ Paused at user request before §5 (topo_scheduler)

User's final message: "先暂停把当前进度打包成可恢复状态" (first pause and package current progress into recoverable state)

This document IS that package.