# Design: Phase 4.5 — MVP Cleanup

> **关联 proposal**: `openspec/changes/2026-06-26-phase-4-5-mvp-cleanup/proposal.md`
> **最后更新**: 2026-07-03

## Decision 0: 架构演化 — SimpleCognitiveOrchestrator 从 MVP 到稳定内部组件

### 背景

原始 Phase 4.5 plan 要求"替换 SimpleCognitiveOrchestrator 为正式实现"。
但 7+ Sprint 后的实际架构演化表明，SimpleCognitiveOrchestrator 已从 MVP 占位符演化为稳定内部组件：

```
Phase 0 (MVP)                   Phase 1+2 (当前)
┌──────────────────┐          ┌────────────────────────────┐
│ SimpleCognitive   │          │ PDK Agent Layer             │
│ Orchestrator       │   →     │  ├─ ReactLoop ─── 委托 ──┐ │
│ (单轮 ReAct)       │          │  ├─ PlanExecuteLoop      │ │
│                    │          │  └─ ForkJoinLoop         │ │
└──────────────────┘          │                           ▼ │
                              │ CognitiveWorker ─ 委托 ──┐ │
                              │           (per-agent)    │ │
                              │                          ▼ │
                              │      SimpleCognitive       │
                              │      Orchestrator          │
                              │      (@internal stable)    │
                              └────────────────────────────┘
```

**当前依赖链**:
```
agent_macros.h (DEFINE_AGENT React)
  → LoopDispatcher<React> → ReactLoop
    → SimpleCognitiveOrchestrator::react_once()

CognitiveWorker::submit_task()
  → SimpleCognitiveOrchestrator::process()

test_simple_orchestrator.cpp (5 tests)
slice_01_tool_call/main.cpp (端到端示例)
```

### 选项对比

| 选项 | 描述 | 工作量和风险 |
|------|------|:---:|
| **A: 重写替换** | 删除 SimpleCognitiveOrchestrator, 重写 CognitiveWorker + ReactLoop 核心循环 | 🔴 3+ 模块改动, 5 测试重写, 高回归风险 (2-3 天) |
| **B: @internal 标记** | 保留实现, 标记为内部 stable component, 清理遗留注释 | 🟢 仅注释+文档 (0.5h) |
| **C: 头文件迁移** | 将 `.h` 从 `include/agenticdsl/cognitive/` 移到 `src/modules/cognitive/` | 🟡 PDK ReactLoop 需修改 include 路径 (1h) |

### 决议: Option B — @internal 标记

- PDK ReactLoop (`include/agenticdsl/pdk/agent_loops/react_loop.h`) 需要 include SimpleCognitiveOrchestrator
- 移动头文件会破坏 PDK 的 public API 契约（PDK 承诺 `include/agenticdsl/pdk/` 自包含）
- SimpleCognitiveOrchestrator 在 52/52 测试中零回归, ASan/TSan 100% clean — 证明其稳定性

**实施**: 仅修改文件头注释 + 清理 TODO(mvp), 零运行时变更。

---

## Decision 1: MockLLMProvider — 保持现状

### 背景

原始 placeholder proposal 提到 "降级为 CI-only fixture"。

### 决议: 不做任何修改

- MockLLMProvider 是 `DSLEngine::from_markdown` 的默认 provider
- 所有 52 测试 + 6 个 C++ 示例依赖它
- 降级为 CI-only fixture 需要额外工厂抽象 (估时 1-2 天), 零价值

---

## Decision 2: examples/ 目录梳理 — 全部保留

### 评估结果

| 目录 | 代码 | 构建 | 用途 | 决策 |
|------|:---:|:---:|------|:---:|
| `agent_basic/` | C++ (.md DSL) | ✅ CMake | 主示例 | 保留 |
| `agent_simple/` | C++ | ✅ (Sprint 19) | MockLLM 单轮 | 保留 |
| `agent_loop/` | C++ | ✅ (Sprint 19) | MockLLM 多轮 | 保留 |
| `slice_01_tool_call/` | C++ | ✅ | 端到端 | 保留 |
| `phase1_model_router_plugin/` | C++ | ✅ | C7 Model Router (4 .so) | 保留 |
| `phase1_plugin_demo/` | C++ | ✅ | PluginLoader 验证 (3 modes) | 保留 |
| `skill_porting/` | `.md` 文档 | N/A | Skill taxonomy 对照 | 保留 (参考文档) |
| `superpowers/` | `.agent.md` DSL | N/A | 12 workflow 对标 | 保留 (对标参考) |

### 决议: 0 删除, 0 合并

- 6 个 C++ 示例全部可编译运行，通过 `-DAGENTICDSL_BUILD_EXAMPLES=ON` 控制
- `skill_porting/` 和 `superpowers/` 是 `.md` 文档, 非构建目标
  - `skill_porting/`: Skill 分类体系 + AgenticDSL 实现对照
  - `superpowers/`: Superpowers 技能的 AgenticDSL 重写对标
- 新建 `examples/README.md` 统一说明 8 个 entry 的用途

---

## Decision 3: TODO(mvp) 清理规则

### 扫描结果

仅 **2 处** 源代码中有 `TODO(mvp)` (不含 C8 自身 placeholder 文件):

| 文件 | 行 | 内容 | 处理 |
|------|:---:|------|------|
| `include/agenticdsl/cognitive/simple_orchestrator.h` | 5 | "MVP 阶段：仅单轮，标 TODO(mvp)；多轮 + 状态机留待后续 Phase 1" | 替换为 "@internal: 多轮 ReAct 由 CognitiveWorker + ReactLoop 在上层实现" |
| `src/modules/cognitive/simple_orchestrator.cpp` | 109 | "TODO(mvp): 多轮 + 真实 prompt 模板留待 Phase 1" | 替换为 "@internal: 多轮循环由 CognitiveWorker 在上层管理；prompt 模板已迁移至 llm_config.json" |

### 决议

- 移除 2 处 `TODO(mvp)`, 替换为准确的当前状态描述
- 不添加新注释 — 最小改动

---

## Decision 4: 文档更新范围

| 文件 | 变更 | 原因 |
|------|------|------|
| `docs/roadmap-status.md` | Phase 4 → 100%, Phase 4.5 → 100% | 进度同步 |
| `AGENTS.md` | § Recent Changes 追加 Phase 4.5 ship 记录 | 交付记录 |
| `examples/README.md` | 新建: 8 个 entry 的用途说明 | 目录梳理产出 |

**不更新**:
- `docs/specs/layer0.md` — 未引用 SimpleCognitiveOrchestrator
- `docs/implementation-roadmap.md` — 状态通过 `roadmap-status.md` 同步
- `docs/adr/*.md` — SimpleCognitiveOrchestrator 是内部实现, 非 ADR 决策范围

---

## Decision 5: 兼容性策略

### 零 breaking change

| 检查项 | 状态 |
|--------|:---:|
| 公开 API 变更 | ❌ 无 (仅注释) |
| 头文件位置变更 | ❌ 无 |
| 测试预期变更 | ❌ 无 |
| 构建系统变更 | ❌ 无 |
| 运行时行为变更 | ❌ 无 |

### 验证策略

```bash
# 修改后必跑
ctest --output-on-failure              # ≥52/52 PASS
python3 tools/adr_lint.py docs/adr/    # exit 0 (已知 1 pre-existing 错误)
python3 tools/docs_drift_audit.py      # 0 critical drift
openspec validate 2026-06-26-phase-4-5-mvp-cleanup  # exit 0
```