# adr-0072-d4-backend-parser

**优先级**: P0 | **来源**: from-roadmap (W5, ADR-0072 D4 `backend:` 字段接入 parser)
**阶段**: post-6c | **分类**: execution-parser
**类型**: feature
**主题**: backend: 字段解析；env:→env_vars: 别名；ADR-0075 EnvBackend 闭环

## 架构依据

ADR-0072 D4 `backend:` 字段 + ADR-0075 EnvBackend (D1+D2+D3+D5 已 ship 2026-08-18)：

- ADR-0075 EnvBackend 已实现 `IEnvBackend` + `LocalBackend` + `DockerBackend` + `EnvValidationHook` + `BackendPolicy`
- 但 `backend:` 字段**未接入 parser** — 节点声明 backend 后无法被 ToolCoordinator 读取
- ADR-0072 D4 强制必补：`backend: docker` / `backend: local` 字段解析 + 存入 node metadata
- `env:` → `env_vars:` 别名（向后兼容既有 DSL 中 `env:` 写法）

## 范围

- **In Scope**:
  - `src/modules/parser/markdown_parser.{h,cpp}` 新增 `backend:` 字段解析
  - `src/modules/parser/node_factory.cpp` parse_context 提取 `backend` / `env` → `env_vars` 到 metadata
  - `tests/test_dsl_extensions.cpp` 新增 3 类测试（backend 字段 / env 别名 / 未知 backend 警告）
  - `docs/specs/dsl.md` §6 新章节（backend: + env_vars: 字段文档）
- **Out of Scope**:
  - IEnvBackend 接口改动（已 ship）
  - ToolCoordinator 运行时 backend 校验逻辑（→ EnvValidationHook 已 ship）
  - DockerBackend 实际执行（已 ship）

## Why

ADR-0075 EnvBackend 已 ship 但 parser 未接线 — `backend:` 字段在 DSL 中无效，节点无法声明执行环境。ADR-0072 D4 是 W5 (2h) 强制必补项（per Oracle session `ses_f9ab25dcfffetx4J5UFA7JYBKV` 提案 #3 P0）。

## What Changes

- **修改** `src/modules/parser/node_factory.cpp` — parse_context 提取 backend/env/env_vars
- **修改** `tests/test_dsl_extensions.cpp` — 3 类新测试
- **新增** `docs/specs/dsl.md` §6 章节

## Acceptance

- [ ] 3 类 pytest PASS（backend 字段解析 / env 别名 / 未知 backend 处理）
- [ ] `backend: docker` 节点 metadata["backend"] == "docker"
- [ ] `env: {K: V}` 节点 metadata["env_vars"] == {"K": "V"}
- [ ] 旧 DSL 兼容（无 backend/env 字段解析不变）
- [ ] docs/specs/dsl.md §6 新章节存在
- [ ] ADR-0072 D4 实施度从 1/6 → 2/6
- [ ] ctest 全量无回归

## Status: ✅ SHIPPED (2026-09-03)

**Ship evidence** (验证日期 2026-10-14 via dual-agent 调研 Oracle + Metis 独立交叉验证):
- **OpenSpec archive**: `openspec/changes/archive/2026-09-03-adr-0072-d4-backend-parser/` (4 files tracked)
- **代码实证**: `src/modules/parser/node_factory.cpp:37-47` (backend 提取 + `env:`→`env_vars:` 别名 + 并存时 `env_vars:` 优先)
- **测试实证**: `tests/test_dsl_extensions.cpp:410-509` W5 测试 (backend/docker + backward-compat)
- **Spec 实证**: `docs/specs/dsl.md:1548-1553` REQ-W5-001 (含未知 backend 容错语义)
- **ADR 翻牌**: `docs/adr/adr-0072-dsl-node-extensions.md:13` D4 行 ❌ 未实施 → ✅ 已 ship (2026-09-03) — **同步翻牌**, 否则 drift 只修一半
- **ADR 维持 🟡 Partial**: D1/D4 ship 仅达到 §3.A 必补门槛, 仍需 Evidence Gate 真实 3 模型重测 (Sprint 25+ carry-over)

**关键细节** (Metis 独立识别):
- 提案验收只写"`env:` {K:V} 节点 metadata['env_vars'] == {...}", 没提**并存场景优先级**. 实施者已 ship 额外加 "并存时 `env_vars:` 胜出" 安全规则 (line 45-47). 这是"代码有行为、提案无契约"的隐性 drift, 文档已同步
- **隐藏缺口 (Metis 意图 D)**: env_vars secrets redaction 协议未覆盖 (ADR-0068 §3.1 不含 env_vars 场景). 已立为独立 improvement `adr-0072-d4-env-vars-redaction.md` (Phase B 推进)

**Pattern 沉淀**: Pattern #10 drift 第 7 实例 (dual-agent 调研). ADR-0072:13 状态行 2026-09-04 阶段 B ship 时漏翻, dual-agent 调研 (2026-10-14) 才识别. Status 段记录原因, 防未来同类 partial-sync 漏掉.
