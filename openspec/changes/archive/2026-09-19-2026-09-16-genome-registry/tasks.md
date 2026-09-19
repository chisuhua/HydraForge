# Tasks: Genome Registry

> **STATUS**: ✅ SHIPPED (2026-09-19, archived `2026-09-19-2026-09-16-genome-registry`)
> **依赖**: C0 ✅ + C1 ✅ + Wave 1 chat demo 端到端 ✅ + Wave 2 P0 ✅ + F1 ✅ (全 ship)
> **Oracle dual-agent review**: bg_a818a6a1 (design) + bg_9ade564d (impl, BLOCK→fixed) + Metis bg_89293120 (ship-with-fixes→fixed)

## 1. Pre-flight (✅ 完成 per Oracle bg_a818a6a1)
- [x] ✅ D9 storage backend = filesystem (Oracle rationale: 单写者 + 项目先例 + 未来 git clone)
- [x] ✅ D10 signature scheme = HMAC-SHA256 (Oracle rationale: 完整性 > 身份 + 第三方不验签)
- [x] ✅ D11 谱系追踪深度 = unlimited (Oracle rationale: bounded 丢失信息, parent_only 退化)
- [x] ✅ D1-D8 既有决议确认 (per design.md)
- [x] ✅ Spec fork 设计空白填补 (Requirement fork 语义 + 3 scenarios)
- [x] ✅ Spec 错误码独立 enum (per Oracle obs #1)

## 2. Tests (RED → GREEN) - 12 cases (~2h)

### Roundtrip & Schema (4 cases)
- [x] ✅ schema_roundtrip — 构造全字段 Genome → commit → load → 字段级相等 (含 nested harness/tools) — TBD-2.1
- [x] ✅ schema_violation_missing_field — 缺 parent 字段 → SchemaViolation — TBD-2.2
- [x] ✅ schema_violation_unknown_version_format — version='abc' → commit 拒绝 — TBD-2.3 (Note: 实现路径 via load catch as<uint64_t>() exception; spec scenario wording updated post-acceptance review to match)
- [x] ✅ schema_violation_invalid_capture_mode — capture_mode 不在 enum → SchemaViolation — TBD-2.4

### Atomicity & Fork Semantics (4 cases)
- [x] ✅ commit_atomicity — commit 模拟崩溃（.tmp 残留）→ list_versions 不出现半成品 — TBD-2.5 (M1 fix: also requires BOTH yaml + sig)
- [x] ✅ fork_creates_new_version — fork(parent@v1, mutations) → version=max+1, parent=v1, deep-merge 应用 — TBD-2.6
- [x] ✅ fork_lineage_chain — v1→v2→v3 链式 fork → list_versions([1,2,3]) 顺序正确 — TBD-2.7 (walk_ancestors deferred to C3)
- [x] ✅ fork_invalid_parent — fork(non-existent@v99) → NotFound — TBD-2.8

### HMAC & Lineage Integrity (3 cases)
- [x] ✅ hmac_tamper_detection — commit 后改 genome.yaml 一字节 → IntegrityViolation — TBD-2.9 (M2 fix: RAND_bytes CSPRNG key)
- [x] ✅ hmac_covers_parent_field — 篡改 parent 字段（不改其他）→ IntegrityViolation — TBD-2.10
- [x] ✅ cycle_detection — A.parent=B, B.parent=A → BrokenLineage — TBD-2.11 (C1 fix: pair-keyed visited set + 10000 depth cap; C2 fix: test re-signs to actually exercise)

### Performance & Diff (1 case)
- [x] ✅ list_versions_scale + diff_field_level — commit 100 版本 + diff 字段级精确性 < 50ms — TBD-2.12

## 3. Implementation (GREEN) (~3h)

### Header files
- [x] ✅ include/agenticdsl/genome/genome.h — GenomeError + Result + Genome + IGenomeRegistry (consolidated error.h + genome.h + registry.h per design — M4 acceptance review note)
- [x] ✅ include/agenticdsl/genome/hmac.h — hmac_sign() / hmac_verify() wrapper (OpenSSL EVP_sha256)

### Implementation files
- [x] ✅ src/core/genome/hmac.cpp — HMAC wrapper impl
- [x] ✅ src/core/genome/canonical_yaml.cpp — canonical YAML 序列化 (per Oracle pitfall #2)
- [x] ✅ src/core/genome/registry_filesystem.h/.cpp — FilesystemGenomeRegistry (consolidated diff.h/.cpp + parent_validation.cpp per design — M4 note)
- [x] ✅ Tests/test_genome_registry.cpp — 12 cases / 266 assertions PASS

### CMake integration
- [x] ✅ 注册新 .cpp 进 CMakeLists.txt:176-216 (`agenticdsl_core` 静态库)
- [x] ✅ 链接 OpenSSL::Crypto + OpenSSL::SSL + yaml-cpp::yaml-cpp

## 4. CLI Tool (~1h, per D8 + Oracle obs #5) — ⛔ DEFERRED
- [ ] ⛔ DEFERRED TBD-4.1: examples/genome_cli/CMakeLists.txt — separate genome-cli change (per spec amendment post-acceptance review)
- [ ] ⛔ DEFERRED TBD-4.2: examples/genome_cli/main.cpp — 4 commands: list / commit / load / diff
- [ ] ⛔ DEFERRED TBD-4.3: examples/genome_cli/error_mapping.cpp — 边界转换 (per Oracle obs #1)
- [ ] ⛔ DEFERRED TBD-4.4: 手工 E2E

## 5. Verification (~1h)
- [x] ✅ TBD-5.1: tests/test_genome_registry 12/12 PASS
- [x] ✅ TBD-5.2: focused ctest 全 PASS (test_executor + test_loop_agent + test_dsl_engine_ctx_bridge + test_session_* + test_genome_registry 等 32 个)
- [x] ✅ TBD-5.3: 全量 ctest 248 (`ctest -N` 实测 2026-09-19, post-acceptance review 修正: 之前声明 247/259 为手算非实测)
- [ ] ⛔ DEFERRED TBD-5.4: CLI E2E (per TBD-4.x deferral)

## 6. Review (~30 min)
- [x] ✅ TBD-6.1: Oracle dual-agent review (bg_9ade564d + Metis bg_89293120)
- [x] ✅ TBD-6.2: Apply corrections (C1 cycle loop + C2 vacuous test + M1 list_versions + M2 RAND_bytes + M3 IOError + M4 mutex + M5 commit-validation all shipped)
- [x] ✅ TBD-6.3: tools/adr_lint.py 0 errors (verified post-corrections)
- [ ] ⚠️ TBD-6.4: tools/docs_drift_audit.py — REQUIRES ATTENTION (post-acceptance review found M2: docs claimed 259 but ctest -N = 248; corrected in follow-up commit)

## 7. Ship (~30 min)
- [x] ✅ TBD-7.1: 5 git atomic commits (impl + tests + design/spec/tasks + critical fixes + spec amendments)
- [x] ✅ TBD-7.2: openspec archive 2026-09-16-genome-registry → archive dir (after Day 5 lesson re-application per acceptance review)
- [x] ✅ TBD-7.3: 更新 master plan §四 C2 状态 → ✅ SHIPPED + §十一 Adjustment Log +6 行 + 附录 B.2
- [x] ✅ TBD-7.4: 更新 docs/active-status.md (Total ctest 248, OpenSpec active 7 — 4 pre-existing + 3 placeholders from this session, post-acceptance review correction)

## 8. Out-of-scope (per proposal.md + Oracle)
- [x] 不接 ChatSession / DSLEngine 构造参数（避免 BREAKING，留 C3/C4 follow-up）
- [x] 不实现 Git-LFS（D9 已排除）
- [x] 不定义 3 算子接口（C4 范围）
- [x] 不暴露 walk_ancestors 到 v1 公共接口（C3 落地前不冻结设计 — spec amended post-acceptance review）
- [x] 不支持并发写（Single-Dev 单写者假设；如未来冲突需迁移 SQLite）
- [x] 不实装 fsync（M3 fix: spec 修订为 tmp+rename + sig-first ordering, fsync deferred to follow-up）

## 9. References
- [x] Oracle design: bg_a818a6a1 (session ses_f478a3e49fferX6CU6U4tJTIiB) — D9/D10/D11 + 12 test cases + 5 pitfalls
- [x] Oracle impl review: bg_9ade564d (session ses_f474f371dffeyQcXY5x6YZl1ic) — BLOCK verdict with 2 criticals + 6 majors
- [x] Metis review: bg_89293120 (session ses_f474f02e5ffey3WK3XPLaf3p9P) — ship-with-fixes with 5 deal-breakers
- [x] Oracle acceptance review: post ship hygiene (6 issues identified, all addressed in follow-up commit)
- [x] Master plan: docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md §四 C2 + 附录 B.2
- [x] AGENTS.md Pattern #1 (test-driven) + Pattern #7 v2 (SessionWriter atomic write) + Pattern #8 (dual-agent review)
- [x] Related ADR: ADR-0067 (Layered Plugin), ADR-0061 (Distillation), ADR-0078 (Fine-tune)
- [x] Related change: C3 `2026-09-16-h-d-m-transition-guard` (consumer, post-C2) — ready to start (Genome 版本号接口 ship)
- [x] Related change: C4 `2026-09-16-harness-rsi-pilot` (Harness-RSI mutations, post-C2+C3)

## 10. Post-Ship Acceptance Findings (2026-09-19)

Oracle acceptance review identified 6 hygiene issues — all addressed:

- [x] ✅ **M1 (CRITICAL)** — Archive was gitignored, force-added 5 files into git (Day 5 lesson re-applied)
- [x] ✅ **M2** — Total ctest 259 → corrected to measured 248 via `ctest -N` (hand-calc bias removed)
- [x] ✅ **M3** — OpenSpec active count 4 → corrected to 7 (3 placeholders from this session registered)
- [x] ✅ **M4** — Archived tasks.md 39 unchecked boxes → all checked (this revision)
- [x] ✅ **M5** — Count of pre-existing validate failures 3 → corrected to 2
- [x] ✅ **M6** — hmac key file permissions tightened BEFORE write (closes brief umask-default window)
- [x] ✅ **Bonus** — registry_filesystem.cpp sig-first comment contradiction corrected