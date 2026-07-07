### AgenticDSL `/lib/inference/decoding`

> **关联**: C13 (`phase5-b2-arch-schemas`) — B2.5 架构层配置 schema
> **实施日期**: 2026-07-07 (C13 ship)
> **底层实现**: C14 `pdk/llama_engine/` 内部委托 llama.cpp sampling

```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
module: "inference::decoding"
signature: "(temperature: float, top_p: float, top_k: int, repeat_penalty: float, sampler: string) -> (status: string, active_sampler: string, unsupported_warning: string)"
permissions:
  - tool: decoding.configure
nodes:
  # 节点1: 配置 decoding 参数
  - id: configure_decoding
    type: tool_call
    tool: decoding.configure
    arguments:
      temperature: "{{ inputs.temperature | default(0.7) }}"        # 0.0-2.0
      top_p: "{{ inputs.top_p | default(0.9) }}"                    # 0.0-1.0
      top_k: "{{ inputs.top_k | default(40) }}"                     # int
      repeat_penalty: "{{ inputs.repeat_penalty | default(1.1) }}"  # float
      sampler: "{{ inputs.sampler | default('greedy') }}"           # greedy | temperature | mirostat_v1 | mirostat_v2 | typical_p
    output_keys: ["status", "active_sampler", "unsupported_warning", "error"]
    next: ["/lib/inference/decoding/check_status"]

  # 节点2: 检查配置结果
  - id: check_status
    type: assert
    condition: "{{ error == null }}"
    on_failure: "/lib/inference/decoding/handle_error"
    next: ["/lib/inference/decoding/success_output"]

  # 节点3: 错误处理
  - id: handle_error
    type: assign
    assign:
      status: "error: {{ error }}"
      active_sampler: ""
      unsupported_warning: ""
    output_keys: ["status", "active_sampler", "unsupported_warning"]
    next: ["/end_soft"]

  # 节点4: 成功输出
  - id: success_output
    type: assign
    assign:
      status: "{{ status | default('ok') }}"
      active_sampler: "{{ active_sampler | default(inputs.sampler) }}"
      unsupported_warning: "{{ unsupported_warning | default('') }}"
    output_keys: ["status", "active_sampler", "unsupported_warning"]
    next: ["/end_soft"]
# --- END AgenticDSL ---
```

## 说明

**功能**: Decoding 参数配置 (架构层配置 schema),委托给当前激活的 engine plugin

**输入**:
- `temperature`: 采样温度 (0.0-2.0,默认 `0.7`)
- `top_p`: nucleus sampling 参数 (0.0-1.0,默认 `0.9`)
- `top_k`: top-K filtering 参数 (int,默认 `40`)
- `repeat_penalty`: 重复惩罚 (float,默认 `1.1`)
- `sampler`: 采样器策略字符串 (默认 `greedy`)
  - 可选值: `greedy` / `temperature` / `mirostat_v1` / `mirostat_v2` / `typical_p`

**输出**:
- `status`: 配置结果状态 (`"ok"` / `"error: ..."`)
- `active_sampler`: 当前生效的采样器 (若不支持请求的 sampler,可能回退到 greedy)
- `unsupported_warning`: 警告信息 (若 sampler 不被引擎支持,警告 + fallback 说明)

**底层实现**:
- **不创建新的 C++ 工具** — 架构层仅暴露配置 schema
- 实际配置委托给当前激活的 engine plugin (C14 `pdk/llama_engine/`)
- engine plugin 内部使用 llama.cpp 的 sampling API (`common_sampler`)
- **采样器 clamp 逻辑内联到 engine plugin 的 generate 工具内部**(D1 决策:不创建独立 SamplerStrategy PDK 接口)

**Sampler 支持矩阵** (以 llama.cpp 为例):
| Sampler | 支持 | 备注 |
|---------|:----:|------|
| `greedy` | ✅ | llama.cpp 原生支持 |
| `temperature` | ✅ | llama.cpp 原生支持 |
| `mirostat_v1` | ✅ | llama.cpp 原生支持 |
| `mirostat_v2` | ✅ | llama.cpp 原生支持 |
| `typical_p` | ✅ | llama.cpp 原生支持 |

**依赖关系**:
- 上游: 无 (架构层基元)
- 下游: C14 `lib/inference/engine.md` (engine plugin 调用本 subgraph)
- 关联: B2.1 + B2.5

**与 D1 决策一致性**: SamplerStrategy PDK 接口**删除**(D1 决策已应用),采样器 clamp 逻辑内联到 generate 工具实现内部,等出现第二个推理后端时再提取公共接口。

**与 D3 决策一致性**: 架构层工具名 `decoding.configure` (不带 `inference/` 前缀)。

---

*文档版本*: 1.0 (C13 ship, 2026-07-07)
*下次更新*: C14 实施后补充底层工具注册位置 + 第二个推理后端出现时评估 SamplerStrategy 接口提取
