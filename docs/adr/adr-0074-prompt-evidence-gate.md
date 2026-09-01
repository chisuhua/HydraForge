# ADR-0074: Prompt Engineering + Evidence Gate

## 状态

✅ Approved (2026-08-03 — 派生自 ADR-0071 §决策 D5, Wave 2 Phase 2.2 ADR; 衔接 ADR-0073 (Tool JSON Schema); Promotion 评审通过 2026-08-25; 实施 2-3 周)

> **V1 ship (2026-08-28, OpenSpec `t21-prompt-evidence-gate`)**: Prompt Evidence Gate V1 完整落地 — `PromptEvidenceGate` (Go ≥90% / Conditional 80-89% / No-Go <80% 阈值 + IEvaluator V2 CompositeEvaluator 集成) + `PromptAssembler` (两阶段注入 ≤8k tokens) + baseline 测量 (`tools/baseline/measure_prompt_baseline.py`, 3 MockLLM × 2 指标) + JSONL 导出 (`tools/prompt/export_training_data.py`) + 30 个 few-shot (`lib/prompt/few_shots/*.md`) + 54 个 golden tasks (`lib/prompt/golden/*.json`) + 3 主题注册 (`llm.dsl.parse_failed` / `llm.dsl.schema_validation_failed` / `prompt.token_limit_exceeded`, ADR-0068 附录 A v1.4)。验证: test_prompt_evidence_gate **19 cases / 338 assertions PASS**, 全量 ctest 动态基线 0 回归 (190/191, 1 pre-existing timing flake `test_event_log_query_perf`), 既有 7 契约零修改。Committs: `808817f`/`63ad838`/`178868d`/`70cdc41`/`b6ebf85` + ship commit。

> **隐含前置 (D7 主题注册)**: §决策 D7 设计 2 个新幻影主题 `llm.dsl.parse_failed` / `llm.dsl.schema_validation_failed`，需 ADR-0068 §附录 A amendment PR 注册。ADR-0068 Appendix A v1.2.2 已 ship (2026-08-27, 同步注册 `skill.compilation.*` 3 主题)，D7 主题注册前置 ✅ 满足（与 ADR-0068 amendment 集成 ship 路径明确）。

## 领域

L0 运行时 / L1 OS Services / LLM-DSL 协同 / Prompt Engineering / 训练数据采集 / 评估方法学

## 关联

### 父 ADR
- [ADR-0071 — LLM-native AgenticDSL 架构](./adr-0071-llm-native-agenticdsl-architecture.md) §决策 D5 (本 ADR 是 D5 的具体实施: 训练数据 + Prompt 策略 + Evidence Gate)
- ADR-0071 §决策 D4 (本 ADR 训练数据 schema_snapshot 锚定 D4 schema 字段)
- ADR-0071 §战略协调 (本 ADR 实施 ≤8k tokens prefix 是 Phase 6 容量协调的关键证明)

### 上游锚定
- [ADR-0001 — ILLMProvider 流式接口](./adr-0001-illm-provider-streaming-interface.md) — LLM 调用契约,本 ADR 使用 `generate()` 与流式回调
- [ADR-0008 — 结构化 Context](./adr-0008-structured-context.md) — LayeredContext (本 ADR Prompt 注入 working / episodic / semantic 三层)
- [ADR-0019 — IInteractionBus MVP](./adr-0019-iinteraction-bus-mvp.md) — LLM 失败事件 (`parse_failed` / `schema_validation_failed`) 通过 IInteractionBus 上报
- [ADR-0068 — 事件发射契约](./adr-0068-event-emission-contract.md) — 本 ADR 设计 **2 个候选幻影主题** `llm.dsl.parse_failed` / `llm.dsl.schema_validation_failed`, **需 ADR-0068 §附录 A amendment PR** 才能正式注册 (per ADR-0068 §决策 2 "新增/修改主题必须 PR 修订附录")
- [ADR-0069 — ToolCoordinator Hooks](./adr-0069-tool-coordinator-hooks.md) — pre-hook 注入 Prompt 增强 (e.g. 注入 stdlib subgraphs 提示)
- [ADR-0073 — Tool JSON Schema 契约](./adr-0073-tool-json-schema-contract.md) — 本 ADR Prompt 必须含 ToolMetadata V3 schema (≥D3) 作为 LLM 约束接口

### 平行/下游
- ADR-0075 (EnvBackend) — 本 ADR Prompt baseline 包含 env-aware 场景 (Local + Docker)
- ADR-0076 (DSL Engine as MCP Server) — 本 ADR Prompt 复用 MCP `inputSchema` 字段 (零额外成本, 衔接 D3)
- ADR-0078 (Fine-tune 基模, Wave 5+) — 本 ADR JSONL 训练数据是 Fine-tune 输入

### 规范
- [`docs/specs/dsl.md`](../specs/dsl.md) v3.10 — DSL 语法 (Prompt few-shot examples 来源)
- [`docs/specs/stdlib-v3.10.md`](../specs/stdlib-v3.10.md) — 20 个 stdlib 子图 (subgraph 选择 Prompt 内容)
- [`docs/specs/architecture.md`](../specs/architecture.md) — L0-L4 分层 (Evidence Gate 在 L0/L1 边界实施)
- JSONL 训练数据格式 — 详见 §决策 D6

---

## 背景

### 问题

LLM-native AgenticDSL 架构 (ADR-0071) 要求 LLM 直接生成 DSL。当前 5 个具体空白:

1. **Few-shot examples 缺失** — 现有 `examples/` 仅 7 个 demo, 数量与多样性远不足以引导 LLM 稳定生成 DSL (目标 ≥30)
2. **没有 held-out 评估集** — 无法量化"LLM 生成的 DSL 是否真能用" (目标 ≥50 任务, 多领域覆盖)
3. **Prompt baseline 未测量** — 不同 LLM (GPT-4 / Claude / DeepSeek) 对 DSL 语法理解差异巨大, 没有基线无法判断是否需要 Fine-tune
4. **没有 Go/No-Go 门控** — Wave 2 → Wave 3 推进没有客观标准, 容易陷入"看起来差不多但实际不能用"的循环
5. **Prompt 注入路径不清** — 一次性塞所有 stdlib (20+ subgraphs) 会超过 LLM context, 需要分阶段注入
6. **训练数据格式未定** — Wave 5+ Fine-tune 需要 JSONL, 现在不定义会欠技术债
7. **失败事件未分类** — `parse_failed` (语法错误) vs `schema_validation_failed` (语义错误) 处理策略不同, 当前散落在各 tool 实现

### 解决方案

建立 **Prompt Engineering + Evidence Gate** 闭环:
- **训练数据采集** (D1-D2): ≥30 few-shot examples + ≥50 held-out golden tasks
- **Baseline 测量** (D3): 3 个 LLM × 2 个指标 (parse-valid + task-success) → 量化基线
- **Go/No-Go 门控** (D4): 客观阈值定义 Wave 2 → Wave 3 推进标准
- **两阶段 Prompt 注入** (D5): 先选 subgraphs → 再生成 DSL, ≤8k tokens prefix
- **训练数据格式** (D6): JSONL with `dsl_version` + `schema_snapshot_hash` (Wave 5+ Fine-tune 兼容)
- **失败事件分类** (D7): 2 个新幻影主题 + 标准化错误反馈给 LLM retry

### 已实证证据

- **GPT-4 / Claude / DeepSeek 的 function-calling 模式**: 3 家均支持 JSON Schema 2020-12 作为约束 (ADR-0073 已确认), Prompt baseline 可直接对比
- **nlohmann/json_schema_validator**: 项目已 vendor, JSONL 训练数据 schema 校验零成本
- **Sub-agent 池** (Sprint 3 DomainWorkerPool): 1000× 并发验证通过, few-shot examples 批量生成与基线测量有充足算力
- **event_bus** (ADR-0019): 28 处 emit 统一, 失败事件分类零额外基础设施成本
- **AgenticDSL stdlib 20+ subgraphs**: 数量足够实施"两阶段注入"而不丢失关键能力

---

## 决策

### D1. Few-shot Examples 采集策略 (30+ examples)

**目标**: 构造 30+ 高质量 few-shot examples, 覆盖 DSL 主要使用模式。

**采集维度** (4 维 × 平均 8 examples = ≥32):

| 维度 | 数量 | 来源 |
|------|:---:|------|
| **领域** (auth/math/human/utils/inference) | 8 | `lib/*` 现有 stdlib 子图 |
| **节点类型** (tool_call/dsl/assign/fork/join/stream) | 8 | `docs/specs/dsl.md` §6 |
| **错误恢复** (重试/分支/回退) | 8 | `examples/agent_basic/agent_loop` ReAct 模式 |
| **MCP 互操作** (inputSchema 输出) | 8 | `pdk/llama_engine/` 12 工具演示 |

**Example 格式** (JSONL, 详见 D6):

```json
{
  "example_id": "auth_001",
  "user_input": "用 GitHub API 检查 PR 状态并发通知",
  "dsl_output": "type: dsl\nname: pr_check\nsteps:\n  - tool_call: github.get_pr\n  - tool_call: slack.notify\n  ...",
  "tags": ["auth", "tool_call", "mcp"],
  "difficulty": "easy"
}
```

**采集方法**:

1. **人工标注** (前 10 个): 架构师手写 + 评审 (质量基准)
2. **现有示例迁移** (中间 12 个): 从 `examples/` 7 个 demo + `lib/` 20 个子图提取, 标注输入输出映射
3. **LLM 辅助生成 + 人工筛选** (后 10 个): LLM 生成候选 → 架构师筛选 (efficiency 优先)

**质量门槛**:

- ✅ 通过 ADR-0073 schema 校验
- ✅ `parse-valid` (能解析为 ParsedGraph)
- ✅ `task-success` (能跑出预期结果, 见 D2 golden suite)
- ❌ 含 hardcoded 路径/密钥

### D2. Held-out Golden Suite 设计 (≥50 tasks)

**目标**: 50 个 held-out 任务 (与 few-shot 不重叠), 用于基线测量 + 回归测试。

**任务分层** (3 层 × 平均 17 tasks = ≥51):

| 层级 | 数量 | 描述 | 例子 |
|------|:---:|------|------|
| **L1 - 简单** | 20 | 1-3 步, 单一 tool | "读取 /etc/hostname" |
| **L2 - 中等** | 20 | 4-8 步, 含分支/循环 | "检查 PR 状态并发通知" |
| **L3 - 复杂** | 11 | ≥9 步, 多领域组合 | "构建完整 CI 流水线" |

**领域覆盖** (与 D1 对齐):

- auth (8) / human (8) / math (8) / utils (8) / inference (10) / mcp (8)

**任务文件结构**:

```
tests/golden_suite/
├── README.md                     # 任务列表 + 评分规则
├── tasks/
│   ├── auth_001.yaml             # user_input + expected_dsl + success_criteria
│   ├── ...
│   └── inference_010.yaml
├── expected_outputs/             # golden 输出 (人工或确定性 mock 生成)
└── scorer.cpp                    # 评分函数 (parse-valid + task-success)
```

**任务 YAML schema** (供 LLM Prompt 引用):

```yaml
task:
  id: "auth_001"
  domain: "auth"
  difficulty: "L2"
  user_input: "用 GitHub API 检查 PR 123 状态"
expected_dsl: |
  type: dsl
  steps:
    - tool_call: github.get_pr
      arguments:
        pr_number: 123
success_criteria:
  parse_valid: true        # 必须能 parse
  tool_calls_made: ["github.get_pr"]  # 必须调用预期 tool
  no_unexpected_calls: true  # 无意外 tool 调用
```

### D3. Prompt Baseline 测量方法 (3 模型 × 2 指标)

**目标**: 在 3 个 LLM 上测量 Prompt baseline, 量化"LLM 能否生成可用 DSL"。

**3 个 LLM** (代表性 + 已接入):

| LLM | Provider | 接入方式 |
|-----|----------|---------|
| GPT-4 Turbo | OpenAI | ADR-0005 factory |
| Claude 3.5 Sonnet | Anthropic | ADR-0005 factory |
| DeepSeek-V2 | DeepSeek | `examples/pdk_chat_demo` 已验证 |

**2 个指标**:

| 指标 | 定义 | 通过标准 |
|------|------|---------|
| **parse-valid** | LLM 输出能解析为 ParsedGraph (无语法错误) | ≥85% (L1+L2+L3 加权) |
| **task-success** | 执行 LLM 输出后达成 success_criteria | ≥70% (L1) / ≥50% (L2) / ≥30% (L3) |

**测量脚本**:

```bash
# 单次 baseline 测量 (50 任务 × 3 模型 = 150 次)
./tools/measure_prompt_baseline \
    --models gpt-4-turbo,claude-3-5-sonnet,deepseek-v2 \
    --golden-suite tests/golden_suite/ \
    --output docs/audits/2026-XX-XX-prompt-baseline-v1.json
```

**Prompt 模板** (≥ 4 版本, 用于 baseline → 优化的迭代):

1. **V0 - 裸 prompt**: 仅 "用 AgenticDSL 生成以下任务的 DSL: {user_input}"
2. **V1 - Schema 约束**: V0 + ToolMetadata V3 schema (ADR-0073 衔接)
3. **V2 - Few-shot**: V1 + 5 个 few-shot examples (从 D1 抽取)
4. **V3 - Subgraph 选择**: V2 + 两阶段注入 (D5)

**Baseline 报告格式** (JSON):

```json
{
  "baseline_id": "2026-XX-XX-V0",
  "prompt_version": "V0",
  "models": {
    "gpt-4-turbo": {
      "parse_valid": 0.92,
      "task_success": {"L1": 0.85, "L2": 0.55, "L3": 0.20},
      "total_tasks": 50,
      "avg_tokens": 1250
    },
    ...
  },
  "decision": "GO | NO-GO | CONDITIONAL"
}
```

> **V1 实施注记 (T21 ship 2026-08-28)**：D3 要求 GPT-4 Turbo / Claude 3.5 / DeepSeek 三模型 baseline 测量，但 V1 ship (`t21-prompt-evidence-gate`) 实际使用 **3 MockLLM × 2 指标** 跑 baseline（参 `tools/baseline/measure_prompt_baseline.py`）。两者偏差原因：
> - V1 用 MockLLM 是为验证 baseline 测量管线（解析协议 + 指标计算 + JSONL 输出）协议正确性
> - 真实 3 模型 baseline 是 V2 硬前置，必须在 **ADR-0072 启动前完成**（Oracle 二次审查识别，session `ses_fb4cd8ff8ffeJlYBgU3JogcnfB`）
> - Evidence Gate 决议文档（`docs/audits/2026-08-25-evidence-gate-momus-decision-summary.md`）当前基于 MockLLM baseline，效力弱于 ADR 原设计；ADR-0072 启动前必须用真实 baseline 复测

### D4. Evidence Gate Go/No-Go 阈值

**目标**: 客观定义 Wave 2 → Wave 3 推进标准, 避免"看起来差不多"陷阱。

**Go (推进 Wave 3) 阈值** — 全部满足:

| 条件 | 阈值 | 测量方法 |
|------|------|---------|
| **parse-valid** (3 模型平均) | ≥85% | D3 baseline |
| **task-success L1** (3 模型平均) | ≥70% | D3 baseline |
| **task-success L2** (3 模型平均) | ≥50% | D3 baseline |
| **task-success L3** (3 模型平均) | ≥30% | D3 baseline |
| **Prompt prefix tokens** | ≤8k | D5 测量 |
| **Few-shot coverage** (≥4 维度 × 8 examples) | 100% | D1 清单 |
| **Golden suite completeness** (≥50 tasks) | 100% | D2 清单 |
| **失败事件分类** (D7 2 主题) | 100% 上线 | IInteractionBus 验证 |

**No-Go (留在 Wave 2 + 补救) 触发**:

- 任意 1 项低于阈值 → No-Go
- 补救策略:
  - parse-valid < 85% → 增加 few-shot examples 数量到 50+ / 强化 schema 约束
  - task-success L1 < 70% → 简化 Golden suite / 重新标注 expected_dsl
  - Prompt prefix > 8k → 强化 D5 两阶段注入
  - 失败事件缺失 → 强制要求 D7 实施

**Conditional Go** (特定子 Wave 推进):

- 仅 parse-valid ≥85% 但 task-success 不足 → Wave 3a (MCP server) 推进, Wave 3b (JSON IR) 推迟
- 仅 GPT-4 达标 → 暂停 Wave 3, 评估是否需要多模型 fallback

**复审节奏**:

- baseline 测量后 24h 内: Evidence Gate 决议 (GO / NO-GO / CONDITIONAL)
- 决议记录在 `docs/audits/<date>-evidence-gate-<version>.md`

> **Evidence Gate 门控范围注记 (Oracle 2026-08-29 追加)**：D4 "Evidence Gate 不可绕过" 的实际**门控范围 = 仅 ADR-0072 语法扩展**（条件性节点扩展 = try/catch / env: / stream: / $var 等 LLM-native DSL 语法），**不门控**：
> - ADR-0075 EnvBackend（L1 OS Services 抽象，工程能力）
> - ADR-0076 DSL Engine as MCP Server（Wave 2 + 控制面工程能力）
> - ADR-0077 gRPC Data Plane（Wave 4 descoped docs-only）
>
> **背景**：ADR-0075 ship 时间（2026-08-18）早于 Evidence Gate 决议时间（2026-08-25）7 天，形式上违反不变量 4。澄清后，ADR-0075 不在门控范围内，故不构成实际违规。本注记与 [ADR-0071 §Evidence Gate 门控范围注记](adr-0071-llm-native-agenticdsl-architecture.md#evidence-gate-门控范围注记-oracle-评审追加-2026-08-29) 保持一致。

### D5. 两阶段 Prompt 注入 (subgraphs 选择 → 生成, ≤8k prefix)

**目标**: Prompt prefix tokens ≤8k, 同时保留所有 20+ stdlib subgraph 可发现性。

**两阶段流程**:

```
[Stage 1] subgraph 选择 (~500 tokens)
  输入: user_input + subgraphs 摘要列表 (name + 1 句描述)
  输出: 选中的 subgraph 列表 (top-3 ~ top-5)
  Prompt: "从以下 subgraphs 中选择最相关的: {摘要列表}\n任务: {user_input}\n回复 JSON: {\"selected\": [...]}"

  ↓

[Stage 2] DSL 生成 (~7k tokens)
  输入: user_input + few-shot examples (D1) + 选中 subgraphs 详细 schema (ADR-0073)
  输出: DSL YAML
  Prompt: "用 AgenticDSL 生成: {user_input}\n可用 subgraph: {详细 schema}\n例子: {few-shot}"
```

**Stage 1 摘要列表** (≤500 tokens):

```yaml
subgraphs:
  - name: github.get_pr
    desc: "查询 GitHub PR 状态 (id, title, state, merged)"
  - name: slack.notify
    desc: "发 Slack 通知 (channel, text)"
  ... # 20 个全部列出
```

**Stage 2 详细 schema** (仅 Stage 1 选中的, ≤5 个):

- 来自 `lib/` 实际子图 (运行时拉取)
- ToolMetadata V3 schema (ADR-0073 衔接)
- 含 1 个相关 few-shot example (从 D1 抽取)

**Token 预算分解**:

| Stage | 组件 | Tokens |
|-------|------|:------:|
| Stage 1 | 系统 prompt | 200 |
| | subgraph 摘要 (20 个) | 400 |
| | user_input | 100 |
| | 输出指示 | 50 |
| | **Stage 1 合计** | **~750** |
| Stage 2 | 系统 prompt + few-shot (5) | 3000 |
| | subgraph schema (top-3) | 1500 |
| | user_input + 上下文 | 500 |
| | 输出指示 | 200 |
| | **Stage 2 合计** | **~5200** |
| **总计** | | **≤8k** |

**回退策略** (Stage 1 选中 0 个):

- 默认注入 top-3 most-used subgraphs (历史 30 天调用频次 top-3)
- 标记 `low_confidence`, 在 audit log 记录, 用于 baseline 迭代

### D6. 训练数据格式 (JSONL with dsl_version + schema_snapshot_hash)

**目标**: JSONL 格式兼容 Wave 5+ Fine-tune + 当前 baseline 测量。

**Schema** (每个 JSONL 行):

```json
{
  "record_id": "uuid-v4",
  "timestamp": "2026-XX-XXTHH:MM:SSZ",
  "model": "gpt-4-turbo-2024-XX",
  "prompt_version": "V3",
  "user_input": "用 GitHub API 检查 PR 状态",
  "dsl_output": "type: dsl\nname: pr_check\n...",
  "stage_1_selected": ["github.get_pr", "slack.notify"],
  "parse_valid": true,
  "task_success": true,
  "error_code": null,
  "context": {
    "dsl_version": "3.10",
    "schema_snapshot_hash": "sha256:abcd1234...",
    "tool_metadata_v": 3,
    "agenticdsl_commit": "cc8c7df"
  },
  "tags": ["auth", "tool_call"],
  "difficulty": "L2"
}
```

**关键字段**:

- `dsl_version`: DSL 规范版本 (e.g. "3.10"), 用于过滤过期训练样本
- `schema_snapshot_hash`: ToolMetadata V3 schema 哈希, 用于验证 schema 与训练样本一致
- `agenticdsl_commit`: 训练数据生成时的 git commit, 用于回溯
- `stage_1_selected`: Stage 1 子图选择 (用于 D5 两阶段注入训练)

**JSONL 文件组织**:

```
data/training/
├── 2026-XX-XX-baseline-V0.jsonl       # 每次 baseline 测量的全部输出
├── 2026-XX-XX-baseline-V1.jsonl
├── golden_tasks_with_dsl.jsonl        # D2 golden suite 的 expected_dsl (ground truth)
└── schema_snapshot_2026-XX-XX.json    # ToolMetadata V3 schema 完整快照
```

**Schema 校验**:

- 训练数据写入前用 ADR-0073 ToolSchemaValidator 校验
- CI 检查 `schema_snapshot_hash` 一致性 (修改 ToolMetadata 必须同步 hash)

**Fine-tune 兼容** (Wave 5+ ADR-0078):

- JSONL 格式兼容 OpenAI fine-tune API (`{"prompt": ..., "completion": ...}` 字段可通过转换器派生)
- Anthropic / DeepSeek fine-tune API 转换器在 ADR-0078 实施

### D7. LLM DSL 失败事件分类与回收 (parse_failed / schema_validation_failed)

**目标**: 标准化失败事件, 支持 LLM retry 反馈 + 训练数据标签。

**2 个新幻影主题** (ADR-0068 事件发射契约兼容):

| 主题 | 触发条件 | Payload |
|------|---------|---------|
| `llm.dsl.parse_failed` ⚠️ pending | MarkdownParser 无法解析 LLM 输出 | `{task_id, raw_output, error_offset, hint}` |
| `llm.dsl.schema_validation_failed` ⚠️ pending | ToolSchemaValidator 拒绝 | `{task_id, tool_name, errors[], hint}` |

**Payload schema**:

```json
{
  "task_id": "uuid-v4",
  "model": "gpt-4-turbo",
  "prompt_version": "V3",
  "stage": 2,
  "raw_output": "type: dsl\nname: pr_chk\n  ...",  // 截断到 1KB
  "error": {
    "code": "ERR_DSL_PARSE | ERR_SCHEMA_VALIDATION",
    "offset": 42,                    // parse_failed: 错误位置
    "errors": [                      // schema_validation_failed: 字段错误列表
      {"path": "/path", "message": "must be at least 1 character", "schema_path": "#/properties/path/minLength"}
    ],
    "hint": "Fix the DSL syntax / Check the tool input_schema"
  },
  "retry_count": 0
}
```

**回收策略** (3 种):

1. **自动 retry (default)**: 失败后自动注入 `hint` 到下一轮 Prompt, 最多 3 次
2. **Fallback model**: 3 次 retry 仍失败 → 切换到更强 LLM (e.g. GPT-4 → o1)
3. **人工标注**: 5 次 retry 仍失败 → 标记为 `human_intervention_required`, 进入训练数据 (负面样本)

**训练数据标签** (D6 衔接):

- 失败记录写入 `data/training/failures_<date>.jsonl`
- 字段 `error_code` 区分两类失败 (parse vs schema)
- 用于 Wave 5+ Fine-tune 强化"语法 + schema 约束"能力

**事件消费者** (现有 bus 拓扑):

- `bus.subscribe("llm.dsl.parse_failed", training_data_recorder)` — 写入 JSONL
- `bus.subscribe("llm.dsl.schema_validation_failed", training_data_recorder)` — 同上
- `bus.subscribe("llm.dsl.*_failed", retry_coordinator)` — 触发 retry + hint 注入

---

## 不变量

### 长期不变量

1. **JSONL 是训练数据唯一格式** — 不引入 CSV / Parquet / protobuf (兼容 Fine-tune API)
2. **`dsl_version` + `schema_snapshot_hash` 是必填字段** — 缺失的训练样本拒绝写入
3. **Prompt prefix tokens ≤8k** — 两阶段注入是唯一合法路径 (单阶段超 8k 即拒绝)
4. **Evidence Gate 阈值是 Wave 推进唯一标准** — 不允许"感觉差不多"的隐性推进
5. **失败事件分类严格 2 类** — `parse_failed` / `schema_validation_failed`, 不引入模糊中间态
6. **Few-shot examples 数量 ≥30, Golden suite ≥50** — 低于阈值 Evidence Gate 必 No-Go
7. **Baseline 测量用 3 模型** — 单模型 baseline 不构成 Wave 推进依据

### Evidence Gate 不变量 (与 §决策 D4 一致)

```
Evidence Gate 输入: parse_valid × 3 + task_success × 3 + tokens + few-shot + golden + events
Evidence Gate 输出: GO | NO-GO | CONDITIONAL
Evidence Gate 不可绕过 — 任何 Wave 推进必须附 Evidence Gate 决议文档
```

### 两阶段注入不变 (与 §决策 D5 一致)

```
Stage 1 (subgraph 选择) → Stage 2 (DSL 生成)
↓ 任一阶段失败 → 回退到 default subgraph (top-3 most-used) + low_confidence flag
Stage 2 必须含至少 1 个 few-shot example (从 D1)
```

---

## 风险

### 高风险

| 风险 | 缓解 |
|------|------|
| **Few-shot examples 数量不足 / 质量低** — D1 30+ 难达成, 或 examples 偏领域覆盖不全 | 4 维度强制要求 (D1); CI 检查每个 example 通过 parse + task-success; 每月 review 增加 5+ 新 examples |
| **Golden suite 任务偏简单** — D2 50 tasks 全是 L1 简单任务, baseline 虚高 | 强制 3 层比例 (20/20/11); L3 任务需架构组评审; baseline 报告必须按层报告, 不能加权平均 |
| **Evidence Gate 阈值过严 / 过松** — D4 阈值定义不准导致 Wave 推进阻塞或低质量推进 | 初版阈值基于 GPT-4 历史 baseline (Phase 5 pdk_chat_demo); 3 个月后根据实际数据调整; 阈值变更需架构组评审 |
| **Prompt prefix > 8k 实际** — D5 两阶段注入未能压到 8k 以内 | Stage 1 严控 ≤500 tokens; Stage 2 严格 schema 截断; baseline 测量强制报告 token 数; 超 8k 立即 No-Go |

### 中风险

| 风险 | 缓解 |
|------|------|
| **LLM API 升级导致 baseline 失效** — GPT-5 / Claude 4 发布, 旧 baseline 不再代表 | baseline 测量每次 LLM API 升级后 24h 内重测; 记录 model + timestamp; JSONL 包含 model 字段 |
| **训练数据漂移** — ToolMetadata 修改但未同步 schema_snapshot_hash | CI hook 在 ToolMetadata 改动时强制更新 hash; JSONL 写入校验 hash 一致性 |
| **失败事件重试风暴** — D7 retry 失败触发多次 LLM 调用, 成本飙升 | retry 上限 3 次 + 模型 fallback 1 次 + 人工标注兜底; audit log 记录 retry_count; 总成本监控 |
| **MCP 互操作 schema 不匹配** — D3 baseline 假设 MCP schema 与 ToolMetadata 一致, 实际可能漂移 | ADR-0076 (MCP server) 实施时 round-trip 验证 schema; baseline 测量包含 1 个 MCP-only 任务 |

### 低风险

| 风险 | 缓解 |
|------|------|
| **JSONL 文件体积膨胀** — 每次 baseline 测量产生新文件, 数月后 ≥10GB | 月度归档 (gzip); 仅保留最近 3 个月 + golden + failures 永久 |
| **Fine-tune 转换器未实施** — D6 JSONL 当前未对接 Fine-tune API | Wave 5+ ADR-0078 实施转换器; 当前 JSONL 设计已预留兼容字段 |

---

## 替代方案

### 替代 1: 不做 Evidence Gate, 直接推进 Wave 3 (拒绝)

**否决理由**: 与 ADR-0071 §战略协调 "Phase 6 容量协调" 不兼容; 重复 Phase 5 INFRASTRUCTURE 无门控导致的债务积累。

### 替代 2: Few-shot examples < 30 (拒绝)

**否决理由**: LLM 引导能力与 example 数量强相关; <30 难以稳定覆盖 4 维度。

### 替代 3: 单阶段 Prompt 注入 (拒绝)

**否决理由**: 一次性塞 20+ subgraph 详细 schema > 12k tokens, 超出 GPT-4 Turbo 16k context 的可用预算; 严重影响响应速度与成本。

### 替代 4: 不定义 Evidence Gate 阈值, 用 LLM-as-judge (暂不采纳)

**思路**: 用 GPT-4 评估其他 LLM 输出, 无需 hardcoded 阈值。

**未采纳理由**: 主观性高, 与 ADR-0071 "客观度量" 原则冲突; 评估成本叠加 (3 模型 × 50 tasks × judge 调用)。

### 替代 5: 训练数据用 SQLite / Protobuf (拒绝)

**否决理由**: Fine-tune API 优先支持 JSONL; 转换成本高; schema 校验复杂度提升。

---

## 影响范围

### 文档
- `tests/golden_suite/README.md` (新增) — 50 任务索引 + 评分规则
- `tools/measure_prompt_baseline/README.md` (新增) — baseline 测量脚本使用
- `docs/audits/2026-XX-XX-prompt-baseline-v1.json` (新增) — V0/V1/V2/V3 baseline 报告
- `docs/audits/2026-XX-XX-evidence-gate-<version>.md` (新增) — Evidence Gate 决议
- `docs/llm/prompt-template-v0..v3.md` (新增) — 4 个 Prompt 版本模板
- `docs/llm/training-data-format.md` (新增) — JSONL schema + schema_snapshot_hash 规范

### 代码
- `tools/measure_prompt_baseline.cpp` (新增) — baseline 测量 CLI
- `tests/golden_suite/scorer.cpp` (新增) — parse-valid + task-success 评分函数
- `src/common/llm/training_data_recorder.cpp` (新增) — JSONL 写入 + schema 校验
- `src/common/llm/retry_coordinator.cpp` (新增) — 失败事件 → retry + hint 注入
- `src/modules/cognitive/prompt_builder.cpp` (新增) — 两阶段 Prompt 注入 (D5)
- `src/common/tools/schema_validator.cpp` (新增, 衔接 ADR-0073) — schema_snapshot_hash 生成

### 测试
- `tests/test_few_shot_examples.cpp` (新增) — 30+ examples 通过 parse + task-success
- `tests/test_golden_suite.cpp` (新增) — 50 tasks 评分函数验证
- `tests/test_prompt_baseline.cpp` (新增) — baseline 测量脚本单元测试
- `tests/test_evidence_gate.cpp` (新增) — Go/No-Go 阈值判定逻辑
- `tests/test_two_stage_prompt.cpp` (新增) — Stage 1 + Stage 2 注入逻辑 + ≤8k 验证
- `tests/test_training_data_recorder.cpp` (新增) — JSONL 写入 + schema 校验
- `tests/test_failure_events.cpp` (新增) — 2 个新幻影主题 (注册前置: ADR-0068 §附录 A amendment) + retry 协调

### 事件 (ADR-0068 衔接)
- 设计 2 个候选幻影主题 (注册前置: ADR-0068 §附录 A amendment PR): `llm.dsl.parse_failed`, `llm.dsl.schema_validation_failed`
- Payload schema 标准化 (D7)
- 现有 28 处 emit 不变, 仅追加 2 个新主题

### 生态
- `examples/` 7 个 demo — 部分可作为 few-shot examples 来源 (D1)
- `lib/` 20 个 stdlib 子图 — Stage 1 摘要列表 + Stage 2 详细 schema 来源
- `pdk/llama_engine/` 12 工具 — D1 MCP 互操作 examples 来源

---

## 后续

### 短期 (Wave 2 Phase 2.2 启动后 1 周内)

1. 创建 `tests/golden_suite/` 目录 + 50 任务 YAML (前 20 个 L1)
2. 实施 `tools/measure_prompt_baseline.cpp` 骨架 (V0/V1 prompt)
3. 在 GPT-4 Turbo 上跑 V0 baseline (parse-valid + task-success)
4. 输出 baseline 报告 `docs/audits/2026-XX-XX-prompt-baseline-V0.json`

### 中期 (Wave 2 Phase 2.2 准出前)

5. Few-shot examples 扩充至 30+ (D1 4 维度)
6. Golden suite 扩充至 50 任务 (含 L2/L3)
7. 实施 V2 (few-shot) + V3 (两阶段) Prompt baseline
8. 实施 Evidence Gate 决议脚本 + GO/NO-GO 判定
9. 设计 2 个失败事件主题 (D7, 注册前置: ADR-0068 §附录 A amendment) + retry coordinator

### Wave 2 Phase 2.3 衔接 (ADR-0075 EnvBackend)

10. Prompt baseline 包含 env-aware 场景 (Local vs Docker)
11. few-shot examples 增加 5+ env-specific 例子

### Wave 3 衔接 (ADR-0076 DSL Engine as MCP Server)

12. Prompt baseline 包含 1 个 MCP-only 任务 (验证 MCP `inputSchema` round-trip)
13. JSONL 训练数据包含 MCP server 输出样本

### Wave 5+ 衔接 (ADR-0078 Fine-tune 基模)

14. JSONL → OpenAI / Anthropic / DeepSeek fine-tune API 转换器
15. Fine-tune 后重跑 baseline 验证提升

### 长期

16. Few-shot examples 持续扩充 (目标 100+)
17. Golden suite 持续扩充 (目标 200+ 任务, 含真实用户 case)
18. Evidence Gate 阈值根据实际数据季度调整

---

## 复审节点

- **Wave 2 Phase 2.2 准出时**: 本 ADR 状态从 🔍 Proposed → 🟡 Partial (D1-D7 已实施, baseline 已测量)
- **Wave 2 → Wave 3 Evidence Gate 通过时**: 本 ADR 状态从 🟡 Partial → ✅ Approved (D4 阈值全部达成)
- **Wave 3 MCP server ship 时**: 交叉验证 MCP `inputSchema` ↔ D3 baseline 任务
- **Wave 5+ Fine-tune ship 时**: 交叉验证 JSONL → Fine-tune API round-trip

### Evidence Gate 决议实证记录 (2026-09-02 C4 ship)

**决议来源**: OpenSpec `from-roadmap-phase-6c-evidence-gate` ✅ ship 2026-09-02 → 决议文档 [`docs/audits/2026-09-02-evidence-gate-v1.md`](../audits/2026-09-02-evidence-gate-v1.md)

**决议状态**: **Conditional** (🟡)

**决议依据**:
- 消费 baseline [`docs/audits/2026-08-18-execution-baseline-v1.yaml`](../audits/2026-08-18-execution-baseline-v1.yaml) 数据:
  - parse_valid_rate = **0.8824** (88.24%) — 临界带 [85.0%, 90.0%) 左闭右开
  - task_success L1 = 0.8500 (mock baseline)
  - task_success L2 = 0.6000 (mock baseline)
  - task_success L3 = 0.1818 (mock baseline)
  - **mock_mode = true** (file:16) — 不构成真实 LLM 能力结论 (per MOMUS REJECT 修正 X 路线)
- `evaluate_gate(0.8824, 0.85, 0.60, 0.1818)` → `GateStatus::Conditional` (per `src/common/prompts/evidence_gate.h:26` D-3 数据完整性 + D-4 左闭右开判定)
- Conditional 触发 ADR-0072 D3 declarative style (C6, 4h P0*) — 但决议文档仅记录建议，C5/C6 启动需独立 OpenSpec change + 人类评审 (per proposal Impact §MUST NOT)

**决议不变量保持**:
- ADR-0074 状态保持 ✅ Approved (2026-08-03 Promotion, per design D-5 修正: 不实施 🔍 Proposed → 🟡 Partial 翻牌;该目标状态不存在)
- Phase 7 启动条件项 #1 状态: 🟡 Conditional (active-status.md §四, 24h 同步)
- Evidence Gate 不可绕过 (per ADR-0074 §不变量 4: 任何 Wave 推进必须附 Gate 决议文档)

**决议后续**:
- 真实 3 模型 baseline 决议推迟至 Sprint 25+ (X 路线 ship 后由独立 OpenSpec change 实施)
- ADR-0074 §决策 D5 v2 amendment (task_success L1/L2/L3 完整判定) 推迟至 Sprint 25+
- Wave 3 (`from-roadmap-phase-6c-execution-dsl`) 启动条件: Evidence Gate 真实 PASS (parse-valid ≥85% + task-success L1 ≥70% from real LLM baseline)

**决议可审计性**: 所有数值引用具体 file:line 证据（如 `execution-baseline-v1.md:42`, `evaluation-baseline-v1.yaml:16`），详见决议文档 §决议字段表。

---

*文档版本: v1.0*
*创建日期: 2026-08-03*
*作者: HydraForge 架构组*
*状态: ✅ Approved (Wave 2 Phase 2.2 ADR; 衔接 ADR-0073; Promotion 评审通过 2026-08-25)*