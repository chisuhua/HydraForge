# Proposal: IGenomeRegistry walk_ancestors + judge_data_freshness 完整实装 (Sprint 34+ follow-up)

> **STATUS**: DRAFT (follow-up registered per Oracle NEEDS_FIX verdict 2026-09-20)
> **Type**: follow-up to ADR-0088 v1.0 ship (commit `0ffc637` + `7a15744`)
> **优先级**: P0 (C4 harness-rsi-pilot 真实前置; 阻塞 readiness gate 条件 1 真实生效)
> **估时**: 2-3 天 (含 Oracle dual-agent review + 6+ atomic commits per AGENTS.md 模式 #4)

## Why (背景)

ADR-0088 v1.0 ship (2026-09-20) 完成了 D1-D3 状态机 + D7 复用契约 + D8 字符串常量。但 Oracle 审查 (NEEDS_FIX, ALIGNMENT SCORE 62) 发现:

- **D5 walk_ancestors 公开接口扩展**: IGenomeRegistry 当前仍是 5 公共方法 (load/commit/fork/list_versions/diff), walk_ancestors 未实现。FilesystemGenomeRegistry 也无 override。
- **D6 judge_data_freshness 完整实装**: `version_pair_diff.cpp:48` 仍返回 stub `AttributionVerdict::Insufficient`, 条件 1 的 HarnessChange 检测实际不工作。
- **D9 walk_ancestors 默认实现**: 当前头文件无 walk_ancestors 接口 (ADR-0088 D9 虚报 ship)。

**没有这些 ship 的后果**:
- C4 harness-rsi-pilot 的 `evaluate_readiness()` 条件 1 (Attributed) 恒走 stub=Insufficient fail-closed, pilot 验证的是空壳
- D8 事件主题常量 `evolution.transition.denied` / `evolution.readiness.denied` 无 walk_ancestors 触发条件 (无失败可记录)
- ADR-0088 "未 ship 实装"声明与代码实际不符, 治理债风险

## What Changes

### 模块边界

**修改**:
1. `include/agenticdsl/genome/genome.h` (~30 行增): IGenomeRegistry 新增 `virtual Result<LineageWalk, GenomeError> walk_ancestors(name, from_version, to_version=nullopt) = 0;` + LineageWalk struct + `GenomeError::NotImplemented` enum 值 + **D9 默认实现** `return Result::failure(NotImplemented)` (per Oracle Q6 CRITICAL + AGENTS.md 模式 #9 ITimerService 先例)
2. `src/core/genome/registry_filesystem.cpp` (~40 行增): walk_ancestors override 完整 lineage walk + lazy load + cache
3. `src/evolution/version_pair_diff.cpp` (~50 行增): judge_data_freshness 完整实装 4 cases (替换 ADR-0086 v1.1 ship 的 stub)

**新增**:
4. `tests/test_genome_walk_ancestors.cpp` (≥6 cases): walk_ancestors 完整覆盖 + judge_data_freshness 4 cases 验证

**复用** (per Oracle M2 YAGNI):
- ADR-0088 D5 LineageWalk schema (Genome.spec.harness 用于 HarnessChange 检测)
- ADR-0086 v1.1 AttributionVerdict (Insufficient / Confounded / Attributed 4 态判定)

## Acceptance (验收标准)

- [ ] **AC-1**: IGenomeRegistry 5 → 6 公共方法 (load/commit/fork/list_versions/diff/walk_ancestors)
- [ ] **AC-2**: walk_ancestors 默认实现返回 `Result::failure(GenomeError::NotImplemented)` (D9 per Oracle Q6)
- [ ] **AC-3**: FilesystemGenomeRegistry::walk_ancestors override 完整 lineage walk + lazy load + cache
- [ ] **AC-4**: walk_ancestors 性能: 大 lineage (≥100 versions) < 100ms
- [ ] **AC-5**: LineageWalk.intermediate_metadata 类型: `vector<agenticdsl::genome::Genome>` (含 spec.harness, NOT GenomeMetadata per Oracle Q4)
- [ ] **AC-6**: judge_data_freshness 4 cases 完整实装 (data==current fast-path / not in lineage / Harness changed after / in lineage no Harness change)
- [ ] **AC-7**: test_genome_walk_ancestors ≥6 cases / ≥20 assertions PASS
- [ ] **AC-8**: test_credit_assignment 12/12 cases 零回归 (judge_data_freshness 完整版替换 stub)
- [ ] **AC-9**: ctest 全量 248+ tests 零回归
- [ ] **AC-10**: Oracle dual-agent review (per AGENTS.md 模式 #8) + ship-with-fixes 修正

## Capabilities (MUST / MUST NOT)

### MUST

- **MUST** 复用 ADR-0088 D5 LineageWalk schema (`vector<Genome>` 含 spec.harness)
- **MUST** 提供 walk_ancestors 默认实现 (避免 LSP cascade, per Oracle Q6 + AGENTS.md 模式 #9)
- **MUST** judge_data_freshness 4 cases 完整实装 (per ADR-0086 v1.1 决策 2 + spec/credit-assignment-v1-1 §data-freshness-algorithm)
- **MUST** TDD 5 步 + atomic commits per AGENTS.md 模式 #4
- **MUST** Oracle dual-agent review + rdd-verifier PASS

### MUST NOT

- **MUST NOT** 重新定义 `GenomeVersion` struct (已在 `include/agenticdsl/types/attribution_record.h` 定义, 遵守 ADR-0088 D5 单一所有权)
- **MUST NOT** 引入新 bus 主题 (留给 `2026-09-20-adr-0068-appendix-a-evolution-themes` follow-up)
- **MUST NOT** 修改 ADR-0086 既有契约字段 (向后兼容, 仅替换 stub)

## Impact (影响范围)

| 模块 | 变更类型 | 行数估计 |
|------|---------|---------|
| `include/agenticdsl/genome/genome.h` | 修改 | +30 (D5 接口 + D9 默认实现) |
| `src/core/genome/registry_filesystem.cpp` | 修改 | +40 (override impl) |
| `src/evolution/version_pair_diff.cpp` | 修改 | +50 (judge_data_freshness 完整实装) |
| `tests/test_genome_walk_ancestors.cpp` | **新增** | +200 (≥6 cases) |
| `tests/CMakeLists.txt` | 修改 | +5 (target_link_libraries) |

**总估计**: +325 行 / -10 行 (主要是 stub 替换完整实现)

## 关联 ADR

- **ADR-0088** (✅ Approved, ship 2026-09-20) — D5/D6/D9 本 change 实装
- **ADR-0086 v1.1** (✅ Approved, ship 2026-09-20) — judge_data_freshness stub 当前实现的归属

## C4 阻塞说明

本 change 是 C4 harness-rsi-pilot 的**真实前置**——没有 walk_ancestors + judge_data_freshness 完整实装, C4 pilot 的 `evaluate_readiness()` 条件 1 恒走 stub=Insufficient fail-closed, pilot 验证是空壳。