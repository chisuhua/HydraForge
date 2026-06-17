# Design: Residual engine.h Decoupling (Stage 4 Task 19 残留)

> **变更类型**: 真实实现 — 本 design 描述新代码架构
> **2026-06-17 v2 修订**: Oracle 审查后修复 6 P0 + 6 P1. 关键: IToolRegistry 必须扩展至 ~7 虚函数覆盖 ToolRegistry 公共 API; LLMProviderFactory 从零构建 (非复用); SecureToolRegistry API 兼容性改造 (选项 A); 析构外置到 engine.cpp; ADR-0019 §1.4 状态更新延后到 T5.2.

## 架构合规性检查

| 约束 | 状态 | 备注 |
|------|------|------|
| 2 空格缩进 | ✅ | 沿用现有 |
| 中文注释优先 | ✅ | 全部新增注释中文 |
| C++20 + CMake 3.20+ | ✅ | 沿用现有 |
| nlohmann_json | ✅ | JSON 字段 |
| Anti-pattern 避免 | ✅ | 不删失败测试,提交前 ctest |
| 工厂模式 (facade) | ✅ | IProviderFactory → MockProviderFactory |
| IToolRegistry 8 虚函数 | ✅ | 覆盖 ToolRegistry 公共 API |
| TraceRecord data-only | ✅ | 头文件可被多处 include |
| PIMPL-lite 模式 | ✅ | 镜像 budget_controller_ 模式 |
| 析构外置到 .cpp | ✅ | PIMPL 必须 |

## 关键设计决策

### 决策 1: IProviderFactory 作为 contract 层抽象 (LLMProviderFactory 从零构建)

**问题**: `engine.h` 直接 include `common/llm/mock_provider.h`,导致 MockLLMProvider 实现细节泄漏到 core 层。

**2026-06-17 v2 关键认知** (Oracle 审查发现): **ADR-0005 §3 `LLMProviderFactory` + 3 `ProviderCreator` 仅是 markdown 设计草图, 编译代码不存在**. `src/common/llm/` 实际仅有 `mock_provider.h` / `llama_adapter_provider.h` / `cloud_adapter.h` / `http_adapter.h` / `sse_stream.h`. 因此本 change 需从零构建.

**方案 (修订后)**:
```cpp
// include/agenticdsl/contract/iprovider_factory.h
namespace agenticdsl {  // 扁平, 与现有 contract 头一致
class ILLMProvider;  // 前向声明
struct LLMConfig;    // 前向声明: 实际定义在 src/common/llm/llm_config.h:28

class IProviderFactory {
 public:
  virtual ~IProviderFactory() = default;
  // 接受统一 per-call LLMConfig (实际 src/common/llm/llm_config.h:28)
  virtual std::unique_ptr<ILLMProvider> create(
      const LLMConfig& config) = 0;
};
}  // namespace agenticdsl
```

**实现 (从零构建)**:
- `LLMProviderFactory` (新建, `src/common/llm/llm_provider_factory.h/cpp`):
  - 单一 `create(LLMConfig)` 方法, 根据 `config.provider` 字段路由 (单一 backend_name)
  - 注册 `MockProviderFactory` (默认)
  - **不实现** CloudProviderFactory / LlamaProviderFactory (YAGNI, 后续 OpenSpec change)
- `MockProviderFactory` (新建, `src/common/llm/mock_provider_factory.h/cpp`):
  - 包装现有 `MockLLMProvider` (不改 MockLLMProvider 本身)
  - `create(LlmConfig)` 返回 `std::make_unique<MockLLMProvider>()` (使用 LLMConfig 默认值)

**DSLEngine 注入**:
```cpp
// src/core/engine.h (修订)
class DSLEngine {
 public:
  // 默认构造: 使用 MockProviderFactory
  DSLEngine(std::vector<ParsedGraph> initial_graphs,
            std::unique_ptr<IProviderFactory> factory = nullptr);
  void set_provider_factory(std::unique_ptr<IProviderFactory> factory);
 private:
  std::unique_ptr<IProviderFactory> provider_factory_;  // 默认 MockProviderFactory
  std::unique_ptr<ILLMProvider> llm_provider_;          // 来自 provider_factory_->create(config)
  std::unique_ptr<IToolRegistry> tool_registry_;        // PIMPL-lite, 默认 ToolRegistry
  // ...
};

// src/core/engine.cpp
DSLEngine::DSLEngine(...)
    : tool_registry_(std::make_unique<ToolRegistry>()),
      provider_factory_(factory ? std::move(factory)
                                : std::make_unique<MockProviderFactory>()),
      ... {}

// 析构外置 (PIMPL 必须)
DSLEngine::~DSLEngine() = default;
```

**理由**:
- **避免虚假复用**: 诚实承认 LLMProviderFactory 不存在, 从零构建
- **范围控制**: 仅 MockProviderFactory (生产 CI 用), Cloud/Llama 留待后续
- **依赖方向**: contract → types (LLMConfig 前向声明) → 不引入 common/

### 决策 1.5: IProviderFactory 与 ADR-0005 §3 关系

**关系**: 后续实施 LLMProviderFactory 时, 实现细节参考 ADR-0005 §3 设计草图 (但需扩展为单一 create(LLMConfig) 接口, 而不是 create(backend_name, BackendConfig)). CloudProviderFactory / LlamaProviderFactory 不在本 change 实现 (避免范围蔓延 + cloud_adapter.h 拆分决策待后续).

**LLMConfig 类型**: 实际编译的 `LLMConfig` 是 `src/common/llm/llm_config.h:28` 的统一 per-call struct (provider, api_url, model, max_tokens, temperature 等). **不是** ADR-0005 §3 描述的 `LLMConfig` (default_backend + backends map, 那是 ADR 草稿). MockProviderFactory 接受统一 struct, 内部使用默认值.

### 决策 2: IToolRegistry 扩展至 8 虚函数 (镜像 ToolRegistry 公共 API)

**问题**: `engine.h` 直接 include `common/tools/registry.h`,ToolRegistry 实现细节泄漏到 core 层。

**2026-06-17 v2 关键认知** (Oracle 审查发现): 原方案 `IToolRegistry` 仅 2 虚函数 (call_tool + has_tool), **严重过窄**. `src/common/tools/registry.h:36-53` 实际暴露 8 个公共方法:
- `call_tool` (line 37)
- `has_tool` (line 36)
- `list_tools` (line 38)
- `register_llm_tool` (line 41)
- `is_llm_tool` (line 42)
- `get_llm_params` (line 43)
- `call_llm_tool` (line 44)
- `set_cost_callback` (line 49, inlined template)

`engine.cpp:114` 调用 `set_cost_callback`, `engine.cpp:191-192` 调用 `register_llm_tool`, `node_executor.cpp:135` 调用 `call_llm_tool` — 这些都需通过 IToolRegistry 虚函数暴露, 否则 PIMPL-lite 会导致编译失败.

**方案 (修订后)**:
```cpp
// include/agenticdsl/contract/itool_registry.h
namespace agenticdsl {
class IToolRegistry {
 public:
  virtual ~IToolRegistry() = default;
  // 基础查询
  virtual bool has_tool(const std::string& name) const = 0;
  virtual std::vector<std::string> list_tools() const = 0;
  // 函数工具调用 (镜像 ADR-0023 §C.3)
  virtual nlohmann::json call_tool(
      const std::string& name,
      const std::unordered_map<std::string, std::string>& args) = 0;
  // LLM 工具管理 (镜像 registry.h:41-44)
  virtual void register_llm_tool(
      std::string name, std::unique_ptr<ILLMTool> tool,
      const LLMParams& default_params = {}) = 0;
  virtual bool is_llm_tool(const std::string& name) const = 0;
  virtual const LLMParams& get_llm_params(const std::string& name) const = 0;
  virtual nlohmann::json call_llm_tool(
      const std::string& name, const std::string& prompt,
      const LLMParams& params = {}) = 0;
  // 成本回调 (镜像 registry.h:49)
  using CostCallback = std::function<void(int tokens, const std::string& model)>;
  virtual void set_cost_callback(CostCallback cb) = 0;
  // 故意省略 register_tool: 实际为模板成员函数 (registry.h:31-34),
  // C++ 禁止模板 virtual
};
}  // namespace agenticdsl
```

**实现**:
- `ToolRegistry : public IToolRegistry` (8 override 关键字, 保持模板 `register_tool`)
- `SecureToolRegistry : public IToolRegistry` (选项 A, 见决策 2.1)

**理由**:
- **镜像 ToolRegistry 公共 API**: 避免 PIMPL-lite 编译失败
- **保持 ADR-0023 §C.3**: call_tool 仍返回 nlohmann::json
- **避免 C++ 错误**: 省略 register_tool 虚函数 (模板不能 virtual)
- **PIMPL 装饰**: SecureToolRegistry 多继承实现

### 决策 2.1: SecureToolRegistry API 兼容性改造 (选项 A)

**问题** (Oracle 审查发现): 当前 `SecureToolRegistry` (在 `include/agenticdsl/tools/secure_tool_registry.h:44`, **不是** `src/common/tools/`) 不继承任何类, 持有 `ToolRegistry* registry_ref_` (指针, 不是值), 暴露 `call_direct` / `call_passthrough` 返回 `Result{bool allowed, nlohmann::json payload, SecurityError error}` 结构. 与 IToolRegistry 的 `call_tool` / `has_tool` API 不兼容.

**方案 (选项 A — 本 change 采用)**:
```cpp
// include/agenticdsl/tools/secure_tool_registry.h (修订)
class SecureToolRegistry : public IToolRegistry {
 public:
  // 保留原有 call_direct / call_passthrough (ADR-0004 兼容)
  Result call_direct(const std::string& tool_name,
                     const std::unordered_map<std::string, std::string>& args);
  Result call_passthrough(const std::string& tool_name,
                          const std::unordered_map<std::string, std::string>& args);

  // IToolRegistry 接口实现 (新)
  bool has_tool(const std::string& name) const override;
  std::vector<std::string> list_tools() const override;
  nlohmann::json call_tool(
      const std::string& name,
      const std::unordered_map<std::string, std::string>& args) override {
    auto r = call_direct(name, args);
    return r.allowed ? r.payload : nlohmann::json{};  // 安全检查失败返回空 JSON
  }
  void register_llm_tool(...) override;  // 委托 base_registry_
  bool is_llm_tool(const std::string& name) const override;
  const LLMParams& get_llm_params(const std::string& name) const override;
  nlohmann::json call_llm_tool(...) override;
  void set_cost_callback(CostCallback cb) override;
 private:
  ToolRegistry base_registry_;  // 由值成员改为持有 (选项 A)
};
```

**理由**:
- **多继承装饰**: PIMPL 风格, 委托给 base_registry_
- **保留 ADR-0004 兼容**: call_direct / call_passthrough 仍可用
- **IToolRegistry 集成**: SecureToolRegistry 可注入 DSLEngine::tool_registry_

**Trade-off**: 增加 ~1.5 天工作量 (选项 B: 不实现 IToolRegistry, DSLEngine 仅在 SecureToolRegistry 注入时持具体类型, 但这会引入新分支路径, 增加复杂度).

### 决策 2.2: get_tool_registry() 返回 IToolRegistry& (Partial Breaking Change)

**问题** (Oracle 审查发现): `SimpleCognitiveOrchestrator` ctor 接受 `ToolRegistry*` (具体类型). 5 个 `test_simple_orchestrator.cpp` 调用点 + 1 个 `examples/slice_01_tool_call/main.cpp:77` 调用点 传 `&engine->get_tool_registry()`. 若返回 `IToolRegistry&`, 编译失败.

**方案 (修订后)**:
```cpp
// src/core/engine.h
class DSLEngine {
 public:
  IToolRegistry& get_tool_registry() { return *tool_registry_; }
  const IToolRegistry& get_tool_registry() const { return *tool_registry_; }
  // 兼容性访问 (用于 SimpleCognitiveOrchestrator 等需要具体类型的调用方)
  ToolRegistry& get_tool_registry_concrete() {
    return *static_cast<ToolRegistry*>(tool_registry_.get());
  }
  const ToolRegistry& get_tool_registry_concrete() const {
    return *static_cast<const ToolRegistry*>(tool_registry_.get());
  }
  // ...
};

// 6 个调用点迁移
// tests/test_simple_orchestrator.cpp:53,79,102,126,150 → 改用 get_tool_registry_concrete()
// examples/slice_01_tool_call/main.cpp:77 → 改用 get_tool_registry_concrete()
```

**Breaking Change 评估**: ⚠️ **部分** (从 ❌ 无 改为 ⚠️ 部分):
- 6 个调用点已列迁移计划 (T2.4)
- 缓解: `get_tool_registry_concrete()` 显式访问
- Trade-off: `static_cast` 安全 (因为 DSLEngine 默认构造 ToolRegistry, 多态 IToolRegistry 必有 ToolRegistry 子对象)

### 决策 2.3: IToolRegistry 与 ADR-0020 ILLMProvider 注入模式协调

**关系**: 互补 (不是重复)
- ADR-0020 §2.2 line 144: `ILLMProvider* llm_provider_` — Per-Worker 持有实例
- 本 change `IProviderFactory` — Per-Engine 持有工厂
- 工厂 `create()` 后实例通过 ILLMProvider* 注入

**生命周期协调**:
- Per-Worker `std::unique_ptr<DSLEngine>` 持有 Per-Engine `std::unique_ptr<IProviderFactory>` (singleton-per-engine)
- IProviderFactory 内部 (LLMProviderFactory): 多线程 `create()` 安全 (内部 mutex)
- ADR-0020 §4.1 mutex+queue 模式: 仍由 WorkerPool 控制, **不与 factory 冲突**

### 决策 3: TraceRecord POD 上移 (data-only, 非严格 POD)

**问题**: `engine.h` 直接 include `modules/trace/trace_exporter.h`,因为 `TraceRecord` 结构体定义在 `trace_exporter.h:16` 中(混合了定义和实现)。

**2026-06-17 v2 关键认知**: TraceRecord 严格意义**非 POD** (含 `nlohmann::json` 堆分配, `std::optional<NodePath>`). 但在本 context "POD" 指 "data-only struct, 无方法". 提案措辞需修正.

**方案**:
- **新增** `include/agenticdsl/types/trace_record.h`:
  ```cpp
  namespace agenticdsl {
  // TraceRecord data-only struct (严格意义非 POD, 含 nlohmann::json 堆分配)
  struct TraceRecord {
    // 字段从 src/modules/trace/trace_exporter.h:16 迁移
    // 依赖: NodePath (core/types/node.h), ExecutionBudget (core/types/budget.h), nlohmann::json
    std::string node_id;
    NodePath node_path;  // core/types/node.h
    std::optional<NodePath> parent_path;
    nlohmann::json context_delta;
    nlohmann::json budget_snapshot;
    // ... (其他字段)
  };
  }  // namespace agenticdsl
  ```
- **保留** `src/modules/trace/trace_exporter.cpp`: 实现不变
- `trace_exporter.h` 改为 include 新头文件
- **同步更新** `docs/adr/adr-0033-session-hierarchy.md` §2: 引用 `agenticdsl/types/trace_record.h`

**理由**:
- `TraceRecord` 字段依赖不深 (NodePath + ExecutionBudget 都是 core/types/ POD-like), 可上移到 include/agenticdsl/types/
- ADR-0033 引用路径同步更新
- 严格意义上"POD"措辞应改为"data-only struct"

### 决策 4: engine.h PIMPL-lite + 析构外置 (强制)

**问题**: 原提案 tasks.md:88-89 任务 "移除 `#include "common/tools/registry.h"`" 与 `tool_registry_` 值成员 (engine.h:112) 矛盾 — 值成员需要完整类型才能编译. 必须 PIMPL-lite.

**方案 (修订后 — 析构必须外置)**:
```cpp
// src/core/engine.h (修订)
class DSLEngine {
 public:
  // ... 公开 API
  IToolRegistry& get_tool_registry() { return *tool_registry_; }
  ToolRegistry& get_tool_registry_concrete();
  const ToolRegistry& get_tool_registry_concrete() const;

  template <typename Func>
  void register_tool(std::string_view name, Func&& func) {
    // 委托给 concrete (通过 get_tool_registry_concrete())
    get_tool_registry_concrete().register_tool(std::string(name), std::forward<Func>(func));
  }

  ~DSLEngine();  // 声明, 但不定义 (在 .cpp 中定义)

 private:
  std::vector<ParsedGraph> full_graphs_;
  std::unique_ptr<IToolRegistry> tool_registry_;        // PIMPL-lite
  std::unique_ptr<IProviderFactory> provider_factory_;  // Per-engine
  std::unique_ptr<ILLMProvider> llm_provider_;
  std::vector<TraceRecord> last_traces_;
  std::unique_ptr<BudgetController> budget_controller_;  // 现有 PIMPL-lite
  std::shared_ptr<IInteractionBus> bus_;
};

// src/core/engine.cpp (析构外置 — 关键!)
DSLEngine::~DSLEngine() = default;  // 完整类型下 default 构造 + 销毁成员
```

**为什么析构必须外置**: 
- `tool_registry_` 是 `unique_ptr<IToolRegistry>`, 析构时需 IToolRegistry 完整类型
- 若析构 `= default` 在 .h (inline), 每个 TU 都需要 IToolRegistry 完整类型 → 反向引入 common/tools/registry.h, 违背 PIMPL
- 必须 `~DSLEngine()` 在 .cpp 中定义, .cpp 中可看到完整类型

**API 兼容性**:
- `get_tool_registry() : IToolRegistry&` (从 `ToolRegistry&`)
- `get_tool_registry_concrete() : ToolRegistry&` (新, 兼容 SimpleCognitiveOrchestrator)
- `register_tool<>` template 通过 `get_tool_registry_concrete()` 委托 (避免 dynamic_cast)

### 决策 5: grep 验收命令 (修订)

**原提案**: `grep -c '#include "modules/\|#include "common/' src/core/engine.h` (期望 = 1)

**修订**:
```bash
# 退出标准: 仅保留 llm_types.h (types 头文件例外)
grep -c '#include "modules/\|#include "common/' src/core/engine.h
# 期望输出: 1 (line 37: #include "common/llm/llm_types.h")

# 注: agenticdsl/contract/*.h 不被统计 (路径不匹配 grep 模式)
# Sprint 1b 新增的 iinteraction_bus.h 不影响退出标准
```

**理由**: 命令已正确, 仅在 success criteria 文档化说明.

## 测试设计

### 单元测试

1. `test_provider_factory.cpp` (新建, ≥ 4 test cases):
   - `MockProviderFactory::create(LlmConfig)` 返回 `MockLLMProvider`
   - 多线程并发 `create()` (1000x) 无 data race
   - DSLEngine 注入 `MockProviderFactory` 默认
   - `LLMProviderFactory` 路由逻辑 (mock 验证 backend_name 解析)

2. `test_tool_registry_interface.cpp` (新建, ≥ 5 test cases):
   - `IToolRegistry::call_tool(name, args)` 返回 `nlohmann::json` (镜像 ADR-0023 §C.3)
   - `call_tool` 参数 `unordered_map<string,string>` 类型校验
   - `has_tool` 虚函数 override 正确性
   - `SecureToolRegistry : public IToolRegistry` 多继承 + 安全检查
   - `IToolRegistry::set_cost_callback` 通过虚函数正确触发

3. `test_trace_record_pod.cpp` (新建, ≥ 3 test cases):
   - TraceRecord 默认构造 + 字段赋值
   - JSON 序列化/反序列化
   - 头文件 include 测试 (验证可在 include/ 中使用)

### 集成测试

1. `test_engine_no_cross_module.cpp` (新建, ≥ 2 test cases):
   - 验证 `engine.h` 跨模块 include grep 退出 = 1 (CI 自动检查)
   - DSLEngine 构造 + 运行简单工作流 + IProviderFactory 注入 + 切换 provider

### 回归测试

1. 全量 27+10 ctest (无回归) — baseline 27 (verified `find tests -name 'test_*.cpp' | wc -l` = 27)
2. TSan/ASan 干净 (新并发测试覆盖)

## 实施计划 (5 周, 25 工作日)

### T1: LLMProviderFactory + MockProviderFactory 从零构建 (5-7 天)
- Day 1: 新建 `include/agenticdsl/contract/iprovider_factory.h` + `src/common/llm/llm_provider_factory.h/cpp`
- Day 2-3: `MockProviderFactory` 实现 + DSLEngine 注入
- Day 4: test_provider_factory.cpp (4 case)
- Day 5-7: 验证 + TSan 并发测试

### T2: IToolRegistry 8 虚函数 + SecureToolRegistry 改造 + 6 调用点迁移 (5 天)
- Day 1: 新建 `include/agenticdsl/contract/itool_registry.h` (8 虚函数)
- Day 2: `ToolRegistry : public IToolRegistry` + 8 override
- Day 3: `SecureToolRegistry : public IToolRegistry` (选项 A, 多继承)
- Day 4: **6 个 get_tool_registry() 调用点迁移** (test_simple_orchestrator.cpp:53,79,102,126,150 + slice_01_tool_call/main.cpp:77)
- Day 5: test_tool_registry_interface.cpp (5 case) + 验证

### T3: TraceRecord 上移 + ADR-0033 路径更新 (2 天)
- Day 1: 拆分 `src/modules/trace/trace_exporter.h:16` → `include/agenticdsl/types/trace_record.h`
- Day 2: 验证编译 + ADR-0033 §2 路径更新

### T4: engine.h 移除 3 include + PIMPL-lite tool_registry_ + 析构外置 (3 天)
- Day 1: `tool_registry_` PIMPL-lite + `provider_factory_` 注入
- Day 2: 析构外置到 engine.cpp + engine.h 移除 3 include + 加 3 抽象 include
- Day 3: 验证编译 + ctest

### T5: 验证 + 同步 (3 天)
- Day 1: 跑全量 27+10 ctest + TSan + ASan
- Day 2: 同步 7 个 ADR + 6 个 docs + R5 重分类 (含 ADR-0019 §1.4 状态更新)
- Day 3: openspec validate + commit

## 风险与缓解 (2026-06-17 v2)

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| SecureToolRegistry API 改造 (选项 A) 范围超估 | 中 | 增加 1.5d | 接受, ADR-0004 V1.1 amendment |
| get_tool_registry() 是 partial breaking change | 高 | 6 调用点迁移 | get_tool_registry_concrete() 显式访问 |
| LLMProviderFactory 从零构建 (5-7d vs 原 3d) | 中 | 估时偏差 | 诚实声明, 增量更新 |
| TraceRecord "POD" 措辞 | 低 | 文档歧义 | 改 "data-only struct" |
| 析构未外置到 engine.cpp | 中 | PIMPL 失败, engine.h 反向引入 registry.h | 强制 T4.2 验收包含析构外置 |
| ADR-0019 §1.4 时间悖论 | 高 | 文档虚假完成 | T5.2 才更新, 不是预先 |
| `LLMProviderFactory` ADR-0005 §3 拆分决策 (cloud_adapter.h) | 中 | 设计不一致 | 留待后续 OpenSpec change, 本 change 仅 Mock |
| R5 retrospective vs P1 active 矛盾 | 中 | OpenSpec workflow 混乱 | proposal + design + project-organization 三处声明 |

## 引用 (2026-06-17 v2)

- `.omo/plans/archive/2026-06-15-archived/project-organization.md` Stage 4 Task 19 残留 (R5 重分类为 P1 active)
- `docs/adr/adr-0019-iinteraction-bus-mvp.md` §1.4 (退出标准)
- `docs/adr/adr-0005-llm-backend-config-factory.md` §3 (LLMProviderFactory 设计草图, 待实现)
- `docs/adr/adr-0023-tool-result-standard.md` §C.3 (call_tool 返回 nlohmann::json)
- `docs/adr/adr-0033-session-hierarchy.md` §2 (TraceRecord 路径变更)
- `docs/adr/adr-0004-toolregistry-security.md` (SecureToolRegistry 多继承改造)
- `docs/adr/adr-0020-thread-model-isolation.md` §2.2 (ILLMProvider* Per-Worker 注入)
- `docs/adr/adr-0022-plugin-loading.md` §4.2 (未来 PDK 协调)
- `docs/adr/adr-0031-execution-policy.md` (Related, Phase 2)
- `openspec/changes/archive/2026-06-17-phase1-bus-integration/` (Sprint 1b)
- `openspec/changes/archive/2026-06-09-docs-code-alignment-fixes` (LayeredContext)
- `openspec/changes/archive/2026-06-16-phase1-toolresult-standardization` (P1-P4)
