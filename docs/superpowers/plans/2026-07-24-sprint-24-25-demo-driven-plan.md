# Sprint 24/25 Demo 驱动任务计划

> **创建日期**: 2026-07-24
> **驱动原则**: 以 `pdk_chat_demo`（已完工 80%）和 `pkm_temporal_demo`（设计阶段）两个 Demo 用例为优先级锚点，辅以架构/调研文档战略方向，对齐 GOVERNANCE.md 治理节奏
> **数据源**:
> - `examples/pdk_chat_demo/DESIGN.md` (784 行，v1 大部分完工)
> - `examples/pkm_temporal_demo/DESIGN.md` (716 行，v0.2 设计中)
> - `docs/architecture/agent-as-plugin-architecture-v1.2.md` (五层架构)
> - `docs/architecture/agent-evolution-pipeline.md` (四阶段进化)
> - `docs/architecture/application-layer-sota-positioning-v2.md` (SOTA 差距)
> - `docs/research/agentscope-comparison.md` (18 个可借鉴模式)
> - `docs/architecture/adr-implementation-status-gap-analysis.md` (差距分析)
> - `docs/GOVERNANCE.md` (治理节奏)
> - `docs/active-status.md` (当前看板)

---

## 一、优先级推导逻辑

### 从 Demo 倒推任务优先级

```
pdk_chat_demo 现状 ──→ v1 剩余缺口 ──→ Sprint 24 收尾
                   ──→ v2 规划 ──→ Sprint 25 启动

pkm_temporal_demo ──→ Phase 1a (PDK skeleton) ──→ Sprint 24 新任务
                  ──→ Phase 1b (demo + tests) ──→ Sprint 25 新任务

架构/调研文档 ──→ P0 缺口 (manifest/descriptor/capability) ──→ Sprint 25 评估
            ──→ P1 快速 win (schema/lazy/semver) ──→ Sprint 25 候选
```

### 为什么 Demo 驱动 > 架构文档驱动

| 维度 | Demo 驱动 | 架构文档驱动 |
|------|----------|------------|
| 风险 | 低 — 每个 Demo 是独立可交付物 | 高 — pdk_manifest/CapabilityRegistry 关联面大 |
| 用户感知 | 高 — Demo 可运行可演示 | 低 — 基础设施改动对用户不可见 |
| 反馈循环 | 快 — AgentForge 消费反馈 | 慢 — 需等上层应用接入 |
| 适合 Solo Dev | ✅ 每项 ~2-4 天 | ⚠️ 部分需跨模块重构 |

---

## 二、输入全景

### 2.1 pdk_chat_demo — 当前完成度评估

| v1 检查项 (DESIGN §十五) | 状态 | 备注 |
|---|---|---|
| `ctest -R pdk_chat` 全绿 | ✅ | 8 test cases, 34 assertions |
| 6 个 Plugin 加载成功 | ✅ | Chat/Loop/Provider/Session/Budget/FS/Shell |
| MockLLMProvider —mock 模式 | ✅ | 端到端可用 |
| 真实 LLM (DeepSeek v4) | ✅ | `--live` 模式可用 |
| 事件流 + 终端输出 | ✅ | 结构化时间戳日志 |
| Session 持久化 (JSONL) | ⚠️ | session_agent 骨架存在，待验证 |
| Budget 累计 + 告警 | ⚠️ | budget_agent 骨架存在，待验证 |
| Schema 校验 (错误输入拒绝) | ❌ | 未实现 |
| Code Review SKILL.md 隔离 | ❌ | 需 ADR-0055 SkillInterpreter |
| Conformance Level 1 | ❌ | 未定义 manifest 校验规则 |

> **估算 v1 完成度: ~80%**。6/10 项彻底完成，2 项骨架存在待验证，2 项未实现。

**v1 剩余缺口 → Sprint 24**:
- G1: Session 持久化验证 + 修复 (受 budget_agent 影响)
- G2: Schema 校验基础版 (拒绝错误输入)
- G3: Code Review SKILL.md 集成 SkillInterpreter

**v2 扩展 (DESIGN §十一) → Sprint 25**:
- PlanExecuteLoop / ForkJoinLoop DSL
- OTel 集成
- Conformance Test Suite

### 2.2 pkm_temporal_demo — 设计就绪，待实施

**Phase 1a: PDK Plugin 骨架 (1-2 天)**:
| 任务 | 产出 |
|------|------|
| `pdk/temporal_agent/` 目录 + CMake | 编译目标 |
| `ITemporalClient` 抽象接口 | `include/temporal_agent.h` |
| `MockTemporalClient` | mock 应答生成器 |
| `TemporalCLIClient` | popen 封装 (复用 shell_tools 模式) |
| `pdk_entry.cpp` (5 工具注册) | `temporal/{start_workflow, start_async, poll, signal, query}` |
| `pdk_manifest.json` | manifest 文件 |

**Phase 1b: Demo 项目 + 测试 (1-2 天)**:
| 任务 | 产出 |
|------|------|
| `examples/pkm_temporal_demo/` 项目 | main.cpp + config.json |
| 4 演示场景 | Blocking / Async+Poll / Idempotency / Query |
| 单元测试 | test_metadata + test_tools_mock |
| 端到端测试 | test_temporal_e2e_mock |

**Phase 1c: CI 集成 (0.5 天)**

### 2.3 架构文档 — 按 Demo 关联度筛选

| 架构 P0 项 | pdk_chat_demo 关联 | pkm_temporal_demo 关联 | Sprint 建议 |
|---|---|---|---|
| `pdk_manifest()` | ⚠️ 硬编码 plugin 路径 | ⚠️ pdk_manifest.json 已写，但无解析器 | Sprint 25 候选 |
| `AgentDescriptor` | ❌ 未使用 `pdk_register_agent` | ❌ 推迟到 Phase 2 | Sprint 25 评估 |
| `CapabilityRegistry` | ❌ 未使用 | ❌ 未使用 | Sprint 25 评估 |
| DSLValidator | ⚠️ `.agent.md` 无 schema 校验 | N/A | Sprint 25 候选 |

### 2.4 调研文档 — 直接可借鉴的模式

| 模式 | 来源 | 当前 Sprint 适用性 |
|------|------|------|
| **Agent as Tool** | AgentScope #16 | `pdk_chat_demo` v2 可引入 `register_agent_as_tool()` |
| **Tool Offloading** | AgentScope #14 | SafeExec 重写参考 (`std::async`→`std::jthread`) |
| **Middleware 洋葱模型** | AgentScope #3 | PDK 生产化的 Decorator 链已有，可文档化 |

### 2.5 GOVERNANCE 节奏约束

| 日期 | 事件 | 对 Sprint 的影响 |
|------|------|------|
| **2026-08-01** (8 天后) | proposals/ 清理 | 需留 30 分钟执行归档 |
| 2026-08-12 | Sprint Review | ctest/ASan 数字验证 |
| 2026-08-19 | Drift Gate | ADR ↔ spec REQ 映射 |

---

## 三、Sprint 24 任务计划 (2026-07-24 ~ 2026-08-05)

> **剩余 12 天** (原 Sprint 24 周期 07-15~07-29 已过半，延长至 08-05 以覆盖新任务)
> **容量**: Solo dev ~22h/周 × 1.7 周 ≈ 37h

| # | 任务 | 驱动来源 | 估时 | 优先级 | 依赖 |
|---|------|---------|:----:|:------:|------|
| **T1** | **pdk_chat_demo v1 收尾** — Session 持久化验证 + Budget 告警修复 | DESIGN §十五 G1 | 4h | 🔴 P0 | — |
| **T2** | **pdk_chat_demo v1 收尾** — Schema 校验基础版 (拒绝错误格式输入) | DESIGN §十五 | 4h | 🔴 P0 | — |
| **T3** | **pkm_temporal_demo Phase 1a** — PDK 骨架 (ITemporalClient + Mock + CLI + pdk_entry) | DESIGN §十三 | 10h | 🔴 P0 | — |
| **T4** | **pkm_temporal_demo Phase 1b** — Demo 项目 (main.cpp + 4 场景 + config.json) | DESIGN §十三 | 8h | 🔴 P0 | T3 |
| **T5** | **pkm_temporal_demo Phase 1b** — 测试 (unit + e2e mock) | DESIGN §十二 | 6h | 🔴 P0 | T3, T4 |
| **T6** | **8/1 proposals/ 清理** — 归档 >3 月的 proposal 目录 | GOV §Step1 | 0.5h | 🟡 P1 | — |
| **T7** | **active-status.md 同步** — 更新 ctest/ASan/Phase 进度 | GOV §Step5 | 0.5h | 🟡 P1 | T1-T5 |
| **T8** | **pkm_temporal_demo Phase 1c** — CI 集成 + 根 CMake 更新 + docs | DESIGN §十三 | 3h | 🟡 P1 | T5 |

> **合计: ~36h**，接近容量上限。T8 可顺延至 Sprint 25 首日。

### Sprint 24 完成定义

- [ ] `ctest -R pdk_chat` 全绿 (含新增 schema 校验 test case)
- [ ] `ctest -R temporal` 全绿 (≥8 test cases)
- [ ] `./pdk_chat_demo --mock` Session 持久化 + Budget 告警正常工作
- [ ] `./pkm_temporal_demo --mock` 4 个演示场景全部 PASS
- [ ] 8/1 前 proposals/ 清理完成
- [ ] active-status.md 更新至 2026-08-05

---

## 四、Sprint 25 任务计划 (2026-08-05 ~ 2026-08-19)

> **14 天**，容量 ≈ 44h
> **前置**: Sprint 24 全部 T1-T8 完成

| # | 任务 | 驱动来源 | 估时 | 优先级 | 依赖 |
|---|------|---------|:----:|:------:|------|
| **U1** | **pdk_chat_demo v2: PlanExecuteLoop + ForkJoinLoop DSL** | DESIGN §十一 | 8h | 🔴 P0 | Sprint 24 |
| **U2** | **pdk_chat_demo v2: Code Review SKILL.md 集成 SkillInterpreter** | DESIGN §十五 | 6h | 🔴 P0 | ADR-0055 V1 |
| **U3** | **PDK 开发者指南** — `include/agenticdsl/pdk/README.md` (~2 页贯穿示例 + 3 loop 模式) | active-status Sprint 25 | 6h | 🔴 P0 | — |
| **U4** | **AgentForge 第 2 个领域 agent** — 验证 PDK 复用性 | active-status Sprint 25 | 8h | 🟡 P1 | U3 |
| **U5** | **DSLValidator 增强** — `.agent.md` schema 校验 (变量声明/非空/schema) | 进化管线 §8.2 | 6h | 🟡 P1 | — |
| **U6** | **ADR-0042 状态对齐** — C16 5 项决议正式批准或独立 ADR | 差距分析 §4.1 | 2h | 🟡 P1 | — |
| **U7** | **pdk_chat_demo v2: OTel 集成评估** (POC 级别) | DESIGN §十一 | 4h | 🟢 P2 | — |
| **U8** | **Sprint 24 末决策** — 评估 Phase 6 服务化是否重启 | ADR-0050 | 2h | 🟡 P1 | Sprint 24 |

> **合计: ~42h**。U7 和 U8 为可选，可削减去保证 P0 项完成。

### Sprint 25 完成定义

- [ ] `examples/pdk_chat_demo` 支持 3 种 Agent Loop (React ✅ / PlanExecute / ForkJoin)
- [ ] Code Review SKILL.md 通过 SkillInterpreter 隔离执行
- [ ] `include/agenticdsl/pdk/README.md` 完成 (含 3 个 loop 示例)
- [ ] AgentForge 第 2 个领域 agent 可独立运行
- [ ] `.agent.md` 加载时有 schema 校验 (拒绝错误格式)
- [ ] ADR-0042 状态不匹配已解决
- [ ] Sprint Review Gate: ctest/ASan 数字验证通过

---

## 五、两个 Demo 的完整演进路线

### 5.1 pdk_chat_demo 路线

```
Sprint 24: v1 收尾 (Session + Budget + Schema)
    │
Sprint 25: v2 启动 (PlanExecute/ForkJoin + SKILL.md + OTel 评估)
    │
Sprint 26+: v2 完成 (OTel 集成 + Conformance + Wasm Agent)
    │
AgentForge: 作为 PDK 消费方的参考实现
```

### 5.2 pkm_temporal_demo 路线

```
Sprint 24: Phase 1a+b+c (PDK skeleton + demo + tests + CI)
    │
Sprint 25: Phase 2 评估 (gRPC 直连可行性 + protobuf 构建成本)
    │
Sprint 26+: Phase 2 实施 (gRPC client + pdk_register_agent)
    │
PKGM 生产: 正式 Temporal 集成
```

---

## 六、架构 P0 缺口的时间线评估

这些是架构文档反复强调的 P0 项，但当前 Sprint 不做，而是明确**何时评估**：

| 缺口 | 当前状态 | Sprint 25 末评估 | 启动条件 |
|------|---------|:---:|------|
| `pdk_manifest()` | ❌ | ✅ 评估 | AgentForge 有 ≥2 个 agent 后需要机器可读 manifest |
| `AgentDescriptor` + `pdk_register_agent()` | ❌ | ✅ 评估 | pkm_temporal Phase 2 需要注册 agent |
| `CapabilityRegistry` | ❌ | ✅ 评估 | ≥3 个 agent plugin 存在时需要 discovery |
| Wasm 技术栈预研 | ❌ | ❌ 推迟 | Phase 6 服务化重启后 |

---

## 七、治理节奏检查点

| 日期 | 检查项 | 负责人 | 产出 |
|------|--------|:---:|------|
| 2026-08-01 | → proposals/ 清理 | T6 | 归档记录 |
| 2026-08-05 | Sprint 24 收官 | — | active-status.md 更新 |
| 2026-08-12 | Sprint Review | — | ctest/ASan 一致性验证 |
| 2026-08-19 | Drift Gate | — | ADR ↔ spec 映射 |

---

## 八、风险矩阵

| 风险 | 概率 | 影响 | 缓解 |
|------|:---:|------|------|
| Solo dev 容量不足 (37h/44h 满载) | 中 | 任务顺延 | T8/U7 可剪切 |
| pkm_temporal Phase 1a popen 实现复杂 | 低 | +2h | 复用 `shell_tools` 已验证的 popen 模式 |
| SkillInterpreter 与 Code Review SKILL.md 集成意外复杂 | 中 | +4h | 先 mock-only 版本，真实隔离推迟到 Sprint 25+ |
| AgentForge 第 2 agent 设计时间超预期 | 中 | +6h | 缩小范围 (trivial domain, ≤4h 设计) |

---

## 九、与 AGENTS.md 的同步项

Sprint 25 收官时需在 AGENTS.md 追加以下 ship 记录：

- pdk_chat_demo v1 收尾 (Schema 校验 + Session/Budget 修复)
- pkm_temporal_demo Phase 1 ship (ITemporalClient + Mock + 5 tools + 8 tests)
- DSLValidator 增强
- ADR-0042 状态对齐

---

**下一步**: 基于本计划开始 Sprint 24 任务实施，或先讨论优先级调整。