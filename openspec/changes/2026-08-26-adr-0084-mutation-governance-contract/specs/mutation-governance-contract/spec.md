# mutation-governance-contract Specification

## ADDED Requirements

### Requirement: IMutationGovernor 是变异治理的门禁与审计抽象接口 (V1 = gate-and-audit only)

The IMutationGovernor MUST provide a pure virtual interface for gating (`propose` / `commit`) and audit-recording (`revert`) mutations, emitting exactly one terminal `mutation.*` audit event per call. V1 MUST NOT store subject versions, MUST NOT restore subjects, MUST NOT enforce any retention window, and MUST NOT implement operational revert recovery.

#### Scenario: L1 prompt 变异 happy path (yolo 模式)

- **GIVEN** MutationContext{subject_ref="prompt_v1", mutation_kind="L1_prompt", source_id="R_T19_GEPA", evaluation_refs=["eval-001"]}
- **AND** source_id 在白名单内
- **AND** ExecutionPolicy mode = yolo
- **AND** 注入的 IEvaluator 对 proposed_change 评估返回 RewardSignal.quality=Excellent
- **AND** 行为回归门 (ADR-0061-02) Verdict=Pass
- **WHEN** 调用 `governor->propose()` 成功后调用 `governor->commit()`
- **THEN** 按顺序 emit `mutation.proposed` → `mutation.committed`
- **AND** `mutation.committed` payload 原样透传 `evaluation_refs=["eval-001"]`（不透明字符串数组，governor 不解释其内容）
- **AND** `mutation.committed` payload 含 `mutation_kind="L1_prompt"` 与调用方提供的 `subject_ref` / `version_id`

#### Scenario: L2 DSL 变异在 yolo 模式被拒绝

- **GIVEN** MutationContext{mutation_kind="L2_dsl", source_id=<白名单内>, evaluation_refs 非空}
- **AND** ExecutionPolicy mode = yolo
- **WHEN** 调用 `governor->propose()`
- **THEN** 返回拒绝结果（不抛异常）
- **AND** emit `mutation.denied` 含 `denial_reason="plan_required"` + `failed_step="authorization"`
- **AND** 不 emit `mutation.proposed`，后续不可 `commit()`

#### Scenario: L3 SKILL.md 变异在 plan 模式被拒绝 (plan_insufficient)

- **GIVEN** MutationContext{mutation_kind="L3_skill", source_id=<白名单内>}
- **AND** ExecutionPolicy mode = plan
- **WHEN** 调用 `governor->propose()`
- **THEN** 返回拒绝结果（不抛异常）
- **AND** emit `mutation.denied` 含 `denial_reason="plan_insufficient"` + `failed_step="authorization"`
- **AND** 不调用 IApprovalHandler，不 emit `mutation.proposed`

#### Scenario: L3 SKILL.md 变异在 agent 模式必须经 IApprovalHandler 人类复核

- **GIVEN** MutationContext{mutation_kind="L3_skill", source_id=<白名单内>}
- **AND** ExecutionPolicy mode = agent
- **AND** governor 持有非空 `IApprovalHandler*` (`include/agenticdsl/policy/iapproval_handler.h`)
- **WHEN** 调用 `governor->propose()`
- **THEN** 调用 `IApprovalHandler::process_request(meta, ctx, preview)` 恰好一次
- **AND** 仅当 `process_request` 返回 true 时才继续后续评估/回归门禁并 emit `mutation.proposed`
- **AND** 当 `process_request` 返回 false 时 emit `mutation.denied` 含 `denial_reason="approval_denied"` + `failed_step="human_review"`，不 emit `mutation.proposed`

#### Scenario: agent+L3 但 IApprovalHandler 未注入 → fail-closed

- **GIVEN** MutationContext{mutation_kind="L3_skill", source_id=<白名单内>}
- **AND** ExecutionPolicy mode = agent
- **AND** governor 的 `IApprovalHandler*` 为 nullptr
- **WHEN** 调用 `governor->propose()`
- **THEN** emit `mutation.denied` 含 `denial_reason="approval_handler_unavailable"` + `failed_step="human_review"`
- **AND** 不 emit `mutation.proposed`

#### Scenario: L4 权重变异在 V1 显式拒绝 (emit-then-throw)

- **GIVEN** MutationContext{mutation_kind="L4_weight", source_id=<白名单内>} + 任意 ExecutionPolicy mode
- **WHEN** 调用 `governor->propose()`
- **THEN** 先 emit `mutation.denied` 含 `denial_reason="l4_forbidden_v1"` + `failed_step="authorization"`
- **AND** 随后抛出 `std::runtime_error`（消息含 "L4 weight mutation forbidden in V1"）
- **AND** 事件发射严格先于异常抛出（测试通过捕获事件后再 catch 异常验证顺序）

### Requirement: mutation.* 审计事件发射与顺序

Each `propose()` / `commit()` / `revert()` call MUST emit exactly one terminal mutation.* event. Denial events MUST be emitted before any exception escapes, and `mutation.proposed` MUST precede `mutation.committed` for the same mutation attempt. All payload assertions in tests MUST be objectively verifiable (topic 字符串 + payload 字段值 + 发射顺序)。

#### Scenario: 审计事件完整性 (propose → commit)

- **GIVEN** L1 happy path（per Requirement #1 Scenario #1）
- **WHEN** 完整走 propose → commit 流程
- **THEN** EventLog/总线按时间顺序含 `mutation.proposed` → `mutation.committed`
- **AND** `mutation.committed` payload 含非空 `evaluation_refs` 数组字段
- **AND** 同一 mutation attempt 不出现第 3 个 mutation.* 事件

#### Scenario: 评估门失败产生可定位的拒绝事件

- **GIVEN** L1 变异 + IEvaluator 返回 RewardSignal.quality=Poor
- **WHEN** 调用 `governor->propose()`（评估门在 propose 阶段内执行）
- **THEN** emit `mutation.denied` 含 `failed_step="evaluation"` + 非空 `denial_reason`
- **AND** 不 emit `mutation.proposed` / `mutation.committed`

#### Scenario: 行为回归门失败产生可定位的拒绝事件

- **GIVEN** L1 变异 + IEvaluator 返回 Excellent + 行为回归门 Verdict=Fail
- **WHEN** 走治理流程
- **THEN** emit `mutation.denied` 含 `failed_step="behavioral_regression"` + 非空 `denial_reason`
- **AND** 不 emit `mutation.committed`

#### Scenario: commit 缺少 evaluation_refs → fail-closed

- **GIVEN** propose 已通过，但 MutationContext.evaluation_refs 为空数组
- **WHEN** 调用 `governor->commit()`
- **THEN** emit `mutation.denied` 含 `denial_reason="missing_evaluation_refs"` + `failed_step="evaluation"`
- **AND** 不 emit `mutation.committed`

#### Scenario: revert 为纯审计记录 (audit-only)，不恢复 subject

- **GIVEN** 一次已 committed 的 L1 变异
- **WHEN** 调用 `governor->revert(target_version="prompt_v1", rollback_reason="regression")`
- **THEN** emit `mutation.reverted` 含 `rollback_reason` + `target_version`（均为调用方提供的不透明标识）
- **AND** governor 不读取/不修改/不恢复任何 subject 内容（V1 无版本存储，无可恢复状态）
- **AND** 实际恢复动作（如 ADR-0079 session fork）由调用方负责，不属于本契约

### Requirement: 变异来源白名单 (fail-closed)

The MutationGovernor MUST receive its source whitelist as an immutable constructor-injected set of source_id strings, owned by the deploying application (e.g., R-track task runner). Any propose/commit/revert whose source_id is absent from the whitelist (including empty string) MUST be denied with `denial_reason="non_whitelisted_source"` before any other gate executes.

#### Scenario: 白名单注入与所有权

- **GIVEN** 部署方构造 governor 时注入白名单 `{"R_T19_GEPA", "R_T20_AFLOW"}`
- **WHEN** 以 source_id="R_T19_GEPA" 调用 `propose()`
- **THEN** 通过白名单检查，进入下一门禁
- **AND** 白名单在构造后不可变（V1 无运行时增删 API）

#### Scenario: 空 source_id → fail-closed

- **GIVEN** MutationContext.source_id = "" (空字符串)
- **WHEN** 调用 `governor->propose()`
- **THEN** emit `mutation.denied` 含 `denial_reason="non_whitelisted_source"` + `failed_step="source_whitelist"`
- **AND** 不执行任何后续门禁（模式矩阵 / 审批 / 评估 / 回归）

#### Scenario: 未注册 source_id → fail-closed

- **GIVEN** 白名单 `{"R_T19_GEPA"}` + MutationContext.source_id = "external_user_input"
- **WHEN** 调用 `governor->propose()`
- **THEN** emit `mutation.denied` 含 `denial_reason="non_whitelisted_source"`
- **AND** 默认构造（空白名单）等价于全部拒绝

### Requirement: V1 边界 (gate-and-audit only, per ADR-0084 §决策 1/5)

V1 MUST implement the L1-L3 gate matrix and L4 rejection, and MUST NOT include: subject version storage, subject restoration, retention-window enforcement (e.g., 24h), or operational revert recovery.

#### Scenario: V1 支持 L1/L2/L3 门禁 + 禁止 L4

- **GIVEN** ADR-0084 §决策 1 V1 边界
- **THEN** `governor->propose()` 接受 mutation_kind ∈ {"L1_prompt", "L2_dsl", "L3_skill"} 进入模式矩阵判定
- **AND** `governor->propose()` 拒绝 mutation_kind="L4_weight"（emit denied 后抛异常）

#### Scenario: V1 不持久化 subject 版本

- **GIVEN** 任意成功的 propose → commit 流程
- **THEN** governor 内部不存在 subject 内容/版本存储（`subject_ref` / `version_id` 仅为 payload 透传的不透明字符串）
- **AND** 无 24h 或任何时长保留窗口逻辑

#### Scenario: V1 边界终止条件

- **GIVEN** ADR-0078 Fine-tune ✅ Approved 之后
- **THEN** V2 OpenSpec follow-up change 可扩展 L4 支持与可操作的 revert recovery
- **AND** 本 change V1 边界解除（via ADR-0084 amendment）

### Requirement: 启动依赖 — IEvaluator 必须先行 ship

The MutationGovernor V1 implementation MUST require a non-null `std::shared_ptr<IEvaluator>` (ADR-0083) at construction and MUST fail fast (throw `std::invalid_argument`) when constructed with nullptr. T17 SkillCompiler MUST NOT be a startup dependency of this gate/audit contract; it is a later L3 producer-integration dependency only.

#### Scenario: 构造时 IEvaluator 为空 → fail-fast

- **GIVEN** 以 `shared_ptr<IEvaluator>(nullptr)` 构造 MutationGovernor
- **WHEN** 调用构造函数
- **THEN** 抛出 `std::invalid_argument`（消息含 "IEvaluator"）
- **AND** 不存在任何可绕过评估门运行的构造路径

#### Scenario: 契约 ship 不依赖 T17

- **GIVEN** T17 SkillCompiler 未 ship
- **WHEN** 运行 `tests/test_mutation_governance.cpp`
- **THEN** 全部用例通过（L3 用例使用合成 MutationContext，不经过 SkillCompiler 产物）
- **AND** T17 集成仅作为后续 producer-wiring change 的依赖
