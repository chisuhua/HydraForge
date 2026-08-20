## 1. P1#1 Warn-mode coercion 透明化修复

- [ ] 1.1 在 `tool_schema_validator.h` 新增 `retry_after_coerce` flag（仅 Warn 模式设置）
- [ ] 1.2 修改 `tool_coordinator.cpp` coercion 步骤：直接修改 `v`（`nlohmann::json` 引用），不调用 `v.dump()` 重序列化
- [ ] 1.3 验证 `"1"` integer coercion 后 `v.get<int>()` 返回 1（非字符串 `"1"`）
- [ ] 1.4 验证 Strict 模式下 coercion 行为不变（仅 Warn 模式改变）

## 2. P1#2 不可转换输入 Warn-mode 处理

- [ ] 2.1 在 `tool_coordinator.cpp` coercion 步骤新增不可转换检测（`nlohmann::json::parse` 失败或类型不匹配）
- [ ] 2.2 不可转换时 emit stderr warning（格式：`[ToolCoordinator] coercion failed for field X: cannot convert "abc" to integer`）
- [ ] 2.3 不可转换时 emit `tool.audit.denied` event，`reason="coercion_failed"`，使用 EventBuilder V2
- [ ] 2.4 验证 Warn 模式下 `"abc"` integer 输入产生 warning + audit event（非静默放行）

## 3. P1#3 enum pre-coercion 误杀修复

- [ ] 3.1 在 `tool_schema_validator.h` 定义 `retry_after_coerce` enum（`Skip / RetryOnce`）
- [ ] 3.2 修改 enum 校验逻辑：pre-coercion 失败时设置 `retry_after_coerce=RetryOnce`（Warn 模式）
- [ ] 3.3 coercion 完成后检查 `retry_after_coerce` flag，若为 RetryOnce 则重试 enum 校验
- [ ] 3.4 验证场景：Warn + `"1"` integer+enum[1,2,3] → accepted after coerce

## 4. Audit denied event session_id/trace_id 继承修复

- [ ] 4.1 修改 `tool_coordinator.h` `emit_audit_denied` 函数签名：新增 `const ToolCallContext& ctx` 参数
- [ ] 4.2 在 `tool_coordinator.cpp` 调用点传递 `ctx`（不硬编码 `"validation"/""`）
- [ ] 4.3 验证 audit denied event 包含真实 `session_id`（从 ctx.session_id 读取）
- [ ] 4.4 验证 audit denied event 包含真实 `trace_id`（从 ctx.trace_id 读取）
- [ ] 4.5 验证 ctx 缺少 session_id/trace_id 时优雅降级（不崩溃，使用空字符串）

## 5. DECLARE_TOOL_V3 默认值变更 + ADR-0073 D3 文档同步

- [ ] 5.1 修改 `include/agenticdsl/pdk/tool_macros.h`：`DECLARE_TOOL_V3` 默认 `ValidationMode::Warn`（从 `Strict` 变更）
- [ ] 5.2 更新 `docs/adr/adr-0073-validation-pipeline.md` D3 §语义：coercion 透明化 + 不可转换 warning + enum retry 行为
- [ ] 5.3 更新 `docs/adr/adr-0073-validation-pipeline.md` §coercion 声明：明确"类型转换对工具可见"
- [ ] 5.4 更新 `docs/adr/adr-0073-validation-pipeline.md` §warning 声明：明确 Warn 模式下发 warning + audit event
- [ ] 5.5 验证 `DECLARE_TOOL_V3` 调用点（无需显式指定 ValidationMode）默认 Warn

## 6. ToolCoordinator validation test suite 新增

- [ ] 6.1 创建 `tests/test_tool_coordinator_validation.cpp` Catch2 测试文件
- [ ] 6.2 P1#1 case: Warn-mode integer coercion 值保留验证
- [ ] 6.3 P1#1 case: Strict-mode integer coercion boundary preservation
- [ ] 6.4 P1#2 case: Warn-mode `"abc"` integer → stderr warning emitted
- [ ] 6.5 P1#2 case: Warn-mode `"abc"` integer → `tool.audit.denied` event with reason="coercion_failed"
- [ ] 6.6 P1#2 case: Warn-mode `"abc"` integer → result is not silently passed through
- [ ] 6.7 P1#3 case: Warn + `"1"` integer+enum[1,2,3] → accepted after coerce
- [ ] 6.8 P1#3 case: Strict + `"1"` integer+enum[1,2,3] → still rejected (pre-coercion fails)
- [ ] 6.9 Audit case: denied event carries real session_id from ctx
- [ ] 6.10 Audit case: denied event carries real trace_id from ctx
- [ ] 6.11 Audit case: ctx without session_id/trace_id → graceful degradation to empty string
- [ ] 6.12 Legacy case: V2 tool without enum schema → business rule only path

## 7. ctest 全量零回归验证

- [ ] 7.1 运行 `ctest --output-on-failure`，确认 baseline 147/147 PASS
- [ ] 7.2 确认新增 `test_tool_coordinator_validation` ≥10 cases 全部 PASS
- [ ] 7.3 运行 `ctest -R test_tool_coordinator_validation`，验证新增测试独立 PASS
- [ ] 7.4 运行 `ctest -R tool_coordinator`，验证所有 tool_coordinator 相关测试 PASS
- [ ] 7.5 运行 `ctest -R tool_schema`，验证所有 tool_schema 相关测试 PASS

## 8. 代码质量 + 架构合规检查

- [ ] 8.1 `grep -r "v.dump()" src/common/tools/tool_coordinator.cpp` 确认 coercion 后无 JSON 重新序列化
- [ ] 8.2 `grep -r "ValidationMode::Strict" include/agenticdsl/pdk/tool_macros.h` 确认 DECLARE_TOOL_V3 默认值已变更
- [ ] 8.3 验证 `emit_audit_denied` 所有调用点已更新为传递 ctx 参数
- [ ] 8.4 验证 EventBuilder V2 用于所有新增 audit event（非 raw BusEvent）
