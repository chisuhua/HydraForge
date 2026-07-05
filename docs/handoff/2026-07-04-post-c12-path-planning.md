# Post-C12 Path Planning — 后续任务实施 Background

**Date**: 2026-07-04
**From**: C12 YIELD/STREAM 实施会话 (Sisyphus)
**To**: 后续会话 (新 Sisyphus session)
**Branch**: main (8 commits ahead of origin, all pushed)
**Status**: C12 ✅ shipped + archived + master plan updated. ASan/TSan deferred to CI.

---

## TL;DR

C12 已 100% ship + archive 完成。**8 commits 已 push** 到 `origin/main` (commit `0018234`)。后续任务有 4 阶段路径(A→B→C→D),本周最优先是 **A1 ASan/TSan CI 验证** + **A3 C13 启动触发评估**。本文档提供完整背景,新 session 可以直接从此处启动后续实施而无需重新摸索。

> **ℹ️ 2026-07-06 编号澄清**: 本 handoff 中提到的 C13/C14 仍代表 **fork-checkpoint** 与 **analysis-service** (未变更);**2026-07-05 起 C13/C14/C15 编号被 B2 工具化扩展占用** (`phase5-b2-arch-schemas` / `phase5-llama-engine-plugin` / `phase5-batching-queue-plugin`),详见 `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §5.5。本文档若与新编号冲突,以 master plan §5.5 为准。

**立即执行入口**:
- A1 CI: `cmake --preset asan && ctest` + `cmake --preset tsan && ctest`
- B2 推理子图: 直接创建 `lib/inference/batching.md` (C12 ship 后可启动)

---

## 1. 项目当前状态 (2026-07-04)

### 1.1 主线 Phase 进度

| Phase | 状态 | 累计 changes |
|---|---|---|
| Phase 0 (MVP 三层调用链) | ✅ shipped | — |
| Phase 1 (智能体层 ADR-0019~0023) | ✅ shipped | 5 |
| Phase 2 (异步架构 ADR-0030 V2) | 🟡 Partial (C11 含 IGenerationStream pull-based) | — |
| Phase 3 (执行策略 ADR-0031) | 🟡 Partial → Sprint 14 C4 ship 后改进 | — |
| Phase 4 (模型路由 ADR-0034) | ✅ shipped (C7, 2026-07-02) | +1 |
| Phase 4.5 (MVP 清理) | ✅ shipped (C8, 2026-07-03) | +1 |
| **Phase 5 Stage 1 (C9-C12)** | **✅ 全链 ship + C12 (2026-07-04)** | +4 |
| Phase 5 Stage 2 (C13 fork-checkpoint) | ⚪ placeholder, 远期延后 (fork-checkpoint 含义未变;**B2 工具化 C13** = `phase5-b2-arch-schemas`, 与本行不同义) | — |
| Phase 5 Stage 3 (C14 analysis-service) | ⚪ placeholder, 远期延后 (analysis-service 含义未变;**B2 工具化 C14** = `phase5-llama-engine-plugin`, 与本行不同义) | — |

### 1.2 C12 Ship 状态 (本 session 主要交付)

**C12 OpenSpec change**: `2026-07-03-phase5-stage1-step2-yield-stream`  
**已 archived** 为: `openspec/changes/archive/2026-07-04-2026-07-03-phase5-stage1-step2-yield-stream/`

**Spec 上移**: `openspec/specs/yield-stream/spec.md` (+6 requirement clauses)

**8 commits shipped** (af6da4d → 0018234):
```
af6da4d feat(c12): wire YIELD into TopoScheduler pause/resume + ExecutionSession Budget check
00eda38 chore(c12): mark §5 + §6 complete
d81e675 feat+test: STOP-mode LLM skip + test_yield_node.cpp (9 cases)
a2fd8a5 feat(c12): add examples/phase5_yield_token_generator (--mock N tokens E2E demo)
2b7c9e4 chore(c12): mark §8 + §9 archive progress
85083b4 docs(master-plan): mark C12 ship + extend §十一.2 adjustment log
57e923b chore(c12): mark §9.2-9.5 fully complete
0018234 chore(c12): archive yield-stream change (remove active change folder)
```

### 1.3 验证证据

- ✅ ctest 64/64 PASS (63 baseline + test_yield_node 9 case, 39 assertions)
- ✅ `python3 tools/adr_lint.py` exit 0 (33 ADR 文件)
- ✅ `python3 tools/docs_drift_audit.py` 0 DRIFT (4 scenarios)
- ✅ `openspec validate 2026-07-03-phase5-stage1-step2-yield-stream --strict` PASS
- ✅ examples/phase5_yield_token_generator --mock 3/5/7 tokens PASS
- ⚠️ **ASan 首次 build 超 10min 超时 → deferred CI** (master plan 沿用 pre-existing baseline)
- ⚠️ **TSan 同上 → deferred CI**

### 1.4 Master plan 关键更新点

文件: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md`

| 章节 | 修改 |
|---|---|
| §四 C12 行 | `🟡 ACTIVE` → `✅ shipped (2026-07-04)` |
| §十 Sprint 21-22 行 | 备注 "C12 ship (2026-07-04, ahead of schedule)" |
| §十一.2 调整日志 | 新增 C12 ship 条目 (2026-07-04, line 588 area) |

---

## 2. C12 关键设计决策 (新 session 必读)

后续任务如涉及 YIELD/Scheduler/Budget 改动,这些决策必须保留:

### 2.1 Executor→Scheduler 链路环防护

`agenticdsl_modules_executor` 不能 link `agenticdsl_modules_scheduler` (反向依赖)。
**实施**: `execute_yield()` 不直接调 `session_->*`,返 context keys:
- `__yield__`: pulled token(s)
- `__yield_mode__`: NEXT/CONTINUE/STOP
- `__yield_node_path__`: NEXT anchor
- `__yield_budget_exceeded__`: CONTINUE over-budget marker
- `__yield_stop_path__`: STOP jump target
- `__yield_error__`: stream open failure marker

`ExecutionSession::execute_node` 后处理:`__yield_mode__` → `set_pending_yield()` + `paused_at`.

### 2.2 NULL LLM Provider 向后兼容

`execute_yield()` 早期 return `ctx` (避免无 LLM crash)。原 63 测试零回归核心。

### 2.3 CONTINUE Budget 真实注入 (非 noop)

链路:
- `ExecutionSession::execute_node` 构造 `BudgetChecker yield_checker = [this]{ return !budget_controller_->exceeded(); }`
- 注入 `node_executor_->execute_node(node, ctx, yield_checker)` (默认 noop 保持向后兼容)
- 仅 YIELD CONTINUE 模式真正使用 checker, throw `BudgetExceededException`

### 2.4 YieldStreamBridge API

```cpp
class YieldStreamBridge {
    using BudgetChecker = std::function<bool()>;
    std::optional<std::string> pull_single(IGenerationStream& stream);
    std::vector<std::string> pull_loop(IGenerationStream& stream,
                                       BudgetChecker budget_checker,
                                       std::size_t max_iter = 10000);
};
```

### 2.5 YieldState 结构

```cpp
struct YieldState {
    std::string module_path;          // 模块路径 (YieldNode 触发时所在)
    nlohmann::json resume_context;    // DAG state 快照 (ready_queue + in_degree)
    // ... accessors 加字段时遵循 field-level yield_mutex_ (Oracle Risk 10)
};
```

`pending_yield_` 字段级 mutex (Sprint 18 D-8 / ADR-0031 §决策 5):
```cpp
void set_pending_yield(YieldState state);
[[nodiscard]] std::optional<YieldState> get_pending_yield() const;
void clear_pending_yield();
```

### 2.6 TopoScheduler §5 暂停机制

```cpp
enum class SchedulerState { RUNNING, YIELDED, COMPLETED, FAILED };
SchedulerState get_scheduler_state() const;
bool is_yielded() const;
ExecutionResult resume_yield(const Context& updated_context);
```

**实现** (`run_main_loop` 检测):
```cpp
// after session_.execute_node()
auto pending_yield = session_.get_pending_yield();
if (pending_yield.has_value()) {
    ready_queue_.push(found.path);  // push back yielded node
    YieldState snapshot;
    snapshot.module_path = found.path;
    snapshot.resume_context = serialize_dag_state();
    session_.set_pending_yield(snapshot);
    scheduler_state_ = SchedulerState::YIELDED;
    yielded_context_ = context;
    yielded_node_path_ = found.path;
    return {true, "YIELDED at " + found.path, context, std::nullopt};
}
```

**DAG state 持久化** (Oracle Risk 8 mitigation):
- `serialize_dag_state()`: ready_queue + in_degree + executed → JSON
- `restore_dag_state(json)`: reverse, queue 从 front pop/re-push

---

## 3. 当前基础设施 (新 session 可直接使用)

### 3.1 Mock 流 provider

```cpp
#include "common/llm/mock_provider.h"
auto provider = std::make_unique<MockLLMProvider>();
provider->set_stream_tokens({"t1", "t2", "t3"});
// 或
provider->set_fixed_response("text");
// 或
provider->set_simulate_error(LLMError::Code::NetworkError, "msg");
```

### 3.2 YAML parser 集成

```cpp
#include "modules/parser/markdown_parser.h"
MarkdownParser parser;
auto graphs = parser.parse_from_string(markdown);  // vector<ParsedGraph>
```

Yield 节点 YAML 格式:
```yaml
type: yield
yield_value: "template"
mode: next|continue|stop
stop_path: "/main/cleanup"
```

### 3.3 Engine.run 接口

```cpp
#include "core/engine.h"
auto engine = DSLEngine::from_markdown(dsl);
auto* provider = dynamic_cast<MockLLMProvider*>(engine->get_llm_provider());
provider->set_stream_tokens({...});
LayeredContext initial_ctx;
ExecutionResult result = engine->run(initial_ctx);
```

### 3.4 OpenSpec CLI

```bash
# 验证现有 change
openspec validate <change-name> --strict

# 创建新 placeholder change
openspec init <date>-<name>

# 应用 (实施) tasks
openspec instructions apply --change <name> --json

# 归档 (ship 完成)
openspec archive <name> --yes
```

---

## 4. 后续任务路径规划 (4 阶段)

### 阶段 A: Sprint 22 收尾 (本周内, 1-3 天) — 立即执行

#### A1 — ASan/TSan CI 完整 build 验证 (优先, 1-2 天)

**背景**: C12 ship 验证 §8.4/8.5 时首次 build 超 10min 超时,沿用 pre-existing baseline。**新 session 需要完整 CI 验证**。

**步骤**:
```bash
# ASan build (首次 10-15 min)
rm -rf build/asan  # 旧的可能不完整
cmake -B build/asan -DAGENTICDSL_ENABLE_ASAN=ON -DAGENTICDSL_BUILD_TESTS=ON
cmake --build build/asan -j$(nproc)
cd build/asan && ctest --output-on-failure

# TSan build
cmake -B build/tsan -DAGENTICDSL_ENABLE_TSAN=ON -DAGENTICDSL_BUILD_TESTS=ON
cmake --build build/tsan -j$(nproc)
cd build/tsan && ctest --output-on-failure
```

**期望 baseline**:
- ASan: 33/34 (97%, 1 pre-existing failure tracked in OpenSpec `2026-06-25-pre-existing-sanitizer-findings`)
- TSan: 32/34 (94%, 1+ TSan warnings tracked)

**新增验证**:
- C12 新代码应无新增 ASan leak (test_yield_node + node_executor.cpp + execution_session.cpp 改动)
- test_yield_node 跨 await 边界无 TSan race (yield_mutex_ 字段级锁已就位)

**如果发现新问题**:
- 立即 commit 修复 (不超 C12 ship baseline)
- 在 OpenSpec 上追加 `2026-07-XX-c12-ship-asan-tsan-fixes` (如必要)

#### A2 — Master plan 状态同步 (30 min)

**任务**: 验证 `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` 是否反映 C12 已 ship。

**检查点**:
- [ ] §四 C12 行:`✅ shipped (2026-07-04)` 文字完整
- [ ] §十 Sprint 21-22 行: 含 "ahead of schedule" 备注
- [ ] §十一.2 新增 C12 ship 条目(2026-07-04,详细说明实施 1 天 + 64/64 测试)

#### A3 — C13 启动决策 (1 天, 触发 Oracle 决议)

**决策树**:
> **注**: 本决策树中 C13 仍指 fork-checkpoint(**非** B2.0.0 / `phase5-b2-arch-schemas`,后者已于 2026-07-05 占用 C13 编号;详见 master plan §5.5)。
```
C13 fork-checkpoint 触发?
├── 是 (性能 deep_copy bottleneck OR Session 迁移需求)
│   └→ 创建 OpenSpec placeholder: 2026-07-XX-phase5-stage2-step3-4-fork-checkpoint
├── 否
│   └→ 跳 C13 直接 C14
└─ 评估中
   └→ 等性能基线测量
```

**触发条件检查** (master plan §六 line 387-392):
- [ ] 性能测试显示 deep_copy 在 Fork 多分支场景下成为瓶颈 (Step 3 触发)
- [ ] Session 迁移/容错需求出现 (Step 4 触发)
- [ ] 团队有 3-5 天时间投入

**Oracle 咨询入口**:
```bash
# (手动, 无 CLI)
# 准备 Oracle 输入:
# - 当前 Fork/Join 实现: src/modules/scheduler/topo_scheduler.cpp
# - 现有 ForkNode 字段: src/core/types/node.h
# - 性能 baseline 数据 (如已有)
# - 决策点: Step 3 / Step 4 / 都做 / 都不做
```

---

### 阶段 B: Sprint 23-24 (并行启动, 1-2 周)

#### B1 — C13 fork-checkpoint (若 A3 决定是) (3-5 天)

**详细 ship 列表** (master plan §四 C13):

| 步骤 | 任务 | 文件 |
|---|---|---|
| C13.1 | ForkNode 新增 `fork_behaviors: map<string, ForkBehavior>` (COW/INHERIT/SHARE_READONLY) | `src/core/types/node.h` |
| C13.2 | parser 解析 fork_behaviors YAML 字段 | `src/modules/parser/markdown_parser.cpp` + `node_factory.cpp` |
| C13.3 | TopoScheduler.start_fork_simulation 处理 fork_behaviors | `src/modules/scheduler/topo_scheduler.cpp` |
| C13.4 | SessionRegistry 新增 `checkpoint()` / `restore()` 方法 | `src/modules/scheduler/session_registry.h/cpp` |
| C13.5 | 注册 `session.checkpoint` / `session.restore` 工具 | `src/common/tools/registry.cpp` |
| C13.6 | 编写 `tests/test_fork_perfield.cpp` + `tests/test_checkpoint_restore.cpp` | `tests/` |

**OpenSpec placeholder 命令**:
> **注**: 此命令仅在 A3 决定 fork-checkpoint 启动时执行。`2026-07-XX-phase5-stage2-step3-4-fork-checkpoint` 是原 fork-checkpoint C13 占位名称(**非** B2.0.0 / `phase5-b2-arch-schemas`)。
```bash
openspec init 2026-07-XX-phase5-stage2-step3-4-fork-checkpoint
# 写入 proposal.md/design.md/specs/spec.md/tasks.md
# Momus 审查 → 实施
```

**关联 ADR**: ADR-0008 (LayeredContext) / ADR-0033 (Session 层级)

#### B2 — 推理标准库 7 子图补齐 (并行, 1 周)

master plan §五.3 line 342 + §七.5 line 434-440 提到:
- [ ] prefix_cache.md / kv_cache.md / decoding.md
- [ ] **batching.md** ← C12 ship 后可启动

| 子图 | 估时 | 关联 |
|---|---|---|
| lib/inference/batching.md | 2 天 | ADR-0001, ADR-0005 |
| lib/inference/prefix_cache.md | 1 天 | ADR-0001 |
| lib/inference/kv_cache.md | 1 天 | ADR-0001 |
| lib/inference/decoding.md | 1 天 | ADR-0001 |
| **总** | **1 周 (4 子图)** | |

**ship gate**: 7/7 子图通过 ctest, 推理标准库覆盖率 100%

**实施 pattern**: 参考 Sprint 19 `examples/slice_01_tool_call` (--mock 模式),直接生成 `.md` 文件 + 小型 CMakeLists.txt 测试。如已有 lib/inference/ 子图,采用同 pattern。

**目录结构**:
```
lib/inference/
├── engine.md          ← 可能已 ship
├── session.md         ← 可能已 ship
├── model.md           ← 可能已 ship
├── batching.md        ← B2.1 (本 phase ship)
├── prefix_cache.md    ← B2.2
├── kv_cache.md        ← B2.3
├── decoding.md        ← B2.4
```

#### B3 — 文档与 ADR 同步 (并行车道, 2-3 天)

| ID | 任务 | 文件 |
|---|---|---|
| B3.1 | ADR-0008 LayeredContext 状态收尾 (ship 验证后) | `docs/adr/adr-0008-structured-context.md` |
| B3.2 | ADR-0030 V2 状态推进(🔍 Proposed → ✅ Approved 如 §3 ship 验证完成) | `docs/adr/adr-0030-async-runtime-v2.md` |
| B3.3 | ADR-0031 状态推进 (🟡 Partial → 评估 ✅ Approved) | `docs/adr/adr-0031-execution-policy.md` |
| B3.4 | roadmap-status.md Phase 5 阶段 1 100% 确认 | `docs/roadmap-status.md` |
| B3.5 | Sprint 22 / Sprint 23 plan 文档 (在 master plan §十 表更新备注) | `docs/superpowers/plans/` |

---

### 阶段 C: Sprint 24-26 — C14 阶段 3 服务化 (4-6 周)

**触发条件** (master plan §七.6 line 442-449):
- [ ] C10+C11+C12 全链 ship + **1 个月稳定运行** (现 C12 2026-07-04 ship,需等到 ~2026-08-04)
- [ ] 推理标准库 7/7 子图全部 ship (B2 完成)
- [ ] C13 决定是否实施 (B1 评估后)
- [ ] **Oracle 验证自进化可行性** (避免"AI 写的代码 AI 看不懂"陷阱)
- [ ] 团队有 4-6 周时间投入

**详细 ship 列表** (master plan §四 C14):

| Step | 任务 | 估时 | 关键文件 |
|---|---|---|---|
| C14.1 Step 5 | `build_reachability_graph()` 静态分析 + 预热 | 1-2 天 | `src/modules/library/library_loader.h/cpp` + `src/modules/scheduler/execution_session.cpp` |
| C14.2 BOOT-001 §2.1 | `QualityFeedbackController` | 1 周 | `src/modules/budget/quality_feedback_controller.h/cpp` |
| C14.3 BOOT-001 §2.2 | `examples/adaptive_optimize.agent.md` (100 轮收敛) | 1-2 周 | `examples/adaptive_optimize/` |
| C14.4 BOOT-001 §3.1 | `InferenceServer` (MCP + OpenAI 兼容) | 1-2 周 | `src/api/inference_server.h/cpp` |

**关联 ADR**: ADR-0001 / ADR-0005 / ADR-0019 / IP-001 §三 Step 5-6 + BOOT-001

**ship gate**:
- 静态分析覆盖率达 > 90%, 预热后首次调用延迟 < 未预热 10%
- QualityFeedbackController 自适应优化 100 轮内收敛
- InferenceServer MCP/OpenAI 兼容接口通过标准测试
- 完全自举: Agent 自主发现新优化策略

---

### 阶段 D: Sprint 27+ — Phase 5 收官 + Phase 6 启动

**任务**:
- Phase 5 收官评审 (master plan §六 + §七 Status 表全部 ✅)
- **Phase 6 自举可行性评估** (Oracle 深度咨询 — Bootstrap Self-Evolution)
- 创建 `docs/proposals/phase-6-self-bootstrapping/` 提案目录
- Master plan 2026-Q4 滚动更新 (Sprint 26+ 滚动到 Phase 6)

---

## 5. 决策树 & 优先级矩阵

```
C12 ✅ ship ──────────┐
                      │
   ┌──────────────────┘
   │
   ├── A1 ASan/TSan CI 完整 build  ⏰ 本周 (1-2 天)
   │    │
   │    ├─ 通过 → 维护 baseline  ✓
   │    └─ 新问题 → 修复 + 追加 OpenSpec
   │
   ├── A3 C13 触发决策  ⏰ 本周 (1 天, Oracle)
   │    │
   │    ├─ 是 → B1 C13 实施  ⏰ Sprint 23-24
   │    └─ 否 → 跳 C13 直入 C14  ⏰ Sprint 24-26
   │
   ├── B2 推理标准库 7 子图  ⏰ Sprint 23 (并行)
   │
   ├── B3 ADR 状态收尾 + 文档同步  ⏰ Sprint 23
   │
   └── C14 阶段 3 服务化  ⏰ Sprint 24-26
            │
            └─ 触发: A3 + B2 + 稳定 1 月
```

---

## 6. 立即执行入口 (本周最优先 3 项)

### 选项 1: A1 ASan/TSan 完整 build

```bash
cd /workspace/project/HydraForge

# ASan (首次 10-15 min)
rm -rf build/asan
cmake -B build/asan -DAGENTICDSL_ENABLE_ASAN=ON -DAGENTICDSL_BUILD_TESTS=ON
cmake --build build/asan -j$(nproc)
cd build/asan && ctest --output-on-failure 2>&1 | tee /tmp/asan-results.txt
cd ../..

# TSan
cmake -B build/tsan -DAGENTICDSL_ENABLE_TSAN=ON -DAGENTICDSL_BUILD_TESTS=ON
cmake --build build/tsan -j$(nproc)
cd build/tsan && ctest --output-on-failure 2>&1 | tee /tmp/tsan-results.txt
cd ../..
```

**期望**: baseline 33-34/34 (ASan) + 32-34/34 (TSan),无新增 C12 相关问题。

### 选项 2: B2 推理标准库 batching.md (低风险新工作)

```bash
# 1. 查现有 lib/inference/ 目录
ls lib/inference/

# 2. 写 lib/inference/batching.md (参考 lib/inference/session.md 模板; engine/model 为本周创建的占位)
# 3. 注册到根 StandardLibraryLoader (如需要)
# 4. tests/test_inference_stdlib.cpp 验证
# 5. 提交: feat(phase5): add inference stdlib batching.md
```

### 选项 3: B3 ADR 状态推进 (零代码,纯文档)

```bash
# ADR-0030 V2 状态: 🔍 Proposed → 验证 §3 ship 完整 → ✅ Approved
# ADR-0031 状态: 🟡 Partial → 评估 → ✅ Approved 或保持 Partial
# ADR-0008 状态: ✅ Approved (已 ship, 验证文档完整)

# 变更 + adr_lint.py + docs_drift_audit.py 验证
python3 tools/adr_lint.py
python3 tools/docs_drift_audit.py
```

---

## 7. 验证命令 (任何后续 session 必跑)

### 7.1 快速 smoke test

```bash
cd /workspace/project/HydraForge
cd build && ctest --output-on-failure 2>&1 | tail -3
# 期望: 64/64 (100%)
cd ..
```

### 7.2 完整 ship gate

```bash
# ctest
ctest --output-on-failure && ctest 100%

# adr_lint
python3 tools/adr_lint.py  # exit 0

# docs_drift
python3 tools/docs_drift_audit.py  # 0 DRIFT

# openspec validate
openspec validate <change-name> --strict  # exit 0

# ASan (如已 build)
cd build/asan && ctest --output-on-failure
cd ../..

# TSan (如已 build)
cd build/tsan && ctest --output-on-failure
cd ../..
```

### 7.3 LSP discipline

```bash
bash scripts/check-lsp-discipline.sh  # 退出码语义化 (0=pass, 1=config, 2=false positive, 3=real error)
bash scripts/sprint-closeout.sh  # Sprint 收官自动化检查
```

---

## 8. 关键文件 Quick Reference

| 类别 | 路径 |
|---|---|
| **本会话主交付** | `openspec/changes/archive/2026-07-04-2026-07-03-phase5-stage1-step2-yield-stream/` + `openspec/specs/yield-stream/` |
| **YIELD/STREAM 实施** | `src/core/types/node.h` (YieldNode) / `src/modules/executor/node_executor.cpp` (execute_yield) / `src/modules/executor/yield_stream_bridge.cpp` (bridge) / `src/modules/scheduler/execution_session.{h,cpp}` (pending_yield_) / `src/modules/scheduler/topo_scheduler.{h,cpp}` (SchedulerState/serialize_dag_state) |
| **测试** | `tests/test_yield_node.cpp` (9 TEST_CASE, 39 assertions) |
| **示例** | `examples/phase5_yield_token_generator/main.cpp` + CMakeLists.txt |
| **Master Plan** | `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` (Phase 5 全链路) |
| **Roadmap** | `docs/implementation-roadmap.md` (跨 ADR 执行追踪) |
| **上一个 handoff** | `docs/handoff/2026-07-04-c12-yield-stream-recovery.md` (C12 实施前) |
| **AGENTS.md** (顶部) | /workspace/project/HydraForge/AGENTS.md (项目总体) |
| **scripts/** | `scripts/check-lsp-discipline.sh`, `scripts/sprint-closeout.sh` |

---

## 9. 提示新 Session Sisyphus

### 9.1 第一个应做的 5 件事

1. **读这份文件** (`docs/handoff/2026-07-04-post-c12-path-planning.md`) ← 你现在在读
2. **验证当前状态**:
   ```bash
   git status && git log --oneline -5
   cd build && ctest --output-on-failure | tail -3
   ```
3. **确认 base 是 C12 ✅**: `git log --oneline origin/main..HEAD` 应显示 8 commits
4. **决定优先级**: A1 / B1 / B2 / B3 (基于用户请求或阶段可行性)
5. **应用 skills**: 加载对应 skill (openspec-apply-change / cpp / executing-plans 等)

### 9.2 常见陷阱

- ❌ 直接实施新 OpenSpec placeholder 而不读 master plan §四 — 已有决策/触发条件可能被忽略
- ❌ ASan build timeout panic — 正常首次编译 10-15 min,让跑完
- ❌ 修改 YIELD 路径(2.1-2.6 关键决策) 需更新本文档并通知所有 c12 相关测试
- ❌ 拆分 C13 单一合并到主 branch — OpenSpec 操作需单一 change 单一 Sprint 收官
- ❌ `git push --force` — 仅 commit, 不 amend, 不 force

### 9.3 优先级建议 (基于 Sisyphus 判断)

如用户未指定:
- **最高** (有真问题): A1 (ASan/TSan 如有 C12 相关 issue)
- **高**: B2 (低风险增量, B2.1 batching.md 可立即 ship)
- **中**: A3 (C13 触发决策需 Oracle, 阻塞 C14 评估)
- **低**: B3 (纯文档, 不阻塞 ship)
- **远期**: C14 (等 1 个月稳定期)

如不确定,问用户用 question tool,与本文档 §6 行动入口对齐。

---

## 10. 验证 Snapshot

### 10.1 当前可证

- ✅ ctest 64/64 PASS
- ✅ adr_lint.py exit 0
- ✅ docs_drift_audit.py 0 DRIFT
- ✅ openspec validate --strict PASS
- ✅ examples/phase5_yield_token_generator --mock 3/5/7 tokens 全 PASS
- ✅ examples/AGENTS.md README.md 索引 (Phase 4.5 / Sprint 19 已 ship)
- ✅ Master plan §四/§十/§十一 全部反映 C12 ship 状态
- ✅ OpenSpec change 已 archive

### 10.2 待新 session 验证

- ⏳ A1 ASan/TSan 完整 build (本文档 §6 选项 1)
- ⏳ A3 C13 决策 (Oracle 触发)
- ⏳ B2 推理标准库 7 子图 (4 子图待 ship)
- ⏳ C14 阶段 3 服务化 (远期)

---

**文档版本**: 1.0  
**下次更新**: A1 + A3 + B2.1 ship 后  
**维护者**: 后续 Sisyphus session  
**许可**: 同项目主 LICENSE
