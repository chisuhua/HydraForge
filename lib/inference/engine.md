### AgenticDSL `/lib/inference/engine`

> **关联**: C14 (`phase5-llama-engine-plugin`) — B2.1 引擎管理 schema
> **实施日期**: 2026-07-07 (C14 tasks §6.1 就绪, 待 C14 编码后激活)
> **底层实现**: `pdk/llama_engine/src/llama_engine.cpp` (8 引擎工具 + 4 架构工具)

```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
module: "inference::engine"
signature: "(model_path: string, n_ctx: int, n_gpu_layers: int) -> (status: string, engine_id: string)"
permissions:
  - tool: inference/engine/init
  - tool: inference/engine/generate
  - tool: inference/engine/stream
  - tool: inference/engine/status
nodes:
  # 节点1: 初始化推理引擎
  - id: init_engine
    type: tool_call
    tool: inference/engine/init
    arguments:
      model_path: "{{ inputs.model_path }}"
      n_ctx: "{{ inputs.n_ctx | default(2048) }}"
      n_gpu_layers: "{{ inputs.n_gpu_layers | default(0) }}"
    output_keys: ["engine_id", "error"]
    next: ["/lib/inference/engine/check_status"]

  # 节点2: 检查初始化结果
  - id: check_status
    type: assert
    condition: "{{ error == null }}"
    on_failure: "/lib/inference/engine/handle_error"
    next: ["/lib/inference/engine/success_output"]

  # 节点3: 错误处理
  - id: handle_error
    type: assign
    assign:
      engine_id: ""
      status: "error: {{ error }}"
    output_keys: ["engine_id", "status"]
    next: ["/end_soft"]

  # 节点4: 成功输出
  - id: success_output
    type: assign
    assign:
      engine_id: "{{ engine_id }}"
      status: "ok"
    output_keys: ["engine_id", "status"]
    next: ["/end_soft"]
# --- END AgenticDSL ---
```

## 说明

**功能**: 推理引擎初始化与管理 —— 创建推理引擎实例,作为推理链路的第一步 (`engine → model → session`)。

**输入**:
- `model_path`: 模型文件路径 (gguf 格式, 如 `/models/llama-3.1-8b.gguf`)
- `n_ctx`: 上下文窗口大小 (默认 `2048`)
- `n_gpu_layers`: GPU 层数 (默认 `0`, 纯 CPU 模式)

**输出**:
- `engine_id`: 引擎唯一标识符 (成功时)
- `status`: 状态字符串 (`"ok"` / `"error: ..."`)

**依赖工具** (C14 注册):
- `inference/engine/init`: 初始化 llama.cpp 引擎 (`pdk/llama_engine/src/llama_engine.cpp`)
- `inference/engine/generate`: 同步文本生成
- `inference/engine/stream`: 流式生成 (C12 YIELD 集成)
- `inference/engine/status`: 引擎状态查询 (kv_cache_size / sampler_config / 等)

**与架构工具的委托关系**:
- `prefix_cache.configure` → 引擎内部委托 llama.cpp 内置 prefix cache（C14 tasks §4.1）
- `kv_cache.configure` → 引擎内部委托 llama.cpp KV cache 策略（C14 tasks §4.1）
- `decoding.configure` → 引擎内部委托 llama.cpp sampling API（C14 tasks §4.1）

**错误处理**:
- 使用 `assert` 节点检查 `error` 字段
- 失败时通过 `on_failure` 跳转到错误处理分支

**使用示例**:
```yaml
## /main/setup
  type: dsl_call
  subgraph: "/lib/inference/engine"
  inputs:
    model_path: "/models/llama-3.1-8b-instruct-q4_0.gguf"
    n_ctx: 8192
    n_gpu_layers: 32
  output_keys: ["engine_id", "status"]
```

## 特点
- 纯 tool_call 聚合, 无新增运行时依赖
- 暴露核心引擎控制参数 (model_path / n_ctx / n_gpu_layers)
- 遵循标准库子图规范 (与 session.md 同模式)
- D3 决策: 工具名统一为 `inference/engine/*` 命名空间

## 与自举的关系

引擎是推理链路的起点 (`engine → model → session`)。Agent 通过 DSL 控制推理引擎初始化, 实现"推理基础架构的 DSL 化"。

## B2.1 实施清单 (C14 编码后)

- [ ] C14 注册 `inference/engine/init` 工具 (`pdk/llama_engine/src/llama_engine.cpp`)
- [ ] C14 注册 `inference/engine/generate` 工具 (同步生成)
- [ ] C14 注册 `inference/engine/stream` 工具 (与 C12 YIELD 集成)
- [ ] C14 注册 `inference/engine/status` 工具 (引擎状态查询)
- [ ] C14 注册 4 个 C13 架构工具 (`pdk/llama_engine/src/inference_arch.cpp`)
- [ ] 添加 `tests/test_llama_engine_plugin.cpp` (12 tests: 8 engine/model + 4 架构)
- [ ] 升级本文件从 PLACEHOLDER → 完整 ship 状态

---

*文档版本*: 1.0 (C14 tasks §6.1 就绪, 2026-07-07)
*Schema 版本*: B2.1 (与 C14 同步激活)
*下次更新*: C14 编码完成后删除 B2.1 实施清单, 改为完整 ship 状态