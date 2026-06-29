# Tasks: Audit Quick Wins (Sprint 15)

> 9 commits shippable on branch `audit/sprint-15-quick-wins`. 41/41 ctest PASS.

## P0 Tasks (P0-A.0)

### Task P0-A.0: Fix 2 pre-existing test failures (ToolCoordinator opt-in)

**Status:** ✅ Done (commit `cf94f78`)

- [x] Investigate root cause: Sprint 14 C4 commit `a48e563` ToolCoordinator default-injection 触发 `LOG_WARN("both tool_coordinator_ and approval_handler_ are set, preferring tool_coordinator_")`
- [x] Choose fix option: Option A (opt-in design, backward-compat)
- [x] `src/core/engine.cpp`: 默认 ctor `tool_coordinator_ = nullptr`
- [x] `src/core/engine.h`: 新增 `set_tool_coordinator(unique_ptr<ToolCoordinator>)` API
- [x] 验证: 41/41 ctest PASS (was: 39/41)

## Code Debt Tasks (A.1 - A.8)

### Task A.1: Fix AGENTS.md cross-module include drift (1 → 4)

**Status:** ✅ Done (commit `1c49162`)

- [x] 修正 `AGENTS.md` 第 101 行: engine.h 跨模块 include 计数从 "2→1" 修正为 "1 types 例外 + 3 common/policy/*"
- [x] 添加 Sprint 15 P0-A.0 ship 记录
- [x] 验证: 无构建/测试影响 (文档变更)

### Task A.2: Remove unreachable fork/join stubs in node_executor.cpp

**Status:** ✅ Done (commit `e563ec5`)

- [x] `execute_node()` switch: FORK/JOIN case 改为 throw `std::logic_error` (routing bug 信号)
- [x] 删除 `execute_fork()` 函数体 (12 行)
- [x] 删除 `execute_join()` 函数体 (12 行)
- [x] `node_executor.h`: 移除两个声明
- [x] 验证: 41/41 ctest PASS

### Task A.3: Clean commented legacy code in execution_session.cpp

**Status:** ✅ Done (commit `29775e3`)

- [x] 删除 L148-156 注释代码块 (Sprint 2 重构遗留)
- [x] 验证: 41/41 ctest PASS

### Task A.4: Extract compute_backoff() in cloud_adapter.cpp

**Status:** ✅ Done (commit `b450f83`)

- [x] 新增 file-static helper `compute_backoff(attempt, base_ms, max_ms, rng)`
- [x] 替换两处内联实现 (L262-270 网络错误路径, L284-291 5xx 路径)
- [x] 验证: 41/41 ctest PASS

### Task A.5: Deduplicate build_dag() in topo_scheduler.cpp

**Status:** ✅ Done (commit `815e64a`)

- [x] 删除 debug drain-and-log 循环 (L141-145)
- [x] `build_dag(DagState&)` 重载改为复用 `build_dag()` 的 `ready_queue_`
- [x] 验证: 41/41 ctest PASS

### Task A.6: PluginLoader: non-Linux stub + unified logging

**Status:** ✅ Done (commit `5fbe771`)

- [x] 替换 `std::cerr` 为项目 `LOG_*` 宏 (via 薄包装函数)
- [x] 非 Linux 平台: `#error` → stub 实现 (所有方法返回 false/empty)
- [x] 验证: 41/41 ctest PASS (Linux 平台)

### Task A.7: Merge make_llm_call / make_dsl_call in node_factory.cpp

**Status:** ✅ Done (commit `692e71a`)

- [x] 提取共享 helper `make_dsl_or_llm_call(path, json, default_llm_tool_name)`
- [x] 提取 `parse_llm_params(json)` 辅助函数
- [x] 两个 public factory 变为薄包装 (仅 default 参数不同)
- [x] 验证: 41/41 ctest PASS

### Task A.8: layered_context.h: split_path return type + at() double navigation

**Status:** ✅ Done (commit `1fd1bb3`)

- [x] `split_path()` 返回类型 `bool` → `void` (永远 true, 无错误路径)
- [x] `navigate()` system 层双重遍历合并为单次遍历
- [x] 4 个 call sites 已更新 (不需要返回值检查)
- [x] 验证: 41/41 ctest PASS

## Verification Task

### Task Verify: Run full test suite + OpenSpec validate

**Status:** ✅ Done

- [x] `cmake --build build -j$(nproc)` 0 errors
- [x] `ctest --output-on-failure`: **41/41 PASS** (100%)
- [x] 无 ASan/TSan 运行 (留 Sprint 10 既有 baseline)
- [x] 9 commits 全部成功 shippable

## Ship Gate

- [x] `openspec validate audit-quick-wins` exit 0
- [x] `git log audit/sprint-15-quick-wins ^main --oneline` = 9 commits (含 plan 提交)
- [x] `ctest` 41/41 PASS
- [x] AGENTS.md "Recent Changes" 已添加 P0-A.0 ship 记录
- [x] plan 文档 `docs/superpowers/plans/2026-06-30-audit-remediation-roadmap.md` 已创建

## Metrics

| 维度 | Before | After |
|------|--------|-------|
| 测试通过率 | 39/41 (95%) | **41/41 (100%)** |
| P0 测试失败 | 2 | **0** |
| 代码债 (# of items) | 8 | **0** |
| 代码行数 (净变化) | — | -80 行 (refactor 删除) |
| API 兼容性 | — | ✅ 完全向后兼容 |

## Out of Scope (留 Sprint 16/17)

- Change B (`2026-06-30-audit-coverage-backfill`): 9 个 MISSING/PARTIAL 测试补齐
- Change C (`2026-06-30-audit-arch-refactor`): execute_single_branch 拆分, layered_context thread safety, topo_scheduler.h 跨模块解耦