# Tasks: ig-genome-registry-walk-ancestors

## 1. Pre-flight (0.5d)
- [ ] 1.1 决策 4 HarnessChange schema 字段对齐 C2 genome.h metadata 字段
- [ ] 1.2 决策 2 kMinBaselineSamples=5 与 T14 Hotelling T² 实现细节对齐
- [ ] 1.3 决策 9 walk_ancestors 接口签名与 ADR-0088 D5 契约一致性

## 2. Dual-agent review (0.5d)
- [ ] 2.1 派 Metis background (intentionality / spec ambiguity / AI failure modes)
- [ ] 2.2 派 Oracle background (architecture / implementation feasibility / physical viability)
- [ ] 2.3 收集 30min 内 2 份报告，应用 Critical/Major 修正

## 3. Implementation (2d) (TDD 5 步 + Oracle Q6 默认实现)
- [ ] 3.0 v1.0 RED: test_genome_walk_ancestors.cpp (≥6 cases) + test_credit_assignment 验证 judge_data_freshness stub 行为
- [ ] 3.1 include/agenticdsl/genome/genome.h: IGenomeRegistry walk_ancestors virtual method (D5) + LineageWalk struct + D9 默认实现返回 `Result::failure(GenomeError::NotImplemented)`
- [ ] 3.2 test_genome_walk_ancestors.cpp RED → GREEN (walk_ancestors override impl)
- [ ] 3.3 src/core/genome/registry_filesystem.cpp: walk_ancestors override 完整 lineage walk + lazy load + cache
- [ ] 3.4 src/evolution/version_pair_diff.cpp: judge_data_freshness 完整 4 cases 实装 (替换 stub)
- [ ] 3.5 test_credit_assignment 验证: 4 个 data-freshness-algorithm scenarios 全部 PASS (替代原 stub Insufficient)

## 4. Ship-with-fixes (0.5d)
- [ ] 4.1 Oracle 实施审查 + ship-with-fixes 修正
- [ ] 4.2 spec amendments (如有)
- [ ] 4.3 ADR-0088 status 段补 D5/D6/D9 ship 标记

## 5. Doc sync (0.5d)
- [ ] 5.1 capability-application-map-2026-08.md §一 #33 行更新 (标记 D5/D6/D9 已 ship)
- [ ] 5.2 self-evolution-architecture-2026-08.md §一.1.3 + §七 #6 更新 judge_data_freshness 完整版引用
- [ ] 5.3 ADR-0086 v1.1 附录 A 标注 judge_data_freshness 已从 stub 升级到完整版

## 6. Archive (0.5d)
- [ ] 6.1 openspec archive → archive/2026-09-20-ig-genome-registry-walk-ancestors/
- [ ] 6.2 git ls-files 验证 4 文件完整
- [ ] 6.3 commit 命名: `feat(ig-genome-registry-walk-ancestors): ...`
- [ ] 6.4 merge to main (--no-ff per AGENTS.md 模式 #4)

## Acceptance

- [ ] AC-1: IGenomeRegistry 5 → 6 公共方法
- [ ] AC-2: walk_ancestors 默认实现 (D9)
- [ ] AC-3: FilesystemGenomeRegistry override 完整 lineage walk
- [ ] AC-4: walk_ancestors 性能 ≥100 versions < 100ms
- [ ] AC-5: LineageWalk.intermediate_metadata 类型 = vector<Genome>
- [ ] AC-6: judge_data_freshness 4 cases 完整实装
- [ ] AC-7: test_genome_walk_ancestors ≥6 cases / ≥20 assertions PASS
- [ ] AC-8: test_credit_assignment 12/12 cases 零回归
- [ ] AC-9: ctest 全量 248+ tests 零回归
- [ ] AC-10: Oracle dual-agent review + ship-with-fixes 修正