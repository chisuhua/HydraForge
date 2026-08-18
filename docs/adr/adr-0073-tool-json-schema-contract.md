# ADR-0073: Tool JSON Schema 契约 (JSON Schema 2020-12)

## 状态

✅ Approved (D2 + D3 + D4 全 ship) (2026-08-18 — Phase 6c C9 `from-roadmap-phase-6c-schema-complete` ship: D3 ToolCoordinator 4 步 sanitization pipeline 落地)

**证据基础**：
- `include/agenticdsl/pdk/manifest.h:17-18` — `input_schema`/`output_schema` 作为 `std::string`（raw JSON）存在于 PDK manifest 结构
- `src/modules/pdk/manifest_validator.cpp` — 校验字段存在且为非空字符串（非 JSON Schema 内容校验）
- `src/common/policy/execution_policy.h:82-86` — **D2 V3 字段已添加** (`input_schema`, `output_schema`, `ValidationMode`)
- `include/agenticdsl/tools/schema_generation.h` — **D4 SchemaGenerator 类型反射已实现**
- `include/agenticdsl/pdk/tool_macros.h` — **D4 DECLARE_TOOL_V3 宏已实现**
- `src/common/tools/tool_coordinator.cpp` — **D3 4 步 sanitization pipeline 已实施** (schema_validate → coercion → required_field → business_rules, 2026-08-18)
- `include/agenticdsl/tools/tool_schema_validator.h` + `src/common/tools/tool_schema_validator.cpp` — D3 JSON Schema 2020-12 最小子集校验器 (type/properties/required/items/enum; vendor nlohmann/json 无 json-schema.hpp, 自包含实现)
- `src/common/policy/dangerous_patterns.{h,cpp}` — D3 业务规则 OWASP 命令注入黑名单 (rm -rf / mkfs / fork bomb / dd / >/dev/sd)
- `src/core/types/tool_result.h` — **ErrorCode::InvalidParams 新增** (JSON-RPC -32602, 4 步拒绝路径统一错误码)
- `tests/test_tool_coordinator_validation.cpp` — 7 test cases / 26 assertions (1 happy + 4 拒绝路径 + V2 legacy + safe shell)
- `tests/test_dangerous_patterns.cpp` — 4 test cases (rm -rf / mkfs / fork bomb / 大小写不敏感)

**D3 实施偏差记录** (plan vs 项目实际, 已在代码注释同步):
- `ValidationMode` 实际为 `Strict/Warn/Ignore` (非 plan 的 Strict/Coerce/Off): Strict=类型不匹配即拒绝; Warn=自动类型转换+stderr 警告; Ignore=跳过
- `ToolCategory` 无 `Dangerous` 枚举值 → 业务规则锚定 `ToolCategory::Execute` (shell/exec 类工具自然分类)
- `meta.input_schema` 为 `std::optional<nlohmann::json>`; `has_value()==false` 表示 V2 legacy 工具 → 跳过 step 1-3, step 4 业务规则仍生效
- `args_hash` 使用 `std::hash` hex 指纹 (项目无 vendored SHA-256, change 禁止引入新外部依赖; 确定性, 仅用于审计关联)

本状态**不声称**已完成完整的 D5（向后兼容验证）与 D6（output_schema 校验）。

## 领域

L1 OS Services / Tool 契约 / 运行时校验 / LLM 约束接口 / MCP 互操作

## 关联

### 父 ADR
- [ADR-0071 — LLM-native AgenticDSL 架构](./adr-0071-llm-native-agenticdsl-architecture.md) §决策 D4 (本 ADR 是 D4 的具体实施)
- ADR-0071 §决策 D5 (本 ADR 提供 schema 约束，LLM 训练数据采集使用)

### 上游锚定
- [ADR-0004 — ToolRegistry 安全模型](./adr-0004-toolregistry-security.md) — ToolMetadata V2 (本 ADR 扩展为 V3)
- [ADR-0021 — PDK 设计](./adr-0021-pdk-design.md) — DECLARE_TOOL 宏 (本 ADR 增强自动 schema 生成)
- [ADR-0031 — 执行策略](./adr-0031-execution-policy.md) — ToolCoordinator 治理路径 (本 ADR 插入 schema 校验层)
- [ADR-0050 — Phase 6 战略评估](./adr-0050-phase6-strategic-evaluation.md) — 容量协调 (本 ADR 估时 1-2 周)

### 平行/下游
- ADR-0074 (Prompt Engineering) — 使用本 ADR schema 作为 LLM 约束接口
- ADR-0076 (DSL Engine as MCP Server) — 使用本 ADR schema 作为 MCP `inputSchema` 字段 (零额外成本)
- ADR-0078 (JSON IR, 备选 5 部分采纳, Wave 3+) — 使用本 ADR schema 作为 constrained-decoding 输入

### 规范
- [JSON Schema Draft 2020-12](https://json-schema.org/draft/2020-12/schema) — 官方规范
- [MCP Spec 2025-11-25](https://modelcontextprotocol.io/specification/2025-11-25) — `inputSchema` 字段等同 JSON Schema 2020-12
- [`docs/specs/dsl.md`](../specs/dsl.md) §6.2 (signature) — 本 ADR 扩展到具体 schema 字段

---

## 背景

### 问题

当前 Tool 调用的输入输出契约是**软约束**：

```yaml
type: tool_call
tool: fs.read
arguments:
  path: "/some/file"
  max_lines: 100
```

执行器只知道 "fs.read" 这个名字和一组键值对。**5 个具体问题**：

1. **LLM 无法知道参数类型** — `max_lines: 100` 是数字还是字符串？`path` 必须非空吗？
2. **运行时缺乏校验** — 用户传 `path: ""` 或 `max_lines: -1` 不会被拦截，下游工具自己报错
3. **LLM 输出无效参数时反馈弱** — 错误信息散落在各 tool 实现，没有统一格式
4. **MCP 互操作成本高** — MCP `inputSchema` 需要 JSON Schema 2020-12；当前 ToolMetadata 没有 schema，需手工转换
5. **Tool 文档与代码漂移** — README 写 "max_lines 必填"，实际实现允许默认 0

### 解决方案

为每个 Tool 增加 `input_schema` 和 `output_schema` 字段（JSON Schema 2020-12），运行时校验 + LLM 约束接口 + MCP 互操作三合一。

### 已实证证据

- **MCP 2025-11-25 spec**（已锁定，ADR-0071 D7）：`tools/list` 返回的 `inputSchema` 字段等同 JSON Schema 2020-12 — 本 ADR schema **零转换** 可用于 MCP
- **OpenAI structured outputs / Anthropic tool use**：均以 JSON Schema 2020-12 作为 LLM 约束接口 — 本 ADR schema **零转换** 可喂给 LLM API（Wave 3+ JSON IR 优化）
- **nlohmann/json_schema_validator**：项目已 vendor `nlohmann_json` 在 `external/`，validator 是 header-only，**零构建影响**

---

## 决策

### D1. 采用 JSON Schema 2020-12 作为 Tool 契约标准

| 候选 | 选/不选 | 理由 |
|------|--------|------|
| **JSON Schema 2020-12** | ✅ 选 | MCP/OpenAI/Anthropic 共同支持；nlohmann 生态成熟 |
| OpenAPI 3.x | ❌ | 重型；不适合运行时校验 |
| TypeScript-style type | ❌ | 无主流 validator；与 JSON 工具链不兼容 |
| 自研 DSL | ❌ | 维护成本 × N；与 LLM 生态脱节 |
| JSON Schema Draft 7 | ❌ | 已过时；2020-12 是当前主流 |

**唯一支持的 Schema 版本**：`https://json-schema.org/draft/2020-12/schema`。其他版本通过 `$schema` 字段识别但 **不保证兼容**；建议所有 ToolMetadata 使用 2020-12。

### D2. ToolMetadata V3 — additive 扩展

ToolMetadata V2 (ADR-0004) 已有字段：`name`, `description`, `domain`, `category`, `min_layer`, `approval`, `allowed_layers`, `cost_estimate`, `timeout_ms`。

**V3 新增 3 个字段**（全部 additive，不破坏 V2）：

```cpp
struct ToolMetadata {
  // V2 fields (existing)
  std::string name;
  std::string description;
  std::string domain;
  ToolCategory category;
  LayerProfile min_layer;
  ApprovalPolicy approval;
  std::vector<LayerProfile> allowed_layers;
  double cost_estimate = 0.0;
  int timeout_ms = 30000;

  // V3 new fields (this ADR)
  /// Input schema (JSON Schema 2020-12 object). Null = no validation.
  std::optional<nlohmann::json> input_schema;

  /// Output schema (JSON Schema 2020-12 object). Null = no validation.
  std::optional<nlohmann::json> output_schema;

  /// Validation mode when schemas are present.
  enum class ValidationMode { Strict, Warn, Ignore };
  ValidationMode validation_mode = ValidationMode::Strict;
};
```

**YAML DSL 表示**：

```yaml
type: tool_call
tool: fs.read
arguments:
  path: "/some/file"
  max_lines: 100
# === V3 metadata（可省略，缺省走 V2 默认）===
metadata:
  input_schema:
    $schema: "https://json-schema.org/draft/2020-12/schema"
    type: object
    properties:
      path: {type: string, minLength: 1}
      max_lines: {type: integer, minimum: 0, default: 0}
    required: [path]
    additionalProperties: false
  output_schema:
    type: object
    properties:
      content: {type: string}
      truncated: {type: boolean, default: false}
    required: [content]
  validation_mode: strict   # strict | warn | ignore
```

### D3. 运行时校验 — 在 ToolCoordinator 4 步 sanitization pipeline 中

**插入位置**（ADR-0071 §安全不变量 已定义）：

```
LLM 生成 → DSL parse → Schema validate (D3) → Layer check → Approval → call_tool
```

**校验器实现**：

- 使用 `nlohmann/json_schema_validator` (header-only, 项目已 vendor `nlohmann_json`)
- 包装为 `src/common/tools/schema_validator.h/cpp`
- 公开 API：
  ```cpp
  class ToolSchemaValidator {
    ValidationResult validate_input(
      const ToolMetadata& meta,
      const nlohmann::json& args);
    ValidationResult validate_output(
      const ToolMetadata& meta,
      const nlohmann::json& result);
  };

  struct ValidationResult {
    bool valid;
    std::vector<ValidationError> errors;  // 字段级错误
    ValidationMode mode_applied;          // 实际生效的模式
  };
  ```

**校验失败行为**：

| `validation_mode` | args 校验失败 | output 校验失败 |
|------------------|--------------|----------------|
| `strict` (默认) | 抛 `SchemaValidationError`，进入 `on_error` | 抛 `SchemaValidationError`，工具调用标记为 `success=false` |
| `warn` | 记录 warning event (`tool.schema.invalid_input`)，继续执行 | 同上 |
| `ignore` | 跳过校验 | 跳过校验 |

**错误格式**（统一给 LLM 反馈）：

```json
{
  "error_code": "ERR_SCHEMA_VALIDATION",
  "tool_name": "fs.read",
  "errors": [
    {"path": "/path", "message": "must be at least 1 character", "schema_path": "#/properties/path/minLength"},
    {"path": "/max_lines", "message": "must be >= 0", "schema_path": "#/properties/max_lines/minimum"}
  ],
  "hint": "Fix the arguments and retry. See schema for fs.read in /lib/tools/fs/read signature."
}
```

LLM 看到错误后可在 retry loop 中修正（ADR-0074 Wave 2 Phase 2.2 设计）。

### D4. PDK DECLARE_TOOL 宏自动生成 schema

**当前 DECLARE_TOOL**（ADR-0004 V2）：

```cpp
DECLARE_TOOL(
  fs_read,
  "File system read",
  "builtin",
  ToolCategory::ReadOnly,
  make_approval("yolo"),
  [](const nlohmann::json& args) -> nlohmann::json { ... }
);
```

**V3 增强**（新增 2 参数）：可选 `input_schema` + `output_schema`，缺省从 C++ 类型反射生成。

```cpp
DECLARE_TOOL(
  fs_read,
  "File system read",
  "builtin",
  ToolCategory::ReadOnly,
  make_approval("yolo"),
  [](const FsReadArgs& args) -> FsReadResult {
    // C++ 强类型 args/result
  },
  /* V3 自动生成 schema from FsReadArgs/FsReadResult */
);
```

**自动生成规则**（PTYPE → JSON Schema type）：

| C++ 类型 | JSON Schema type | 附加约束 |
|---------|------------------|---------|
| `std::string` | `string` | `minLength` / `maxLength` / `pattern` |
| `int`, `long`, `size_t` | `integer` | `minimum` / `maximum` |
| `float`, `double` | `number` | `minimum` / `maximum` |
| `bool` | `boolean` | — |
| `std::vector<T>` | `array` | `items` / `minItems` / `maxItems` |
| `std::optional<T>` | (type of T) | `required` 取决于 schema mode |
| `std::map<string, T>` | `object` | `additionalProperties` |
| `enum class E` | `string` | `enum: ["val1", "val2", ...]` |
| struct | `object` | 嵌套 properties |

**示例：FsReadArgs 自动生成**：

```cpp
struct FsReadArgs {
  std::string path;          // {type: string, minLength: 1}
  int max_lines = 0;          // {type: integer, minimum: 0, default: 0}
  bool include_hidden = false; // {type: boolean, default: false}
};
```

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "type": "object",
  "properties": {
    "path": {"type": "string", "minLength": 1},
    "max_lines": {"type": "integer", "minimum": 0, "default": 0},
    "include_hidden": {"type": "boolean", "default": false}
  },
  "required": ["path"]
}
```

**手工覆盖**（当自动生成不够时）：

```cpp
DECLARE_TOOL(
  fs_read,
  ...,
  /* schema_source */ "auto",        // default: auto-generate from C++ types
  /* input_schema_override */ custom_input_schema_json,  // optional
  /* output_schema_override */ custom_output_schema_json // optional
);
```

### D5. 向后兼容 — V2 工具无需 schema

**默认行为**：未提供 `input_schema` / `output_schema` 的工具**完全按 V2 行为执行**，零校验。

```yaml
type: tool_call
tool: legacy_tool_v2
arguments:
  anything_goes: true
# 无 input_schema → 不校验
# output_schema → 不校验
```

**迁移路径**：

1. Phase 1: 新工具强制要求 schema（ADR-0071 Wave 2 准出门槛）
2. Phase 2: 已注册 V2 工具**逐步添加** schema，**不强制**
3. Phase 3: 所有 P0/P1 工具补齐 schema
4. Phase 4: 考虑对未补齐 schema 的工具发 warning（不强制）

### D6. Schema 版本与兼容性

**Schema 元数据**（每个 schema 必须包含）：

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "title": "fs.read.input.v3",
  "description": "Schema for fs.read tool input"
}
```

**Workflow metadata**（DSL 文件级）：

```yaml
AgenticDSL `/__meta__`
version: "3.11"
schema_draft: "2020-12"
```

**Schema 升级规则**（additive-only）：

- ✅ 可添加 optional 字段
- ✅ 可放宽约束（如 `minLength: 1` → `minLength: 0`）
- ❌ 不可删除 required 字段
- ❌ 不可收紧约束（如 `minimum: 0` → `minimum: 1`）
- ❌ 不可改变字段类型
- 突破性变更 → 新 schema 版本号（`v2` 后缀）

---

## 不变量

### 长期不变量

1. **JSON Schema 2020-12 是唯一支持的 schema 标准** — 不引入 Draft 7 / OpenAPI / 自研
2. **ToolMetadata V3 是 V2 的超集** — V2 工具代码继续工作，零修改
3. **PDK DECLARE_TOOL 自动生成是默认** — 手工 override 是例外
4. **Schema 校验在 ToolCoordinator execute 流中** — 不绕过
5. **`additionalProperties: false` 是 P0/P1 工具默认** — 防 LLM 幻觉字段

### Schema 校验位置不变（与 §安全不变量 一致）

```
LLM 输出 → DSL parse → Schema validate → Layer check → Approval → Backend
```

任一阶段失败即拒绝。Schema 校验失败 = ERR_SCHEMA_VALIDATION，不允许 fallback 到 raw execution。

---

## 风险

### 高风险

| 风险 | 缓解 |
|------|------|
| **DECLARE_TOOL 自动生成 bug** — C++ 类型 → JSON Schema 反射不完整或错误 | 自动化生成 + 手工覆盖双路径；CI 测试套件验证每个 DECLARE_TOOL 的 schema 满足 round-trip（schema → instance → schema 验证） |
| **LLM 训练数据采集不兼容 V3 schema** — Wave 2.2 prompt baseline 假设 V2 schema | ADR-0074 同步启动：训练数据 schema_snapshot 包含 schema_draft 字段；CI 验证采集的 schema 必为 2020-12 |

### 中风险

| 风险 | 缓解 |
|------|------|
| **nlohmann/json_schema_validator 性能** — 每次 tool call 校验有开销 | 缓存已编译的 schema validator 实例（按 tool name 缓存）；benchmark target ≤100µs per validation |
| **Schema 与 Tool 实际行为漂移** — schema 说 optional，tool 实际会 crash on missing | CI 工具测试覆盖 "按 schema 必填字段为空时，tool 返回 ERR_SCHEMA_VALIDATION（而非 crash）" |
| **PDK plugin 用户不升级** — 第三方 plugin 仍用 V2 DECLARE_TOOL | V2 持续支持；V3 是 additive；不强升级 |
| **Schema 库缺失字段** — `pattern`, `format`, `oneOf`, `anyOf` 等不被 validator 完全支持 | nlohmann/json_schema_validator 支持 2020-12 核心子集；不支持的特性在 spec 中明确列出 |

### 低风险

| 风险 | 缓解 |
|------|------|
| **Schema 文件位置管理** — schema 放 ToolMetadata vs 独立文件 | 强制 inline 在 ToolMetadata 中（YAML/JSON）；不放独立文件（避免漂移） |
| **Validator 编译时间** — nlohmann/json_schema_validator header-only | 已是项目依赖；零额外构建影响 |
| **LLM API 对 2020-12 支持** — 部分老 LLM 仅支持 2020-12 子集 | Phase 2 prompt baseline 测量；不支持时降级为 Markdown-first + warn mode |

---

## 替代方案

### 替代 1：保持 V2 软约束（拒绝）

**否决理由**：5 个具体问题全部未解；MCP 互操作成本高；LLM 错误反馈弱；与 ADR-0071 LLM-native 方向不兼容。

### 替代 2：OpenAPI 3.x（拒绝）

**否决理由**：OpenAPI 重型（路径/方法/参数/响应 body），与单 tool 调用模型不匹配；运行时验证器复杂。

### 替代 3：JSON Schema Draft 7（拒绝）

**否决理由**：已是 legacy；MCP/Anthropic 都用 2020-12。

### 替代 4：自定义 DSL schema 格式（拒绝）

**否决理由**：维护成本 × N；LLM 训练数据需要 schema 学习，重复造轮子；MCP 不兼容。

### 替代 5：JSON Schema 2020-12 + Protobuf 二进制互转（暂不采纳）

**思路**：LLM 输出 Protobuf 字节流（更紧凑），反序列化为 JSON 后校验。

**未采纳理由**：复杂度收益不匹配；LLM 输出 JSON Schema 约束字符串已能 ~100% 正确；Protobuf 是"为了"。

---

## 影响范围

### 文档
- [`docs/specs/dsl.md`](../specs/dsl.md) — §6.2 signature 扩展到具体 schema 字段示例
- [`docs/specs/stdlib-v3.10.md`](../specs/stdlib-v3.10.md) — 20 个子图添加 input/output_schema
- `docs/llm/agenticdsl-grammar.md` (新增) — LLM 友好 schema 字段说明

### 代码
- `include/agenticdsl/policy/execution_policy.h` — ToolMetadata V3 字段
- `include/agenticdsl/tools/schema_validator.h` (新增) — ToolSchemaValidator 接口
- `src/common/tools/schema_validator.cpp` (新增) — 基于 nlohmann/json_schema_validator
- `include/agenticdsl/pdk/tool_macros.h` — DECLARE_TOOL 增强 (auto-schema-generation)
- `src/common/tools/tool_coordinator.cpp` — execute 流插入 schema 校验 (4 步 pipeline)
- `src/core/engine.h/cpp` — 错误格式统一 (`ERR_SCHEMA_VALIDATION` 错误码)

### 测试
- `tests/test_schema_validator.cpp` (新增) — 单元测试 (nlohmann/json_schema_validator 包装)
- `tests/test_tool_metadata_v3.cpp` (新增) — V3 字段 round-trip 测试
- `tests/test_declare_tool_auto_schema.cpp` (新增) — DECLARE_TOOL 自动生成 schema 测试
- `tests/test_tool_coordinator_schema_validation.cpp` (新增) — execute 流插入测试 (strict/warn/ignore)
- `tests/test_schema_compatibility.cpp` (新增) — V2 ↔ V3 兼容性测试

### Schema 自动生成示例（参考实现）
- `examples/schema_generation/fs_read_args.cpp` — FsReadArgs → JSON Schema
- `examples/schema_generation/schema_demo.md` — DECLARE_TOOL V3 使用示例

### 生态
- 现有 `examples/` 7 个示例（agent_basic, agent_simple, agent_loop, slice_01_tool_call, phase1_*, pdk_chat_demo）— Phase 1 添加 schema（V2 兼容）
- `lib/` 20 个 stdlib 子图 — Phase 2 补齐 schema
- `pdk/` 已注册的 12+ 个工具 — Phase 2-3 逐步迁移到 V3 DECLARE_TOOL

---

## 后续

### 短期（Wave 2 Phase 2.1 准入后 1 周内）

1. 评估 nlohmann/json_schema_validator 与项目 nlohmann_json 版本兼容性
2. 实现 ToolSchemaValidator 包装类（header + impl）
3. 更新 ToolMetadata V3 结构（V2 字段保留 + V3 新增）
4. 启动 DECLARE_TOOL V3 增强设计（auto-generation）
5. 创建 ADR-0073 spec.md 与 tests/test_schema_validator.cpp

### 中期（Wave 2 Phase 2.1 准出前）

6. ToolCoordinator execute 流插入 schema 校验 (4 步 pipeline)
7. 错误格式统一（ERR_SCHEMA_VALIDATION 错误码）
8. DECLARE_TOOL 自动生成 + 手工 override 双路径
9. CI 测试套件：每个 DECLARE_TOOL 的 schema round-trip 验证

### Wave 2 Phase 2.2 衔接（ADR-0074）

10. ADR-0074 训练数据 schema_snapshot 字段：含 schema_draft = "2020-12"
11. Prompt baseline 在 3 个 LLM 上测量带 schema 约束的 prompt 大小（目标 ≤8k tokens prefix）

### Wave 3 衔接（ADR-0076）

12. DSL Engine as MCP server：复用 ToolMetadata V3 schema 字段作为 MCP `inputSchema`（零转换）
13. MCP client 拉取的外部 tool schema 自动注册到本地 ToolCoordinator

### Wave 3+ JSON IR 衔接（备选 5 部分采纳）

14. LLM API constrained-decoding：直接喂 ToolMetadata V3 schema 到 OpenAI/Anthropic tool_use API
15. Round-trip 工具：JSON IR ⇄ Markdown DSL

### 长期

16. 所有 P0/P1 工具补齐 schema（覆盖率 ≥90%）
17. Schema 覆盖率 CI 检查（warn if < 80%）
18. 向后兼容测试：V2 工具在 V3 runtime 中行为不变（回归测试）

---

## 复审节点

- **Wave 2 Phase 2.1 准出时（Phase 6b W1, 2026-08-13）**：本 ADR 状态从 🔍 Proposed → 🟡 Partial（**证据基础** — 仅 manifest schema 边界部分采纳，详见 `adr-0073-impl-scope-audit.md`）
- **Phase 6c C9 ship 时（2026-08-18）**：本 ADR 状态从 🟡 Partial → ✅ Approved（D2 ToolMetadata V3 + D3 ToolCoordinator 4 步校验层 + D4 DECLARE_TOOL V3 全部实施；OpenSpec change `from-roadmap-phase-6c-schema-complete`）
- **MCP server ship 时**（Wave 3）：交叉验证 schema → MCP `inputSchema` round-trip
- **JSON IR 引入时**（Wave 3+）：交叉验证 schema → constrained-decoding round-trip

---

*文档版本: v1.2*
*创建日期: 2026-08-02*
*最后更新: 2026-08-18*
*作者: HydraForge 架构组*
*状态: ✅ Approved (D2 + D3 + D4 全 ship; Phase 6c C9 `from-roadmap-phase-6c-schema-complete`, 2026-08-18)*