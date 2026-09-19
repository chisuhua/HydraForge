# Tasks: Fix Flatten Layers Comment Drift

> **STATUS: PLACEHOLDER** — depends on F1 ship confirmation

## 1. Pre-flight (~5 min)
- [ ] TBD: grep `flatten_layers` src/ tests/ — 列出所有引用点
- [ ] TBD: 验证 react/loop 路径无引用（如有，标记 "初判错"）
- [ ] TBD: Oracle 评审 design（minimal fix, ~30 min 估时合理）

## 2. Code comment audit (~10 min)
- [ ] TBD: 任何 src/ 下注释引用 "flatten_layers + react/loop" → 标记【初判错】或移除
- [ ] TBD: tests/test_dsl_engine_ctx_bridge.cpp 注释核对
- [ ] TBD: tests/test_react_loop_real_llm.cpp 注释核对

## 3. Plan docs alignment (~5 min)
- [ ] TBD: master plan §十 Drift Log 加【初判错】标签
- [ ] TBD: active-status.md F1 节核对

## 4. AGENTS.md pattern refinement (~10 min)
- [ ] TBD: Pattern #1 step 4 追加 "初判与确认根因分离" 子条目
- [ ] TBD: 引用 Oracle `ses_f4d05cdb0` 作为正例

## 5. Docs drift gate (~5 min)
- [ ] TBD: docs_drift_audit.py 0 DRIFT items
- [ ] TBD: openspec validate --strict "Change is valid"

## 6. Ship & Archive (~5 min)
- [ ] TBD: git atomic commit + archive `2026-09-18-fix-flatten-layers-comment-drift`