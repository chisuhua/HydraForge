# Proposal: Harness-RSI Pilot —— 实验验证 Harness-RSI 价值

> **STATUS: PLACEHOLDER** ⚠️
> **依赖 C3 (h-d-m-transition-guard) ship 后启动**
> **GO/NO-GO DECISION GATE**: pilot 完成后决定是否扩展 Model-RSI 方向
> **关联 ADR**: ADR-0084 (Mutation Governance), ADR-0061-13 (Distillation Output)
> **追溯范围**: `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §四 C4
> **优先级**: P2 (Wave 2.5, Sprint 36, Go/No-Go 决策)
> **估时**: 1-2 周

---

## Why（背景概要）

**YAGNI 原则 + Oracle 评审**:
- 不造重型调度框架，pilot 验证 Harness-RSI 价值前不投入
- pilot 结果决定后续是否需要更重的 Model-RSI 方向（Wave 3）
- 必须证明 H→D→M 守卫有效 + Harness 变异经 ApprovalPolicy 拦截

**当前可达成的目标**:
- IHarnessRSI 首个真实实现（直接调 ChatConfig::override_* 方法）
- 端到端 mock 闭环（Genome 变异 → ApprovalPolicy 通过 → 重新加载到 ChatSession → 1 turn 验证）
- 真实 LLM 1 turn 验证（D2 配置 deepseek, 1 个 prompt_delta 验证应用）
- 变异经 ApprovalPolicy 拦截测试

**Go 决策标准** (pilot 完成后):
- Mock 闭环全部通过
- 真实 LLM 1 turn 验证 prompt delta 生效
- ApprovalPolicy 拦截测试通过
- ctest 零回归
- → 立项 ADR-0078 Model-RSI pilot (Wave 3)

**No-Go 决策**:
- 归档 Wave 2 skeleton
- 等待需求驱动（真实训练数据 / 评估基线就绪）
- → 不扩展 Model-RSI 方向

## What Changes（待起草时详细制定）

### 1. IHarnessRSI 首个真实实现
- TBD: `src/modules/evolution/harness_rsi.cpp`
- TBD: 接受 `GenomeMutations{prompt_delta, tools_add/remove, workflow_patch}` → 产出新 Genome
- TBD: 直接调 ChatConfig::override_* 方法实现 prompt delta
- TBD: tools add/remove 通过 ToolRegistry API
- TBD: workflow patch 通过 Genome Registry 引用替换

### 2. 双重门禁集成
- TBD: 走 `evaluate_readiness()` (C3 状态机) 验证 4 条件
- TBD: 走 `MutationGovernanceVerdict` (ADR-0084) 验证硬门禁
- TBD: 失败时返回 `Result::failure`，不修改任何状态

### 3. Mock 闭环
- TBD: Genome 变异 → ApprovalPolicy 通过 → 重新加载到 ChatSession → 1 turn 验证响应
- TBD: 3 个 mock test case (简单 prompt delta, tool add, tool remove)

### 4. 真实 LLM 1 turn
- TBD: D2 配置 deepseek, 1 个 prompt_delta 验证应用
- TBD: 比对应用 delta 前后 LLM 响应差异
- TBD: 1 个真实 LLM test case（CI 可选，本地验证）

### 5. ApprovalPolicy 拦截测试
- TBD: 2 个 test case (low-trust 工具 add 被拦截, dangerous 工具 add 被拦截)

### 6. Go/No-Go 决策记录
- TBD: pilot 完成后写 Decision Record
- [ ] Go 路径: 立项 ADR-0078 Model-RSI pilot (Wave 3)
- [ ] No-Go 路径: 归档 Wave 2 skeleton

## Capabilities（待详细制定）

### ADDED Requirements (placeholder)
- `iharness-rsi-impl`: IHarnessRSI 首个真实实现 (PLACEHOLDER)
- `mutation-application-protocol`: Mutation application 契约 (prompt/tools/workflow 3 路径)
- `mock-loop-verification`: Mock 闭环端到端流程 (PLACEHOLDER)
- `real-llm-1turn-verification`: 真实 LLM 1 turn 验证 prompt delta 生效 (PLACEHOLDER)
- `approval-policy-veto-test`: ApprovalPolicy 拦截测试 (PLACEHOLDER)
- `go-no-go-decision-record`: Go/No-Go 决策记录格式 (PLACEHOLDER)

## Non-goals
- ❌ 不实现 Model-RSI 实际执行（依赖 ADR-0078，pilot 验证后立项）
- ❌ 不实现真实 LoRA 训练
- ❌ 不实现多 Agent 协同进化（per self-evolution §一）
- ❌ 不实现 Meta Co-Evolution
- ❌ 不引入新框架依赖

## Estimated Effort
**总计**: 1-2 周（含 IHarnessRSI 实现 + mock 闭环 + 真实 LLM 1 turn + ApprovalPolicy 拦截 + Go/No-Go 决策记录）

## 详细制定 TODO（待 C3 ship 后）
- [ ] 1. 决策前置: IHarnessRSI 输入 schema (mutation 表达力) + 双门禁交互协议
- [ ] 2. 写完整 design.md（IHarnessRSI 实现 + 端到端 mock 流程 + ApprovalPolicy 交互）
- [ ] 3. 写完整 tasks.md（mock 闭环 5 case × 5 步 + 真实 LLM 1 case + ApprovalPolicy 2 case）
- [ ] 4. 写完整 spec.md（R1-R6 见 Capabilities 章节）
- [ ] 5. 移除 PLACEHOLDER 标记
- [ ] 6. openspec validate
- [ ] 7. 更新 master plan §四 C4 状态
- [ ] 8. 启动 Sprint 36 实施
- [ ] 9. **pilot 完成后**: 写 Go/No-Go Decision Record
- [ ] 10. **Go 路径**: 立项 ADR-0078 Model-RSI pilot (Wave 3)
- [ ] 11. **No-Go 路径**: 归档 Wave 2 skeleton

## 依赖
- **上游**: C3 (h-d-m-transition-guard) ship — 双门禁基础
- **下游** (Go 决策后): Wave 3 (ADR-0078 Model-RSI pilot)

## 关联文档
- MetaRSI-v1 论文 Harness-RSI 概念 (unverified)
- `docs/architecture/self-evolution-architecture-2026-08.md` §三
- `docs/adr/adr-0084-mutation-governance-contract.md` (V1 ship)
- `docs/adr/adr-0061-13-distillation-output-format.md`
- Oracle 评审: `task_id=ses_f55f307f6ffeRJ9SIny8iUbZ8Y` §6 替代方案
- Master plan: `docs/roadmap/2026-09-16-pdk-chat-demo-evolution-roadmap.md` §五 Sprint 36
