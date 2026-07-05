### AgenticDSL `/lib/inference/model` (PLACEHOLDER)

> ⚠️ **Placeholder** — 创建于 2026-07-04 (Week 1 Day 1 drift 修复)
> **状态**: 占位 (结构同 session.md 模板,等待 B2 Week 2 实施填充)
> **关联**: `lib/inference/engine.md` (同为占位,提供 engine_id) + `lib/inference/session.md` (已 ship 完整模板,消费 model_id)

```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
module: "inference::model"
signature: "(engine_id: string, model_path: string, model_type: string, quantization: string) -> (model_id: string, status: string)"
permissions:
  - tool: inference.model_load
nodes:
  # 节点1: 加载模型
  - id: load_model
    type: tool_call
    tool: inference.model_load
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

**功能**: 推理模型加载 —— 在已初始化的引擎上加载具体模型,作为推理链路的第二步。

**输入**:
- `engine_id`: 引擎 ID (由 `lib/inference/engine.md` 返回)
- `model_path`: 模型文件路径 (gguf / safetensors)
- `model_type`: 模型架构 (llama / mistral / qwen / gemma)
- `quantization`: 量化方案 (q4_0 / q8_0 / fp16 / bf16)

**输出**:
- `model_id`: 模型唯一标识符 (成功时)
- `status`: 状态字符串 ("ok" 或 "error: ...")

**依赖工具**:
- `inference.model_load`: 底层 C++ 工具 (待 B2 实施注册)

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
- 纯 tool_call 聚合,无新增运行时依赖
- 消费 `engine.md` 的输出 (engine_id)
- 暴露核心模型控制参数 (model_path / model_type / quantization)
- 遵循标准库子图规范 (与 session.md 同模式)

## 与自举的关系

模型加载是推理链路的核心步骤 (`engine → model → session`)。Agent 通过 DSL 控制模型加载参数,实现"模型选择的 DSL 化"。

## B2 实施清单 (待执行)

- [ ] 注册 `inference.model_load` 工具 (`src/common/tools/registry.cpp`)
- [ ] 实现 `model_load` 底层 C++ 函数 (调用 llama.cpp llama_load_model_from_file 等)
- [ ] 添加 `tests/test_model_subgraph.cpp` (load_model / check_status / error 路径)
- [ ] 更新本文件 `placeholder` 标记 → 删除,改为完整 ship 状态
- [ ] 在 master plan §五.4 + §十六.4 标记 model.md "shipped"

---

*文档版本*: PLACEHOLDER v0.1 (2026-07-04)
*下次更新*: B2 Week 2 实施完成后