### AgenticDSL `/lib/inference/cloud_engine` (PLACEHOLDER)

> ⚠️ **PLACEHOLDER** — 第三方 cloud LLM engine plugin 契约 schema (Phase 5 Stage 2+)
> **关联**: C13 (`phase5-b2-arch-schemas`) — cloud_engine.md 占位契约
> **关联**: C16 (`phase5-illmprovider-call-chain-v2`) — Cloud plugin 化基础
> **第三方 plugin 团队** (待加入): `pdk/cloud_engine/openai/`, `pdk/cloud_engine/anthropic/`, `pdk/cloud_engine/deepseek/`, 等
> **底层实现**: 待 C16 + 后续 sprint 阶段实施,本文件仅定义 schema 契约

```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
module: "inference::cloud_engine"
signature: "(provider: string, model: string, api_key_ref: string) -> (status: string, provider: string, model: string)"
permissions:
  - tool: cloud_engine.configure
nodes:
  # 节点1: 配置 cloud engine provider + model
  - id: configure_cloud_engine
    type: tool_call
    tool: cloud_engine.configure
    arguments:
      provider: "{{ inputs.provider }}"           # openai | anthropic | deepseek | qwen | moonshot | custom
      model: "{{ inputs.model }}"
      api_key_ref: "{{ inputs.api_key_ref }}"     # 引用 secret store 而非明文
    output_keys: ["status", "provider", "model", "error"]
    next: ["/lib/inference/cloud_engine/check_status"]

  # 节点2: 检查配置结果
  - id: check_status
    type: assert
    condition: "{{ error == null }}"
    on_failure: "/lib/inference/cloud_engine/handle_error"
    next: ["/lib/inference/cloud_engine/success_output"]

  # 节点3: 错误处理
  - id: handle_error
    type: assign
    assign:
      status: "error: {{ error }}"
      provider: ""
      model: ""
    output_keys: ["status", "provider", "model"]
    next: ["/end_soft"]

  # 节点4: 成功输出 (PLACEHOLDER — 实施后从底层 plugin 读取)
  - id: success_output
    type: assign
    assign:
      status: "{{ status | default('ok') }}"
      provider: "{{ provider | default(inputs.provider) }}"
      model: "{{ model | default(inputs.model) }}"
    output_keys: ["status", "provider", "model"]
    next: ["/end_soft"]
# --- END AgenticDSL ---
```

## 说明

**功能**: Cloud engine provider + model 配置 (架构层占位 schema),路由到第三方 cloud engine plugin。

**输入**:
- `provider`: cloud provider 标识 (默认无,必填)
  - 可选值: `openai` / `anthropic` / `deepseek` / `qwen` / `moonshot` / `custom`
- `model`: 模型标识 (e.g., `gpt-4o`, `claude-3-5-sonnet`, 等)
- `api_key_ref`: API key 引用 (**不存明文**,引用 secret store / 环境变量)

**输出**:
- `status`: 配置结果状态 (`"ok"` / `"error: ..."`)
- `provider`: 当前生效的 provider
- `model`: 当前生效的 model

**⚠️ 占位状态 — 实施推迟到 Phase 5 Stage 2+**:

本文件定义的是**架构层契约**,供第三方 plugin 团队按 schema 实施 `cloud_engine.configure` 工具。当前**无底层 C++ 实现**:

- ❌ **没有** `pdk/cloud_engine/openai/` plugin
- ❌ **没有** `pdk/cloud_engine/anthropic/` plugin
- ❌ **没有** `pdk/cloud_engine/deepseek/` 等其他 provider
- ❌ **没有** `src/modules/llm/cloud_engine.cpp` 工具实现

**未来实施路径** (Phase 5 Stage 2+ 启动后):
1. 第三方 plugin 团队按本 schema 实施 `cloud_engine.configure` 工具
2. plugin 工具通过 `pdk/cloud_engine/{provider}/` 目录组织
3. `LLMProviderFactory` 增加 `cloud_engine` 路由条目
4. secret store 抽象 (`api_key_ref` 解析) 由 `src/common/secret_store.h` 提供

**依赖关系**:
- 上游: 无 (架构层基元)
- 下游: C16 (`phase5-illmprovider-call-chain-v2`) — Cloud plugin 化基础
- 后续: 第三方 plugin 团队按 schema 实施
- 关联: Stage 2+ 启动条件 (性能/运维需求出现时)

**安全约束**:
- `api_key_ref` 必须引用 secret store,禁止明文存储
- 实施时需要 `src/common/secret_store.h` 提供引用解析
- CloudLLMAdapter 当前通过 HTTP header 注入 key, 后续 plugin 化保持一致

---

*文档版本*: PLACEHOLDER v0.1 (C13 ship, 2026-07-07)
*下次更新*: Phase 5 Stage 2+ 启动后,由第三方 plugin 团队实施底层工具
