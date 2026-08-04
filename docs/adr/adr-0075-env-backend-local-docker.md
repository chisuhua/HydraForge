# ADR-0075: EnvBackend 多环境执行 (Local + Docker)

## 状态

🔍 Proposed (2026-08-03 — 派生自 ADR-0071 §决策 D6, Wave 3 Phase 1+2 ADR; Local + Docker 双 backend, K8s/SSH 推迟到独立 follow-up; 衔接 ADR-0069 ToolCoordinator hooks; 待架构组评审; 实施 2-3 周)

## 领域

L1 OS Services / 多环境执行抽象 / Backend 路由 / DSL 节点扩展

## 关联

### 父 ADR
- [ADR-0071 — LLM-native AgenticDSL 架构](./adr-0071-llm-native-agenticdsl-architecture.md) §决策 D6 (本 ADR 是 D6 的具体实施: EnvBackend 抽象 + Local + Docker 实现)
- ADR-0071 §决策 D5 (本 ADR `backend:` 字段衔接 D5 重命名, 避免与 `ExecOptions.env` 冲突)

### 上游锚定
- [ADR-0004 — ToolRegistry 安全模型](./adr-0004-toolregistry-security.md) — ToolCategory 矩阵 (本 ADR `shell.exec` 强制 dangerous + approval)
- [ADR-0031 — 执行策略](./adr-0031-execution-policy.md) — IExecutionPolicy + ApprovalHandler (本 ADR backend 选择不绕过 policy)
- [ADR-0068 — 事件发射契约](./adr-0068-event-emission-contract.md) — 本 ADR 设计 **2 个候选事件** `env.backend.exec.start/end` (注册前置: ADR-0068 §附录 A amendment PR)
- [ADR-0069 — ToolCoordinator Hooks](./adr-0069-tool-coordinator-hooks.md) — pre-hook 注入 env validation (path / working dir / env vars)

### 平行/下游
- ADR-0076 (DSL Engine as MCP Server, Wave 3 末) — 本 ADR LocalBackend 复用 MCP `stdio` 传输模式
- ADR-0077 (gRPC Data Plane, Wave 4, descoped) — 本 ADR interface 设计为 gRPC backend 预留扩展点
- ADR-0072 (DSL 节点扩展, Wave 2.4 gated) — 本 ADR `backend:` 字段是节点扩展的一部分

### 规范
- [`docs/specs/dsl.md`](../specs/dsl.md) v3.10 §6 — DSL 节点语法 (本 ADR 新增 `backend:` 字段)
- [`docs/specs/architecture.md`](../specs/architecture.md) §3 — L1 OS Services 多环境抽象
- [Docker Engine SDK](https://docs.docker.com/engine/api/sdk/) — DockerBackend 依赖 (`libcurl` + Docker REST API)

---

## 背景

### 问题

当前 DSL 节点 `shell.exec` (来自 ADR-0071 §决策 D6) **没有 backend 抽象**, 隐含假设在**当前进程所在环境**直接执行。具体 4 个空白:

1. **无法指定隔离环境** — LLM 生成的 `shell.exec` 总是污染 host 进程; 无沙箱隔离
2. **无法在 Docker 容器中执行** — 测试/部署场景需要在容器内跑, 但 hardcoded 路径不可移植
3. **backend 选择与 policy 脱节** — 当前 `env:` 字段名与 `ExecOptions.env` (环境变量) 冲突, LLM 困惑; 此外没有"production 必须审批"的强制策略
4. **审计链路缺失** — 跨 backend 执行无统一事件, 安全审计散落各 shell.exec 实现

### 解决方案

引入 **`IEnvBackend` 抽象** + **2 个实现** (Local + Docker) + **DSL 字段 `backend:`** + **ToolCoordinator pre-hook 注入 env validation**:

```cpp
// 抽象接口
class IEnvBackend {
public:
  virtual ~IEnvBackend() = default;
  virtual ExecResult exec(
    const ExecRequest& req,
    const ExecOptions& opts
  ) = 0;
  virtual std::string name() const = 0;          // "local" | "docker:prod"
  virtual BackendCapabilities capabilities() const = 0;
};

// 实现
class LocalBackend : public IEnvBackend { ... };   // fork + exec
class DockerBackend : public IEnvBackend { ... }; // docker exec API
// (Phase 2+: K8sBackend, SSHBackend)
```

### 已实证证据

- **ADR-0071 §D6 已定义抽象骨架**: 4 个 backend 类 + `backend:` 字段绑定; 本 ADR 是其具体实施
- **Docker Engine SDK**: 项目已 vendor `libcurl`, 集成零额外依赖; REST API 简洁 (`POST /containers/{id}/exec`)
- **fork + exec 模式**: `examples/pkm_temporal_demo` Sprint 24 (2026-07-31) ship 的 `popen` 实现可复用 (PDK `shell_tools`)
- **ToolCoordinator hook 集成**: ADR-0069 pre-hook 已立项 (Wave 1 第 4 项), 本 ADR 复用其 `env.validation` hook

---

## 决策

### D1. IEnvBackend 接口 — 简洁 exec 原语

```cpp
namespace agenticdsl::env {

// 执行请求
struct ExecRequest {
  std::string cmd;                  // e.g. "ls -la /tmp"
  std::vector<std::string> args;    // 替代 cmd 拼接, 防注入
  std::optional<std::string> working_dir;
};

// 执行选项
struct ExecOptions {
  std::map<std::string, std::string> env;   // 环境变量 (与 backend 字段不冲突)
  int timeout_ms = 30000;
  size_t max_output_bytes = 1024 * 1024;     // 1 MB 上限
  bool capture_stderr = true;
  std::optional<std::string> stdin_input;   // e.g. for heredoc
};

// 执行结果
struct ExecResult {
  int exit_code;
  std::string stdout_buf;
  std::string stderr_buf;
  std::chrono::milliseconds duration;
  bool timed_out = false;
  std::optional<std::string> error_code;    // ERR_BACKEND_TIMEOUT / ERR_BACKEND_OOM etc.
};

// 后端能力
struct BackendCapabilities {
  bool supports_isolation = false;          // Docker: true, Local: false
  bool supports_persistent_fs = false;      // Docker: volume, Local: true
  int max_concurrent_execs = 1;             // Local: 1 (fork), Docker: 池大小
};

// 抽象接口
class IEnvBackend {
public:
  virtual ~IEnvBackend() = default;
  virtual ExecResult exec(const ExecRequest& req, const ExecOptions& opts) = 0;
  virtual std::string name() const = 0;
  virtual BackendCapabilities capabilities() const = 0;
};

// Backend 工厂 (ADR-0071 §D6 已定义模式)
std::unique_ptr<IEnvBackend> create_backend(
  const std::string& backend_spec,        // "local" | "docker:container_id"
  const BackendConfig& config
);

}  // namespace agenticdsl::env
```

**关键设计选择**:

- **`cmd` + `args` 分离** — 防 shell 注入 (LLM 经常忘记 quote)
- **`max_output_bytes` 强制上限** — 防 OOM; 超限返回 `error_code = "ERR_OUTPUT_TOO_LARGE"`
- **`backend_spec` 字符串** — `"local"` / `"docker:<container_id>"` / `"docker:<image>:<tag>"` (按需)
- **不引入 sandboxing 抽象** — sandboxing 由 backend 自身能力决定 (Docker 提供 namespace 隔离)

### D2. LocalBackend — fork + exec (Phase 1, 1 周)

```cpp
class LocalBackend : public IEnvBackend {
public:
  LocalBackend();  // 读取 /proc/self/status, 限制资源
  ExecResult exec(const ExecRequest& req, const ExecOptions& opts) override;
  std::string name() const override { return "local"; }
  BackendCapabilities capabilities() const override {
    return {.supports_isolation = false, .supports_persistent_fs = true, .max_concurrent_execs = 1};
  }
private:
  // fork() + execve() 实现, 复用 examples/pkm_temporal_demo 的 popen 模式
};
```

**实现要点**:

1. **进程隔离**: `fork()` + `execve()`, 子进程 stdio 重定向到 pipe
2. **超时控制**: 父进程 `waitpid(timeout)` + `kill(SIGKILL)` (超时)
3. **资源限制**: `setrlimit(RLIMIT_AS)` + `RLIMIT_CPU` (防 fork bomb)
4. **环境变量白名单**: 仅透传 `ExecOptions.env` 显式声明的变量 (不继承 parent env)
5. **审计**: 触发 `env.backend.exec.start/end` 事件 (注册前置: ADR-0068 §附录 A amendment)

**错误码**:

| 错误 | 触发条件 | 错误码 |
|------|---------|--------|
| Fork 失败 | `errno == EAGAIN` | `ERR_BACKEND_FORK_FAILED` |
| Exec 失败 | `errno == ENOENT` | `ERR_BACKEND_COMMAND_NOT_FOUND` |
| 超时 | timeout 触发 | `ERR_BACKEND_TIMEOUT` |
| 输出超限 | stdout/stderr > max_output_bytes | `ERR_OUTPUT_TOO_LARGE` |
| 退出非零 | cmd exit code != 0 | `error_code = null` (正常非零退出) |

### D3. DockerBackend — docker exec REST API (Phase 2, 1 周)

```cpp
class DockerBackend : public IEnvBackend {
public:
  DockerBackend(const DockerConfig& config);  // socket path, container id, image
  ExecResult exec(const ExecRequest& req, const ExecOptions& opts) override;
  std::string name() const override { return "docker:" + container_id_; }
  BackendCapabilities capabilities() const override {
    return {.supports_isolation = true, .supports_persistent_fs = true, .max_concurrent_execs = 16};
  }
private:
  std::string socket_path_;       // e.g. "unix:///var/run/docker.sock"
  std::string container_id_;      // 目标容器
  std::optional<std::string> image_;  // 启动新容器模式
  // POST /containers/{id}/exec + POST /exec/{id}/start + GET /exec/{id}/inspect
};
```

**两种使用模式**:

1. **exec into existing container**: `docker exec <container_id> <cmd>`
   - 适用: dev/test 环境已运行容器
2. **create + run ephemeral container**: `docker run --rm <image> <cmd>`
   - 适用: CI / batch job / 临时 sandbox

**实现要点**:

1. **Docker daemon 通信**: HTTP REST API over Unix socket (项目已 vendor libcurl)
2. **Exec 实例创建**: `POST /containers/{id}/exec` 返回 exec_id
3. **Exec 启动**: `POST /exec/{id}/start` with stdin/stdout/stderr hijack
4. **Exec 轮询**: `GET /exec/{id}/inspect` 直到 exit code != null (timeout 控制)
5. **超时控制**: 客户端轮询 + SIGTERM docker exec
6. **资源限制**: 通过 `HostConfig.Memory` + `NanoCpus` (创建容器时声明)

**安全约束**:

- ❌ **不允许 privileged mode** (root in container = root on host)
- ❌ **不允许挂载 host 根目录** (read-only mounts OK)
- ✅ **允许 tmpfs mount** (沙箱内临时文件)
- ✅ **允许 network namespace** (默认 bridge, 可禁用)

### D4. DSL 字段 `backend:` 引入 — 重命名避免冲突 (ADR-0071 §D5 已批准)

**当前草案** (ADR-0071 §3):

```yaml
- type: shell.exec
  cmd: "ls -la /tmp"
  env: production          # ⚠️ 与 ExecOptions.env 冲突
```

**重命名后** (本 ADR):

```yaml
- type: shell.exec
  cmd: "ls -la /tmp"
  env_vars:                # ExecOptions.env 重命名
    DEBUG: "1"
  backend: local           # 本 ADR 新增: 执行目标
  timeout: 30000
```

**字段语义**:

| 字段 | 作用域 | 例子 |
|------|--------|------|
| `env_vars` | 进程环境变量 (key=value) | `DEBUG=1`, `PATH=...` |
| `backend` | 执行目标 (进程级) | `local`, `docker:container_xyz`, `docker:python:3.12` |
| `working_dir` | 工作目录 | `/workspace` |

**LLM Prompt 模板更新** (衔接 ADR-0074 D3):

- V0/V1 已有 `shell.exec` 字段 `cmd`, `args`
- V3 增加 `backend:` 字段说明 + LocalBackend/DockerBackend 例子
- V3 Prompt 强调: "若未指定 backend, 默认 `local`, 沙箱场景需显式 `backend: docker:<image>`"

### D5. ToolCoordinator pre-hook 集成 — env validation (ADR-0069 衔接)

**目标**: ToolCoordinator `shell.exec` execute 流插入 env validation pre-hook, 强制 backend policy 检查。

**Hook 注入位置** (ADR-0069 §决策 D3):

```
shell.exec ToolCall → NestingGuard → pre_hook(env.validation) → layer check → ApprovalHandler → execute
```

**EnvValidationHook 默认实现**:

```cpp
class EnvValidationHook : public IToolHook {
public:
  ToolCallResult pre(const ToolCallContext& ctx) override {
    const auto& meta = ctx.tool_metadata;
    if (meta.category != ToolCategory::Dangerous) return ToolCallResult::ok();

    // 1. backend policy 检查
    auto backend_spec = ctx.args.value("backend", "local");
    auto policy = policy_.get_backend_policy(backend_spec);

    if (policy.requires_approval && !ctx.approval_granted) {
      return ToolCallResult::deny("Backend policy requires approval");
    }

    // 2. env_vars 白名单
    auto env_vars = ctx.args.value("env_vars", json::object());
    for (auto& [k, v] : env_vars.items()) {
      if (!policy.allowed_env_vars.contains(k)) {
        return ToolCallResult::deny("env var not allowed: " + k);
      }
    }

    // 3. working_dir 白名单
    auto working_dir = ctx.args.value("working_dir", "");
    if (!policy.is_path_allowed(working_dir)) {
      return ToolCallResult::deny("working_dir not allowed: " + working_dir);
    }

    return ToolCallResult::ok();
  }
private:
  IExecutionPolicy& policy_;
};
```

**BackendPolicy 配置** (per-backend):

```cpp
struct BackendPolicy {
  bool requires_approval = true;     // docker:prod 默认审批
  std::set<std::string> allowed_env_vars;  // {"PATH", "HOME", "DEBUG"}
  std::vector<std::string> allowed_paths;  // ["/workspace", "/tmp"]
  bool allow_network = true;
  size_t max_memory_mb = 512;
  int max_cpu_cores = 2;
};
```

**默认策略**:

| backend | requires_approval | allowed_env_vars | allowed_paths | max_memory_mb |
|---------|:---:|---|---|:---:|
| `local` | ✅ true | PATH/HOME/USER/LANG | $HOME, /tmp, /workspace | ∞ (host) |
| `docker:*` (ephemeral) | ❌ false (sandbox) | * (隔离环境) | container FS | 512 |
| `docker:prod` (named) | ✅ true | * | per-deployment | per-deployment |

---

## 不变量

### 长期不变量

1. **IEnvBackend 是 backend 唯一接口** — 不允许 ToolCoordinator 直接调用 `fork` / `docker exec` (绕过 backend)
2. **`cmd` + `args` 必填字段** — 不允许只填 `cmd` (防 shell 注入)
3. **`backend:` 字段推荐必填**, 缺省 = `local` (向后兼容 V3.10 已 ship 示例, ADR-0072 D4 实施)
4. **LocalBackend 不继承 parent env** — 仅透传 `ExecOptions.env` 显式声明 (D2)
5. **DockerBackend 禁用 privileged mode** — 防容器逃逸 (D3)
6. **EnvValidationHook 是 dangerous 类工具必走** — 不允许 hook 旁路 (D5)
7. **K8sBackend / SSHBackend 推迟到独立 ADR** — 本 ADR Phase 3+ 不实施

### 安全不变量 (与 ADR-0071 §安全不变量 一致)

```
LLM 生成 → DSL parse → backend policy check → env validation hook → ApprovalHandler → backend exec
```

任一阶段失败即拒绝。backend 选择 = `docker:prod` 必须审批 (D5); `local` 默认审批 (D5)。

### Field 命名不变量 (D4 重命名)

- ❌ **`env:`** 字段 (与 ExecOptions.env 冲突) — DSL 解析器拒绝
- ✅ **`env_vars:`** 字段 (ExecOptions.env 重命名)
- ✅ **`backend:`** 字段 (本 ADR 新增)

---

## 风险

### 高风险

| 风险 | 缓解 |
|------|------|
| **Docker socket 暴露攻击面** — LocalBackend 误用 Docker socket 导致 host 沦陷 | LocalBackend 与 DockerBackend 完全隔离; DockerBackend 仅接受 Unix socket (默认 `/var/run/docker.sock`); CI 测试验证 socket path 白名单 |
| **shell 注入** — LLM 生成 `cmd: "ls; rm -rf /"` 利用 LocalBackend 漏洞 | D2 强制 `cmd + args` 分离, 不允许 shell 解析; args 数组逐个 `execve`; CI 测试覆盖 OWASP 命令注入清单 |
| **EnvValidationHook 误杀** — hook 拒绝合法执行 (e.g. legitimate `apt-get`) | BackendPolicy 配置 + per-environment; 默认策略保守, 渐进开放; hook 失败事件记录用于调优 |
| **Docker exec 超时不准确** — REST API 轮询 + client timeout 双层控制易漂移 | 单一时间源 (docker daemon 端 wall-clock); client 端 timeout 仅兜底, 不主导 |

### 中风险

| 风险 | 缓解 |
|------|------|
| **Docker daemon 不可用** — daemon down 导致所有 docker exec 失败 | backend 工厂 fallback 到 `local` (仅当 policy 允许); 失败事件 `env.backend.unavailable` 上报 (候选主题, 注册前置: ADR-0068 §附录 A amendment) |
| **容器镜像 drift** — `docker:python:3.12` 拉取的镜像版本变化, baseline 失真 | 默认锁定 digest (e.g. `python:3.12@sha256:abc...`); baseline 测量强制 digest 一致 |
| **LocalBackend fork 资源耗尽** — 大量并发 `shell.exec` 触发 fork bomb | BackendCapabilities.max_concurrent_execs=1; 排队机制在 ToolCoordinator 侧; CI 测试覆盖 1000× 并发 |
| **Hook 链性能** — EnvValidationHook + 其他 hook 叠加延迟 | hook 链顺序优化 (env validation 在 layer check 之后); benchmark target ≤50µs per hook |

### 低风险

| 风险 | 缓解 |
|------|------|
| **Backend 接口扩展性差** — 新 backend (K8s/SSH) 需重构接口 | D1 接口设计已抽象 `BackendCapabilities` + 工厂模式; 新 backend 实现零接口变更 |
| **DSL 字段重命名迁移成本** — 现有 DSL 文件用 `env:` 字段需迁移 | 向后兼容: 解析器接受 `env:` 作为 `env_vars:` 别名 (1 个 Sprint 后删除); 文档同步更新 |

---

## 替代方案

### 替代 1: 不引入 backend 抽象, 单一 local 执行 (拒绝)

**否决理由**: 与 ADR-0071 §D6 LLM-native 沙箱需求不兼容; 测试/部署场景无法隔离; 安全审计缺失。

### 替代 2: 仅 LocalBackend, 不做 DockerBackend (拒绝)

**否决理由**: 用户已确认 "Local + Docker 双 backend"; CI/部署场景强需求 Docker 隔离; 估时 +1 周可接受。

### 替代 3: 用已有库 (e.g. libcontainer, containerd) 替代 docker exec REST API (暂不采纳)

**思路**: 直接集成 containerd 客户端, 跳过 docker daemon。

**未采纳理由**: 复杂度收益不匹配; libcontainer/containerd C++ 集成成本 > REST API; 用户项目当前用 docker daemon (CI 验证)。

### 替代 4: 4 个 backend 一次性实施 (Local/Docker/K8s/SSH) (拒绝)

**否决理由**: 与 ADR-0071 §D6 Phase 1+2 计划不符; K8s/SSH 场景用户量不足 (Phase 6 内部 demo 不需要); 估时 4-6 周超容量。

### 替代 5: backend 字段保留 `env:` 名称 (拒绝)

**否决理由**: ADR-0071 §D5 已批准重命名为 `backend:`; 与 `ExecOptions.env` 命名冲突, LLM 困惑; 字段歧义无法靠 prompt 解决。

---

## 影响范围

### 文档
- [`docs/specs/dsl.md`](../specs/dsl.md) — §6 shell.exec 节点新增 `backend:` 字段示例
- `docs/specs/env-backend.md` (新增) — IEnvBackend 接口契约 + 2 个 backend 实现规范
- `docs/security/backend-policy.md` (新增) — BackendPolicy 默认策略 + per-environment 配置

### 代码
- `include/agenticdsl/env/env_backend.h` (新增) — IEnvBackend + ExecRequest/Options/Result
- `include/agenticdsl/env/local_backend.h` (新增) — LocalBackend 类
- `include/agenticdsl/env/docker_backend.h` (新增) — DockerBackend 类
- `src/common/env/local_backend.cpp` (新增) — fork + execve 实现
- `src/common/env/docker_backend.cpp` (新增) — libcurl + Docker REST API
- `src/common/env/backend_factory.cpp` (新增) — `create_backend(backend_spec, config)` 工厂
- `src/common/policy/backend_policy.h/cpp` (新增) — BackendPolicy 配置 + 默认策略
- `src/common/hooks/env_validation_hook.cpp` (新增) — ToolCoordinator pre-hook (D5)
- `src/modules/parser/markdown_parser.cpp` — DSL 解析 `backend:` 字段 (D4 重命名)

### 测试
- `tests/test_local_backend.cpp` (新增) — fork + exec + 超时 + 输出截断
- `tests/test_docker_backend.cpp` (新增) — docker exec + ephemeral container + 资源限制
- `tests/test_backend_factory.cpp` (新增) — `create_backend("local")` / `create_backend("docker:...")`
- `tests/test_env_validation_hook.cpp` (新增) — EnvValidationHook policy 检查
- `tests/test_backend_policy.cpp` (新增) — 默认策略 + per-environment 配置
- `tests/test_shell_exec_backend_field.cpp` (新增) — DSL 解析 `backend:` 字段 + `env:` → `env_vars:` 别名
- `tests/test_backend_security.cpp` (新增) — shell 注入 + Docker privileged mode 拒绝

### 生态
- `examples/` 7 个 demo — 1-2 个 demo 增加 `backend: docker:python:3.12` 示例
- `lib/` stdlib 20 个子图 — `shell.*` 子图标记 backend policy
- ADR-0076 (MCP Server) — LocalBackend 复用 MCP stdio 传输模式

---

## 后续

### 短期 (Wave 3 Phase 1+2 启动后 1 周内)

1. 创建 `include/agenticdsl/env/` 目录骨架 (env_backend.h + ExecRequest/Options/Result)
2. 实施 LocalBackend (fork + execve), 复用 `pkm_temporal_demo` popen 模式
3. LocalBackend 单元测试 (含 shell 注入 / 超时 / 输出截断)
4. DSL 解析器支持 `backend:` 字段 + `env:` → `env_vars:` 别名

### 中期 (Wave 3 Phase 1+2 准出前)

5. 实施 DockerBackend (libcurl + Docker REST API)
6. DockerBackend 单元测试 (ephemeral container + 资源限制 + privileged 拒绝)
7. 实施 EnvValidationHook + BackendPolicy 默认策略
8. ToolCoordinator execute 流集成 EnvValidationHook
9. CI 测试覆盖: OWASP shell 注入 + Docker security best practices

### Wave 3 末衔接 (ADR-0076 DSL Engine as MCP Server)

10. MCP `stdio` 传输模式复用 LocalBackend 架构
11. MCP client 拉取的外部 tool backend policy 默认 `docker` 隔离

### 长期 (Phase 3+ 推迟)

12. K8sBackend — K8s exec API (Phase 7+ 估时 2-3 周)
13. SSHBackend — libssh 集成 (Phase 7+ 估时 1-2 周)
14. Backend warm pool (避免每次 exec cold start) (Phase 8+ 评估)

---

## 复审节点

- **Wave 3 Phase 1 (LocalBackend) ship 时**: 本 ADR 状态从 🔍 Proposed → 🟡 Partial (D1+D2 已实施)
- **Wave 3 Phase 2 (DockerBackend) ship 时**: 本 ADR 状态保持 🟡 Partial (D3 已实施, D4+D5 进行中)
- **Wave 3 末 (DSL Engine as MCP Server ship) 时**: 本 ADR 状态从 🟡 Partial → ✅ Approved (D1-D5 全部 ship)
- **K8sBackend / SSHBackend ship 时** (Phase 7+): 独立 ADR 立项, 本 ADR 仅追加 §后续 长期项

---

*文档版本: v1.0*
*创建日期: 2026-08-03*
*作者: HydraForge 架构组*
*状态: 🔍 Proposed (Wave 3 Phase 1+2 ADR; Local + Docker; 衔接 ADR-0069 hooks; K8s/SSH 推迟; 待架构组评审)*