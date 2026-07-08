# Proposal: ADR 命名政策落地 (fix-adr-naming-policy-2026-07-08)

> **STATUS: PLACEHOLDER** 🟡
> **创建日期**: 2026-07-08
> **创建者**: Sisyphus
> **关联 hotfix**: `openspec/changes/fix-adr-doc-alignment-hotfix-2026-07-08/` (Change A, 30 min ship)
> **关联原始 mega-change**: `openspec/changes/fix-adr-doc-alignment-2026-07-08/` (47 tasks 拆分记录)
> **关联审查**: 2026-07-08 架构文档对齐审查 + Metis 审查 `ses_0bf414b4affe5zB7zN06vHudKN`
> **关联 ADRs**: ADR-0034 (Model Router, Approved), ADR-0043 (PDK Naming, Proposed)
> **估时**: ~2 h (待与 B2 owner 协调时序后启动)
> **优先级**: 中 (依赖 B2 启动时序, 不阻塞 hotfix ship)

## Why

本 change 是 `fix-adr-doc-alignment-2026-07-08` (47 tasks) 经 Metis 审查后拆分的 **Change B (命名政策落地)**。

Change A (hotfix) 已 ship 修复了 4 个 P0/P1 问题 (STATUS-GLOSSARY 状态 + README 拼写 + D5 step 编号 + ADR-0036 归档)。但 **3 个 P0 命名问题未在 hotfix 中处理**：

1. **P0-2**: 5 个 C13 `lib/inference/*.md` 文件工具名 DOT 风格 (`prefix_cache.configure` 等)
2. **P0-3**: `decisions-2026-07-07.md` D3 整章 DOT 风格映射表 + "C13 架构工具命名边界" 错误小节
3. **命名政策落地**: 需要同步修改 PDK 代码 (`pdk/llama_engine/src/*.cpp`) + C13/C14/C15 spec/tasks/tests 中所有 DOT 风格工具名引用

**为什么 now**:
- P0-2 是 B2 实施 (C13/C14/C15) 启动前置, 否则 B2 实施会按旧 DOT 写新代码
- P0-3 是决策文件本身的事实修正, 应在 B2 启动前完成
- **必须与 B2 owner 协调时序**: 在 B2 实施启动前 ship, 否则 B2 实施可能与命名政策冲突

**为什么不在 hotfix 中**:
- Metis 审查指出: P0-2 SLASH 化需同步改 PDK 代码 (违反 hotfix "零代码变更" 原则)
- 跨多文件同步涉及 C13/C14/C15 三个 OpenSpec change owner, 需协调时序
- hotfix 30 min 启动 + ship 的设计不适合"协调性变更"

## What Changes (计划, 待 Metis/Oracle 启动后细化)

### 1. C13 5 个 `lib/inference/*.md` DOT → SLASH

- `lib/inference/prefix_cache.md`: `prefix_cache.configure` → `prefix_cache/configure`
- `lib/inference/kv_cache.md`: `kv_cache.configure` → `kv_cache/configure`
- `lib/inference/decoding.md`: `decoding.configure` → `decoding/configure`
- `lib/inference/cloud_engine.md`: `cloud_engine.configure` → `cloud_engine/configure`
- `lib/inference/batching.md`: `batching.submit_and_wait` → `batching/submit_and_wait`

### 2. `decisions-2026-07-07.md` D3 整章重写

- 删除 DOT 风格映射表 (8 行)
- 明确"统一 SLASH 格式 `inference/engine/init`"
- 删除"C13 架构工具命名边界"小节 (与 ADR-0034 §命名约定矛盾)

### 3. PDK 代码 + C13/C14/C15 spec/tasks/tests 同步

- `pdk/llama_engine/src/inference_arch.cpp`: 工具名注册同步
- `tests/test_llama_engine_plugin.cpp`: 测试断言同步
- `openspec/changes/phase5-b2-arch-schemas/proposal.md`: C13 工具名引用同步
- `openspec/changes/phase5-llama-engine-plugin/tasks.md`: C14 任务描述同步

### 4. `specs/adr-doc-alignment/` capability 正式化

- 基于 hotfix 已 ship 的 `specs/adr-doc-alignment-hotfix/spec.md` 4 个 Requirements
- 扩展为完整命名政策 spec (8-10 个 Requirements)
- 引入 `tools/adr_lint.py` 验证支持

## Non-goals

- **不修改** `openspec/changes/phase5-illmprovider-call-chain-v2/proposal.md` (经 Metis 审查, `inference.*` 是合法事件 topic notation)
- **不修改** STATUS-GLOSSARY 状态 (已在 hotfix 中处理)
- **不修改** ADR-0036 归档 (已在 hotfix 中处理)
- **不修改** P1-4 STATUS-GLOSSARY 📋 双语义 (属 Change C 范围)
- **不重跑** `tools/adr_relationships.py` (属 Change C 范围)

## Capabilities

### New Capabilities

- `pdk-tool-naming-policy`: PDK 工具命名强制规范 (SLASH 唯一合法, DOT 仅允许 C++ methods)

### Modified Capabilities

- 修改 `adr-doc-alignment-hotfix` 扩展为 `adr-doc-alignment-full`, 引入 SLASH 强制 + lint 验证

## Impact (待 Change B 启动时细化)

| 文件 | 变更类型 |
|------|---------|
| `lib/inference/prefix_cache.md` + 4 个 | DOT → SLASH 工具名 |
| `decisions-2026-07-07.md` D3 | 整章重写 |
| `pdk/llama_engine/src/inference_arch.cpp` | 工具名注册字符串同步 |
| `tests/test_llama_engine_plugin.cpp` | 测试断言同步 |
| C13/C14/C15 proposal/tasks | 工具名引用同步 |

## Open Questions

1. **启动时序**: 与 B2 owner 协商 — 在 B2 实施启动前 ship, 还是 B2 实施完成后 ship (后者风险高)
2. **PDK 代码同步范围**: 是否包含 `pdk/llama_engine/src/llama_engine.cpp` + `llama_model.cpp` (已 ship 部分) 的工具名注册
3. **测试同步**: 是否包含 `tests/test_pdk_*` 的所有 PDK 相关测试

## 启动条件

- [ ] B2 owner 确认启动时序
- [ ] PDK 代码同步范围确认
- [ ] 测试同步范围确认
- [ ] Change A 已 ship (本 change 启动时, Change A 必须已 ship)

## 当前状态: PLACEHOLDER

**说明**: 本 change 当前仅有 proposal.md 占位, 完整 artifacts (design/specs/tasks) 待 Change A ship 后 + B2 时序确认后, 由 Sisyphus 启动正式化工作。
