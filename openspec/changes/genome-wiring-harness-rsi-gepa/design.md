# Genome Wiring — Design (v2, post dual-agent review)

## Context

C2/C3/C4 三组件已 ship 但闭环第 7 环断裂（详见 proposal §Why）。本 design 基于 Oracle `bg_3c06ae5b` 接线分析 + **Metis `bg_687a5662` + Oracle `bg_534a2541` 双审修正 (2026-09-21)**。双审收敛信号: "搭车 ADR-0068 follow-up" 已 archive (Metis DB-1 = Oracle #8, 最高置信度) + 版本断言/root 引导交叉命中 (Oracle #2/#4 = Metis SF-10/SF-11)。

关键现状签名（已核实）：
- `apply_harness_mutation(const GenomeMutations&, std::string& system_prompt, std::vector<std::string>& tools, IToolRegistry&, const MutationGateContext&)` — `harness_rsi.h:61-66`
- `MutationGateContext { EvolutionState current; const AttributionRecord* attribution; IEvaluator* evaluator; IBudgetController* budget; IInteractionBus* bus; MutationGovernancePolicy policy; }` — `harness_rsi.h:36-43`
- `IGenomeRegistry::fork(name, parent_version, mutations)` → `Result<CommitResult, GenomeError>`；fork 内部 = load(parent) + `deep_merge_spec` + `commit`（`registry_filesystem.cpp:383-398`）
- `deep_merge_spec`（`:218-226`）：非空字段才覆盖 parent；**空 tools 继承 parent**（`if (!mutations.tools.empty()) result.tools = mutations.tools;`）
- `commit()` 版本号 = `max(既有版本)+1`（`:338-350`），**非 parent_version+1**
- `fork` 硬编码 `created_by="fork"`（`:411`）；load 对不存在 parent 返回 NotFound（`:278-280`）
- `GEPALoop::Config` — `gepa_loop.h:27-31`；成员全 `shared_ptr`（`:59-63`）；commit 分支 — `gepa_loop.cpp:171-188`；`result.success=true` 在 `:181`
- `MutationContext.version_id` 是调用方提供的**不透明字符串**（`mutation_record.h:50`）；governor `commit` 只校验白名单 + evaluation_refs + emit（`mutation_governor.cpp:189-210`）
- ADR-0068 Appendix A 是**纯文档表**（docs/adr/adr-0068-event-emission-contract.md），无运行时注册 API、无 topic-registry 测试；`genome.*` 主题全库 0 命中
- 测试先例: MockRegistry (`test_credit_assignment.cpp:200`, 5 纯虚 + walk_ancestors 默认) / StubMutationGovernor (`test_gepa_phase2.cpp:97-117`) / CapturingBus (`test_harness_rsi_pilot.cpp:99-119`) / hermetic env fixture (`test_genome_registry.cpp:30-41`)

## Goals / Non-Goals

**Goals**:
- 变异成功路径产生**可加载的 Genome 版本**（闭环第 7 环闭合）
- persist-before-apply：保持"失败路径零状态变更"不变量
- `genome.committed` / `genome.persist_failed` 事件可审计（含 trace_id 因果链）
- 最小 V1 回滚：自包含快照 undo + Genome 版本锚点

**Non-Goals**:
- 不改 `IMutationGovernor::revert` 的 audit-only 语义（ADR-0084 决策 5 红线）
- 不实现 ADR-0079 session-fork 回滚（独立 follow-up）
- 不恢复 `tools_remove` 已 unregister 的 ToolFunc（IToolRegistry 无 getter，V1 文档化限制）
- 不做 `load(genome@N) → 重建 ChatSession → 1 turn` E2E（留 harness-rsi-pilot V2）
- 不隐式解析 parent_version（V1 显式传参；解析 helper 留 V2）
- 不实现 registry scope 隔离（Wave 3）
- **不试图表达"清空全部工具"**（deep_merge_spec 空 tools 继承 parent 语义边界，见 D1 修正）

## Decisions

### D1: Gate 3 = persist-before-apply（双审修正后）

**决策**: `apply_harness_mutation` 门禁链为：
```
Gate 0 校验（含 workflow_patch 检查，纯校验无副作用）→ evaluate_readiness (Gate 1)
→ is_tool_allowed add+remove (Gate 2) → tools_add registry 预检 (Gate 2.5)
→ Genome 预提交 (Gate 3, persist-before-apply) → apply
```

**双审修正 #3 (Oracle 🔴)**: `workflow_patch → UnsupportedVariant` 检查**上移到 Gate 0**（与 `harness_rsi.cpp:83-100` 的 InvalidMutation 校验并列）。原设计"Gate 2.5 之后、Apply 之前"会让 workflow_patch 变异**先 fork 落盘成功、再返回 UnsupportedVariant** —— 失败路径磁盘已变，违反零状态变更不变量。

**双审修正 #1 (Oracle 🔴)**: 最终态 tools 为**空**时（remove 覆盖全部工具），`deep_merge_spec` 空列表继承 parent（`:221`）→ 落盘 Genome 与内存 tools 静默分叉。**V1 决策：最终态 tools 为空 → 返回 `InvalidMutation` 并文档化限制**（清空工具集在 pilot 语义下危险且无持久化表达；不采用"直接 commit 绕 deep_merge"方案——那会改变 fork 使用面，超出本 change）。

**版本语义**: 持久化后的版本号 = `fork` 返回的 `CommitResult.version`（= `max+1`），**禁止**在 spec/测试中写死 `parent_version+1`（双审修正 #2）。

**Gate 3 流程**（registry 非空时）：
1. 计算结果态 `GenomeSpec`: `harness = system_prompt + prompt_delta`；`tools = (tools + tools_add) − tools_remove`（**最终态**；为空 → InvalidMutation）
2. 检查 `genome_name` 非空 → 否则 `InvalidMutation`（Gate 0 级，事件零发射）
3. `fork(genome_name, parent_version, spec)` → 失败（NotFound/IOError/SchemaViolation）→ `RegistryRejected` + 零状态变更 + `genome.persist_failed`
4. 成功 → 记录 version → apply（必成功，预检已过）

**Alternatives**:
- (a) apply-then-persist → commit 失败留下不可审计状态，否决
- (b) 在 `MutationGovernor` 内持久化 → 违反 ADR-0084 划界，否决
- (c) 独立 orchestrator → YAGNI，否决

### D2: GEPA commit 分支持久化（双审修正后）

**决策**: `GEPALoop::Config` 增加 `std::shared_ptr<genome::IGenomeRegistry> genome_registry`（默认 nullptr）+ `genome_name` + `parent_version`。**persist-then-commit** 顺序（双审修正 #6, Oracle 🟠）：

```
1. fork(name, parent_version, GenomeSpec{harness = candidate.compiled_content, 其余继承})  ← result.success=true (:181) 之前
2. fork 失败 → gepa.commit.denied(genome_persist_failed) + result.success 保持 false
3. fork 成功 → proposal.version_id = name@N（先改写）→ governor_->commit(proposal)
   → mutation.committed 与后续 gepa.commit.committed 的 version_id 一致 = name@N
4. governor deny → 留孤儿 genome 版本（文档化为 dangling version，无引用即可清理）
```

**双审修正 #6**: 原设计"governor commit 成功后改写 version_id"造成双 id 不一致（`mutation.committed` 已带 reflection_id，`gepa.commit.committed` 带 name@N）。persist-then-commit 让两条审计事件版本一致。

**双审修正 #9/#DB-2**: `created_by` 经 fork 继承 parent（fork 硬编码 "fork"，`:411`）；不尝试 `created_by="gepa"`（经 fork 不可达）。GEPA 构造的 `GenomeSpec` 规则写入 spec（harness = candidate 全文、其余继承 parent），测试断言 `load(name, N).spec.harness == candidate.compiled_content`。

**命名空间（双审 SF-2 定案）**: **分离命名空间** —— harness_rsi 使用 `"chat_harness"`，GEPA 使用 `"gepa_skill"`，spec 场景用占位名。避免 `judge_data_freshness` lineage 混淆。

### D3: 2 个 genome.* 事件 + 直接修订 ADR-0068 Appendix A

**双审修正 #8 (Metis DB-1 = Oracle #8, 收敛)**: ~~搭车 `2026-09-20-adr-0068-appendix-a-evolution-themes`~~ —— 该 change **已 archive**（`archive/2026-09-21-2026-09-20-adr-0068-appendix-a-evolution-themes/`），无 active follow-up 可搭车。**本 change 直接修订 `docs/adr/adr-0068-event-emission-contract.md` Appendix A**：新增 `genome.committed` + `genome.persist_failed` 两行（Amendment v2.3）。主题登记从 docs-only 移入实现任务（3.5/3.7），加 grep 验收（`grep -c "genome.committed" ... >= 1`，合法 doc-grep AC，per 该 archived change 自身 AC-3 先例）。

**payload**: `genome.committed` = args{genome_name, version, parent, mutation_kind} + meta{trace_id（harness 侧取 ctx.trace_id，GEPA 侧取 failed_trace.trace_id）}；`genome.persist_failed` = args{genome_name, error(GenomeError 枚举名)} + meta{trace_id}。同一 mutation **至多发射一个** genome.* 事件（committed XOR persist_failed）。不新增 `genome.reverted`（回滚审计复用 `mutation.reverted`）。

### D4: 自包含快照 undo（双审修正后）

**双审修正 #5 (Oracle 🟠)**: 原设计"apply 在 Gate 3 前快照"无出口 —— 签名无 out-param、`AppliedMutation` 无快照字段，快照随返回销毁，`undo_applied_mutation` 拿不到。**修正: `AppliedMutation` 增加 `std::string prompt_snapshot` + `std::vector<std::string> tools_snapshot` 2 字段**（apply 成功时填充），`undo_applied_mutation(const AppliedMutation&, std::string&, std::vector<std::string>&, IToolRegistry*)` **从 AppliedMutation 读取快照** —— 自包含、跨作用域可用。

**V1 限制（双审 SF-5 明确断言）**: undo 恢复 prompt/tools 至快照；`registry.has_tool(removed)` **保持 false**（未恢复）。**后果文档化**: undo 后 `tools` 向量与 registry 分叉 —— 后续 mutation 的最终态 spec 会基于与 registry 不匹配的向量构建，调用方需在 undo 后显式重建状态。恢复锚点 = Genome `parent_version`。

**回滚审计**: undo 后调用方以 `target_version = "name@parent_version"` 调 `governor->revert()`（`mutation_governor.cpp:216`，白名单 fail-closed）→ emit `mutation.reverted`，target_version 为可加载 Genome 版本。

### D5: 与 C4 M2 的关系（双审 #13, Oracle 🟡）

C4 proposal M2 明确"返回类型改 `Result<AppliedMutation,…>`（**无 Genome, 无 IGenomeRegistry 依赖**）"（archive/2026-09-21-2026-09-16-harness-rsi-pilot/proposal.md:39）。本 change **不违反** M2 字面（返回类型不变，仍 `Result<AppliedMutation,…>`）与 ADR-0088 D4（未复活 IHarnessRSI 三算子接口，仍为轻量函数）。本 change 经 `MutationGateContext` **可选注入** registry —— 属演进非反转。**无需 ADR 修正案**，本 design 段即为演进注记。

## Risks / Trade-offs

- [tools 最终态误传增量] → R2 测试断言 `load().spec.tools == 期望最终集`（整体替换语义回归守卫）
- [清空全部工具不可表达] → D1 显式 `InvalidMutation` + spec 文档化（V1 边界）
- [parent_version 语义] → **定案**: MUST 指向已存在版本（≥1）；`0` 或缺失 → `InvalidMutation`（双审 SF-1）
- [GEPA 孤儿版本] → persist-then-commit 下 governor deny 留 dangling version —— 文档化为可清理无引用
- [harness 字段语义歧义] → C2 测试中 `spec.harness` 是**路径**（`test_genome_registry.cpp:454`），本 change 存**系统提示词全文**。spec 显式声明统一语义：harness 字符串（path 用法是 root-genome 特例）
- [undo 后向量/registry 分叉] → D4 文档化，调用方负责重建
- [与 harness-rsi-remove-governance 冲突] → **硬串行**；字段追加顺序 MUST 为 trace_id（remove-governance）→ genome_registry/genome_name/parent_version（本 change），两 change 不得重排既有字段（双审 SF-7）
- [测试依赖宿主机 key] → tasks 新增 hermetic env fixture（复制 test_genome_registry.cpp:30-41），防 C1 掩盖机制复现（Oracle #7）

## Migration Plan

1. **前置**: `harness-rsi-remove-governance` ship + archive（本 change 在其 commit 上 rebase）
2. 本 change 独立原子 commit：`MutationGateContext` +3 可空字段（末尾追加 + 默认值，非 BREAKING）；`AppliedMutation` +3 字段（snapshot ×2 + committed_genome_version，默认值兜底）
3. 回归守卫：`test_harness_rsi_pilot` 既有 9 cases / 43 assertions 零回归（registry=nullptr 路径逐字节一致）；`test_gepa_phase2` 既有 cases 零回归
4. 回滚：单 commit revert；registry 注入为可选参数，移除后回到 V1 行为

## Open Questions（双审已定案）

- ~~OQ1 parent_version 来源~~ → **定案**: 显式传参，0/缺失 = InvalidMutation；helper 留 V2
- ~~OQ2 命名空间~~ → **定案**: 分离命名空间 `chat_harness` / `gepa_skill`
- ~~OQ3 persist_failed 状态回退~~ → **定案**: V1 仅记录，状态由调用方决定
