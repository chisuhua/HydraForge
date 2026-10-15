## ADDED Requirements

### Requirement: Harness mutation commit 真实 LLM 端到端验证 (R1)

The system SHALL provide a `tests/test_harness_mutation_commit_real_llm.cpp` test binary (must_realllm CTest label) that verifies 4 redesigned cases of harness mutation commit + IGenomeRegistry persist path with real LLM (DEEPSEEK_API_KEY) end-to-end.

#### Scenario: Case 1 commit 成功路径 (state consistency)

- **GIVEN**: 真实 LLM provider 配置 (DEEPSEEK_API_KEY) + 初始 genome_version=N + 初始 Genome spec.harness=S
- **AND**: 真实 LLM 生成 mutation proposal (含 tools_add/prompt_delta 等合法字段)
- **WHEN**: `executor.commit(proposal)` 执行 Gate 0-3 → `IGenomeRegistry::fork()` → `apply_harness_mutation` → 触发 `genome.committed` 事件
- **THEN**:
  - `IGenomeRegistry::load(commit_result.genome_version)` 返回 Genome spec.harness' 与 S 逐字段一致 (diff 工具比对)
  - `walk_ancestors(commit_result.genome_version)` 链长度 ≥ 2 (含新版本 + 父版本)
  - `genome.committed` 事件 payload MUST contain `genome_version` 字段
- **AND NOT**: 断言 Gate 评分具体值
- **AND NOT**: 断言 commit 后某字段等于某值

#### Scenario: Case 2 commit 事件流 (InMemoryBus wait_for_drain)

- **GIVEN**: 真实 LLM provider + initial state + 真实 LLM mutation commit 触发
- **WHEN**: `bus->wait_for_drain()` 排空 in-flight 事件
- **THEN**:
  - `bus->drain()` 返回事件列表 MUST contain `genome.committed`
  - 事件 payload MUST contain `genome_version` + `parent_version` + `mutation_diff`
  - `walk_ancestors(commit_result.genome_version)` 链 MUST be ≥ 2

#### Scenario: Case 3 gate denied 确定性回滚 (deterministic 非法 input)

- **GIVEN**: 真实 LLM provider + 构造缺必需字段的 proposal (缺 `tools_add` 或 `prompt_delta`)
- **OR**: 构造 `tools_add` 包含 ToolMetadata V3 白名单未注册的工具名
- **WHEN**: `executor.commit(bad_proposal)` 执行 Gate 0-3
- **THEN**:
  - Gate MUST return denied (确定性, 非 LLM 依赖)
  - `IGenomeRegistry` MUST NOT 调 `fork()` (零状态变更)
  - `genome.persist_failed` 事件 MUST trigger (per ADR-0068 Appendix A v2.3)
  - `genome.committed` 事件 MUST NOT trigger
- **AND**: 跑 ≥3 iter 验证 100% consistency (零 flaky)

#### Scenario: Case 4 drop_ratio descope (per R5)

- **GIVEN**: 本 change MUST descope Case 4 (per dual-agent 共识 + R5)
- **AND**: drop_ratio 红线 5% 触发验证属独立 OpenSpec change (per `.rddf/audits/2026-10-14-rddf-improvements-audit.md`)
- **WHEN**: N/A (无实施)
- **THEN**: 本 change MUST NOT 含 drop_ratio 断言

### Requirement: Recording Provider 守卫 (R2)

The system SHALL provide a `RecordingLLMProvider` (per AGENTS.md §5 守卫模式) that:

- MUST implement `ILLMProvider` 三虚 (`generate` / `generate_stream` / `available_models`)
- MUST 录制真实 LLM req/payload 到内存 (录制一次, CI 回放)
- MUST 在 CI `HYDRAFORGE_SKIP_REAL_LLM=1` 下回放录制, 验证 contract 行为
- MUST 在本地 `DEEPSEEK_API_KEY` set 时调真 LLM + 录制
- MUST 严格遵循 AGENTS.md §5 "production code 传给 provider 的参数契约" 模式
- MUST NOT 改 must_realllm 强制语义 (本地必须真 LLM, CI Recording 回放)

#### Scenario: Recording Provider 录制 + 回放

- **GIVEN**: 本地 `DEEPSEEK_API_KEY` set + 4 cases 跑过 (录制模式)
- **WHEN**: 切换 CI `HYDRAFORGE_SKIP_REAL_LLM=1` + 重跑 4 cases (回放模式)
- **THEN**: 4 cases MUST 全部 `SUCCEED` (or `WARN` + reason) — 验证 contract 行为在 CI 路径有效

### Requirement: hermetic HOME fixture (R3)

The system SHALL provide a `HermeticGenomeHome` test fixture that:

- MUST 测试 setup/teardown 隔离 `~/.hydraforge/genome.key` 宿主状态
- MUST `setenv("HOME", "/tmp/hermetic_<uuid>", 1)` + `setenv("HYDRAFORGE_HOME", "/tmp/hermetic_<uuid>", 1)`
- MUST 析构时恢复 `saved_home_` / `saved_hydraforge_home_` + 清理 tmpdir
- MUST 强制 fresh-deploy 路径 (per 2026-09-21 C1 genome.key 教训, 宿主状态掩盖 production bug)

#### Scenario: hermetic HOME fresh-deploy 路径测试

- **GIVEN**: 全新部署 (无既存 `~/.hydraforge/` 状态)
- **WHEN**: 跑 4 cases
- **THEN**: 测试 MUST 不 IOError, MUST 不依赖宿主 key 状态, MUST 真实 LLM commit → reload 路径完整

### Requirement: Case 3 gate denied 确定性构造 (R4)

The system MUST 实现 Case 3 deterministic 构造, MUST NOT 依赖真实 LLM 生成非法 proposal (LLM 输出不可控, flaky 风险高). Case 3 真实 LLM 失败回滚 MUST 用确定性非法输入构造.

**Critical redesign (per 2026-10-15 dual-agent 共识)**: 跑 ≥3 iter 验证 100% consistency (零 flaky).

#### Scenario: Gate 0 deterministic deny

- **GIVEN**: 真实 LLM provider + 构造 `tools_add` 包含 ToolMetadata V3 白名单未注册的工具名
- **WHEN**: `executor.commit(bad_proposal)` 执行 Gate 0-3
- **THEN**:
  - Gate MUST return denied (确定性, 非 LLM 依赖)
  - `IGenomeRegistry` MUST NOT 调 `fork()` (零状态变更)
  - `genome.persist_failed` 事件 MUST trigger
  - `genome.committed` 事件 MUST NOT trigger

#### Scenario: Missing required field deny

- **GIVEN**: 真实 LLM provider + 构造缺必需字段的 proposal (缺 `tools_add` 或 `prompt_delta`)
- **WHEN**: `executor.commit(bad_proposal)` 执行 Gate 0-3
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

### Requirement: InMemoryBus wait_for_drain() 守卫 (R6)

The system MUST 在 Case 2 commit 返回后, NOT 立即查 `genome.committed` 事件, MUST 先 `InMemoryBus::wait_for_drain()` 排空 (per AGENTS.md Pattern #9 contract drain API).

#### Scenario: wait_for_drain 守卫事件断言稳定

- **GIVEN**: 真实 LLM provider + 真实 LLM mutation commit 触发
- **WHEN**: `executor.commit(mutation)` → `bus->wait_for_drain()` → `bus->drain()`
- **THEN**:
  - 事件列表 MUST contain `genome.committed`
  - 事件断言 MUST 稳定 (无 race)

### Requirement: must_realllm label 复用既有机制 (R7)

新 test binary MUST `LABELS "must_realllm"` (per 2026-09-29 ship 机制).

#### Scenario: must_realllm label 强制验证

- **GIVEN**: 新 test binary `test_harness_mutation_commit_real_llm` 在 `tests/CMakeLists.txt::add_catch_test` 注册
- **WHEN**: `ctest --test-dir build -L must_realllm` 跑
- **THEN**:
  - CI 强制验证: skip=1 触发 FAIL (per AGENTS.md §3 短路不 FAIL 语义 — Recording Provider 必须替代)
  - 本地 `DEEPSEEK_API_KEY` set 时 4 cases MUST PASS

### Requirement: 0 源代码改动 (R8)

本 change MUST NOT 改源代码:

- MUST NOT 改 `src/modules/cognitive/harness_rsi.cpp` (Gate 0-3 + apply_harness_mutation 已有, per archive/genome-wiring-harness-rsi-gepa-2026-09-22/)
- MUST NOT 改 `src/core/genome/registry_filesystem.cpp` (fork/load/walk_ancestors 已有)
- MUST NOT 改 `src/core/genome/genome.h` (Genome / AttributionRecord 已有)
- MUST NOT 改 `src/common/llm/llm_types.h` (ILLMProvider 接口已有)
- 1 file 新增 (test) + 1 file CMake 改 (`tests/CMakeLists.txt`) + 1 file spec (本文件) + 1 file AGENTS.md entry

#### Scenario: 0 src/ 改动验证

- **GIVEN**: 本 change 实施完成
- **WHEN**: `git diff main --stat -- 'src/*' 'include/*'`
- **THEN**: 输出 MUST 为空 (0 src/ 改动)

## REMOVED Requirements

无. Genome wiring 闭环第 7 环 ship 时已定义 Requirements, 本 change MUST NOT 移除任何既有契约, 仅补 R3 元指标实证层.

## MODIFIED Requirements

无. Genome wiring 闭环第 7 环 ship 时已定义 Requirements, 本 change MUST NOT 修改既有契约, 仅补 R3 元指标实证层.

## 14 Acceptance Criteria 完整列表 (per proposal.md)

- AC-1: `tests/test_harness_mutation_commit_real_llm.cpp` 新建并入 CTest
- AC-2: `LABELS "must_realllm"`
- AC-3: 4 cases 全部 PASS at real LLM (DEEPSEEK_API_KEY)
- AC-4: 4 cases 全部 `SUCCEED` (or `WARN` + reason) at `HYDRAFORGE_SKIP_REAL_LLM=1`
- AC-5: Recording Provider 配对实现
- AC-6: hermetic HOME fixture 实现
- AC-7: Case 3 deterministic 构造通过 ≥3 iter 100% consistency
- AC-8: `tests/test_harness_rsi_pilot.cpp` 22 cases 零回归
- AC-9: `tests/test_genome_registry.cpp` 13 cases 零回归
- AC-10: `tests/test_harness_mutation_proposal_real_llm.cpp` 1 case 零回归
- AC-11: `ctest --test-dir build -L must_realllm` 0 失败 (本地有 key)
- AC-12: TSan / ASan 0 新增 error
- AC-13: ctest `--rerun-failed` flaky < 5%
- AC-14: AGENTS.md §Recent Changes 段登记本 ship (含 R3 元指标实证 claim)
