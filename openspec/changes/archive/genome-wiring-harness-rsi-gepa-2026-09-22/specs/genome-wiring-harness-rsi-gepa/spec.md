# Genome Wiring — Harness-RSI + GEPA → IGenomeRegistry Spec (v2, post dual-agent review)

## ADDED Requirements

### Requirement: Genome 持久化上下文注入

`MutationGateContext` MUST 提供可空的 Genome 持久化上下文：`genome::IGenomeRegistry* genome_registry`（默认 `nullptr`）、`std::string genome_name`、`uint64_t parent_version`（默认 0）。当 `genome_registry == nullptr` 时，`apply_harness_mutation` 的行为 MUST 与 V1 逐字节一致（无持久化、无 genome.* 事件）。字段追加顺序 MUST 为 `trace_id`（由 harness-rsi-remove-governance 提供）→ `genome_registry` / `genome_name` / `parent_version`，两个 change 均 MUST NOT 重排既有字段。

#### Scenario: 未注入 registry 时行为不变
- **WHEN** `ctx.genome_registry == nullptr` 且 mutation 合法
- **THEN** `apply_harness_mutation` 返回 success 且 `AppliedMutation.committed_genome_version == 0`
- **AND** 不发射任何 `genome.*` 事件
- **AND** 既有 9 cases / 43 assertions 全部 PASS 且未修改

#### Scenario: 注入 registry 但缺 genome_name
- **WHEN** `ctx.genome_registry != nullptr` 且 `ctx.genome_name` 为空
- **THEN** 返回 `MutationError::InvalidMutation`（Gate 0 级校验，不调 fork）
- **AND** 零状态变更 + 零事件发射（`evolution.readiness.denied` 亦不发射）

#### Scenario: parent_version 为 0 或缺失
- **WHEN** `ctx.genome_registry != nullptr` 且 `ctx.parent_version == 0`
- **THEN** 返回 `MutationError::InvalidMutation`
- **AND** 零状态变更（`parent_version` MUST 指向已存在版本，≥1）

### Requirement: Root 引导路径

`IGenomeRegistry::fork(name, parent_version, …)` MUST 在 `parent_version` 指向不存在版本时失败（`load(parent)` → NotFound → `RegistryRejected`）。因此，**首次使用 MUST 由调用方先经 `commit()` 建立 root Genome 版本**（`metadata.parent` 为空、`capture_mode` 合法，否则 commit 自身返回 SchemaViolation）。

#### Scenario: fresh genome name 无 root 时 fork 失败
- **WHEN** `genome_name` 无任何已提交版本且调用 apply（parent_version 指向不存在版本）
- **THEN** 返回 `MutationError::RegistryRejected`
- **AND** 发射 `genome.persist_failed`（error = "NotFound"）
- **AND** 零状态变更

#### Scenario: root 已建立后第一次 apply 成功
- **WHEN** 调用方先 `commit()` 一个 root Genome（v1），再以 `parent_version = 1` 调 apply
- **THEN** 返回 success
- **AND** 磁盘存在 `genome_name@N` 其中 `N == fork 返回值.version`（fresh lineage 下 N == 2）

### Requirement: apply 前预提交（persist-before-apply）

当 `ctx.genome_registry != nullptr` 时，`apply_harness_mutation` MUST 在全部门禁通过后、内存 apply 之前调用 `fork(genome_name, parent_version, final_spec)`。门禁顺序 MUST 为：`Gate 0 校验（含 workflow_patch）→ evaluate_readiness → is_tool_allowed (add+remove) → tools_add registry 预检 → Genome 预提交 → apply`。fork 失败 MUST 返回 `MutationError::RegistryRejected` 且 prompt / tools / registry 零变更。`final_spec.harness` MUST 为 `system_prompt + prompt_delta`（**系统提示词全文**，非路径）；`final_spec.tools` MUST 为**最终态列表** `(tools + tools_add) − tools_remove`。

#### Scenario: 持久化成功
- **WHEN** registry 注入 + `parent_version` 指向已存在版本 + mutation 合法
- **THEN** 磁盘存在 `genome_name@<fork 返回的 CommitResult.version>` 且可 `load()` 回读
- **AND** `AppliedMutation.committed_genome_version == fork 返回的 version`（MUST NOT 断言 `parent_version+1`；fork 非 tip parent 时版本为 `max+1`）
- **AND** 内存 apply 正常完成（prompt/tools 已变）

#### Scenario: fork 失败零状态变更
- **WHEN** `parent_version` 指向不存在的版本（fork 返回 NotFound）
- **THEN** 返回 `MutationError::RegistryRejected`
- **AND** `system_prompt` / `tools` / registry 与调用前完全一致（本 scenario 守卫的是 persist-before-apply 顺序回归）
- **AND** 发射 `genome.persist_failed`

#### Scenario: tools 最终态语义
- **WHEN** 既有 `tools = {A, B}`，`tools_add = {C}`，`tools_remove = {A}`
- **THEN** `load(genome_name@N).spec.tools == {B, C}`（整体替换为最终态，非增量叠加）

#### Scenario: tools 最终态为空（V1 边界）
- **WHEN** `tools_remove` 覆盖全部既有工具（最终态 tools 为空列表）
- **THEN** 返回 `MutationError::InvalidMutation`
- **AND** 零状态变更（V1 不表达"清空全部工具"——`deep_merge_spec` 空 tools 继承 parent，落盘会与内存分叉）

#### Scenario: workflow_patch 先于 Genome 预提交
- **WHEN** mutation 含 `workflow_patch` 且 registry 已注入
- **THEN** 返回 `MutationError::UnsupportedVariant`（Gate 0 校验）
- **AND** 磁盘零新增版本（Genome 预提交未执行）

#### Scenario: fork 内部错误映射
- **WHEN** fork 因 SchemaViolation / IOError 失败（非 NotFound）
- **THEN** 返回 `MutationError::RegistryRejected`
- **AND** `genome.persist_failed` 的 error 字段为该 GenomeError 枚举名

### Requirement: genome 事件发射与主题登记

持久化成功 MUST 经 `ctx.bus` 发射 `genome.committed`（args: `genome_name` / `version` / `parent` / `mutation_kind`；meta: `trace_id` 取自 `ctx.trace_id`——该字段由 harness-rsi-remove-governance 提供）。持久化失败 MUST 发射 `genome.persist_failed`（args: `genome_name` / `error`（`GenomeError` 枚举名如 "NotFound"/"IOError"/"SchemaViolation"）；meta: `trace_id`）。同一 mutation MUST 至多发射一个 genome.* 事件（committed XOR persist_failed）。两个主题 MUST 直接登记入 `docs/adr/adr-0068-event-emission-contract.md` Appendix A（Amendment v2.3，本 change 自行修订——无可搭车的 active follow-up）。

#### Scenario: genome.committed 携带 trace_id
- **WHEN** `ctx.trace_id = "trace-xyz"` 且 fork 成功
- **THEN** `genome.committed` 事件 meta.trace_id == "trace-xyz"
- **AND** args.version == fork 返回的版本号

#### Scenario: bus 为空时不崩溃
- **WHEN** `ctx.bus == nullptr` 且 fork 成功
- **THEN** 持久化正常完成，无事件发射，无异常

#### Scenario: ADR-0068 Appendix A 登记
- **WHEN** 本 change ship 后检查 Appendix A
- **THEN** `grep -c "genome.committed" docs/adr/adr-0068-event-emission-contract.md >= 1`
- **AND** `grep -c "genome.persist_failed" docs/adr/adr-0068-event-emission-contract.md >= 1`

### Requirement: GEPA 候选持久化

`GEPALoop::Config` MUST 提供可空的 `genome::IGenomeRegistry` 注入（默认 nullptr）+ `genome_name` + `parent_version`。当 registry 注入时，fork MUST 发生在 `result.success = true`（`gepa_loop.cpp:181`）**之前**，且 MUST 为 **persist-then-commit** 顺序：fork 成功 → 改写 `proposal.version_id = genome_name + "@" + version` → 再调 `governor_->commit(proposal)`。GEPA 构造的 `GenomeSpec` MUST 为 `{harness = candidate.compiled_content（全文）, tools = {}, budget = 0, model_routing = "", prompt_cache_prefix = ""}`，`created_by`/`capture_mode` 经 fork 继承 parent。成功后 `gepa.commit.committed` payload MUST 含 `genome_version`。fork 失败 MUST 走 `gepa.commit.denied`（reason="genome_persist_failed"）且 `result.success` 保持 false、`result.candidate_skills` 不记录该候选。**命名空间 MUST 分离**: harness_rsi 用 `"chat_harness"`，GEPA 用 `"gepa_skill"`。

#### Scenario: GEPA commit 持久化成功
- **WHEN** registry 注入 + fork 成功 + governor commit 成功
- **THEN** `result.success == true`
- **AND** `proposal.version_id == "genome_name@N"`（N 为 fork 返回版本）
- **AND** `load(genome_name, N).spec.harness == candidate.compiled_content`（内容回读断言）
- **AND** `gepa.commit.committed` payload 含 `genome_version == N`

#### Scenario: 两条审计事件版本一致
- **WHEN** persist-then-commit 顺序下 governor commit 成功
- **THEN** `mutation.committed` 的 version_id == `gepa.commit.committed` 的 version_id == `"genome_name@N"`

#### Scenario: GEPA fork 失败视为未提交
- **WHEN** registry 注入但 fork 失败（如 parent_version 不存在）
- **THEN** `result.success == false`（fork 在 `:181` 之前）
- **AND** 发射 `gepa.commit.denied`（reason="genome_persist_failed"）
- **AND** `result.candidate_skills` 不记录该候选

#### Scenario: governor deny 留孤儿版本
- **WHEN** fork 成功但 `governor_->commit` deny
- **THEN** 磁盘存在无引用的 genome 版本（dangling version，文档化为可清理）
- **AND** `result.success == false`

#### Scenario: 未注入 registry 时 GEPA 行为不变
- **WHEN** `Config.genome_registry == nullptr`
- **THEN** `proposal.version_id` 保持 V1 语义（`reflection_id`）
- **AND** 既有 GEPA 测试零回归

### Requirement: 自包含快照 undo 与 V1 回滚锚点

`AppliedMutation` MUST 携带 `std::string prompt_snapshot` + `std::vector<std::string> tools_snapshot`（apply 成功时填充）。`undo_applied_mutation(const AppliedMutation&, std::string& system_prompt, std::vector<std::string>& tools, IToolRegistry*)` MUST 从 `AppliedMutation` 读取快照并恢复。**V1 限制 MUST 文档化**: `tools_remove` 已 unregister 的 `ToolFunc` 无法经 `IToolRegistry` 恢复（接口无 getter）；undo 后 `tools` 向量与 registry **分叉**，调用方 MUST 显式重建。恢复锚点 MUST 为 Genome `parent_version`。回滚审计 MUST 复用既有 `IMutationGovernor::revert()`（audit-only，ADR-0084 决策 5 红线不变）。

#### Scenario: undo 恢复 prompt 与 tools
- **WHEN** 应用 mutation（prompt_delta + tools_add）后调用 `undo_applied_mutation()`
- **THEN** `system_prompt` / `tools` 等于 `AppliedMutation` 中的快照

#### Scenario: tools_remove 的 V1 限制
- **WHEN** mutation 含 `tools_remove` 且 undo
- **THEN** `registry.has_tool(removed) == false`（保持移除，未恢复）
- **AND** `system_prompt` / `tools` 恢复至快照（tools 向量与 registry 分叉，文档化）

#### Scenario: 回滚审计使用可加载版本号
- **WHEN** undo 后调用方以 `target_version = "name@parent_version"` 调 `governor->revert()`
- **THEN** `mutation.reverted` 事件发射
- **AND** `target_version` 可经 `IGenomeRegistry::load()` 解析（非不可加载的不透明字符串）

#### Scenario: undo 后锚点保持
- **WHEN** undo 完成
- **THEN** `AppliedMutation.committed_genome_version` 保持原值（锚点 = 已持久化的版本，供重新物化）

### Requirement: ADR-0084 V1 红线保持

本 change MUST NOT 修改以下不变量：(a) `IMutationGovernor::revert()` 保持 audit-only（不恢复状态）；(b) L4 权重变异保持 emit-then-throw 禁止；(c) 门禁顺序保持 `Gate 0（含 workflow_patch）→ evaluate_readiness → is_tool_allowed (add+remove) → registry 预检 → Genome 预提交 → apply`；(d) 变异来源白名单保持 fail-closed（registry 注入不得绕过 `policy.denied_tools`）。

#### Scenario: registry 注入不绕过白名单
- **WHEN** `genome_registry` 注入且 `mutations.tools_add` 含 `policy.denied_tools` 中的工具
- **THEN** 返回 `GovernanceDenied`（Gate 2 先于 Gate 3）
- **AND** 无 Genome 版本产生

#### Scenario: L4 emit-then-throw 回归守卫
- **WHEN** mutation_kind 为 `L4_weights`（L4 权重路径）
- **THEN** `mutation.denied`（reason=`l4_forbidden_v1`）在 throw **之前**发射
- **AND** 无 `genome.*` 事件发射（fork 从未调用）
- **AND** 断言镜像既有 `test_mutation_governance.cpp` L4 case（真实回归守卫，非 prose）

## Out of Scope

- `load(genome@N) → 重建 ChatSession → 1 turn` E2E（留 harness-rsi-pilot V2）
- registry scope 隔离（跨 agent 工具隔离，Wave 3）
- `parent_version` 自动解析 helper（V2）
- `IMutationGovernor::revert` 语义变更（ADR-0084 决策 5 红线）
- `tools_remove` registry 恢复（IToolRegistry 无 getter，V1 限制）
- "清空全部工具"的持久化表达（deep_merge_spec 空列表继承 parent，V1 返回 InvalidMutation）
