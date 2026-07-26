# 项目路线图

> **驱动计划**: [`docs/superpowers/plans/2026-07-24-sprint-24-25-demo-driven-plan.md`](docs/superpowers/plans/2026-07-24-sprint-24-25-demo-driven-plan.md)
> **核心原则**: Demo 驱动 > 架构文档驱动 — 每个 Demo 是独立可交付物，反馈循环快，适合 Solo Dev
> **Phase**: 6 — Agent-as-Plugin (2026-07-15 ~ 至今, Phase 5 ✅ 收官)

## 元信息
- **版本**: 1
- **创建时间**: 2026-07-24T00:00:00+08:00
- **最后更新**: 2026-07-24T00:00:00+08:00
- **当前阶段**: phase-6a
- **治理节奏**: 2026-08-01 proposals/ 清理 → 2026-08-12 Sprint Review → 2026-08-19 Drift Gate

---

## 阶段定义

### Phase 6a: Sprint 24 — Demo 收尾与 TDK 骨架 (phase-6a)

**目标**: pdk_chat_demo v1 收尾 + pkm_temporal_demo PDK 骨架落地
**状态**: 🔄 进行中
**周期**: 2026-07-24 ~ 2026-08-05 (12 天, ~37h 容量)
**完成条件**:
  - [ ] `ctest -R pdk_chat` 全绿 (含新增 schema 校验 test case)
  - [ ] `ctest -R temporal` 全绿 (≥8 test cases)
  - [ ] `./pdk_chat_demo --mock` Session 持久化 + Budget 告警正常工作
  - [ ] `./pkm_temporal_demo --mock` 4 个演示场景全部 PASS
  - [ ] 8/1 前 proposals/ 清理完成
  - [ ] active-status.md 更新至 2026-08-05

#### 任务分类

| 分类ID | 名称 | 描述 | 优先级 |
|--------|------|------|:------:|
| `demo-chat-v1` | pdk_chat_demo v1 收尾 | Session/Budget 验证修复 + Schema 校验 | P0 |
| `demo-temporal-1a` | pkm_temporal_demo PDK 骨架 | ITemporalClient + Mock + CLI + pdk_entry (5 工具) | P0 |
| `demo-temporal-1b` | pkm_temporal_demo 项目 | Demo 项目 + 4 场景 + 测试 (unit + e2e) | P0 |
| `demo-temporal-1c` | pkm_temporal_demo CI 集成 | 根 CMake 更新 + docs + CI hook | P1 |
| `governance` | 治理节奏 | proposals/ 清理 + active-status.md 同步 | P1 |

#### 任务明细

| ID | 任务 | 分类 | 估时 | 优先级 | 依赖 |
|:---|------|------|:----:|:------:|------|
| T1 | pdk_chat_demo: Session 持久化验证 + Budget 告警修复 | `demo-chat-v1` | 4h | P0 | — |
| T2 | pdk_chat_demo: Schema 校验基础版 (拒绝错误格式输入) | `demo-chat-v1` | 4h | P0 | — |
| T3 | pkm_temporal_demo: PDK 骨架 (ITemporalClient + Mock + CLI + pdk_entry) | `demo-temporal-1a` | 10h | P0 | — |
| T4 | pkm_temporal_demo: Demo 项目 (main.cpp + 4 场景 + config.json) | `demo-temporal-1b` | 8h | P0 | T3 |
| T5 | pkm_temporal_demo: 测试 (unit + e2e mock, ≥8 test cases) | `demo-temporal-1b` | 6h | P0 | T3, T4 |
| T6 | proposals/ 清理: 归档 >3 月的 proposal 目录 | `governance` | 0.5h | P1 | — |
| T7 | active-status.md 同步: ctest/ASan/Phase 进度 | `governance` | 0.5h | P1 | T1-T5 |
| T8 | pkm_temporal_demo: CI 集成 + 根 CMake 更新 + docs | `demo-temporal-1c` | 3h | P1 | T5 |

> **合计**: ~36h, T8 可顺延至 Sprint 25 首日

---

### Phase 6b: Sprint 25 — Demo 扩展与平台化 (phase-6b)

**目标**: pdk_chat_demo v2 启动 + AgentForge 第 2 agent + 平台文档
**状态**: ⏳ 未开始
**前置阶段**: phase-6a
**周期**: 2026-08-05 ~ 2026-08-19 (14 天, ~44h 容量)
**完成条件**:
  - [ ] `examples/pdk_chat_demo` 支持 3 种 Agent Loop (React ✅ / PlanExecute / ForkJoin)
  - [ ] Code Review SKILL.md 通过 SkillInterpreter 隔离执行
  - [ ] `include/agenticdsl/pdk/README.md` 完成 (含 3 个 loop 示例)
  - [ ] AgentForge 第 2 个领域 agent 可独立运行
  - [ ] `.agent.md` 加载时有 schema 校验 (拒绝错误格式)
  - [ ] ADR-0042 状态不匹配已解决
  - [ ] Sprint Review Gate: ctest/ASan 数字验证通过

#### 任务分类

| 分类ID | 名称 | 描述 | 优先级 |
|--------|------|------|:------:|
| `demo-chat-v2` | pdk_chat_demo v2 扩展 | PlanExecute/ForkJoin DSL + SkillInterpreter + OTel 评估 | P0 |
| `platform` | PDK 平台化 | 开发者指南 + AgentForge 第 2 领域 agent | P0 |
| `architecture` | 架构对齐 | DSLValidator 增强 + ADR-0042 状态 + 服务化评估 | P1 |
| `governance` | 治理节奏 | Sprint Review + Drift Gate 准备 | P1 |

#### 任务明细

| ID | 任务 | 分类 | 估时 | 优先级 | 依赖 |
|:---|------|------|:----:|:------:|------|
| U1 | pdk_chat_demo: PlanExecuteLoop + ForkJoinLoop DSL 实现 | `demo-chat-v2` | 8h | P0 | Phase 6a |
| U2 | pdk_chat_demo: Code Review SKILL.md 集成 SkillInterpreter | `demo-chat-v2` | 6h | P0 | ADR-0055 V1 |
| U3 | PDK 开发者指南: `include/agenticdsl/pdk/README.md` (~2 页 + 3 loop 示例) | `platform` | 6h | P0 | — |
| U4 | AgentForge 第 2 个领域 agent: 验证 PDK 复用性 | `platform` | 8h | P1 | U3 |
| U5 | DSLValidator 增强: `.agent.md` schema 校验 (变量声明/非空/schema) | `architecture` | 6h | P1 | — |
| U6 | ADR-0042 状态对齐: C16 5 项决议正式批准或独立 ADR | `architecture` | 2h | P1 | — |
| U7 | pdk_chat_demo: OTel 集成评估 (POC 级别) | `demo-chat-v2` | 4h | P2 | — |
| U8 | Phase 6 服务化重启评估 (ADR-0050 §启动条件重检) | `architecture` | 2h | P1 | Phase 6a |

> **合计**: ~42h, U7/U8 可削减去保证 P0 项完成

---

## 架构 P0 缺口评估时间线

> 这些是架构文档反复强调的 P0 项, 当前 Sprint 不做, 明确**何时评估**:

| 缺口 | 当前状态 | Sprint 25 末评估 | 启动条件 |
|------|---------|:---:|------|
| `pdk_manifest()` | ❌ | ✅ 评估 | AgentForge 有 ≥2 个 agent 后需要机器可读 manifest |
| `AgentDescriptor` + `pdk_register_agent()` | ❌ | ✅ 评估 | pkm_temporal Phase 2 需要注册 agent |
| `CapabilityRegistry` | ❌ | ✅ 评估 | ≥3 个 agent plugin 存在时需要 discovery |
| Wasm 技术栈预研 | ❌ | ❌ 推迟 | Phase 6 服务化重启后 |

---

## 治理节奏检查点

| 日期 | 检查项 | 对应任务 | 产出 |
|------|--------|:---:|------|
| 2026-08-01 | proposals/ 清理 | T6 | 归档记录 |
| 2026-08-05 | Sprint 24 收官 | — | active-status.md 更新 |
| 2026-08-12 | Sprint Review | — | ctest/ASan 一致性验证 |
| 2026-08-19 | Drift Gate | U6 | ADR ↔ spec 映射 |

---

## 两个 Demo 完整演进路线

```
pdk_chat_demo:
  Sprint 24 (Phase 6a): v1 收尾 (Session + Budget + Schema)
      │
  Sprint 25 (Phase 6b): v2 启动 (PlanExecute/ForkJoin + SKILL.md + OTel 评估)
      │
  Sprint 26+: v2 完成 (OTel 集成 + Conformance + Wasm Agent)
      │
  AgentForge: 作为 PDK 消费方的参考实现

pkm_temporal_demo:
  Sprint 24 (Phase 6a): Phase 1a+b+c (PDK skeleton + demo + tests + CI)
      │
  Sprint 25 (Phase 6b): Phase 2 评估 (gRPC 直连可行性 + protobuf 构建成本)
      │
  Sprint 26+: Phase 2 实施 (gRPC client + pdk_register_agent)
      │
  PKGM 生产: 正式 Temporal 集成
```

---

## 风险矩阵

| 风险 | 概率 | 影响 | 缓解 |
|------|:---:|------|------|
| Solo dev 容量不足 (37h/44h 满载) | 中 | 任务顺延 | T8/U7 可剪切 |
| pkm_temporal Phase 1a popen 实现复杂 | 低 | +2h | 复用 `shell_tools` 已验证的 popen 模式 |
| SkillInterpreter 与 Code Review SKILL.md 集成意外复杂 | 中 | +4h | 先 mock-only 版本，真实隔离推迟到 Sprint 26+ |
| AgentForge 第 2 agent 设计时间超预期 | 中 | +6h | 缩小范围 (trivial domain, ≤4h 设计) |