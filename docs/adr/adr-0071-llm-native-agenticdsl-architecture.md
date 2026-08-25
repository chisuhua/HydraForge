# ADR-0071: LLM-native AgenticDSL 架构 (LLM as DSL Author)

## 状态

✅ Approved (2026-08-02 — 顶层架构方向 ADR, 锚定 Phase 6+ 演化路径; 待架构组评审; 实施分 4 Wave 派生子 ADR/Change)

## 领域

L0 运行时 / L1 OS Services / DSL 语言演进 / LLM-DSL 协同

## 关联

### 上游锚定 (已存在)
- [ADR-0009 — DSL 标准库规划](./adr-0009-dsl-standard-library.md) — 标准库分层结构
- [ADR-0008 — 结构化 Context](./adr-0008-structured-context.md) — LayeredContext (L1-L5)
- [ADR-0001 — ILLMProvider 流式接口](./adr-0001-illm-provider-streaming-interface.md) — LLM 调用契约
- [ADR-0019 — IInteractionBus MVP](./adr-0019-iinteraction-bus-mvp.md) — 进程内事件总线
- [ADR-0021 — PDK 设计](./adr-0021-pdk-design.md) — Plugin 机制
- [ADR-0050 — Phase 6 战略评估](./adr-0050-phase6-strategic-evaluation.md) — Solo Dev 战略 (Candidate B 服务化) — **本 ADR 吸收 C20 轨道，详见 §战略协调**
- [ADR-0051 — Phase 6 PDK 组合 Spike](./adr-0051-phase6-pdk-composition-spike.md) — 服务化方向
- [ADR-0066 — SkillInterpreter 架构](./adr-0066-skill-interpreter-arch.md) — 沙箱隔离
- [ADR-0067 — L2/L3/L4 分层插件架构](./adr-0067-layered-plugin-architecture-split.md) — 插件分层

### 同 Wave 1 (进行中, 正交)
- [ADR-0068 — 事件发射契约](./adr-0068-event-emission-contract.md) — 28 处 emit 统一; 提供 LLM 训练数据采集的 observability
- [ADR-0069 — ToolCoordinator Hooks](./adr-0069-tool-coordinator-hooks.md) — pre/post 钩子; 强制 LLM 生成的 shell.exec 走 env 校验
- [ADR-0070 — DECLARE_COMMAND](./adr-0070-declare-command.md) — 人类输入层 Command registry; 不涉及 LLM 输出层

### 下游派生 (待创建, 本 ADR 列举)
- **ADR-0072**: DSL 节点扩展 (try/catch / env: / stream: / $var / declarative style)
- **ADR-0073**: Tool JSON Schema 契约 (input_schema / output_schema, JSON Schema 2020-12)
- **ADR-0074**: LLM 训练数据 + Prompt Engineering 策略
- **ADR-0075**: EnvBackend 多环境 (Local + Docker first, K8s/SSH later)
- **ADR-0076**: DSL Engine as MCP Server (控制面, MCP 2025-11-25 spec, 静态 token MVP)
- **ADR-0077**: gRPC Data Plane (数据面, protobuf + grpc-cpp)
### 规范

- [`docs/specs/dsl.md`](../specs/dsl.md) v3.10 — 现行 DSL 规范
- [`docs/specs/stdlib-v3.10.md`](../specs/stdlib-v3.10.md) — 标准库 v3.10
- [`docs/specs/architecture.md`](../specs/architecture.md) — AgenticOS 五层架构
- [`docs/proposals/vision/`](../proposals/vision/) — 自举愿景

### 战略协调: 与 ADR-0050 Candidate B 服务化路径的关系

ADR-0050 (2026-07-23) 选择 **Candidate B (服务化)** 作为 Phase 6 方向，聚焦 "暴露 HydraForge 能力为外部可消费服务 — InferenceServer MCP + OpenAI-compatible API"，估时 4-6 周。本 ADR-0071 在不废弃 ADR-0050 决策的前提下，**吸收并重新诠释**该路径：

| 维度 | ADR-0050 原路径 | ADR-0071 重新诠释 |
|------|-----------------|-----------------|
| **核心交付** | InferenceServer MCP + OpenAI-compatible API | DSL Engine as MCP Server (D7) — **同**一个东西的更聚焦版本 |
| **估时** | 4-6 周 (1-2 工程师) | Wave 3 ADR-0076 估时 2-3 周 (含 §Wave 2 重排后证据门) |
| **Phase 6 启动方式** | C20 analysis-service placeholder + C19 fork-checkpoint placeholder | D7 直接 ship，**不再需要 C19/C20 placeholder** |
| **支撑轨道** | ADR-0051 PDK 组合 Spike + `docs/service-composition/spike-onboarding.md` | 由 ADR-0076 复用 spike 产出作为 MCP server 实现参考 |
| **能力差异化** | "外部可消费服务" | "LLM-native DSL 引擎作为外部可消费服务" — 在 Candidate B 基础上叠加 LLM 生成能力 |
| **Oracle 重开条件** | §Candidate B 重开条件 (若条件不满足) | 仍生效；LLM-native 不构成替代路径 |

**结论**：

- ⚠️ **D7 (DSL Engine as MCP Server) INTEGRATES WITH Phase 6 Candidate B** — 但 ship **gated** by [`docs/active-status.md` §四](../active-status.md#四) "Candidate B 结构性暂缓" 启动条件 (AgentForge ≥ Sprint 25 milestone + Solo Dev 容量 ≥2 人); 启动条件未满足时 D7 仅作为本地 PDK 集成能力, 不进入服务化路径
- ✅ 本 ADR 不引入新工作量，仅把服务化路径与 LLM-native 方向**对齐**
- ✅ `docs/service-composition/` 目录下的 spike 产出（ADR-0051/C19/C20）作为 ADR-0076 实施参考保留
- ✅ ADR-0050 §Candidate B 重开条件**不变**：若服务稳定性瓶颈不解决，本 ADR-0076 自动阻塞

**容量约束一致性**：

ADR-0050 选 Candidate B 的核心理由是 "唯一匹配团队容量（1-2 工程师, 4-6 周）"。本 ADR-0071 派生路径估时：

- Wave 2 (Phase 2.1-2.4): **3-4 周** (Schema + Prompt + Evidence Gate + 条件性 Syntax)
- Wave 3 (D5 + D6): **4-6 周** (EnvBackend + DSL Engine as MCP Server)
- 总计: **7-10 周** for LLM-native + 服务化双重目标

**vs ADR-0050 单独估时 4-6 周**：增加 3-4 周（Wave 2 LLM-native 部分），但产出**超越** ADR-0050 — 不止服务化，还获得 LLM-native DSL 引擎。

**重开条件追加**：若 Wave 2 Evidence Gate 未达标（≤7 周内），LLM-native 部分 descoped，**仅保留** ADR-0050 服务化路径（D7 不变，但失去 LLM-native 增强）。

## 背景

### 范式问题

当前 LLM 调用外部执行的范式：

```
LLM (GPT-4 / Claude / DeepSeek)
  ↓ 生成 shell 命令
bash / zsh / powershell
  ↓ 进程执行
OS 资源
```

**3 个根本痛点**：

1. **平台碎片化**：LLM 必须为 bash / zsh / powershell / fish 各自学会语法；同一意图 4 种语法。
2. **工作流碎片化**：多步操作需要 LLM 多轮 round-trip；每步独立审批；上下文丢失。
3. **审计碎片化**：每次 round-trip 产生独立命令；缺乏节点级因果关系；事故溯源困难。

**3 个深层问题**：

4. **能力语义缺失**：LLM 不知道 AgenticDSL 已有 `/lib/tools/fs/read` 节点；只能手写 `cat / sed`。
5. **可验证性弱**：shell 命令执行结果难结构化；LLM 必须解析 stdout 字符串。
6. **生态绑定**：每换一个 LLM 必须重新微调 shell 模式；缺乏中立的"AI 执行语言"标准。

### 替代范式

```
LLM
  ↓ 原生生成 AgenticDSL markdown
DSLEngine (DSL runtime)
  ↓ 解析 + DAG 调度
Backend (local / docker / k8s / ssh / mcp / grpc)
  ↓ 进程/网络执行
资源
```

**6 个根本收益**：

1. **统一执行语义**：同一份 DSL 在 4 个 backend 行为一致；平台差异由 backend 吸收。
2. **多步工作流原生支持**：一次 DSL 提交 = 完整 DAG；LLM 单轮生成 8+ 节点。
3. **节点级审计**：每个节点产生结构化 Trace；因果链通过 CausalClock 串联 (ADR-0037)。
4. **能力语义丰富**：DSL 调用 `/lib/tools/fs/read` 而非 `cat`；可重试、可并发、可恢复。
5. **强可验证性**：`assert` 节点 + Trace + `expected_output` 形成 RLHF 数据闭环。
6. **生态中立**：AgenticDSL 作为"AI 时代的 shell"；不绑定具体 LLM 厂商。

### 范式证据

- **OpenAI Code Interpreter**：sandboxed Python 执行环境（类似范式）
- **Anthropic Computer Use**：tool use API 标准化（类似范式）
- **LangChain AgentExecutor**：action → tool → observation 循环（类似范式）
- **AutoGen**：structured output → tool execution（类似范式）
- **CodeAct**：LLM 直接生成代码在 sandbox 执行（最相似范式）

**AgenticDSL 的差异化**：DAG 声明式（vs 代码命令式）+ 节点级 Trace + `/lib/**` 强契约 + 跨后端 backend 抽象。

### 3 层文档实证缺口

(2026-08-02 复核 `docs/specs/dsl.md` + `adr-0009` + `stdlib-v3.10.md`)

| 缺口 | dsl.md | adr-0009 | stdlib-v3.10 | 影响 |
|------|--------|----------|--------------|------|
| 无 `try/catch` 节点族 | ❌ | ❌ | 部分（assert 替代） | LLM 无法声明错误恢复 |
| 无 `env:` 声明 | ❌ | ❌ | ❌ | 多环境执行不显式 |
| `stream: true` 仅 LLM | ❌ | ❌ | ❌ | shell/gRPC 无法流式 |
| Inja `{{ }}` 与 Markdown 冲突 | ✅ 已记录 | n/a | n/a | LLM prompt 解析歧义 |
| 无 JSON Schema input/output | ❌ | ❌ | ❌ | LLM 必须猜字段类型 |
| 10+ stdlib 子图未实现 | n/a | n/a | ❌ | LLM 实际可调用集与 spec 倒挂 |
| `tool: llm.call` 旧名 | 标记 deprecated | n/a | ❌ 未迁 | LLM 训练数据混新旧 |
| 缺 few-shot examples per subgraph | ❌ | ❌ | ❌ | LLM 难 few-shot 学习 |
| `available_subgraphs` prompt 段不标准化 | §9 有但散落 | §5.1 有 | §使用说明有 | LLM 训练数据难收集 |
| 缺错误模式文档 | ❌ | ❌ | ❌ | LLM 不知如何处理失败 |

**结论**：当前 DSL 3 层文档为**人类程序员**而设计，不为 **LLM 作者** 设计。

## 决策

### D1. 顶层方向：LLM-native AgenticDSL

AgenticDSL 演化的长期目标是成为 **可验证、可审计、约束动作空间的工作流语言**——LLM 原生生成、DSL 引擎原生执行、Backend 原生适配。

**核心断言**：

> LLM 应该直接生成 AgenticDSL markdown（DSL 作为**结构化动作输出**的标准形式），而不是生成 shell 命令。DSL runtime 通过 `tool: shell.exec` 等节点统一调用各 backend（local/docker/k8s/ssh/mcp/grpc），backend 负责吸收平台差异。

**真正差异化是安全/企业级定位**（不是"AI 时代的 shell"品牌）：
- 节点级因果 Trace（ADR-0037 CausalClock）
- Schema 验证的工具调用（D4）
- Approval-gated 执行（ADR-0031 ApprovalHandler）
- 跨 backend 的可移植语义

**非目标**：

- ❌ 取代 LLM 在 DSL 之外的用法（LLM 仍可直接生成任意文本）
- ❌ 强制所有 LLM 调用走 AgenticDSL（保留 `text_response` 路径）
- ❌ 取代 Skill 沙箱（沙箱层 + DSL 层正交，详见 §D6）
- ❌ 把 AgenticDSL 定位为"AI 时代的 shell"与 bash 竞争——LLM 预训练分布精通 bash/code；AgenticDSL 的差异化是**约束 + 审计**，不是语言可读性

### D2. 3 平面架构 (Operator / DSL / Backend Planes)

```
┌─────────────────────────────────────────────────────────────┐
│  Operator Plane — 人类接口 (极薄)                            │
│  `agenticdsl run workflow.agent.md` (10 行 launcher)        │
│  `agenticdsl shell` (可选 REPL, 复用 DSL 节点)              │
└──────────────────────────┬──────────────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────────────┐
│  DSL Plane — LLM 母语 (核心)                                │
│  DSLEngine runtime + L1-L5 LayeredContext + IInteractionBus│
│  节点族: start/end/assign/assert/if-else/try-catch/         │
│          tool_call (含 shell/mcp/grpc/fs 子类)             │
└──────────────────────────┬──────────────────────────────────┘
                           │
   ┌─────────┬─────────────┼──────────────┬──────────────┐
   ▼         ▼             ▼              ▼              ▼
 Local    Docker       K8s           SSH           MCP Server   gRPC
(Backend)(Backend)   (Backend)     (Backend)       (协议)     (协议)
```

**关键不变量**：

- DSL 是 LLM 输出的**唯一推荐格式**（prompt 强引导，工具调用模式）
- Backend 是 DSL 节点 `tool: shell.exec` 的执行目标，差异由 backend 吸收
- Operator 仅作为人类入口，不参与 LLM 调用

### D3. DSL 节点扩展（分层优先级，证据门驱动）

**ADR-0072 派生，详细设计见 ADR-0072。** 本 ADR 仅列出优先级分层：

#### 3.A 必补（高确定性，Wave 2 直接做）

| 节点/字段 | 必补原因 | LLM-native 价值 |
|----------|---------|----------------|
| `backend:` 字段 (workflow + node 级) | 跨 backend 透明执行 | LLM 显式声明 "this runs in docker" |

> **重命名说明** (**两个独立重命名, 避免 LLM 困惑**):
>
> 1. **`env:` → `backend:`** (backend-selection 字段, 执行目标选择) — 原 `env:` 字段与 `ExecOptions.env` (环境变量) 命名冲突, 重命名为 `backend:` 区分语义。
> 2. **`ExecOptions.env` → `env_vars:`** (env vars 字段, 进程环境变量) — 独立重命名避免与 backend 字段再次混淆 (ADR-0075 D4 实施)。
>
> 同时 "backend=production 必须审批" 的安全策略迁移到 ToolMetadata/capability flags，**不再耦合到字符串字面量**。

#### 3.B 条件性（取决于 Wave 2 Evidence Gate 测量结果）

| 节点/字段 | 条件 | 证据 |
|----------|------|------|
| `try` / `catch` 子图实现 | **先实现 `/lib/reasoning/try_catch@v1` 子图**（dsl.md:984 已 spec）；原生节点族仅在子图证明 ergonomic 不足时引入 | Wave 2 evidence gate 评估 ≥50 任务套件 |
| `$var` 替代 Inja `{{ }}` | 仅在 LLM 频繁生成 fenced DSL + `{{ }}` 引发 parse 失败时才引入 | Wave 2 prompt baseline 测量 parse-valid rate |
| `stream: true` (扩展到所有 tool) | shell/gRPC 也需流式；先做 LLM 流式 + tool 流式分阶段 | 当前 dsl.md 已支持 LLM 流式；tool 流式作为 Q2 后续 |
| `exec:` declarative style (可选) | LLM-friendly 节点语法；保留作为可选语法糖 | 用户反馈 |

#### 3.C 不做（明确排除）

- ❌ **原生 `try/catch/finally` 节点族（默认不做）**——违反 3 层架构（dsl.md §2.1）。先做子图，原生节点族需证明子图表达力不足。

**breaking change 政策**：

- Wave 2 默认零 breaking change：仅加 `backend:` 字段（additive）。
- `$var` 和原生 try/catch 节点族**仅在证据门通过后**进入 v3.11，二者均需 6 月双语法共存期。

### D4. Tool 契约升级：input_schema + output_schema (JSON Schema 2020-12)

**ADR-0073 派生。** ToolMetadata V3 扩展：

```yaml
type: tool_call
tool: shell.exec
input_schema:
  type: object
  properties:
    command: {type: string, minLength: 1}
    backend: {type: string, enum: [local, docker, k8s_prod, ssh_dev]}
    timeout: {type: integer, minimum: 0, default: 30000}
  required: [command]
output_schema:
  type: object
  properties:
    stdout:       {type: string}
    stderr:       {type: string}
    exit_code:    {type: integer}
    duration_ms:  {type: integer}
  required: [exit_code]
```

**Schema 双重职责**（不只是 Markdown 校验）：

1. **运行时校验** — DSL 解析后，args 必须匹配 `input_schema`，输出必须匹配 `output_schema`
2. **LLM 约束接口** — JSON Schema 2020-12 是 OpenAI structured outputs / Anthropic tool use / llama.cpp grammar 的共同输入格式；同 schema 可同时驱动 LLM 约束生成（Phase 3+ 引入）
3. **MCP 互操作** — MCP `inputSchema` 字段等同 JSON Schema 2020-12，D4 直接喂给 MCP server（零额外成本）

**Schema 来源**：PDK `DECLARE_TOOL` 宏**自动从 C++ 类型生成** schema（ADR-0004 V2 metadata 生成的同模式），避免手维护 schema 漂移。

**Validator 选型**：使用 `nlohmann/json_schema_validator`（项目已 vendor `nlohmann_json` 在 `external/`），**不自行实现**。

### D5. LLM 训练数据 + Prompt Engineering 策略

**ADR-0074 派生。** 分两阶段：

**Phase 1 — Prompt Engineering (0 成本, 2-3 周)**

- 30+ few-shot examples（自然语言 → AgenticDSL）
- 标准化 `available_subgraphs` prompt 段
- 节点手册 `docs/llm/agenticdsl-grammar.md`（LLM 友好）
- Schema 验证 + 错误反馈循环（LLM 看见 parse error 重试）
- **目标模型**：GPT-4 / Claude 3.5+ (90%+ 接受率)

**Phase 2 — Fine-tune (AgenticMind 项目, 决策延后)**

- 基模候选：Qwen2.5-Coder-7B / DeepSeek-Coder-6.7B / 端侧模型
- 训练数据：~500 (prompt, DSL) 对（Phase 1 examples + 合成）
- 评估：200 验证集，与 prompt baseline 对比
- **决策点**：AgenticMind 项目探索结果，**不在本 ADR 拍板**

**LLM 无关设计原则**：

- AgenticDSL 训练数据格式中立（不绑定具体 LLM API）
- Prompt 模板可移植到 Anthropic / OpenAI / 开源模型
- Tool schema 用 JSON Schema 2020-12 而非 LLM 私有格式

### D6. Backend 多环境 (Local + Docker first)

**ADR-0075 派生。** `EnvBackend` 抽象 + 4 个实现：

```cpp
class EnvBackend {
  virtual ExecResult exec(
    const std::string& cmd,           // 命令字符串
    const ExecOptions& opts           // env, cwd, timeout, capture
  ) = 0;
  virtual std::string name() const = 0;
};

class LocalBackend : public EnvBackend { ... };      // fork + exec
class DockerBackend : public EnvBackend { ... };    // docker exec API
class K8sBackend : public EnvBackend { ... };       // k8s exec API
class SSHBackend : public EnvBackend { ... };       // libssh
```

**Phase 1 实现顺序**：

1. `LocalBackend` (1 周) — fork+exec
2. `DockerBackend` (1 周) — docker exec API
3. `K8sBackend` / `SSHBackend` (Phase 2)

**DSL 节点绑定**：

```yaml
type: tool_call
tool: shell.exec
arguments:
  command: "ls -la /tmp"
  env: docker_prod          # docker_backend 查找
  timeout: 10000
```

**安全不变量**：

- `shell.exec` 必须经 ToolCoordinator (ADR-0031) layer check + ApprovalHandler
- `env: production` 强制 `force_approval_always: true`（不可 bypass）
- `shell.exec` 不在 Cognitive layer（仅 Workflow + Thinking）

### D7. 控制面：DSL Engine as MCP Server (Phase 3)

**ADR-0076 派生。** 锁定 **MCP 2025-11-25 spec (Streamable HTTP 稳定版)**，不追 2026-07-28 RC（变更中）。

**MCP server 暴露的工具**：

- `agent_run`: 调 `DSLEngine::run()`
- `agent_status`: 查询 session 状态
- `tools_list`: 列内部 tool + MCP client 拉到的 tool
- `skill_run`: 调 `SkillInterpreter`
- `shell_exec`: 调 EnvBackend（**仅在 backend policy 允许时**）

**MCP client 消费的外部 tool**：

- GitHub MCP / Slack MCP / Notion MCP / ...
- 自动注册到 `IToolRegistry` + `ToolCoordinator` 治理路径

**鉴权 MVP**：**静态 token** (Bearer token from env), OAuth 2.1 完整版 Phase 2。

**🔒 安全硬约束**（不变量）：

- **MVP 默认绑定 127.0.0.1**（仅 localhost）；非 loopback 绑定需显式配置 `--allow-network` flag
- **单租户假设**：静态 token 等同单一服务所有者；多租户等 OAuth 2.1 完整版（Phase 2）
- **MCP-Protocol-Version 头协商**：必须支持版本协商降级，避免单一 spec 锁定风险
- **静态 token 泄露 = 远程 shell RCE**：必须在 README/Dockerfile 显式警告，禁止默认开放网络端口

**跨进程 A2A 决策**：**❌ 不做**（用户决议）。理由：A2A 跨进程标准化尚未成熟（Google A2A 协议在进行中），先做 in-process A2A 增强（`InMemoryBus` + correlation_id，见 ADR-0068 后续）。

### D8. 数据面：gRPC for High-Throughput Channels (Phase 4)

**ADR-0077 派生。** 选择 **grpc-cpp + protobuf**。

**4 个核心 service**：

```protobuf
service LLMDataPlane {              // LLM token 流
  rpc StreamTokens(...) returns (stream TokenChunk);
}
service BlobTransfer {              // 大文件 / 模型权重
  rpc Upload(stream BlobChunk) returns (BlobResult);
  rpc Download(BlobRequest) returns (stream BlobChunk);
}
service RemoteExecutor {            // 远程执行 (替代 SSH)
  rpc Exec(stream ExecCommand) returns (stream ExecOutput);
}
service Telemetry {                 // OTel trace/span 上报
  rpc PushMetrics(stream Metric) returns (Ack);
}
```

**与 MCP 边界判定规则**：

- `payload < 64KB && !streaming` → MCP
- `payload >= 64KB || streaming` → gRPC

### D9. 训练基模延后决策

Fine-tune 基模选型**不在本 ADR 决策**。决议路径：

1. Phase 1 Prompt baseline 跑通（2-3 周）
2. AgenticMind 项目独立探索 fine-tune（4-6 周，并行）
3. AgenticMind 探索结果回流后，**新建 ADR-0078** 决策基模
4. 本 ADR-0071 仅承诺：训练数据格式中立 + JSON Schema 工具契约

## 不变量

### 长期不变量（永不破坏）

1. **DSL 路径命名空间**：`/lib/**`, `/dynamic/**`, `/main/**`, `/app/**` (dsl.md §6.1) — 不变
2. **三层架构**：执行原语层 / 标准原语层 / 知识应用层 (dsl.md §2.1) — 不变
3. **分层上下文 L1-L5** (ADR-0008) — 不变
4. **Skill 沙箱不替代**：MCP / gRPC 是协议层，Skill 是进程隔离层，正交保留
5. **资源声明 `/__meta__/resources`** (dsl.md §6.4) — 不变
6. **签名 `signature.inputs/outputs`** (dsl.md §6.2) — 仅扩展 JSON Schema 字段，不破坏现有文本契约

### 安全不变量（🔒 必须遵守）

7. **LLM 输出 = 不可信输入**。LLM 生成的 DSL 必须经过完整 sanitization pipeline 才能到达 backend：
   ```
   LLM 生成 → DSL parse → JSON Schema 验证 (D4) → ToolCoordinator layer check → ApprovalHandler → Backend
   ```
   任一阶段失败即拒绝，不允许 fallback 到 raw execution。
8. **Backend 是 hermetic 的**：跨 backend 数据流必须显式通过 `Context` / `output_keys`，**禁止通过文件系统假设**（local `/tmp` ≠ docker `/tmp` ≠ k8s `/tmp`）。
9. **`shell.exec` 安全门槛**（沿用 ADR-0031）：必须经 ToolCoordinator layer check + ApprovalHandler；Cognitive layer 禁用；`backend=production` 由 capability flag 触发 `force_approval_always`，**不耦合到字符串字面量**。

### 短期不变量（v3.11 期间）

10. `tool: llm.call` 旧名保留为 deprecated alias
11. `{{ }}` Inja 与 `$var` 共存（双语法解析期 6 个月）**仅在 Evidence Gate 通过后启动**
12. `tool_call` 旧签名 `tool + arguments + output_keys` 保留
13. `ctx["key"]` Context 访问保留（与新 `working.data.key` 共存）
14. **Wave 2 默认零 breaking change**：仅加 `backend:` 字段（additive），原生 try/catch 节点族推迟到证据门通过

## 风险

### 高风险

| 风险 | 缓解 |
|------|------|
| **LLM 训练数据偏差** — 合成数据可能放大模型既有偏差 | 真实人工标注 ≥ 30%；多模型 baseline 对比 |
| **DSL 表达力天花板** — 某些操作难在 DAG DSL 表达 | 保留 `tool: shell.exec` escape hatch（任意复杂操作委派 backend）|
| **Wave 2 投入但 LLM-native 失败** — 投入 4-6 周后 Evidence Gate 未达标 | 重排 Wave 2 把 schema+prompt 先做（零 breaking change）；fallback = 保持人类创作 + LLM 辅助 schema-constrained tool calls（备选 5 路径）|
| **MCP 协议仍在演进** — 2026-07-28 RC 变更可能破坏 2025-11-25 实现 | 锁定 2025-11-25 spec；D7 强制 `MCP-Protocol-Version` 头协商；6 个月后重新评估 |

### 中风险

| 风险 | 缓解 |
|------|------|
| **Backend 安全** — shell.exec 是 LLM 高频目标 | ToolCoordinator layer check + ApprovalHandler (ADR-0031) 强制；`backend=production` 由 capability flag 触发 force_approval；**禁止**耦合到字符串字面量 |
| **多 Backend 状态一致性** — local 与 docker 看到不同文件系统 | backend metadata 注入 LLM prompt；LLM 显式声明 backend；**不变量**：跨 backend 数据流仅通过 Context/output_keys |
| **MCP MVP 静态 token + 网络暴露** | D7 硬不变量：MVP 默认绑定 127.0.0.1；非 loopback 需 `--allow-network` flag + 显式警告 |
| **Prompt injection via shell.exec args** | LLM 输出 = 不可信输入；4 步 sanitization pipeline (parse → schema validate → layer check → approval) 强制 |
| **Fine-tune 模型漂移** — 基础模型升级破坏 fine-tune | 训练数据格式中立（JSONL with `prompt_template_version` + `dsl_version` + `tool_schema_snapshot_hash`）; CI 回归测试 |
| **EventBus 容量** — 7 个幻影主题真发射后高频事件可能拥塞 | ADR-0068 频率策略 + ADR-0037 causal clock 限流 |
| **LLM DSL 失败无可观测性** — parse 失败/验证失败/执行失败的事件流 | ADR-0074 scope 加入 `llm.dsl.parse_failed` / `llm.dsl.schema_validation_failed` / `llm.dsl.execution_failed` 事件；通过 ADR-0068 canonical topic registry |

### 低风险

| 风险 | 缓解 |
|------|------|
| **Tool schema 升级 breaking** | JSON Schema 2020-12 additive-only；CI schema diff 检查 |
| **gRPC 依赖体积（已被 descoped 为 Wave 4 暂缓）** | Wave 4 启动前必须有具体消费者；否则永久 descoped |
| **AgenticMind 项目延迟** | 训练数据 0 成本 baseline 已可用；fine-tune 是优化 |

### 已识别但推迟处理

| 风险 | 何时处理 |
|------|---------|
| **DSL 输出 verbosity tax** — Markdown DSL 比 bash 长 2-5x，输出 token 成本高 | Wave 3+ 引入 JSON IR（备选 5 部分采纳）解决；当前 Wave 2 接受 |
| **Inja 双语法期 LLM 困惑** | **不再是 high risk**——`$var` 推迟到 Evidence Gate 通过后才引入；6 月共存期仅在确实必要时启动 |
| **Prompt token 预算** — 20+ 子图 schema + 30 examples = 15-40k tokens | ADR-0074 scope: 两阶段 prompt 注入（先选 subgraphs → 再生成）；目标 ≤8k tokens prefix |

## 替代方案

### 替代 1：维持 LLM → shell 现状（否决）

**否决理由**：6 个根本收益全部丢失；与 OpenAI Code Interpreter / Anthropic Computer Use / CodeAct 趋势脱节；项目失去"AI 时代 shell"定位。

### 替代 2：建立独立 DSL-Lang 框架（否决）

**否决理由**：维护负担 × 2；用户认知成本 × 2；新框架 vs AgenticDSL v3.10+ 的差距可通过语法扩展补齐。

### 替代 3：只做 Tool Schema 不动 DSL 语法（部分采纳）

**采纳点**：Tool JSON Schema (D4) 独立于 DSL 节点扩展 (D3)，可先 ship Schema。

**未采纳点**：D3 的 try/catch / env: / stream: / $var 必须做，因为这是 LLM-native 的核心收益。

### 替代 4：Fine-tune 优先 (不采纳)

**未采纳理由**：训练数据是 fine-tune 的基础；先做 Phase 1 prompt + 数据采集，AgenticMind 探索 fine-tune 决策延后。

### 替代 5：Constrained-Decoding JSON IR (部分采纳 — Hybrid 模式)

**思路**：LLM 直接生成 **JSON IR**（受 JSON Schema 严格约束），runtime 渲染为 Markdown DSL 或直接执行 JSON IR。Markdown DSL 仅作为人类可读/审计形式存在，不作为 LLM 输出格式。

**采纳点**（Phase 3+ 增量引入，不替代 Markdown-first）：

- ✅ **D4 schema 双重职责** 已为此铺路：同一 JSON Schema 既是运行时校验，也是 LLM 约束接口
- ✅ **Parse 失败 ~100% 消除**：OpenAI structured outputs / Anthropic tool use / llama.cpp grammar 强制约束
- ✅ **Prompt token 节省**：不需要 30+ few-shot examples，仅 schema 即可驱动生成
- ✅ **开源模型兼容性**：llama.cpp grammar 正在快速成熟；vLLM guided JSON 已可用
- ✅ **Schema 单一事实源**：避免 Markdown 自由文本 + Schema 软约束的双重维护

**未采纳点**（Markdown 仍是 source of truth）：

- ❌ Markdown 的人类可读性 + diff/review/PR 友好性是核心审计能力，不能丢
- ❌ Round-trip 复杂度：需要 IR ↔ Markdown 无损转换工具
- ❌ 不是所有 LLM API 都支持 constrained decoding（边缘场景需 Markdown fallback）
- ❌ 工程复杂度增加：需要 IR parser、render 工具、validator 三件套

**Hybrid 设计**：

```
Phase 1 (Wave 2):
  LLM 训练数据采集 (prompt, markdown) ← Markdown-first
  DSL 解析器接受 markdown

Phase 2 (Wave 3+ 增量):
  同一训练数据生成 (prompt, json_ir) 平行格式
  DSL 解析器接受 json_ir (schema 约束)
  Round-trip 工具保证 markdown ↔ json_ir 无损

Phase 3 (远期):
  LLM API 支持时, 优先走 JSON IR (100% parse-valid)
  人类创作/审计仍用 Markdown
```

**结论**：JSON IR 是 **Wave 3+ 的优化**，不是 Wave 2 阻塞项。Wave 2 先做 Markdown-first + JSON Schema 双重职责（让 schema 准备好），Wave 3+ 视 LLM API 成熟度引入 IR。

## 派生 ADR / Change 路线图

| 编号 | 标题 | 估时 | Wave | 状态 |
|------|------|------|------|------|
| **ADR-0073** | Tool JSON Schema 契约 | 1-2 周 | Wave 2 (FIRST) | 🔍 待创建 |
| **ADR-0074** | LLM 训练数据 + Prompt Engineering 策略 + Evidence Gate | 1-2 周 | Wave 2 (SECOND) | 🔍 待创建 |
| **ADR-0072** | DSL 节点扩展 (`backend:` 必补, try/catch 子图化) | 2-3 周 | Wave 2 (GATED) | 🔍 待创建 |
| **ADR-0075** | EnvBackend 多环境 (Local + Docker first) | 2-3 周 | Wave 3 | 🔍 待创建 |
| **ADR-0076** | DSL Engine as MCP Server (控制面) | 2-3 周 | Wave 3 | 🔍 待创建 |
| **ADR-0077** | gRPC Data Plane (数据面, **descoped pending consumer**) | 2-3 周 | Wave 4 | ⏸ 暂缓 |
| **ADR-0078** | Fine-tune 基模选型 (AgenticMind 回流后) | 4-6 周 | Wave 5+ | ⏸ 延后 |

**Wave 2 关键变更（Oracle 反馈应用）**：

1. **重排顺序**：Schema (0073) → Prompt+Data (0074) → **Evidence Gate** → Syntax (0072)
   - 理由：Schema + Prompt 是 LLM-native 核心价值，**零 breaking change**
   - Syntax 扩展依赖 Evidence Gate 测量结果，**避免过早承诺**
2. **Wave 2 准出门槛**（go/no-go）：
   - ≥85% parse-valid rate on ≥50 任务 held-out suite
   - ≥70% task-success rate (端到端执行)
   - 在 ≥2/3 模型上达成 (GPT-4 / Claude 3.5+ / DeepSeek)
   - 未达 → Wave 3 重规划，**fallback: DSL 保持人类创作 + LLM 辅助 schema-constrained tool calls**（备选 5 路径）
3. **ADR-0072 估时修正**：2-3 周 → **3-4 周**（107 测试绿 + error routing 语义 + parser dual-syntax 风险）

**Wave 依赖图**：

```
Wave 1 (in-progress)          Wave 2 (gate-driven)              Wave 3                  Wave 4               Wave 5+
─────────────────────         ─────────────────────            ────────                ────────             ─────────
adr-0068 (events)   ──┐                                                              ┌──► ADR-0075
adr-0069 (hooks)    ──┼──►   0073 (Schema) ──► 0074 (Prompt) ──► EVIDENCE ──► 0072  ──┼──► ADR-0076
adr-0070 (commands) ──┘                                  │            GATE      (gated) │
                                                        └─ go/no-go ────────────────┘──► ADR-0077 (deferred)
```

**Wave 2 内 Phase 关系**：

- **Phase 2.1** (1-2 周): ADR-0073 Tool Schema — 零 breaking change
- **Phase 2.2** (1-2 周): ADR-0074 Prompt Engineering + 训练数据采集 + 评估 harness — 零 breaking change
- **Phase 2.3** (1 周): **Evidence Gate** — 测量 ≥50 任务套件，决定 0072 是否值得做
- **Phase 2.4** (2-3 周, 条件性): ADR-0072 DSL 节点扩展 — 仅在 Evidence Gate 通过后启动

## 实施检查清单

### Wave 2 准入 (本 ADR 决议生效后)
- [ ] Tool Schema V3 草案公开评审（ADR-0073）
- [ ] Prompt 模板初稿 + 30+ examples 启动采集（ADR-0074）
- [ ] **不启动** DSL v3.11 语法扩展（等 Evidence Gate）

### Wave 2 Phase 2.1 准出 (ADR-0073 Schema)
- [ ] `nlohmann/json_schema_validator` 集成 ship
- [ ] Tool JSON Schema 运行时验证器 ship
- [ ] PDK `DECLARE_TOOL` 宏**自动生成** schema（防漂移）
- [ ] 现有 tool 测试覆盖 schema 校验

### Wave 2 Phase 2.2 准出 (ADR-0074 Prompt)
- [ ] ≥50 任务 held-out golden suite ship（CI 集成）
- [ ] VCR-style recorded model responses ship
- [ ] 30+ few-shot examples 收集完成
- [ ] Prompt baseline 在 3 个 LLM (GPT-4 / Claude 3.5 / DeepSeek) 上跑通
- [ ] **两阶段 prompt 注入实现**：先选 subgraphs → 再生成（≤8k tokens prefix 目标）

### Wave 2 Phase 2.3 Evidence Gate
- [ ] **≥85% parse-valid rate** on ≥50 任务套件
- [ ] **≥70% task-success rate** (端到端执行)
- [ ] 在 **≥2/3 模型**上达成
- [ ] **未达 → fallback**：保持人类创作 + LLM 辅助 schema-constrained tool calls（备选 5 路径）

### Wave 2 Phase 2.4 准出 (ADR-0072 Syntax, 条件性)
- [ ] Evidence Gate 通过
- [ ] `backend:` 字段 ship（additive, 零 breaking）
- [ ] `/lib/reasoning/try_catch@v1` 子图实现 + 测试覆盖
- [ ] **仅在子图证明不充分时**：原生 try/catch 节点族（需新 ADR）
- [ ] `$var` 推迟到下一 Wave（除非 Evidence Gate 证明必要）

### Wave 3 准出 (ADR-0075 + ADR-0076)
- [ ] LocalBackend + DockerBackend E2E 测试通过
- [ ] DSL Engine as MCP server 端到端 demo
- [ ] **MCP server localhost-bind 默认** ship
- [ ] **MCP-Protocol-Version 头协商** ship
- [ ] 4 步 sanitization pipeline 测试 (parse → schema → layer → approval)

### Wave 4 准出 (ADR-0077, **仅在 Wave 3 末有具体消费者时启动**)
- [ ] gRPC 具体消费者确认（否则永久 descoped）
- [ ] gRPC service ship + 与 MCP 边界判定 lint

### 长期
- [ ] AgenticMind fine-tune 探索完成
- [ ] ADR-0078 基模选型决议
- [ ] LLM-native DSL 在生产环境跑通 3 个月
- [ ] JSON IR (备选 5) Phase 2 评估启动

## 影响范围

### 文档

- [`docs/specs/dsl.md`](../specs/dsl.md) — v3.10 → v3.11 升级
- [`docs/specs/stdlib-v3.10.md`](../specs/stdlib-v3.10.md) — 节点命名统一 + JSON Schema 补充
- [`docs/adr/adr-0009-dsl-standard-library.md`](./adr-0009-dsl-standard-library.md) — 路径版本号统一
- [`docs/llm/agenticdsl-grammar.md`](../llm/agenticdsl-grammar.md) — **新增** LLM-friendly 节点手册
- [`docs/llm/few-shot/`](../llm/few-shot/) — **新增** LLM 训练数据目录

### 代码

- `include/agenticdsl/dsl/parser.h` — `$var` 语法 + try/catch 解析
- `include/agenticdsl/dsl/tool_metadata.h` — JSON Schema 字段
- `include/agenticdsl/dsl/schema_validator.h` — **新增** JSON Schema 验证
- `src/core/backend/env_backend.h` — **新增** Backend 抽象
- `src/core/backend/local_backend.cpp` — **新增**
- `src/core/backend/docker_backend.cpp` — **新增**
- `src/core/mcp/dsl_mcp_server.cpp` — **新增** MCP server
- `src/core/mcp/mcp_tool_client.cpp` — **新增** MCP client
- `proto/agenticdsl_data.proto` — **新增** gRPC service 定义
- `src/core/grpc/grpc_data_channel.cpp` — **新增**

### 测试

- `tests/test_dsl_extension.cpp` — try/catch / env: / stream: 节点
- `tests/test_tool_schema.cpp` — JSON Schema 验证
- `tests/test_env_backend.cpp` — Local + Docker E2E
- `tests/test_mcp_server.cpp` — MCP server 端到端
- `tests/test_grpc_data_plane.cpp` — 4 service E2E

### 生态

- AgenticMind 项目（fine-tune 探索，外部）
- VSCode/Cursor 插件（消费 DSL Engine as MCP server）
- LLM 训练数据集（~500 对，开源）

## 后续

### 短期（Wave 2 准入后 1 周内）

1. 创建 `docs/llm/agenticdsl-grammar.md` 节点手册初稿
2. 启动 30+ few-shot examples 采集（从 `examples/` 7 个 example 扩展）
3. 创建 ADR-0072 / ADR-0073 / ADR-0074 三个 Wave 2 子 ADR
4. AgenticMind 项目 kickoff（fine-tune 探索）

### 中期（Wave 2 准出后）

5. 创建 ADR-0075 / ADR-0076（Wave 3）
6. 启动 LocalBackend / DockerBackend 实现
7. DSL Engine MCP server 端到端 demo

### 长期（Wave 4+ 准出后）

8. 创建 ADR-0077（gRPC）
9. 创建 ADR-0078（fine-tune 基模，AgenticMind 回流）
10. LLM-native AgenticDSL 正式 v4.0 发布

### 复审节点

- 每 Wave 准出后：架构组评审 + drift gate
- 6 个月后：MCP 2026-07-28 评估（是否升级 spec）
- 12 个月后：Fine-tune 效果评估（若已 ship）

---

*文档版本: v1.0*
*创建日期: 2026-08-02*
*作者: HydraForge 架构组*
*状态: ✅ Approved (待架构组评审)*
