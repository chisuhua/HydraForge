# EnvBackend 多环境执行规范 (ADR-0075 D1+D2+D3)

> **状态**：✅ Approved (2026-08-18 — Wave 3-A `from-roadmap-phase-6c-execution-envbackend` ship: D1 IEnvBackend 接口 + D2 LocalBackend (C11) + D3 DockerBackend (C12) + D5 EnvValidationHook (C13))
>
> **关联**：[ADR-0075 — EnvBackend 多环境执行](./adr/adr-0075-env-backend-local-docker.md) §决策 D1-D3 / §不变量 1-7 · [ADR-0003](./adr/adr-0003-dslengine-thread-safety.md) 线程安全 · [ADR-0004](./adr/adr-0004-toolregistry-security.md) ToolCategory · [ADR-0069](./adr/adr-0069-tool-coordinator-hooks.md) pre-hook 集成 · [ADR-0068](./adr/adr-0068-event-emission-contract.md) 事件脱敏

## 一、设计目标

为 AgenticDSL Execution Plane 提供**隔离执行抽象**，支持：

1. **多环境路由** — 同一 `shell.exec` 节点可在 `local` / `docker:<container_id>` / `docker:<image>:<tag>` 三种 backend 间无缝切换
2. **命令注入防御** — `cmd + args` 数组分离 + `execve()` 逐参传递，**不**走 `system()` / `popen()` shell 解析路径（OWASP A03:2021 防御）
3. **资源隔离** — LocalBackend fork+execve + `RLIMIT_AS` / `RLIMIT_CPU`；DockerBackend `HostConfig.Memory` / `NanoCpus` / `Privileged=false`
4. **后端策略** — Per-backend `requires_approval` / `allowed_env_vars` / `allowed_paths` / `image_allowlist` 强制检查，由 EnvValidationHook pre-hook 拦截（ADR-0069 §决策 D3）

## 二、接口契约

### 2.1 `IEnvBackend` 抽象接口 (`include/agenticdsl/env/env_backend.h`)

```cpp
namespace agenticdsl {

struct ExecRequest {
  std::string cmd;                  // 可执行文件绝对路径 (execve 不经 PATH 解析)
  std::vector<std::string> args;    // 参数数组, 逐参 execve 传递, 不经 shell
  std::string working_dir;          // 可选; 空 = 继承当前目录
};

struct ExecOptions {
  uint64_t timeout_ms = 30000;               // 默认 30s
  size_t max_output_bytes = 64 * 1024;       // 默认 64KB (见 §四 与 ADR-0075 D1 1MB 上限差异)
  bool capture_stderr = true;
  std::map<std::string, std::string> env;    // env 白名单 — 仅透传显式声明
};

struct ExecResult {
  int exit_code = -1;               // WEXITSTATUS; 信号死亡 = 128 + signo
  std::string stdout_buf;
  std::string stderr_buf;
  bool timed_out = false;
  BackendErrorCode error_code = BackendErrorCode::Success;
  uint64_t duration_ms = 0;
};

struct BackendCapabilities {
  bool supports_isolation = false;
  bool supports_persistent_fs = false;
  uint32_t max_concurrent_execs = 1;
};

class IEnvBackend {
 public:
  virtual ~IEnvBackend() = default;
  virtual ExecResult exec(const ExecRequest&, const ExecOptions&) const = 0;
  virtual BackendCapabilities capabilities() const = 0;
};

std::shared_ptr<const IEnvBackend> create_backend(
    const std::string& backend_spec, const BackendConfig& config);

}  // namespace agenticdsl
```

### 2.2 错误码表 (`BackendErrorCode`)

| 枚举 | 字符串 | 触发场景 |
|------|--------|----------|
| `Success` | `SUCCESS` | 子进程正常 exit |
| `ForkFailed` | `ERR_BACKEND_FORK_FAILED` | LocalBackend `fork()` 返回 -1 / `pipe()` 失败 |
| `CommandNotFound` | `ERR_BACKEND_COMMAND_NOT_FOUND` | `execve` 返回 ENOENT（exec CLOEXEC 管道收到 errno） |
| `Timeout` | `ERR_BACKEND_TIMEOUT` | `waitpid(timeout)` 升级 SIGTERM→SIGKILL 后子进程仍存活 |
| `OutputTooLarge` | `ERR_OUTPUT_TOO_LARGE` | pipe 输出超 `max_output_bytes`（默认 64KB） |
| `Unavailable` | `ERR_BACKEND_UNAVAILABLE` | Docker daemon `/ping` 返回非 200（默认 `fail_fast` policy） |
| `SecurityViolation` | `ERR_BACKEND_SECURITY_VIOLATION` | DockerBackend `Privileged=true`（D-7 强制） |
| `Unknown` | `ERR_BACKEND_UNKNOWN` | execve 失败但 errno 非 ENOENT / 兜底 |

错误码 → 字符串映射函数 `agenticdsl::backend_error_name(BackendErrorCode)`。

### 2.3 Backend factory 解析规则

`create_backend(spec, config)` 按以下规则解析：

| spec 格式 | 实例 | backend |
|-----------|------|---------|
| `"local"` | `local` | `LocalBackend` |
| `"docker:<container_id>"` | `docker:abc123def456` | `DockerBackend` (mode a, exec into existing) |
| `"docker:<image>[:<tag>][@<digest>]"` | `docker:python:3.12@sha256:abc...` | `DockerBackend` (mode b, ephemeral) |
| 未知 | `k8s:pod-x` | `nullptr`（预留扩展点） |

Daemon 不可达行为由 `BackendConfig::docker_unavailable_policy` 控制（默认 `FailFast`）。

## 三、LocalBackend 实施细节 (`src/common/env/local_backend.cpp`)

### 3.1 fork+execve POSIX 子进程隔离

**采用 `fork() + execve()` 而非 `posix_spawn`**（per ADR-0075 §决策 D2 line 145-176）：

- 父进程需 `waitpid(timeout)` 实施超时控制（`posix_spawn` 在 Linux 上底层仍 fork+execve，但语义不变）
- 安全约束全部借鉴 ADR-0055 SkillInterpreter 模式（FD cleanup + env 白名单 + stdio 重定向）

**OWASP 命令注入防御**：

```cpp
// 错误示例（禁止）: ::system("ls; rm -rf /")  — shell 解析, 元字符展开
// 正确做法（实施）:
std::vector<char*> argv = {"/bin/ls", "-la", "/tmp", nullptr};
std::vector<char*> envp = {"PATH=/usr/bin", nullptr};  // 不继承 parent env
::execve("/bin/ls", argv.data(), envp.data(), envp.size());
```

- `cmd` + `args` 数组分离 + `execve()` 逐参传递，**不使用** `system()` / `popen()`
- 测试覆盖 OWASP 注入清单：`ls; rm -rf /` / `$(whoami)` / `` `whoami` `` → execve 数组逐参不解析 shell（`tests/test_backend_security.cpp`）

### 3.2 超时控制（SIGTERM → SIGKILL grace period）

```cpp
// waitpid 循环, 超时升级 SIGTERM (grace) → SIGKILL
const auto kill_grace = [&](int grace_ms) {
  ::kill(pid, SIGTERM);                                    // 优雅退出
  const auto deadline = Clock::now() + std::chrono::milliseconds(grace_ms);
  while (Clock::now() < deadline) {
    if (::waitpid(pid, &st, WNOHANG) == pid) return st;     // 子进程响应 SIGTERM 退出
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  ::kill(pid, SIGKILL);                                    // 强杀兜底
  ::waitpid(pid, &st, 0);
  return st;
};
```

| 触发条件 | grace period | 升级信号 |
|----------|--------------|----------|
| `timeout_ms` 到期 | 5 秒 | SIGTERM @ 5s → SIGKILL @ 10s |
| `max_output_bytes` 截断 | 1 秒 | SIGTERM @ 1s → SIGKILL @ 2s |

`tests/test_local_backend.cpp::timeout_5s_kill_grace_period` 覆盖超时路径。

### 3.3 资源限制（`setrlimit`）

| 资源 | 默认 | 用途 |
|------|------|------|
| `RLIMIT_AS` | 1 GB | 防止内存爆炸（OOM） |
| `RLIMIT_CPU` | soft=300s, hard=305s | 防 fork bomb / CPU 死循环；SIGXCPU 在 soft 触发，默认 action Terminate，hard+5s 兜底 SIGKILL |

```cpp
struct rlimit cpu_lim{rlimit_cpu_sec_, rlimit_cpu_sec_ + 5};
::setrlimit(RLIMIT_CPU, &cpu_lim);
```

**5s grace 必要性**：`rlim_cur == rlim_max` 时内核跳过 SIGXCPU 直接 SIGKILL（man setrlimit），故设置 `hard = soft + 5` 强制走 SIGXCPU 默认终止路径。

`tests/test_local_backend.cpp::rlimit_cpu_kills_cpu_bound_child` 验证。

### 3.4 环境变量白名单

```cpp
std::vector<char*> envp;
for (const auto& [k, v] : opts.env) env_storage.push_back(k + "=" + v);
// envp 仅含显式声明, 不继承 parent env (per ADR-0075 §不变量 4)
```

`tests/test_local_backend.cpp::env_whitelist_does_not_inherit_parent_env` 验证 `HOME=/host` / `PARENT_SECRET=...` 在子进程不可见。

### 3.5 输出截断（max_output_bytes 默认 64KB）

```cpp
const auto append_capped = [&](std::string& dst, const char* data, size_t n) {
  if (dst.size() >= opts.max_output_bytes) { truncated = true; return; }
  const size_t room = opts.max_output_bytes - dst.size();
  dst.append(data, std::min(room, n));
  if (n > room) truncated = true;
};
```

输出超限触发 1s grace period SIGTERM，截断后的内容保留，`error_code = OutputTooLarge`。

### 3.6 audit 事件 `env.backend.exec.start/end`

```cpp
// 启动
bus_->emit(EventBuilder("env.backend.exec.start")
               .args({{"backend_spec", "local"}, {"cmd_hash", sha256_hex(req.cmd)}})
               .build());
// 结束
bus_->emit(EventBuilder("env.backend.exec.end")
               .args({{"backend_spec", "local"},
                      {"cmd_hash", cmd_hash},
                      {"exit_code", result.exit_code},
                      {"timed_out", result.timed_out},
                      {"error_code", backend_error_name(result.error_code)}})
               .latency_ms(result.duration_ms)
               .build());
```

- **`cmd_hash` 脱敏**：`SHA256(req.cmd)` 64 字符 hex（per ADR-0068 §5.11 args-only-keys 政策）
- **不记录** raw args / raw env（防 secret 泄漏）
- `bus` 为 `nullptr` 时跳过事件发射（无侵入式注入）

## 四、输出截断阈值 64KB vs ADR-0075 D1 line 96 1MB 差异说明

| 项 | 本提案 (env-backend) | ADR-0075 §决策 D1 |
|------|---------------------|-------------------|
| `max_output_bytes` 默认 | **64 KB** | 1 MB |
| `max_output_bytes` 可配置 | ✅ | ✅ |
| 触发截断行为 | SIGTERM 子进程 + 截断 buffer + `OutputTooLarge` | 同 |

**差异原因**：

- 1 MB 输出会撑爆 NodeExecutor 上下文（LayeredContext L3 working 默认 < 256 KB）
- 64 KB 是 LLM 工具调用的合理上限（超出应 truncate + 警告用户，而非整块塞入 LLM 上下文）

**未来调整**：若用户场景需要更大输出上限，可在 `BackendConfig` 注入 `max_output_bytes_override`（Phase 7+ 决议保留）。

## 五、DockerBackend 实施细节 (`src/common/env/docker_backend.cpp`)

### 5.1 适配说明：cpp-httplib 而非 libcurl

ADR-0075 §决策 D3 指定 **libcurl + Docker REST API**，但本提案实施时检查发现：

- `external/libcurl/` **未 vendor**（proposal 假设错误）
- 改用已 vendor 的 **cpp-httplib**（`external/cpp-httplib/` header-only）
- `httplib::Client::set_address_family(AF_UNIX)` 原生支持 unix socket，TCP `host:port` 用于 httplib mock server 测试

零新增外部依赖，满足 "MUST NOT introduce new external dependencies"。

### 5.2 双模式

**(a) exec into existing container** — 适用 dev/test：

```
POST /containers/{id}/exec  →  { "Id": "<exec_id>" }
POST /exec/{id}/start       →  {"stdout_buf", "stderr_buf"} demux
GET  /exec/{id}/inspect     →  {"ExitCode": <int>}
```

**(b) ephemeral container lifecycle** — 适用 CI/sandbox：

```
POST /containers/create?name=hydraforge-<hash>-<seq>  → { "Id": "<cid>" }
POST /containers/{cid}/start                           → 204
POST /containers/{cid}/wait                            → { "StatusCode": <int> }
GET  /containers/{cid}/logs?stdout=1&stderr=1          → 多路复用流
DELETE /containers/{cid}?force=true                    → 无残留清理
```

ephemeral container 命名规范 `hydraforge-<cmd_hash 前 12>-<atomic seq>`，测试后 `docker ps -a` 无残留（per ADR-0075 D-3 §SHOULD 项）。

### 5.3 资源限制（`HostConfig` 字段）

| 字段 | 默认 | 说明 |
|------|------|------|
| `Memory` | 512 MB | `max_memory_mb * 1024 * 1024` |
| `NanoCpus` | 2 × 10⁹ | `max_cpu_cores * 1e9` |
| `Privileged` | **false** | D-7 强制 false，不可覆盖 |
| `NetworkMode` | `bridge` | 默认 bridge 网络，可选 none/host |
| `Tmpfs[/tmp]` | `rw,nosuid,nodev,size=64m` | 临时存储 |

`tests/test_docker_backend.cpp::resource_limits_appear_in_create_body` 验证。

### 5.4 安全约束（D-7 强制）

| 检查 | 位置 | 失败行为 |
|------|------|----------|
| `cfg.privileged == false` | exec 入口前置 | `SecurityViolation` + 不发任何 HTTP 请求 |
| 禁止 `docker` CLI 调用 | `grep -r "docker "` 强制约束 | 仅 libcurl/httplib + REST API |
| 容器镜像 digest 锁定 | `docker:python:3.12@sha256:abc...` 透传 | image 不含 `@sha256:` 时仅 WARN |

`tests/test_backend_security.cpp::docker_privileged_mode_rejected` + `tests/test_docker_backend.cpp::privileged_mode_request_rejected_with_security_violation` 双重覆盖。

### 5.5 Daemon 不可达 fallback policy

```cpp
// BackendConfig::docker_unavailable_policy
enum class DockerUnavailablePolicy {
  FailFast,        // 默认保守: 不静默降级
  FallbackToLocal  // 显式 opt-in: stderr warning + 回退 LocalBackend
};
```

`tests/test_docker_backend.cpp::docker_unavailable_fallback_to_local` + `docker_daemon_unavailable_fail_fast` 双覆盖。

### 5.6 audit 事件

DockerBackend 复用 §3.6 LocalBackend 同一事件 schema，`backend_spec` 字段填 `docker:<container_id>` 或 `docker:<image>` 区分模式。

## 六、EnvValidationHook (C13, `src/common/hooks/env_validation_hook.cpp`)

### 6.1 工厂函数

```cpp
PreHook make_env_validation_hook(BackendConfig config);
```

注册到 `ToolHookRegistry`：

```cpp
hooks.register_pre_hook("*", make_env_validation_hook(config), 0,
                        HookErrorPolicy::FailClosed);
```

### 6.2 四步 policy 校验

仅对 `ToolCategory::Execute` 工具生效，其他类目放行：

| 步骤 | 检查 | 失败时 deny reason |
|------|------|-------------------|
| 1 | backend 命中 `BackendPolicy`（`find_policy`） | `unknown backend: <spec>` |
| 2 | docker 镜像 allowlist | `image not in allowlist: <image>` |
| 3 | env 白名单（`env.<NAME>` 键） | `env var not allowed: <NAME>` |
| 4 | working_dir 前缀白名单 | `working_dir not allowed: <path>` |
| 5 | approval gate（`requires_approval=true` 需 `__approved=true`） | `Backend policy requires approval` |

任意步骤失败返回 `PreHookResult::Deny`，ToolCoordinator 自动 emit `tool.audit.denied` 事件（per ADR-0068）。

### 6.3 ADR-0069 hook 顺序

```
ToolCoordinator.execute(meta, ctx, args)
    │
    ├─[pre-hook 列表]→ make_env_validation_hook ← EnvValidationHook (本 ADR)
    ├─ ApprovalHandler (auto-approve / event_bus / stdin)
    └─ registry.call_tool(meta, ctx, args)
```

EnvValidationHook 在 `ToolCoordinator.execute` 入口**强制**触发（per ADR-0075 §不变量 6 + ADR-0069 §决策 D3），包括 legacy V2 工具与 plugin 加载工具；hook 旁路视为安全违规。

## 七、工具节点集成（`tests/test_env_validation_hook.cpp::tool_coordinator_dispatch_full_flow`）

```cpp
ToolRegistry registry;
ToolHookRegistry hooks;
hooks.register_pre_hook("*", make_env_validation_hook(BackendConfig::with_defaults()),
                        0, HookErrorPolicy::FailClosed);

auto backend = create_backend("local", BackendConfig::with_defaults());
registry.register_tool_function("shell/exec", make_exec_meta(),
    [backend](const auto& args) -> json {
      ExecRequest req{args.at("cmd"), {}, args.count("working_dir") ? args.at("working_dir") : ""};
      auto r = backend->exec(req, ExecOptions{});
      return {{"exit_code", r.exit_code}, {"stdout", r.stdout_buf}};
    });

ToolCoordinator coordinator(registry, std::make_shared<AgentModePolicy>(),
                           make_test_auto_callback(true));
coordinator.set_hook_registry(&hooks);

auto result = coordinator.execute(make_exec_meta(), make_ctx(), {
    {"backend", "local"}, {"cmd", "/bin/echo"},
    {"args", "hello-env-hook"}, {"__approved", "true"}});
```

端到端验证：`ToolCoordinator.execute` → `EnvValidationHook` → `ApprovalHandler` → `LocalBackend.exec` 完整链路。

## 八、测试覆盖矩阵

| 文件 | 测试用例数 | 覆盖 |
|------|-----------|------|
| `tests/test_local_backend.cpp` | 7 | happy / ENOENT / timeout SIGTERM→SIGKILL / truncate 64KB / env 白名单 / RLIMIT_CPU / 并发线程安全 / capabilities |
| `tests/test_docker_backend.cpp` | 7 | ephemeral lifecycle / exec into existing / privileged 拒绝 / 资源限制生效 / daemon 503 fail_fast / fallback_to_local / image digest 锁定 |
| `tests/test_backend_factory.cpp` | 4 | local 创建 / docker 容器 ID 解析 / docker image:tag 解析 / 未知 spec 拒绝 |
| `tests/test_backend_policy.cpp` | 4 | 默认策略表 3 档 / per-environment override / image allowlist / docker_unavailable_policy fallback |
| `tests/test_env_validation_hook.cpp` | 7 | backend 命中 / env var 拒绝 / working_dir 拒绝 / approval gate / ephemeral 免审批 / docker:prod 审批 / 非 Execute 类目跳过 + tool_coordinator_dispatch_full_flow |
| `tests/test_backend_security.cpp` | 4 | OWASP shell injection × 3 (`ls; rm -rf /` / `$(whoami)` / backticks) + DockerBackend privileged 拒绝 |

**合计 ≥33 case**（远超 proposal 验收 ≥27 case 门槛）。

## 九、向后兼容与回滚

- **API 零变更**：DSLEngine / TopoScheduler / NodeExecutor / ToolCoordinator 现有签名不变；本提案新增 `set_backend_factory` opt-in（默认不注入）
- **回滚**：`revert` 后 baseline tools 100% 兼容（env_backend 是可选 backend 选择，default 行为不变）
- **ADR-0075 D4** (`backend:` 字段 DSL 解析) 留 Phase 6c W5 独立前置提案，本提案不依赖

## 十、参考

- [ADR-0075 — EnvBackend 多环境执行](./adr/adr-0075-env-backend-local-docker.md) — 本规范的主 ADR
- [ADR-0003 — DSLEngine 线程安全](./adr/adr-0003-dslengine-thread-safety.md) — `shared_ptr<const IEnvBackend>` 不可变约束
- [ADR-0004 — ToolRegistry 安全模型](./adr/adr-0004-toolregistry-security.md) — `ToolCategory::Execute` dangerous 类目矩阵
- [ADR-0055 — SkillInterpreter posix_spawn 模式](./adr/adr-0055-skill-interpreter.md) — FD cleanup + env whitelist 安全参考
- [ADR-0068 — 事件发射契约](./adr/adr-0068-event-emission-contract.md) — `env.backend.exec.start/end` 主题 + `cmd_hash` 脱敏
- [ADR-0069 — ToolCoordinator Hooks](./adr/adr-0069-tool-coordinator-hooks.md) — pre-hook 注入规范
- [Docker Engine SDK](https://docs.docker.com/engine/api/sdk/) — DockerBackend REST API 端点
- [POSIX setrlimit(2) man page](https://man7.org/linux/man-pages/man2/setrlimit.2.html) — `RLIMIT_CPU` grace period 语义
- [OWASP A03:2021 — Injection](https://owasp.org/Top10/A03_2021-Injection/) — shell 注入防御