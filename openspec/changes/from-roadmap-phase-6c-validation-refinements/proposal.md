# from-roadmap-phase-6c-validation-refinements

## Why

ADR-0073 D3 已 ship `ToolCoordinator` 4 步 sanitization pipeline（schema validate → coercion → required field → business rules）。Oracle review (`ses_fec4689a4ffeZbJNK9LDO8iWlQ`) 发现 3 个 P1 语义缺口，导致 ADR 文档声明与代码行为不一致，且在 `DECLARE_TOOL_V3` 进入生产前必须修复：

- **P1#1**: Warn-mode coercion 对工具不可见（写回 `v.dump()` 重新序列化，转换 transient）
- **P1#2**: 不可转换输入在 Warn 模式下静默放行（不发 warning 也不拒绝）
- **P1#3**: Enum 检查在 coercion 之前执行，误杀合法 Warn 输入

生产阻塞：首个使用 `DECLARE_TOOL_V3` + schema 含 `integer/number/boolean` 字段的工具在 Strict 默认 mode 下 100% 拒绝（string-map 输入下类型必然不匹配）。同时 `emit_audit_denied` 丢弃 `session_id`/`trace_id`，影响 validation 拒绝事件关联。

## What Changes

**In Scope**:

- 修复 P1#1 Warn-mode coercion 透明化（保留 JSON 转换值，正确写回 string-map）
- 修复 P1#2 不可转换输入静默放行（emit stderr warning + `tool.audit.denied`/`warn` event with `reason="coercion_failed"`）
- 修复 P1#3 enum pre-coercion 误杀（标记 `retry_after_coerce`，coercion 后重试 enum 校验）
- Audit fix：`emit_audit_denied` 继承 `ctx.session_id`/`ctx.trace_id`（不硬编码 `"validation"/""`）
- ADR-0073 D3 文档语义更新（声明与代码一致）
- `DECLARE_TOOL_V3` macro 默认从 `Strict` → `Warn`
- ToolCoordinator validation test suite 新增 ≥4 cases（覆盖 P1#1/#2/#3 + audit session_id 传递）

### 关键场景

1. **Warn + `"1"` integer enum** → accepted after coerce（regression fix for P1#3）
2. **Warn + `"abc"` integer** → warning emitted, audit reason="coercion_failed"（P1#2 + audit fix）
3. **Strict + `"8080"` integer schema** → still rejected（P1#1 boundary preservation）
4. **V2 legacy + enum schema absent** → business rule only path（向后兼容）
5. **Audit denied event** → session_id/trace_id 真实传递，非硬编码空字符串
6. **`DECLARE_TOOL_V3` macro 调用** → 默认 `ValidationMode::Warn`，与 ToolCoordinator 行为匹配

**Out of Scope**:

- Option B 传输层类型变更（`unordered_map<string,string>` → `nlohmann::json`，BREAKING）— 推迟 Phase 6d
- V2 legacy 工具行为变化（保持向后兼容）
- 新增 `ValidationMode::Coerce`（保留未来扩展位）

## Capabilities

- 改动范围限定在 `src/common/tools/tool_coordinator.{h,cpp}` + `include/agenticdsl/tools/tool_schema_validator.h` + `include/agenticdsl/pdk/tool_macros.h`
- ADR-0073 D3 文档语义与代码实现保持一致（双向同步：代码变更触发 ADR 更新，反之亦然）
- V2 legacy 路径行为保持完全不变（向后兼容，已有工具零回归）
- 新增审计事件必须使用 EventBuilder V2（`include/agenticdsl/contract/event_builder.h`），不直接 emit raw BusEvent
- ToolCoordinator 4 步 pipeline 顺序保持不变：schema validate → coercion → required field → business rules
- ctest full 零回归（基线 147/147 不能引入新 fail）
- Option B 传输层 BREAKING 变更不在本次范围
- 新增 validation 测试 ≥10 cases，全部 PASS

## Impact

- (no items specified)

## Acceptance

- [ ] ToolCoordinator validation test suite ≥10 cases, all PASS
- [ ] ctest full 零回归
- [ ] ADR-0073 文档语义与实际代码一致
- [ ] `DECLARE_TOOL_V3` 默认值与 ToolCoordinator 行为匹配
- [ ] audit denied events carry real session_id/trace_id when available

