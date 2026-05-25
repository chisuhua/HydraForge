### AgenticDSL `/lib/inference/session`

```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
module: "inference::session"
signature: "(engine_id: string, model_id: string, temperature: float, top_p: float, max_tokens: int, stop_tokens: list) -> (session_id: string, status: string)"
permissions:
  - tool: inference.engine_init
  - tool: inference.model_load
  - tool: inference.session_create
nodes:
  # 节点1: 创建推理会话
  - id: create_session
    type: tool_call
    tool: inference.session_create
    arguments:
      engine_id: "{{ inputs.engine_id }}"
      model_id: "{{ inputs.model_id }}"
      temperature: "{{ inputs.temperature }}"
      top_p: "{{ inputs.top_p }}"
      max_tokens: "{{ inputs.max_tokens }}"
      stop_tokens: "{{ inputs.stop_tokens }}"
    output_keys: ["session_id", "error"]
    next: ["/lib/inference/session/check_status"]

  # 节点2: 检查创建结果
  - id: check_status
    type: assert
    condition: "{{ error == null }}"
    on_failure: "/lib/inference/session/handle_error"
    next: ["/lib/inference/session/success_output"]

  # 节点3: 错误处理
  - id: handle_error
    type: assign
    assign:
      session_id: ""
      status: "error: {{ error }}"
    output_keys: ["session_id", "status"]
    next: ["/end_soft"]

  # 节点4: 成功输出
  - id: success_output
    type: assign
    assign:
      session_id: "{{ session_id }}"
      status: "ok"
    output_keys: ["session_id", "status"]
    next: ["/end_soft"]
# --- END AgenticDSL ---
```

## 说明

**功能**: 推理会话聚合 —— 整合引擎、模型和推理参数，创建完整推理会话。

**输入**:
- `engine_id`: 引擎 ID（由 engine.md 返回）
- `model_id`: 模型 ID（由 model.md 返回）
- `temperature`: 采样温度（0.0-2.0）
- `top_p`: nucleus sampling 参数（0.0-1.0）
- `max_tokens`: 最大生成 token 数
- `stop_tokens`: 停止 token 列表（如 ["\n", "###"]）

**输出**:
- `session_id`: 会话唯一标识符（成功时）
- `status`: 状态字符串（"ok" 或 "error: ..."）

**依赖工具**:
- `inference.session_create`: 底层 C++ 工具，创建推理会话

**错误处理**:
- 使用 `assert` 节点检查 `error` 字段
- 失败时通过 `on_failure` 跳转到错误处理分支

**使用示例**:
```yaml
## /main/setup
  type: dsl_call
  subgraph: "/lib/inference/session"
  inputs:
    engine_id: "{{ engine.engine_id }}"
    model_id: "{{ model.model_id }}"
    temperature: 0.7
    top_p: 0.9
    max_tokens: 1024
    stop_tokens: ["\n", "###"]
  output_keys: ["session_id", "status"]
```

## 特点
- 纯 dsl_call 聚合，无新增运行时依赖
- 聚合 engine.md 和 model.md 的输出
- 暴露核心推理控制参数（temperature, top_p, max_tokens, stop_tokens）
- 遵循标准库子图规范

## 与自举的关系

这是自举链路的关键节点：
```
阶段0: C++ 硬编码参数 → 无法动态调整
阶段1: 本 session.md → Agent 通过 DSL 控制 temperature/top_p/max_tokens
阶段2: 未来 → Agent 根据 workload 自动选择 decoding 策略
```

通过本子图，Agent 可以在工作流中动态调整推理参数，实现"推理策略的 DSL 化"。
