# Tasks: Genome Registry

> **STATUS**: ACTIVE (C2 Sprint 35) — D9/D10/D11 resolved via Oracle bg_a818a6a1
> **依赖**: C0 ✅ + C1 ✅ + Wave 1 chat demo 端到端 ✅ + Wave 2 P0 ✅ + F1 ✅ (全 ship)

## 1. Pre-flight (✅ 完成 per Oracle bg_a818a6a1)
- [x] ✅ D9 storage backend = filesystem (Oracle rationale: 单写者 + 项目先例 + 未来 git clone)
- [x] ✅ D10 signature scheme = HMAC-SHA256 (Oracle rationale: 完整性 > 身份 + 第三方不验签)
- [x] ✅ D11 谱系追踪深度 = unlimited (Oracle rationale: bounded 丢失信息, parent_only 退化)
- [x] ✅ D1-D8 既有决议确认 (per design.md)
- [x] ✅ Spec fork 设计空白填补 (Requirement fork 语义 + 3 scenarios)
- [x] ✅ Spec 错误码独立 enum (per Oracle obs #1)

## 2. Tests (RED) - 12 cases (~2h)

### Roundtrip & Schema (4 cases)
- [ ] TBD-2.1: `schema_roundtrip` — 构造全字段 Genome → commit → load → 字段级相等（含 nested harness/tools）
- [ ] TBD-2.2: `schema_violation_missing_field` — 缺 parent 字段 → SchemaViolation
- [ ] TBD-2.3: `schema_violation_unknown_version_format` — version='abc' → commit 拒绝
- [ ] TBD-2.4: `schema_violation_invalid_capture_mode` — capture_mode 不在 enum → SchemaViolation

### Atomicity & Fork Semantics (4 cases)
- [ ] TBD-2.5: `commit_atomicity` — commit 模拟崩溃（.tmp 残留）→ list_versions 不出现半成品
- [ ] TBD-2.6: `fork_creates_new_version` — fork(parent@v1, mutations) → version=2, parent=v1, deep-merge 应用
- [ ] TBD-2.7: `fork_lineage_chain` — v1→v2→v3 链式 fork → walk_ancestors(v3) = [v2, v1]
- [ ] TBD-2.8: `fork_invalid_parent` — fork(non-existent@v99) → NotFound

### HMAC & Lineage Integrity (3 cases)
- [ ] TBD-2.9: `hmac_tamper_detection` — commit 后改 genome.yaml 一字节 → IntegrityViolation
- [ ] TBD-2.10: `hmac_covers_parent_field` — 篡改 parent 字段（不改其他）→ IntegrityViolation
- [ ] TBD-2.11: `cycle_detection` — A.parent=B, B.parent=A → BrokenLineage

### Performance & Diff (1 case)
- [ ] TBD-2.12: `list_versions_scale` + `diff_field_level` — commit 100 版本 + diff 字段级精确性 < 50ms

## 3. Implementation (GREEN) (~3h)

### Header files
- [ ] TBD-3.1: `include/agenticdsl/genome/error.h` — `GenomeError` enum (6 cases per D7)
- [ ] TBD-3.2: `include/agenticdsl/genome/genome.h` — `Genome` struct + `GenomeMetadata` + `GenomeSpec` (D2/D3)
- [ ] TBD-3.3: `include/agenticdsl/genome/registry.h` — `IGenomeRegistry` interface (5 public + 1 internal `walk_ancestors`)
- [ ] TBD-3.4: `include/agenticdsl/genome/hmac.h` — `hmac_sign()` / `hmac_verify()` wrapper (OpenSSL EVP_sha256)

### Implementation files
- [ ] TBD-3.5: `src/core/genome/hmac.cpp` — HMAC wrapper impl
- [ ] TBD-3.6: `src/core/genome/canonical_yaml.cpp` — canonical YAML 序列化 (per Oracle pitfall #2)
- [ ] TBD-3.7: `src/core/genome/registry_filesystem.h/.cpp` — FilesystemGenomeRegistry (per D9 + tmp+rename)
- [ ] TBD-3.8: `src/core/genome/diff.h/.cpp` — GenomeDiff field-level comparison
- [ ] TBD-3.9: `src/core/genome/parent_validation.cpp` — cycle detection + timestamp ordering (D11)

### CMake integration
- [ ] TBD-3.10: 注册新 .cpp 进 `src/core/CMakeLists.txt` (`agenticdsl_core` 静态库)
- [ ] TBD-3.11: 链接 OpenSSL (已有依赖)

## 4. CLI Tool (~1h, per D8 + Oracle obs #5)
- [ ] TBD-4.1: `examples/genome_cli/CMakeLists.txt` — 新建独立 binary (避 pdk_chat_demo namespace pollution)
- [ ] TBD-4.2: `examples/genome_cli/main.cpp` — 4 commands: `list` / `commit` / `load` / `diff`
- [ ] TBD-4.3: `examples/genome_cli/error_mapping.cpp` — 边界转换 `GenomeError → ToolResult::ErrorCode` (per Oracle obs #1)
- [ ] TBD-4.4: 手工 E2E: commit 3 versions → list → diff → 验证 CLI 输出

## 5. Verification (~1h)
- [ ] TBD-5.1: `tests/test_genome_registry` 12/12 PASS
- [ ] TBD-5.2: focused ctest 全 PASS (test_executor + test_loop_agent + test_dsl_engine_ctx_bridge + test_session_* 等 24 个)
- [ ] TBD-5.3: 全量 ctest 247/247 维持 (含 16 known pre-existing failures)
- [ ] TBD-5.4: CLI E2E: commit 5 versions → list → diff → load

## 6. Review (~30 min)
- [ ] TBD-6.1: Oracle dual-agent review (Metis + Oracle per master plan §九.1)
- [ ] TBD-6.2: Apply corrections (per Pattern #4 SHIP-with-fixes)
- [ ] TBD-6.3: `tools/adr_lint.py` 0 errors
- [ ] TBD-6.4: `tools/docs_drift_audit.py` 0 DRIFT

## 7. Ship (~30 min)
- [ ] TBD-7.1: git atomic commit (impl + tests + CLI + design/spec updates)
- [ ] TBD-7.2: `openspec archive 2026-09-16-genome-registry` → archive dir
- [ ] TBD-7.3: 更新 master plan §四 C2 状态 → ✅ SHIPPED
- [ ] TBD-7.4: 更新 `docs/active-status.md` (total ctest, OpenSpec active/archived)

## 8. Out-of-scope (per proposal.md + Oracle)
- [x] 不接 ChatSession / DSLEngine 构造参数（避免 BREAKING，留 C3/C4 follow-up）
- [x] 不实现 Git-LFS（D9 已排除）
- [x] 不定义 3 算子接口（C4 范围）
- [x] 不暴露 walk_ancestors 到 v1 公共接口（C3 落地前不冻结设计）
- [x] 不支持并发写（Single-Dev 单写者假设；如未来冲突需迁移 SQLite）

## 9. References
- [x] Oracle session: `ses_f478a3e49fferX6CU6U4tJTIiB` (bg_a818a6a1) — D9/D10/D11 + 12 test cases + 5 pitfalls
- [x] Master plan: `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §四 C2
- [x] AGENTS.md Pattern #1 (test-driven) + Pattern #7 v2 (SessionWriter atomic write)
- [x] Related ADR: ADR-0067 (Layered Plugin), ADR-0061 (Distillation), ADR-0078 (Fine-tune)
- [x] Related change: C3 `2026-09-16-h-d-m-transition-guard` (consumer, post-C2)
- [x] Related change: C4 `2026-09-16-harness-rsi-pilot` (Harness-RSI mutations, post-C2+C3)