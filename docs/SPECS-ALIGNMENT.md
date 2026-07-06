# 规范对齐计划

> ⚠️ **DEPRECATED (2026-07-06)** — 本文件自标 ⚠ 半准确,不再维护。
>
> 当前生效 spec 集见 [`docs/specs/`](specs/) 根目录的 5 个有效规范 (`architecture.md` + `layer0.md` + `layer0-refactor.md` + `dsl.md` + `stdlib-v3.10.md` + `memory-v3.10.md`)。
>
> 历史归档 spec (Phase 2 标准库 v1.0 等) 见 `docs/archive/specs/`。
>
> 本文件保留仅供未来追溯(2026-06-12 重新审计 §结论);实际维护以 `docs/adr-management/` 中的状态词汇表与 ADR 关系图为权威。

## 背景

我们已创建 9 个 ADR，定义了 Phase 1 的架构决策。以下规范文档需要更新以对齐。

## 需要更新的规范

### 🔴 高优先级

#### 1. `specs/dsl.md` (DSL 规范)

**需要更新以反映**：
- ADR-1: ILLMProvider 流式接口 (`generate_stream`, `IGenerationStream`)
- ADR-3: DSLEngine 线程安全 (多实例)
- ADR-8: 结构化 Context (LayeredContext)

**更新内容**：
- 新增 `dsl_call` 节点的流式模式
- 新增 `IGenerationStream` 类型定义
- 新增 `LayeredContext` 结构说明
- 更新 DSL 错误类型 (LLMError)

#### 2. `specs/stdlib-v3.10.md` (DSL 标准库规范，2026-06-12 合并自 dsl-lib + stdlib)

**需要更新以反映**：
- ADR-9: DSL 标准库结构 (`/lib/reasoning/`, `/lib/tools/`, `/lib/workflow/`)

**更新内容**：
- 更新标准库目录结构
- 新增 `react.md`, `plan.md` 等标准子图

#### 3. `specs/layer0.md` (L0 运行时规范)

**需要更新以反映**：
- ADR-3: DSLEngine 线程安全
- ADR-6: HarnessEngine 后台线程模型

**更新内容**：
- 新增 `HarnessEngine` 组件
- 新增 `Agent` 生命周期管理
- 新增 `std::jthread` 并发模型

---

### 🟡 中优先级

#### 4. `guides/developer-guide.md` (开发者指南)

**需要更新以反映**：
- ADR-4: ToolRegistry 安全模型
- ADR-7: Context 压缩
- ADR-8: 结构化 Context

**更新内容**：
- 新增"安全工具注册"章节
- 新增"状态管理"章节
- 新增"上下文压缩"章节

#### 5. `guides/rt-guide.md` (运行时指南)

**需要更新以反映**：
- ADR-2: EventBus 有界队列
- ADR-6: HarnessEngine

**更新内容**：
- 新增 EventBus 配置
- 新增 Harness CLI 使用说明

---

## 执行计划

| # | 任务 | 负责人 | 优先级 |
|---|------|--------|--------|
| 1 | 更新 `specs/dsl.md` 对齐 ADR-1,3,8 | TBA | 🔴 高 |
| 2 | 更新 `specs/stdlib-v3.10.md` 对齐 ADR-9 | TBA | 🔴 高 |
| 3 | 更新 `specs/layer0.md` 对齐 ADR-3,6 | TBA | 🔴 高 |
| 4 | 更新 `guides/developer-guide.md` | TBA | 🟡 中 |
| 5 | 更新 `guides/rt-guide.md` | TBA | 🟡 中 |

---

## 变更追踪

当规范更新后，在此记录（每条均经过 2026-06-12 重新审计验证）：

- [x] `specs/dsl.md` - ✅ 已对齐 (v3.10, 2026-06-13 §4.1 定义 LayeredContext; 与 ADR-0008 🟡 Partial 状态一致 — spec 已批准, code 由 Stage 3 实现)
- [x] `specs/stdlib-v3.10.md` - ✅ 已对齐 (v3.10, 2026-06-12 由 Stage 2 / Task 8 合并自 dsl-lib + stdlib)
- [x] `specs/memory-v3.10.md` - ✅ 已对齐 (v3.10, 2026-06-12 由 Stage 2 / Task 9 合并自 memory.md + dsl.md §10.3)
- [~] `specs/layer0.md` - ⚠️ **部分对齐**: §6.1 NodeExecutor 与 dsl.md §5.9 / G.2 已对齐 C1 状态; **§21 HarnessEngine (385 行) 与 ADR-0006 ⛔ Superseded 不一致** — 见 Stage 4 / Task 21 整改
- [~] `guides/developer-guide.md` - ⚠️ **部分对齐**: §4.4 ToolRegistry (ADR-4), §4.5 LayeredContext (ADR-8), §4.6 Context 压缩 (ADR-7) 已写入; 但 ADR-0004 安全模型仅 P1 实施, §4.4 描述超前
- [x] `guides/rt-guide.md` - ✅ 已对齐 (2026-06-12 重审: EventBus 配置 + HarnessEngine CLI 章节齐全, 引用 ADR-0006 状态已标 ⛔ Superseded)
- [ ] `specs/architecture.md` - 待更新（3 处 `llm_generate_dsl` 残留：L59, L220, L356 — 引用了不存在的原语）

### 重新审计发现（2026-06-12, Stage 2 / Task 10）

| 文件 | 之前的声称 | 重新审计结论 |
|------|-----------|-------------|
| `specs/dsl.md` (LayeredContext) | "✅ 已更新" | ✅ **准确**（v3.10 §4.1 定义完整，与 ADR-0008 🟡 Partial 状态自洽） |
| `specs/stdlib-v3.10.md` | "✅ 已更新" | ✅ **准确**（Stage 2 / Task 8 新合并，v3.10 20 子图齐全） |
| `specs/layer0.md` (HarnessEngine) | "✅ 已更新" | ⚠️ **不准确** — §21 HarnessEngine 详细描述了被 ADR-0020 替代的 ADR-0006 模型，应加废弃注脚 |
| `guides/developer-guide.md` | "✅ 已更新" | ⚠️ **部分准确** — 章节存在但 ADR-0004 安全模型仅 P1 实施，文档超前于代码 |
| `specs/architecture.md` | 待更新 | ✅ **仍待更新** — `llm_generate_dsl` 引用未清理 |
| `specs/architecture.md` 实际路径 | "应为 `docs/proposals/architecture/`" | ⚠️ **半准确** — 推测的"应有"路径不存在；该 spec 应保留在 `docs/specs/` 并清理 `llm_generate_dsl` |

### 待跟进项

1. `specs/layer0.md` §21 HarnessEngine 章节应加 ⛔ Superseded 横幅（指向 ADR-0020），类似 `AGENTS.md` 的处理
2. `specs/architecture.md` 需清理 `llm_generate_dsl` 引用，标记为已废弃原语
3. `guides/developer-guide.md` §4.4 应区分"已实现（ToolRegistry 本身）"与"未实施（安全模型层）"

---

**2026-06-12 重新审计**: Stage 2 / Task 10 完成本节。Stage 1 + Stage 2 累计 27 个 spec/ADR 状态变更已交叉验证。

*最后更新: 2026-06-12*

---

## 附录：本文件跟踪的审计变更

| 变更 ID | 链接 | 严重度 | 状态 |
|---------|------|:------:|:----:|
| `docs-code-alignment-fixes` | [OpenSpec change](openspec/changes/archive/2026-06-09-docs-code-alignment-fixes/) | 🔴 P0 4 / 🟠 P1 16 / 🟡 P2 8 | ✅ 已归档 |
| `tech-debt-and-doc-cleanup` | [OpenSpec change](openspec/changes/archive/2026-06-10-tech-debt-and-doc-cleanup/) | — | ✅ 已归档 |
| `phase1-toolresult-standardization` | [OpenSpec change](openspec/changes/archive/2026-06-16-phase1-toolresult-standardization/) | 🟢 Sprint 1a | ✅ 已归档 (2026-06-16) |
| `phase1-bus-integration` | [OpenSpec change](openspec/changes/archive/2026-06-17-phase1-bus-integration/) | 🟢 Sprint 1b | ✅ 已归档 (2026-06-17, 吸收 3 deep modules/ 移除) |
| `2026-06-15-residual-engine-h-decoupling` | [OpenSpec change](openspec/changes/2026-06-15-residual-engine-h-decoupling/) | 🔴 P0 10 / 🟠 P1 10 (审查发现) | 🟡 3/4 完成 (Sprint 1b 2026-06-17), R5 重分类为 P1 active, 估时 3 周 |
| `project-organization` (Stage 1+2) | [Plan](.omo/plans/project-organization.md) | — | 🔄 进行中 (Stage 1 [~] 待 commit, Stage 2 ✅ 4/6 任务) |