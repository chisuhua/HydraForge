# adr-0072-d1-stream-true-parser

**优先级**: P0 | **来源**: from-roadmap (W4, ADR-0072 D1 `stream: true` 字段接入 parser, 阶段 A = parser 字段透传)
**阶段**: post-6c | **分类**: execution-parser
**类型**: feature
**主题**: stream: true 字段解析；节点 metadata 透传；阶段 B (IStreamHandle 语义) 留 Sprint 26

## 架构依据

ADR-0072 D1 `stream: true` 全局支持（3 类节点 tool_call/shell.exec/dsl_call 流式行为 + IStreamHandle 抽象）。

Oracle session `ses_f9ab25dcfffetx4J5UFA7JYBKV` 警告：估时 6h 大概率低估，建议拆两阶段：
- **阶段 A (本 change, 2h)**: parser 字段透传 — 仅存储到 node.metadata，不实施运行时行为
- **阶段 B (Sprint 26, 4h)**: IStreamHandle 新契约 + 3 类节点运行时实现 + 与 `generate_stream(req, token)` 交互

## 范围

- **In Scope**:
  - `src/modules/parser/node_factory.cpp::parse_context` 新增 `stream` 字段提取
  - `tests/test_dsl_extensions.cpp` 新增 3 类 W4 测试
  - `docs/specs/dsl.md` REQ-W4-001 章节
- **Out of Scope**:
  - IStreamHandle 新契约（→ Sprint 26 阶段 B）
  - 3 类节点运行时流式行为
  - 与 stop_token 传播交互（→ Sprint 26）

## Why

`stream: true` 字段是 ADR-0072 D1 的解析层落地。阶段 A 先让 parser 识别并存储到 metadata，确保下游 IStreamHandle 设计时已有解析数据可用。

## What Changes

- **修改** `src/modules/parser/node_factory.cpp` — parse_context 新增 stream 字段提取 (+3 行)
- **修改** `tests/test_dsl_extensions.cpp` — 新增 3 类 W4 TEST_CASE
- **新增** `docs/specs/dsl.md` REQ-W4-001 章节

## Acceptance

- [ ] 3 类 pytest PASS（stream true 解析 / stream false 解析 / stream 缺省向后兼容）
- [ ] `stream: true` 节点 metadata["stream"] == true
- [ ] `stream: false` 节点 metadata["stream"] == false
- [ ] stream 缺省时 metadata 不含 "stream" key
- [ ] docs/specs/dsl.md REQ-W4-001 章节存在
- [ ] ADR-0072 D1 实施度 0/6 → 1/6（仅字段层）
- [ ] ctest 全量无回归

## Status: ✅ SHIPPED (2026-09-03, commit c61a6d0)

**Ship evidence** (验证日期 2026-10-14 via dual-agent 调研 Oracle + Metis 独立交叉验证):
- **Commit**: `c61a6d0` (2026-09-03)
- **OpenSpec archive**: `openspec/changes/archive/2026-09-03-adr-0072-d1-stream-true-parser/` (4 files tracked)
- **代码实证**: `src/modules/parser/node_factory.cpp:49-52` `if (json.contains("stream")) { ctx.metadata["stream"] = json["stream"]; }`
- **测试实证**: `tests/test_dsl_extensions.cpp:514-566` 3 个 W4 TEST_CASE (stream-true / stream-false / backward-compat)
- **Spec 实证**: `docs/specs/dsl.md:1572` REQ-W4-001 "✅ 已实现阶段 A (2026-09-03, commit c61a6d0)"
- **ADR 实施度**: 0/6 → 1/6 (仅字段层) — per `dsl.md:1590`

**Note**: 提案原写 "3 类 pytest PASS" — 项目是 Catch2/ctest 不是 pytest, 实施时已用 Catch2 措辞 (3 TEST_CASE 表述正确). Status 段同步纠正措辞.

**Pattern 沉淀**: Pattern #10 drift 第 6 实例, 详见 `.rddf/audits/2026-10-14-p0-proposals-dual-agent-review.md`