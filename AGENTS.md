# PROJECT KNOWLEDGE BASE

**Generated:** 2026-05-11
**Commit:** cc8c7df
**Branch:** main

## OVERVIEW
AgenticDSL 是一个 DSL 执行引擎，通过 Markdown DSL 定义工作流图（DAG），支持 LLM 调用、工具注册、资源管理和预算控制。C++20 实现，使用 llama.cpp 作为 LLM 后端。

## STRUCTURE
```
HydraForge/
├── src/
│   ├── core/          # DSLEngine 核心入口
│   │   └── types/     # Context, Node, Budget, Resource 类型定义
│   ├── common/        # 共享组件
│   │   ├── llm/       # LlamaAdapter (llama.cpp 封装)
│   │   ├── tools/     # ToolRegistry (工具注册表)
│   │   └── utils/     # YAML/JSON 解析、模板渲染
│   └── modules/       # 10 个功能模块
│       ├── parser/    # MarkdownParser → ParsedGraph
│       ├── scheduler/ # TopoScheduler (DAG 拓扑调度)
│       ├── executor/  # NodeExecutor (节点执行器)
│       ├── context/   # Context 管理
│       ├── budget/    # BudgetController (预算控制)
│       ├── trace/     # TraceRecord 追踪
│       ├── library/   # StandardLibraryLoader (标准库加载)
│       ├── system/    # System 模块
│       ├── cognitive/ # 认知编排 (SimpleCognitiveOrchestrator, ADR-0019/0020)
│       └── exports/   # 导出类型定义与设计稿
├── lib/               # DSL 标准库 (.md 文件)
│   ├── auth/          # 认证相关 DSL
│   ├── human/         # 人类交互 DSL
│   ├── math/          # 数学工具 DSL
│   ├── utils/         # 通用工具 DSL
│   └── inference/     # 推理控制面 (engine, model, session 子图)
├── external/          # 第三方依赖 (llama.cpp, nlohmann_json, inja, yaml-cpp)
├── include/           # 公共头文件 (ADR-0019 契约层: contract/cognitive/policy/types)
├── tests/             # Catch2 单元测试 (20 个测试文件)
└── examples/          # 3 个示例程序
    ├── agent_basic/   # 主要示例：加载 .agent.md 工作流
    ├── agent_simple/  # 简化示例
    └── agent_loop/    # 循环执行示例
```

> **注意 (2026-06-13 审计更正, OpenSpec change `docs-code-drift-audit-2026-06`)**:`examples/agent_simple/` 和 `examples/agent_loop/` 的 DEPRECATED 注释基于错误删除假设撰写——`LlamaAdapter`/`InjaTemplateRenderer`/`extract_pathed_blocks` 实际**仍存在**于代码库;`PromptBuilder` 在 commit `9a619f3` (2025-11-05) 真删(早于原注释误归的 `ac9e684` 7 个月);`get_llm_adapter()` 真删(应改 `get_llm_provider()`)。具体真实编译错误见各文件 ACTUAL STATE NOTE 注释。迁移至 `MockLLMProvider` + `ILLMProvider` 模式的工作将由独立 OpenSpec change 完成(范围超出本 audit change)。

## WHERE TO LOOK
| Task | Location | Notes |
|------|----------|-------|
| 添加新节点类型 | `src/modules/executor/node_executor.h` | execute_xxx 方法 + execute_node 分发 |
| 添加新 DSL 语法 | `src/modules/parser/markdown_parser.h` | create_node_from_json |
| 修改调度逻辑 | `src/modules/scheduler/topo_scheduler.cpp` | build_dag() / schedule() |
| LLM 调用修改 | `src/common/llm/llama_adapter.cpp` | generate() 底层 |
| 工具注册/调用 | `src/common/tools/registry.cpp` | call_tool() / register_tool() |
| 预算管理 | `src/modules/budget/budget_controller.cpp` | ExecutionBudget 扣费 |
| 编写测试 | `tests/test_*.cpp` | Catch2，tag 格式 `[module][stageN]` |
| DSL 标准库 | `lib/*.md` | Markdown 格式的子图定义 |

## CODE MAP (Key Symbols)

| Symbol | Type | Location | Role |
|--------|------|----------|------|
| DSLEngine | class | src/core/engine.h | 主入口，from_markdown / run |
| ParsedGraph | struct | src/core/types/node.h | 解析后的图结构 |
| TopoScheduler | class | src/modules/scheduler/topo_scheduler.h | DAG 调度器 |
| NodeExecutor | class | src/modules/executor/node_executor.h | 节点执行器 |
| ToolRegistry | class | src/common/tools/registry.h | 工具注册表 |
| LlamaAdapter | class | src/common/llm/llama_adapter.h | llama.cpp 封装 |
| ExecutionBudget | struct | src/core/types/budget.h | 预算结构 |
| LayeredContext | struct | include/agenticdsl/types/layered_context.h | 5-层结构化上下文 (L1-L5, ADR-0008) |
| DomainWorkerPool | class | include/agenticdsl/cognitive/domain_worker_pool.h | 领域智能体工作线程池 (Sprint 3, ADR-0020 §3.2 ✅ Resolved) |
| DomainTask | struct | include/agenticdsl/cognitive/domain_worker_pool.h | 领域任务结构 (domain/tool_name/arguments/output_key) |
| DECLARE_TOOL (macro) | macro | include/agenticdsl/pdk/tool_macros.h | PDK 工具注册宏 (Sprint 4, ADR-0021 🟡 Partial) |
| DEFINE_AGENT (macro) | macro | include/agenticdsl/pdk/agent_macros.h | PDK Agent 循环宏 (React MVP, PlanExecute/ForkJoin Phase 2) |
| SafeExec | class | include/agenticdsl/pdk/safe_exec.h | PDK 沙箱执行封装 (超时+异常 MVP) |

## CONVENTIONS
- **2 空格缩进**，中文注释（避免中英混杂）
- **命名规范**：CamelCase 类名/结构体，snake_case 变量，SCREAMING_SNAKE_CASE 宏
- **文件头注释**：功能描述、作者、日期
- **CMake**：每个模块独立 CMakeLists.txt，最终聚合成 agenticdsl_core

## ANTI-PATTERNS (THIS PROJECT)
- **禁止** `include_directories()` 全局包含 → 应用 `target_include_directories()`
- **禁止** `link_directories()` → 应在 CMake target_link_libraries 中指定完整路径
- **禁止** `as any` / `@ts-ignore` 类型压制
- **禁止** 空 catch 块 `catch(e) {}`
- **禁止** 删除失败的测试来"通过"

## BUILD SYSTEM
- CMake 3.20+，C++20
- 根 `CMakeLists.txt` 聚合 10 个模块静态库 → `agenticdsl_core`
- 构建：`./build.sh` 或 `mkdir build && cd build && cmake .. && make -j$(nproc)`
- 测试：`cmake .. -DAGENTICDSL_BUILD_TESTS=ON && make && ctest --output-on-failure`

## NOTES
- `engine.h` 直接 `#include` 6 个模块头文件 (scheduler/topo_scheduler, parser/markdown_parser, budget/budget_controller, common/llm/llm_types, common/llm/mock_provider, common/tools/registry) — ADR-0019 §1.4 已识别为问题待解, Stage 4 (core-interface-inversion) 处理
- **2026-06-17 更新**: ADR-0019 §1.4 已解决. Sprint 1b (commit `248d209`) 吸收 3 deep modules/ 移除 (topo_scheduler.h / markdown_parser.h / budget_controller.h 通过 PIMPL-lite). 剩余 4 跨模块 include (3 common/ + 1 modules/trace/) 由 OpenSpec change [`2026-06-15-residual-engine-h-decoupling`](openspec/changes/2026-06-15-residual-engine-h-decoupling/) 处理 (P1 active, 估时 3 周). `engine.h` 当前保留 `common/llm/llm_types.h` (types 头文件例外).
- **2026-06-18 更新**: P1.T3 完成 (commit `01666fa`). `engine.h` 第 47 行 `modules/trace/trace_exporter.h` → `agenticdsl/types/trace_record.h` (TraceRecord data-only struct 上移到 include/agenticdsl/types/). 跨模块 include 计数 4 → 3 (仅剩 3 common/). 27/27 测试零回归. 剩余 3 个 common/ include 由 T1 (LLMProviderFactory 从零构建 5-7d) + T2 (IToolRegistry 7 虚函数 5d) 处理, 完成后将达到 ADR-0019 §1.4 完全退出标准 (1 个 modules/common include).
- **2026-06-18 P1 全部 ship (17 commits, T1+T2+T3+T4+T5)**: `engine.h` 跨模块 include 计数 **2→1** (仅 `common/llm/llm_types.h` types 例外). 29/29 测试零回归 (含新增 test_provider_factory + test_tool_registry_interface). ADR-0019 §1.4 状态更新为 ✅ **已解决**. OpenSpec change `2026-06-15-residual-engine-h-decoupling` 准备 archive. 关键实施: IProviderFactory + LLMProviderFactory 路由 (P1.T1) + IToolRegistry 9 虚函数 (P1.T2) + SecureToolRegistry 委托多继承 (P1.T2 v3 修订) + engine.h PIMPL-lite tool_registry_ (P1.T4) + NodeExecutor/TopoScheduler/ExecutionSession ToolRegistry&→IToolRegistry& (P1.T4 依赖倒置).
- **2026-06-19 (Sprint 3 DomainWorkerPool 实施, OpenSpec change `2026-06-30-domain-worker-pool`)**: 新增 `include/agenticdsl/cognitive/domain_worker_pool.h` + `src/modules/cognitive/domain_worker_pool.cpp` (N 个 std::jthread worker + 共享 FIFO 任务队列多消费者模式 + shared_mutex 保护 handler 注册表 + IInteractionBus 集成, 构造双重重载, CP.22 协议 6/6 项通过, 异常隔离 try-catch + catch(...), 队列排空策略). 新增 `tests/test_domain_worker_pool.cpp` 7 个 test case (默认构造 / submit 派发 / 1000x 并发 / 异常隔离 / shutdown 等待 in-flight / graceful vs forced shutdown / bus 集成). 新增 `Dockerfile.tsan` 容器化 TSan 验证 (ubuntu:22.04 + gcc-13 + ASLR=0). 31/31 ctest pass (30 baseline + 1 new test_domain_worker_pool w/ 94 assertions), 零回归. ADR-0020 §2.2.1 状态从 🟡 Partial → ✅ **Resolved**, §3.2 标记为"已实施". OpenSpec change `2026-06-30-domain-worker-pool` 准备 archive.
- **2026-06-19 (Sprint 4 PDK 骨架 实施, OpenSpec change `2026-07-07-pdk-skeleton`)**: 新增 `include/agenticdsl/pdk/{tool_macros,agent_macros,safe_exec,pdk}.h` (DECLARE_TOOL 宏 + DEFINE_AGENT 模板 (React MVP) + SafeExec 封装 (超时+异常 MVP) + PDK 统一入口). 新增 `pdk/CMakeLists.txt` monorepo 子目录 (INTERFACE 库 `hydraforge_pdk`). 根 CMakeLists.txt 添加 `add_subdirectory(pdk)`. 新增 `tests/test_pdk_macros.cpp` 5 个 test case (DECLARE_TOOL 展开 / DEFINE_AGENT 实例化 / SafeExec 超时 / SafeExec 异常 / PDK Runtime 解耦). 32/32 ctest pass (31 baseline + 1 new test_pdk_macros w/ 33 assertions), 零回归. ADR-0021 状态从 🔍 Proposed → 🟡 **Partial**, P3 静态链接验证 (PDK 头文件仅依赖 `agenticdsl/contract/*.h`). OpenSpec change `2026-07-07-pdk-skeleton` 准备 archive. T4b (`hydraforge-pdk` 独立仓库推送) 留 Sprint 4 ship 后异步 (外部阻塞: GitHub 组织存在性).
- **2026-06-19 (Sprint 4 T4b Dual-Repo 治理)**: PDK 采用 Dual-Repo 策略 (Option C: vendored + 单独发布仓库, **非 submodule**). 新增 `scripts/sync-pdk.sh` (~17KB) 自动化同步脚本 (preflight + 文件拷贝 + README 生成 + commit/push + standalone 构建验证). ADR-0021 §7 追加 Dual-Repo Policy (vendored in monorepo `include/agenticdsl/pdk/` + 单独发布仓库 `github.com/chisuhua/hydraforge-pdk`). 触发时机: 每个 Sprint ship 后 / PDK 头文件 API 变更 / 紧急 patch. 路径映射: monorepo `agenticdsl/pdk/` ↔ standalone `hydraforge/pdk/` (API 兼容). 外部消费者通过 `find_package(hydraforge_pdk 0.1 REQUIRED)` 接入. 零内部摩擦 (vs submodule 需要 init), 保留独立版本演进 (sync script).
- **2026-06-18 Sprint 2 CognitiveWorker 实施 (OpenSpec change `2026-06-23-cognitive-worker`)**: 新增 `include/agenticdsl/cognitive/cognitive_worker.h` + `src/modules/cognitive/cognitive_worker.cpp` (per-agent 隔离, 状态机 idle/running/stopped, 构造签名 `(unique_ptr<DSLEngine>, shared_ptr<IInteractionBus>)`, F7 构造时强制注入 bus, TD-CW-02 析构 out-of-line 隐式 stop()+join, TD-CW-03 错误码 bridge 覆盖 9 处 legacy string). 新增 `tests/test_cognitive_worker.cpp` 9 个 test case (生命周期 / 任务提交 / 错误传播 / 并发 10×100 / 状态机 / bridge / F7 / 析构安全). 30/30 ctest pass (29 baseline + 1 new test_cognitive_worker w/ 33 assertions), 零回归. ADR-0020 §2.2.2 新增, §3.1 V3.2 修正, ADR-0019 状态变更日志追加. OpenSpec change `2026-06-23-cognitive-worker` 准备 archive.
- `lib/` 目录存放 `.md` DSL 文件，非 C++ 库
- `src/modules/exports/` 存放导出类型定义
- `src/common/contract/` ADR-0019 契约层 (IInteractionBus, InMemoryBus) — 与 `include/agenticdsl/contract/` 头文件配套
- `llm_config.json` 运行时 LLM 配置（模型路径、温度等）
- `.clang-format` / `.clang-tidy` 存在 (项目根)
- 构建预设: `CMakePresets.json` (cmake --preset debug|release|asan|tsan|tests)
- `compile_commands.json` 根目录软链接 (Stage 5 / Task 23, 指向 `build/compile_commands.json`)
- GitHub Actions CI: `.github/workflows/ci.yml` (Stage 5 / Task 25, 2 presets × 2 compilers matrix)

## Recent Changes
- 2026-06-26 (Sprint 11 启动 / C0 doc-alignment 收官): OpenSpec change `2026-06-26-doc-alignment-adr-states` ship — 4 处文档/ADR 状态同步: ① 新建 `docs/adr/adr-0030-async-runtime-v2.md` (状态 🔍 Proposed, Phase 2 异步架构, 基于 Slice 00 ship + Sprint 2/3 std::jthread 验证, V2 替代归档的 V1); ② ADR-0030 V1 (`docs/archive/adr/adr-0030-async-runtime-dual-layer.md`) 标记 SUPERSEDED by V2; ③ ADR-0032 (`docs/archive/adr/adr-0032-cost-collector.md`) 状态 ❌ Not Implemented → 🟡 Partial (`tests/test_cost_collector.cpp` 已 ship, 2026-06-14); ④ `docs/implementation-roadmap.md` §Phase 2 ADR 引用 V1 → V2 + ADR-0032 状态更新 + `docs/roadmap-status.md` §Phase 2 行 `⏸ 阻塞中` → `⏸ 待启动 (Sprint 12, 依赖 C0+C1)` + §四 实施日志 2026-06-26 行. C0 ship gate 全部通过 (`adr_lint.py` exit 0, `docs_drift_audit.py` 0 critical drift, `openspec validate` exit 0). OpenSpec change 准备 archive.
- 2026-06-26 (Roadmap-Driven Development 启用): Master plan `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` 创建 (590 行, 含 §9-§13 Review Gates), 9 个 OpenSpec changes 规划 (C0+C1 immediate + C2-C8 placeholder), 2 个 skills 创建 (`master-plan-driven-changes` + `open-spec-placeholder-fill`), 1 个工具创建 (`tools/check_roadmap_drift.py`, 4 类 drift 检测, 当前检测到 2 个 CRITICAL drift 待 C0 修复). Sprint 收官 checklist 加入 Drift Detection 强制项 (§六.6.2), `scripts/sprint-closeout.sh` wrapper 创建。
- 2026-06-26 (Sprint 10 收官): OpenSpec change `2026-06-25-pre-existing-sanitizer-findings` shipped — 2 pre-existing sanitizer 问题全部修复: (1) P1 `test_cognitive_worker` ASan/TSan `stack-use-after-scope` → `std::thread`→`std::jthread` RAII 替换 (commit `d69e2d9`, +20/-10); (2) P2 `test_domain_worker_pool` 12 TSan warnings → Strategy A `std::atomic<bool>` flag post-check 消除 Catch2 framework 数据竞争 (commit `0c44a18`, +34/-13)。Ship gate: ctest 34/34 PASS + ASan 34/34 + TSan 34/34 (0 errors/warnings)。P2.1 调查产出 `docs/audits/p2-tsan-investigation.md` (427 行)。Audit report `docs/audits/2026-06-25-sanitizer-revalidation.md` 追加 §7 Sprint 10 修复后结果。roadmap-status.md ASan/TSan 表更新为 34/34 (100%)。OpenSpec change archived。
- 2026-06-25: `2026-06-24-engine-include-final-decoupling` shipped — engine.cpp cross-module includes 10→3 (commits `e7306d9` + `18ce4aa` + `8f2ad54` + `a8abc35` + review fix `a8abc35`), 34/34 ctest pass (新增 15 测试: 7 scheduler `b3ad5bc` + 5 parser `4d1a855` + 3 engine_factory `3681ba8`), pre-existing ASan/TSan findings documented in `docs/roadmap-status.md` (不阻塞 archive), 4-change archive chain closed (`tech-debt-cleanup-sprint-6` → `sprint-9-handle-node-completion` → `2026-06-24-engine-include-final-decoupling` → `tech-debt-and-phase1-closure`), Sprint 10 starts with 0 active OpenSpec changes.
- 2026-06-25 (P2.5 ship gate 复验): `cmake --preset asan -DAGENTICDSL_BUILD_TESTS=ON && ctest` **33/34 PASS** (97%), `cmake --preset tsan -DAGENTICDSL_BUILD_TESTS=ON && ctest` **32/34 PASS** (94%)。2 pre-existing 失败由独立 OpenSpec change `2026-06-25-pre-existing-sanitizer-findings` 跟踪: (1) `test_cognitive_worker` (Sprint 2, `stack-use-after-scope` at test_cognitive_worker.cpp:226, 修复策略: `std::thread` → `std::jthread` C++20 RAII 替换); (2) `test_domain_worker_pool` (Sprint 3, 12 TSan warnings 但 94 assertions 全 PASS, Catch2 framework + std::jthread 已知交互, 产品代码 `DomainWorkerPool` Sprint 3 ship 时 18/18 InMemoryBus 并发断言无 data race 验证已通过, 决策: 文档化非修复)。优雅降级依据: `engine-include-decoupling` spec §"sanitizer-revalidation / 历史 race/leak 优雅降级" Scenario (openspec v1.4.1 per-machine spec registry 维护,非 git-tracked)。Sprint 10 ship gate **PASS** (per pre-existing tracking change 跟踪)。同时完整 ship gate 验证报告 `docs/audits/2026-06-25-sanitizer-revalidation.md` 创建 + `docs/superpowers/plans/2026-06-24-engine-include-final-decoupling.md` plan 文档 commit (895 行, commit `41440c8`) + 2 active 跟踪 plan 目录 README 对账。
- 2026-06-24 (Sprint 5 收官 + 6.3.x 全关闭): OpenSpec change `tech-debt-and-phase1-closure` ship, Phase 1 智能体层 80% → 100%。5 ADR (0019/0020/0021/0022/0023) → ✅ Approved (2026-06-24, Sprint 5 ship)。Sprint 5 S5.T3 phase1_plugin_demo 3 mode CLI (commit `10dc028`)。Sprint 10 起点零 OpenSpec backlog (剩 plugin-loader archive 待执行)。
- 2026-06-22 (Sprint 6 STATUS NOTE 决议): Oracle 深度审查 (session `ses_112a9f9c5ffesqpYeefOBgMkjH`) 决议 — 4 个代码 commit (`7cc4239` / `6c5557c` / `9fa0364` / `7923b2a`) **保持合入不回退** (33/33 ctest pass, 行为保持), **不 archive OpenSpec change**, 偏离 spec 验收项全部推迟到 Sprint 7 follow-up。Top 修复: 🔴 scheduler fork 重复 (`topo_scheduler.cpp:636-642` 死分支) + 🟠 scheduler factory 死代码 (零调用) + 🟠 execute 222 行 vs spec ≤60 + 🟠 engine.cpp 10 include vs spec ≤3 + 🔴 15 个新测试 (7 scheduler + 5 parser + 3 factory) 0 交付。详细偏离表见 `openspec/changes/tech-debt-cleanup-sprint-6/tasks.md` §6.1, Sprint 7 follow-up 见 §6.3。新增 OpenSpec change `2026-07-22-sprint-7-tech-debt-followup` 跟踪。
- 2026-06-21 (commit `fb0e118`): Sprint 6 tech-debt-cleanup OpenSpec change artifacts 提交 (proposal/design/tasks/3 specs)
- 2026-06-20 (commit `7cc4239`): refactor(core): engine.cpp 工厂化, 2/10 跨模块 include 替换 (Sprint 6 P2-7) — **LIMFALL**: 实际计数未降 (10→10), 0 factory 测试, scheduler factory 死代码
- 2026-06-20 (commit `6c5557c`): refactor(parser): introduce NodeFactoryRegistry, eliminate 216行 if-else in create_node_from_json (Sprint 6 P2-6) — **LIMFALL**: 11 NodeType 一一对应零丢失 ✓, 但 0 parser 测试, `has_factory` 预检使 throw 路径成死分支
- 2026-06-20 (commit `9fa0364`): refactor(scheduler): split execute() 308行 → orchestration + 3 subfunctions (Sprint 6 P1-4) — **LIMFALL**: 行为保持 ✓, 但 execute 222 行 vs spec ≤60, 2/3 函数命名不符, fork 处理逻辑被复制两处, 0 scheduler 测试
- 2026-06-20 (commit `7923b2a`): test(plugin): add 7 state-based test cases for PluginLoader (Sprint 6 P1-5) — **LIMFALL**: 7 case 名称/范围与 spec 点名不符, E2E `TEST_PLUGIN_FIXTURE_PATH` 宏未注入, Loaded 状态零覆盖
- 2026-06-14 (commit `451e395`): 修复 3 个失败测试（test_layered_context / test_path_policy / test_secure_tool_registry），全部 25 个测试 100% 通过。核心修复：`flatten` 重命名为 `flatten_layers` 并修复 merge lambda 逻辑、`PathPolicy` 对 `allowed_prefixes` 同样做 `weakly_canonical`、`ShellGuard` 增加 `"| sh"` 模式
- 2026-06-09 (commit `ac9e684`): 删除 `src/modules/prompts.yaml`（LLM prompt 模板改由各模块硬编码或 `llm_config.json` 管理）

<!-- code-review-graph MCP tools -->
## MCP Tools: code-review-graph

**IMPORTANT: This project has a knowledge graph. ALWAYS use the
code-review-graph MCP tools BEFORE using Grep/Glob/Read to explore
the codebase.** The graph is faster, cheaper (fewer tokens), and gives
you structural context (callers, dependents, test coverage) that file
scanning cannot.

### When to use graph tools FIRST

- **Exploring code**: `semantic_search_nodes` or `query_graph` instead of Grep
- **Understanding impact**: `get_impact_radius` instead of manually tracing imports
- **Code review**: `detect_changes` + `get_review_context` instead of reading entire files
- **Finding relationships**: `query_graph` with callers_of/callees_of/imports_of/tests_for
- **Architecture questions**: `get_architecture_overview` + `list_communities`

Fall back to Grep/Glob/Read **only** when the graph doesn't cover what you need.

### Key Tools

| Tool | Use when |
| ------ | ---------- |
| `detect_changes` | Reviewing code changes — gives risk-scored analysis |
| `get_review_context` | Need source snippets for review — token-efficient |
| `get_impact_radius` | Understanding blast radius of a change |
| `get_affected_flows` | Finding which execution paths are impacted |
| `query_graph` | Tracing callers, callees, imports, tests, dependencies |
| `semantic_search_nodes` | Finding functions/classes by name or keyword |
| `get_architecture_overview` | Understanding high-level codebase structure |
| `refactor_tool` | Planning renames, finding dead code |

### Workflow

1. The graph auto-updates on file changes (via hooks).
2. Use `detect_changes` for code review.
3. Use `get_affected_flows` to understand impact.
4. Use `query_graph` pattern="tests_for" to check coverage.
