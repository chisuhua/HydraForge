# Design: adr-0084-mutation-governance-contract

> 承接 2026-08-26 Oracle 评审发现对 ADR-0084 契约的收口设计。仓库约定允许 change 携带 design.md（参见 `openspec/changes/from-roadmap-phase-6c-*/design.md`）。
> 治理范式：single-developer mode（AGENTS.md），全部门禁由自审 + 测试断言承担，无团队评审流程假设。

## Context

ADR-0084 起草稿存在 4 处 Oracle 判定的不可实施/不可验证点：

1. **范围蔓延**：决策 5 引入"24h 保留窗口 + subject 恢复"，隐含 governor 需要版本存储与恢复机制 — 超出门禁契约本身。
2. **幻影注册点**：proposal/tasks 规划 `mutation_topics.cpp` 做主题注册，但全库 grep 确认 EventBuilder 仅为 payload builder，无 `register_topic` / `TopicRegistry` API — 该编译单元无事可做。
3. **L3 语义不统一**：ADR 决策 2 矩阵 "agent+L3 人类复核" 与 spec scenario "plan+L3 → human_review_required" 互相矛盾，且未指定复核调用点。
4. **不可验证断言**：固定 ctest 总数（185/185）、`evaluation_refs` 语义未定义、审计事件顺序未定义（尤其 L4 拒绝与抛异常的先后）。

## Decisions

### D1 — V1 = gate-and-audit contract only

V1 契约职责严格二分：

| 职责 | V1 |
|---|---|
| 门禁（whitelist → L4 拒绝 → 模式矩阵 → 审批 → 评估 → 回归） | ✅ |
| 审计（4 mutation.* 事件发射，payload 完整可验证） | ✅ |
| subject 版本存储 | ❌ |
| subject 恢复 | ❌ |
| 保留窗口（24h 等） | ❌ |
| 可操作 revert recovery | ❌（`revert()` 为纯审计记录 API） |

实际恢复动作由调用方经 ADR-0079 session fork 完成，不属于本契约。`subject_ref` / `version_id` / `target_version` 均为**调用方提供的不透明字符串**，governor 透传不解释、不持久化。

### D2 — 无运行时主题注册；ADR-0068 附录 A 为文档注册

EventBuilder（`include/agenticdsl/contract/event_builder.h`）确认无注册 API。因此：
- **删除** `mutation_topics.cpp` 任务；
- 4 个 mutation.* 主题仅在 ADR-0068 附录 A 登记 payload schema（文档注册）；
- 计数同步路径为 `docs/architecture/adr-implementation-status-gap-analysis.md` §四（修正早期 tasks 中错误的 `tools/adr_implementation_status_gap_analysis.md` 路径）。

### D3 — L3 语义统一

| 模式 + L3 | 行为 | denial_reason |
|---|---|---|
| yolo + L3 | 拒绝 | `plan_required`（矩阵统一规则） |
| plan + L3 | 拒绝 | `plan_insufficient` |
| agent + L3 | 调用 `IApprovalHandler::process_request(meta, ctx, preview)`；仅 true 继续 | false → `approval_denied`；handler 缺失 → `approval_handler_unavailable`（fail-closed） |

调用点复用既有 `include/agenticdsl/policy/iapproval_handler.h`（Sprint 19 抽象），governor 持 `IApprovalHandler*` 非拥有指针（与 NodeExecutor 模式一致）。

### D4 — evaluation_refs = 不透明 evaluation_id 字符串数组

- `MutationContext.evaluation_refs: std::vector<std::string>`，元素为 evaluation_id，由 IEvaluator 契约层（ADR-0083 评估环节）产出与消费；
- governor **仅透传**，不解析、不生成、不验证格式；
- `commit()` 时为空 → fail-closed `denial_reason="missing_evaluation_refs"` + `failed_step="evaluation"`；
- 与 ADR-0083 决策 4 的 `evaluation.result` 事件记录的关联由评估层负责。

### D5 — 来源白名单：内容 / 所有权 / 注入 / fail-closed

- **内容**：部署方声明的 source_id 字符串集合（如 `R_T19_GEPA` / `R_T20_AFLOW`）；
- **所有权**：编排应用（R 轨任务 runner），非 governor；
- **注入**：构造函数参数 `std::unordered_set<std::string>`，构造后不可变（V1 无运行时增删 API）；
- **fail-closed**：source_id 缺失 / 空串 / 不在集合 → `denial_reason="non_whitelisted_source"` + `failed_step="source_whitelist"`，且**不执行任何后续门禁**；默认空白名单 = 全部拒绝。

### D6 — 审计事件顺序（门禁链全序）

每次 `propose()` / `commit()` / `revert()` 调用**恰好发射一个**终态 mutation.* 事件。门禁链顺序：

```
propose:  whitelist(D5) → L4 拒绝 → 模式矩阵(D3) → agent+L3 审批(D3) → IEvaluator 评估门 → 行为回归门 → emit proposed
commit:   evaluation_refs 非空校验(D4) → emit committed
revert:   emit reverted（audit-only, D1）
任意失败: emit denied（含 denial_reason + failed_step），不 emit 后续事件
L4:       emit denied(l4_forbidden_v1) 严格先于 std::runtime_error 抛出（emit-then-throw；测试先捕获事件再 catch 异常验证）
```

### D7 — 启动依赖显式化

- `MutationGovernor` 构造签名要求非空 `std::shared_ptr<IEvaluator>`；nullptr → `std::invalid_argument`（fail-fast，无可绕过路径）；
- 因此 OpenSpec `2026-08-26-ship-ievaluator-reward-contract` 为本 change **硬前置**（用户批准的执行顺序：IEvaluator 先 ship，变异治理后启动）；
- T17 SkillCompiler **降级**为后续 L3 producer 集成依赖：本契约测试使用合成 MutationContext，不经过 SkillCompiler 产物。

### D8 — 可客观验证的测试与 ship gate

- 全部断言面向：topic 字符串、payload 字段值、事件发射顺序、返回结果、异常类型（`REQUIRE_THROWS_AS`）；
- ctest 基线为**动态措辞**（"启动时 main 实测计数 + 净增"），禁止硬编码 185/185 等快照数字；
- 范围外显式列举：ADR 状态翻转、G11 翻 ✅、issue #14 关闭、共享架构文档更新、T17 接线。

## Risks

| 风险 | 缓解 |
|---|---|
| V1 revert 仅审计不恢复，调用方误以为已恢复 | spec Requirement #2 revert scenario 显式声明；ADR-0084 §决策 5 同步重写 |
| evaluation_refs 两端（IEvaluator / governor）语义漂移 | D4 规定 governor 不透明透传，格式所有权归 ADR-0083 |
| 白名单部署方忘配 → 全部拒绝 | D5 默认空白名单 = fail-closed，属预期安全行为 |
| L3 审批 handler 注入遗漏 | D3 handler=nullptr → fail-closed `approval_handler_unavailable` |
