# ADR-0055: SKILL.md 执行与隔离模型

## 状态

✅ Approved (2026-07-16, 架构评审确认)
✅ 实施完成 (2026-07-22, skill-interpreter-real-loading change ship, 18/18 tests pass)

## 领域

Agent-as-Plugin 架构 / SKILL 运行时

## 关联

- [ADR-0021 — PDK Design](../adr-0021-pdk-design.md) — SafeExec 沙箱封装 (§3.3, **注**: SafeExec 当前 MVP 仅 `std::async + wait_for`, fork/seccomp 为 Phase 2/3, 本 ADR 不依赖其实现)
- [ADR-0004 — ToolRegistry Security](../adr-0004-toolregistry-security.md) — 权限校验
- [ADR-0031 — Execution Policy](../adr-0031-execution-policy.md) — 审批策略
- [ADR-0053 — AgentDescriptor 与 pdk_register_agent](./adr-0053-agent-descriptor-interface.md) — `requires_isolation` 字段
- [ADR-0052 — Agent Plugin Manifest](./adr-0052-agent-plugin-manifest.md) — `trust_level` 与 `requires_isolation`

## 实施修订记录 (2026-07-21)

> 来源: OpenSpec change `skill-interpreter-real-loading` 设计审查, ADR 修订走正式流程

### 修订 1 — 隔离技术: fork() → posix_spawn() + execve()

**原决策**: `fork()` + `seccomp(BPF)` + `setrlimit()`

**修订后**: **`posix_spawn()` + `execve(/proc/self/exe, "--skill-child")` + `seccomp(BPF)` + `setrlimit()`**

**理由**: 父进程 `DSLEngine` 持有 `InMemoryBus` dispatch_thread + 可能持有 `DomainWorkerPool`/`CognitiveWorker`(`std::jthread`), 属于多线程进程. 直接 `fork()` 会继承 mutex 状态, 子进程调用任何非 async-signal-safe 函数(`std::string`/`std::vector`/`nlohmann::json`/`malloc`)立即死锁. `posix_spawn()` 是 POSIX 为多线程程序设计的进程创建接口, `execve` 重置地址空间后子进程可用完整 C++20.

**影响**: 决策 1 理由中"与 ADR-0021 SafeExec 的 `with_timeout` + `fork` 模式一致"陈述**不准确**(SafeExec 当前无 fork 实现), 本修订同时澄清.

### 修订 2 — Capability 注入: 决策 3 实施分期

**原决策**: Capability 三方交集(`manifest` × `SKILL metadata` × `OS policy`), 启动时一次注入.

**修订后**: 
- **V1**: 硬编码 `default_skill_capability()` 返回固定值, 不实现 `derive_capability()` 三方交集. SkillInterpreter::run() 接受 2 参数 `(skill_path, cap)`, 调用方负责提供 cap.
- **V2 (独立 future change)**: 实现 `derive_capability(manifest, skill_meta, os_policy)` 三方交集; 引入 `pdk_manifest.json` 加载器、SKILL frontmatter `requires_*` 字段、`IExecutionPolicy::get_capability()` 集成.

**理由**: V1 阶段 manifest 加载器未实现, YAGNI 原则下避免提前设计抽象; V1 硬编码 cap 即可满足 pdk_chat_demo 验证需求.

### 修订 3 — 语法子集: V1 不实现 read_context

**原决策 4**: host function 接口包含 6 个 (`host_call_tool` / `host_emit_event` / `host_consume_budget` / `host_read_context` / `host_llm_generate` / `host_log`).

**修订后 V1**:
- **实现**: 4 个 (`host_call_tool` / `host_emit_event` / `host_consume_budget` / `host_llm_generate`) —— `host_consume_budget` 用 SkillInterpreter 内部 `std::atomic<double>` 计数器, 不调用 IBudgetController (见 design.md Decision 11 + 修订 4)
- **V2 defer**: `host_read_context(key)` 留待 V2 实现 (LayeredContext 序列化 + IPC 协议扩展)
- **静默丢弃**: `host_log` —— V1 SKILL 无显式日志 API; 子进程日志通过 `pipe_err` 收集到 `SkillResult.stderr_content` (见 design.md Decision 10), 满足调试需求

**理由**: V1 阶段 LayeredContext 桥接期 (ADR-0008, OpenSpec `migrate-context-to-layered` 2026-08-01 ship) 尚未稳定, read_context 的 IPC 序列化语义需与桥接层对齐, 提前实现会引入 Breaking change. `host_log` 与 `pipe_err` 功能重叠, V1 不重复实现.

### 修订 4 — 错误码扩展

新增 6 个 P3 ErrorCode 值于 `src/core/types/tool_result.h` (2026-07-21):
- `SandboxViolation` — 子进程触发 seccomp 白名单违规
- `MaxStepsExceeded` — 父进程 IPC 循环 max_steps 强制触发
- `Crash` — 子进程因信号异常退出
- `BudgetExhausted` — Skill USD 预算耗尽
- `UnsupportedPlatform` — 非 Linux 平台调用
- `InvalidArg` — 错误输入参数 (SKILL.md 不存在 / frontmatter 缺失字段 / 环境变量缺失)

`SkillResult.error_code` (skill_interpreter 内部) 复用这些值.

## 背景

### 问题

SKILL.md 是 Agent 的一种实现形态——非结构化描述，解释执行，适合快速迭代。但 SKILL 可以来自不可信来源（Marketplace、社区贡献、用户自行编写）。OS 必须提供：

1. **隔离执行环境**：防止恶意 SKILL 访问系统资源
2. **受控的 capability 注入**：SKILL 只能调用被允许的 OS 服务
3. **资源限制**：限制 SKILL 的执行时间、内存、LLM 调用次数

### 目标

定义 SKILL.md 的隔离执行模型，包括隔离技术、语法子集、capability 注入机制。

## 决策

### 决策 1 — 隔离技术：进程隔离 + seccomp

> ⚠️ **2026-07-21 修订**: 见 [实施修订记录](#实施修订记录-2026-07-21) § 修订 1. 实际技术栈从 `fork()` 改为 `posix_spawn() + execve()`.
> 以下原始决策保留作为架构意图记录，实施细节已变更.

**v1 推荐技术栈（原始, 已修订）**：`posix_spawn()` + `execve(/proc/self/exe, "--skill-child")` + `seccomp(BPF)` + `setrlimit()`

```
[SkillInterpreter (主进程)]
        │
        ├── posix_spawn → [SANDOXED CHILD via execve(--skill-child)]
        │                │
        │                ├── seccomp 白名单（~20 syscalls）
        │                ├── setrlimit(RLIMIT_CPU, 30s)
        │                ├── setrlimit(RLIMIT_AS, 256MB)
        │                ├── setrlimit(RLIMIT_NOFILE, 8)
        │                │
        │                ├── [SKILL Engine] 解释执行 SKILL.md
        │                │       │
        │                │       ├── host_call_tool()     → 父进程 IPC
        │                │       ├── host_emit_event()    → 父进程 IPC
        │                │       ├── host_consume_budget() → 父进程 IPC
        │                │       └── host_read_context()   → 父进程 IPC
        │                │
        │                └── exit(0)
        │
        ├── waitpid(timeout=30s)
        ├── SIGKILL (if timeout)
        └── collect result + stderr
```

**seccomp 白名单**（Linux x86_64）：

```
允许: read, write, close, exit_group, exit,
      mmap, munmap, brk, mprotect, futex,
      clock_gettime, getrandom, sched_yield,
      stat, lseek, fstat, writev

禁止: openat, open, execve, socket, clone, 
      fork, vfork, kill, ptrace, ioctl
```

**资源限制**：

| 资源 | 默认值 | 配置位置 |
|------|--------|---------|
| CPU 时间 | 30s | manifest `resources.timeout_ms` |
| 内存 | 256MB | 硬限制 |
| 文件描述符 | 8 | 硬限制 |
| 子进程 | 0 | seccomp 禁止 fork/clone |
| 网络 | 0 | seccomp 禁止 socket |

**理由**：
- `seccomp(BPF)` 是 Linux 上最轻量的 syscall 过滤方案（无容器开销）
- ~~与 ADR-0021 `SafeExec` 的 `with_timeout` + `fork` 模式一致~~ （**已废止**: SafeExec 当前实现是 `std::async`，无 fork 模式；ADR-0021 §3.3 计划中 fork/seccomp 为 Phase 2/3。本修订 1 将 fork 改为 posix_spawn）
- 无需 Docker/Firecracker 等重型容器

### 决策 2 — SKILL.md 语法子集

**白名单原则**：只允许明确列出的操作，禁止所有未列出的操作。

**允许的操作**：

| 操作 | 接口 | 备注 |
|------|------|------|
| 工具调用 | `call_tool(name, args)` | 受审批策略和 capability 限制 |
| 事件推送 | `emit_event(topic, payload)` | 受 topic 白名单限制 |
| 预算消耗 | `consume_budget(amount)` | 从 SKILL 限额中扣除 |
| 上下文读取 | `read_context(key)` | 只读，不可写 |
| LLM 推理 | `llm_generate(prompt)` | 受 budget 控制 |
| 变量赋值 | `assign(key, value)` | 内部变量，不持久化 |

**禁止的操作**：

| 操作 | 原因 | 拦截方式 |
|------|------|---------|
| 文件系统读写 | 安全边界 | seccomp 禁止 `openat` |
| 网络请求 | 安全边界 | seccomp 禁止 `socket` |
| 环境变量读取 | 密钥泄露 | seccomp + cap 白名单 |
| 子进程启动 | 权限提升 | seccomp 禁止 `fork`/`execve` |
| 无限循环 | 资源耗尽 | SKILL 引擎 `max_steps` 硬限制 |
| sleep > 5s | 资源占用 | SKILL 引擎拦截 |
| 修改 capability | 安全边界 | 运行时不可变 |

**`max_steps` 硬限制**：
```markdown
# My Skill
## metadata
- max_steps: 50           # 硬限制，超过自动终止
- timeout_ms: 30000       # 硬限制
```

### 决策 3 — Capability 注入机制

Capability 在 SKILL **启动时注入**，运行时不可修改：

```cpp
struct SkillCapability {
    std::vector<std::string> allowed_tools;     // 允许调用的工具名
    std::vector<std::string> allowed_topics;    // 允许推送的事件主题
    uint32_t max_steps;                         // 最大执行步数
    std::chrono::milliseconds timeout_ms;       // 超时
    double budget_limit_usd;                    // 预算上限（USD）
    bool allow_llm;                             // 是否允许 LLM 推理
};
```

**Capability 来源**（按优先级）：

| 来源 | 示例 | 说明 |
|------|------|------|
| Agent manifest | `pdk_manifest.json` | Plugin 开发者声明 |
| SKILL metadata | SKILL.md `## capabilities` | Skill 作者声明 |
| OS 执行策略 | `IExecutionPolicy` | OS 强制缩小（不可放大） |
| 用户运行前确认 | CLI --allow-network | 用户显式授权（Phase 2） |

**缩小规则**：最终 Capability = 三者的**交集**（最小权限原则）。

```yaml
# pdk_manifest.json 声明
allowed_tools: ["code_review/run", "code_review/suggest", "fs/read"]

# SKILL.md 声明
allowed_tools: ["code_review/run"]

# OS 执行策略
allowed_tools: ["code_review/run", "fs/read"]

# 交集（最终生效）
allowed_tools: ["code_review/run"]
```

### 决策 4 — 与 OS 服务的 host function 接口

```cpp
// 暴露给 SKILL 引擎的接口（通过 IPC 传递）

// 工具调用
nlohmann::json host_call_tool(
    const char* name, 
    const char* args_json
);  // 检查 name 在 allowed_tools 中

// 事件推送
void host_emit_event(
    const char* topic, 
    const char* payload_json
);  // 检查 topic 在 allowed_topics 中

// 预算消耗
bool host_consume_budget(
    double amount
);  // 从 budget_limit 中扣除

// 上下文读取
const char* host_read_context(
    const char* key
);  // 只读，返回 LayeredContext 快照

// LLM 推理
const char* host_llm_generate(
    const char* prompt
);  // 消耗 budget，检查 allow_llm

// 日志
void host_log(
    const char* level, 
    const char* message
);  // 写入 SKILL 的独立日志流
```

**IPC 协议**（进程隔离场景）：

```
子进程 → 父进程:
  JSON-RPC over pipe[1]:
  {
    "method": "call_tool",
    "params": {"name": "code_review/run", "args": {...}},
    "id": 1
  }

父进程 → 子进程:
  {
    "result": {"issues": [...]},
    "id": 1
  }
```

### 决策 5 — 超时与资源回收

> ⚠️ **2026-07-21 修订**: 步骤 1 已从 `fork()` 改为 `posix_spawn()` + `execve(self, "--skill-child")`. 其余步骤保持.

```
SkillInterpreter::run(skill_path, capability):
  1. posix_spawn() + execve(/proc/self/exe, "--skill-child", envp) 子进程
  2. 子进程：seccomp + setrlimit + 注入 capability
   3. 子进程：解释执行 SKILL.md，通过 IPC 调用 host functions
   4. 父进程：waitpid(timeout) + EINTR 重试
      ├── 正常退出 → 收集结果
      ├── 超时 → SIGKILL → 返回 ErrorCode::Timeout
      ├── SIGSYS (seccomp 违规) → 返回 ErrorCode::SandboxViolation（修订 4）
      └── 其他信号死亡 (SIGSEGV/SIGABRT/SIGBUS) → 返回 ErrorCode::Crash（修订 4）
  
  5. 回收子进程资源
     - close pipe fds
     - waitpid() 确认僵尸进程已回收
```

## 替代方案

### 方案 A：Docker/Firecracker 容器隔离

**否决理由**：
- 启动延迟高（200-500ms vs 进程 fork 的 <1ms）
- 镜像管理复杂
- 对于 LLM Agent（每次执行数秒）隔离开销占比小，但 SKILL 可能短时间执行（ms 级）

### 方案 B：Wasm 解释器隔离

**否决理由**：
- Wasm 的 host function 模型与 SKILL.md 的动态性不匹配（SKILL 需要动态 `call_tool`，Wasm host function 是静态绑定的）
- Wasm 隔离用于固化后的 Agent（ADR-0056），不用于原型阶段的 SKILL

### 方案 C：纯语言沙箱（如 Lua sandbox）

**否决理由**：
- 需要嵌入一个完整的脚本语言解释器
- 语言沙箱在 C++ 侧的开销 > 进程隔离

## 不变量

- SKILL 形态的 Agent `requires_isolation` 必须 = true（ADR-0053 决策 4）
- 子进程崩溃不影响主进程（OS 的稳定性不依赖 SKILL 的正确性）
- Capability 在运行时不可修改（防止权限提升）
- 超时后必须 SIGKILL（SIGTERM 可能被忽略）

## 权衡

| 决策 | 选择 | 理由 |
|------|------|------|
| 隔离技术 | 进程 + seccomp | 成熟、轻量、与 SafeExec 一致 |
| 语法约束 | 白名单 | 安全：只允许明确的 |
| Capability 注入 | 启动时一次注入 | 运行时不可变 |
| 超时 | SIGKILL | 防止 SKILL 忽略 signal |
| IPC | JSON-RPC over pipe | 简单、可靠、跨语言 |
| Wasm 隔离 | 不留 SKILL | Wasm 用于固化阶段 |

## 后续行动

- 实现 `SkillInterpreter` 组件（**posix_spawn + execve(self, --skill-child)** + seccomp + capability injection，见修订 1）
- ADR-0056: Wasm 隔离（固化后的 Agent 用 Wasm 而不是进程）
- Phase 2: 用户显式授权（CLI 交互审批超出默认 capability 的操作）

## 参考

- [ADR-0021 — PDK Design §3.3 SafeExec](../adr-0021-pdk-design.md)
- [ADR-0004 — ToolRegistry Security](../adr-0004-toolregistry-security.md)
- [ADR-0053 — AgentDescriptor 与 pdk_register_agent](./adr-0053-agent-descriptor-interface.md)
- seccomp(2) — Linux 内核 syscall 过滤机制
- OSGi — Bundle 的 capability 声明与解析