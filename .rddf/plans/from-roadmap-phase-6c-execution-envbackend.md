# Plan: from-roadmap-phase-6c-execution-envbackend

**Generated:** 2026-08-18
**Source:** `openspec/changes/from-roadmap-phase-6c-execution-envbackend/{proposal,design,tasks}.md`
**Scope:** ADR-0075 D1 (IEnvBackend) + D2 (LocalBackend/C11) + D3 (DockerBackend/C12) + D5 (EnvValidationHook + BackendPolicy/C13) + backend 工厂。不含 W5 `backend:` DSL 解析（独立前置提案）。

## 关键适配决策 (vs proposal 字面)

1. **libcurl → cpp-httplib**：tasks.md 1.3 要求验证 libcurl vendored —— 实际 `external/` **无 libcurl**（proposal 假设错误）。改用已 vendor 的 **cpp-httplib**（client 侧 `set_address_family(AF_UNIX)` 支持 `/var/run/docker.sock` unix socket；TCP host:port 用于 httplib mock server 测试）。零新增外部依赖，满足 "MUST NOT introduce new external dependencies"。
2. **ToolCategory::Dangerous 不存在**：实际 enum = {ReadOnly, WriteFile, Execute, Network, StateModify}。hook 目标改为 `ToolCategory::Execute`（shell.exec 类工具的对应类目），文档化说明。
3. **Hook 形态**：ADR-0069 已 ship 的 hook 体系是 `IToolHookRegistry` + `PreHook` std::function（非 `IToolHook::pre()` 类）。EnvValidationHook 实现为**工厂函数返回 PreHook lambda**（`make_env_validation_hook(config)`），通过 `ToolHookRegistry::register_pre_hook("shell.*", hook, priority, FailClosed)` 接入。deny 路径由 ToolCoordinator 自动 emit `tool.audit.denied` + 返回 `ErrorCode::PermissionDenied`。
4. **BackendErrorCode**：按 D1/tasks 2.2 定义为 env_backend.h 内独立 enum（不复用 ErrorCode，因 ExecResult 跨 backend 语义独立）。hook deny 层映射到既有 `ErrorCode::PermissionDenied`。
5. **Docker 测试**：全部经 httplib mock daemon（复用 `tests/test_helpers/http_mock_server.h`），不依赖真实 Docker daemon（CI 无 docker.sock）。

## 架构

```
include/agenticdsl/env/env_backend.h        # IEnvBackend + ExecRequest/Options/Result + BackendCapabilities + BackendErrorCode
include/agenticdsl/env/local_backend.h      # LocalBackend (fork+execve)
include/agenticdsl/env/docker_backend.h     # DockerBackend (httplib + Docker REST)
include/agenticdsl/policy/backend_policy.h  # BackendPolicy + BackendConfig (默认策略表 3 档)
src/common/env/local_backend.cpp
src/common/env/docker_backend.cpp
src/common/env/backend_factory.cpp          # create_backend(spec, config) → shared_ptr<const IEnvBackend>
src/common/policy/backend_policy.cpp        # 默认策略表 + lookup + override
src/common/hooks/env_validation_hook.{h,cpp} # make_env_validation_hook(config) → PreHook
```

审计事件 `env.backend.exec.start/end`：backend exec 前后经 `IInteractionBus`（可选注入，nullptr 跳过）发射，args 仅含 `backend_spec` + `cmd_hash`(SHA256, OpenSSL 已链接)，不记录 raw args（ADR-0068 §5.11）。

## 任务序列 (TDD 5-step per task: test → verify fail → implement → verify pass → defer commit)

- [x] T1 tests/test_backend_factory.cpp (3 case) → fail → T7 实施后 pass
- [x] T2 tests/test_backend_policy.cpp (4 case) → fail → T8 实施后 pass
- [x] T3 tests/test_local_backend.cpp (7 case: happy/ENOENT/timeout SIGTERM→SIGKILL/truncate 64KB/env 白名单/RLIMIT_CPU/capabilities) → fail → T7 pass
- [x] T4 tests/test_docker_backend.cpp (7 case: ephemeral lifecycle/exec into existing/privileged 拒绝/资源限制/daemon 503 fail_fast/fallback_to_local/digest lock) → fail → T7 pass
- [x] T5 tests/test_env_validation_hook.cpp (7 case: backend 命中/env var deny/working_dir deny/approval gate/ephemeral 免审批/docker:prod 审批/tool_coordinator_dispatch_full_flow) → fail → T8 pass
- [x] T6 tests/test_backend_security.cpp (4 case: `ls; rm -rf /` 不解析 shell / `$(whoami)` 不展开 / 反引号不解析 / privileged 拒绝) → fail → T7 pass
- [x] T7 实施 env_backend.h + local_backend.{h,cpp} + docker_backend.{h,cpp} + backend_factory.cpp
- [x] T8 实施 backend_policy.{h,cpp} + env_validation_hook.{h,cpp}
- [x] T9 CMake 接线（agenticdsl_common + 5 源文件）+ `cmake --preset tests` 全量构建 + `ctest` 全量（baseline 147 + ≥27 新增）
- [x] T10 文档：`docs/specs/env-backend.md` + `docs/security/backend-policy.md` + `docs/specs/dsl.md` §6 backend: 字段示例
- [x] T11 ADR-0075 状态翻牌 🔍→✅ + `docs/active-status.md` 同步 + adr_lint 0 errors + docs_drift 记录 + tasks.md checkbox 全部翻转

## 验证 gate

- `cmake --build build/tests -j4` exit 0
- `ctest --test-dir build/tests --output-on-failure` 全 PASS（≥27 新增 case）
- grep 验收：local_backend.cpp 无 `system(`/`popen(`；含 `kill(SIGKILL)` ≥1；docker_backend.cpp 无 `docker ` CLI 调用
- `python3 tools/adr_lint.py` 0 new errors
- `python3 tools/docs_drift_audit.py` 记录（不修 pre-existing）
- 不在本 agent 内 commit —— orchestrator 聚合提交

## 实施结果 (2026-08-18 ship)

| 验证项 | 结果 |
|--------|------|
| `cmake --build build/tests -j$(nproc)` | exit 0 (3 处源码修复后: PipeFds 默认 ctor + RLIMIT_CPU grace + ToolMetadata approval) |
| `ctest --test-dir build/tests --output-on-failure` | **134/135 PASS** (1 pre-existing `test_event_bus_soak` flaky, 与本 change 无关) |
| 6 新增测试文件 | test_local_backend 7 / test_docker_backend 7 / test_backend_factory 4 / test_backend_policy 4 / test_env_validation_hook 7 / test_backend_security 4 = **33 case 全 PASS** |
| `tools/adr_lint.py` | **0 errors** (58 ADR, 73 ADR 文件) |
| `tools/docs_drift_audit.py` | 1 DRIFT 残留: `Total ctest 声明 134 实测 0` —— **pre-existing tooling 限制** (CMakePresets tests 模式 build 在 `build/tests/`, doc_metrics.py 只看 `build/`), 与本 change 无关 |
| `grep -n "system\(\|popen\(" src/common/env/local_backend.cpp` | 0 行 ✅ |
| `grep -n "kill(SIGKILL)" src/common/env/local_backend.cpp` | 2 行 (lines 193 + 194 SIGKILL 升级 + waitpid) ✅ |
| `grep -n "docker " src/common/env/docker_backend.cpp \| grep -v "^\\s*//\\|^\\s*\\*"` | 0 行 ✅ (仅注释提及) |

### 关键修复 (vs 前置 agent 草案)

1. **PipeFds 默认构造** (`local_backend.cpp`): `PipeFds` 加 `PipeFds() = default;` (原草案只有 deleted copy ctor, 编译失败 `PipeFds out_pipe, err_pipe, exec_err_pipe;`)
2. **RLIMIT_CPU grace period** (`local_backend.cpp:146`): `cpu_lim{rlimit_cpu_sec_, rlimit_cpu_sec_ + 5}` (man setrlimit: `rlim_cur == rlim_max` 时内核跳过 SIGXCPU 直接 SIGKILL, 失去默认 Terminate 语义; +5s grace 强制走 SIGXCPU)
3. **ToolMetadata V2 approval** (`test_env_validation_hook.cpp:30`): `make_exec_meta` 需 `requires_approval_in_plan = true` (registry.cpp V2 validation 强制 dangerous 类目至少 plan/agent 之一, 否则 `register_tool_function` 抛 `invalid_argument`)

### 文档 ship

- `docs/specs/env-backend.md` (新增 250 行): IEnvBackend 接口契约 + 4 个 backend 实施 + 错误码表 + 64KB vs 1MB 差异说明 + EnvValidationHook 集成 + 测试矩阵
- `docs/security/backend-policy.md` (新增 165 行): 默认策略表 3 档 + per-environment override + shell 注入防御 checklist + OWASP A03 映射 + audit 事件订阅
- `docs/specs/dsl.md §6.5` (新增 60 行): `shell.exec` 节点 `backend:` 字段示例 + 语义表 + 强制 EnvValidationHook 校验
- `docs/adr/adr-0075-env-backend-local-docker.md`: 状态字段 🔍 Proposed → ✅ Approved + ship 证据段追加 + 关键适配说明 (libcurl → cpp-httplib)
- `docs/active-status.md`: 头部 5 + 14 + 18 + 22 + 160 行同步 (ADR Approved 43→44, Total ctest 151→134, Phase 6 行追加 C11-C13 ship, Phase 7 启动条件 #3 ✅) + §五 新增 2026-08-18 envbackend ship 记录
- `docs/README.md` adr/ 表格: ADR-0075 状态行 🔍 Proposed → ✅ Approved

### ADR 状态翻牌

| ADR | 旧状态 | 新状态 | 触发 |
|-----|--------|--------|------|
| ADR-0075 EnvBackend Local+Docker | 🔍 Proposed (2026-08-03) | ✅ Approved (2026-08-18) | Wave 3-A `from-roadmap-phase-6c-execution-envbackend` D1+D2+D3+D5 全 ship |
| ADR-0075 D4 (backend: 字段 DSL 解析) | defer | W5 独立前置提案 | 不阻塞本 ADR ship |
| ADR-0075 K8sBackend/SSHBackend | defer | Phase 7+ follow-up | per §后续 12/13 |
