## ADDED Requirements

### Requirement: engine-h-zero-cross-module

`src/core/engine.h` MUST NOT 包含任何 `modules/` 或 `common/` 下的非 types 头文件。
例外: `common/llm/llm_types.h`(types 头文件,允许保留)。
抽象 MUST 通过 `include/agenticdsl/contract/` 下的 i* 接口实现:
- `IProviderFactory` 是 contract 层抽象 (新建, 从零构建 LLMProviderFactory + MockProviderFactory, 因 ADR-0005 §3 设计草图未实现) — 替代 `common/llm/mock_provider.h` 直接 include
- `IToolRegistry` 含 8 虚函数 (镜像 `src/common/tools/registry.h:36-53` 公共 API, 排除 `register_tool` 模板成员函数) — 替代 `common/tools/registry.h` 直接 include
- `TraceRecord` data-only struct 上移到 `include/agenticdsl/types/trace_record.h` — 替代 `modules/trace/trace_exporter.h` 直接 include

#### Scenario: engine.h 跨模块 include 退出

- **WHEN** `grep -c '#include "modules/\|#include "common/' src/core/engine.h`
- **THEN** MUST = 1 (仅 llm_types.h types 头文件)
- **AND** `agenticdsl/contract/*` 头文件不被统计 (路径不匹配 grep 模式)
- **AND** Sprint 1b 已 ship 的 `iinteraction_bus.h` 不影响退出标准

#### Scenario: IProviderFactory contract 抽象存在

- **WHEN** 检查 `include/agenticdsl/contract/iprovider_factory.h`
- **THEN** MUST 含 `IProviderFactory` 接口 + `create(const LLMConfig&)` 虚函数 (1 个)
- **AND** MUST NOT 含 `factory_name()` 虚函数 (YAGNI, PDK out-of-scope)
- **AND** `LLMProviderFactory` 在 `src/common/llm/llm_provider_factory.h` (从零构建, 不复用 ADR-0005 §3 设计草图 — 草图未实现)
- **AND** `MockProviderFactory` 在 `src/common/llm/mock_provider_factory.h` (包装现有 MockLLMProvider)
- **AND** 命名空间 `agenticdsl` (扁平, 与现有 contract 头一致)

#### Scenario: IToolRegistry 8 虚函数镜像 ToolRegistry 公共 API

- **WHEN** 检查 `include/agenticdsl/contract/itool_registry.h`
- **THEN** MUST 含 8 个虚函数:
  - `has_tool(const std::string&) const` (基础查询)
  - `list_tools() const` (基础查询)
  - `call_tool(name, unordered_map)` 返回 `nlohmann::json` (镜像 ADR-0023 §C.3)
  - `register_llm_tool(name, unique_ptr<ILLMTool>, LLMParams)` (LLM 工具管理)
  - `is_llm_tool(name) const` (LLM 工具管理)
  - `get_llm_params(name) const` (LLM 工具管理)
  - `call_llm_tool(name, prompt, LLMParams)` (LLM 工具管理)
  - `set_cost_callback(CostCallback)` (成本回调)
- **AND** MUST NOT 含 `register_tool` 虚函数 (实际为模板成员函数, C++ 禁止模板 virtual)
- **AND** `ToolRegistry : public IToolRegistry` 在 `src/common/tools/registry.h` 加 8 override

#### Scenario: SecureToolRegistry 多继承 IToolRegistry (ADR-0004 V1.1)

- **WHEN** 检查 `include/agenticdsl/tools/secure_tool_registry.h` (**注意路径是 include/agenticdsl/tools/, 不是 src/common/tools/**)
- **THEN** `SecureToolRegistry : public IToolRegistry` (多继承装饰)
- **AND** 内部持有 `ToolRegistry base_registry_` (值成员, 不是指针)
- **AND** `call_tool` / `has_tool` / `list_tools` / `register_llm_tool` / `is_llm_tool` / `get_llm_params` / `call_llm_tool` / `set_cost_callback` 8 个 override 全部实现
- **AND** 保留原有 `call_direct` / `call_passthrough` 方法 (返回 `Result` 结构, ADR-0004 兼容)

#### Scenario: TraceRecord data-only struct 上移

- **WHEN** 检查 `include/agenticdsl/types/trace_record.h`
- **THEN** MUST 含 TraceRecord struct 定义 (data-only, 无方法)
- **AND** 字段依赖: `NodePath` (core/types/node.h) + `ExecutionBudget` (core/types/budget.h) + `nlohmann::json`
- **AND** `src/modules/trace/trace_exporter.h` MUST NOT 重新定义 TraceRecord (仅 include 新头文件)
- **AND** `docs/adr/adr-0033-session-hierarchy.md` §2 MUST 引用 `agenticdsl/types/trace_record.h`
- **AND** "POD" 措辞 MUST 改为 "data-only struct" (严格意义非 POD, nlohmann::json 堆分配)

#### Scenario: engine.h tool_registry_ PIMPL-lite + 析构外置

- **WHEN** 检查 `src/core/engine.h` 的 `tool_registry_` 成员
- **THEN** MUST 为 `std::unique_ptr<IToolRegistry>` (PIMPL-lite, 镜像 budget_controller_ 模式)
- **AND** `get_tool_registry()` 返回 `IToolRegistry&`
- **AND** `get_tool_registry_concrete()` 返回 `ToolRegistry&` (兼容性访问, 用于 SimpleCognitiveOrchestrator 等)
- **AND** `register_tool<>` template 通过 `get_tool_registry_concrete()` 委托 (避免 dynamic_cast)
- **AND** **`~DSLEngine()` 必须在 `engine.cpp` 中定义** (`= default` 不能在 .h, 否则违反 PIMPL)

#### Scenario: 6 个 get_tool_registry() 调用点迁移 (Partial Breaking Change)

- **WHEN** 本 change 完成
- **THEN** `tests/test_simple_orchestrator.cpp:53` MUST 改用 `get_tool_registry_concrete()`
- **AND** `tests/test_simple_orchestrator.cpp:79` MUST 改用 `get_tool_registry_concrete()`
- **AND** `tests/test_simple_orchestrator.cpp:102` MUST 改用 `get_tool_registry_concrete()`
- **AND** `tests/test_simple_orchestrator.cpp:126` MUST 改用 `get_tool_registry_concrete()`
- **AND** `tests/test_simple_orchestrator.cpp:150` MUST 改用 `get_tool_registry_concrete()`
- **AND** `examples/slice_01_tool_call/main.cpp:77` MUST 改用 `get_tool_registry_concrete()`
- **AND** 编译通过 (零回归)

#### Scenario: ADR-0019 §1.4 状态变更 (T5.2 执行时, 不是预先)

- **WHEN** T5.2 同步 7 个 ADR 时
- **THEN** `docs/adr/adr-0019-iinteraction-bus-mvp.md` §1.4 MUST 标记为 ✅ 已解决 (2026-06-XX)
- **AND** Sprint 1b (commit `248d209`, 2026-06-17) 吸收 3 deep modules/ 移除的工作 MUST 在变更日志中记录
- **AND** T5.2 完成前 MUST NOT 预先更新状态 (避免时间悖论)

#### Scenario: 7 个 ADR 同步更新

- **WHEN** T5.2 同步时
- **THEN** `docs/adr/adr-0005-llm-backend-config-factory.md` §3 MUST 修正实现状态说明 (LLMProviderFactory 设计草图未实现, 本 change 从零构建)
- **AND** `docs/adr/adr-0023-tool-result-standard.md` §C.3.1 MUST 加 "IToolRegistry 8 虚函数镜像" 说明
- **AND** `docs/adr/adr-0033-session-hierarchy.md` §2 MUST 更新 TraceRecord include path
- **AND** `docs/adr/adr-0004-toolregistry-security.md` MUST 加 "SecureToolRegistry V1.1 多继承" 改造
- **AND** `docs/adr/adr-0020-thread-model-isolation.md` §2.2.1 MUST 加 "IProviderFactory Per-Worker 协调"
- **AND** `docs/adr/adr-0022-plugin-loading.md` §4.2 MUST 加 "未来 PDK IToolRegistry& 注入" 协调
- **AND** `docs/adr/adr-0031-execution-policy.md` MUST 加 Related 注释 (Phase 2 PIMPL-lite 模式)

#### Scenario: 6 个 docs 同步更新 (T5.2)

- **WHEN** 本 change 完成
- **THEN** `docs/roadmap-status.md` line 49 P1 MUST 标 3/4 完成 + 链接 OpenSpec change
- **AND** `docs/phase1-roadmap.md` line 122 MUST 加 P1 状态更新
- **AND** `docs/SPRINT-1A-COMPLETION-REPORT.md` line 213 MUST 加 post-sprint 注释
- **AND** `AGENTS.md` line 17 MUST 移除 `budget_controller.h` (已 PIMPL-lite) + 引用本 change
- **AND** `.omo/plans/archive/2026-06-15-archived/project-organization.md` R5 章节 MUST 重分类为 P1 active (**注意路径是 archive/**)
- **AND** `.omo/boulder.json` MUST 标记 3/4 完成
- **AND** `docs/SPECS-ALIGNMENT.md` MUST 加变更追踪项
- **AND** `docs/implementation-roadmap.md` MUST 更新 25/25 → 27/27

#### Scenario: R5 retrospective → P1 active 重分类

- **WHEN** 检查 `.omo/plans/archive/2026-06-15-archived/project-organization.md` R5 章节 (**注意路径是 archive/**)
- **THEN** MUST 重分类为 "P1: Residual engine.h Decoupling (active, 2026-06-15 启动, 5 周估时)"
- **AND** R1-R4 在 `archive/2026-06-15-retrospectives/` 维持 retrospective
- **AND** R5 是 P1 active, 待在 `openspec/changes/` (非 archive)

#### Scenario: 全量测试零回归

- **WHEN** `ctest --output-on-failure`
- **THEN** MUST ≥ 27/27 PASS (baseline per `find tests -name 'test_*.cpp' | wc -l` = 27)
- **AND** MUST 含新增 IProviderFactory (≥ 4) + IToolRegistry (≥ 5) + TraceRecord (≥ 3) + LLMProviderFactory (≥ 3) + EngineNoCrossModule (≥ 2) 测试
- **AND** TSan 干净 (新增并发测试覆盖 MockProviderFactory 多线程创建)
- **AND** ASan 干净

#### Scenario: ADR lint + OpenSpec validate

- **WHEN** `python3 tools/adr_lint.py docs/adr/`
- **THEN** MUST exit 0
- **WHEN** `openspec validate 2026-06-15-residual-engine-h-decoupling`
- **THEN** MUST exit 0
