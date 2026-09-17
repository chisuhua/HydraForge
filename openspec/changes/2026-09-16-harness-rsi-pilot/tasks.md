# Tasks: Harness-RSI Pilot

> **STATUS: PLACEHOLDER** — depends on C3 ship

## 1. Pre-flight
- [ ] TBD: IHarnessRSI 输入 schema (mutation 表达力)
- [ ] TBD: 双门禁交互协议（evaluate_readiness + MutationGovernance）
- [ ] TBD: 端到端 mock 流程编排

## 2. Tests (RED)
- [ ] TBD: Mock Case 1: 简单 prompt_delta → ChatSession 重新加载 → 1 turn 响应包含新内容
- [ ] TBD: Mock Case 2: tool add (trusted) → ChatSession 工具注册成功
- [ ] TBD: Mock Case 3: tool remove → ChatSession 工具取消注册
- [ ] TBD: Real LLM 1 case: deepseek 配置 + 1 prompt_delta 验证（CI skip, 本地跑）
- [ ] TBD: ApprovalPolicy Case 1: low-trust 工具 add 被拦截
- [ ] TBD: ApprovalPolicy Case 2: dangerous 工具 add 被拦截

## 3. Implementation (GREEN)
- [ ] TBD: include/agenticdsl/evolution/harness_rsi.h (IHarnessRSI interface)
- [ ] TBD: src/modules/evolution/harness_rsi.cpp (首个真实实现)
- [ ] TBD: 调 ChatConfig::override_system_prompt (prompt_delta 路径)
- [ ] TBD: 调 ToolRegistry::register/unregister (tools 路径)
- [ ] TBD: 调 IGenomeRegistry::commit (workflow 路径)
- [ ] TBD: 集成 evaluate_readiness (C3)
- [ ] TBD: 集成 MutationGovernance (ADR-0084)

## 4. End-to-end verification
- [ ] TBD: Mock 闭环 3 case 全部 PASS
- [ ] TBD: 真实 LLM 1 turn 验证 (本地跑, deepseek 配置)
- [ ] TBD: ApprovalPolicy 拦截 2 case PASS

## 5. Go/No-Go Decision
- [ ] TBD: 写 Decision Record (docs/audits/<date>-harness-rsi-pilot-go-no-go.md)
- [ ] TBD: Go 路径: 立项 ADR-0078 Model-RSI pilot (Wave 3) → 写新 OpenSpec change proposal
- [ ] TBD: No-Go 路径: 归档 Wave 2 skeleton → 等需求驱动

## 6. Ship gate
- [ ] TBD: ctest 零回归
- [ ] TBD: adr_lint 0 errors
- [ ] TBD: docs_drift_audit 0 DRIFT
- [ ] TBD: openspec validate
- [ ] TBD: dual-agent review (Metis + Oracle)

## 7. Archive
- [ ] TBD: openspec archive
- [ ] TBD: 更新 master plan §四 C4 状态 + §十 Drift Log (Go/No-Go 决策)

## 8. Out-of-scope
- [ ] TBD: 不实现 Model-RSI 实际执行
- [ ] TBD: 不实现真实 LoRA 训练
- [ ] TBD: 不实现多 Agent 协同进化

## 9. References
- [ ] TBD: MetaRSI-v1 论文 Harness-RSI 概念 (unverified)
- [ ] TBD: ADR-0084 (Mutation Governance V1 ship)
- [ ] TBD: ADR-0061-13 (Distillation Output Format)
- [ ] TBD: Oracle session `ses_f55f307f6ffeRJ9SIny8iUbZ8Y` §6
