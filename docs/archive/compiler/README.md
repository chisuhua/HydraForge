# SKILL Compiler 设计文档

> ⚠️ **本目录已于 2026-07-06 归档** (从 `docs/compiler/` 移至 `docs/archive/compiler/`)
>
> **编号冲突警告**: 本目录下的 `adr-0019` / `adr-0020` / `adr-0021` / `adr-0022` 编号
> 与主项目 [`docs/adr/`](../../adr/) 中的 ADR 编号**存在重叠**,但议题完全不同:
>
> | 本目录 ADR | 主项目 ADR |
> |-----------|-----------|
> | `adr-0019-dynamic-graph-execution-model.md` (Fork/Join) | `docs/adr/adr-0019-iinteraction-bus-mvp.md` (IInteractionBus) |
> | `adr-0020-skill-registry-lifecycle.md` (Skill Registry) | `docs/adr/adr-0020-thread-model-isolation.md` (线程模型) |
> | `adr-0021-skill-compiler-architecture.md` (SSL-as-IR) | `docs/adr/adr-0021-pdk-design.md` (PDK 设计) |
> | `adr-0022-bootstrap-loader-design.md` (Bootstrap) | `docs/adr/adr-0022-plugin-loading.md` (插件加载) |
>
> 该冲突对自动化工具链 (`tools/adr_lint.py`、`tools/adr_relationships.py`)**当前无害**,
> 因为这些工具仅扫描 `docs/adr/` + `docs/adr/plugin/`,**不扫描本目录**。
> 但人工搜索时需注意区分。
>
> 恢复方法 (如未来重新激活 SKILL Compiler 子项目): `git mv docs/archive/compiler docs/compiler`

**目录创建**: 2026-05-24
**归档时间**: 2026-07-06
**状态**: ⏸ 设计已决 (12/12 决策已确认),实施未启动 (Phase 5 主线聚焦 PDK + Plugin Loader,不依赖独立编译器)

## 概述

本文档系列覆盖 SKILL Compiler 的完整设计——从自然语言 SKILL.md 到可执行 AgenticDSL 计算图的编译管线（引入 SSL-as-IR 中间表示），以及编译器自身的自举闭环。

## 关键架构决策（已确认）

| # | 决策点 | 决策 | 依据 |
|---|--------|------|------|
| D1 | Fork/Join 并发模型 | **Option C**: jthread 域内并行，ForkNode→JoinNode 边界内并发执行分支 | 与 Kahn 调度器最小侵入；jthread 零外部依赖 |
| SR1 | 编译器中间表示 | **SSL-as-IR**: 三层 YAML（scheduling/structural/logical）嵌入 SKILL.md | 解耦 LLM 创作与确定性编译；零幻觉 P2 路径 |
| D7 | Phase 检测粒度 | **SSL structural.stages**: 结构层直接映射 phase，替代 LLM 自由提取 | SSL-as-IR 使 P4 变为确定性模板映射 |
| D9 | 编译/手写格式兼容 | **Option A**: 编译-创作分离，`enforce_ssl=true` 默认，transpile 为遗留技能转换路径 | 确定性自举需要；SSL 块作为可审查产物 |

## 文档索引

### 架构 ADRs（记录决策）

| 文档 | 覆盖决策点 | 状态 |
|------|-----------|------|
| `adr-0019-dynamic-graph-execution-model.md` | D1 (Fork/Join), D2 (动态图注入) | 已决 ✅ |
| `adr-0020-skill-registry-lifecycle.md` | D4 (Registry), D5 (特权工具) | 已决 ✅ |
| `adr-0021-skill-compiler-architecture.md` | SR1 (SSL), D7, D9, D11, D12 | D7/D9 已决 ✅ |
| `adr-0022-bootstrap-loader-design.md` | D6 (Bootstrap) + SSL-as-IR 集成 | 已决 ✅ |

### 设计 Specs（全部已决 ✅）

| 文档 | 内容 | 状态 |
|------|------|------|
| `spec-skill-md-format.md` | SKILL.md 格式（含 SSL YAML 块规范） | 已决 ✅ |
| `spec-ssl-ir-format.md` | SSL 三层 IR 精确 Schema + 版本策略 | 已决 ✅ |
| `spec-codelet-call.md` | `codelet_call` 节点类型 + Python 子进程执行 | 已决 ✅ |
| `spec-compiled-skill-format.md` | 编译产物标准 .agent.md 结构 | 已决 ✅ |
| `spec-bootstrap-loader.md` | 入口点 DAG（含 SSL 分类路由） | 已决 ✅ |
| `spec-ref-lazy-loading.md` | `[LOAD_REF]` 协议 | 已决 ✅ |

### 实施计划

| 文档 | 内容 | 状态 |
|------|------|------|
| `plan-phase1-foundation.md` | Fork/Join + codelet_call 基础设施 + 动态图注入 | 待细化 |
| `plan-phase2-skill-infrastructure.md` | Skill Registry + 特权工具 + transpile 命令 | 待讨论 |
| `plan-phase3-compiler-pipeline.md` | 编译器 7 Phase + SSL 路径 | 待讨论 |
| `plan-phase4-bootstrap-self-test.md` | Bootstrap + 自举验证 + 交叉编译 | 待讨论 |

## 全部决策总表（12/12 已决 ✅）

| # | 决策点 | 决定 | 文档 |
|---|--------|------|------|
| D1 | Fork/Join 并发模型 | Option C: jthread 域内并行 | `adr-0019` ✅ |
| D2 | 动态图注入机制 | 加固现有机制（Step 1 修复 + Step 2 优化） | `adr-0019` ✅ |
| SR1 | 编译器中间表示 | SSL-as-IR 三层 YAML | `adr-0021` ✅ |
| D4 | Registry 存储 | 文件扫描 + 运行时注册 + JSON 持久化 | `adr-0020` ✅ |
| D5 | 特权工具模型 | 独立 `privileged:` 命名空间 + 路径门控 | `adr-0020` ✅ |
| D6 | Bootstrap 形式 | 混合：C++ 初始化 + bootstrap.agent.md DSL 路由 | `adr-0022` ✅ |
| D7 | Phase 检测粒度 | SSL structural.stages 直接映射 | `adr-0021` ✅ |
| D9 | 格式兼容 | 编译-创作分离, enforce_ssl=true + transpile | `adr-0021` ✅ |
| D11 | 验证策略 | 交叉编译 + structure validation (validate-dsl.py) | `adr-0021` ✅ |
| D12 | 模块边界 | 分散到现有模块，不建新模块 | `adr-0021` ✅ |
| — | codelet_call 节点 | 新节点类型 + stdin/stdout JSON 子进程 | `spec-codelet-call` ✅ |
| — | Bootstrap + SSL | 4 种技能类型的分类路由 DAG | `spec-bootstrap-loader` ✅ |
