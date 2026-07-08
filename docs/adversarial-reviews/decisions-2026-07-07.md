# Phase 5 B2 架构决策记录 (2026-07-07)

> **创建日期**: 2026-07-07
> **创建者**: Sisyphus (基于 Momus 深度审查 + 用户逐一确认)
> **关联 Adversarial Review**: `docs/adversarial-reviews/main-report.md`
> **关联 Master Plan**: `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md`
> **状态**: ✅ 已定稿

---

## 决策清单

### D1: C13 SamplerStrategy 接口去留

- **决策**: 选项 A — **删除**
- **描述**: 保持 `decoding.md` 的 sampler 字段为纯字符串选择（5 种），不创建 `include/agenticdsl/pdk/sampler_strategy.h` PDK 接口。采样器 clamp 逻辑内联到 `inference/engine/generate` 工具实现内部，等有第二个推理后端时再提取公共接口。
- **影响**:
  - C13: 删除 `tasks.md §3.2`（创建 sampler_strategy.h）
  - C14: 删除 `tasks.md §4`（LlamaSampler PDK 暴露）
  - PDK 接口计数: 8→7（-12.5%）
  - 节约工时: ~30min（C13）+ 1h（C14）

### D2: C15 BatchingQueue 处理方式

- **决策**: 选项 A — **推迟，仅保留 schema**
- **描述**: 删除 `BatchingQueue` 5 方法 PDK 接口和 `LlamaBatchingQueue` reference 实现。仅创建 `lib/inference/batching.md` schema（40 行 PLACEHOLDER），标注"实际实现推迟到第二个推理后端出现时"。第三方贡献流程文档一并推迟。
- **影响**:
  - 删除 `include/agenticdsl/pdk/batching_queue.h` 创建任务
  - 删除 `pdk/llama_engine/src/llama_batching.cpp` 创建任务
  - 删除 4 个贡献流程文档创建任务
  - 删除 ADR-0021 §8 追加任务
  - 节约工时: ~1-1.5 天
  - C15 估时: 原 0.5-1 天 → 精简至 ~2h

### D3: C14 工具命名空间

- **决策**: 选项 A — **统一 `inference.*`**
- **描述**: C14 的工具注册从 `llama_engine/init`、`llama_model/load` 等改为 `inference/engine/init`、`inference/model/load`。与现有 `lib/inference/engine.md` 占位文件的 `inference.engine_init` 风格对齐。
- **命名映射** (8 个 engine/model 工具):
  | 原名 | 新名 |
  |------|------|
  | `llama_engine/init` | `inference/engine/init` |
  | `llama_engine/generate` | `inference/engine/generate` |
  | `llama_engine/stream` | `inference/engine/stream` |
  | `llama_engine/status` | `inference/engine/status` |
  | `llama_model/load` | `inference/model/load` |
  | `llama_model/unload` | `inference/model/unload` |
  | `llama_model/list` | `inference/model/list` |
  | `llama_model/switch` | `inference/model/switch` |
- **C13 架构工具命名边界** (2026-07-07 补充):
  - D3 决策**不适用于** C13 架构层工具（prefix_cache/kv_cache/decoding/cloud_engine）
  - C13 架构工具命名规则: `<领域>.<动作>`（不带 `inference` 前缀），例如：
    | 工具 | 命名 |
    |------|------|
    | prefix_cache 配置 | `prefix_cache.configure` |
    | kv_cache 配置 | `kv_cache.configure` |
    | decoding 配置 | `decoding.configure` |
    | cloud_engine 配置 | `cloud_engine.configure` |
  - 理由: 架构工具属于"基础设施层配置"，与"业务层推理调用"语义不同；`inference.*` 前缀专用于面向业务的推理链路（engine/model），架构配置工具不加此前缀以避免命名空间混淆
  - C13 schema 文件（`lib/inference/prefix_cache.md` 等）使用相同命名，**内部一致**
- **影响**:
  - C14 proposal.md: 8 处工具名替换
  - C14 tasks.md: 8 处工具名替换
  - lib/inference/engine.md + model.md: 工具名对齐
  - C13 schema 文件: 4 处架构工具命名（不应用 D3 重写）
  - 附加工时: ~30min

### D4: 优先级排序

- **决策**: 选项 C — **并行推进**
- **描述**: C13（纯 .md schema）立即启动不受 TSan 阻塞；TSan race 修复与 C14 工具名重写并行；C14 编码阶段在 TSan gate 100% pass 后启动。
- **时序**:
  ```
  时间 →
  ├── C13 (schema-only) ───────────────────────── ship
  ├── TSan race fix  ───── (0.5-1d) ── ✅ TSan gate
  ├── C14 命名空间重写 ── (30min) ──┐
  ├── C14 编码工作 ────────────────┴── (TSan 修复后) ── ship
  ├── C15 (batching.md only) ─────────────────── ship
  ```

---

### D5: C14 DSLEngine 默认注入 plugin 策略

- **决策**: 选项 B — **删除默认注入 + 显式 load_plugin**
- **描述**: 删除 `DSLEngine()` 构造时默认 dlopen `libhydraforge_llama_engine.so` 并 fallback 到 `LlamaAdapter` 的逻辑。改为提供显式 `DSLEngine::load_plugin(name)` API，由调用方决定何时加载。
- **理由**:
  1. 与 Adversarial Review `main-report.md` §3.2 建议一致
  2. 与 C16 D4（`LlamaAdapter` + `LlamaAdapterProvider` 标 `[[deprecated]]`）路径一致
  3. CI 行为更可控（plugin 缺失时显式失败，无静默 fallback）
  4. plugin 生命周期更清晰
- **影响**:
  - C14 proposal §6: 改"默认注入 + fallback"为"显式 `load_plugin(name)` + 缺失时 WARN log"
  - C14 tasks §7: 任务从"默认注入"改为"显式加载 API + 测试/示例迁移"
  - 所有现有测试/示例需要添加 `engine.load_plugin("pdk/llama_engine")` 调用
  - 向后兼容 BREAKING（DSLEngine 构造不再自动加载 plugin）
  - 与 C16 D4 同步（`LlamaAdapter` deprecate 路径一致）
- **实施步骤**:
  1. 删除 `DSLEngine` 构造中的默认注入逻辑
  2. 新增 `DSLEngine::load_plugin(const std::string& name)` 公开方法
  3. 添加 `test_load_plugin.cpp` 单元测试 (4 test cases)
  4. 迁移所有测试/示例（添加 `load_plugin("pdk/llama_engine")` 调用）
  5. 更新 `lib/dsl-reference.md` §3.2 记录 API 变更
- **决策日期**: 2026-07-08 (基于 author Chi Suhua 后续 ship 确认: ✅ 已签字 by Chi Suhua, commit `22a417b` 2026-07-08 "fix(openspec): C14 文档同步至 D5 决策 (删除 fallback + 显式 load_plugin)")
- **替代方案**: 选项 A（保留默认注入 + fallback）— 已否决，理由见 main-report.md §3.2

---

## 后续维护

- 各 OpenSpec change 必须在 tasks.md 中引用本决策文件作为前置依赖
- 本文件追加到 `docs/adversarial-reviews/` 目录，与 `README.md` + `main-report.md` 并列
- 如有新架构决策，按此格式追加
