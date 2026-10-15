# r13-4-confidential-redaction-real-llm 提案 Dual-Agent 调研 (2026-10-15)

**触发**: 推进 🟠 P1 真 LLM 验证盲点, 选 r13-4-confidential-redaction-real-llm 立项
**会话**: ses_ec179a1b8ffe4Ul1wGfS1XcKFL
**调研方法**: Metis + Oracle 并行 (per AGENTS.md Pattern #8)
**关联 subagent sessions**:
- Oracle: ses_ec1540a00ffeoqYH8dp7b32Zcx
- Metis: ses_ec1545fcdffe79LuLYcKsBH38D

---

## 🔴 关键发现 (Oracle + Metis 4 强命中, 最高置信度)

**R13.4 提案是 R9 误标的"隐蔽变体"** — LLM 在回路但**断言目标与 LLM 无关**（redact 是 provider-agnostic 整字段替换）：

| 误标谱系 | 案例 | 类型 |
|---------|------|------|
| 强变体 | rsi-anti-cheat-real-llm (R9.1/2/3) | LLM 不在回路, parser-side 拦截 |
| **弱变体** | **r13-4-confidential-redaction-real-llm (本案)** | **LLM 在回路但断言 LLM-independent** |
| 历史 | test_fork_join_real_llm (2026-09-29 删) | ForkJoinLoop 默认 handler 不调 LLM |
| 历史 | harness-mutation P2 commit `7af59b7` (Oracle M1 打回) | Recording Provider 0 实例化 |

**4 项强命中**:

1. **Case 1/2 断言恒真** (`redact_trace_fields` line 590-592 无条件整字段替换 response, 真 LLM 复述 turn_input 不影响断言结果)
2. **Case 2 物理不可测** (EvolutionSession 是 N 个独立 context 复用 ChatSession, **非 turn1→turn2** 多轮, Oracle 误判"多轮可行但断言仍恒真")
3. **Case 3 幻影需求** (`redact_trace_fields` 582-602 不含 tool_calls, trace schema 不含 tool_calls 字段)
4. **Case 4 spec 冲突** (spec R13.4 保留 context_id/session_id/sensitivity 供 audit trail, 脱敏违反 spec)

**4 项独占发现**:

- Metis C: **跨 context 泄漏**真 bug — 共享 ChatSession 下, public 的 ctx2 response 可能复述 confidential 的 ctx1 内容且不脱敏 (per Oracle `:144-149` 循环复用)
- Metis B: **`expected_eval_quality` 漏脱敏** (per spec 409, 实现漏字段, 是 spec/impl gap 真 bug)
- Oracle B.3: spec R13.4 NOTE `:417` 显式豁免 wire-level: "Real LLM provider call payload is NOT redacted by L2" — confidential turn_input 明文发 DeepSeek 是 **by design**
- Oracle E: **Recording Provider case** 可固化 wire-level 明文设计决策 (合规审计 trail)

---

## 📋 Dual-Agent 5 维分析摘要

### Oracle 5 维

**A. 架构可行性**: 可行性高但验证价值存疑. `redact_trace_fields` 是**纯 JSON 变换**, 与 provider 类型零耦合. 真 LLM 切换已 ship (l2-evolution-real-llm-provider commit `7fc6d46`). Case 1/2 真 LLM **零增量信号**.

**B. 物理风险**: ① 断言恒真 (无条件替换 response, grep 任何 pattern 找不到 leak) ② Case 3 幻影 (trace schema 不含 tool_calls) ③ spec 显式豁免 wire-level ④ Case 1-2 ≈ 2-6 calls cost 低但 ROI 更低.

**C. Critical Issues**:
- 🔴 must_realllm 误标第 4 次弱变体 (LLM 调了但断言不依赖输出)
- Case 2 多轮物理可行 (`chat_session_` 跨 contexts 持久), 但断言仍恒真
- Case 4 spec 冲突
- effort 0.5 天合法范围 (Case 1 + hermetic fixture), 价值 ≈ 0
- mock 测试 line 265-301 已覆盖唯一非平凡路径

**D. 兼容性**: ADR-0068 v2.4 无冲突. Case 4 spec 冲突. D5 fail-fast 复用. hermetic HOME 复用. 两阶段 CI 升级 checklist 硬门 "提案引用 ≥1 处断言依赖 provider 输出的调用点" — Case 1/2 不满足.

**E. 实施路径**: 不推荐原案. 路径 1 (推荐): 降级为 contract/静态守卫 + Recording Provider case 固化 spec NOTE. 路径 2 (可选): 最小真 LLM smoke [realllm] 非 must_realllm. 路径 3: Case 3/4 拆独立 change.

### Metis 5 维

**A. 隐藏意图**: 与 R9 对比关键 — R9 是 "LLM 完全不在回路"; 本提案 LLM 确在回路 (line 201 `provider->generate` 真实存在), 但**被测对象 `redact_trace_fields` 是 provider 无关**, 效果上等同误标 (第 4 次).

**B. 歧义点**: Case 1 脱敏是"整字段替换", 断言与 mock 完全相同 → 空洞. Case 2 EvolutionSession 无多轮模型 → 物理不可测. Case 3 `redact_trace_fields` 根本不含 tool_calls → 新 production 功能. Case 4 spec 要求脱敏 `expected_eval_quality`, 实现漏此字段 → 真 bug 但与 LLM 无关.

**C. AI 失败模式**: 最大失败 **must_realllm 误标第 4 次** (提案全文 0 处引用 `provider->generate`). spec 417 NOTE 明确"provider call payload 不由 L2 脱敏", 进一步证明此测试测的是 provider-agnostic 逻辑. **跨 context 泄漏**真 bug.

**D. 范围蔓延**: 4 case vs mock 1 case, 4 倍膨胀. Case 3 + Case 4 = 新 production code + 新 ADR. effort 严重低估. 30-45 真调用 cost, 断言空洞 → 纯浪费 token.

**E. 与既有架构关系**: R9 因"LLM 不在回路"被砍; 本提案 LLM 在回路但**断言目标与 LLM 无关** → "误标"隐蔽变体. 提案 4 pattern 直接适用: 同 P2 因 must_realllm 误标被 Oracle M1 打回, 重命名为确定性集成测试.

---

## 🎯 收敛信号 (Dual-Agent 独立交叉命中)

**4 项强命中** (Oracle + Metis 独立识别):

1. **断言恒真 / LLM 零增量信号** (Oracle A + Metis A/C)
2. **Case 2 物理不可行 / 断言恒真** (Oracle C + Metis B, Oracle 称"多轮可行但断言恒真"vs Metis 称"无多轮模型" — 共识=Case 2 不可 ship)
3. **Case 3 幻影需求** (Oracle C.2 + Metis B)
4. **Case 4 spec 冲突 / 漏脱敏真 bug** (Oracle C.3 + Metis B)

**3 项独占发现**:

- Metis C: 跨 context 泄漏真 bug (共享 ChatSession, public ctx2 复述 confidential ctx1)
- Metis B: `expected_eval_quality` 漏脱敏 (spec 409 vs impl 582-602)
- Oracle B.3: spec 417 NOTE 显式豁免 wire-level, 需 Recording Provider case 固化设计决策

---

## 🛡️ 实施路径 (per Oracle 路径 1 推荐 + 用户 2026-10-15 决策)

### 步骤 1: 关闭本提案 (Path 1 同 rsi-anti-cheat-real-llm)

`chore(closure): r13-4-confidential-redaction-real-llm 提案关闭 (Path 1)`, 标记 ⛔ SUPERSEDED + 引用 dual-agent 调研 + L2 spec NOTE 417

### 步骤 2: 新建 deterministic 静态守卫 (mock only)

`tests/test_r13_4_redaction_static_guard.cpp`:
- Case 1: 构造 response 含 turn_input 复述 (e.g., `"You asked about secret internal diagnosis, here's the answer..."`) → redact 后断言 `[REDACTED-confidential]`
- Case 2: meta.context_id / meta.session_id / meta.is_hidden / meta.eval_quality 字段非脱敏 (spec R13.4 NOTE 保留 audit trail) → 断言保留
- Case 3: sensitivity="internal" 路径验证 (与 confidential 平行的 level)
- 不加 must_realllm 标, 普通 `l2-evolution` tag 即可

### 步骤 3: Recording Provider case 固化 wire-level 明文设计 (per Oracle E + spec 417)

- 在 test_r13_4_redaction_static_guard.cpp 追加 Case 4: Recording Provider 录制 LLM call payload, 断言 `request.turn_input == "secret internal diagnosis: ..."` (明文发 DeepSeek, by design per spec 417)
- 注释明确: 此 case **固化** spec NOTE 417 设计决策, 不是测试脱敏 (脱敏由 L2 侧负责, wire 侧接受明文)

### 步骤 4: 立独立 change 修 2 个真 bug

新 OpenSpec change `2026-10-15-r13-4-real-bug-fixes`:
- Bug 1: `expected_eval_quality` 漏脱敏 (per spec 409)
  - 修改 `evolution_session.cpp:582-602` redact 字段列表, 加 `expected_eval_quality` (if sensitivity == "confidential" 或 "internal")
  - 新 test case 验证
- Bug 2: 跨 context 泄漏
  - 修改 `evolution_session.cpp:144-149` ChatSession 复用模式, 加 context-scope 隔离 (新 ChatSession per context OR explicit reset)
  - 新 test case 验证 public ctx2 响应不含 confidential ctx1 turn_input

---

## 📌 Pattern 沉淀 (新发现, 升级 must_realllm 误标反模式分类)

### Pattern #13: must_realllm 误标反模式 (升级分类)

**触发**: 提案标 `must_realllm` 但生产代码 / 断言与 LLM 输出弱/零耦合.

**4 次命中谱系**:

| 强度 | 案例 | 描述 |
|------|------|------|
| 强 | rsi-anti-cheat-real-llm (R9.1/2/3) | LLM 不在回路 (parser-side 拦截) |
| **弱** | **r13-4-confidential-redaction-real-llm** | **LLM 在回路但断言 LLM-independent (provider-agnostic 整字段替换)** |
| 强 | test_fork_join_real_llm (2026-09-29 删) | ForkJoinLoop 默认 handler 不调 LLM |
| 强 | harness-mutation P2 commit `7af59b7` (Oracle M1 打回) | Recording Provider 0 实例化 |

**升级硬门建议** (per Oracle D.5 + Metis E):
- 旧硬门 (提案 4 P2 后): "proposal 引用 ≥1 处 `provider->generate` 调用点"
- **新硬门 (本案后)**: "proposal 必须引用 ≥1 处**断言依赖 provider 输出**的调用点 + Recording Provider 录制的 req/payload 包含至少 1 个 LLM 输出字段被断言使用"
- 通过 grep + 静态分析自动验证, 在 rdd-builder P0 case 1 (approve) 之前

---

## 推进状态

**用户决策 (2026-10-15)**: Oracle 路径 1 (推荐) — 关闭本提案 + 静态守卫 + Recording Provider + 立独立 change 修 2 个真 bug.

**预计产出**: 4 atomic commits + 1 OpenSpec change (`2026-10-15-r13-4-real-bug-fixes`).
