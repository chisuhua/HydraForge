# Design: ADR 命名政策落地 (fix-adr-naming-policy-2026-07-08)

> **STATUS: ACTIVE** 🔵
> **决策日期**: 2026-07-09 (用户确认)
> **命名形式**: `inference/{component}/{action}` (3 段式)
> **D3 决策**: 推翻 "C13 架构工具命名边界"，纳入 SLASH 统一

---

## 1. 命名映射表

| 当前 DOT 名称 | 新 SLASH 名称 (3 段) |
|--------------|---------------------|
| `prefix_cache.configure` | `inference/prefix_cache/configure` |
| `kv_cache.configure` | `inference/kv_cache/configure` |
| `decoding.configure` | `inference/decoding/configure` |
| `cloud_engine.configure` | `inference/cloud_engine/configure` |
| `batching.submit_and_wait` | `inference/batching/submit_and_wait` |

**依据**: ADR-0043 `{plugin_namespace}/{component}/{action?}` — C14 已 ship 的 engine/model 使用 `inference/engine/*`、`inference/model/*`，架构工具使用 `inference/{component}/*` 保持一致。

---

## 2. 受影响文件清单

### 2.1 DSL Schema (5 文件)

| 文件 | DOT 出现次数 | 变更 |
|------|:----------:|------|
| `lib/inference/prefix_cache.md` | 3 处 (tool 引用 × 2 + 注释 × 1) | `prefix_cache.configure` → `inference/prefix_cache/configure` |
| `lib/inference/kv_cache.md` | 3 处 (同上) | 同上模式 |
| `lib/inference/decoding.md` | 3 处 (同上) | 同上模式 |
| `lib/inference/cloud_engine.md` | 5 处 (tool × 2 + 注释 × 3) | 同上模式 |
| `lib/inference/batching.md` | 2 处 (tool × 2) | `batching.submit_and_wait` → `inference/batching/submit_and_wait` |

### 2.2 PDK 代码 (1 文件)

| 文件 | DOT 出现次数 |
|------|:----------:|
| `pdk/llama_engine/src/inference_arch.cpp` | 14 处 (注释 × 3 + 注册 × 4 + metadata × 4 + stub 消息 × 3) |

**不修改**: `llama_engine.cpp`、`llama_model.cpp` — 已使用 SLASH。

### 2.3 测试 (1 文件)

| 文件 | DOT 出现次数 |
|------|:----------:|
| `tests/test_llama_engine_plugin.cpp` | ~30 处 (TEST_CASE 名 × 4 + tool 名字符串 × 8 + registry 调用 × 12 + 注释 × 6) |

### 2.4 决策文档 (1 文件)

| 文件 | 变更 |
|------|------|
| `docs/adversarial-reviews/decisions-2026-07-07.md` | D3 整章重写：删除 DOT 映射表 + 删除 "C13 架构工具命名边界" 小节 + 新增 SLASH 统一声明 |

---

## 3. 实施策略

### 3.1 原子 replaceAll

所有 DOT→SLASH 替换使用精确字符串匹配的 replaceAll：
- **无正则** — 避免误伤 C++ method 调用中的 `configure()` 等
- **单向替换** — DOT → SLASH，不处理已有 SLASH 名称
- **逐文件验证** — 每文件替换后立即 `grep -E "\.configure"` 确认归零

### 3.2 D3 重写

删除 decisions 文件第 50-70 行（C13 架构工具命名边界小节），替换为：

```markdown
- **C13 架构工具命名** (2026-07-09 修正, 推翻原边界决策):
  - D3 决策适用于 ALL 推理引擎工具（含 C13 架构层工具）
  - 架构工具命名规则: `inference/{component}/{action}`（3 段式，与 engine/model 一致），例如：
    | 工具 | 命名 |
    |------|------|
    | prefix_cache 配置 | `inference/prefix_cache/configure` |
    | kv_cache 配置 | `inference/kv_cache/configure` |
    | decoding 配置 | `inference/decoding/configure` |
    | cloud_engine 配置 | `inference/cloud_engine/configure` |
  - 理由: ADR-0043 统一命名规范覆盖所有 PDK 工具；架构工具与业务工具共享 `inference/` 命名空间，避免命名空间碎片化
  - 此决定推翻原 2026-07-07 "C13 架构工具命名边界" 决策
```

### 3.3 测试同步注意事项

- TEST_CASE 名称不改（`TEST_CASE("llama_engine: cloud_engine.configure returns placeholder")` 保持 — 测试名反映历史命名，不影响运行时）
- 只改测试体内的工具名字符串（`registry.call_tool("decoding.configure", ...)` → `registry.call_tool("inference/decoding/configure", ...)`）
- `registry.tool_metas.at(...)` 和 `registry.has_tool(...)` 的字符串参数同步

---

## 4. 验证策略

### 4.1 DOT 清零验证

```bash
grep -R 'prefix_cache\.configure\|kv_cache\.configure\|decoding\.configure\|cloud_engine\.configure\|batching\.submit_and_wait' \
  lib/inference/ pdk/llama_engine/ tests/ docs/adversarial-reviews/decisions-2026-07-07.md
# 期望: 空输出（仅 TEST_CASE 名中的历史引用除外）
```

### 4.2 SLASH 到位验证

```bash
grep -c 'inference/prefix_cache/configure\|inference/kv_cache/configure\|inference/decoding/configure\|inference/cloud_engine/configure\|inference/batching/submit_and_wait' \
  lib/inference/*.md pdk/llama_engine/src/inference_arch.cpp tests/test_llama_engine_plugin.cpp
# 期望: 每个文件至少 2+ 命中
```

### 4.3 ctest 零回归

```bash
cmake --preset tests && ctest --output-on-failure
```

### 4.4 openspec validate

```bash
openspec validate fix-adr-naming-policy-2026-07-08
```

---

## 5. 回滚策略

```bash
git revert <commit-hash>  # 单 commit 回滚全部变更
```

单个文件回退：
```bash
git checkout HEAD~1 -- lib/inference/prefix_cache.md
```

---

## 6. Non-goals

- 不修改 C++ 逻辑（纯字符串替换）
- 不修改 engine/model 工具（已 SLASH）
- 不修改 `lib/inference/engine.md` / `model.md`（已 SLASH）
- 不修改 C16 proposal（`inference.*` 是合法事件 topic notation）
- 不修改已归档的 C13/C14 OpenSpec change 文件
- 不重跑 `tools/adr_relationships.py`（属 Change C）

## 7. 依赖

- **前置**: Change A (hotfix) ✅ 已 ship + archive
- **无阻塞**: 本 change 独立执行，不依赖 B2 owner 协调（C13/C14 已归档）
- **后续**: Change C (P2 cleanup) 等待本 change ship