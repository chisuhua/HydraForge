# from-roadmap-phase-6c-schema-complete — Handoff

**Ship date**: 2026-08-18  
**ADR**: ADR-0073 D3 — ToolCoordinator 4 步 sanitization pipeline  
**Status**: ✅ Shipped, 🟡 Follow-up Required

---

## What Shipped

ADR-0073 D3 运行时校验层完整实施：

1. **4 步 pipeline**: schema validate → coercion → required field check → business rules
2. **ToolCoordinator::execute()** 入口插入验证，任一拒绝 → 短路返回 `ErrorCode::InvalidParams` (JSON-RPC -32602)
3. **V2 legacy 共存**: `input_schema` 未设置时跳过前 3 步，仅执行 business rules
4. **Dangerous pattern blocklist**: 5 OWASP patterns (rm -rf / mkfs / fork bomb / dd / >/dev/sd), case-insensitive
5. **Audit redaction**: args 仅记录 SHA256 hash + matched_pattern，不记录 raw command
6. **ErrorCode::InvalidParams**: 新增枚举值 + JSON-RPC -32602 映射
7. **7 test cases**: 1 happy + 4 reject (schema/coerce/required/business) + V2 legacy + safe shell
8. **Documentation**: ADR-0073 🟡 Partial → ✅ Approved; active-status.md Phase 6c C9 ✅ ship

**Verification**: 129/129 ctest PASS, adr_lint 0 errors, docs_drift_audit 1 pre-existing drift (不在本 change 范围)

---

## Known Limitations (P1 — Must Track)

**Oracle review (session `ses_fec4689a4ffeZbJNK9LDO8iWlQ`, 2026-08-18) identified 3 P1 issues that MUST be resolved before production V3 tool adoption:**

### P1#1: Warn-mode coercion 在字符串传输层失效

- **Root cause**: ToolCoordinator::execute() args 签名是 `unordered_map<string,string>`, coerce_args 将 `"8080"` 转为 `int 8080` 但写回时 `v.dump()` 又转回字符串传输给 registry
- **Observable**: test_tool_coordinator_validation.cpp:124 断言 `echo.port == "8080"` 证明类型转换对工具不可见
- **Impact**: ValidationMode::Warn 功能退化为"仅打印 stderr 告警"，ADR-0073 声称的"自动类型转换"未生效
- **Recommendation**: 
  - **Option A**: 修改 ToolCoordinator::execute 签名接受 `nlohmann::json` (BREAKING, 需 registry 适配)
  - **Option B**: 文档化 Warn 模式为"告警-only"，将 coercion 明确标注为"仅用于 schema validate 的前置转换，不修改 registry 输入"
  - **Option C**: 在 Tool 注册时强制 input_schema → `nlohmann::json` 类型参数绑定 (需 DECLARE_TOOL_V3 macro 扩展)

### P1#2: DECLARE_TOOL_V3 默认 Strict 与实际 Warn 不一致

- **Root cause**: `include/agenticdsl/pdk/tool_macros.h:42` (D4 已 ship) 默认 `ValidationMode::Strict`, 而 ToolCoordinator pipeline 实际行为是 Warn (coercion 不生效但不拒绝)
- **Impact**: V3 工具作者期望 strict type check 但实际收到宽松输入
- **Recommendation**: 修改 DECLARE_TOOL_V3 默认为 `ValidationMode::Warn` + 文档注明 Warn 实际为告警-only

### P1#3: Enum coercion 缺失 + 错误码归类混淆

- **Root cause**: coerce_args 对 `"Running"` → enum integer 未实现；ValidationStage::Coercion 拒绝与 SchemaValidate 拒绝都返回同一 `InvalidParams`，无字段级区分
- **Impact**: 
  - Enum 类型参数的 Warn 模式工具会意外拒绝合法字符串输入
  - 调试时无法区分"类型错误"vs"Schema不匹配"
- **Recommendation**: 
  - 补 enum string→int coercion (`nlohmann::json` 反射层或 SchemaGenerator 映射)
  - ValidationResult.errors 字段路径传递给 ToolResult.metadata, 区分 stage

---

## Follow-up Change Requirements

**MUST ship BEFORE production V3 tool adoption**:

```yaml
change_id: from-roadmap-phase-6c-validation-refinements
priority: P1
dependencies:
  - from-roadmap-phase-6c-schema-complete (this change)
tasks:
  - P1#1: 决策 Warn 模式语义 (告警-only vs 真转换) + 文档化 OR 修改传输层
  - P1#2: DECLARE_TOOL_V3 默认改 Warn + 文档注明 Warn 实际行为
  - P1#3: enum coercion 实现 + validation error 字段路径传递
estimate: 4-6 hours (Short)
acceptance:
  - Warn 模式行为与 ADR-0073 声明一致 (文档 OR 代码)
  - DECLARE_TOOL_V3 默认值与实际行为匹配
  - test 覆盖 enum coercion 场景
```

**Optional (P2)**:
- audit event `session_id` / `trace_id` 字段传递 (目前仅有 `metadata` JSON)
- pipeline 与 layer check 的相对顺序单测 (目前隐式依赖实现顺序)
- coercion non-convertible 值的 stderr 告警 (目前静默通过)

---

## Deployment Notes

- **V2 / V3 共存**: 现有 V2 工具 (无 input_schema) 仍可用，business rules 仍强制生效
- **Migration path**: V3 工具默认 Strict mode (per DECLARE_TOOL_V3 macro)，实际行为为 Warn (P1#2 noted)
- **Dangerous 类工具**: 即使 V2，危险 pattern 也被强制拦截 (no opt-out)
- **ErrorCode 兼容性**: 新增 `InvalidParams` (-32602), 不影响现有 `InvalidArg` 使用
- **Performance**: 每次 tool 调用多 1 次 schema validation (nlohmann JSON Schema subset, O(fields))

---

## References

- **ADR-0073**: `docs/adr/adr-0073-tool-json-schema-contract.md` (D3 section, ✅ Approved)
- **Oracle review**: `ses_fec4689a4ffeZbJNK9LDO8iWlQ` (2026-08-18, 12m 59s)
- **Proposal**: `openspec/changes/archive/2026-08-18-from-roadmap-phase-6c-schema-complete/proposal.md`
- **Tests**: `tests/test_tool_coordinator_validation.cpp` (7 cases, 129/129 PASS)
- **Code**: `src/common/tools/tool_coordinator.cpp` (4-step pipeline, lines 507-582)
