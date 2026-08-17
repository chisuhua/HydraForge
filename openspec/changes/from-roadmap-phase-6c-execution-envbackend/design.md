## Context

ADR-0075「EnvBackend 多环境执行（Local + Docker）」为 AgenticDSL Execution Plane 提供隔离执行抽象，本提案覆盖 ADR-0075 D1+D2+D3+D5 的实施层落地（D4 `backend:` DSL 字段解析由独立 W5 提案覆盖，与本提案为前置依赖关系）：

- **D1 IEnvBackend 抽象接口**：`include/agenticdsl/env/env_backend.h` 定义 `IEnvBackend` 接口 + `ExecRequest`（含 `cmd + args` 强制分离防 shell 注入）/`ExecOptions`（`timeout_ms` + `max_output_bytes` + `capture_stderr`）/`ExecResult`（含 `exit_code` / `stdout_buf` / `stderr_buf` / `timed_out` / `error_code`）/`BackendCapabilities`（`supports_isolation` / `supports_persistent_fs` / `max_concurrent_execs`）四个值类型 + `create_backend(backend_spec, config)` 工厂。
- **D2 LocalBackend**（C11）：fork + execve POSIX 子进程隔离 + 超时控制（`waitpid` + `SIGTERM`→`SIGKILL` grace period）+ 输出截断（默认 64KB，可配 `max_output_bytes`）+ 资源限制（`setrlimit(RLIMIT_AS)` + `RLIMIT_CPU` 防 fork bomb）+ 环境变量白名单（仅透传 `ExecOptions.env` 显式声明，不继承 parent env，per ADR-0075 §不变量 4）+ audit 事件 `env.backend.exec.start/end` 触发。
- **D3 DockerBackend**（C12）：libcurl + Docker REST API（已 vendor 零额外依赖），双模式：(a) exec into existing container（`POST /containers/{id}/exec`）— 适用 dev/test；(b) create + run ephemeral container（`POST /containers/create` + `/containers/{id}/start` + `/containers/{id}/wait` + 删除）— 适用 CI/sandbox。安全约束：❌ privileged mode 禁用 + ❌ host 根目录挂载禁用 + ✅ tmpfs mount + ✅ network namespace 默认 bridge。
- **D5 EnvValidationHook**（C13）：ToolCoordinator pre-hook 集成（per ADR-0069 hook 体系），对 `ToolCategory::Dangerous` 工具强制 backend policy 检查：(1) `requires_approval` 检查 + (2) `env_vars` 白名单 + (3) `working_dir` 白名单 + (4) 输出 `ToolCallResult::deny(reason)` 三层 deny 路径。

依赖链（per roadmap.md line 280-282）：
```
W5 (backend: 字段 DSL 解析) ──→ C11 (D2 LocalBackend, 8h)
                                ├──→ C12 (D3 DockerBackend, 8h)
                                └──→ C13 (D5 EnvValidationHook + BackendPolicy, 6h)
```

总 **22h P0**——Phase 6c **单一类别最大**（远超 C9 schema-complete 6h + C4 evidence-gate 4h + C10 control-plane-eval 2h），载明容量压力点。

承接关系：(1) ADR-0075 状态当前 🔍 Proposed（2026-08-03 立项），本提案 ship 后翻牌 ✅ Approved（D1+D2+D3+D5 全 ship）；(2) ADR-0069 ToolCoordinator Hooks 基础设施需先 ship（Phase 6a/6b Wave 1 第 4 项，本提案不重做）；(3) ADR-0055 SkillInterpreter posix_spawn 模式作安全参考（env 白名单 + stdio 重定向 + FD cleanup），本提案 fork+execve 借鉴其安全约束而非 API。

**fork vs posix_spawn 说明**：ADR-0075 §决策 D2 明确选择 `fork() + execve()` 而非 `posix_spawn`（line 145-176），父进程需 `waitpid(timeout)` + `kill(SIGKILL)` 实施超时控制。本提案尊重 ADR-0075 设计选择，安全约束全部借鉴 ADR-0055 模式（FD cleanup + env whitelist + stdio 重定向）。

## Goals / Non-Goals

**Goals:**

1. **C11 LocalBackend ship**：`include/agenticdsl/env/local_backend.h` + `src/common/env/local_backend.cpp`（fork + execve + pipe 双向 stdio 重定向 + `waitpid` + SIGTERM→SIGKILL grace period + 输出截断 + `setrlimit` 资源限制 + env 白名单）；错误码表 `ERR_BACKEND_FORK_FAILED` / `ERR_BACKEND_COMMAND_NOT_FOUND` / `ERR_BACKEND_TIMEOUT` / `ERR_OUTPUT_TOO_LARGE`（per ADR-0075 §决策 D2 表）。
2. **C12 DockerBackend ship**：`include/agenticdsl/env/docker_backend.h` + `src/common/env/docker_backend.cpp`（libcurl HTTP client + Unix socket `/var/run/docker.sock` + exec into existing container + ephemeral container 生命周期 + `HostConfig.Memory` + `NanoCpus` 资源限制 + privileged mode 拒绝）。
3. **C13 EnvValidationHook + BackendPolicy ship**：`include/agenticdsl/policy/backend_policy.h` + `src/common/policy/backend_policy.cpp`（per-backend `requires_approval` / `allowed_env_vars` / `allowed_paths` / `allow_network` / `max_memory_mb` / `max_cpu_cores`）+ `src/common/hooks/env_validation_hook.cpp`（`IToolHook::pre()` 实现，per ADR-0069 hook 体系接入 `ToolCoordinator.execute` 入口）+ 默认策略表。
4. **Backend 工厂**：`src/common/env/backend_factory.cpp` `create_backend(backend_spec, config)` — 解析 `"local"` / `"docker:<container_id>"` / `"docker:<image>:<tag>"` 三种 spec，工厂模式为后续 K8sBackend/SSHBackend 预留扩展点（per ADR-0075 §不变量 1）。
5. **测试覆盖**：`tests/test_local_backend.cpp` 6 case + `tests/test_docker_backend.cpp` 5 case + `tests/test_backend_factory.cpp` 3 case + `tests/test_backend_policy.cpp` 4 case + `tests/test_env_validation_hook.cpp` 6 case + `tests/test_backend_security.cpp` 3 case，共 ≥27 case。
6. **文档**：`docs/specs/env-backend.md`（新增，IEnvBackend 接口契约 + 2 个 backend 实现规范 + 与 ADR-0075 §决策 D1 line 96 1MB 上限的 64KB 截断差异说明）+ `docs/security/backend-policy.md`（新增，默认策略 + per-environment 配置）。
7. **架构合规性 + 零回归**：`ctest --output-on-failure` 全量零回归（147/147 baseline + ≥27 新增 case 全 PASS）。
8. **ADR 状态翻牌**：ADR-0075 🔍 Proposed → ✅ Approved（D1+D2+D3+D5 全 ship）。
9. **ADR-0003 线程安全保留**：LocalBackend/DockerBackend 调用点验证 `DSLEngine::run` 线程模型下 backend 实例不可变（`const` 接口 + `backend_factory` 返回 `shared_ptr<const IEnvBackend>`）。

**Non-Goals:**

- **W5 `backend:` DSL 字段解析**（独立前置提案；2026-08-17 评估 W5 仍在 Phase 6b carry-over 队列，可能延后 C11-C13 启动）。
- **D4 重命名 `env:` → `env_vars:` 别名兼容**（与 W5 同源处理；DSL parser 接受 `env:` 作为 `env_vars:` 别名 1 个 Sprint 后删除）。
- **K8sBackend**（per ADR-0075 §不变量 7 + §后续 长期 12，Phase 7+ 估时 2-3 周）。
- **SSHBackend**（per ADR-0075 §后续 长期 13，Phase 7+ 估时 1-2 周）。
- **Docker 镜像构建 pipeline**（D3 仅消费既有镜像；CI/CD 镜像 registry 集成由独立提案覆盖）。
- **Network policy enforcement**（i.e., `--network=none` 强制 / egress allowlist，超出 ADR-0075 §决策 D3 范围）。
- **Secrets management**（env var 含 secret 的脱敏/注入策略由 ADR-0068 §5.11 args-only-keys 政策间接覆盖）。
- **Backend warm pool**（per ADR-0075 §后续 长期 14，Phase 8+ 评估；当前每次 exec cold start）。

## Decisions

### D-1. IEnvBackend 抽象接口 + 4 个值类型

**决策**: `IEnvBackend` 抽象接口定义 `exec(const ExecRequest&, const ExecOptions&) -> ExecResult` + `capabilities() -> BackendCapabilities` 两个虚函数；4 个值类型 `ExecRequest` / `ExecOptions` / `ExecResult` / `BackendCapabilities` 全部 POD-like struct。

**理由**: 单一执行入口 + 能力查询入口便于 BackendPolicy 根据 capabilities 字段决策；POD 值类型便于序列化与跨 backend 切换。

### D-2. LocalBackend 使用 fork + execve（per ADR-0075 §决策 D2）

**决策**: LocalBackend 实施 `fork() + execve()` POSIX 接口，命令注入防御采用 `cmd + args` 数组分离 + `execve()` 逐参数传递（**不使用** `system()` / `popen()` 的 shell 解析路径，防 OWASP shell injection）。

**理由**: ADR-0075 §决策 D2 line 145-176 明确选择；父进程需 `waitpid(timeout)` 实施超时控制（`posix_spawn` 在 Linux 上底层仍 fork+execve，但 `waitpid` 超时语义不变）。安全约束全部借鉴 ADR-0055 SkillInterpreter 模式（FD cleanup + env whitelist + stdio pipe 重定向）。

### D-3. DockerBackend 使用 libcurl + Docker REST API

**决策**: DockerBackend 实施 libcurl HTTP client，通过 Unix socket `/var/run/docker.sock` 调用 Docker REST API（`POST /containers/create` + `POST /containers/{id}/exec` + `POST /exec/{id}/start` + `GET /exec/{id}/inspect`）；**禁止** 通过 `docker` CLI 调用。

**理由**: libcurl 已 vendor（无新增依赖）；Docker REST API 避免 shell injection 风险 + 进程管理复杂度 + docker.sock 权限泄漏风险。

### D-4. 输出截断默认 64KB（与 ADR-0075 D1 line 96 的 1MB 不一致）

**决策**: 输出截断在 `max_output_bytes` 阈值**默认 64KB**（与 ADR-0075 §决策 D1 line 96 的 1MB 上限不一致）。本提案降低到 64KB 出于 NodeExecutor 上下文大小约束。

**理由**: 1MB 输出会撑爆 NodeExecutor 上下文；64KB 是 LLM 工具调用的合理上限（超出应 truncate + 警告用户）。差异需在 `docs/specs/env-backend.md` 文档化说明。

### D-5. BackendPolicy 默认策略表 3 档

**决策**: 默认 BackendPolicy 覆盖 3 档：
- `local` → `requires_approval=true` + `allowed_env_vars={PATH, HOME, USER, LANG}` + `allowed_paths={/tmp, $HOME}` + `allow_network=false` + `max_memory_mb=1024` + `max_cpu_cores=1`
- `docker:*` ephemeral → `requires_approval=false` + `allowed_env_vars=*` + `allowed_paths=*` + `allow_network=true` + `max_memory_mb=512` + `max_cpu_cores=2`
- `docker:prod` named → `requires_approval=true` + `allowed_env_vars=per-deployment` + `allowed_paths=per-deployment`

**理由**: 默认策略覆盖 3 类使用场景；per-environment override 由 BackendConfig 文件提供（`BackendConfig::override_default_policy(backend_spec, BackendPolicy)`）。

### D-6. EnvValidationHook 在 ToolCoordinator 入口强制触发

**决策**: EnvValidationHook pre-hook 在 `ToolCoordinator.execute` 入口**强制**触发（per ADR-0075 §不变量 6 + ADR-0069 §决策 D3 hook 顺序），包括 legacy V2 工具与 plugin 加载工具；hook 旁路视为安全违规，code review 阻断。

**理由**: 安全路径不能因工具类型而失效；强制触发 + code review 阻断是行业惯例。

### D-7. DockerBackend 禁止 Privileged Mode

**决策**: DockerBackend **禁止** 启用 `Privileged: true`（per ADR-0075 §决策 D3 安全约束）；CI 测试覆盖拒绝路径（`Privileged=true` request → 返回 `ERR_BACKEND_SECURITY_VIOLATION`）。

**理由**: Privileged mode 等同 root 权限，破坏容器隔离承诺。

### D-8. audit event 脱敏（per ADR-0068 §5.11）

**决策**: `env.backend.exec.start/end` 事件携带 `backend_spec` + `cmd_hash`（SHA256 防 raw command 泄漏，per ADR-0068 §5.11 args-only-keys 政策），**不记录** raw args。

**理由**: trace log 不可包含用户 secret；SHA256 哈希提供审计可追溯性但不暴露敏感内容。

### D-9. backend_factory 返回 `shared_ptr<const IEnvBackend>`

**决策**: `create_backend(backend_spec, config)` 返回 `shared_ptr<const IEnvBackend>`，backend 实例不可变，多线程并发 exec 走 `BackendCapabilities.max_concurrent_execs` 排队，不引入 data race。

**理由**: ADR-0003 §决策 4 强制 DSLEngine 线程模型下 backend 不可变；`const` 接口 + shared_ptr 提供线程安全保证 + 共享复用。

## Risks / Trade-offs

- **[Risk: fork+execve 子进程泄漏]** → Mitigation: RAII 子进程包装 + `waitpid` 强制 + SIGKILL 兜底 + `tests/test_local_backend.cpp::fork_resource_cleanup` 测试。
- **[Risk: Docker daemon 不可达导致 silent failure]** → Mitigation: `BackendConfig::docker_unavailable_policy` 可配置（`fail_fast` 默认保守 / `fallback_to_local` 显式 opt-in），不静默。
- **[Risk: 输出截断阈值 64KB 与 ADR-0075 D1 1MB 不一致]** → Mitigation: `docs/specs/env-backend.md` 文档化差异 + `tests/test_local_backend.cpp::output_truncate_64kb` 测试明确验证。
- **[Risk: BackendPolicy bypass]** → Mitigation: EnvValidationHook pre-hook 强制触发 + code review checklist 验证 + grep CI 检查 hook 顺序。
- **[Risk: 22h P0 容量压力]** → Mitigation: 任务拆分（C11/C12/C13 独立 ship + 互不依赖），允许 W5 提案延后时仍 ship C11-C13（不依赖 W5 解析层）。
- **[Risk: DockerBackend libcurl 集成复杂度]** → Mitigation: libcurl 已 vendor（无新增依赖），参考 `tests/test_http_adapter.cpp` 已有 HTTP client 模式 + httplib mock server 复用。
- **[Risk: 容器镜像 digest 漂移]** → Mitigation: DockerBackend 使用容器镜像 digest 锁定（`docker:python:3.12@sha256:abc...`），baseline 测量强制 digest 一致。
- **[Risk: 环境变量白名单误过滤必要变量]** → Mitigation: 默认策略表覆盖 3 档使用场景；per-environment override 灵活配置；CI 测试覆盖 PATH/HOME/USER/LANG 4 必备变量。
- **[Risk: ADR-0075 D3 实施效果不佳]** → Mitigation: proposal 已明确 descope 路径（D4 重命名 + K8sBackend + Docker 镜像构建均 follow-up）。

## Migration Plan

1. ADR-0069 ToolCoordinator Hooks 基础设施必须先 ship（Phase 6a/6b Wave 1 第 4 项）。
2. ADR-0055 SkillInterpreter posix_spawn 模式作安全参考（已 ship 2026-07-22）。
3. 本提案独立 ship C11/C12/C13（不依赖 W5 解析层，W5 延后不影响）。
4. CI 阶段验证所有现有 tools 仍正常执行（baseline 147/147 ctest 不变 + ≥27 新增 case PASS）。
5. ADR-0075 状态翻牌 🔍 Proposed → ✅ Approved（D1+D2+D3+D5 全 ship）。
6. ship 后 `docs/active-status.md` §一 Phase 6c C11-C13 行标记 ✅ ship + ADR-0075 状态行更新。

回滚策略：C11/C12/C13 各自独立 commit，可逐个 revert。revert 后 baseline tools 仍 100% 兼容（env_backend 是可选 backend 选择，default 行为不变）。

## Open Questions

1. K8sBackend 与 SSHBackend 实施时机（per ADR-0075 §后续 长期 12/13）？当前留 Phase 7+。
2. Docker 镜像构建 pipeline 集成？当前 D3 仅消费既有镜像，CI/CD 镜像 registry 由独立提案覆盖。
3. Network policy enforcement（`--network=none` 强制 / egress allowlist）？当前超出 ADR-0075 §决策 D3 范围，留 follow-up。
4. Backend warm pool？当前每次 exec cold start，Phase 8+ 评估。
5. BackendConfig override 配置语法（YAML vs TOML vs JSON）？当前方案留 Phase 7+ 决议。
