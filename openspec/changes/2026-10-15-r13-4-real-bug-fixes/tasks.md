# Tasks: R13.4 confidential redaction 2 真 bug 修复

**Change ID**: `2026-10-15-r13-4-real-bug-fixes`
**Status**: draft (P1 实施待 24h cooling-off 满)
**对应 proposal**: `proposal.md`
**对应 design**: `design.md`

## Phase 0: pre-impl dual-agent review (per AGENTS.md Pattern #8)

- [x] 派 Metis + Oracle 并行 (per user decision 2026-10-15)
- [x] 调研审计入 `.rddf/audits/2026-10-15-r13-4-confidential-redaction-real-llm-dual-agent-review.md` (200+ 行, 4 强命中 + 3 独占发现)
- [x] 交叉命中 4 项: (1) Case 1/2 断言恒真 / (2) Case 2 物理不可测 / (3) Case 3 幻影需求 / (4) Case 4 spec 冲突
- [x] 独占发现 3 项: (Metis) 跨 context 泄漏真 bug + expected_eval_quality 漏脱敏 + (Oracle) spec 417 wire-level by design

## Phase 1: OpenSpec change artifacts (本 change 完成)

- [x] 1.1 创建 `openspec/changes/2026-10-15-r13-4-real-bug-fixes/` 目录
- [x] 1.2 创建 `.openspec.yaml` (含 pre_impl_dual_agent_review metadata + 5 pre_issues + 关联 3 提案)
- [x] 1.3 创建 `proposal.md` (含 2 bug 详细 Why + 14 AC + 5 决策)
- [x] 1.4 创建 `design.md` (D1-D6 设计决策 + 风险回滚 + 14 AC 测试设计)
- [x] 1.5 创建 `tasks.md` (本文件, 8 phase TDD 5-step)
- [x] 1.6 创建 `specs/r13-4-redaction-bug-fixes/spec.md` (R13.4 compliance 提升 scenario)
- [x] 1.7 `openspec validate` exit 0 (per Day-5 lesson)

## Phase 2: TDD 5-step 实施 (24h cooling-off 后启动)

### 步骤 1: RED Bug 1 (expected_eval_quality 漏脱敏)

- [ ] 2.1 在 `tests/test_reverse_indicators.cpp` 加 Case 5 (构造 ContextRequest `metadata.sensitivity = "confidential"`, `expected_eval_quality = "Poor"`)
- [ ] 2.2 跑 `ctest -R test_reverse_indicators -j1` 验证 Case 5 FAIL
- [ ] 2.3 验证失败信息显示 `event["expected_eval_quality"] == "Poor"` 而非 `"[REDACTED-confidential]"`
- [ ] 2.4 验证既有 Case 1-4 仍 PASS (零回归)

### 步骤 2: GREEN Bug 1

- [ ] 2.5 在 `examples/pdk_chat_demo_evolution/evolution_session.cpp:582-602` `redact_trace_fields` 函数 line 590-592 之后加 `expected_eval_quality` 脱敏分支 (per D2)
- [ ] 2.6 跑 Case 5 PASS
- [ ] 2.7 跑既有 4 cases 零回归

### 步骤 3: RED Bug 2 (跨 context 泄漏)

- [ ] 3.1 在 `tests/test_reverse_indicators.cpp` 加 Case 6 (2 contexts: public + confidential, 验证 confidential context trace 不含 public turn_input 明文)
- [ ] 3.2 跑 `ctest -R test_reverse_indicators -j1` 验证 Case 6 FAIL
- [ ] 3.3 验证失败信息显示 cross-context pollution

### 步骤 4: GREEN Bug 2

- [ ] 3.4 抽 helper `EvolutionSession::recreate_chat_session()` 复用 `phase1_init` make_unique 模式 (per D1)
- [ ] 3.5 在 `phase2_baseline` 入口 (line 290 之前) 调 `recreate_chat_session()`
- [ ] 3.6 跑 Case 6 PASS
- [ ] 3.7 跑既有 4 cases 零回归

### 步骤 5: 全量回归

- [ ] 4.1 `ctest --test-dir build/examples/pdk_chat_demo_evolution/tests -L l2-evolution` 11/11 PASS (9 mock + 2 new)
- [ ] 4.2 `ctest --test-dir build -LE must_realllm -j1` 全 PASS
- [ ] 4.3 `cmake --build build --target agenticdsl_core` 100% Built (g++-14.2 + C++23)
- [ ] 4.4 `ctest -R test_skill_interpreter` 单独 PASS (避免 pre-existing 7.S29-1 flaky)
- [ ] 4.5 `drop_ratio = 0%` (R8 红线守住)
- [ ] 4.6 `LSP discipline check` 全部通过

## Phase 2.5: Oracle post-impl SHIP-with-fixes review (per Pattern #4)

- [ ] 5.1 派 Oracle 后台审查 (session_id 由 rdd-builder P0 case 1 触发)
- [ ] 5.2 等待 verdict: SHIP / SHIP-with-fixes / BLOCK
- [ ] 5.3 若 SHIP-with-fixes: 应用修正 (按严重度排序)
- [ ] 5.4 若 BLOCK: 修复后重新提交 + 再审

## Phase 3: Archive + 文档同步 (per Day-5 lesson)

- [ ] 6.1 `git mv openspec/changes/2026-10-15-r13-4-real-bug-fixes/ openspec/changes/archive/2026-10-15-r13-4-redaction-bug-fixes/`
- [ ] 6.2 更新 `docs/architecture/rsi-architecture-2026-09.md` §12.3 R13.4 验证矩阵 (新增 "Bug 修复" 行)
- [ ] 6.3 更新 `openspec/changes/archive/2026-09-24-pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md:409` 注释 (R13.4 compliance 提升)
- [ ] 6.4 AGENTS.md Recent Changes 段本 ship 登记 (含 Reverse Indicator 5 字段)
- [ ] 6.5 验证 Day-5 5-file integrity (`.openspec.yaml` + `proposal.md` + `design.md` + `tasks.md` + `specs/<name>/spec.md`)
- [ ] 6.6 更新 `.rddf/improvements/r13-4-confidential-redaction-real-llm.md` 头注 "2 真 bug 修复" 段 (per proposal line 53-55)

## Phase 4: 关联提案同步

- [ ] 7.1 `.rddf/improvements/adr-0072-d4-env-vars-redaction.md` 头注登记 "后续 1 提案: 2026-10-15-r13-4-real-bug-fixes 已 ship" (per adr-0072 是 parallel OPEN, 不在本 change 范围)
- [ ] 7.2 `.rddf/audits/2026-10-15-rsi-anti-cheat-real-llm-dual-agent-review.md` 更新 "姐妹审计" 段 (per cross-reference)

## Phase 5: rdd-verifier 阶段 (可选, per ADR-0034)

- [ ] 8.1 派 rdd-verifier retry loop (max 3 per ADR-0034)
- [ ] 8.2 验证 14 AC 全部满足
- [ ] 8.3 若 verifier FAIL: 修复 + retry

## 升级触发 (per proposal §Cooling-off)

| 触发条件 | 行动 |
|---------|------|
| Oracle verdict 含 Critical | 修复后才能 archive |
| per-context reset 破坏 l2-evolution-real-llm-provider Case 5 (hermetic dual-track) | 立刻回滚 D1 切到选项 C |
| 9/9 mock tests 因 chat_session reset 行为变化失败 ≥ 1 | 切到 D1 选项 D (ChatSession 公开新 API) |
| redact expected_eval_quality 破坏 spec R13.4 schema 兼容性 | 立刻回滚 |
| `ctest -LE must_realllm` drop > 0% | 立刻回滚 |

## Non-goals (对照 design §1)

- 不引入新 redact 协议主题 (D4)
- 不动 ChatSession 公开 API (D5)
- 不重写 ChatSession::chat() (范围最小化)
- 不动 std::expected 公开 ABI (保持 ship 后契约)
- 不修 env_vars secrets redaction (parallel OPEN, adr-0072-d4-env-vars-redaction 独立)
- 不修 wire-level redaction (spec R13.4 NOTE `:417` by design 不在 L2 范围)
