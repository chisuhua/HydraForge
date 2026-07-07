### AgenticDSL `/lib/inference/batching` (PLACEHOLDER)

> ⚠️ **PLACEHOLDER** — 实现在 Phase 5 Stage 2+
> **关联**: C15 (`phase5-batching-queue-plugin`) — 精简版 (按 D2 决策)
> **推迟原因**: Adversarial Review 调研 7 个主流推理引擎 (vLLM / SGLang / llama.cpp / TRT-LLM / TGI / LMDeploy / lit-gpt) 后确认**零项目有独立 BatchingQueue 接口**,batching 策略均内联在 engine scheduler/executor 中
> **重启条件**: 第二个推理后端出现时,或云端 batch queue 需求出现时
> **底层实现**: ❌ **无** (本文件仅 schema 占位,推迟到 Stage 2+)

```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
module: "inference::batching"
signature: "(prompt: string, timeout_ms: int) -> (request_id: int, result: string)"
permissions:
  - tool: batching.submit_and_wait
nodes:
  # 节点1: 提交并等待批处理结果
  - id: submit_and_wait
    type: tool_call
    tool: batching.submit_and_wait
    arguments:
      prompt: "{{ inputs.prompt }}"
      timeout_ms: "{{ inputs.timeout_ms | default(30000) }}"
    output_keys: ["request_id", "result", "error"]
    next: ["/lib/inference/batching/check_status"]

  # 节点2: 检查结果
  - id: check_status
    type: assert
    condition: "{{ error == null }}"
    on_failure: "/lib/inference/batching/handle_error"
    next: ["/lib/inference/batching/success_output"]

  # 节点3: 错误处理 (PLACEHOLDER 路径)
  - id: handle_error
    type: assign
    assign:
      request_id: 0
      result: "error: not_yet_implemented — C15 schema-only ship, BatchingQueue 接口推迟到第二个推理后端出现时"
    output_keys: ["request_id", "result"]
    next: ["/end_soft"]

  # 节点4: 成功输出 (PLACEHOLDER 路径)
  - id: success_output
    type: assign
    assign:
      request_id: "{{ request_id | default(0) }}"
      result: "{{ result | default('') }}"
    output_keys: ["request_id", "result"]
    next: ["/end_soft"]
# --- END AgenticDSL ---
```

## 说明

**功能**: Batching (架构层占位 schema),定义 DSL 层 batching 工具契约。本文件**仅 schema 占位**。

**输入**:
- `prompt`: 输入 prompt
- `timeout_ms`: 超时时间 (ms,默认 `30000`)

**输出**:
- `request_id`: 请求 ID (批处理请求追踪)
- `result`: 推理结果 (同步返回)

## ⚠️ C15 推迟决策 (D2 已应用)

Adversarial Review 2026-07-07 (`docs/adversarial-reviews/main-report.md` §3) 调研结论:
- **零主流推理引擎** (vLLM / SGLang / llama.cpp / TRT-LLM / TGI / LMDeploy / lit-gpt) 有独立 BatchingQueue 接口
- batching 策略均**内联在 engine scheduler / executor** 中
- 在只有 1 个后端 (llama.cpp) 时抽取 `BatchingQueue` 接口是**过度抽象**

**已删除** (D2 决策已应用):
- ❌ `include/agenticdsl/pdk/batching_queue.h` (BatchingQueue PDK 接口, 5 方法)
- ❌ `pdk/llama_engine/src/llama_batching.cpp` (LlamaBatchingQueue reference impl)
- ❌ 4 个第三方贡献流程文档任务
- ❌ ADR-0021 §8 追加任务 (semver 政策非本 change 范围)

## 未来重启条件

本 schema 在以下任一条件满足时进入实施阶段:

1. **第二个推理后端出现** (例如 vLLM / SGLang / cloud provider)
2. **云端 batch queue 需求出现** (例如 Anthropic Batch API / OpenAI Batch API)
3. **跨后端 batching 策略需要抽象** (例如评估不同 scheduler 策略)

实施时**必须**:
1. 创建 `include/agenticdsl/pdk/batching_queue.h` 接口
2. 创建 `pdk/{new_backend}/batching/` 各后端 reference impl
3. `LLMProviderFactory` 增加 `batching` 路由条目
4. 添加 batch request 追踪 (request_id 持久化 / 查询接口)
5. 第三方贡献流程文档

---

*文档版本*: PLACEHOLDER v0.1 (C15 ship, 2026-07-07, 精简版)
*下次更新*: 第二个推理后端出现 / 云端 batch queue 需求出现时,启动 C15.1 实施
