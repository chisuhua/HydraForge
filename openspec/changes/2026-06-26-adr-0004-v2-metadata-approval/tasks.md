# Tasks: ADR-0004 V2 — ToolRegistry Security (Metadata + Approval)

> **STATUS: ACTIVE** 🟢 — proposal/design/spec/tasks 全部填充完成
> **预估工时**: 1 周 (Sprint 16 主体)
> **关联 master plan**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C6
> **前置依赖**: C3 + C4 全部 ship ✅ (已验证)

---

## 1. IToolRegistry 抽象接口升级 (Day 1)

- [ ] 1.1 `include/agenticdsl/contract/itool_registry.h`: 修改 `register_tool_function()` 签名
      - 增加 `ToolMetadata meta` 入参（非缺省）
      - 虚函数计数: 9 → 9（签名变化，数量不变）
- [ ] 1.2 前向声明 `ToolMetadata`（需 `#include "common/policy/execution_policy.h"`）
- [ ] 1.3 编译验证: `make agenticdsl_core` 通过（此时 SecureToolRegistry 等子类也会失效，预期）

## 2. ToolRegistry 实现升级 (Day 1-2)

- [ ] 2.1 `src/common/tools/registry.h`:
      - `register_tool_function()` 签名同步升级 (name, meta, fn)
      - 存储层新增 `std::unordered_map<std::string, ToolMetadata> tool_metadata_`
- [ ] 2.2 `src/common/tools/registry.cpp`:
      - `register_tool_function()` 实现: 存储 meta → fn 映射
      - `register_default_tools()` 更新: 所有内置工具补 ToolMetadata
- [ ] 2.3 `IToolRegistry` 子类修复:
      - `src/common/tools/secure_tool_registry.cpp`: `register_tool_function` override 同步
      - `tests/test_secure_tool_registry.cpp`: 适配新签名

## 3. 注册时 validation 实现 (Day 2)

- [ ] 3.1 `src/common/tools/registry.cpp` 新增 `validate_tool_metadata()`:
      - 名称非空 + 合法字符检查
      - `category × approval_policy` 冲突检查
      - `min_layer × allowed_layers` 一致性
      - `allowed_layers` 在权限矩阵中合法性检查
- [ ] 3.2 重复名称检测（注册前预检，避免 `unordered_map` 静默覆盖）
- [ ] 3.3 在 `register_tool_function()` 入口调用 validation
- [ ] 3.4 validation 失败抛 `std::invalid_argument`
- [ ] 3.5 `src/common/tools/registry.h` 新增 `list_metadata() const` — 返回全部 `vector<pair<string, ToolMetadata>>`

## 4. DECLARE_TOOL 宏升级 (Day 3)

- [ ] 4.1 `include/agenticdsl/pdk/tool_macros.h`:
      - 签名从 `DECLARE_TOOL(name, desc, ...)` → `DECLARE_TOOL(name, desc, category, policy, ...)`
      - `ToolSpec` 结构体扩展: 增加 `ToolMetadata metadata` 字段
      - `tool_spec_##name` 初始化包含完整 metadata
- [ ] 4.2 `policy` 参数宏内解析: `"plan"` → `ApprovalPolicy{true,true,false,false}`
      `"agent"` → `{true,true,false,false}` / `"yolo"` → `{false,false,true,false}` / `"always"` → `{true,true,true,true}`
- [ ] 4.3 更新头文件注释和用法示例

## 5. DECLARE_TOOL 编译时检查 (Day 3)

- [ ] 5.1 宏参数计数验证: `static_assert` 或 `BOOST_PP` 技巧 — 确认 4 参数必填
- [ ] 5.2 可选: `static_assert` 对 `ToolCategory` 枚举值范围检测（C++20 `std::is_scoped_enum`）

## 6. 示例 plugin 更新 (Day 3-4)

- [ ] 6.1 `examples/phase1_plugin_demo/main.cpp`:
      - 更新 `DECLARE_TOOL` 调用: 补 category + policy 参数
      - 验证编译通过
- [ ] 6.2 `examples/phase1_model_router_plugin/main.cpp`（如果存在类似宏调用）
- [ ] 6.3 编译验证: `make -j$(nproc)` 所有 example 编译通过

## 7. 注册时 Layer × ToolCategory 权限矩阵 (Day 4)

- [ ] 7.1 `src/common/policy/layer_profile.cpp` 新增 `check_registration_permission(ToolMetadata)`:
      - 遍历 `allowed_layers`，对每个 entry 调用 `check_layer_permission()` 验证合法性
      - 若 `allowed_layers` 为空，跳过
- [ ] 7.2 ToolRegistry 在 validate_tool_metadata() 中调用 `check_registration_permission()`
- [ ] 7.3 文档同步: `src/common/policy/layer_profile.h` 注释更新

## 8. TUI `/apply` 桥接增强 (Day 5)

- [ ] 8.1 `src/common/tools/tool_coordinator.cpp`:
      - `call_tool_with_policy()` 中，构造 `ToolPreview` 时填入 `meta_to_json_string(meta)`
- [ ] 8.2 新增 helper: `ToolCoordinator::metadata_to_preview_json(const ToolMetadata&)`:
      - 返回 JSON: `{"name","category","min_layer","allowed_layers","cost_estimate","timeout_ms","approval"}`
- [ ] 8.3 验证: TUI callback 可收到完整 `preview.metadata_json`

## 9. 审批桥接 ToolPreview 增强 (Day 5)

- [ ] 9.1 `src/common/tools/tool_coordinator.cpp`:
      - `call_tool_with_policy()` 中，调用 `ApprovalHandler::process_request()` 前构造 `ToolPreview` 时
        通过新增 helper `metadata_to_json(const ToolMetadata&)` 填充 `preview.metadata_json`
- [ ] 9.2 新增 helper: `ToolCoordinator::metadata_to_json(const ToolMetadata&)`:
      - 返回 JSON: `{"name","category","min_layer","allowed_layers","cost_estimate","timeout_ms","approval"}`
- [ ] 9.3 `tool.audit.invoked` / `tool.audit.completed` payload 不变 (保持审计日志简洁，design Decision 5 明确选择 C)
- [ ] 9.4 验证: TUI `/apply` callback 可收到完整 `preview.metadata_json`（测试通过 `test_tool_coordinator.cpp` 增强 case）

## 10. 测试: ToolRegistry V2 (Day 6-7)

- [ ] 10.1 新建 `tests/test_tool_registry_v2.cpp` (8-10 TEST_CASE):
      - [ ] `register_with_meta`: 注册 + 查询 metadata 一致
      - [ ] `register_without_meta_compile_error`: 编译验证（单独编译单元）
      - [ ] `validate_conflict_execute_never_approval`: Execute + never → throw
      - [ ] `validate_min_layer_not_in_allowed`: mismatch → throw
      - [ ] `validate_allowed_layer_not_in_matrix`: 非法层 → throw
      - [ ] `validate_empty_allowed_layers_ok`: 跳过矩阵检查
      - [ ] `validate_duplicate_name`: 重复 → throw
      - [ ] `register_default_tools_has_meta`: 内置工具都有 meta
- [ ] 10.2 新建 `tests/test_pdk_macros_v2.cpp` (5 TEST_CASE):
      - [ ] `declare_tool_v2_expands`: 宏展开为完整 ToolSpec
      - [ ] `declare_tool_v2_missing_category`: 编译错误（单独编译单元）
      - [ ] `declare_tool_v2_policy_plan_string`: "plan" → correct ApprovalPolicy
      - [ ] `declare_tool_v2_policy_always_string`: "always" → force_approval_always
      - [ ] `declare_tool_v2_handler_works`: handler 包装正确
- [ ] 10.3 新建 `tests/test_layer_profile_matrix.cpp` (4 TEST_CASE):
      - [ ] `check_registration_cognitive_network`: Cognitive + Network → reject
      - [ ] `check_registration_thinking_readonly`: Thinking + ReadOnly → accept
      - [ ] `check_registration_workflow_all`: Workflow + any → accept
      - [ ] `check_registration_empty_layers`: 空 allowed → skip

## 11. 集成测试增强 (Day 7)

- [ ] 11.1 修改 `tests/test_tool_coordinator.cpp`:
      - 增强 "approval handler called with preview" case: 验证 preview.metadata_json 非空
- [ ] 11.2 修改 `tests/test_secure_tool_registry.cpp`:
      - 修复 register_tool_function 签名变更导致的编译错误
      - 增加 V2 metadata 注册 case

## 12. 验证 (Day 8)

- [ ] 12.1 `ctest --output-on-failure` ≥ 47/47 + 新增 N 个 PASS
- [ ] 12.2 `cmake --preset tsan && ctest` 0 race
- [ ] 12.3 `cmake --preset asan && ctest` 0 leak
- [ ] 12.4 `python3 tools/adr_lint.py` exit 0
- [ ] 12.5 `python3 tools/docs_drift_audit.py` 0 critical drift
- [ ] 12.6 `git status` clean

## 13. 同步与归档 (Day 8-9)

- [ ] 13.1 更新 `docs/adr/adr-0004-toolregistry-security.md`: 🟡 Partial → ✅ Approved (V2)
- [ ] 13.2 更新 `docs/README.md` § adr/ 状态表
- [ ] 13.3 更新 `docs/roadmap-status.md` §一 Phase3 进度
- [ ] 13.4 更新 `AGENTS.md` § Recent Changes: C6 Sprint 16 ship
- [ ] 13.5 同步 PDK 头文件: `./scripts/sync-pdk.sh` (双仓库同步)
- [ ] 13.6 更新 master plan C6 行: ✅ archived
- [ ] 13.7 触发 C8 (Phase 4.5 MVP 清理) 启动准备

---

## 验证检查清单 (C6 ship gate)

- [ ] 1. ADR-0004 V2 完整 design
- [ ] 2. ToolRegistry register_tool_function 签名升级 (BREAKING)
- [ ] 3. 注册时 validation 实现 (5 检查项)
- [ ] 4. DECLARE_TOOL 宏升级 (category + policy 强制)
- [ ] 5. 审批桥接 (ToolPreview.metadata_json)
- [ ] 6. 审计日志 payload 增强
- [ ] 7. 权限矩阵注册时检查
- [ ] 8. ctest 全绿 (含新增 3 测试文件 ~17+ TEST_CASE)
- [ ] 9. ASan/TSan 100% clean
- [ ] 10. ADR-0004 status ✅ Approved (V2)
- [ ] 11. master plan C6 状态更新