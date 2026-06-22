# Design: Sprint 7 Tech-Debt Follow-up

> **范围来源**: Oracle 审查报告 session `ses_112a9f9c5ffesqpYeefOBgMkjH` (2026-06-22) 对 `tech-debt-cleanup-sprint-6` 4 commit (`7cc4239` / `6c5557c` / `9fa0364` / `7923b2a`) 的偏离项审计。
> **目标**: 闭环 `tech-debt-cleanup-sprint-6/tasks.md` §6.3 所有推迟项, 让 Sprint 6 最终 archive。

## Decision 1: 修 fork 重复 — 删除 `dispatch_next_node` 内的 fork 块 (NOT `execute()` 内)

**方案对比**:
- **A. 删 `dispatch_next_node()` 内 fork 块 (`topo_scheduler.cpp:636-642`)** ← 推荐
  - 理由: `execute()` 是入口层, fork 处理必须在此层 (入口即调度分支); `dispatch_next_node` 接收时 `is_executing_fork_branches_` 已被 `execute()` 置 false, 故此处块为死代码
  - 验证: ctest 33/33 pass + git diff 显示单边修改
- B. 删 `execute()` 内 fork 块 (`topo_scheduler.cpp:161-167`)
  - 拒绝理由: 破坏入口层语义, fork 是入口决策不属于派发循环

**实施**: 单 commit `fix(scheduler): remove duplicated fork handling in dispatch_next_node`, 1 行编辑 + 注释更新。

## Decision 2: scheduler factory 复活 vs 删除

**方案 A (推荐)**: 补 `Config` 参数 + 改 `engine.cpp:188` 调用
```cpp
// factory.h
namespace agenticdsl::scheduler {
  std::unique_ptr<IScheduler> create(
    const SchedulerConfig& cfg,
    IToolRegistry& tools,
    ILLMProvider* provider,
    const std::vector<ParsedGraph>* graphs);
}
```
- 优点: 符合 design.md:141 设计意图; engine.cpp 跨模块 include 可降 1
- 缺点: 需补 `IScheduler` 接口抽象 (Sprint 1b 已有, 复用)

**方案 B**: 删除 `scheduler/factory.{h,cpp}` + CMake 移除
- 优点: 简单, 消除死代码
- 缺点: 偏离 design.md 设计意图; 阻塞 `engine.cpp` include ≤3 推进

**选择**: 方案 A。理由 — Sprint 7 整体目标包括 `engine.cpp` ≤3 include, 保留 factory 是必要条件。

## Decision 3: `IBudgetController` 抽象引入方式

**方案 A (推荐)**: 在 `src/modules/budget/budget_controller.h` 同文件加纯虚基类
```cpp
class IBudgetController {
public:
  virtual ~IBudgetController() = default;
  virtual bool try_consume(double cost) = 0;
  virtual void record_llm_call(const std::string& model, double cost) = 0;
  virtual double remaining() const = 0;
  virtual void reset() = 0;
};
class BudgetController : public IBudgetController { /* 现有实现 */ };
```
- 优点: 单文件改动, `engine.cpp` 切换完整型 → 接口引用
- 缺点: 4 virtual 函数的 vtable 性能开销 (实测 < 5ns/call, 可忽略)

**方案 B**: 拆分到新文件 `src/modules/budget/i_budget_controller.h`
- 拒绝理由: 拆分文件增加 include 路径, 收益小

**选择**: 方案 A。`budget/factory.h:13` 改 `unique_ptr<IBudgetController>`, `engine.cpp` `budget_controller_` 类型改 `IBudgetController*` (PIMPL) 或 `unique_ptr<IBudgetController>`。

## Decision 4: 测试补齐顺序

```
Day 1: 🔴 修 fork 重复 + 跑 ctest 33/33 → commit 1
Day 2-3: 🔴 scheduler 7 测试 (test_scheduler.cpp 0 → 7 cases) → commit 2
Day 4: 🔴 parser 5 测试 + TSan concurrent → commit 3
Day 5-7: 🟠 scheduler 拆分收紧 (DagState + execute ≤ 60 + 修访问不一致) → commit 4-5
Day 8-9: 🟠 scheduler factory 补 Config + engine.cpp 调用迁移 → commit 6
Day 10-11: 🟠 engine.cpp include ≤ 3 续推 (ToolRegistry factory + IBudgetController + MockLLMProvider factory) → commit 7-8
Day 12-13: 🔴 factory 3 测试 (新建 test_engine_factory.cpp) → commit 9
Day 14-15: 🟠 plugin 7 case 改名 + mock .so fixture + E2E TEST_PLUGIN_FIXTURE_PATH 注入 → commit 10-11
Day 16: 🟡 Minor (spec 笔误 + docs 同步 + AGENTS.md) → commit 12-13
Day 17: 📦 ship gate 全跑 + openspec archive tech-debt-cleanup-sprint-6 → final commit 14
```

## Decision 5: plugin E2E mock .so fixture 设计

**方案 A (推荐)**: CMake `add_custom_target` 在 build 时用 g++ 编译一个最小 .so
```cmake
# tests/CMakeLists.txt
add_custom_target(plugin_fixture_${name} ALL
  COMMAND ${CMAKE_CXX_COMPILER} -shared -fPIC
    ${CMAKE_SOURCE_DIR}/tests/fixtures/mock_plugin.cpp
    -o ${CMAKE_BINARY_DIR}/mock_plugin_${name}.so
)
add_dependencies(test_plugin_loader plugin_fixture_${name})
target_compile_definitions(test_plugin_loader PRIVATE
  TEST_PLUGIN_FIXTURE_PATH="${CMAKE_BINARY_DIR}/mock_plugin_${name}.so")
```
- 优点: CI 兼容, 无需预编译二进制
- 缺点: 编译时间 +1s/test

**方案 B**: 提交预编译 .so 到 repo
- 拒绝理由: 二进制污染, 跨平台难

**mock_plugin.cpp 设计**:
```cpp
#include "agenticdsl/plugin/plugin_info.h"
#include <cstdint>
extern "C" int32_t pdk_register_tools(void* /*registry*/) {
  return 0;  // 返回 PDK ABI version 0
}
extern "C" const char* pdk_plugin_name() { return "mock"; }
```
- 提供 3 个变体: valid (PDK_ABI_VERSION=1) / abi_mismatch_v0 (PDK_ABI_VERSION=0) / dlsym_missing (无 pdk_register_tools 符号)

## Risks

- 🟠 **`IBudgetController` 引入回归**: 现有 `BudgetController` 完整覆盖 4 virtual 函数是隐含假设, 若漏函数 ctest 编译期即可发现, 无运行时风险
- 🟠 **`ToolRegistry` factory 化构造参数**: `make_unique<ToolRegistry>()` 默认构造 vs factory 注入路径策略 (eager vs lazy) 需对照旧调用, 否则 silent behavior diff
- 🟡 **`execute_single_branch` 118 行不动**: 决策已锁定 (spec §2.5.3), Sprint 7 严守, 不因"顺手"修改
- 🟡 **TSan 在 GitHub Actions CI 矩阵**: Dockerfile.tsan 已 ship (Sprint 3), 复用; 但 factory 并发测试需 4+1 线程, runner 资源足够

## Cross-references

- `tech-debt-cleanup-sprint-6/tasks.md` §6.3 — Sprint 7 follow-up 完整 checklist (12 项)
- `tech-debt-cleanup-sprint-6/specs/dag-scheduler-pipeline/spec.md` — STATUS NOTE 标注偏离
- `tech-debt-cleanup-sprint-6/specs/node-factory-registry/spec.md` — STATUS NOTE 标注 spec 笔误
- `tech-debt-cleanup-sprint-6/specs/tech-debt-cleanup/spec.md` — STATUS NOTE 标注 ship gate 偏离
- ADR-0019 §1.4 (engine.h/cpp decoupling 退出标准: 跨模块/common include ≤ 3)
- Oracle session `ses_112a9f9c5ffesqpYeefOBgMkjH` — 完整审查报告
