# ADR-0036：三层服务协议与调用契约

## 状态

**🔍 Proposed** (2026-05-28, 当前未实施 — 描述 hybrid kernel + 三层服务架构, 与 ADR-0036-hybrid-kernel-architecture 旧 archive 同源, 等待 Phase 5 自举阶段决定采纳策略)

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

### Unix 进程模型类比

| 操作系统概念 | HydraForge 对应 | 说明 |
|:---|:---|:---|
| kernel | 基座层 | 提供 syscall、调度、内存管理 |
| init (PID 1) | SessionManager + DomainAgentManager | 系统启动、服务生命周期、保活 |
| **bash (shell)** | **CognitiveOrchestrator** | **解析用户输入、编排执行、等待结果、循环** |
| terminal (PTY) | IInteractionBus + TUI | 用户 IO 通道 |
| 可执行程序 (/usr/bin/*) | IDomainAgent 注册的工具 | 被 fork+exec 调用的领域能力 |
| fork() | 创建 SubtaskSession（复制上下文） | 隔离的执行环境 |
| exec() | ToolRegistry.call() | 加载并运行领域逻辑 |
| pipe (\|) | DAG 边 / ToolResult 传递 | 进程间数据流 |
| wait/waitpid | IPER Execute 等待工具完成 | 等待子进程返回 |
| & (后台执行) | Taskflow executor.async() | 并行提交 |
| shell 脚本 (.sh) | DSL 工作流 (.agent.md) | 预定义的命令序列 |
| $PATH | ToolRegistry 已注册工具列表 | 程序发现机制 |
| $? (exit code) | ToolResult.ok | 退出状态 |
| SIGINT | ICognitiveOrchestrator.interrupt() | 用户中断信号 |
| 环境变量 | SessionContext / domain_state | 进程继承的上下文 |

### 交互流程类比

```
Unix 模型：
  用户 → terminal → bash 解析 → fork+exec(程序) → wait → 显示结果 → 等待下一条命令

HydraForge 模型：
  用户 → TUI → CognitiveOrchestrator 解析意图 → fork(SubtaskSession)+exec(工具) → wait → 显示结果 → 等待下一条消息
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

- **非纯宏内核**：领域智能体通过 IDomainAgent 接口隔离，可独立开发/测试/替换
- **非纯微内核**：认知层（shell）编译进基座（内模块），避免 IPER 循环中高频 Session 访问的 IPC 开销
- **演进路径**：未来可将 ToolRegistry.call() 从函数调用变为异步消息传递，实现分布式/热替换

---

## 背景

`multi-domain-agent-architecture.md` 定义了"认知智能体编排、领域智能体执行"的概念模型，但存在以下缺口：

1. **ICognitiveOrchestrator 接口未定义**——基座如何启动认知层处理？
2. **工具调用链不完整**——IExecutionPolicy 审批在哪里介入？
3. **调用方向约束未明确**——谁可以调用谁？
4. **LLM 调用归属不清**——认知层独占还是领域层也可直接调用？
5. **fork/exec 语义未明确**——工具调用时的上下文隔离策略

---

## 决策

### 一、三层角色定义

| 层级 | Unix 类比 | 心职责 | 运行态 |
|:---|:---|:---|:---|
| **基座层** | kernel + init | 提供运行时、通信、存储、调度；管理生命周期 | 内核态 |
| **认知智能体层** | bash (shell) | 解析用户意图、编排执行、等待结果、IPER 循环 | 内核态（共享地址空间） |
| **领域智能体层** | /usr/bin/* (程序) | 提供特定领域的工具能力，被按需调用 | 用户态（接口隔离） |

### 二、核心接口定义

#### 2.1 基座 → 认知层：ICognitiveOrchestrator

```cpp
// src/cognitive/icognitive_orchestrator.h
// 类比：kernel 启动 shell 进程
class ICognitiveOrchestrator {
public:
    virtual ~ICognitiveOrchestrator() = default;
    
    // 主入口：等同于 shell 收到一行用户输入
    // 前置条件：消息已追加到 UserSession.messages
    // 职责：驱动 IPER 循环直到任务完成或需要更多用户输入
    virtual void process(const std::string& session_id) = 0;
    
    // 用户中断（等同于 SIGINT）
    virtual void interrupt(const std::string& session_id) = 0;
    
    // 模式切换（等同于 shell 环境变量变更）
    virtual void on_mode_changed(const std::string& session_id, 
                                  SessionMode new_mode) = 0;
};
```

#### 2.2 认知层 → 领域层：fork + exec 语义

```cpp
// 认知层不直接持有领域智能体引用
// 通过 ToolRegistry 按名调用 = shell 通过 $PATH 找到并 exec 程序
//
// 完整调用链（类比：bash 解析并执行一条命令）：
//
//   CognitiveOrchestrator (shell 解析命令)
//     → IExecutionPolicy.requires_approval()     [权限检查，类似 sudo]
//     → [如需审批] EventBus → await 用户确认     [类似 sudo 密码输入]
//     → ToolRegistry.call("code::edit_file")     [fork + exec]
//       → IDomainAgent 注册的工具实现            [程序 main() 执行]
//     → ToolResult                               [exit code + stdout]
//     → EventBus.emit("tool.call.finished")      [审计日志]
```

#### 2.3 领域层 → 基座：IDomainAgent

```cpp
// src/common/plugin/idomain_agent.h
// 类比：程序安装到系统 (/usr/bin/ + man page)
class IDomainAgent {
public:
    virtual ~IDomainAgent() = default;
    
    // 身份（类似程序名和描述）
    virtual std::string domain_prefix() const = 0;   // "code"
    virtual std::string display_name() const = 0;    // "编程助手"
    
    // 注册工具（类似 make install）
    virtual void register_tools(ToolRegistry& registry) = 0;
    
    // 生命周期钩子（类似 systemd service 的 ExecStart/ExecStop）
    virtual void on_session_start(SessionContext& ctx) {}
    virtual void on_session_end(SessionContext& ctx) {}
    virtual void on_mode_change(SessionMode mode, SessionContext& ctx) {}
    
    // 能力声明（类似 --help / man page）
    virtual std::vector<std::string> task_types() const { return {}; }
    virtual bool supports_fleet() const { return false; }
    
    // 模型路由策略（类似程序声明自己需要的运行环境）
    virtual IModelRouter* create_router() { return nullptr; }
};
```

### 三、fork/exec 执行语义

#### 3.1 上下文隔离策略

工具调用时的上下文隔离遵循以下规则：

| 场景 | fork 行为 | 理由 |
|:---|:---|:---|
| 舰队模式并行子任务 | **强制 fork**：每个子任务独立 SubtaskSession | 并行执行必须隔离，防止数据竞争 |
| 普通写入工具 (edit_file) | **不 fork**：共享 TaskSession 上下文 | 单次顺序调用无竞争风险，减少开销 |
| 只读工具 (grep, ls) | **不 fork**：共享 TaskSession 上下文 | 无副作用，无需隔离 |

**决策**：MVP 阶段仅舰队模式 fork SubtaskSession（Option A），保持简单。普通工具调用在 TaskSession 上下文中直接执行。

**演进保留**：未来如需完全隔离（如沙箱执行不信任的第三方插件），可升级为 Option B（每次调用都 fork）。接口不变，仅内部实现变化。

#### 3.2 类比细节

```
Unix fork+exec 流程：
  pid = fork()         // 创建子进程，复制父进程地址空间
  if (pid == 0) {
    exec("/usr/bin/ls") // 子进程中加载新程序
  }
  wait(&status)        // 父进程等待子进程完成

HydraForge 工具调用流程（舰队模式）：
  subtask = task.create_subtask(input_slice)  // fork：创建 SubtaskSession
  result = registry.call(tool_name, params)    // exec：加载并运行工具
  // wait 由 Taskflow executor.wait_for_all() 实现

HydraForge 工具调用流程（普通模式）：
  result = registry.call(tool_name, params)    // 直接执行（无 fork，类似 builtin 命令）
```

### 四、调用方向约束

| 调用方 → 被调用方 | 允许？ | 方式 | Unix 类比 |
|:---|:---:|:---|:---|
| 基座 → 认知层 | ✅ | `ICognitiveOrchestrator.process()` | kernel 唤醒 shell |
| 认知层 → 基座 | ✅ | ToolRegistry / EventBus / SessionManager | shell 调用 syscall |
| 认知层 → 领域层 | ✅ | `ToolRegistry.call("domain::tool")` | shell fork+exec 程序 |
| 领域层 → 基座 | ✅ | EventBus.emit() / SessionContext | 程序调用 write()/read() |
| 领域层 → 认知层 | **❌** | — | 程序不能控制 shell |
| 领域层 → 领域层 | ⚠️ | `ToolRegistry.call("other::tool")` | 程序调用其他程序（允许但不推荐） |
| 认知层直接调 LLM | ✅ | `ModelRegistry.get_provider().generate()` | shell 内置命令 |
| 领域层直接调 LLM | **❌** | — | 普通程序无 shell 内置权限 |

**关键规则**：
- **LLM 调用权归属认知层**——如同只有 shell 能执行内置命令（cd, export 等），LLM 推理是认知层的"内置能力"
- **工具调用经过基座路由**——如同程序通过 syscall 访问硬件，而非直接操作
- **审批 = sudo**——IExecutionPolicy 检查如同 sudo 机制，对认知层透明

### 五、状态访问权限矩阵

| 状态 | 基座层 | 认知层 (shell) | 领域层 (程序) | Unix 类比 |
|:---|:---:|:---:|:---:|:---|
| UserSession.messages | R/W(append) | R | — | /var/log（仅追加） |
| UserSession.mode | R/W | R | R | 全局环境变量 |
| UserSession.budget | R/W | R | R | ulimit |
| TaskSession.working_context | R/W | R/W | — | shell 局部变量 |
| TaskSession.plan | R/W | R/W | — | 当前脚本内容 |
| SubtaskSession.* | R/W | R/W | R(own) | 子进程私有内存 |
| domain_state("code::*") | R/W | R | R/W(本域) | 程序配置文件 |
| EventBus | 全部 | 全部 | 发布/订阅 | syslog / dbus |
| ToolRegistry | 管理 | 调用 | 注册 | /usr/bin/ 目录 |
| ModelRegistry | 管理 | 查询+调用 | 查询 | 硬件设备（仅 kernel 可驱动） |

### 六、完整请求处理流程

```
用户输入 "重构这个函数"
    │
    ▼
┌── ① 基座层：接收与路由（类比：terminal → kernel → 唤醒 shell）──┐
│ IInteractionBus.on_user_message(session_id, text)               │
│   → UserSession.append_message({role:"user", content:text})     │
│   → BudgetController.check(session_id)                          │
│   → ICognitiveOrchestrator.process(session_id)                  │
└─────────────────────────────────────────────────────────────────┘
    │
    ▼
┌── ② 认知层：IPER 编排（类比：bash 解析并执行命令）──────────────┐
│ process(session_id) {                                           │
│   // 类似 bash 的 read-eval-print-loop                          │
│   task = SessionManager.start_task(session_id)                  │
│                                                                 │
│   loop (max 3 retries) {                                        │
│     // Infer（解析命令）                                         │
│     intent = LLM.generate(UserSession.messages)                 │
│                                                                 │
│     // Plan（展开别名、处理管道）                                │
│     plan = LLM.generate(intent → DAG)                           │
│     if (!policy->should_auto_execute())                          │
│       await user_confirmation  // Plan 模式：如同 -n (dry-run)  │
│                                                                 │
│     // Execute（fork+exec 每个命令）                             │
│     for (tool_call in plan.steps) {                             │
│       result = call_tool_with_policy(tool_call)  ──→ ③          │
│     }                                                           │
│                                                                 │
│     // Reflect（检查 $?）                                        │
│     if (success) break                                          │
│     if (policy->should_auto_decide_retry()) continue            │
│     else await user_decision                                    │
│   }                                                             │
│                                                                 │
│   UserSession.append_message({role:"assistant", content})       │
│   IInteractionBus.push_response(session_id, response)           │
│ }                                                               │
└─────────────────────────────────────────────────────────────────┘
    │
    ▼
┌── ③ 工具调用链（类比：fork+exec+wait）────────────────────────┐
│ call_tool_with_policy(tool_call) {                               │
│   meta = ToolRegistry.get_metadata(tool_name)  // 查找 $PATH    │
│                                                                 │
│   // 3a. 权限检查（类似 file permission + sudo）                 │
│   if (!check_layer_permission(meta, caller_layer))              │
│     return ToolResult::permission_denied()                      │
│                                                                 │
│   // 3b. 审批检查（类似 sudo 密码确认）                          │
│   if (policy->requires_approval(meta, ctx)) {                   │
│     preview = generate_preview(tool_call)                       │
│     EventBus.emit("tool.approval.requested", preview)           │
│     response = await user_approval (timeout 5min)               │
│     if (!response.approved) return ToolResult::rejected()       │
│   }                                                             │
│                                                                 │
│   // 3c. 执行（fork+exec）                                      │
│   result = ToolRegistry.call(tool_name, params)                 │
│                                                                 │
│   // 3d. 审计（类似 auditd 记录）                                │
│   EventBus.emit("tool.call.finished", result.meta)              │
│                                                                 │
│   return result  // $? + stdout                                 │
│ }                                                               │
└─────────────────────────────────────────────────────────────────┘
    │
    ▼
┌── ④ 结果输出（类似：write(STDOUT) → terminal 显示）────────────┐
│ UserSession.append_message({role:"assistant", content})          │
│ IInteractionBus.push_token(session_id, response) → TUI 显示     │
└─────────────────────────────────────────────────────────────────┘
```

### 七、各层持有的服务引用

```cpp
// ===== 基座层（kernel + init）=====
struct InfrastructureServices {
    // kernel 提供的核心服务
    AsyncRuntime runtime;                     // 调度器 (scheduler)
    InMemoryEventBus event_bus;               // IPC 机制 (dbus/syslog)
    ToolRegistry tool_registry;               // 程序注册表 ($PATH)
    SessionManager session_manager;           // 进程管理 (init/systemd)
    ModelRegistry model_registry;             // 设备驱动 (仅 kernel 可操作)
    ParallelExecutor parallel_executor;       // 多核调度
    CostCollector cost_collector;             // 资源计量 (cgroups)
    DomainAgentManager domain_manager;        // 服务管理 (systemd units)
    
    // shell 实例（基座持有，类似 getty 启动 bash）
    std::unique_ptr<ICognitiveOrchestrator> cognitive;
};

// ===== 认知层（shell）可访问的服务 =====
struct CognitiveServices {
    ToolRegistry& tools;              // $PATH + exec
    SessionManager& sessions;         // job control
    ModelRegistry& models;            // 内置命令 (builtin)
    IEventBus& events;                // 信号处理
    ParallelExecutor& parallel;       // 后台执行 (&)
    IExecutionPolicy* policy;         // umask / sudo 规则
    IModelRouter* router;             // 命令别名 (alias)
};

// ===== 领域层（程序）可访问的服务 =====
// 通过 IDomainAgent 接口方法的参数间接获取：
//   register_tools(ToolRegistry&)          → make install
//   on_session_start(SessionContext&)      → 程序初始化 (constructor)
//   create_router()                        → 声明运行环境需求
// 可选直接注入：
//   IEventBus&                             → syslog 写入
```

### 八、舰队模式 = Shell 并行执行

```bash
# Unix 并行模式
for file in *.cpp; do
    review "$file" &    # 后台并行
done
wait                    # 等待全部完成
cat /tmp/results/*      # 聚合结果
```

```
# HydraForge 舰队模式
┌─ 认知层 (shell) ─────────────────────────────────────────────┐
│ 1. 判断任务适合并行 (如同 shell 识别循环+&)                    │
│ 2. 拆分子任务 + 创建 SubtaskSession × N (fork × N)           │
│ 3. 选模型 (Router → Flash)                                    │
│ 4. 准备每个子任务的 prompt                                    │
└───────────────────────────────┬───────────────────────────────┘
                                │ 提交 (& 后台执行)
                                ▼
┌─ 基座层 (kernel 调度) ────────────────────────────────────────┐
│ ParallelExecutor.execute_batch(tasks)                         │
│   → Taskflow executor.async() × N  (多核并行)                │
│   → 每个 task: ModelRegistry.get_provider(flash).generate    │
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
| 1 | `IDomainAgent` | 领域层实现 | 基座调用 | 程序入口 (main) | 稳定 |
| 2 | `ToolRegistry` | 基座服务 | 认知层调用/领域层注册 | exec + $PATH | 稳定 |
| 3 | `IEventBus` | 基座服务 | 所有层 | write(fd) / poll(fd) | 稳定 |
| 4 | `SessionManager` | 基座服务 | 认知层 | fork / wait / kill | 稳定 |
| 5 | `ModelRegistry` | 基座服务 | 认知层调用/领域层查询 | 设备文件 /dev/* | 稳定 |
| 6 | `ParallelExecutor` | 基座服务 | 认知层 | clone(CLONE_VM) | 稳定 |
| 7 | `ICognitiveOrchestrator` | 认知层实现 | 基座调用 | shell 接口 | 稳定 |
| 8 | `IExecutionPolicy` | 认知层实现 | 基座查询 | sudoers 规则 | 稳定 |
| 9 | `IModelRouter` | 领域层可选实现 | 认知层使用 | 程序 capabilities | 可扩展 |
| 10 | `SessionContext` | 基座提供 | 领域层使用 | environ / argv | 稳定 |

**稳定性原则**：标记为"稳定"的接口一旦发布，只允许增加方法（带默认实现），不允许删除或修改已有方法签名。如同 Linux syscall table 只增不减。

---

## 待讨论：开放设计问题

以下问题在未来实施中需进一步明确：

### 问题 1：fork/exec 粒度

当前决策（Option A）：仅舰队模式 fork SubtaskSession。普通工具调用共享 TaskSession。

未来可能的演进：
- **Option B（完全隔离）**：每次 ToolRegistry.call() 都创建独立 SubtaskSession，类似真正的 fork+exec。优点是完全隔离，缺点是开销大。
- **Option C（写入隔离）**：仅写入类工具（edit_file, delete_file）fork，只读工具直接执行。类似 Copy-on-Write fork。

触发升级条件：当引入不信任的第三方插件，或需要沙箱隔离时。

### 问题 2：领域层是否永远不能调 LLM？

当前决策：禁止。LLM 调用权完全归属认知层。

潜在场景：领域层需要本地 embedding（如代码向量化），这不经过认知层编排。

可能方案：区分"推理调用"（禁止）和"工具性调用"（允许），定义 `EmbeddingService` 作为基座服务而非 LLM 推理。

### 问题 3：多认知智能体（多 Shell）

当前决策：单例 CognitiveOrchestrator，通过 session_id 区分。

未来场景：不同用户/不同任务需要不同的认知策略（如一个用 IPER，一个用 ReAct）。

可能方案：每个 UserSession 关联一个 cognitive profile，选择不同的 Orchestrator 实现（类似用户登录时选择 bash/zsh/fish）。

### 问题 4：领域层跨域调用的治理

当前决策：允许通过 ToolRegistry 跨域调用，但不推荐。

风险：如果 code:: 频繁调用 fs:: 工具，形成隐式耦合，绕过认知层编排。

可能方案：增加跨域调用审计日志；或限制领域层只能调用同域工具（需认知层中转跨域）。

---

## 后果

### 优点
- 三层协作有明确规范和直觉类比，新开发者可用 Unix 经验快速理解架构
- 防止领域层越权（如自行调 LLM 导致成本失控）
- fork/exec 语义为未来沙箱隔离预留空间
- 接口稳定性承诺（syscall 只增不减）保障领域插件长期兼容

### 缺点
- 领域层不能直接调 LLM 限制了灵活性
- Unix 类比可能被过度延伸（不是所有方面都完美对应）
- 认知层作为"shell"的 bug 可能影响全局（与 kernel 同生命周期）

### 缓解措施
- embedding 需求通过基座 EmbeddingService 满足（非 LLM 推理）
- 类比仅作为心智模型，不强制实现细节完全对齐
- 认知层稳定性通过充分测试保障

---

## 相关文档

| 文档 | 关系 |
|:---|:---|
| `docs/guides/multi-domain-agent-architecture.md` | 保留为入门概览，本 ADR 为正式规范 |
| ADR-0019 (IInteractionBus) | Terminal/PTY 层协议 |
| ADR-0020 (线程模型) | 线程边界定义 |
| ADR-0030 (AsyncRuntime) | Taskflow + async_simple 双层执行引擎 |
| ADR-0031 (IExecutionPolicy) | sudo/umask 策略接口 |
| ADR-0033 (Session 层级) | fork 创建的执行上下文模型 |
| ADR-0034 (ModelRouter) | 模型路由 = 设备驱动选择 |

