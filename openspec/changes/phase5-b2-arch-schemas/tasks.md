# Tasks: Phase 5 B2 Architecture-Layer Schemas (C13)

> **STATUS: ACTIVE** 🟡
> **关联 proposal**: `proposal.md`
> **关联 spec**: `specs/inference-config-schemas/spec.md`
> **关联 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §五
> **关联 Oracle session**: `ses_0ce717ac4ffejvLa2We0gzbuds`
> **前置依赖**: C12 ✅ archived (2026-07-04)
> **后续依赖**: C14 (pdk/llama_engine plugin), C15 (batching queue plugin)
> **估时**: 0.5-1 天
> **最后更新**: 2026-07-05

---

## 1. B2.3 prefix_cache.md schema

- [ ] 1.1 创建 `lib/inference/prefix_cache.md` (~50 行)
  - 文件头注释：功能描述 / 设计依据（Day 1 schema-only ship，C14 plugin 实施实际能力）
  - YAML signature: `(enabled: bool, max_size: int) -> (config: json, status: string)`
  - tool_call 节点：`prefix_cache.configure` 工具
  - 字段：enabled (默认 true), max_size (默认 512 pattern)
- [ ] 1.2 顶部加占位说明（仅 schema，实际 engine plugin 内部实现 prefix cache）
- [ ] 1.3 与 lib/inference/session.md 模板对齐（参考现有结构）

## 2. B2.4 kv_cache.md schema

- [ ] 2.1 创建 `lib/inference/kv_cache.md` (~45 行)
  - YAML signature: `(evict_policy: string, max_size_gb: float) -> (config: json, status: string)`
  - tool_call 节点：`kv_cache.configure` 工具
  - 字段：evict_policy enum (lru/lfu/fifo，默认 lru), max_size_gb (默认 4.0)
- [ ] 2.2 与 prefix_cache.md 模板对齐

## 3. B2.5 decoding.md schema

- [ ] 3.1 创建 `lib/inference/decoding.md` (~60 行)
  - YAML signature: `(temperature, top_p, top_k, repeat_penalty, sampler) -> (config, status, unsupported_warning)`
  - 5 种 sampler 选项 (greedy/temperature/mirostat_v1/mirostat_v2/typical_p)
  - 默认值：temperature=0.7, top_p=0.9, top_k=40, repeat_penalty=1.1, sampler=greedy
  - 采样器 clamp 逻辑由 engine plugin 内部实现（C14 `inference/engine/generate` 工具），本 change 不创建 PDK 接口

## 4. cloud_engine.md schema（第三方 plugin 占位）

- [ ] 4.1 创建 `lib/inference/cloud_engine.md` (~55 行)
  - 顶部明确 PLACEHOLDER 标记：`> ⚠️ PLACEHOLDER — 实现在 Phase 5 Stage 2+`
  - YAML signature: `(provider: string, model: string, api_key_ref: string) -> (config: json, status: string)`
  - tool_call 节点：`cloud_engine.configure` 工具
  - 字段：provider (openai/anthropic/deepseek/qwen), model (string), api_key_ref (引用 secret store)
  - 文档说明：第三方 plugin 按 schema 实现 (pdk/cloud_engine/openai/, pdk/cloud_engine/anthropic/, 等)

## 5. 文档同步

- [ ] 5.1 更新 `docs/handoff/2026-07-05-week1-day1-day2-completion.md` §10.2
  - 标记 C13 完成（4 个 schema + 1 个接口声明 ship）
  - 移除 §10.2 "⏳ B2.1+B2.2 engine/model 实施" (改为 C14 范围)
- [ ] 5.2 更新 `docs/active-status.md` 活跃变更看板 Phase 5 进度
  - Phase 5 Stage 1 进度: "3/7 ship + 2/7 占位" → "4/7 ship + 1/7 占位 (cloud_engine) + 2/7 待 C14"
  - 实施日志追加 C13 ship 行
- [ ] 5.3 更新 `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §5.4
  - B2 拆分说明（C13/C14/C15 三个 change 替代原 handoff §5.1-5.3）
  - master plan §三 §四 添加 C13/C14/C15 行

## 6. 验证

- [ ] 6.1 `cmake --build build -j$(nproc)` 100% 编译通过（schema-only ship 无 .cpp 变更）
- [ ] 6.2 `cd build/debug && ctest --output-on-failure` 64/64 PASS 零回归
- [ ] 6.3 `python3 tools/adr_lint.py` exit 0（无新 ADR 文件）
- [ ] 6.4 `python3 tools/docs_drift_audit.py` 0 DRIFT（schema 引用文档同步）
- [ ] 6.5 `openspec validate phase5-b2-arch-schemas` exit 0

## 7. 提交与归档

- [ ] 7.1 Git 提交 1: `feat(phase5-stdlib): add inference config schemas (prefix_cache/kv_cache/decoding/cloud_engine)`
- [ ] 7.2 Git 提交 2: `docs(phase5): update roadmap + master plan + handoff for C13/C14/C15 split`
- [ ] 7.3 Git 提交 3: `chore(openspec): mark C13 tasks complete before archive`
- [ ] 7.4 `openspec archive phase5-b2-arch-schemas`

---

## 验证检查清单 (C13 ship gate)

- [ ] 1. 4 个 lib/inference/*.md schema ship（前缀缓存/KV缓存/解码/cloud 占位）
- [ ] 2. docs_drift_audit.py 0 DRIFT
- [ ] 3. ctest 64/64 PASS（schema-only ship 零回归）
- [ ] 4. adr_lint.py exit 0
- [ ] 5. openspec validate exit 0
- [ ] 6. handoff/roadmap/master plan 三处文档同步
- [ ] 7. lib/inference 覆盖率: 1/7 → 4/7 ship + 1/7 占位 (cloud_engine) + 2/7 待 C14 (engine/model)
- [ ] 8. Git 3 commits pushed to origin/main
- [ ] 9. C13 archived via openspec archive

## 关联 change 状态

- ✅ C9 (ADR impl-scope audit) — archived 2026-07-03
- ✅ C10 (Lazy ModuleState) — archived 2026-07-03
- ✅ C11 (SessionRegistry) — archived 2026-07-04
- ✅ C12 (YIELD/STREAM) — archived 2026-07-04
- 🟡 **C13 (B2 Architecture Schemas)** — ACTIVE (本 change)
- ⚪ C14 (pdk/llama_engine plugin) — placeholder, 待 C13 ship 后启动
- ⚪ C15 (BatchingQueue plugin) — placeholder, 待 C14 ship 后启动