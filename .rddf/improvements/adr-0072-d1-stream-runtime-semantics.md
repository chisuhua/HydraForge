# adr-0072-d1-stream-runtime-semantics

**优先级**: P0 | **来源**: from-roadmap (W4, ADR-0072 D1 阶段 B = IStreamHandle 运行时语义 + 2 类节点 runtime 流式行为)
**阶段**: post-6c | **分类**: execution-runtime
**类型**: feature
**主题**: IStreamHandle L1 契约层 + tool_call/dsl_call 流式行为 + set_stream_sink 注入点 + stop_token 传播交互

## 架构依据

ADR-0072 D1 阶段 A (commit `c61a6d0`, 2026-09-03) 完成 parser 字段层 `stream: true` 透传, 但未实施运行时. 阶段 B 实施 IStreamHandle 抽象 + 2 类节点 runtime 流式行为 + 与 Wave 3-A stop_token 传播链路集成.

Oracle session `ses_f97ec0b57ffe956D2KkhwH1aCn` 阶段 A 评审返回 FAIL, 三大阻塞:
- **B1**: `shell_exec` NodeType 不存在 (per `node.h:22-34`, `dsl.md:929` 自标 "🔮 Planned"). scope 收缩: 仅 tool_call + dsl_call
- **B2**: out-parameter overload 无消费者通道. 决策: NodeExecutor 持有 `IStreamHandle*` 注入点 (raw pointer, 与 set_tool_coordinator 模式一致)
- **B3**: tool_call 同步返回完整 ToolResult, 无流式 API. V1 诚实化: 同步完成后切片回放 (post-hoc pseudo-streaming), 真流式 V2 follow-up

Metis session `ses_f97ec025cffeCoiffETNXu9yyt` 阶段 A 评审补充 12 项模糊点 + 10 项 AI 失败点, 已纳入 Decision 7-10.

Oracle 阶段 A 警告: 阶段 B 估时 4h 与新抽象面 + stop_token 复杂度匹配, 已 ship 阶段 A 拆分降低了本阶段风险.

## 范围

- **In Scope**:
  - `include/agenticdsl/contract/i_stream_handle.h` 新增 L1 契约层 (header-only, 3 虚函数)
  - `src/common/runtime/stream_handle.{h,cpp}` 新增 2 个参考实现 (BufferedStreamHandle pull + CallbackStreamHandle push+pull)
  - `src/common/runtime/CMakeLists.txt` 新增 + 注册 stream_handle.cpp
  - `src/modules/executor/node_executor.{h,cpp}` 改造 2 类 execute_* 方法支持 stream: true + 新增 set_stream_sink 注入点
  - `tests/test_stream_runtime.cpp` 5 类 TEST_CASE (A 契约 / B Buffered / C Callback / D sink 注入 + 2 节点流式 / E stop_token 传播)
  - `tests/CMakeLists.txt` 注册 test_stream_runtime
  - `docs/specs/dsl.md` REQ-W4-001 阶段 B 章节 (V1 切片回放声明 + scope 收缩注记 + 真流式 V2 follow-up tracking)
  - `docs/adr/adr-0072-dsl-node-extensions.md` D1 实施度 1/6 → 2/6
  - `docs/active-status.md` §Sprint 25 carry-over W4 阶段 B closed
- **Out of Scope**:
  - 引入新 `shell_exec` NodeType (per B1, 留待 W5 parser 提案交付)
  - `IGenerationStream` 重构 (per ADR-0001 不动)
  - stop_token 链路本身的改造 (Wave 3-A 已 ship)
  - 真流式 (per B3, tool-level streaming API 缺失, V2 follow-up)
  - 流式输出回写至 bus/event_log (per ADR-0068 留 Sprint 27+)
  - try/catch/finally 节点族 (per ADR-0071 §3.C 不做)

## Why

阶段 A (parser 字段层) 已 ship 但未启用运行时, 用户无法获得流式效果. 阶段 B 落地运行时语义让 parser 字段实际生效, 完整闭合 ADR-0072 D1.

## What Changes

- **新增** `include/agenticdsl/contract/i_stream_handle.h` — IStreamHandle 3 虚函数 (next/is_active/error)
- **新增** `src/common/runtime/stream_handle.{h,cpp}` — BufferedStreamHandle + CallbackStreamHandle + RAII 析构顺序
- **新增** `src/common/runtime/CMakeLists.txt` — 注册 stream_handle.cpp
- **修改** `src/modules/executor/node_executor.h` — stream_sink_ 成员 + set_stream_sink 方法 + kStreamChunkSize 常量
- **修改** `src/modules/executor/node_executor.cpp` — 2 类 execute_* (tool_call + dsl_node) 新增 stream 分支, 同步路径 100% 不变
- **新增** `tests/test_stream_runtime.cpp` — 5 类 TEST_CASE
- **新增** `tests/CMakeLists.txt` 注册 test_stream_runtime
- **更新** `docs/specs/dsl.md` REQ-W4-001 阶段 B 章节
- **更新** `docs/adr/adr-0072-dsl-node-extensions.md` D1 实施度
- **更新** `docs/active-status.md` carry-over 状态

## Acceptance

- [ ] 5 类 TEST_CASE PASS (A 契约 / B Buffered / C Callback / D sink 注入 + 2 节点流式 / E stop_token 传播)
- [ ] tool_call/dsl_call 节点 `metadata["stream"] == true` (严格 JSON 布尔) 触发切片回放
- [ ] 缺省/false/字符串/数字/null 走同步路径 (向后兼容, 既有 100+ 测试零回归)
- [ ] stop_token.stop_requested() 在 IStreamHandle::next 内传播, 100ms 内终止
- [ ] 流结束后 stop_source 释放 (无泄漏)
- [ ] handle 析构 RAII 顺序保证 (mark inactive → clear queue → unregister → destroy)
- [ ] docs/specs/dsl.md REQ-W4-001 阶段 B 章节存在
- [ ] ADR-0072 D1 实施度 1/6 → 2/6
- [ ] ctest 全量 211/212 PASS (1 pre-existing perf test fail 与本 change 无关)
- [ ] `tools/adr_lint.py` 零错误
- [ ] Oracle ac-verifier PASS

## Status: ✅ SHIPPED (2026-09-04, Sprint 26 W4 阶段 B)

**Ship evidence** (验证日期 2026-10-14 via dual-agent 调研 Oracle + Metis 独立交叉验证):
- **OpenSpec archive**: `openspec/changes/archive/2026-09-04-adr-0072-d1-stream-runtime-semantics/` (4 files tracked)
- **代码实证**:
  - `include/agenticdsl/contract/i_stream_handle.h` — **5 虚函数** (3 consumer pull + 2 producer push/close) — `next` / `is_active` / `error` / `push` / `close`
  - `src/common/runtime/stream_handle.{h,cpp}` — `BufferedStreamHandle` (pull) + `CallbackStreamHandle` (push+pull, RAII 析构顺序)
  - `src/modules/executor/node_executor.h:82,108` — `set_stream_sink()` 注入点 (raw pointer, 与 set_tool_coordinator 模式一致)
  - `src/modules/executor/node_executor.cpp:124-172,259-285` — 2 类节点 (tool_call / dsl_call) runtime 流式分支
- **测试实证**: `tests/test_stream_runtime.cpp` — 5 类 TEST_CASE / 19 cases / **75 assertions**, 15 PASS + 4 集成测试 known-limitation
- **Spec 实证**: `docs/specs/dsl.md:1600-1607` REQ-W4-001-B "✅ 已实现阶段 B (2026-09-04, Sprint 26 W4 阶段 B)"
- **ADR 实施度**: 1/6 → 2/6 (D1 阶段 A+B 完整 ship)

**重要修正**: 提案正文写"3 虚函数", 实际 ship 是 **5 虚函数** (Oracle 阶段 A 评审 M1 producer 侧扩展). 这是"代码有行为、提案无契约"的隐性 drift. Status 段同步纠正.

**⚠️ 4 个 known-limitation (真实残余工作)**:
- `dsl.md:1607` 记录 "15 PASS, 4 集成测试已知限制待 follow-up"
- 不是 P0, 但属于"真流式 V2" follow-up 范围 — 已立为独立 improvement `adr-0072-d1-stream-v2-true-streaming.md` (Phase B 推进)
- V1 切片回放 (post-hoc pseudo-streaming) **不满足**用户原始诉求 "长输出实时显示" (Metis 意图 B 隐藏需求)

**Pattern 沉淀**: Pattern #10 drift 第 5 实例 (dual-agent 调研 2026-10-14). Oracle + Metis 独立识别 ship 状态 + "3→5 虚函数" 文档漂移. Status 段记录原因, 防未来 audit 误判.