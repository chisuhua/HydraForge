# ADR-0036：三层服务协议与调用契约（混合内核架构）
> 📋 **Phase 8 规划: 混合内核架构** (规划于 2026-05/06, 2026-06-09 整理归档) — 见 `implementation-roadmap.md`
**状态**：❌ 未实施 (2026-05-28, 2026-06-09 标注废弃)

代码侧无 `HybridKernel` 类。详见 OpenSpec change `tech-debt-and-doc-cleanup`。
**日期**：2026-05-28
**领域**：基座 / 系统架构
**关联**：ADR-0019（IInteractionBus）、ADR-0020（线程模型）、ADR-0030（AsyncRuntime）、ADR-0031（IExecutionPolicy）、ADR-0033（Session 层级）、ADR-0034（ModelRouter）
**取代**：`docs/guides/multi-domain-agent-architecture.md` 中的"服务调用模式"部分（该文档保留为入门概览）

---

## 架构哲学

### 混合内核模型

HydraForge 采用**混合内核（Hybrid Kernel）架构**：

- **基座层** = 操作系统内核，提供最小化的系统调用接口
- **认知智能体层** = **Shell（bash）**，运行在内核态（共享地址空间），解析用户意图并编排执行
- **领域智能体层** = 用户态可执行程序（/usr/bin/*），通过 fork+exec 语义被按需调用

这一设计在 MVP 阶段最大化开发效率和运行时性能，同时保留向微内核演进的能力——所有跨层接口均为抽象（I-接口），传输层可在不修改业务逻辑的情况下从函数调用演进为消息传递。

> **注意**：本文档描述**目标架构**。部分组件尚处于设计阶段，具体实施进度见 `docs/implementation-roadmap.md`。

### Unix 进程模型类比

| 操作系统概念 | HydraForge 对应 | 说明 |
|:---|:---|:---|
| kernel | 基座层 | 提供 syscall、调度、内存管理 |
| init (PID 1) | SessionManager + DomainAgentManager | 系统启动、服务生命周期、保活 |
| **bash (shell)** | **CognitiveOrchestrator** | **解析用户输入、编排执行、等待结果、循环** |
| terminal (PTY) | IInteractionBus + TUI | 用户 IO 通道 |
| 可执行程序 (/usr/bin/*) | 领域智能体注册的工具 | 被调用的领域能力 |
| builtin 命令 (cd, export) | 普通工具调用 | 直接函数调用，无隔离开销 |
| pipe (\|) | DAG 边 / ToolResult 传递 | 进程间数据流 |
| & (后台执行) | Taskflow executor.async() | 并行提交 |
| shell 脚本 (.sh) | DSL 工作流 (.agent.md) | 预定义的命令序列 |
| $PATH | ToolRegistry 已注册工具列表 | 程序发现机制 |
| $? (exit code) | ToolResult.ok | 退出状态 |
| SIGINT | 用户中断信号 | 中断认知层执行 |
| 环境变量 | SessionContext / domain_state | 进程继承的上下文 |

> **关于 fork+exec 类比**：本 ADR 用 fork+exec 描述**调用语义**（隔离环境、数据传递、结果回传），而非实现策略。实际实现中：
> - **普通工具调用**（无副作用或顺序执行）→ **builtin 命令**，直接函数调用，共享 TaskSession 上下文
> - **舰队模式并行** → **fork+exec**，创建独立 SubtaskSession，Taskflow 并行执行
> - **未来沙箱隔离** → 可升级为真正的进程级 fork（接口不变，实现变化）

### 交互流程类比

```
Unix 模型：
  用户 → terminal → bash 解析 → fork+exec(程序) → wait → 显示结果 → 等待下一条命令

HydraForge 模型：
  用户 → TUI → CognitiveOrchestrator 解析意图 → 工具调用 → 结果 → 等待下一条消息
```

### 为什么认知智能体是 Shell 而非 init

| 特征 | init (PID 1) | bash (shell) | 认知智能体实际行为 |
|:---|:---|:---|:---|
| 解析用户输入 | ❌ | ✅ | ✅ 意图理解 |
| 编排命令执行 | ❌ | ✅ | ✅ IPER Plan + Execute |
| 管理服务生命周期 | ✅ | ❌ | ❌（SessionManager 负责） |
| 等待子进程结果 | ❌ | ✅ | ✅ 等待工具调用返回 |
| 支持脚本 | ❌ | ✅ | ✅ DSL 工作流 |
| 支持管道 | ❌ | ✅ | ✅ DAG 数据流 |
| 循环等待输入 | ❌ | ✅ (REPL) | ✅ IPER 循环 |
| 系统保活 | ✅ | ❌ | ❌ |

### 为什么是混合内核

- **非纯宏内核**：领域智能体通过接口隔离，可独立开发/测试/替换
- **非纯微内核**：认知层（shell）编译进基座（内核模块），避免 IPER 循环中高频 Session 访问的 IPC 开销
- **演进路径**：未来可将 ToolRegistry.call() 从函数调用变为异步消息传递，实现分布式/热替换

---

## 背景

`multi-domain-agent-architecture.md` 定义了"认知智能体编排、领域智能体执行"的概念模型，但存在以下缺口：

1. **ICognitiveOrchestrator 接口未定义**——基座如何启动认知层处理？
2. **工具调用链不完整**——IExecutionPolicy 审批在哪里介入？
3. **调用方向约束未明确**——谁可以调用谁？
4. **LLM 调用归属不清**——认知层独占还是领域层也可直接调用？
5. **调用语义不明确**——是函数调用还是进程隔离？

---

## 决策

### 一、三层角色定义

| 层级 | Unix 类比 | 核心职责 | 运行态 |
|:---|:---|:---|:---|
| **基座层** | kernel + init | 提供运行时、通信、存储、调度；管理生命周期 | 内核态 |
| **认知智能体层** | bash (shell) | 解析用户意图、编排执行、等待结果、IPER 循环 | 内核态（共享地址空间） |
| **领域智能体层** | /usr/bin/* (程序) | 提供特定领域的工具能力，被按需调用 | 用户态（接口隔离） |

### 二、核心接口定义

#### 2.1 基座 → 认知层：ICognitiveOrchestrator

```cpp
// include/agenticdsl/cognitive/icognitive_orchestrator.h
// 类比：kernel 启动 shell 进程

// 认知层可访问的服务——通过依赖注入实现真正的隔离
// 认知层无法获取基座层的全部服务引用
struct CognitiveServices {
    ToolRegistry& tools;              // $PATH + exec
    SessionManager& sessions;         // job control
    ModelRegistry& models;            // 设备信息
    IEventBus& events;                // 信号/syslog
    ParallelExecutor& parallel;       // 后台执行 (&)
    IExecutionPolicy* policy;         // sudoers 规则
    IModelRouter* router;             // 命令别名
};

class ICognitiveOrchestrator {
public:
    virtual ~ICognitiveOrchestrator() = default;
    
    // 主入口：等同于 shell 收到一行用户输入
    // 异步：因为 IPER 循环包含 LLM 调用（秒级）+ 用户审批（分钟级）
    // 调用方在独立线程执行，不阻塞基座事件循环
    // 前置条件：消息已追加到 UserSession.messages
    // 职责：驱动 IPER 循环直到任务完成或需要更多用户输入
    // on_complete 在认知层工作线程调用（非主线程/事件循环线程）
    // 基座层负责将结果安全地转发到 IInteractionBus
    virtual void process(const std::string& session_id,
                         std::function<void(ExecutionResult)> on_complete) = 0;
    
    // 用户中断（等同于 SIGINT）
    virtual void interrupt(const std::string& session_id) = 0;
    
    // 模式切换（等同于 shell 环境变量变更）
    virtual void on_mode_changed(const std::string& session_id,
                                  SessionMode new_mode) = 0;
};
```

#### 2.2 认知层 → 领域层：工具调用链

```cpp
// 完整调用链（类比：bash 解析并执行一条命令）：
//
//   CognitiveOrchestrator (shell 解析命令)
//     → IExecutionPolicy.requires_approval()     [权限检查，类似 sudo]
//     → [如需审批] EventBus → await 用户确认     [类似 sudo 密码输入]
//     → ToolRegistry.call("code::edit_file")     [builtin 或 fork+exec]
//       → 领域工具实现                            [程序 main() 执行]
//     → ToolResult                               [exit code + stdout]
//     → EventBus.emit("tool.call.finished")      [审计日志]
```

#### 2.3 领域层注册契约（延迟，见下方说明）

领域智能体的注册契约（原议题 7 / ADR-0035）**延迟到 ADR-0022（插件加载）和 ADR-0034 P1（ModelRegistry）实现后**。

当前占位：领域智能体通过 `ToolRegistry.register_tool()` 注册工具，生命周期管理和能力声明的 `IDomainAgent` 接口将在后续 ADR 中定义。

```
当前状态：
  ToolRegistry.register_tool("code::edit_file", handler)  // ✅ 已实现

延迟（ADR-0022 + ADR-0034 P1 就绪后）：
  IDomainAgent::on_session_start()       // 生命周期
  IDomainAgent::create_router()          // 路由注入
  IDomainAgent::task_types()             // 能力声明
```

### 三、调用语义

#### 3.1 调用策略

工具调用时的上下文隔离遵循以下策略：

| 场景 | 语义 | 实现方式 | Unix 类比 |
|:---|:---|:---|:---|
| 舰队模式并行子任务 | **fork+exec** | 每个子任务独立 SubtaskSession，Taskflow 并行 | `for ...; do cmd &; done; wait` |
| 普通写入工具 (edit_file) | **builtin** | 共享 TaskSession 上下文，直接函数调用 | shell 内置命令 (cd, export) |
| 只读工具 (grep, ls) | **builtin** | 共享 TaskSession 上下文，直接函数调用 | 同左 |

**决策**：MVP 阶段仅舰队模式创建独立 SubtaskSession（Option A），保持简单。内置命令和外部程序的类比仅用于传达概念，不强制实现策略对齐。

**演进保留**：未来如需完全隔离（如沙箱执行不信任的第三方插件），可升级为每次调用都创建 SubtaskSession。接口不变，仅内部实现从 builtin 变为 fork+exec。

#### 3.2 类比限制说明

```
fork+exec 类比有效范围：
  ✅ 调用者等待 → wait
  ✅ 数据流 → 管道
  ✅ 隔离环境 → SubtaskSession
  ✅ 退出状态 → ToolResult
  ❌ 实现方式 → 不是真的 fork() 系统调用
  ❌ 资源开销 → 不是进程级隔离
  ❌ 地址空间 → 共享地址空间（混合内核）
```

### 四、调用方向约束

| 调用方 → 被调用方 | 允许？ | 方式 | Unix 类比 |
|:---|:---:|:---|:---|
| 基座 → 认知层 | ✅ | `ICognitiveOrchestrator.process()` | kernel 唤醒 shell |
| 基座 → 领域层 | ✅ | `DomainAgentManager` 生命周期通知 / `PluginLoader` 注册 | init 启动服务 |
| 认知层 → 基座 | ✅ | ToolRegistry / EventBus / SessionManager | shell 调用 syscall |
| 认知层 → 领域层 | ✅ | `ToolRegistry.call("domain::tool")` | shell 执行命令 |
| 领域层 → 基座 | ✅ | EventBus.emit() | 程序调用 write() |
| 领域层 → 认知层 | **❌** | — | 程序不能控制 shell |
| 领域层 → 领域层 | ⚠️ | `ToolRegistry.call("other::tool")` | 允许但不推荐 |
| 认知层直接调 LLM | ✅ | `ModelRegistry.get_provider().generate()` | shell 内置命令 |
| 领域层直接调 LLM | **❌** | — | 普通程序无内置命令权限 |

**关键规则**：
- **LLM 推理调用权归属认知层**——如同只有 shell 能执行内置命令（cd, export 等）
- **ML 推理作为工具**（如 embedding 分类）可通过基座的 `IEmbeddingService` 满足，不经过 LLM 推理路径
- **跨域调用**（领域A → 领域B的工具）Phase 2 起应记录审计日志
- **审批 = sudo**——IExecutionPolicy 检查如同 sudo 机制

### 五、状态访问权限矩阵

| 状态 | 基座层 | 认知层 (shell) | 领域层 (程序) | Unix 类比 |
|:---|:---:|:---:|:---:|:---|
| UserSession.messages | R/W(append) | R | — | /var/log（仅追加） |
| UserSession.mode | R/W | R | R | 全局环境变量 |
| UserSession.budget | R/W | R | R | ulimit |
| TaskSession.working_context | R/W | R/W | — | shell 局部变量 |
| SubtaskSession.* | R/W | R/W | R(own) | 子进程私有内存 |
| EventBus | 全部 | 全部 | 发布/订阅 | syslog |
| ToolRegistry | 管理 | 调用 | 注册 | /usr/bin/ |
| ModelRegistry | 管理 | 查询+调用 | 查询 | /dev/（只读） |
| domain_state("code::*") | R/W | R | R/W(本域) | 程序配置 (~/.config) |

### 六、完整请求处理流程

```
用户输入 "重构这个函数"
    │
    ▼
┌── ① 基座层：接收与路由 ─────────────────────────────────────┐
│ IInteractionBus.on_user_message(session_id, text)            │
│   → UserSession.append_message({role:"user", content:text})  │
│   → BudgetController.check(session_id)                       │
│   → ICognitiveOrchestrator.process(session_id, callback)     │
│     (在独立线程执行，不阻塞事件循环)                           │
└──────────────────────────────────────────────────────────────┘
    │
    ▼
┌── ② 认知层：IPER 编排 ──────────────────────────────────────┐
│ process(session_id) {                                        │
│   task = SessionManager.start_task(session_id)               │
│                                                              │
│   loop (max 3 retries) {                                     │
│     // Infer（解析命令）                                      │
│     intent = LLM.generate(UserSession.messages)              │
│                                                              │
│     // Plan（展开别名、处理管道）                             │
│     plan = LLM.generate(intent → DAG)                        │
│     if (!policy->should_auto_execute())                      │
│       await user_confirmation                                │
│                                                              │
│     // Execute（builtin 或 fork+exec）                       │
│     for (tool_call in plan.steps) {                          │
│       result = call_tool_with_policy(tool_call)  ──→ ③      │
│     }                                                        │
│                                                              │
│     // Reflect（检查 $?）                                    │
│     if (success) break                                       │
│     if (policy->should_auto_decide_retry()) continue         │
│     else await user_decision                                 │
│   }                                                          │
│                                                              │
│   UserSession.append_message({role:"assistant", content})    │
│   IInteractionBus.push_response(session_id, response)        │
│ }                                                            │
└──────────────────────────────────────────────────────────────┘
    │
    ▼
┌── ③ 工具调用链 ─────────────────────────────────────────────┐
│ call_tool_with_policy(tool_call) {                            │
│   meta = ToolRegistry.get_metadata(tool_name)                │
│                                                              │
│   // 3a. 权限检查（类似 file permission + sudo）             │
│   if (!check_layer_permission(meta, caller_layer))           │
│     return ToolResult::permission_denied()                   │
│                                                              │
│   // 3b. 审批检查（类似 sudo 密码确认）                      │
│   if (policy->requires_approval(meta, ctx)) {                │
│     preview = generate_preview(tool_call)                    │
│     EventBus.emit("tool.approval.requested", preview)        │
│     response = await user_approval (timeout 5min)            │
│     if (!response.approved) return ToolResult::rejected()    │
│   }                                                          │
│                                                              │
│   // 3c. 执行（builtin，未来可升级为 fork+exec）            │
│   result = ToolRegistry.call(tool_name, params)              │
│                                                              │
│   // 3d. 审计                                               │
│   EventBus.emit("tool.call.finished", result.meta)           │
│                                                              │
│   return result                                              │
│ }                                                            │
└──────────────────────────────────────────────────────────────┘
    │
    ▼
┌── ④ 结果输出 ───────────────────────────────────────────────┐
│ UserSession.append_message({role:"assistant", content})       │
│ IInteractionBus.push_token(session_id, response) → TUI 显示  │
└──────────────────────────────────────────────────────────────┘
```

### 七、各层持有的服务引用

```cpp
// ===== 基座层（kernel + init）=====
struct InfrastructureServices {
    AsyncRuntime runtime;                     // 调度器
    InMemoryEventBus event_bus;               // IPC
    ToolRegistry tool_registry;               // 程序注册表 ($PATH)
    SessionManager session_manager;           // 进程管理
    ModelRegistry model_registry;             // 模型注册
    ParallelExecutor parallel_executor;       // 多核调度
    CostCollector cost_collector;             // 资源计量
    DomainAgentManager domain_manager;        // 服务管理

    std::unique_ptr<ICognitiveOrchestrator> cognitive;
};

// ===== 认知层（shell）可访问的服务 =====
// 通过 CognitiveServices（见 2.1 节）依赖注入传入
// 真正隔离——CognitiveOrchestrator 拿不到 InfrastructureServices 的引用

// ===== 领域层（程序）可访问的服务 =====
// 当前通过函数参数注入：
//   register_tools(ToolRegistry&)   → 工具注册
//   EventBus（可选注入）            → 事件发布
```

### 八、舰队模式 = Shell 并行执行

```
# HydraForge 舰队模式
┌─ 认知层 (shell) ─────────────────────────────────────────────┐
│ 1. 判断任务适合并行                                           │
│ 2. 拆分子任务 + 创建 SubtaskSession × N (fork × N)           │
│ 3. 选模型 (Router → Flash)                                    │
│ 4. 准备每个子任务的 prompt                                    │
└───────────────────────────────┬───────────────────────────────┘
                                │ 提交（& 后台执行）
                                ▼
┌─ 基座层 (kernel 调度) ───────────────────────────────────────┐
│ ParallelExecutor.execute_batch(tasks)                         │
│   → Taskflow executor.async() × N  (多核并行)                │
│   → 每个 task: models.get_provider(flash).generate(prompt)   │
│   → wait_for_all()  (wait)                                   │
└───────────────────────────────┬───────────────────────────────┘
                                │ 结果回流
                                ▼
┌─ 认知层 (shell 聚合) ────────────────────────────────────────┐
│ 5. aggregate(results, strategy)  (cat results/*)              │
│ 6. 写入 TaskSession → 进入 Reflect                            │
└───────────────────────────────────────────────────────────────┘
```

### 九、错误处理 = Shell 的错误语义

| 错误源 | 处理方 | Unix 类比 |
|:---|:---|:---|
| 工具执行失败 | 认知层 Reflect 决定重试/放弃 | `$?` 非零 → shell 决定下一步 |
| LLM 调用失败 | 认知层内部重试 max 3 次 | 命令超时重试 |
| 预算超限 | 基座拒绝 → 通知用户 | ulimit 超限 → SIGXCPU |
| 审批被拒 | 返回 rejected → 认知层换方案 | sudo 密码错误 |
| Session 切割 | 基座自动归档 | 日志轮转 (logrotate) |
| 用户中断 | `interrupt()` → 清理 | Ctrl+C → SIGINT |

### 十、系统调用接口清单

基座向上层暴露的完整"syscall table"：

| # | 接口 | 类型 | 消费者 | Unix 类比 | 稳定性 |
|:---|:---|:---|:---|:---|:---|
| 1 | `ToolRegistry` | 基座服务 | 认知层调用/领域层注册 | exec + $PATH | ✅ MVP |
| 2 | `IEventBus` | 基座服务 | 所有层 | syslog | ✅ MVP |
| 3 | `SessionManager` | 基座服务 | 认知层 | fork / wait / kill | ✅ MVP |
| 4 | `ModelRegistry` | 基座服务 | 认知层调用/领域层查询 | /dev/* | ✅ MVP |
| 5 | `ParallelExecutor` | 基座服务 | 认知层 | 多核调度 | ✅ MVP |
| 6 | `ICognitiveOrchestrator` | 认知层实现 | 基座调用 | shell 接口 | ✅ MVP |
| 7 | `IExecutionPolicy` | 认知层实现 | 基座查询 | sudoers 规则 | ✅ MVP |
| 8 | `IModelRouter` | 领域层可选实现 | 认知层使用 | 程序 capabilities | 🔧 SDK v1 |
| 9 | `IDomainAgent` + `SessionContext` | 领域层实现 | 基座调用 | 程序入口 (main) | 🔧 延迟 |
| 10 | `IEmbeddingService` | 基座服务 | 领域层使用 | 硬件驱动 | 💡 未来 |

> **稳定性说明**：
> - `✅ MVP` = 当前可实现，语义稳定但接口可能扩展
> - `🔧 SDK v1` = 在 Plugin SDK 发布时承诺 ABI 稳定
> - `🔧 延迟` = 依赖其他 ADR 实现，暂不定义
> - `💡 未来` = 预留概念位置，无具体设计

> **当前实现状态速查**（帮助新开发者判断接口可用性）：
> - `ToolRegistry`：✅ 已实现（`src/common/tools/registry.h`）
> - `IEventBus`：❌ 仅设计（`docs/adr/adr-0002-eventbus-bounded-queue.md`）
> - `SessionManager`：❌ 仅设计（`docs/adr/adr-0033-session-hierarchy.md`）
> - `ModelRegistry`：❌ 仅设计（`docs/adr/adr-0034-model-router.md`）
> - `ICognitiveOrchestrator`：❌ 仅设计（本文档）
> - `IExecutionPolicy`：❌ 仅设计（`docs/adr/adr-0031-execution-policy.md`）
> - `IModelRouter`：❌ 仅设计（`docs/adr/adr-0034-model-router.md`）
> - `ParallelExecutor`：❌ 依赖 ADR-0030 实现
> - 其余接口：❌ 尚未进入设计阶段

---

## 待讨论：开放设计问题

以下问题在未来实施中需进一步明确：

### 问题 1：领域层是否永远不能调 LLM？

当前决策：禁止 LLM **推理**调用。ML **工具性**调用（如 embedding）通过 `IEmbeddingService` 基座服务满足。

潜在场景：领域层需要本地小模型做分类（不经过认知层编排）。可能需要定义 L0/L1 本地模型路径。

### 问题 2：多认知智能体（多 Shell）

当前决策：单例 CognitiveOrchestrator，通过 session_id 区分。

未来场景：不同用户/不同任务需要不同的认知策略（如一个用 IPER，一个用 ReAct）。

可能方案：每个 UserSession 关联一个 cognitive profile，选择不同的 Orchestrator 实现（类似 `chsh` 为用户设置默认 shell，或以 shebang 方式为特定任务指定 Orchestrator —— 如 `#!/cognitive/react`）

### 问题 3：跨域调用的治理

当前决策：允许通过 ToolRegistry 跨域调用，不推荐。

Phase 2 计划：增加跨域调用审计日志（EventBus 记录每次跨域调用）。
Phase 3 可选：`restrict_cross_domain_calls = true` 配置，限制领域层只能调同域工具。

---

## 后果

### 优点
- 三层协作有明确规范和直觉类比
- 防止领域层越权（如自行调 LLM 导致成本失控）
- 调用语义的灵活区分（builtin vs fork+exec）避免过度工程
- ICognitiveOrchestrator 回调签名适配异步本质
- 接口稳定性分阶段承诺，避免过早 ABI 锁定

### 缺点
- 领域层不能直接调 LLM 推理限制了灵活性
- Unix 类比不能过度延伸（已通过"类比限制说明"缓解）
- IDomainAgent 延迟引入意味着当前无法声明生命周期和能力

### 缓解措施
- embedding 需求通过基座 `IEmbeddingService` 满足
- 类比限制明确文档化
- IDomainAgent 延迟到 ADR-0022 + ADR-0034 P1 就绪后补充

---

## 相关文档

| 文档 | 关系 |
|:---|:---|
| `docs/guides/multi-domain-agent-architecture.md` | 保留为入门概览，本 ADR 为正式规范 |
| ADR-0019 (IInteractionBus) | Terminal/PTY 层协议 |
| ADR-0020 (线程模型) | 线程边界定义 |
| ADR-0030 (AsyncRuntime) | Taskflow + async_simple 双层执行引擎 |
| ADR-0031 (IExecutionPolicy) | sudo/umask 策略接口 |
| ADR-0033 (Session 层级) | SubtaskSession 隔离执行模型 |
| ADR-0034 (ModelRouter) | 模型路由 = 设备驱动选择 |
| `docs/implementation-roadmap.md` | 实施进度追踪，含 IDomainAgent 延迟状态 |

---

*文档版本: v1.1*
*最后更新: 2026-05-28*
