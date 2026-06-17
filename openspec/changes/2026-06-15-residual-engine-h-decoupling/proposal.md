# Residual engine.h Decoupling (Stage 4 Task 19 残留)

> **变更类型**: 真实实现 change (新功能)
> **作者**: Sisyphus (atlas session)
> **创建日期**: 2026-06-15
> **追溯范围**: `.omo/plans/archive/2026-06-15-archived/project-organization.md` Stage 4 Task 19 残留 (2026-06-17: R5 从 retrospective 重分类为 P1 active)
> **amends**: `openspec/specs/tech-debt-cleanup/spec.md`
> **前置**: `openspec/changes/archive/2026-06-17-phase1-bus-integration/` (Sprint 1b 已 ship, 吸收了 3 个 deep modules/ 移除)
> **2026-06-17 v2 修订**: Oracle 审查后修复 6 P0 + 6 P1 问题. **关键认知**: `LLMProviderFactory` 不存在于编译代码 (仅在 ADR-0005 §3 设计草图中); SecureToolRegistry API 不匹配 IToolRegistry 接口; 实际 call_tool/has_tool 调用点与提案不一致.

## Why

`engine.h` 仍有 4 个跨模块 include(3 common/ + 1 modules/trace/),违反 ADR-0019 §1.4 "0 跨模块 include" 退出标准。本次 change 移除残留 4 个 include(保留 `llm_types.h` 因是 types 头文件),通过 3 个独立抽象:

1. `IProviderFactory` (新建) — 替代 `common/llm/mock_provider.h` 直接 include
2. `IToolRegistry` (新建, 含 ~7 个虚函数覆盖 ToolRegistry 公共 API) — 替代 `common/tools/registry.h` 直接 include
3. `TraceRecord` 上移 — 替代 `modules/trace/trace_exporter.h` 直接 include

> **2026-06-17 修订注**: Sprint 1b (commit `248d209`, 2026-06-17) 在 `engine.h` 吸收了 3 个 deep modules/ 的移除 (PIMPL-lite `BudgetController` + 2 个抽象接口)。本 change 实际处理剩余 4 个 (3 common/ + 1 modules/trace/)。

不解决此问题:① ADR-0019 §1.4 永久保持 🟡 部分解决 状态;② Phase 1 Sprint 2 (DomainWorkerPool) 的多线程测试可能受 MockLLMProvider 紧耦合影响;③ 后续 5/6 examples 修复需要先解耦 engine.h。

## What Changes

### 代码侧 (新代码)

- **新增** `include/agenticdsl/contract/iprovider_factory.h`:
  ```cpp
  namespace agenticdsl {  // 扁平, 与现有 contract 头一致
  class ILLMProvider;  // 前向声明
  struct LLMConfig;    // 前向声明

  class IProviderFactory {
   public:
    virtual ~IProviderFactory() = default;
    virtual std::unique_ptr<ILLMProvider> create(
        const LLMConfig& config) = 0;
  };
  }  // namespace agenticdsl
  ```

  > **2026-06-17 v2 修订说明**: 原方案声称"沿用 ADR-0005 §3 LLMProviderFactory", 但 **`LLMProviderFactory`/`ProviderCreator`/`OpenAICreator`/`AnthropicCreator`/`LlamaCreator` 仅存在于 ADR-0005 §3 设计草图 (md fenced code block), 不存在于编译代码**. `src/common/llm/` 实际仅有 `mock_provider.h` / `llama_adapter_provider.h` / `cloud_adapter.h` / `http_adapter.h` / `sse_stream.h`. 因此本 change **需从零构建** `LLMProviderFactory` + `MockProviderFactory` (单一 backend_name 路由), 估时从 3 天 → 5-7 天. CloudProviderFactory/LlamaProviderFactory 作为后续 OpenSpec change (避免 YAGNI).

- **新增** `include/agenticdsl/contract/itool_registry.h`:
  ```cpp
  namespace agenticdsl {
  // 镜像 src/common/tools/registry.h 公共方法 (除 register_tool 模板)
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
    // LLM 工具管理
    virtual void register_llm_tool(
        std::string name, std::unique_ptr<ILLMTool> tool,
        const LLMParams& default_params = {}) = 0;
    virtual bool is_llm_tool(const std::string& name) const = 0;
    virtual const LLMParams& get_llm_params(const std::string& name) const = 0;
    virtual nlohmann::json call_llm_tool(
        const std::string& name, const std::string& prompt,
        const LLMParams& params = {}) = 0;
    // 成本回调 (engine.cpp:114)
    using CostCallback = std::function<void(int tokens, const std::string& model)>;
    virtual void set_cost_callback(CostCallback cb) = 0;
    // 故意省略 register_tool: 实际为模板成员函数 (registry.h:31-34),
    // C++ 禁止模板 virtual
  };
  }  // namespace agenticdsl
  ```

  > **2026-06-17 v2 修订说明**: 原方案 `IToolRegistry` 仅 2 虚函数 (call_tool + has_tool), **严重过窄** — `engine.cpp:114` `set_cost_callback` / `engine.cpp:191-192` `register_llm_tool` / `node_executor.cpp:135` `call_llm_tool` (NodeExecutor 内部) 均无法通过 narrow interface 调用. 现扩展至 ~7 虚函数覆盖 ToolRegistry 公共 API. `SecureToolRegistry` (ADR-0004) 需 API 兼容性改造 (见决策 2.1).

- **新增** `include/agenticdsl/types/trace_record.h`:
  ```cpp
  // TraceRecord data-only struct 上移到 include/agenticdsl/types/
  // 字段依赖: NodePath (core/types/node.h) + ExecutionBudget (core/types/budget.h) + nlohmann::json
  // 注: 严格意义上非 POD (nlohmann::json 堆分配), 但 "POD" 在本 context 指 "data-only, no methods"
  namespace agenticdsl {
  struct TraceRecord {
    // ... (字段从 src/modules/trace/trace_exporter.h:16 迁移)
  };
  }  // namespace agenticdsl
  ```

- **修改** `src/core/engine.h`:
  - **删除** 3 行 include: `mock_provider.h` (line 38), `registry.h` (line 39), `trace_exporter.h` (line 47)
  - **保留** `llm_types.h` (line 37, types 头文件例外)
  - **新增** 3 行 include: `agenticdsl/contract/iprovider_factory.h`, `agenticdsl/contract/itool_registry.h`, `agenticdsl/types/trace_record.h`
  - **修改** `tool_registry_` 从 `ToolRegistry` (值成员 line 112) 改为 `std::unique_ptr<IToolRegistry>` (PIMPL-lite, 镜像 `budget_controller_` line 115)
  - **新增** `std::unique_ptr<IProviderFactory> provider_factory_` (默认 `MockProviderFactory`)
  - **修改** `get_tool_registry()` 返回 `IToolRegistry&` (从 `ToolRegistry&`)
  - **保留** `get_tool_registry_concrete() : ToolRegistry&` (显式访问, 兼容 `SimpleCognitiveOrchestrator` 等)
  - **修改** `register_llm_tool` 委托 `tool_registry_->register_llm_tool(...)` (通过 IToolRegistry 虚函数)

- **修改** `src/core/engine.cpp`:
  - 构造时 `tool_registry_ = std::make_unique<ToolRegistry>()` (默认) — 完整类型仅在 .cpp 可见
  - 构造时 `provider_factory_ = std::make_unique<MockProviderFactory>()` (默认) — 替代直接构造 `MockLLMProvider`
  - **析构移到 .cpp** (`= default` 在 .cpp 而非 .h, 否则违反 PIMPL)
  - `from_markdown` 改为 `provider_factory_->create(config)` 后注入 `llm_provider_`

- **修改** `src/modules/trace/trace_exporter.h`:
  - 改为 `#include "agenticdsl/types/trace_record.h"` (替代内联定义)
  - `TraceRecord` 定义从 `trace_exporter.h:16` 删除 (迁移到新头文件)

- **修改** `src/modules/trace/trace_exporter.cpp`: 实现不变
- **修改** `src/common/tools/registry.h`:
  - `class ToolRegistry : public IToolRegistry` (加 `override`)
  - 所有 7 个虚函数加 `override` 关键字
  - `register_tool` **保持模板成员函数** (不强行抽象)

- **修改** `include/agenticdsl/tools/secure_tool_registry.h` (注意: **路径是 include/agenticdsl/tools/, 不是 src/common/tools/**):
  - 需 API 兼容性改造 — 当前 SecureToolRegistry 暴露 `call_direct` / `call_passthrough` (返回 `Result{bool, json, SecurityError}`), 与 IToolRegistry 接口不直接兼容
  - 选项 A: 添加 IToolRegistry 多继承 + 重写 call_tool/has_tool (1.5 天改造)
  - 选项 B: SecureToolRegistry 不实现 IToolRegistry, DSLEngine 仅在 SecureToolRegistry 注入时持具体类型
  - **本 change 采用选项 A** (设计决策, ADR-0004 V1.1 amendment)

### 文档侧

- **更新** `docs/adr/adr-0019-iinteraction-bus-mvp.md` §1.4: 状态在 T5 执行时更新为 ✅ 已解决 (**不是现在, 避免时间悖论**)
- **更新** `docs/adr/adr-0005-llm-backend-config-factory.md` §3: 修正实现状态 (LLMProviderFactory 仅是设计草图, 由本 change 从零实现)
- **更新** `docs/adr/adr-0023-tool-result-standard.md` §C.3.1: IToolRegistry 扩展覆盖 7 虚函数
- **更新** `docs/adr/adr-0033-session-hierarchy.md`: §2 引用新 TraceRecord 位置
- **更新** `docs/adr/adr-0004-toolregistry-security.md`: §SecureToolRegistry 多继承改造
- **更新** `docs/adr/adr-0020-thread-model-isolation.md` §2.2.1: IProviderFactory Per-Worker 协调
- **更新** `docs/adr/adr-0031-execution-policy.md`: Related 注释
- **更新** `docs/roadmap-status.md` line 49: P1 标 3/4 完成
- **更新** `docs/phase1-roadmap.md` line 122: P1 状态更新
- **更新** `docs/SPRINT-1A-COMPLETION-REPORT.md` line 213: post-sprint 注释
- **更新** `AGENTS.md` line 17: 移除 budget_controller.h (已 PIMPL-lite), 引用本 change
- **更新** `.omo/plans/archive/2026-06-15-archived/project-organization.md` R5 章节: 重分类为 P1 active
- **更新** `.omo/boulder.json`: 标记 3/4 完成
- **更新** `docs/SPECS-ALIGNMENT.md`: 加变更追踪项
- **更新** `docs/implementation-roadmap.md`: 25/25 → 27/27

## Impact

- **Affected specs**: `openspec/specs/tech-debt-cleanup/spec.md` (amend 1 REQ)
- **Affected ADRs**: 7 个 (见上 文档侧)
- **Affected code**:
  - `src/core/engine.h` (改 include, 改 tool_registry_ 为 PIMPL-lite)
  - `src/core/engine.cpp` (IProviderFactory 注入, 析构外置)
  - `src/modules/trace/trace_exporter.h` (TraceRecord 拆分)
  - `src/common/tools/registry.h` (加 `override`)
  - `include/agenticdsl/tools/secure_tool_registry.h` (API 兼容性改造, 选项 A)
  - `src/common/llm/` (新建 llm_provider_factory.h/cpp + mock_provider_factory.h/cpp)
- **Affected tests**: 27+10+ (0 回归) — baseline 27, 新增 10+ 测试
- **Breaking change**: ⚠️ **部分** — `get_tool_registry()` 返回 `IToolRegistry&` (从 `ToolRegistry&`). 6 个调用点需迁移:
  - `tests/test_simple_orchestrator.cpp:53,79,102,126,150` (5 sites) → 改用 `get_tool_registry_concrete()`
  - `examples/slice_01_tool_call/main.cpp:77` → 改用 `get_tool_registry_concrete()`
  - 缓解: 提供 `get_tool_registry_concrete() : ToolRegistry&` 显式访问

## Success Criteria

- [ ] `grep -c '#include "modules/\|#include "common/' src/core/engine.h` = 1 (仅 llm_types.h)
- [ ] `tools/adr_lint.py docs/adr/` exit 0
- [ ] 27+10 测试通过 (baseline 27 + 新增 10+)
- [ ] TSan clean (新增并发测试覆盖 MockProviderFactory 多线程创建)
- [ ] ASan clean
- [ ] **ADR-0019 §1.4 状态在 T5 执行时更新为 ✅ 已解决** (避免时间悖论)
- [ ] 6 个 docs + 7 个 ADR 同步更新
- [ ] R5 重分类: project-organization.md R5 → P1 active (路径: archived)

## Out of Scope (Non-goals)

- ❌ 不实现 CloudProviderFactory / LlamaProviderFactory (避免 YAGNI, 留待后续 OpenSpec change)
- ❌ 不实现 `factory_name()` 虚函数 (YAGNI, 留待未来 PDK)
- ❌ 不实现 PDK (独立 OpenSpec change: `2026-07-07-pdk-skeleton`)
- ❌ 不实现 PluginLoader (独立 OpenSpec change: `2026-07-14-plugin-loader`)
- ❌ 不修改 ToolResult 现有 P1 字段(由 `2026-06-16-phase1-toolresult-standardization` 处理 P2-P4)

## Dependencies

- **Block**: `openspec/changes/archive/2026-06-17-phase1-bus-integration/` (Sprint 1b 已 ship, 吸收 3 deep modules/ 移除)
- **Block**: `openspec/changes/archive/2026-06-09-docs-code-alignment-fixes` (LayeredContext 稳定)
- **Block by**: `openspec/changes/archive/2026-06-16-phase1-toolresult-standardization` (P1-P4 已 ship)
- **Related (out-of-scope)**: `2026-07-14-plugin-loader`

## Estimated Effort

5 周 (25 工作日), 5 个独立 sub-task:
- T1: LLMProviderFactory + MockProviderFactory 从零构建 (5-7 天)
- T2: IToolRegistry 7 虚函数 + ToolRegistry/SecureToolRegistry 改造 + 6 个调用点迁移 (5 天)
- T3: TraceRecord 上移 + ADR-0033 路径更新 (2 天)
- T4: engine.h 移除 3 include + PIMPL-lite tool_registry_ + 析构外置 (3 天)
- T5: 验证 + 7 ADR 同步 + 6 docs 同步 + R5 重分类 + openspec validate (3 天)

> **2026-06-17 v2 修订对比原 10 工作日 +15 天**: 增量来自 (a) T1 实际工作量为从零构建, 不是 3 天复用; (b) T2 IToolRegistry 从 2 虚函数扩展至 7 虚函数 + SecureToolRegistry API 改造 + 6 个调用点迁移.

## R5 重分类说明 (2026-06-17)

原 `.omo/plans/archive/2026-06-15-archived/project-organization.md` R5 章节 (line 795-805) 将本 change 列为 "retrospective (spec snapshot)", 但本 change **是真实实现** (5 周工作量). **R5 重分类为 P1 active**, 待在 `openspec/changes/` (active). 注: 文件路径是 `archive/` 因为 plan 本身已 archive (2026-06-15 ship 后归档), 但 R5 是 active work.

## 验证命令

```bash
# engine.h 退出标准
grep -c '#include "modules/\|#include "common/' src/core/engine.h  # expected: 1

# ADR lint
python3 tools/adr_lint.py docs/adr/  # expected: exit 0

# 测试
cd build && ctest --output-on-failure  # expected: ≥ 27+10 PASS

# TSan (并发测试)
cmake -DCMAKE_BUILD_TYPE=TSan .. && make && ctest  # expected: clean

# OpenSpec 验证
openspec validate 2026-06-15-residual-engine-h-decoupling  # expected: exit 0
```

## 已知风险 (2026-06-17 v2)

- **SecureToolRegistry API 兼容性改造**: 选项 A 增加 ~1.5 天工作量
- **`get_tool_registry()` 返回 IToolRegistry& 是部分 breaking change**: 6 个调用点已列, 缓解通过 `get_tool_registry_concrete()`
- **LLMProviderFactory 从零构建**: 5-7 天, 不是 3 天
- **TraceRecord "POD" 措辞**: 严格意义非 POD, 用 "data-only struct" 更准确
- **ADR-0019 §1.4 状态更新时机**: 不应预先更新为 ✅ 已解决, 留待 T5 执行后更新
