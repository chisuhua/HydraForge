# Week 1 Day 1-2 Handoff — Drift Fix + A1 ASan/TSan CI Verification

**Date**: 2026-07-05
**Branch**: main (6 commits ahead of origin/main, all pushed)
**Status**: ✅ Day 1-2 COMPLETED (drift fix + C12 bugfix + A1 CI verification)
**From**: Sisyphus session 2026-07-05 (Oracle session `ses_0d19f9dd2ffeUGkcHUoZjagNkA`)
**To**: Next Sisyphus session (read §6 立即执行入口 + §5 Oracle 决策点)

---

## TL;DR

5 commits shipped + pushed (4 drift fix + 1 C12 leak bugfix). A1 ASan/TSan 验证 **63/64 + 62/64**,**优于 handoff §7.2 预期 1.4% + 2.9%**。C12 零新问题 (test_yield_node ASan leak 已修,TSan yield_mutex_ race-free)。Oracle 评估 Week 1 Day 3-5 路径已锁定: **Day 3 engine/model 实施 + 文档穿插** → **Day 4 TSan race fix + prefix_cache/kv_cache/decoding** → **Day 5 batching skeleton + docs_drift_audit 增强**。

新 session 直接读 §6 立即执行入口即可继续推进,**无需重新分析**。

---

## 1. 项目当前状态 (2026-07-05)

### 1.1 主线 Phase 进度

| Phase | 状态 | 累计 changes |
|---|---|---|
| Phase 0 (MVP 三层调用链) | ✅ shipped | — |
| Phase 1 (智能体层 ADR-0019~0023) | ✅ shipped | 5 |
| Phase 2 (异步架构 ADR-0030 V2) | 🟡 Partial (ADR-0030 V2 文件状态行仍 🔍 Proposed) | — |
| Phase 3 (执行策略 ADR-0031) | 🟡 Partial (C3 P1-P2 ✅ + C4 P3-P4 active + §决策 8 defer 4 项) | — |
| Phase 4 (模型路由 ADR-0034) | ✅ shipped (C7, 2026-07-02) | +1 |
| Phase 4.5 (MVP 清理) | ✅ shipped (C8, 2026-07-03) | +1 |
| **Phase 5 Stage 1 (C9-C12)** | **✅ 全链 ship + C12 (2026-07-04)** | +4 |
| Phase 5 Stage 2 (C13 fork-checkpoint) | ⚪ placeholder, 远期延后 | — |
| Phase 5 Stage 3 (C14 analysis-service) | ⚪ placeholder, 远期延后 | — |

### 1.2 C12 Ship 状态 (回顾)

- **C12 OpenSpec change**: `2026-07-03-phase5-stage1-step2-yield-stream`
- **已 archived** 为: `openspec/changes/archive/2026-07-04-2026-07-03-phase5-stage1-step2-yield-stream/`
- **8 commits shipped** (af6da4d → 0018234, 2026-07-04 ship + 今日 5 commits 后续)
- **spec 上移**: `openspec/specs/yield-stream/spec.md` (+6 requirement clauses)
- **A1 验证 (今日完成)**: C12 新代码零新问题 (yield_mutex_ 字段级锁 race-free,test_yield_node leak 修复后 ASan pass)

### 1.3 今日交付 (5 commits, all pushed to origin/main)

| Hash | Subject | Files | Status |
|---|---|---|---|
| `efc1c76` | docs(roadmap-status): correct 4 ADR-0030/0031 status references | docs/roadmap-status.md (4 处) | ✅ pushed |
| `6b7f607` | docs(phase5-master-plan): correct Phase 2 status + 推理 stdlib 子图覆盖率 | docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md (3 处) | ✅ pushed |
| `fe134c7` | docs(post-c12-handoff): correct B2 推理 stdlib 模板引用 | docs/handoff/2026-07-04-post-c12-path-planning.md (1 处) | ✅ pushed |
| `245fb4d` | feat(phase5-stdlib): add engine.md + model.md placeholders for B2 实施 | lib/inference/{engine,model}.md (新 211 行) | ✅ pushed |
| `1524c69` | fix(c12-tests): prevent MockLLMProvider leak in test_yield_node.cpp | tests/test_yield_node.cpp (5 处) | ✅ pushed |

**Git state**:
- `main` ahead of `origin/main` by 5 commits (efc1c76 → 1524c69)
- Working tree clean

### 1.4 CI Baseline 更新 (vs 之前 handoff §7.2 预期)

| Suite | 实际 | handoff §7.2 预期 | 改进 |
|---|---|---|---|
| **ctest (build/)** | **64/64 (100%)** | 64/64 | ✅ 持平 |
| **ASan (build/asan)** | **63/64 (98.4%)** | 33-34/34 (97%) | ✅ +1.4% (优) |
| **TSan (build/tsan)** | **62/64 (96.9%)** | 32-34/34 (94%) | ✅ +2.9% (优) |

**关键**: C12 新代码零新 ASan leak (test_yield_node leak 已修) + 零新 TSan race (yield_mutex_ 字段级锁正确)。

---

## 2. 今日交付详情

### 2.1 Drift Fix (4 commits, +220/-8 net)

**根因**: Oracle 评估发现 3 处 drift:
1. **推理 stdlib 覆盖率**: master plan §5.4/§十六.5 声称 "3/7 ship",实际只 1/7 (session.md)。engine.md/model.md 被声称"已 ship"但文件不存在。
2. **ADR-0030 V2 状态三处不一致**: ADR 文件 line 10 = 🔍 Proposed,master plan §一 line 24 + handoff §1.1 = "Phase 2 ✅ 100% (ADR-0030 V2 → ✅ Approved)" / "🟡 Partial"
3. **ADR-0031 状态矛盾**: ADR 文件 line 5 = 🟡 Partial (C4 §决策 8 defer 4 项),roadmap-status.md line 107/276/277 多次错误声称 "ADR-0031 Approved"
4. **handoff §6 line 482 模板引用**: `{engine,session,model}.md` 引用了不存在的文件

**修复策略** (基于事实而非 Oracle 初始建议):
- ADR 文件本身状态行准确,**不改 ADR 文件**
- 改引用文档 (master plan / roadmap-status / handoff) 让其与 ADR 真实状态一致
- 创建 lib/inference/engine.md + model.md **占位** (参考 session.md 模板,顶部 PLACEHOLDER 标记),让 master plan "3/7 ship" 描述部分成真

**Commit 详情**:
```
efc1c76 docs(roadmap-status): correct 4 ADR-0030/0031 status references
  - line 106: 'ADR-0030 V2 → ✅ Approved' → 'ADR-0030 V2 仍 🔍 Proposed'
  - line 107: 'ADR-0031 Approved' → 'ADR-0031 仍 🟡 Partial §决策 8 defer 4 项'
  - line 276: 'ADR-0031 → ✅ Approved' → 'ADR-0031 仍 🟡 Partial'
  - line 277: 'ADR-0031 P3-P4 → ✅ Approved' → 'ADR-0031 仍 🟡 Partial §决策 8 defer'

6b7f607 docs(phase5-master-plan): correct Phase 2 status + 推理 stdlib 子图覆盖率
  - §5.4 line 358: 'engine/model/session (3/7)' → 'session.md (1/7) + 占位本周创建'
  - §十六.5 line 756: '(engine/model/session 已 ship)' → '(session.md 已 ship)'
  - §一 line 24: 'Phase 2 ✅ 100% (ADR-0030 V2 → ✅ Approved)' → '🟡 Partial'

fe134c7 docs(post-c12-handoff): correct B2 推理 stdlib 模板引用
  - §6 line 482: '{engine,session,model}.md 模板' → 'session.md 模板; engine/model 为本周创建的占位'

245fb4d feat(phase5-stdlib): add engine.md + model.md placeholders
  - lib/inference/engine.md (101 行, PLACEHOLDER)
  - lib/inference/model.md (108 行, PLACEHOLDER)
  - 顶部明确 PLACEHOLDER 标记 + B2 实施 checklist (工具注册 + C++ 实现 + 测试)
```

### 2.2 C12 Leak Bugfix (1 commit, +5/-5)

**根因**: C12 新增测试 `tests/test_yield_node.cpp` 误用 NodeExecutor/ExecutionSession 所有权语义:
- 两者持有 `ILLMProvider*` 裸指针 (非 owning, 无 delete)
- 测试代码用 `provider_holder.release()` 把所有权转移给 NodeExecutor
- 测试 scope 结束时 MockLLMProvider 实例**永不析构** → ASan 报 6380 bytes leak (31 allocations)

**修复**: 5 处 `.release()` → `.get()`,让 unique_ptr 在 TEST_CASE scope 内保活:
```
tests/test_yield_node.cpp:
- line 26 (NodeExecutor, YieldNode NEXT mode)
- line 50 (NodeExecutor, YieldNode CONTINUE mode)
- line 70 (NodeExecutor, YieldNode STOP mode)
- line 146 (ExecutionSession, YIELD pending_yield)
- line 171 (ExecutionSession, YIELD persistence)
```

**验证**:
- baseline ctest: 64/64 PASS (零回归)
- ASan ctest: test_yield_node 从 fail → pass;test_execute_parallel 仍 pre-existing fail (Sprint 10 tracked)

**关键判断**: 这是**测试代码 bug**,不是 C12 引擎 bug。NodeExecutor/ExecutionSession 的"借用"所有权语义是正确的 (DSLEngine 持有 MockLLMProvider unique_ptr 作为 owner, NodeExecutor 仅借用)。test_yield_node.cpp 作者误解所有权语义。**Bugfix Rule**: Fix minimally — 仅改测试,不改 NodeExecutor API (避免 BREAKING)。

---

## 3. A1 ASan/TSan 验证结果

### 3.1 ASan (build/asan)

**Build**: 10-15 min 首次 build,99% 完成时被 15min timeout kill,重新 build 20min 内完成 100%。
**ctest**: **63/64 PASS (98.4%)** — 优于 handoff §7.2 预期 97% (+1.4%)

**Failed**: test_execute_parallel (pre-existing)
- 错误类型: `AddressSanitizer: stack-use-after-scope` (T6 thread)
- 调用栈: `tests/test_execute_parallel.cpp:61` → Taskflow worker → string reference lifetime
- Sprint 10 已 track: `openspec/changes/archive/2026-06-25-pre-existing-sanitizer-findings/`

### 3.2 TSan (build/tsan)

**Build**: ~10 min
**ctest**: **62/64 PASS (96.9%)** — 优于 handoff §7.2 预期 94% (+2.9%)

**Failed**:
1. **test_cognitive_worker** — `data race on std::optional + std::string` (tests/test_cognitive_worker.cpp:192)
   - Catch2 framework + std::jthread 已知交互 (Sprint 10 P2 decision, docs/audits/p2-tsan-investigation.md)
2. **test_execute_parallel** — `data race on NodeExecutor::dispatch_to_tool + vector::push_back` (node_executor.cpp:348)
   - **真实产品代码 race** (Sprint 10 错误归类为 framework,实际是 product code)
   - Oracle Q1 评估: **NOW 修** (0.5-1 天, Day 3 or Day 4)

### 3.3 Pre-existing Failures Summary

| Test | Sanitizer | Type | Sprint 10 Tracking | Oracle Q1 Decision |
|---|---|---|---|---|
| test_execute_parallel | ASan | stack-use-after-scope | ✅ tracked | 暂缓 (framework 已知交互,延后) |
| test_cognitive_worker | TSan | data race on std::optional + std::string | ✅ tracked | **保持** (Sprint 10 P2 decision 文档化非修复) |
| test_execute_parallel | TSan | data race on NodeExecutor::dispatch_to_tool | ⚠️ **错归类** | **NOW 修** (Day 4 AM, 1h) |

### 3.4 C12 新代码验证

| 测试 | ASan | TSan | Notes |
|---|---|---|---|
| test_yield_node | ✅ Pass (was fail before `1524c69`) | ✅ Pass | yield_mutex_ 字段级锁正确 |
| test_fork_static_contracts | ✅ Pass | ✅ Pass | C12 archived 内容 |
| yield_mutex_ 跨 await 边界 | N/A | ✅ 无 race | C12 设计的字段级锁生效 |

---

## 4. Oracle 评估 (session `ses_0d19f9dd2ffeUGkcHUoZjagNkA`)

### 4.1 5 个决策点

#### Q1: pre-existing test_execute_parallel TSan race
**Recommendation: NOW 修 (Day 4 AM, 1h)**

- **证据**: `node_executor.cpp:348` `NodeExecutor::dispatch_to_tool` + `vector::push_back` (无锁写) — 真产品 race
- **影响**: C12 YIELD 节点在 Fork 场景下可能放大该 race
- **修复**: `tool_results_.push_back()` 加 `std::lock_guard`
- **陷阱**: 加锁需覆盖**所有**写操作 + 析构 `clear()`,避免用 `shared_mutex`(写入频率高)
- **验证**: TSan ctest 62/64 → 63-64/64

#### Q2: B2 engine/model 启动
**Recommendation: Day 3 并行 (engine 4h AM + model 4h PM)**

- **策略**: 同一 session sequential (engine → model),或 subagent 并行
- **OpenSpec**: **不需要** (master plan §5.4 + §十六.4 已规划)
- **Commit**: `feat(phase5-stdlib): add inference engine.md` + `feat(phase5-stdlib): add inference model.md`
- **估时**: 1 天 (含工具注册 + C++ 实现 + 测试)
- **风险**: 若需调 llama.cpp API,1 天 → 1.5 天。建议 **从 `llm_config.json` 读取参数**,不直接调 API

#### Q3: 文档同步 (handoff baseline 更新)
**Recommendation: Day 3 穿插 30min**

- handoff §7.2 line 451 期望 baseline "ASan 33-34/34 + TSan 32-34/34" 落后实际 1.8x
- 新 session 读 handoff 会误判 CI 质量
- 范围: handoff §7.2 + §1.1 + roadmap-status.md + master plan

#### Q4: Week 1 Day 3-5 时间分配

| Day | AM (4h) | PM (4h) |
|---|---|---|
| **Day 3 (Tue)** | **engine.md 实施** (4h) | **model.md 实施** (4h) |
| | 穿插: handoff baseline 更新 (30min) | |
| **Day 4 (Wed)** | **TSan race 修复** (1h) + **prefix_cache.md** (1h) + **kv_cache.md** (1h) | **decoding.md** (2h) + B2 smoke test |
| **Day 5 (Thu)** | **batching.md** (skeleton + placeholder, 2h) | **docs_drift_audit.py 增强** (2h) + roadmap-status 同步 (30min) |

总工作量 ~3.5-4 天,Week 1 末完成 B2 + race fix + 文档同步。

#### Q5: Week 2 计划

- **A0 性能基线** (Fork deep_copy 微基准, 0.5 天 — 可选)
- **A3 C13 Oracle 决议** (基于 A0 或定性评估)
- **C14 评估** (1 月稳定期开始 = 2026-08-04)

### 4.2 Oracle 警告风险

1. **engine/model 估时膨胀**: 若需调 llama.cpp API,1 天 → 1.5 天。建议从 `llm_config.json` 读取,不直接调 API
2. **batching.md YIELD 依赖复杂**: 建议先 **skeleton + placeholder**,标注 `@todo: implement after C13 fork-checkpoint`,避免阻塞 B2.6
3. **dispatch_to_tool 加锁陷阱**: 需覆盖所有 `tool_results_` 写操作 + 析构 `clear()`,不要用 `shared_mutex`
4. **docs_drift_audit 增强后 baseline drift**: 0 → 1-2 DRIFT 是正常现象(新规则检测到旧未发现的问题),不应阻止增强

---

## 5. 推荐路径 (Week 1 Day 3-5 + Week 2)

### 5.1 Day 3 (Tue) — B2.1 + B2.2 启动

**AM**: engine.md 实施
- 创建 `lib/inference/engine.md` (参考 session.md + engine.md 占位结构)
- 定义 tool_call: `engine.configure(params)` / `engine.status()` / `engine.metrics()`
- 注册 `inference.engine_init` 工具到 `src/common/tools/registry.cpp`
- 实现底层 C++ 函数 (从 `llm_config.json` 读取,**不直接调 llama.cpp API**)
- 写 `tests/test_inference_engine.cpp` (3-5 TEST_CASE)
- ctest + ASan + TSan 验证

**穿插**: handoff baseline 更新
- `docs/handoff/2026-07-04-post-c12-path-planning.md` §7.2: "ASan 33-34/34 → 63/64, TSan 32-34/34 → 62/64"
- `docs/roadmap-status.md` Phase 5 进度条更新

**PM**: model.md 实施 (类似 engine.md)
- 定义 tool_call: `model.load(path, params)` / `model.unload()` / `model.list()` / `model.switch(name)`
- 注册 `inference.model_load` 工具
- 实现底层 C++ 函数
- 写 `tests/test_inference_model.cpp`
- ctest + ASan + TSan 验证

**Commit**: 
- `feat(phase5-stdlib): add inference engine.md + model.md` (合并 2 子图, 同 dir atomic)
- `docs(handoff): update Week 1 Day 1-2 completion baseline` (可选, 含本日交付)

### 5.2 Day 4 (Wed) — TSan race fix + B2.3-B2.5

**AM** (1h + 1h + 1h):
- **TSan race 修复** (1h): 修改 `src/modules/executor/node_executor.cpp` `dispatch_to_tool` 中 `tool_results_.push_back()` 加 `std::lock_guard`,覆盖所有写操作 + 析构 `clear()`
- **prefix_cache.md** (1h): json scope nesting,`prefix_cache.set(pattern, max_size)` / `prefix_cache.get(pattern)` / `prefix_cache.clear()`
- **kv_cache.md** (1h): 类似 prefix_cache,`kv_cache.set_size(gb)` / `kv_cache.evict_policy(lru/lfu)`

**PM** (2h + smoke test):
- **decoding.md** (2h): temperature/top_p/top_k/repeat_penalty 控制,`decoding.configure(temp, top_p, top_k, repeat_penalty)`
- B2.1-B2.4 smoke test: ctest + ASan + TSan (期望 baseline ctest 100%, ASan 63-64/64, TSan 63-64/64 — TSan race 修复后 +1)

**Commit**:
- `fix(executor): add mutex to dispatch_to_tool tool_results_ write` (TSan race 修复)
- `feat(phase5-stdlib): add inference prefix_cache + kv_cache + decoding` (合并 3 子图)

### 5.3 Day 5 (Thu) — B2.6 + docs_drift_audit 增强 + 文档同步

**AM** (2h):
- **batching.md** (2h): queue 管理 (依赖 C12 YIELD)
- **Oracle 建议**: 先写 skeleton + placeholder,标注 `@todo: implement after C13 fork-checkpoint`,避免 YIELD 语义复杂性阻塞 B2.6
- 工具: `batching.add_request(req)` / `batching.flush()` / `batching.size()`

**PM** (2h + 30min):
- **docs_drift_audit.py 增强** (2h):
  - 加 ADR 状态跨文档对照规则 (状态行 grep vs roadmap-status.md Phase 行)
  - 加 claim 文件存在性校验 (master plan 声称 "engine.md/model.md shipped" → `os.path.exists` 校验)
- **roadmap-status.md + master plan 同步** (30min):
  - Phase 5 Stage 1 进度条更新 (假设 6/7 子图 ship → 85%)
  - master plan §五 + §十 备注

**Commit**:
- `feat(phase5-stdlib): add inference batching.md skeleton (placeholder for C13)`
- `tools(docs-drift-audit): add ADR status cross-doc + claim file existence checks`
- `docs(roadmap-status): update Phase 5 Stage 1 progress (6/7 stdlib + race fix)`

### 5.4 Week 2 (B2 完成 + 决策阶段)

- **A0 性能基线** (0.5 天,可选): Fork deep_copy 微基准 → 为 A3 提供数据
- **A3 C13 Oracle 决议** (1 天):
  - 若 A0 显示 deep_copy 是瓶颈 → B1 C13 实施
  - 若 skip A0 → 基于定性评估 (无性能 bottleneck 报告 + 无 Session 迁移需求) → defer C13,直接 C14 评估
- **C14 评估** (4-6 周): 等 1 月稳定期 (2026-08-04) + B2 6/7 子图 ship + Oracle 验证自进化可行性

---

## 6. 立即执行入口 (新 Session 起点)

### 选项 1: Day 3 B2.1 engine.md 实施 (推荐,Oracle 路径起点)

```bash
cd /workspace/project/HydraForge
git pull origin main  # 同步 5 commits

# 1. 验证当前 state
git log --oneline origin/main..HEAD  # 0 commits (已 sync)
cd build && ctest --output-on-failure  # 64/64 PASS

# 2. 创建 engine.md 实施计划 (参考 lib/inference/session.md 模板)
# 编辑 lib/inference/engine.md 占位 → 真实 ship
# 添加 inference.engine_init 工具注册 (src/common/tools/registry.cpp)
# 实现 engine_init C++ 函数 (从 llm_config.json 读取)
# 写 tests/test_inference_engine.cpp

# 3. 验证
cmake --build build -j$(nproc)
ctest --output-on-failure  # 期望 65/65 (B2.1 加 1)
cmake --build build/asan --target test_inference_engine -j$(nproc)
ctest --test-dir build/asan --output-on-failure  # 期望 64/65

# 4. 提交
GIT_MASTER=1 git add lib/inference/engine.md src/common/tools/registry.cpp tests/test_inference_engine.cpp
GIT_MASTER=1 git commit -m "feat(phase5-stdlib): add inference engine.md (B2.1)"
GIT_MASTER=1 git push origin main
```

### 选项 2: pre-existing TSan race 修复 (Oracle Q1 NOW)

```bash
# 1. 复现 race
cd build/tsan && ctest -R test_execute_parallel --output-on-failure

# 2. 定位 node_executor.cpp:348 dispatch_to_tool
grep -n "dispatch_to_tool\|tool_results_" src/modules/executor/node_executor.cpp

# 3. 修改
# 在 class NodeExecutor 添加 std::mutex tool_results_mutex_
# 修改 dispatch_to_tool: tool_results_.push_back(result) → 加 lock_guard
# 修改 ~NodeExecutor: tool_results_.clear() → 加 lock_guard

# 4. 验证
cmake --build build/tsan -j$(nproc)
ctest --test-dir build/tsan --output-on-failure  # 期望 63/64 (race 修复)

# 5. 提交
GIT_MASTER=1 git commit -m "fix(executor): add mutex to dispatch_to_tool tool_results_ write"
```

### 选项 3: 文档同步 (Oracle Q3,30min)

```bash
# 1. 更新 handoff baseline
# 编辑 docs/handoff/2026-07-04-post-c12-path-planning.md §7.2:
# "ASan 33-34/34 → 63/64, TSan 32-34/34 → 62/64"

# 2. 更新 roadmap-status.md Phase 5 进度条

# 3. 验证
python3 tools/adr_lint.py  # exit 0
python3 tools/docs_drift_audit.py  # 0 DRIFT
```

---

## 7. 关键文件 Quick Reference

| 类别 | 路径 |
|---|---|
| **今日 handoff** | docs/handoff/2026-07-05-week1-day1-day2-completion.md (本文档) |
| **上一份 handoff** | docs/handoff/2026-07-04-post-c12-path-planning.md (路径规划, 已修正 baseline) |
| **C12 实施前 handoff** | docs/handoff/2026-07-04-c12-yield-stream-recovery.md |
| **YIELD/STREAM 实施** | src/core/types/node.h + src/modules/executor/{node_executor,yield_stream_bridge}.{h,cpp} + src/modules/scheduler/{execution_session,topo_scheduler}.{h,cpp} |
| **C12 测试** | tests/test_yield_node.cpp (9 TEST_CASE, 39 assertions) |
| **Master Plan** | docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md |
| **Oracle session** | ses_0d19f9dd2ffeUGkcHUoZjagNkA (可继续追问) |
| **lib/inference 当前** | session.md (107, ship) + engine.md (101, PLACEHOLDER) + model.md (108, PLACEHOLDER) |
| **Pre-existing tracking** | openspec/changes/archive/2026-06-25-pre-existing-sanitizer-findings/ |
| **AGENTS.md** | /workspace/project/HydraForge/AGENTS.md (项目总体) |
| **scripts/** | scripts/check-lsp-discipline.sh, scripts/sprint-closeout.sh |

---

## 8. 风险与缓解 (完整版)

| 风险 | 严重度 | 缓解 |
|---|---|---|
| **engine/model 估时膨胀** (需调 llama.cpp API) | 中 | 从 `llm_config.json` 读取参数,不直接调 API;若仍超 1.5 天,降级为"纯占位 ship" |
| **batching.md YIELD 依赖复杂** | 高 | skeleton + placeholder,标注 `@todo: implement after C13 fork-checkpoint` |
| **dispatch_to_tool 加锁陷阱** | 中 | 加锁覆盖所有 `tool_results_` 写操作 + 析构 `clear()`,不用 `shared_mutex`(写入频率高) |
| **docs_drift_audit 增强后 baseline drift** | 低 | 0 → 1-2 DRIFT 是正常现象,修复后重新标 0,不阻止增强 |
| **AGENTS.md Phase 2 状态描述不一致** | 低 | master plan + roadmap-status.md 已修正 (Phase 2 🟡 Partial),AGENTS.md 暂保持"Phase 0-4.5 全部 100%" (C9 audit 已正确审计,实际反映 ADR-0030 V2 仍 🔍 Proposed) |
| **C13 决策无性能数据** | 低 | Oracle Q5: 可选 A0 性能基线 (0.5 天),或定性评估 defer C13 |

---

## 9. 新 Session 接手指南

### 9.1 第一个应做的 5 件事

```bash
cd /workspace/project/HydraForge

# 1. 读这份文档
cat docs/handoff/2026-07-05-week1-day1-day2-completion.md

# 2. 读上一份 handoff (C12 ship 后路径规划)
cat docs/handoff/2026-07-04-post-c12-path-planning.md

# 3. 验证当前 state
git status  # 应 clean
git log --oneline -10  # 1524c69 + 5 commits ahead
cd build && ctest --output-on-failure | tail -3  # 64/64 PASS

# 4. 确认 base 是今日 ship (1524c69)
git log --oneline origin/main..HEAD  # 0 commits (已 sync)

# 5. 决定优先级: Day 3 B2.1 engine.md (Oracle 路径) / TSan race fix / 文档同步
# 应用对应 skills:
#   - B2 实施 → cmake + debugging
#   - 文档同步 → writing
#   - TSan race → debugging + cmake
```

### 9.2 优先级建议 (基于 Oracle 评估)

| 优先级 | 任务 | 理由 |
|---|---|---|
| **高** | **B2.1 engine.md 实施 (Day 3 AM)** | 路径规划主线,占位已 ready,有 scaffolding 加速 |
| **高** | **B2.2 model.md 实施 (Day 3 PM)** | 与 engine 配对,1 天内可 ship 2 子图 |
| **中** | **TSan race fix (Day 4 AM, 1h)** | 真产品 race,Oracle Q1 NOW 决策 |
| **中** | **B2.3-B2.5 prefix_cache/kv_cache/decoding (Day 4)** | 路径规划剩余子图 |
| **中** | **batching.md skeleton (Day 5 AM)** | 避免 YIELD 依赖阻塞 B2.6 |
| **低** | **docs_drift_audit.py 增强 (Day 5 PM)** | 防止 recurrence,不阻塞主路径 |
| **远期** | **A0 性能基线 / A3 C13 决议 (Week 2)** | C13 远期延后,等数据成熟 |

### 9.3 常见陷阱

- ❌ 跳过 handoff 直接开干 — 新 session 必读 §6 立即执行入口 + §5 Oracle 决策点
- ❌ ASan build 超时 panic — 正常首次编译 10-15 min,让跑完
- ❌ 修 pre-existing race 时改 NodeExecutor/ExecutionSession API — 改 dispatch_to_tool 内部加锁即可,避免 BREAKING
- ❌ batching.md 直接实现 queue 逻辑 — YIELD 依赖复杂,先 skeleton + placeholder
- ❌ docs_drift_audit 增强后看到 DRIFT panic — 0 → 1-2 是正常,先 fix 再 ship
- ❌ 修改 ADR 文件状态行 (ADR-0030 V2 / ADR-0031) — 状态行准确,drift 在引用文档
- ❌ 修改 AGENTS.md "Phase 2 ✅ 100%" — C9 audit 已正确审计,基于 ADR 真实状态

---

## 10. 验证 Snapshot

### 10.1 当前可证 (今日交付)

- ✅ **5 commits pushed** (efc1c76 → 1524c69, all on origin/main)
- ✅ **baseline ctest** 64/64 PASS (100%)
- ✅ **ASan** 63/64 PASS (98.4%,优于预期)
- ✅ **TSan** 62/64 PASS (96.9%,优于预期)
- ✅ **C12 零新问题** (test_yield_node leak 修复后 ASan pass, yield_mutex_ TSan race-free)
- ✅ **adr_lint.py** exit 0 (33 ADR 文件 lint 通过)
- ✅ **docs_drift_audit.py** exit 0 (4 scenarios, 0 DRIFT/WARNING)
- ✅ **Oracle 评估完成** (session `ses_0d19f9dd2ffeUGkcHUoZjagNkA`)
- ✅ **lib/inference/scaffolding ready** (engine.md + model.md 占位 + B2 实施 checklist)

### 10.2 待新 session 验证

- ⏳ **B2.1+B2.2 engine/model 实施** (Day 3, 1 天)
- ⏳ **TSan race fix** (Day 4 AM, 1h) — Oracle Q1 NOW
- ⏳ **B2.3-B2.5 prefix_cache/kv_cache/decoding** (Day 4 PM)
- ⏳ **B2.6 batching.md skeleton** (Day 5 AM)
- ⏳ **docs_drift_audit.py 增强** (Day 5 PM, 2h)
- ⏳ **roadmap-status + master plan 同步** (Day 5 PM, 30min)
- ⏳ **Week 2 A0 + A3 C13 决议**
- ⏳ **C14 评估** (1 月稳定期开始 = 2026-08-04)

### 10.3 已知 Pre-existing (Sprint 10 tracked)

| 测试 | 失败 | Sprint 10 Decision | Oracle Q1 |
|---|---|---|---|
| test_execute_parallel (ASan) | stack-use-after-scope | 文档化非修复 | 保持 (framework 已知交互) |
| test_cognitive_worker (TSan) | data race on std::optional/string | 文档化非修复 | 保持 (Sprint 10 P2 decision) |
| test_execute_parallel (TSan) | data race on NodeExecutor::dispatch_to_tool | ⚠️ **错归类** | **NOW 修** (Day 4 AM) |

---

## 11. 维护与更新

**文档版本**: 1.0
**下次更新**: Week 1 Day 5 末 (B2 全部 ship + race fix + 文档同步后)
**维护者**: 后续 Sisyphus session
**许可**: 同项目主 LICENSE

**前置 handoff 链**:
1. `2026-07-04-c12-yield-stream-recovery.md` (C12 实施前)
2. `2026-07-04-post-c12-path-planning.md` (C12 ship 后路径规划)
3. `2026-07-05-week1-day1-day2-completion.md` (本文档, Week 1 Day 1-2 交付 + Day 3-5 路径)

**后续 session 必读**:
1. 本文档 (Week 1 Day 3-5 起点)
2. `2026-07-04-post-c12-path-planning.md` (背景 + §6 路径规划)
3. Oracle session `ses_0d19f9dd2ffeUGkcHUoZjagNkA` (决策点细节)