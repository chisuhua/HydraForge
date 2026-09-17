# Proposal: Genome Registry —— Harness 配置可版本化基础设施

> **STATUS: PLACEHOLDER** ⚠️
> **依赖 C0 + C1 ship 后启动** (Wave 1 chat demo 端到端跑通才能采集真实事件流)
> **关联 ADR**: ADR-0067 (Layered Plugin Architecture), ADR-0061-13 (Distillation Output Format), ADR-0078 (Fine-tune, Model-RSI 依赖)
> **追溯范围**: `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §四 C2
> **优先级**: P1 (Wave 2 中期, Sprint 35)
> **估时**: 1 周

---

## Why（背景概要）

**MetaRSI-v1 论文 (unverified)** 提出 "Genome" 概念：Harness 完整配置打包为可版本化、可分享对象。

**HydraForge 现状** (Oracle 评审): 配置散落 3 个地方（`config.json`, `lib/loop/*.agent.md`, `ChatConfig` 隐式），无可版本化抽象。

**没有 Genome 抽象的后果**:
- Harness-RSI 变异没有 diff/rollback/fork 锚点
- 自进化闭环（per `self-evolution-architecture-2026-08.md`）无法落地
- ADR-0078 Model-RSI 训练数据采集缺追溯基础
- 多 Sprint 间配置漂移无法审计

**Oracle 评审 (M2)** 关键决策: **取消** 原计划的 3 算子接口框架 (IDataRSI/IHarnessRSI/IModelRSI) — 映射既有 ADR-0083/0084/0086 契约栈。本 change **只** 提供 Registry 基础设施，**不**定义算子接口。

## What Changes（待起草时详细制定）

### 1. Genome CRD schema 定义
- TBD: `pdk/genome/spec/genome-v1.yaml` (apiVersion: genome/v1, kind: AgentGenome)
- TBD: metadata: {name, version, parent, created_by, capture_mode}
- TBD: spec: {harness{system_prompt, loop_type, workflow}, tools[], budget, model_routing, prompt_cache_prefix}
- TBD: 决策 D9 storage backend (filesystem vs SQLite vs Git-LFS)

### 2. IGenomeRegistry 接口
- TBD: `include/agenticdsl/genome/registry.h`
- TBD: `load(name@version) -> Genome` (取具体版本)
- TBD: `commit(genome) -> CommitResult` (写入新版本)
- TBD: `fork(name, parent_version, mutations) -> Genome` (从父版本 fork 出新版本)
- TBD: `list_versions(name) -> vector<version>` (列出所有版本)
- TBD: `diff(v1, v2) -> GenomeDiff` (返回两版本差异)

### 3. 文件系统后端
- TBD: `src/core/genome/registry.cpp`
- TBD: 路径布局: `~/.hydraforge/genomes/<name>/<version>/genome.yaml`
- TBD: 谱系追踪: commit 强制 `parent` 字段必填
- TBD: 完整性校验: HMAC 签名 (D10) + schema validation

### 4. 错误处理
- TBD: 加载不存在版本 → `Result::failure(GenomeError::NotFound)`
- TBD: 谱系断裂 → `Result::failure(GenomeError::BrokenLineage)`
- TBD: HMAC 校验失败 → `Result::failure(GenomeError::IntegrityViolation)`
- TBD: 错误码与 ADR-0023 ToolResult 错误码对齐

### 5. 测试覆盖
- TBD: 8 个 test case (load/commit/fork/diff/version_listing/parent_validation/HMAC/invalid_schema)

## Capabilities（待详细制定）

### ADDED Requirements (placeholder)
- `genome-crd-schema`: Genome CRD YAML schema 定义 (PLACEHOLDER)
- `igenome-registry-interface`: IGenomeRegistry 接口 5 方法 (PLACEHOLDER)
- `filesystem-backend`: 文件系统后端 + 谱系追踪 (PLACEHOLDER)
- `hmac-integrity`: HMAC 签名完整性校验 (PLACEHOLDER)
- `genome-error-codes`: 错误码对齐 ADR-0023 (PLACEHOLDER)

## Non-goals
- ❌ 不接 ChatSession/DSLEngine 构造参数（避免 BREAKING，留作 follow-up）
- ❌ 不实现 Git-LFS 后端（filesystem 优先）
- ❌ 不定义 3 算子接口 (IDataRSI/IHarnessRSI/IModelRSI) — Oracle 评审取消
- ❌ 不实现 Model-RSI 实际执行（依赖 ADR-0078）

## Estimated Effort
**总计**: 1 周（含 schema 设计 + Registry 接口 + filesystem 后端 + 8 个 test case + 真实 LLM 端到端验证）

## 详细制定 TODO（待 C0+C1 ship 后）
- [ ] 1. 决策前置: D9 storage backend (filesystem vs SQLite vs Git-LFS) + D10 signature scheme (HMAC vs ed25519)
- [ ] 2. 写完整 design.md（Genome schema + IGenomeRegistry 接口 + filesystem layout + 谱系图）
- [ ] 3. 写完整 tasks.md（schema 8 case × 5 步 + registry 6 case × 5 步）
- [ ] 4. 写完整 spec.md（R1-R5 见 Capabilities 章节）
- [ ] 5. 移除 PLACEHOLDER 标记
- [ ] 6. openspec validate
- [ ] 7. 更新 master plan §四 C2 状态
- [ ] 8. 启动 Sprint 35 实施

## 依赖
- **上游**: C0 + C1 (Wave 1 ship) — chat demo 端到端跑通才能采集真实事件流
- **下游**: C3 (h-d-m-transition-guard) 需要 Genome 版本号接口才能判断"过期数据"

## 关联文档
- MetaRSI-v1 论文 (unverified, external framework reference)
- `docs/architecture/self-evolution-architecture-2026-08.md` §一
- `docs/adr/adr-0061-13-distillation-output-format.md`
- `docs/adr/adr-0078-finetune-base-model.md` (Model-RSI 依赖, 🔍 Proposed)
- Oracle 评审: `task_id=ses_f55f307f6ffeRJ9SIny8iUbZ8Y` §4 Wave 2
- Master plan: `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md`
