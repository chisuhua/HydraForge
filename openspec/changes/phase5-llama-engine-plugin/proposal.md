# Proposal: Phase 5 Llama Engine Plugin (C14)

> **STATUS: ACTIVE** 🟡
> **关联 Oracle 决议**: Architecture Reflection 2026-07-05 (session `ses_0ce717ac4ffejvLa2We0gzbuds`)
> **关联 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §五
> **关联 handoff**: `docs/handoff/2026-07-05-week1-day1-day2-completion.md` §5.1-5.2
> **关联 ADR**: ADR-0021 (PDK 设计), ADR-0034 (Model Router plugin 范式)
> **前置依赖**: C13 (架构层 schema) — active in parallel
> **后续依赖**: C15 (BatchingQueue plugin)
> **最后更新**: 2026-07-05

## Why

Phase 5 自举服务化需要将推理后端的特定实现（engine/model/batching）从架构层下沉到 PDK plugin 层，遵循 ADR-0034 Model Router 已验证的"核心保留契约 + 算法 plugin 化"范式。

handoff §5.1-5.3 推荐的 B2 实施路径将所有 6 个推理子图（engine/model/prefix_cache/kv_cache/decoding/batching）硬编码注册到 `src/common/tools/registry.cpp`，导致：

1. **固化僵硬** — 添加新后端（vLLM/SGLang/cloud）必须修改核心代码
2. **依赖污染** — llama.cpp 是核心依赖，vLLM Python runtime / cloud SDK 不应进核心
3. **发布周期耦合** — 引擎优化策略演进必须与核心同步发布
4. **测试割裂** — 架构层测试通过不能保证 plugin 实现正确

**Oracle 架构反思结论**（2026-07-05 session `ses_0ce717ac4ffejvLa2We0gzbuds`）：

> Engine/model/batching 是**多后端并存** + **外部贡献预期** + **依赖隔离需求**三重命中 → PDK plugin 层
> 参考 ADR-0034 范式：C7 Phase 1-2 已 ship 3 个策略 .so（cost/quality/latency）+ ModelRegistry 证明可行

## What Changes

### 1. 创建 `pdk/llama_engine/` plugin 骨架

**目录结构**：
```
pdk/llama_engine/
├── CMakeLists.txt              # 参考 pdk/model_router/CMakeLists.txt
├── README.md                   # Plugin usage + contribution guide
├── include/
│   └── hydraforge/pdk/
│       └── llama_engine.h      # public API (Plugin exports)
└── src/
    ├── llama_engine.cpp        # B2.1 engine: load/generate/stream (含采样器 clamp 逻辑内联)
    ├── llama_model.cpp         # B2.2 model: load/unload/list/switch
    └── pdk_plugin_info.cpp     # Plugin metadata (ABI version)
```

**编译产出**：`libhydraforge_llama_engine.so`

### 2. B2.1 engine plugin 实现

**注册工具**（参照 `pdk/model_router/model_registry.cpp` 的 `pdk_register_tools` 模式）：

```cpp
// pdk/llama_engine/src/llama_engine.cpp
extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
    // inference/engine/init - 初始化引擎 (从 llm_config.json 读取)
    registry.register_tool_function(
        "inference/engine/init",
        meta_readonly,
        [](auto args) -> json {
            return llama_engine_init(args);  // 调用 llama.cpp
        });

    // inference/engine/generate - 同步生成
    registry.register_tool_function(
        "inference/engine/generate",
        meta_execute,
        [](auto args) -> json {
            return llama_engine_generate(args);
        });

    // inference/engine/stream - 流式生成 (C12 YIELD 集成点)
    registry.register_tool_function(
        "inference/engine/stream",
        meta_execute,
        [](auto args) -> json {
            return llama_engine_stream(args);
        });

    // inference/engine/status - 引擎状态查询
    registry.register_tool_function(
        "inference/engine/status",
        meta_readonly,
        [](auto args) -> json {
            return {{"loaded", true}, {"backend", "llama.cpp"}, {"version", "b####"}};
        });
}
```

**参考**：`pdk/model_router/cost_strategy/cost_router.cpp` 的 `pdk_register_tools` + `pdk_plugin_info` 模式（同样支持路径分隔符 `inference/engine/init`）

### 3. B2.2 model plugin 实现

```cpp
// pdk/llama_engine/src/llama_model.cpp
extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
    // inference/model/load - 加载模型
    registry.register_tool_function(
        "inference/model/load",
        meta_state_modify,
        [](auto args) -> json {
            std::string path = args.at("path");
            // 调用 llama.cpp load_model
            return {{"status", "loaded"}, {"model_id", path}};
        });

    // inference/model/unload - 卸载模型
    registry.register_tool_function(
        "inference/model/unload",
        meta_state_modify,
        [](auto args) -> json {
            // 调用 llama.cpp free_model
            return {{"status", "unloaded"}};
        });

    // inference/model/list - 列出已加载模型
    registry.register_tool_function(
        "inference/model/list",
        meta_readonly,
        [](auto args) -> json {
            return json::array({/* loaded models */});
        });

    // inference/model/switch - 切换活跃模型
    registry.register_tool_function(
        "inference/model/switch",
        meta_state_modify,
        [](auto args) -> json {
            std::string name = args.at("name");
            return {{"status", "switched"}, {"active", name}};
        });
}
```

### 4. 已删除（按 Adversarial Review D1 决策 — SamplerStrategy 接口推迟到出现第二个推理后端时再提取）

**D1 决策应用细节**：
- 删除 `include/agenticdsl/pdk/sampler_strategy.h` PDK 接口
- 删除 `pdk/llama_engine/src/llama_sampler.cpp` reference impl
- 采样器 clamp 逻辑内联到 `inference/engine/generate` 工具实现内部：
  - `temperature = std::clamp(temperature, 0.0f, 2.0f)`
- `decoding.configure` 工具支持以下 5 种 sampler 字符串选择（与 llama.cpp API 对齐）：
  | 字符串 | 采样器类型 |
  |--------|----------|
  | `greedy` | 贪心采样（argmax） |
  | `temperature` | 温度采样（带温度的 softmax） |
  | `mirostat_v1` | Mirostat v1（自适应 perplexity 控制） |
  | `mirostat_v2` | Mirostat v2（改进版自适应控制） |
  | `typical_p` | Typical-P 采样（信息论典型性阈值） |
- 传入其他值时记录 `unsupported_warning` 并 fallback 到 `greedy`
- 参数 clamp 范围：`temperature ∈ [0.0, 2.0]`, `top_p ∈ [0.0, 1.0]`

### 5. lib/inference/engine.md + model.md 从 PLACEHOLDER 升级

**当前**：
```yaml
# lib/inference/engine.md
> ⚠️ PLACEHOLDER — 创建于 2026-07-04 (Week 1 Day 1 drift 修复)
> **状态**: 占位 (结构同 session.md 模板,等待 B2 Week 2 实施填充)
```

**目标**（与 B2.1/B2.2 plugin 工具对齐）：
```yaml
# lib/inference/engine.md
signature: "(model_path: string, n_ctx: int, n_gpu_layers: int) -> (status: string, engine_id: string)"

## /init
  type: tool_call
  tool: inference/engine/init  # 引用 plugin 工具
  arguments:
    model_path: "{{ inputs.model_path }}"
    n_ctx: "{{ inputs.n_ctx | default(2048) }}"
    n_gpu_layers: "{{ inputs.n_gpu_layers | default(0) }}"
  output_keys: ["status", "engine_id"]
```

类似地 model.md 升级。

### 6. DSLEngine 显式 load_plugin() API（D5 Option B）

**当前**：DSLEngine 启动时不自动加载任何 engine plugin

**目标**（按 D5 决策）：
- DSLEngine 构造**不**默认加载 plugin（删除原 fallback 设计）
- 提供公开 API `DSLEngine::load_plugin(const std::string& name)`，由调用方显式加载
- 加载失败时输出 WARN log，**不抛异常**，**不 fallback**（CI 行为可控 + plugin 生命周期清晰）
- 与 C16 D4 同步（`LlamaAdapter` deprecate 路径一致）

**实施位置**：`src/core/engine.h/.cpp` 暴露 `load_plugin()` 公开方法

```cpp
// src/core/engine.h
class DSLEngine {
public:
    // D5 (C14, decisions-2026-07-07.md Option B): 显式加载 PDK plugin
    // 返回 true 表示加载成功; false 表示 .so 不存在或加载失败
    // 使用示例: engine->load_plugin("pdk/llama_engine");
    bool load_plugin(const std::string& plugin_name);
private:
    std::unique_ptr<hydraforge::PluginLoader> plugin_loader_;
};

// src/core/engine.cpp
bool DSLEngine::load_plugin(const std::string& plugin_name) {
    if (!plugin_loader_) {
        plugin_loader_ = std::make_unique<hydraforge::PluginLoader>();
    }
    try {
        return plugin_loader_->load_so(plugin_name, get_tool_registry());
    } catch (const std::exception& e) {
        LOG_WARN("Failed to load plugin '" << plugin_name << "': " << e.what());
        return false;
    }
}
```

**BREAKING 变更**：DSLEngine 构造不再自动加载 plugin；调用方需显式 `engine.load_plugin("pdk/llama_engine")`。所有现有测试/示例需迁移。

### 7. 测试

**新增文件**：`tests/test_llama_engine_plugin.cpp`

**测试用例**（≥7 case，参考 `test_model_router_policy.cpp` 8 case 模式）：
1. plugin dlopen 成功
2. pdk_plugin_info ABI version 匹配
3. `inference/engine/init` 工具注册成功
4. `inference/engine/generate` 同步生成返回结果
5. `inference/engine/stream` 与 C12 YIELD 集成
6. `inference/model/load` + `unload` 生命周期
7. `inference/model/switch` 切换活跃模型

### 8. 文档同步

- `pdk/llama_engine/README.md`（新建）— Plugin usage + contribution
- `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §五 + §十一 — 标记 C14 ship
- `docs/adr/adr-0021-pdk-design.md` — 追加 engine plugin 范式参考
- `AGENTS.md` CODE MAP — `pdk/llama_engine/` 加入

## What Does NOT Change

- **架构层 schema (C13)** — prefix_cache/kv_cache/decoding/cloud_engine 保持架构层
- **现有 src/common/llm/LlamaAdapter** — 保留作为 reference + fallback（不删除）
- **C12 YIELD/STREAM 实现** — 不修改，plugin 通过 IGenerationStream 集成
- **pdk/model_router/ (C7)** — 已 ship，不修改
- **lib/inference/ session.md** — 已 ship，不修改

## Capabilities

### ADDED Requirements

- `llama-engine-plugin-builds`: `pdk/llama_engine/CMakeLists.txt` MUST 成功编译产出 `libhydraforge_llama_engine.so`
- `llama-engine-plugin-registers-tools`: `pdk_register_tools` MUST 注册 ≥6 个工具：inference/engine/{init, generate, stream, status} + inference/model/{load, unload, list, switch}
- `lib-inference-engine-md-upgraded`: `lib/inference/engine.md` MUST 从 PLACEHOLDER 升级为引用 inference/engine/init 工具的真实 schema
- `lib-inference-model-md-upgraded`: `lib/inference/model.md` MUST 从 PLACEHOLDER 升级为引用 inference/model/load 工具的真实 schema
- `dslengine-explicit-load-plugin`: DSLEngine MUST 暴露 `bool load_plugin(const std::string& name)` 公开 API；构造时**不**自动加载 plugin；`load_plugin()` 失败 MUST 输出 WARN log 而**不抛异常**，**不** fallback 到内嵌实现
- `tests-test-llama-engine-plugin-shipped`: `tests/test_llama_engine_plugin.cpp` MUST 包含 ≥7 test cases，覆盖 plugin dlopen / ABI 匹配 / 6 工具 / generate/model lifecycle

## Impact

**新增文件**:
- `pdk/llama_engine/CMakeLists.txt` (~40 行)
- `pdk/llama_engine/README.md` (~80 行)
- `pdk/llama_engine/include/hydraforge/pdk/llama_engine.h` (~30 行)
- `pdk/llama_engine/src/llama_engine.cpp` (~150 行)
- `pdk/llama_engine/src/llama_model.cpp` (~100 行)
- `pdk/llama_engine/src/pdk_plugin_info.cpp` (~20 行)
- `tests/test_llama_engine_plugin.cpp` (~250 行)

**修改文件**:
- `pdk/CMakeLists.txt` (+5 行: add_subdirectory(llama_engine))
- `lib/inference/engine.md` (占位 → 真实 schema)
- `lib/inference/model.md` (占位 → 真实 schema)
- `src/core/engine.cpp` (+30 行: 默认 plugin 注入 + fallback)
- `AGENTS.md` (+3 行: CODE MAP)
- `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` (~15 行更新)

**总净增**: ~750 行新文件 + ~60 行修改

**API 兼容性**: **BREAKING change**（D5 决策）
- DSLEngine 构造时**不**自动加载 plugin（删除原默认注入 + fallback 设计）
- 调用方必须显式调用 `engine.load_plugin("pdk/llama_engine")`
- lib/inference/engine.md + model.md 从占位升级为真实 schema，向后兼容（DSL 工具名不变）
- 所有现有测试/示例需迁移：添加 `engine.load_plugin("pdk/llama_engine")` 调用

**估时**: 2-3 天（原 1-1.5 天，因 D5 决策新增 4 个 C13 架构工具注册 + D5 显式 load_plugin() API 改造）
- plugin 骨架 + CMakeLists: 2h
- B2.1 engine 实现: 4h
- B2.2 model 实现: 2h
- C13 架构工具注册（4 工具）: 2h
- lib/inference 升级: 1h
- D5 显式 `load_plugin()` API: 1h
- 测试 + 文档 + 示例迁移: 3h

## Non-goals

- **不实现 vLLM/SGLang/cloud plugin** — 仅 pdk/llama_engine/，第三方 plugin 留 C15/PR 模板阶段
- **不实现真实 batching** — pdk/llama_engine/ 仅声明"不支持 batching"，BatchingQueue 接口及贡献流程推迟到第二个推理后端出现时（按 Adversarial Review D2 决策）
- **不实现 prefix_cache/kv_cache 内部逻辑** — engine plugin 内部使用 llama.cpp 内置，架构层仅 schema
- **不修改 LlamaAdapter 现有实现** — 保留作为 reference implementation（不再作为 fallback，D5 决策后 plugin 缺失仅 WARN log）
- **不修改 C12 YIELD/STREAM** — 通过 IGenerationStream 集成即可
- **不修改 pdk/model_router/** — 已 ship C7

## 关联 change

- **前置**: C12 ✅ + C13 🟡 (并行 active)
- **后续**: C15 (BatchingQueue plugin)
- **未来**: 第三方 cloud engine plugin (pdk/cloud_engine/) — Stage 2+

## 验证标准

- [ ] ctest baseline 64/64 + 新增 test_llama_engine_plugin 7 cases = **71/71 PASS** 零回归
- [ ] `cmake --build build -j$(nproc)` 100% 编译通过
- [ ] `cmake --build build/pdk -j$(nproc)` 100% plugin 编译通过
- [ ] `libhydraforge_llama_engine.so` 成功 dlopen
- [ ] `pdk_plugin_info.abi_version == CURRENT_ABI_VERSION` (ABI 兼容性)
- [ ] `python3 tools/adr_lint.py` exit 0
- [ ] `python3 tools/docs_drift_audit.py` 0 DRIFT
- [ ] `openspec validate phase5-llama-engine-plugin` exit 0
- [ ] ASan baseline ≥ 64/64 (新增 plugin 测试不引入新 leak)
- [ ] TSan baseline ≥ 63/64 (新增 plugin 测试不引入新 race)

## Oracle 决策依据

**会话**: `ses_0ce717ac4ffejvLa2We0gzbuds` (2026-07-05)
**关键判据命中**:
1. ✅ 实现多样性: llama.cpp / vLLM / SGLang / cloud 多后端并存
2. ✅ 外部贡献预期: 第三方 plugin 团队 (vLLM/SGLang/cloud providers)
3. ✅ 依赖隔离: vLLM Python runtime / cloud SDK 不应进核心
4. ✅ ABI 契约稳定: ILLMProvider 已存在且稳定
5. ✅ 演进速度差异: engine 算法优化频繁，独立发布周期

**5 条判据全部命中 → 强 plugin 候选**

**范式参考**: ADR-0034 Model Router plugin（C7 Phase 1-2 ship）已证明"核心保留契约 + 算法 plugin 化"可行