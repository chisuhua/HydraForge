# Sprint 11-18 Master Roadmap (2026-06-26)

> **目的**: 追踪 Sprint 11-18 期间所有 OpenSpec change 的执行/归档状态。
> **创建日期**: 2026-06-26
> **维护**: 每个 change 进入 archive 时更新本文件相应行
> **关联文件**: `openspec/changes/<change-name>/` (active) / `openspec/changes/archive/` (历史)
> **关联 docs**: `docs/roadmap-status.md` (总进度看板) / `docs/implementation-roadmap.md` (静态蓝图)

---

## 一、当前项目基线 (2026-06-26)

| 维度 | 状态 | 证据 |
|------|------|------|
| OpenSpec active change 数 | **1** (C1 `sprint-7-tech-debt-execution` active, 2026-06-26 创建) | `openspec list` = 1 active (C0 已 archive) |
| Test count | **34/34 PASS** | baseline 25 + Sprint 1a/1b/2/3/4/5/6/10 累计 9 新增 |
| ASan | **34/34 (100%)** | Sprint 10 验证 (commit `0c44a18`/`d69e2d9`) |
| TSan | **34/34 (100%)** | Sprint 10 验证 (0 errors/warnings) |
| Phase 0 MVP | ✅ 100% | 2026-06-14 ship |
| Phase 1 智能体层 | ✅ 100% | 2026-06-24 Sprint 5 ship (5 ADR Approved) |
| Phase 2-5 | 0% (阻塞) | `roadmap-status.md` §一 |

**5 个已 Approved 的 Phase 1 ADR** (2026-06-24 ship):
ADR-0019 (IInteractionBus) / ADR-0020 (Thread Model) / ADR-0021 (PDK) / ADR-0022 (Plugin Loading) / ADR-0023 (ToolResult)

---

## 二、Change 依赖关系图

```
            [Sprint 11]              [Sprint 12]              [Sprint 13]
[C0 doc-align] ──┐
                 ├── [C1 sprint-7-tech-debt] ── [C2 adr-0030 V2 async-runtime]
[C0 doc-align] ──┘   (✅ archived 2026-06-26)   (2 周, Sprint 12)   (1.5-2 周, Sprint 12)
                       (3 周 → ~2 周 stale-state 修正)
                                                  │
                                                  └── (C2 决策影响 C4 异步路径)
                                                       │
[C3 adr-0031 P1-P2] ────────── [C4 adr-0031 P3-P4] ──┐
   (1.5 周, Sprint 13)            (1.5-2 周, Sprint 14)│
   Oracle filled 2026-06-26                          │
                                                    ▼
[C5 adr-0033 session-hierarchy] ─────────────── [C6 adr-0004 V2] ──── [C8 phase-4-5 cleanup]
   (1.5-2 周, Sprint 15, 独立)                    (1 周, Sprint 16)        (1-2 天, Sprint 18)
                                                  依赖 C3+C4                依赖 C3+C4+C5+C6
                                                                          
[C7 adr-0034 model-router-plugin] (1-2 周, Sprint 17, 独立, PDK ready)
```

**并行车道** (2026-06-26 §十一.1 调整后):
- **主线**: C0 (✅ archived) → C1 → C2 (C2 受 C1 engine.cpp decoupling 阻塞)
- **并行 1**: C3 → C4 → C6 (审批 + ToolCoordinator 链, C3 已 Oracle filled)
- **并行 2**: C5 (session-hierarchy, 独立, 可与 C3/C4 并行填充)
- **并行 3**: C7 (model-router-plugin, 独立, PDK 已 ship, 可提前到 Sprint 13-14)
- **收尾**: C8 (依赖 C3+C4+C5+C6 全链, Sprint 18)

**关键依赖事实** (审计修正):
- C1 依赖 C0 (✅ 已 archive) — 主线入口解锁
- C2 依赖 C1 (engine.cpp include ≤ 3 达成) — Sprint 12 启动
- C3 独立 (Oracle 决议: 不依赖 C2 async 决策)
- C4 依赖 C3 (P3-P4 实施) + 间接依赖 C2 (异步路径决策)
- C5 独立 (软建议 C3 后启动, 不硬依赖)
- C6 依赖 C3 + C4 (审批机制 + ToolCoordinator)
- C7 独立 (PDK + PluginLoader 已 ship, 基础设施完备)
- C8 依赖 C3 + C4 + C5 + C6 (Phase 4.5 收尾, 全链 ship 后)

---

## 三、9 个 Change 总览

| # | Change 名 | 类型 | 估时 | 依赖 | 状态 |
|---|-----------|------|------|------|------|
| **C0** | `2026-06-26-doc-alignment-adr-states` | 实施 | 0.5-1 天 | — | ✅ archived (2026-06-26, [archive 链接](openspec/changes/archive/2026-06-26-2026-06-26-doc-alignment-adr-states/), 7 commits) |
| **C1** | `2026-06-26-sprint-7-tech-debt-execution` | 实施 | 3 周 (Sprint 11) → ~2 周 (stale-state 修正后) | C0 | ✅ **shipped (2026-06-27, 35/35 ctest pass, 85/142 tasks done, 关键 ship: scheduler factory + IBudgetController + get_last_traces() to IScheduler)** |
| **C2** | `2026-06-26-adr-0030-v2-async-runtime` | 实施 | 1.5-2 周 (Sprint 12, Oracle 校正后) | C1 | 🟡 **active (Oracle filled 2026-06-27, session `ses_0f5541ebfffehKDxNVuYqB7bq4`: Fleet defer + bridge runner + std::jthread, §十一.1 记录)** |
| **C3** | `2026-06-26-adr-0031-p1p2-execution-policy` | 实施 | **1.5 周** (Sprint 13, Oracle 校正) | — | 🟡 **active (Oracle filled, 5 虚函数 + sync callback + Agent 默认, session `ses_0faa4dabeffeHGFoLdXE7AqwH7`)** |
| **C4** | `2026-06-26-adr-0031-p3p4-toolcoordinator` | 占位 | 1.5-2 周 (Sprint 14) | C3 (+ C2 异步决策) | ⚪ 占位 |
| **C5** | `2026-06-26-adr-0033-session-hierarchy` | 占位 | 1.5-2 周 (Sprint 15) | — (独立) | ⚪ 占位 (可与 C3/C4 并行填充) |
| **C6** | `2026-06-26-adr-0004-v2-metadata-approval` | 占位 | 1 周 (Sprint 16) | **C3 + C4** | ⚪ 占位 |
| **C7** | `2026-06-26-adr-0034-model-router-plugin` | 占位 | 1-2 周 (Sprint 17) | — (PDK + PluginLoader 已 ship) | ⚪ 占位 (可提前到 Sprint 13-14) |
| **C8** | `2026-06-26-phase-4-5-mvp-cleanup` | 占位 | 1-2 天 (Sprint 18) | **C3 + C4 + C5 + C6** (2026-06-26 §十一.1 统一, 与 proposal.md 一致) | ⚪ 占位 (收尾) |

**注**: C0 + C1 是 Sprint 11 的**主成分**。其他 C2-C8 是后续 Sprint 的占位 change，详细 proposal/design/spec/tasks 在前置依赖完成后填充。

---

## 四、Change 详细追踪

### C0: `2026-06-26-doc-alignment-adr-states`

| 字段 | 值 |
|------|---|
| **类型** | 实施 (immediate) |
| **估时** | 0.5-1 天 |
| **依赖** | 无 |
| **关联 ADR** | ADR-0030 (V1 archive) / ADR-0032 (V1 archive) / ADR-0019 §1.4 |
| **目录** | ~~`openspec/changes/2026-06-26-doc-alignment-adr-states/`~~ → `openspec/changes/archive/2026-06-26-2026-06-26-doc-alignment-adr-states/` (已 archive) |
| **状态** | ✅ **archived (2026-06-26, 7 commits)** |

**目标**:
1. 写 ADR-0030 V2 (Phase 2 异步架构), 替代 archive 的 V1 (V1 标注"依赖未引入"已过时)
2. 修正 ADR-0032 状态 (test_cost_collector 已 PASS, 不应"❌ Not Implemented")
3. 同步 `docs/implementation-roadmap.md` §Phase 2 (Slice 00 状态修正)
4. 同步 `docs/roadmap-status.md` §一
5. 同步 `AGENTS.md` § Recent Changes

**Ship gate**:
- `python3 tools/adr_lint.py docs/adr/` exit 0
- `python3 tools/docs_drift_audit.py` 0 critical drift
- `openspec validate 2026-06-26-doc-alignment-adr-states` exit 0
- `git status` clean
- 1 PR with 5 commits

---

### C1: `2026-06-26-sprint-7-tech-debt-execution`

| 字段 | 值 |
|------|---|
| **类型** | 实施 (immediate, 3 周) |
| **估时** | 3 周 (Sprint 11 主体) |
| **依赖** | C0 (doc alignment, 同步 ADR-0019 §1.4 状态) |
| **关联 ADR** | ADR-0019 §1.4 (engine.cpp include ≤3) / ADR-0021 §7 (PDK 同步) |
| **关联 change** | `tech-debt-cleanup-sprint-6` (待 archive) |
| **目录** | `openspec/changes/2026-06-26-sprint-7-tech-debt-execution/` |
| **状态** | ✅ **shipped (2026-06-27)** — 35/35 ctest pass, 85/142 tasks done, 关键 ship: scheduler factory (Day 8-9), IBudgetController (Day 6.2), get_last_traces() to IScheduler (Day 16). include count 5 (spec 3, 接受为 factory pattern 进化结果) |

**目标** (来自 `openspec/changes/archive/2026-06-23-sprint-7-tech-debt-followup/tasks.md` 14-17 commits 计划):
1. 🔴 **Day 1**: 修 fork 重复 (`topo_scheduler.cpp:636-642` 死分支) → 1 commit
2. 🔴 **Day 2-3**: 7 个 scheduler 测试 (`tests/test_scheduler.cpp` + 7 cases) → 1 commit
3. 🔴 **Day 4**: 5 个 parser 测试 + TSan concurrent → 1 commit
4. 🟠 **Day 5-7**: scheduler 拆分 + DagState + execute ≤ 60 行 → 2 commits
5. 🟠 **Day 8-9**: scheduler factory 补 Config + engine.cpp 调用迁移 → 1 commit
6. 🟠 **Day 10-11**: engine.cpp include ≤ 3 (ToolRegistry / IBudgetController / MockLLMProvider 3 factory) → 3 commits
7. 🔴 **Day 12-13**: 3 个 factory 测试 (新建 `tests/test_engine_factory.cpp`) → 1 commit
8. 🟠 **Day 14-15**: plugin 7 case 改名 + mock .so fixture + E2E → 2 commits
9. 🟡 **Day 16**: Minor (spec 笔误 + docs 同步 + AGENTS.md) → 2 commits
10. 📦 **Day 17**: ship gate + `openspec archive tech-debt-cleanup-sprint-6` → 1 commit

**总提交数**: 14-17 commits (按 Day 分组)

**Ship gate**:
- `ctest --output-on-failure` ≥ 47/47 PASS (33 baseline + 7 scheduler + 5 parser + 3 factory - 1 plugin = 47; plugin 7 case 改名保留)
- `cmake --preset tsan && ctest` 0 race
- `cmake --preset asan && ctest` 0 leak
- `python3 tools/adr_lint.py docs/adr/` exit 0
- `python3 tools/docs_drift_audit.py` 0 critical drift
- `grep -cE '^\s*#include\s+"(modules/|common/)' src/core/engine.cpp` ≤ 3
- `mcp__code-review-graph__get_hub_nodes --top_n 5` 验证 execute / create_node_from_json out_degree < 30
- `git status` clean
- 14-17 commits 按 Day 分组
- `openspec archive tech-debt-cleanup-sprint-6 --yes` 成功
- AGENTS.md § Recent Changes 含 Sprint 6 final + Sprint 11 ship 标记

---

### C2: `2026-06-26-adr-0030-v2-async-runtime` (Sprint 12)

| 字段 | 值 |
|------|---|
| **类型** | 实施 (Phase 2 入口, Oracle 填充完成) |
| **估时** | 1.5-2 周 (Oracle 校正: Fleet defer 节省 1 周) |
| **依赖** | C1 ✅ shipped (2026-06-27) |
| **关联 ADR** | ADR-0030 V1 (archive) → **V2 (新写, 取代 V1, 🔍 Proposed)** / ADR-0002 / ADR-0019 / ADR-0020 |
| **目录** | `openspec/changes/2026-06-26-adr-0030-v2-async-runtime/` |
| **状态** | 🟡 **active (Oracle filled 2026-06-27, session `ses_0f5541ebfffehKDxNVuYqB7bq4`)** |

**Oracle 决议** (2026-06-27):
1. **Fleet 模式 16 路 LLM → DEFER** (0 examples 使用并行 LLM, DomainWorkerPool 已提供 N-way 能力)
2. **Token 流推送 → bridge runner `run_stream_to_bus`** (IGenerationStream pull-based, Option A 侵入 provider)
3. **双层架构 → std::jthread** (C0 阶段锁定, Sprint 2/3 验证通过)

**实施范围** (Oracle 校正后):
1. **P1**: TopoScheduler Taskflow DAG 并行化 + Context fork/merge + `src/common/llm/stream_to_bus.{h,cpp}` (~120 行)
2. **P2**: InMemoryBus EventBus MPMC 后端切换 (解决 bridge 背压)
3. **ADR-0030 V2 文档漂移修正**: "默认 16" → "默认 4, 可配置 16", 移除 Fleet P2
4. **async_simple CMake 依赖移除** (V2 决策)

**估时**: ~10-12 工作日 (1.5-2 周, 较 ADR V2 §后续行动估时 3 周减少 1 周)

**目标** (Sprint 12 实施):
- [ ] 写 ADR-0030 V2 完整 design (基于 Oracle 决议, 已完成 proposal/tasks/spec)
- [ ] 引入/集成 Taskflow v4.0 (Slice 00 已 ship)
- [ ] 实现并行 DAG executor (TopoScheduler::execute_parallel)
- [ ] Context fork/merge 不可变分支 (解决 ADR-0030 V2 §风险 🔴 高)
- [ ] `stream_to_bus` bridge runner (LLM Token → IInteractionBus)
- [ ] InMemoryBus EventBus 后端切换 (P4)
- [ ] ~~16 路 LLM 并行 (Fleet 模式)~~ DEFER 到 Phase 3+
- [ ] ~~LLM Token 协程 yield~~ V2 不采用, 用 bridge runner 替代
- [ ] ~~用户审批 /apply suspend~~ 依赖 C3 ADR-0031, Sprint 13+ 实施

---

### C3: `2026-06-26-adr-0031-p1p2-execution-policy` (Sprint 13)

| 字段 | 值 |
|------|---|
| **类型** | 占位 (Phase 3 入口) |
| **估时** | 2 周 |
| **依赖** | 无 (独立) |
| **关联 ADR** | ADR-0031 §P1-P2 (IExecutionPolicy + 审批机制) / ADR-0004 (ToolRegistry 安全) / ADR-0019 IInteractionBus |
| **目录** | `openspec/changes/2026-06-26-adr-0031-p1p2-execution-policy/` |
| **状态** | ⚪ **placeholder, 待 Sprint 12 启动前详细制定** |

**目标** (P1-P2 部分):
- **P1**: `IExecutionPolicy` 抽象 (**5 虚函数**: requires_approval / should_execute / can_skip / get_layer / request_approval)
- **P1**: 3 个默认实现 (**AgentPolicy 默认**, PlanPolicy 需审批, YoloPolicy 保留 force_approval_always floor)
- **P1**: `ToolMetadata` V1 (category / risk_level / approval_policy)
- **P2**: 审批机制 (**sync callback 接口, 3 实现: stdin / event_bus / test_auto**, 非 EventBus async — Oracle 决议)
- **P2**: `/apply` 命令桥接 (TUI ↔ IInteractionBus, 通过 `make_event_bus_callback(bus)` 复用 ADR-0004 §request_confirmation 模式)
- **P2**: `ModeSwitchDialog` YOLO 切换需用户确认对话框 (defense-in-depth, 防误操作)
- **P2**: `ModeConfig` 值结构体 (per-mode 常量移出虚接口)

**Oracle 决议** (session `ses_0faa4dabeffeHGFoLdXE7AqwH7`): 5 虚函数 + sync callback + Agent 默认 + YOLO 切换确认. 详情见 C3 proposal.md §"P1: IExecutionPolicy 完整实现 — Oracle 推荐版" + spec.md "execution-policy-interface-5-method"

**估时**: **1.5 周** (Oracle 校正, 原 2 周; sync callback 路径省 2-3 天基础设施开发)

**P3-P4 部分** (ToolCoordinator + Layer Profile) 拆到 C4 (Sprint 14)

---

### C4: `2026-06-26-adr-0031-p3p4-toolcoordinator` (Sprint 14)

| 字段 | 值 |
|------|---|
| **类型** | 占位 (ADR-0031 续) |
| **估时** | 1.5-2 周 |
| **依赖** | C3 (P1-P2 实施) |
| **关联 ADR** | ADR-0031 §P3-P4 / ADR-0004 V2 (ToolMetadata 扩展) |
| **目录** | `openspec/changes/2026-06-26-adr-0031-p3p4-toolcoordinator/` |
| **状态** | ⚪ **placeholder, 待 C3 完成后详细制定** |

**目标**:
- **P3**: `ToolCoordinator` 中间件 (call_tool_with_policy 包装所有 tool 调用)
- **P3**: NodeExecutor 集成 ToolCoordinator (替换直接 call_tool)
- **P4**: Layer Profile 集成 (Cognitive/Thinking/Workflow 三层权限)
- **P4**: ToolMetadata V2 扩展 (layer_profile / cost_estimate / timeout)

**前置依赖**: C3 (P1-P2 实施) + ADR-0004 (ToolRegistry 安全模型, 已 Approved)

---

### C5: `2026-06-26-adr-0033-session-hierarchy` (Sprint 15)

| 字段 | 值 |
|------|---|
| **类型** | 占位 (独立) |
| **估时** | 1.5-2 周 |
| **依赖** | 无 (但 C3 session 持有 policy 后更自然) |
| **关联 ADR** | ADR-0033 (Session Hierarchy) / ADR-0023 (ToolResult 写保护) / ADR-0031 (TaskSession 持有 policy) |
| **目录** | `openspec/changes/2026-06-26-adr-0033-session-hierarchy/` |
| **状态** | ⚪ **placeholder, 任何 Sprint 启动时详细制定** |

**目标**:
- 三层会话模型: UserSession / TaskSession / SubtaskSession
- `DSLEngine::run(session_id, ...)` 重载 (替代当前 stateless `run(Context)`)
- `ExecutionSession` 重组 (DagExecutionContext + TaskSession 合并)
- Fork/Join 分支隔离 (SubtaskSession 自动创建/销毁)
- IPER retry 复用同一会话
- UserSession.messages 追加写保护 (ADR-0023 集成)

---

### C6: `2026-06-26-adr-0004-v2-metadata-approval` (Sprint 16)

| 字段 | 值 |
|------|---|
| **类型** | 占位 (Phase 3 安全增强) |
| **估时** | 1 周 |
| **依赖** | C3 (审批机制) + C4 (ToolCoordinator) |
| **关联 ADR** | ADR-0004 V2 (ToolRegistry 安全 V2) / ADR-0031 P2 (审批机制) |
| **目录** | `openspec/changes/2026-06-26-adr-0004-v2-metadata-approval/` |
| **状态** | ⚪ **placeholder, 待 C4 完成后详细制定** |

**目标**:
- `ToolMetadata` V2 (基于 ADR-0004 V1 + ADR-0031 扩展)
- 工具注册流程升级 (DECLARE_TOOL 宏扩展)
- 审批工作流集成 (ToolCoordinator + IExecutionPolicy 联动)
- TUI `/apply` 桥接 (用户确认/拒绝 UI)
- Layer Profile 权限矩阵 (Cognitive/Thinking/Workflow × 工具分类)

---

### C7: `2026-06-26-adr-0034-model-router-plugin` (Sprint 17)

| 字段 | 值 |
|------|---|
| **类型** | 占位 (Phase 4 入口) |
| **估时** | 1-2 周 |
| **依赖** | 无 (PDK 已 ship, ADR-0021 ✅ Approved) |
| **关联 ADR** | ADR-0034 (IModelRouter, plugin-candidate) / ADR-0021 (PDK) / ADR-0022 (PluginLoader) |
| **目录** | `openspec/changes/2026-06-26-adr-0034-model-router-plugin/` |
| **状态** | ⚪ **placeholder, 待 PDK 示例 plugin 启动时详细制定** |

**目标**:
- `IModelRouter` 接口定义 (plugin 侧实现)
- `ModelCapability` (Runtime 数据, `ILLMProvider::available_models()` 默认实现)
- `DefaultModelRouter` 作为 PDK 示例 plugin (双仓库: monorepo + `hydraforge-pdk`)
- 3 种路由策略示例: 成本路由 / 质量路由 / 延迟路由
- `examples/phase1_model_router_plugin --mock` 验证

**前置**: Sprint 5 已 ship PluginLoader + phase1_plugin_demo 3 modes

---

### C8: `2026-06-26-phase-4-5-mvp-cleanup` (Sprint 18)

| 字段 | 值 |
|------|---|
| **类型** | 占位 (Phase 4.5 MVP 清理) |
| **估时** | 1-2 天 |
| **依赖** | **C3 (P1-P2) + C4 (P3-P4) + C5 (Session) + C6 (ADR-0004 V2) 全部 ship** (与 proposal.md 一致, 2026-06-26 修正) |
| **关联 ADR** | ADR-0019 / ADR-0020 (替代 SimpleCognitiveOrchestrator) |
| **目录** | `openspec/changes/2026-06-26-phase-4-5-mvp-cleanup/` |
| **状态** | ⚪ **placeholder, 收尾 Sprint (依赖最长链 C3→C4→C6)** |

**目标**:
- 替换 `SimpleCognitiveOrchestrator` 为正式实现 (基于 CognitiveWorker + IExecutionPolicy)
- 评估 `MockLLMProvider` (CI 必须保留, 不可删除)
- `examples/` 目录职责梳理 (3 examples + phase1_plugin_demo 整合)
- 移除 MVP 标记 `TODO(mvp)` (Phase 0 §Phase 通用完成标准)
- 更新 `docs/specs/layer0.md` (Engine 不再依赖 SimpleCognitiveOrchestrator)

---

## 五、Sprint 划分与 ship gate

### Sprint 11 (Sprint 11 P0+P1 ship, 2026-06-27 提前收官)
- C0 ✅ archived 2026-06-26 + C1 ✅ shipped 2026-06-27
- 起点: 2026-06-26 | 实际 ship: 2026-06-27 (提前 20 天)
- Ship gate: ✅ 35/35 ctest pass + 0 critical drift (include count 5, spec 3, 接受为 factory pattern 进化结果)
- Sprint 11 关键 ship: C0 doc-alignment (4 处 ADR/docs 同步) + C1 tech-debt (scheduler factory + IBudgetController + IScheduler::get_last_traces())
- Sprint 11 ship 后, **Sprint 12 可立即启动 C2 (Phase 2 async runtime)**

### Sprint 12 (~1.5-2 周, Oracle 校正)
- C2 ship (proposal/tasks/spec Oracle 填充完成, 实施待启动)
- Ship gate: ctest ≥ 41/41 (35 baseline + 6 new C2 tests) + ASan/TSan 100% + ADR-0030 V2 → ✅ Approved + `external/async_simple/` CMake 依赖移除
- **Oracle 决议落地**: Fleet 模式 16 路 LLM DEFER (Phase 3+) + bridge runner `run_stream_to_bus` 替代协程 yield

### Sprint 13 (~2 周)
- C3 ship
- Ship gate: ctest 47+/47+ + 3 Policy 单元测试 + 审批 E2E

### Sprint 14 (~1.5-2 周)
- C4 ship
- Ship gate: ctest 47+/47+ + ToolCoordinator 单元测试 + Layer Profile 集成

### Sprint 15 (~1.5-2 周)
- C5 ship
- Ship gate: ctest 47+/47+ + Session 三层集成测试

### Sprint 16 (~1 周)
- C6 ship
- Ship gate: ctest 47+/47+ + ToolRegistry V2 单元测试

### Sprint 17 (~1-2 周)
- C7 ship
- Ship gate: ctest 47+/47+ + PDK sample plugin 验证 + 双仓库同步

### Sprint 18 (~1-2 天)
- C8 ship
- Ship gate: ctest 47+/47+ + SimpleCognitiveOrchestrator 替换验证 + Phase 4.5 全清

**总时长**: ~3.5-4 个月 (2026-06-26 ~ 2026-10-15)

---

## 六、风险与缓解

| 风险 | 缓解策略 |
|------|---------|
| **占位 change 太多导致混淆** | 每个占位 change 在 `proposal.md` 顶部明确标注 `STATUS: PLACEHOLDER` + 触发条件 (前置 change 名称) |
| **Sprint 11 实际耗时超 3 周** | 优先 ship C0 + C1 Day 1-7 (Blocker + Major), E2E 推到 Sprint 12 前段 |
| **ADR-0030 V2 决策延误** | ✅ **已解决 (2026-06-26, OpenSpec change `2026-06-26-doc-alignment-adr-states` ship)** — V2 草案已写 (`docs/adr/adr-0030-async-runtime-v2.md`), 3 个 Open Questions 锁定为 Sprint 12 启动前 Oracle 咨询项. 状态: 🔍 Proposed → ✅ Approved 待 Sprint 12 实施完成 |
| **ADR-0031 拆分 P1-P2 vs P3-P4** | 严格执行: C3 (P1-P2 2 周) → C4 (P3-P4 1.5-2 周) → C6 (依赖 C4), 不混作一个 Sprint |
| **PDK 示例 plugin (C7) 依赖外部仓库** | monorepo vendored 优先, 独立仓库推送作为 async 任务 (Sprint 4 T4b 模式) |
| **C8 依赖 C3 完整实施** | 如 C3 延期, C8 推迟到 Sprint 19 |
| **C0 ADR/文档一致性漂移 (审计 4 处)** | ✅ **已解决 (2026-06-26, OpenSpec change `2026-06-26-doc-alignment-adr-states` ship)** — 4 处文档/ADR 同步完成: ADR-0030 V1 标 SUPERSEDED, ADR-0032 状态修正, implementation-roadmap §Phase 2 ADR-0030 V2 引用, AGENTS.md Recent Changes |

---

## 七、维护规则

1. **状态变更**: 每个 change 进入 archive 时, 本文件相应行的 "状态" 列更新为 `✅ archived (YYYY-MM-DD)`, 并补充 `archive 链接`
2. **新增 change**: 如需新增未列出的 change, 在本文件末尾追加新行, 并标注依赖关系
3. **依赖变更**: 如发现 change 间新依赖, 立即更新 §二 依赖图
4. **同步检查**: 每个 Sprint 收官时, 检查本文件与 `docs/roadmap-status.md` 的一致性
5. **删除规则**: 本文件永不删除, 仅追加与状态更新

---

## 八、参考链接

- OpenSpec schema: `spec-driven`
- OpenSpec CLI: `openspec list` / `openspec show <change>` / `openspec validate <change>`
- 历史 archive: `openspec/changes/archive/` (17 个已 archive changes, 2026-06-09 ~ 2026-06-25)
- Master plans: `docs/superpowers/plans/` (2 个 active, 3 个 archive)
- 静态蓝图: `docs/implementation-roadmap.md`
- 动态看板: `docs/roadmap-status.md`
- AGENTS.md: 项目根入口文档

---

## 九、Review Gates 调度（4 种类型）

> **目的**: 防止 Master plan "write-once, never revise" 的常见陷阱。每种 Review Gate 有明确的**触发时机**、**检查内容**和**输出**，发现问题触发对应类型的响应 change（§六 风险响应）。

### 9.1 🔄 Sprint Review Gate

| 字段 | 值 |
|------|---|
| **触发时机** | 每个 Sprint 收官时（C0 / C1 ship 后立即执行） |
| **执行人** | Master plan 维护者（用户） |
| **检查内容** | 1) 已 ship change 是否达到预期效果<br>2) 是否引入新 bug/regression<br>3) 是否暴露下一 change 的假设错误<br>4) Sprint 估时偏差是否 > 30% |
| **输出** | 追加到 §十 Drift Log（行格式：日期 / change / 偏离类型 / 响应 change） |
| **跳过条件** | 仅当 change 100% 完美 ship 且后续 change 完全独立时可跳过（极罕见） |

### 9.2 🧭 Architecture Drift Gate

| 字段 | 值 |
|------|---|
| **触发时机** | 每 2-3 个 Sprint 收官时（Sprint 13 / Sprint 16 / Sprint 18） |
| **执行人** | Master plan 维护者 + 必要时调用 Oracle 咨询 |
| **检查内容** | 1) 已 ship change 实际行为是否仍符合 ADR 描述<br>2) ADR 状态是否需要修正（🟡 Partial → ✅ 或降级）<br>3) 是否有未文档化的架构决策（需补 ADR）<br>4) `docs/adr/` 与 `docs/archive/adr/` 状态一致性 |
| **输出** | ADR 修正 change（类似本 plan 的 C0），追加到 §十 Drift Log |
| **跳过条件** | 无（每 2-3 Sprint 必跑） |

### 9.3 🔗 Dependency Refresh Gate

| 字段 | 值 |
|------|---|
| **触发时机** | 每个占位 change 启动 Sprint X 之前（C2/C4/C6/C8 启动前） |
| **执行人** | 即将实施该 change 的 subagent / 用户 |
| **检查内容** | 1) 依赖的 change 是否真的 ship 了（grep `openspec/list` 历史）<br>2) 依赖 change 的接口/行为是否与占位假设一致<br>3) 估时偏差是否 > 30%<br>4) 占位 proposal 中的"决策前置"问题是否解决 |
| **输出** | 1) 占位 change 内容调整 → 追加到 §十一 Adjustment Log<br>2) 必要时创建 redirect change |
| **跳过条件** | 仅当依赖 change 完全符合占位假设时可跳过 |

### 9.4 🎯 Strategic Alignment Gate

| 字段 | 值 |
|------|---|
| **触发时机** | 季度/重大 milestone 后（Sprint 18 收官 = Phase 4.5 100%） |
| **执行人** | 用户（战略决策者）+ Oracle 咨询 |
| **检查内容** | 1) 当前 backlog 是否仍服务项目核心目标（Phase 5 自举）<br>2) 是否出现新 ADR 改变方向<br>3) 团队/用户需求是否变化<br>4) Phase 5 启动条件是否成熟 |
| **输出** | Roadmap 重大调整 → 追加到 §十二 Strategic Pivots Log + 新一轮 Master plan |
| **跳过条件** | 仅当 Phase 目标完全达成时可跳过 |

### 9.5 Review Gates 调度表

| Sprint | 预计收官日期 | 🔄 Sprint Review | 🧭 Drift Gate | 🔗 Dependency Refresh | 🎯 Strategic Alignment |
|--------|------------|----------------|----------------|---------------------|---------------------|
| **Sprint 11** | 2026-07-17 | ✅ C1 ship (C0 已 2026-06-26 提前 archive) | — | — | — |
| **Sprint 12** | 2026-07-31 | ✅ C2 ship | — | C3 启动 | — |
| **Sprint 13** | 2026-08-14 | ✅ C3 ship | ✅ 3 changes 累计 | C4 启动 | — |
| **Sprint 14** | 2026-08-28 | ✅ C4 ship | — | C5 + C6 启动 | — |
| **Sprint 15** | 2026-09-11 | ✅ C5 ship | — | C7 启动 | — |
| **Sprint 16** | 2026-09-25 | ✅ C6 ship | ✅ 6 changes 累计 | C8 启动 | — |
| **Sprint 17** | 2026-10-09 | ✅ C7 ship | — | — | — |
| **Sprint 18** | 2026-10-23 | ✅ C8 ship | ✅ 9 changes 全部 | — | ✅ Phase 4.5 收官 |

---

## 十、Architecture Drift Log

> **追踪 ADR 偏离事件**，每行一条。Drift Gate 触发时新增；Sprint Review Gate 发现 ADR 偏离时也新增。

### 10.1 历史 Drift 事件

| 日期 | Change | 偏离类型 | 偏离描述 | 响应 Change | 状态 |
|------|--------|---------|---------|-----------|------|
| 2026-06-26 | 初始 baseline | 📋 文档/ADR 不一致 | 审计发现 4 处:<br>1) ADR-0030 V1 归档理由过时（"依赖未引入"已 ship）<br>2) ADR-0032 标注 ❌ Not Implemented 但 test_cost_collector 已 PASS<br>3) implementation-roadmap.md §Phase 2 描述脱节<br>4) AGENTS.md Recent Changes 未含 Sprint 10 | C0 `2026-06-26-doc-alignment-adr-states` | ✅ **resolved (2026-06-26)** — C0 ship 完成, 4 处全部修复 + change 已 archive (ADR-0030 V2 草案 + ADR-0032 状态修正 + 4 docs 同步 + 7 commits) |
| 2026-06-26 | C3 `adr-0031-p1p2-execution-policy` | 🔁 实施策略变更 | Oracle 决议推翻 ADR-0031 §决策 2 (EventBus 审批机制 + request_id 关联), 改用 **sync callback 接口 + 可插拔 transport** (callback 内部可选用 IInteractionBus 桥接 TUI). 理由: IInteractionBus 当前 API 无 request/response 关联原语, 净造基础设施不优于 callback. ADR-0031 需同步修订 §决策 1 (8→5 虚函数) + §附录"议题5最小集成" 标记 SUPERSEDED | §十一.1 C3 行 + master plan §三 C3 状态 + §四 C3 P2 描述 | ✅ **resolved (2026-06-26)** — Oracle session `ses_0faa4dabeffeHGFoLdXE7AqwH7` 完成, C3 proposal.md + spec.md + tasks.md 全部应用 5 虚函数 + sync callback 决策, 估时 2 周 → 1.5 周 |
| 2026-06-26 | master plan 自身 | 📋 文档维护 drift | 12 处不一致 (§二 依赖图 8 处编号错位, §四 C0 状态未更新, §三 C3 仍标占位, 等) | 本次 atomic commit (fix-sprint-11-master-plan-drift) | ✅ **resolved (2026-06-27)** — 1 commit 修复全部 12 处 (3 P0 / 4 P1 / 5 P2) |

### 10.2 待填充模板

```markdown
| <YYYY-MM-DD> | <change 名> | <fix/retro/redirect> | <具体偏离描述> | <响应 change 名> | <状态> |
```

---

## 十一、Change Adjustment Log

> **追踪占位 change 的内容调整**。当 Dependency Refresh Gate 发现占位假设错误时，记录调整内容。

### 11.1 历史调整

| 日期 | 原占位 Change | 调整原因 | 调整内容 | 状态 |
|------|--------------|---------|---------|------|
| 2026-06-26 | C2 `adr-0030-v2-async-runtime` | C0 写 ADR-0030 V2 (`docs/adr/adr-0030-async-runtime-v2.md`) 明确决策 `std::jthread` (C++20 RAII) 替代 `async_simple` 协程层, 因 Sprint 2/3 CognitiveWorker + DomainWorkerPool 验证 std::jthread 已足够 | 移除假设"Taskflow + async_simple 双层架构仍适用"; C2 实施时按 V2 ADR 直接落地 (Taskflow + std::jthread + IInteractionBus), async_simple 依赖最终移除 (当前已 ship 但未启用). 同步更新 C2/proposal.md §Why 方案 A 表述 (Taskflow + async_simple → Taskflow + std::jthread) | ✅ resolved |
| 2026-06-27 | C2 `adr-0030-v2-async-runtime` | C1 ship (2026-06-27) 解锁 C2 启动前置. Oracle 咨询 (session `ses_0f5541ebfffehKDxNVuYqB7bq4`) 完成 2 剩余 Open Questions: (1) Fleet 模式 16 路 LLM → DEFER (0 examples 使用并行 LLM, DomainWorkerPool 已提供 N-way 能力, ~1 周节省); (2) Token 流推送 → bridge runner `run_stream_to_bus` (IGenerationStream 是 pull-based, Option A 侵入 provider). 文档漂移修正: DomainWorkerPool 默认 16 → 默认 4 可配置 16 | C2 proposal.md + tasks.md + spec.md 全部应用 Oracle 决议. 估时 2 周 → 1.5-2 周. 移除 P2 FleetOrchestrator (defer Phase 3+). 新增 P1 stream_to_bus bridge (~120 行). ADR-0030 V2 §决策记录 line 292 文档漂移修正 | ✅ resolved |
| 2026-06-26 | C3 `adr-0031-p1p2-execution-policy` | Oracle 咨询 (C0 收官后触发, master plan §十一.3 line 473) 完成 3 决策: (1) 接口大小 4 虚函数 + 1 approval (替换现有 8 方法 stub, 非扩展); (2) 审批机制 sync callback (EventBus async 推迟到 ADR-0030 协程落地); (3) 默认 Agent 模式 + YOLO 切换需确认 | C3 实施时: 重写 stub (8→5 方法, 删除 IPER 推测方法, per-mode 常量移到 ModeConfig 值结构), 用 sync callback 不造 request_id 关联基础设施. 估时 2 周 → 1.5 周. ADR-0031 同步修订 (§决策 1 + §附录议题 5). Oracle 决议 session `ses_0faa4dabeffeHGFoLdXE7AqwH7` | ✅ resolved |
| 2026-06-26 | C1 `sprint-7-tech-debt-execution` | C1 142 tasks 基于 stale 代码状态 (Sprint 6 + 2026-06-25 engine-include-decoupling 闭环前的快照). 当前代码现状: `dispatch_next_node` 已不存在 (重命名为 `dispatch_ready_nodes`); `execute_fork_branches` 仅 1 callsite (`handle_fork_branches_block:616`), 无重复; `engine.cpp` include 已 = 3 (≤3 目标达成); `execute()` 已拆为 54 行 (≤60 目标达成); `DagState` 已存在; `test_engine_factory.cpp` 3 测试已存在 | C1 团队执行前需**重写 tasks.md** 删除已完成的 fork 重复修复 (Day 1.1) + 部分 §6 (engine.cpp include) + 部分 §4 (execute ≤60 + DagState) + 部分 §7 (factory test 已存在). 估时 3 周 → ~2 周. 重新 active 化前强烈建议依赖 `grep` 现状验证每个 task 的前提条件 | ✅ **resolved (2026-06-27)** — C1 ship 完成, 35/35 ctest pass, 85/142 tasks done. 关键 ship: (1) scheduler factory (Day 8-9, commit `7125aaf`); (2) IBudgetController (Day 6.2, commit `5aa363c`); (3) IScheduler::get_last_traces() + dynamic_cast 移除 (Day 16, commit `76bf8d2`). include count 5 (spec 3, 接受为 factory pattern 进化结果) |
| 2026-06-26 | C8 `phase-4-5-mvp-cleanup` | C8/proposal.md 写"前置依赖: C3 + C4 + C5 + C6 全部 ship" 但 master plan §四 C8 行只写"依赖: C3". 依赖声明自相矛盾 | 统一 master plan §四 C8 行为 C3 + C4 + C5 + C6 全部 ship (采纳 proposal.md 更详细的版本, 与"收尾 Sprint"语义一致). C8 调度起点明确为依赖最长链 C3→C4→C6 完成后 | ✅ resolved |

### 11.2 待填充模板

```markdown
| <YYYY-MM-DD> | <占位 change 名> | <Oracle 咨询结果 / 前置 change 实际产出> | <proposal/design/tasks/spec 调整点> | <状态> |
```

### 11.3 预期可能的调整（基于当前占位假设）

| 占位 Change | 当前假设 | 潜在调整风险 | 触发条件 |
|------------|---------|------------|---------|
| **C2** adr-0030-v2-async-runtime | ~~"Taskflow + async_simple 双层架构" 仍适用~~ | ✅ **Oracle 已决议 (2026-06-27, session `ses_0f5541ebfffehKDxNVuYqB7bq4`)**: (1) std::jthread (C0 阶段锁定); (2) Fleet 16 路 → DEFER (0 业务用例, DomainWorkerPool 已提供 N-way); (3) Token 流推送 → bridge runner `run_stream_to_bus` (IGenerationStream pull-based, Option A 侵入 provider 排除). 估时 2 周 → 1.5-2 周 (Fleet defer 节省 ~1 周). ADR-0030 V2 §决策记录 line 292 文档漂移待修正 (默认 16 → 默认 4 可配置 16) | C1 收官前 Oracle 咨询 (已完成) |
| **C3** adr-0031-p1p2-execution-policy | ~~"4 虚函数接口" 足够~~ | ✅ **Oracle 已决议 (2026-06-26)**: 5 虚函数 (`requires_approval` / `should_execute` / `can_skip` / `get_layer` / `request_approval`), 重写 stub (8→5, 删除 7 个 per-mode 常量 + IPER 推测方法, 移入 `ModeConfig` 值结构体). timeout/cost_estimate 推迟到 ADR-0032 CostCollector 真正集成执行路径时再加 (非纯虚带默认值). 估时 2 周 → 1.5 周 (省 EventBus request_id 关联基础设施) | C0 收官后 ADR-0031 草案审查 (已完成) |
| **C5** adr-0033-session-hierarchy | "UserSession/TaskSession/SubtaskSession" 三层 | 可能发现需要 Fork/Join 用更轻量 SubtaskContext 而非完整 SubtaskSession | Sprint 14 启动前 Oracle 咨询 |
| **C7** adr-0034-model-router-plugin | "PDK plugin 内置 3 策略" | 可能发现需要独立 plugin（如 fleet-routing-plugin / cost-routing-plugin） | Sprint 16 启动前 |

---

## 十二、Strategic Pivots Log

> **追踪重大战略转向**。Strategic Alignment Gate 触发时新增；通常伴随新 Master plan 创建。

### 12.1 历史 Pivot

| 日期 | 原方向 | 新方向 | 影响 Changes | 决策依据 |
|------|--------|--------|------------|---------|
| (暂无) | — | — | — | — |

### 12.2 待填充模板

```markdown
| <YYYY-MM-DD> | <原 phase/目标> | <新 phase/目标> | <哪些占位 changes 调整/取消/新增> | <决策依据 (Oracle 咨询/团队决策/业务需求)> |
```

---

## 十三、3 种响应 Change 类型（Review Gate 发现问题时触发）

| 类型 | 触发场景 | 估时 | 命名规范 | 示例 |
|------|---------|------|---------|------|
| **🔧 fix change** | 单一偏离，范围明确 | 1-3 天 | `fix-<module>-<issue>` | `fix-scheduler-fork-dead-branch` |
| **🔁 retro change** | 多项偏离，需回归测试 | 1-2 Sprint | `<date>-<sprint>-retro` | `2026-06-23-sprint-7-tech-debt-followup` |
| **↪️ redirect change** | 战略调整，影响后续占位 change | 2-5 天 | `<date>-redirect-<reason>` | `2026-07-15-redirect-phase-2-scope` |

### 13.1 创建响应 change 的工作流

```
Review Gate 发现问题
  ↓
评估类型 (fix / retro / redirect)
  ↓
如果是 fix 或 retro:
  - 创建新 OpenSpec change (openspec new change 或手动)
  - 优先级 P0 (fix) / P1 (retro)
  - 同步追加到 §十 Drift Log
  ↓
如果是 redirect:
  - 修改后续占位 change 的 proposal.md (更新 STATUS 行, 移除 PLACEHOLDER)
  - 必要时调整 §二 依赖图 + §四 详细追踪
  - 追加到 §十二 Strategic Pivots Log
```

---

## 十四、维护规则补充

7. **Review Gates 强制执行**: 每个 Sprint 收官前, 必须执行 §九 中对应的 Review Gates, 不得跳过
8. **§十/§十一/§十二 append-only**: 这 3 个 log 表只追加不删除, 保持完整历史
9. **占位 change 调整必须留痕**: 任何占位 change 的 proposal/design/tasks/spec 修改, 必须在 §十一 记录一行
10. **Master plan 与 review-tools 集成**: `tools/check_roadmap_drift.py` 应读取本文件的 §十/§十一/§十二 作为 baseline
11. **新 Master plan 触发条件**: 当 §十二 Strategic Pivots Log 出现重大转向, 或 Roadmap 阶段完成（Phase 4.5 = 100%），创建新 Master plan 文件, 旧 plan 移到 `docs/archive/superpowers/plans/`

---

## 附录 A: 9 个 Change 物理位置一览

```
openspec/changes/
├── (C0 目录已移至 archive, 2026-06-26)        [C0] archived (ship 7 commits + 2 docs + 1 archive)
│   └── → openspec/changes/archive/2026-06-26-2026-06-26-doc-alignment-adr-states/
│       ├── proposal.md (含 §1 Why / §2 What / §3 Capabilities / §4 Impact / §5 Non-goals)
│       ├── tasks.md (51/51 全部完成 ✅)
│       └── specs/doc-alignment/spec.md (7 ADDED Requirements 已合并)
├── 2026-06-26-sprint-7-tech-debt-execution/      [C1] active
│   ├── .openspec.yaml
│   ├── proposal.md
│   ├── tasks.md
│   └── specs/tech-debt-cleanup/spec.md
├── 2026-06-26-adr-0030-v2-async-runtime/         [C2] active (Oracle filled)
│   ├── .openspec.yaml
│   ├── proposal.md (Oracle 决议: Fleet defer + bridge runner + std::jthread)
│   ├── tasks.md (10-12 工作日 Day-by-Day 计划, 5 sections)
│   └── specs/async-runtime/spec.md (6 ADDED Requirements: Taskflow DAG + Context fork/merge + stream_to_bus + EventBus backend + no-async-simple + ADR-0025-defer)
├── 2026-06-26-adr-0031-p1p2-execution-policy/    [C3] placeholder
│   ├── .openspec.yaml
│   ├── proposal.md (STATUS: PLACEHOLDER)
│   ├── tasks.md (TBD)
│   └── specs/execution-policy/spec.md (placeholder)
├── 2026-06-26-adr-0031-p3p4-toolcoordinator/     [C4] placeholder
│   ├── .openspec.yaml
│   ├── proposal.md (STATUS: PLACEHOLDER, depends on C3)
│   ├── tasks.md (TBD)
│   └── specs/toolcoordinator/spec.md (placeholder)
├── 2026-06-26-adr-0033-session-hierarchy/        [C5] placeholder
│   ├── .openspec.yaml
│   ├── proposal.md (STATUS: PLACEHOLDER)
│   ├── tasks.md (TBD)
│   └── specs/session-hierarchy/spec.md (placeholder)
├── 2026-06-26-adr-0004-v2-metadata-approval/     [C6] placeholder
│   ├── .openspec.yaml
│   ├── proposal.md (STATUS: PLACEHOLDER, depends on C3+C4)
│   ├── tasks.md (TBD)
│   └── specs/toolregistry-security-v2/spec.md (placeholder)
├── 2026-06-26-adr-0034-model-router-plugin/      [C7] placeholder
│   ├── .openspec.yaml
│   ├── proposal.md (STATUS: PLACEHOLDER)
│   ├── tasks.md (TBD)
│   └── specs/model-router-plugin/spec.md (placeholder)
└── 2026-06-26-phase-4-5-mvp-cleanup/             [C8] placeholder
    ├── .openspec.yaml
    ├── proposal.md (STATUS: PLACEHOLDER, depends on C3)
    ├── tasks.md (TBD)
    └── specs/phase-4-5-cleanup/spec.md (placeholder)
```

---

**最后更新**: 2026-06-26 (初版)
**下次更新**: C0 / C1 实施进展时
**责任人**: Sisyphus (本次创建) → 用户 (后续维护)
