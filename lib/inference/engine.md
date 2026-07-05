### AgenticDSL `/lib/inference/engine` (PLACEHOLDER)

> ⚠️ **Placeholder** — 创建于 2026-07-04 (Week 1 Day 1 drift 修复)
> **状态**: 占位 (结构同 session.md 模板,等待 B2 Week 2 实施填充)
> **关联**: `lib/inference/model.md` (同为占位) + `lib/inference/session.md` (已 ship 完整模板)

```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
module: "inference::engine"
signature: "(engine_type: string, config: json) -> (engine_id: string, status: string)"
permissions:
  - tool: inference.engine_init
nodes:
  # 节点1: 初始化推理引擎
  - id: init_engine
    type: tool_call
    tool: inference.engine_init
    arguments:
      engine_type: "{{ inputs.engine_type }}"
      config: "{{ inputs.config }}"
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

**功能**: 推理引擎初始化 —— 创建推理引擎实例,作为推理链路的第一步。

**输入**:
- `engine_type`: 引擎类型 (llama.cpp / vllm / sglang)
- `config`: 引擎配置 JSON (model_path / n_ctx / n_threads 等)

**输出**:
- `engine_id`: 引擎唯一标识符 (成功时)
- `status`: 状态字符串 ("ok" 或 "error: ...")

**依赖工具**:
- `inference.engine_init`: 底层 C++ 工具 (待 B2 实施注册)

**错误处理**:
- 使用 `assert` 节点检查 `error` 字段
- 失败时通过 `on_failure` 跳转到错误处理分支

**使用示例**:
```yaml
## /main/setup
  type: dsl_call
  subgraph: "/lib/inference/engine"
  inputs:
    engine_type: "llama.cpp"
    config: {"model_path": "/models/llama-3.1-8b.gguf", "n_ctx": 8192, "n_threads": 8}
  output_keys: ["engine_id", "status"]
```

## 特点
- 纯 tool_call 聚合,无新增运行时依赖
- 暴露核心引擎控制参数 (engine_type + config)
- 遵循标准库子图规范 (与 session.md 同模式)

## 与自举的关系

引擎是推理链路的起点 (`engine → model → session`)。Agent 通过 DSL 控制推理引擎初始化,实现"推理基础架构的 DSL 化"。

## B2 实施清单 (待执行)

- [ ] 注册 `inference.engine_init` 工具 (`src/common/tools/registry.cpp`)
- [ ] 实现 `engine_init` 底层 C++ 函数 (调用对应引擎 API)
- [ ] 添加 `tests/test_engine_subgraph.cpp` (init_engine / check_status / error 路径)
- [ ] 更新本文件 `placeholder` 标记 → 删除,改为完整 ship 状态
- [ ] 在 master plan §五.4 + §十六.4 标记 engine.md "shipped"

---

*文档版本*: PLACEHOLDER v0.1 (2026-07-04)
*下次更新*: B2 Week 2 实施完成后