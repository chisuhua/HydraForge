# Tasks: Chat-Real-LLM Coverage Phase H

> **STATUS: PLACEHOLDER** — depends on F1 ship confirmation + DEEPSEEK_API_KEY

## 1. Pre-flight (~30 min)
- [ ] TBD: Oracle 评审 design（断言强度分层 per AGENTS.md Pattern #3 — strict / 宽松 / 能力断言）
- [ ] TBD: 决定 R6 是否实施（可选，可 deferred to Phase I）
- [ ] TBD: 决定是否需要 prompt_builder helper（视 case 复杂度）
- [ ] TBD: 决定 LLM provider（DEEPSEEK 维持 vs 扩展多 provider）

## 2. RED: Failing Tests (~1.5h)
- [ ] TBD: 写 Case R1-R5 (5 cases) + 可选 R6
- [ ] TBD: 验证 case 5/5 FAIL（无 fix 时无真实 LLM 路径覆盖）
- [ ] TBD: 或在 sandbox 验证 case 5/5 SKIP（HYDRAFORGE_SKIP_REAL_LLM=1）

## 3. GREEN: Test Infrastructure (~1h)
- [ ] TBD: 如需要，新建 prompt_builder helper
- [ ] TBD: CMakeLists.txt 更新（如新增 helper 文件）
- [ ] TBD: 验证 SKIP 状态正确（sandbox 无 key）

## 4. Oracle / Metis review (~1h)
- [ ] TBD: Oracle 评审断言强度分层
- [ ] TBD: Metis 评审歧义点
- [ ] TBD: Apply 修正

## 5. Ship & Archive (~30 min)
- [ ] TBD: docs_drift_audit.py 0 DRIFT items
- [ ] TBD: openspec validate --strict "Change is valid"
- [ ] TBD: git atomic commit + archive 2026-09-18-chat-real-llm-coverage-phase-h