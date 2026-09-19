# Design: Genome Registry

> **STATUS**: ACTIVE (C2 Sprint 35)
> **Oracle design review**: `bg_a818a6a1` (session `ses_f478a3e49fferX6CU6U4tJTIiB`, 1m 45s) — D9/D10/D11 + 12 test cases + 5 pitfalls 已收敛
> **AGENTS.md 模式引用**: Pattern #1 (test-driven) + Pattern #7 v2 (SessionWriter atomic write pattern)
> **追溯范围**: `openspec/changes/2026-09-16-genome-registry/{proposal,specs,tasks}.md` + master plan §四 C2

---

## Decisions (D1-D11)

### D1-D8 (来自 proposal.md 既有决议)

- **D1** ✅ Genome CRD schema name = `genome-v1` (per `proposal.md` §Genome CRD schema)
- **D2** ✅ metadata 字段: `name` (string, required), `version` (uint64 monotonic, required), `parent` (string `name@version` or null), `created_by` (string, e.g. `"solo-dev"`), `created_at` (RFC3339 UTC string), `capture_mode` (enum: `mock` / `real` / `hybrid`)
- **D3** ✅ spec 字段: `harness` (string, references `lib/loop/*.agent.md`), `tools` (vector<string>), `model_routing` (string), `budget` (uint64), `prompt_cache_prefix` (string optional)
- **D4** ✅ IGenomeRegistry 接口 6 方法: `load(name@version)` / `commit(genome)` / `fork(name@version, mutations)` / `list_versions(name)` / `diff(v1, v2)` / `walk_ancestors(name@version)` (内部方法, 非公开 API v1)
- **D5** ✅ 所有方法返回 `Result<T, GenomeError>` (per `src/common/llm/llm_types.h` Result<T,E> pattern)
- **D6** ✅ 文件路径 layout: `~/.hydraforge/genomes/<name>/<version>/genome.yaml` + 同目录 `signature.hmac` + 同目录 `parent.yaml` (parent reference 单独文件, 不嵌入 genome.yaml 以便谱系 walk 不必反序列化完整 genome)
- **D7** ✅ GenomeError 独立 enum (`NotFound`, `SchemaViolation`, `IntegrityViolation`, `BrokenLineage`, `CycleDetected`, `IOError`) — **per Oracle obs #1, 不对齐 ToolResult::ErrorCode**
- **D8** ✅ CLI demo 入口放 `examples/genome_cli/main.cpp` (per Oracle obs #5, 避 pdk_chat_demo namespace pollution)

### D9: Storage Backend — **filesystem** ✅ (Oracle bg_a818a6a1)

**Rationale**:
- Single-Dev + 低频写入 (<100 genomes/day) + 项目既有先例 (session.jsonl / capture-mode YAML / distillation YAML 全走 filesystem) 三重信号一致
- SQLite 引入新依赖与 schema 迁移负担，<100 行写入量需要索引是过度工程
- Git-LFS 为大二进制设计，genome.yaml 是几 KB 文本，工具错配
- filesystem 的 path layout 天然 content-addressable 目录树，未来第三方 fetch 直接 `git clone` 或 `rsync` 只读目录即可

**Trade-offs accepted**:
- 无 SQL 查询能力 — `list_versions` 用 `std::filesystem::directory_iterator` 扫描，<100 版本量级 <1ms
- 跨进程并发写无事务保护 — Single-Dev 单写者假设下用 `.tmp` + `rename(2)` 原子提交即可（与 SessionWriter 同模式）
- 谱系查询是逐目录 walk — D11 unlimited 时深度 <100，线性 walk O(depth) 满足约束

**Implementation hint**: `commit` 用 `write tmp → fsync → rename` 原子落盘（复用 `src/core/session_writer.cpp` 的 `file_mutex_` 模式），`genome.yaml` 旁放 `parent` 字段于 YAML 内部而非单独文件，减少 I/O。

### D10: Signature / Integrity Scheme — **HMAC-SHA256** ✅ (Oracle bg_a818a6a1)

**Rationale**:
- 完整性 > 身份认证、single-Dev 无多写者争议、第三方 read 信任 repo owner 不需验签
- HMAC-SHA256 对称、单 secret、OpenSSL 已在依赖树（httplib 用），一行 `HMAC(EVP_sha256(), ...)` 搞定
- ed25519 解决"谁签的"，但签名者=验证者，身份认证是伪需求
- git-sha 与 D9 filesystem 矛盾

**Trade-offs accepted**:
- Secret 管理 — `~/.hydraforge/genome.key` 0600 权限文件，首次 commit 自动生成 32 字节随机 key；丢失即历史签名不可验（single-Dev 可重新签）
- 不提供不可否认性 — 未来多写者场景需迁移 ed25519；触发条件：出现第二个 committer
- HMAC 必须覆盖 `parent_hash` 字段（D11 依赖），否则谱系可篡改

**Implementation hint**: 签名输入 = canonical YAML bytes（先序列化再签，避免字段顺序敏感），`signature.hmac` 与 `genome.yaml` 同目录，`key` 路径支持 `HYDRAFORGE_GENOME_KEY` env override 便于测试隔离。

### D11: Lineage 谱系追踪深度 — **unlimited** ✅ (Oracle bg_a818a6a1)

**Rationale**:
- bounded_10 人为信息丢失：第 11 代 fork 时 transition-guard 无法判断"是否源自已知-good root"
- parent_only 退化逐次 load: N 层深度需 N 次磁盘 I/O + N 次 HMAC，且无法在 commit 时检测 cycle
- unlimited 成本低：每个 genome.yaml 存 `parent` 单字段，walk O(depth) 指针追踪

**Trade-offs accepted**:
- 深度无上限 walk 时间长 — 实际谱系深度 <<1000，walk 1000 次 YAML parse 约 50ms，可接受；若成瓶颈，加 depth cache 文件（`.lineage_cache`），不改 schema
- commit 时 cycle 检测需 walk 全链 — 同上量级可接受，commit 是低频操作

**Implementation hint**: `Genome.metadata` 只存 `parent` (name@version 或 hash) 单字段；registry 提供 `walk_ancestors(name@version)` 内部方法（不进 v1 公开接口，C3 需要时再提升），walk 中维护 `unordered_set<hash>` 做 cycle 检测 + 校验 `parent.created_at < child.created_at`。

---

## Test Cases (12, per Oracle 推荐)

| # | Name | Validates | Oracle ref |
|---|------|-----------|-----------|
| 1 | `schema_roundtrip` | 全字段 Genome → commit → load → 字段级相等（含 nested harness/tools） | t1 |
| 2 | `schema_violation_missing_field` | 缺 parent 字段 → SchemaViolation | t2 |
| 3 | `schema_violation_unknown_version_format` | version='abc' 非规范 → commit 拒绝 | t3 |
| 4 | `commit_atomicity` | commit 模拟崩溃（写到 .tmp 未 rename）→ list_versions 不出现半成品 | t4 |
| 5 | `fork_creates_new_version` | fork(parent@v1, mutations) → 新版本 parent 字段 == v1 hash | t5 |
| 6 | `fork_lineage_chain` | v1→v2→v3 链式 fork → walk_ancestors(v3) 返回 [v2,v1] 顺序正确 | t6 |
| 7 | `cycle_detection` | A.parent=B, B.parent=A → load/walk 返回 BrokenLineage | t7 |
| 8 | `parent_timestamp_ordering` | parent.created_at >= child.created_at → commit 拒绝 | t8 |
| 9 | `hmac_tamper_detection` | commit 后改 genome.yaml 一字节 → IntegrityViolation | t9 |
| 10 | `hmac_covers_parent_field` | 篡改 parent 字段（不改其他）→ HMAC 校验失败 | t10 |
| 11 | `diff_field_level` | v1 vs v2 仅 system_prompt 不同 → diff 返回精确字段路径 | t11 |
| 12 | `list_versions_scale` | commit 100 版本 → list_versions < 50ms 且按版本序 | t12 |

---

## Implementation Plan (TDD 5 步)

### 1. RED (failing tests, ~2h)
- 新建 `tests/test_genome_registry.cpp` (独立 binary, CMakeLists.txt GLOB 自动注册)
- 12 cases 写完, 验证 12/12 FAIL (编译通过 + 测试全 fail)
- 不实现任何 production code

### 2. GREEN (minimal impl, ~3h)
- 新建 `include/agenticdsl/genome/` + `src/core/genome/`:
  - `error.h` — `GenomeError` enum (6 cases per D7)
  - `genome.h` — `Genome` struct (metadata + spec, per D2/D3)
  - `registry.h` — `IGenomeRegistry` interface (5 public + 1 internal, per D4)
  - `registry_filesystem.h/.cpp` — `FilesystemGenomeRegistry` impl
  - `diff.h/.cpp` — `GenomeDiff` field-level comparison
  - `hmac.h/.cpp` — `sign(bytes)` / `verify(bytes, sig)` wrapper over OpenSSL
- 注册新 .cpp 进 `agenticdsl_core` 静态库 (参考 Sprint 19 模式)
- 验证 12/12 tests PASS

### 3. REFACTOR (~1h)
- 提取 helpers (`canonical_yaml_serialize`, `parent_validation`, `walk_ancestors`)
- 确保 public API 零破坏性变更
- ctest 维持 247/247 baseline (含 16 known pre-existing failures)

### 4. INTEGRATION (~1h)
- 新建 `examples/genome_cli/main.cpp` (per D8): `hydraforge genome list/commit/load/diff` 4 commands
- 注册进 `examples/CMakeLists.txt`
- 手工跑 E2E: commit 3 versions → diff → list

### 5. REVIEW (~30 min)
- Oracle dual-agent review (per master plan §九.1)
- Apply corrections
- 全量 ctest 验证

---

## Pitfalls to Avoid (Oracle bg_a818a6a1)

1. ❌ 不要把 GenomeError 塞进 ToolResult::ErrorCode — spec 现有映射表自相矛盾，强对齐制造假等价；独立 enum + 边界转换
2. ❌ HMAC 签名输入必须基于 **canonical 序列化字节**而非内存对象 — 否则 YAML emitter 字段顺序变化导致同内容不同签名
3. ❌ commit 必须 `tmp + rename` 原子写 — 直接 ofstream 写 genome.yaml 在崩溃时留下截断文件（SessionWriter 已在模式 #7 v2 付出过学费）
4. ❌ 不要在 IGenomeRegistry v1 接口暴露 walk_ancestors — C3 消费者未落地前暴露公共 API 等于冻结未验证设计
5. ❌ 避免在 genome registry 里发明版本字符串解析器 — 用单调整数，一行 compare 搞定，semver 解析是纯负债

---

## Dual-Agent Review Outcome (2026-09-18)

**Oracle bg_9ade564d** (BLOCK → SHIP-with-fixes post-corrections):
- C1 (critical) ✅ FIXED: cycle detection infinite loop — visited set keyed on (name, version) PAIRS + depth cap 10000
- C2 (critical) ✅ FIXED: walk wrapped in try/catch (IOError returns); cycle_detection test re-signed to actually exercise cycle path
- M1 ✅ FIXED: list_versions requires BOTH yaml + sig; commit writes sig-first ordering
- M2 ✅ FIXED: HMAC key uses OpenSSL RAND_bytes (CSPRNG)
- M3 ✅ FIXED: atomic_write returns Result + checks stream state
- M4 ✅ FIXED: commit guarded by std::mutex commit_mutex_
- M5 ✅ FIXED: lineage validation also at commit (not only load)
- M6 DEFERRED: Result template — minor, can co-exist with llm_types.h::Result
- m1 ✅ FIXED: fork generates fresh RFC3339 UTC timestamp
- m2 ✅ SPEC AMENDED: fork version semantics `max+1` (replaces `parent+1`)
- m3 PARTIAL: cycle test sets HYDRAFORGE_GENOME_KEY (other tests still use $HOME key)
- m4: CycleDetected/BrokenLineage semantics clarified — cycle returns BrokenLineage per spec

**Metis bg_89293120** (ship-with-fixes → SHIP post-corrections):
- A1 ✅ FIXED (matches Oracle C2b)
- A2 ✅ FIXED (matches Oracle C2c)
- A3 DEFERRED: hard timing <50ms — flake risk acknowledged, kept for now (generous buffer)
- S1 ✅ SPEC AMENDED: spec text now matches impl (load rejects non-monotonic version via parse exception)
- S2 ✅ SPEC AMENDED: fork version semantics
- S3 ✅ SPEC AMENDED: spec text relaxed from "fsync" to "tmp + rename" with sig-first ordering guarantee
- S4 ✅ FIXED (matches Oracle M1): list_versions double-file check
- I1 DEFERRED: walk_ancestors — already deferred to C3, spec text amended
- I2 ✅ FIXED (matches Oracle m1): fork timestamp
- I3 PARTIAL: cycle test isolation via env override
- I4 ✅ SPEC AMENDED: parent clarified as optional
- I5 DEFERRED: CLI tool — already out of scope per design.md §Out-of-scope, spec amended

**Deferred to follow-up changes** (not in this C2 scope):
- Fsync (file + directory fdatasync) — Linux-specific hardening
- CLI tool (`examples/genome_cli/`) — separate `genome-cli` change
- walk_ancestors — C3 transition-guard change
- Hard timing <50ms flake risk — consider relaxation to <500ms

---

## File Layout (planned)

```
include/agenticdsl/genome/
  ├── error.h              # GenomeError enum (D7)
  ├── genome.h             # Genome struct (D2/D3)
  ├── registry.h           # IGenomeRegistry interface (D4)
  └── hmac.h               # HMAC sign/verify wrapper

src/core/genome/
  ├── registry_filesystem.h
  ├── registry_filesystem.cpp
  ├── diff.h
  ├── diff.cpp
  ├── hmac.cpp
  └── canonical_yaml.cpp     # canonical serialization (pitfall #2)

tests/
  └── test_genome_registry.cpp  # 12 cases

examples/genome_cli/
  ├── CMakeLists.txt
  └── main.cpp               # 4 CLI commands (D8)
```

---

## Out of Scope (per proposal.md §Non-Goals + Oracle obs)

- 不接 ChatSession / DSLEngine 构造参数（避免 BREAKING）
- 不实现 Git-LFS（D9 已排除）
- 不定义 3 算子接口（CapturedBy / Captures / FuseBy — C4 范围）
- 不暴露 walk_ancestors 到 v1 公共接口（C3 落地前不冻结设计）
- 不支持并发写（Single-Dev 单写者假设；如未来冲突需迁移 SQLite）

---

## References

- Oracle session: `ses_f478a3e49fferX6CU6U4tJTIiB` (bg_a818a6a1)
- Master plan: `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §四 C2
- AGENTS.md Pattern #1: test-driven bug discovery
- AGENTS.md Pattern #7 v2: SessionWriter atomic write (`src/core/session_writer.cpp`)
- Related ADR: ADR-0067 (Layered Plugin), ADR-0061 (Distillation), ADR-0078 (Fine-tune)
- Related change: C3 `2026-09-16-h-d-m-transition-guard` (consumer, post-C2)
- Related change: C4 `2026-09-16-harness-rsi-pilot` (Harness-RSI mutations, post-C2+C3)
- MetaRSI-v1 论文 (unverified): Genome 概念参考