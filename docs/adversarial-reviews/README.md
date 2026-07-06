# Phase 5 B2 架构 Adversarial Review — 文件集入口

> **创建日期**: 2026-07-06
> **创建者**: Sisyphus (基于 4 个并行调研 agent: 3 explore + 1 librarian)
> **关联 session**: `ses_0cb1027ccffeN7BmCaOQTpQl1Y`(B2 深度审查) + `ses_0cb0ff434ffeQORz3s79pF0I3q`(C7 范式) + `ses_0cb0fbfdeffeWyeCdcN1Ja5kP1`(lib/inference 状态) + `ses_0cb0f8694ffec5qaKlCnGlh7r0`(外部对比)
> **状态**: ✅ 文档完整

---

## 背景

本文件集是 Phase 5 B2 推理标准库扩展 (C13/C14/C15) 的 **Adversarial Review** 产物。

### 审查动机

2026-07-05, Oracle 架构反思 (session `ses_0ce717ac4ffejvLa2We0gzbuds`) 将 B2 推理标准库 7 子图拆分为三 Change:
- **C13** (`phase5-b2-arch-schemas`): 4 个架构层 schema (prefix_cache/kv_cache/decoding/cloud_engine) + SamplerStrategy hook
- **C14** (`phase5-llama-engine-plugin`): pdk/llama_engine/ plugin 骨架 (engine/model 实现)
- **C15** (`phase5-batching-queue-plugin`): BatchingQueue 接口 + 第三方贡献流程

在启动实施前,用户要求先做 Adversarial Review + 架构梳理,以 B 评估三层架构边界的正确性。

### 审查方法

4 路并行调研:
1. **B2 openspec changes 深度审查** (explore) — 读取 3 个 change 的 proposal/spec/tasks 全文
2. **PDK Plugin 范式审查** (explore) — 读取 ADR-0034 C7 Model Router 已 ship 的实施经验
3. **B1 lib/inference 真实状态** (explore) — 检查 lib/inference/ 7 子图实际 ship 情况
4. **外部推理引擎对比** (librarian) — 调研 vLLM/SGLang/llama.cpp/TRT-LLM/TGI/LMDeploy/lit-gpt 7 个项目的插件抽象设计

### 核心结论 (TL;DR)

| 组件 | 评级 | 建议 |
|------|:----:|------|
| C13 4 个 .md schema | ✅ | 保留, 可立即 ship (纯 .md, 零 C++) |
| C13 SamplerStrategy hook | ❌ | **删除** — 1 虚接口仅 1 个实现, `supports()` 永远 true |
| C14 engine/model plugin | ⚠️ | 保留,但工具名统一为 `inference.*`(避免 break); 删除 DSLEngine 默认注入 |
| C15 BatchingQueue 5 方法 | ❌ | **推迟** — 7 个主流项目零独立 BatchingQueue 接口; 建议改为 SchedulerPolicy 抽象或内联 |
| C15 贡献流程文档 | ❌ | **推迟** — 0 第三方贡献者时写贡献流程是空文档 |
| **最佳方案** | | **方案 B (合并 + 裁剪)**: 只做 C13(无 SamplerStrategy) + C14(调整后), 不做 C15 |

---

## 📖 需阅读的架构/ADR 文档

新 Session 建议按以下顺序阅读:

### 必须阅读 (核心背景, 8 份)

| # | 文档 | 为什么读 | 估时 |
|---|------|---------|:----:|
| 1 | `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` | Phase 5 整体 master plan, §五 B2 + §5.5 编号重定义 | 15min |
| 2 | `docs/adr/plugin/adr-0034-model-router.md` | Model Router plugin 范式 (C7 已 ship, B2 参考设计) | 5min |
| 3 | `docs/adr/adr-0021-pdk-design.md` | PDK 设计 (Dual-Repo, semver ABI, Interfac) | 10min |
| 4 | `docs/adr/adr-0022-plugin-loading.md` | PluginLoader 设计 (dlopen 机制, 安全策略) | 5min |
| 5 | `docs/handoff/2026-07-05-week1-day1-day2-completion.md` | Week 1 Day 1-2 完成, B2 Oracle 拆分源头, §5 B2 拆分决策 | 15min |
| 6 | `docs/handoff/2026-07-04-post-c12-path-planning.md` | Post-C12 路径规划 (B2 后续任务路径, A/B/C/D 4 阶段) | 10min |
| 7 | `AGENTS.md` | 项目知识库 (结构/代码映射/最近变更/Phase 状态) | 10min |
| 8 | `openspec/changes/archive/2026-07-06-docs-cleanup-phase-2/proposal.md` | 本次文档清理的背景 (SKILL Compiler 归档 + 编号重定义 + 状态同步) | 5min |

### 参考阅读 (B2 三 Change, 3 份)

| # | 文档 | 为什么读 | 估时 |
|---|------|---------|:----:|
| 9 | `openspec/changes/phase5-b2-arch-schemas/proposal.md` | C13 proposal (当前目标) | 10min |
| 10 | `openspec/changes/phase5-llama-engine-plugin/proposal.md` | C14 proposal | 10min |
| 11 | `openspec/changes/phase5-batching-queue-plugin/proposal.md` | C15 proposal | 10min |

### 快速参考 (B2 代码/实现状态, 2 份)

| # | 文档 | 为什么读 | 估时 |
|---|------|---------|:----:|
| 12 | `lib/inference/` (3 文件) | 查看 engine.md / model.md PLACEHOLDER 占位内容 | 5min |
| 13 | `pdk/model_router/` (目录) | C7 已 ship 的 Plugin 实施参考 (CMake/工具注册模式) | 10min |

---

## 📚 本文件集索引

| 文件 | 内容 | 来源 | 页数 |
|------|------|------|:----:|
| **`README.md`** (本文) | 背景 + 文档索引 + 决策点 | — | — |
| **`main-report.md`** | 综合 Adversarial Review 报告 (5 大发现 + 4 方案对比) | 4 个探索 agent 综合 | 8 页 |
| **`ref-1-b2-oopenspec-arch.md`** | B2 三个 openspec change 架构边界深度审查 | explore bg_0a74e604 | 6 页 |
| **`ref-2-pdk-paradigm-c7.md`** | C7 Model Router PDK Plugin 范式审查与 B2 复用性 | explore bg_54cd898e | 8 页 |
| **`ref-3-lib-inference-state.md`** | B1 lib/inference 真实状态与 B2 起点假设验证 | explore bg_594ba48f | 5 页 |
| **`ref-4-external-llm-comparison.md`** | 7 个主流推理引擎 Plugin 抽象对比调研 | librarian bg_ee40d227 | 7 页 |
| **`architecture-overview-two-plugin-communication.md`** | 推理引擎+编排双 Plugin 四通道通信架构总览 (2026-07-06 Oracle 审查产出) | Oracle ses_0ca3dce4 | 4 页 |

---

## 🎯 需决策事项

新 Session 需在阅读上述文档后, 对以下 4 个决策点做出选择:

### 决策 1: C13 SamplerStrategy 接口去留

- **选项 A (删除)**: 保持 decoding.md 的 sampler 字段为纯字符串选择, 等真第二实现时再提取接口
  - 影响: 删除 `include/agenticdsl/pdk/sampler_strategy.h` 创建任务, 节约 C13 约 30 分钟
- **选项 B (保留)**: 按当前 proposal 声明 3 虚函数接口 + Doxygen 注释
  - 影响: 增加 PDK 接口 25% (8→10 头文件), LlamaSampler `supports()` 始终 true

### 决策 2: C15 BatchingQueue 处理方式

- **选项 A (推迟)**: 仅创建 `lib/inference/batching.md` schema(40 行), 不做 BatchingQueue 接口
  - 影响: 节约 1-1.5 天, 避免"零项目先例"的抽象接口
- **选项 B (保留)**: 按当前 C15 proposal 实施 5 方法 BatchingQueue + LlamaBatchingQueue
  - 影响: 发布 `supports_batching=false` / `cancel()` 始终 false 的接口 0 项目有类似设计

### 决策 3: C14 工具命名空间

- **选项 A (统一 `inference.*`)**: 保持与现有 `lib/inference/engine.md` 占位文件的 `inference.engine_init` 一致
  - 影响: 与 C14 proposal 第 186 行冲突 (原写 `llama_engine/init`), 需重写 tasks.md
- **选项 B (维持 `llama_engine/`)**: 按当前 C14 proposal 实施 `llama_engine/init`
  - 影响: 现有占位文件中 `inference.engine_init` 引用断裂, 需要额外兼容层

### 决策 4: 优先级排序

- **选项 A (先 B2)**: 按精简版 B2 启动 (C13 先 ship, C14 并行, C15 推迟)
- **选项 B (先 TSan)**: 处理已知 pre-existing TSan race (test_execute_parallel), B2 延后
- **选项 C (并行)**: B2 核心 schema + TSan 并行推进 (C13 半天 + TSan 半天)

---

## 🔗 交叉引用

本文件集引用的外部文档:
- `docs/adr/` — 全部 ADR 文档位于此目录
- `docs/superpowers/plans/` — 活跃/归档的 superpowers plan
- `openspec/changes/` — 活跃(active)与已归档(archive)的 openspec changes
- `openspec/specs/` — openspec spec 定义 (gitignored, per-machine ephemeral)
- `lib/inference/` — 推理标准库 `.md` 子图
- `pdk/` — PDK Plugin 源代码目录
- `include/agenticdsl/pdk/` — PDK 公共头文件
- `src/common/llm/` — LLM 后端实现 (llama_adapter, http_adapter, llm_types 契约)