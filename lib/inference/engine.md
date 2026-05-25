### AgenticDSL `/lib/inference/engine`

```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
module: "inference::engine"
signature: "(device: string, gpu_layers: int, memory_limit_mb: int) -> (engine_id: string, status: string)"
permissions:
  - tool: inference.engine_init
nodes:
  # 节点1: 初始化推理引擎
  - id: init_engine
    type: tool_call
    tool: inference.engine_init
    arguments:
      device: "{{ inputs.device }}"
      gpu_layers: "{{ inputs.gpu_layers }}"
      memory_limit_mb: "{{ inputs.memory_limit_mb }}"
    output_keys: ["engine_id", "error"]
    next: ["/lib/inference/engine/check_status"]

  # 节点2: 检查初始化结果
  - id: check_status
    type: assert
    condition: "{{ error == null }}"
    on_failure: "/lib/inference/engine/handle_error"
    next: ["/lib/inference/engine/success_output"]

  # 节点3: 错误处理分支
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

**功能**: 推理引擎生命周期管理 —— 初始化推理引擎并返回引擎 ID。

**输入**:
- `device`: 设备类型，"cuda" 或 "cpu"
- `gpu_layers`: GPU 层数（0 表示全 CPU）
- `memory_limit_mb`: 内存限制（MB）

**输出**:
- `engine_id`: 引擎唯一标识符（成功时）
- `status`: 状态字符串（"ok" 或 "error: ..."）

**依赖工具**:
- `inference.engine_init`: 底层 C++ 工具，调用 llama.cpp 的引擎初始化

**错误处理**:
- 使用 `assert` 节点检查 `error` 字段
- 失败时通过 `on_failure` 跳转到错误处理分支
- 返回空 engine_id 和错误状态

**使用示例**:
```yaml
## /main/init
  type: dsl_call
  subgraph: "/lib/inference/engine"
  inputs:
    device: "cuda"
    gpu_layers: 35
    memory_limit_mb: 8192
  output_keys: ["engine_id", "status"]
```

## 特点
- 纯 tool_call 包装，零新增运行时依赖
- 使用现有 `assert` + `on_failure` 机制做错误处理
- 遵循标准库子图规范（signature、permissions、注释）
