# 项目路线图

> **驱动计划**: [`docs/superpowers/plans/2026-07-24-sprint-24-25-demo-driven-plan.md`](docs/superpowers/plans/2026-07-24-sprint-24-25-demo-driven-plan.md)
> **核心原则**: Demo 驱动 > 架构文档驱动 — 每个 Demo 是独立可交付物，反馈循环快，适合 Solo Dev
> **Phase**: 6 — Agent-as-Plugin (2026-07-15 ~ 至今, Phase 5 ✅ 收官)
> **路线图 v3** (2026-08-03 重写): 整合 **三平面架构** (Execution/Control/Data) + 6 个新 LLM-native ADR (0072/0074/0075/0076/0077/0078) + Oracle 容量审查 (session `ses_037e12115ffeLkeR1QTIko0BHb`) + active-status.md §四 Candidate B 结构性暂缓

## 元信息
- **版本**: 3 (2026-08-03 重写, 三平面架构)
- **创建时间**: 2026-07-24T00:00:00+08:00
- **最后更新**: 2026-08-03 — 路线图重写, 三平面重组
- **当前阶段**: phase-6a (2026-07-24 ~ 2026-08-05, 37h 容量)
- **下一阶段**: phase-6b (2026-08-05 ~ 2026-08-19, 44h 容量)
- **阶段规划**:
  - **Phase 6c** (2026-08-19 ~ 09-09, ~80h) — **Execution Plane 完整 ship**
  - **Phase 7** (2026-09+ 起, gated) — **Control Plane (MCP)**
  - **Phase 8a** (Phase 8 启动评估) — **Data Plane (gRPC)**
  - **Phase 8b** (条件触发) — **Execution Plane 支撑 (Fine-tune)**
- **治理节奏**: 2026-08-05 Sprint 24 收官 → 2026-08-12 Sprint Review → 2026-08-19 Drift Gate + Phase 6c 启动 → 2026-09-09 Phase 6c 收官 + Control Plane 启动评估

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

### Phase 6a: Sprint 24 — Demo 收尾与 TDK 骨架 (phase-6a)

**目标**: pdk_chat_demo v1 收尾 + pkm_temporal_demo PDK 骨架落地
**状态**: 🔄 进行中 (2026-08-03 ~50% 完成)
**周期**: 2026-07-24 ~ 2026-08-05 (12 天, ~37h 容量)
**完成条件**:
  - [x] `ctest -R pdk_chat` 全绿 (含新增 schema 校验 test case)
  - [x] `ctest -R temporal` 全绿 (≥8 test cases)
  - [x] `./pdk_chat_demo --mock` Session 持久化 + Budget 告警正常工作
  - [x] `./pkm_temporal_demo --mock` 4 个演示场景全部 PASS
  - [x] **PDK SafeExec jthread 重写 + 8 test cases PASS** (Phase 6a 任务 2, OpenSpec `2026-08-10-pdk-safe-exec-tests` archived 2026-08-10)
  - [x] **PDK Doxygen 覆盖率 ≥90% + pdk/README.md 3 章节扩展** (Phase 6a 任务 2)
  - [ ] 8/1 前 proposals/ 清理完成
  - [x] active-status.md 更新至 2026-08-10

#### 任务分类 (现状不变, 无 LLM-native 工作)

| 分类ID | 名称 | 描述 | 优先级 |
|--------|------|------|:------:|
| `demo-chat-v1` | pdk_chat_demo v1 收尾 | Session/Budget 验证修复 + Schema 校验 | P0 |
| `demo-temporal-1a` | pkm_temporal_demo PDK 骨架 | ITemporalClient + Mock + CLI + pdk_entry (5 工具) | P0 |
| `demo-temporal-1b` | pkm_temporal_demo 项目 | Demo 项目 + 4 场景 + 测试 | P0 |
| `demo-temporal-1c` | pkm_temporal_demo CI 集成 | 根 CMake 更新 + docs + CI hook | P1 |
| `governance` | 治理节奏 | proposals/ 清理 + active-status.md 同步 | P1 |

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

---

### Phase 6b: Sprint 25 — Demo 扩展 + Execution Plane 基础 (phase-6b)

**目标**: pdk_chat_demo v2 启动 + **Execution Plane 基础 ship (Wave 2 强制决策 24-32h)**
**状态**: ⏳ 未开始
**前置阶段**: phase-6a
**周期**: 2026-08-05 ~ 2026-08-19 (14 天, ~44h 容量)
**完成条件**:
  - [ ] `examples/pdk_chat_demo` 支持 3 种 Agent Loop (React ✅ / PlanExecute / ForkJoin)
  - [ ] Code Review SKILL.md 通过 SkillInterpreter 隔离执行
  - [ ] `include/agenticdsl/pdk/README.md` 完成
  - [ ] AgentForge 第 2 个领域 agent 可独立运行
  - [ ] `.agent.md` 加载时有 schema 校验
  - [ ] **🎯 Execution Plane: ADR-0073 翻牌 🟡 Partial**
  - [ ] **🎯 Execution Plane: ADR-0074 D3 baseline 第一次测量** (3 模型 × 50 tasks)
  - [ ] **🎯 Execution Plane: ADR-0072 D1+D4 强制决策 ship** (`stream: true` + `backend:` 字段)
  - [ ] **🎯 ADR-0068 §附录 A amendment PR 起草** (14 候选主题注册)
  - [ ] ADR-0042 状态不匹配已解决
  - [ ] Sprint Review Gate: ctest/ASan 数字验证通过

#### 任务分类

| 分类ID | 名称 | 描述 | 优先级 |
|--------|------|------|:------:|
| `demo-chat-v2` | pdk_chat_demo v2 扩展 | PlanExecute/ForkJoin DSL + SkillInterpreter | P0 |
| `platform` | PDK 平台化 | 开发者指南 + AgentForge 第 2 领域 agent | P0 |
| **`execution-plane-wave2`** | **🎯 Execution Plane Wave 2 强制决策** | ADR-0073 翻牌 + ADR-0074 baseline + ADR-0072 D1+D4 | **P0** |
| `execution-plane-prep` | Execution Plane 准备 | ADR-0068 amendment PR + Phase 6c 准备 | P0 |
| `architecture` | 架构对齐 | ADR-0042 状态 + 服务化评估 (U8 削减) | P1 |
| `governance` | 治理节奏 | Sprint Review + Drift Gate 准备 | P1 |

#### 任务明细

| ID | 任务 | 分类 | 估时 | 优先级 | 依赖 |
|:---|------|------|:----:|:------:|------|
| U1 | pdk_chat_demo: PlanExecuteLoop + ForkJoinLoop DSL 实现 | `demo-chat-v2` | 8h | P0 | Phase 6a |
| U2 | pdk_chat_demo: Code Review SKILL.md 集成 SkillInterpreter | `demo-chat-v2` | 6h | P0 | ADR-0055 V1 |
| U3 | PDK 开发者指南: `include/agenticdsl/pdk/README.md` | `platform` | 6h | P0 | — |
| U4 | AgentForge 第 2 个领域 agent: 验证 PDK 复用性 | `platform` | 8h | P1 | U3 |
| U5 | DSLValidator 增强: `.agent.md` schema 校验 | `architecture` | 6h | P1 | — |
| U6 | ADR-0042 状态对齐 | `architecture` | 2h | P1 | — |
| **W1** | **🎯 ADR-0073 翻牌 🟡 Partial** (审阅 Sprint 21 ship 内容 + 修正 docs/README.md) | `execution-plane-wave2` | 2h | P0 | Phase 6a |
| **W2** | **🎯 ADR-0074 D3 baseline 测量** (`tools/measure_prompt_baseline` + 50 golden tasks YAML + V0 prompt + 3 模型) | `execution-plane-wave2` | 10h | P0 | — |
| **W3** | **🎯 ADR-0074 D6 JSONL 训练数据结构** (data/training/ + schema_snapshot_hash) | `execution-plane-wave2` | 4h | P0 | — |
| **W4** | **🎯 ADR-0072 D1 `stream: true` 扩展** (tool_call/shell.exec/dsl_call 3 处 + IStreamHandle) | `execution-plane-wave2` | 6h | P0 | — |
| **W5** | **🎯 ADR-0072 D4 `backend:` 字段** (DSL 解析器 + `env:` → `env_vars:` 别名) | `execution-plane-wave2` | 2h | P0 | — |
| **W6** | **🎯 ADR-0068 §附录 A amendment PR 起草** (14 候选主题注册) | `execution-plane-prep` | 4h | P0 | — |
| U8 | ~~Phase 6 服务化重启评估~~ — **CUT per Oracle 修复 #1** | (削减) | — | — | — |

> **合计**: ~64h, **超 6b 44h 容量 20h**
>
> **Descope 路径** (任选 1):
> 1. **U4 推迟** → 节省 8h → 总 56h (仍超 12h)
> 2. **U7 OTel 集成评估** 整项削减 → 节省 4h
> 3. **ADR-0074 D1+D2 (few-shot + golden)** 推迟至 Phase 6c (仅测 baseline) → 节省 ~12h
> 4. **U4+U5 削减** → 总 50h
>
> **推荐 descope 组合**: 选项 3 + 选项 2 (ADR-0074 D1+D2 推迟 + U7 削减) → 总 ~48h (接近 44h 容量)
>
> ⚠️ 需 08-05 收官前最终确认 descope 路径

---

### Phase 6c: Sprint 26-27 — Execution Plane 完整 ship (phase-6c, 新增)

**目标**: Execution Plane 完整 ship (Wave 2 全部 + Wave 2.5 EnvBackend 启动) + Evidence Gate 决议 + Control Plane 启动评估
**平面范围**: 🎯 **AgenticDSL Execution Plane** (5 个 ADR: 0071 顶层 + 0072/0073/0074/0075 + 0078 准备)
**状态**: ⏸ 未开始 (Phase 6b 收官后启动)
**前置阶段**: phase-6b
**周期**: 2026-08-19 ~ 2026-09-09 (3 周, ~80h 容量)
**触发条件**: Phase 6b Sprint Review 通过 + ADR-0068 amendment PR ship + W1-W6 完成

**完成条件**:
  - [ ] ADR-0074 baseline V1/V2/V3 prompt 完整 ship (含 few-shot 30+ + golden 50+)
  - [ ] **Evidence Gate 第一次决议** (per ADR-0074 D4): parse-valid ≥85% + task-success L1 ≥70%
  - [ ] ADR-0072 D2 `$var` 实施 (条件: parse-valid < 85% 触发)
  - [ ] ADR-0072 D3 declarative style (条件: `85% ≤ parse-valid < 90%` 临界带)
  - [ ] ADR-0072 D5 双语法共存期启动 (D2+D3 触发后强制)
  - [ ] ADR-0073 D2 ToolMetadata V3 完整 ship (DECLARE_TOOL V3 自动生成)
  - [ ] ADR-0073 D3 运行时校验 (ToolCoordinator 4 步 sanitization pipeline)
  - [ ] **🆕 ADR-0075 Phase 1: LocalBackend ship** (fork + execve + 超时 + 输出截断)
  - [ ] **🆕 ADR-0075 Phase 2: DockerBackend ship** (libcurl + Docker REST API)
  - [ ] **🆕 ADR-0075 D5 EnvValidationHook + BackendPolicy ship** (ToolCoordinator pre-hook)
  - [ ] Control Plane 启动决策树输出 (per active-status.md §四)

#### 任务分类

| 分类ID | 名称 | 描述 | 优先级 |
|--------|------|------|:------:|
| `execution-baseline` | Prompt baseline 完整化 | V1 schema 约束 + V2 few-shot + V3 两阶段注入 | P0 |
| `evidence-gate` | Evidence Gate 第一次决议 | baseline 数据 + Go/No-Go 阈值 | P0 |
| `execution-dsl` | DSL 节点扩展条件性 ship | D2 `$var` + D3 declarative + D5 共存期 (条件触发) | P0 |
| `schema-complete` | Tool JSON Schema 完整 ship | DECLARE_TOOL V3 自动生成 + 校验层 | P0 |
| `execution-envbackend` | 🆕 EnvBackend ship | LocalBackend + DockerBackend + EnvValidationHook | P0 |
| `control-plane-eval` | Control Plane 启动评估 | 4 项启动条件逐项检查 + 决策树 | P1 |

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
| C8 | ADR-0073 D2 DECLARE_TOOL V3 自动生成 (C++ 类型反射) | `schema-complete` | 8h | P0 | W1 (Phase 6b ship) |
| C9 | ADR-0073 D3 ToolCoordinator 校验层集成 (4 步 sanitization pipeline) | `schema-complete` | 6h | P0 | C8 |
| **C11** | **🆕 ADR-0075 D2 LocalBackend ship** (fork + execve + 超时 + 输出截断) | `execution-envbackend` | 8h | P0 | W5 (`backend:` 字段) |
| **C12** | **🆕 ADR-0075 D3 DockerBackend ship** (libcurl + Docker REST API) | `execution-envbackend` | 8h | P0 | C11 |
| **C13** | **🆕 ADR-0075 D5 EnvValidationHook + BackendPolicy ship** (ToolCoordinator pre-hook) | `execution-envbackend` | 6h | P0 | C11 |
| C10 | Control Plane 启动评估文档 (per active-status.md §四 4 项条件) | `control-plane-eval` | 2h | P1 | C4 |

> **合计**: ~88h / ~80h 容量, **超额 8h** (推荐延后 C7 1 周至 Phase 7a 之前)
>
> *C5+C6 条件性: Evidence Gate PASS (parse-valid ≥85%) → C5 跳过; parse-valid ≥90% → C6 也跳过

---

### Phase 7: Control Plane (gated) — MCP Server (phase-7)

**目标**: 启动 **📡 MCP 控制面** — DSL Engine 暴露为 MCP server (stdio + HTTP+SSE)
**平面范围**: 📡 **MCP Control Plane** (1 个 ADR: 0076)
**状态**: ⏸ **gated** (需 Phase 6c 收官时启动条件评估通过)
**周期**: 2026-09-09 ~ 2026-11-04 (估时 6-8 周, 仍超 Solo Dev 容量, 需拆分 7a/7b/7c)
**前置条件** (per Oracle 修复 #1 + active-status.md §四 + ADR-0071 §战略协调):

| 启动条件 | 阈值 | 现状 (2026-08-03) | 评估时点 |
|---------|------|------------------|---------|
| AgentForge ≥ Sprint 25 milestone | 第 2 agent ship | ❌ 当前 Sprint 24 | Phase 6c 收官 |
| Solo Dev 容量 | ≥2 人 OR ≥80h/双周 | ❌ 当前 1 人, 81h/3 周 | Phase 6c 收官 |
| ADR-0068 §附录 A amendment PR | 14 候选主题 ship | ❌ Phase 6b W6 起草中 | Phase 6b 收官 |
| ADR-0073 完整 ship | D2+D3 ship | ❌ Phase 6c C8-C9 进行 | Phase 6c 收官 |
| Evidence Gate PASS | parse-valid ≥85% + task-success 阈值 | ⏸ 待 Phase 6c C4 决议 | Phase 6c 收官 |
| ADR-0075 EnvBackend ship | Local+Docker ship | ❌ Phase 6c C11-C13 | Phase 6c 收官 |

**完成条件** (全部启动条件满足时):
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
| **Phase 7a** | Sprint 28-29 (~80h) | 6-8 周 | D1 stdio + D2 token + D3 tools/* + D8 Stateless |
| **Phase 7b** | Sprint 30-31 (~80h) | 6-8 周 | D4 prompts/* + D5 resources/* + D7 external client |
| **Phase 7c** | Phase 8+ | 4 周 | D1 HTTP+SSE transport (long-tail) |

**Descope 推荐**: 启动 Phase 7a 仅 (D1+D2+D3+D8 核心暴露), Phase 7b/7c 待 Phase 8a ship ≥3 个月后启动

#### 任务分类 (Phase 7a, gated)

| 分类ID | 名称 | 描述 | 优先级 | 状态 |
|--------|------|------|:------:|------|
| `control-stdio` | MCP stdio transport | pipe + JSON-RPC 2.0 | P0 | ⏸ gated |
| `control-token` | 静态 token 鉴权 | Bearer / X-MCP-Token + 0600 校验 | P0 | ⏸ gated |
| `control-tools` | MCP tools/* 暴露 | ToolCoordinator.execute 路由 + inputSchema 零转换 | P0 | ⏸ gated |
| **`control-stateless`** | **🆕 MCP Stateless 设计** | per-request, no session state, horizontal scaling | P0 | ⏸ gated |
| `control-prompts` | MCP prompts/* 暴露 | ADR-0074 baseline V0-V3 + select_subgraphs + evidence_gate_audit | P0 | ⏸ gated (Phase 7b) |
| `control-resources` | MCP resources/* 暴露 | lib/ 12 stdlib subgraphs URI | P1 | ⏸ gated (Phase 7b) |
| `control-client` | 外部 MCP client | 外部 tool 拉取 + backend policy 强制 | P1 | ⏸ gated (Phase 7b) |
| `control-http-sse` | MCP HTTP+SSE transport | httplib + SSE + bearer token | P2 | ⏸ descoped (Phase 7c) |

---

### Phase 8a: Data Plane (gated) — gRPC Data Plane (phase-8a)

**目标**: 启动 **📊 gRPC 数据面** — High-throughput 流式通道 (LLM token stream / 模型权重 / 大文件 / 分布式遥测)
**平面范围**: 📊 **gRPC Data Plane** (1 个 ADR: 0077)
**状态**: ⏸ **gated** (Phase 7 ship ≥3 个月 + 路由阈值实测校准需求)
**周期**: 2026-11+ (估时 2-3 周, 容量 ~80-120h)
**前置条件** (per ADR-0077 D8):

| 启动条件 | 阈值 | 现状 | 评估时点 |
|---------|------|------|---------|
| Phase 7 ship ≥3 个月 | MCP server 稳定运行 | ⏸ Phase 7 启动评估中 | 2026-12+ |
| Control Plane 容量超额 | 64KB 阈值不准, 需实测 | ⏸ 待 Control Plane ship | Phase 7 ship 后 |
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

### Phase 8b: Execution Plane 支撑 (gated) — Fine-tune (phase-8b)

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
| **ADR-0068 §附录 A 主题注册** | ❌ 14 pending | **✅ Phase 6b W6 ship** | — | **LLM-native 实施前置** |
| **ADR-0073 完整 ship** | 🟡 Sprint 21 部分 | ✅ Phase 6c C8-C9 ship | — | **Execution Plane 完整前置** |
| **ADR-0075 EnvBackend ship** | ❌ | ✅ Phase 6c C11-C13 ship | — | **Execution Plane 完整前置** |
| Wasm 技术栈预研 | ❌ | — | ✅ Phase 8a 评估 | gRPC Data Plane 启动后 |

---

## LLM-native 三平面实施 Wave 推进路径 (新增)

```
Phase 6a (Sprint 24, 2026-07-24 ~ 08-05): Demo 收尾 (无 LLM-native 工作)
    │
Phase 6b (Sprint 25, 2026-08-05 ~ 08-19): Execution Plane Wave 2 基础 ship
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
| 2026-08-12 | Sprint Review + Wave 2 强制决策中期评估 | U1-U3 进度 | ctest/ASan 一致性验证 |
| 2026-08-19 | Sprint 25 收官 + Drift Gate | U6 + W1-W6 | ADR ↔ spec 映射 + LLM-native ADR 整合 |
| 2026-08-26 | Phase 6c 启动评估 | C10 | Control Plane 启动决策树 + Active 条件检查 |
| 2026-09-02 | Phase 6c 中期: Evidence Gate 数据 preview | C3 进度 | baseline V3 prompt 数据 |
| 2026-09-09 | Phase 6c 收官 + Evidence Gate 决议 | C4 + C8-C9 + C11-C13 | Go/No-Go 决议 + Control Plane 启动评估 |
| 2026-09-16 | Phase 7a 启动评估 | C10 (Control Plane 评估) | 启动条件 6/6 检查 |
| 2026-11+ | Phase 7 ship ≥3 个月 | — | Phase 8a (Data Plane) 启动评估 |
| 2026-12+ | Phase 8a 启动评估 | — | gRPC 4 service 实施 |

---

## 风险矩阵 (更新)

| 风险 | 概率 | 影响 | 缓解 |
|------|:---:|------|------|
| **Phase 6b 容量超额 20h** (64h vs 44h) | **高** | Sprint 失败 | descope 路径 (C1+C2 推迟 / U4 削减) 需 08-05 收官前最终确认 |
| **ADR-0068 §附录 A PR 阻塞 Wave 2 实施** (14 主题未注册) | **高** | 4 个 LLM-native ADR 不能实施 phantom 事件 | Phase 6b W6 PR 起草 + Phase 6c 第 1 周 merge ship |
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

**Active 状态**: Phase 6a 进行中 (T1-T8 现有任务保留), Phase 6b/6c/7/8 全部 gated by Phase 6a/6b/6c/7 ship 评估

**关键决策** (供用户审阅):
- **A**: Phase 6b descope 选择 — 推荐 选项 3+2 (ADR-0074 D1+D2 推迟 + U7 削减)
- **B**: Phase 6c EnvBackend ship 时间窗 (推荐: C11-C13 集中在 Sprint 27 末, 避免与 Evidence Gate 冲突)
- **C**: Phase 7 拆分 7a/7b/7c descope 路径 (推荐: 7a 启动, 7b/7c Phase 8 后)
- **D**: Three-Plane 战略章节接受度 (核心战略定位, 影响后续所有 LLM-native 决策)