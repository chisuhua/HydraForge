# Tasks: Genome Registry

> **STATUS: PLACEHOLDER** — depends on C0+C1 ship

## 1. Pre-flight
- [ ] TBD: D9 storage backend 决策 (filesystem vs SQLite vs Git-LFS)
- [ ] TBD: D10 signature scheme 决策 (HMAC vs ed25519)
- [ ] TBD: D11 谱系追踪深度（无限祖先 vs 限定深度）

## 2. Tests (RED) - 8 case × 5 步
- [ ] TBD: load_valid_genome → Genome struct
- [ ] TBD: commit_new_genome → CommitResult ok + file created
- [ ] TBD: fork_from_parent → Genome with new version + parent reference
- [ ] TBD: diff_v1_v2 → GenomeDiff with field-level changes
- [ ] TBD: list_versions → vector of all version strings
- [ ] TBD: parent_validation → reject if parent missing
- [ ] TBD: hmac_integrity → reject if signature invalid
- [ ] TBD: invalid_schema → reject if YAML fails schema

## 3. Implementation (GREEN)
- [ ] TBD: pdk/genome/spec/genome-v1.yaml (CRD schema)
- [ ] TBD: include/agenticdsl/genome/registry.h (IGenomeRegistry interface)
- [ ] TBD: src/core/genome/registry.cpp (filesystem backend)
- [ ] TBD: include/agenticdsl/genome/error.h (GenomeError enum)
- [ ] TBD: src/core/genome/diff.cpp (GenomeDiff implementation)

## 4. Storage layout
- [ ] TBD: `~/.hydraforge/genomes/<name>/<version>/genome.yaml`
- [ ] TBD: `~/.hydraforge/genomes/<name>/<version>/signature.hmac`
- [ ] TBD: 谱系图: commit 时写 parent_version 字段

## 5. Integration
- [ ] TBD: ChatSession 暂不接（避免 BREAKING，留 follow-up）
- [ ] TBD: DSLEngine 暂不接（同上）
- [ ] TBD: CLI 工具 demo: `hydraforge genome list/commit/load/diff`

## 6. Verification
- [ ] TBD: 8 个 test case 全部 PASS
- [ ] TBD: 真实 LLM 模式 + Genome commit/load 端到端
- [ ] TBD: 谱系图审计测试 (commit 100 个 version, 验证 list_versions 性能)

## 7. Ship gate
- [ ] TBD: ctest 零回归
- [ ] TBD: adr_lint 0 errors
- [ ] TBD: docs_drift_audit 0 DRIFT
- [ ] TBD: openspec validate
- [ ] TBD: dual-agent review (Metis + Oracle)

## 8. Archive
- [ ] TBD: openspec archive
- [ ] TBD: 更新 master plan §四 C2 状态

## 9. Out-of-scope
- [ ] TBD: 不接 ChatSession/DSLEngine 构造参数
- [ ] TBD: 不实现 Git-LFS
- [ ] TBD: 不定义 3 算子接口

## 10. References
- [ ] TBD: MetaRSI-v1 论文 Genome 概念 (unverified)
- [ ] TBD: ADR-0023 (ToolResult 错误码对齐)
- [ ] TBD: ADR-0061-13 (Distillation Output Format)
- [ ] TBD: ADR-0078 (Model-RSI 依赖)
