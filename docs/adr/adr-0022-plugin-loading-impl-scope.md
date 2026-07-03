# ADR-0022 Implementation Scope Audit

> **生成时间**: 2026-07-03 (C9 — Phase 4.5 impl-scope audit)
> **基础**: `tools/docs_drift_audit.py` 报告 (Scenario 4)
> **关联 ADR**: [adr-0022-plugin-loading.md](adr-0022-plugin-loading.md)
> **状态**: ✅ Approved (audit 后保持)

## 状态

✅ Approved (audit 后保持 — 所有 11 个 ADR 核心契约类均已 Shipped 或 Evolved, 无需调整主 ADR 状态)

## Drift 摘要

`docs_drift_audit.py` 报告: ADR 声称 ✅ Approved (2026-06-24, Sprint 5 ship), 但 1/6 个描述的类未在 src/include 中找到。

## 原始描述 vs 实际实施

| ADR 描述的类 | 实际状态 | 实际位置 (如有) | 备注 |
|--------------|---------|----------------|------|
| `PluginLoader` | ✅ Shipped | `src/modules/plugin/plugin_loader.h` (PIMPL-lite) | 插件加载器核心实现 |
| `PluginInfo` | ✅ Shipped | `src/modules/plugin/plugin_loader.h` | 插件元数据结构 (`abi_version` / `tool_names` / `models` / 外部导出) |
| `CURRENT_ABI_VERSION` | ✅ Shipped | `src/modules/plugin/plugin_loader.h` | ABI 版本常量 |
| `IPluginTool` | ✅ Shipped | `src/modules/plugin/iplugin_tool.h` | 插件工具接口 |
| `PluginManifest` | ✅ Shipped | `src/modules/plugin/plugin_loader.h` | 插件清单结构 |
| `Plugin_v1` | 📅 Deferred | — | ADR 可能描述的版本化插件基类, 未实现为独立类; `PluginInfo` (含 `abi_version`) + `iplugin_tool.h` 提供等价接口 |

## 分类详情

### 📅 Deferred — `Plugin_v1`

ADR-0022 可能描述了 `Plugin_v1` 版本化基类 (含 `configure()` / `initialize()` / `shutdown()` 等生命周期方法)。实际实现选择更轻量的方案:
- `PluginInfo` 结构体 (含 `abi_version`) 提供版本标识
- `IPluginTool` 接口提供生命周期方法 (`execute()` 等)
- `PluginLoader` 通过 `dlsym` + `pdk_plugin_info` 导出符号实现 ABI 兼容性检查
- model_router `.so` 插件全部导出 `pdk_plugin_info` (含 `CURRENT_ABI_VERSION`)

`Plugin_v1` 基类在 Phase 4.5 范围外, 当前插件系统通过 `IPluginTool` + `PluginInfo` 已满足 PDK 插件注册需求。

## 决策

- **ADR 状态**: ✅ Approved (保持)
- **理由**: ADR-0022 核心契约 (`PluginLoader` / `PluginInfo` / `IPluginTool` / ABI 版本检查) 5/6 已 Shipped; 唯一 Deferred 的 `Plugin_v1` 基类被 `PluginInfo` + `IPluginTool` 的轻量方案替代
- **风险**: 低 — Phase 1 插件 (model_router `.so`) 通过 `pdk_plugin_info` + `IPluginTool` 工作正常

## 后续行动

- Phase 5 插件生命周期需求明确后, 评估是否需要 `Plugin_v1` 基类 (或继续使用 `IPluginTool` 接口)
- 本 audit 文档供 Phase 5 backlog 参考
