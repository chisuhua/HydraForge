## Context

ADR-0073 D3 已 ship `ToolCoordinator` 4 步 sanitization pipeline（schema validate → coercion → required field → business rules）。Oracle review（`ses_fec4689a4ffeZbJNK9LDO8iWlQ`）发现 3 个 P1 语义缺口，导致 ADR 文档声明与代码行为不一致，且在 `DECLARE_TOOL_V3` 进入生产前必须修复：

- **P1#1**: Warn-mode coercion 对工具不可见（写回 `v.dump()` 重新序列化，转换 transient）
- **P1#2**: 不可转换输入在 Warn 模式下静默放行（不发 warning 也不拒绝）
- **P1#3**: Enum 检查在 coercion 之前执行，误杀合法 Warn 输入

当前 `DECLARE_TOOL_V3` macro 默认 `Strict` mode，导致首个使用 schema 含 `integer/number/boolean` 字段的工具在 string-map 输入下 100% 拒绝（类型必然不匹配）。同时 `emit_audit_denied` 硬编码 `"validation"/""` 丢弃真实 `session_id`/`trace_id`，影响 validation 拒绝事件关联。

依赖关系（per roadmap.md）：
- **前置**: `from-roadmap-phase-6c-execution-baseline` change 已 ship（提供 baseline 测量数据）
- **后置 unlock**: 本 change PASS → `from-roadmap-phase-6c-execution-dsl` 提案可启动 Phase 6d Option B 传输层重构

## Goals / Non-Goals

**Goals:**

1. 修复 P1#1 Warn-mode coercion 透明化：coercion 结果正确写回 string-map（保留 JSON 转换值，不因 `v.dump()` 重新序列化丢失）
2. 修复 P1#2 不可转换输入静默放行：Warn 模式下不可转换输入 emit stderr warning + `tool.audit.denied` event，`reason="coercion_failed"`
3. 修复 P1#3 enum pre-coercion 误杀：enum 校验标记 `retry_after_coerce`，coercion 完成后重试 enum 校验（Warn + `"1"` integer enum → accepted）
4. Audit fix：`emit_audit_denied` 继承 `ctx.session_id`/`ctx.trace_id`（不硬编码 `"validation"/""`）
5. ADR-0073 D3 文档语义与代码实现双向同步（代码变更触发 ADR 更新）
6. `DECLARE_TOOL_V3` macro 默认从 `Strict` → `Warn`

**Non-Goals:**

- Option B 传输层类型变更（`unordered_map<string,string>` → `nlohmann::json`，BREAKING）— 推迟 Phase 6d
- V2 legacy 工具行为变化（保持向后兼容）
- 新增 `ValidationMode::Coerce`（保留未来扩展位）
- 修改 ToolCoordinator 4 步 pipeline 顺序（schema validate → coercion → required field → business rules 保持不变）

## Decisions

### D-1. Warn-mode coercion 值保留策略

**决策**: coercion 结果直接修改 `v`（`nlohmann::json` 引用），不依赖 `v.dump()` 重新序列化。coercion 后原类型转换对调用方透明可见（integer 输入 `"1"` → `1`，工具通过 `v.get<int>()` 获取正确类型）。

**替代方案拒绝**:
- `v.dump()` 重序列化（当前行为，P1#1 根因）：JSON 序列化丢失类型信息，转换是 transient
- 新增 parallel `coerced_values` map（P1#2 方案）：增加 API 复杂度，coercion 结果应直接写回 v

**理由**: ADR-0073 D3 §coercion 声明"类型转换对工具透明"，当前实现违反该语义。直接修改 `v` 是最小侵入修复。

### D-2. 不可转换输入 Warn-mode 处理策略

**决策**: 不可转换输入在 Warn 模式下 emit stderr warning + `tool.audit.denied` event（`reason="coercion_failed"`），不拒绝不放行，静默路径消除。

**替代方案拒绝**:
- 静默放行（当前 P1#2 行为）：违反 ADR-0073 D3 §warning 声明，用户无感知
- 严格拒绝（Strict 模式）：Warn 模式本意为"记录 warning 但不放行"，当前实现无误杀风险

**理由**: Warn 模式语义是"宽松但不丢信息"，coercion 失败必须可观测。stderr + audit event 双通道确保用户与系统都能感知。

### D-3. Enum 校验时机策略

**决策**: enum 校验标记 `retry_after_coerce` flag，coercion 完成后若之前 enum 校验失败则重试 enum 校验（仅 Warn 模式）。

**替代方案拒绝**:
- 移除 pre-coercion enum 校验（过度修复）：可能导致非法 enum 值在 Strict 模式下也误放
- 仅在 post-coercion 执行 enum 校验（破坏 Strict 模式语义）：Strict 模式应在 coercion 失败时立即拒绝

**理由**: Warn 模式下用户可能输入 `"1"` 而 schema 要求 `integer` + `enum [1, 2, 3]`，coercion 后重试 enum 校验保证合法输入通过。

### D-4. DECLARE_TOOL_V3 默认值变更

**决策**: `DECLARE_TOOL_V3` macro 默认 `ValidationMode::Warn`（从 `Strict` 变更），与 ToolCoordinator Warn 模式行为匹配。

**替代方案拒绝**:
- 保持 `Strict` 默认（当前行为）：生产工具在 string-map 输入下 100% 拒绝，无法渐进迁移
- `Coerce` 默认（过度设计）：coercion 是副作用行为，默认开启违背最小惊讶原则

**理由**: ADR-0073 D3 生产就绪要求 `DECLARE_TOOL_V3` 默认 Warn，新工具开发者无需显式指定即可获得合理行为。

### D-5. Audit denied event session_id/trace_id 继承策略

**决策**: `emit_audit_denied` 接收 `const ToolCallContext& ctx` 参数，从 ctx 读取 `session_id`/`trace_id`（若存在），不再硬编码 `"validation"/""`。

**替代方案拒绝**:
- 硬编码 `"validation"/""`（当前 P1#4 行为）：validation 拒绝事件无法关联到具体 session/trace，审计链路断裂
- 从 BusEvent metadata 推断（不可靠）：metadata 可能缺失，BusEvent 不保证包含 session 上下文

**理由**: ToolCallContext 在 ToolCoordinator 入口已填充 session_id/trace_id，直接复用是最可靠方案。

## Architecture

### 改动文件清单

| 文件 | 改动类型 | 说明 |
|------|----------|------|
| `src/common/tools/tool_coordinator.cpp` | 修改 | P1#1/P1#2/P1#3 修复 + emit_audit_denied session_id 继承 |
| `src/common/tools/tool_coordinator.h` | 修改 | emit_audit_denied 函数签名变更（新增 ctx 参数） |
| `include/agenticdsl/tools/tool_schema_validator.h` | 修改 | `retry_after_coerce` flag 添加 + enum 校验时机调整 |
| `include/agenticdsl/pdk/tool_macros.h` | 修改 | `DECLARE_TOOL_V3` 默认值 Strict → Warn |
| `tests/test_tool_coordinator_validation.cpp` | 新增 | ≥10 cases 覆盖 P1#1/#2/#3 + audit session_id 传递 |
| `docs/adr/adr-0073-validation-pipeline.md` | 修改 | D3 §语义同步（文档与代码一致） |

### 数据流

```
Input JSON (string-map)
    │
    ▼
[Step 1] Schema Validation
    │ (validate_schema)
    ▼
[Step 2] Coercion (P1#1/P1#2 fix)
    │ - 直接修改 v (nlohmann::json 引用)
    │ - 不可转换 → emit stderr + tool.audit.denied (reason=coercion_failed)
    ▼
[Step 3] Enum Check (P1#3 fix)
    │ - 若 pre-coercion enum 失败且 retry_after_coerce=true → coercion 后重试
    ▼
[Step 4] Business Rules
    │
    ▼
Output / Rejection
```

### 关键场景验证矩阵

| 场景 | Mode | Input | Schema | 期望行为 |
|------|------|-------|--------|----------|
| P1#1 回归 | Warn | `"1"` | integer | coerce 保留，工具获取 int 1 |
| P1#2 回归 | Warn | `"abc"` | integer | stderr warning + audit denied(reason=coercion_failed) |
| P1#3 回归 | Warn | `"1"` | integer+enum[1,2,3] | coerce 后 enum 重试 → accepted |
| Strict 边界 | Strict | `"8080"` | integer | 仍拒绝（P1#1 boundary preservation） |
| Legacy 兼容 | V2 | (any) | enum absent | business rule only path |
| Audit 修复 | any | (any) | (any) | session_id/trace_id 真实传递 |
