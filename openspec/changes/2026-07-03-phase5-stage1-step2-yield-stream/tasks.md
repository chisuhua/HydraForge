# Tasks: Phase 5 Stage 1 Step 2 — YIELD/STREAM Node (C12)

> **STATUS: ACTIVE** 🟡 (Oracle 深度审查完成 2026-07-03)
> **关联 proposal**: `proposal.md`
> **关联 spec**: `specs/yield-stream/spec.md`
> **关联 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §十六.3
> **前置依赖**: C10 ✅ + C11 ✅
> **估时**: 3.5-4.5 天 (Oracle 审查后调整: +2.7d for 5 risk mitigations)
> **最后更新**: 2026-07-03 (Oracle 深度审查 session `ses_0d5985f3effeS1npyEV6SYk2RW`)

---

## 1. NodeType::YIELD 枚举 + YieldNode 结构

- [x] 1.1 `src/core/types/node.h` — NodeType 加 YIELD
- [x] 1.2 `src/core/types/node.h` — 定义 `enum class YieldMode { NEXT, CONTINUE, STOP }`
- [x] 1.3 `src/core/types/node.h` — 定义 `struct YieldNode { yield_value, mode, stop_path }`
- [x] 1.4 Node 继承体系加 `YieldNode : public Node` 子类 (沿用现有 10 个 Node 子类模式 — Node 是多态基类, 非 union; 见 `src/core/types/node.h:35-55` 的 polymorphic 架构)
- [x] 1.5 audit 全库 grep `switch.*NodeType` 站点, 确保 YIELD case 全覆盖 (Oracle Risk 12: exhaust switch)

---

## 2. MarkdownParser JSON 解析

- [ ] 2.1 `src/modules/parser/markdown_parser.cpp` — 解析 `type: yield` 节点
- [ ] 2.2 解析 yield_value / mode / stop_path 字段
- [ ] 2.3 解析失败时清晰错误信息

---

## 3. NodeExecutor execute_yield() 实现

- [ ] 3.1 `src/modules/executor/node_executor.h` — 声明 `execute_yield(LayeredContext&, YieldNode&)`
- [ ] 3.2 `src/modules/executor/node_executor.cpp` — 渲染 yield_value 模板
- [ ] 3.3 实现 NEXT 模式: 包装 ToolResult + 设置 pending_yield_
- [ ] 3.4 实现 CONTINUE 模式: 循环 `IGenerationStream::next(std::stop_token)`, **每 1 token** 检查 budget (Oracle Risk 11;实际接口在 `src/common/llm/llm_types.h:53-61`,方法名是 `next` 不是 `pull_next`)
- [ ] 3.5 实现 STOP 模式: 终止流, 跳到 stop_path (Oracle Q2: stop_path 为已定义后续节点)
- [ ] 3.6 `src/modules/executor/yield_stream_bridge.h/cpp` 新建 — YieldStreamBridge 封装 `next(token)` → YieldState (Oracle Risk 9)
- [ ] 3.7 CONTINUE 模式 BudgetExceededException **携带已消费 token 片段** (非空结果丢弃) (Oracle Risk 11)
- [ ] 3.8 **`src/modules/executor/node_executor.cpp` dispatch switch (line 46) 新增 `case NodeType::YIELD: return execute_yield(...)`** — Oracle Risk 12 mitigation (避免 exhaust switch warning, 验证 §1.5 grep 找到的 dispatch 站点都已加 case)

---

## 4. ExecutionSession pending_yield_ 扩展

- [ ] 4.1 `src/modules/scheduler/execution_session.h` — 定义 `struct YieldState { module_path, resume_context }`
- [ ] 4.2 加 `std::optional<YieldState> pending_yield_` 字段
- [ ] 4.3 默认值: `std::nullopt`

---

## 5. TopoScheduler yield pause/resume (Oracle Risk 8 mitigation)

- [ ] 5.0 `src/modules/scheduler/topo_scheduler.h` — 新增 `SchedulerState` 枚举 (Oracle Risk 8: state machine)
- [ ] 5.1 `src/modules/scheduler/topo_scheduler.cpp` — yield 暂停: **不跳出主 while 循环**, 循环内检测 pending_yield_ 后挂起 (Oracle Risk 8: DAG state 保持)
- [ ] 5.2 实现 `resume_yield(session_id, token_value)` 公共方法
- [ ] 5.3 **DAG state 持久化**: resume_context 保存 `ready_queue` + `in_degree_table` (O(|V|+|E|) 避免重建) (Oracle Risk 8)
- [ ] 5.4 端到端测试: NEXT → 调用者 receive token → 决策 → 调 resume → 继续

---

## 6. Budget 集成 (Oracle Risk 11 mitigation)

- [ ] 6.0 `src/modules/scheduler/execution_session.h` — **新增** `struct BudgetExceededException : public std::exception { std::vector<std::string> consumed_tokens; std::string message; ... }` (C1 fix: 新异常类型, 必须显式声明字段 — 全代码库当前 0 匹配, tasks §3.7/§6.3/§6.4 + spec.md line 127 都引用该类型)
- [ ] 6.1 CONTINUE 模式 **每 pull 1 token** 检查 `is_budget_exceeded()` (可配置, 默认 1) (Oracle Risk 11)
- [ ] 6.2 超过预算立即终止流
- [ ] 6.3 抛 `BudgetExceededException` **携带已消费 token 向量** (新增 exception 类型, 在 execution_session.h) (Oracle Risk 11)
- [ ] 6.4 DSLEngine::run() 捕获后转换为 ExecutionResult 错误状态 (含 partial_result 字段)

## 6a. yield_stream_bridge (Oracle Risk 9 mitigation)

- [ ] 6a.1 `src/modules/executor/yield_stream_bridge.h` 新建 — `class YieldStreamBridge { pull_single(), pull_loop() }`
- [ ] 6a.2 `src/modules/executor/yield_stream_bridge.cpp` 实现 pull_single (NEXT) + pull_loop (CONTINUE)
- [ ] 6a.3 `pull_loop()` 接受 `std::function<bool()> budget_checker` callback (Oracle Risk 11 每 token 检查)
- [ ] 6a.4 集成到 `execute_yield()` — NEXT 调 pull_single, CONTINUE 调 pull_loop

## 6b. cross-thread YIELD safety (Oracle Risk 10 mitigation)

- [ ] 6b.1 `execution_session.h` — pending_yield_ 访问加 `std::mutex yield_mutex_` (字段级, 非 ExecutionSession 整体锁) (Oracle Risk 10)
- [ ] 6b.2 `resume_yield()` 原子操作: check pending → clear → continue DAG (Oracle Risk 10)
- [ ] 6b.3 TSan 必验证: cross-thread resume 0 data race (Oracle Risk 10)

---

## 7. 单元测试

- [ ] 7.1 `tests/test_yield_node.cpp` 新建
- [ ] 7.2 test case: NEXT 模式返回单 token
- [ ] 7.3 test case: CONTINUE 模式流式拉取到 stream 结束
- [ ] 7.4 test case: STOP 模式跳转 stop_path
- [ ] 7.5 test case: CONTINUE 模式 budget 触发中断
- [ ] 7.6 test case: TopoScheduler yield pause + resume 端到端
- [ ] 7.7 test case: pending_yield_ 跨 await 持久化
- [ ] 7.8 test case: IGenerationStream pull-based 集成 (mock stream)
- [ ] 7.9 test case: ASan 零 leak
- [ ] 7.10 test case: TSan 零 race (yield 跨 await 边界)

---

## 8a. 示例程序 (master plan §四 ship gate 要求, D3b 修复)

- [ ] 8a.1 `examples/phase5_yield_token_generator/` 目录创建 (master plan §四 line 225 ship list item 6)
- [ ] 8a.2 `main.cpp` 实现: 加载 `.agent.md` (NEXT 模式 yield_value 模板), N 次调用返回 N 个 token
- [ ] 8a.3 `CMakeLists.txt` + 根 `CMakeLists.txt` `AGENTICDSL_BUILD_EXAMPLES` opt-in 注册 (沿用 Sprint 19 `agent_simple`/`agent_loop` mock LLM 模式)
- [ ] 8a.4 E2E 验证: yield 之间 module_state 保持 (验证 C10 lazy module_state + C12 YIELD 集成)
- [ ] 8a.5 `phase5_yield_token_generator --mock` 可运行, 验证 N 次调用返回 N 个 token (master plan §四 line 228 ship gate)

---

## 8. 验证

- [ ] 8.1 `ctest --output-on-failure` ≥ 63/63 (C11 ship 后基线) + 新增 test_yield_node 8-10 case 全绿 (master plan §四 line 228 要求 ≥64/64, post-C12 总数应 ≥71/71)
- [ ] 8.2 `python3 tools/adr_lint.py` exit 0 (零 ADR 修改)
- [ ] 8.3 `python3 tools/docs_drift_audit.py` 0 DRIFT
- [ ] 8.4 `cmake --preset asan && ctest` 零 ASan error
- [ ] 8.5 `cmake --preset tsan && ctest` 零 TSan warning (关键: yield 跨 await)
- [ ] 8.6 `openspec validate 2026-07-03-phase5-stage1-step2-yield-stream` exit 0

---

## 9. 同步与归档

- [ ] 9.1 提交 (推荐 2 commits: `feat(c12): add YIELD node + YieldMode` + `feat(c12): wire YIELD into TopoScheduler + Budget check`)
- [ ] 9.2 `git push origin main`
- [ ] 9.3 `openspec archive 2026-07-03-phase5-stage1-step2-yield-stream`
- [ ] 9.4 写 §十一 调整日志到 master plan (C12 ship 状态, 估时调整 +0.5 天 验证)
- [ ] 9.5 更新 master plan §四 C12 行状态

---

## 验证检查清单 (C12 ship gate)

- [ ] 1. NodeType::YIELD 已 ship
- [ ] 2. YieldMode 3 模式已 ship
- [ ] 3. execute_yield() 3 模式实现完成
- [ ] 4. pending_yield_ 持久化已 ship
- [ ] 5. TopoScheduler yield pause/resume 已 ship
- [ ] 6. Budget 检查集成完成
- [ ] 7. test_yield_node.cpp ≥ 8 test case 全绿
- [ ] 8. ctest 零回归
- [ ] 9. ASan 0 leak
- [ ] 10. TSan 0 race
- [ ] 11. ADR-0030 V2 状态保持 ✅ Approved
- [ ] 12. master plan §四 C12 行更新
- [ ] 13. change 已 archive
- [ ] 14. 2b 推理子图 (prefix_cache/kv_cache/decoding) 评估 (C10 ship gate 联动)
