# ADR Self-Review Checklist (标准化清单)

> **用途**: 每个 ADR 自审时引用此清单 (单一作者,自审 = 自决)
> **关联**: AGENTS.md "Single-Developer Mode" + `openspec/changes/2026-08-25-sprint-24-pre-launch-self-review/` Step 2
> **创建日期**: 2026-08-25
> **状态**: ✅ Active

---

## 一、清单结构 (3 大类 12 项)

### A. 设计完整性 (1-4)

- [ ] **A1. 背景与上下文** (`§背景` 段) — 清晰说明问题、动机、与既有架构的关系
- [ ] **A2. 决策 (Decision)** (`§决策` 段) — 列出具体决策项, 每项有推荐方案与理由
- [ ] **A3. 不变量 (Invariants)** (`§不变量` 段) — 列出实施后必须保持的属性 (类型/语义/接口)
- [ ] **A4. 触发条件** (`§触发` 段,如适用) — 列出 ADR 实施的前置条件与解锁动作

### B. 风险与备选 (5-8)

- [ ] **B1. 风险评估** (`§风险` 段) — ≥3 项风险, 每项含影响范围与缓解措施
- [ ] **B2. 备选方案** (`§备选` 段) — ≥2 项替代方案, 解释为何不采纳
- [ ] **B3. 反向影响** — 不影响已 ship 的能力, 或明确标注 breaking change
- [ ] **B4. 安全/合规评估** — 涉及 PII/凭据/隐私的决策需安全角色评估 (但 single-dev 模式由 Oracle 替代)

### C. 实施与依赖 (9-12)

- [ ] **C1. 实施计划** (`§实施` 段) — 列出具体实施步骤与估时
- [ ] **C2. 依赖关系** — ADR 引用的其他 ADR 全部存在且状态正确
- [ ] **C3. 契约层一致性** — 与 `include/agenticdsl/contract/` 现有契约协调 (命名/接口/语义)
- [ ] **C4. 文档同步** — `capability-application-map-2026-08.md` §二/§三/§四/§八 引用同步

---

## 二、专用清单 (按 ADR 类型)

### 2.1 接口契约类 ADR (如 ADR-0083 IEvaluator)

| # | 检查项 | 关键问题 |
|---|---|---|
| 1 | 接口位置 | `contract/` vs `evaluation/` (与现有契约层一致) |
| 2 | V1 范围 | 简化 vs 完整 (避免 ADR-0057 零实施重蹈) |
| 3 | 依赖类型 | 引用类型是否已定义? (避免循环依赖,如 ExecutionTrace) |
| 4 | 与现有类型关系 | 正交/扩展/替换? (避免破坏现有调用) |

### 2.2 协议/状态机类 ADR (如 ADR-0071 顶层架构)

| # | 检查项 | 关键问题 |
|---|---|---|
| 1 | 派生子 ADR | 列出所有派生子 ADR, 状态如何? |
| 2 | 触发条件 | 父未批 → 子项冻结 (避免 G13 类情况) |
| 3 | 是否需 amendment | 与现有 ADR 关系? 整合还是独立? |
| 4 | Promotion 路径 | 草案 → 评审 → Approved 流程完整性 |

### 2.3 安全/隐私敏感 ADR (如 ADR-0080 v1.2)

| # | 检查项 | 关键问题 |
|---|---|---|
| 1 | 失败模式 | fail-closed vs fail-open (默认安全) |
| 2 | 三重保护 | CLI 标志 + 路径前缀 + WARNING |
| 3 | PII 处理 | 脱敏/加密/隔离? |
| 4 | 审计链路 | 事件发射 (ADR-0068)? |

### 2.4 性能/路由类 ADR (如 ADR-0061-04 SLM)

| # | 检查项 | 关键问题 |
|---|---|---|
| 1 | 契约复用 | 是否扩展现有 IModelRouter? |
| 2 | 路由策略 | 优先/fallback/empty 处理 |
| 3 | 性能基准 | ≤X ms 延迟, ≥Y rps 吞吐 |
| 4 | 测试覆盖 | ≥5 cases + 10+ assertions |

---

## 三、决策框架 (3 选 1)

| 决策 | 触发条件 | 后续动作 |
|---|---|---|
| ✅ **Approved** | 12 项 + 专用清单全过 | 更新 ADR 状态字段 + capability-map |
| ❌ **Rejected** | 任一项不通过 + 无修改空间 | 修订后重新评审 (新 issue) |
| ⏸ **Deferred** | 前置未达 (如其他 ADR 未批) | 关闭 issue, 前置达成后重开 |

---

## 四、24h Cooling-Off 窗口

**目的**: 给"睡一觉再决定"留窗口, 避免冲动决策。

**规则**:
- Issue 创建后 24h 内为冷却期
- 冷却期内可更新 checklist / 修改决策
- 冷却期结束后填写"自审决策"节 (24h 后再决策)

**例外** (可缩短至 8h "睡一觉即可"):
- 紧急 hotfix (安全漏洞)
- 外部阻塞解除 (依赖到位)
- Sprint 收官前最后冲刺

---

## 五、与 Oracle 协作 (替代"安全/合规角色")

单人开发模式无独立安全/合规角色, 用 Oracle session 替代:

| 决策类型 | Oracle session 角色 |
|---|---|
| 隐私/数据治理 | Oracle 风险评估 + 替代方案分析 |
| 安全/攻击面 | Oracle 安全审计 + Threat Model |
| 性能/可扩展性 | Oracle 性能基准 + Spike 评估 |
| 架构一致性 | Oracle 架构评审 (与 ADR/契约层) |

Oracle 输出写入 ADR `## Oracle 评审` 节或 `## 风险评估` 节, 作为决策证据。

---

## 六、关联文档

- **使用入口**: `.github/ISSUE_TEMPLATE/adr-review.md` (issue body 模板)
- **使用流程**: `openspec/changes/2026-08-25-sprint-24-pre-launch-self-review/` Step 2
- **治理范式**: AGENTS.md "Single-Developer Mode"
- **能力地图**: `docs/architecture/capability-application-map-2026-08.md` §X (引用 §二/§三/§四/§八)

---

## 七、维护与演进

### 维护规则

- 清单更新触发: (1) 新增 ADR 类型 (扩展专用清单); (2) Oracle 评审发现新风险维度; (3) Single-Developer Mode 流程演进
- 修订流程: 在新 issue 中提议清单修改, Self-review 后 commit
- 版本兼容: 保留旧版本清单作为 `archive/architecture/` 参考

### 演进方向

- **当前**: 12 项通用 + 4 类专用 (覆盖 80% ADR 场景)
- **未来**: 引入 "Oracle auto-review" (post-write), 替代人工自审的机械检查
- **未来**: 与 ctest + LSP 集成 (强制 §实施 段引用既有测试)

---

**审批与维护**:
- 创建: 2026-08-25 (基于 Sprint 23 完成状态)
- 维护者: solo-dev
- 关联: AGENTS.md "Single-Developer Mode" 章节
- 下一修订: Sprint 24 收官 (评估清单是否覆盖本 Sprint 新增 ADR 类型)