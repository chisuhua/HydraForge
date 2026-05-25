### AgenticDSL `/lib/inference/model`

```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
module: "inference::model"
signature: "(engine_id: string, path: string, quant: string, ctx_size: int) -> (model_id: string, status: string)"
permissions:
  - tool: inference.model_load
nodes:
  # 节点1: 加载模型
  - id: load_model
    type: tool_call
    tool: inference.model_load
    arguments:
      engine_id: "{{ inputs.engine_id }}"
      path: "{{ inputs.path }}"
      quant: "{{ inputs.quant }}"
      ctx_size: "{{ inputs.ctx_size }}"
    output_keys: ["model_id", "error"]
    next: ["/lib/inference/model/check_status"]

  # 节点2: 检查加载结果
  - id: check_status
    type: assert
    condition: "{{ error == null }}"
    on_failure: "/lib/inference/model/handle_error"
    next: ["/lib/inference/model/success_output"]

  # 节点3: 错误处理
  - id: handle_error
    type: assign
    assign:
      model_id: ""
      status: "error: {{ error }}"
    output_keys: ["model_id", "status"]
    next: ["/end_soft"]

  # 节点4: 成功输出
  - id: success_output
    type: assign
    assign:
      model_id: "{{ model_id }}"
      status: "ok"
    output_keys: ["model_id", "status"]
    next: ["/end_soft"]
# --- END AgenticDSL ---
```

## 说明

**功能**: 模型加载管理 —— 在指定引擎上加载模型并返回模型 ID。

**输入**:
- `engine_id`: 引擎 ID（由 engine.md 返回）
- `path`: 模型文件路径
- `quant`: 量化格式（"f16", "f8", "q4"）
- `ctx_size`: 上下文窗口大小

**输出**:
- `model_id`: 模型唯一标识符（成功时）
- `status`: 状态字符串（"ok" 或 "error: ..."）

**依赖工具**:
- `inference.model_load`: 底层 C++ 工具，调用 llama.cpp 的模型加载

**错误处理**:
- 使用 `assert` 节点检查 `error` 字段
- 失败时通过 `on_failure` 跳转到错误处理分支

**使用示例**:
```yaml
## /main/load
  type: dsl_call
  subgraph: "/lib/inference/model"
  inputs:
    engine_id: "{{ engine.engine_id }}"
    path: "/models/llama-7b.gguf"
    quant: "q4"
    ctx_size: 4096
  output_keys: ["model_id", "status"]
```

## 特点
- 纯 tool_call 包装，零新增运行时依赖
- 依赖 engine.md 返回的 engine_id
- 遵循标准库子图规范
