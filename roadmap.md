# 项目路线图

> **驱动计划**: [`docs/superpowers/plans/2026-07-24-sprint-24-25-demo-driven-plan.md`](docs/superpowers/plans/2026-07-24-sprint-24-25-demo-driven-plan.md)
> **核心原则**: Demo 驱动 > 架构文档驱动 — 每个 Demo 是独立可交付物，反馈循环快，适合 Solo Dev
> **Phase**: 6 — Agent-as-Plugin (2026-07-15 ~ 至今, Phase 5 ✅ 收官)
> **路线图 v3** (2026-08-03 重写): 整合 **三平面架构** (Execution/Control/Data) + 7 个新 LLM-native ADR (0072-0078) + Oracle 容量审查 (session `ses_037e12115ffeLkeR1QTIko0BHb`) + active-status.md §四 Candidate B 结构性暂缓

## 元信息
- **版本**: 3.2 (2026-09-03 修订: Sprint 25 治理收官补登 — ADR-0042 🔍→🟡 翻牌同步 + ADR-0072 D1 阶段 A `stream:` 字段层 ship + ADR-0072 D4 `backend:`/`env_vars:` parser ship + ctest baseline 192→211 + Phase 7 启动复评机制精化 + baseline-retest runbook 引用)
- **创建时间**: 2026-07-24T00:00:00+08:00
- **最后更新**: 2026-09-03 — Sprint 25 治理收官 6 changes ship + archived (per Oracle sessions `ses_f9e000cb4ffeeYPcLW4j0QKviI` + `ses_f9a5905a`)
- **当前阶段**: phase-6c 收官期 (2026-08-19 ~ 09-09, ~80h 容量) — 🟢 实质 ship (11/13 任务完成 + C5 N/A + C4 Conditional 决议，真实 3 模型 baseline 重测 defer Sprint 25+)
- **下一阶段**: phase-7 (2026-09+ 起, gated by 启动条件 3/6 FAIL — 结构性条件不满足，本 Sprint 内不启动 Phase 7a)
- **阶段规划**:
  - **Phase 6c** (2026-08-19 ~ 09-09, ~80h) — 🟢 实质 ship 完成（仅真实 baseline 重测 carry-over）
  - **Phase 7** (2026-09+ 起, gated) — ⏸ Phase 7a **不启动**（3/6 FAIL 决议；结构性条件 1-agent + 1-人 ~27h/周 不满足）
  - **Phase 8a** (Phase 8 启动评估) — **Data Plane (gRPC)**（保持 gated by Phase 7a ship ≥3 月）
  - **Phase 8b** (条件触发) — **Execution Plane 支撑 (Fine-tune)**
- **治理节奏**: 2026-08-11 Sprint 25 kickoff → 2026-08-19 Sprint 25 收官 + Drift Gate → 2026-08-19 ~ 09-09 Phase 6c (实质 ship 完成) → 2026-09-02 Phase 7 启动条件实测 (3/6 FAIL, 不启动 Phase 7a) → Sprint 25+ 待真实 3 模型 baseline 重测 → Phase 7 启动条件复评

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
  - [x] **🎯 ADR-0072 D1+D4 强制决策 ship** (W5 `backend:`/`env_vars:` parser 完整 ship + W4 D1 阶段 A `stream:` 字段层 ship; **W4 阶段 B IStreamHandle 语义 4h 仍 carry-over to Sprint 25+**)
  - [x] **🎯 ADR-0068 §附录 A amendment PR** (W6 archived 2026-08-13；14 候选主题注册)
  - [x] ADR-0042 状态不匹配已解决 (U6, commit `622b742` 2026-09-03 翻牌 🔍 → 🟡; C16 5 项决策已 ship: Decorator 链 / Dual Consumer / available_models pure virtual / PluginLoader V2 / LlamaAdapter deprecated)
  - [ ] Sprint Review Gate: ctest/ASan 数字验证通过 (Sprint Review artifact 尚未完成)

#### 任务分类

| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|:------:|-------------|
| `demo-chat-v2` | pdk_chat_demo v2 扩展 | PlanExecute/ForkJoin DSL + SkillInterpreter | P0 | PlanExecute循环；ForkJoin循环；SkillInterpreter隔离 |
| `platform` | PDK 平台化 | 开发者指南 + AgentForge 第 2 领域 agent | P0 | PDK开发者指南；AgentForge第二领域agent |
| **`execution-plane-wave2`** | **🎯 Execution Plane Wave 2 强制决策** | ADR-0073 翻牌 + ADR-0074 baseline + ADR-0072 D1+D4 | **P0** | ADR-0073 schema契约；ADR-0074 prompt baseline；ADR-0072 stream扩展；ADR-0072 backend字段 |
| `execution-plane-prep` | Execution Plane 准备 | ADR-0068 amendment PR + Phase 6c 准备 | P0 | canonical topic registry；Evidence Gate准备 |
| `architecture` | 架构对齐 | ADR-0042 状态 ✅ ship + 服务化评估 (U8 削减) | P1 | ADR-0042状态对齐（已 ship, commit `622b742` 2026-09-03 🔍 → 🟡）；服务化评估 |
| `governance` | 治理节奏 | Sprint Review + Drift Gate 准备 | P1 | Sprint Review；Drift Gate；ADR与spec对齐 |

#### 任务明细

| ID | 任务 | 分类 | 估时 | 优先级 | 依赖 |
|:---|------|------|:----:|:------:|------|
| U1 | pdk_chat_demo: PlanExecuteLoop + ForkJoinLoop DSL 实现 | `demo-chat-v2` | 8h | P0 | Phase 6a |
| U2 | pdk_chat_demo: Code Review SKILL.md 集成 SkillInterpreter | `demo-chat-v2` | 6h | P0 | ADR-0055 V1 |
| U3 | PDK 开发者指南: `include/agenticdsl/pdk/README.md` | `platform` | 6h | P0 | — |
| U4 | AgentForge 第 2 个领域 agent: 验证 PDK 复用性 | `platform` | 8h | P1 | U3 |
| U5 | DSLValidator 增强: `.agent.md` schema 校验 | `architecture` | 6h | P1 | — |
| U6 | ADR-0042 状态对齐 🔍 → 🟡 | `architecture` | 2h | P1 | — | commit `622b742` 2026-09-03 ship + cross-file 同步 (ADR/README/relationships/iteration.json) |
| **W1** | **✅ ADR-0073 翻牌 🟡 Partial** (Phase 6a manifest schema 边界部分采纳 + 修正 docs/README.md + adr-0073-impl-scope-audit.md) → **2026-08-18 ✅ Approved** (D2+D3+D4 全 ship) | `execution-plane-wave2` | 2h | P0 | Phase 6a |
| **W2** | **✅ ADR-0074 D3 baseline 测量** (`tools/baseline/measure_prompt_baseline.py` + 50 golden tasks YAML + V0 prompt + mock 模型) | `execution-plane-wave2` | 10h | P0 | — |
| **W3** | **✅ ADR-0074 D6 JSONL 训练数据结构** (实际路径 `lib/prompt/{few_shots,golden}/` + `tools/prompt/export_training_data.py` + `schema_snapshot_hash`; 原规划 `data/training/` 路径已废弃) | `execution-plane-wave2` | 4h | P0 | — |
| **W4** | **✅ ADR-0072 D1 `stream:` 字段层 ship（阶段 A）** (`src/modules/parser/node_factory.cpp` `parse_context` 新增 stream 字段提取 + `tests/test_dsl_extensions.cpp` 3 类 W4 测试 PASS) — commit `c61a6d0` 2026-09-03; **阶段 B IStreamHandle 语义 4h 仍待 ship**（carry-over to Sprint 25+） | `execution-plane-wave2` | 6h (含阶段 B 4h) | P0 | — |
| **W5** | **✅ ADR-0072 D4 `backend:`/`env_vars:` 字段 parser 接入 ship** (`node_factory.cpp` parse_context 提取 backend/env/env_vars + `tests/test_dsl_extensions.cpp` 4 类 W5 测试 PASS + `docs/specs/dsl.md REQ-W5-001` 章节 + `env:` 作 `env_vars:` 别名) — commit `ca03071` 2026-09-03; ADR-0075 backend_policy.h ↔ parser **闭环** | `execution-plane-wave2` | 2h | P0 | ✅ Done |
| **W6** | **🎯 ADR-0068 §附录 A amendment PR 起草** (14 候选主题注册) | `execution-plane-prep` | 4h | P0 | — |
| U8 | ~~Phase 6 服务化重启评估~~ — **CUT per Oracle 修复 #1** | (削减) | — | — | — |

> **实际执行回顾（2026-08-17）**: 原计划 64h / 44h 容量超额未在 08-05 前完成 descope 决策；phase-6b 实际 08-11 kickoff，已完成 21 个归档 changes，但仍有 8/11 gates 未满足。
>
> **carry-over 决策**:
> 1. U2/U3/U4/U6 与 W2/W3/W4/W5 不再假设于 08-19 前完成，转为 Phase 6c 的显式前置队列。**状态更新 (2026-09-03)**：U6 已 ship (commit `622b742`)、W5 parser 已 ship (commit `ca03071`)、W4 阶段 A 字段层已 ship (commit `c61a6d0`)；当前真实未 ship = U2 + U3 + U4 + W4 阶段 B (IStreamHandle 语义, 4h) + ADR-0072 D6 (条件触发)。
> 2. 历史 descope 选项已废止；U8 Phase 6 服务化重启评估继续维持 CUT。
> 3. ADR-0074 few-shot/golden 工作与 W2 baseline 保持依赖链，不在未有 baseline 证据前强行宣称完成。
>
> phase-6b 的 change-level `21/21` 不等于 gate-level 完成；阶段状态保持 partial ship + carry-over。

---

### Phase 8: Sprint 26-27 — Execution Plane 完整 ship (phase-6c)

**目标**: Execution Plane 完整 ship (Wave 2 全部 + Wave 2.5 EnvBackend 启动) + Evidence Gate 决议 + Control Plane 启动评估
**平面范围**: 🎯 **AgenticDSL Execution Plane** (7 个 ADR: 0071 顶层 + 0072/0073/0074/0075/0076/0077 派生；0078 Wave 5+ descoped)
**状态**: 🟢 实质 ship 完成 (2026-09-02 闭环归档) — 11/13 任务 ✅ ship + C5 N/A（Gate Conditional 未触发 D2）+ C4 Conditional 决议（mock 88.24%，非真实 PASS）；真实 3 模型 baseline 重测 defer Sprint 25+ carry-over
**前置阶段**: phase-6b (partial ship)
**周期**: 2026-08-19 ~ 2026-09-09 (3 周, ~80h 容量) — **2026-09-02 收官**
**触发条件**: Phase 6b Sprint Review 通过 + 8 项 carry-over（U2/U3/U4/U6/W2/W3/W4/W5）在第一周内纳入

> **实际可执行性（2026-09-02 收官实况，2026-09-03 Sprint 25 治理收官补登）**: Phase 6b 8 项 carry-over 中 **W2（baseline 工具）实际已 ship**（`tools/baseline/measure_prompt_baseline.py` + `tools/measure_prompt_baseline.cpp`）；**2026-09-03 Sprint 25 治理收官后真实剩余 carry-over = U2（code-review-run.skill.md 缺 .cpp 集成，6h）+ U3（`include/agenticdsl/pdk/README.md` 未创建，6h）+ U4（AgentForge 第 2 agent 未启动，8h）+ W4 阶段 B（ADR-0072 D1 IStreamHandle 语义，4h）+ ADR-0072 D6（条件触发）**。C12 DockerBackend **实际以 cpp-httplib 替代 libcurl**（降成本，避免 1 周节省缺口）。
>
> *2026-09-03 Sprint 25 已 ship 闭环清单：U6（ADR-0042 🔍→🟡 翻牌, commit `622b742`）、W5（ADR-0072 D4 `backend:` parser, commit `ca03071`）、W4 阶段 A（ADR-0072 D1 `stream:` 字段层, commit `c61a6d0`）。*

**完成条件 (2026-09-02 实况)**:
  - [x] **C1** ADR-0074 D1 few-shot examples 30+ 采集（实际 32 文件 `lib/prompt/few_shots/`）✅ 2026-08-28 ship (T21)
  - [x] **C2** ADR-0074 D2 held-out golden suite 50 tasks（实际 54 文件 `lib/prompt/golden/`）✅ 2026-08-28 ship (T21)
  - [x] **C3** ADR-0074 D3 V1/V2/V3 prompt 实施 + mock baseline 测量（mock_mode=true parse-valid 88.24%）✅ 2026-08-28 ship (T21)
  - [x] **C4** Evidence Gate 第一次决议：**Conditional**（mock 88.24% ∈ [85,90) 不触发 D2，触发 D3；真实 3 模型重测 defer Sprint 25+）✅ 2026-09-02 ship (`docs/audits/2026-09-02-evidence-gate-v1.md`)
  - [N/A] **C5** ADR-0072 D2 `$var` 实施 — **Conditional 决议**触发 D3 不触发 D2，N/A
  - [x] **C6** ADR-0072 D3 declarative style (`exec:` 语法糖) — Conditional 触发 ✅ 2026-09-02 ship (`from-roadmap-phase-6c-execution-dsl`；`src/modules/parser/declarative_style.{h,cpp}` + `test_dsl_extensions.cpp` 4 cases)
  - [x] **C7** ADR-0072 D5 双语法共存期基础设施（D3 arm `exec: ↔ type: tool_call` 共存 + `dual_syntax_lint.cpp` C7）✅ 2026-09-02 ship (同 C6 change)
  - [x] **C8** ADR-0073 D2 ToolMetadata V3 / DECLARE_TOOL V3 自动生成 ✅ 2026-08-14 提前 ship
  - [x] **C9** ADR-0073 D3 ToolCoordinator 4 步 sanitization pipeline 集成 ✅ 2026-08-18 ship (Wave 1 followup 2026-08-23)
  - [x] **C11** ADR-0075 D2 LocalBackend ship（fork + execve + 超时 + 输出截断）✅ 2026-08-18 ship (Wave 3-A)
  - [x] **C12** ADR-0075 D3 DockerBackend ship（cpp-httplib REST API）✅ 2026-08-18 ship (Wave 3-A)
  - [x] **C13** ADR-0075 D5 EnvValidationHook + BackendPolicy ship（ToolCoordinator pre-hook）✅ 2026-08-18 ship (Wave 3-A)
  - [x] **C10** Control Plane 启动评估文档 (`docs/audits/2026-09-02-control-plane-eval-v1.md`) — 决议 3/6 FAIL，Phase 7a 不启动

#### 任务分类

| 分类ID | 名称 | 描述 | 优先级 | 预期改进方向 |
|--------|------|------|:------:|-------------|
| `execution-baseline` | Prompt baseline 完整化 | V1 schema 约束 + V2 few-shot + V3 两阶段注入 | P0 | ✅ mock baseline 88.24%；真实重测 defer |
| `evidence-gate` | Evidence Gate 第一次决议 | baseline 数据 + Conditional 决议 | P0 | ✅ Conditional 决议 ship |
| `execution-dsl` | DSL 节点扩展条件性 ship | D3 declarative style + D5 D3 arm 共存 ship；D2 N/A | P0 | ✅ D3+D5 ship (C6+C7)；D2 N/A (C5) |
| `schema-complete` | Tool JSON Schema 完整 ship | DECLARE_TOOL V3 + ToolCoordinator 4 步 pipeline | P0 | ✅ D2+D3+D4 全 ship (C8+C9) |
| `execution-envbackend` | EnvBackend ship | LocalBackend + DockerBackend + EnvValidationHook | P0 | ✅ D2+D3+D5 全 ship (C11-C13) |
| `control-plane-eval` | Control Plane 启动评估 | 6 项启动条件实测 → 3/6 FAIL 决议 | P0 | ✅ C10 ship FAIL 决议 |

#### 任务明细（2026-09-02 收官实况）

| ID | 任务 | 分类 | 状态 | Ship 时间 | 备注 |
|:---|------|------|:----:|-----------|------|
| C1 | ADR-0074 D1 few-shot examples 30+ 采集 | `execution-baseline` | ✅ | 2026-08-28 | 32 文件于 `lib/prompt/few_shots/` |
| C2 | ADR-0074 D2 held-out golden suite 50 tasks | `execution-baseline` | ✅ | 2026-08-28 | 54 文件于 `lib/prompt/golden/` |
| C3 | ADR-0074 D3 V1/V2/V3 prompt 实施 + mock 测量 | `execution-baseline` | ✅ | 2026-08-28 | `tools/baseline/measure_prompt_baseline.py` |
| C4 | Evidence Gate 决议（Conditional） | `evidence-gate` | ✅ Conditional | 2026-09-02 | `2026-09-02-evidence-gate-v1.md`；mock 88.24% ∈ [85,90) |
| C5 | ADR-0072 D2 `$var` | `execution-dsl` | [N/A] | — | Conditional 决议不触发 D2 |
| C6 | ADR-0072 D3 declarative style | `execution-dsl` | ✅ | 2026-09-02 | Conditional 触发；`from-roadmap-phase-6c-execution-dsl` |
| C7 | ADR-0072 D5 双语法共存期 D3 arm | `execution-dsl` | ✅ | 2026-09-02 | 同 C6 change；`dual_syntax_lint.cpp` C7 |
| C8 | ADR-0073 D2 DECLARE_TOOL V3 | `schema-complete` | ✅ | 2026-08-14 | 提前 ship；`adr-0073-d2-declare-tool-v3` |
| C9 | ADR-0073 D3 ToolCoordinator 4 步 pipeline | `schema-complete` | ✅ | 2026-08-18 | Wave 1 followup 2026-08-23 修 P1 语义缺口 |
| C11 | ADR-0075 D2 LocalBackend | `execution-envbackend` | ✅ | 2026-08-18 | Wave 3-A `from-roadmap-phase-6c-execution-envbackend` |
| C12 | ADR-0075 D3 DockerBackend | `execution-envbackend` | ✅ | 2026-08-18 | cpp-httplib REST API（**替代 libcurl**，降成本） |
| C13 | ADR-0075 D5 EnvValidationHook + BackendPolicy | `execution-envbackend` | ✅ | 2026-08-18 | ToolCoordinator pre-hook |
| C10 | Control Plane 启动评估 | `control-plane-eval` | ✅ FAIL | 2026-09-02 | `2026-09-02-control-plane-eval-v1.md` 3/6 FAIL |

> **事后容量复盘（2026-09-02 收官实况）**: 原估 ~124h（80h + 44h carry-over）超 ~28h。**实际通过 C5 N/A 省 8h + C12 改用 cpp-httplib 降成本 + Phase 6b W2 baseline 工具提前 ship 缓解 carry-over**——在 ~80h 容量内 ship。证据：13 任务 11 完成 + C5 N/A + C4 Conditional；C12 实施细节见 ADR-0075 ship commit。
>
> *C5 条件性: Evidence Gate PASS (parse-valid ≥85%) → C5 跳过; parse-valid ≥90% → C6 也跳过（**实测 C4 Conditional 触发 C6 不触发 C5**，与设计一致）*

---

### Phase 9: Control Plane (gated) — MCP Server (phase-7)

**目标**: 启动 **📡 MCP 控制面** — DSL Engine 暴露为 MCP server (stdio + HTTP+SSE)
**平面范围**: 📡 **MCP Control Plane** (1 个 ADR: 0076)
**状态**: ⏸ **gated + Phase 7a 不启动** (2026-09-02 实测 3/6 启动条件 FAIL，**结构性条件 1-agent + 1-人 ~27h/周 不满足**，文档工作无法改变；Phase 7a 本 Sprint 内不启动)
**周期**: TBD（原估 2026-09-09 ~ 2026-11-04；**实际启动取决于 (1) 真实 3 模型 baseline 重测 PASS + (2) AgentForge 第 2 agent 上线 + (3) 人力扩张至 ≥2 人**）
**前置条件** (2026-09-03 Sprint 25 治理收官后实测 6 项启动条件；治理接入 baseline-retest runbook + `--relaxed` 模式):

| 启动条件 | 阈值 | 现状 (2026-09-03) | 评估 | 评估时点 |
|---------|------|------------------|------|---------|
| AgentForge 第 2 agent | 可独立运行 | ❌ U4 仍未启动（carry-over） | ❌ FAIL | Phase 7a 重新评估（人力 + 真实 baseline 重测通过后） |
| Solo Dev 容量 | ≥2 人 OR ≥80h/双周 | ❌ 当前 1 人, ~27h/周；**Sprint 25 `control-plane-eval-c2-alignment` (commit `a742195`) ship `--relaxed` 模式 — C2 FAIL 不再触发 DescopeOrContinue，保住复评信号价值** | ❌ FAIL | 外部人力变化；每 Sprint 收官重跑 `scripts/control-plane-eval.py --relaxed` |
| ADR-0068 §附录 A amendment PR | 14 候选主题 ship | ✅ 2026-08-13 archived（W6 已满足） | ✅ PASS | — |
| ADR-0073 完整 ship | D2+D3+D4 ship | ✅ 2026-08-18 ✅ Approved（D2+D3+D4 全 ship） | ✅ PASS | — |
| Evidence Gate PASS | parse-valid ≥85% + task-success L1 ≥70% | 🟡 Conditional（mock 88.24%，非真实 PASS）；**Sprint 25 `baseline-retest-wait-condition` (commit `f1a6397`) ship `docs/runbooks/baseline-retest.md` 4 部分契约 — 触发信号 + runbook + Evidence Gate 重跑 + 容量预算 ≤8h/次 + 失败 fallback** | ❌ FAIL | Sprint 25+ 真实 3 模型重测（per runbook §1 触发条件） |
| ADR-0075 EnvBackend ship | Local+Docker ship | ✅ 2026-08-18 Wave 3-A ship (C11+C12+C13) + **Sprint 25 W5 parser 闭环 (commit `ca03071`)** | ✅ PASS | — |

> **实测决议 (2026-09-02)**：3/6 FAIL（AgentForge 第 2 agent / Solo Dev 容量 / Evidence Gate PASS），3/6 PASS（ADR-0068 / ADR-0073 / ADR-0075）。**结构性 FAIL 项（1-agent + 1-人 ~27h/周）无文档解药**，Phase 7a 不启动；Evidence Gate FAIL 项需 Sprint 25+ 真实 3 模型 baseline 重测。Phase 7 启动最早时点 = 三项 FAIL 全部转 PASS（详见 `docs/audits/2026-09-02-control-plane-eval-v1.md`）。
>
> **决策树（per C10 决议）**：
> - 2026-09-02 当前：Phase 7a **不启动**
> - 触发 Phase 7a 评估：3 项 FAIL 中**至少 AgentForge 第 2 agent + Evidence Gate PASS** 两项转 PASS，且 Solo Dev 容量无下降
> - 评估机制：每 Sprint 收官时重跑 `control-plane-eval-v1.md` 检查表

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
| 2026-09-02 | **Phase 6c 收官 + Evidence Gate 决议** ✅ | C4 + C9 + C11-C13 + C6 + C7 | `2026-09-02-evidence-gate-v1.md` (Conditional 决议) + `2026-09-02-control-plane-eval-v1.md` (3/6 FAIL) + `from-roadmap-phase-6c-execution-dsl` (ADR-0072 D3+D5 ship) |
| **2026-09-02** | **ADR-0072 翻牌 🟡 Partial** | — | `docs/adr/adr-0072-dsl-node-extensions.md` 状态变更；治理异常"实施先于翻牌"文档化 |
| **2026-09-03** | **Sprint 25 治理收官** ✅ (per Oracle session `ses_f9a5905a` 优先级排序) | U6 + W4 阶段 A + W5 + governance | 6 个 OpenSpec changes 全 ship + archived：`adr-0072-flip-to-partial` (12 项 Self-Review + 24h cooling-off, commits `e02b3b8`/`7276173`/`59fb3db`/`a7c22cd`)、`control-plane-eval-c2-alignment` (`--relaxed` mode, commits `a742195`/`236deaf`)、`baseline-retest-wait-condition` (runbook, commit `f1a6397`)、`adr-0042-state-alignment` (🔍→🟡, commit `622b742`)、`adr-0072-d4-backend-parser` (commit `ca03071`)、`adr-0072-d1-stream-true-parser` 阶段 A (commit `c61a6d0`)。ADR-0072 实施度 D1 0/6→1/6 + D4 1/6→2/6；ctest baseline 192→211 (+19, T1-T7 evolution-budget/mcts-axis6/signature-validation/cognitive-tools/workflow-materializer)。**Phase 7a 启动复评触发条件** = C1 + C5 转 PASS 且 C2 不下降 |
| **2026-09-02** | **Phase 7a 不启动决议** | C10 | DescopeOrContinue 决议入 active-status.md；3 项 FAIL 中 2 项为结构性（无文档解药）|
| Sprint 25+ | 真实 3 模型 baseline 重测（carry-over） | — | 触发条件：外部模型窗口可用 + Evidence Gate 重跑 |
| Sprint 25+ | Phase 7a 启动复评 | C10 重跑 | 每 Sprint 收官重跑 `control-plane-eval-v1.md` 6 项检查 |
| Phase 7a ship 后 ≥3 个月 | Phase 7a 稳定窗口 | — | Phase 8a (Data Plane) 启动评估 |
| Phase 7a ship 后 ≥3 个月 | Phase 7a 稳定窗口 | — | Phase 8a (Data Plane) 启动评估 |
| 2026-12+ | Phase 8a 启动评估 | — | gRPC 4 service 实施 |

---

## 风险矩阵 (更新)

| 风险 | 概率 | 影响 | 缓解 |
|------|:---:|------|------|
| **Phase 6b 容量超额**（原计划 64h vs 44h；实际 8 项 carry-over） | **已发生** | 6b partial ship；U2/U3/U4/U6/W2/W3/W4/W5 carry-over 至 6c；**2026-09-03 Sprint 25 收官**部分闭环：U6 (ADR-0042 🔍→🟡)、W5 (`backend:` parser)、W4 阶段 A (`stream:` 字段层) — 仍余 U2/U3/U4 + W4 阶段 B + ADR-0072 D6 | 显式 descope：U4 可延后；C5/C6/C7 按 Evidence Gate 条件性执行 |
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

**Active 状态**: Phase 6a 已于 2026-08-11 完成；Phase 6b 为 partial ship + carry-over（部分任务已 ship 缓解）；**Phase 6c 已于 2026-09-02 实质 ship 收官**（11/13 任务 + C5 N/A + C4 Conditional）；**Phase 7a 决议不启动**（3/6 FAIL 实测）；Phase 8a/8b 仍 gated by Phase 7a ship。

> **ADR 前置声明（2026-09-03 更新）**: Phase 6b/6c/7 引用 ADR 状态（截至 2026-09-03）：
> - **✅ Approved（已 ship）**：ADR-0071（08-25 Promotion）、ADR-0073（08-18 D2+D3+D4 全 ship）、ADR-0074（08-25 Promotion）、ADR-0075（08-18 Wave 3-A 全 ship + parser 闭环）
> - **🟡 Partial**：ADR-0042（**2026-09-03 翻牌**——C16 5 项决策 ship: Decorator 链 / Dual Consumer / available_models pure virtual / PluginLoader V2 / LlamaAdapter deprecated；C17+ 演进路径待独立 change 跟踪, commit `622b742`）、ADR-0072（**2026-09-02 翻牌**——D3 declarative style + D5 D3 arm 共存 ship；**2026-09-03 增量 ship**: D1 阶段 A 字段层 (`c61a6d0`) + D4 `backend:` 字段 (`ca03071`)；阶段 B IStreamHandle + D2 N/A + D6 OFF；治理异常"实施先于翻牌"已文档化）
> - **🔍 Proposed（待评审/排期）**：ADR-0076（Phase 7 gated）、ADR-0077（Phase 8a descoped docs-only）、ADR-0078（Phase 8b Wave 5+ descoped）
>
> ⚠️ **治理异常文档化**：ADR-0072 的 D3+D5 实现 ship 早于 ADR 评审通过（per `from-roadmap-phase-6c-execution-dsl` 2026-09-02 change），按 single-dev mode 治理（issue + 24h cooling-off）允许实施先于翻牌，但翻牌 OpenSpec 待建 `2026-09-02-adr-0072-flip-to-partial`。

**v3.2 关键改进** (2026-09-03 Sprint 25 治理收官补登):
1. **ADR-0042 翻牌 🔍 → 🟡 Partial 同步**: C16 5 项决策已 ship (Decorator 链 / Dual Consumer / available_models pure virtual / PluginLoader V2 / LlamaAdapter deprecated), commit `622b742` 跨 4 文件一致性 (ADR/README/relationships/iteration.json); C17+ 演进路径保留独立 change 跟踪 — **关闭 Phase 6b carry-over U6**
2. **ADR-0072 D1 阶段 A 字段层 ship**: commit `c61a6d0` `src/modules/parser/node_factory.cpp` `parse_context` 新增 `stream:` 字段提取 + `tests/test_dsl_extensions.cpp` 3 类 W4 测试 PASS; 实施度 D1 0/6 → 1/6; **阶段 B IStreamHandle 语义 4h 仍 carry-over**
3. **ADR-0072 D4 parser 字段接入 ship**: commit `ca03071` `backend:`/`env:`/`env_vars:` 解析 + 4 类 W5 测试 PASS + `docs/specs/dsl.md REQ-W5-001` 章节 + `env:` 作 `env_vars:` 别名; 实施度 D4 1/6 → 2/6; **ADR-0075 ↔ parser 闭环**
4. **Phase 7 启动条件精化**: Sprint 25 治理接入（commit `a742195` + `f1a6397`）
   - C2 Solo Dev `--relaxed` 模式（`scripts/control-plane-eval.py --relaxed`）— C2 FAIL 不再触发 DescopeOrContinue，保住复评信号价值
   - C5 Evidence Gate `docs/runbooks/baseline-retest.md` 4 部分契约 — 触发信号 + 重测 runbook + Evidence Gate 重跑 + 容量预算 ≤8h/次 + 失败 fallback
5. **ctest baseline 更新 192 → 211 (+19)**: T1-T7 evolution-budget 6 / mcts-axis6 6 / signature-validation 7 / cognitive-tools 4 / workflow-materializer 5 + Sprint 24 审计补全 2; **210/211 PASS (1 pre-existing timing flake per AGENTS.md)**
6. **ASan pre-existing 跟踪**: `test_skill_interpreter` (ADR-0066 V1 ship, 但 ASan-only 功能失败, debug PASS) — 建议独立 OpenSpec change 跟踪修复; 持续 follow-up
7. **Sprint 25 治理异常修复**: 24h cooling-off `adr-0072-flip-to-partial` change 由 Oracle `ses_f9ab25dcfffetx4J5UFA7JYBKV` 纠偏为 honest cooling-off record (commits `a7c22cd` + `236deaf`) — 治理节奏收紧

**Active 状态更新**: Phase 6c 已于 2026-09-02 实质 ship 收官 (11/13 任务 + C5 N/A + C4 Conditional)；**Sprint 25 治理收官 6 changes 全 ship + archived 2026-09-03**（ADR-0042 翻牌 + ADR-0072 D1 阶段 A + D4 parser + governance flywheel）；**Phase 7a 决议不启动**（3/6 FAIL 实测 + `--relaxed` + baseline-retest runbook 接入复评机制）；**Phase 7a 启动复评触发条件** = C1 (AgentForge ≥2 agents) + C5 (Evidence Gate 真实 PASS) 两项转 PASS 且 C2 (Solo Dev) 不下降；Phase 8a/8b 仍 gated by Phase 7a ship + ≥3 个月稳定窗口。

**关键决策** (供用户审阅):
- **A**: Phase 6b 已采用 carry-over，而非过期 descope 选项；详见 L221-L228。
- **B**: Phase 6c EnvBackend ship 时间窗（C11-C13）；需与 carry-over 和 Evidence Gate 重新排程。
- **C**: Phase 7 拆分 7a/7b/7c：先评估 7a；7b/7c 待 **Phase 7a ship ≥3 个月且 Phase 8a 启动条件满足** 后再评估。
- **D**: Three-Plane 战略章节接受度（核心战略定位, 影响后续所有 LLM-native 决策）。