# Design: ADR-0004 V2 — ToolRegistry Security (Metadata + Approval)

> **STATUS: ACTIVE** 🟢 — 设计完成
> **前置依赖**: C3 (P1-P2 IExecutionPolicy) + C4 (P3-P4 ToolCoordinator + LayerProfile) — 已全部 ship

## Decision 1: register_tool_function 签名升级

### 问题

`ToolRegistry::register_tool_function()` 当前只接受 `(string name, ToolFunc fn)`，不包含元数据。

### 选项

- **A**: 新增重载 `register_tool_function(name, meta, fn)`，旧签名标记 `[[deprecated]]`
- **B**: 直接替换旧签名为 `register_tool_function(name, meta, fn)`（BREAKING）
- **C**: 独立方法 `register_metadata(name, meta)` + 现有 `register_tool_function(name, fn)` 组合调用

### 决策

**选择 B** — 直接替换旧签名。

理由：
1. 仅有 1 个内部调用点 `register_default_tools()`（`registry.cpp`），无外部消费者
2. PDK DECLARE_TOOL 宏同时升级，不存在遗留旧路径
3. 拆分注册（Option C）允许工具注册后不绑定 metadata，违背安全模型
4. `[[deprecated]]` 过渡（Option A）增加 tech debt，C6 本就是主动 upgrade

### 实现

```cpp
// itool_registry.h (IToolRegistry 接口)
virtual void register_tool_function(std::string name,
                                    ToolMetadata meta,
                                    ToolFunc fn) = 0;

// registry.h (ToolRegistry 实现) — register_tool_function 签名升级
void register_tool_function(std::string name,
                            ToolMetadata meta,
                            ToolFunc fn) override;

// registry.h (ToolRegistry 模板) — register_tool<Func> 模板同步升级
// 旧签名: register_tool(std::string name, Func&& func)
// 新签名: register_tool(std::string name, ToolMetadata meta, Func&& func)
template<typename Func>
void register_tool(std::string name, ToolMetadata meta, Func&& func) {
    ToolFunc erased = [fn = std::forward<Func>(func)](const auto& args) {
        return fn(args);
    };
    register_tool_function(std::move(name), std::move(meta), std::move(erased));
}
```

**受影响调用点**（全部需升级）:
1. `register_default_tools()` 中 3 个内置工具 — 每个补 ToolMetadata
2. `IToolRegistry` 所有子类: `SecureToolRegistry`、`MockToolRegistry`(测试)
3. `tests/test_tool_registry_interface.cpp` / `test_engine_factory.cpp` / `test_execute_parallel.cpp` / `test_plugin_loader.cpp` — mock 实现

## Decision 2: 注册时 validation 策略

### 问题

注册时需检测元数据冲突。但 validation 到何种程度？

### 选项

- **A**: 最小 validation — 仅检测 `category × approval_policy` 不可行组合
- **B**: 严格 validation — A + `min_layer × allowed_layers` 一致性 + 名称唯一性 + `cost_estimate` 范围
- **C**: 可配置 validation severity — `ToolRegistryConfig` 枚举 (`relaxed` / `strict` / `audit-only`)

### 决策

**选择 B** — 严格 validation。
- validation 失败 → `std::invalid_argument` throw（与现有 error handling 一致）
- 检查项：
  1. 名称非空、不包含非法字符 (`::` 前缀保留，`/` 用于域分隔)
  2. `allowed_layers` 若非空：每个条目对应的 category 在权限矩阵中合法
  3. `category = Execute` 且 `approval = {false, false, true}` — 不安全，throw
  4. `min_layer` 一致性：若 `min_layer = Workflow`，`allowed_layers` 必须包含 Workflow
  5. 名称不重复（哈希表已保证，throw 更早）

### 例外

`cost_estimate` 和 `timeout_ms` 的 IBudgetController / std::async 集成 defer 至 C8，C6 不做范围检查。

## Decision 3: DECLARE_TOOL 宏参数升级

### 问题

当前 `DECLARE_TOOL(name, description, body)` 不强制类别和审批策略。

### 选项

- **A**: 宏签名升级为 `DECLARE_TOOL(name, description, category, policy, body)` — 4 强制参数
- **B**: 结构体参数 `DECLARE_TOOL(name, description, {.category=..., .policy=...}, body)` — designated initializer
- **C**: 属性宏 `TOOL_CATEGORY(category)` + `DECLARE_TOOL(name, desc, body)` 分离声明

### 决策

**选择 A** — 4 强制参数。

理由：
1. Option B 需要 C++20 designated initializer，PDK 要求 C++20 已满足
2. Option C 增加心智负担（开发者需记得配套声明）
3. 4 参数形式与 `ToolMetadata` 字段一一对应，IDE 可自动提示

### 签名

```cpp
#define DECLARE_TOOL(name, description, category, approval_policy, ...)
```

- `category`: `ToolCategory` 枚举值（`ReadOnly` / `WriteFile` / `Execute` / `Network` / `StateModify`）
- `approval_policy`: 简化字符串（`"plan"` / `"agent"` / `"yolo"` / `"always"`），宏内展开为 `ApprovalPolicy` 结构体
- 宏展开：自动生成 `tool_spec_##name`（含完整 ToolMetadata）+ `tool_handler_##name`

### 迁移

现有 2 个调用点需要更新：
1. `include/agenticdsl/pdk/tool_macros.h` 中的 `DECLARE_TOOL` 宏测试案例（头文件内注释示例）
2. `examples/phase1_plugin_demo/` 中的实际使用

## Decision 4: 注册时权限矩阵集成

### 问题

当前 `check_layer_permission()` 仅在执行时检查。注册时应该验证工具的 `allowed_layers` 与 `ToolCategory` 的兼容性。

### 选项

- **A**: 注册时检查 matrix + 保持运行时 check_layer_permission 不变
- **B**: 统一为注册时检查，运行时不再重复
- **C**: 注册时 relax（warn only），运行时 strict（throw）

### 决策

**选择 A** — 注册时静态验证 + 运行时保持。

理由：
1. 注册时 catch 明显误配置（e.g. Network 工具声明 `allowed_layers = [Cognitive]`）
2. 运行时仍需要 check（Session 动态注入可能更改 layer）
3. 双保险：注册时 throw + 运行时 throw（ToolCoordinator 现有逻辑）

### 验证矩阵

```
allowed_layers ⊆ matrix[category]
where matrix[c][l] 来自 ADR-0004 §8:
  Cognitive  ← {ReadOnly}
  Thinking   ← {ReadOnly, WriteFile}
  Workflow   ← {ReadOnly, WriteFile, Execute, Network, StateModify}
```

## Decision 5: TUI 桥接增强方式

### 问题

当前 `/apply` 事件仅传递工具名和 args keys。需要显示完整 V2 元数据让用户知情审批。

### 选项

- **A**: 在审计日志 `tool.audit.invoked` payload 中附加 ToolMetadata 完整信息
- **B**: 独立事件 `tool.metadata.requested` 用于按需查询
- **C**: 只在 ToolCoordinator 审批流程中附加 metadata，不修改审计格式

### 决策

**选择 C** — ToolCoordinator 审批流程中附加 metadata。

理由：
1. 审计日志 `tool.audit.invoked` 应该保持 concise（已有 args keys，加 metadata 会使 payload 膨胀）
2. ToolCoordinator 在发起 `ApprovalHandler::process_request()` 前已有 `meta` 对象
3. 将 metadata 传给 TUI bridge 的 callback，由 TUI 渲染器决定显示内容

### 实现

```cpp
// ToolCoordinator 审批前 (现有, C4 ship)
if (approval_handler_ && meta_) {
    auto preview = ToolPreview{args_key_only, ""};
    approval_handler_->process_request(*meta_, ctx, preview);
}

// 增强后 (C6)
if (approval_handler_ && meta_) {
    auto preview = ToolPreview{
        args_key_only_or_full,  // TUI 可选显示
        meta_to_json_string(*meta_)  // V2 metadata 完整 JSON
    };
    approval_handler_->process_request(*meta_, ctx, preview);
}
```

## 引用

- `docs/adr/adr-0004-toolregistry-security.md` — ADR-0004 V1 安全模型
- `src/common/policy/execution_policy.h` — ToolMetadata / ToolCategory / ApprovalPolicy / LayerProfile 定义
- `src/common/tools/tool_coordinator.{h,cpp}` — ToolCoordinator 5-step middleware
- `src/common/tools/registry.{h,cpp}` — ToolRegistry 主实现
- `include/agenticdsl/contract/itool_registry.h` — IToolRegistry 抽象接口
- `include/agenticdsl/pdk/tool_macros.h` — DECLARE_TOOL 宏 MVP