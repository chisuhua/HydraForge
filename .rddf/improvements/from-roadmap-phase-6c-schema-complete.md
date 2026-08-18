# from-roadmap-phase-6c-schema-complete

**优先级**: P0 | **来源**: from-roadmap (phase-6c/schema-complete, ADR-0073 D3)
**阶段**: phase-6c | **分类**: schema-complete
**类型**: functional
**主题**: DECLARE_TOOL V3自动生成；ToolCoordinator校验层

## 架构依据

ADR-0073「Tool JSON Schema 契约」分 4 个决策落地 schema-complete 类别，本提案聚焦 D3（运行时校验层）——D2（ToolMetadata V3 字段）与 D4（DECLARE_TOOL V3 自动生成）已提前 ship 2026-08-14 (`adr-0073-d2-declare-tool-v3`)，D3 是 schema-complete 类别闭合的最后一公里：

- **D2（✅ 已 ship）**：ToolMetadata V3 字段 `input_schema` / `output_schema` / `ValidationMode` 在 `src/common/policy/execution_policy.h:82-86` 实现；SchemaGenerator 类型反射在 `include/agenticdsl/tools/schema_generation.h` 实现。
- **D4（✅ 已 ship）**：DECLARE_TOOL_V3 宏（`include/agenticdsl/pdk/tool_macros.h`）通过 C++ 类型 → JSON Schema 2020-12 自动生成（PTYPE 映射：string/int/float/double/bool/vector/optional/map/enum class/struct），覆盖 4 类型测试 `tests/test_declare_tool_auto_schema.cpp`。
- **D3（本提案，C9）**：将 D2 生成的 schema 字段接入 ToolCoordinator execute 流（ADR-0069 hook 体系），4 步 sanitization pipeline 顺序执行：**input schema 校验 → 参数 coercion → 必填字段检查 → 业务规则强制**。这是 schema-complete 类别从「schema 已声明」到「schema 实际生效」的桥梁。

**依赖链**（per roadmap.md line 278-279）：

```
C8 (D2/D4 ship 2026-08-14) ──→ C9 (D3 本提案)
                                  │
                                  └──→ ADR-0073 翻牌 🟡 → ✅ Approved
```

**JSON-RPC 错误兼容**：D3 校验失败映射到 MCP/JSON-RPC 标准错误码 `-32602 Invalid params`（参数级别拒绝），区别于 `-32601 Method not found`（路由级拒绝）和 `-32603 Internal error`（运行时错误）——保证上层 MCP client 的标准错误处理路径不受破坏（ADR-0071 §决策 D7 锁定 MCP 2025-11-25 规范）。

## 范围

- **In Scope**:
  - ToolCoordinator 4 步 sanitization pipeline 实施：`src/common/tools/tool_coordinator.{h,cpp}` 在 `call_tool()` 入口插入 4 步顺序检查（schema validate → coercion → required fields → business rules）。
  - 复用 D2 生成的 `input_schema` JSON Schema 2020-12 字符串（不重新生成、不解析）：通过 nlohmann `json_schema_validator` 包装层 `include/agenticdsl/tools/tool_schema_validator.h` 做内容校验。
  - 错误传播链：4 步任一拒绝 → 返回 `ToolResult{ok=false, error_code=ErrorCode::InvalidParams, metadata={reason, field_path}}` → emit `tool.audit.denied` 事件（per ADR-0068）→ 不调用下游 `registry.call_tool()`。
  - 业务规则强制层：对 shell/exec 类工具强制 path validation + dangerous-pattern blocklist（`rm -rf` / `mkfs` / `:(){ :|:& };:` 等）；路径检查复用 `common/policy/path_policy.cpp`。
  - 测试：`tests/test_tool_coordinator_validation.cpp`（≥6 case，覆盖 schema/coerce/required/business rule 四类拒绝路径 + 1 happy path）。
- **Out of Scope**:
  - DECLARE_TOOL V3 宏本身（D4 已 ship，不重新实施）。
  - ToolMetadata V3 字段扩展（D2 已 ship）。
  - 运行时 output schema 校验（属于 D5/D6 推迟范围，per `adr-0073-impl-scope-audit.md` 行 51-52）。
  - V2 工具强制迁移（V2 + V3 共存期由 D5 覆盖，留 Sprint 27+）。
  - 业务规则语义的扩展（仅实现 shell/exec 安全规则；其他 tool category 规则留 follow-up）。

## 关键场景

- GIVEN 工具已通过 DECLARE_TOOL V3 注册，input 字段类型 + 必填项匹配 schema
  WHEN 调用方经 ToolCoordinator.execute 触发该工具
  THEN 4 步 pipeline 全 pass，工具正常执行，返回 `ToolResult{ok=true}`，`tool.audit.invoked` 事件携带 schema 校验阶段耗时。

- GIVEN 工具已注册（`input_schema.required=["path","mode"]`），调用方传入 `{"path": "x.txt"}`（缺 `mode`）
  WHEN ToolCoordinator 4 步 pipeline 顺序执行到 required field check 步骤
  THEN 拒绝执行，返回 `ToolResult{ok=false, error_code=ErrorCode::InvalidParams, metadata={reason:"missing required field", field_path:"mode"}}`，错误向上层暴露为 JSON-RPC `-32602`。

- GIVEN 工具 schema 要求 `port: integer`，调用方传入 `{"port": "8080"}`（字符串而非整数）
  WHEN ToolCoordinator coercion 步骤执行类型转换
  THEN 按 ToolMetadata.ValidationMode 决策：`Strict` 模式立即拒绝（`-32602`，reason="type mismatch"）；`Coerce` 模式自动转换 `"8080"` → `8080` 并记录 coercion 事件到 audit log；行为确定性由 ValidationMode 字段控制。

- GIVEN 调用方调用 `shell/exec` 工具，参数含 `cmd: "rm -rf /tmp/build"`
  WHEN ToolCoordinator 4 步 pipeline 执行到 business rule 强制步骤
  THEN 拒绝执行（早于 registry.call_tool），返回 `-32602` + reason="business rule violation: dangerous pattern detected"；`tool.audit.denied` 事件 metadata 含 matched_pattern 与 input_cmd hash（脱敏，不记录 raw command 防 secret 泄露，per ADR-0068 §5.11 args-only-keys 政策）。

## 技术约束

- MUST 复用 D2 生成的 `input_schema` 字段（`ToolMetadata::input_schema` JSON 字符串），禁止在 ToolCoordinator 内重新生成 schema——单一真相源（single source of truth）。
- MUST 4 步 pipeline 顺序固定：schema validate → coercion → required field check → business rules；任意步骤失败立即短路返回，不继续后续步骤。
- MUST 错误消息含稳定 schema field path（如 `params.port` / `params.options.timeout`），便于 LLM 修正 prompt 时定位精确字段。
- MUST NOT bypass sanitization for any tool——包括 legacy V2 工具与 plugin 加载工具；V2 工具缺 `input_schema` 时，pipeline 跳过 schema/coerce/required 三步但仍执行 business rule（path validation 不依赖 schema）。
- MUST JSON-RPC 错误码 `-32602 Invalid params` 仅用于参数校验拒绝，不用于路由错误（`-32601`）或运行时错误（`-32603`）；错误码边界由 ToolCoordinator 错误映射表维护。
- MUST 业务规则层不记录 raw args（per ADR-0068 §5.11）：audit event 仅记录 matched_pattern + args field key 列表 + SHA256(input_args)；防止 shell/exec 等敏感命令泄露到 trace log。
- SHOULD coercion policy（Strict / Coerce / Off）通过 ToolMetadata.ValidationMode 字段按工具配置；默认值 Coerce（向后兼容）。
- SHOULD nlohmann `json_schema_validator` 校验错误本地化为人类可读消息，含 expected type / actual value / path。

## 验收标准

- [ ] ToolCoordinator 4 步 sanitization pipeline 实施完成：`tool_coordinator.cpp::call_tool()` 入口插入 4 步顺序检查，每步独立可单元测试。
- [ ] 复用 D2 `input_schema` 字段（不重新生成）：grep 验证 ToolCoordinator 代码无 schema 构造逻辑（仅 `json_schema_validator` 实例化与接受外部 schema 字符串）。
- [ ] `tests/test_tool_coordinator_validation.cpp` ≥6 case 全 PASS：4 拒绝路径（schema/coerce/required/business rule）+ 1 happy path + 1 V2 legacy tool 业务规则仍生效路径。
- [ ] shell/exec business rule 测试通过：`rm -rf /tmp/build` → `-32602` 拒绝；`mkfs.ext4 /dev/sda` → `-32602` 拒绝；`echo hello` → pass。
- [ ] JSON-RPC 错误码映射正确：schema/coerce/required/business rule 四类拒绝均返回 `-32602`；与 `-32601`/`-32603` 无串扰。
- [ ] ADR-0069 tool_execution hooks 集成：4 步 pipeline 在 `pre_execute_hook` 之前完成；audit event `tool.audit.denied` 含 reason / field_path / matched_pattern（脱敏）。
- [ ] `ctest --output-on-failure` 全量零回归（147/147 baseline 不变；新增 `test_tool_coordinator_validation` ≥6 case 全 PASS）。
- [ ] `docs/adr/adr-0073-tool-json-schema-contract.md` 状态翻转：🟡 Partial → ✅ Approved（D2/D3/D4 全 ship，per `adr-0073-impl-scope-audit.md` 行 47-50 的 Partial 状态同步更新）；`docs/active-status.md` §一 Phase 6c C9 行标记 ✅ ship。
