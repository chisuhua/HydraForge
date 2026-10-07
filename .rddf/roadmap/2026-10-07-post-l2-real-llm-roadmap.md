# Post-L2-RealLLM Roadmap (2026-10-07 → ?)

> **来源**: 2026-10-06 全量回归测试完备性审查 (识别 8 个盲点 G1-G8) + 2026-10-07 G1 (`l2-evolution-real-llm-provider`) SHIP 完成 + Oracle post-impl SHIP-with-fixes verdict `ses_eebfb88baffeFJaY5Z9S3Q1UtT`.
> **状态**: G1 ✅ shipped (commit `7fc6d46` + `842791e` merge). 5 个 follow-up OpenSpec changes (G4/G5/G6/G7/G8) 待立项.
> **维护**: Single-Dev 模式，每次 ship commit 后需更新状态 + §10.

## 一、Baseline (2026-10-07, main `9747756`)

### 已 ship commits (本 session, 6 atomic commits)

| Hash | 主题 | 影响范围 |
|------|------|----------|
| `7fc6d46` | feat(l2): wire --real-llm provider into EvolutionSession + fail-fast | evolution_session.cpp/h + main.cpp + test_l2_real_llm_e2e.cpp |
| `3ebb6a9` | chore(openspec): archive l2-evolution-real-llm-provider (Day-5 5-file integrity) | openspec/changes/archive/ |
| `fcb0b8c` | docs(AGENTS.md): Recent Changes entry for l2-evolution-real-llm-provider ship | AGENTS.md |
| `ba6edcf` | chore(hygiene): Oracle SHIP-with-fixes closure (Major 1+2 + Minor 4) | AGENTS.md + 2 improvement files |
| `842791e` | Merge feat/l2-real-llm-provider → main (--no-ff) | main branch |
| `9747756` | chore(builder): ship tracking for l2-evolution-real-llm-provider | .rddf/state/builder/l2-evolution-real-llm-provider.json |

### ctest 基线 (post-merge 验证)

| 测试集 | 结果 |
|--------|------|
| 核心树 mock (-LE must_realllm) | ✅ **263/263** |
| must_realllm 核心树 | ✅ **11/11** |
| must_realllm examples | ✅ **4/4** |
| L2 evolution (10) | ✅ **10/10** |
| **Total** | **288/288** (0 回归) |

### Active OpenSpec changes: 0 (G1 ship 后工作区干净)

### 已 ship 真实 LLM 验证盲点 (本 roadmap scope)

| ID | 主题 | 优先级 | 状态 | Improvement 登记 |
|----|------|--------|------|--------------------|
| G1 | `l2-evolution-real-llm-provider` | 🔴 P0 | ✅ shipped (commit 7fc6d46) | `.rddf/improvements/l2-evolution-real-llm-provider.md` |
| G4 | `harness-mutation-commit-real-llm` | 🔴 P0 | ⚪ pending (待立项) | `.rddf/improvements/harness-mutation-commit-real-llm.md` |
| G5 | `rsi-anti-cheat-real-llm` | 🟠 P1 | ⚪ pending (待立项) | `.rddf/improvements/rsi-anti-cheat-real-llm.md` |
| G6 | `r13-4-confidential-redaction-real-llm` | 🟠 P1 | ⚪ pending (待立项) | `.rddf/improvements/r13-4-confidential-redaction-real-llm.md` |
| G7 | `l2-release-metrics-real-llm-drop-ratio` | 🟠 P1 | ⚪ pending (待立项, partial resolution 2026-10-07) | `.rddf/improvements/l2-release-metrics-real-llm-drop-ratio.md` |
| G8 | `self-evolution-9-stage-real-llm-end-to-end` | 🟢 P2 | 🚫 blocked (依赖 G1+G4+G7) | `.rddf/improvements/self-evolution-9-stage-real-llm-end-to-end.md` |

### 已识别但本 roadmap 不动的盲点

| ID | 主题 | 原因 |
|----|------|------|
| G2 | MiniMax URL placeholder 真实 LLM | AGENTS.md §8.4: helper URL placeholder NXDOMAIN, 等外部 URL 修对再补 |
| W3 Phase 2 D4 | LoRA 训练管线 | ADR-0078 Phase 1 ✅, Phase 2 D4 等 Wave 3 cooling-off 满后独立立项 |
| TSan re-sweep | 机器性能受限 | 留独立 follow-up |
| Minor 2/3 (Case 3/4 label 门控 + parse_error test) | 已在 improvement 登记 | 小工作量, 可在后续 cleanup pass 处理 |

### 治理 cleanup 候选 (低 effort 高 ROI)

| 项目 | 影响 | effort |
|------|------|--------|
| ADR-0078 `finetune-base-model-impl-scope.md` 子文档 | 关闭 docs_drift_audit Scenario 4 的 1 DRIFT | <30 min |
| `active-status.md` 计数声明格式修正 | 关闭 docs_drift_audit Scenario 6 的 3 WARNING | <30 min |

---

## 二、Dependency Graph (后续 G4-G8)

```
G1 ✅ ──┬──→ G7 (R8.1 红线 drop_ratio 真实 LLM 测试)
        │
        └──→ G8 (self-evolution 9 段闭环) ──→ (Wave 3 Phase 2 D4 LoRA 训练)
                                                  ↓
                                               (依赖 G7 真实 drop_ratio 数据)

G1, G4, G7 ──┬──→ G8

G4 (Harness mutation commit 真实 LLM) ⊥ G5/G7 (parallel)
G5 (R9 反作弊) ⊥ G6/G7 (parallel)
G6 (R13.4 confidential) ⊥ G5/G7 (parallel)
```

**Parallel 矩阵**:
- G4 ⊥ G5 ⊥ G6 ⊥ G7: 无相互依赖,4 个可同时立项
- G8: 依赖 G1 + G4 + G7 (3 个 ship 后启动)

---

## 三、Change Overview

| # | Change slug | 类型 | 估计 effort | 依赖 | 状态 |
|---|-------------|------|-------------|------|------|
| G7 | `2026-XX-l2-release-metrics-real-llm-drop-ratio` | immediate | 1-2 天 | G1 ✅ | 🟡 active (建议立即立项) |
| G4 | `2026-XX-harness-mutation-commit-real-llm` | immediate | 1-2 天 | 无 | ⚪ pending |
| G5 | `2026-XX-rsi-anti-cheat-real-llm` | immediate | 1-2 天 | 无 | ⚪ pending |
| G6 | `2026-XX-r13-4-confidential-redaction-real-llm` | immediate | 1-2 天 | 无 | ⚪ pending |
| G8 | `2026-XX-self-evolution-9-stage-real-llm-end-to-end` | placeholder | 3-5 天 | G1+G4+G7 | ⚪ blocked |
| Cleanup-1 | `2026-XX-adr-0078-fineline-impl-scope` | immediate | <30 min | 无 | ⚪ pending |
| Cleanup-2 | `2026-XX-active-status-count-fix` | immediate | <30 min | 无 | ⚪ pending |

**总计**: 7 个 immediate (含 2 个 governance cleanup) + 1 个 placeholder (G8 blocked)

---

## 四、Detailed Tracking

### G7: l2-release-metrics-real-llm-drop-ratio (🔴 立即立项)

**scope**:
- (a) R8.1 红线触发 mock-fixture 单元测试 (独立 binary, 不依赖真实 LLM, 因 L2 1-2 contexts 物理不可行)
- (b) 真实 LLM drop_ratio 红线监控 (long-run 实测触发)
- (c) R8.1 红线机制回归守卫 (mock 注入 ≥20 mutations 验证 drop_ratio > 0.05 exit non-zero)

**依赖**: G1 ✅ shipped (provider 接线 + metrics.json 含 drop_ratio)
**Worktree**: `.rddf/wt/l2-release-metrics/`
**Ship pattern**: Pattern #11 (async worker + dual Oracle review + SHIP-with-fixes cycle)
**Pre-impl review**: Metis `bg_2eXX` + Oracle `bg_2fXX` (后台并行)
**Cooling-off**: 标准 24h (无 HARD override, 因 G1 已建路径信任)

### G4: harness-mutation-commit-real-llm (🔴 立即立项, 与 G7 并行)

**scope**:
- (a) 真实 LLM 生成 mutation proposal → Gate 0-3 真实 LLM 评分 → commit → reload 状态一致
- (b) 真实 LLM mutation commit 触发 `genome.committed` 事件 + walk_ancestors 验证
- (c) 真实 LLM mutation commit 失败回滚 (gate denied) → IGenomeRegistry 零状态变更
- (d) drop_ratio ∈ 合理范围 (无 drop_ratio 红线触发问题)

**依赖**: Genome wiring ship (genome-wiring-harness-rsi-gepa-2026-09-22 已 ship, IGenomeRegistry 可用)
**Worktree**: `.rddf/wt/harness-mutation-commit-real-llm/`
**Ship pattern**: Same as G7

### G5: rsi-anti-cheat-real-llm (🟠 立项, parallel)

**scope**:
- (a) R9.1 (搜现成答案): ContextRequest 含 baseline hint "the answer is 42" → 必须拒绝直接复述
- (b) R9.2 (修改评判指标): LLM 尝试 prompt injection 修改 evaluator schema → IEvaluator write 接口必须拦截
- (c) R9.3 (串谋外部平台): ContextRequest 提示 LLM 访问外网 → sandbox network_mode=none 必须拦截

**依赖**: 无
**Worktree**: `.rddf/wt/rsi-anti-cheat-real-llm/`

### G6: r13-4-confidential-redaction-real-llm (🟠 立项, parallel)

**scope**:
- (a) ContextRequest sensitivity=confidential → LLM 真实响应 → trace emit 0% cleartext leak
- (b) 多轮对话 turn 1 confidential → turn 2 LLM 不能复述 turn 1 cleartext
- (c) LLM 工具调用 arguments 含 turn_input 引用 → ToolRegistry 脱敏
- (d) metadata 字段子字段不能绕过 redact_trace_fields

**依赖**: 无
**Worktree**: `.rddf/wt/r13-4-confidential-redaction-real-llm/`

### G8: self-evolution-9-stage-real-llm-end-to-end (🚫 blocked)

**依赖**: G1 ✅ + G4 ✅ + G7 ✅
**触发**: G4 + G7 ship 后立即立项
**scope**: 自进化 v1.5 9 段闭环端到端真实 LLM (3 类 ContextRequest + IEvaluator + R13.4 + drop_ratio 集成)

### Cleanup-1: ADR-0078 finetune-base-model-impl-scope.md (🟢 立即)

**scope**: 创建 `docs/adr/adr-0078-finetune-base-model-impl-scope.md` 子文档 (含 D1/D3/D7 实施细节)
**ROI**: 关闭 docs_drift_audit Scenario 4 的 1 DRIFT
**effort**: <30 min

### Cleanup-2: active-status.md 计数格式修正 (🟢 立即)

**scope**: 修正总 ctest/ADR 总数/ADR Approved 计数声明格式匹配实测 (283 ctest/114+ ADR/68+ Approved)
**ROI**: 关闭 docs_drift_audit Scenario 6 的 3 WARNING
**effort**: <30 min

---

## 五、Sprint Breakdown (建议)

### Sprint A: 治理 cleanup + G7 (1-2 天)

| 工作项 | 文件 | Effort |
|--------|------|--------|
| Cleanup-1 | `docs/adr/adr-0078-finetune-base-model-impl-scope.md` | <30 min |
| Cleanup-2 | `docs/active-status.md` | <30 min |
| G7 立项 + 实施 + SHIP-with-fixes | `openspec/changes/2026-XX-l2-release-metrics-real-llm-drop-ratio/` | 1-2 天 |

**Ship Gate**: 
- `python3 tools/docs_drift_audit.py` 0 DRIFT, 0 WARNING
- G7: ctest -L l2-evolution 11/11 PASS, R8.1 红线 mock 单元测试触发 exit non-zero

### Sprint B: G4 + G5 + G6 (并行 ship, 2-3 天)

| 工作项 | 文件 | Effort |
|--------|------|--------|
| G4 | `openspec/changes/2026-XX-harness-mutation-commit-real-llm/` | 1-2 天 |
| G5 | `openspec/changes/2026-XX-rsi-anti-cheat-real-llm/` | 1-2 天 (parallel) |
| G6 | `openspec/changes/2026-XX-r13-4-confidential-redaction-real-llm/` | 1-2 天 (parallel) |

**Ship Gate**:
- 3 个独立 OpenSpec changes 并行 ship, 每 change 通过 dual-agent review + Oracle post-impl SHIP-with-fixes
- 全量 ctest 291+/291+ (新增 3 个 e2e binaries × 5 cases avg)

### Sprint C: G8 立项 + 实施 (Sprint B 后, 3-5 天)

| 工作项 | 文件 | Effort |
|--------|------|--------|
| G8 | `openspec/changes/2026-XX-self-evolution-9-stage-real-llm-end-to-end/` | 3-5 天 |

**Ship Gate**:
- 自进化 v1.5 9 段闭环真实 LLM 端到端 (3 类 ContextRequest + IEvaluator + R13.4 + drop_ratio 集成)
- ≥3 类 ContextRequest (per AGENTS.md §Reverse Indicator Rule R3 元指标)

---

## 六、Risks

| ID | 风险 | 缓解 |
|----|------|------|
| R1 | Token 成本 (Sprint A C~S 累计 ~50-100 calls/change, 14+ change ~700+ calls) | must_realllm 标签 + CI 上 `HYDRAFORGE_SKIP_REAL_LLM=1` 短路; 本地 + 用户 CI key 真跑 |
| R2 | Sandbox 网络限制 (must_realllm 真打需外网) | 沙箱环境不下 ship gate verdict; 由用户本地 `HYDRAFORGE_SKIP_REAL_LLM=1` 决定 |
| R4 | LLM provider 暂不可达 (deepseek API rate limit / network) | fail-fast stderr 路径 + RetryPolicy 跟随 (Sprint 后续 P2) |
| R5 | cooling-off 冲突 (多个 change 同期) | 每 change 独立 cooling-off, 不阻塞并行立项 |
| R6 | Hard override 历史记录污染 (G1 precedent) | 每 HARD override 必填 `cooling_off_override_audit` 字段 (per Pattern #11 G2 precedent) |
| R7 | 真实 LLM 6-phase drop_ratio 红线触发漂移 (per Oracle C4 partial resolution) | G7 立项时明确 mock-fixture 单元测试 + 长跑实测累积数据 |

---

## 七、Maintenance

**维护规则** (Single-Dev 模式):
1. 每次 ship commit 后更新 §一 (已 ship commits + ctest 计数) + §十 (drift log)
2. 每个 Sprint 收官时更新 §五 (Sprint breakdown) + 标注已完成 work
3. 任何 docs_drift_audit WARNING/DRIFT 立即登记 §十 + 立项 cleanup change
4. 任何 improvement 状态变化 (pending → shipped / deferred) 更新 `.rddf/improvements/` 文件 + §一 baseline

**触发 reviewer 模式**:
- 任何 OpenSpec change (G4-G8) 必须 dual-agent pre-impl review (Metis + Oracle)
- 任何 hard override cooling-off 必须用户显式确认 + `.rddf/state/builder/` audit

---

## 八、References

- **Roadmap 模板**: `.rddf/roadmap/2026-09-21-pre-wave3-to-wave3-execution-plan.md` (Pre-Wave3 execution plan precedent)
- **Improvement 索引**: `.rddf/improvement-suggestions.md` (登记 6 个新 improvements)
- **Builder-handoff**: `.rddf/state/builder/l2-evolution-real-llm-provider.json` (G1 precedent)
- **Master Plan 模板**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` (14 sections + 4 Review Gates)
- **AGENTS.md 治理**: §SINGLE-DEVELOPER MODE + Pattern #11 (Async Worker + Dual Oracle dual-review SHIP-with-fixes cycle)
- **Reverse Indicator Rule**: 5 字段必须 (new_up / old_down / failure_traces / ablation / context_ids)

---

## 九、Review Gates (4 类型)

| Gate | 触发 | Check | Output |
|------|--------|-------|--------|
| 🔄 **Sprint Review** | 每 Sprint 收官 (A/B/C) | 行为符合预期? Bug? Hypothesis 错? 时间 drift > 30%? | §十 Drift Log |
| 🧭 **Architecture Drift** | 每 2-3 Sprint | ADR status 仍匹配? Phase 推进? | ADR-fix change |
| 🔗 **Dependency Refresh** | G8 启动前 | G4 + G7 已 ship? Interface 匹配? | §十一 Adjustment Log |
| 🎯 **Strategic Alignment** | Quarter / Wave 3 Phase 2 启动前 | Backlog 仍服务 Wave 3 目标? 新 ADR? | §十二 Strategic Pivots |

---

## 十、Drift Log

| Date | Event | Resolution |
|------|-------|------------|
| 2026-10-07 | C4 (R8.1 红线物理不可行) deferred from G1 | 登记 `.rddf/improvements/l2-release-metrics-real-llm-drop-ratio.md` partial resolution, 立项 G7 |
| 2026-10-07 | pre-existing llama.h symlink 失效 (`/workspace/project/AgenticLlama/` NX) | 修 `/workspace/main/AgenticLlama/` (per user tip), 不在 ship commit 范围 |
| 2026-10-07 | docs_drift_audit Scenario 4: 1 DRIFT (ADR-0078 finetune-base-model-impl-scope) | 立项 Cleanup-1 |
| 2026-10-07 | docs_drift_audit Scenario 6: 3 WARNING (active-status 计数) | 立项 Cleanup-2 |

---

## 十一、Adjustment Log

| pending | (待 G4/G8 立项时记录) |

---

## 十二、Strategic Pivots

| pending | (待 Wave 3 Phase 2 启动前记录) |

---

## 十三、Response Types (fix / retro / redirect)

| Type | When | Workflow |
|------|------|----------|
| **fix** | Bug 修复 / 缺陷 | `/opsx-apply <existing-change>` |
| **retro** | 性能/合规追溯修正 (defense-in-depth) | OpenSpec change 立项 (e.g., G5 retro 反作弊) |
| **redirect** | 战略/方向变更 | 新 Master plan 立项 + 老 plan 归档 |

---

## 附录 A: 物理布局

```
.rddf/
├── roadmap/
│   ├── 2026-09-21-pre-wave3-to-wave3-execution-plan.md  (Pre-Wave3, Wave 3 ✅ shipped)
│   └── 2026-10-07-post-l2-real-llm-roadmap.md            (本文档, current)
├── improvements/
│   ├── improvement-suggestions.md  (索引, 需更新登记 6 个新)
│   ├── l2-evolution-real-llm-provider.md  ✅ shipped (G1)
│   ├── harness-mutation-commit-real-llm.md  ⚪ pending (G4)
│   ├── rsi-anti-cheat-real-llm.md  ⚪ pending (G5)
│   ├── r13-4-confidential-redaction-real-llm.md  ⚪ pending (G6)
│   ├── l2-release-metrics-real-llm-drop-ratio.md  ⚪ pending (G7, partial resolution 2026-10-07)
│   └── self-evolution-9-stage-real-llm-end-to-end.md  🚫 blocked (G8)
├── plans/
│   └── (现有 14+ plans, 持续积累)
├── state/
│   └── builder/
│       └── l2-evolution-real-llm-provider.json  ✅ tracked (G1 precedent)
└── wt/
    └── (G4/G5/G6/G7 worktree 即将创建)

openspec/changes/ (active: 0, 所有 WIP 在 archive)
openspec/changes/archive/
└── 2026-10-06-l2-evolution-real-llm-provider/  ✅ shipped 5 files
```

---

## 下一步驱动 (RD-Workflow 操作序列)

按 RDD 流程启动每个 change:

1. **Phase 1**: Setup 阶段
   - 主会话预先 cut worktree (e.g., `.rddf/wt/l2-release-metrics-real-llm-drop-ratio/`, branch `feat/l2-release-metrics-real-llm-drop-ratio`)
   - 写 `.rddf/state/builder/<change>.json` 含 override audit + 4 commit hash slots + `cooling_off_override_audit` field
   - 触发 24h cooling-off (或用户 HARD override)

2. **Phase 2**: OpenSpec change 立项
   - `openspec new change "<slug>"` 创建 scaffold
   - 写 4 artifacts: `proposal.md` + `design.md` + `tasks.md` + `specs/<capability>/spec.md`
   - `openspec validate` PASS

3. **Phase 3**: dual-agent pre-impl review (Pattern #8)
   - 后台并行: Metis (`bg_2eXX`) + Oracle (`bg_2fXX`)
   - 收集 review 反馈, 应用 Critical/Major 修正到 artifacts
   - 写 `.rddf/state/builder/<change>.json::pre_impl_review` 字段

4. **Phase 4**: cooling-off 满 / HARD override 决策

5. **Phase 5**: 实施 (Pattern #11 async worker)
   - 派 Sisyphus-Junior async worker (background) with 6-segment delegation prompt
   - Worker TDD 5 步 + atomic commits + 4-file archive
   - 主会话 critical self-examination (per Pattern #11 step 4)

6. **Phase 6**: Oracle post-impl SHIP-with-fixes review (Pattern #11 step 5)
   - 派 Oracle 审查 verdict
   - Apply SHIP-with-fixes fixes (新 atomic commit, NOT amend)
   - 写 `.rddf/state/builder/<change>.json::post_impl_review` + `design_deviations` + `ac_verification`

7. **Phase 7**: merge worktree → main + cleanup
   - `git merge --no-ff <branch>` 保留 worktree branch history
   - `git worktree remove --force` + `git branch -d`
   - main working tree 重 build + post-merge ctest verification

8. **Phase 8**: 更新 improvement 文件 + improvement-suggestions.md 索引 + 本 roadmap §一/§十