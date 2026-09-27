# l2-evolution-deferred-follow-up — Proposal

> **Change Slug**: `l2-evolution-deferred-follow-up`
> **Status**: 🔍 Proposed
> **Created**: 2026-09-24
> **Updated**: 2026-09-25 (supersedes evidence-gate real-verdict question, see note)
> **依赖**: [`pdk-chat-demo-evolution-reference-example`](../archive/2026-09-24-pdk-chat-demo-evolution-reference-example/) (L2 ✅ shipped)

> **🔄 Supersedes (2026-09-25, per Oracle ses_f2b923412ffeTFfBdDqQFOMdT9)**:
> The 2026-08-25 evidence-gate MOMUS decision summary (`docs/audits/2026-08-25-evidence-gate-momus-decision-summary.md`) raised an open question on "real LLM data verdict for evaluate_gate". This change's **Phase 1 (R8 full metrics)** — specifically `--release-metrics` with real IEvaluator V2 wiring and `--ablation-mode=full` with attribution_verdict distributions — **owns the "real verdict" question going forward**. The evidence-gate worktree (`openspec/from-roadmap-phase-6c-evidence-gate`) was verified 100% merged into main (`main..HEAD = 0 commits`, 2026-09-25); no separate R8 implementation under evidence-gate should be created.

---

## Why (动机)

### 当前问题

L2 change `pdk-chat-demo-evolution-reference-example` (archived 2026-09-24) 已完成 6-phase demo + 9 test binaries + 39 cases 的 reference example。但 L2 设计文档 (design.md) 明确标识了 **7 项 deferred 事项**，各因 scope/schedule/infra 原因排除在 L2 范围外。这些事项形成项目后续演进的 actionable backlog：

1. **R9.3 真 sandbox 隔离未实现**：L2 仅做 `turn_input` keyword-rejection 降级版。真 sandbox `network_mode=none` 因 `docker_backend.cpp:186` hardcoded `"NetworkMode", "bridge"` 物理不可行 —— 需 infra 层支持（独立 Wave 4）。
2. **R8 全量 metrics 未完整端到端**：L2 的 `--release-metrics` 仅 stub（`drop_ratio=0%`），`--ablation-mode=full` 无真实输出，`--failure-event-format=v2` 未实现。需要 IEvaluator V2 接线后才能产生有意义的回归度量。
3. **workflow_patch V1 (`Case 1.5`) 未 ship**：`harness-rsi-pilot/spec.md` Scenario "workflow_patch NOT supported" + L2 design D6 明确 deferred。需要独立 Wave 3 Phase 2 D4-D7 立项。
4. **V2 缺口闭环 (load(genome@N) → 重建 ChatSession → 1 turn) 未完整**：L2 Phase 4 reload_rerun 在 mock mode 下 stub 通过，但真实 `IGenomeRegistry::load -> to_agent_config -> ChatSession 11-param ctor` 路径未在大批量 mutation 后验证。
5. **≥5 baseline samples 统计断言未支持**: BehavioralEquivalence.compare() 在 kMinBaselineSamples=5 限制下返回 Insufficient/NotAttempted —— L2 诚实标记为 deferred。
6. **S4 Agent-Agent 协同进化未启动**: 纯 research 路径，不在当前实施 roadmap 上。

### 现状证据

- L2 design.md §D6, §D7, §D10, §R8, §R9.3: 全部标记 "deferred / not L2 scope"
- L2 design.md 630: "真 sandbox 物理不可实现, defer to Wave 4"
- L2 design.md 682-683: "真实回归度量不属 L2 demo 范围"
- L2 design.md 358: "workflow_patch 完整化随 Wave 3 Phase 2 立项"
- L2 spec.md R9.3: "Deferred to Wave 4 (independent OpenSpec change)"
- `docs/architecture/rsi-architecture-2026-09.md` §11.8: 反作弊三模式红线
- `AGENTS.md` Reverse Indicator Rule: R8.1 drop_ratio > 5% 自动 block (机制需全量 metrics)

---

## What (实施内容)

### 1. Wave 4: 真实 sandbox network isolation for R9.3

- 在 `docker_backend.cpp` 或兄弟 layer 加 `network_mode` 配置项（当前 hardcoded `"bridge"`）
- L2 的 `test_anti_cheat_sandbox_escape` 从 keyword-rejection 升级为真沙箱隔离验证
- 反向指标门 R8 配套：真 sandbox 拦截率加入 ablation_report Segment 3

### 2. R8 全量 metrics (--release-metrics / --ablation-mode=full / --failure-event-format=v2)

- `main.cpp` R8 flags 从 stub 升级为真正实现：
  - `--release-metrics`: 调用 IEvaluator V2 打标各 ContextRequest 的 attribution_verdict，计算 drop_ratio
  - `--ablation-mode=full`: 3 段对照写入 ablation_report.json（依赖 ≥5 samples + 不同 Harness 配置）
  - `--failure-event-format=v2`: 加入 failure_event context_id 链
- 配套 test_anti_cheat* 测试 infra 升级

### 3. workflow_patch V1 (Case 1.5)

- 随 Wave 3 Phase 2 立项的独立 change
- 解锁 harness_rsi 的 `workflow_patch` mutation 变体
- 5-tier gate 增加 G4.5 workflow_patch 专项校验

### 4. Case 1.5 / R13 full e2e (ChatSession 完整接线)

- evolution_session::phase4_reload_rerun 从 mock stub 升级为真 IGenomeRegistry::load + to_agent_config + ChatSession 重建
- IDistillationWriter 捕获 Training-mode Session JSONL
- IEvaluator V2 输出真实的 BehavioralEquivalence compare 值

### 5. S4 Agent-Agent 协同进化 (research)

- 纯 research 路径 —— 定义问题空间 + 相关 work survey
- 不纳入当前 sprint 周期

---

## Impact (影响评估)

| 维度 | 评估 |
|------|------|
| N1 (main.cpp zero diff) | ✅ 保持，不修改 examples/pdk_chat_demo/main.cpp |
| N2 (include/ zero diff) | ⚠️ R9.3 sandbox infra 可能需 contract 层微调 (独立审核) |
| N5 (Contract layer freeze) | ⚠️ Wave 4 sandbox 可能需扩展 docker_backend.h 契约 |
| 9 L2 test binaries | ✅ 向后兼容，不重新红线 |
| 反向指标门 R8 | ✅ 从机制演示升级为真实回归度量 |
| 增量测试 | +3~5 新 test binaries |
| timeline | Wave 4 sandbox: 独立立项；R8 full: Sprint 37+；workflow_patch: Wave 3 Phase 2 |

---

## Reverse Indicator (R8 per AGENTS.md)

```
[Reverse Indicator]
+ new_up: 4 deferred items documented as actionable follow-up change;
          R9.3 sandbox infra path defined; R8 metrics upgrade path defined
- old_down: drop_ratio=0% (纯文档 + OpenSpec 注册, 无既有能力退化)
failure_traces: N/A (文档 commit)
ablation: N/A (文档 commit)
context_ids: N/A (文档 commit, 无 ContextRequest)
```
---

## 📦 Cancellation Note (2026-09-25)

**STATUS**: ❌ **CANCELLED / SUPERSEDED** (same day creation)

**Reason**: Per DECISION C (`wave-3-phase-2-d4-lora-pipeline`, commit `65ff2dd`),
Phase 3 (Wave 3 Phase 2 D4-D7) of this change is now better tracked as a
separate OpenSpec change with proper 24h cooling-off per AC-12.

**Other phases disposition**:
- **Phase 1 (R8 full metrics)**: Will be separate change when Sprint 37+ starts
  (per Oracle ses_f2b923412ffeTFfBdDqQFOMdT9 DECISION B sequence)
- **Phase 2 (Wave 4 sandbox)**: Deferred to Wave 4 (infra-heavy, requires ADR)
- **Phase 3 (Wave 3 Ph2 D4-D7)**: **Superseded by `wave-3-phase-2-d4-lora-pipeline`
  (DECISION C, commit 65ff2dd, archived as 2026-09-25-wave-3-phase-2-d4-lora-pipeline)**
- **Phase 4 (S4 research)**: Indefinite (research path)

**Supersede note** (evidence-gate R8 ownership) preserved at top of this proposal —
no separate R8 implementation under evidence-gate should be created.
