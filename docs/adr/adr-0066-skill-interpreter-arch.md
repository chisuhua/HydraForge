# ADR-0066: SkillInterpreter 模块架构

## 状态

🟡 Partial (V1 implemented, 2026-07-22)

## 领域

Skill 隔离执行 / Agent-as-Plugin 运行时

## 关联

- [ADR-0055 — Skill 执行与隔离模型](../adr-0055-skill-isolation.md) — 隔离模型决策 (posix_spawn/seccomp/capability)
- [ADR-0021 — PDK Design](../adr-0021-pdk-design.md) — SafeExec 沙箱封装
- [ADR-0004 — ToolRegistry Security](../adr-0004-toolregistry-security.md) — 权限校验
- [ADR-0031 — Execution Policy](../adr-0031-execution-policy.md) — 审批策略
- [ADR-0053 — AgentDescriptor](../adr-0053-agent-descriptor-interface.md) — `requires_isolation` 字段
- [ADR-0059 — Cross-Process Protocol](../adr-0059-cross-process-protocol.md) — 跨进程 IPC 协议
- [ADR-0060 — Agent Composition](../adr-0060-agent-composition.md) — Agent 组合协议

## 背景

### 问题

AgenticDSL 需要执行 SKILL.md 文件（命令式 DSL），在多线程父进程（DSLEngine 持有 InMemoryBus dispatch_thread + DomainWorkerPool + CognitiveWorker）环境下创建隔离子进程。需求：

1. **进程隔离**: 子进程不能继承父进程的 mutex 状态
2. **能力限制**: 子进程只能使用授权工具和事件 topic
3. **资源控制**: 步数限制 + 预算限制 + 超时
4. **IPC 协议**: 父进程通过 pipe 发送/接收 JSON 消息
5. **安全保证**: seccomp(BPF) 白名单 + setrlimit 资源限制

### V1 范围 (2026-07-22 ship)

| 功能 | 状态 |
|------|------|
| `posix_spawn()` + `execve(/proc/self/exe, --skill-child)` | ✅ |
| seccomp(BPF) 白名单 | ✅ |
| pipe-based JSON IPC | ✅ |
| `host_call_tool` (工具调用) | ✅ |
| `host_emit_event` (事件发送) | ✅ |
| `host_consume_budget` (预算消耗) | ✅ |
| `host_llm_generate` (LLM 调用) | ✅ |
| `SkillCapability` 注入 | ✅ (硬编码 `default_skill_capability()`) |
| `host_read_context` | ❌ Deferred (V2) |
| `host_log` | ❌ Dropped (stderr pipe 替代) |
| `derive_capability()` 三方交集 | ❌ Deferred (V2) |

## 决策

### 决策 1 — PIMPL 设计

**选择**: SkillInterpreter 使用 PIMPL 模式 (pointer-to-implementation)

```cpp
class SkillInterpreter {
 public:
  SkillInterpreter(IToolRegistry& tools, IInteractionBus& bus,
                   ILLMProvider* llm, const LayeredContext* ctx);
  ~SkillInterpreter();
  SkillResult run(const std::string& skill_path, const SkillCapability& cap);

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};
```

**理由**:
- 公开头文件 `include/agenticdsl/skill/skill_interpreter.h` 仅 106 行，无需暴露 `posix_spawn` / `pipe` / `seccomp` 等系统依赖
- Impl 在 `src/modules/skill_interpreter/skill_interpreter.cpp`（708 行）中实现，包含所有系统调用细节
- 析构函数 out-of-line，自动 `waitpid()` 防止僵尸进程
- 与项目中其他 PIMPL 模式保持一致（TopoScheduler, MarkdownParser, ExecutionSession 等，ADR-0019 §1.4）

### 决策 2 — 子进程架构: skill_child_main

**选择**: 子进程入口在独立 translation unit `skill_child_main.cpp` (671 行)，通过 `execve(/proc/self/exe, "--skill-child")` 启动

**子进程生命周期**:
```
main() → 检查 --skill-child flag
  → skill_child_main() 入口
  → 加载 SKILL.md 文件
  → 解析 IPC 参数 (stdin/stdout pipe fd)
  → 安装 seccomp(BPF) 过滤器
  → 执行 SKILL 指令
  → 通过 IPC 调用 host function
  → 退出
```

**理由**:
- `execve()` 重置地址空间，解决父进程多线程 `fork()` 的 async-signal-safety 问题
- 独立的 `.cpp` 文件使子进程代码与父进程完全隔离
- seccomp 过滤器在子进程安装，不影响父进程

### 决策 3 — IPC 协议: JSON over pipe

**选择**: 父子进程通过 `pipe()` 双向 JSON 通信

**协议格式** (父→子):
```json
{"type": "response", "seq": 1, "result": {...}, "error": null}
```

**协议格式** (子→父):
```json
{"type": "call_tool", "seq": 1, "tool": "fs.read", "args": {...}}
{"type": "emit_event", "seq": 2, "topic": "tool.audit", "data": {...}}
{"type": "consume_budget", "seq": 3, "amount": 0.001}
{"type": "llm_generate", "seq": 4, "prompt": "..."}
```

**序列号机制**: 每条请求携带递增 `seq`，响应匹配 `seq` 确保同步语义。

**理由**:
- JSON 在 AgenticDSL 中已广泛使用 (nlohmann_json)
- pipe 是 POSIX 最轻量 IPC 机制，零依赖
- 同步 RPC 协议 (父子轮流读写) 避免并发 IPC 复杂性

### 决策 4 — 4 个 Host Function (V1)

**选择**: V1 实现 4 个 host function，2 个 deferred/dropped:

| Host Function | V1 | 说明 |
|---------------|-----|------|
| `host_call_tool` | ✅ | 通过 `IToolRegistry::call_tool()` 转发 |
| `host_emit_event` | ✅ | 通过 `IInteractionBus::emit()` 转发，topic 白名单检查 |
| `host_consume_budget` | ✅ | SkillInterpreter 内部 `std::atomic<double>` 计数器 |
| `host_llm_generate` | ✅ | 通过 `ILLMProvider::generate()` 转发 (需 `allow_llm=true`) |
| `host_read_context` | ❌ V2 | LayeredContext 序列化 + IPC 协议扩展 |
| `host_log` | ❌ Dropped | stderr pipe 替代 (`SkillResult.stderr_content`) |

**理由**: ADR-0055 §修订 3 + 修订 4

### 决策 5 — Capability 注入模型

**选择**: V1 硬编码 `default_skill_capability()`，调用方通过 `SkillInterpreter::run(skill_path, cap)` 注入

```cpp
struct SkillCapability {
  std::vector<std::string> allowed_tools;   // 白名单工具名
  std::vector<std::string> allowed_topics;  // emit_event topic 白名单
  uint32_t max_steps = 50;                  // IPC 循环最大步数
  std::chrono::milliseconds timeout_ms{30000};
  double budget_limit_usd = 0.01;
  bool allow_llm = false;
};
```

**V2 计划** (独立 future change): `derive_capability(manifest, skill_meta, os_policy)` 三方交集

**理由**: ADR-0055 §修订 2

## 模块结构

```
include/agenticdsl/skill/
  └── skill_interpreter.h          # 公开头文件 (PIMPL, 106 行)

src/modules/skill_interpreter/
  ├── CMakeLists.txt
  ├── skill_interpreter.cpp        # Impl (PIMPL body, 708 行)
  └── skill_child_main.cpp         # 子进程入口 (671 行)

tests/
  ├── test_skill_interpreter.cpp   # 18 test cases
  └── main_skill_test_runner.cpp   # test runner
```

## 不变量

1. **逻辑隔离 (非物理)**: 父子进程通过 pipe IPC 完全隔离地址空间
2. **thread_local 限制**: 子进程禁止使用 `thread_local` 变量（`execve` 后地址空间重置）
3. **单向依赖**: SkillInterpreter 依赖 IToolRegistry/IInteractionBus，不反向依赖
4. **无全局状态**: SkillInterpreter 每个实例独立的 `std::atomic<double>` 预算计数器

## 限制与后续工作

| 限制 | 影响 | 后续 |
|------|------|------|
| V1 无 `derive_capability` | 调用方需手动提供 SkillCapability | V2: 从 manifest/skill_meta/os_policy 三方交集派生 |
| V1 无 `host_read_context` | SKILL 不能读取父进程 LayeredContext | V2: LayeredContext 序列化 + IPC 协议扩展 |
| seccomp 白名单保守 | 可能阻止合法系统调用 | 随着使用场景增加逐步放宽 |
| 无 `fork()` 模式 | 不支持共享内存 IPC | 永久决策 (posix_spawn 满足需求) |

## 实施记录

- **2026-07-21**: Spike §0.4 完成 — 5 CRITICAL 风险中 3 PASS / 1 FAIL (C5 顺序) / 1 PASS（详见 `docs/audits/2026-07-21-skill-interpreter-spike-0.4.md`）
- **2026-07-22**: OpenSpec change `skill-interpreter-real-loading` ship — 18/18 ctest pass, posix_spawn + seccomp + pipe IPC 全链路验证通过
- **2026-07-22**: ADR-0055 正式 Approved → Implementation Complete
- **2026-07-22**: Wave 3 debt — 顺延至下 Sprint eval

## 变更日志

| 日期 | 变更 | 理由 |
|------|------|------|
| 2026-07-22 | 创建 ADR-0066 | Debt audit: skill_interpreter 模块缺少独立架构 ADR |
