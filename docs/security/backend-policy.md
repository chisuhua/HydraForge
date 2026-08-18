# Backend Policy 安全指南 (ADR-0075 D5)

> **状态**：✅ Approved (2026-08-18 — Wave 3-A `from-roadmap-phase-6c-execution-envbackend` ship)
>
> **关联**：[ADR-0075 — EnvBackend 多环境执行](../adr/adr-0075-env-backend-local-docker.md) §决策 D5 · [ADR-0004 — ToolRegistry 安全模型](../adr/adr-0004-toolregistry-security.md) · [ADR-0069 — ToolCoordinator Hooks](../adr/adr-0069-tool-coordinator-hooks.md) · [docs/specs/env-backend.md §六](../specs/env-backend.md) — `EnvValidationHook` 集成

本文档面向**安全工程师 / DevOps / AgenticDSL 应用开发者**，提供 BackendPolicy 的：

1. 默认策略表（3 档：local / docker ephemeral / docker prod）
2. Per-environment 配置覆盖（`override_default_policy`）
3. Shell 注入防御 checklist
4. Audit 事件订阅指南（`env.backend.exec.start/end`）

## 一、默认策略表（BackendConfig::with_defaults）

| Backend spec | `requires_approval` | `allowed_env_vars` | `allowed_paths` | `allow_network` | `max_memory_mb` | `max_cpu_cores` | `image_allowlist` |
|---------------|---------------------|--------------------|-----------------|-----------------|-----------------|-----------------|-------------------|
| `local` | ✅ `true` | `PATH`, `HOME`, `USER`, `LANG` | `/tmp`, `$HOME` | ❌ `false` | 1024 | 1 | (N/A) |
| `docker:<image>:<tag>` (ephemeral) | ❌ `false` | `*` (全部) | `*` (全部) | ✅ `true` | 512 | 2 | 空 (不限制) |
| `docker:prod` (named) | ✅ `true` | (空 = 全拒) | (空 = 全拒) | ❌ `false` | 1024 | 2 | (per-deployment) |

**优先级链**（`BackendConfig::find_policy(spec)`）：

```
spec → overrides_[spec] (精确匹配)
    → spec == "local"          → local_policy
    → spec == "docker:prod"    → docker_prod_policy
    → spec.rfind("docker:", 0) == 0  → docker_ephemeral_policy
    → 其他 (e.g. "k8s:pod")    → nullptr (未知 backend → hook deny)
```

## 二、Per-Environment Override

### 2.1 `BackendConfig::override_default_policy(spec, policy)`

```cpp
auto config = BackendConfig::with_defaults();

BackendPolicy custom_local;
custom_local.requires_approval = false;             // 开发环境免审批
custom_local.allowed_env_vars = {"PATH", "HOME", "USER", "LANG", "DEBUG"};
custom_local.allowed_paths = {"*"};                // 开发期无路径限制
custom_local.max_memory_mb = 4096;
custom_local.max_cpu_cores = 4;
config.override_default_policy("local", custom_local);

BackendPolicy custom_python;
custom_python.image_allowlist = {"python:3.12", "python:3.12@sha256:abc..."};
custom_python.requires_approval = true;            // 生产 still 审批
config.override_default_policy("docker:python:3.12", custom_python);
```

### 2.2 Per-Environment 配置文件（建议路径）

| 部署环境 | 配置文件路径 | 加载方式 |
|----------|--------------|----------|
| Dev | `~/.config/agenticdsl/backend-policy-dev.json` | `BackendConfig::load("/path/to/dev.json")` |
| Staging | `/etc/agenticdsl/backend-policy-staging.json` | 同 |
| Prod | `/etc/agenticdsl/backend-policy-prod.json` | 同 |

**注**：YAML vs TOML vs JSON 语法决策留 Phase 7+（per ADR-0075 §后续 Open Question 5）。

### 2.3 CI/CD 推荐配置

**生产环境 (`prod.json`)**：

```json
{
  "local": {
    "requires_approval": true,
    "allowed_env_vars": ["PATH"],
    "allowed_paths": [],
    "allow_network": false,
    "max_memory_mb": 512,
    "max_cpu_cores": 1
  },
  "docker:prod": {
    "requires_approval": true,
    "allowed_env_vars": ["LANG", "TZ"],
    "allowed_paths": ["/srv/app"],
    "allow_network": false,
    "max_memory_mb": 2048,
    "max_cpu_cores": 4,
    "image_allowlist": ["internal-registry.company.com/agent:v1.2.3@sha256:..."]
  },
  "docker_unavailable_policy": "FailFast"
}
```

**Dev环境 (`dev.json`)**：

```json
{
  "local": {
    "requires_approval": false,
    "allowed_env_vars": ["*"],
    "allowed_paths": ["*"],
    "allow_network": true,
    "max_memory_mb": 4096,
    "max_cpu_cores": 8
  },
  "docker_unavailable_policy": "FallbackToLocal"
}
```

## 三、Shell 注入防御 Checklist

### 3.1 强制约束（per ADR-0075 §MUST/MUST NOT）

| 检查项 | 实施位置 | 验证 |
|--------|----------|------|
| ❌ **不使用 `system()` / `popen()`** | `local_backend.cpp` | `grep -r "system\(\|popen\(" src/common/env/` 应返回 0 行 |
| ❌ **不使用 `docker` CLI** | `docker_backend.cpp` | `grep -r "docker " src/common/env/` 应仅返回注释 |
| ✅ **cmd + args 数组分离 + `execve()` 逐参传递** | `local_backend.cpp:149` | `tests/test_backend_security.cpp` 4 case |
| ✅ **env 白名单：不继承 parent env** | `local_backend.cpp:104` | `tests/test_local_backend.cpp::env_whitelist_does_not_inherit_parent_env` |
| ✅ **Privileged mode 禁用** | `docker_backend.cpp:94`（exec 入口前置检查） | `tests/test_docker_backend.cpp::privileged_mode_request_rejected_with_security_violation` |
| ✅ **容器镜像 digest 锁定** | `docker_backend.cpp` (image spec 透传 `@sha256:...`) | `tests/test_docker_backend.cpp::image_digest_lock_passed_through_to_create_body` |

### 3.2 推荐约束

| 检查项 | 理由 | 实施方式 |
|--------|------|----------|
| 启用 `BackendConfig::docker_unavailable_policy = FailFast` | 避免 docker daemon 故障静默降级到 local | `with_defaults()` 默认即 FailFast |
| 设置 `BackendPolicy.image_allowlist` 限定白名单 | 防 LLM 生成恶意镜像名（如 `malicious:latest`） | `override_default_policy("docker:*", { image_allowlist = {...} })` |
| 注册 `EnvValidationHook` 至 `*` tool pattern | 防止 hook 旁路（per ADR-0075 §不变量 6） | `hooks.register_pre_hook("*", make_env_validation_hook(config), 0, FailClosed)` |
| 订阅 `env.backend.exec.end` 事件审计 | 实时告警 `ERR_BACKEND_SECURITY_VIOLATION` 等异常 | 详见 §五 |
| 启用 `BackendCapabilities.max_concurrent_execs` 限流 | 防 backend 过载 DoS | `LocalBackend::capabilities()` 返回 `max_concurrent_execs=16` |

### 3.3 OWASP Top 10 防御映射

| OWASP A03:2021 — Injection | EnvBackend 防御 |
|-----------------------------|-------------------|
| 命令注入（OS Command Injection） | `cmd + args` 数组分离 + `execve()` 逐参 |
| 环境变量注入 | env 白名单仅透传显式声明 |
| Path 遍历 | `working_dir` 前缀白名单 |
| Privilege Escalation | DockerBackend `Privileged=false` 强制 + `RLIMIT_*` |
| Container Breakout | 容器隔离 + 镜像 digest 锁定 + image_allowlist |

## 四、BackendPolicy 决策流程图

```
shell.exec 节点 (DSL layer)
    │
    ▼
[ ToolCoordinator.execute 入口 ]
    │
    ▼
[ EnvValidationHook pre-hook ]
    │
    ├─[ 1 ]→ backend_spec 命中 BackendPolicy::find_policy()?
    │         NO  → deny("unknown backend: <spec>")
    │         YES ↓
    │
    ├─[ 2 ]→ docker: image 在 image_allowlist (非空时)?
    │         NO  → deny("image not in allowlist: <image>")
    │         YES / 非 docker ↓
    │
    ├─[ 3 ]→ args["env.<NAME>"] 都在 allowed_env_vars (含 "*" 通配)?
    │         NO  → deny("env var not allowed: <NAME>")
    │         YES ↓
    │
    ├─[ 4 ]→ args["working_dir"] 前缀匹配 allowed_paths (含 "*")?
    │         NO  → deny("working_dir not allowed: <path>")
    │         YES / 无 working_dir ↓
    │
    ├─[ 5 ]→ requires_approval=true 需 args["__approved"]="true"?
    │         NO  → deny("Backend policy requires approval")
    │         YES / 不需审批 ↓
    │
    ▼
[ ApprovalHandler ]
    │
    ▼
[ registry.call_tool → LocalBackend.exec / DockerBackend.exec ]
    │
    ▼
[ emit env.backend.exec.start/end events (cmd_hash 脱敏) ]
```

**Hook 旁路检测**：若代码修改绕过 EnvValidationHook（如直接在 NodeExecutor 调用 backend 而不经过 ToolCoordinator），code review 阻断 + grep CI 检查 `ToolCoordinator.execute` 内部 hook 调用顺序。

## 五、Audit 事件订阅

### 5.1 `env.backend.exec.start` (per ADR-0068 §5.11)

```json
{
  "topic": "env.backend.exec.start",
  "data": {
    "backend_spec": "local",                       // 或 "docker:python:3.12"
    "cmd_hash": "sha256:abc123..."                 // SHA256(req.cmd) 64 字符 hex
    // 故意 NOT 记录: cmd, args, env (防 secret 泄漏)
  },
  "meta": {
    "trace_id": "trace-2026-08-18-...",
    "timestamp": "2026-08-18T17:30:00Z"
  }
}
```

### 5.2 `env.backend.exec.end`

```json
{
  "topic": "env.backend.exec.end",
  "data": {
    "backend_spec": "local",
    "cmd_hash": "sha256:abc123...",
    "exit_code": 0,
    "timed_out": false,
    "error_code": "SUCCESS"                        // 或 "ERR_BACKEND_TIMEOUT" / "ERR_OUTPUT_TOO_LARGE" / ...
  },
  "meta": {
    "trace_id": "trace-2026-08-18-...",
    "timestamp": "2026-08-18T17:30:01Z",
    "latency_ms": 1234
  }
}
```

### 5.3 实时告警推荐规则

| 触发条件 | 推荐响应 |
|----------|----------|
| `error_code == "ERR_BACKEND_SECURITY_VIOLATION"` | 🚨 立即告警 → 可能 LLM 提权攻击或配置错误 |
| `error_code == "ERR_BACKEND_UNAVAILABLE"` | ⚠️ 警告 → docker daemon 故障，检查 `docker_unavailable_policy` 配置 |
| `error_code == "ERR_BACKEND_TIMEOUT"` 高频出现 | 🔍 检查 agent 工作流是否有死循环 / 后端资源不足 |
| `error_code == "ERR_OUTPUT_TOO_LARGE"` 高频出现 | 🔍 检查是否有 LLM 生成无限输出，max_output_bytes 阈值是否需调整 |
| 同一 `cmd_hash` 在 1 分钟内多次失败 | 🚨 告警 → 可能 retry storm，需限流 |

## 六、迁移与回滚

### 6.1 渐进迁移步骤（推荐）

| 阶段 | 操作 | 验证 |
|------|------|------|
| 1 | 部署默认 `BackendConfig::with_defaults()` + 注册 `EnvValidationHook` | 现有 ctest 134/135 通过（不注入新行为） |
| 2 | 在 staging 启用 `docker_unavailable_policy=FallbackToLocal` | staging E2E 跑通 |
| 3 | 在 prod 启用 `docker_unavailable_policy=FailFast` + 严格 image_allowlist | prod smoke test 通过 |
| 4 | 订阅 `env.backend.exec.end` 事件接入 SIEM | 1 周观察期，无异常 |

### 6.2 回滚策略

C11/C12/C13 各自独立 commit，可逐个 revert。revert 后 baseline tools 仍 100% 兼容（env_backend 是可选 backend 选择，default 行为不变）。

### 6.3 紧急降级

若新发现安全漏洞，可通过 1 行配置回退到保守：

```cpp
config.docker_unavailable_policy = DockerUnavailablePolicy::FailFast;
config.local_policy.requires_approval = true;
config.local_policy.allowed_env_vars = {"PATH"};  // 仅 PATH
config.local_policy.allowed_paths = {"/tmp"};
```

## 八、参考

- [ADR-0075 — EnvBackend 多环境执行](../adr/adr-0075-env-backend-local-docker.md) — 本指南的主 ADR
- [ADR-0004 — ToolRegistry 安全模型](../adr/adr-0004-toolregistry-security.md) — ToolCategory × ApprovalPolicy 矩阵
- [ADR-0068 — 事件发射契约](../adr/adr-0068-event-emission-contract.md) — args-only-keys 政策 (event.data 仅含 backend_spec + cmd_hash)
- [ADR-0069 — ToolCoordinator Hooks](../adr/adr-0069-tool-coordinator-hooks.md) — pre-hook 注入
- [docs/specs/env-backend.md §六 EnvValidationHook 集成](../specs/env-backend.md) — 工厂函数 + 注册方式
- [OWASP A03:2021 — Injection](https://owasp.org/Top10/A03_2021-Injection/) — 防御映射
- [CIS Docker Benchmark](https://www.cisecurity.org/benchmark/docker) — Container Breakout 防御基线