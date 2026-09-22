# wave-3-finetune-base-model

> **来源**: Pre-Wave3 Plan §3 (`Phase 7 中期: Wave 3 立项 (1-2 天, Week 2 起点)`) + ADR-0078 🔍 Proposed 翻牌激活 + Pre-Wave3 Plan §2 4-Gate 序列 (G1+G2+G3+G4) 全部 SHIPPED 触发
> **生成**: 2026-09-22 via main session (24h cooling-off 期间 Wave 3 立项准备 per Single-Dev 治理 — 读 ADR background + 写 improvement 5-segment 草稿; 不正式立项, 待 cooling-off 满后 rdd-arch 启动)
> **依据**: ADR-0078 v1.0 (2026-08-03, Wave 5+ descoped docs-only; D1-D7 设计就绪; D2 触发条件已审阅)

## Why

Pre-Wave3 Plan §3 明确 Wave 3 立项依据: G1+G3+G4+G2 4-Gate 序列全部 SHIPPED (2026-09-22) + 24h cooling-off 满后 + Oracle 复审通过即触发 Wave 3 立项.

**Wave 3 = ADR-0078 Model-RSI pilot 立项** — Fine-tune 基模选型 + 训练管线 激活, 是 HydraForge 闭环自进化的下一步.

**3 项 Wave 3 价值**:
1. **闭环第 7 环配套完整**: G1/G2/G3/G4 ship 后, 自进化变异→评估→决策→提交全链路打通. Wave 3 把"提交"的产出 (Genome + attribution + reward quality) 作为训练数据回流, 实现"自进化→自我训练"正循环.
2. **D2 触发条件已部分满足**:
   - ✅ AgenticMind 项目独立进展 — 2026-08-03 立项, 持续跟踪 (虽未 ship, 但 Phase 5+ descoped 允许延后)
   - ✅ G4 ship 后 `evolution.readiness.denied` 事件 `eval_quality` 真实值 (G2 ship) + `genome.committed/persist_failed` 事件 (G4 ship) — 训练数据来源 D3 §ADR-0074 D6 JSONL + ADR-0074 D7 失败事件 现成
   - ⚠ Production 用户 ≥ 10 + 训练数据 ≥ 1 万条 — 依赖 pkm_temporal_demo + production 部署, Phase 7 部署就绪后可达
   - ✅ Fine-tune 价格 ≤ $1 / 1M tokens — DeepSeek-V2 cache hit $0.14/1M, Llama/Qwen 本地部署仅电费
3. **D1 4 维度评分框架就绪**: llm-tool-eval + cost-monitoring 已 ship, 可量化评分候选基模.

**Wave 3 不做的话**:
- 自进化闭环仅"变异→评估→决策"通, 缺"决策→训练→更强基模"正反馈
- AgenticMind 探索结果无 HydraForge 落地通道
- Fine-tune 模型注册为 ILLMProvider 是 MCP server + gRPC data plane 的前置, 阻断下游

## What Changes

- **ADR-0078 状态翻牌**: 🔍 Proposed (Wave 5+ descoped docs-only) → ✅ Approved (Pilot 激活). 同时 Phase 5+ descoped 标签移除, Pilot 改名为 Wave 3 Phase 1.
- **OpenSpec change 创建**: `openspec/changes/wave-3-finetune-base-model/` (或 `finetune-base-model-pilot-phase1` 更具体名称). 4 件套: proposal.md + design.md + tasks.md + specs/*/spec.md. 实施 D1 + D3 (基模选型 + 训练数据准备), D4-D7 延后到 Wave 3 Phase 2+ (Phase 5+ training)。
- **D1 基模选型实施**: 4 维度 (Capability/Latency/Cost/Openness) 评分, 候选 5 个模型 (gpt-4-turbo / claude-3.5-sonnet / llama-3.1-70b / qwen-2.5-72b / deepseek-v2), 选 Weighted ≥ 7.5 + 4 过滤条件全过. 复用现有 `tests/test_llm_tool*.cpp` 评分基础设施 + `examples/cost_tracking_decorator` cost 数据.
- **D3 训练数据准备**: 3 路汇总 (ADR-0074 D6 baseline JSONL + ADR-0074 D7 失败事件 + AgenticMind 回流). Schema 兼容 ADR-0074 D6 (`dsl_version` + `schema_snapshot_hash` + `stage_1_selected` 等元数据), 新增 `source` 字段 (`baseline` / `failure` / `agenticmind`).
- **D7 serving 集成 (Phase 1 最小版)**: 注册 Fine-tune 模型为 ILLMProvider via `LLMProviderFactory::register_dynamic(name, DynamicFactoryFn)` (per `src/common/llm/llm_provider_factory.h:33`, 实际 API 名称; 不是 `register_provider`). Factory fn 签名 `std::function<std::unique_ptr<ILLMProvider>(const LLMConfig&)>` (LLMConfig 非 json). 接入 MCP `prompts/*` 更新 (ADR-0076 衔接) 延后 Wave 3 Phase 2+.
- **决策记录更新**: `docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md` §5.1 4-Gate 序列 Pre-Wave3 收盘 + §3 摩擦 1 (G2 已 resolved) + Wave 3 立项条件新增"Phase 1 容量评估"段落.

**Non-Goals**:
- ❌ 不实施 D4 训练方法 (LoRA / QLoRA / API fine-tune) — 延后 Wave 3 Phase 2+ (估时 2-4 周)
- ❌ 不实施 D5/D6 评估方法学 + AgenticMind 回流 — 延后 Wave 3 Phase 2+ (依赖 AgenticMind ship)
- ❌ 不实施 D7 MCP prompts/* 更新 — 延后 Wave 3 Phase 2+ (依赖 ADR-0076 gRPC ship)
- ❌ 不实施 ADR-0071/0074/0076/0077 改动 — 4 个上游 ADR 各自独立 OpenSpec change
- ❌ 不开 new PDK plugin — Fine-tune model 作为现有 ILLMProvider 接口实现

## Acceptance

- [ ] AC-1: ADR-0078 状态从 🔍 Proposed → ✅ Approved (per `.rddf/roadmap/2026-09-21-pre-wave3-to-wave3-execution-plan.md` §3 + Decision Record §5.1 更新)
- [ ] AC-2: `openspec/changes/wave-3-finetune-base-model-pilot-phase1/` (或等价名) 4 件套完整 (.openspec.yaml + proposal.md + design.md + tasks.md + specs/*/spec.md)
- [ ] AC-3: `openspec validate wave-3-finetune-base-model-pilot-phase1 --strict` → "Change is valid"
- [ ] AC-4: D1 基模选型 — 至少 3 个候选模型评分 + 1 个最终选择 + 评分 yaml 持久化 (`docs/research/wave-3-base-model-selection.md`)
- [ ] AC-5: D3 训练数据准备 — ADR-0074 D6 JSONL 数据 D3 §新增 `source` 字段迁移脚本 + `tests/test_training_data_pipeline.cpp` 新建 ≥ 1 case (过滤 `parse_valid && task_success`)
- [ ] AC-6: D7 serving 集成 (Phase 1 最小版) — `LLMProviderFactory::register_provider` 新增 fine-tune provider + `tests/test_llm_provider_factory.cpp` 新增 ≥ 1 case (注册 + 解析 config)
- [ ] AC-7: 既有 `tests/test_llm_tool*` + `tests/test_cost_tracking_decorator` + `tests/test_genome_registry` 零回归 (≥6 个 binary PASS)
- [ ] AC-8: 全量 `ctest -N` 计数 = expected (新增 `test_training_data_pipeline` binary, 无 PDK 跨库影响)
- [ ] AC-9: 1 atomic commit per AGENTS.md 模式 #4 + Oracle post-impl `SHIP-with-fixes` 复评通过 (主会话派, bg_a0ebaef9 模板)
- [ ] AC-10: archive 时 `git ls-files openspec/changes/archive/wave-3-.../` 验证 4 文件全在 (防 AGENTS.md Day-5 lesson 陷阱)
- [ ] AC-11: AGENTS.md Recent Changes + ADR-0078 状态翻牌 + `docs/audits/2026-09-21-harness-rsi-pilot-go-no-go.md` §3 摩擦 (摩擦 1 已 resolved, 无新增摩擦) + `proposal-suggestions.md` 同步
- [ ] AC-12: 24h cooling-off 自 G2 merge (2026-09-22 22:30) 起算, Wave 3 cooling-off 自 Wave 3 merge 起算, 链式合规

## Capabilities

### MUST DO (执行红线)
- 工作目录: `.rddf/wt/wave-3-finetune-base-model/` (per `.rddf/wt/` 已 ignore)
- 主会话已 ship 准备: improvement 草稿 + builder state (post_impl_review_prompt 已写)
- TDD 5 步: 先写 failing test → 验证 fail → 实施最小代码 → 验证 pass → defer commit
- 仅 1 atomic commit per AGENTS.md 模式 #4 (`feat(llm): Wave 3 finetune-base-model pilot phase 1`)
- 同步 ADR-0078 状态翻牌 + `.rddf/improvements/` improvement 文件留档
- `lsp_diagnostics` 全部改动文件零错误
- 实施完成后必派 Oracle post-impl `SHIP-with-fixes` 复评 (per user 决策 + mode #11 闭环)
- Day-5 trap 防御: archive 时 `git ls-files` 5 文件验证 (`.openspec.yaml` + 4 件套)
- 24h cooling-off 链式合规 (Pre-Wave3 → Wave 3)

### MUST NOT DO
- ❌ 不要碰 ADR-0071/0074/0076/0077 主体 (4 个上游 ADR 独立 OpenSpec change)
- ❌ 不要开 new PDK plugin (Fine-tune model 作为现有 ILLMProvider 接口实现)
- ❌ 不要 amend 上次 commit (Atomic commits 不能 amend)
- ❌ 不要在 main 直接修改 (违反 worktree-discipline)
- ❌ 不要跳 24h cooling-off (Wave 3 cooling-off 自 Wave 3 merge 起算)
- ❌ 不要跳 Oracle SHIP-with-fixes 复评 (mode #11 闭环强制)
- ❌ 不要实施 D4-D7 (训练方法/评估/回流/serving-Phase 2) — Wave 3 Phase 1 边界
- ❌ 不要引入新外部依赖 (DeepSeek API / HF TRL / PEFT) — 不增加 build complexity
- ❌ 不要在 main 上改动 .rddf/state/builder/*.json (builder state 是 mode #11 pre-flight 字段)

## Impact

**Production files (估算 4-6)**:
- `src/common/llm/finetune_provider.h/cpp` — `FinetuneBaseModelProvider : ILLMProvider` (Phase 1 stub: 注册 + config 解析, 推理延后 Wave 3 Phase 2)
- `src/common/llm/llm_provider_factory.cpp` — `register_provider("agenticdsl-llama-3.1-70b-lora-v1", ...)` 入口
- `docs/adr/adr-0078-finetune-base-model.md` — 状态行 + Wave 5+ descoped 标签移除 + Phase 1 容量评估段落
- `docs/research/wave-3-base-model-selection.md` — D1 评分 yaml + 选择论证
- `scripts/prepare_training_data.py` — ADR-0074 D6 JSONL 迁移 (加 `source` 字段) + 过滤脚本

**Test files (2-3)**:
- `tests/test_training_data_pipeline.cpp` — D3 JSONL 迁移 + 过滤 (≥1 case)
- `tests/test_llm_provider_factory.cpp` — D7 fine-tune provider 注册 + config 解析 (≥1 case)
- (D1 评分是文档性, 无需新增 test binary; 复用现有 cost_tracking_decorator)

**依赖变更**: 0 外部依赖 (无 LoRA/QLoRA 训练 library 引入 — Phase 2 才需要)
**总估时**: 1-2 天 (Phase 1 边界; D4-D7 延后)
**推荐路线**: complex (per `.rddf/state/.planner-handoff.json::recommended_route=complex` + Wave 3 跨 4-6 files + ADR-0078 翻牌)
**Execution mode**: worktree (per user 决策 + cross-process 不确定性 + OpenSpec change 大改)
**Trigger**: 24h cooling-off 满 (起点 2026-09-22 22:30 = G2 merge `dc12a17`, 满点 2026-09-23 22:30) → rdd-arch 启动 Wave 3 立项

## Implementation Roadmap (Phase 1 边界)

| Step | 内容 | 估时 | 阻塞 |
|------|------|:---:|------|
| 1 | ADR-0078 状态翻牌 + Decision Record §3 更新 | 0.5h | 无 |
| 2 | OpenSpec change 4 件套创建 + dual-agent review (Oracle bg_c706862b + Metis bg_d9744d91 模板) | 1h | Step 1 |
| 3 | TDD RED: test_training_data_pipeline + test_llm_provider_factory 失败测试 | 1h | Step 2 |
| 4 | GREEN: scripts/prepare_training_data.py + FinetuneBaseModelProvider stub | 2h | Step 3 |
| 5 | D1 评分 yaml 持久化 (`docs/research/wave-3-base-model-selection.md`) | 1h | Step 4 |
| 6 | archive 5 文件 + 1 atomic commit | 0.5h | Step 5 |
| 7 | Oracle post-impl SHIP-with-fixes review + apply fixes | 1h | Step 6 |
| 8 | merge → main + cleanup worktree | 0.5h | Step 7 |
| 9 | Wave 3 cooling-off 起算 (24h) + Phase 2 (D4-D7) 规划 | (24h+) | Step 8 |

**Phase 2 (D4-D7, 估时 2-4 周, Wave 3 cooling-off 满后启动)**:
- D4: LoRA/QLoRA 训练 + HF TRL + PEFT 引入 (build 复杂度上升)
- D5: 重跑 ADR-0074 D3 baseline + Evidence Gate 复测
- D6: AgenticMind 项目 ship 后回流数据格式标准化
- D7: MCP `prompts/*` 更新 + gRPC LLMDataPlane (ADR-0077) 衔接

## NOT-VERIFIED (主会话 post-merge 必补)

- 全量 ctest 252 binaries 零回归 (G1+G2+G3+G4 + Wave 3 Phase 1 都需验证)
- TSan 跑 (机器性能受限跳过, 同 G1/G4 NOT-VERIFIED 项)
- D1 评分 yaml 的实际候选模型 benchmark 数据 (依赖 llm-tool-eval 实时跑, 不在本 change 范围)

---

*文档版本: v1.0*
*生成日期: 2026-09-22 (cooling-off 期间 Wave 3 立项准备)*
*状态: 📝 Draft (待 24h cooling-off 满后 rdd-arch 正式立项)*
*触发: 2026-09-23 22:30 (G2 merge + 24h)*