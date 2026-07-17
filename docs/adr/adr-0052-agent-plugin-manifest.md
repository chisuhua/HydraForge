# ADR-0052: Agent Plugin Manifest 规范

## 状态

✅ Approved (2026-07-16, 架构评审确认)

## 领域

Agent-as-Plugin 架构 / PDK 扩展

## 关联

- [ADR-0021 — PDK Design](../adr-0021-pdk-design.md) — PDK 宏与工具注册
- [ADR-0022 — Plugin Loading](../adr-0022-plugin-loading.md) — Plugin 加载机制
- [ADR-0043 — PDK Tool Naming Convention](../adr-0043-pdk-tool-naming-convention.md) — 工具命名规则
- [ADR-0053 — AgentDescriptor 与 pdk_register_agent](./adr-0053-agent-descriptor-interface.md) — Agent 描述与注册
- MCP 2026-07-28 RC — JSON Schema 2020-12 标准化

## 背景

### 问题

当前 PDK Plugin 只有 C++ 导出的 `PluginInfo` 结构体（`name`, `version`, `abi_version`），没有机器可读的 manifest 文件。这意味着 OS **必须在加载 .so 之后**才能了解 Plugin，无法做轻量发现、版本校验、信任检查。

具体缺失：
- 无法在加载前检查版本兼容性
- 无法了解 Agent 的工具 schema
- 无法声明版本约束和信任来源
- 无法声明资源需求（timeout / concurrency）

### 目标

定义一个机器可读的 manifest 格式，使 OS 能：
1. **在不加载 .so 的情况下**发现 Plugin 元数据
2. 校验版本兼容性（abi + semver）
3. 获取工具输入/输出 schema
4. 了解安全要求（隔离/信任）
5. 了解资源需求

## 决策

### 决策 1 — 文件格式与位置

**文件命名**: `pdk_manifest.json`
**位置要求**: Plugin 根目录（与 `CMakeLists.txt` 同级）
**扩展名**: 建议启用 `.hfpkg` 包时也可内嵌

```json
{
  "$schema": "https://schemas.hydraforge.io/pdk-manifest-v1.json",
  "id": "code.review",
  "name": "Code Review Agent",
  "version": "0.1.0",
  "abi_version": 2,
  "min_host_version": "2.0.0",
  "max_host_version": "3.0.0",
  "interface_versions": ["IAgentV1"],
  "implementation_forms": ["skill", "dsl"],
  "entry_tool": "code_review/run",
  "provided_tools": ["code_review/run", "code_review/suggest"],
  "capabilities": ["code_review", "static_analysis"],
  "input_schema": { ... },
  "output_schema": { ... },
  "requires_isolation": false,
  "resources": {
    "timeout_ms": 30000,
    "max_concurrent": 4
  },
  "publisher": "hydraforge-team",
  "trust_level": "high"
}
```

**Wasm 插件**: manifest 嵌入在 `.wasm` 的 custom section 中（Phase 2）。

### 决策 2 — 必填字段（v1）

| 字段 | 类型 | 说明 |
|------|------|------|
| `id` | string | reverse-DNS 风格唯一标识 |
| `name` | string | 人类可读名称 |
| `version` | string | SemVer 版本号 |
| `abi_version` | int | OS ABI 版本号 |
| `implementation_forms` | string[] | 至少一个：`skill`, `dsl`, `cpp`, `wasm` |
| `entry_tool` | string | 入口工具名 |
| `provided_tools` | string[] | 提供的所有工具名 |

### 决策 3 — 推荐字段（v1 强烈建议）

| 字段 | 类型 | 说明 |
|------|------|------|
| `min_host_version` | string | 最低 OS 版本支持 |
| `max_host_version` | string | 最高 OS 版本支持 |
| `interface_versions` | string[] | 支持的 IAgent 接口版本 |
| `capabilities` | string[] | 能力标签（用于 CapabilityRegistry） |
| `input_schema` | object | JSON Schema 2020-12 |
| `output_schema` | object | JSON Schema 2020-12 |
| `requires_isolation` | bool | 是否需要隔离环境 |
| `resources` | object | `timeout_ms`, `max_concurrent` |
| `publisher` | string | 发布者标识 |
| `trust_level` | string | `high`, `medium`, `low`, `untrusted` |
| `activation_events` | string[] | 懒加载触发条件 |

### 决策 4 — 版本约束

```json
{
  "abi_version": 2,
  "min_host_version": "2.0.0",
  "max_host_version": "3.0.0"
}
```

| 字段 | 匹配规则 | 不满足时的行为 |
|------|---------|--------------|
| `abi_version` | **硬匹配**（不相等 = 拒绝） | `ERR_ABI_MISMATCH`，拒绝加载 |
| `min_host_version` | **软约束**（OS ≥ min） | warn 仍可加载 |
| `max_host_version` | **软约束**（OS < max） | warn 仍可加载 |

版本比较使用 SemVer 语义（major.minor.patch）。

**理由**：ABI 不兼容必然 crash，必须 fail-fast。版本号宽松可以给开发者缓冲期。

### 决策 5 — Schema 校验

使用 **JSON Schema 2020-12** 作为 `input_schema` 和 `output_schema` 的标准：

```json
{
  "input_schema": {
    "type": "object",
    "properties": {
      "code": {"type": "string"},
      "language": {"type": "string"},
      "severity": {"type": "string", "enum": ["low", "medium", "high"]}
    },
    "required": ["code", "language"]
  },
  "output_schema": {
    "type": "object",
    "properties": {
      "issues": {"type": "array", "items": {"$ref": "#/$defs/Issue"}},
      "summary": {"type": "string"}
    }
  }
}
```

**规则**：
- `input_schema` 空或缺失 → 接受任意 JSON
- `output_schema` 空或缺失 → 不校验输出
- 运行时校验由 ADR-0058 定义
- Schema 定义与 MCP 2026-07-28 RC 兼容

### 决策 6 — 与 C++ 导出的关系

**双重来源，以 `PluginInfo` 为最终依据**：

```
OS 发现流程：
  1. 读 pdk_manifest.json（轻量发现，不加载 .so）
     - 检查 abi_version 硬匹配
     - 检查 min/max_host_version 软约束
     - 获取 input/output schema（用于工具发现）
  
  2. PluginLoader::load_so(name)
     - 读 PluginInfo（验证 manifest 中的 name/version/abi 一致）
     - 不一致时 emit warn，以 PluginInfo 为准
  
  3. pdk_register_tools(registry)
  4. pdk_register_agent(desc)    // ADR-0053
```

**冲突处理**：

| 字段 | manifest.json vs PluginInfo | 最终来源 |
|------|---------------------------|---------|
| `name`, `version`, `abi_version` | 必须一致，否则 warn | `PluginInfo` |
| 所有其他字段 | manifest 为唯一来源 | `pdk_manifest.json` |

### 决策 7 — 签名与信任（Phase 2）

v1 只声明不验证：

```json
{
  "publisher": "hydraforge-team",
  "trust_level": "high"
}
```

**信任等级规则**：
| 等级 | 行为 |
|------|------|
| `high` | 默认允许所有操作 |
| `medium` | 敏感操作需要审批（ADR-0031） |
| `low` | 所有操作需要审批 |
| `untrusted` | 必须隔离执行，`requires_isolation` 自动 = true |

Phase 2 增加 ED25519 签名 + 验证。

## 替代方案

### 方案 A: 仅用 C++ 导出，不要 manifest

**否决理由**：
- 必须加载 .so 才能了解 Plugin，无法做 CI 兼容性检查
- 无法在 Agent Marketplace 中展示 schema
- 与其他 SOTA 标准（MCP, Zylos）不一致

### 方案 B: 使用 YAML 而非 JSON

**否决理由**：
- JSON Schema 2020-12 是 MCP 标准，生态最广
- `pdk_manifest.yaml` 无标准 schema 校验工具链
- JSON 的 IDE 支持更好

### 方案 C: 将 manifest 嵌入 C++ PluginInfo

**否决理由**：
- 仍然需要加载 .so 才能读取
- 与轻量发现的目标矛盾

## 不变量

- manifest 是**轻量发现**用途，不参与运行时决策（运行时用 `PluginInfo`）
- `abi_version` 硬匹配不可绕过（安全底线）
- `input_schema` 与 MCP 2026-07-28 RC 兼容
- manifest 缺失时不影响向后兼容（OS 发出 warn 仍可加载）

## 权衡

| 决策 | 选择 | 理由 |
|------|------|------|
| 格式 | JSON | MCP 标准，生态最广 |
| Schema 标准 | JSON Schema 2020-12 | 与 MCP 对齐 |
| abi 版本 | 硬匹配 | 安全底线 |
| 版本号 | 软约束 | 给缓冲期 |
| 签名 | Phase 2 | 投入大，推迟 |
| 信任等级 | Phase 2 验证 | v1 仅声明 |

## 后续行动

- ADR-0057: `activation_events` 懒加载机制
- ADR-0058: 工具输入/输出 schema 运行时校验
- ADR-0054: `CapabilityRegistry` 使用 `input_schema` 和 `capabilities` 索引
- Phase 2: 签名验证 + Wasm custom section 嵌入

## 参考

- [ADR-0043 — PDK Tool Naming Convention](../adr-0043-pdk-tool-naming-convention.md)
- [ADR-0053 — AgentDescriptor 与 pdk_register_agent](./adr-0053-agent-descriptor-interface.md)
- [ADR-0022 — Plugin Loading](../adr-0022-plugin-loading.md)
- MCP 2026-07-28 RC: `blog.modelcontextprotocol.io/posts/2026-07-28-release-candidate/`
- JSON Schema 2020-12: `json-schema.org/specification-links.html`
- [docs/architecture/agent-as-plugin-architecture-v1.1.md](../architecture/agent-as-plugin-architecture-v1.1.md)