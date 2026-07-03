# Tasks: Phase 5 Stage 1 Step 2 — YIELD/STREAM Node (C12)

> **STATUS: PLACEHOLDER** ⚠️
> **关联 proposal**: `proposal.md`
> **关联 spec**: `specs/yield-stream/spec.md`
> **关联 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §十六.3
> **前置依赖**: C10 ✅ + C11 ✅
> **估时**: 2.5-3 天
> **最后更新**: 2026-07-03

---

## 1. NodeType::YIELD 枚举 + YieldNode 结构

- [ ] 1.1 `src/core/types/node.h` — NodeType 加 YIELD
- [ ] 1.2 `src/core/types/node.h` — 定义 `enum class YieldMode { NEXT, CONTINUE, STOP }`
- [ ] 1.3 `src/core/types/node.h` — 定义 `struct YieldNode { yield_value, mode, stop_path }`
- [ ] 1.4 Node 联合体加 `YieldNode yield_data`

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
- [ ] 3.4 实现 CONTINUE 模式: 循环 pull IGenerationStream, 每 N tokens 检查 budget
- [ ] 3.5 实现 STOP 模式: 终止流, 跳到 stop_path

---

## 4. ExecutionSession pending_yield_ 扩展

- [ ] 4.1 `src/modules/scheduler/execution_session.h` — 定义 `struct YieldState { module_path, resume_context }`
- [ ] 4.2 加 `std::optional<YieldState> pending_yield_` 字段
- [ ] 4.3 默认值: `std::nullopt`

---

## 5. TopoScheduler yield pause/resume

- [ ] 5.1 `src/modules/scheduler/topo_scheduler.cpp` — yield 暂停: 跳出主 while 循环
- [ ] 5.2 实现 `resume_yield(session_id, token_value)` 公共方法
- [ ] 5.3 resume 时从 pending_yield_.resume_context 恢复
- [ ] 5.4 端到端测试: NEXT → 调用者 receive token → 决策 → 调 resume → 继续

---

## 6. Budget 集成 (Oracle 风险 mitigation)

- [ ] 6.1 CONTINUE 模式每 pull 10 tokens 检查 `is_budget_exceeded()` (可配置 N)
- [ ] 6.2 超过预算立即终止流
- [ ] 6.3 抛 `BudgetExceededException` (新增 exception 类型, 在 execution_session.h)
- [ ] 6.4 DSLEngine::run() 捕获后转换为 ExecutionResult 错误状态

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

## 8. 验证

- [ ] 8.1 `ctest --output-on-failure` ≥ 61/61 + 新增 test_yield_node 8-10 case 全绿
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
