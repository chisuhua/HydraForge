## Context

ADR-0073「Tool JSON Schema 契约」分 4 个决策落地 schema-complete 类别，本提案聚焦 D3（运行时校验层）——D2（ToolMetadata V3 字段）与 D4（DECLARE_TOOL V3 自动生成）已提前 ship 2026-08-14 (`adr-0073-d2-declare-tool-v3`)，D3 是 schema-complete 类别闭合的最后一公里：

- **D2（✅ 已 ship）**：ToolMetadata V3 字段 `input_schema` / `output_schema` / `ValidationMode` 在 `src/common/policy/execution_policy.h:82-86` 实现；SchemaGenerator 类型反射在 `include/agenticdsl/tools/schema_generation.h` 实现。
- **D4（✅ 已 ship）**：DECLARE_TOOL_V3 宏（`include/agenticdsl/pdk/tool_macros.h`）通过 C++ 类型 → JSON Schema 2020-12 自动生成（PTYPE 映射：string/int/float/double/bool/vector/optional/map/enum class/struct），覆盖 4 类型测试 `tests/test_declare_tool_auto_schema.cpp`。
- **D3（本提案，C9）**：将 D2 生成的 schema 字段接入 ToolCoordinator execute 流（ADR-0069 hook 体系），4 步 sanitization pipeline 顺序执行：**input schema 校验 → 参数 coercion → 必填字段检查 → 业务规则强制**。这是 schema-complete 类别从「schema 已声明」到「schema 实际生效」的桥梁。

依赖链（per roadmap.md line 278-279）：
```
C8 (D2/D4 ship 2026-08-14) ──→ C9 (D3 本提案)
                                  │
                                  └──→ ADR-0073 翻牌 🟡 → ✅ Approved
```

JSON-RPC 错误兼容：D3 校验失败映射到 MCP/JSON-RPC 标准错误码 `-32602 Invalid params`（参数级别拒绝），区别于 `-32601 Method not found`（路由级拒绝）和 `-32603 Internal error`（运行时错误）——保证上层 MCP client 的标准错误处理路径不受破坏（ADR-0071 §决策 D7 锁定 MCP 2025-11-25 规范）。

## Goals / Non-Goals

**Goals:**

1. **ToolCoordinator 4 步 sanitization pipeline**：`src/common/tools/tool_coordinator.{h,cpp}` 在 `call_tool()` 入口插入 4 步顺序检查（schema validate → coercion → required field check → business rules），每步独立可单元测试。
2. **复用 D2 生成的 `input_schema`**：通过 nlohmann `json_schema_validator` 包装层 `include/agenticdsl/tools/tool_schema_validator.h` 做内容校验，禁止重新生成 schema（单一真相源）。
3. **错误传播链**：4 步任一拒绝 → 返回 `ToolResult{ok=false, error_code=ErrorCode::InvalidParams, metadata={reason, field_path}}` → emit `tool.audit.denied` 事件（per ADR-0068）→ 不调用下游 `registry.call_tool()`。
4. **业务规则强制层**：对 shell/exec 类工具强制 path validation + dangerous-pattern blocklist（`rm -rf` / `mkfs` / `:(){ :|:& };:` 等）；路径检查复用 `common/policy/path_policy.cpp`。
5. **测试**：`tests/test_tool_coordinator_validation.cpp` ≥6 case 覆盖 schema/coerce/required/business rule 四类拒绝路径 + 1 happy path + 1 V2 legacy tool 业务规则路径。
6. **架构合规性 + 零回归**：`ctest --output-on-failure` 全量零回归（147/147 baseline + 新增测试 PASS）。
7. **ADR 状态翻牌**：ADR-0073 🟡 Partial → ✅ Approved（D2/D3/D4 全 ship）。

**Non-Goals:**

- DECLARE_TOOL V3 宏本身（D4 已 ship，不重新实施）。
- ToolMetadata V3 字段扩展（D2 已 ship）。
- 运行时 output schema 校验（属于 D5/D6 推迟范围，per `adr-0073-impl-scope-audit.md` 行 51-52）。
- V2 工具强制迁移（V2 + V3 共存期由 D5 覆盖，留 Sprint 27+）。
- 业务规则语义的扩展（仅实现 shell/exec 安全规则；其他 tool category 规则留 follow-up）。

## Decisions

### D-1. 复用 D2 input_schema（单一真相源）

**决策**: ToolCoordinator 仅接受外部传入的 `ToolMetadata::input_schema` JSON Schema 字符串 + 通过 `json_schema_validator` 实例化校验，禁止内部 schema 构造逻辑。

**理由**: proposal Capabilities §MUST 强制；单一真相源防止 schema 构造代码与 D2 反射实现 drift。grep 验证 ship gate。

### D-2. 4 步 pipeline 顺序固定 + 短路返回

**决策**: 4 步顺序固定（schema validate → coercion → required field check → business rules）；任意步骤失败立即短路返回，不继续后续步骤。

**理由**: 短路返回避免无意义的 coercion（已知会被拒的字段无需转换），减少错误消息歧义（只报告第一个失败原因）。

### D-3. JSON-RPC `-32602 Invalid params` 错误码边界

**决策**: `-32602 Invalid params` 仅用于参数校验拒绝（4 步 pipeline 失败），不用于路由错误（`-32601 Method not found`）或运行时错误（`-32603 Internal error`）。错误码边界由 ToolCoordinator 错误映射表维护。

**理由**: ADR-0071 §决策 D7 锁定 MCP 2025-11-25 规范的错误码语义；上层 MCP client 标准错误处理依赖此区分。

### D-4. audit event 仅记录 matched_pattern + SHA256(input_args)

**决策**: 业务规则层拒绝时，audit event `tool.audit.denied` 仅记录 `matched_pattern`（如 `rm -rf`）+ args field key 列表 + SHA256(input_args)；**不记录** raw args（含 raw command），防止 shell/exec 等敏感命令泄漏到 trace log。

**理由**: ADR-0068 §5.11 args-only-keys 政策强制；trace log 不可包含用户 secret。

### D-5. V2 legacy tool 仍执行 business rule

**决策**: V2 工具缺 `input_schema` 字段时，pipeline 跳过 schema/coerce/required 三步但仍执行 business rule（path validation 不依赖 schema）。

**理由**: proposal Impact §MUST NOT bypass for any tool；安全路径不能因 schema 缺失而失效。

### D-6. coercion policy per-tool 配置（默认 Coerce 向后兼容）

**决策**: `ToolMetadata::ValidationMode` 字段控制 coercion policy（`Strict` 立即拒 / `Coerce` 自动转换 / `Off` 跳过 coercion）；默认 `Coerce`（向后兼容）。

**理由**: 不同工具对类型严格度的要求不同（LLM 工具期望 Coerce 容忍字符串→数字；安全工具期望 Strict 拒绝任何偏差）。

## Risks / Trade-offs

- **[Risk: 4 步 pipeline 性能开销（每次 call_tool 多 4 步检查）]** → Mitigation: D2 schema 字符串复用 + `json_schema_validator` 实例缓存（per-tool）；基准测试验证 < 1ms 开销。
- **[Risk: 错误消息中包含敏感字段值]** → Mitigation: audit event 仅记录 field_path（不含 value）；错误消息脱敏（仅 type mismatch，不含 actual value）。
- **[Risk: V2 tool 触发 business rule 但缺 path_policy 配置]** → Mitigation: 默认策略表（tool category → path_policy），拒绝 fallback 到 unsafe default。
- **[Risk: nlohmann `json_schema_validator` 与项目 vendor 版本不兼容]** → Mitigation: 已在 `external/nlohmann_json/` vendored，确认版本兼容；CI 阶段跑 sanity check。
- **[Risk: ADR-0073 D3 实施导致 ADR-0069 hook 顺序冲突]** → Mitigation: proposal 已明确 4 步 pipeline 在 `pre_execute_hook` 之前完成（line 80）；CI 验证 hook 顺序。
- **[Risk: schema validate 失败但被 caller 误处理为可恢复]** → Mitigation: 错误消息明确标识 `error_code=InvalidParams`，禁止 retry；ADR-0068 §附录 A 主题已 ship。

## Migration Plan

1. ADR-0073 D2/D4 已 ship（2026-08-14）— 无前置依赖。
2. 本 change 实施 ToolCoordinator 4 步 sanitization pipeline（additive，不破坏现有 V2 工具）。
3. CI 阶段验证所有现有 tools 仍正常执行（baseline 147/147 ctest 不变）。
4. ADR-0073 状态翻牌 🟡 → ✅ Approved（D2/D3/D4 全 ship）。
5. ship 后 `docs/active-status.md` §一 Phase 6c C9 行标记 ✅ ship + ADR 状态行更新。

回滚策略：D3 仅在 ToolCoordinator 入口插入 4 步检查，revert commit 即可恢复 V2-only 行为。无破坏性 schema 变更。

## Open Questions

1. output schema 校验（D5/D6）实施时机？当前推迟至 Sprint 27+，需评估与 runtime validation 的优先级。
2. 业务规则扩展（其他 tool category）由 follow-up change 覆盖还是本 change 延期？当前仅 shell/exec 安全规则，follow-up 留 Sprint 28+。
3. V2 → V3 迁移是否强制（时间表）？当前 V2 + V3 共存期由 ADR-0073 D5 覆盖，留 Sprint 27+。
