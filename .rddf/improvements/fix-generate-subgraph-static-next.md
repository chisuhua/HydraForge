# fix-generate-subgraph-static-next (PLACEHOLDER archived)

**优先级**: P3 (deferred from F1) | **阶段**: 自由 (phase-n/a) | **分类**: 技术债追踪
**类型**: debt-tracking | **主题**: GenerateSubgraphNode static-next 引用与 dsl.md 描述一致性
**状态**: placeholder-archived (2026-09-25)
**生成时间**: 2026-09-25
**关联 ADR**: ADR-0071 §D5 (LLM-native DSL), ADR-0072 §D3 (DSL node extensions)
**原 OpenSpec change**: `openspec/changes/2026-09-17-fix-generate-subgraph-static-next/` (PLACEHOLDER 状态，2026-09-17 创建，2026-09-25 归档)

## 背景

原 OpenSpec change 是 PLACEHOLDER 状态（仅 proposal.md 7 行 + tasks.md 18 行），创建于 2026-09-17 至 2026-09-25 期间未进展。

**触发根源**: F1 `fix-react-decide-empty-response` SHIPPED 后 (2026-09-18) 残留 drift。GenerateSubgraphNode 文档 `dsl.md §423/§438/§1114` 与实现存在描述与实现矛盾 (`/dynamic/x` 静态 next 引用是否被 `parse_node_wait_for_deps` 接受)。

**调研**: Oracle `ses_f4fd88215ffeUWSe2StAWtFPSQ` (DAG 动态组合流程调研)
**估时**: 1-2 sprint (Phase A 诊断 1-2d + Phase B 实施 3-5d + Phase C ship-with-fixes)

## In Scope

- Phase A: 根因诊断 (1-2d)
  - 验证 `parse_node_wait_for_deps` 是否接受 `/dynamic/x` 静态 next 引用
  - 若不接受：决定方案 (a) `build_dag` 加 `/dynamic/` 豁免 OR (b) 修正 dsl.md 文档
  - `dsl.md §423/§438/§1114` 描述与实现矛盾具体定位
- Phase B: 实施 + 真实 LLM E2E (3-5d)
  - 实施选定方案（`build_dag` 修复 OR `dsl.md` 文档重写）
  - 3 happy path tests (generate → register → execute 完整链路)
  - 3 error path tests (parse failure / next not found / runtime error)
  - `[realllm]` tag: 真实 DeepSeek LLM 端到端
- Phase C: ship-with-fixes
  - Oracle review (mandatory for cross-file)
  - ctest 245/245 零回归
  - openspec archive

## Why (为什么现在归档)

1. **信息保留**: 8 天 PLACEHOLDER 状态未推进，proposal 内容（Oracle session、估时、范围）有价值，不应丢失
2. **避免 noise**: 空壳 proposal 污染 `openspec/changes/` active 列表，迟早触发 doctor/validate drift
3. **Wave 2 语境已变**: 原 Wave 2 P1 引用 2026-09-16 旧 roadmap，该 roadmap 已被 L2 ref example 系列吸收
4. **优先级 P3**: 与 `chat-real-llm-coverage-phase-h`/`provider-llm-tool-empty-passthrough` (P2) 相比，F1 §5 Latent Sites 修复更优先

## Why (为什么现在不实施)

- 当前 P1 工作流已被 Oracle 推荐路径占满：
  - DECISION B(a): ProviderLLMTool 空文本守卫 (1-2h, P2, F1 Latent Site #3)
  - DECISION B(c): chat-real-llm-coverage Phase H (3-5h, P2, 6 E2E cases)
  - DECISION C: Wave 3 Phase 2 D4 规划（独立 24h cooling-off）
- Single-Dev 串行纪律：WIP 上限 2，本周无额外 slot

## Acceptance (重新启动条件)

- [ ] Wave 3 Phase 2 D4 (LoRA pipeline) ship 后 / 实际 LLM 路径测试基础设施就绪
- [ ] 任何未来 reader 跑 `ctest` 发现 `generate_subgraph` / `parse_node_wait_for_deps` 真实 bug 即触发
- [ ] F1 §5 Latent Sites 全部修复完成后（`#3 #4 #6` 全部 closed）

## Reverse Indicator (R8 per AGENTS.md)

```
[Reverse Indicator]
+ new_up: 8 天 PLACEHOLDER 信息保留至 improvement 跟踪
- old_down: drop_ratio=0% (纯文档归档，无行为变更)
- failure_traces: N/A (no code change)
- ablation: N/A
- context_ids: N/A
```

## 关联文档

- 原 placeholder: `openspec/changes/2026-09-17-fix-generate-subgraph-static-next/` (待 archive)
- F1 主 fix: `openspec/changes/archive/2026-09-18-fix-react-decide-empty-response/`
- AGENTS.md Pattern #1 step 4 — 系统性记录同类潜伏站点
- ADR-0071 §D5 (LLM-native DSL), ADR-0072 §D3 (DSL node extensions)