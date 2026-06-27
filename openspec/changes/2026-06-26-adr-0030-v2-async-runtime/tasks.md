# Tasks: ADR-0030 V2 — Async Runtime (Sprint 12 主体)

> **状态**: 🟡 active (Oracle 咨询已完成 2026-06-27, 占位内容已填充, 实施待启动)
> **预估工时**: ~10-12 工作日 (1.5-2 周, Oracle 校正后)
> **Oracle 决议 session**: `ses_0f5541ebfffehKDxNVuYqB7bq4`
> **关联 master plan**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C2

---

## 1. Oracle 决策前置 (已完成 ✅ 2026-06-27)

- [x] 1.1 OQ1 Fleet 模式 16 路 LLM 并行 → DEFER (Oracle: 0 examples 需并行, DomainWorkerPool 已提供 N-way)
- [x] 1.2 OQ2 Token 流式推送 → bridge runner `run_stream_to_bus` (Oracle: IGenerationStream 是 pull-based, Option A 侵入 provider)
- [x] 1.3 OQ3 双层架构 → std::jthread (C0 阶段已锁定, Sprint 2/3 验证通过)
- [x] 1.4 写 proposal.md 详细化 (Oracle 决议应用, Fleet defer + bridge runner 决策落地)
- [x] 1.5 写 tasks.md (本文件, Day-by-Day 计划)
- [x] 1.6 写 specs/async-runtime/spec.md (5 ADDED Requirements)
- [x] 1.7 移除 PLACEHOLDER 标记, 更新 STATUS 行
- [x] 1.8 修正 ADR-0030 V2 文档漂移 (默认 16 → 默认 4, 可配置 16)

---

## 2. P1 Week 1: Taskflow 集成 + bridge runner

### 2.1 Taskflow DAG 并行 executor

- [ ] 2.1.1 编辑 `src/modules/scheduler/topo_scheduler.h`: 新增 `tf::Executor` 成员 + `execute_parallel()` 方法签名
- [ ] 2.1.2 编辑 `src/modules/scheduler/topo_scheduler.cpp`: 实现 `execute_parallel()`, 使用 `tf::Taskflow` 构建 DAG, Fork/Join 改 Subflow
- [ ] 2.1.3 编辑 `src/modules/scheduler/topo_scheduler.cpp`: 移除 `topo_scheduler.h` 中对 `ExecutionSession` 的 `execute()` 方法的 coupling (如需要)
- [ ] 2.1.4 编辑 `src/modules/scheduler/CMakeLists.txt`: 添加 `taskflow` 到 `target_link_libraries`
- [ ] 2.1.5 编辑根 `CMakeLists.txt`: **移除** `add_subdirectory(external/async_simple)` (async_simple 不再使用, V2 决策)
- [ ] 2.1.6 编辑根 `CMakeLists.txt`: `add_subdirectory(external/taskflow)` 已存在, 验证 target_link_libraries 链
- [ ] 2.1.7 验证: `grep -n "tf::Executor\|tf::Taskflow" src/modules/scheduler/topo_scheduler.cpp` ≥ 5 命中
- [ ] 2.1.8 验证: `grep -n "async_simple" CMakeLists.txt src/CMakeLists.txt` 0 命中
- [ ] 2.1.9 `cmake --build build` 编译通过
- [ ] 2.1.10 提交: `git commit -m "feat(scheduler): integrate Taskflow executor for parallel DAG (C2 P1, ADR-0030 V2)"`

### 2.2 stream_to_bus bridge runner (Oracle Q2 决议)

- [ ] 2.2.1 新建 `src/common/llm/stream_to_bus.h`: `run_stream_to_bus()` 函数签名 + 注释 (40 行)
- [ ] 2.2.2 新建 `src/common/llm/stream_to_bus.cpp`: pull-loop 实现 + ToolResult payload 构造 + stop_token 检查
- [ ] 2.2.3 编辑 `src/common/llm/CMakeLists.txt`: 添加 `stream_to_bus.cpp` 到 `agenticdsl_common` 静态库
- [ ] 2.2.4 验证: `grep -n "run_stream_to_bus" src/common/llm/llm_types.h` 类型前向声明

### 2.3 Context fork/merge 不可变分支

- [ ] 2.3.1 编辑 `src/core/types/context.h`: 新增 `Context::fork()` 方法 (深拷贝 Layer, `Layer` 类型来自 context.h)
- [ ] 2.3.2 编辑 `src/core/types/context.h`: 新增 `Context::merge(const Context&)` 方法 (策略合并: parent 优先 + child 覆盖)
- [ ] 2.3.3 编辑 `src/core/types/context.cpp`: 实现 fork/merge (注意 ExecutionBudget 非拷贝, 用 optional<ExecutionBudget> 移动)
- [ ] 2.3.4 编辑 `src/modules/scheduler/topo_scheduler.cpp`: 在 `execute()` DAG 节点派发前调用 `fork()`, 节点完成时 `merge()`

### 2.4 P1 测试 (Day 4-5)

- [ ] 2.4.1 新建 `tests/test_stream_to_bus.cpp`:
  - `TEST_CASE("run_stream_to_bus emits llm.token per chunk")`: MockProvider + InMemoryBus, 验证 token 事件顺序
  - `TEST_CASE("run_stream_to_bus emits llm.token.done on completion")`: 验证结束事件 + finish_reason
  - `TEST_CASE("run_stream_to_bus emits llm.token.error on failure")`: 验证错误事件 payload
  - `TEST_CASE("run_stream_to_bus respects stop_token cancellation")`: 验证 stop_token 触发后立即停止
  - `TEST_CASE("run_stream_to_bus aggregates final GenerationResult")`: 验证返回值的 text 拼接
- [ ] 2.4.2 编辑 `tests/test_scheduler.cpp`: 新增 `TEST_CASE("execute_parallel dispatches independent nodes concurrently")` (验证 2 个独立节点并行, 时间 < 串行时间)
- [ ] 2.4.3 编辑 `tests/test_context.cpp` (或新建): 新增 `TEST_CASE("Context::fork creates deep copy")` + `TEST_CASE("Context::merge applies child overrides")`
- [ ] 2.4.4 `ctest --output-on-failure` ≥ 38/38 PASS (35 baseline + 3 new stream_to_bus)
- [ ] 2.4.5 `cmake --preset tsan && ctest -R "stream_to_bus|execute_parallel"` 0 race
- [ ] 2.4.6 提交: `git commit -m "feat(llm): stream_to_bus bridge runner + scheduler parallel + context fork/merge (C2 P1)"`

---

## 3. P2 Week 2-3: IInteractionBus 后端切换为 EventBus

### 3.1 EventBus MPMC 后端集成

- [ ] 3.1.1 编辑 `src/common/contract/inmemory_bus.cpp`: 实现 EventBus MPMC 有界队列后端 (用 `std::queue<T>` + mutex + condition_variable)
- [ ] 3.1.2 编辑 `src/common/contract/inmemory_bus.cpp`: `emit()` 改为入队 + notify_one (非同步通知所有 subscriber)
- [ ] 3.1.3 编辑 `src/common/contract/inmemory_bus.cpp`: 后台 dispatch 线程从队列取事件, 同步通知 subscribers
- [ ] 3.1.4 验证: `IInteractionBus` 公共 API 不变 (emit/subscribe/unsubscribe 签名保持)

### 3.2 P2 测试

- [ ] 3.2.1 编辑 `tests/test_interaction_bus.cpp`: 新增 `TEST_CASE("InMemoryBus emit decoupled from subscriber speed")`: 验证 emit 不阻塞 (slow subscriber 不影响 emit)
- [ ] 3.2.2 新增 `TEST_CASE("InMemoryBus bounded queue backpressure")`: 验证队列满时 emit 策略 (drop oldest / block / expand)
- [ ] 3.2.3 新增 `TEST_CASE("InMemoryBus 1000x concurrent emit no race")`: TSan 验证
- [ ] 3.2.4 `ctest --output-on-failure` ≥ 41/41 PASS (38 + 3 new interaction_bus)
- [ ] 3.2.5 `cmake --preset tsan && ctest` 0 race

### 3.3 ADR-0030 V2 文档漂移修正

- [ ] 3.3.1 编辑 `docs/adr/adr-0030-async-runtime-v2.md` §决策记录: "DomainWorkerPool 默认 16" → "默认 4, 可配置 16"
- [ ] 3.3.2 编辑 §线程模型: "M (domain, 默认 16)" → "M (domain, 默认 4, 可配置 16)"
- [ ] 3.3.3 编辑 §决策 2 P2 范围: 移除 "Fleet 模式 16 路 LLM 并行" (defer 到 Phase 3+)
- [ ] 3.3.4 编辑 §决策 2 P1: 新增 "stream_to_bus bridge runner (Token 流推送)" (Oracle Q2 决议)
- [ ] 3.3.5 提交: `git commit -m "docs(adr): correct V2 doc drift (DomainWorkerPool default + Fleet defer + bridge runner)"`

---

## 4. Ship Gate (Week 3 Day 11-12)

### 4.1 全量验证

- [ ] 4.1.1 `ctest --output-on-failure` ≥ 41/41 PASS (含新增 6 测试)
- [ ] 4.1.2 `cmake --preset tsan && ctest` 0 race
- [ ] 4.1.3 `cmake --preset asan && ctest` 0 leak
- [ ] 4.1.4 `python3 tools/adr_lint.py docs/adr/ docs/archive/adr/ docs/adr/plugin/` exit 0
- [ ] 4.1.5 `python3 tools/docs_drift_audit.py` 0 critical drift
- [ ] 4.1.6 `openspec validate 2026-06-26-adr-0030-v2-async-runtime` exit 0
- [ ] 4.1.7 `grep -c "async_simple" CMakeLists.txt src/CMakeLists.txt` 0 (P1 移除验证)
- [ ] 4.1.8 `grep -n "5 虚函数\|sync callback" docs/adr/adr-0031-execution-policy.md` ≥ 1 命中 (C3 兼容性)

### 4.2 ADR-0030 V2 状态升级

- [ ] 4.2.1 编辑 `docs/adr/adr-0030-async-runtime-v2.md` frontmatter: 🔍 Proposed → ✅ Approved (2026-06-XX, C2 ship)
- [ ] 4.2.2 编辑 `docs/adr/adr-0030-async-runtime-v2.md` §状态变更日志: 追加 C2 ship 行

---

## 5. 同步与归档 (Week 3 Day 12-13)

- [ ] 5.1 更新 `docs/roadmap-status.md` §一 Phase 2 行: 0% → 100% (P1+P2 ship)
- [ ] 5.2 更新 `docs/roadmap-status.md` §四 实施日志: 追加 Sprint 12 ship 行
- [ ] 5.3 更新 `AGENTS.md` § Recent Changes: 追加 Sprint 12 C2 ship 摘要
- [ ] 5.4 同步 PDK 头文件: `./scripts/sync-pdk.sh --dry-run` 无 error (如变更了 PDK 公共 API)
- [ ] 5.5 `openspec archive 2026-06-26-adr-0030-v2-async-runtime --yes`
- [ ] 5.6 同步 master plan §三 C2 行: 状态 → ✅ archived
- [ ] 5.7 同步 master plan §四 C2 行: 状态 → ✅ shipped → ✅ archived
- [ ] 5.8 同步 master plan §五 Sprint 12: ✅ C2 ship
- [ ] 5.9 同步 master plan §十一.1 C2 行: 🟡 in-progress → ✅ resolved
- [ ] 5.10 提交: `git commit -m "chore(openspec): archive C2 + Sprint 12 ship (ADR-0030 V2 ✅ Approved)"`

---

## 验证检查清单 (C2 ship gate)

- [ ] 1. Oracle 3 决策全部应用 (Fleet defer + bridge runner + std::jthread)
- [ ] 2. ADR-0030 V2 文档漂移修正 (默认 16 → 4, Fleet defer 标注)
- [ ] 3. async_simple CMake 依赖移除
- [ ] 4. stream_to_bus bridge runner 实施 + 5 测试 pass
- [ ] 5. Taskflow DAG 并行 executor 实施 + 并行测试 pass
- [ ] 6. Context fork/merge 实施 + 测试 pass
- [ ] 7. InMemoryBus EventBus 后端切换 + 3 测试 pass
- [ ] 8. ctest ≥ 41/41 + ASan/TSan 100% clean
- [ ] 9. `adr_lint.py` + `docs_drift_audit.py` + `openspec validate` 全 exit 0
- [ ] 10. master plan C2 行状态 ✅ archived + OpenSpec archive 完成