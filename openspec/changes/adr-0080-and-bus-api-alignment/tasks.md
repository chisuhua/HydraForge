# Tasks: adr-0080-and-bus-api-alignment

> **关键不变量**：零代码/测试改动；纯文档勘误；真相源 = 代码 emit 点 → ADR-0068 附录 A → 其余一切
> **TDD 5 步结构**：每任务按 Write failing assertion → Verify fail → Implement → Verify pass → Commit
> **V1 边界**：仅修本 change 列出的 13 项；6 幻影主题机制修复 out of scope（需代码改动，独立 change）

## Phase 0: 基线快照（pre-fix 验证）

- [x] **T0.1** 记录 baseline grep 结果（用作修改后对比）
  - `grep -c "^| \`" docs/adr/adr-0068-event-emission-contract.md` → 63
  - `grep -c "subscribe_glob" docs/adr/adr-00{19,68}*.md` → ≥ 2
  - `grep "v1.2 amendment" docs/adr/adr-0080-append-only-event-log.md` → 0
  - `grep "^## 不变量" docs/adr/adr-0080-append-only-event-log.md` → 2
  - `grep "llm.token" docs/adr/adr-0068-event-emission-contract.md | grep -v "^|.*llm\.token"` → 0（附录 A 内）
  - `grep "ADR-0025" docs/adr/adr-0030-async-runtime-v2.md | grep "paralle"` → 1
  - `grep "v1.2" docs/adr/adr-0079-unified-session-4scope.md | head -3` → 仅正文 D7-D10
- [x] **T0.2** Snapshot `python3 tools/adr_lint.py` baseline（须 exit 0）
- [x] **T0.3** Snapshot `python3 tools/docs_drift_audit.py` baseline（须 0 DRIFT）
- [x] **T0.4** Commit baseline（不实际 commit，仅记录）

## Phase 1: ADR-0080 D9 主题表 → 引用 ADR-0068（估时 0.2h）

- [x] **T1.1** Read `docs/adr/adr-0080-append-only-event-log.md` L221-247（D9 表全文）
- [x] **T1.2** Write failing assertion：期望 D9 表不重复 ADR-0068 主题规范
- [x] **T1.3** Modify：D9 标题保留 §决策 D9（"Topic 命名约定"）但表格整体替换为引用段落
  - 替换为：*"主题规范以 [ADR-0068 附录 A](../adr-0068-event-emission-contract.md#附录-acanonical-topic-registry) 为准；EventLog 以 `bus_->subscribe("*")` 全量订阅。SessionWriter 的会话子集主题见 [ADR-0079 §决策 D6](../adr-0079-unified-session-4scope.md#决策-d6事件到-jsonl-映射)。**机制缺口提醒**：以下主题当前无生产发射方（`attempt.*`/`conversation.*`/`phase.completed`/`branch.created`/`attempt.converged`）—— `session_writer.cpp:30-47` whitelist 订阅但零 emit；发射归口后须回填 ADR-0068 附录 A。"*
- [x] **T1.4** Verify：`grep -c "^| \`" docs/adr/adr-0080-append-only-event-log.md`（与 baseline 对比，D9 移除后应减少 ≥ 18 行）
- [x] **T1.5** Verify：`grep "ToolCoordinator" docs/adr/adr-0080-append-only-event-log.md` ≥ 1（如有保留偶发主题行）
- [x] **T1.6** Commit：`docs(adr-0080): §决策 D9 主题表整体替换为 ADR-0068 引用 (issue 1 + 6)`

## Phase 2: ADR-0080 附录 A JSONL 示例 + D2 示例字段对齐（估时 0.3h）

- [x] **T2.1** Read 附录 A L455-459 与 D2 L60-75
- [x] **T2.2** Write failing assertion：期望附录 A 示例不含 `args`、字段名 `ts_wall`、单位毫秒
- [x] **T2.3** Modify：附录 A L455-459 5 个示例
  - 第 1 行（llm.request）：`"ts":1737281400` → `"ts_wall":1737281400123`、加 `"causal_time":1`
  - 第 2 行（llm.response）：同上
  - 第 3 行（tool.execution.start）：`payload` 由 `{"tool":"file.write", "args":{"path":"/tmp/test.py"}}` → `{"tool":"file.write", "layer":"workflow", "ok":true, "duration_ms":42}`；`ts`→`ts_wall`
  - 第 4 行（tool.execution.end）：payload 由 `{"tool":"file.write", "ok":true}` → `{"tool":"file.write", "layer":"workflow", "ok":true, "duration_ms":42}`；`ts`→`ts_wall`
  - 第 5 行（attempt.ended）：`"ts":1737281404-005` → `"ts_wall":...` 等
- [x] **T2.4** Modify：D2 字段表 L72 ts_wall 字段说明末尾加 "（注：附录 A 示例 5 行均已统一为本字段）"
- [x] **T2.5** Modify：D2 字段示例 L63 `"ts":1737281400` → `"ts_wall":1737281400123`、加 `"causal_time":1`
- [x] **T2.6** Verify：`grep "ts_wall" docs/adr/adr-0080-append-only-event-log.md | wc -l` ≥ 8（schema 表 + 5 示例 + 字段说明 + 附录 E）
- [x] **T2.7** Verify：`grep -E '"ts":[0-9]+' docs/adr/adr-0080-append-only-event-log.md` 0 命中
- [x] **T2.8** Verify：`grep "args" docs/adr/adr-0080-append-only-event-log.md | grep "paralle"` 不在附录 A JSONL 示例内
- [x] **T2.9** Commit：`docs(adr-0080): 附录 A + D2 示例字段与 schema 对齐 (issue 2 + 3)`

## Phase 3: ADR-0080 D10.3 ↔ v1.2 amendment 双向指针（估时 0.3h，**最易改错**）

- [x] **T3.1** Read `docs/adr/adr-0080-v1-2-amendment-d10-decouple.md`（全文，理解 CaptureMode 三态 + Training fail-open 三重保护的设计意图）
- [x] **T3.2** Write failing assertion：期望 D10.3 段尾含 `v1.2 amendment` 引用
- [x] **T3.3** Modify：ADR-0080 D10.3 L283 段尾
  - 旧：*"开启后依赖 ADR-0081 pre-step hook 在 emit 前 scrub（先 scrub 后落盘）——**未 ship 时不暴露**。"*
  - 新：*"开启后依赖 ADR-0081 pre-step hook 在 emit 前 scrub（先 scrub 后落盘）—— **Online 模式**：scrub 后暴露；**Training 模式**：fail-open（详见 [v1.2 amendment](../adr-0080-v1-2-amendment-d10-decouple.md) 引入的 CaptureMode 三态 + Training fail-open 三重保护，2026-08-25 评审通过，Oracle G12 解锁原 ADR-0081→0082 死锁）。"*
- [x] **T3.4** Modify：ADR-0080 v1.2 amendment 文件头部加反向指针
  - 在 `## 状态` 段后加：*"> **主文档引用**：本 amendment 修订 D10 的解耦语义，请同步阅读 [adr-0080-append-only-event-log.md §决策 D10.3](../adr-0080-append-only-event-log.md#决策-d10模型可见字节的条件持久化distillation-capture)。"*
- [x] **T3.5** Verify：`grep -c "v1.2 amendment" docs/adr/adr-0080-append-only-event-log.md` ≥ 1
- [x] **T3.6** Verify：`grep -c "v1.2 amendment" docs/adr/adr-0080-v1-2-amendment-d10-decouple.md` ≥ 1（反向指针）
- [x] **T3.7** Verify：人工 review D10.3 新表述，确认"Online 依赖、Training fail-open"语义与 amendment 一致
- [x] **T3.8** Commit：`docs(adr-0080): D10.3 与 v1.2 amendment 双向指针 (issue A1, 最严重遗漏)`

## Phase 4: ADR-0080 结构清理（不变量重复 + 附录 C 计数）估时 0.1h

- [x] **T4.1** Read 重复的 `## 不变量` 章节 L370-378 与 L380-387
- [x] **T4.2** 决定保留哪一份：保留含 D10.5（capture-off 默认）的 L380-387 版本（7 条），删除 L370-378
- [x] **T4.3** Modify：删除 L370-378 的重复章节
- [x] **T4.4** Modify：附录 C L480 "15 个 topic" → "13 个 topic"
- [x] **T4.5** Verify：`grep -c "^## 不变量" docs/adr/adr-0080-append-only-event-log.md` = 1
- [x] **T4.6** Verify：`grep "13 个" docs/adr/adr-0080-append-only-event-log.md | grep "topic"` ≥ 1
- [x] **T4.7** Commit：`docs(adr-0080): 合并重复不变量章节 + 附录 C 计数校正 (issue A7 + A8)`

## Phase 5: ADR-0068 措辞 + 版本号 + 计数 + llm.token 补登（估时 0.3h）

- [x] **T5.1** Read ADR-0068 L25（状态行）、L64（决策 1 表格）、L80（决策 2 计数）、L174（附录 A 标题）、L242（表格末行）
- [x] **T5.2** Write failing assertion：
  - 期望 `subscribe_glob` 0 命中
  - 期望附录 A 含 `llm.token` 3 行
  - 期望附录 A 标题版本号 = (v2.0, 2026-08-31)
- [x] **T5.3** Modify：L25 "subscribe_glob + CausalClock 均已 ship" → "subscribe() 通配符支持 + CausalClock 均已 ship"
- [x] **T5.4** Modify：L64 决策 1 表格 "emit/subscribe/subscribe_glob/CausalClock" → "emit/subscribe (通配符支持)/CausalClock"
- [x] **T5.5** Modify：L80 §决策 2 "去重后 22 个" → "v1 初始 22 个（截至 Appendix A v2.0 共 N 个，可复现命令 `grep -c "^| \`" docs/adr/adr-0068-event-emission-contract.md`）"
- [x] **T5.6** Modify：L151 转 Approved 条件 "附录 A Registry 22 个主题登记完成" → "附录 A Registry 22 个 v1 主题（当前 N 个，含后续 amendment 增量）"
- [x] **T5.7** Modify：L174 附录 A 标题 `(v1.6, 2026-08-28)` → `(v2.0, 2026-08-31)`
- [x] **T5.8** Modify：附录 A 末行（L242 之后）补登 3 行：
  - `| \`llm.token\` | stream_to_bus | 流式 token | \`session_id\`, \`token\`, \`index\` | ✅ |`
  - `| \`llm.token.done\` | stream_to_bus | 流式完成 | \`total_tokens\` | ✅ |`
  - `| \`llm.token.error\` | stream_to_bus | 流式错误 | \`error_code\`, \`message\` | ✅ |`
- [x] **T5.9** Verify：`grep -c "subscribe_glob" docs/adr/adr-0068-event-emission-contract.md` = 0
- [x] **T5.10** Verify：`grep -c "^| \`llm\.token" docs/adr/adr-0068-event-emission-contract.md` = 3
- [x] **T5.11** Verify：`grep "v2.0, 2026-08-31" docs/adr/adr-0068-event-emission-contract.md` ≥ 1
- [x] **T5.12** Commit：`docs(adr-0068): 措辞 + 版本号 + 计数 + llm.token 补登 (issue 5 + A2 + A3 + A4)`

## Phase 6: ADR-0030 死链 + ADR-0079 头部版本 + ADR-0019 状态注记（估时 0.2h）

- [x] **T6.1** Read ADR-0030 L325；ADR-0079 L4；ADR-0019 L5
- [x] **T6.2** Write failing assertion：期望 ADR-0030 不含指向不存在的 `./adr-0025-*.md` 链接
- [x] **T6.3** Modify：ADR-0030 L325 `[ADR-0025: 并行子任务](./adr-0025-parallel-subtasks.md) — Fleet 协议依据` → `- Fleet 并行执行：预留编号 ADR-0025（未创建）；FleetOrchestrator 已 DEFER，见 [adr-implementation-status-gap-analysis §2.1 ADR-0030](../../architecture/adr-implementation-status-gap-analysis.md)`
- [x] **T6.4** Modify：ADR-0079 §状态 L4 修订记录追加：`v1.2 amendment 2026-08-20：§决策 D7-D10（node-id 稳定寻址 / branch cursor 持久化 / path-extraction fork / 4 套存储命名空间分配）`
- [x] **T6.5** Modify：ADR-0019 L5 状态注记改："🟡 Partial (2026-07-06 更新 — glob 通配符订阅已 ship（`subscribe()` 支持 `*`/`?`，InMemoryBus Change B 2026-07-27）；带 layer 检查的独立 `subscribe_topic` API 当前零消费者，DEFER) — 待 follow-up PR ship `subscribe_topic` 扩展" 删除整段，保留"glob 已 ship"说明
- [x] **T6.6** Verify：`grep "\.md)" docs/adr/adr-0030-async-runtime-v2.md | grep "adr-0025"` 0 命中
- [x] **T6.7** Verify：`grep "v1.2 amendment" docs/adr/adr-0079-unified-session-4scope.md | head -1` 含 2026-08-20
- [x] **T6.8** Verify：`grep "subscribe_topic" docs/adr/adr-0019-iinteraction-bus-mvp.md` 命中数 ≥ 0（应已无"待 follow-up PR ship subscribe_topic"）
- [x] **T6.9** Commit：`docs(adr-0030/79/19): 死链/版本注记/glob 措辞 (issue 4 + 5 + A6)`

## Phase 7: ship gate 验证（0.1h）

- [x] **T7.1** `python3 tools/adr_lint.py` exit 0 且新增 ERROR = 0
- [x] **T7.2** `python3 tools/docs_drift_audit.py` 0 DRIFT
- [x] **T7.3** `openspec validate adr-0080-and-bus-api-alignment --strict` PASS
- [x] **T7.4** `grep -c "^| \`" docs/adr/adr-0068-event-emission-contract.md` 与 §决策 2 计数 N 自洽
- [x] **T7.5** `grep -c "^| \`" docs/adr/adr-0080-append-only-event-log.md` 较 baseline -18（D9 表移除）
- [x] **T7.6** `grep "subscribe_glob" docs/adr/adr-00{19,68}*.md` 0 命中
- [x] **T7.7** `grep "v1.2 amendment" docs/adr/adr-0080-*.md` ≥ 2（主文档 + amendment 双向）
- [x] **T7.8** `grep "^## 不变量" docs/adr/adr-0080-append-only-event-log.md` = 1
- [x] **T7.9** `grep "llm.token" docs/adr/adr-0068-event-emission-contract.md` 附录 A 内 ≥ 3 行
- [x] **T7.10** `git log --oneline` 检查所有 commit 存在
- [x] **T7.11** `openspec archive adr-0080-and-bus-api-alignment`（本任务交付完成）

## 总估时

- Phase 0: 0.05h
- Phase 1: 0.2h
- Phase 2: 0.3h
- Phase 3: 0.3h（**最易改错**）
- Phase 4: 0.1h
- Phase 5: 0.3h
- Phase 6: 0.2h
- Phase 7: 0.1h
- **总估时: ~1.5h**（纯文档勘误）

## 关键不变量（强制遵守）

- ❌ 任何代码改动（仅 5 个 ADR 文件）
- ❌ 任何测试改动
- ❌ 任何公开 API 改动（IInteractionBus / EventBuilder / ToolResult 等）
- ❌ 任何 ctest 数字硬编码
- ❌ 凭印象改 ADR 内容（必须实读被引用的代码行）
- ❌ 在 linter 失败时强行 commit

## 明确 out of scope (需独立 change)

- **6 幻影主题机制修复**（attempt.*/conversation.*/phase.completed/branch.created/attempt.converged 有消费者零发射方）—— 需在 `pdk/loop_agent/` `pdk/plan_execute_loop` `pdk/fork_join_loop` `examples/pdk_chat_demo/chat_session` 等位置加 emit 调用点
- **ADR-0019 状态翻 Approved**：glob 已 ship 后判定（独立 review）
- **ADR-0080 v1.3 同步**：本 change 同步到 v2.0
- **`session_writer.cpp` 13 topic whitelist → ADR-0068 附录 A 全量注册**（与 6 主题机制修复同批次）
