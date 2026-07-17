# Tasks — ADR Batch Agent-as-Plugin Architecture

**关联**: `proposal.md`

## Day 1 (2026-07-18, 治理 ship)

- [x] 1.1 创建 OpenSpec change: `openspec/changes/2026-07-18-adr-batch-agent-as-plugin-architecture/`
- [x] 1.2 编写 proposal.md (Why / What / Impact / 14 ADR 表)
- [x] 1.3 编写 tasks.md (本文件)
- [x] 1.4 编写 design.md (架构骨架 - ADR 之间的关系图)
- [x] 1.5 验证: `openspec validate 2026-07-18-adr-batch-agent-as-plugin-architecture --strict` exit 0

## Day 1.5 (Sprint 24 Day 5, ADR 状态保持)

- [x] 2.1 14 个顶层 ADR 状态保持 🔍 Proposed (未 ship)
- [x] 2.2 12 个 Skill 子 ADR 状态保持 🔍 Proposed
- [x] 2.3 6 个 PDK Plugin + 1 demo 已 ship, 不需要本 change 重复验证

## Day 2+ (Sprint 25 起, 顺延)

- [ ] 3.1 ADR-0053 AgentDescriptor v2 实施 (Sprint 25)
- [ ] 3.2 ADR-0057 Agent Lifecycle 实施 (Sprint 25)
- [ ] 3.3 ADR-0064 PDK Conformance Test 实施 (Sprint 25)
- [ ] 3.4 ADR-0060 Agent Composition 实施 (Sprint 25)
- [ ] 3.5 ADR-0055 Skill Isolation 实施 (Sprint 26)
- [ ] 3.6 ADR-0063 OTel Tracing 实施 (Sprint 26)
- [ ] 3.7 ADR-0056 WASM Runtime 实施 (Sprint 27)
- [ ] 3.8 ADR-0059 Cross-process Protocol 实施 (Sprint 28)
- [ ] 3.9 ADR-0061 Agent Evolution (12 子 ADR 拆分 ship) (Sprint 29+)
- [ ] 3.10 ADR-0062 Agent Marketplace 实施 (Sprint 30)
- [ ] 3.11 ADR-0065 Multi-language PDK Python 实施 (Sprint 30+)

每个 Sprint 实施对应 ADR 时, 单独创建 OpenSpec change (e.g. `2026-MM-DD-adr-0053-agent-descriptor-impl/`) 跟踪实施 + ADR 状态翻转 (Proposed → Approved)。