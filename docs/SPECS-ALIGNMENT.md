# 规范对齐计划

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

#### 2. `specs/dsl-lib.md` (DSL 库规范)

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
| 1 | 更新 `specs/dsl.md` 对齐 ADR-1,3,8 | TBD | 🔴 高 |
| 2 | 更新 `specs/dsl-lib.md` 对齐 ADR-9 | TBD | 🔴 高 |
| 3 | 更新 `specs/layer0.md` 对齐 ADR-3,6 | TBD | 🔴 高 |
| 4 | 更新 `guides/developer-guide.md` | TBD | 🟡 中 |
| 5 | 更新 `guides/rt-guide.md` | TBD | 🟡 中 |

---

## 变更追踪

当规范更新后，在此记录：

- [x] `specs/dsl.md` - ✅ 已更新（v3.10，流式接口、LayeredContext）
- [x] `specs/dsl-lib.md` - ✅ 已更新（v3.10，完全重写）
- [x] `specs/layer0.md` - ✅ 已更新（HarnessEngine）
- [x] `guides/developer-guide.md` - ✅ 已更新（v3.10）
- [ ] `guides/rt-guide.md` - 待更新
- [ ] `specs/architecture.md` - 待更新（llm_generate_dsl 残留）

> **2026-06-09 审计修正（docs-code-alignment-fixes 变更）**：
> - `specs/dsl.md` (LayeredContext) — **该声称与代码不符,见 ADR-0008 当前状态** (`docs/README.md` 已降级为 ❌ 未实施)。
> - `specs/layer0.md` (HarnessEngine) — **该声称与代码不符**,ADR-0006 已被 ADR-0020 替代。
> - 本审计变更已修正 `docs/README.md` 的 ADR 状态标签,后续本节"已完成"勾选待重新审计后再勾选。
> - `specs/architecture.md` 实际路径应为 `docs/agenticdsl/architecture/`。

---

**2026-06-08 更新**: layer0.md §6.1（C1 NodeExecutor 示例）与 dsl.md §5.9 / G.2（C1 ILLMProvider 集成点）已对齐代码实际状态。

*最后更新: 2026-05-13*

---

## 附录：本文件跟踪的审计变更

| 变更 ID | 链接 | 严重度 | 状态 |
|---------|------|:------:|:----:|
| `docs-code-alignment-fixes` | [OpenSpec change](openspec/changes/docs-code-alignment-fixes/) | 🔴 P0 4 / 🟠 P1 16 / 🟡 P2 8 | 进行中 |