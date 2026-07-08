# Tasks: ADR 命名政策落地 (fix-adr-naming-policy-2026-07-08)

> **STATUS: SHIPPED** ✅ (commit pending)
> **关联 proposal**: `proposal.md`
> **关联 design**: `design.md`
> **关联 spec**: `specs/pdk-tool-naming-policy/spec.md`
> **命名形式**: `inference/{component}/{action}` (3 段式)
> **估时**: ~45 min

---

## 1. DSL Schema DOT → SLASH (5 files)

- [x] 1.1 `lib/inference/prefix_cache.md`: `prefix_cache.configure` → `inference/prefix_cache/configure` (3 处)
- [x] 1.2 `lib/inference/kv_cache.md`: `kv_cache.configure` → `inference/kv_cache/configure` (3 处)
- [x] 1.3 `lib/inference/decoding.md`: `decoding.configure` → `inference/decoding/configure` (3 处)
- [x] 1.4 `lib/inference/cloud_engine.md`: `cloud_engine.configure` → `inference/cloud_engine/configure` (5 处)
- [x] 1.5 `lib/inference/batching.md`: `batching.submit_and_wait` → `inference/batching/submit_and_wait` (2 处)
- [x] 1.6 验证: `grep -E 'prefix_cache\.configure|kv_cache\.configure|decoding\.configure|cloud_engine\.configure|batching\.submit_and_wait' lib/inference/*.md` 输出为空
- [x] 1.7 验证: `grep -c 'inference/prefix_cache/configure\|inference/kv_cache/configure\|inference/decoding/configure\|inference/cloud_engine/configure\|inference/batching/submit_and_wait' lib/inference/*.md` 每文件 ≥2

## 2. PDK 代码同步 (1 file)

- [x] 2.1 `pdk/llama_engine/src/inference_arch.cpp`: 注释 3 处 DOT → SLASH
- [x] 2.2 `pdk/llama_engine/src/inference_arch.cpp`: `register_tool_function` 4 处 DOT → SLASH
- [x] 2.3 `pdk/llama_engine/src/inference_arch.cpp`: metadata `tool_name` 4 处 DOT → SLASH
- [x] 2.4 `pdk/llama_engine/src/inference_arch.cpp`: placeholder stub 消息 `cloud_engine.configure` → `inference/cloud_engine/configure` (1 处)
- [x] 2.5 验证: `grep 'prefix_cache.configure\|kv_cache.configure\|decoding.configure\|cloud_engine.configure' pdk/llama_engine/src/inference_arch.cpp` 输出为空

## 3. 测试同步 (1 file)

- [x] 3.1 `tests/test_llama_engine_plugin.cpp`: 工具名数组 (lines 92-95) 4 处 DOT → SLASH
- [x] 3.2 `tests/test_llama_engine_plugin.cpp`: `has_tool("...configure")` 4 处 DOT → SLASH
- [x] 3.3 `tests/test_llama_engine_plugin.cpp`: `call_tool("...configure", ...)` 7 处 DOT → SLASH
- [x] 3.4 `tests/test_llama_engine_plugin.cpp`: `tool_metas.at("...configure")` 1 处 DOT → SLASH
- [x] 3.5 `tests/test_llama_engine_plugin.cpp`: 注释 `decoding.configure / kv_cache.configure` 5+ 处 DOT → SLASH
- [x] 3.6 验证: `grep 'prefix_cache.configure\|kv_cache.configure\|decoding.configure\|cloud_engine.configure' tests/test_llama_engine_plugin.cpp` 输出仅 TEST_CASE 名 (历史命名，不改)

## 4. D3 决策文档重写 (1 file)

- [x] 4.1 `docs/adversarial-reviews/decisions-2026-07-07.md`: 删除 "C13 架构工具命名边界" 小节 (lines 50-65)
- [x] 4.2 新增 "C13 架构工具命名 (2026-07-09 修正)" 段，使用 SLASH 3 段式
- [x] 4.3 更新 "影响" 段: "不应用 D3 重写" → "应用 D3 SLASH 重写"
- [x] 4.4 验证: `grep "C13 架构工具命名边界\|不带 .inference. 前缀" docs/adversarial-reviews/decisions-2026-07-07.md` 输出为空

## 5. 跨文件 DOT 清零验证

- [x] 5.1 跑: `grep -R 'prefix_cache\.configure\|kv_cache\.configure\|decoding\.configure\|cloud_engine\.configure' lib/inference/ pdk/llama_engine/ tests/ docs/adversarial-reviews/decisions-2026-07-07.md`
- [x] 5.2 期望输出: 仅 `decisions` 历史参考 + TEST_CASE 名（不改）

## 6. ctest 零回归

- [x] 6.1 `cmake --preset tests && ctest --output-on-failure` — 全部 PASS

## 7. openspec validate

- [x] 7.1 `openspec validate fix-adr-naming-policy-2026-07-08` — exit 0

## 8. Commit

- [x] 8.1 `git add` 所有 8 个变更文件
- [x] 8.2 commit: `docs: unify PDK tool naming to SLASH (3-segment inference/{component}/{action})`
- [x] 8.3 `git log --oneline -1` 验证

---

**总任务数**: 28 个
**总估时**: ~45 min