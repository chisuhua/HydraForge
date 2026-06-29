# ADR-0031: IExecutionPolicy 执行策略与三模式审批

## 状态

**✅ Approved (2026-07-31, Oracle session `ses_0faa4dabeffeHGFoLdXE7AqwH7`, 5-method interface + sync callback + Agent default + YOLO confirm)**

## 领域

基座 / 安全 / 执行策略

## 关联

- ADR-0004（ToolRegistry 安全模型）— ToolCategory、ApprovalPolicy、LayerProfile
- ADR-0002（EventBus）— 审批事件传输
- ADR-0030（AsyncRuntime）— 协程挂起等待审批
- ADR-0031 自身 §2（三模式定义）— Plan/Agent/YOLO 模式定义

---

## 背景

### 当前代码库状态

| 组件 | 现状 | 评估 |
|------|------|------|
| **模式概念** | ❌ **零实现**——无 Plan/Agent/YOLO 模式 | 需要新增 |
| **审批机制** | ❌ **零实现**——工具调用后直接执行 | 需要新增 |
| **ToolRegistry** | ⚠️ 简单实现——仅 `name` + `std::function` | 需增加元数据 |
| **NodeExecutor** | ⚠️ 同步执行——`execute_tool_call()` 直接调用 | 需增加协程路径 |
| **EventBus** | ❌ **零实现**——ADR-0002 仅有设计文档 | 依赖 ADR-0002 V2 |

### 问题

1. **无模式感知**：Plan/Agent/YOLO 三种模式下工具行为应不同，但当前无此概念
2. **无审批机制**：工具调用后直接执行，无法在执行前暂停等待用户确认
3. **审批与协程集成**：等待用户 `/apply` 是一个长等待操作，需要 async_simple 协程支持
4. **Layer Profile 未集成**：ADR-0004 设计了 Cognitive/Thinking/Workflow 三层权限，但与执行流程未绑定

---

## 决策

### 1. IExecutionPolicy 接口（基座层定义 — 5 方法精简接口）

**关键决策**：基座层（`src/common/`）定义接口 + 三种默认实现，认知层可以扩展但基座不依赖认知层。

**Oracle 决议** (session `ses_0faa4dabeffeHGFoLdXE7AqwH7`)：从 8 个 virtual 方法精简为 5 个核心方法。原 `should_auto_execute()`、`should_show_plan()`、`should_show_result_summary()`、`should_auto_decide_retry()`、`should_show_reflection()`、`fleet_max_concurrency()`、`mode_name()` 等配置型方法迁移至 `ModeConfig` 值结构体，不在接口中定义。

```cpp
// ===== src/common/policy/execution_policy.h =====

// 模式配置值结构体（替代接口中的配置型方法）
struct ModeConfig {
    bool show_plan = true;               // 是否展示计划（原 should_show_plan + should_auto_execute 组合）
    bool show_result_summary = true;     // 是否显示结果摘要
    bool auto_decide_retry = true;       // 是否自动决定重试
    bool show_reflection = true;         // 是否展示反思
    size_t fleet_max_concurrency = 8;    // 舰队最大并行度
    std::string mode_name = "agent";     // 模式名称
};

// 工具调用的上下文信息（辅助决策）
struct ToolCallContext {
    std::string session_id;
    std::string caller_layer;          // "cognitive" / "thinking" / "workflow"
    std::string target_path;           // 目标文件/目录路径
    bool is_in_fleet_mode = false;     // 是否在舰队模式中
    size_t call_count_this_session = 0; // 本 session 第几次调用
};

// 审批回调类型（同步，非协程）
using ApprovalCallback = std::function<void(bool approved, const std::string& reason)>;

// 执行策略接口（5 方法精简版）
class IExecutionPolicy {
public:
    virtual ~IExecutionPolicy() = default;

    // 核心决策：此工具调用是否需要用户审批？
    virtual bool requires_approval(const ToolMetadata& meta,
                                   const ToolCallContext& ctx) const = 0;

    // 条件决策：在 requires_approval=true 的前提下，是否真正执行？
    // false = 拒绝执行（无需等待用户审批）
    virtual bool should_execute(const ToolMetadata& meta,
                                const ToolCallContext& ctx) const = 0;

    // 条件决策：是否可以跳过此工具调用（非阻塞跳过）？
    virtual bool can_skip(const ToolMetadata& meta,
                          const ToolCallContext& ctx) const = 0;

    // 返回工具所属的权限层（用于 Layer Profile 校验）
    virtual int get_layer(const ToolMetadata& meta) const = 0;

    // 请求用户审批（同步回调模式，非协程）
    // callback 在用户响应后被调用 — 传递 approved 和 reason
    virtual void request_approval(const ToolMetadata& meta,
                                  const ToolCallContext& ctx,
                                  const ToolPreview& preview,
                                  ApprovalCallback callback) = 0;

    // 获取模式配置（非 virtual，由构造注入）
    const ModeConfig& config() const { return config_; }

protected:
    ModeConfig config_;  // 子类构造时设置
};
```

### 2. 三种模式的策略实现

```cpp
// ===== src/common/policy/execution_policy.cpp =====

// -------------------- Plan 模式 --------------------
class PlanModePolicy : public IExecutionPolicy {
public:
    PlanModePolicy() {
        config_.show_plan = true;
        config_.show_result_summary = true;
        config_.auto_decide_retry = false;
        config_.show_reflection = true;
        config_.fleet_max_concurrency = 8;
        config_.mode_name = "plan";
    }

    bool requires_approval(const ToolMetadata& meta,
                           const ToolCallContext& ctx) const override {
        // Plan 模式：所有写入操作都需要审批
        return meta.category != ToolCategory::ReadOnly;
    }

    bool should_execute(const ToolMetadata& meta,
                        const ToolCallContext& ctx) const override {
        return true;  // Plan 模式通常批准后执行
    }

    bool can_skip(const ToolMetadata& meta,
                  const ToolCallContext& ctx) const override {
        return false;  // Plan 模式不跳过工具调用
    }

    int get_layer(const ToolMetadata& meta) const override {
        return meta.approval.default_layer;
    }

    void request_approval(const ToolMetadata& meta,
                          const ToolCallContext& ctx,
                          const ToolPreview& preview,
                          ApprovalCallback callback) override {
        // 同步回调：UI 层调用此方法，callback 在用户响应后被调用
        callback(true, "");  // 默认自动批准（实际由 UI 层拦截）
    }
};

// -------------------- Agent 模式 --------------------
class AgentModePolicy : public IExecutionPolicy {
public:
    AgentModePolicy() {
        config_.show_plan = false;
        config_.show_result_summary = true;
        config_.auto_decide_retry = true;
        config_.show_reflection = false;
        config_.fleet_max_concurrency = 16;
        config_.mode_name = "agent";
    }

    bool requires_approval(const ToolMetadata& meta,
                           const ToolCallContext& ctx) const override {
        // Agent 模式：遵循工具自身的审批策略
        return meta.approval.requires_approval_in_agent;
    }

    bool should_execute(const ToolMetadata& meta,
                        const ToolCallContext& ctx) const override {
        return true;
    }

    bool can_skip(const ToolMetadata& meta,
                  const ToolCallContext& ctx) const override {
        return false;
    }

    int get_layer(const ToolMetadata& meta) const override {
        return meta.approval.default_layer;
    }

    void request_approval(const ToolMetadata& meta,
                          const ToolCallContext& ctx,
                          const ToolPreview& preview,
                          ApprovalCallback callback) override {
        callback(true, "");
    }
};

// -------------------- YOLO 模式 --------------------
class YoloModePolicy : public IExecutionPolicy {
public:
    YoloModePolicy() {
        config_.show_plan = false;
        config_.show_result_summary = false;
        config_.auto_decide_retry = true;
        config_.show_reflection = false;
        config_.fleet_max_concurrency = 32;
        config_.mode_name = "yolo";
    }

    bool requires_approval(const ToolMetadata& meta,
                           const ToolCallContext& ctx) const override {
        // YOLO 模式：只有 force_approval_always 的工具需要审批
        // 安全底线：delete_file, exec_shell(dangerous) 仍需确认
        return meta.approval.force_approval_always;
    }

    bool should_execute(const ToolMetadata& meta,
                        const ToolCallContext& ctx) const override {
        return true;
    }

    bool can_skip(const ToolMetadata& meta,
                  const ToolCallContext& ctx) const override {
        return false;
    }

    int get_layer(const ToolMetadata& meta) const override {
        return meta.approval.default_layer;
    }

    void request_approval(const ToolMetadata& meta,
                          const ToolCallContext& ctx,
                          const ToolPreview& preview,
                          ApprovalCallback callback) override {
        callback(true, "");
    }
};
```

### 3. 审批流程（同步回调模式，非协程）

**Oracle 决议** (session `ses_0faa4dabeffeHGFoLdXE7AqwH7`)：审批采用同步回调模式而非协程。`request_approval()` 接受 `ApprovalCallback`，UI 层持有该 callback 并在用户响应时调用。不引入 `async_simple` 协程依赖。

```cpp
// ===== src/common/policy/tool_executor.h =====

ToolResult execute_tool_with_policy(
    ToolRegistry& registry,
    ToolCall& call,
    IExecutionPolicy* policy,
    IInteractionBus& bus)
{
    // Step 1: 获取工具元数据
    auto* meta = registry.get_metadata(call.tool_name);
    if (!meta) {
        return ToolResult::error("ERR_TOOL.NOT_FOUND",
                                  "Tool not registered: " + call.tool_name);
    }

    // Step 2: Layer Profile 检查
    ToolCallContext ctx{
        .session_id = call.session_id,
        .caller_layer = call.caller_layer,
    };

    if (!check_layer_permission(*meta, ctx)) {
        return ToolResult::error("ERR_TOOL.PERMISSION_DENIED",
            "Layer " + ctx.caller_layer + " cannot call " + call.tool_name);
    }

    // Step 3: 是否应跳过错略（非阻塞）
    if (policy->can_skip(*meta, ctx)) {
        return ToolResult::skipped(call.tool_name);
    }

    // Step 4: 是否应执行？（false = 拒绝执行，不审批）
    if (!policy->should_execute(*meta, ctx)) {
        return ToolResult::rejected(call.tool_name, "Policy refused execution");
    }

    // Step 5: 审批检查（同步回调模式）
    if (policy->requires_approval(*meta, ctx)) {
        auto preview = generate_preview(call, *meta);
        bool approved = false;
        std::string reason;
        std::mutex mtx;
        std::condition_variable cv;

        policy->request_approval(*meta, ctx, preview,
            [&](bool ap, const std::string& r) {
                std::lock_guard lk(mtx);
                approved = ap;
                reason = r;
                cv.notify_one();
            });

        // 等待用户响应（同步等待，实际由 UI 层驱动）
        std::unique_lock lk(mtx);
        cv.wait(lk, [&] { return true; });  // callback 已调用

        if (!approved) {
            return ToolResult::rejected(call.tool_name, reason);
        }
    }

    // Step 6: 执行工具
    auto result = registry.execute(call.tool_name, call.params);

    // Step 7: 发布执行完成事件
    bus.emit(EventTypes::Tool::CallFinished, call.session_id, {
        {"tool_name", call.tool_name},
        {"success", result.ok},
        {"duration_ms", result.meta.duration_ms},
        {"category", to_string(meta->category)}
    });

    return result;
}
```

### 4. 预览生成（diff / 命令预览）

```cpp
// ===== src/common/policy/preview_generator.h =====

ToolPreview generate_preview(
    const ToolCall& call, const ToolMetadata& meta)
{
    ToolPreview preview;
    preview.tool_name = call.tool_name;

    switch (meta.category) {
        case ToolCategory::WriteFile: {
            auto current = read_file(call.params["path"]);
            auto proposed = apply_changes(current, call.params["changes"]);
            preview.diff = generate_unified_diff(current, proposed);
            preview.display_type = "diff";
            break;
        }
        case ToolCategory::Execute: {
            preview.command = call.params["command"];
            preview.display_type = "shell_command";
            preview.warning = classify_command_risk(preview.command);
            break;
        }
        default:
            preview.display_type = "json";
            preview.raw_params = call.params.dump(2);
    }

    return preview;
}
```

### 5. 权限检查的性能设计

**关键问题**：权限检查在锁内还是锁外？

```cpp
// 方案：两阶段检查

ToolResult ToolRegistry::call_tool(
    const std::string& name, 
    const nlohmann::json& params,
    const ToolCallContext& ctx) 
{
    // Phase 1: 锁内——快速查找工具和元数据
    ToolHandler handler;
    ToolMetadata meta;
    {
        std::shared_lock lock(mutex_);
        auto it = tools_.find(name);
        if (it == tools_.end()) return ToolResult::not_found(name);
        handler = it->second.handler;
        meta = it->second.metadata;
    }
    // 锁已释放
    
    // Phase 2: 锁外——权限检查（可能耗时）
    if (!check_layer_permission(meta, ctx)) {
        return ToolResult::permission_denied(name, ctx.caller_layer);
    }
    
    // Phase 3: 锁外——执行（可能很慢）
    return handler(params);
}
```

**性能保证**：
- 锁持有时间 < 1μs（仅哈希表查找）
- 权限检查 < 10μs（枚举比较）
- 不会出现长时间持锁阻塞其他线程

### 6. YOLO 切换确认（防御深度）

> **Oracle 决议** (session `ses_0faa4dabeffeHGFoLdXE7AqwH7`)：YOLO 模式是高安全敏感度模式。任何涉及 YOLO 的模式切换必须经用户确认对话框，防止误操作。

**决策**：Agent→YOLO / YOLO→Agent / Plan→YOLO / YOLO→Plan 模式切换必须经 `ModeSwitchDialog::confirm_yolo_switch()` 用户确认对话框。Plan↔Agent 切换可静默。

**理由**：防止误操作——用户在 Plan 模式时误触 `/mode yolo` 会立即获得全部 `force_approval_always` 豁免。

**实施位置**：`src/common/policy/mode_switch_dialog.{h,cpp}`（C3 tasks §6.4）。

### 6. 模式切换时的安全转换

**Oracle 决议 — 新增 YOLO 切换确认**：模式切换是高安全敏感度操作。Agent→YOLO / YOLO→Agent / Plan→YOLO / YOLO→Plan 模式切换必须经 `ModeSwitchDialog::confirm_yolo_switch()` 用户确认对话框。Plan↔Agent 切换可静默。实施在 `src/common/policy/mode_switch_dialog.{h,cpp}`。

```cpp
// 模式切换事件处理
void handle_mode_change(
    const std::string& session_id,
    const std::string& new_mode,
    const std::string& current_mode)
{
    // 判断是否需要 YOLO 确认
    if (new_mode == "yolo" || current_mode == "yolo") {
        // 任何涉及 YOLO 的切换都需要用户确认（防御深度）
        ModeSwitchDialog dialog;
        if (!dialog.confirm_yolo_switch(current_mode, new_mode)) {
            return;  // 用户取消切换
        }
    }
    // Plan↔Agent 切换可静默（无需确认）

    // 发布模式切换事件
    bus.emit(EventTypes::Session::ModeChanged, session_id, {
        {"from_mode", current_mode},
        {"to_mode", new_mode}
    });
}
```

---

## 实施计划

### Phase 1：核心接口

| # | 文件 | 操作 | 说明 |
|---|------|------|------|
| 1 | `src/common/policy/CMakeLists.txt` | 新建 | 静态库 `agenticdsl_policy` |
| 2 | `src/common/policy/execution_policy.h` | 新建 | `IExecutionPolicy` 接口 |
| 3 | `src/common/policy/execution_policy.cpp` | 新建 | 三种模式实现 |
| 4 | `src/common/policy/preview_generator.h` | 新建 | 预览生成器 |
| 5 | `src/common/policy/tool_executor.h` | 新建 | `execute_tool_with_policy()` |
| 6 | `tests/test_execution_policy.cpp` | 新建 | 单元测试 |
| 7 | `CMakeLists.txt`（根目录） | 修改 | 添加 `add_subdirectory(common/policy)` |

### Phase 2：集成

| # | 文件 | 操作 | 说明 |
|---|------|------|------|
| 8 | `src/common/tools/registry.h` | 修改 | 增加 `ToolMetadata` 支持 |
| 9 | `src/common/tools/registry.cpp` | 修改 | 保留旧 API 兼容 |
| 10 | `src/modules/executor/node_executor.h` | 修改 | 增加协程路径 |
| 11 | `src/modules/executor/node_executor.cpp` | 修改 | 实现 `execute_tool_call_async()` |
| 12 | `src/core/engine.h` | 修改 | 添加 `IExecutionPolicy` 成员 |

> **Related (2026-06-17)**: OpenSpec change `2026-06-15-residual-engine-h-decoupling` 完成 ADR-0019 §1.4 跨模块耦合解耦, 移除 `engine.h` 全部跨模块 include (保留 `common/llm/llm_types.h` types 头文件例外). 本 ADR Phase 2 line 379 添加 `IExecutionPolicy` 成员时, 建议采用 PIMPL-lite 模式 (`std::unique_ptr<IExecutionPolicy>`), 镜像 `budget_controller_` / `tool_registry_` 模式 (PIMPL-lite 在 2026-06-15-residual-engine-h-decoupling 完成). 避免重新引入 `engine.h` 跨模块 include.
>
> **Oracle 更新 (2026-07-31)**: IExecutionPolicy 当前为 5 方法接口, 新增 `ModeSwitchDialog` 组件 (`src/common/policy/mode_switch_dialog.{h,cpp}`) 处理 YOLO 切换确认。

### Phase 3：TUI 集成（依赖 ADR-0019）

| # | 文件 | 操作 | 说明 |
|---|------|------|------|
| 13 | `examples/agent_chat/src/tui.cpp` | 修改 | 显示审批对话框 |

---

## 验证标准

| 标准 | 验证方法 |
|------|---------|
| Plan 模式 | 写入工具调用触发审批请求 |
| Agent 模式 | 遵循工具自身 ApprovalPolicy |
| YOLO 模式 | 仅 force_approval_always 工具触发审批 |
| Layer 违规 | L3 调用 WriteFile 工具返回 PermissionDenied |
| 审批超时 | 同步 callback 模式，由 UI 层管理超时 |
| 性能 | 权限检查 < 10μs |
| should_execute 拒绝 | 策略返回 false 时工具不被执行且不触发审批 |
| can_skip 跳过 | 策略返回 true 时工具被跳过（非阻塞） |
| YOLO 切换确认 | YOLO 相关模式切换均触发 confirm_yolo_switch 对话框 |

---

## 风险与缓解

| 风险 | 严重度 | 缓解 |
|------|--------|------|
| API 破坏性变更 | 高 | 5 方法极致精简，ModeConfig 分离配置型方法 |
| 同步 callback 阻塞 UI | 中 | callback 被 UI 层持有，UI 事件循环调度；实际不阻塞线程 |
| 审批流程阻塞执行 | 中 | callback 模式释放调用线程，UI 层异步驱动 |
| 模式切换滥用 | 低 | YOLO 切换需要 `ModeSwitchDialog::confirm_yolo_switch()` 确认 |
| ModeConfig 漂移 | 中 | ModeConfig 在接口中提供 const 访问，构造后不可变 |
| YOLO 防御深度不足 | 低 | YOLO→非YOLO 切换同样需要确认，防误切回 |

---

## 参考

- [ADR-0004: ToolRegistry 安全模型](./adr-0004-toolregistry-security.md) — ToolCategory、ApprovalPolicy、LayerProfile
- [ADR-0002: EventBus 有界队列架构](./adr-0002-eventbus-bounded-queue.md) — 审批事件传输
- [ADR-0030: AsyncRuntime 双层异步架构](../archive/adr/adr-0030-async-runtime-dual-layer.md) — 协程挂起等待
- [ADR-0031 §2: 三种模式的策略实现](#2-三种模式的策略实现) — Plan/Agent/YOLO 模式定义
- [ADR-0033: Session Hierarchy 执行会话层级体系](./adr-0033-session-hierarchy.md) — TaskSession 持有当前策略，模式切换与会话生命周期绑定

---

*文档版本: v2.0*
*最后更新: 2026-07-31*

*Oracle 决议 (session `ses_0faa4dabeffeHGFoLdXE7AqwH7`): 5-method interface + sync callback + Agent default + YOLO confirm*

---

## 附录：议题 5 最小集成说明

**⚠️ SUPERSEDED (2026-07-31, Oracle session `ses_0faa4dabeffeHGFoLdXE7AqwH7`)**：此附录中的 3 个方法（`should_auto_decide_retry()`、`should_show_reflection()`、`fleet_max_concurrency()`）已随 8→5 方法精简迁移至 `ModeConfig` 值结构体，不再在 `IExecutionPolicy` 接口中定义。

### 背景
议题 5 提出了完整的 IPER-Policy 集成方案（8 个新方法 + 协程实现）。经评估，当前代码库：
- 无 IPER 循环架构（仅有 DAG 执行）
- 无协程基础设施（ADR-0030 未实现）
- DSLEngine 为 stateless 设计

### 决策
采用**最小集成**（Option B）：仅扩展 3 个方法，其余 defer 到 IPER 架构就绪后。

### 新增方法
| 方法 | 用途 | Plan | Agent | YOLO |
|------|------|------|-------|------|
| `should_auto_decide_retry()` | Reflect 后是否自动重试 | false | true | true |
| `should_show_reflection()` | 是否展示反思结果 | true | false | false |
| `fleet_max_concurrency()` | 舰队模式最大并行度 | 8 | 16 | 32 |

### 未纳入的方法（defer）
- `should_auto_continue()` — 需要 IPER 循环支持
- `should_show_step_results()` — 与 `should_show_result_summary()` 语义重叠
- `should_show_inference()` — 需要 IPER Infer 阶段
- `mode_description()` — 纯展示，非核心
- `fleet_requires_batch_approval()` — 舰队模式审批策略，P2 特性

### 后续工作
1. ADR-0015 V2 定义 IPER 循环架构
2. ADR-0030 实现 async_simple 协程基础设施
3.  revisit 完整 IPER-Policy 集成