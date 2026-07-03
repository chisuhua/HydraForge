# Phase 5 — Self-Bootstrapping & Service-ification Master Plan (2026-07-03 to 2026-10-31)

> **目的**: 追踪 Sprint 19-25 期间所有 OpenSpec change 的执行/归档状态,覆盖 Phase 5 自举服务化三阶段。
> **创建日期**: 2026-07-03
> **触发条件**: Strategic Alignment Gate §9.4 (`2026-06-26-sprint-11-to-18-roadmap.md`) — Sprint 18 (Phase 4.5) 100% 收官
> **维护**: 每个 change 进入 archive 时更新本文件相应行
> **关联文件**: `openspec/changes/<change-name>/` (active) / `openspec/changes/archive/` (历史)
> **关联 docs**: `docs/roadmap-status.md` (总进度看板) / `docs/proposals/implementation/self-bootstrapping-path.md` (BOOT-001 阶段 0-3) / `docs/proposals/implementation-roadmap/01-roadmap.md` (IP-001 Step 0-6) / `2026-06-26-sprint-11-to-18-roadmap.md` (上游 9-change 路线图, 已 archive)

---

## 一、当前项目基线 (2026-07-03)

| 维度 | 状态 | 证据 |
|------|------|------|
| OpenSpec active change 数 | **1** (C9 `2026-07-03-phase4-5-impl-scope-audit`, 占位) | `ls openspec/changes/` = 1 active (C0-C8 全部 archived) |
| Test count | **61/61 PASS** | baseline 25 + Sprint 1a/1b/2/3/4/5/6/10/15/16/17/19/20 累计 36 新增 |
| ASan | **61/61 (100%)** | Sprint 10 验证 + Sprint 17 Phase 1+2 后复验 |
| TSan | **61/61 (100%)** | Sprint 10 修复验证 (P1 jthread + P2 atomic flag) |
| Phase 0 MVP | ✅ 100% | 2026-06-14 ship |
| Phase 1 智能体层 | ✅ 100% | 2026-06-24 Sprint 5 ship (5 ADR Approved) |
| Phase 2 异步运行时 | ✅ 100% | 2026-07-31 C2 ship + archive (ADR-0030 V2 → ✅ Approved) |
| Phase 3 执行策略+安全 | ✅ 100% | 2026-07-02 C5+C6 ship + archive |
| Phase 4 模型路由 | ✅ 100% | 2026-07-02 C7 fully shipped |
| Phase 4.5 MVP清理 | ✅ 100% | 2026-07-03 C8 ship (SimpleCognitiveOrchestrator @internal + examples/ 目录梳理) |
| **Phase 5 自举服务化** | **0% (待启动)** | **C9 audit 完成后启动 C10-C14, 详见本 plan** |

**C9 audit 关键结论** (详见 `openspec/changes/2026-07-03-phase4-5-impl-scope-audit/proposal.md`):
- 11 个 ADR 全部需要 impl-scope audit (类声明 vs 实际代码 grep 比对)
- 预计输出: 11 个 `*-impl-scope.md` 子文档 + ADR 状态校准
- C9 ship 是 Phase 5 启动的硬前置 (0 个 DRIFT items)

**Strategic Pivot 决议** (见 `2026-06-26-sprint-11-to-18-roadmap.md` §十二.1):
- 路径选 B (tech-debt retro 先) 而非 A (直接启动), 避免 Phase 5 中途被 drift 中断
- C9 是 Phase 4.5 → Phase 5 过渡的 audit 中间阶段

---

## 二、Change 依赖关系图

```
              [Sprint 19]            [Sprint 20-22]              [Sprint 23-25]
[C9 audit] ─────┐                     ┌── [C10 phase5-stage1-step0]  (Lazy ModuleState)
 (1 周)         │                     │     估时 1-2 天
 (Phase 4.5→5   │                     │     Oracle 决议: Session 3 (无 schema/imports/fork_behavior)
  过渡 audit)   │                     │
                ├── [C11 phase5-stage1-step1]  (Session Registry + Session Vars)
                │     估时 2-3 天
                │     Oracle 决议: SessionRegistry 由 DSLEngine 持有 (与 ToolRegistry 模式一致)
                │
                ├── [C12 phase5-stage1-step2]  (YIELD / STREAM 节点)
                │     估时 2-3 天
                │     Oracle 决议: 单 YIELD 节点类型, mode 参数 (next/continue/stop)
                │
                │   ┌── [并行: 推理标准库 7 子图] ──┐
                │   │   2a (立即): engine.md / model.md / session.md (3/7)
                │   │   2b (Step 0 后): prefix_cache.md / kv_cache.md / decoding.md
                │   │   2c (Step 2 后): batching.md
                │   └────────────────────────────────┘
                │
                └── [C13 phase5-stage2-step3-4]  (Fork per-field + Checkpoint)
                       估时 3-5 天
                       触发条件: 性能测试显示 deep_copy 成为瓶颈 (Step 3 延后)
                       触发条件: Session 迁移或容错需求出现 (Step 4 延后)
                      
[C14 phase5-stage3-step5-6]  (静态分析 + 服务化)
   估时 4-6 周
   依赖: C10+C11+C12+C13 ship + Phase 3 稳定运行
```

**并行车道** (基于 `docs/proposals/implementation-roadmap/01-roadmap.md` §三阶段):
- **主线**: C9 (audit) → C10 (Lazy ModuleState) → C11 (Session Registry) → C12 (YIELD/STREAM)
- **并行 1**: 推理标准库 7 子图 (engine/model/session 立即, prefix_cache/kv_cache/decoding Step 0 后, batching Step 2 后)
- **延后启动**: C13 (Stage 2) 在性能/运维需求出现时启动
- **远期**: C14 (Stage 3) 在 Stage 1+2 稳定后启动

**关键依赖事实**:
- C9 独立 (0 依赖, Phase 4.5 → Phase 5 过渡前置)
- C10 依赖 C9 (audit 0 DRIFT)
- C11 独立于 C10 (SessionRegistry 与 ModuleState 平行) 但**实施建议顺序 C10 → C11** (Oracle 推荐增量引入)
- C12 依赖 C10 (YIELD 需要 module_state 持久化基础)
- C13 延后启动, 依赖 Stage 1 全链 ship
- C14 延后启动, 依赖 Stage 1+2 全链 ship + Phase 3 稳定

---

## 三、6 个 Change 总览 (C9-C14)

| # | Change 名 | 类型 | 估时 | 依赖 | 状态 |
|---|-----------|------|------|------|------|
| **C9** | `2026-07-03-phase4-5-impl-scope-audit` | 审计 | 1 周 | C8 ✅ archived (2026-07-03) | 🟡 **in progress** (Agent 1 工作, 11 个 ADR impl-scope.md 编写) |
| **C10** | `2026-07-XX-phase5-stage1-step0-lazy-modulestate` | 实施 | 1-2 天 | C9 ✅ ship | ⚪ **placeholder, 待 C9 完成后启动** |
| **C11** | `2026-07-XX-phase5-stage1-step1-session-registry` | 实施 | 2-3 天 | C9 ✅ ship (+ 建议 C10 ship 后启动) | ⚪ **placeholder, 待 C9+C10 完成后详细制定** |
| **C12** | `2026-07-XX-phase5-stage1-step2-yield-stream` | 实施 | 2-3 天 | C10 ✅ ship (YIELD 需要 module_state 持久化) | ⚪ **placeholder, 待 C10 完成后详细制定** |
| **C13** | `2026-07-XX-phase5-stage2-step3-4-fork-checkpoint` | 实施 (延后) | 3-5 天 | C10+C11+C12 ✅ ship + 性能/运维需求触发 | ⚪ **placeholder, 远期延后** |
| **C14** | `2026-07-XX-phase5-stage3-step5-6-analysis-service` | 实施 (远期) | 4-6 周 | C10+C11+C12+C13 ✅ ship + Phase 3 稳定 | ⚪ **placeholder, 远期延后** |

**注**: C9 是 audit change, 不属于 Phase 5 实施主体。Phase 5 实际执行链路为 C10 → C11 → C12 → (C13 远期) → (C14 远期), 详细 proposal/design/spec/tasks 在前置依赖完成后填充。

---

## 四、Change 详细追踪

### C9: `2026-07-03-phase4-5-impl-scope-audit`

| 字段 | 值 |
|------|---|
| **类型** | 审计 (Phase 4.5 → Phase 5 过渡) |
| **估时** | 1 周 |
| **依赖** | C8 (Phase 4.5 MVP 清理) ✅ archived (2026-07-03) |
| **关联 ADR** | ADR-0001/0003/0004/0005/0007/0008/0019/0020/0022/0023/0033 (11 个) |
| **关联工具** | `tools/docs_drift_audit.py` (11 DRIFT items) + `docs/adr-management/STATUS-GLOSSARY.md` |
| **目录** | `openspec/changes/2026-07-03-phase4-5-impl-scope-audit/` |
| **状态** | 🟡 **in progress** (Agent 1 并行工作) |

**目标**:
1. 11 个 ADR 全部创建 `*-impl-scope.md` 子文档, 把"ADR 原始描述的类"按 Shipped / Evolved / Deferred 三类重新归类
2. ADR 状态校准 (Shipped/Evolved 保持 ✅ Approved, Deferred 改 🟡 Partial)
3. 文档同步 (更新 `docs/README.md` 表格 + `docs/adr-management/relationships.md` + `docs/roadmap-status.md`)

**Ship gate**:
- `python3 tools/docs_drift_audit.py` 输出 `0 DRIFT items` (原本 11)
- `python3 tools/adr_lint.py` exit 0
- `python3 tools/adr_relationships.py` 成功生成
- `openspec validate 2026-07-03-phase4-5-impl-scope-audit` exit 0

---

### C10: `phase5-stage1-step0-lazy-modulestate` (Sprint 19)

| 字段 | 值 |
|------|---|
| **类型** | 实施 (Stage 1 入口, IP-001 §阶段 1 Step 0) |
| **估时** | 1-2 天 |
| **依赖** | C9 ✅ ship |
| **关联 ADR** | ADR-0008 (LayeredContext 5 层, 验证) / ADR-0033 (Session Hierarchy, 容器地址稳定) / IP-001 §三 Step 0 |
| **目录** | `openspec/changes/<date>-phase5-stage1-step0-lazy-modulestate/` |
| **状态** | ⚪ **placeholder, 待 C9 完成后启动** |

**目标** (来自 IP-001 §阶段 1 Step 0):
- 模块状态通过 json scope nesting 持久化 (`session.module_states["/lib/inference/kv_cache"] = {...}`)
- Lazy init: 首次 dsl_call 时自动创建空 json
- 无 schema 校验, 无 imports 声明, 无 fork_behavior 配置 (Oracle Session 3 决议)

**关键 ship 列表**:
1. `src/modules/scheduler/execution_session.h` 新增 `module_states_: map<string, json>` 成员
2. `src/modules/scheduler/execution_session.cpp` 新增 `ensure_module_state()` lazy init
3. `src/modules/executor/node_executor.cpp` dsl_call 执行时传入 module_states_ 引用
4. 1 个新测试 `tests/test_lazy_modulestate.cpp` (创建/读取/跨 dsl_call 持久化验证)
5. 文档: `docs/specs/dsl.md` 新增 `module_state` 语法段 (lazy 模式说明)

**Ship gate**:
- `ctest --output-on-failure` ≥ 62/62 PASS
- 跨 dsl_call 状态持久化 E2E 测试通过
- `python3 tools/adr_lint.py` exit 0
- 0 编译错误, ASan/TSan 0 警告

---

### C11: `phase5-stage1-step1-session-registry` (Sprint 20)

| 字段 | 值 |
|------|---|
| **类型** | 实施 (Stage 1 续, IP-001 §阶段 1 Step 1) |
| **估时** | 2-3 天 |
| **依赖** | C9 ✅ ship (建议 C10 ✅ ship 后启动, Oracle 增量引入) |
| **关联 ADR** | ADR-0033 (Session Hierarchy 验证, UserSession 是 SessionRegistry 注册单元) / IP-001 §三 Step 1 |
| **目录** | `openspec/changes/<date>-phase5-stage1-step1-session-registry/` |
| **状态** | ⚪ **placeholder, 待 C9+C10 完成后详细制定** |

**目标** (来自 IP-001 §阶段 1 Step 1):
- 多 Session 隔离, Session 级变量
- SessionRegistry 由 DSLEngine 持有 (与 ToolRegistry 模式一致, 成员非单例)
- SessionVars 在 ExecutionSession 中隔离
- 注册 `session.create` / `session.destroy` / `session.set_var` / `session.get_var` 工具

**关键 ship 列表**:
1. 新建 `src/modules/scheduler/session_registry.h/cpp` (SessionRegistry 类)
2. `src/modules/scheduler/execution_session.h` 新增 `session_id_` / `session_vars_`
3. `DSLEngine` 持有 `session_registry_` (PIMPL-lite, 避免模块耦合)
4. 注册 4 个 session.* 工具到 `src/common/tools/registry.cpp`
5. 1 个新测试 `tests/test_session_registry.cpp` (创建 2 个 Session 验证隔离 + 销毁释放)

**Ship gate**:
- `ctest --output-on-failure` ≥ 63/63 PASS
- 2 Session 状态隔离 E2E 测试通过
- `tools/code-review-graph` 显示 `session_registry_` 与 `tool_registry_` 同耦合模式

---

### C12: `phase5-stage1-step2-yield-stream` (Sprint 21-22)

| 字段 | 值 |
|------|---|
| **类型** | 实施 (Stage 1 收尾, IP-001 §阶段 1 Step 2) |
| **估时** | 2-3 天 (单 Sprint 装不下, 拆为 2 周) |
| **依赖** | C10 ✅ ship (YIELD 需要 module_state 持久化) |
| **关联 ADR** | ADR-0008 (LayeredContext) / ADR-0019 (IInteractionBus token push 复用) / IP-001 §三 Step 2 |
| **目录** | `openspec/changes/<date>-phase5-stage1-step2-yield-stream/` |
| **状态** | ⚪ **placeholder, 待 C10 完成后详细制定** |

**目标** (来自 IP-001 §阶段 1 Step 2):
- Token-by-token 生成器 (YIELD 节点)
- 单 YIELD 节点类型 (无 CONTINUE_STREAM/STOP_STREAM, Oracle Session 3 决议)
- mode 参数控制行为: next / continue / stop
- Session 持有 `std::optional<YieldState> pending_yield_`

**关键 ship 列表**:
1. `src/core/types/node.h` NodeType 枚举新增 `YIELD` (yield_value 字段, mode 字段)
2. `src/modules/parser/markdown_parser.cpp` 解析 yield 节点 (mode 字段)
3. `src/modules/executor/node_executor.h/cpp` 新增 `execute_yield()` (渲染 yield_value 模板)
4. `src/modules/scheduler/topo_scheduler.cpp` yield 暂停逻辑 + resume 回调
5. 1 个新测试 `tests/test_yield_node.cpp` (token generator .agent.md 调用 N 次验证每次返回 1 token)
6. 1 个 examples: `examples/phase5_yield_token_generator/main.cpp` (演示 YIELD)

**Ship gate**:
- `ctest --output-on-failure` ≥ 64/64 PASS
- `examples/phase5_yield_token_generator` 可运行, N 次调用返回 N 个 token
- yield 之间 module_state 保持 E2E 测试通过
- `tools/adr_lint.py` exit 0

---

### C13: `phase5-stage2-step3-4-fork-checkpoint` (远期)

| 字段 | 值 |
|------|---|
| **类型** | 实施 (Stage 2 入口, IP-001 §阶段 2 Step 3+4) |
| **估时** | 3-5 天 (Step 3 2-3 天 + Step 4 1-2 天) |
| **依赖** | C10+C11+C12 ✅ ship + 性能测试/运维需求触发 |
| **关联 ADR** | ADR-0008 / ADR-0033 / IP-001 §三 Step 3-4 |
| **目录** | `openspec/changes/<date>-phase5-stage2-step3-4-fork-checkpoint/` |
| **状态** | ⚪ **placeholder, 远期延后** |

**目标** (来自 IP-001 §阶段 2 Step 3+4):
- **Step 3 (延后)**: Fork 语义扩展 (per-field fork_behavior: COW/INHERIT/SHARE_READONLY), 触发条件 = 性能测试显示 deep_copy 成为瓶颈
- **Step 4 (延后)**: Checkpoint/Restore (Session 状态可序列化), 触发条件 = Session 迁移/容错需求出现

**关键 ship 列表** (C13 启动时细化):
1. `src/core/types/node.h` ForkNode 新增 `fork_behaviors: map<string, ForkBehavior>` 字段
2. `src/modules/parser/markdown_parser.cpp` 解析 fork_behaviors 字段
3. `src/modules/scheduler/topo_scheduler.cpp` start_fork_simulation 处理 fork_behaviors
4. `src/modules/scheduler/session_registry.h/cpp` 新增 `checkpoint()` / `restore()` 方法
5. 注册 `session.checkpoint` / `session.restore` 工具

**Ship gate** (C13 启动时细化):
- Fork COW 行为 E2E 测试 (读零开销, 写才复制)
- Checkpoint/Restore 状态可序列化测试
- Session 迁移测试

---

### C14: `phase5-stage3-step5-6-analysis-service` (远期)

| 字段 | 值 |
|------|---|
| **类型** | 实施 (Stage 3 入口, IP-001 §阶段 3 + BOOT-001 §阶段 2-3) |
| **估时** | 4-6 周 |
| **依赖** | C10+C11+C12+C13 ✅ ship + Phase 3 稳定运行 |
| **关联 ADR** | ADR-0001 / ADR-0005 / ADR-0019 / IP-001 §三 Step 5-6 + BOOT-001 任务 2.1-3.1 |
| **目录** | `openspec/changes/<date>-phase5-stage3-step5-6-analysis-service/` |
| **状态** | ⚪ **placeholder, 远期延后** |

**目标** (来自 IP-001 §阶段 3 + BOOT-001 §阶段 2-3):
- **Step 5 (延后)**: 静态分析优化 (消除热路径首次调用延迟), 触发条件 = 生产环境性能测试
- **Step 6 / BOOT-001 任务 2.1-3.1 (远期)**: QualityFeedbackController + InferenceServer 服务化 + MetaOptimizer 自进化

**关键 ship 列表** (C14 启动时细化):
1. `src/modules/library/library_loader.h/cpp` 新增 `build_reachability_graph()` (Step 5)
2. `src/modules/scheduler/execution_session.cpp` 解析时预初始化可达模块 (Step 5)
3. `src/modules/budget/quality_feedback_controller.h/cpp` (BOOT-001 任务 2.1)
4. `examples/adaptive_optimize.agent.md` (BOOT-001 任务 2.2)
5. `src/api/inference_server.h/cpp` (BOOT-001 任务 3.1, MCP + OpenAI 兼容)
6. 7+ 个推理标准库子图 (lib/inference/) 补齐到 7/7

**Ship gate** (C14 启动时细化):
- 静态分析覆盖率达 > 90%, 预热后首次调用延迟 < 未预热 10%
- QualityFeedbackController 自适应优化 100 轮内收敛
- InferenceServer MCP/OpenAI 兼容接口通过标准测试
- 完全自举: Agent 自主发现新优化策略

---

## 五、阶段 1 任务切分 (C10 + C11 + C12 + 推理标准库并行)

> **目标**: Agent 可通过 DSL 控制推理参数, 推理标准库 (lib/inference/) 可被 Agent 工作流调用
> **里程碑**: 一个推理 Session 可以完整创建/配置/运行/销毁; 多个 Session 之间状态完全隔离; 推理标准库 7/7 子图全部 ship
> **总工期**: 5-8 周 (Sprint 19-22, 含 C9 audit 1 周)

### 5.1 C10 任务: Lazy ModuleState (Sprint 19, 1-2 天)

**实施步骤** (引用 IP-001 §阶段 1 Step 0 + IP-002 §4):
1. `src/modules/scheduler/execution_session.h` 新增 `std::map<std::string, nlohmann::json> module_states_` 成员
2. `src/modules/scheduler/execution_session.cpp` 新增 `ensure_module_state(const std::string& module_path)` lazy init
3. `src/modules/executor/node_executor.cpp` dsl_call 执行时: 检查子图 module_state 声明, 首次调用时 init 空 json
4. 编写 `tests/test_lazy_modulestate.cpp` (counter 例子: 第一次调用 count=1, 第二次 count=2)

**关键代码模式** (引用 IP-001 §阶段 1 Step 0):
```cpp
// execution_session.h
std::map<std::string, nlohmann::json> module_states_;

// execution_session.cpp - Lazy init
void ExecutionSession::ensure_module_state(const std::string& module_path) {
    if (module_states_.find(module_path) == module_states_.end()) {
        module_states_[module_path] = nlohmann::json::object();
    }
}
```

### 5.2 C11 任务: Session Registry (Sprint 20, 2-3 天)

**实施步骤** (引用 IP-001 §阶段 1 Step 1 + IP-002 §6):
1. 新建 `src/modules/scheduler/session_registry.h/cpp` (SessionRegistry 类)
2. `ExecutionSession` 新增 `session_id_` / `session_vars_` 成员
3. `DSLEngine` 持有 `session_registry_` (PIMPL-lite, 避免模块耦合)
4. 注册 4 个 session.* 工具到 `src/common/tools/registry.cpp`
5. 编写 `tests/test_session_registry.cpp`

### 5.3 C12 任务: YIELD / STREAM (Sprint 21-22, 2-3 天)

**实施步骤** (引用 IP-001 §阶段 1 Step 2 + IP-002 §1.5 + §3.2 + §5):
1. `src/core/types/node.h` NodeType 枚举新增 `YIELD`, 新增 `YieldNode` 结构 (yield_value + mode 字段)
2. `src/modules/parser/markdown_parser.cpp` 解析 yield 节点 (mode 字段, 默认 NEXT)
3. `src/modules/executor/node_executor.h/cpp` 新增 `execute_yield()` (渲染 yield_value 模板, 返回 `__yield__` 上下文)
4. `src/modules/scheduler/topo_scheduler.cpp` yield 暂停逻辑 (保存 resume_at, 跳出主循环)
5. `ExecutionSession` 新增 `pending_yield_` 状态 (Sprint 22)
6. 编写 `tests/test_yield_node.cpp` + `examples/phase5_yield_token_generator/`

### 5.4 推理标准库 7 子图 (并行车道)

**实施批次** (引用 IP-001 §三阶段 + IP-002 §7):

| 批次 | 子图 | 依赖 | 估时 | 启动时机 |
|------|------|------|------|---------|
| **2a (立即)** | engine.md | 纯 tool_call | 0.5 天 | C10 启动时并行 |
| **2a (立即)** | model.md | 纯 tool_call | 0.5 天 | C10 启动时并行 |
| **2a (立即)** | session.md | dsl_call 聚合 | 1 天 | C11 启动时并行 (依赖 session 工具) |
| **2b (Step 0 后)** | prefix_cache.md | json scope nesting | 1 天 | C10 ship 后启动 |
| **2b (Step 0 后)** | kv_cache.md | json scope nesting | 1 天 | C10 ship 后启动 |
| **2b (Step 0 后)** | decoding.md | json scope nesting | 1 天 | C10 ship 后启动 |
| **2c (Step 2 后)** | batching.md | queue 管理 | 2 天 | C12 ship 后启动 |

**已创建**: engine.md, model.md, session.md (3/7, 来自 Sprint 19 之前)
**待创建**: prefix_cache.md, kv_cache.md, decoding.md, batching.md (4/7)

---

## 六、阶段 2 任务切分 (C13 远期)

> **目标**: Agent 可编排复杂工作流 (Fork 隔离 + Session 迁移)
> **触发条件**: C10+C11+C12 全链 ship 后, 性能测试显示 deep_copy 是瓶颈 或 Session 迁移/容错需求出现
> **总工期**: 3-5 天 (1 Sprint 装下, 延后启动)

### 6.1 Step 3: Fork per-field behavior (2-3 天)

**实施步骤** (引用 IP-001 §阶段 2 Step 3 + IP-002 §1.4 + §3.1):
1. `src/core/types/node.h` ForkNode 新增 `fork_behaviors: map<string, ForkBehavior>` 字段
2. `src/modules/parser/markdown_parser.cpp` 解析 fork_behaviors 字段
3. `src/modules/scheduler/topo_scheduler.cpp` start_fork_simulation 处理 fork_behaviors (DEEP_COPY/COW/INHERIT/SHARE_READONLY)
4. `src/core/types/context.h` 新增 CowState 辅助类 (RAII 包装, 写时复制)
5. 编写 `tests/test_fork_perfield.cpp` (Fork 2 分支, 分支 A 修改不影响分支 B)

### 6.2 Step 4: Checkpoint / Restore (1-2 天)

**实施步骤** (引用 IP-001 §阶段 2 Step 4 + IP-002 §6):
1. `src/modules/scheduler/session_registry.h/cpp` 新增 `checkpoint(id)` / `restore(id, json)` 方法
2. 注册 `session.checkpoint` / `session.restore` 工具
3. 编写 `tests/test_session_checkpoint.cpp` (checkpoint → destroy → restore → 验证状态恢复)

### 6.3 触发条件检查清单

C13 启动前必须满足:
- [ ] 性能测试显示 deep_copy 在 Fork 多分支场景下成为瓶颈 (Step 3 触发)
- [ ] Session 迁移或容错需求出现 (Step 4 触发)
- [ ] 团队有 3-5 天时间投入

如未触发, 跳过 C13, 直接进入 C14 Stage 3。

---

## 七、阶段 3 任务切分 (C14 远期)

> **目标**: 提供推理 API 服务, Agent 能够自主发现新的优化策略
> **触发条件**: Stage 1 全链 ship + Stage 2 稳定运行 + Oracle 验证自进化可行性
> **总工期**: 4-6 周 (2-3 Sprint)
> **关联**: BOOT-001 任务 2.1-3.1 + IP-001 §阶段 3

### 7.1 Step 5: 静态分析优化 (1-2 天, IP-001 §阶段 2 Step 5)

**实施步骤** (引用 IP-001 §阶段 2 Step 5 + IP-002 §6):
1. `src/modules/library/library_loader.h/cpp` 新增 `build_reachability_graph()` (从 .agent.md 解析调用关系)
2. `src/modules/scheduler/execution_session.cpp` 解析时预初始化可达模块
3. 编写 `tests/test_static_analysis.cpp` (覆盖率 > 90%, 预热后首次调用延迟 < 未预热 10%)

### 7.2 BOOT-001 任务 2.1: QualityFeedbackController (1 周)

**实施步骤** (引用 self-bootstrapping-path.md §阶段 2 任务 2.1):
1. 新建 `src/modules/budget/quality_feedback_controller.h/cpp` (QualityFeedbackController 类)
2. 记录 `FeedbackRecord { task_id, backend, QualityMetrics, strategy, context }`
3. 提供 `record_feedback()` / `get_average_quality()` / `generate_suggestions()` API
4. 编写 `tests/test_quality_feedback.cpp`

### 7.3 BOOT-001 任务 2.2: 自适应优化循环 (1-2 周)

**实施步骤** (引用 self-bootstrapping-path.md §阶段 2 任务 2.2):
1. 新建 `examples/adaptive_optimize.agent.md` (自适应优化工作流, 100 轮内收敛)
2. 编写 `tests/test_adaptive_optimize.cpp` (验证收敛性 + 质量提升可量化)
3. 集成到 TUI: 实时显示优化进度

### 7.4 BOOT-001 任务 3.1: InferenceServer 服务化 (1-2 周)

**实施步骤** (引用 self-bootstrapping-path.md §阶段 3 任务 3.1):
1. 新建 `src/api/inference_server.h/cpp` (InferenceServer 类, MCP + OpenAI 兼容)
2. `api/chat_completions.agent.md` (OpenAI 兼容接口子图)
3. MCP 接口实现 (符合协议规范)
4. 服务分层实现 (高质量 → 云端, 低质量 → 本地)
5. 编写 `tests/test_inference_server.cpp` (MCP 协议 + OpenAI 兼容 + 服务分层 E2E)

### 7.5 推理标准库 7 子图补齐 (并行车道, 1 周)

**待创建** (C10-C12 实施期间完成大部分, C14 阶段补齐):
- prefix_cache.md / kv_cache.md / decoding.md (Step 0 完成后启动)
- batching.md (Step 2 完成后启动)

**验证**: 7/7 子图通过 ctest, 推理标准库覆盖率 100%

### 7.6 触发条件检查清单

C14 启动前必须满足:
- [ ] C10+C11+C12 全链 ship + 1 个月稳定运行
- [ ] 推理标准库 7/7 子图全部 ship
- [ ] C13 决定是否实施 (性能/运维需求)
- [ ] Oracle 验证自进化可行性 (避免"AI 写的代码 AI 看不懂"陷阱)
- [ ] 团队有 4-6 周时间投入

---

## 八、Phase 5 验证标准

### 8.1 阶段 1 验证 (C10-C12 + 推理标准库)

- [ ] 1 个推理 Session 可完整创建/配置/运行/销毁
- [ ] 多 Session 之间状态完全隔离 (2 Session E2E 测试通过)
- [ ] 推理标准库 7/7 子图全部 ship (engine/model/session + prefix_cache/kv_cache/decoding/batching)
- [ ] YIELD 节点 token-by-token 生成器工作正常 (N 次调用返回 N 个 token)
- [ ] yield 之间 module_state 保持 (C10 基础)
- [ ] Session 工具 4 个注册成功 (create/destroy/set_var/get_var)
- [ ] ctest ≥ 64/64 PASS, ASan/TSan 0 警告

### 8.2 阶段 2 验证 (C13)

- [ ] Fork per-field COW 行为 E2E 测试通过 (读零开销, 写才复制)
- [ ] Checkpoint/Restore 状态可序列化 (迁移后状态完全恢复)
- [ ] ctest ≥ 67/67 PASS

### 8.3 阶段 3 验证 (C14)

- [ ] 静态分析覆盖率 > 90%, 预热后首次调用延迟 < 未预热 10%
- [ ] QualityFeedbackController 自适应优化 100 轮内收敛
- [ ] InferenceServer MCP 协议测试通过
- [ ] OpenAI 兼容接口通过标准测试 (chat completions + embeddings)
- [ ] 服务分层: 高质量任务 → 云端, 低质量任务 → 本地 llama.cpp
- [ ] 完全自举: Agent 自主发现新优化策略, 性能优于基线
- [ ] ctest ≥ 80/80 PASS, ASan/TSan 0 警告

### 8.4 跨阶段不变式

- [ ] `python3 tools/adr_lint.py docs/adr/` exit 0 (每个 Sprint 收官)
- [ ] `python3 tools/docs_drift_audit.py` 0 critical drift
- [ ] `python3 tools/check_roadmap_drift.py` 0 CRITICAL
- [ ] `grep -cE '^\s*#include\s+"(modules/|common/)' src/core/engine.cpp` ≤ 1 (ADR-0019 §1.4 维持)
- [ ] 每个 Sprint 收官前 4 个 Review Gate 全跑 (§九)

---

## 九、Review Gates 调度 (5 种类型)

> **目的**: 防止 Master plan "write-once, never revise" 的常见陷阱。详细定义参考 `2026-06-26-sprint-11-to-18-roadmap.md` §九。

### 9.1 Sprint Review Gate (沿用, 强制执行)

| 字段 | 值 |
|------|---|
| **触发时机** | 每个 Sprint 收官时 (C10/C11/C12 ship 后立即执行) |
| **检查内容** | 1) 已 ship change 是否达到预期效果; 2) 是否引入新 bug; 3) 是否暴露下一 change 的假设错误; 4) 估时偏差 > 30% |
| **输出** | 追加到 §十 Drift Log |
| **跳过条件** | 仅当 change 100% 完美 ship 且后续 change 完全独立时 |

### 9.2 Architecture Drift Gate (沿用, 强制执行)

| 字段 | 值 |
|------|---|
| **触发时机** | 每 2-3 个 Sprint 收官时 (Sprint 21 / Sprint 24) |
| **检查内容** | 1) ADR 状态是否需要修正; 2) 是否有未文档化的架构决策; 3) `docs/adr/` 与 `docs/archive/adr/` 一致性 |
| **输出** | ADR 修正 change (类似 C9) |

### 9.3 Dependency Refresh Gate (沿用, 强制执行)

| 字段 | 值 |
|------|---|
| **触发时机** | 每个占位 change 启动 Sprint 之前 (C11/C12/C13/C14 启动前) |
| **检查内容** | 1) 依赖的 change 是否 ship; 2) 接口/行为是否与占位假设一致; 3) 估时偏差 > 30% |
| **输出** | 占位 change 内容调整 → 追加到 §十一 Adjustment Log |

### 9.4 Strategic Alignment Gate (沿用, 季度触发)

| 字段 | 值 |
|------|---|
| **触发时机** | 季度/重大 milestone 后 (Phase 5 全部 ship) |
| **检查内容** | 1) 当前 backlog 是否仍服务项目核心目标 (Phase 6?); 2) 是否出现新 ADR 改变方向; 3) Phase 6 启动条件 |
| **输出** | 新一轮 Master plan (Phase 6) |

### 9.5 🆕 Phase 5 Stage Gate (新增, 阶段切换强制)

| 字段 | 值 |
|------|---|
| **触发时机** | Stage 1 → Stage 2 切换时 (C12 ship 后); Stage 2 → Stage 3 切换时 (C13 ship 后) |
| **检查内容** | 1) Stage 1 全部 ship + 稳定运行 2 周; 2) 下一 Stage 触发条件满足; 3) 团队时间投入可用 |
| **输出** | 阶段切换决议追加到 §十二 Pivots Log; 推迟/启动记录在 §十一 |
| **跳过条件** | 仅当阶段目标完全达成时 |

### 9.6 Review Gates 调度表 (Sprint-by-Sprint)

| Sprint | 预计收官日期 | 🔄 Sprint Review | 🧭 Drift Gate | 🔗 Dependency Refresh | 🎯 Strategic Alignment | 🆕 Stage Gate |
|--------|------------|----------------|----------------|---------------------|---------------------|-------------|
| **Sprint 19** | 2026-07-17 | ✅ C10 ship (C9 收官后立即) | — | C11 启动 | — | — |
| **Sprint 20** | 2026-07-31 | ✅ C11 ship | ✅ 累计 3 changes | C12 启动 | — | — |
| **Sprint 21-22** | 2026-08-21 | ✅ C12 ship | — | C13 启动评估 | — | ✅ Stage 1→2 评估 |
| **Sprint 23-24** | 2026-09-18 | ✅ C13 ship (如启动) | ✅ 累计 5 changes | C14 启动评估 | — | ✅ Stage 2→3 评估 |
| **Sprint 25+** | 2026-10-31 | ✅ C14 ship | ✅ 累计 6 changes | — | ✅ Phase 5 收官 | ✅ Phase 5→6 |

**总工期**: ~4 个月 (2026-07-03 ~ 2026-10-31)

---

## 十、Architecture Drift Log

> **追踪 Phase 5 期间 ADR 偏离事件**。Drift Gate 触发时新增; Sprint Review Gate 发现 ADR 偏离时也新增。

### 10.1 历史 Drift 事件 (来自上游 plan)

详见 `2026-06-26-sprint-11-to-18-roadmap.md` §十 (本 plan 启动前的 3 条历史 drift 记录)。

### 10.2 Phase 5 期间 Drift 事件 (待填充)

| 日期 | Change | 偏离类型 | 偏离描述 | 响应 Change | 状态 |
|------|--------|---------|---------|-----------|------|

### 10.3 待填充模板

```markdown
| <YYYY-MM-DD> | <change 名> | <fix/retro/redirect> | <具体偏离描述> | <响应 change 名> | <状态> |
```

---

## 十一、Change Adjustment Log

> **追踪 Phase 5 期间占位 change 的内容调整**。当 Dependency Refresh Gate 或 Stage Gate 发现占位假设错误时, 记录调整内容。

### 11.1 历史调整 (来自上游 plan)

详见 `2026-06-26-sprint-11-to-18-roadmap.md` §十一 (本 plan 启动前的 5 条历史调整记录)。

### 11.2 Phase 5 期间调整 (待填充)

| 日期 | 占位 Change | 调整原因 | 调整内容 | 状态 |
|------|------------|---------|---------|------|

### 11.3 预期可能的调整 (基于当前占位假设)

| 占位 Change | 当前假设 | 潜在调整风险 | 触发条件 |
|------------|---------|------------|---------|
| **C10** phase5-stage1-step0 | "json scope nesting 无 schema 校验" | 可能发现需要 schema 校验才能支撑推理标准库 prefix_cache 复杂状态 | C10 启动前 Oracle 咨询 |
| **C11** phase5-stage1-step1 | "SessionRegistry 成员模式 (与 ToolRegistry 一致)" | 可能发现 SessionRegistry 需要 P0 业务并行场景, 必须改单例 | C11 启动前 (Sprint 19 末) |
| **C12** phase5-stage1-step2 | "单 YIELD 节点 + mode 参数" | 可能发现 CONTINUE_STREAM/STOP_STREAM 状态机需要更细粒度 | C12 启动前 (Sprint 20 末) |
| **C13** phase5-stage2 | "性能/运维需求触发" | 性能测试可能显示 deep_copy 不是瓶颈 (5+ 个分支场景下), Step 3 永远不启动 | C13 评估 (Sprint 22) |
| **C14** phase5-stage3 | "完全自举 + 服务化" | Oracle 可能发现自进化风险过大 (VN-001 §风险), 拆分为 C14 (服务化) + C15 (自进化延后) | C14 启动前 (Sprint 24) |

---

## 十二、Strategic Pivots Log

> **追踪 Phase 5 期间重大战略转向**。Strategic Alignment Gate 或 Stage Gate 触发时新增。

### 12.1 历史 Pivot (来自上游 plan)

详见 `2026-06-26-sprint-11-to-18-roadmap.md` §十二.1 (本 plan 启动前的 1 条历史 pivot: Phase 4.5 → Phase 5 过渡加 1 个 audit 中间阶段)。

### 12.2 Phase 5 期间 Pivot (待填充)

| 日期 | 原方向 | 新方向 | 影响 Changes | 决策依据 |
|------|--------|--------|------------|---------|

### 12.3 待填充模板

```markdown
| <YYYY-MM-DD> | <原 phase/目标> | <新 phase/目标> | <哪些占位 changes 调整/取消/新增> | <决策依据 (Oracle 咨询/团队决策/业务需求)> |
```

---

## 十三、3 种响应 Change 类型

> **直接复制自 `2026-06-26-sprint-11-to-18-roadmap.md` §十三**, 本 plan 沿用相同规范。

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

## 十四、维护规则

1. **状态变更**: 每个 change 进入 archive 时, 本文件相应行的 "状态" 列更新为 `✅ archived (YYYY-MM-DD)`, 并补充 `archive 链接`
2. **新增 change**: 如需新增未列出的 change, 在本文件末尾追加新行, 并标注依赖关系
3. **依赖变更**: 如发现 change 间新依赖, 立即更新 §二 依赖图
4. **同步检查**: 每个 Sprint 收官时, 检查本文件与 `docs/roadmap-status.md` 的一致性
5. **删除规则**: 本文件永不删除, 仅追加与状态更新
6. **Review Gates 强制执行**: 每个 Sprint 收官前, 必须执行 §九 中对应的 Review Gates, 不得跳过
7. **§十/§十一/§十二 append-only**: 这 3 个 log 表只追加不删除, 保持完整历史
8. **占位 change 调整必须留痕**: 任何占位 change 的 proposal/design/tasks/spec 修改, 必须在 §十一 记录一行
9. **上游 plan 引用**: §十/§十一/§十二 历史条目引用 `2026-06-26-sprint-11-to-18-roadmap.md` 对应章节, 避免重复

---

## 十五、参考链接

- 上游 master plan: [`2026-06-26-sprint-11-to-18-roadmap.md`](2026-06-26-sprint-11-to-18-roadmap.md) (C0-C8 全部 ship + archived)
- 上游 audit plan: [`2026-06-30-audit-remediation-roadmap.md`](2026-06-30-audit-remediation-roadmap.md) (A/B/C 3 change, 部分 ship)
- C7 实施 plan: [`2026-07-02-c7-phase2-model-router-plugin.md`](2026-07-02-c7-phase2-model-router-plugin.md) (C7 完整 ship)
- C9 audit change: [`openspec/changes/2026-07-03-phase4-5-impl-scope-audit/`](../../openspec/changes/2026-07-03-phase4-5-impl-scope-audit/) (Agent 1 工作中)
- **Phase 5 详细设计**:
  - [BOOT-001 自举实施路径](../../proposals/implementation/self-bootstrapping-path.md) — 4 阶段 13 任务
  - [IP-001 增量实现路线图](../../proposals/implementation-roadmap/01-roadmap.md) — 6 步计划
  - [IP-002 代码扩展点映射](../../proposals/implementation-roadmap/02-code-mapping.md) — 每步的精确代码改动位置
  - [VN-001 自举愿景](../../proposals/vision/01-self-bootstrapping-vision.md) — 顶层抽象
  - [VN-002 语言演进路线图](../../proposals/vision/02-language-evolution-roadmap.md) — 阶段 A/B/C
- 静态蓝图: `docs/implementation-roadmap.md`
- 动态看板: `docs/roadmap-status.md`
- OpenSpec CLI: `openspec list` / `openspec show <change>` / `openspec validate <change>`
- AGENTS.md: 项目根入口文档

---

**最后更新**: 2026-07-03 (初始创建, Strategic Alignment Gate §9.4 触发)
**下次更新**: C9 ship 后, C10 启动时填写 proposal/design/spec/tasks
**责任人**: Sisyphus (master plan 创建) → 用户 (后续维护)
