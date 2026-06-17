# Tasks: Residual engine.h Decoupling (Stage 4 Task 19 残留)

> **变更类型**: 真实实现 — 全部 task 标 [ ],需实际执行
> **2026-06-17 v2 修订**: Oracle 审查后修复 6 P0 + 6 P1. 关键认知: LLMProviderFactory 仅在 ADR-0005 §3 设计草图 (md), 编译代码不存在; IToolRegistry 必须扩展至 ~7 虚函数覆盖 ToolRegistry 公共 API; SecureToolRegistry 需 API 兼容性改造; 6 个 SimpleCognitiveOrchestrator 调用点需迁移; 工作量从 10 → 25 工作日 (5 周).
> **测试基线**: 27/27 (per `find tests -name 'test_*.cpp' | wc -l`)

## 任务依赖图

```
T1 LLMProviderFactory 从零构建 (5-7d)
 ├── T1.1 IProviderFactory 接口定义 (0.5d)
 ├── T1.2 LLMProviderFactory 实现 (1.5d) — 单一 backend_name 路由
 ├── T1.3 MockProviderFactory 实现 (1d) — 包装现有 MockLLMProvider
 ├── T1.4 DSLEngine 注入 provider_factory_ (1d)
 ├── T1.5 test_provider_factory.cpp (1.5d) — ≥ 4 case
 ↓
T2 IToolRegistry 7 虚函数 + SecureToolRegistry 改造 (5d)
 ├── T2.1 IToolRegistry 接口定义 (1d) — 7 虚函数
 ├── T2.2 ToolRegistry : public IToolRegistry + 7 override (0.5d)
 ├── T2.3 SecureToolRegistry API 兼容性改造 (1.5d) — 添加 IToolRegistry 多继承
 ├── T2.4 6 个 get_tool_registry() 调用点迁移 (1d):
 │       - tests/test_simple_orchestrator.cpp:53,79,102,126,150 (5 sites)
 │       - examples/slice_01_tool_call/main.cpp:77 (1 site)
 │       → 改用 get_tool_registry_concrete()
 ├── T2.5 test_tool_registry_interface.cpp (1d) — ≥ 5 case
 ↓
T3 TraceRecord 上移 (2d)
 ├── T3.1 拆分 src/modules/trace/trace_exporter.h → include/agenticdsl/types/trace_record.h (1d)
 ├── T3.2 ADR-0033 §2 路径更新 + 验证编译 (1d)
 ↓
T4 engine.h 移除 3 include + PIMPL-lite (3d)
 ├── T4.1 tool_registry_ 改 unique_ptr<IToolRegistry> (1d)
 ├── T4.2 provider_factory_ 注入 + 析构外置到 engine.cpp (1d)
 ├── T4.3 engine.h 移除 3 include + 加 3 抽象 include + 验证 (1d)
 ↓
T5 验证 + 同步 (3d)
 ├── T5.1 跑 27+10 ctest + TSan + ASan (1d)
 ├── T5.2 同步 7 个 ADR + 6 个 docs + R5 重分类 (1d)
 ├── T5.3 openspec validate + commit (1d)
```

## Tasks

### T1. LLMProviderFactory 从零构建 (5-7 天)

> **2026-06-17 v2 关键认知**: ADR-0005 §3 的 `LLMProviderFactory` + 3 个 `ProviderCreator` 仅是设计草图 (markdown fenced code block), 编译代码不存在. 本任务是从零构建, 不是"沿用现有".

- **文件**:
  - `include/agenticdsl/contract/iprovider_factory.h` (新建, 1 个虚函数 `create(LLMConfig)`)
  - `src/common/llm/llm_provider_factory.h/cpp` (新建, 单一 backend_name 路由)
  - `src/common/llm/mock_provider_factory.h/cpp` (新建, 包装现有 `MockLLMProvider`)
  - `src/core/engine.h` (使用 `IProviderFactory` 而非直接 include `mock_provider.h`)
  - `src/core/engine.cpp` (工厂注入 + MockProviderFactory 默认注册)
  - `tests/test_provider_factory.cpp` (新建, ≥ 4 test cases)
- **粒度**: 5-7 天
- **验收**:
  - [ ] IProviderFactory 接口定义在 `include/agenticdsl/contract/` (1 个虚函数)
  - [ ] LLMProviderFactory 实现包含 backend_name 路由 (`config.provider` 字段)
  - [ ] MockProviderFactory::create(LlmConfig) 返回 MockLLMProvider (包装现有类, 不改 MockLLMProvider)
  - [ ] DSLEngine 默认使用 MockProviderFactory
  - [ ] 多线程 1000x create() 无 data race
  - [ ] test_provider_factory ≥ 4 case 通过
  - [ ] `grep "common/llm/mock_provider" src/core/engine.h` = 0
  - [ ] 命名空间: `namespace agenticdsl` (扁平, 与现有 contract 头一致)
- **Out of scope**: CloudProviderFactory / LlamaProviderFactory (后续 OpenSpec change)

### T2. IToolRegistry 7 虚函数 + SecureToolRegistry 改造 + 6 调用点迁移 (5 天)

> **2026-06-17 v2 关键认知**: IToolRegistry 必须扩展至 ~7 虚函数覆盖 ToolRegistry 公共 API (registry.h:36-53), 否则 `engine.cpp:114,191-192` `set_cost_callback`/`register_llm_tool` 编译失败; `node_executor.cpp:135` `call_llm_tool` 也需 IToolRegistry 暴露.

- **文件**:
  - `include/agenticdsl/contract/itool_registry.h` (新建, 7 虚函数)
  - `src/common/tools/registry.h` (加 `override`, 保持模板 `register_tool`)
  - `include/agenticdsl/tools/secure_tool_registry.h` (API 兼容性改造 — 选项 A: 添加 IToolRegistry 多继承 + 重写 call_tool/has_tool)
  - **6 个 get_tool_registry() 调用点迁移**:
    - `tests/test_simple_orchestrator.cpp:53` → 改用 `get_tool_registry_concrete()`
    - `tests/test_simple_orchestrator.cpp:79` → 改用 `get_tool_registry_concrete()`
    - `tests/test_simple_orchestrator.cpp:102` → 改用 `get_tool_registry_concrete()`
    - `tests/test_simple_orchestrator.cpp:126` → 改用 `get_tool_registry_concrete()`
    - `tests/test_simple_orchestrator.cpp:150` → 改用 `get_tool_registry_concrete()`
    - `examples/slice_01_tool_call/main.cpp:77` → 改用 `get_tool_registry_concrete()`
  - `src/core/engine.h` (`get_tool_registry()` 返回 `IToolRegistry&`, 加 `get_tool_registry_concrete() : ToolRegistry&`)
  - `tests/test_tool_registry_interface.cpp` (新建, ≥ 5 test cases)
- **粒度**: 5 天
- **验收**:
  - [ ] IToolRegistry 接口定义在 `include/agenticdsl/contract/` (7 虚函数)
  - [ ] call_tool 返回 `nlohmann::json` (镜像 ADR-0023 §C.3)
  - [ ] call_tool 参数 `unordered_map<string,string>` (镜像 registry.h:37)
  - [ ] call_tool / has_tool / list_tools / register_llm_tool / is_llm_tool / get_llm_params / call_llm_tool / set_cost_callback = 8 虚函数 (镜像 ToolRegistry 公共 API)
  - [ ] 故意省略 `register_tool` 虚函数 (实际为模板成员函数)
  - [ ] ToolRegistry : public IToolRegistry (现有 27/27 测试零回归)
  - [ ] SecureToolRegistry : public IToolRegistry (多继承装饰, 选项 A)
  - [ ] **6 个 get_tool_registry() 调用点全部迁移** (5 test + 1 example)
  - [ ] test_tool_registry_interface ≥ 5 case 通过
  - [ ] `grep "common/tools/registry" src/core/engine.h` = 0

### T3. TraceRecord 上移到 include/agenticdsl/types/ (2 天)

- **文件**:
  - `include/agenticdsl/types/trace_record.h` (新建, data-only struct)
  - `src/modules/trace/trace_exporter.h` (改为 include 新头文件, 删 TraceRecord 内联定义)
  - `src/modules/trace/trace_exporter.cpp` (实现, 不变)
  - `docs/adr/adr-0033-session-hierarchy.md` §2 (路径更新)
- **粒度**: 2 天
- **验收**:
  - [ ] TraceRecord struct 在 `include/agenticdsl/types/trace_record.h` (data-only, 无方法)
  - [ ] trace_exporter.h 不再定义 TraceRecord (仅 include 新头文件)
  - [ ] ADR-0033 §2 路径同步更新
  - [ ] 现有 trace 相关测试零回归
  - [ ] `grep "modules/trace/trace_exporter" src/core/engine.h` = 0

### T4. engine.h 移除 3 include + PIMPL-lite tool_registry_ + 析构外置 (3 天)

- **文件**:
  - `src/core/engine.h` (核心修改)
  - `src/core/engine.cpp` (析构移到类外, 默认 factory 构造)
- **粒度**: 3 天
- **验收**:
  - [ ] `tool_registry_` 从 `ToolRegistry` 值成员改为 `std::unique_ptr<IToolRegistry>` (PIMPL-lite)
  - [ ] `provider_factory_` 新增为 `std::unique_ptr<IProviderFactory>` (默认 `MockProviderFactory`)
  - [ ] `get_tool_registry()` 返回 `IToolRegistry&`
  - [ ] `get_tool_registry_concrete()` 返回 `ToolRegistry&` (兼容性访问)
  - [ ] **析构移到 engine.cpp** (`= default` 在 .cpp 而非 .h, 避免 PIMPL 违反)
  - [ ] 移除 `#include "common/llm/mock_provider.h"` (line 38)
  - [ ] 移除 `#include "common/tools/registry.h"` (line 39)
  - [ ] 移除 `#include "modules/trace/trace_exporter.h"` (line 47)
  - [ ] 保留 `#include "common/llm/llm_types.h"` (line 37, types 头文件例外)
  - [ ] 加 `#include "agenticdsl/contract/iprovider_factory.h"`
  - [ ] 加 `#include "agenticdsl/contract/itool_registry.h"`
  - [ ] 加 `#include "agenticdsl/types/trace_record.h"`
  - [ ] `grep -c '#include "modules/\|#include "common/' src/core/engine.h` = 1 (仅 llm_types.h)
  - [ ] `grep -c '#include "agenticdsl/contract/' src/core/engine.h` ≥ 5 (ischeduler/iparser/iinteraction_bus/iprovider_factory/itool_registry)

### T5. 验证 + 同步 (3 天)

- **文件**:
  - 7 个 ADR: 0019/0005/0023/0033/0004/0020/0031
  - 6 个 docs: roadmap-status / phase1-roadmap / SPRINT-1A / AGENTS / project-organization / SPECS-ALIGNMENT / implementation-roadmap
  - `.omo/boulder.json` (P1 3/4 完成标记)
- **粒度**: 3 天
- **验收**:
  - [ ] `cmake --build build && ctest --output-on-failure` ≥ 27+10 PASS
  - [ ] TSan 干净 (新增并发测试覆盖)
  - [ ] ASan 干净
  - [ ] `tools/adr_lint.py docs/adr/` exit 0
  - [ ] `openspec validate 2026-06-15-residual-engine-h-decoupling` exit 0
  - [ ] **ADR-0019 §1.4 状态更新为 ✅ 已解决** (在 T5.2 执行时, **不是现在**)
  - [ ] ADR-0005 §3 修正 LLMProviderFactory 实现状态说明
  - [ ] ADR-0023 §C.3.1 IToolRegistry 8 虚函数说明 (note: 8 = 7 + cost_callback typedef)
  - [ ] ADR-0033 §2 TraceRecord 新位置
  - [ ] ADR-0004 SecureToolRegistry 多继承改造
  - [ ] ADR-0020 §2.2.1 IProviderFactory 协调
  - [ ] ADR-0031 Related 注释
  - [ ] docs/roadmap-status.md line 49 P1 标 3/4 完成
  - [ ] docs/phase1-roadmap.md line 122 P1 状态更新
  - [ ] docs/SPRINT-1A-COMPLETION-REPORT.md line 213 post-sprint 注释
  - [ ] AGENTS.md line 17 移除 budget_controller.h (已 PIMPL-lite) + 引用本 change
  - [ ] `.omo/plans/archive/2026-06-15-archived/project-organization.md` R5 重分类为 P1 active
  - [ ] `.omo/boulder.json` 标记 3/4 完成
  - [ ] docs/SPECS-ALIGNMENT.md 加变更追踪项
  - [ ] docs/implementation-roadmap.md 25/25 → 27/27
  - [ ] Single commit `refactor(core): complete engine.h decoupling (IProviderFactory facade + IToolRegistry 8-method + TraceRecord POD)`
  - [ ] 提交信息符合 conventional commits

## 总工作量

~25 工作日 (5 周单人)

## 验证清单

- [ ] 27 baseline + 10+ 新增 (4 IProviderFactory + 5 IToolRegistry + 3 TraceRecord + LLMProviderFactory 3) = 37+ 测试零回归
- [ ] TSan 干净 (含新增并发测试)
- [ ] ASan 干净
- [ ] CI 6 jobs 全绿
- [ ] `openspec validate` 0 error
- [ ] engine.h 跨模块 include 退出 = 1 (仅 llm_types.h types 头文件例外)
- [ ] **ADR-0019 §1.4 状态在 T5.2 完成后更新** (避免时间悖论)
- [ ] 7 个 ADR 同步更新
- [ ] 6 个 docs 同步更新
- [ ] R5 重分类: project-organization.md R5 → P1 active

## 提交策略

**Single commit**: `refactor(core): complete engine.h decoupling (IProviderFactory facade + IToolRegistry 8-method + TraceRecord POD)`
**包含**: T1-T5 全部代码 + 测试 + 7 ADR 更新 + 6 docs 同步

## 风险 (2026-06-17 v2)

- T2 SecureToolRegistry API 兼容性改造 (选项 A 增加 ~1.5d) — 接受
- T2 get_tool_registry() 返回 IToolRegistry& 是 partial breaking change — 6 调用点已列迁移计划
- T1 LLMProviderFactory 从零构建, 工作量 5-7d — 不是 3d 复用
- T3 TraceRecord "POD" 措辞不准确 — 用 "data-only struct"
- T4 析构必须外置到 engine.cpp — 否则违反 PIMPL
- T5 ADR-0019 §1.4 状态更新时机 — T5.2 执行时, 不能预先
- R5 retrospective vs P1 active 矛盾 — proposal.md + design.md + project-organization.md 三处显式声明
