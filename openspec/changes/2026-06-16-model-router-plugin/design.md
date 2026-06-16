# Design: ModelRouter Plugin Stub (Sprint 0)

> **关联**: `proposal.md` (Sprint 0 入口, K1 决策)
> **设计依据**: phase1-execution.md §Sprint 0

## 架构决策

### K1 决策: Plugin Stub 而非真 Plugin

| 选项 | 描述 | 优势 | 劣势 |
|------|------|------|------|
| **A. 真 .so 加载** | Sprint 0 直接做 `dlopen` 验证 | 端到端 | 依赖 Sprint 4 PDK + Sprint 5 PluginLoader, Sprint 0 无法交付 |
| **B. Plugin Stub 验证** (K1 推荐) | 独立可执行 + MockLLMProvider + 复用 Sprint 4/5 Policy 逻辑 | Sprint 0 1 天可见 demo | Sprint 4/5 需迁移 Policy 到 PDK |
| **C. 推迟 Sprint 0 到 Sprint 5** | 整个 4 周计划变 4 Sprint | 避免 Stub 迁移 | 失去 1 天 Sprint 0 价值, 缓冲 -1 天 |

**决策**: B (Plugin Stub 验证)。Sprint 4/5 实施时复用 `ModelRouterPolicy` 类, 仅迁移调用入口 (从独立可执行 → `.so` Plugin)。

### Runtime 数据 vs Plugin 决策边界

| 边界 | 实现位置 | 原因 |
|------|----------|------|
| **数据**: 模型列表 + 能力 + context window | Runtime (`ILLMProvider::available_models()`) | 数据透明, 易于测试, 易于扩展 |
| **决策**: 路由策略 (cost / quality / latency / 第一个可用) | Plugin (`ModelRouterPolicy`) | 业务逻辑, 多样化, 第三方可扩展 |

**反例**: 若 Runtime 实现 `DefaultModelRouterPolicy`, 第三方想用"按用户层级路由"必须 fork Runtime → 违反 K1 决策。

## 数据结构

### `ModelCapability` enum

```cpp
enum class ModelCapability {
  Chat,        // 对话生成 (e.g. gpt-4, claude-3, mock-llm-v1)
  Completion,  // 文本补全 (e.g. gpt-3.5-turbo-instruct)
  Embedding,   // 向量嵌入 (e.g. text-embedding-3-small)
  ToolUse,     // 工具调用 (e.g. gpt-4-turbo, mock-llm-v1)
  Vision,      // 视觉输入 (e.g. gpt-4-vision)
};
```

**设计权衡**: 当前 5 个值足够覆盖 Phase 1 范围 (Sprint 1a 工具调用 + Sprint 4 PDK 工具注册)。Phase 2 可扩展 (FunctionCalling, JSON, Streaming)。

### `ModelInfo` struct

```cpp
struct ModelInfo {
  std::string name;                                  // 唯一标识
  std::vector<ModelCapability> capabilities;         // 能力标签
  std::int64_t context_window = 0;                   // 上下文窗口 (tokens)
  std::string provider = "unknown";                  // 提供方
};
```

**字段排序**: 按使用频率, `name` 唯一标识最先; `capabilities` 路由决策核心次之; `context_window` 容量检查; `provider` 调试元数据。

## 路由决策实现

### `ModelRouterPolicy::route(provider)` (Phase 1 Sprint 0 实现)

```cpp
static ModelInfo route(const ILLMProvider& provider) {
  const auto models = provider.available_models();
  if (models.empty()) {
    throw std::runtime_error("no models available from provider");
  }
  auto it = std::find_if(models.begin(), models.end(), [](const ModelInfo& m) {
    return std::any_of(m.capabilities.begin(), m.capabilities.end(),
                       [](ModelCapability c) { return c == ModelCapability::Chat; });
  });
  if (it == models.end()) {
    throw std::runtime_error("no Chat-capable model available");
  }
  return *it;
}
```

**算法**: 第一个 Chat-capable 模型, O(n*m) 其中 n=模型数, m=能力数。Phase 1 假设 n≤10, m≤5, 性能不敏感。

**错误处理**: 双重检查 (空列表 + 无 Chat-capable), 两类 `std::runtime_error` 区分原因, 便于上层决策 fallback。

## 实施计划

### Sprint 0 T1 (0.2d): Runtime 端

- 修改 `src/common/llm/llm_types.h`:
  - 新增 `ModelCapability` enum
  - 新增 `ModelInfo` struct
  - 新增 `virtual available_models() const` 默认空实现

### Sprint 0 T2 (0.3d): MockLLMProvider override

- 修改 `src/common/llm/mock_provider.h`:
  - 添加 `std::vector<ModelInfo> available_models() const override;`
- 修改 `src/common/llm/mock_provider.cpp`:
  - 实现返回 1 个 mock 模型 (`mock-llm-v1`)

### Sprint 0 T3 (0.3d): Plugin 端 examples

- 新建 `examples/phase1_model_router_plugin/`:
  - `ModelRouterPolicy` 静态类 + `main()` 演示
- 新建 `examples/phase1_plugin_demo/`:
  - 端到端 demo (MockLLMProvider + ModelRouterPolicy + generate())

### Sprint 0 T4 (0.2d): 单元测试

- 新建 `tests/test_model_router_policy.cpp`:
  - 5 个 TEST_CASE (含 StubProvider for 边界条件)

### Sprint 0 T5 (0.1d): 文档 + 收官

- 更新 OpenSpec 4 件套
- 更新 `docs/roadmap-status.md` Sprint 0 状态行
- commit + push

## 风险与缓解

| 风险 | 等级 | 缓解 |
|------|------|------|
| Sprint 5 迁移 Policy 时漏改调用入口 | 中 | Sprint 5 验收清单显式要求"复用 phase1_model_router_plugin/main.cpp Policy 逻辑" |
| `ModelCapability` 字段不足覆盖未来 Provider | 低 | 当前 5 值覆盖 Phase 1, Phase 2 可加 enum 值 (扩展点明确) |
| `available_models()` 调用频繁导致开销 | 低 | Runtime 默认空 vector, Provider 选择性 override, 调用次数 < 1/min 假设 |
| Sprint 0 改动 Phase 0 核心 API (`ILLMProvider`) | 中 | 仅加 default virtual, **不修改**既有 `generate()` / `generate_stream()` 签名, MockLLMProvider 选择性 override |

## Sprint 0 验收

- [ ] 5/5 new test cases PASS
- [ ] 26/26 ctest 全量 PASS
- [ ] 2 个 examples `--mock` 跑通
- [ ] commit message 符合 `<type>(<scope>): <subject>` 格式
- [ ] OpenSpec 4 件套完整 (proposal + design + tasks + specs)
