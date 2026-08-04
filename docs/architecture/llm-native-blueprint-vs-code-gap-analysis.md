# LLM-native 架构蓝图 vs 代码库差距分析

**生成日期**: 2026-08-03
**触发**: guide-arch Phase 2 起草 6 个新 LLM-native ADR (0072/0074/0075/0076/0077/0078), 引用 2 个父 ADR (0071/0073); Oracle 深度审查发现 5 项 MUST-FIX 已应用
**分析范围**: 8 个 LLM-native ADR (0071-0078) vs 代码库实施状态
**数据源**: `docs/adr/adr-0071` ~ `adr-0078`, Oracle 审查报告 (session `ses_037e12115ffeLkeR1QTIko0BHb`), `docs/active-status.md`, AGENTS.md, 源码扫描

---

## 一、总体状态概览

| 状态 | 计数 | 占比 | ADR 列表 |
|------|:---:|:----:|---------|
| ✅ Approved + 已 ship | 0 | 0% | (无, 0071-0078 全部 🔍 Proposed) |
| 🔍 Proposed + 强制实施 | 3 | 37.5% | 0072 D1+D4, 0074 D1-D4+D6+D7, 0075 D1-D5 |
| 🔍 Proposed + 候选启动 | 1 | 12.5% | 0076 (gated by Phase 6 Candidate B 启动条件) |
| 🔍 Proposed + descoped (docs-only) | 2 | 25% | 0077, 0078 |
| 🟡 Partial + 实施中 | 0 | 0% | (无, 0073 已 ship 但 ADR 状态 🔍) |
| 父 ADR (0071) + 已 ship 部分 (0073) | 2 | 25% | 0071 顶层, 0073 schema |
| **总计** | **8** | **100%** | |

**关键观察**:
- 8 个 ADR 全部处于 🔍 Proposed 状态, **无 Approved + 已 ship** — LLM-native 架构仍处于"蓝图阶段"
- ADR-0073 (Tool JSON Schema) 已部分 ship (Phase 5 Sprint 21, per AGENTS.md), 但 ADR 自身状态未翻牌 (docs/README.md 显示 ✅, ADR 内部 🔍 — 待 ADR-0073 状态对齐)
- 4 个 ADR 含 D-number 强制决策 (0072 D1+D4, 0074 D1-D4+D6+D7, 0075 D1-D5, 0076 D1+D2+D3+D5+D6+D7) — 总计约 19 个强制决策
- 14 个候选幻影主题在 4 个 ADR 声称 (0074/0075/0076/0077), 全部 ⚠️ pending (注册前置: ADR-0068 §附录 A amendment)

---

## 二、8 个 ADR 详细差距分析

### 2.1 ADR-0071: LLM-native AgenticDSL 架构 (顶层, Wave 2 锚定)

**状态**: 🔍 Proposed (2026-08-02, 顶层方向 ADR, 锚定 Phase 6+ 演化)

**实施差距**:

| D-Number | 决策 | 派生 ADR | 当前状态 |
|----------|------|---------|---------|
| D1 | 顶层架构方向 | (无派生, 自身) | 蓝图阶段 |
| D2 | DSL 规范升级 | (D3+D5 涵盖) | D3+D5 起草中 |
| D3 | DSL 节点扩展 | **ADR-0072** | ✅ 起草 (本会话) |
| D4 | Tool 契约 JSON Schema | **ADR-0073** | ✅ 起草 + 部分 ship |
| D5 | LLM 训练数据 + Prompt | **ADR-0074** | ✅ 起草 (本会话) |
| D6 | Backend 多环境 | **ADR-0075** | ✅ 起草 (本会话) |
| D7 | DSL Engine as MCP Server | **ADR-0076** | ✅ 起草 (本会话, gated) |
| D8 | gRPC 数据面 | **ADR-0077** | ✅ 起草 (Wave 4 descoped) |
| D9 | Fine-tune 延后 | **ADR-0078** | ✅ 起草 (Wave 5+ descoped) |

**完整度**: D1-D9 全部有派生 ADR 起草 ✅, 7/8 派生 ADR 含具体决策 D-number. D1 顶层方向作为"愿景"无实施.

**代码对齐**: 0/9 实施 (Wave 2-5+ 全部未启动).

---

### 2.2 ADR-0073: Tool JSON Schema 契约 (Wave 2.1)

**状态**: 🔍 Proposed (ADR 内部) / ✅ Approved (docs/README.md) — **状态不一致待修**

**实施差距**:
- 决策 D1-D6: JSON Schema 2020-12 选型 + V3 字段 + 运行时校验 + DECLARE_TOOL 自动生成 + 向后兼容 + Schema 版本
- 估时: 1-2 周
- 实际: **已部分 ship** per AGENTS.md (Phase 5 Sprint 21 ship, ToolMetadata V3 字段), 但 ADR 状态未翻牌 → 🟡 Partial 待 apply

**Oracle 审查发现 (NICE-TO-HAVE #10)**: docs/README.md L~12 与 ADR 文本状态不一致 — 推荐翻牌 🟡 Partial 或修正 README.

---

### 2.3 ADR-0072: DSL 节点扩展 (Wave 2.4, **GATED**)

**状态**: 🔍 Proposed (2026-08-03, 本会话起草)

**6 个决策**:

| D | 内容 | 状态 | 触发条件 |
|---|------|------|---------|
| **D1** | `stream: true` 扩展到 tool/shell/dsl | 强制 | 立即实施 |
| **D2** | `$var` 替代 `{{ }}` | 条件 | ADR-0074 Evidence Gate parse-valid < 85% |
| **D3** | declarative style (`exec:`) | 条件 | 用户反馈 + `85% ≤ parse-valid < 90%` 临界带 (**Oracle 修复 #3**) |
| **D4** | `backend:` 字段 | 强制 | 立即实施 (衔接 ADR-0075 D4) |
| **D5** | 双语法共存期 6 月 | 条件 | D2+D3 触发后强制 |
| **D6** | `try/catch/finally` | **OFF** | 双条件 (子图表达力不足 + Evidence Gate PASS) |

**Oracle MUST-FIX #3 已应用**: D3 触发条件 `parse-valid < 90%` → `85% ≤ x < 90%` 临界带 (避免与 ADR-0074 D4 GO gate ≥85% 不可达冲突).

**实施差距**: 0/6 实施. Wave 2.4 GATED, 等 Evidence Gate.

---

### 2.4 ADR-0074: Prompt Engineering + Evidence Gate (Wave 2.2)

**状态**: 🔍 Proposed (2026-08-03, 本会话起草)

**7 个决策**:

| D | 内容 | 状态 |
|---|------|------|
| **D1** | Few-shot Examples 采集 (30+) | 待实施 |
| **D2** | Held-out Golden Suite (≥50 tasks) | 待实施 |
| **D3** | Prompt Baseline 测量 (3 模型 × 2 指标) | 待实施 |
| **D4** | Evidence Gate Go/No-Go 阈值 | 待实施 (关键门控) |
| **D5** | 两阶段 Prompt 注入 (≤8k prefix) | 待实施 |
| **D6** | 训练数据 JSONL 格式 | 待实施 |
| **D7** | LLM DSL 失败事件分类 | 待实施 (2 候选主题 ⚠️ pending ADR-0068 amendment) |

**Oracle MUST-FIX #2 已应用**: D7 中 2 个候选幻影主题 `llm.dsl.parse_failed` / `llm.dsl.schema_validation_failed` 标注 "⚠️ pending + ADR-0068 §附录 A amendment PR".

**实施差距**: 0/7 实施. Wave 2.2 估时 2-3 周 (vs Phase 6a 37h 容量, 接近上限).

---

### 2.5 ADR-0075: EnvBackend Local + Docker (Wave 3 Phase 1+2)

**状态**: 🔍 Proposed (2026-08-03, 本会话起草)

**5 个决策**:

| D | 内容 | 状态 |
|---|------|------|
| **D1** | IEnvBackend 接口抽象 | 待实施 |
| **D2** | LocalBackend (fork+exec) | Phase 1, 1 周 |
| **D3** | DockerBackend (libcurl + REST) | Phase 2, 1 周 |
| **D4** | `backend:` 字段 DSL | 待实施 (衔接 ADR-0072 D4) |
| **D5** | EnvValidationHook ToolCoordinator 集成 | 待实施 (衔接 ADR-0069 hooks) |

**K8s/SSH 推迟**: 替代方案 #4 明确拒绝 "4 backend 一次性实施" (估时 4-6 周超容量).

**Oracle MUST-FIX #2 + #5 已应用**:
- 2 个候选事件 `env.backend.exec.start/end` + `env.backend.unavailable` 标注 "⚠️ pending + ADR-0068 §附录 A amendment"
- §不变量 3 "`backend:` 字段必填" → "推荐必填, 缺省 local (向后兼容 V3.10)"

**实施差距**: 0/5 实施. Wave 3 Phase 1+2 估时 2-3 周 (vs Phase 6b 44h 容量, 接近上限).

---

### 2.6 ADR-0076: DSL Engine as MCP Server (Wave 3 末, **GATED**)

**状态**: 🔍 Proposed (2026-08-03, 本会话起草)

**7 个决策**:

| D | 内容 | 状态 |
|---|------|------|
| **D1** | 双 transport (stdio + HTTP+SSE) | 待实施 |
| **D2** | 静态 token MVP | 待实施 |
| **D3** | tools/* 暴露 | 待实施 |
| **D4** | prompts/* 暴露 | 待实施 (衔接 ADR-0074 baseline) |
| **D5** | resources/* 暴露 | 待实施 |
| **D6** | inputSchema 零转换 | 待实施 (衔接 ADR-0073) |
| **D7** | 外部 MCP client 反向拉取 | 待实施 |

**Oracle MUST-FIX #1 + #2 已应用**:
- §状态 §关联 §替代方案 §文档尾 4 处 "IS Phase 6 Candidate B" → "**INTEGRATES WITH Phase 6 Candidate B** (gated by active-status.md §四)"
- 6 个候选幻影主题 (`mcp.server.{connected,disconnected,request,response}` + `mcp.client.{request,response}`) 标注 "⚠️ pending + ADR-0068 §附录 A amendment"

**Ship 启动条件** (per Oracle 修复):
- AgentForge ≥ Sprint 25 milestone (per active-status.md §四)
- Solo Dev 容量 ≥2 人 (当前 1 人, 37h/44h)
- 当前 ship **结构性暂缓**

**实施差距**: 0/7 实施. Wave 3 末估时 2-3 周 (vs Phase 6b 44h 容量, 接近上限 + 启动条件未满足).

---

### 2.7 ADR-0077: gRPC Data Plane (Wave 4, descoped docs-only)

**状态**: 🔍 Proposed (2026-08-03, 本会话起草, **docs-only**)

**7 个决策**:

| D | 内容 | 状态 |
|---|------|------|
| **D1** | 4 service (LLMDataPlane / BlobTransfer / RemoteExecutor / Telemetry) | Phase 7+ 实施 |
| **D2** | MCP/gRPC 路由规则 (64KB 阈值) | Phase 7+ 实施 |
| **D3** | mTLS Phase 2 鉴权 | Phase 8+ 实施 |
| **D4** | protobuf + grpc-cpp 集成 | Phase 7+ 实施 |
| **D5** | 4 grpc.* 事件主题 | Phase 7+ 实施 |
| **D6** | GRPCBackend EnvBackend 集成 | Phase 7+ 实施 |
| **D7** | ToolCoordinator 路由决策 | Phase 7+ 实施 |

**Oracle MUST-FIX #2 已应用**: D5 中 4 个候选事件 `grpc.stream.{start,chunk,end}` + `grpc.connection.{up,down}` 标注 "⚠️ pending + ADR-0068 §附录 A amendment"

**重新激活条件** (4 项): 团队 ≥2 人 / AgenticMind ship + LLMDataPlane 需求 / K8s 分布式部署 / MCP 阈值实测校准

**实施差距**: 0/7 实施, Phase 7+ 评估

---

### 2.8 ADR-0078: Fine-tune 基模 (Wave 5+, descoped docs-only)

**状态**: 🔍 Proposed (2026-08-03, 本会话起草, **docs-only**)

**7 个决策**: D1 4 维度评分 + D2 触发条件 + D3 训练数据来源 + D4 训练方法 + D5 评估方法学 + D6 AgenticMind 回流 + D7 serving 集成

**重新激活条件** (4 项): AgenticMind ship / Evidence Gate FAIL / 用户 ≥10 / Fine-tune 价格 ≤$1

**实施差距**: 0/7 实施, Phase 5+ 评估

---

## 三、Oracle MUST-FIX 5 项应用状态

| # | 修复内容 | 涉及 ADR | 应用位置 | 验证 |
|---|---------|---------|---------|------|
| **#1a** | "IS Candidate B" → "INTEGRATES WITH (gated)" | 0076 §状态 L5 | ✅ 应用 | 文本替换确认 |
| **#1b** | 同上 + 交叉引用 active-status.md | 0076 §关联 L14 | ✅ 应用 | 文本替换确认 |
| **#1c** | 同上 | 0076 文档尾 L686 | ✅ 应用 | 文本替换确认 |
| **#1d** | 措辞修正 + 注明仍 gated | 0076 §替代方案 L563 | ✅ 应用 | 文本替换确认 |
| **#1e** | "✅ D7 IS" → "⚠️ D7 INTEGRATES WITH" | 0071 §战略协调 L58 | ✅ 应用 | 文本替换确认 |
| **#2a** | 2 llm.dsl.* 主题 "⚠️ pending + ADR-0068 amendment" | 0074 D7 (L22, L375-376, L532, L529, L558) | ✅ 应用 | 5 处文本确认 |
| **#2b** | 2 env.backend.* 主题同 | 0075 §风险 (L20, L165, L368) | ✅ 应用 | 3 处文本确认 |
| **#2c** | 6 mcp.* 主题同 | 0076 D7 (L23, L486, L624-625) | ✅ 应用 | 4 处文本确认 |
| **#2d** | 4 grpc.* 主题同 | 0077 D5 (L392-395, L567) | ✅ 应用 | 6 处文本确认 |
| **#3** | `parse-valid < 90%` → `85% ≤ x < 90%` 临界带 | 0072 D3 (L54, L173-179) | ✅ 应用 | 文本替换 + 新增"为何用临界带"说明 |
| **#4** | 拆分 `env:` → `backend:` + `ExecOptions.env` → `env_vars:` 两个独立重命名 | 0071 §3.A L212 | ✅ 应用 | 拆分为 2 个独立声明 |
| **#5** | "`backend:` 字段必填" → "推荐必填, 缺省 local" | 0075 §不变量 3 L331 | ✅ 应用 | 文本替换确认 |

**总编辑数**: ~18 处, 跨 6 个 ADR (0071/0072/0074/0075/0076/0077)

**验证**: `tools/adr_lint.py` 72/72 PASS (零回归)

---

## 四、Solo Dev 容量 vs 估时 3.5-5x 超额分析

**Oracle 关键发现**:

| 维度 | 估时 | 容量 | 超额倍数 |
|------|------|------|:---:|
| **Wave 2** (0073+0074+0072) | 3-4 周 (24-32h) | Phase 6a 37h | ✅ 0.65-0.86x (可行) |
| **Wave 3** (0075+0076) | 4-6 周 (160-240h) | Phase 6b 44h | ❌ **3.6-5.5x 超额** |
| **总 LLM-native** (0071 + 7 派生) | 7-10 周 (280-400h) | Phase 6a+6b = 81h | ❌ **3.5-5x 超额** |

**Oracle 建议**:
> Solo Dev 无法承担 Wave 3 全 scope; 必须 descope 至 Wave 2 only (Schema + Prompt + 基础 Evidence Gate) 再审 Wave 3.

**实际推进策略** (建议):

```
Sprint 24 (Phase 6a, 2026-07-24 ~ 2026-08-05):
  ├─ pdk_chat_demo v1 (active, P0)
  ├─ pkm_temporal_demo PDK 骨架 (active, P0)
  └─ (Wave 2 决策不抢容量)

Sprint 25 (Phase 6b, 2026-08-05 ~ 2026-08-19):
  ├─ Wave 2 Phase 2.1-2.3 (ADR-0073 翻牌 + ADR-0074 baseline + ADR-0072 D1+D4 强制)
  │  估时: 24-32h / 容量 44h ✅ 可行
  └─ 跳 Wave 3 至 Sprint 26+ (待容量评估)

Sprint 26+:
  └─ Wave 3 评估 (0075 + 0076)
     ├─ 启动条件: AgentForge ≥ Sprint 25 + Solo Dev ≥2 人 (per active-status.md §四)
     └─ 当前: 结构性暂缓 (per Oracle 修复 #1)
```

---

## 五、跨 ADR 依赖图 + Wave 推进路径

```
                    ┌─────────────────────────────────┐
                    │  Wave 2 锚定 (Phase 6b 优先)    │
                    └─────────────────────────────────┘
                                       │
            ┌──────────────────────────┼──────────────────────────┐
            │                          │                          │
       ┌────▼─────┐               ┌─────▼─────┐              ┌─────▼─────┐
       │ ADR-0073 │               │ ADR-0074  │              │ ADR-0072  │
       │  Schema  │───────────────▶│  Prompt   │─────────────▶│  D1+D4    │
       │ (ship)   │  schema       │  Baseline │  Evidence    │  强制     │
       └──────────┘  snapshot     │  + Gate   │  Gate        └───────────┘
                                  └───────────┘
                                       │
                                       │ (Gate PASS)
                                       ▼
                              ┌─────────────┐
                              │ ADR-0072    │
                              │ D2/D3/D5/D6 │
                              │ 条件性触发   │
                              └─────────────┘

                    ┌─────────────────────────────────┐
                    │  Wave 3 (Phase 7+ 启动评估)     │
                    └─────────────────────────────────┘
                                       │
                       ┌───────────────┴───────────────┐
                       │                               │
                  ┌────▼─────┐                  ┌───────▼───────┐
                  │ ADR-0075 │─────────────────▶│ ADR-0076      │
                  │ EnvBackend│ stdio 模式      │ MCP Server    │
                  │ Local+Docker│ 复用          │ (gated by     │
                  │ Phase 1+2 │                 │  active-status│
                  └──────────┘                  │  Candidate B) │
                                                └───────────────┘

                    ┌─────────────────────────────────┐
                    │  Wave 4+ (Phase 7+ 评估)        │
                    └─────────────────────────────────┘
                                       │
                       ┌───────────────┴───────────────┐
                       │                               │
                  ┌────▼─────┐                  ┌───────▼───────┐
                  │ ADR-0077 │                  │ ADR-0078      │
                  │ gRPC     │                  │ Fine-tune     │
                  │ DataPlane│                  │ (AgenticMind  │
                  │ (docs-   │                  │  回流前置)    │
                  │  only)   │                  │ (docs-only)   │
                  └──────────┘                  └───────────────┘
```

**Wave 推进依赖链** (Oracle Wave Ordering 验证通过):

1. ✅ 0073 BEFORE 0074: schema snapshot 是 baseline 测量前提
2. ✅ 0074 BEFORE 0072: Evidence Gate 是 0072 D2/D3/D6 触发条件
3. ⚠️ 0075 → 0076 stdio 复用: 0075 LocalBackend 必须先 ship 1 周, 0076 才可启动 MCP server
4. ✅ 0072 D1 (stream:true 强制) 与 Gate 并行, 立即可 ship
5. ✅ 0077/0078 descoped 重新激活条件明确 (4 项各自)

---

## 六、关键风险与建议

### 高风险 (Oracle 指出 + 修复后状态)

| 风险 | 修复前 | 修复后 | 缓解状态 |
|------|--------|--------|---------|
| **战略层冲突**: 0076 IS Candidate B vs active-status.md 暂缓 | ❌ 矛盾 | ✅ "INTEGRATES WITH + gated" | 已缓解 |
| **事件注册层冲突**: 14 候选主题未在 ADR-0068 §附录 A | ❌ 违反 §决策 2 | ✅ "⚠️ pending + ADR-0068 amendment PR" | 已缓解 (需 PR) |
| **容量冲突**: Wave 3 4-6 周 vs 6b 44h | ❌ 3.5-5x 超额 | ⚠️ 显式 descope 路径 | **待用户决策** |
| **触发条件不可达**: D3 `<90%` 与 gate `≥85%` | ❌ 不可达 | ✅ 临界带 `85% ≤ x < 90%` | 已缓解 |
| **重命名叙述混淆**: env: vs ExecOptions.env | ❌ 混淆 | ✅ 两个独立声明 | 已缓解 |
| **必填/缺省矛盾**: 0072 D4 vs 0075 §不变量 3 | ❌ 矛盾 | ✅ "推荐必填, 缺省 local" | 已缓解 |

### 中风险 (未在 MUST-FIX 但应跟踪)

- **0075 EnvValidationHook 位置**: 0075 L261 hook 在 layer check **之前**, 与 ADR-0071 §不变量 7 canonical 顺序不一致 (Oracle SHOULD-FIX #7, 未应用)
- **0076 token 轮换**: "季度轮换建议" 在 §风险 不是 Decision (Oracle SHOULD-FIX #6, 未应用)
- **0072 `$var` 双语法 6 月**: 不现实, OpenSpec 工作流建议 2-Sprint + lint (Oracle SHOULD-FIX #9, 未应用)
- **0076 D7 外部 MCP tool `--allow-dangerous-external`**: 在 §风险 不在 Decision (Oracle SHOULD-FIX #8, 未应用)

### 低风险 (NICE-TO-HAVE)

- **ADR-0073 status 不一致**: docs/README.md vs ADR 内部 (Oracle #10)
- **ADR-0077 重激活条件 #4**: 缺 benchmark script 引用 (Oracle #11)

### 建议下一步行动

**A. 必须 (Oracle MUST-FIX 已全部应用, ✅)**

**B. 推荐 (用户决策)**:

1. **Phase 6b 优先 Wave 2 强制决策** (0072 D1+D4, 0074 D1-D4+D6+D7, 0073 status 翻牌) — 估时 24-32h, 与 Phase 6b 44h 容量匹配 ✅
2. **Wave 3 descope 至 Phase 7+** — 等启动条件 (AgentForge ≥ Sprint 25 + Solo Dev ≥2 人)
3. **提交 ADR-0068 §附录 A amendment PR** — 14 个候选幻影主题批量注册 (LLM.dsl.*, env.backend.*, mcp.*, grpc.*)

**C. 可选 (后续 Sprint)**:

4. 应用 SHOULD-FIX #6-#9 (token 轮换 Decision + hook 位置 + $var 共存期 + D7 dangerous flag)
5. 修正 docs/README.md ADR-0073 status (NICE-TO-HAVE #10)
6. ADR-0077 重激活条件 #4 增加 `tools/benchmark_mcp_vs_grpc.cpp` 引用

---

*文档版本: v1.0*
*创建日期: 2026-08-03*
*作者: Sisyphus (guide-arch Phase 3)*
*关联*: Oracle 审查 session `ses_037e12115ffeLkeR1QTIko0BHb`
*下一次更新*: Sprint 25 末 (Wave 2 ship 后) 或 Phase 7 启动评估时