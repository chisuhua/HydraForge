### AgenticDSL `/lib/inference/model`

> **关联**: C14 (`phase5-llama-engine-plugin`) — B2.2 模型管理 schema
> **实施日期**: 2026-07-07 (C14 tasks §6.2 就绪, 待 C14 编码后激活)
> **底层实现**: `pdk/llama_engine/src/llama_model.cpp` (4 模型工具)

```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
module: "inference::model"
signature: "(engine_id: string, model_path: string, model_type: string, quantization: string) -> (model_id: string, status: string)"
permissions:
  - tool: inference/model/load
  - tool: inference/model/unload
  - tool: inference/model/list
  - tool: inference/model/switch
nodes:
  # 节点1: 加载模型
  - id: load_model
    type: tool_call
    tool: inference/model/load
    arguments:
      engine_id: "{{ inputs.engine_id }}"
      model_path: "{{ inputs.model_path }}"
      model_type: "{{ inputs.model_type }}"
      quantization: "{{ inputs.quantization }}"
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

**功能**: 推理模型加载与管理 —— 在已初始化的引擎上加载具体模型,作为推理链路的第二步 (`engine → model → session`)。

**输入**:
- `engine_id`: 引擎 ID (由 `lib/inference/engine.md` 返回)
- `model_path`: 模型文件路径 (gguf / safetensors 格式)
- `model_type`: 模型架构 (`llama` / `mistral` / `qwen` / `gemma`)
- `quantization`: 量化方案 (`q4_0` / `q8_0` / `fp16` / `bf16`)

**输出**:
- `model_id`: 模型唯一标识符 (成功时)
- `status`: 状态字符串 (`"ok"` / `"error: ..."`)

**依赖工具** (C14 注册):
- `inference/model/load`: 加载模型到引擎 (`pdk/llama_engine/src/llama_model.cpp`)
- `inference/model/unload`: 释放模型资源
- `inference/model/list`: 列出已加载模型
- `inference/model/switch`: 切换活跃模型

**错误处理**:
- 使用 `assert` 节点检查 `error` 字段
- 失败时通过 `on_failure` 跳转到错误处理分支

**使用示例**:
```yaml
## /main/setup
  type: dsl_call
  subgraph: "/lib/inference/model"
  inputs:
    engine_id: "{{ engine.engine_id }}"
    model_path: "/models/llama-3.1-8b-instruct-q4_0.gguf"
    model_type: "llama"
    quantization: "q4_0"
  output_keys: ["model_id", "status"]
```

## 特点
- 纯 tool_call 聚合, 无新增运行时依赖
- 消费 `engine.md` 的输出 (engine_id)
- 暴露核心模型控制参数 (model_path / model_type / quantization)
- 遵循标准库子图规范 (与 session.md 同模式)
- D3 决策: 工具名统一为 `inference/model/*` 命名空间

## 与自举的关系

模型加载是推理链路的核心步骤 (`engine → model → session`)。Agent 通过 DSL 控制模型加载参数, 实现"模型选择的 DSL 化"。

## B2.2 实施清单 (C14 编码后)

- [ ] C14 注册 `inference/model/load` 工具 (`pdk/llama_engine/src/llama_model.cpp`)
- [ ] C14 注册 `inference/model/unload` 工具
- [ ] C14 注册 `inference/model/list` 工具
- [ ] C14 注册 `inference/model/switch` 工具
- [ ] 添加 `tests/test_llama_engine_plugin.cpp` (4 model tests: load/unload/list/switch)
- [ ] 升级本文件从 PLACEHOLDER → 完整 ship 状态

---

*文档版本*: 1.0 (C14 tasks §6.2 就绪, 2026-07-07)
*Schema 版本*: B2.2 (与 C14 同步激活)
*下次更新*: C14 编码完成后删除 B2.2 实施清单, 改为完整 ship 状态