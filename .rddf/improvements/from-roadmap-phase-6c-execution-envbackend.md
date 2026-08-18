# from-roadmap-phase-6c-execution-envbackend

**优先级**: P0 | **来源**: from-roadmap (phase-6c/execution-envbackend, ADR-0075 D2/D3/D5)
**阶段**: phase-6c | **分类**: execution-envbackend
**类型**: functional
**主题**: LocalBackend；DockerBackend；EnvValidationHook

## 架构依据

ADR-0075「EnvBackend 多环境执行（Local + Docker）」为 AgenticDSL Execution Plane 提供隔离执行抽象，本提案覆盖 ADR-0075 D1+D2+D3+D5 的实施层落地（D4 `backend:` DSL 字段解析由独立 W5 提案覆盖，与本提案为前置依赖关系）：

- **D1 IEnvBackend 抽象接口**：`include/agenticdsl/env/env_backend.h` 定义 `IEnvBackend` 接口 + `ExecRequest`（含 `cmd + args` 强制分离防 shell 注入）/`ExecOptions`（`timeout_ms` + `max_output_bytes` + `capture_stderr`）/`ExecResult`（含 `exit_code` / `stdout_buf` / `stderr_buf` / `timed_out` / `error_code`）/`BackendCapabilities`（`supports_isolation` / `supports_persistent_fs` / `max_concurrent_execs`）四个值类型 + `create_backend(backend_spec, config)` 工厂。
- **D2 LocalBackend**（C11）：fork + execve POSIX 子进程隔离 + 超时控制（`waitpid` + `SIGTERM`→`SIGKILL` grace period）+ 输出截断（默认 64KB，可配 `max_output_bytes`）+ 资源限制（`setrlimit(RLIMIT_AS)` + `RLIMIT_CPU` 防 fork bomb）+ 环境变量白名单（仅透传 `ExecOptions.env` 显式声明，不继承 parent env，per ADR-0075 §不变量 4）+ audit 事件 `env.backend.exec.start/end` 触发。
- **D3 DockerBackend**（C12）：libcurl + Docker REST API（已 vendor 零额外依赖），双模式：(a) exec into existing container（`POST /containers/{id}/exec`）— 适用 dev/test；(b) create + run ephemeral container（`POST /containers/create` + `/containers/{id}/start` + `/containers/{id}/wait` + 删除）— 适用 CI/sandbox。安全约束：❌ privileged mode 禁用 + ❌ host 根目录挂载禁用 + ✅ tmpfs mount + ✅ network namespace 默认 bridge。
- **D5 EnvValidationHook**（C13）：ToolCoordinator pre-hook 集成（per ADR-0069 hook 体系），对 `ToolCategory::Dangerous` 工具强制 backend policy 检查：(1) `requires_approval` 检查 + (2) `env_vars` 白名单 + (3) `working_dir` 白名单 + (4) 输出 `ToolCallResult::deny(reason)` 三层 deny 路径。`BackendPolicy` 按 backend 配置（`local`/`docker:*` ephemeral/`docker:prod` named），per-environment 可覆盖。

**依赖链**（per roadmap.md line 280-282）：

```
W5 (backend: 字段 DSL 解析) ──→ C11 (D2 LocalBackend, 8h)
                                ├──→ C12 (D3 DockerBackend, 8h)
                                └──→ C13 (D5 EnvValidationHook + BackendPolicy, 6h)
```

总 **22h P0**——Phase 6c **单一类别最大**（远超 C9 schema-complete 6h + C4 evidence-gate 4h + C10 control-plane-eval 2h），载明容量压力点。本提案**不**覆盖 W5 `backend:` DSL 字段解析（独立前置提案；2026-08-17 评估 W5 仍在 Phase 6b carry-over 队列，可能延后 C11-C13 启动），亦**不**覆盖 D4 重命名（`env:` → `env_vars:` 别名兼容，与 W5 同源处理）。

**承接关系**：(1) ADR-0075 状态当前 🔍 Proposed（2026-08-03 立项），本提案 ship 后翻牌 🟡 → ✅ Approved（D1+D2+D3+D5 全 ship，per ADR-0075 §复审节点 line 472-477 的"Wave 3 末"节点提前到 Phase 6c 收官）；(2) ADR-0069 ToolCoordinator Hooks 基础设施需先 ship（Phase 6a/6b Wave 1 第 4 项，本提案不重做）；(3) ADR-0055 SkillInterpreter posix_spawn 模式作安全参考（env 白名单 + stdio 重定向 + FD cleanup），本提案 fork+execve 借鉴其安全约束而非 API（per ADR-0075 §决策 D2 设计选择）。

**fork vs posix_spawn 说明**：ADR-0075 §决策 D2 明确选择 `fork() + execve()` 而非 `posix_spawn`（line 145-176），父进程需 `waitpid(timeout)` + `kill(SIGKILL)` 实施超时控制（`posix_spawn` 在 Linux 上底层仍 fork+execve，但 `waitpid` 超时语义不变）。本提案尊重 ADR-0075 设计选择，安全约束全部借鉴 ADR-0055 模式（FD cleanup + env whitelist + stdio 重定向）。

## 范围

- **In Scope**:
  - **C11 LocalBackend ship**：`include/agenticdsl/env/local_backend.h` + `src/common/env/local_backend.cpp`（fork + execve + pipe 双向 stdio 重定向 + `waitpid` + SIGTERM→SIGKILL grace period + 输出截断 + `setrlimit` 资源限制 + env 白名单）；错误码表 `ERR_BACKEND_FORK_FAILED` / `ERR_BACKEND_COMMAND_NOT_FOUND` / `ERR_BACKEND_TIMEOUT` / `ERR_OUTPUT_TOO_LARGE`（per ADR-0075 §决策 D2 表）；`tests/test_local_backend.cpp` 6 case（fork happy / execve ENOENT / 超时 SIGKILL / 输出截断 / env 白名单不继承 parent / RLIMIT_CPU）。
  - **C12 DockerBackend ship**：`include/agenticdsl/env/docker_backend.h` + `src/common/env/docker_backend.cpp`（libcurl HTTP client + Unix socket `/var/run/docker.sock` + exec into existing container + ephemeral container 生命周期 + `HostConfig.Memory` + `NanoCpus` 资源限制 + privileged mode 拒绝）；`tests/test_docker_backend.cpp` 5 case（ephemeral container create+run+delete / exec into existing / privileged mode 拒绝 / 资源限制生效 / daemon unreachable 错误传播）。
  - **C13 EnvValidationHook + BackendPolicy ship**：`include/agenticdsl/policy/backend_policy.h` + `src/common/policy/backend_policy.cpp`（per-backend `requires_approval` / `allowed_env_vars` / `allowed_paths` / `allow_network` / `max_memory_mb` / `max_cpu_cores`）+ `src/common/hooks/env_validation_hook.cpp`（`IToolHook::pre()` 实现，per ADR-0069 hook 体系接入 `ToolCoordinator.execute` 入口）+ 默认策略表（`local` 审批+PATH/HOME/USER/LANG env；`docker:*` ephemeral 不审批+`*` env；`docker:prod` 审批+per-deployment env）。
  - **Backend 工厂**：`src/common/env/backend_factory.cpp` `create_backend(backend_spec, config)` — 解析 `"local"` / `"docker:<container_id>"` / `"docker:<image>:<tag>"` 三种 spec，工厂模式为后续 K8sBackend/SSHBackend 预留扩展点（per ADR-0075 §不变量 1）。
  - **测试**：`tests/test_backend_factory.cpp` + `tests/test_backend_policy.cpp` + `tests/test_env_validation_hook.cpp` + `tests/test_backend_security.cpp`（shell 注入 OWASP + privileged mode 拒绝）。
  - **文档**：`docs/specs/env-backend.md`（新增，IEnvBackend 接口契约 + 2 个 backend 实现规范）+ `docs/security/backend-policy.md`（新增，默认策略 + per-environment 配置）。
- **Out of Scope**:
  - **W5 `backend:` DSL 字段解析**（独立前置提案；2026-08-17 评估 W5 仍在 Phase 6b carry-over 队列，可能延后 C11-C13 启动）。
  - **D4 重命名 `env:` → `env_vars:` 别名兼容**（与 W5 同源处理；DSL parser 接受 `env:` 作为 `env_vars:` 别名 1 个 Sprint 后删除）。
  - **K8sBackend**（per ADR-0075 §不变量 7 + §后续 长期 12，Phase 7+ 估时 2-3 周）。
  - **SSHBackend**（per ADR-0075 §后续 长期 13，Phase 7+ 估时 1-2 周）。
  - **Docker 镜像构建 pipeline**（D3 仅消费既有镜像；CI/CD 镜像 registry 集成由独立提案覆盖）。
  - **Network policy enforcement**（i.e., `--network=none` 强制 / egress allowlist，超出 ADR-0075 §决策 D3 范围）。
  - **Secrets management**（env var 含 secret 的脱敏/注入策略由 ADR-0068 §5.11 args-only-keys 政策间接覆盖，本提案不重做）。
  - **Backend warm pool**（per ADR-0075 §后续 长期 14，Phase 8+ 评估；当前每次 exec cold start）。

## 关键场景

- GIVEN DSL 节点 `shell.exec` 带 `backend: local` 且 `cmd: "ls -la /tmp"` + `args: ["-la", "/tmp"]`
  WHEN ToolCoordinator.execute 派发（经 EnvValidationHook pre-hook 验证 backend=`local` 命中默认 BackendPolicy）
  THEN LocalBackend.fork() → 子进程 execve("/bin/ls", ["ls", "-la", "/tmp"], env_white_list_only) → pipe 双向捕获 stdout/stderr → waitpid(30s timeout) → 输出截断到 64KB → 返回 `ExecResult{exit_code=0, stdout_buf, stderr_buf, duration, error_code=null}` + 触发 `env.backend.exec.start/end` audit 事件。

- GIVEN DSL 节点 `shell.exec` 带 `backend: docker:python:3.12` 且 `cmd: "pytest"`
  WHEN EnvValidationHook 验证通过（ephemeral docker 默认策略 `requires_approval=false` + `max_memory_mb=512` + `max_cpu_cores=2`）
  THEN DockerBackend 创建 ephemeral container（`HostConfig.Memory=512m` + `NanoCpus=2000000000` + 拒绝 `Privileged=true`）→ exec `pytest` → wait exit → delete container → 返回结果；过程中如 daemon unreachable，返回 `ExecResult{error_code="ERR_BACKEND_UNAVAILABLE"}` 不静默。

- GIVEN 任意 tool request 进入 ToolCoordinator.execute，EnvValidationHook 在 layer check 之后、ApprovalHandler 之前 pre-hook 触发（per ADR-0069 §决策 D3 hook 顺序）
  WHEN hook 检查工具 `ToolCategory::Dangerous`（如 `shell.exec`）的 `backend` 字段 + `env_vars` + `working_dir` 三者
  THEN 任一违反 `BackendPolicy` 立即短路：env var 不在白名单 → `ToolCallResult::deny("env var not allowed: " + k)`；working_dir 不在 `allowed_paths` → `ToolCallResult::deny("working_dir not allowed: " + path)`；backend `requires_approval=true` 且未审批 → `ToolCallResult::deny("Backend policy requires approval")`，三层 deny 路径均发 `tool.audit.denied` 事件（per ADR-0068，metadata 含 reason + matched_pattern 脱敏）。

- GIVEN LocalBackend 进程运行时间超过 `timeout_ms`（默认 30s）
  WHEN `waitpid(timeout)` 返回 + 子进程仍存活
  THEN LocalBackend 升级到 `kill(SIGTERM)`（grace period 5s）→ 子进程仍存活 → 升级 `kill(SIGKILL)`（强杀）；返回 `ExecResult{exit_code=-1, timed_out=true, error_code="ERR_BACKEND_TIMEOUT"}` + 输出截断当前 pipe 内容 + 触发 `env.backend.exec.end` 事件 metadata 含 `timed_out=true`。

- GIVEN DockerBackend 调用时 Docker daemon 不可用（socket 不存在 / 403 Forbidden / 500 错误）
  WHEN backend 工厂 `create_backend("docker:...")` 探测 daemon 可达性失败
  THEN **fallback 策略可配置**（`BackendConfig::docker_unavailable_policy`）：`fallback_to_local` → 记录 warning + 回退 LocalBackend 执行；`fail_fast` → 返回 `ExecResult{error_code="ERR_BACKEND_UNAVAILABLE"}`；默认 `fail_fast`（保守，避免 silent degradation，per ADR-0075 §风险 中风险 第 1 项缓解"不静默失败"原则）。

## 技术约束

- **MUST** LocalBackend 使用 `fork() + execve()` POSIX 接口（per ADR-0075 §决策 D2 line 145-176 实施要点），命令注入防御必须采用 `cmd + args` 数组分离 + `execve()` 逐参数传递（不使用 `system()` / `popen()` 的 shell 解析路径，防 OWASP shell injection）；借鉴 ADR-0055 SkillInterpreter 安全模式（FD cleanup + env 白名单 + stdio pipe 重定向），但 API 形态遵循 ADR-0075 D2（fork+execve 而非 posix_spawn，因父进程需 `waitpid(timeout)` 实施超时控制）。
- **MUST** DockerBackend 使用 libcurl HTTP client（项目已 vendor，无新增依赖），通过 Unix socket `/var/run/docker.sock` 调用 Docker REST API（`POST /containers/create` + `POST /containers/{id}/exec` + `POST /exec/{id}/start` + `GET /exec/{id}/inspect`）；**禁止** 通过 `docker` CLI 调用（避免 shell injection + 进程管理复杂度 + docker.sock 权限泄漏风险）。
- **MUST** 输出截断在 `max_output_bytes` 阈值（**默认 64KB**，与 ADR-0075 §决策 D1 line 96 的 1MB 上限不一致；本提案降低到 64KB 出于 NodeExecutor 上下文大小约束，差异需在 `docs/specs/env-backend.md` 文档化说明）；超限返回 `error_code = "ERR_OUTPUT_TOO_LARGE"` + 截断当前 buffer + stderr warning（防 OOM）。
- **MUST** BackendPolicy 按 backend spec + layer 提供 per-tool allowlist 配置（`allowed_env_vars` + `allowed_paths` + `requires_approval`），默认策略表覆盖 `local` / `docker:*` ephemeral / `docker:prod` named 三档，per-environment 可覆盖。
- **MUST NOT** 允许 backend 选择绕过 EnvValidationHook——pre-hook 在 ToolCoordinator.execute 入口**强制**触发（per ADR-0075 §不变量 6 + ADR-0069 §决策 D3 hook 顺序），包括 legacy V2 工具与 plugin 加载工具；hook 旁路视为安全违规，code review 阻断。
- **MUST NOT** DockerBackend 启用 `Privileged: true`（per ADR-0075 §决策 D3 安全约束）；CI 测试覆盖拒绝路径（`Privileged=true` request → 返回 `ERR_BACKEND_SECURITY_VIOLATION`）。
- **MUST NOT** LocalBackend 继承 parent env（per ADR-0075 §不变量 4 + §决策 D2 line 164）——仅透传 `ExecOptions.env` 显式声明的变量，测试验证 `HOME=/host` 在子进程中不出现（默认 `BackendPolicy.local.allowed_env_vars = {PATH, HOME, USER, LANG}`，未声明的 env var 被过滤）。
- **SHOULD** DockerBackend 使用容器镜像 digest 锁定（per ADR-0075 §风险 中风险 第 2 项缓解），即 `docker:python:3.12@sha256:abc...` 而非 `docker:python:3.12`；baseline 测量强制 digest 一致。
- **SHOULD** DockerBackend 容器镜像白名单（不接受任意 image spec），通过 `BackendPolicy.docker.image_allowlist` 字段限定；超出 allowlist → `ToolCallResult::deny("image not in allowlist")`；默认 allowlist 空（保守，全显式 allow）。
- **SHOULD** DockerBackend ephemeral container 短期生命周期（创建 → exec → wait → 删除，无残留），CI 测试验证 `docker ps -a` 在测试后无 `hydraforge-*` 容器残留。
- **SHOULD** env.backend.exec.start/end 事件携带 `backend_spec` + `cmd_hash`（SHA256 防 raw command 泄漏，per ADR-0068 §5.11 args-only-keys 政策），不记录 raw args。

## 验收标准

- [ ] C11 完成：`include/agenticdsl/env/local_backend.h` + `src/common/env/local_backend.cpp` 实施 LocalBackend（fork + execve + pipe + waitpid + SIGTERM→SIGKILL + 输出截断 + setrlimit + env 白名单）；`tests/test_local_backend.cpp` ≥6 case 全 PASS（fork happy path / execve ENOENT 错误码 / 超时 SIGKILL / 输出截断超限 / env 白名单不继承 parent / RLIMIT_CPU fork bomb 防御）。
- [ ] C12 完成：`include/agenticdsl/env/docker_backend.h` + `src/common/env/docker_backend.cpp` 实施 DockerBackend（libcurl + Docker REST API + ephemeral container 生命周期 + exec into existing container + 资源限制 + privileged mode 拒绝）；`tests/test_docker_backend.cpp` ≥5 case 全 PASS（ephemeral create+run+delete / exec into existing / privileged 拒绝 / 资源限制生效 / daemon unreachable 错误传播）。
- [ ] C13 完成：`include/agenticdsl/policy/backend_policy.h` + `src/common/env/env_validation_hook.cpp` 实施 EnvValidationHook + BackendPolicy（per-backend 默认策略表 + requires_approval + allowed_env_vars + allowed_paths 三层 deny 路径）；ToolCoordinator.execute 入口集成 pre-hook（per ADR-0069 hook 顺序）；`tests/test_env_validation_hook.cpp` ≥6 case 全 PASS（backend 命中 / env var 拒绝 / working_dir 拒绝 / approval gate / ephemeral docker 不需 approval / docker:prod 需 approval）。
- [ ] Backend 工厂 ship：`src/common/env/backend_factory.cpp` `create_backend(backend_spec, config)` 解析 3 种 spec（`local` / `docker:<container_id>` / `docker:<image>:<tag>`）；`tests/test_backend_factory.cpp` ≥3 case（local 创建 / docker 容器 ID 解析 / docker image:tag 解析）。
- [ ] BackendPolicy 可配置：`tests/test_backend_policy.cpp` ≥4 case 全 PASS（默认策略表 / per-environment override / image allowlist / docker_unavailable_policy fallback 行为）。
- [ ] 超时强制测试：`test_local_backend.cpp` 含 `timeout_5s_kill_grace_period` 用例（子进程 `sleep 30`，LocalBackend `timeout_ms=5000` → SIGTERM @ 5s → SIGKILL @ 10s → 返回 `timed_out=true`），grep 验证 `kill(SIGKILL)` 在 `local_backend.cpp` 出现至少 1 次。
- [ ] 输出截断测试：`test_local_backend.cpp` 含 `output_truncate_64kb` 用例（子进程 `yes` 输出 > 100KB，LocalBackend `max_output_bytes=64*1024` → 截断到 64KB + `ERR_OUTPUT_TOO_LARGE`），grep 验证截断常量与 ADR-0075 D1 line 96 1MB 上限的差异在 `docs/specs/env-backend.md` 文档化。
- [ ] Docker REST API mock 测试：`test_docker_backend.cpp` 含 `docker_daemon_unavailable_fail_fast` + `docker_unavailable_fallback_to_local` 双用例；mock Docker daemon（httplib mock server，per `tests/test_helpers/http_mock_server.h` 模式）返回 503，验证 fallback 行为可配置。
- [ ] ToolCoordinator 集成测试：`test_env_validation_hook.cpp` 含 `tool_coordinator_dispatch_full_flow` 用例（DSL 节点 `shell.exec` 带 `backend: local` + `cmd: ls` 端到端经 ToolCoordinator.execute 触发 EnvValidationHook → ApprovalHandler → LocalBackend，验证 hook 顺序符合 ADR-0069 §决策 D3）。
- [ ] Shell 注入安全测试：`tests/test_backend_security.cpp` ≥3 case 全 PASS（OWASP 命令注入清单：`ls; rm -rf /` → execve 数组逐参传递不解析 shell → 子进程收到 `ls` 单参数；`$(whoami)` → 不展开；`\`rm\`` → 不解析）；DockerBackend `Privileged=true` → `ERR_BACKEND_SECURITY_VIOLATION` 拒绝。
- [ ] ADR-0075 状态翻牌：🟡 Partial → ✅ Approved（D1+D2+D3+D5 全 ship），`docs/active-status.md` §一 Phase 6c C11-C13 行标记 ✅ ship + ADR 状态行更新。
- [ ] `ctest --output-on-failure` 全量零回归（147/147 baseline 不变；新增 `test_local_backend` ≥6 case + `test_docker_backend` ≥5 case + `test_env_validation_hook` ≥6 case + `test_backend_factory` ≥3 case + `test_backend_policy` ≥4 case + `test_backend_security` ≥3 case 共 ≥27 case 全 PASS）。
- [ ] ADR-0003 线程安全保留：LocalBackend/DockerBackend 调用点验证 `DSLEngine::run` 线程模型下 backend 实例不可变（`const` 接口 + `backend_factory` 返回 `shared_ptr<const IEnvBackend>`），多线程并发 exec 走 `BackendCapabilities.max_concurrent_execs` 排队，不引入 data race。
- [ ] 文档 ship：`docs/specs/env-backend.md` 新增（IEnvBackend 接口契约 + 2 backend 实现规范 + 与 ADR-0075 §决策 D1 line 96 1MB 上限的 64KB 截断差异说明）+ `docs/security/backend-policy.md` 新增（默认策略表 + per-environment 配置 + shell 注入防御 checklist）；`docs/specs/dsl.md` §6 shell.exec 节点示例增加 `backend:` 字段（指向 W5 提案交付的解析层）。