# Sprint 23 Capacity Commitment — C20-Spike W2-W3 实施

> **commitment 日期**: 2026-07-16
> **commitment 范围**: Sprint 23 (2026-07-19 ~ 2026-08-01, 2 周)
> **commitment owner**: 项目负责人 (HydraForge 平台团队 lead)
> **commitment 接收方**: C20-Spike W2-W3 实施 (`openspec/changes/phase6-service-ification-v1/`)
> **目标**: 自动解锁 Stage Gate `Preq 4` (团队容量确认) + `Unlock 2` (Sprint 23 capacity commitment)
> **依赖**: Stage Gate 2026-07-18 PASS + ADR-0051 ✅ Approved (experimental) 在 W3 ship 后

---

## 一、commitment 内容

### 1.1 团队容量

**正式 commitment**: 1.5 工程师 × 2 周 = **22.5 人天** 无中断可用

| 角色 | 投入 | 主要职责 |
|------|------|---------|
| **Primary Engineer** (FT) | 1.0 × 2 周 = 14 人天 | G3 plugin 实现 + E2E 集成 + ADR-0051 §不变量验证 |
| **Reviewer** (50%) | 0.5 × 2 周 = 8.5 人天 | G1 plugin 实现 + 合约 review + Layer 1 review checklist + Layer 3 memo |
| **Total** | 1.5 × 2 周 | 22.5 人天 (无会议冲突, 无 oncall 轮值) |

### 1.2 时间窗口

```
Sprint 23 时间线:
┌─────────────────────────────────────────────────┐
│ Week 1 (2026-07-19 ~ 2026-07-25):               │
│   D1-2: G3 plugin (pdk/g3_knowledge_base/) 完整 │
│   D3-4: G3 unit tests + ctest 集成              │
│   D5  : G1 plugin (pdk/g1_coding_assistant/) 启动│
│ Week 2 (2026-07-26 ~ 2026-08-01):               │
│   D6-7: G1 plugin 完成 + E2E 集成 (G1→G3)       │
│   D8-9: Awkward pattern 3 层检测 + 5 triggers   │
│   D10 : ADR-0051 定稿 + onboarding doc          │
│   D11 : 完整 ship gate 验证 (ctest + ASan)     │
│   D12 : ADR-0051 → ✅ Approved + archive        │
└─────────────────────────────────────────────────┘
```

### 1.3 工作分配 (W2-W3 per ADR-0051 §估时)

| tasks.md 章节 | 内容 | 人天 | 角色 |
|--------------|------|------|------|
| §2 (G3 Plugin) | 15 tasks | 4 | Primary |
| §3 (G1 Plugin) | 12 tasks | 3 | Reviewer |
| §4 (E2E Integration) | 10 tasks | 2 | Primary |
| §5 (Awkward Pattern Detection) | 9 tasks | 2 | Reviewer |
| §6 (Escalation Trigger Monitoring) | 7 tasks | 2 | Primary (ToolCoordinator RAII) + Reviewer (audit events) |
| §7 (ADR-0051 Finalization) | 5 tasks | 1 | Primary |
| §8 (Onboarding Documentation) | 7 tasks | 1 | Reviewer |
| §9 (Complete Test Coverage) | 5 tasks | 2 | Primary |
| §10 (Ship Gate Verification) | 11 tasks | 0.5 | Both |
| §11 (Archive + ADR-0051 Flip) | 11 tasks | 0.5 | Primary |
| **Buffer / Risk** | — | **4.5** | Both |
| **Total** | 93 tasks | **22.5** | — |

---

## 二、Sprint 23 启动条件 checklist

### 2.1 启动前必须满足 (GATE)

```
[ ] 2026-07-18 Stage Gate 重新评估 PASS (per docs/handoff/2026-07-31-stage-gate-evaluation.md)
[ ] ADR-0050 §决策 / §启动条件 保持不动 (per Oracle Q6 Spike framing)
[ ] ADR-0051 状态仍为 🔍 Proposed (W3 D15 ship 时翻 ✅ Approved experimental)
[ ] ctest 72+N/72+N 零回归 (基线 + Sprint 22 累计 + W1 新增)
[ ] ASan 72+N/72+N 零回归 (无新增 use-after-scope / leak)
[ ] 工具名 knowledge_base/query (slash, ADR-0043) 全 codebase 验证
[ ] DECLARE_TOOL 不出现在 v1 合约 (复用 IToolRegistry::register_tool_function())
[ ] G3 ToolCategory::Execute + {Workflow} only (NOT ReadOnly)
```

### 2.2 启动会议议程 (2026-07-19 D1 上午)

```
1. [10min]  重读 tasks.md §1 W1 fix list 11/12 完成状态
2. [15min]  ADR-0051 §决策 / §不变量 / §Ship Gate 宣读
3. [15min]  tasks.md §2 G3 plugin D1-2 任务分配 (primary FT)
4. [10min]  tasks.md §3 G1 plugin D5 任务分配 (reviewer)
5. [10min]  风险 + 缓解 walkthrough (R1-R5 from audit)
6. [10min]  Q&A + 工具链 ready 确认 (build/ + compile_commands.json + LSP)
7. [5min]   Slack #hydraforge-phase6 channel 开通
```

---

## 三、风险 + 缓解 (继承 ADR-0051 §风险 + Sprint 23 特定)

| # | 风险 | 概率 | 影响 | 缓解 |
|---|------|:----:|:----:|------|
| **R1** | Sprint 23 中途被打断 (vacation / oncall / fire) | MED | HIGH | Buffer 4.5 人天 (20% 容量) + Reviewer 50% 可独立推进 G1 |
| **R2** | MockLLMProvider 单线程 vs ctest 并行 | MED | MED | tasks.md §2.7 硬性禁止 G3 内部调需审批 tool + 已有 audit 事件覆盖 |
| **R3** | G3 ≤30 行约束导致 golfed code | MED | MED | tasks.md §2.13 强制 CI wc -l 检查 + Layer 1 review 识别 |
| **R4** | ToolCoordinator RAII guard 误杀正常调用 | LOW | HIGH | tasks.md §6.6 5 单元测试 + E2E (4.7) 全链验证 |
| **R5** | Sprint 23 内团队扩充 (新增成员) | LOW | LOW | 不接受中途扩充, 维持 1.5 eng 恒定 |
| **R6** | W3 D14 ship gate 失败触发 Drift Kill | LOW | HIGH | W2 D10 E2E 早检 + W3 D11 ADR-0051 早定稿留缓冲 |

---

## 四、unlock 自动验证 (commitment 即时生效)

### 4.1 Stage Gate Preq 4 (团队容量确认)

**预期 verification**:
```bash
$ ./scripts/check-stage-gate-readiness.sh 2>&1 | grep "B4"
✅ B4: Sprint 23 commitment doc 存在 (2026-07-16) — 1.5 eng × 2 周 ✅
```

### 4.2 C20-Spike Unlock 2 (Sprint 23 capacity)

**预期 verification**:
```bash
$ ./scripts/check-stage-gate-readiness.sh 2>&1 | grep "C2"
✅ C2: Sprint 23 commitment doc 存在 (2026-07-16) — capacity committed ✅
```

### 4.3 Stage Gate 整体解锁 (2026-07-18 后)

**预期**:
```
Stage Gate 2026-07-18 Readiness Summary
========================================
Total checks: 15 (7 + 5 + 3)
✅ PASSED:   11  (Item 1, 2, 3 + 4, 5, 7 + Preq 1, 2, 3, 4, 5 + Unlock 2, 3)
⚠️  WARNINGS: 4   (Item 6 C19 触发 + Preq 4 已 PASS, 实际 0 WARNINGS 全部转 ✅)

⚠️  Overall: NEEDS REVIEW → READY  (2026-07-18 后)
```

---

## 五、commitment owner 签字

```
签字:  ___________________________
      (HydraForge 平台团队 lead)

日期:  2026-07-16

附注:
- 本 commitment 是项目内部资源分配, 非法律合同
- Sprint 23 期间如遇不可抗力, 提前 48h 通知并协商推迟至 Sprint 24
- Sprint 22 工作饱和问题 (per handoff/2026-07-31 §四) 已在 Sprint 22 收官后消化, Sprint 23 起始 buffer 健康
```

---

## 六、关联文档

| 文档 | 用途 |
|------|------|
| `docs/handoff/2026-07-31-stage-gate-evaluation.md` | Stage Gate 原始评估 + 7 项 Item 来源 |
| `docs/audits/2026-07-14-stage-gate-readiness.md` | Stage Gate readiness audit (8 ✅ + 4 🟡 + 3 ⚠️) |
| `openspec/changes/phase6-service-ification-v1/tasks.md` | C20-Spike 12 sections × 93 tasks |
| `docs/adr/adr-0050-phase6-strategic-evaluation.md` | Phase 6 战略 (保持不动) |
| `docs/adr/adr-0051-phase6-pdk-composition-spike.md` | C20-Spike 范围 + Ship Gate + 提升标准 |
| `scripts/check-stage-gate-readiness.sh` | 自动化 readiness 检查 (本 commitment 即时解锁 B4 + Unlock 2) |

---

**最后更新**: 2026-07-16 (项目负责人 commitment)
**生效日期**: 2026-07-16 (签字即生效, 不依赖 Sprint 23 启动)
**Sprint 23 启动**: 2026-07-19 (周一)
**Sprint 23 结束**: 2026-08-01 (周五)