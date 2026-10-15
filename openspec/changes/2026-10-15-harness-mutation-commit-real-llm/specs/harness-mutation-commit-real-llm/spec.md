## ADDED Requirements

### Requirement: Harness mutation commit 集成测试 (R1, UPDATED per 2026-10-15 M1 路径 A 修正)

The system SHALL provide a `tests/test_harness_mutation_commit_persist.cpp` test binary that verifies 4 cases of harness mutation commit + IGenomeRegistry persist path as a **deterministic integration test** (NOT real LLM, per M1 路径 A 修正). The original "real LLM 端到端" 声称 is descoped per Oracle SHIP-with-fixes 评审 — must_realllm label removed (M1), Recording Provider 守卫 descoped (M2, per Case 4 先例).

**注 (per 2026-10-15 Oracle 评审)**: 本集成测试不调用 LLM (deterministic 路径). 真实 LLM 端到端验证属独立 must_realllm change 范围 (H2/H3/H4 真实 LLM 验证 — 见 `.rddf/audits/2026-10-14-rddf-improvements-audit.md` "G5 rsi-anti-cheat-real-llm" + "H4 evolution.readiness.denied 真实 LLM 验证" 等独立 change 范围).

#### Scenario: Case 1 commit 成功路径 (state consistency)

- **GIVEN**: 确定性 prompt_delta (硬编码字符串, 本测试不调 LLM) + 初始 genome_version=N + 初始 Genome spec.harness=S
- **AND**: 真实 Gate 0-3 确定性规则通过 (tool 白名单 / 依赖图 / schema 验证) — 走真实 `apply_harness_mutation()` 路径
- **WHEN**: `executor.commit(proposal)` 执行 Gate 0-3 → `IGenomeRegistry::fork()` → `apply_harness_mutation` → 触发 `genome.committed` 事件
- **THEN**:
  - `IGenomeRegistry::load(commit_result.genome_version)` 返回 Genome spec.harness' 与 S 逐字段一致 (diff 工具比对)
  - `walk_ancestors` 链含父版本 + 新版本 (长度 ≥2)
- **AND NOT**: 断言 Gate 评分具体值
- **AND NOT**: 断言 commit 后某字段等于某值

#### Scenario: Case 2 commit 事件流 (per M3 修正 payload 4 字段断言)

- **GIVEN**: 确定性 prompt_delta + 真实 Gate 0-3 路径
- **WHEN**: `bus->emit` 触发 `genome.committed` 事件
- **THEN** (per M3 修正 + ADR-0068 Appendix A v2.3 实际 emit 字段, src/evolution/harness_rsi.cpp:225-228):
  - 事件 payload `args` MUST contain `genome_name` 字段
  - 事件 payload `args` MUST contain `version` 字段 (commit 后的 genome version)
  - 事件 payload `args` MUST contain `parent` 字段 (parent_version)
  - 事件 payload `args` MUST contain `mutation_kind` 字段 (= "harness")
  - 事件 payload `meta` MUST contain `trace_id` 字段
- **AND**: `walk_ancestors(commit_result.genome_version)` 链 MUST be ≥ 2

#### Scenario: Case 3 gate denied 确定性回滚 (per M4 校正 spec 事件契约)

- **GIVEN**: 确定性非法 input (缺必需字段 — 全部空 mutation)
- **AND**: CapturingBus 拦截事件 (per M4 修正)
- **WHEN**: `executor.commit(bad_proposal)` 执行 Gate 0
- **THEN** (per M4 校正 spec 事件契约):
  - Gate MUST return denied (确定性, 非 LLM 依赖)
  - `IGenomeRegistry` MUST NOT 调 `fork()` (零状态变更)
  - `genome.committed` 事件 MUST NOT trigger (per M4 校正 — denied 路径零事件发射)
  - `genome.persist_failed` 事件发射**不在**本 change scope (per M4: Gate 0 deny 路径不发射 persist_failed, 实际发射路径是 Gate 3 persist 失败, 属独立 change 范围)
- **AND**: 跑 ≥3 iter 验证 100% consistency (零 flaky)

#### Scenario: Case 4 drop_ratio descope (per R5)

- **GIVEN**: 本 change MUST descope Case 4 (per dual-agent 共识 + R5)
- **WHEN**: N/A (无实施)
- **THEN**: 本 change MUST NOT 含 drop_ratio 断言, MUST NOT 实施 L2 红线验证

### Requirement: Recording Provider 守卫 (R2, DESCOPED per 2026-10-15 M2)

**DESCOPED** per Oracle SHIP-with-fixes M2 评审: 本集成测试不调 LLM, Recording Provider 守卫模式不适用 (RecordingLLMProvider 类定义存在但 0 实例化, 0 调用, 是死代码).

**原 spec 承诺 (废弃)**: MUST implement `RecordingLLMProvider` (per AGENTS.md §5 守卫模式) + MUST 录制真实 LLM req/payload + MUST 在 CI `HYDRAFORGE_SKIP_REAL_LLM=1` 下回放录制 + MUST 在本地 `DEEPSEEK_API_KEY` set 时调真 LLM + MUST 严格遵循 AGENTS.md §5 模式.

**理由 (per M2 descope 先例 — Case 4 descope format)**:
- 真实 LLM 守卫模式适用于"调真 LLM 的 test" (must_realllm 标签), 本集成测试不调 LLM
- Recording Provider 守卫 descope 不影响 R1 集成测试价值 (确定性集成覆盖是 mock 单测的增量)
- 真实 LLM 守卫需求属独立 must_realllm change (H2/H3/H4 等) 范围

### Requirement: hermetic HOME fixture (R3)

The system SHALL provide a `HermeticGenomeHome` test fixture that:

- MUST 测试 setup/teardown 隔离 `~/.hydraforge/genome.key` 宿主状态
- MUST `setenv("HOME", "/tmp/hermetic_<random>", 1)` + `setenv("HYDRAFORGE_HOME", "/tmp/hermetic_<random>", 1)`
- MUST 析构时恢复 + 清理 tmpdir
- MUST 强制 fresh-deploy 路径 (per 2026-09-21 C1 genome.key 教训, 宿主状态掩盖 production bug)

#### Scenario: hermetic HOME fresh-deploy 路径测试

- **GIVEN**: 全新部署 (无既存 `~/.hydraforge/` 状态)
- **WHEN**: 跑 4 cases
- **THEN**: 测试 MUST 不 IOError, MUST 不依赖宿主 key 状态, MUST 真实 commit → reload 路径完整

### Requirement: Case 3 gate denied 确定性构造 (R4, UPDATED per M4 校正)

The system MUST 实现 Case 3 deterministic 构造, MUST NOT 依赖真实 LLM 生成非法 proposal (LLM 输出不可控, flaky 风险高). Case 3 真实 LLM 失败回滚 MUST 用确定性非法输入构造.

#### Scenario: Gate 0 deterministic deny (per M4 校正事件契约)

- **GIVEN**: 真实 Gate 0 路径 + 构造缺必需字段的 proposal (e.g. 缺 `tools_add` 或 `prompt_delta`) — **确定性非法 input**
- **AND**: CapturingBus 拦截事件 (per M4 修正)
- **WHEN**: `executor.commit(bad_proposal)` 执行 Gate 0
- **THEN** (per M4 校正):
  - Gate MUST return denied (确定性, 非 LLM 依赖)
  - `IGenomeRegistry` MUST NOT 调 `fork()` (零状态变更)
  - `genome.committed` 事件 MUST NOT trigger
  - `genome.persist_failed` 事件 **不在本 change scope** (per M4: Gate 0 deny 路径不发射 persist_failed, 实际发射路径是 Gate 3 persist 失败, 属独立 change 范围)

#### Scenario: Missing required field deny

- **GIVEN**: 真实 Gate 0 路径 + 构造缺必需字段的 proposal
- **WHEN**: `executor.commit(bad_proposal)` 执行 Gate 0
- **THEN**: 同 Scenario 1 (Gate 0 deterministic deny)

### Requirement: Case 4 drop_ratio descope (R5, UPDATED per 2026-10-15)

The system MUST descope Case 4 (per dual-agent 共识 + R5). drop_ratio 红线 5% 触发 (per AGENTS.md §R8.1) 在 L2 1-2 contexts 粒度物理不可行:

- 实测 0/50/100% 二值, 5% 阈值无法稳定构造 (per 2026-10-06 l2-evolution-real-llm-provider C4 deferred ack)
- 需 ≥20 mutations 才能稳定触发
- 红线验证 MUST 属独立 OpenSpec change

#### Scenario: drop_ratio descope ack

- **GIVEN**: 本 change MUST descope Case 4 (per dual-agent 共识 + R5)
- **WHEN**: N/A (无实施)
- **THEN**: 本 change MUST NOT 含 drop_ratio 断言, MUST NOT 实施 L2 红线验证

### Requirement: 0 源代码改动 (R8)

本 change MUST NOT 改源代码:

- MUST NOT 改 `src/modules/cognitive/harness_rsi.cpp`
- MUST NOT 改 `src/core/genome/registry_filesystem.cpp`
- MUST NOT 改 `src/core/genome/genome.h`
- MUST NOT 改 `src/common/llm/llm_types.h`
- 1 file 新增 (test) + 1 file CMake 改 + 1 file spec (本文件) + 1 file AGENTS.md entry

#### Scenario: 0 src/ 改动验证

- **GIVEN**: 本 change 实施完成
- **WHEN**: `git diff main --stat -- 'src/*' 'include/*'`
- **THEN**: 输出 MUST 为空 (0 src/ 改动)

## REMOVED Requirements

无. Genome wiring 闭环第 7 环 ship 时已定义 Requirements, 本 change MUST NOT 移除任何既有契约.

## MODIFIED Requirements

- R1 (UPDATED per M1 路径 A): 移除 "真实 LLM" 声称, 改为 "deterministic integration test"
- R2 (DESCOPED per M2): Recording Provider 守卫 descoped
- R4 (UPDATED per M4): 校正事件契约 — `genome.committed` MUST NOT trigger (denied 路径), `genome.persist_failed` 不在本 change scope
- R5 (UNCHANGED per 原 D3 决议): Case 4 descope 维持

## 14 Acceptance Criteria 完整列表 (per proposal.md, UPDATED per M1/M2/M3/M4/Minor 1-6 修正)

- AC-1: `tests/test_harness_mutation_commit_persist.cpp` 新建并入 CTest
- AC-2: **REMOVED** per M1 (must_realllm label removed)
- AC-3: **UPDATED per M1** — 4 cases 全部 PASS at deterministic integration test (无 must_realllm 要求)
- AC-4: **UPDATED per M2** — Recording Provider 守卫 descoped (属独立 must_realllm change 范围)
- AC-5: **REMOVED per M2** — Recording Provider 配对实现不需要
- AC-6: hermetic HOME fixture 实现 ✅
- AC-7: Case 3 deterministic 构造通过 ≥3 iter 100% consistency
- AC-8: `tests/test_harness_rsi_pilot.cpp` 22 cases 零回归
- AC-9: `tests/test_genome_registry.cpp` 13 cases 零回归
- AC-10: `tests/test_harness_mutation_proposal_real_llm.cpp` 1 case 零回归
- AC-11: **UPDATED per M1** — `ctest --test-dir build -L <新标签>` (本测试无 must_realllm 标签)
- AC-12: TSan / ASan 0 新增 error
- AC-13: flaky < 5% (deterministic → 0% flaky, test run <1s)
- AC-14: AGENTS.md §Recent Changes 段登记本 ship (含 4 Major + 6 Minor 修正 + M1 路径 A 决策)
