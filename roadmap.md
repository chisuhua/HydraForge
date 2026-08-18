# 项目路线图

> **驱动计划**: [`docs/superpowers/plans/2026-07-24-sprint-24-25-demo-driven-plan.md`](docs/superpowers/plans/2026-07-24-sprint-24-25-demo-driven-plan.md)
> **核心原则**: Demo 驱动 > 架构文档驱动 — 每个 Demo 是独立可交付物，反馈循环快，适合 Solo Dev
> **Phase**: 6 — Agent-as-Plugin (2026-07-15 ~ 至今, Phase 5 ✅ 收官)
> **路线图 v3** (2026-08-03 重写): 整合 **三平面架构** (Execution/Control/Data) + 6 个新 LLM-native ADR (0072/0074/0075/0076/0077/0078) + Oracle 容量审查 (session `ses_037e12115ffeLkeR1QTIko0BHb`) + active-status.md §四 Candidate B 结构性暂缓

## 元信息
- **版本**: 3 (2026-08-03 重写, 三平面架构)
- **创建时间**: 2026-07-24T00:00:00+08:00
- **最后更新**: 2026-08-17 — roadmap-state-phase6b-sync ship; Phase 6b/7 状态校准 (per Oracle audit)
- **当前阶段**: phase-6b (2026-08-11 ~ 2026-08-19, 44h 容量) — 🔄 partial ship + carry-over (21/21 changes completed, 3/11 gates satisfied, 8 gates remain)
- **下一阶段**: phase-6c (2026-08-19 ~ 09-09, ~80h 容量)
- **阶段规划**:
  - **Phase 6c** (2026-08-19 ~ 09-09, ~80h) — **Execution Plane 完整 ship**
  - **Phase 7** (2026-09+ 起, gated) — **Control Plane (MCP)**
  - **Phase 8a** (Phase 8 启动评估) — **Data Plane (gRPC)**
  - **Phase 8b** (条件触发) — **Execution Plane 支撑 (Fine-tune)**
- **治理节奏**: 2026-08-11 Sprint 25 kickoff → 2026-08-19 Sprint 25 收官 + Drift Gate → Phase 6c 启动评估 → 2026-09-09 Phase 6c 收官 + Evidence Gate 决议 + Control Plane 启动评估

---

## 🎯 三平面架构 (Three-Plane Architecture)

> **核心战略框架**: LLM-native AgenticDSL 采用三平面架构, 各平面独立演进 + 顺序依赖

### 1. 架构定位与职责分工

```
┌─────────────────────────────────────────────────────────────────────┐
│                     Three-Plane Architecture                          │
│                                                                       │
│  ┌─────────────────────────────────────┐  ┌─────────────────────────┐ │
│  │     📡 MCP 控制面 (Control Plane)   │  │  📊 gRPC 数据面          │ │
│  │     (ADR-0076)                       │  │  (Data Plane)            │ │
│  │     Phase 7 (gated)                  │  │  (ADR-0077)              │ │
│  │                                       │  │  Phase 8a (gated)        │ │
│  │  • Capability 暴露                    │  │                         │ │
│  │    (tools/prompts/resources)         │  │  • High-throughput 流式  │ │
│  │  • 鉴权 (静态 token MVP)              │  │    (LLM token stream)    │ │
│  │  • 元数据 + 配置                       │  │  • 大 payload 传输       │ │
│  │  • 路由决策 (payload < 64KB)         │  │    (模型权重 / 数据集)   │ │
│  │  • Stateless 模式                     │  │  • RemoteExecutor        │ │
│  │                                       │  │  • Telemetry (OTel)      │ │
│  │  协议: JSON-RPC 2.0 / MCP 2025-11-25 │  │                         │ │
│  │  Transport: stdio + HTTP+SSE         │  │  协议: gRPC + protobuf   │ │
│  └─────────────────────────────────────┘  │  Transport: HTTP/2       │ │
│           ↑                                  ↑                       │
│           │ 调用 capability                    │ 流式通道              │
│           │                                    │                       │
│  ┌────────┴────────────────────────────────────┴──────────────────┐ │
│  │           🎯 AgenticDSL 执行面 (Execution Plane)                  │ │
│  │           (ADR-0071 顶层 + 0072/0073/0074/0075/0078 派生)        │ │
│  │           Phase 6b/6c (基础)                                     │ │
│  │                                                                    │ │
│  │  • LLM 输出原生语言 (AgenticDSL markdown)                          │ │
│  │  • DSL runtime 解析 → ToolCoordinator 安全检查 → EnvBackend 执行    │ │
│  │  • EnvBackend 吸收平台差异 (local/docker/k8s/ssh/mcp/grpc)        │ │
│  │  • Schema 校验 (ADR-0073 JSON Schema 2020-12)                       │ │
│  │  • Prompt Engineering + Evidence Gate (ADR-0074)                     │ │
│  │  • Fine-tune 反馈 (ADR-0078, Phase 8b)                              │ │
│  └────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────┘
```

### 2. AgenticDSL 替代 CLI 作为 LLM 原生输出语言 (核心战略)

**❌ Before (传统 CLI 范式)**:
```
LLM → bash/shell 命令 → 环境工具
```
局限: 平台耦合 / 注入风险 / 跨平台不一致 / 无统一审计

**✅ After (LLM-native 范式)**:
```
LLM → AgenticDSL markdown → DSL runtime → EnvBackend (local/docker/k8s/ssh/mcp/grpc)
```
优势:
- LLM 输出**结构化动作** (parse 验证, 避免 bash 注入)
- EnvBackend 吸收平台差异, LLM 不感知具体 backend
- ToolCoordinator 层统一安全 + 审批 + 审计 (per ADR-0031)
- MCP 作为外部 tool 调用接口 (LLM ↔ 外部服务)
- 训练数据收集 (JSONL, ADR-0074 D6) 闭环优化

**量化收益目标** (per ADR-0074 D4 Evidence Gate):
- parse-valid ≥85%
- task-success L1 ≥70% / L2 ≥50% / L3 ≥30%
- Prompt prefix tokens ≤8k (两阶段注入)
- 失败事件自动 retry (≤3 次 + model fallback)

### 3. MCP/gRPC 路由规则 (ADR-0077 D2)

| 条件 | 路由 | 理由 |
|------|------|------|
| `payload < 64KB && !streaming` | **MCP (控制面)** | 小 payload + 静态 token 鉴权 |
| `payload >= 64KB \|\| streaming` | **gRPC (数据面)** | 大 payload + 流式性能优势 |
| 强制 gRPC service | LLMDataPlane / BlobTransfer / RemoteExecutor / Telemetry | 设计如此 |
| gRPC 不可用 | fallback MCP + warn event | (degrade gracefully) |

### 4. MCP Stateless 设计原则 (ADR-0076 D8)

- 每个 MCP request 完全独立, 无 server-side session state (除 stream context 外)
- 静态 token per-request (ADR-0076 D2 已 ship)
- 工具调用不依赖之前调用历史 (LayeredContext per-request)
- **Horizontal scaling 优势**: 无状态 = 多实例 + 故障恢复 + A/B 测试

### 5. 演进依赖链

```
Phase 6b/6c ──Execution Plane 基础 ──┐
                                      ↓
Phase 7 ────────Control Plane (MCP) ──┤  (gated by Active status §四)
                                      ↓
Phase 8a ───────Data Plane (gRPC) ────┤  (gated by Control Plane ship ≥3 月)
                                      ↓
Phase 8b ───────Fine-tune ────────────┘  (gated by AgenticMind ship)
```

**核心原则**: Execution Plane 必须先 ship, Control Plane 与 Data Plane 都依赖它. Data Plane 进一步依赖 Control Plane 稳定运行.

---

## 阶段定义

### Phase 6: Sprint 24 — Demo 收尾与 TDK 骨架 (phase-6a)

**目标**: pdk_chat_demo v1 收尾 + pkm_temporal_demo PDK 骨架落地
**状态**: ✅ 完成 (2026-08-11 收官, 拖期 6d; T1-T8 全 ship)
**周期**: 2026-07-24 ~ 2026-08-05 (12 天, ~37h 容量)
**完成条件**:
  - [x] `ctest -R pdk_chat` 全绿 (含新增 schema 校验 test case)
  - [x] `ctest -R temporal` 全绿 (≥8 test cases)
  - [x] `./pdk_chat_demo --mock` Session 持久化 + Budget 告警正常工作
  - [x] `./pkm_temporal_demo --mock` 4 个演示场景全部 PASS
  - [x] **PDK SafeExec jthread 重写 + 8 test cases PASS** (Phase 6a 任务 2, OpenSpec `2026-08-10-pdk-safe-exec-tests` archived 2026-08-10)
  - [x] **PDK Doxygen 覆盖率 ≥90% + pdk/README.md 3 章节扩展** (Phase 6a 任务 2)
  - [x] **proposals/ 清理完成** (T6 — 重组为 `improvements/` + `proposal-suggestions.md`/`proposal-approved.md`; 24 entries, 0 条 >3 月)
  - [x] **pkm_temporal_demo CI 集成** (T8 — root CMake `add_subdirectory` 接入; ctest 通过 auto-discovery 自动覆盖; DESIGN.md + README.md 完整)
  - [x] active-status.md 更新至 2026-08-10

#### 任务分类 (现状不变, 无 LLM-native 工作)

| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|:------:|-------------|
| `demo-chat-v1` | pdk_chat_demo v1 收尾 | Session/Budget 验证修复 + Schema 校验 | P0 | Session持久化；Budget告警；Schema校验基础 |
| `demo-temporal-1a` | pkm_temporal_demo PDK 骨架 | ITemporalClient + Mock + CLI + pdk_entry (5 工具) | P0 | ITemporalClient接口；MockBackend；CLI入口 |
| `demo-temporal-1b` | pkm_temporal_demo 项目 | Demo 项目 + 4 场景 + 测试 | P0 | 4场景演示；测试覆盖 |
| `demo-temporal-1c` | pkm_temporal_demo CI 集成 | 根 CMake 更新 + docs + CI hook | P1 | CI集成；根CMake更新 |
| `governance` | 治理节奏 | proposals/ 清理 + active-status.md 同步 | P1 | proposals清理；active-status同步 |

#### 任务明细

| ID | 任务 | 分类 | 估时 | 优先级 | 依赖 |
|:---|------|------|:----:|:------:|------|
| T1 | pdk_chat_demo: Session 持久化验证 + Budget 告警修复 | `demo-chat-v1` | 4h | P0 | — |
| T2 | pdk_chat_demo: Schema 校验基础版 | `demo-chat-v1` | 4h | P0 | — |
| T3 | pkm_temporal_demo: PDK 骨架 | `demo-temporal-1a` | 10h | P0 | — |
| T4 | pkm_temporal_demo: Demo 项目 | `demo-temporal-1b` | 8h | P0 | T3 |
| T5 | pkm_temporal_demo: 测试 (≥8 cases) | `demo-temporal-1b` | 6h | P0 | T3, T4 |
| T6 | proposals/ 清理: 归档 >3 月的 proposal 目录 | `governance` | 0.5h | P1 | — |
| T7 | active-status.md 同步 | `governance` | 0.5h | P1 | T1-T5 |
| T8 | pkm_temporal_demo: CI 集成 + 根 CMake 更新 + docs | `demo-temporal-1c` | 3h | P1 | T5 |

> **合计**: ~36h, T8 可顺延至 Sprint 25 首日

> **Phase 6a 收官注记 (2026-08-11)**: 全部 T1-T8 已 ship, 完成条件 8/8 全绿. 详见 `openspec/changes/archive/` 内 6 个 Phase 6a 相关 archive 目录. 下一步: 启动 Phase 6b (Sprint 25, Execution Plane 基础 ship).

---

### Phase 7: Sprint 25 — Demo 扩展 + Execution Plane 基础 (phase-6b)

**目标**: pdk_chat_demo v2 启动 + **Execution Plane 基础 ship (Wave 2 强制决策 24-32h)**
**状态**: 🔄 partial ship + carry-over (2026-08-11 kickoff；21/21 已登记 changes 完成；3/11 gates 满足)
**前置阶段**: phase-6a ✅
**周期**: 2026-08-11 ~ 2026-08-19（实际 kickoff；原计划 08-05，剩余工作 carry-over 至 Phase 6c）
**已 ship**: U1（PlanExecute/ForkJoin）、W1（ADR-0073 翻牌）、W6（ADR-0068 Appendix A）、表外治理/债务修复 changes。
**部分完成**: U5 对应的 demo-level YAML DSL structured parsing 已 ship（`from-roadmap-phase-6b-platform`），但 `.agent.md` 核心 schema gate 仍未满足。
**未 ship / carry-over**: U2、U3、U4、U6、W2、W3、W4、W5；这些任务无法在 08-19 前完成，转入 Phase 6c 前置队列。
**完成条件**:
  - [x] `examples/pdk_chat_demo` 支持 3 种 Agent Loop (React / PlanExecute / ForkJoin；U1 archived 2026-08-14)
  - [ ] Code Review SKILL.md 通过 SkillInterpreter 隔离执行 (U2)
  - [ ] `include/agenticdsl/pdk/README.md` 完成 (U3)
  - [ ] AgentForge 第 2 个领域 agent 可独立运行 (U4)
  - [ ] `.agent.md` 加载时有 schema 校验 (U5 核心 gate；demo-level parser 已部分 ship)
  - [x] **🎯 Execution Plane: ADR-0073 翻牌 🟡 Partial** (W1 archived 2026-08-13)
  - [ ] **🎯 ADR-0074 D3 baseline 第一次测量** (W2 carry-over；3 模型 × 50 tasks)
  - [ ] **🎯 ADR-0072 D1+D4 强制决策 ship** (W4/W5 carry-over；`stream: true` + `backend:`)
  - [x] **🎯 ADR-0068 §附录 A amendment PR** (W6 archived 2026-08-13；14 候选主题注册)
  - [ ] ADR-0042 状态不匹配已解决 (U6)
  - [ ] Sprint Review Gate: ctest/ASan 数字验证通过 (Sprint Review artifact 尚未完成)

#### 任务分类

| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|:------:|-------------|
| `demo-chat-v2` | pdk_chat_demo v2 扩展 | PlanExecute/ForkJoin DSL + SkillInterpreter | P0 | PlanExecute循环；ForkJoin循环；SkillInterpreter隔离 |
| `platform` | PDK 平台化 | 开发者指南 + AgentForge 第 2 领域 agent | P0 | PDK开发者指南；AgentForge第二领域agent |
| **`execution-plane-wave2`** | **🎯 Execution Plane Wave 2 强制决策** | ADR-0073 翻牌 + ADR-0074 baseline + ADR-0072 D1+D4 | **P0** | ADR-0073 schema契约；ADR-0074 prompt baseline；ADR-0072 stream扩展；ADR-0072 backend字段 |
| `execution-plane-prep` | Execution Plane 准备 | ADR-0068 amendment PR + Phase 6c 准备 | P0 | canonical topic registry；Evidence Gate准备 |
| `architecture` | 架构对齐 | ADR-0042 状态 + 服务化评估 (U8 削减) | P1 | ADR-0042状态对齐；服务化评估 |
| `governance` | 治理节奏 | Sprint Review + Drift Gate 准备 | P1 | Sprint Review；Drift Gate；ADR与spec对齐 |

#### 任务明细

| ID | 任务 | 分类 | 估时 | 优先级 | 依赖 |
|:---|------|------|:----:|:------:|------|
| U1 | pdk_chat_demo: PlanExecuteLoop + ForkJoinLoop DSL 实现 | `demo-chat-v2` | 8h | P0 | Phase 6a |
| U2 | pdk_chat_demo: Code Review SKILL.md 集成 SkillInterpreter | `demo-chat-v2` | 6h | P0 | ADR-0055 V1 |
| U3 | PDK 开发者指南: `include/agenticdsl/pdk/README.md` | `platform` | 6h | P0 | — |
| U4 | AgentForge 第 2 个领域 agent: 验证 PDK 复用性 | `platform` | 8h | P1 | U3 |
| U5 | DSLValidator 增强: `.agent.md` schema 校验 | `architecture` | 6h | P1 | — |
| U6 | ADR-0042 状态对齐 | `architecture` | 2h | P1 | — |
| **W1** | **✅ ADR-0073 翻牌 🟡 Partial** (Phase 6a manifest schema 边界部分采纳 + 修正 docs/README.md + adr-0073-impl-scope-audit.md) | `execution-plane-wave2` | 2h | P0 | Phase 6a |
| **W2** | **🎯 ADR-0074 D3 baseline 测量** (`tools/measure_prompt_baseline` + 50 golden tasks YAML + V0 prompt + 3 模型) | `execution-plane-wave2` | 10h | P0 | — |
| **W3** | **🎯 ADR-0074 D6 JSONL 训练数据结构** (data/training/ + schema_snapshot_hash) | `execution-plane-wave2` | 4h | P0 | — |
| **W4** | **🎯 ADR-0072 D1 `stream: true` 扩展** (tool_call/shell.exec/dsl_call 3 处 + IStreamHandle) | `execution-plane-wave2` | 6h | P0 | — |
| **W5** | **🎯 ADR-0072 D4 `backend:` 字段** (DSL 解析器 + `env:` → `env_vars:` 别名) | `execution-plane-wave2` | 2h | P0 | — |
| **W6** | **🎯 ADR-0068 §附录 A amendment PR 起草** (14 候选主题注册) | `execution-plane-prep` | 4h | P0 | — |
| U8 | ~~Phase 6 服务化重启评估~~ — **CUT per Oracle 修复 #1** | (削减) | — | — | — |

> **实际执行回顾（2026-08-17）**: 原计划 64h / 44h 容量超额未在 08-05 前完成 descope 决策；phase-6b 实际 08-11 kickoff，已完成 21 个归档 changes，但仍有 8/11 gates 未满足。
>
> **carry-over 决策**:
> 1. U2/U3/U4/U6 与 W2/W3/W4/W5 不再假设于 08-19 前完成，转为 Phase 6c 的显式前置队列。
> 2. 历史 descope 选项已废止；U8 Phase 6 服务化重启评估继续维持 CUT。
> 3. ADR-0074 few-shot/golden 工作与 W2 baseline 保持依赖链，不在未有 baseline 证据前强行宣称完成。
>
> phase-6b 的 change-level `21/21` 不等于 gate-level 完成；阶段状态保持 partial ship + carry-over。

---

### Phase 8: Sprint 26-27 — Execution Plane 完整 ship (phase-6c)

**目标**: Execution Plane 完整 ship (Wave 2 全部 + Wave 2.5 EnvBackend 启动) + Evidence Gate 决议 + Control Plane 启动评估
**平面范围**: 🎯 **AgenticDSL Execution Plane** (6 个 ADR: 0071 顶层 + 0072/0073/0074/0075/0078)
**状态**: ⏸ 未开始 (Phase 6b carry-over 队列先行)
**前置阶段**: phase-6b (partial ship)
**周期**: 2026-08-19 ~ 2026-09-09 (3 周, ~80h 容量)
**触发条件**: Phase 6b Sprint Review 通过 + 8 项 carry-over（U2/U3/U4/U6/W2/W3/W4/W5）在第一周内纳入

> **实际可执行性（2026-08-17 评估）**: Phase 6b 8 项 carry-over 合计约 44h（明细以 U2/U3/U4/U6/W2/W3/W4/W5 任务表为准）；若不在第一周（~27h）前完成，phase-6c 核心 C1-C4 Evidence Gate 依赖链将断裂。C8（ADR-0073 D2/D4，`adr-0073-d2-declare-tool-v3`）已于 2026-08-14 提前 ship，6c 自身剩余任务约 80h；carry-over 与 6c 合计约 124h，明显超过 80h 容量。

**完成条件**:
  - [ ] ADR-0074 baseline V1/V2/V3 prompt 完整 ship (含 few-shot 30+ + golden 50+)
  - [ ] **Evidence Gate 第一次决议** (per ADR-0074 D4): parse-valid ≥85% + task-success L1 ≥70%
  - [ ] ADR-0072 D2 `$var` 实施 (条件: parse-valid < 85% 触发)
  - [ ] ADR-0072 D3 declarative style (条件: `85% ≤ parse-valid < 90%` 临界带)
  - [ ] ADR-0072 D5 双语法共存期启动 (D2+D3 触发后强制)
  - [x] ADR-0073 D2 ToolMetadata V3 / DECLARE_TOOL V3 部分 ship（2026-08-14；D2/D4 完成，D3 仍待 C9）
  - [ ] ADR-0073 D3 运行时校验 (ToolCoordinator 4 步 sanitization pipeline)
  - [ ] **🆕 ADR-0075 Phase 1: LocalBackend ship** (fork + execve + 超时 + 输出截断)
  - [ ] **🆕 ADR-0075 Phase 2: DockerBackend ship** (libcurl + Docker REST API)
  - [ ] **🆕 ADR-0075 D5 EnvValidationHook + BackendPolicy ship** (ToolCoordinator pre-hook)
  - [ ] Control Plane 启动决策树输出 (per active-status.md §四)

#### 任务分类

| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|:------:|-------------|
| `execution-baseline` | Prompt baseline 完整化 | V1 schema 约束 + V2 few-shot + V3 两阶段注入 | P0 | few-shot examples；golden suite；V1/V2/V3 prompt |
| `evidence-gate` | Evidence Gate 第一次决议 | baseline 数据 + Go/No-Go 阈值 | P0 | Evidence Gate决议；parse-valid阈值；task-success阈值 |
| `execution-dsl` | DSL 节点扩展条件性 ship | D2 `$var` + D3 declarative + D5 共存期 (条件触发) | P0 | $var变量；declarative语法糖；双语法共存 |
| `schema-complete` | Tool JSON Schema 完整 ship | DECLARE_TOOL V3 自动生成 + 校验层 | P0 | DECLARE_TOOL V3自动生成；ToolCoordinator校验层 |
| `execution-envbackend` | 🆕 EnvBackend ship | LocalBackend + DockerBackend + EnvValidationHook | P0 | LocalBackend；DockerBackend；EnvValidationHook |
| `control-plane-eval` | Control Plane 启动评估 | 4 项启动条件逐项检查 + 决策树 | P1 | 启动条件评估；Control Plane决策树 |

#### 任务明细

| ID | 任务 | 分类 | 估时 | 优先级 | 依赖 |
|:---|------|------|:----:|:------:|------|
| C1 | ADR-0074 D1 few-shot examples 30+ 采集 (4 维度 × 8 examples) | `execution-baseline` | 12h | P0 | W2 (baseline 测量) |
| C2 | ADR-0074 D2 held-out golden suite 50 tasks YAML | `execution-baseline` | 10h | P0 | W2 |
| C3 | ADR-0074 D3 V1/V2/V3 prompt 实施 + 测量 | `execution-baseline` | 8h | P0 | C1, C2 |
| C4 | ADR-0074 D4 Evidence Gate 第一次决议 (`docs/audits/<date>-evidence-gate-v1.md`) | `evidence-gate` | 4h | P0 | C3 |
| C5 | ADR-0072 D2 `$var` 实施 (条件: parse-valid < 85%) | `execution-dsl` | 8h | P0* | C4 (Gate 决议) |
| C6 | ADR-0072 D3 declarative style (`exec:` 语法糖, 条件: `85% ≤ x < 90%`) | `execution-dsl` | 4h | P0* | C4 |
| C7 | ADR-0072 D5 双语法共存期基础设施 (lint + 警告) | `execution-dsl` | 4h | P0 | C5, C6 |
| C8 | ~~ADR-0073 D2 DECLARE_TOOL V3 自动生成 (C++ 类型反射)~~ — **提前 ship 2026-08-14** (`adr-0073-d2-declare-tool-v3`; D2/D4 Partial) | `schema-complete` | 8h | P0 | W1 (已满足) |
| C9 | ADR-0073 D3 ToolCoordinator 校验层集成 (4 步 sanitization pipeline) | `schema-complete` | 6h | P0 | C8 (D2/D4 已提前 ship) |
| **C11** | **🆕 ADR-0075 D2 LocalBackend ship** (fork + execve + 超时 + 输出截断) | `execution-envbackend` | 8h | P0 | W5 (`backend:` 字段) |
| **C12** | **🆕 ADR-0075 D3 DockerBackend ship** (libcurl + Docker REST API) | `execution-envbackend` | 8h | P0 | C11 |
| **C13** | **🆕 ADR-0075 D5 EnvValidationHook + BackendPolicy ship** (ToolCoordinator pre-hook) | `execution-envbackend` | 6h | P0 | C11 |
| C10 | Control Plane 启动评估文档 (per active-status.md §四 4 项条件) | `control-plane-eval` | 2h | P1 | C4 |

> **容量重估（2026-08-17）**: 原估 ~88h / ~80h；C8（ADR-0073 D2/D4）已于 2026-08-14 提前 ship，6c 自身剩余任务约 ~80h。另需先处理 6b carry-over 约 44h，总需求约 ~124h；即使 C5/C6/C7 条件性跳过（最多节省约 16h）仍超约 28h。09-09 收官需追加显式 descope（建议 U4 延后、C12 DockerBackend 推迟、U2 降级 mock-only），否则日期顺延至 09-16+。
>
> *C5+C6 条件性: Evidence Gate PASS (parse-valid ≥85%) → C5 跳过; parse-valid ≥90% → C6 也跳过

---

### Phase 9: Control Plane (gated) — MCP Server (phase-7)

**目标**: 启动 **📡 MCP 控制面** — DSL Engine 暴露为 MCP server (stdio + HTTP+SSE)
**平面范围**: 📡 **MCP Control Plane** (1 个 ADR: 0076)
**状态**: ⏸ **gated** (Phase 6c 收官时启动条件评估；当前人力与 Evidence Gate 均不满足)
**周期**: TBD（原估 2026-09-09 ~ 2026-11-04；实际启动取决于 Phase 6c 收官和人力条件）
**前置条件** (2026-08-17 Oracle 审计校准):

| 启动条件 | 阈值 | 现状 (2026-08-17) | 评估时点 |
|---------|------|------------------|---------|
| AgentForge 第 2 agent | 可独立运行 | ❌ 当前无第 2 agent（U4 carry-over 至 6c） | Phase 6c 收官 |
| Solo Dev 容量 | ≥2 人 OR ≥80h/双周 | ❌ 当前 1 人, ~27h/周（不满足 ≥80h/双周）| Phase 6c 收官 + 外部人力变化 |
| ADR-0068 §附录 A amendment PR | 14 候选主题 ship | ✅ 2026-08-13 archived（W6 已满足） | — |
| ADR-0073 完整 ship | D2+D3 ship | 🟡 D2 部分 ship（2026-08-14）；D3 为 Phase 6c C9 | Phase 6c 收官 |
| Evidence Gate PASS | parse-valid ≥85% + task-success L1 ≥70% | ❌ W2 baseline 未完成（carry-over；待 C4 决议） | Phase 6c 第二周 |
| ADR-0075 EnvBackend ship | Local+Docker ship | ❌ Phase 6c C11-C13 | Phase 6c 收官 |

> **循环依赖修正（Oracle 审计）**: Evidence Gate 依赖链（W2→C1→C3→C4）需等到 Phase 6c 第二周才能完成，因 W2 baseline 是 6b carry-over。Phase 7 启动最早时点为 Phase 6c 收官 + 人力条件满足（≥2 人 OR ≥80h/双周），推断为 2026-10 起或更晚。

**完成条件** (全部启动条件满足后):
  - [ ] ADR-0076 D1 stdio transport ship (JSON-RPC 2.0 over pipe)
  - [ ] ADR-0076 D2 静态 token 鉴权 ship (0600 权限校验 + Bearer / X-MCP-Token 双 header)
  - [ ] ADR-0076 D3 tools/list + tools/call ship (ToolCoordinator.execute 路由)
  - [ ] ADR-0076 D6 inputSchema 零转换 验证 (ToolMetadata V3 → MCP round-trip)
  - [ ] **🆕 ADR-0076 D8 Stateless 设计 ship** (per-request, no session state)
  - [ ] ADR-0076 D4 prompts/* ship (MCP prompts V0/V1/V2/V3 + select_subgraphs + evidence_gate_audit)
  - [ ] ADR-0076 D5 resources/* ship (lib/ 12 stdlib subgraphs URI)
  - [ ] ADR-0076 D7 external MCP client ship (外部 tool 拉取 + backend policy)
  - [ ] Control Plane 收官评估: Data Plane 启动条件检查

#### Phase 7 拆分 (per Oracle 容量评估, descope 路径)

| Sub-phase | 周期 | 估时 | 内容 |
|-----------|------|:---:|------|
| **Phase 7a** | Sprint 28-29 (~80h) | ~4 周 | D1 stdio + D2 token + D3 tools/* + D6 inputSchema 零转换 + D8 Stateless |
| **Phase 7b** | Sprint 30-31 (~80h) | ~4 周 | D4 prompts/* + D5 resources/* + D7 external client |
| **Phase 7c** | Phase 8+ | 4 周 | D1 HTTP+SSE transport (long-tail) |

**Descope 推荐**: 启动 Phase 7a 仅（D1+D2+D3+D6+D8 核心暴露）；Phase 7b/7c 在 **Phase 7a ship ≥3 个月且 Phase 8a 启动条件满足** 后再评估。不得以 Phase 8a ship 作为 Phase 7b/7c 的前置条件，避免与 Phase 8a 的“Phase 7a ship ≥3 个月”门禁形成循环依赖。

#### 任务分类 (Phase 7a, gated; 状态说明移入描述)

| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|:------:|-------------|
| `control-stdio` | MCP stdio transport | pipe + JSON-RPC 2.0（⏸ gated） | P0 | JSON-RPC 2.0；stdio transport |
| `control-token` | 静态 token 鉴权 | Bearer / X-MCP-Token + 0600 校验（⏸ gated） | P0 | 静态token；Bearer鉴权；0600权限 |
| `control-tools` | MCP tools/* 暴露 | ToolCoordinator.execute 路由 + inputSchema 零转换（⏸ gated） | P0 | tools/list；tools/call；inputSchema零转换 |
| **`control-stateless`** | **🆕 MCP Stateless 设计** | per-request, no session state, horizontal scaling（⏸ gated） | P0 | per-request；无session状态；横向扩展 |
| `control-prompts` | MCP prompts/* 暴露 | ADR-0074 baseline V0-V3 + select_subgraphs + evidence_gate_audit（⏸ Phase 7b） | P0 | MCP prompts；select_subgraphs；Evidence Gate审计 |
| `control-resources` | MCP resources/* 暴露 | lib/ 12 stdlib subgraphs URI（⏸ Phase 7b） | P1 | MCP resources；stdlib subgraph URI |
| `control-client` | 外部 MCP client | 外部 tool 拉取 + backend policy 强制（⏸ Phase 7b） | P1 | 外部MCP client；backend policy |
| `control-http-sse` | MCP HTTP+SSE transport | httplib + SSE + bearer token（⏸ descoped Phase 7c） | P2 | HTTP+SSE；SSE transport |

---

### Phase 10: Data Plane (gated) — gRPC Data Plane (phase-8a)

**目标**: 启动 **📊 gRPC 数据面** — High-throughput 流式通道 (LLM token stream / 模型权重 / 大文件 / 分布式遥测)
**平面范围**: 📊 **gRPC Data Plane** (1 个 ADR: 0077)
**状态**: ⏸ **gated** (Phase 7a ship ≥3 个月 + 路由阈值实测校准需求)
**周期**: 2026-11+ (估时 2-3 周, 容量 ~80-120h)
**前置条件** (per ADR-0077 D8):

| 启动条件 | 阈值 | 现状 | 评估时点 |
|---------|------|------|---------|
| Phase 7a ship ≥3 个月 | MCP server 稳定运行 | ⏸ Phase 7a 启动评估中 | Phase 7a ship 后 |
| Control Plane 容量超额 | 64KB 阈值不准, 需实测 | ⏸ 待 Control Plane ship | Phase 7a ship 后 |
| 分布式部署需求 | K8s / multi-region | ❌ 当前单实例 | 外部触发 |
| LLMDataPlane 高频需求 | Fine-tune 数据采集 > 100 events/s | ❌ 当前 <10 events/s | AgenticMind ship |

**完成条件** (全部启动条件满足时):
  - [ ] ADR-0077 D1 4 service ship (LLMDataPlane / BlobTransfer / RemoteExecutor / Telemetry)
  - [ ] ADR-0077 D2 64KB 路由规则实测校准 + 落地
  - [ ] ADR-0077 D4 protobuf + grpc-cpp vendor 集成
  - [ ] ADR-0077 D6 GRPCBackend EnvBackend 集成 (Execution Plane ↔ Data Plane 衔接)
  - [ ] ADR-0077 D7 ToolCoordinator 路由决策 (payload size + streaming 自动选择)
  - [ ] Data Plane 收官评估: 启动 Phase 8b Fine-tune 条件检查

---

### Phase 11: Execution Plane 支撑 (gated) — Fine-tune (phase-8b)

**目标**: LLM 模型微调, 强化 AgenticDSL 生成能力
**平面范围**: 🎯 **AgenticDSL Execution Plane 支撑组件** (1 个 ADR: 0078)
**状态**: ⏸ **gated** (AgenticMind ship + Evidence Gate FAIL OR 用户 ≥10)
**周期**: 估时 4-6 周, ~160-240h (远超 Solo Dev 容量, 需 Phase 7-Phase 8a 团队扩张 OR descope)

**启动条件** (per ADR-0078 D2, 任一):
1. AgenticMind 项目 ship + 探索结果回流
2. Evidence Gate FAIL (parse-valid < 85% 或 task-success 阈值)
3. Production 用户 ≥ 10 + 训练数据 ≥ 1 万条
4. Fine-tune 价格 ≤ $1 / 1M tokens

**完成条件**:
  - [ ] ADR-0078 D1 4 维度基模选型 (Capability/Latency/Cost/Openness)
  - [ ] ADR-0078 D3 训练数据来源 3 路汇总 (baseline JSONL + 失败事件 + AgenticMind 回流)
  - [ ] ADR-0078 D4 LoRA/QLoRA fine-tune (单 A100 80GB × 24h)
  - [ ] ADR-0078 D5 重跑 baseline + A/B (p < 0.05)
  - [ ] ADR-0078 D7 Fine-tune 模型注册为 ILLMProvider + MCP prompts/* 更新

---

## 架构 P0 缺口评估时间线 (更新)

| 缺口 | 当前状态 | Phase 6c 末评估 | Phase 7 评估 | 启动条件 |
|------|---------|:---:|:---:|------|
| `pdk_manifest()` | ❌ | ✅ 评估 | — | AgentForge ≥2 agent 后需机器可读 manifest |
| `AgentDescriptor` + `pdk_register_agent()` | ❌ | ✅ 评估 | — | pkm_temporal Phase 2 需注册 agent |
| `CapabilityRegistry` | ❌ | ✅ 评估 | — | ≥3 个 agent plugin 存在时需 discovery |
| **ADR-0068 §附录 A 主题注册** | ✅ 14 topics registered (2026-08-13) | **✅ W6 archived** | — | **LLM-native 实施前置** |
| **ADR-0073 完整 ship** | 🟡 D2/D4 Partial（2026-08-14 ship；D3 校验层仍待 C9，详见 adr-0073-impl-scope-audit.md） | ✅ Phase 6c C9 | — | **Execution Plane 完整前置** |
| **ADR-0075 EnvBackend ship** | ❌ | ✅ Phase 6c C11-C13 ship | — | **Execution Plane 完整前置** |
| Wasm 技术栈预研 | ❌ | — | ✅ Phase 8a 评估 | gRPC Data Plane 启动后 |

---

## LLM-native 三平面实施 Wave 推进路径 (新增)

```
Phase 6a (Sprint 24, 2026-07-24 ~ 08-05): Demo 收尾 (无 LLM-native 工作)
    │
Phase 6b (Sprint 25, 2026-08-11 ~ 08-19): Execution Plane Wave 2 partial ship + carry-over
    │ ├─ W1: ADR-0073 翻牌 🟡 Partial
    │ ├─ W2: ADR-0074 D3 baseline 测量
    │ ├─ W3: ADR-0074 D6 JSONL 训练数据结构
    │ ├─ W4: ADR-0072 D1 stream:true 扩展
    │ ├─ W5: ADR-0072 D4 backend: 字段
    │ └─ W6: ADR-0068 amendment PR 起草
    │
Phase 6c (Sprint 26-27, 2026-08-19 ~ 09-09): Execution Plane 完整 ship
    │ ├─ C1-C4: ADR-0074 baseline 完整化 + Evidence Gate 第一次决议
    │ ├─ C5-C7: ADR-0072 D2+D3+D5 条件性 (Gate 触发)
    │ ├─ C8-C9: ADR-0073 完整 ship
    │ ├─ C11-C13: ADR-0075 EnvBackend Local+Docker ship  ← 🆕 从 Phase 7 移此
    │ └─ C10: Control Plane 启动评估
    │
Phase 7 (Sprint 28+, 2026-09+): Control Plane (gated)
    │ ├─ Phase 7a: ADR-0076 stdio + token + tools/* + D8 Stateless
    │ ├─ Phase 7b: ADR-0076 prompts/* + resources/* + external client
    │ └─ Phase 7c: ADR-0076 HTTP+SSE (long-tail)
    │
Phase 8a (Phase 8 启动): Data Plane (gated)
    │ └─ ADR-0077 4 service + 路由规则 + GRPCBackend 集成
    │
Phase 8b (条件触发): Execution Plane 支撑 Fine-tune (gated)
    └─ ADR-0078 D1-D7 (基模选型 + 训练管线 + AgenticMind 回流 + serving)
```

---

## 两个 Demo 完整演进路线 (更新)

```
pdk_chat_demo:
  Sprint 24 (Phase 6a): v1 收尾 (Session + Budget + Schema)
      │
  Sprint 25 (Phase 6b): v2 启动 (PlanExecute/ForkJoin + SKILL.md)
      │
  Sprint 26+ (Phase 6c/7): v2 完成 (OTel 集成 + Conformance + Wasm Agent — descope per 容量)
      │
  AgentForge: 作为 PDK 消费方的参考实现

pkm_temporal_demo:
  Sprint 24 (Phase 6a): Phase 1a+b+c (PDK skeleton + demo + tests + CI)
      │
  Sprint 25 (Phase 6b): Phase 2 评估 (gRPC 直连可行性 + protobuf 构建成本)
      │
  Sprint 26+ (Phase 7/8a): Phase 2 实施 (gRPC client + pdk_register_agent)
      │
  PKGM 生产: 正式 Temporal 集成
```

---

## 治理节奏检查点 (更新)

| 日期 | 检查项 | 对应任务 | 产出 |
|------|--------|:---:|------|
| 2026-08-03 | 路线图 v3 重写 (三平面架构) | — | roadmap.md v3 |
| 2026-08-05 | Sprint 24 收官 | — | active-status.md 更新 + Wave 2 baseline 预览 |
| 2026-08-12 | Sprint Review + Wave 2 强制决策中期评估 | U1-U3 进度 | **未形成独立 Sprint Review artifact；Gate 顺延至 6b 收官** |
| 2026-08-19 | Sprint 25 收官 + Drift Gate | U6 + W1-W6 | ADR ↔ spec 映射 + LLM-native ADR 整合；**实际为 partial ship + carry-over** |
| 2026-08-26 | Phase 6c 启动评估 | C10 | carry-over 纳入检查 + Control Plane 启动决策树 |
| 2026-09-02 | Phase 6c 中期: Evidence Gate 数据 preview | C3 进度 | baseline V3 prompt 数据 |
| 2026-09-09 | Phase 6c 收官 + Evidence Gate 决议 | C4 + C9 + C11-C13 | Go/No-Go 决议 + Control Plane 启动评估（取决于 carry-over） |
| 2026-09-16+ | Phase 7a 启动评估 | C10 (Control Plane 评估) | **启动条件 6/6 检查；当前仅部分满足，不能预先承诺日期** |
| Phase 7a ship 后 ≥3 个月 | Phase 7a 稳定窗口 | — | Phase 8a (Data Plane) 启动评估 |
| 2026-12+ | Phase 8a 启动评估 | — | gRPC 4 service 实施 |

---

## 风险矩阵 (更新)

| 风险 | 概率 | 影响 | 缓解 |
|------|:---:|------|------|
| **Phase 6b 容量超额**（原计划 64h vs 44h；实际 8 项 carry-over） | **已发生** | 6b partial ship；U2/U3/U4/U6/W2/W3/W4/W5 carry-over 至 6c | 显式 descope：U4 可延后；C5/C6/C7 按 Evidence Gate 条件性执行 |
| **ADR-0068 §附录 A PR 阻塞 Wave 2 实施** (14 主题未注册) | **已闭环** | W6 已于 2026-08-13 ship | — |
| **Solo Dev 容量 3.5-5x 超额** (Phase 7 6-8 周 vs Solo Dev 1 人) | **高** | Phase 7/8 部分 descope | Phase 7a/7b/7c 拆分 descope 路径 |
| **Wave 3 启动条件未满足** (6 项均依赖 Phase 6c 收官) | 中 | Phase 7 暂缓 | 启动条件 6 项显式评估 (per active-status.md §四) |
| **Evidence Gate FAIL** (parse-valid < 85% 或 task-success 阈值) | 中 | Wave 2 条件性决策 (D2+D3+D6) 推迟 | C4 决议 → D2+D3 不触发 → Phase 8b fine-tune 路径触发 |
| **Candidate B 结构性暂缓** (active-status.md §四) | 中 | Phase 6 服务化路径阻塞 | Phase 8d 重新评估, 不影响 Phase 6a/6b/6c/7 |
| **三平面依赖链断裂** (Execution 未 ship, Control 启动) | 中 | Control Plane 失败 | Phase 7 启动条件强制要求 Execution Plane ship (C1-C13) |
| pkm_temporal Phase 1a popen 实现复杂 | 低 | +2h | 复用 `shell_tools` 已验证的 popen 模式 |
| SkillInterpreter 与 Code Review SKILL.md 集成意外复杂 | 中 | +4h | 先 mock-only 版本，真实隔离推迟到 Sprint 26+ |
| AgentForge 第 2 agent 设计时间超预期 | 中 | +6h | 缩小范围 (trivial domain, ≤4h 设计), descope to Phase 6c |

---

## 关键变更说明 (vs roadmap.md v2)

**v3 关键改进** (2026-08-03):
1. **三平面架构框架**: 路线图顶部新增 "Three-Plane Architecture" 章节 (~80 行), 明确 Execution/Control/Data 三平面职责分工 + 路由规则 + 依赖链
2. **AgenticDSL 替代 CLI 战略定位**: 显式框架化 "LLM 输出原生语言" 战略, 不再仅作为技术细节
3. **MCP Stateless 显式化**: 路线图 + ADR-0076 D8 双重声明 stateless 设计原则
4. **Phase 6c EnvBackend ship**: ADR-0075 (Local+Docker+EnvValidationHook) 从 Phase 7 移至 Phase 6c (与 Execution Plane 同阶段)
5. **Phase 7 仅 Control Plane**: 不再混入 EnvBackend, 纯 MCP server 拆分 7a/7b/7c
6. **Phase 8a 独立 Data Plane**: ADR-0077 gRPC 拆分为独立 Phase, 显式 gated by Control Plane ship ≥3 月
7. **启动条件 6 项显式**: Phase 7 启动条件从 4 项扩至 6 项 (新增 ADR-0075 EnvBackend ship + ADR-0073 完整 ship)
8. **依赖链 ASCII 图**: 三平面依赖链视觉化

**Active 状态**: Phase 6a 已于 2026-08-11 完成；Phase 6b 为 partial ship + carry-over；Phase 6c/7/8 全部 gated by Phase 6b/6c/7 ship 评估。

> **ADR 前置声明（2026-08-17）**: Phase 6b/6c/7 引用的 ADR-0072/0074/0075/0076/0077/0078 当前仍为 🔍 Proposed（待架构组评审）；相关实施排期以对应 ADR 评审通过为前提，不将 Proposed 误记为 Approved。

**关键决策** (供用户审阅):
- **A**: Phase 6b 已采用 carry-over，而非过期 descope 选项；详见 L221-L228。
- **B**: Phase 6c EnvBackend ship 时间窗（C11-C13）；需与 carry-over 和 Evidence Gate 重新排程。
- **C**: Phase 7 拆分 7a/7b/7c：先评估 7a；7b/7c 待 **Phase 7a ship ≥3 个月且 Phase 8a 启动条件满足** 后再评估。
- **D**: Three-Plane 战略章节接受度（核心战略定位, 影响后续所有 LLM-native 决策）。