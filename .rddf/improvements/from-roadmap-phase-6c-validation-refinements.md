# from-roadmap-phase-6c-validation-refinements

**优先级**: P1 | **来源**: Oracle review `ses_fec4689a4ffeZbJNK9LDO8iWlQ` of `from-roadmap-phase-6c-schema-complete`
**阶段**: wave-1-followup | **分类**: debt
**类型**: fix/refinement
**状态**: 🔍 **Proposed** — 待 guide-arch Phase 5.5 审查/直接作为 OpenSpec change ship
**主题**: ADR-0073 D3 ToolCoordinator validation semantics

> **前置依赖**: `from-roadmap-phase-6c-schema-complete` 已 ship (2026-08-18 archived)
>
> **生产阻塞性**: 🚫 **MUST ship before any production DECLARE_TOOL_V3 tool with non-string typed fields**
> 当前 zero production V3 tools exist, so no active regression; this is a hard gate for V3 adoption.

## 问题背景

`from-roadmap-phase-6c-schema-complete` 已实施 ToolCoordinator 4 步 sanitization pipeline (schema validate → coercion → required field → business rules), 但在 Oracle review 中发现 3 个 P1 级语义缺口：

| # | 问题 | 根因 | 当前行为 vs ADR-0073 声明 |
|---|------|------|---------------------------|
| P1#1 | Warn-mode coercion 对工具不可见 | `ToolCoordinator::execute()` 入参是 `unordered_map<string,string>`; `coerce_args` 将 `"8080"` → `8080` 后，写回循环用 `v.dump()` 又序列化回字符串，下游 registry 永远收到 `"8080"` | ADR: "Warn=自动类型转换"; 实际：转换 transient, 对工具输入无影响 |
| P1#2 | Warn 模式对不可转换输入静默放行 | 当 coercion 失败（如 `"abc"` 对 integer），`coerce_args` 保持原值 → `coerced == args_json` → 不发 warning 也不拒绝 | ADR: 至少应发出 warning; 实际：完全静默 |
| P1#3 | Enum 检查在 coercion 之前执行，误杀合法 Warn 输入 | Step 1 `ToolSchemaValidator` 对 PRE-coercion 的 string 值做 enum 校验：`schema {"type":"integer","enum":[1,2,3]}` + arg `"1"` 会被拒绝为 "value not in enum" | Warn 语义下 `"1"` 应被接受并转为 `1` |

## 影响面

- **生产风险**: 首个使用 `DECLARE_TOOL_V3` 且 schema 含 `integer/number/boolean` 字段的工具会被默认 Strict mode 100% 拒绝 (因为 string-map 输入下类型必然不匹配)
- **ADR 可信度**: 文档声明与代码行为不符
- **审计**: `emit_audit_denied` 当前丢弃 `session_id` / `trace_id`, 影响 validation 拒绝事件关联

## 建议方案

### Option A: 文档化当前行为 (最小改动)

1. 修改 ADR-0073 §D3 / active-status.md：明确声明
   - `ValidationMode::Strict` 仅对 JSON-native 输入有效
   - `ValidationMode::Warn` 的 coercion **不改变** string-map 传输值，仅作可转换性校验 + stderr warning
   - `ValidationMode::Coerce` (新?) 保留未来扩展
2. 同步更新 `DECLARE_TOOL_V3` macro 默认从 `Strict` → `Warn`
3. 修复 P1#2 (non-convertible silent pass): 在 Warn 模式下，若值不可转换，emit warning 并记录 audit
4. 修复 P1#3 (enum pre-coercion): 将 enum mismatch 从 fatal 改为 deferred, 在 coercion 后重试 enum 校验

### Option B: 改变传输层类型 (正确但 BREAKING)

1. 将 `ToolCoordinator::execute(const ToolMetadata&, const unordered_map<string,string>&)` 改为接受 `nlohmann::json` 参数 (或在内部保持 JSON 对象并传给 registry)
2. 更新 `registry.call_tool()` 调用链 / `ToolCallContext` 等下游接口
3. 需要更新所有 existing tool tests and possibly PDK plugins

**推荐 Option A** for short-term fix; Option B as Phase 6d tracked future work.

## 具体任务

1. **P1#1**: 修改 `ToolCoordinator::execute()` 写回逻辑，让 Warn 模式至少能传递转换后的 JSON 值 (if downstream accepts string, convert back via `to_string` not `dump()`; if downstream requires JSON, keep JSON object). Document the final semantics.
2. **P1#2**: 在 `coerce_args` 失败分支 emit `stderr` warning + `tool.audit.denied`/`warn` event with `reason="coercion_failed"`.
3. **P1#3**: 在 `ToolSchemaValidator::validate()` 或 ToolCoordinator 步骤间，对 enum mismatch 标记为 `retry_after_coerce`; after step 2 re-run enum check on coerced value.
4. **Audit fix**: `emit_audit_denied` 应继承 `ctx.session_id` / `ctx.trace_id` (if available), 不要硬编码 `"validation"/""`.
5. **Tests**: 新增 cases
   - Warn + `"1"` with integer enum → accepted
   - Warn + `"abc"` with integer → warning emitted, audit has `reason="coercion_failed"`
   - Strict + `"8080"` → still rejected
   - V2 legacy + enum schema absent → business rule only
6. **Docs**: ADR-0073 D3 语义段更新; active-status.md 添加 follow-up done note.

## 验收标准

- [ ] ToolCoordinator validation test suite ≥10 cases, all PASS
- [ ] ctest full 零回归
- [ ] ADR-0073 文档语义与实际代码一致
- [ ] `DECLARE_TOOL_V3` 默认值与 ToolCoordinator 行为匹配
- [ ] audit denied events carry real session_id/trace_id when available

## 估时

Short — 4-6 小时 (single developer)

## 依赖

- `from-roadmap-phase-6c-schema-complete` ✅ archived
- `from-roadmap-phase-6c-execution-envbackend` ✅ archived (no direct dependency, but good to rebase after)

## 相关文件

- `src/common/tools/tool_coordinator.{h,cpp}`
- `include/agenticdsl/tools/tool_schema_validator.h`
- `src/core/types/tool_result.{h,cpp}` (ErrorCode)
- `include/agenticdsl/pdk/tool_macros.h` (DECLARE_TOOL_V3 default ValidationMode)
- `docs/adr/adr-0073-tool-json-schema-contract.md`
- `docs/active-status.md`
