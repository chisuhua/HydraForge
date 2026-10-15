# adr-0072-d1-stream-v2-true-streaming

**优先级**: 🟠 P1 | **来源**: dual-agent 调研 (2026-10-14) — Metis 意图 B 隐藏需求
**阶段**: post-6c | **分类**: execution-runtime
**类型**: feature
**主题**: 真正增量流式输出 (非切片回放); 满足用户原始诉求 "长输出实时显示"

## 架构依据

per 2026-10-14 dual-agent 调研 (`ses_ec5f36ffaffeT3KEsoFqAUPq43`):
- **ADR-0072 D1 阶段 B (2026-09-04) ship** "切片回放 (post-hoc pseudo-streaming)" — V1 诚实化
- **V1 ≠ 用户真实诉求**: 用户原始需求 (per `adr-0072-dsl-node-extensions.md:243`) "tool_call/shell.exec 无法流式输出 (用户需求: 长输出实时显示)"
- **V1 切片回放 = 同步完成后切片重放, 不是增量输出** — 表面像流式, 实际仍是同步等待
- **4 个 known-limitation** (`dsl.md:1607` 记录) — 是真实残余工作, 提案 v1 已 ship 但未完成
- V1 V2 分界: V1 ship 时提案明确 "真流式 V2 follow-up", 但 V2 依赖 IToolRegistry streaming API (ToolMetadata V3, ADR-0073) — 不是 4h 活, 是独立 change

## Why

### V1 切片回放的 4 个 known-limitation (per dsl.md:1607)

1. **同步阻塞**: 工具调用仍需完整执行结束才能"开始流式" — LLM 不能中断, 用户看到的是"完成 → 突然开始显示"不是"边执行边显示"
2. **UTF-8 切片边界 bug**: `kStreamChunkSize=64` 字节切片可能在 CJK 多字节字符中间截断, 产生无效 UTF-8 chunk (Metis 失败模式 5)
3. **Chunk 大小固定**: 64 字节对 ASCII OK, 对长行 / 短响应 / 结构化数据不友好
4. **与 stop_token 集成仅 consumer 侧**: producer 侧 (push) 不感知 stop_token, 取消信号无法从 DSL 传到上游

### 真实流式 V2 需要

- **工具 API 流式化**: `IToolRegistry::call_tool_stream(req, token, sink)` 而非 `call_tool(req)` 同步返回
- **依赖 ToolMetadata V3 (ADR-0073)**: 工具需要声明 streaming capability, 否则无法用 V2
- **流式回写至 bus/event_log** (per ADR-0068 留 Sprint 27+): 跨消费者可见

## Status: 🔴 OPEN (2026-10-15 audit verification, 真待 ship)

**Status 段补全** (per Pattern #10 hygiene):

**5-维判定**:
- 维度 1 self-declared: header 无 SUPERSEDED, 无 status
- 维度 2 archive: ❌ (本提案为 dual-agent 调研新立, 2026-10-14 commit `3e33929` Phase B 立项)
- 维度 3 commit: ✅ commit `3e33929` (立 4 个新独立 improvement) — 不是 ship, 是 proposal creation
- 维度 4 ADR: ADR-0072 自身 🟡 Partial (D1 stream:true 未实施)
- 维度 5 fallback: V1 ship 2026-09-04 (切片回放) ≠ V2 真流式 (本提案范围)

**判定依据**:
- 本提案对应 ADR-0072 D1 增量 ship (V2 真流式, 非 V1 切片回放)
- V2 真流式需要 IToolRegistry streaming API + ToolMetadata V3 (ADR-0073) + ADR-0068 留 Sprint 27+ bus/event_log 流式回写
- effort 估: 3-5 day (1 day IToolRegistry stream API + 1 day tool executor 改造 + 1 day DSL parser + 1 day tests + 1 day integration)

**判定结论**: 🔴 OPEN (真待 ship, 需独立 OpenSpec change + 24h cooling-off 后 TDD 5-step 实施)

**触发条件** (per proposal §触发条件):
- 用户要求 "真流式 V2 长输出实时显示"
- 任何 tool_call/shell.exec 长输出阻塞 bug 发现
- Wave 4+ 启动需要真流式基础

**Pattern 沉淀**: dual-agent 调研立项新 OPEN 提案 (per 2026-10-14 Phase B), 不是 ship drift 是真待 ship, 与类 A GATED / 类 C ship drift 区分. 本提案与 `fix-stream-handle-sink-lifecycle` (B2) + `fix-stream-handle-utf8-chunk-boundary` (B3) + `adr-0072-d4-env-vars-redaction` (B4) 同属 dual-agent Phase B 立 4 个 improvement (per commit `3e33929`).
- **stop_token 双侧传播**: producer 侧需要 handle 取消

## What Changes (proposal scope)

- **V2 步骤 1**: IToolRegistry 新增 `call_tool_stream(req, sink, token)`, 保留 `call_tool` 兼容
- **V2 步骤 2**: ToolMetadata V3 (ADR-0073) 增 `streaming: bool` 字段
- **V2 步骤 3**: NodeExecutor `execute_tool_call` 改走 stream 路径 (if metadata.streaming)
- **V2 步骤 4**: 解决 UTF-8 切片边界 (单独 improvement `fix-stream-handle-utf8-chunk-boundary.md`)
- **V2 步骤 5**: 流式 event 主题 (per ADR-0068 Appendix A 后续 amendment)

## Out of Scope (独立 improvement)

- UTF-8 切片 bug fix — 独立 `fix-stream-handle-utf8-chunk-boundary.md` (Phase B3)
- sink pointer RAII 生命周期 — 独立 `fix-stream-handle-sink-lifecycle.md` (Phase B4)
- 流式 event 协议扩展 — ADR-0068 amendment follow-up

## Dependencies

- **前置 ship**: ADR-0073 ToolMetadata V3 (per `adr-0073:73` 已 ship 部分, V3 完整待启动)
- **ADR-0075 EnvBackend 闭环** — 与本 change 正交, 不阻塞
- **4 个 known-limitation fix** — 应在 V2 步骤 1 之前 ship, 否则 V2 仍不完整

## Acceptance Criteria (建议)

- AC-1: IToolRegistry `call_tool_stream()` 实现, MockLLMProvider 兼容
- AC-2: ToolMetadata V3 `streaming: bool` 字段实现
- AC-3: NodeExecutor `execute_tool_call` stream 路径全过 (5 个集成 known-limitation 关闭 ≥3)
- AC-4: UTF-8 切片边界 fix (依赖 Phase B3)
- AC-5: stop_token 双侧传播 (producer + consumer) 集成测试 PASS
- AC-6: 流式 event 主题 (per ADR-0068) 实现 + 4 known-limitation 全部关闭

## Estimated Effort

- V2 步骤 1-3: 2-3 天 (与 V1 同量级, 但需 IToolRegistry 扩展)
- V2 步骤 4-5: 1-2 天 (UTF-8 fix + event 协议)
- 总: 3-5 天 (per 2026-10-14 dual-agent 调研)

## Reference

- V1 ship: `openspec/changes/archive/2026-09-04-adr-0072-d1-stream-runtime-semantics/` (commit per archive)
- 4 known-limitation: `docs/specs/dsl.md:1607`
- ToolMetadata V3: `docs/adr/adr-0073-tool-json-schema-contract.md`
- Dual-agent 调研: `.rddf/audits/2026-10-14-p0-proposals-dual-agent-review.md`

## Status

📋 **登记但未立项** (per 2026-10-14 dual-agent 调研, Metis 意图 B 隐藏需求发现). 待 ADR-0073 V3 + 4 known-limitation fix ship 后, 独立 OpenSpec change 立项实施.
