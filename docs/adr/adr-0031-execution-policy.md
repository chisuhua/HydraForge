# ADR-0031: IExecutionPolicy 执行策略与三模式审批

## 状态

**🟡 Partial** (2026-05-27, 2026-06-12 状态对齐 — 仅头文件 stub)

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

### 1. IExecutionPolicy 接口（基座层定义）

**关键决策**：基座层（`src/common/`）定义接口 + 三种默认实现，认知层可以扩展但基座不依赖认知层。

```cpp
// ===== src/common/policy/execution_policy.h =====

// 工具调用的上下文信息（辅助决策）
struct ToolCallContext {
    std::string session_id;
    std::string caller_layer;       // "cognitive" / "thinking" / "workflow"
    std::string target_path;        // 目标文件/目录路径
    bool is_in_fleet_mode = false;  // 是否在舰队模式中
    size_t call_count_this_session = 0;  // 本 session 第几次调用
};

// 执行策略接口
class IExecutionPolicy {
public:
    virtual ~IExecutionPolicy() = default;
    
    // 核心决策：此工具调用是否需要用户审批？
    virtual bool requires_approval(const ToolMetadata& meta, 
                                   const ToolCallContext& ctx) const = 0;
    
    // Plan 模式：是否允许自动进入 Execute 阶段？
    virtual bool should_auto_execute() const = 0;
    
    // 是否展示完整计划给用户？
    virtual bool should_show_plan() const = 0;
    
    // 获取当前模式名称
    virtual std::string mode_name() const = 0;
    
    // 工具执行后是否显示结果摘要？
    virtual bool should_show_result_summary() const = 0;
    
    // ===== IPER 行为控制（议题 5 最小集成）=====
    
    // Reflect 后是否自动决定重试/结束？（false = 询问用户）
    virtual bool should_auto_decide_retry() const = 0;
    
    // 是否展示 Reflect 分析结果？
    virtual bool should_show_reflection() const = 0;
    
    // ===== 舰队模式行为 =====
    
    // 舰队模式最大并行度（0 = 禁用舰队模式）
    virtual size_t fleet_max_concurrency() const = 0;
};
```

### 2. 三种模式的策略实现

```cpp
// ===== src/common/policy/execution_policy.cpp =====

// -------------------- Plan 模式 --------------------
class PlanModePolicy : public IExecutionPolicy {
public:
    bool requires_approval(const ToolMetadata& meta, 
                           const ToolCallContext& ctx) const override {
        // Plan 模式：所有写入操作都需要审批
        return meta.category != ToolCategory::ReadOnly;
    }
    
    bool should_auto_execute() const override { return false; }
    bool should_show_plan() const override { return true; }
    std::string mode_name() const override { return "plan"; }
    bool should_show_result_summary() const override { return true; }
    
    // IPER 行为：Plan 模式不自动重试，展示反思，保守并行度
    bool should_auto_decide_retry() const override { return false; }
    bool should_show_reflection() const override { return true; }
    size_t fleet_max_concurrency() const override { return 8; }
};

// -------------------- Agent 模式 --------------------
class AgentModePolicy : public IExecutionPolicy {
public:
    bool requires_approval(const ToolMetadata& meta, 
                           const ToolCallContext& ctx) const override {
        // Agent 模式：遵循工具自身的审批策略
        return meta.approval.requires_approval_in_agent;
    }
    
    bool should_auto_execute() const override { return true; }
    bool should_show_plan() const override { return false; }
    std::string mode_name() const override { return "agent"; }
    bool should_show_result_summary() const override { return true; }
    
    // IPER 行为：Agent 模式自动重试，不展示反思，中等并行度
    bool should_auto_decide_retry() const override { return true; }
    bool should_show_reflection() const override { return false; }
    size_t fleet_max_concurrency() const override { return 16; }
};

// -------------------- YOLO 模式 --------------------
class YoloModePolicy : public IExecutionPolicy {
public:
    bool requires_approval(const ToolMetadata& meta, 
                           const ToolCallContext& ctx) const override {
        // YOLO 模式：只有 force_approval_always 的工具需要审批
        // 安全底线：delete_file, exec_shell(dangerous) 仍需确认
        return meta.approval.force_approval_always;
    }
    
    bool should_auto_execute() const override { return true; }
    bool should_show_plan() const override { return false; }
    std::string mode_name() const override { return "yolo"; }
    bool should_show_result_summary() const override { return false; }
    
    // IPER 行为：YOLO 模式自动重试，不展示反思，高并行度
    bool should_auto_decide_retry() const override { return true; }
    bool should_show_reflection() const override { return false; }
    size_t fleet_max_concurrency() const override { return 32; }
};
```

### 3. 审批流程（与 async_simple + EventBus 集成）

```cpp
// ===== src/common/policy/tool_executor.h =====

using Lazy = async_simple::coro::Lazy;

Lazy<ToolResult> execute_tool_with_policy(
    ToolRegistry& registry,
    ToolCall& call,
    IExecutionPolicy* policy,
    IEventBus& bus,
    AsyncRuntime& runtime) 
{
    // Step 1: 获取工具元数据
    auto* meta = registry.get_metadata(call.tool_name);
    if (!meta) {
        co_return ToolResult::error("ERR_TOOL.NOT_FOUND", 
                                     "Tool not registered: " + call.tool_name);
    }
    
    // Step 2: Layer Profile 检查
    ToolCallContext ctx{
        .session_id = call.session_id,
        .caller_layer = call.caller_layer,
    };
    
    if (!check_layer_permission(*meta, ctx)) {
        co_return ToolResult::error("ERR_TOOL.PERMISSION_DENIED",
            "Layer " + ctx.caller_layer + " cannot call " + call.tool_name);
    }
    
    // Step 3: 审批检查
    if (policy->requires_approval(*meta, ctx)) {
        // 3a. 生成预览
        auto preview = co_await generate_preview(call, *meta);
        
        // 3b. 发送审批请求事件
        bus.emit(EventTypes::Tool::ApprovalReq, call.session_id, {
            {"tool_name", call.tool_name},
            {"preview", preview.to_json()},
            {"category", to_string(meta->category)},
            {"request_id", call.request_id}
        });
        
        // 3c. 协程挂起，等待用户响应（不占线程）
        auto response = co_await wait_for_event(
            bus,
            EventTypes::Tool::ApprovalResp,
            [&](const Event& e) {
                return e.payload["request_id"] == call.request_id;
            },
            std::chrono::minutes(5)  // 超时
        );
        
        // 3d. 处理响应
        if (!response.has_value()) {
            co_return ToolResult::error("ERR_TOOL.APPROVAL_TIMEOUT",
                                         "User approval timed out");
        }
        if (!response->payload["approved"].get<bool>()) {
            co_return ToolResult::rejected(
                response->payload.value("reason", "User rejected"));
        }
    }
    
    // Step 4: 执行工具（在 Taskflow IO 线程池中）
    auto result = co_await runtime.await_future(
        runtime.io_executor().async([&]() {
            return registry.execute(call.tool_name, call.params);
        })
    );
    
    // Step 5: 发布执行完成事件
    bus.emit(EventTypes::Tool::CallFinished, call.session_id, {
        {"tool_name", call.tool_name},
        {"success", result.ok},
        {"duration_ms", result.meta.duration_ms},
        {"category", to_string(meta->category)}
    });
    
    co_return result;
}
```

### 4. 预览生成（diff / 命令预览）

```cpp
// ===== src/common/policy/preview_generator.h =====

Lazy<ToolPreview> generate_preview(
    const ToolCall& call, const ToolMetadata& meta) 
{
    ToolPreview preview;
    preview.tool_name = call.tool_name;
    
    switch (meta.category) {
        case ToolCategory::WriteFile: {
            // 生成 unified diff
            auto current = read_file(call.params["path"]);
            auto proposed = apply_changes(current, call.params["changes"]);
            preview.diff = generate_unified_diff(current, proposed);
            preview.display_type = "diff";
            break;
        }
        case ToolCategory::Execute: {
            // 显示即将执行的命令
            preview.command = call.params["command"];
            preview.display_type = "shell_command";
            preview.warning = classify_command_risk(preview.command);
            break;
        }
        default:
            preview.display_type = "json";
            preview.raw_params = call.params.dump(2);
    }
    
    co_return preview;
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

### 6. 模式切换时的安全转换

```cpp
// 模式切换事件处理
Lazy<void> handle_mode_change(
    const std::string& session_id,
    const std::string& new_mode,
    IEventBus& bus) 
{
    // 验证切换合法性
    if (new_mode == "yolo") {
        // YOLO 切换需要额外确认（安全警告）
        bus.emit(EventTypes::Tool::ApprovalReq, session_id, {
            {"tool_name", "__mode_switch__"},
            {"preview", {
                {"display_type", "warning"},
                {"message", "切换到 YOLO 模式将跳过所有非危险操作的审批确认。"
                            "delete_file 和 exec_shell 仍需确认。是否继续？"}
            }},
            {"request_id", generate_id()}
        });
        
        auto resp = co_await wait_for_approval(session_id);
        if (!resp || !resp->approved) co_return;
    }
    
    // 发布模式切换事件
    bus.emit(EventTypes::Session::ModeChanged, session_id, {
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
| 审批超时 | 5 分钟无响应返回 ApprovalTimeout |
| 性能 | 权限检查 < 10μs |
| IPER 自动重试 | Plan 模式询问用户；Agent/YOLO 自动决定 |
| Reflect 展示 | Plan 模式展示反思；Agent/YOLO 静默 |
| 舰队并行度 | Plan=8 / Agent=16 / YOLO=32 |

---

## 风险与缓解

| 风险 | 严重度 | 缓解 |
|------|--------|------|
| API 破坏性变更 | 高 | 保留旧 API 作为 deprecated 包装 |
| 同步/协程双路径维护 | 中 | Phase 1 使用 `sync_await()` 桥接，Phase 2 全面协程化 |
| 审批流程阻塞执行 | 中 | 协程挂起释放线程，不阻塞其他任务 |
| 模式切换滥用 | 低 | YOLO 切换需要额外确认 |
| IPER 依赖未实现 | 中 | 议题 5 最小集成：仅添加 3 个 Policy 方法，不引入 IPER 循环 |
| 舰队模式并行度失控 | 低 | `fleet_max_concurrency()` 上限 32，实际受 Taskflow 线程池限制 |

---

## 参考

- [ADR-0004: ToolRegistry 安全模型](./adr-0004-toolregistry-security.md) — ToolCategory、ApprovalPolicy、LayerProfile
- [ADR-0002: EventBus 有界队列架构](./adr-0002-eventbus-bounded-queue.md) — 审批事件传输
- [ADR-0030: AsyncRuntime 双层异步架构](../archive/adr/adr-0030-async-runtime-dual-layer.md) — 协程挂起等待
- [ADR-0031 §2: 三种模式的策略实现](#2-三种模式的策略实现) — Plan/Agent/YOLO 模式定义
- [ADR-0033: Session Hierarchy 执行会话层级体系](./adr-0033-session-hierarchy.md) — TaskSession 持有当前策略，模式切换与会话生命周期绑定

---

*文档版本: v1.1*
*最后更新: 2026-05-28*

---

## 附录：议题 5 最小集成说明

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