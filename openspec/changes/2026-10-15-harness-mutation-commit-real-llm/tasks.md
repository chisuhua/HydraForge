## 1. OpenSpec Artifacts (per Day-5 5-file integrity)

- [x] 1.1 创建 `openspec/changes/2026-10-15-harness-mutation-commit-real-llm/` 目录
- [x] 1.2 创建 `.openspec.yaml` (含 pre_impl_dual_agent_review metadata)
- [x] 1.3 创建 `proposal.md` (含 4 cases redesign + 6 pre_issues + 14 AC + 6-file integrity)
- [x] 1.4 创建 `design.md` (含 8 Decisions + TDD 5-step + references)
- [x] 1.5 创建 `tasks.md` (本文件)
- [x] 1.6 创建 `specs/harness-mutation-commit-real-llm/spec.md` (8 Requirements, R5 UPDATED descope)
- [x] 1.7 `openspec validate` exit 0 (per Day-5 lesson)

## 2. TDD — 写失败测试 (4 cases redesign)

- [ ] 2.1 新建 `tests/test_harness_mutation_commit_real_llm.cpp` 4 TEST_CASE:
  - Case 1: commit 成功路径 (state consistency assertion, per D1)
  - Case 2: commit 事件流 (InMemoryBus wait_for_drain, per D6)
  - Case 3: gate denied 确定性回滚 (deterministic 非法 input, per D2 + D4)
  - Case 4: drop_ratio descope (per D3, no implementation, only R5 spec rationale)
- [ ] 2.2 实现 `RecordingLLMProvider` fixture (per D4):
  - 实现 `ILLMProvider` 三虚 (`generate` / `generate_stream` / `available_models`)
  - 录制 req/payload 到内存 (CI 回放)
  - 本地 DEEPSEEK_API_KEY set 时调真 LLM + 录制
- [ ] 2.3 实现 `HermeticGenomeHome` fixture (per D5):
  - setup: setenv("HOME", "/tmp/hermetic_<uuid>") + setenv("HYDRAFORGE_HOME", "/tmp/hermetic_<uuid>")
  - teardown: 恢复 + cleanup tmpdir
- [ ] 2.4 在 `tests/CMakeLists.txt` 注册 test_harness_mutation_commit_real_llm + LABELS "must_realllm"
- [ ] 2.5 构建并运行验证 4 cases 在 Recording Provider fixture 缺失时 fail, baseline 36 cases 不变

## 3. TDD — 最小实现 (RED → GREEN)

- [ ] 3.1 验证 4 cases 在 Recording Provider + HermeticGenomeHome fixture 完整时 PASS
  - Case 1: state consistency assertion 触发 (per D1)
  - Case 2: InMemoryBus wait_for_drain 后事件断言稳定 (per D6)
  - Case 3: ≥3 iter 100% consistency (per D7)
  - Case 4: descope, no implementation (per D3)
- [ ] 3.2 验证 baseline 36 cases 零回归:
  - `tests/test_harness_rsi_pilot.cpp` 22 cases
  - `tests/test_genome_registry.cpp` 13 cases
  - `tests/test_harness_mutation_proposal_real_llm.cpp` 1 case
- [ ] 3.3 验证 `ctest --test-dir build -L must_realllm` 0 失败 (本地有 key, 实测 2026-10-14 DEEPSEEK_API_KEY 已设)

## 4. TDD — REFACTOR 提取共享 helper (optional, 可 deferred)

- [ ] 4.1 提取 `tests/test_helpers/recording_provider.h` (若后续 must_realllm 测试复用)
- [ ] 4.2 提取 `tests/test_helpers/hermetic_home.h` (若后续 genome 测试复用)

## 5. Ship Gate 验证

- [ ] 5.1 `cmake --build build --target test_harness_mutation_commit_real_llm` exit 0
- [ ] 5.2 `ctest --test-dir build -L must_realllm -R test_harness_mutation_commit_real_llm` PASS
- [ ] 5.3 `ctest --test-dir build -LE must_realllm` baseline 36 cases + new 4 cases 全 PASS (零回归)
- [ ] 5.4 TSan / ASan 0 新增 error (尤其 UAF / race 路径)
- [ ] 5.5 ctest `--rerun-failed` flaky < 5% (per AGENTS.md 2026-09-12 经验)
- [ ] 5.6 `python3 tools/adr_lint.py` 0 error
- [ ] 5.7 `python3 tools/docs_drift_audit.py` 0 DRIFT (除已知 baseline 1 + 3 warning)

## 6. 24h Cooling-Off + Archive

- [ ] 6.1 实施完成后, 24h cooling-off 触发 (per AGENTS.md Single-Dev Mode)
- [ ] 6.2 cooling-off 期间, 可继续做 Phase B 立 4 新立 improvement (B1-B4)
- [ ] 6.3 cooling-off 满, user 确认 ship:
  - 6.3.1 `git status --short` 干净 (除无关遗留 active change)
  - 6.3.2 `git log -1` 看 ship commit message 含 [Reverse Indicator] 5 字段
  - 6.3.3 `git show <ship-commit>` 验证 6-file integrity (per Day-5 lesson)
- [ ] 6.4 `git mv openspec/changes/2026-10-15-harness-mutation-commit-real-llm/ openspec/changes/archive/`
- [ ] 6.5 AGENTS.md §Recent Changes 段登记本 ship (per AC-14)
  - R3 元指标实证 claim
  - Case 4 descope rationale 引用
  - 4 case redesign 引用 dual-agent 调研

## 7. Post-Ship 验证 (per P2.5 review 模式)

- [ ] 7.1 全量 ctest: `ctest --test-dir build -LE must_realllm` 全 PASS
- [ ] 7.2 TSan preset: `cmake --preset tsan && ctest --test-dir build-tsan -LE must_realllm` 全 PASS
- [ ] 7.3 ASan preset: `cmake --preset asan && ctest --test-dir build-asan -LE must_realllm` 全 PASS
- [ ] 7.4 真实 LLM 调用次数: ≤ 30 calls/test (避免 token 浪费)
- [ ] 7.5 Drop_ratio 物理不可行 ack (per D3 R5)

## 8. Process Improvement (per AGENTS.md Pattern #10)

- [ ] 8.1 在 `tools/audit-rddf-improvements.py` 加入 5 维静态 ship 判定 (架构级 fix 候选, per 2026-10-14 dual-agent 调研沉淀)
- [ ] 8.2 在 `tests/CMakeLists.txt` 全局 `set_tests_properties(... PROPERTIES LABELS "must_realllm")` 检查 (防 missing label 漂移)
- [ ] 8.3 在 pre-commit hook 加入 `git log --grep="real_llm_smoke" --since="1 month ago"` 检查 (防老旧 must_realllm 测试)

## 9. Reverse Indicator (per AGENTS.md §Reverse Indicator Rule)

按 commit message 5 字段格式:

```
[Reverse Indicator]
+ new_up: 4 cases redesign (state consistency / 确定性 / descope) + Recording Provider + hermetic HOME + must_realllm label + R3 元指标实证 claim
- old_down: drop_ratio=0% (test only, 0 src/ 改动, 36 baseline 零回归)
failure_traces: genome.committed event 验证存在但 real LLM 路径未覆盖, R3 元指标未实证 (dual-agent 2026-10-14 调研发现)
ablation: N/A (test only, no harness core change)
context_ids: code-class-ex-001 (per L2 spec §R3 reference)
```

## 10. Pre-existing Latent Issues 跟踪 (per AGENTS.md Pattern #10 + dual-agent 调研)

| Issue | 源 | 跟踪 |
|-------|-----|------|
| H4 evolution.readiness.denied 真实 LLM 验证 | dual-agent 调研 | 留独立 change 范围 (本 change 仅 H2+H3) |
| H5 反作弊 R9 真实 LLM 验证 | dual-agent 调研 | `rsi-anti-cheat-real-llm` 独立 change (OPEN) |
| H6 4-tier gate 反向校验 + drop_ratio 真实 LLM 验证 | dual-agent 调研 | Case 4 descope, 红线验证属独立 `R8.1 红线 触发 验证` 提案 (OPEN) |
| env_vars secrets redaction | Metis 独占 E | `adr-0072-d4-env-vars-redaction.md` (Phase B 推进) |
| UTF-8 切片 bug | Metis 独占 F | `fix-stream-handle-utf8-chunk-boundary.md` (Phase B 推进) |
| sink pointer RAII 生命周期 | Oracle 独占 G | `fix-stream-handle-sink-lifecycle.md` (Phase B 推进) |

## References

- `.rddf/audits/2026-10-14-p0-proposals-dual-agent-review.md` (dual-agent 调研产物)
- `.rddf/state/.arch-handoff.json::trigger_pre_issues` (6 pre_issues 记录)
- `.rddf/state/.planner-handoff.json` (6 pre_issues_must_resolve_before_rdd_builder_p0)
- `.rddf/improvements/harness-mutation-commit-real-llm.md` (5-segment 修订完成)
- `openspec/changes/archive/genome-wiring-harness-rsi-gepa-2026-09-22/` (闭环第 7 环)
- `docs/architecture/harness-architecture-2026-09.md` §十二 H1-H6
- `docs/architecture/self-evolution-architecture-2026-08.md` §十二 R3 元指标
- `docs/adr/adr-0068-event-emission-contract.md` Appendix A v2.3 (genome.* 主题)
- `tests/test_helpers/real_llm_env.h` (must_realllm helper)
- AGENTS.md §REAL LLM TESTING §5 (Recording Provider 模式)
- AGENTS.md Pattern #3 / #7 / #9 / #10
- 2026-09-21 C1 genome.key fresh-deploy 教训
- 2026-10-06 l2-evolution-real-llm-provider C4 deferred ack
