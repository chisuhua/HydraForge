# from-roadmap-phase-6b-execution-envbackend

**优先级**: P0 | **来源**: from-roadmap (phase-6b/execution-envbackend, ADR-0075 D2/D3/D5)
**阶段**: phase-6b | **分类**: execution-envbackend
**类型**: feature
**主题**: LocalBackend；DockerBackend；EnvValidationHook

## 架构依据

ADR-0075 EnvBackend 为 AgenticDSL Execution Plane 提供隔离执行环境（沙箱 / 容器）：

- D2 LocalBackend：fork + execve + 超时 + 输出截断（本地进程隔离）。
- D3 DockerBackend：libcurl + Docker REST API（容器隔离）。
- D5 EnvValidationHook + BackendPolicy：ToolCoordinator pre-hook 强制 backend 选择。
- 3 件合并提供"backend 选择" 抽象，是后续 Skill 隔离执行（ADR-0055）的基础。

## 范围

- **In Scope**:
  - `include/agenticdsl/contract/ienv_backend.h` L3 契约：`execute(command, env, timeout) → Result`。
  - `src/modules/env_backend/local_backend.cpp` fork + execve 实现（pipe 双向 + waitpid + timeout）。
  - `src/modules/env_backend/docker_backend.cpp` libcurl + Docker REST API（创建容器 + exec + wait + 删除）。
  - `src/modules/env_backend/env_validation_hook.cpp` ToolCoordinator pre-hook（验证 backend 字段 + BackendPolicy）。
  - `include/agenticdsl/policy/backend_policy.h` BackendPolicy 值类型（按工具名 + layer 强制 backend）。
  - 6 类测试：LocalBackend fork/exec/timeout/output-truncate / DockerBackend lifecycle / EnvValidationHook 路径。
- **Out of Scope**:
  - Podman / Kubernetes backend（留 follow-up）。
  - gVisor / Firecracker 高级沙箱（无需求）。
  - 持久化 backend 实例（每次执行新建）。

## 关键场景

- GIVEN DSL 节点 `shell/exec` 带 `backend: local`
  WHEN ToolCoordinator 派发
  THEN EnvValidationHook 验证 backend=local → LocalBackend.execute → fork → execve → 输出截断返回。

- GIVEN DSL 节点 `shell/exec` 带 `backend: docker, image: python:3.12`
  WHEN 派发
  THEN DockerBackend 创建容器 `hydraforge-<uuid>` → exec → wait → 删除 → 返回结果。

- GIVEN DSL 节点 `shell/exec` 不带 backend 字段
  WHEN BackendPolicy 配置 default_backend=docker
  THEN EnvValidationHook 默认注入 docker，行为等价于显式声明。

- GIVEN LocalBackend 进程超时（> 30s）
  WHEN 派发
  THEN kill(SIGKILL) 子进程 + 输出截断 + 返回 TimeoutError。

## 技术约束

- MUST LocalBackend 使用 fork + execve（禁止 system()，避免 shell injection）。
- MUST DockerBackend 使用容器短期生命周期（创建 → exec → 删除，无残留）。
- MUST EnvValidationHook 与 ToolCoordinator 集成（依赖 adr-0069 hooks 基础设施）。
- MUST 输出截断限制（默认 10MB，超出截断并 stderr warning）。
- MUST NOT 在 backend 层引入第二套 tool registry（沿用 IToolRegistry）。
- SHOULD DockerBackend 复用 libcurl 连接池（性能优化）。

## 验收标准

- LocalBackend 4 测试通过（fork/exec/timeout/output-truncate）。
- DockerBackend lifecycle 测试通过（创建 → exec → 删除）。
- EnvValidationHook 集成测试通过（backend 字段 + BackendPolicy）。
- BackendPolicy 按 layer 配置测试通过。
- ctest 全量零回归。
- ADR-0075 状态可从 🟡 Partial 提升 ✅ Approved。