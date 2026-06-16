# Design: Residual engine.h Decoupling (Stage 4 Task 19 残留)

> **变更类型**: 真实实现 — 本 design 描述新代码架构

## 架构合规性检查

| 约束 | 状态 | 备注 |
|------|------|------|
| 2 空格缩进 | ✅ | 沿用现有 |
| 中文注释优先 | ✅ | 全部新增注释中文 |
| C++20 + CMake 3.20+ | ✅ | 沿用现有 |
| nlohmann_json | ✅ | JSON 字段 |
| Anti-pattern 避免 | ✅ | 不删失败测试,提交前 ctest |
| 工厂模式 | ✅ | IProviderFactory/IToolRegistry |
| TraceRecord POD | ✅ | 头文件可被多处 include |

## 关键设计决策

### 决策 1: IProviderFactory 抽象

**问题**: `engine.h` 直接 include `common/llm/mock_provider.h`,导致 MockLLMProvider 实现细节泄漏到 core 层。

**方案**:
```cpp
// include/agenticdsl/contract/iprovider_factory.h
namespace agenticdsl::contract {
class IProviderFactory {
 public:
  virtual ~IProviderFactory() = default;
  virtual std::unique_ptr<ILLMProvider> create(const LLMConfig& config) = 0;
  virtual std::string factory_name() const = 0;
};
}  // namespace agenticdsl::contract
```

**实现**:
- `MockProviderFactory` (默认,生产 CI)
- `CloudProviderFactory` (OpenAI/Anthropic/DeepSeek)
- `LlamaProviderFactory` (本地 llama.cpp)

**DSLEngine 注入**: `DSLEngine(std::unique_ptr<IProviderFactory>)`,默认构造使用 `MockProviderFactory`。

**理由**: 工厂模式 + 抽象,允许运行时切换 provider,同时解除 engine.h 与具体实现的耦合。

### 决策 2: IToolRegistry 抽象

**问题**: `engine.h` 直接 include `common/tools/registry.h`,ToolRegistry 实现细节泄漏到 core 层。

**方案**:
```cpp
// include/agenticdsl/contract/itool_registry.h
namespace agenticdsl::contract {
class IToolRegistry {
 public:
  virtual ~IToolRegistry() = default;
  virtual ToolResult call_tool(const std::string& name, const nlohmann::json& args) = 0;
  virtual void register_tool(const std::string& name,
                              std::function<ToolResult(const nlohmann::json&)> fn) = 0;
  virtual bool has_tool(const std::string& name) const = 0;
};
}  // namespace agenticdsl::contract
```

**实现**:
- `ToolRegistry : public IToolRegistry` (现有类加 `override` 关键字)
- 方法签名不变,仅添加 `override`

**理由**: 最小化 API 变更,纯依赖反转。

### 决策 3: TraceRecord POD 上移

**问题**: `engine.h` 直接 include `modules/trace/trace_exporter.h`,因为 `TraceRecord` 结构体定义在 `trace_exporter.h` 中(混合了定义和实现)。

**方案**:
- **新增** `include/agenticdsl/types/trace_record.h`: 纯 POD 结构体,无实现
- **保留** `src/modules/trace/trace_exporter.cpp`: 实现仍在此
- `trace_exporter.h` 改为 include 新 POD 头文件

**理由**: POD 结构体可被多处 include 而无副作用,符合"types 在 include/,实现 src/" 模式。

## 测试设计

### 单元测试

1. `test_provider_factory.cpp` (新增):
   - `MockProviderFactory::create(LlmConfig)` 返回 MockLLMProvider
   - `CloudProviderFactory::create(...)` 返回 CloudLLMAdapter (需 Mock HTTP)
   - 多线程并发 `create()` (1000x) 无 data race
   - 工厂名称唯一性

2. `test_tool_registry_interface.cpp` (新增):
   - `IToolRegistry::call_tool(name, args)` 返回 ToolResult
   - 工具注册/查找
   - 错误处理 (未知工具)

3. `test_trace_record_pod.cpp` (新增):
   - TraceRecord 默认构造 + 字段赋值
   - JSON 序列化/反序列化
   - 头文件 include 测试 (验证可在 include/ 中使用)

### 集成测试

1. `test_engine_no_cross_module.cpp` (新增):
   - 验证 engine.h 0 跨模块 include (除 llm_types.h)
   - DSLEngine 构造 + 运行简单工作流
   - IProviderFactory 注入 + 切换 provider

### 回归测试

1. 全量 25/25 ctest (无回归)
2. TSan/ASan 干净 (新并发测试覆盖)

## 实施计划

### T1: IProviderFactory 抽象 (4 天)
- Day 1: 定义接口 + MockProviderFactory 实现
- Day 2: DSLEngine 注入 IProviderFactory
- Day 3: CloudProviderFactory + LlamaProviderFactory 实现
- Day 4: test_provider_factory.cpp + 验证

### T2: IToolRegistry 抽象 (3 天)
- Day 1: 定义接口 + ToolRegistry 加 `override`
- Day 2: NodeExecutor 改用 IToolRegistry
- Day 3: test_tool_registry_interface.cpp + 验证

### T3: TraceRecord 上移 (1 天)
- 0.5 天: 拆分 `trace_exporter.h` → `trace_record.h` (POD) + `trace_exporter.h` (实现)
- 0.5 天: 验证编译

### T4: 验证 + ADR 更新 (2 天)
- Day 1: 跑全量 25/25 + TSan + ASan
- Day 2: 更新 ADR-0019 §1.4 + 提交

## 风险与缓解

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| 工厂模式引入性能开销 | 中 | Provider 创建慢 | 工厂单例 + 缓存 |
| ToolRegistry 抽象破坏旧调用方 | 低 | 编译错误 | 方法签名不变 + `override` 关键字 |
| TraceRecord 上移触发 include 循环 | 低 | 编译错误 | POD 无依赖,放在 types/ |
| NodeExecutor 改 IProviderFactory 引入循环依赖 | 中 | 编译错误 | DSLEngine 注入而非 NodeExecutor 持有 |

## 引用

- `.omo/plans/project-organization.md` Stage 4 Task 19 残留
- `docs/adr/adr-0019-iinteraction-bus-mvp.md` §1.4
- `openspec/changes/2026-06-15-core-interface-inversion` (前置)
- `openspec/changes/2026-06-15-layered-context-implementation` (前置)
