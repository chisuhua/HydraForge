# Genome Wiring — Harness-RSI + GEPA → IGenomeRegistry — Proposal (v2, post dual-agent review)

## Why

Oracle 深度审查 (session `bg_3c06ae5b`, 2026-09-21) 确认**自进化闭环第 7 环"版本提交/发布"端到端断裂**：C2 Genome Registry (2026-09-19 ship) / C3 Transition Guard (2026-09-20 ship) / C4 Harness-RSI Pilot (2026-09-21 ship) 三个组件各自可运行，但**不构成闭环**：

- `apply_harness_mutation` (`src/evolution/harness_rsi.cpp:149-179`) 只改内存，`MutationGateContext` (`harness_rsi.h:36-43`) 无 registry 参数 → **变异结果无版本、不可追溯、不可回滚**
- `GEPALoop::reflect_and_commit` (`src/modules/cognitive/gepa_loop.cpp:171-188`) 的 `governor_->commit(proposal)` 只 emit 审计事件，候选 skill 文本仅存于 `result.candidate_skills` (L182) → **"提交"后无物可加载**
- 全生产树 `IGenomeRegistry` 唯一调用方是 `src/evolution/version_pair_diff.cpp:43,56`；`create_filesystem`/`commit`/`fork` **零生产调用点**

后果：ADR-0086 v1.1 `HarnessChange` confounder 检测永远拿不到 harness 版本数据；ADR-0084 决策 5 的"回滚由调用方负责"无版本锚点；C4 的"Mock 闭环"不可重放。**修复不需要新组件**——只需把已 ship 的 `IGenomeRegistry::fork` 接到两个既有成功路径上。**v2 已按 Metis + Oracle 双审修正**（2026-09-21，模式 #8）：搭车对象已 archive → 直接修订 ADR-0068；版本断言改为 fork 返回值；补 root 引导路径；workflow_patch 上移 Gate 0；undo 快照自包含；GEPA 改 persist-then-commit。

## What Changes

- **apply_harness_mutation Gate 3 (persist-before-apply)**: `MutationGateContext` 增加可空 `genome_registry` / `genome_name` / `parent_version`；registry 非空时，全部门禁通过后、内存 apply 之前调 `fork(genome_name, parent_version, final_spec)`。fork 失败 → `RegistryRejected` + 零状态变更；**workflow_patch 检查上移 Gate 0**；**最终态 tools 为空 → InvalidMutation**（V1 边界）
- **Root 引导**: 首次使用 MUST 由调用方先 `commit()` root Genome（fresh name 上 fork 必 NotFound）；`parent_version` MUST ≥1（0 → InvalidMutation）
- **GEPA commit 分支持久化 (persist-then-commit)**: `GEPALoop::Config` 可注入 registry；fork 在 `result.success=true` 前，成功后 `proposal.version_id = name@N` 再 governor commit（两条审计事件版本一致）；fork 失败 → `gepa.commit.denied` + success=false
- **2 个新事件**: `genome.committed` / `genome.persist_failed`（**直接修订** ADR-0068 Appendix A v2.3）
- **自包含快照 undo**: `AppliedMutation` 增加 `prompt_snapshot`/`tools_snapshot`/`committed_genome_version` 字段；`undo_applied_mutation()` 从 AppliedMutation 读快照；`tools_remove` registry 恢复标注 V1 限制（has_tool 保持 false），恢复锚点 = Genome parent 版本

## Capabilities

### New Capabilities
- `genome-wiring-harness-rsi-gepa`: 把 Genome Registry 接入 Harness-RSI 与 GEPA 两条变异成功路径 — persist-before-apply 顺序、root 引导、genome.* 事件、自包含快照 undo、V1 回滚锚点。覆盖自进化闭环第 7 环"版本提交/发布"

### Modified Capabilities
- `harness-rsi-pilot`: `apply_harness_mutation` 在 registry 注入时新增 Gate 3 持久化阶段（registry=nullptr 时行为与 V1 逐字节一致）
- `t19-gepa-phase2-commit`: `GEPALoop` commit 分支持久化候选为 Genome 版本（persist-then-commit）

## Impact

- `include/agenticdsl/evolution/harness_rsi.h` — `MutationGateContext` +3 可空字段（末尾追加, 非 BREAKING）+ `AppliedMutation` +3 字段 + `undo_applied_mutation` 声明
- `src/evolution/harness_rsi.cpp` — Gate 0 workflow_patch 上移 + Gate 3 持久化 + 2 事件发射 + undo 实现
- `include/agenticdsl/cognitive/gepa_loop.h` + `src/modules/cognitive/gepa_loop.cpp` — Config registry 注入 + persist-then-commit 分支持久化 + version_id 改写
- `docs/adr/adr-0068-event-emission-contract.md` — **Appendix A Amendment v2.3**: 登记 2 个 `genome.*` 主题（直接修订，非搭车）
- `tests/test_harness_rsi_pilot.cpp` — hermetic env fixture + Gate 3 七态测试 (7a-7h) + undo 测试 + L4 回归守卫
- `tests/test_gepa_phase2.cpp` — GEPA 持久化三态测试 (9/9b/9c)
- **依赖**: `harness-rsi-remove-governance` (active, 同改 `MutationGateContext`) MUST 先 ship；本 change 在其上 rebase（trace_id 字段被 R3 的 meta.trace_id 复用）
