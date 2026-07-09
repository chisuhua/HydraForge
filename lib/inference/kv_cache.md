### AgenticDSL `/lib/inference/kv_cache`

> **关联**: C13 (`phase5-b2-arch-schemas`) — B2.4 架构层配置 schema
> **实施日期**: 2026-07-07 (C13 ship)
> **底层实现**: C14 `pdk/llama_engine/` 内部委托 llama.cpp 内置 KV cache

```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
module: "inference::kv_cache"
signature: "(evict_policy: string, max_size_gb: float) -> (status: string, active_policy: string, current_size_gb: float)"
permissions:
  - tool: inference/kv_cache/configure
nodes:
  # 节点1: 配置 KV cache 策略
  - id: configure_kv_cache
    type: tool_call
    tool: inference/kv_cache/configure
    arguments:
      evict_policy: "{{ inputs.evict_policy | default('lru') }}"  # lru | lfu | fifo
      max_size_gb: "{{ inputs.max_size_gb | default(4.0) }}"
    output_keys: ["status", "active_policy", "current_size_gb", "error"]
    next: ["/lib/inference/kv_cache/check_status"]

  # 节点2: 检查配置结果
  - id: check_status
    type: assert
    condition: "{{ error == null }}"
    on_failure: "/lib/inference/kv_cache/handle_error"
    next: ["/lib/inference/kv_cache/success_output"]

  # 节点3: 错误处理
  - id: handle_error
    type: assign
    assign:
      status: "error: {{ error }}"
      active_policy: ""
      current_size_gb: 0.0
    output_keys: ["status", "active_policy", "current_size_gb"]
    next: ["/end_soft"]

  # 节点4: 成功输出
  - id: success_output
    type: assign
    assign:
      status: "{{ status | default('ok') }}"
      active_policy: "{{ active_policy | default(inputs.evict_policy) }}"
      current_size_gb: "{{ current_size_gb | default(0.0) }}"
    output_keys: ["status", "active_policy", "current_size_gb"]
    next: ["/end_soft"]
# --- END AgenticDSL ---
```

## 说明

**功能**: KV cache 配置 (架构层配置 schema),委托给当前激活的 engine plugin

**输入**:
- `evict_policy`: 驱逐策略 (`lru` | `lfu` | `fifo`,默认 `lru`)
- `max_size_gb`: 最大容量 (GB,默认 `4.0`)

**输出**:
- `status`: 配置结果状态 (`"ok"` / `"error: ..."`)
- `active_policy`: 当前生效的驱逐策略
- `current_size_gb`: 当前 KV cache 大小 (GB)

**底层实现**:
- **不创建新的 C++ 工具** — 架构层仅暴露配置 schema
- 实际配置委托给当前激活的 engine plugin (C14 `pdk/llama_engine/`)
- engine plugin 内部使用 llama.cpp 内置 KV cache (通过 `n_cache_size_gb` 等参数)

**依赖关系**:
- 上游: 无 (架构层基元)
- 下游: C14 `lib/inference/engine.md` (engine plugin 可调用本 subgraph)
- 关联: B2.1 + B2.4

**与 D1 决策一致性**: KV cache 策略算法在 engine plugin 内部实现 (不在此处抽取接口)。

**与 D3 决策一致性**: 架构层工具名 `inference/kv_cache/configure` (不带 `inference/` 前缀)。

---

*文档版本*: 1.0 (C13 ship, 2026-07-07)
*下次更新*: C14 实施后补充底层工具注册位置
