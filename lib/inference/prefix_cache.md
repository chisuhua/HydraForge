### AgenticDSL `/lib/inference/prefix_cache`

> **关联**: C13 (`phase5-b2-arch-schemas`) — B2.3 架构层配置 schema
> **实施日期**: 2026-07-07 (C13 ship)
> **底层实现**: C14 `pdk/llama_engine/` 内部委托 llama.cpp 内置 prefix cache

```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
module: "inference::prefix_cache"
signature: "(enabled: bool, max_size: int) -> (status: string, active_patterns: int)"
permissions:
  - tool: prefix_cache.configure
nodes:
  # 节点1: 配置 prefix cache
  - id: configure_prefix_cache
    type: tool_call
    tool: prefix_cache.configure
    arguments:
      enabled: "{{ inputs.enabled | default(true) }}"
      max_size: "{{ inputs.max_size | default(512) }}"  # pattern 数量上限
    output_keys: ["status", "active_patterns", "error"]
    next: ["/lib/inference/prefix_cache/check_status"]

  # 节点2: 检查配置结果
  - id: check_status
    type: assert
    condition: "{{ error == null }}"
    on_failure: "/lib/inference/prefix_cache/handle_error"
    next: ["/lib/inference/prefix_cache/success_output"]

  # 节点3: 错误处理
  - id: handle_error
    type: assign
    assign:
      status: "error: {{ error }}"
      active_patterns: 0
    output_keys: ["status", "active_patterns"]
    next: ["/end_soft"]

  # 节点4: 成功输出
  - id: success_output
    type: assign
    assign:
      status: "{{ status | default('ok') }}"
      active_patterns: "{{ active_patterns | default(0) }}"
    output_keys: ["status", "active_patterns"]
    next: ["/end_soft"]
# --- END AgenticDSL ---
```

## 说明

**功能**: Prefix cache 配置 (架构层配置 schema),委托给当前激活的 engine plugin

**输入**:
- `enabled`: 是否启用 prefix cache (默认 `true`)
- `max_size`: pattern 数量上限 (默认 `512`,用于限制 cache 占用)

**输出**:
- `status`: 配置结果状态 (`"ok"` / `"error: ..."`)
- `active_patterns`: 当前 cache 中活跃 pattern 数量

**底层实现**:
- **不创建新的 C++ 工具** — 架构层仅暴露配置 schema
- 实际配置委托给当前激活的 engine plugin (C14 `pdk/llama_engine/`)
- engine plugin 内部使用 llama.cpp 自带的 prefix cache 功能

**依赖关系**:
- 上游: 无 (架构层基元)
- 下游: C14 `lib/inference/engine.md` (engine plugin 可调用本 subgraph)
- 关联: B2.1 + B2.3

**与 D1 决策一致性**: 采样器 clamp 逻辑内联到 engine plugin (不在此处抽取接口)。

**与 D3 决策一致性**: 架构层工具名 `prefix_cache.configure` (不带 `inference/` 前缀)。

---

*文档版本*: 1.0 (C13 ship, 2026-07-07)
*下次更新*: C14 实施后补充底层工具注册位置
