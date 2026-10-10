# fork-join-dsl-migration

**优先级**: P2 | **来源**: ADR-0089 v1.4 D9.future-change-tracker
**阶段**: phase-6b | **分类**: 治理债 + 性能迁移
**类型**: refactor (DSL 路径承接 ForkJoinLoop 4-worker 并行能力)
**主题**: ForkJoinLoop → DSL `loop/run_fork_join` 工具迁移

> **状态**: 🔍 **Future Change Tracker**（仅记录提案，**不进入实施周期**；立项触发条件: ADR-0089 v1.4 soft-deprecation 观察期满 ≥ 1-2 月 + 用户明确启动）
>
> **关联文档**: `docs/adr/adr-0089-v1-4-amendment-soft-deprecate-cpp-agent-loops.md` D9 段

---

## 架构依据

- **ADR-0089 v1.4 D9.future-change-tracker**: 5-segment draft 形式登记本 future change
- **ADR-0089 v1.4 D8.inv.forkjoin**: ForkJoinLoop 内部 `DomainWorkerPool` 4-worker 语义不变，ADR-0087 benchmark 不回归（v1.4 ship 时维持现状）
- **ADR-0087 Cloud Adapter Threading Model**: 4-worker benchmark 3.3× speedup (2026-09-16 commit `13ac53f`, 2834ms → 858ms) 是 ADR-0087 资产，**承载对象不变**
- **ADR-0020 §3.2 DomainWorkerPool**: 4 jthread pool 独立模块，被多个消费者用（不仅是 ForkJoinLoop，还有 GEPA Loop / Pipeline Stage 等）
- **ADR-0021 §3.5 PDK 头依赖限制**: "PDK 头文件仅依赖 `agenticdsl/contract/*.h`" — ForkJoinLoop `fork_join_loop.h` 当前已包含 `cognitive/domain_worker_pool.h`，是已知历史债（§3.5 known caveat）。本 migration 可顺带清偿此债
- **AGENTS.md Pattern #12** (`consolidate-loop-phases-to-shared-helpers`): React/PlanExecute phase helper 单源化已 ship (v1.3, 2026-10-09)。ForkJoin 是 Pattern #12 不动 class 阶段的 scope guard 解除后留下的"4 组件胶水"债
- **Metis session `ses_edbe24cc1ffeqFnkc2v9e56nqt`**: ForkJoinLoop 真实复杂度 = 4 紧耦合组件胶水 (DomainWorkerPool + InMemoryBus + mutex/cv + handler), 不是 lambda 可移
- **Oracle session `ses_edbe211c0ffemuq7g2KEC5qglC`**: ForkJoinDSL 完全迁移需 1 个完整 Sprint + DomainWorkerPool 跨 ABI 暴露面评估 + ADR-0087 benchmark DSL 路径复跑 ≥ 3×

---

## Why

### 用户原始动机链

1. 用户提问 "C++ 版本的 Agent Loop 可以停用，只暴露 DSL 路径吗？" (2026-10-10)
2. Metis + Oracle dual-agent review 评估 4 选项（A 硬删 / B 软停 / C 现状 / B' OpenSpec）
3. Z-1 决策: **本会话仅软停用 ReactLoop + PlanExecuteLoop**；ForkJoinLoop **不动**
4. ADR-0089 v1.4 翻牌: ForkJoinLoop 维持现状 (D8.inv.forkjoin) + 立本 future change tracker

### 真实复杂度

读 `include/agenticdsl/pdk/agent_loops/fork_join_loop.h:138-340` 后量化：

| 组件 | ForkJoinLoop 怎样用 | 当前 DSL `loop/process_task` (pdk_entry.cpp:547-603) 状态 |
|---|---|---|
| **DomainWorkerPool(4)** | `pool_ = make_unique<DomainWorkerPool>(4, bus_)` 创建 4 jthread | ❌ 无 |
| **Pool lifecycle** | `start()` → `submit_task(N)` × N → `cv.wait(all done)` → `stop()` | ❌ 无 |
| **Domain handler 注册** | `register_domain_handler("branch", handler)` | ❌ 无 |
| **InMemoryBus 订阅** | `subscribe("domain.task.completed"/"failed", callback)` | ❌ 无 |
| **mutex + cv 同步** | `tracker->cv.wait(all done || any_failed || cancelled)` | ❌ 无 |
| **fail-fast** | `tracker->any_failed` atomic + first-failure 记录 | ❌ 无 |
| **cancel token** | `token.stop_requested() → pool_->stop()` | ❌ 无 |
| **顺序 merge** | `for branch in branches: final_context.working["data"][branch] = ...` | ❌ 无 |

**8 项行为差异** 需 DSL 路径重建镜像 → **1 个完整 Sprint 工作量**。

### D4 vs D9 关系

| 版本 | 状态 | 含义 |
|---|---|---|
| **v1.3 D4.inv.forkjoin** (2026-10-09 ship) | ✅ 已 ship | 当 change 范围限定 = 不动 ForkJoinLoop |
| **v1.4 D8.inv.forkjoin** (2026-10-10 ship, 继承) | ✅ 已 ship | ForkJoinLoop 维持现状 + 加 ADR 戳记 |
| **v1.5 (future) D-DSL.forkjoin** | 🔍 Proposed (本 tracker) | ForkJoinLoop 内部逻辑迁出到 DSL 工具; DomainWorkerPool 4-worker 原语保留 |

**D4 → D8 → (future) D-DSL**: 3 阶段决策链，每阶段独立 OpenSpec 治理。**未来不会一次性反转**，每个决策点都有独立 Oracle 审查。

---

## What Changes

### 范围（仅在用户启动 future change 后实施）

1. **新增 `loop/run_fork_join(branches[], context)` 工具注册** in `pdk/loop_agent/src/pdk_entry.cpp`：
   - 入参: `branches: string[]` + `context: JSON object` (optional)
   - 出参: `{ok, success, error_code, results: {branch_id: result}, total_steps}`
   - 实现: 内部封装 `DomainWorkerPool(4)` + `InMemoryBus` 事件订阅 + `mutex/cv` 同步 + `fail-fast` + 顺序 merge
   - **dispatch 表 (DAG 节点类型)**: 暂不引入（避免侵入 TopoScheduler），仅作为 DSL 工具函数暴露

2. **修改 `lib/loop/fork_join.agent.md`** 使用新工具替代 fork/join node 编排：
   - 现 `fork_tasks` + `task_a`/`b`/`c` + `join_results` + `synthesize` 节点 → 替换为单次 `loop/run_fork_join` 工具调用
   - `synthesize` 节点保留（接收 `loop/run_fork_join` 输出后再 LLM 合成）
   - `type: fork` + `type: join` 节点可保留作为 alternative path（向后兼容）

3. **评估 `DomainWorkerPool` 跨 ABI 暴露合规性**：
   - pdk_entry.cpp 当前实现已暴露 `DomainWorkerPool` 使用（因 lambda 内调用），但**头文件**链需审查
   - 违反 ADR-0021 §3.5 时需 amendment: 增加 `agenticdsl/cognitive/iworker_pool.h` contract 层接口，本 migration 顺带清偿
   - **审计要求**: `grep -r "cognitive/domain_worker_pool.h" pdk/` 验证影响范围
   - **可接受 fallback**: 若 pdk_entry.cpp 不能直接 include `domain_worker_pool.h` (违反 §3.5), 抽 contract 层接口

4. **复跑 ADR-0087 benchmark** 在 DSL 路径上：
   - 测试用例: 3 branch 并行 LLM call (与 ADR-0087 同条件)
   - 时序: serial baseline vs `loop/run_fork_join` parallel 4-worker
   - **验收**: 至少 3× speedup (per ADR-0087 baseline 2834ms → 858ms)
   - **稳定性**: 至少 3 次连续运行 speedup 一致 (排除 CI 抖动)

5. **`fork_join_loop.h` 退役** (post-3-月观察):
   - ForkJoinLoop class 不删除（保 4-worker benchmark 复跑基线），仅加 `[[deprecated]]` 标签
   - `tests/test_pdk_fork_join.cpp` + `examples/pdk_chat_demo/tests/test_fork_join_loop_integration.cpp` 镜接到 DSL 路径
   - 删除条件: ADR-0087 benchmark 在 DSL 路径稳定 1 Sprint + 用户确认

### 不在范围

- ❌ 删 `ForkJoinLoop` class 本 v1.4 + 未来 future change 都不删 (D8.inv.unchanged-class)
- ❌ 删 `DomainWorkerPool` (4 jthread pool 是其他模块依赖, ADR-0020 §3.2)
- ❌ 改 `g1_coding_assistant` (M3 仅 ReactLoop)
- ❌ 改 `lib/loop/react.agent.md` + `lib/loop/plan_execute.agent.md` (已 Phase 2 ship)
- ❌ 改 `loop_phases.h` (Phase 1 ship v1.3, D1 不变量保留)
- ❌ 改 TopoScheduler fork/join DAG 节点（侵入核心引擎，风险过高，建议未来单独 ADR）

---

## Acceptance

### 启动条件

1. ADR-0089 v1.4 ship 后 **观察期 ≥ 1-2 月** (用户决策可调, 默认 2 个月)
2. 用户明确 "启动 fork-join-dsl-migration change"
3. 立 OpenSpec change `openspec/changes/2026-XX-XX-fork-join-dsl-migration/` (4-file artifacts: proposal / design / tasks / specs)
4. 走完整 OpenSpec 流程 (24h cooling-off + Metis+Oracle dual-agent pre-impl review)

### 实施 ship gate

1. `cmake --build build` 零 error
2. `ctest --test-dir build -LE must_realllm` 100% PASS (零回归)
3. `ctest --test-dir build -L must_realllm` 真实 LLM 测试通过 (如账号有余)
4. **ADR-0087 benchmark 复跑**: 3 branch 并行 ≥ 3× speedup（基于 v1.4 commit baseline, CI 环境稳定 3 次）
5. `nm build/pdk/g1_coding_assistant/libG1CodingAssistant.so | grep -E "DEFINE_AGENT|ReactLoop"` 仍命中 (M3 + v1.4 deprecation warning 仍可编译)
6. ADR-0021 §3.5 compliance check: `grep -r "cognitive/domain_worker_pool.h" pdk/` 验证（若仍 include, 抽 contract 接口）
7. 必须_realllm: `test_react_loop_real_llm` 7/7 + `test_plan_execute_realllm` 3/3 PASS (v1.4 ship 后基线)
8. 反向指标 (per AGENTS.md Reverse Indicator Rule): drop_ratio ≤ 5% (预期 ≤ 2% 因 fork 工具化不可避免 minor behavior change)

### 验收后续

1. ADR-0089 v1.5 amendment ship (若需要)
2. `.rddf/improvements/fork_join_dsl_migration.md` status → "✅ Tracked (Future Change Closed)"
3. `.rddf/improvements/improvement-suggestions.md` 删除本条目 or 标记 completed
4. AGENTS.md Pattern #12 段补 v1.5 沉淀 (若 change 完成)

---

## Capabilities

### 新增能力

1. DSL `lib/loop/fork_join.agent.md` 路径获得 **DomainWorkerPool 4-worker 并行** 保障（与 ForkJoinLoop class 形态无关）
2. `loop/run_fork_join` 工具可被外部 DSL 文件任意组合调用（不限于 fork_join.agent.md）
3. **`fork_join_loop.h` 退役前置条件** 满足: 4-worker 行为已在 DSL 工具层复刻
4. **ADR-0021 §3.5 compliance 清偿**: pdk_entry.cpp 不再 include `cognitive/domain_worker_pool.h`（若抽 contract 接口）

### 不新增能力（明确限制）

- ❌ TopoScheduler DAG 节点 (fork/join) 增强
- ❌ 通用 worker pool DSL 暴露（仅 `loop/run_fork_join` 1 个封装）
- ❌ fork/join DAG 节点删除（保留作为 fallback path）

---

## Impact

### 影响范围（仅在用户启动 future change 后评估具体数字）

| 类型 | 文件 | 操作 | 假设计数 |
|---|---|---|---|
| 新增 | `pdk/loop_agent/src/pdk_entry.cpp` | `loop/run_fork_join` 工具注册 (~80 行 lambda) | 1 文件 |
| 修改 | `lib/loop/fork_join.agent.md` | 替换 fork/join 节点 → `loop/run_fork_join` 调用 | 1 文件 |
| 修改 | `tests/test_pdk_fork_join.cpp` | (v1.4 ship 时不变) + mirror 到 DSL 路径 (post-3-month) | 1 文件 |
| 修改 | `examples/pdk_chat_demo/tests/test_fork_join_loop_integration.cpp` | (v1.4 ship 时不变) + mirror 到 DSL 路径 (post-3-month) | 1 文件 |
| 新增 | `include/agenticdsl/contract/iworker_pool.h` (可选, 若 §3.5 违反时) | contract 层接口抽象 | 1 文件 |
| 修改 | `include/agenticdsl/cognitive/domain_worker_pool.h` | (如抽 contract 接口, 改为实现 contract) | 0-1 文件 |
| 修改 | `docs/adr/adr-0021-pdk-design.md §3.5` | (如 §3.5 amendment, 加 IWorkerPool 例外) | 0-1 文件 |
| 新增 | `docs/adr/adr-0089-v1-5-amendment-fork-join-dsl-migration.md` | (若立 v1.5 amendment) | 0-1 文件 |
| 新增 | `openspec/changes/2026-XX-XX-fork-join-dsl-migration/` | OpenSpec change artifacts | 1 目录 |

**总修改量估算**: ~5 新 + 3-5 改 = **~8-10 文件**, 高复杂度, **1 个完整 Sprint 工作量**.

### 风险登记

| # | 风险 | 严重度 | 缓解 |
|---|---|---|---|
| R1 | DomainWorkerPool 暴露 contract 层时性能 microsecond 级损失 | 🟢 低 | DomainWorkerPool 实现可保留, 仅抽 interface |
| R2 | DSL `fork_join.agent.md` 工具化后与原 DAG 节点 fork/join 行为不等价 | 🟠 中 | fork/join DAG 节点保留作为 alternative path; ADR-0087 benchmark 复跑作为 ground truth |
| R3 | 1 Sprint 工作量评估 (Oracle ses_edbe211c) 可能低估 | 🟡 中 | 实施前重评估; 若低估, 拆分为 2 个 sub-change (工具化 + ADR-0087 复跑) |
| R4 | ABI 暴露兼容 standalone `hydraforge-pdk` 仓库 | 🟢 低 | v1.4 已确认无外部消费者; 但 standalone repo 通知仍需经 sync-pdk.sh |
| R5 | 用户跳过 1-2 月观察期立即启动 | 🟢 低 | D9.inv.tracker-not-implementation + D9.inv.depends-on-v1.4-observation 显式约束 |
| R6 | Pattern #10: post-acceptance hygiene 风险 (per 2026-09-22 G1 案例) | 🟢 低 | 仅在 main execution 启动; 本 tracker 阶段无实际代码改动 |

---

## 立项触发条件（用户决策）

满足下列任一即可启动本 future change:

1. **观察期满触发**: ADR-0089 v1.4 ship (commits `7dc83a7` + `87aadc4` + `96172e3` + `f47acaa`, 2026-10-10) 后 ≥ 1-2 月 + 用户无 deprecation rollback 意图
2. **用户显式启动**: 用户直接命令 "启动 fork-join-dsl-migration change"
3. **强制触发**: ADR-0087 benchmark 4-worker 行为在未来 ship 中 regression (立即评估)
4. **Wave-level 触发**: Wave 3 Phase 2 或 Phase 6+ 总线服务化需要 DSL 路径并行原语 (caller 提案)
5. **must_realllm 真 LLM 链路触发**: `test_react_loop_real_llm` 或 `test_plan_execute_realllm` 因 deprecation warning 出现 LLM 调用回归

满足任一触发条件后:
- 用户决策启动 → 5-segment draft → rdd-planner → 立 OpenSpec change artifacts (4-file) → 24h cooling-off → Metis+Oracle pre-impl dual-agent review → rdd-builder P0-P3 → ship
- 不启动 → 本 tracker 长期保持 future 状态, observe 持续

---

## 参考资料

- [ADR-0089 v1.4 — Soft-Deprecate C++ Agent Loops](adr-0089-v1-4-amendment-soft-deprecate-cpp-agent-loops.md) — 本 tracker 父 ADR
- [ADR-0089 v1.3 — Loop Phase Consolidation](adr-0089-v1-3-amendment-loop-phase-shared-helpers.md) — 前置 ship, D4 forkjoin 决策来源
- [ADR-0087 — Cloud Adapter Threading Model](adr-0087-cloud-adapter-threading-model.md) — 4-worker benchmark 3.3× speedup 来源
- [ADR-0020 — Thread Model Isolation](adr-0020-thread-model-isolation.md) — DomainWorkerPool 模式
- [ADR-0021 §3.5 — PDK 头依赖限制](adr-0021-pdk-design.md#35-pdk-plugin-依赖约束) — 跨 ABI 暴露面合规要求
- [ADR-0046 — Plugin Communication Protocol](adr-0046-plugin-communication-protocol.md) — InMemoryBus 协议
- `include/agenticdsl/pdk/agent_loops/fork_join_loop.h:138-340` — ForkJoinLoop 真实实现 (Migration source of truth)
- `pdk/loop_agent/src/pdk_entry.cpp:547-603` — `loop/process_task` 当前实现 (Migration target structural reference)
- `pdk/loop_agent/README.md` "双循环架构" 段 — 现状描述
- Metis session `ses_edbe24cc1ffeqFnkc2v9e56nqt` — 8 项组件胶水分析
- Oracle session `ses_edbe211c0ffemuq7g2KEC5qglC` — Sprint 工作量估算 + ABI 合规
- AGENTS.md Pattern #12 — 双轨治理 → 单源 + 入口软停用
- AGENTS.md Pattern #10 — post-acceptance hygiene 风险预防
- AGENTS.md Pattern #4 — SHIP-with-fixes 流程

---

## 维护规则

| 触发 | 更新 |
|---|---|
| 用户决策启动未来 change | 本 tracker status → "🔄 In Implementation"，指向 OpenSpec change path |
| 启动失败 / cancel | status → "❌ Cancelled" with rationale reference |
| 1-2 月观察期满无启动 | status → "⏰ Ready to Launch" (用户决策窗口提示) |
| 实施 ship | status → "✅ Tracked (Future Change Closed)"，引向 v1.5 amendment path |
| ADR-0089 v1.4 amendment 状态变更（翻 Approved / rollback） | 复核本 tracker D9 引用一致性 |

> 本 tracker 不属于当前 sprint backlog，本会话范围外。如需启动实施，请用户显式命令。
