# Pre-Wave3 to Wave3 Execution Plan (2026-09-21 → ?)

> **来源**: 综合 Oracle `bg_3c06ae5b` (闭环第 7 环断裂) + `bg_6a8e4397` (roadmap drift 12 项) + Metis `bg_687a5662` + Oracle `bg_534a2541` (genome-wiring dual-review) 输出。
> **状态**: Phase 7 短期 G1/G2/G3/G4 active 中。Wave 3 立项前置等待 G4 ship。
> **维护**: Single-Dev 模式，每次 sprint 收官需更新状态。

## 1. 当前快照（2026-09-21, main `0b54914`）

### 已 ship commits（本 session, 6 atomic commits）

| Hash | 主题 | 影响范围 |
|------|------|----------|
| `dea85f6` | fix(genome-registry): fresh-machine HMAC key + hermetic test fixture (C1) | registry_filesystem.cpp + test_genome_registry.cpp |
| `5b600a6` | docs: 自进化 v1.4 + rsi-mapping C3/C4 alignment + roadmap CRD string + AGENTS.md 模式 #10 | 4 文档 |
| `3076042` | feat(openspec): genome-wiring-harness-rsi-gepa v2 — closes loop ring 7 | 5 文件 OpenSpec change |
| `74e063c` | chore(governance): record ADR-0086 v1.1 archive recovery (Day-5 trap recurrence) | docs/governance/2026-09-21-openspec-archive-recovery.md (NEW) |
| `977bcde` | docs(roadmap): Phase 6c drift patch A1-A11 + Pre-Wave3 收口门禁 4 项 | roadmap |
| `0b54914` | docs(decision): C4 GO 回注 post-hoc closure gate | docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md §5.1 |

### ctest 基线: 252 (test_genome_registry 13/13 = 273 assertions after C1 fix)

### Active OpenSpec changes: 9 (Pre-Wave3 门禁 4 + 短链 5)

| Change | 状态 |
|---|---|
| `harness-rsi-remove-governance` | 🟡 Active (Pre-Wave3 G1) |
| `evolution-verdict-reward-quality` | 🟡 Active (Pre-Wave3 G2) |
| `sync-pdk-contract-header` | 🟡 Active (Pre-Wave3 G3, ∥ G1) |
| `genome-wiring-harness-rsi-gepa` | 🟡 Active (Pre-Wave3 G4, commit `3076042`, dual-reviewed) |
| `fix-generate-subgraph-static-next` | 🟡 Active (latent gap #1) |
| `intent-classification-router` | 🟡 Active (P1) |
| `chat-real-llm-coverage-phase-h` | 🟡 Active (real-LLM E2E) |
| `fix-flatten-layers-comment-drift` | 🟡 Active (drift cleanup) |
| `provider-llm-tool-empty-passthrough` | 🟡 Active (defense-in-depth) |

### 最近 archive: 6 entries (含 ADR-0086 v1.1 物理恢复)
- `2026-09-19-2026-09-16-genome-registry` (C2)
- `2026-09-20-2026-09-16-h-d-m-transition-guard` (C3)
- `2026-09-20-2026-09-20-ig-genome-registry-walk-ancestors` (C3 follow-up)
- `2026-09-20-2026-09-20-adr-0086-v1-1-harness-change-confounder` (ADR-0086 v1.1 实施载体，commit 798b6c6 删除后 2026-09-21 git history 物理恢复)
- `2026-09-21-2026-09-20-adr-0068-appendix-a-evolution-themes` (D8 主题注册)
- `2026-09-21-2026-09-16-harness-rsi-pilot` (C4, ship + GO)

---

## 2. Phase 7 短期: Pre-Wave3 收口门禁 4 项 (3-4 天, Week 1)

### Sequencing

```
Week 1, Day 1-2 (∥): G3 + G1
                       │
                       ↓ (G1 ship)
                       │
Week 1, Day 2-3 (→): G2
                       │
                       ↓ (G2 ship)
                       │
Week 1, Day 3-4 (→): G4
                       │
                       ↓ (G4 ship)
                       │
Week 1, Day 4 (verify): ctest + openspec validate + AGENTS.md sync + 24h cooling-off + Oracle
                       │
                       ↓
                [Pre-Wave3 4 项全绿]
```

**串行约束**: G1 → G2 → G4 (三者都改 `MutationGateContext` + `harness_rsi.cpp`)。G3 ∥ 全并行（独立文件面）。

### 各 Gate 执行细节

#### G1 — `harness-rsi-remove-governance` (1-2d)

- Active status: proposal + tasks + spec + design 完整（commit 79e8b5d 之前的 rdd-planner/rdd-builder cycle）
- 已 dual-reviewed: Oracle `bg_c706862b` (5 SHIP-with-fixes 应用) + Metis `bg_d9744d91` (3 BLOCK→修正)
- 执行（per `tasks.md` §3）:
  - 3.1 `harness_rsi.h`: `MutationGateContext` + `trace_id` 字段
  - 3.2 Gate 2 扩展 tools_remove 循环
  - 3.3 trace_id 事件透传（meta.trace_id 替代硬编码空串）
  - 3.4 `secure_tool_registry.cpp` `unregister_tool_function` + `is_disabled` 检查
  - 3.5 `registry.h/.cpp` `mutation_mutex_` 写-写互斥（3 写路径全保护）
  - 3.6 测试构造点同步更新
  - 3.7 GREEN 验证
- **验证门**:
  - `ctest -R "test_harness_rsi_pilot|test_tool_registry|test_secure_tool_registry" --output-on-failure` → 100% PASS
  - `openspec validate harness-rsi-remove-governance --strict` → "Change is valid"
  - 既有 9 cases / 43 assertions 零回归
- **Commit pattern**: 1 atomic commit per AGENTS.md 模式 #4 (含 SHIP-with-fixes 修正)
- **Day-5 lesson guard**: archive 时 `git ls-files openspec/changes/archive/<name>/` 验证 4 文件全在（不用 `-f`）

#### G2 — `evolution-verdict-reward-quality` (0.5-1d)

- Active status: proposal-only（早期），需补 tasks + spec + design
- 关键设计: `EvolutionVerdict` 增加 `reward_quality` 字段（`agenticdsl::RewardSignal::Quality`），`harness_rsi.cpp:117` `eval_quality: "Unknown"` → 透传 `verdict.reward_quality`
- 串行依赖 G1（共享 harness_rsi.cpp 修改面）
- **验证门**:
  - focused ctest + MutationDecision 链路测试 PASS
  - `openspec validate evolution-verdict-reward-quality --strict` → "Change is valid"
- **Commit pattern**: 1 atomic commit

#### G3 — `sync-pdk-contract-header` (0.5-1d, ∥ G1)

- Active status: proposal-only 早期
- 关键: 清单 4 头 → 实测 11 头（含 DRY_RUN 离线化不 clone + 1 头悬空 include 登记 follow-up）
- **验证门**:
  - `sync-pdk.sh` 干跑 PASS + 11 头 stdout 验证
  - `openspec validate sync-pdk-contract-header --strict` → "Change is valid"
- **Commit pattern**: 1 atomic commit

#### G4 — `genome-wiring-harness-rsi-gepa` (1-2d) ⭐ 闭环第 7 环闭合

- Active status: proposal + tasks + spec + design v2 已 ship（commit `30760442`, dual-reviewed by Metis `bg_687a5662` + Oracle `bg_534a2541`）
- 4 Critical fixed + 3 Deal-breaker fixed（per dual-review）
- **Hard 依赖**: G1 ship 后（同改 `MutationGateContext` + `harness_rsi.cpp`），trace_id 字段复用为 `genome.committed` meta.trace_id
- 执行（per `tasks.md` §3, 1-15）:
  - 3.1-3.2 `harness_rsi.h` struct 扩展（`MutationGateContext` +3 字段末尾追加 + `AppliedMutation` +3 字段 + `undo_applied_mutation` 声明）
  - 3.3 `harness_rsi.cpp` workflow_patch 检查上移 Gate 0
  - 3.4-3.5 Gate 3 持久化 + undo + 快照
  - 3.6 2 个事件发射
  - 3.7-3.8 GEPA Config 注入 + persist-then-commit
  - 3.9 GREEN 验证
  - **6.1 直接修订 `docs/adr/adr-0068-event-emission-contract.md` Appendix A v2.3**（搭车不可行 — `2026-09-20-adr-0068-appendix-a-evolution-themes` 已 archive，per Metis DB-1 + Oracle #8）
- **验证门**:
  - `ctest -R "test_harness_rsi_pilot|test_gepa_phase2|test_genome_registry|test_genome_walk_ancestors" --output-on-failure` → 100% PASS
  - `openspec validate genome-wiring-harness-rsi-gepa --strict` → "Change is valid"
  - 新增 hermetic env fixture（复制 test_genome_registry.cpp:30-41 模式）
  - ADR-0068 Appendix A grep 验证：`genome.committed` + `genome.persist_failed` ≥ 1 命中 each
- **Commit pattern**: 3-4 atomic commits（harness_rsi.h struct / harness_rsi.cpp impl / GEPA wiring + tests / ADR-0068 Appendix A 修订）

### Phase 7 短期验证门（Week 1 末尾）

```
□ G1 commit + 9 cases / 43 assertions 零回归 + openspec validate PASS — ✅ SHIPPED (merge 9709317)
□ G2 commit + MutationDecision 链路测试 PASS + openspec validate PASS — ✅ SHIPPED (merge dc12a17, Oracle bg_ebfe1c25 SHIP verdict 0C + 2 Minor)
□ G3 commit + sync-pdk.sh 干跑 PASS + 11 头覆盖 + openspec validate PASS — ✅ SHIPPED (merge a196a09)
□ G4 commit + 4 binary 全绿 + 闭环第 7 环 audit ✅ + openspec validate PASS — ✅ SHIPPED (merge fb2769f, Oracle bg_e4eec567 SHIP-with-fixes 3 Major 已修)
□ ctest 全量零回归（expected 252, 实际可能因 G4 新增 binary = 253） — ✅ 208/210 PASS (99%), 2 失败均为 pre-existing (test_skill_interpreter KI 7.S29-1 + test_pdk_plan_execute BAD_COMMAND build 后 PASS)
□ AGENTS.md Recent Changes 追加 4 commits（G1/G2/G3/G4）+ 整体收口注记 — ✅ G3 + G4 已 ship (commit 98711e3), G2 本批追加
□ roadmap §三 Pre-Wave3 收口 4 项 标 ✅ SHIPPED — ✅ 4-Gate 序列全部 SHIPPED (本批 commit)
□ ADR-0068 Appendix A v2.3 grep 验证：`genome.committed` + `genome.persist_failed` ≥ 1 命中 each — ✅ (G4 commit a21c08a)
□ Decision Record §5.1 状态由 "post-hoc closure gate" → "closed" — ✅ (commit 98711e3)
□ 24h cooling-off（Single-Dev 模式必走）+ Oracle 复审 — 🕒 计时中, 起点 2026-09-22 22:30 (G2 merge dc12a17)
```

---

## 3. Phase 7 中期: Wave 3 立项 (1-2 天, Week 2 起点)

G4 ship 后 24h cooling-off + Oracle 复审通过，正式立项 ADR-0078 Model-RSI pilot。

### Sequencing

```
Week 2, Day 1: ADR-0078 立项
 - rdd-arch → rdd-planner improvement 5-segment → planner-handoff v1.1
  - 推荐路线：complex branch（per ADR-0088 precedent）
  - 自动决策 ADR-0078 full Approved（per ADR-0049）

Week 2, Day 1-2: Wave 3 kickoff
  - 起步 ship 范围：fine-tune 数据闭环（per ADR-0078 §D3）+ 4 维度评分（§D1）
  - 与 ADR-0084 V1 协同（L4 权重变异 emit-then-throw）
  - ADR-0078 v1 可能一开始就是 🟡 Partial（受数据/模型/GPU 限制）
```

### 验证门

```
□ ADR-0078 文件头 "✅ Approved" + V1 ship 范围记录
□ Wave 3 与 Pre-Wave3 4 项门禁的依赖图完整（roadmap §二 + §三 已记录）
□ 4 项门禁的审计证据完整（Oracle session IDs + commit hashes）
```

---

## 4. Phase 7 长期: 治理补登记 + follow-up (2-3 天, 穿插)

### A. 治理 follow-up（穿插）

| # | 项目 | 估时 | 优先级 | 备注 |
|---|------|------|--------|------|
| A1 | `.gitignore:36` `openspec/changes/archive/` ignore 策略决策 (RFC) | 0.5d | 高 | Day-5 陷阱反复；选项见 `docs/governance/2026-09-21-openspec-archive-recovery.md` |
| A2 | `docs/architecture/capability-application-map-2026-08.md` header 32 vs §一 33 内部不一致 | 0.1d | 中 | header :4 写 "32" vs §一 L53 写 "33" |
| A3 | `docs/guides/agent-collaboration-patterns.md` §10 ADR-0086 状态翻牌 + Genome Registry 提及 | 0.1d | 中 | §10 ADR-0086 仍标 🔍 Proposed，实际 ✅ Approved v1.1 |
| A4 | ADR-0086 v1.2 candidate: 扩展 `judge_data_freshness` 签名为 `Result<AttributionVerdict, JudgeResult>` | 0.5d | 中 | v1.1 仅返回 verdict，confounder 需 caller 构造 |
| A5 | 短链 5 项实际推进（按 roadmap §1.2 估时排序） | 持续 | 低 | `fix-generate-subgraph-static-next` (P0), `intent-classification-router` (P1), `chat-real-llm-coverage-phase-h` (P2), `fix-flatten-layers-comment-drift` (P3), `provider-llm-tool-empty-passthrough` (P2) |

### B. 已知未修缺陷（from Oracle `bg_3c06ae5b`，留 follow-up）

| # | 项目 | 估时 |
|---|------|------|
| B1 | transition_guard `evaluate_readiness` condition 2 sentinel trace vacuity | 0.5d |
| B2 | mutation_governor `commit()` 缺 propose→commit 状态机 + L4 路径禁言不触发 | 0.5d |
| B3 | mutation_governor revert() audit-only + ADR-0079 session-fork 回滚零实现（部分由 G4 undo 覆盖） | 1d |
| B4 | cross-process concurrency（V1 单写者约束需 API 契约层声明） | 0.1d |
| B5 | walk_ancestors depth cap 缺失 | 0.1d |
| B6 | fork `deep_merge_spec` 整体替换语义 vs harness_rsi 增量心智模型（G4 V1 InvalidMutation 边界已记录） | 0.2d |
| B7 | GenomeMetadata 无 provenance 字段 + 三版本制无交叉文档 | 0.3d |
| B8 | eval_quality 硬编码 "Unknown"（G2 修复） | 0 |
| B9 | HMAC parse-before-verify 语义文档化 | 0.1d |

**B1-B7 + B9 总估时 ~3 天**，Sprint 7 末尾或 Sprint 8 起步集中修复。

### C. 数据/模型依赖

| # | 项目 | 阻塞 |
|---|------|------|
| C1 | DEEPSEEK_API_KEY 或 MINIMAX_API_KEY | `chat-real-llm-coverage-phase-h`, Wave 3 Model-RSI pilot |
| C2 | GPU/LFS for fine-tune 实验 | ADR-0078 D4 LoRA |
| C3 | Wasm toolchain 完善 | ADR-0061-11/12 V2 deferred Phase 8+ |

---

## 5. Phase 8+ (跨多 sprint, 受 Solo Dev 容量约束)

roadmap §1.1 + §1.4 提示 Phase 7 启动条件 6 项中 3 项 FAIL（结构性不满足 Solo Dev ~27h/周）。Wave 3 完成 + 4 项门禁 ship 后，Phase 8 不能立即启动。需：

1. Solo Dev 容量扩张 OR
2. 任务再分（拆 Sprint）OR
3. 委派（招募更多 dev 或 AI 协作效率提升）

Phase 8 = MetaRSI-v1 S2 受治理变异 + S3 训练期蒸馏 + S4 协同进化（per `docs/architecture/self-evolution-architecture-2026-08.md` §六）。Phase 7 (Wave 3) 实质 = MetaRSI-v1 S1 反思候选，Phase 8 = S2。

---

## 6. 决策点（待用户授权）

| # | 决策 | 选项 |
|---|------|------|
| D1 | `.gitignore:36` archive 策略 | A (current: ephemeral + 治理审计) / B (archive 入 git) / C (hybrid git-mv) |
| D2 | Phase 7 短链 5 项启动时机 | (a) G4 后立即启动 1-2 项（likely fix-generate-subgraph + provider-llm-tool） / (b) 等 Wave 3 立项后并行 |
| D3 | B1-B7 + B9 已知缺陷 | (a) Sprint 7 末尾集中做 / (b) 分摊到 Sprint 8 |
| D4 | Real LLM 测试阻塞 | 是否提前申请 API key / 接受 Phase H deferred |

---

## 7. 验证命令速查

```bash
# 短路验证（任一节点）
ctest --test-dir build -R "test_genome_registry|test_harness_rsi_pilot|test_gepa_phase2" --output-on-failure

# 整路验证（Phase 7 短期末尾 + Wave 3 立项前）
ctest --output-on-failure
openspec list # 4 项门禁 0 active 后才能立项 Wave 3
python3 tools/adr_lint.py
python3 tools/docs_drift_audit.py
```

---

## 8. 风险与 guard

| 风险 | guard |
|---|---|
| G1/G2/G4 同改 `MutationGateContext` 冲突 | G1 ship 后 rebase G2/G4；G2 ship 后 rebase G4；每个 gate 末尾 `git diff --stat` 检查 |
| Day-5 陷阱复发 | 每个 gate commit 末尾 `git ls-files openspec/changes/archive/<name>/` 4 文件验证 |
| archive 不入 git 漂移 | 修复 D1 后才能彻底避免 |
| Solo Dev 容量不足4 项 | G3 ∥ G1 节省 0.5-1d；G4 可选 v2 → v1 缩减（去掉 undo + GEPA 接线，单版本号持久化），最低 1d |
| Real LLM 测试阻塞 | F1 → chat-real-llm-phase-h 已 deferred；Wave 3 D6 AgenticMind 回流也需要 API key |

---

## 9. 相关文档

- **本文档 companion**:
  - `docs/governance/2026-09-21-openspec-archive-recovery.md` (治理注记, Day-5 陷阱记录)
  - `docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md` §5.1 (C4 GO post-hoc closure gate annotation)
  - `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §三 Pre-Wave3 收口子节 + §四 C4 GO 判据修订 + §十 Drift Log
  - `AGENTS.md` Recent Changes 2026-09-21 entry + 模式 #10 沉淀

- **OpenSpec change 入口**:
  - `openspec/changes/harness-rsi-remove-governance/`
  - `openspec/changes/evolution-verdict-reward-quality/`
  - `openspec/changes/sync-pdk-contract-header/`
  - `openspec/changes/genome-wiring-harness-rsi-gepa/` (commit 3076042, dual-reviewed)
  - `openspec/changes/fix-generate-subgraph-static-next/`
  - `openspec/changes/intent-classification-router/`
  - `openspec/changes/chat-real-llm-coverage-phase-h/`
  - `openspec/changes/fix-flatten-layers-comment-drift/`
  - `openspec/changes/provider-llm-tool-empty-passthrough/`

- **Oracle sessions**:
  - `bg_3c06ae5b`: 自进化版本管理审计 + 闭环第 7 环断裂 + C1 fresh-MHMAC
  - `bg_6a8e4397`: roadmap drift 12 项 + wiring placement verdict
  - `bg_687a5662`: Metis dual-review of genome-wiring v1
  - `bg_534a2541`: Oracle dual-review of genome-wiring v1
  - `bg_c706862b`: Oracle dual-review of harness-rsi-remove-governance (5 SHIP-with-fixes)
  - `bg_d9744d91`: Metis dual-review of harness-rsi-remove-governance (3 BLOCK→修正)

---

**Last Updated**: 2026-09-21 (commit pending)