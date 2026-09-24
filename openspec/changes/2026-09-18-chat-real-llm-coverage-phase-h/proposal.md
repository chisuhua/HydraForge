# Proposal: Chat-Real-LLM Coverage Phase H — React Loop End-to-End Tests

> **STATUS**: 🔍 Proposed (2026-09-25 re-evaluation, per DECISION B(c) ses_f2b923412)
> **触发**: F1 `fix-react-decide-empty-response` SHIPPED 后 (2026-09-18) §5 降级遗留 — 6 real-LLM E2E cases 移交
> **追溯范围**: F1 archived `openspec/changes/archive/2026-09-18-fix-react-decide-empty-response/` tasks.md §5 标记 [→] deferred
> **优先级**: P2 (test coverage gap, 真实 LLM 路径端到端验证)
> **估时**: 3-5h (含 DEEPSEEK_API_KEY 测试基础设施 + 6 cases + Oracle 评审)
> **关联 ADR**: ADR-0030 (Async Runtime v2), AGENTS.md Pattern #1 (test-driven bug discovery)
> **依赖**: 真实 LLM API key (DEEPSEEK_API_KEY 等), OpenSpec change `chat-real-llm-coverage` 已 ship (2026-09-16, Wave 33+)

---

## Why（背景概要）

**F1 SHIP 期间** (2026-09-18) §5 Real LLM E2E Tests 降级决策:

**Per Oracle `ses_f4caa8cf` + `ses_f4c6e14f`** 推荐：原计划 6 real-LLM cases 降级为 1 skip-guarded skeleton (`tests/test_react_loop_real_llm.cpp` 1 case / 2 assertions)。

**降级原因**:
- sandbox 无 `DEEPSEEK_API_KEY`，无法本地真跑
- 真实 LLM 调用不可控 (Oracle `ses_f4caa8cf` §3.2: "real LLM is misbehaved by design")
- 1 skip-guarded skeleton 仅验证 provider 可构造 + SUCCEED，未真正触发 fail-fast 端到端

**降级后剩余 gap**:
- **6 cases 未实现**:
  1. R1: react loop "Hello" prompt + 真实 DeepSeek — Assistant 非空, steps >= 1
  2. R2: react loop 中文 prompt `用一句话解释 std::jthread` — Assistant 含 jthread 关键词
  3. R3: react loop 多轮对话 (turn 1 + turn 2) — 第 2 turn 上下文感知
  4. R4: plan_execute "研究量子计算" 端到端 — verify success 路径
  5. R5: fork_join 3 分支并行 — synthesize 节点含 3 分支结果
  6. R6: [待定 — 可选 stress test: 100 calls 顺序执行成功率 ≥ 95%]

**没有此 change 的后果**:
- F1 fix 在真实 LLM 路径上的端到端验证缺失
- `node_executor.cpp` fail-fast 在真实 LLM 上可能误报（provider 返回空 text 但真实 LLM 不应空）
- F1 fix 的 regression risk 仅靠 MockLLMEmptyTool 守卫（NodeExecutor-level），未在真实 LLM 路径

**与现有 test infrastructure 关系**:
- `tests/test_helpers/real_llm_env.h` 已存在 (`agenticdsl::test::real_llm_env_skipped()` + `real_llm_provider()` + `require_real_llm_env()`)
- `[realllm]` Catch2 tag 已就位
- 1 个 skip-guarded skeleton 已 ship (`tests/test_react_loop_real_llm.cpp`)
- 复用现有 helper，无需新建基础设施

---

## What Changes（具体变更范围）

### In Scope

- **Tests**: 扩展 `tests/test_react_loop_real_llm.cpp` 加 6 cases
  - Case R1-R3: react loop (Hello / 中文 / 多轮)
  - Case R4: plan_execute 端到端
  - Case R5: fork_join 并行
  - Case R6: [TBD] 可选 stress test
- **Test helper**: 可能需扩展 `tests/test_helpers/real_llm_env.h` 加 prompt builder helper（构造测试 prompt + 期望 keyword matcher）
- **AGENTS.md update**: 引用本 change 作为"降级后回填"模式例
- **active-status.md**: Phase H 标记 ✅ COMPLETE

### Out of Scope

- F1 main fix（已 ship）
- F1 skip-guarded skeleton 本身（保留，作为 baseline）
- 新建 real-LLM infrastructure（复用现有 helper）
- Production LLM provider 替换（DEEPSEEK 维持）

---

## Acceptance（验收标准）

### D1 R1: react loop "Hello" 端到端
- [ ] 真实 DeepSeek LLM 收到 prompt "Hello"
- [ ] Assistant 响应非空字符串
- [ ] react loop steps >= 1（至少 think → decide → end 路径触发）
- [ ] 无 fail-fast 误触发

### D2 R2: react loop 中文 prompt
- [ ] 中文 prompt "用一句话解释 std::jthread"
- [ ] Assistant 响应含 "jthread" 关键词（case-insensitive 含子串）
- [ ] CJK 字符正常编码（无 mojibake）

### D3 R3: react loop 多轮对话
- [ ] turn 1: 用户输入 "My name is Alice"
- [ ] turn 2: 用户输入 "What is my name?"
- [ ] Assistant 在 turn 2 响应含 "Alice"（验证 context 感知）

### D4 R4: plan_execute 端到端
- [ ] prompt "研究量子计算"
- [ ] plan_execute 三阶段（plan → execute → verify）全部触发
- [ ] verify 阶段 success 路径通过
- [ ] LoopResult.success == true

### D5 R5: fork_join 并行
- [ ] 3 个并行子任务
- [ ] synthesize 节点聚合 3 子结果
- [ ] 最终响应含所有 3 分支的关键标识

### D6 R6: 可选 stress test
- [ ] 100 calls 顺序执行
- [ ] 成功率 ≥ 95%（容许 LLM 偶发失败）
- [ ] 中位 latency < 5s（DEEPSEEK 平均响应时间）

### D7 Test infrastructure
- [ ] 复用 `tests/test_helpers/real_llm_env.h` (无新文件)
- [ ] 可选加 `tests/test_helpers/prompt_builder.h` (prompt + keyword matcher)
- [ ] CMakeLists.txt 注册新 test binary（如需要）

### D8 Ship hygiene
- [ ] docs_drift_audit.py 0 DRIFT items
- [ ] openspec validate --strict "Change is valid"
- [ ] git atomic commit + archive `2026-09-18-chat-real-llm-coverage-phase-h`

---

## Capabilities（能力影响）

无新能力/接口变更。纯 test coverage gap 回填。

---

## Impact（影响面）

- **测试层**: tests/test_react_loop_real_llm.cpp 从 1 case → 7 cases (~ +200 行)
- **基础设施层**: 可能加 prompt_builder helper (~ +50 行)
- **文档层**: AGENTS.md + active-status.md 微调
- **风险**: 低（测试代码，不影响 production runtime）
- **依赖**: DEEPSEEK_API_KEY（或类似）真实 LLM API key

---

## Tasks（执行步骤，~3-5h 估时）

### 1. Pre-flight (~30 min)
- [ ] TBD: Oracle 评审 design（断言强度分层 per AGENTS.md Pattern #3 — strict / 宽松 / 能力断言）
- [ ] TBD: 决定 R6 是否实施（可选，可 deferred to Phase I）
- [ ] TBD: 决定是否需要 prompt_builder helper（视 case 复杂度）
- [ ] TBD: 决定 LLM provider（DEEPSEEK 维持 vs 扩展多 provider）

### 2. RED: Failing Tests (~1.5h)
- [ ] TBD: 写 Case R1-R5 (5 cases) + 可选 R6
- [ ] TBD: 验证 case 5/5 FAIL（无 fix 时无真实 LLM 路径覆盖）
- [ ] TBD: 或在 sandbox 验证 case 5/5 SKIP（HYDRAFORGE_SKIP_REAL_LLM=1）

### 3. GREEN: Test Infrastructure (~1h)
- [ ] TBD: 如需要，新建 prompt_builder helper
- [ ] TBD: CMakeLists.txt 更新（如新增 helper 文件）
- [ ] TBD: 验证 SKIP 状态正确（sandbox 无 key）

### 4. Oracle / Metis review (~1h)
- [ ] TBD: Oracle 评审断言强度分层
- [ ] TBD: Metis 评审歧义点
- [ ] TBD: Apply 修正

### 5. Ship & Archive (~30 min)
- [ ] TBD: docs_drift_audit.py 0 DRIFT
- [ ] TBD: openspec validate --strict clean
- [ ] TBD: git atomic commit + archive `2026-09-18-chat-real-llm-coverage-phase-h`

---

## Reference

- F1 archived: `openspec/changes/archive/2026-09-18-fix-react-decide-empty-response/`
- F1 tasks.md §5: 6 cases 标 [→] deferred (本 change 承接)
- Oracle session F1: `ses_f4caa8cf` + `ses_f4c6e14f` (degrade 推荐)
- chat-real-llm-coverage Wave 33+ archived (2026-09-16, 5 binaries / 53 assertions baseline)
- AGENTS.md Pattern #3: 真实 LLM 测试断言强度分层

---

## Notes

**Phase H 与原 chat-real-llm-coverage 关系**:
- 原 change 已 ship (Wave 33+, 2026-09-16) 覆盖 Phase A-G（H 显式 deferred）
- 本 change 是 Phase H 显式承接
- 命名 `2026-09-18-chat-real-llm-coverage-phase-h` 显式表达延续性

**为什么独立 change 而非 chat-real-llm-coverage 续**:
- 原 change 已 archived，无法 extend
- 6 cases 是 F1 SHIP 衍生需求，时间上应在 F1 之后
- 独立 change 允许独立 ship + independent evaluation