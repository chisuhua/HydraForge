# IP-003: 推理标准库实现规格

**ID**: IP-003
**日期**: 2026-05-20
**状态**: 已批准（Oracle 确认依赖分析）
**关联**: ADR-006, ADR-002, ADR-003
**调研依据**: Oracle 依赖分析验证（bg_0a82905e）

---

## 目录结构

```
lib/inference/
├── engine.md              # 推理引擎生命周期      ← 现在可构建
├── model.md               # 模型管理              ← 现在可构建
├── kv_cache.md             # KV-cache 策略        ← 现在可构建
├── prefix_cache.md         # Prefix-cache 策略    ← 现在可构建
├── batching.md             # 动态 batching        ← 现在可构建
├── decoding.md             # 解码策略              ← 现在可构建
└── session.md              # 完整推理会话          ← 现在可构建

# orchestrate.md 移除 — 编排是用户 workflow 层业务逻辑，非标准库原语
```

---

## 每个文件的内容规格

### engine.md

```yaml
# lib/inference/engine.md
graph_type: subgraph
signature: "(device: string, gpu_layers: int, memory_limit_mb: int) -> (engine_id: string)"

module_state:
  initialized:
    type: bool
    scope: session
    init: false
    fork_behavior: inherit

工具调用:
  - inference.engine_init
```

### model.md

```yaml
# lib/inference/model.md
graph_type: subgraph
signature: "(model_path: string, quantization: string, context_size: int) -> (model_id: string)"

输入验证:
  - quantization in ["f16", "f8", "q4_0", "q4_K_M", "q8_0"]
  - context_size 必须是 512 的倍数

工具调用:
  - inference.model_load
```

### kv_cache.md

```yaml
# lib/inference/kv_cache.md
graph_type: subgraph
signature: "(strategy: string, max_pages: int, page_size: int) -> (config: json)"

module_state:
  page_table:
    type: array<int>
    scope: session
    init: "[]"
    fork_behavior: deep_copy
  current_pos:
    type: int
    scope: session
    init: 0

输入验证:
  - strategy in ["fifo", "lru", "lfu", "adaptive"]

工具调用:
  - inference.kv_cache_config
  - inference.kv_cache_stats
  - inference.kv_cache_clear

跨模块导入:
  - batching.md 可以 readonly 读取 page_table
```

### prefix_cache.md

```yaml
# lib/inference/prefix_cache.md
graph_type: subgraph
signature: "(enabled: bool, algorithm: string, max_entries: int) -> (config: json)"

输入验证:
  - algorithm in ["rolling_hash", "exact", "suffix"]

工具调用:
  - inference.prefix_cache_config
  - inference.prefix_cache_stats
```

### batching.md

```yaml
# lib/inference/batching.md
graph_type: subgraph
signature: "(max_batch_size: int, scheduling: string, timeout_ms: int) -> (config: json)"

module_state:
  queue:
    type: array
    scope: session
    init: "[]"
    fork_behavior: inherit

imports_module_state:
  - module: "/lib/inference/kv_cache"
    fields:
      - name: page_table
        access: readonly
      - name: current_pos
        access: readonly

输入验证:
  - scheduling in ["oldest_first", "shortest_first", "priority"]

工具调用:
  - inference.batch_config
  - inference.batch_stats
```

### decoding.md

```yaml
# lib/inference/decoding.md
graph_type: subgraph
signature: "(strategy: string, n_speculate: int, draft_model: string) -> (config: json)"

module_state:
  rng_state:
    type: int
    scope: session
    init: "{{uuid()}}"
    fork_behavior: inherit

输入验证:
  - strategy in ["greedy", "sampling", "mtp", "speculative"]

工具调用:
  - inference.decode_config
  - inference.decode_stats
```

### session.md

```yaml
# lib/inference/session.md
graph_type: subgraph
signature: "(device: string, model: string, configs: json) -> (session: json)"

内部流程:
  1. engine.init
  2. model.load
  3. fork（并行初始化所有子系统）
     - kv_cache
     - prefix_cache
     - batching
     - decoding
  4. 等待全部就绪
  5. 返回 session

module_state:
  engine_id: { type: string, scope: session, init: "", fork_behavior: inherit }
  model_id:  { type: string, scope: session, init: "", fork_behavior: inherit }
```

### orchestrate.md

```yaml
# lib/inference/orchestrate.md
graph_type: subgraph
signature: "(workload: json) -> (engine: json)"

内部流程:
  1. dsl_call: 分析 workload（LLM 选择策略）
  2. fork: 并行应用所有策略
  3. engine.init
  4. 监控循环: get_stats → 分析 → 调整/继续
```

---

## 工具 → 标准库映射表

| C++ 工具 | 标准库子图 | 暴露给 Agent 作为 |
|----------|-----------|-----------------|
| inference.engine_init | engine.md | 子图调用 |
| inference.model_load | model.md | 子图调用 |
| inference.kv_cache_config | kv_cache.md | 子图调用 |
| inference.prefix_cache_config | prefix_cache.md | 子图调用 |
| inference.batch_config | batching.md | 子图调用 |
| inference.decode_config | decoding.md | 子图调用 |
| inference.generate | (直接调用) | 工具调用 |
| inference.forward_token | (直接调用，用于 yield) | 工具调用 |

---

## Oracle 依赖分析验证（2026-05-21）

> 在 Lazy ModuleState（json scope nesting）的 MVP 路径下，之前的依赖分析过于悲观。
> Oracle 确认后的正确矩阵：

| 子图 | 需要新运行时？ | MVP 可构建？ | 说明 |
|------|-------------|------------|------|
| engine.md | ❌ 零依赖 | ✅ **现在** | 纯 tool_call 包装 |
| model.md | ❌ 零依赖 | ✅ **现在** | 同上 |
| prefix_cache.md | ❌ 零依赖 | ✅ **现在** | 同上 |
| session.md | ❌ 零依赖 | ✅ **现在** | 纯 dsl_call 聚合 |
| kv_cache.md | 😶 可绕过 | ✅ **现在** | 用 json scope nesting 存 page_table / current_pos |
| batching.md | 😶 可绕过 | ✅ **现在** | 自包含 queue 管理，不 import 其他模块 |
| decoding.md | 😶 可绕过 | ✅ **现在** | 同 kv_cache.md |
| ~~orchestrate.md~~ | — | ❌ 用户 workflow | 编排不是标准库原语 |

**关键发现**：当前代码库 + Lazy ModuleState（Step 0 json scope nesting）已经足够实现全部 7 个积木子图。
不需要等待 YIELD、ModuleState schema、imports_module_state。

### 子图间的调用关系（修正后）

```
engine.md ────────────────────→ 提供 engine_id
  │
  ├── model.md ──────────────→ 返回 model_id（依赖 engine_id）
  │
  ├── kv_cache.md ───────────→ 配置 KV-cache（依赖 engine_id, scope nesting 存状态）
  │
  ├── prefix_cache.md ───────→ 全局配置，无依赖
  │
  ├── batching.md ───────────→ 自包含 queue 管理（不 import kv_cache）
  │
  ├── decoding.md ───────────→ 配置解码（scope nesting 存 position）
  │
  └── session.md ────────────→ 聚合调用以上全部
```

---

## 关联文档

| 文档 | 关系 |
|------|------|
| [01-interface-design.md](01-interface-design.md) | 推理标准库的接口设计 — 本文 subgraph 定义的直接上游 |
| [IP-001: 实施路线图](../implementation-roadmap/01-roadmap.md) | 本文依赖的 Step 1~4 的详细实施计划 |
| [IP-002: 扩展点映射](../implementation-roadmap/02-code-mapping.md) | 工具接口注册的代码改动位置 |
| [docs/specs/stdlib-v3.10.md](../../specs/stdlib-v3.10.md) | 当前标准库目录结构（reasoning/workflow/tools/cognitive），推理标准库遵循其路径约定 |
| [docs/adr/adr-0009-dsl-standard-library.md](../../adr/adr-0009-dsl-standard-library.md) | 标准库规划，推理标准库是规划的扩展方向 |
| [docs/archive/specs/phase2-standard-library-v1.0.md](../../archive/specs/phase2-standard-library-v1.0.md) | 已归档的 Phase 2 标准库规划（ADR-0010~0018），与本文推理标准库互补（已退役） |
