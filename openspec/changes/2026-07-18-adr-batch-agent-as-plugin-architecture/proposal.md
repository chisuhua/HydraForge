# ADR Batch — Agent-as-Plugin Architecture (14 ADRs)

**STATUS**: 🔍 Proposed
**日期**: 2026-07-18
**关联**: docs/adr/adr-0052~0065.md, docs/adr/skill/adr-0061-{01..12}.md

## Why

Sprint 24 (2026-07-15 ~ 2026-07-17) Phase A 端到端 demo (`examples/pdk_chat_demo`) + 6 个 PDK Agent Plugin (loop/provider/session/budget/fs/shell) 实施期间，产生了 **14 个顶层 ADR + 12 个 skill 子 ADR**，但这些 ADR 在 git 上**没有任何 OpenSpec change 关联**，违反项目的 OpenSpec governance 规则 (AGENTS.md §CONVENTIONS: "OpenSpec change 是 ship 的实施记录")。

本 change **纯治理目的**：建立 OpenSpec change ↔ ADR 的双向链接，作为 14 个 ADR 的 ship gate 跟踪载体。**不实施新功能**（所有 ADR 内容已存在 `docs/adr/`）。

## What Changes

### 14 个顶层 ADR 关联 (docs/adr/adr-0052 ~ adr-0065)

| ADR | 主题 | 关联代码/Sprint |
|-----|------|----------------|
| **ADR-0052** agent-plugin-manifest | PDK Plugin 元数据 schema (name/abi/capabilities/io_schema) | pdk/loop_agent/pdk_manifest.json 等 6 份 |
| **ADR-0053** agent-descriptor-interface | AgentDescriptor/AgentForm 接口设计 (v2 仅设计,未实施) | 预留 ADR, Sprint 25+ 实施 |
| **ADR-0054** capability-discovery | Plugin 能力发现协议 (capabilities → matched tools) | pdk/*/pdk_manifest.json `capabilities` 字段 |
| **ADR-0055** skill-isolation | Skill 沙箱隔离 (memory/fs/network 边界) | 预留 ADR, Sprint 26 实施 |
| **ADR-0056** wasm-runtime | WASM runtime 作为 skill 默认执行环境 | 预留 ADR, Sprint 27 实施 |
| **ADR-0057** agent-lifecycle | Agent 生命周期 (init→run→pause→resume→terminate) | 预留 ADR, Sprint 25 实施 |
| **ADR-0058** tool-schema-validation | Tool I/O schema 验证 (JSON Schema / inja) | pdk/*/pdk_manifest.json `input_schema/output_schema` |
| **ADR-0059** cross-process-protocol | Cross-process agent 通信协议 (类似 LSP) | 预留 ADR, Sprint 28 实施 |
| **ADR-0060** agent-composition | Agent 组合 (sequential/parallel/hierarchical) | 预留 ADR, Sprint 25 实施 |
| **ADR-0061** agent-evolution-and-solidification | Agent 自我进化 + 凝固为 skill | 12 个子 ADR, Sprint 29+ 实施 |
| **ADR-0062** agent-marketplace | Agent 市场 (publish/discover/versioning) | 预留 ADR, Sprint 30 实施 |
| **ADR-0063** opentelemetry-tracing | OTel tracing 集成 (跨 plugin 链路追踪) | 预留 ADR, Sprint 26 实施 |
| **ADR-0064** pdk-conformance-test-suite | PDK 一致性测试套件 (ABI/manifest/tools) | 预留 ADR, Sprint 25 实施 |
| **ADR-0065** multi-language-pdk-python | 多语言 PDK (Python 绑定 + pybind11) | 预留 ADR, Sprint 30+ 实施 |

### 12 个 Skill 子 ADR (docs/adr/skill/adr-0061-{01..12}.md)

均为 ADR-0061 (Agent Evolution & Solidification) 的实施拆分，主题涵盖:
- `01-skill-std.md`: Skill 标准化格式
- `02-behavioral-regression.md`: 行为回归测试
- `03-skill-compiler.md`: Skill 编译器 (DSL → binary)
- `04-slm-routing.md`: 小模型路由 (低成本演化)
- `05-cpp-wasm-toolchain.md`: C++ → WASM 工具链
- `06-trajectory-ir.md`: 执行轨迹 IR (演化数据)
- `07-paste-speculation.md`: 粘贴推测 (UI 演化触发)
- `08-aflow-search.md`: AFlow 搜索 (演化算法)
- `09-gepa-loop.md`: GEPA loop (演化循环)
- `10-formal-lint.md`: 形式化 lint (演化质量门禁)
- `11-dsl-wasm.md`: DSL → WASM 编译
- `12-webllm.md`: WebLLM (浏览器内执行)

### 现有 PDK Agent Plugin 关联 (已 ship)

| Plugin | ADR 关联 | 文件 |
|--------|---------|------|
| `pdk/loop_agent/` | ADR-0021 PDK 基础, ADR-0052 manifest | 3 files |
| `pdk/provider_agent/` | ADR-0021, ADR-0052 | 6 files |
| `pdk/session_agent/` | ADR-0021, ADR-0052 | 5 files |
| `pdk/budget_agent/` | ADR-0021, ADR-0052 | 5 files |
| `pdk/fs_tools/` | ADR-0021, ADR-0052, ADR-0055 (isolation) | 3 files |
| `pdk/shell_tools/` | ADR-0021, ADR-0052, ADR-0055 (危险命令黑名单) | 3 files |

### 现有 Demo 关联 (已 ship)

| 资产 | ADR 关联 | 文件 |
|------|---------|------|
| `examples/pdk_chat_demo/` | 上述全部 (端到端验证) | 12 files |
| `lib/loop/react.agent.md` | ADR-0021, ADR-0057 (lifecycle: react) | 1 file |
| `skills/code-review/` | ADR-0061 (skill isolation/compiler) | 2 files |

## Capabilities

### New Capabilities
无 (本 change 仅为治理链接, 不新增 capability)

### Modified Capabilities
无 (不修改任何已 ship 的 spec/capability)

## Impact

- **影响范围**: 0 代码变更, 仅 OpenSpec governance 补全
- **依赖**: 无前置依赖
- **后续动作**: 每个 ADR 进入 Sprint 25+ 实施时, 创建独立 OpenSpec change (e.g. `2026-MM-DD-adr-0052-agent-plugin-manifest-impl/`)
- **风险**: 0 (纯文档治理)