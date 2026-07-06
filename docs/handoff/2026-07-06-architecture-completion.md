# 双 Plugin 通信架构 Handoff — 设计已完成, 等待实施启动

**Date**: 2026-07-06
**Branch**: `phase5-inference-orchestration` (新建, 与 main 隔离)
**Status**: ✅ 设计阶段 COMPLETED — 10 新 ADR + 1 架构总览文档 + 编号冲突已修复
**From**: Sisyphus session 2026-07-06 (上一会话: `/tmp/opencode/handoff-message.txt` handoff)
**To**: Next Sisyphus session (从 §6 立即执行入口继续推进)

---

## TL;DR

本会话收尾两件事：(1) 完成双 Plugin 通信架构的 **Oracle 三轮审查 + ADR 编号冲突修复**；(2) 给出 **下一阶段推进路径** (路径 D → A → B → C, **非**直接路径 A)。

**关键决策已落地**：
- 10 个新 ADR (0035/0038/0039/0040/0041/0042/0043/0044/**0045**/**0046**) 全部 🔍 Proposed + 编号冲突已修复
- 三层 ILLMProvider 消费链: `DSLEngine → 编排 Plugin (with approval) → 推理 Plugin (direct) → AgenticLlama (llama.cpp fork)`
- 四通道通信协议: Tool / Event / Config / Query
- 工具命名: slash 分隔 (`inference/generate`), 事件命名: dot 分隔 (`inference.lifecycle.idle`)
- 编排 Plugin 持有 **双 IToolRegistry**: external (ToolCoordinator-wrapped) + internal (bypass for orchestration reasoning, emit `orchestration.audit.internal.*`)

**下一会话路径 D** (AgenticLlama 端最小可验证 PoC) **优先级最高**, 因为：
- 设计经 3 轮 Oracle 修订仍处 🔍 Proposed, 实施不友好的灰区 (典型如 `internal_registry bypass` 安全边界 + `PluginInfoV2 ABI bump` 兼容性) 需要实证反馈
- PoC 反推 ADR 修订点, 让 OpenSpec change 生成时 ADR 已稳定
- 1 周 PoC 拿到首个能跑的 `.so` 后, 后续 C++ 实施风险降到最低

---

## 1. 当前项目状态 (2026-07-06)

### 1.1 工作区状态

| 项目 | 状态 |
|---|---|
| 当前分支 | `phase5-inference-orchestration` (基于 main `ee9603c`, 新建) |
| Working tree | 3 modified + 3 modified (lib/inference) + 10 untracked (新 ADR) + 1 untracked dir (adversarial-reviews) |
| 待 commit 内容 | 本会话 ADR renumber + handoff doc |
| 上游 main | `ee9603c docs(phase5-master-plan): log docs-cleanup-phase-2 ship in §十一.2 调整日志` |

### 1.2 ADR 编号冲突修复 ✅ (本会话完成)

**问题**: 设计产物中出现新 ADR-0036 (编排 Plugin) / ADR-0037 (通信协议), 但仓库已存在旧 ADR-0036 (三层服务协议) / ADR-0037 (跨 Worker 因果序), 全部 🔍 Proposed 状态。文档索引、cross-ref、openspec 都有歧义。

**解决** (最小变更原则, 选择**新 → 高编号**方向, 避免改动旧 ADR):
- 新 `adr-0036-orchestration-plugin-spec.md` → 新 `adr-0045-orchestration-plugin-spec.md`
- 新 `adr-0037-plugin-communication-protocol.md` → 新 `adr-0046-plugin-communication-protocol.md`
- 13 个 cross-ref 文件全局替换: `ADR-0036 → ADR-0045`, `ADR-0037 → ADR-0046` (限定新版本上下文, 不动旧版本文件)
- 全部 10 个新 ADR 状态行加 `renumber` note 说明编号变更

**未误伤验证**:
- 10 个旧版本关联文件 (adr-0004/0030/0033/adr-0036-three-layer-service-protocol/adr-0037-causal-ordering/docs-adr-management/implementation-roadmap/implementation-slices/archive-adr-0030) 的 0036/0037 引用全部完整保留
- ADR cross-ref 完整性: 12 个新文件每个仅 1 处 0036/0037 残留 (即 renumber note 中的历史叙事)

### 1.3 设计产物清单 (本会话累计)

| 类型 | 路径 | 状态 |
|---|---|---|
| 新 ADR (P0) | `docs/adr/adr-0035-inference-engine-plugin-spec.md` | 🔍 Proposed |
| 新 ADR (P0) | `docs/adr/adr-0045-orchestration-plugin-spec.md` *(原 0036)* | 🔍 Proposed |
| 新 ADR (P0) | `docs/adr/adr-0046-plugin-communication-protocol.md` *(原 0037)* | 🔍 Proposed |
| 新 ADR (P1) | `docs/adr/adr-0038-dynamic-config-interface.md` | 🔍 Proposed |
| 新 ADR (P1) | `docs/adr/adr-0039-performance-metadata-contract.md` | 🔍 Proposed |
| 新 ADR (P1) | `docs/adr/adr-0040-inference-plugin-build-strategy.md` | 🔍 Proposed |
| 新 ADR (P1) | `docs/adr/adr-0042-illmprovider-evolution-path.md` | 🔍 Proposed |
| 新 ADR (P2) | `docs/adr/adr-0041-pluginloader-lifecycle-extension.md` | 🔍 Proposed |
| 新 ADR (P2) | `docs/adr/adr-0043-pdk-tool-naming-convention.md` | 🔍 Proposed |
| 新 ADR (P2) | `docs/adr/adr-0044-inference-plugin-security-model.md` | 🔍 Proposed |
| 架构总览 | `docs/adversarial-reviews/architecture-overview-two-plugin-communication.md` | ✅ 落地 |
| 修改 ADR | `docs/adr/adr-0019-iinteraction-bus-mvp.md` (subscribe_topic 扩展) | (working tree modified) |
| 修改 ADR | `docs/adr/adr-0022-plugin-loading.md` (PluginInfoV2 + dual ABI + 5 symbols) | (working tree modified) |
| 修改 ADR | `docs/adr/plugin/adr-0034-model-router.md` (slash 命名先例) | (working tree modified) |
| 修改 DSL | `lib/inference/{engine,model,session}.md` (dot → slash tool refs) | (working tree modified) |

### 1.4 设计经 3 轮 Oracle 审查定稿的核心决策

| 决策点 | 决议 |
|---|---|
| 三层 ILLMProvider 链 | Option C dual ILLMProvider: DSLEngine → 编排 (with approval) → 推理 (direct) |
| 性能/指标通信 | Option C hybrid metrics: 编排 Plugin 通过 Tool 调用 `inference/perf/snapshot` 拉数据, 不直连 llama.cpp 内部结构 |
| 工具命名分隔符 | Option A slash: `inference/generate` (与 dot 事件 `inference.lifecycle.idle` 区分) |
| 内部 vs 外部 ToolCoordinator | 编排 Plugin 持有**双 IToolRegistry**: external (ToolCoordinator-wrapped) + internal (bypass, emit `orchestration.audit.internal.*`) |
| IInteractionBus 扩展 | `subscribe_topic(topic_pattern, callback)` per ADR-0046 §2.1 (有意识部分退让"零框架改动"原则) |
| PluginInfo V2 | `abi_version=2` (1104 字节, 含 `dependencies[256]`), 与 `abi_version=1` (848 字节, 旧) 共存 |
| 依赖加载顺序 | PluginLoader **强拓扑排序** (非仅警告), 缺依赖拒加载 |
| inference/engine/init Category | StateModify (非 ReadOnly, 因为修改全局 engine state) |
| 性能策略 prefer enum | 显式 5 值: `{latency, memory_saving, quality, throughput, balanced}` |
| 动态配置 L3a/L3b 拆分 | L3a `inference/configure` (原子数值参数) + L3b `inference/sampler/configure` (sampler 链组合) |
| 向后兼容 | dot-style 工具名: warn + 接受 6 个月, 然后 hard error |
| Factory symbol 返回类型 | `std::shared_ptr<agenticdsl::ILLMProvider>` (非原始指针, 解决 dlclose 前 release 顺序, 与 Sprint 17 C7 destruction order bug 同模式) |

---

## 2. 与已存在的 3 个 phase5 OpenSpec change 的关系 (未解决)

`openspec/changes/` 已有 3 个 active phase5 changes, **与新双 Plugin 架构设计存在张力**:

| OpenSpec change | 编号 | 状态 | 与新设计的关系 |
|---|---|---|---|
| `phase5-b2-arch-schemas` | C13 | 🟡 ACTIVE | 把 prefix_cache/kv_cache/decoding 留在架构层 (架构反思决议)。**与新 ADR-0035 把 6 工具全部下沉到推理 plugin 不一致** |
| `phase5-llama-engine-plugin` | C14 | 🟡 ACTIVE | 描述 `pdk/llama_engine/` 骨架 + C7 ModelRouter 范式, **没有编排 Plugin 概念**。双 plugin 架构超出 C14 范围 |
| `phase5-batching-queue-plugin` | C15 | 🟡 ACTIVE | BatchingQueue 接口抽象 + LlamaBatchingQueue reference。**未涉及编排 Plugin** |

**结论**: 下一会话需决定边界 — **新双 Plugin 架构 vs 已有 C13/C14/C15 边界划分**。这是路径 A (生成 OpenSpec change) 启动前**必须先回答的问题**, 否则会产生**相互冲突的 OpenSpec changes**。

---

## 3. 4 个真实风险 (按优先级)

### 风险 1 (已修复): ADR 编号冲突
- 状态: ✅ 已修复 (本会话 §1.2)
- 工作量: 0 (已完成)

### 风险 2 (待决策): 新双 Plugin 架构 vs 现有 C13/C14 OpenSpec change 设计不一致
- 表现: C13 把 prefix_cache/kv_cache/decoding 留架构层, 新 ADR-0035 把 6 工具全部下沉到推理 plugin
- 决策选项:
  - **2a**: 新双 Plugin 架构覆盖 C13/C14, 重写 C13/C14 proposal 反映新设计
  - **2b**: 新设计在 C13/C14 之上叠加, C13 处理"配置 schema", 新设计处理"plugin 实现"
  - **2c**: 双 plugin 架构独立, 不复用 C13/C14, 重新生成新 OpenSpec changes
- 推荐: **2c** (最干净), C13/C14 在新设计 ship 后 archive

### 风险 3 (待澄清): ADR-0045 (旧 0036-编排) vs 旧 ADR-0036-三层服务协议 概念高度重叠
- 表现: 旧的"基座/认知/领域"三层 = kernel+shell+usr/bin; 新的"DSL/Orchestration/Inference"三层 = engine+编排+推理
- 这俩**可能是同构异构表达**, 需要确认是合并/互引/互斥, 否则 ADR 状态机逻辑会打架
- 推荐: 在新 ADR-0045 §关联 加 cross-ref 旧 ADR-0036, 明确"新三层是旧三层的 plugin 化实现, 概念同构, 实现路径不同"

### 风险 4 (待实证): AgenticLlama 改造路径的实证缺口
- 现状: AgenticLlama 是 llama.cpp fork + Triton backend 改造 (目标: MiniMind-3 on llama.cpp + Triton)
- 待实证:
  - 暴露 `pdk_plugin_info` + `pdk_register_tools` + `pdk_create_llm_provider` (C ABI 表面) 是否需要 fork 改动 libllama.h 公共 API
  - Triton backend 在 plugin 内部成为可选 backend 是否破坏 MiniMind-3 端到端
  - 6 个推理工具 (`inference/{generate,configure,init,get,status,...}`) 全部下沉到 plugin 后, 架构层是否还需要任何 LLM 直通入口
- 推荐: 路径 D PoC 解决

---

## 4. 推荐推进路径: D → A → B → C

| 阶段 | 工作 | 工作量 | 产出 | 阻塞 |
|---|---|---|---|---|
| **D (现在)** | AgenticLlama 端最小可验证 PoC | 1 周 | 1 个能 `dlopen` 加载的 `.so` + 6 工具暴露 + 1 个 end-to-end `inference/generate` 调用 | 路径 A 启动前提 |
| **A** | OpenSpec change 生成 (`phase5-inference-orchestration`) | 2-3 天 | `proposal.md` + `tasks.md` + `specs.md` (基于稳定后的 ADR) | PoC 反馈的 ADR 修订完成 |
| **B** | 编号 cleanup + 边界对齐 + 旧 ADR-0036/0037 互引 | 1-2 天 | cross-ref 完整、ADR 状态机一致 | OpenSpec 落地后 |
| **C** | 完整 C++ 实施 (`pdk/inference_engine/` + `pdk/orchestration/` + 集成测试) | 4-6 周 | 完整插件链 + 全测试 + archive chain | 路径 A/B 完成 |

### 4.1 为什么不直接走 A?
ADR 经 3 轮 Oracle 修订仍处 🔍 Proposed, 意味着设计上还有对实施不友好的灰区 (典型如 `internal_registry bypass` 安全边界、`PluginInfoV2 ABI bump` 兼容性)。没有 PoC 反馈的 OpenSpec 大概率要被实施回炉修订。

### 4.2 为什么不直接走 C?
当前没有任何 `pdk/llama_engine/` 目录骨架, 连最小 CMake 都没有; 同时 10 个 ADR 互相 cross-ref、4 个 modified ADR 在工作区、3 个 phase5 OpenSpec change 与新设计存在张力——直接进 C++ 等于在沙地上盖楼。

### 4.3 路径 D 的具体范围

```cpp
// AgenticLlama/pdk_shim/ 最小骨架:
extern "C" {
  const PdkPluginInfo* pdk_plugin_info();
  int pdk_register_tools(PdkToolRegistry* reg);
  std::shared_ptr<agenticdsl::ILLMProvider> pdk_create_llm_provider(
    const PdkProviderConfig* config);
}

// 暴露 6 工具 (最小集):
//   inference/generate       — 核心 (llama.cpp decode loop 封装)
//   inference/configure      — KV cache / n_ctx / batch_size
//   inference/init           — 加载模型 (从 gguf path)
//   inference/get/status     — 当前状态 (loaded model, slots, etc.)
//   inference/perf/snapshot  — 性能指标 (TPS, latency, memory)
//   inference/sampler/configure — sampler chain 组合

// HydraForge 端验证:
//   PluginLoader.load("libagenticllama_pdk.so")
//   ToolRegistry.list() → 应含 inference/* 6 个
//   DSL workflow: 用 inference/generate 跑最小 prompt
//   结果: 返回 token + metrics (latency/tokens_per_sec)
```

预期 PoC 暴露的真实问题 (反推 ADR 修订):
- `pdk_create_llm_provider` 返回 `shared_ptr<ILLMProvider>` 与 dlclose 销毁顺序的细节
- `inference/init` 的 GPU context 初始化与 Triton backend 冲突时的 fallback 策略
- llama.cpp 公共 API 暴露的最小子集 (不需要暴露 `llama_context` 全结构)
- 6 工具在 SLO/权限/审批矩阵中的具体坐标

---

## 5. AgenticLlama 端的 2 个前置问题 (启动路径 D 前需澄清)

1. **AgenticLlama 的 "PDK 化改造" 是否要保留 llama.cpp 上游兼容?**
   - 保留: 只能加新文件 + 改 CMakeList 顶层, 不能改 libllama.h 公共 API
   - 不保留: 可深度耦合 HydraForge, 但失去 llama.cpp 上游 merge 能力

2. **Triton backend 与 PDK plugin 的关系?**
   - 选项 a: Triton backend 在 plugin 内部 (PDK 不感知后端)
   - 选项 b: Triton backend 作为独立 plugin (PDK 调度选择 backend)
   - 推荐: **选项 a** (PDK 不感知后端是 plugin 化的核心原则, 选 b 会把"plugin 化"目标翻倍复杂化)

---

## 6. 立即执行入口 (新会话起点)

```bash
# 1. 切到正确分支
cd /workspace/project/HydraForge
git checkout phase5-inference-orchestration
git pull --ff-only  # 同步远端 (如有)

# 2. 验证当前状态
git log --oneline -5
git status --short
# 期望: working tree 干净 (本会话将提交 ADR renumber + handoff doc)

# 3. 决定是否启动路径 D
#    D 启动前需先回答 §5 的 2 个前置问题
#    如 D 不启动, 可直接走 A (生成 OpenSpec change), 但有 §3 风险 2/3/4 未解决的代价

# 4. 路径 D 启动:
mkdir -p /workspace/project/AgenticLlama/pdk_shim/{include,src}
# 参考 §4.3 骨架实施
```

---

## 7. 引用

- **架构总览**: `docs/adversarial-reviews/architecture-overview-two-plugin-communication.md`
- **Phase 5 master plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md`
- **上一会话 handoff**: `/tmp/opencode/handoff-message.txt` (本会话起点)
- **Oracle 审查 session**: `ses_0ca3dce4fffeck5vmAQMs6R94m` (三轮 ILLMProvider/EventBus/naming 决议)
- **架构反思 session**: `ses_0ce717ac4ffejvLa2We0gzbuds` (2026-07-05, C13/C14/C15 边界决议)

---

## 8. 历史变更记录 (本会话)

| 时间 | 动作 | 文件 |
|---|---|---|
| 2026-07-06 上半段 | (上一会话) 设计 10 ADR + 1 架构总览 + 3 DSL rename + 3 ADR modify | 详见上一会话 handoff |
| 2026-07-06 本会话 | 新建分支 | `phase5-inference-orchestration` (基于 main `ee9603c`) |
| 2026-07-06 本会话 | ADR 编号冲突修复 | 2 文件 rename + 13 文件 cross-ref 替换 + 8 文件状态行 renumber note |
| 2026-07-06 本会话 | 本 handoff 文档落地 | `docs/handoff/2026-07-06-architecture-completion.md` |

---

**Status**: ✅ 全部任务完成, 设计阶段关闭, 等待下一会话按 §6 启动路径 D。