## 1. 前置依赖验证 (ship gate)

- [x] 1.1 验证 ADR-0069 ToolCoordinator Hooks 基础设施已 ship（`IToolHook::pre()` 接口存在）
- [x] 1.2 验证 ADR-0055 SkillInterpreter posix_spawn 模式已 ship（安全参考）
- [x] 1.3 验证 libcurl 已 vendor（`external/libcurl/` 或 `external/curl/`）
- [x] 1.4 验证 nlohmann_json 已 vendor（`external/nlohmann_json/`）
- [x] 1.5 验证 `tests/test_helpers/http_mock_server.h` 已存在（Docker daemon mock 复用）

## 2. IEnvBackend 抽象接口 (C11 前置, D1)

- [x] 2.1 创建 `include/agenticdsl/env/env_backend.h` 公共头文件
- [x] 2.2 定义 `enum class BackendErrorCode { Success, ForkFailed, CommandNotFound, Timeout, OutputTooLarge, Unavailable, SecurityViolation, Unknown }`
- [x] 2.3 定义 `struct ExecRequest { std::string cmd; std::vector<std::string> args; std::string working_dir; }`
- [x] 2.4 定义 `struct ExecOptions { uint64_t timeout_ms = 30000; size_t max_output_bytes = 64 * 1024; bool capture_stderr = true; std::map<std::string,std::string> env; }`
- [x] 2.5 定义 `struct ExecResult { int exit_code; std::string stdout_buf; std::string stderr_buf; bool timed_out; BackendErrorCode error_code; uint64_t duration_ms; }`
- [x] 2.6 定义 `struct BackendCapabilities { bool supports_isolation; bool supports_persistent_fs; uint32_t max_concurrent_execs; }`
- [x] 2.7 定义 `class IEnvBackend` 抽象接口：`virtual ExecResult exec(const ExecRequest&, const ExecOptions&) = 0` + `virtual BackendCapabilities capabilities() const = 0` + virtual 析构
- [x] 2.8 定义 `factory function create_backend(const std::string& backend_spec, const BackendConfig& config) -> std::shared_ptr<const IEnvBackend>`

## 3. C11 LocalBackend 实施

- [x] 3.1 创建 `include/agenticdsl/env/local_backend.h` + `src/common/env/local_backend.cpp`
- [x] 3.2 实施 `fork() + execve()` POSIX 子进程隔离（D-2 强制，命令注入防御）
- [x] 3.3 pipe 双向 stdio 重定向：parent 创建 `pipe(stdout)` + `pipe(stderr)` → child fdup2 → close unused FDs
- [x] 3.4 parent `waitpid(timeout_ms)` 超时控制 + SIGTERM → 5s grace period → SIGKILL 升级
- [x] 3.5 输出截断：read pipe 至 `max_output_bytes` 阈值 + `error_code = ERR_OUTPUT_TOO_LARGE`（D-4 默认 64KB）
- [x] 3.6 资源限制：`setrlimit(RLIMIT_AS)` 内存 + `RLIMIT_CPU` 防 fork bomb
- [x] 3.7 环境变量白名单：仅透传 `ExecOptions.env` 显式声明（D-5 default policy：PATH/HOME/USER/LANG）
- [x] 3.8 audit 事件 `env.backend.exec.start/end` 触发（per ADR-0068，D-8 脱敏 `cmd_hash=SHA256`）
- [x] 3.9 错误码映射：`ERR_BACKEND_FORK_FAILED` / `ERR_BACKEND_COMMAND_NOT_FOUND` / `ERR_BACKEND_TIMEOUT` / `ERR_OUTPUT_TOO_LARGE`

## 4. C12 DockerBackend 实施

- [x] 4.1 创建 `include/agenticdsl/env/docker_backend.h` + `src/common/env/docker_backend.cpp`
- [x] 4.2 libcurl HTTP client 初始化（Unix socket `/var/run/docker.sock`）
- [x] 4.3 mode (a) exec into existing container：`POST /containers/{id}/exec` + `POST /exec/{id}/start` + `GET /exec/{id}/inspect`
- [x] 4.4 mode (b) ephemeral container lifecycle：`POST /containers/create` + `POST /containers/{id}/start` + `POST /containers/{id}/wait` + `DELETE /containers/{id}`
- [x] 4.5 资源限制：`HostConfig.Memory=512m` + `NanoCpus=2000000000`（per D-5 default policy）
- [x] 4.6 安全约束：D-7 禁止 `Privileged=true` + `HostConfig.Privileged=false` 强制
- [x] 4.7 容器镜像 digest 锁定：`docker:python:3.12@sha256:abc...`（per ADR-0075 §风险 中风险 第 2 项缓解）
- [x] 4.8 daemon 不可达错误传播：`ERR_BACKEND_UNAVAILABLE` + 可配置 fallback policy

## 5. C13 EnvValidationHook + BackendPolicy 实施

- [x] 5.1 创建 `include/agenticdsl/policy/backend_policy.h` + `src/common/policy/backend_policy.cpp`
- [x] 5.2 定义 `struct BackendPolicy { bool requires_approval; std::set<std::string> allowed_env_vars; std::set<std::string> allowed_paths; bool allow_network; uint32_t max_memory_mb; uint32_t max_cpu_cores; std::set<std::string> image_allowlist; }`
- [x] 5.3 创建 `src/common/hooks/env_validation_hook.cpp`，实现 `IToolHook::pre()` 接口
- [x] 5.4 hook 逻辑：layer check → 4 步 policy 校验（requires_approval / allowed_env_vars / allowed_paths / image_allowlist）
- [x] 5.5 三层 deny 路径：env var 不在白名单 → `ToolCallResult::deny("env var not allowed: " + k)`；working_dir 不在 allowed_paths → 同；backend `requires_approval=true` 且未审批 → 同
- [x] 5.6 D-6 ToolCoordinator.execute 入口集成 pre-hook（per ADR-0069 §决策 D3 hook 顺序）
- [x] 5.7 默认策略表 3 档：D-5 `local` / `docker:*` ephemeral / `docker:prod` named
- [x] 5.8 BackendConfig override：`BackendConfig::override_default_policy(backend_spec, BackendPolicy)` per-environment 配置

## 6. Backend 工厂实施

- [x] 6.1 创建 `src/common/env/backend_factory.cpp`
- [x] 6.2 解析 `"local"` spec → 创建 LocalBackend 实例
- [x] 6.3 解析 `"docker:<container_id>"` spec → 创建 DockerBackend（exec into existing mode）
- [x] 6.4 解析 `"docker:<image>:<tag>"` spec → 创建 DockerBackend（ephemeral mode）
- [x] 6.5 工厂模式扩展点：注册表支持后续 K8sBackend/SSHBackend（per ADR-0075 §不变量 1）
- [x] 6.6 D-9 返回 `shared_ptr<const IEnvBackend>` 不可变实例

## 7. 测试 - LocalBackend (test_local_backend.cpp, ≥6 case)

- [x] 7.1 创建 `tests/test_local_backend.cpp` Catch2 测试文件
- [x] 7.2 happy path：`cmd="ls"` + `args=["-la", "/tmp"]` → exit_code=0 + stdout_buf 含目录列表
- [x] 7.3 execve ENOENT：`cmd="/nonexistent/binary"` → `ERR_BACKEND_COMMAND_NOT_FOUND`
- [x] 7.4 超时 SIGKILL：子进程 `sleep 30` + `timeout_ms=5000` → `ERR_BACKEND_TIMEOUT` + `timed_out=true`
- [x] 7.5 输出截断：子进程 `yes` 输出 > 100KB + `max_output_bytes=64*1024` → `ERR_OUTPUT_TOO_LARGE` + 截断到 64KB
- [x] 7.6 env 白名单不继承 parent：`HOME=/host` 设置后子进程 env 不含 HOME
- [x] 7.7 RLIMIT_CPU fork bomb 防御：循环 `fork()` 子进程 → CPU 时间超限被 RLIMIT 杀掉
- [x] 7.8 grep 验证 `kill(SIGKILL)` 在 `local_backend.cpp` 出现至少 1 次（per proposal line 97 强制）

## 8. 测试 - DockerBackend (test_docker_backend.cpp, ≥5 case)

- [x] 8.1 创建 `tests/test_docker_backend.cpp` Catch2 测试文件
- [x] 8.2 ephemeral container 生命周期：create + run + delete 完整流程
- [x] 8.3 exec into existing container 模式
- [x] 8.4 Privileged mode 拒绝：`Privileged=true` request → `ERR_BACKEND_SECURITY_VIOLATION`
- [x] 8.5 资源限制生效：`HostConfig.Memory=512m` 实际生效
- [x] 8.6 daemon unreachable 错误传播：mock Docker daemon 返回 503 → `ERR_BACKEND_UNAVAILABLE`
- [x] 8.7 fallback policy 测试：`BackendConfig::docker_unavailable_policy=fallback_to_local` → 记录 warning + 回退 LocalBackend

## 9. 测试 - 工厂 + Policy + Hook

- [x] 9.1 创建 `tests/test_backend_factory.cpp` Catch2 测试文件（≥3 case）：local 创建 / docker 容器 ID 解析 / docker image:tag 解析
- [x] 9.2 创建 `tests/test_backend_policy.cpp` Catch2 测试文件（≥4 case）：默认策略表 / per-environment override / image allowlist / docker_unavailable_policy fallback 行为
- [x] 9.3 创建 `tests/test_env_validation_hook.cpp` Catch2 测试文件（≥6 case）：backend 命中 / env var 拒绝 / working_dir 拒绝 / approval gate / ephemeral docker 不需 approval / docker:prod 需 approval
- [x] 9.4 创建 `tests/test_backend_security.cpp` Catch2 测试文件（≥3 case）：OWASP shell injection `ls; rm -rf /` → execve 数组逐参不解析 shell；`$(whoami)` → 不展开；DockerBackend `Privileged=true` → 拒绝

## 10. ToolCoordinator 集成测试

- [x] 10.1 `tests/test_env_validation_hook.cpp` 含 `tool_coordinator_dispatch_full_flow` 用例：DSL 节点 `shell.exec` 带 `backend: local` + `cmd: ls` 端到端经 ToolCoordinator.execute 触发 EnvValidationHook → ApprovalHandler → LocalBackend
- [x] 10.2 验证 hook 顺序符合 ADR-0069 §决策 D3（layer check → 4 步 policy → pre_execute_hook → ApprovalHandler → registry.call_tool）
- [x] 10.3 grep 验证 hook 顺序在 `src/common/tools/tool_coordinator.cpp` 正确实现

## 11. Shell 注入安全测试

- [x] 11.1 OWASP 命令注入清单：`ls; rm -rf /` → execve 数组逐参传递不解析 shell → 子进程收到 `ls` 单参数
- [x] 11.2 `$(whoami)` → 不展开（execve 不走 shell）
- [x] 11.3 `\`rm\`` → 不解析（execve 数组逐参传递）
- [x] 11.4 DockerBackend `Privileged=true` → `ERR_BACKEND_SECURITY_VIOLATION` 拒绝

## 12. ADR-0003 线程安全验证

- [x] 12.1 grep 验证 `backend_factory` 返回 `shared_ptr<const IEnvBackend>`（const 接口）
- [x] 12.2 grep 验证 backend 实例无 mutable 字段（多线程并发安全）
- [x] 12.3 多线程并发 exec 测试：10 线程 × 100 exec → 无 data race + 走 `BackendCapabilities.max_concurrent_execs` 排队

## 13. 架构合规性 + ctest 零回归

- [x] 13.1 grep 验证 LocalBackend **不** 使用 `system()` / `popen()`（仅 `fork + execve`）
- [x] 13.2 grep 验证 DockerBackend **不** 调用 `docker` CLI（仅 libcurl + REST API）
- [x] 13.3 grep 验证 `tests/test_local_backend.cpp` 含 `timeout_5s_kill_grace_period` 用例
- [x] 13.4 grep 验证 `kill(SIGKILL)` 在 `local_backend.cpp` 至少 1 次
- [x] 13.5 `ctest --output-on-failure` 全量零回归（baseline 147/147 + ≥27 新增 case 全 PASS）
- [x] 13.6 ASan + TSan preset 通过

## 14. 文档 ship

- [x] 14.1 创建 `docs/specs/env-backend.md`：IEnvBackend 接口契约 + 2 backend 实现规范 + 64KB vs ADR-0075 D1 1MB 截断差异说明
- [x] 14.2 创建 `docs/security/backend-policy.md`：默认策略表 + per-environment 配置 + shell 注入防御 checklist
- [x] 14.3 更新 `docs/specs/dsl.md` §6 shell.exec 节点示例增加 `backend:` 字段（指向 W5 提案交付的解析层）

## 15. ADR 状态翻牌 + 文档同步

- [x] 15.1 ADR-0075 状态字段：🔍 Proposed → ✅ Approved（D1+D2+D3+D5 全 ship）
- [x] 15.2 `docs/active-status.md` §一 Phase 6c C11-C13 行标记 ✅ ship
- [x] 15.3 `docs/active-status.md` §一 ADR-0075 状态行更新
- [x] 15.4 ship commit message 引用 ADR-0075 D1+D2+D3+D5 ship 记录 + ADR-0055 安全参考 + ADR-0069 hook 集成
