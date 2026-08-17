## 1. 前置依赖验证

- [ ] 1.1 验证 ADR-0073 D2 已 ship（`ToolMetadata V3` 字段存在于 `src/common/policy/execution_policy.h`）
- [ ] 1.2 验证 ADR-0073 D4 已 ship（DECLARE_TOOL_V3 宏存在于 `include/agenticdsl/pdk/tool_macros.h`）
- [ ] 1.3 验证 `SchemaGenerator` 类型反射在 `include/agenticdsl/tools/schema_generation.h` 已 ship
- [ ] 1.4 grep 验证 D2 input_schema 字段已就绪（`ToolMetadata::input_schema`）

## 2. ToolCoordinator 4 步 sanitization pipeline 实施

- [ ] 2.1 `src/common/tools/tool_coordinator.cpp::call_tool()` 入口插入 4 步顺序检查（schema validate → coercion → required field check → business rules）
- [ ] 2.2 创建 `include/agenticdsl/tools/tool_schema_validator.h`，封装 nlohmann `json_schema_validator` 包装层
- [ ] 2.3 实现 step 1：schema validate（接受 `ToolMetadata::input_schema` JSON Schema 字符串，实例化 validator 校验）
- [ ] 2.4 实现 step 2：coercion（根据 `ToolMetadata::ValidationMode` 决策 — `Strict` 拒绝 / `Coerce` 自动转换 / `Off` 跳过）
- [ ] 2.5 实现 step 3：required field check（遍历 `input_schema.required[]` 字段，缺失抛 `InvalidParams` + field_path）
- [ ] 2.6 实现 step 4：business rules（对 `ToolCategory::Dangerous` 工具强制 path validation + dangerous-pattern blocklist）
- [ ] 2.7 实现 D-2 短路返回：任一步骤失败立即短路，不继续后续步骤

## 3. 错误传播链实施

- [ ] 3.1 定义错误映射表（`ToolCoordinator::map_to_error_code(ValidationStage)`）：schema/coerce/required 4 步全部映射到 `ErrorCode::InvalidParams`（对应 JSON-RPC `-32602`）
- [ ] 3.2 错误消息构造：含 `reason` + `field_path`（稳定 schema path 如 `params.port`）+ expected type（脱敏，不含 actual value）
- [ ] 3.3 emit `tool.audit.denied` 事件（per ADR-0068），payload 含 `reason` + `field_path` + `matched_pattern`（仅 business rule 拒绝时）+ `args_hash`（SHA256）
- [ ] 3.4 4 步任一拒绝 → 不调用下游 `registry.call_tool()`，返回 `ToolResult{ok=false, error_code=ErrorCode::InvalidParams, metadata={...}}`

## 4. 业务规则层实施

- [ ] 4.1 创建 `src/common/policy/dangerous_patterns.cpp`，定义 dangerous pattern blocklist（`rm -rf` / `mkfs` / `:(){ :|:& };:` 等 OWASP 命令注入清单）
- [ ] 4.2 路径检查复用 `common/policy/path_policy.cpp` 现有 `PathPolicy::is_allowed()` 函数
- [ ] 4.3 业务规则匹配逻辑：对 `ToolCategory::Dangerous` 工具遍历 args，匹配 dangerous pattern 即拒
- [ ] 4.4 D-4 audit 脱敏：仅记录 `matched_pattern`（如 `"rm -rf"`）+ args field key 列表 + SHA256(input_args)，**不记录** raw command

## 5. V2 legacy tool 兼容路径 (D-5)

- [ ] 5.1 检测 V2 工具：缺 `input_schema` 字段 → 跳过 schema/coerce/required 3 步
- [ ] 5.2 V2 工具仍执行 step 4 business rule（path validation 不依赖 schema）
- [ ] 5.3 V2 工具 audit event metadata 含 `tool_version=v2` + 跳过步骤列表
- [ ] 5.4 grep 验证 `ToolCoordinator::call_tool()` 无 `if (meta.input_schema.empty()) return;` 早返（必须仍走 business rule）

## 6. 测试用例 (test_tool_coordinator_validation.cpp)

- [ ] 6.1 创建 `tests/test_tool_coordinator_validation.cpp` Catch2 测试文件
- [ ] 6.2 happy path：合法输入通过 4 步 pipeline，返回 `ToolResult{ok=true}`
- [ ] 6.3 schema validate 拒绝：缺 input_schema 字段（required field check 拒）→ `-32602` + field_path
- [ ] 6.4 coercion Strict 模式：`port: "8080"`（字符串而非整数）→ `-32602` + reason="type mismatch"
- [ ] 6.5 coercion Coerce 模式：`port: "8080"` → 自动转换 + audit event 含 coercion 记录
- [ ] 6.6 required field check：`required=["path","mode"]` 但缺 `mode` → `-32602` + field_path="mode"
- [ ] 6.7 business rule 拒绝（shell/exec）：`cmd: "rm -rf /tmp/build"` → `-32602` + matched_pattern
- [ ] 6.8 business rule 通过：`cmd: "echo hello"` → pass
- [ ] 6.9 V2 legacy tool：缺 input_schema 但 cmd="ls" → 跳过 schema 3 步但仍执行 business rule pass

## 7. audit event 验证

- [ ] 7.1 grep 验证 `tool.audit.denied` emit 位置在 4 步 pipeline 拒绝分支（不调用 `registry.call_tool()` 之前）
- [ ] 7.2 audit event payload 含 `args_hash`（SHA256）但**不含** raw args value
- [ ] 7.3 audit event topic + payload 字段匹配 ADR-0068 EventBuilder V2 API（topic, ok, error_code, metadata）

## 8. ADR-0069 ToolCoordinator Hooks 集成验证

- [ ] 8.1 验证 4 步 pipeline 在 `pre_execute_hook` 之前完成（per proposal line 80）
- [ ] 8.2 验证 `tool.audit.denied` 事件在 pre-hook 顺序中正确位置触发
- [ ] 8.3 grep 验证 hook 顺序：`layer_check → 4-step pipeline → pre_execute_hook → ApprovalHandler → registry.call_tool`

## 9. 架构合规性 + ctest 零回归

- [ ] 9.1 grep 验证 ToolCoordinator 代码**无** schema 构造逻辑（仅 `json_schema_validator` 实例化 + 接受外部 schema 字符串）
- [ ] 9.2 grep 验证 `src/common/tools/tool_coordinator.cpp` 仅新增 4 步检查，不修改 `call_tool()` 现有签名
- [ ] 9.3 `ctest --output-on-failure` 全量零回归（baseline 147/147 + 新增 `test_tool_coordinator_validation` ≥9 case 全 PASS）
- [ ] 9.4 `scripts/sprint-closeout.sh` Step 6 验证 ctest + audit grep 全部通过

## 10. ADR 状态翻牌 + 文档同步

- [ ] 10.1 ADR-0073 状态字段：🟡 Partial → ✅ Approved（D2/D3/D4 全 ship）
- [ ] 10.2 `docs/active-status.md` §一 Phase 6c C9 行标记 ✅ ship
- [ ] 10.3 `docs/active-status.md` §一 ADR-0073 状态行更新为 ✅ Approved
- [ ] 10.4 ship commit message 引用 ADR-0073 D2/D3/D4 ship 记录 + ADR-0068 §附录 A audit event 主题引用
