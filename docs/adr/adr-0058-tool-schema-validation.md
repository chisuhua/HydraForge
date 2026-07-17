# ADR-0058: Tool Input/Output Schema 强制校验

## 状态

✅ Approved (2026-07-16, 架构评审确认)

## 领域

Agent-as-Plugin 架构 / 工具契约

## 关联

- [ADR-0004 — ToolRegistry Security](../adr-0004-toolregistry-security.md) — ToolMetadata V2 基础
- [ADR-0023 — ToolResult Standard](../adr-0023-tool-result-standard.md) — 返回值格式
- [ADR-0052 — Agent Plugin Manifest](./adr-0052-agent-plugin-manifest.md) — `input_schema` / `output_schema` 字段
- [ADR-0054 — Capability Discovery](./adr-0054-capability-discovery.md) — schema 用于 Agent 发现
- MCP 2026-07-28 RC — JSON Schema 2020-12 标准化

## 背景

### 问题

当前 `ToolMetadata` 的 `input_schema` / `output_schema` 字段**只声明不校验**。工具调用时传入的 args 可能：

- 缺少必需字段
- 字段类型错误（string vs number）
- 枚举值不在允许列表中
- 返回值不符合声明的 schema

OS 在调用前不做任何验证，错误只能在工具实现内部被发现。这在以下场景中特别危险：

- **Agent Marketplace**：不可信的第三方 Agent 可能传任意参数
- **SKILL.md**：LLM 生成的 SKILL 可能构造不合法的工具调用
- **跨进程调用**：远程 Agent 的输入不可信

### 目标

在 `call_tool` 调用路径上加入 schema 校验，确保输入输出符合声明。

## 决策

### 决策 1 — 校验时机：`call_tool` 入口处自动校验

```cpp
nlohmann::json IToolRegistry::call_tool(
    const std::string& name, 
    const nlohmann::json& args
) {
    auto metadata = get_tool_metadata(name);
    
    // 输入校验（按严格模式）
    if (schema_validation_level_ != ValidationLevel::Off) {
        auto result = validate_json_schema(args, metadata.input_schema);
        if (!result.valid) {
            if (schema_validation_level_ == ValidationLevel::Strict) {
                return ToolResult::error(
                    ErrorCode::ERR_SCHEMA_VALIDATION,
                    "Tool '" + name + "' input validation: " + result.error
                );
            }
            // Warn: log + 尝试填充默认值后继续
            logger->warn("Tool '{}' input validation: {}", name, result.error);
            if (result.has_defaults) {
                fill_defaults(args, metadata.input_schema);
            }
        }
    }
    
    auto result = actual_call(name, args);
    
    // 输出校验（仅 strict 模式）
    if (schema_validation_level_ == ValidationLevel::Strict && 
        !metadata.output_schema.is_null()) {
        auto out_result = validate_json_schema(result, metadata.output_schema);
        if (!out_result.valid) {
            return ToolResult::error(
                ErrorCode::ERR_SCHEMA_VALIDATION,
                "Tool '" + name + "' output validation: " + out_result.error
            );
        }
    }
    
    return result;
}
```

### 决策 2 — 三级严格模式

```cpp
enum class ValidationLevel {
    Off,      // 不校验（兼容旧 Plugin）
    Warn,     // 校验 + 日志 + 填充默认值 + 继续执行（默认）
    Strict    // 校验 + 校验失败拒绝执行
};
```

| 模式 | 输入校验 | 输出校验 | 默认值填充 | 适用场景 |
|------|:--------:|:--------:|:----------:|---------|
| `Off` | ❌ | ❌ | ❌ | 开发初期，兼容旧 Plugin |
| `Warn` | ✅ log + 继续 | ❌ | ✅ | **默认模式**，过渡期 |
| `Strict` | ✅ 拒绝 | ✅ 拒绝 | ❌ | 生产、CI、Marketplace |

**默认值为 `Warn`**——不破坏向后兼容，但让开发者感知到问题。

### 决策 3 — Schema 格式

使用 **JSON Schema 2020-12**（与 MCP 对齐）：

```json
{
  "input_schema": {
    "$schema": "https://json-schema.org/draft/2020-12/schema",
    "type": "object",
    "properties": {
      "code": {
        "type": "string",
        "description": "Code to review"
      },
      "language": {
        "type": "string",
        "enum": ["cpp", "python", "rust"],
        "default": "cpp"
      },
      "severity": {
        "type": "string",
        "enum": ["low", "medium", "high"],
        "default": "medium"
      }
    },
    "required": ["code", "language"]
  }
}
```

**支持的 Schema 特性**（v1）：

| 特性 | 支持 | 说明 |
|------|:----:|------|
| `type` | ✅ | object, string, number, integer, boolean, array |
| `properties` | ✅ | 对象字段定义 |
| `required` | ✅ | 必填字段 |
| `enum` | ✅ | 枚举值约束 |
| `default` | ✅ | 默认值填充 |
| `minimum` / `maximum` | ✅ | 数值范围 |
| `minLength` / `maxLength` | ✅ | 字符串长度 |
| `pattern` | ✅ | 正则匹配 |
| `items` | ✅ | 数组元素类型 |
| `$ref` | ❌ Phase 2 | 引用外部 schema |
| `allOf` / `anyOf` | ❌ Phase 2 | 复合约束 |

### 决策 4 — 全局与 per-tool 覆盖

```cpp
// 全局设置（默认 Warn）
registry.set_validation_level(ValidationLevel::Strict);

// per-tool 覆盖（某些工具可以更宽松）
metadata.validation_override = ValidationLevel::Off;  // 旧工具豁免
```

**覆盖规则**：
- Per-tool 设置优先级高于全局
- 未设置 per-tool 时使用全局默认
- Marketplace Plugin 强制 `Strict`

### 决策 5 — 性能

```cpp
// Schema 编译缓存：只编译一次
class SchemaCache {
    std::unordered_map<size_t, std::unique_ptr<jsonschema::Validator>> cache_;
public:
    const jsonschema::Validator& get(const nlohmann::json& schema) {
        auto hash = std::hash<nlohmann::json>{}(schema);
        auto it = cache_.find(hash);
        if (it == cache_.end()) {
            it = cache_.emplace(hash, 
                std::make_unique<jsonschema::Validator>(schema)).first;
        }
        return *it->second;
    }
};
```

**性能目标**：
- 校验时间 < 1μs（缓存编译后的 schema）
- 仅对传入的 args 做单次校验

## 替代方案

### 方案 A：只在注册时校验，调用时不校验

**否决理由**：
- 无法防止 LLM 生成的 SKILL 传非法参数
- 无法防止远程 Agent 的恶意输入

### 方案 B：用 Protobuf 而非 JSON Schema

**否决理由**：
- Protobuf 需要编译期绑定，不适合动态 Agent 场景
- JSON Schema 与 MCP 对齐，生态最广

### 方案 C：默认 Strict

**否决理由**：
- 破坏向后兼容
- 很多现有工具可能没有精确的 schema

## 不变量

- schema 缺失时 `Off` 模式等同、`Warn` 和 `Strict` 模式跳过校验（不阻断）
- 校验错误不影响 OS 稳定性（`ToolResult::error` 返回而非 crash）
- `Off` 模式的完整语义兼容旧版本行为

## 权衡

| 决策 | 选择 | 理由 |
|------|------|------|
| 默认模式 | Warn | 向后兼容 |
| 输出校验 | 仅在 Strict | 输出结构可能动态 |
| Schema 标准 | JSON Schema 2020-12 | MCP 对齐 |
| 性能 | 缓存编译后 schema | <1μs |

## 后续行动

- ADR-0054: CapabilityRegistry 的 query 使用 schema 匹配
- ADR-0060: Agent 组合的契约校验也使用 schema
- Phase 2: `$ref` / `allOf` / `anyOf` 复合约束

## 参考

- [ADR-0004 — ToolRegistry Security](../adr-0004-toolregistry-security.md)
- [ADR-0052 — Agent Plugin Manifest](./adr-0052-agent-plugin-manifest.md)
- MCP 2026-07-28 RC: `blog.modelcontextprotocol.io/posts/2026-07-28-release-candidate/`
- JSON Schema 2020-12: `json-schema.org/specification-links.html`