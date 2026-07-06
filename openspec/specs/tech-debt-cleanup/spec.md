# tech-debt-cleanup Specification

## Purpose
2026-06-09 综合审计 (`docs/audits/2026-06-09-tech-debt-and-doc-cleanup.md`, 95 分钟, 3 并行 explore agent + 30+ 直接检查) 发现 36 个问题分 4 级 (6 P0 / 13 P1 / 11 P2 / 6 P3):代码债 — `HttpLLMAdapter` 切换至 `ILLMProvider` (删 `src/common/llm/llm_adapter.h` 节省 58 行) + 日志门面 `src/common/log/` + `SPDLOG_ACTIVE_LEVEL` 替代 20+ 处 `std::cout "[DEBUG]"` 直接写生产代码 + 9 个 ADR-0010~0018 归档废弃 (1743 行) + 文档死链接清理 + 缺失文件补齐。Sprint 6 实施,约 5-7 工作日工作量。
## Requirements
### Requirement: no-deprecated-base-class

`HttpLLMAdapter` MUST NOT 继承 deprecated `ILLMAdapter` 接口, MUST 改为继承 `ILLMProvider`。
迁移完成后 `src/common/llm/llm_adapter.h` MUST 被删除,整个 deprecated `LLMConfig` 与 `ILLMAdapter` 旧版结构从代码库消失。

#### Scenario: HttpLLMAdapter 继承链

- **WHEN** 检查 `HttpLLMAdapter` 的基类
- **THEN** 必须是 `ILLMProvider` 而非 `ILLMAdapter`
- **AND** 编译时不得触发 `-Wdeprecated-declarations` 警告

#### Scenario: 删除 lllm_adapter.h

- **WHEN** `src/common/llm/llm_adapter.h` 已删除
- **THEN** 全项目编译通过,无 `#include` 引用缺失
- **AND** ctest 20/20 通过

### Requirement: unified-logging-facade

项目 MUST 提供 `agenticdsl::log::debug/info/warn/error()` 统一日志门面。
生产代码 MUST NOT 直接使用 `std::cout`/`std::cerr` 写调试/警告/错误信息。

#### Scenario: 替换 topo_scheduler.cpp 中的 std::cout

- **WHEN** 实施日志门面
- **THEN** `topo_scheduler.cpp` 中 15 处 `std::cout << "[DEBUG] ..."` MUST 全部替换为 `LOG_DEBUG(...)` 或 `log::debug(...)`
- **AND** `[WARNING]`/`[ERROR]` 同理替换为 `LOG_WARN/ERROR`

#### Scenario: release 编译剥除

- **WHEN** CMake `CMAKE_BUILD_TYPE=Release`
- **THEN** `LOG_DEBUG` 宏 MUST 完全剥除(零运行时开销)
- **AND** `LOG_INFO/WARN/ERROR` 输出到 stderr 而非 stdout (避免污染 DAG 输出流)

### Requirement: adr-marked-deprecated

9 个 ADR-0010~0018 与 5 个 ADR-0030/0032/0034/0036 MUST 在文档头部加 ⛔ 横幅,
明确"代码 0 命中,仅作设计历史保留"。`adr-0001` MUST 替换 `(YYYY-MM-DD)` 字面量为真实日期。

#### Scenario: ADR-0010 头部

- **WHEN** 阅读 `docs/adr/adr-0010-memory-system.md` 顶部
- **THEN** MUST 包含 `> ⛔ 已废弃 (2026-06-09)` 横幅
- **AND** MUST 引用本 OpenSpec change 作为废弃依据

#### Scenario: ADR-0001 日期

- **WHEN** 阅读 `docs/adr/adr-0001-illm-provider-streaming-interface.md:5`
- **THEN** MUST 显示真实日期(如 `2026-05-28`)而非 `YYYY-MM-DD` 字面量

### Requirement: cost-tracker-integration

`src/modules/budget/budget_controller.h` MUST 新增 `cost_tracker` 子结构跟踪 LLM 调用成本。
LLM 调用路径 MUST 累积 `total_cost_usd`,暴露 `get_session_cost()` API。

#### Scenario: LLM 调用累积 cost

- **WHEN** `call_llm_tool` 成功执行一次 LLM 调用
- **THEN** `cost_tracker.total_cost_usd` MUST 增加(基于 `LLMResult.tokens_generated` 计算)
- **AND** 单测 `tests/test_cost_collector.cpp` 验证多次调用后累计值正确

#### Scenario: 暴露查询 API

- **WHEN** 业务代码调用 `engine.get_session_cost()`
- **THEN** MUST 返回当前累计 `total_cost_usd`

### Requirement: execution-policy-implementations

`IExecutionPolicy` MUST 提供 3 个默认实现: `PlanModePolicy`, `AgentModePolicy`, `YoloModePolicy`,
对应 ADR-0031 §1 承诺的"三种模式"。`icognitive_orchestrator.h` 编译依赖 MUST 被解除。

#### Scenario: 3 个实现存在

- **WHEN** grep `class.*Policy.*public.*IExecutionPolicy` 在 src/
- **THEN** MUST 命中 3 个 (PlanMode/AgentMode/YoloMode)
- **AND** 每个 MUST 实现 8 个 IExecutionPolicy 纯虚方法

#### Scenario: 单测覆盖

- **WHEN** `ctest -R test_execution_policy`
- **THEN** MUST 至少 3 个 TEST_CASE (每个 policy 一组), 全通过

### Requirement: lib-stdlib-orphan-cleanup

`lib/` 下 6 个未被 parser 注册且无文档引用的孤儿 .md 子图 MUST 被删除或合并到
`docs/adr/agenticdsl/inference-stdlib/02-specification.md`。`parser` 注册的 2 个子图
(`/lib/utils/noop`, `/lib/math/add`) MUST 保持不变。

#### Scenario: 删除 6 个孤儿

- **WHEN** 实施 lib/ 清理
- **THEN** MUST 删除: `lib/auth/verify_session.md`, `lib/human/clarify_input.md`,
  `lib/human/confirm_action.md`, `lib/inference/engine.md`, `lib/inference/model.md`,
  `lib/inference/session.md` 中除 `session.md` 外的 4 个 (实际为 5 个删除,保留 1 个)
- **AND** 实施前 MUST grep 验证无引用

#### Scenario: parser 仍能加载 2 个保留子图

- **WHEN** `cmake --build . && ctest --output-on-failure`
- **THEN** 现有 20 个测试全通过,parser 行为不变

### Requirement: dead-link-fixes

`docs/` 下 5 处 `AgenticOS_*` 死链 MUST 修复为正确相对路径。
`docs/SPECS-ALIGNMENT.md:113`, `docs/roadmap-status.md:262`, `docs/adr/adr-0029.md:6`,
`docs/adr/adr-0035.md:6` 4 处 OpenSpec 链接 MUST 指向 `archive/2026-06-09-` 前缀路径。
4 处 ADR 相对路径错误(`../adr-0030` → `../adr/adr-0030`) MUST 修复。

#### Scenario: 死链修复

- **WHEN** 实施死链修复
- **THEN** `grep -rn "AgenticOS_Layer0_Spec\|AgenticOS_Architecture\|AgenticOS_Layer0_RefactoringPlan" docs/`
  MUST 返回 0 命中
- **AND** `grep -rn "../adr-0030[^/]" docs/adr/` MUST 返回 0 命中(只允许 `../adr/adr-0030`)

#### Scenario: OpenSpec 链接路径

- **WHEN** 实施 OpenSpec 链接修复
- **THEN** 4 处链接 MUST 全部以 `archive/2026-06-09-docs-code-alignment-fixes` 结尾
- **AND** 链接 MUST 可解析(目录存在)

### Requirement: call-llm-tool-test-coverage

`tests/test_call_llm_tool.cpp` MUST 新增,验证上一变更 (`docs-code-alignment-fixes`)
的 `kDefaults{}` sentinel 修复:用户显式传 `max_tokens: 512` MUST 真正生效,不被
`default_params` 中的 2048 覆盖。

#### Scenario: 用户显式传 512

- **WHEN** 业务代码调用 `call_llm_tool("foo", "prompt", {max_tokens: 512})`
- **THEN** `merged_params.max_tokens` MUST 为 512, NOT 2048
- **AND** 实际传给 LLM provider 的 `params.max_tokens` MUST 为 512

#### Scenario: 用户省略 max_tokens

- **WHEN** 业务代码调用 `call_llm_tool("foo", "prompt", {})`
- **THEN** `merged_params.max_tokens` MUST 为 `default_params.max_tokens` (2048)

### Requirement: req1-md-llmcallnode-banner

`src/modules/exports/req1.md` MUST 在引用 `LLMCallNode` 的 5 处位置(line 2151/2388/2439/2496/2514)
加 `<!-- ARCHIVED: superseded by DSLNode (v3.10) -->` 横幅。

#### Scenario: 5 处横幅存在

- **WHEN** grep "ARCHIVED: superseded by DSLNode" `src/modules/exports/req1.md`
- **THEN** MUST 命中 ≥ 5 次 (5 处 LLMCallNode 引用各加 1 个横幅)

### Requirement: 5 个 ADR 实现 vs 文档审计

> 本 requirement SHALL 跟踪 5 个 ADR（用户原消息中提及）的实现状态。

#### Scenario: ADR-0002 EventBus 状态

- **WHEN** 检查 `docs/adr/adr-0002-eventbus-bounded-queue.md` 描述的 `EventBus` 类
- **THEN** grep `src/` 下 `class.*EventBus` 定义
- **AND** 当前结果：无 `EventBus` 类存在，仅有 `InMemoryBus`（ADR-0019 MVP）
- **AND** ADR-0002 描述的 FTXUI/HarnessEngine 系统不在 AgenticDSL 代码库中
- **AND** 实现 vs 文档 drift 状态：见 `docs/adr/adr-0002-impl-scope.md`

#### Scenario: ADR-0004 ToolRegistry Security 状态

- **WHEN** 检查 `docs/adr/adr-0004-toolregistry-security.md` 描述的类
- **THEN** grep `src/` 下 `PathPolicy`/`ShellGuard`/`SecureToolRegistry`/`ApprovalPolicy`/`ToolCategory`
- **AND** 当前结果：均不存在
- **AND** 仅有 `execution_policy.h` 族系（agent/plan/yolo mode），非 ToolRegistry 安全层
- **AND** 实现 vs 文档 drift 状态：见 `docs/adr/adr-0004-impl-scope.md`

#### Scenario: ADR-0030 AsyncRuntime 状态

- **WHEN** 检查 `docs/archive/adr/adr-0030-async-runtime-dual-layer.md`
- **THEN** grep `src/` 下 `AsyncRuntime` 类定义
- **AND** 当前结果：无（`taskflow` + `async_simple` 依赖已引入 external/ 但 AsyncRuntime 包装层未实现）
- **AND** ADR 状态 ❌ 未实施（用户描述准确）

#### Scenario: ADR-0034 IModelRouter 状态

- **WHEN** 检查 `docs/archive/adr/adr-0034-model-router.md`
- **THEN** grep `src/` 下 `IModelRouter`/`ModelRouter` 类定义
- **AND** 当前结果：无
- **AND** ADR 状态 ❌ 未实施（用户描述准确）

### Requirement: plan self-audit 不可作为真实 verification

> 本 requirement SHALL 跟踪 `.omo/plans/project-organization.md` 中 F1-F4 self-audit 的局限性。

#### Scenario: F1 占位符输出

- **WHEN** 读取 plan F1 输出行
- **THEN** 当前文本含字面占位符 `Compliance [N/N] | VERDICT: APPROVE/REJECT`
- **AND** `[N/N]` 未填充数字
- **AND** F1 MUST 标 `[ ]` 而非 `[x]`

#### Scenario: F2 自我承认 BLOCKED

- **WHEN** 读取 plan F2 输出行
- **THEN** 当前文本 `Build [BLOCKED-env] | Tests [BLOCKED-env] | Lint [PASS]`
- **AND** F2 自我承认 2/3 验证未执行
- **AND** F2 MUST 标 `[ ]` + 注脚 "需独立 unspecified-high agent 执行 cmake/ctest"

#### Scenario: F3 部分数字准确

- **WHEN** 读取 plan F3 输出行
- **THEN** 当前文本 `Examples [2/6 build] | engine.h [2/3 modules/ removed]`
- **AND** Examples 实际状态：1/6 build (agent_basic) + 1/6 conditionally build (slice_01_tool_call)
- **AND** engine.h 实际状态：1 modules/ + 3 common/ 残留（既非 2/3 也非 0）
- **AND** F3 数字与实际不完全一致，需重审计

#### Scenario: F4 数字无依据

- **WHEN** 读取 plan F4 输出行
- **THEN** 当前文本 `Tasks [20/22 done, 4 commit gates pending]`
- **AND** 实际 archived tasks.md 分别 31/38 sub-tasks，无 22 这个数
- **AND** "4 commit gates pending" 在任何 tasks.md 中无依据
- **AND** F4 MUST 标 `[ ]` + 注脚 "20/22 数字无依据，需重新审计"
- **AND** F4 APPROVE verdict 撤销

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

### Requirement: scheduler-fork-dedup

`src/modules/scheduler/topo_scheduler.cpp` MUST NOT 含重复的 fork 处理逻辑 — `execute()` L161-167 与 `dispatch_next_node()` L636-642 当前逐字相同。

#### Scenario: fork 处理仅 1 处

- **WHEN** `grep -n "execute_fork_branches" src/modules/scheduler/topo_scheduler.cpp`
- **THEN** MUST 仅 1 个调用点 (位于 `execute()` 内, 排除函数定义与文档注释)
- **AND** `dispatch_next_node()` MUST NOT 调用 `execute_fork_branches`

### Requirement: scheduler-pipeline-tightened

`TopoScheduler::execute()` MUST 进一步拆分为纯函数式 3 子函数, 函数体 MUST ≤ 60 行:

- MUST 新增 `struct DagState { unordered_map<NodeId, NodeExecutionStatus> nodes; queue<NodeId> ready_queue; int pending_count; };`
- MUST 真正提取动态 `wait_for` 解析 / jump 处理 / fork-join / 动态图重建 各自成子函数
- MUST 3 子函数声明为 `private`, 仅通过 `DagState&` 参数通信, 不直接修改 `TopoScheduler` 成员
- MUST 使用 `session_.get_pending_dynamic_deps()` 访问器 (非直接访问 `pending_dynamic_deps_`)

#### Scenario: execute 函数行数上限

- **WHEN** `awk '/^ExecutionResult TopoScheduler::execute/,/^}$/' src/modules/scheduler/topo_scheduler.cpp | wc -l`
- **THEN** MUST ≤ 60 行

#### Scenario: DagState 结构体存在

- **WHEN** `grep "struct DagState" src/modules/scheduler/topo_scheduler.h`
- **THEN** MUST 命中 1 个结构体定义
- **AND** MUST 含 `nodes` / `ready_queue` / `pending_count` 3 个字段

#### Scenario: 3 子函数命名匹配 spec

- **WHEN** `grep "prepare_dag_state\|dispatch_ready_nodes\|handle_node_completion" src/modules/scheduler/topo_scheduler.h`
- **THEN** MUST 命中 3 个函数声明
- **AND** 名称 MUST 严格匹配 (非 `dispatch_next_node` / `finalize_execution` 等偏离命名)

#### Scenario: 访问一致

- **WHEN** `grep "session_\.pending_dynamic_deps_" src/modules/scheduler/ -r`
- **THEN** MUST 返回 0 命中 (统一使用 accessor)

#### Scenario: Hub out_degree

- **WHEN** `mcp__code-review-graph__get_hub_nodes --top_n 5`
- **THEN** `topo_scheduler::execute` out_degree MUST < 30
- **AND** 3 子函数各自 out_degree MUST < 25

### Requirement: scheduler-test-coverage

`tests/test_scheduler.cpp` MUST 含 ≥ 7 个新 TEST_CASE 覆盖 DagState 子函数行为

#### Scenario: scheduler 7 测试通过

- **WHEN** `ctest -R test_scheduler --output-on-failure`
- **THEN** MUST ≥ 7 新 case pass
- **AND** 既有测试 MUST 零回归

### Requirement: parser-test-coverage

`tests/test_parser.cpp` MUST 含 ≥ 5 个新 TEST_CASE 覆盖 NodeFactoryRegistry

#### Scenario: parser 5 测试通过

- **WHEN** `ctest -R test_parser --output-on-failure`
- **THEN** MUST ≥ 5 新 case pass
- **AND** 既有测试 MUST 零回归

#### Scenario: parser 5 测试 TSan 干净

- **WHEN** `cmake --preset tsan && ctest -R test_parser`
- **THEN** MUST 0 race (尤其 `factory_registry_concurrent_access` case)

### Requirement: node-factory-registry-count-corrected

`openspec/changes/tech-debt-cleanup-sprint-6/specs/node-factory-registry/spec.md` MUST 修正 NodeType 计数 13 → 11

#### Scenario: spec 笔误修正

- **WHEN** `grep -E "13 (types|Nodes)|types.*13" tech-debt-cleanup-sprint-6/specs/node-factory-registry/spec.md`
- **THEN** MUST 返回 0 命中 (全部替换为 11)

#### Scenario: spec throw → nullptr 修正

- **WHEN** `grep -E "throw.*unknown.*type|unknown.*throw" tech-debt-cleanup-sprint-6/specs/node-factory-registry/spec.md`
- **THEN** MUST 返回 0 命中
- **AND** MUST 含 "returns nullptr" 字样

### Requirement: engine-cpp-include-le-3

`src/core/engine.cpp` 跨模块 include 计数 MUST ≤ 3 (ADR-0019 §1.4 退出标准)

#### Scenario: include 计数验证

- **WHEN** `grep -cE '^\s*#include\s+"(modules/|common/)' src/core/engine.cpp`
- **THEN** MUST ≤ 3

#### Scenario: ToolRegistry factory 化

- **WHEN** `grep "make_unique<ToolRegistry>" src/core/engine.cpp`
- **THEN** MUST 返回 0 命中
- **AND** MUST 改用 `agenticdsl::tools::create_registry()`

#### Scenario: MockLLMProvider factory 化

- **WHEN** `grep "make_unique<MockLLMProvider>" src/core/engine.cpp`
- **THEN** MUST 返回 0 命中
- **AND** MUST 改用 `agenticdsl::llm::create_mock_provider()`

#### Scenario: IBudgetController 抽象注入

- **WHEN** `grep "BudgetController" src/core/engine.cpp`
- **THEN** MUST 仅 1 命中 (类型声明, 完整型不再使用)
- **AND** MUST 改用 `IBudgetController` 接口

### Requirement: scheduler-factory-resurrected

`src/modules/scheduler/factory.{h,cpp}` MUST 补 Config 参数并被 engine.cpp 调用

#### Scenario: factory 签名带 Config

- **WHEN** `grep "create.*SchedulerConfig" src/modules/scheduler/factory.h`
- **THEN** MUST 命中 ≥ 1
- **AND** 签名 MUST 含 `SchedulerConfig` + `IToolRegistry&` + `ILLMProvider*` + `vector<ParsedGraph>*` 4 参数

#### Scenario: engine.cpp 调用 factory

- **WHEN** `grep "agenticdsl::scheduler::create" src/core/engine.cpp`
- **THEN** MUST 命中 ≥ 1
- **AND** 替换 `make_unique<TopoScheduler>(...)` 调用

### Requirement: engine-factory-test-coverage

`tests/test_engine_factory.cpp` MUST 新建并含 3 个 TEST_CASE

#### Scenario: factory 3 测试通过

- **WHEN** `ctest -R test_engine_factory --output-on-failure`
- **THEN** MUST 3 case pass
- **AND** 覆盖: scheduler create / budget create / provider factory create

### Requirement: plugin-test-rename-and-e2e

`tests/test_plugin_loader.cpp` MUST 改名 7 case 匹配 spec + 实施 mock .so fixture E2E 测试

#### Scenario: 7 case 改名匹配 spec

- **WHEN** `grep -E "load_valid_plugin|abi_version_mismatch_strict|abi_version_mismatch_non_strict|dlsym_missing_register_fn|dlopen_failure_invalid_path|unload_all_raii_verification|load_all_search_paths" tests/test_plugin_loader.cpp`
- **THEN** MUST 命中 7

#### Scenario: mock .so fixture 存在

- **WHEN** `find build -name "mock_plugin*.so"`
- **THEN** MUST ≥ 3 文件存在 (mock_plugin + mock_plugin_v0 + mock_plugin_no_register)

#### Scenario: Loaded 状态覆盖

- **WHEN** `grep "state.*==.*Loaded\|set_loaded\|mark_loaded" tests/test_plugin_loader.cpp`
- **THEN** MUST ≥ 1 命中

#### Scenario: TEST_PLUGIN_FIXTURE_PATH 宏注入

- **WHEN** `grep "TEST_PLUGIN_FIXTURE_PATH" tests/CMakeLists.txt`
- **THEN** MUST 命中 `target_compile_definitions(test_plugin_loader PRIVATE TEST_PLUGIN_FIXTURE_PATH=...)`

### Requirement: ship-gate-all-pass

C1 ship gate 全部验证 MUST pass

#### Scenario: ctest 全绿

- **WHEN** `cd build && ctest --output-on-failure`
- **THEN** MUST ≥ 47/47 PASS

#### Scenario: TSan 全绿

- **WHEN** `cmake --preset tsan && ctest --output-on-failure`
- **THEN** MUST 0 race / 0 warning

#### Scenario: ASan 全绿

- **WHEN** `cmake --preset asan && ctest --output-on-failure`
- **THEN** MUST 0 leak

#### Scenario: docs audit 干净

- **WHEN** `python3 tools/adr_lint.py docs/adr/`
- **THEN** MUST exit 0
- **WHEN** `python3 tools/docs_drift_audit.py`
- **THEN** MUST 返回 0 critical drift

#### Scenario: openspec validate 成功

- **WHEN** `openspec validate 2026-06-26-sprint-7-tech-debt-execution`
- **THEN** MUST exit 0

#### Scenario: tech-debt-cleanup-sprint-6 archive 成功

- **WHEN** `openspec archive tech-debt-cleanup-sprint-6 --yes`
- **THEN** MUST exit 0
- **AND** `openspec list --specs` MUST 显示 `tech-debt-cleanup` spec 已合并

#### Scenario: PDK 同步无 error

- **WHEN** `./scripts/sync-pdk.sh --dry-run`
- **THEN** MUST exit 0, 0 error

