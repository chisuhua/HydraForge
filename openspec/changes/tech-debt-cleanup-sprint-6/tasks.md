# Tasks: Tech Debt Cleanup Sprint 6

> **变更类型**: 真实实现 (3 Sprint, ~3 周总工时)
> **关联 plan**: `.omo/plans/tech-debt-cleanup-sprint-6.md` (本文件即 plan)
> **关联 ADR**: 0020/0021/0022 (状态同步), 0019 §1.4 (engine.h 解耦延续)
> **关联 change**: `openspec/changes/tech-debt-cleanup-sprint-6/`
> **创建日期**: 2026-06-21 (综合健康审计产出)
> **修订说明**: 与 design.md 3 个 Decision 一一对应,按 Sprint 分组便于并行跟踪

---

## 1. Sprint 6a (W1) - P0 清理 + plugin_loader 测试增强

### 1.1 plugin_loader WIP 提交

- [ ] 1.1.1 检查 `git status` 列出 7 个 untracked 文件
- [ ] 1.1.2 验证文件清单: `include/agenticdsl/plugin/plugin_info.h` + `plugin_loader.h` + `src/modules/plugin/plugin_loader.cpp` + `src/modules/plugin/CMakeLists.txt` + `tests/test_plugin_loader.cpp` + `openspec/changes/2026-07-14-plugin-loader/design.md` + `specs/plugin-loader/spec.md`
- [ ] 1.1.3 `git add` 上述 7 个文件 + 修改的 CMakeLists.txt
- [ ] 1.1.4 更新 `openspec/changes/2026-07-14-plugin-loader/tasks.md`: S5.T1.1 + S5.T1.2 + S5.T2 标 `[x]` (3 个 task)
- [ ] 1.1.5 提交: `git commit -m "wip(plugin): Sprint 5 S5.T1+T2 (PluginInfo POD + PluginLoader skeleton + dlopen implementation)"`
- [ ] 1.1.6 验证: `git status` clean, `git log --oneline -1` 显示 WIP commit

### 1.2 yaml_json 死注释清理

- [ ] 1.2.1 `grep -n "DEBUG-removed" src/common/utils/yaml_json.cpp` 确认 2 处
- [ ] 1.2.2 编辑删除 line 72 + line 75 的 2 个 `// [DEBUG-removed] ...` 注释
- [ ] 1.2.3 验证: `grep "DEBUG-removed" src/` 返回 0 命中
- [ ] 1.2.4 `cmake --build build` + `ctest --output-on-failure` 确认 32/32 pass
- [ ] 1.2.5 提交: `git commit -m "chore(utils): remove 2 [DEBUG-removed] dead comments in yaml_json.cpp (audit 2026-06-09 P1-2 残留)"`

### 1.3 ADR 状态同步

- [ ] 1.3.1 编辑 `docs/adr/adr-0020-thread-model-isolation.md` 顶部: 状态 → `✅ Resolved (2026-06-19)` + 引用本 change
- [ ] 1.3.2 编辑 §2.2.1 + §3.2: 标 `Resolved` + commit hash (`aa54605`)
- [ ] 1.3.3 编辑 `docs/adr/adr-0021-pdk-design.md` 顶部: 状态 → `🟡 Partial (2026-06-19 Sprint 4 ship)`
- [ ] 1.3.4 更新 §3 实施状态,引用 Sprint 4 commits (`2d7126c`/`2ca57de`/`fe17979`/`70526d9`/`d5b295e`)
- [ ] 1.3.5 编辑 `docs/adr/adr-0022-plugin-loading.md` 顶部: 状态 → `🟡 Partial (Sprint 5 in-flight)`
- [ ] 1.3.6 更新 §1.2 PluginInfo POD 状态 + §4 dlopen 实现状态
- [ ] 1.3.7 编辑 `docs/README.md` ADR 表格 3 行: 同步状态字段
- [ ] 1.3.8 验证: `grep -E "✅ Resolved|🟡 Partial|🔍 Proposed" docs/README.md docs/adr/adr-002{0,1,2}*.md` 与设计一致
- [ ] 1.3.9 提交: `git commit -m "docs(adr+status): sync ADR-0020/0021/0022 status to code reality (Sprint 6)"`

### 1.4 plugin_loader 测试增强 (≥ 7 个 case)

- [ ] 1.4.1 打开 `tests/test_plugin_loader.cpp`, 添加 7 个新 TEST_CASE 框架
- [ ] 1.4.2 实现 `load_valid_plugin`: mock 一个返回 PluginInfo 的 .so, 验证注册成功
- [ ] 1.4.3 实现 `abi_version_mismatch_strict`: mock abi_version=0 (vs CURRENT=1), strict=true, 期望返回 false
- [ ] 1.4.4 实现 `abi_version_mismatch_non_strict`: 同上 strict=false, 期望返回 true + warn 日志
- [ ] 1.4.5 实现 `dlsym_missing_register_fn`: mock 缺 `pdk_register_tools` 符号, 期望返回 false + dlerror 日志
- [ ] 1.4.6 实现 `dlopen_failure_invalid_path`: 不存在的路径, 期望返回 false
- [ ] 1.4.7 实现 `unload_all_raii_verification`: scope guard, 析构时 dlclose 被调用
- [ ] 1.4.8 实现 `load_all_search_paths`: 设置临时目录 + 2 个 mock plugin, 验证都加载
- [ ] 1.4.9 `ctest -R test_plugin_loader --output-on-failure` 验证 7 个新 case 全 pass
- [ ] 1.4.10 提交: `git commit -m "test(plugin): add 7 test cases for PluginLoader (ABI mismatch / dlsym / path / RAII / scan) (Sprint 6 P1-5)"`

### 1.5 Sprint 6a 收尾

- [ ] 1.5.1 全量回归: `ctest --output-on-failure` ≥ 39/39 (32 baseline + 7 plugin)
- [ ] 1.5.2 TSan 矩阵: `cmake --preset tsan && ctest -R plugin_loader` 0 race
- [ ] 1.5.3 `python3 tools/adr_lint.py docs/adr/` exit 0
- [ ] 1.5.4 更新 `AGENTS.md` 顶部状态日志追加 Sprint 6a 完成

---

## 2. Sprint 6b (W2) - P1-4 scheduler 三段式拆分

### 2.1 DagState 结构定义

- [ ] 2.1.1 在 `src/modules/scheduler/topo_scheduler.h` 新增 `struct DagState` (含 `std::unordered_map<NodeId, NodeExecutionStatus>` + `std::queue<NodeId> ready_queue` + `int pending_count`)
- [ ] 2.1.2 添加 `struct NodeExecutionStatus { NodeId id; NodeStatus status; NodeResult result; int indegree; std::vector<NodeId> dependents; }`
- [ ] 2.1.3 验证编译通过

### 2.2 prepare_dag_state 子函数实现

- [ ] 2.2.1 在 `topo_scheduler.h` 声明 `private DagState prepare_dag_state(const ParsedGraph& graph, Context& ctx);`
- [ ] 2.2.2 在 `topo_scheduler.cpp` 实现: 解析 graph → 计算 indegree → 拓扑排序 → 填充 ready_queue
- [ ] 2.2.3 处理循环依赖: 拓扑排序失败时 throw `std::runtime_error("DAG cycle detected")`
- [ ] 2.2.4 函数体内注释 > 5 行的算法必须有中文说明
- [ ] 2.2.5 `cmake --build build` 编译通过

### 2.3 dispatch_ready_nodes 子函数实现

- [ ] 2.3.1 声明 `private size_t dispatch_ready_nodes(DagState& state, ExecutionSession& session);`
- [ ] 2.3.2 实现: 遍历 ready_queue → 调用 `session.execute_node(node)` → 更新 state.status = Running
- [ ] 2.3.3 返回成功派发的节点数
- [ ] 2.3.4 处理 session 抛异常: catch + 标记节点 Failed + 传播到 dependents

### 2.4 handle_node_completion 子函数实现

- [ ] 2.4.1 声明 `private bool handle_node_completion(DagState& state, const NodeResult& result);`
- [ ] 2.4.2 实现: 更新 result → 遍历 dependents → 减 indegree → 减至 0 时 push ready_queue
- [ ] 2.4.3 失败传播: 若 result.status == Failed, 所有 dependents 标记为 Skipped
- [ ] 2.4.4 返回 `pending_count == 0` (调度完成判定)

### 2.5 execute() 编排层重写

- [ ] 2.5.1 重写 `TopoScheduler::execute()`, 仅保留: prepare → 循环 dispatch + handle → 返回 ExecutionResult
- [ ] 2.5.2 函数体 ≤ 60 行 (用 `awk '/^ExecutionResult TopoScheduler::execute/,/^}$/' | wc -l` 验证)
- [ ] 2.5.3 `execute_single_branch` MUST NOT 修改（保留 118 行原样）
- [ ] 2.5.4 `ctest -R test_scheduler --output-on-failure` 既有测试零回归

### 2.6 scheduler 测试增强 (≥ 7 个 case)

- [ ] 2.6.1 在 `tests/test_scheduler.cpp` 添加 7 个新 TEST_CASE 框架
- [ ] 2.6.2 `prepare_dag_state_simple_linear`: 3 节点线性 DAG, 验证 ready_queue 顺序
- [ ] 2.6.3 `prepare_dag_state_diamond`: 4 节点菱形, 验证拓扑序正确
- [ ] 2.6.4 `prepare_dag_state_cycle_detection`: A→B→A 循环, 期望 runtime_error
- [ ] 2.6.5 `dispatch_ready_nodes_initial`: 初始 ready 队列派发 2 节点
- [ ] 2.6.6 `dispatch_ready_nodes_parallel`: 3 个独立节点并发派发
- [ ] 2.6.7 `handle_node_completion_success`: 完成后下游节点入 ready_queue
- [ ] 2.6.8 `handle_node_completion_failure`: 失败传播, 2 个 downstream 标 Skipped
- [ ] 2.6.9 `ctest -R test_scheduler --output-on-failure` ≥ 7 新 case pass

### 2.7 Sprint 6b 收尾

- [ ] 2.7.1 全量回归 ≥ 39/39 ctest pass
- [ ] 2.7.2 TSan + ASan 矩阵验证 scheduler 无 race / leak
- [ ] 2.7.3 Hub 出度验证: `code-review-graph get_hub_nodes --top_n 5` 验证 `topo_scheduler::execute` out_degree < 30
- [ ] 2.7.4 提交: `git commit -m "refactor(scheduler): split execute() 308行 → 3子函数 + 7 tests (Sprint 6 P1-4)"`
- [ ] 2.7.5 更新 `AGENTS.md` 顶部状态日志追加 Sprint 6b

---

## 3. Sprint 6c (W3) - P2-6 + P2-7 架构层改进

### 3.1 NodeFactoryRegistry 类实现

- [ ] 3.1.1 新建 `include/agenticdsl/parser/node_factory.h` (~80 行)
- [ ] 3.1.2 定义 `class NodeFactoryRegistry` 含 shared_mutex + unordered_map + 4 个公共方法 + global() 单例
- [ ] 3.1.3 文件头注释: 功能描述 + ADR + 作者 + 日期
- [ ] 3.1.4 新建 `src/modules/parser/node_factory.cpp` 实现 register / create / has_factory / size / global()
- [ ] 3.1.5 global() 单例用 Meyers singleton (函数内 static), 线程安全
- [ ] 3.1.6 CMakeLists.txt 注册新源文件 (parser 模块)
- [ ] 3.1.7 `cmake --build build` 编译通过

### 3.2 13 个 NodeType factory 函数迁移

- [ ] 3.2.1 在 `src/modules/parser/markdown_parser.cpp` 末尾添加静态注册块: `static bool _ = [] { ... }();`
- [ ] 3.2.2 提取每个 NodeType 的构造逻辑为独立 factory 函数: `make_llm_node` / `make_tool_node` / `make_http_node` / `make_branch_node` / `make_loop_node` / `make_parallel_node` / ...
- [ ] 3.2.3 验证注册数: `grep -c "register_factory" src/modules/parser/markdown_parser.cpp` ≥ 13
- [ ] 3.2.4 每个 factory 函数保留原构造逻辑, 仅签名变化 `json -> unique_ptr<Node>`

### 3.3 create_node_from_json 重构

- [ ] 3.3.1 重写 `MarkdownParser::create_node_from_json` 为 `return NodeFactoryRegistry::global().create(node_type, spec);`
- [ ] 3.3.2 函数体 ≤ 30 行 (含参数检查 + 异常处理)
- [ ] 3.3.3 保留 throw on unknown type 行为: `if (!registry.has_factory(type)) throw std::runtime_error("Unknown NodeType: " + ...);`
- [ ] 3.3.4 `ctest -R test_parser --output-on-failure` 既有测试零回归

### 3.4 NodeFactoryRegistry 测试 (≥ 5 个 case)

- [ ] 3.4.1 在 `tests/test_parser.cpp` 添加 5 个新 TEST_CASE
- [ ] 3.4.2 `factory_registry_registers_all_types`: size() == 13
- [ ] 3.4.3 `factory_registry_creates_correct_subtype`: typeid 检查返回正确子类
- [ ] 3.4.4 `factory_registry_unknown_type_throws`: type = 999, 期望 runtime_error
- [ ] 3.4.5 `factory_registry_global_singleton`: 两次 global() 调用返回同一地址
- [ ] 3.4.6 `factory_registry_concurrent_access`: 4 线程并发 create + 1 线程 register, TSan 0 race
- [ ] 3.4.7 `cmake --preset tsan && ctest -R test_parser` 全 pass

### 3.5 engine.cpp 工厂函数实现 (3 个)

- [ ] 3.5.1 新建 `src/modules/scheduler/factory.h` 声明 `agenticdsl::scheduler::create(const SchedulerConfig&) -> std::unique_ptr<IScheduler>`
- [ ] 3.5.2 新建 `src/common/llm/factory.h` 声明 `agenticdsl::llm::create_provider_factory(const LLMConfig&) -> std::unique_ptr<IProviderFactory>`
- [ ] 3.5.3 新建 `src/modules/budget/factory.h` 声明 `agenticdsl::budget::create_controller(const BudgetConfig&) -> std::unique_ptr<BudgetController>`
- [ ] 3.5.4 实现 3 个工厂函数体 (返回 make_unique 对应类型)
- [ ] 3.5.5 CMakeLists.txt 各自模块注册新 factory 源文件
- [ ] 3.5.6 `cmake --build build` 编译通过

### 3.6 engine.cpp 工厂调用迁移

- [ ] 3.6.1 打开 `src/core/engine.cpp`, 替换 10 个跨模块 include 为 3 个 contract 头
- [ ] 3.6.2 替换 `make_unique<TopScheduler>(...)` 为 `agenticdsl::scheduler::create(...)`
- [ ] 3.6.3 替换 `make_unique<BudgetController>(...)` 为 `agenticdsl::budget::create_controller(...)`
- [ ] 3.6.4 替换 `make_unique<LLMProviderFactory>(...)` 为 `agenticdsl::llm::create_provider_factory(...)`
- [ ] 3.6.5 替换 `make_unique<MockLLMProvider>(...)` 为 `agenticdsl::llm::create_mock_provider()` (新增 fallback factory)
- [ ] 3.6.6 验证: `grep -c '^#include' src/core/engine.cpp` 跨模块 ≤ 3

### 3.7 engine.cpp 工厂测试 (≥ 3 个 case)

- [ ] 3.7.1 新建 `tests/test_engine_factory.cpp`
- [ ] 3.7.2 `test_scheduler_create_default_config`: 验证默认 SchedulerConfig 构造正确
- [ ] 3.7.3 `test_budget_create_default_config`: 验证默认 BudgetConfig 构造正确
- [ ] 3.7.4 `test_provider_factory_create_llm_config`: 验证 LLMConfig 路由到正确 provider
- [ ] 3.7.5 `ctest -R test_engine_factory --output-on-failure` ≥ 3 pass

### 3.8 Sprint 6c 收尾

- [ ] 3.8.1 全量回归 ≥ 47/47 ctest pass (32 baseline + 7 plugin + 7 scheduler + 5 parser + 3 factory)
- [ ] 3.8.2 TSan + ASan 全矩阵验证
- [ ] 3.8.3 Hub 出度验证: `code-review-graph get_hub_nodes --top_n 5` 验证 `create_node_from_json` out_degree < 30
- [ ] 3.8.4 engine.cpp include 验证: `grep -c "modules/\|common/" src/core/engine.cpp` ≤ 3
- [ ] 3.8.5 `python3 tools/adr_lint.py docs/adr/` exit 0
- [ ] 3.8.6 提交: `git commit -m "refactor(parser+engine): NodeFactoryRegistry + factory.cpp migration (Sprint 6 P2-6 + P2-7)"`
- [ ] 3.8.7 更新 `AGENTS.md` 顶部状态日志追加 Sprint 6c

---

## 4. 文档同步与归档

- [ ] 4.1 更新 `docs/roadmap-status.md` 追加 Sprint 6 完成状态
- [ ] 4.2 更新 `docs/implementation-roadmap.md` 数字
- [ ] 4.3 创建 `docs/audits/2026-07-21-tech-debt-sprint-6.md` 记录本 Sprint 决议
- [ ] 4.4 更新 `AGENTS.md` 顶部状态日志 (Sprint 6a/b/c 完成时间)
- [ ] 4.5 验证 `openspec validate tech-debt-cleanup-sprint-6` exit 0
- [ ] 4.6 `openspec archive tech-debt-cleanup-sprint-6 --yes` (前提: 所有 task 100% 完成 + ctest 47/47 + ADR lint 0)
- [ ] 4.7 同步 PDK 头文件: `./scripts/sync-pdk.sh` (Sprint 6 涉及 factory.h, 双仓库需同步)

---

## 5. 验证检查清单 (Sprint 6 ship gate)

- [ ] 5.1 `cd build && ctest --output-on-failure` ≥ 47/47 PASS
- [ ] 5.2 `cmake --preset tsan && ctest --output-on-failure` 0 race report
- [ ] 5.3 `cmake --preset asan && ctest --output-on-failure` 0 leak report
- [ ] 5.4 `python3 tools/adr_lint.py docs/adr/` exit 0
- [ ] 5.5 `python3 tools/docs_drift_audit.py` 0 critical drift
- [ ] 5.6 `openspec validate tech-debt-cleanup-sprint-6` exit 0
- [ ] 5.7 `code-review-graph get_hub_nodes --top_n 5` 验证 2 个目标函数 out_degree < 30
- [ ] 5.8 `git status` clean, 5-7 个 commit 按 Sprint 分组
- [ ] 5.9 AGENTS.md 状态日志包含 Sprint 6 完成标记
- [ ] 5.10 PDK 同步脚本无 error (`./scripts/sync-pdk.sh --dry-run`)