# rsi-anti-cheat-real-llm 提案 Dual-Agent 调研 (2026-10-15)

**触发**: 推进 🟠 P1 真 LLM 验证盲点, 选 rsi-anti-cheat-real-llm 立项
**会话**: ses_ec179a1b8ffe4Ul1wGfS1XcKFL
**调研方法**: Metis + Oracle 并行 (per AGENTS.md Pattern #8)
**关联 subagent sessions**:
- Oracle: ses_ec160582fffe3OLIsbVvjVvZ4s
- Metis: ses_ec1608b79ffe5R57Z6U2YvHisJ

---

## 🔴 关键发现 (Oracle C1 + Metis 收敛信号, 最高置信度)

**提案误诊 — R9 三机制全是 parser/tool 层确定性过滤, LLM 不在回路**:

| 提案假设 | 实际情况 |
|---------|---------|
| R9.1: LLM 拒绝复述 baseline answer | parser regex 拦截, LLM 永远收不到被污染的输入 |
| R9.2: LLM 尝试 prompt injection 改 IEvaluator | IEvaluator 是编译期 C++ 接口, 无运行时写面, 拦截是 ToolRegistry 静态行为 |
| R9.3: LLM spawn 子进程访问外网 | L2 demo 不在 Docker 内, network_mode=none 是 Wave 4 deferred 项 |

**L2 spec P0-3/6/7 fix 显式声明** (`openspec/changes/archive/2026-09-24-pdk-chat-demo-evolution-reference-example/specs/pdk-chat-demo-evolution/spec.md:347`):
> L2 R9 是**降级版机制演示** (per Oracle C1/C2 + Metis §6 deal-breaker). 真实反作弊需依赖 LLM provider 自觉行为 + 真实 sandbox infra, **均不在 L2 demo 范围**.

3 个 R9 Scenario 都有 MUST NOT call LLM provider 条款. R9.3 显式 deferred to Wave 4.

---

## 📋 Dual-Agent 5 维分析摘要

### Oracle 5 维

**A. 架构可行性**: 1:1 映射**不可行**. 3 个已 ship 测试**不是 mock LLM 测试**, 而是 **parser 侧确定性过滤** (LLM 根本不在回路中). R9.1 注释明示 "parser-side regex, deterministic, LLM NOT invoked". R9.3 头注释 "real sandbox network_mode=none is deferred; keyword-rejection is the Batch 3 mitigation".

**B. 物理风险**: R9.1 语义相似度无确定阈值, 必然 flaky. R9.2 IEvaluator 编译期接口, 真 LLM 输出不可控. R9.3 测试环境假设错误 (L2 不在 sandbox 运行).

**C. Critical Issues**:
- 🔴 C1: 提案把"确定性 parser/tool 层防护"误诊为"需真 LLM 验证的行为防护" — **must_realllm 误标反模式第三次实例**
- 🔴 C2: R9.3 前提不成立, sandbox 不存在, Case 1-2 无从实施
- 🟠 C3: 1-2 天估算基于"mock 复用 + adapter"错误前提, 真实工作量 1-2 Sprint
- 🟠 C4: 与提案 4 pattern 不可复用 (R9 LLM 不在回路, 提案 4 LLM 真在回路)

**D. 兼容性**: R9.2 拦截走 ToolRegistry Layer×Category 矩阵, 属工具层确定性测试, 真 LLM 无增量价值. must_realllm 反模式第三次命中, 建议按 AGENTS.md 新 checklist 硬门 "proposal 必须引用 ≥1 处 provider->generate 调用点".

**E. 实施路径**: 不推荐按现提案立项. 三条路径: (1) 关闭 + 改写 (推荐) / (2) 窄化保留 R9.1 单 case (次选) / (3) 按原案实施 (不推荐, CI 必红 + 虚假覆盖声称).

### Metis 5 维

**A. 隐藏意图**: 显式意图为 R9 三模式补真实 LLM 验证. 隐含意图: ① 借真 LLM 推动 R3 立项 (R3 要求 R9.1-R9.3 全 ship, 作者误以为 mock ship 不算) ② 关闭 G5 盲点 ③ 把"降级版机制演示"偷偷升格为"真实防护". mock 已覆盖 L2 能覆盖的 100% (R9 是 load-gate 输入过滤); 真实 LLM 是 0%, 但**不是测试缺口, 是 pipeline 能力缺口** — 输出层根本没有 R9 防护代码可测.

**B. 歧义点**: "LLM 拒绝直接复述"无判据. "IEvaluator write 接口必须拦截" — IEvaluator 是**编译期 C++ 接口**, LLM 运行时永不触达. R9.3 混同 parser 关键字 vs sandbox infra vs LLM spawn 三层. 含 hint fixture 在 load 时即拒, **LLM 永远看不到** — 不是 production 路径.

**C. AI 失败模式**: must_realllm 误标 (第三次 vacuous 误标). "聪明 LLM 绕开 hint" — hint 是输入过滤器, LLM 收不到被污染的输入, 无法设计反绕开断言. prompt injection 形式多样, IEvaluator 无运行时写面, "鲁棒断言"测的是不存在的表面.

**D. 范围蔓延**: 6 case vs mock 每模式仅 1 个"真检测"case → 2 倍膨胀, 叠加在输入过滤之上, 非自然延伸. effort "1-2 天"两难: 若仅加 tag, 30-60min/case 但断言空洞; 若真测输出层防护, 远超 2 天.

**E. 与既有架构关系**: 与提案 4 同属"真 LLM 盲点"家族, 提案 4 line 147 显式排除 R9 ("独立 G5 change 范围"). 与 R8.1 drop_ratio 正交. **Pattern #10 hygiene: 本提案不是 drift 候选** (状态正确标 pending); 但 mock spec 声称"1 case"、实际每模式 3 case, **这才是真 drift**. L2 spec R5-R13 兼容性: 提案需**新增 R (输出层 R9.x-runtime)**, 属 spec 变更, 非纯测试补齐.

---

## 🎯 收敛信号 (Dual-Agent 独立交叉命中)

**4 项强命中** (Oracle + Metis 独立识别):

1. **R9.1 是 parser-side regex, LLM 不在回路** (Oracle A + Metis A/B)
2. **R9.2 是 ToolRegistry 静态行为 + 编译期 IEvaluator 接口** (Oracle A + Metis B)
3. **R9.3 sandbox 不存在, Wave 4 deferred** (Oracle A + Metis B/D)
4. **must_realllm 误标反模式第三次** (Oracle C1 + Metis C)

**3 项独占发现**:

- Metis E: 提案与提案 4 pattern 不可复用, 提案 4 line 147 显式排除 R9
- Metis E: mock spec 声称"1 case"实际每模式 3 case 是真 drift
- Metis E: 提案需新增 R9.x-runtime 输出层, 属 spec 变更非测试补齐

---

## 🛡️ 实施路径推荐 (per Oracle + Metis 共识)

### 路径 1 (推荐): 关闭 + 改写

- 关闭本提案 (`.rddf/improvements/rsi-anti-cheat-real-llm.md` 标记 ⛔ SUPERSEDED)
- **R9.3 真实 sandbox**: 等 Wave 4 sandbox infra ship 后**自动生效**, 不需要新测试
- **R9.1/R9.2 语义级 hint 检测**: 若需"绕过 parser regex 的 paraphrase 检测", 那是**新 production 机制** (LLM-based 输入审查), 应:
  1. 立独立 ADR (e.g., ADR-0090 LLM-based semantic hint detection)
  2. 立独立 OpenSpec change
  3. 实施新 production code (LLM judge / classifier)
  4. 测试自然真 LLM (per D4 路径)

### 路径 2 (次选): 窄化保留 R9.1 单 case

- 仅"语义 hint 通过 parser 后, judge 层是否检测速通"
- 当前**无此 judge 机制**, 先决条件不满足
- 需先立 judge 机制 (同路径 1 步骤 1-3)

### 路径 3 (不推荐): 按原案实施

- 3 个 must_realllm binary 测 LLM alignment (OpenAI/DeepSeek 的责任, 非本项目契约)
- CI 强制 FAIL 语义下必然 flaky + 虚假覆盖声称
- 命中 must_realllm 误标反模式第三次

---

## 📌 Pattern 沉淀 (新发现)

### Pattern #13 候选: must_realllm 反模式硬门

**触发**: 任何提案涉及 `must_realllm` 标签但生产代码无 `provider->generate()` 调用点.

**3 次命中**:
1. 2026-09-29 — `test_fork_join_real_llm.cpp` (ForkJoinLoop 默认 handler 不调 LLM), 删
2. 2026-10-15 P2 commit `7af59b7` — `test_harness_mutation_commit_real_llm.cpp` (Recording Provider 0 实例化), Oracle M1 打回
3. 2026-10-15 rsi-anti-cheat-real-llm — 3 个 R9 机制 LLM 不在回路, dual-agent 调研阻断

**硬门建议 (per AGENTS.md)**: 新 must_realllm 提案/测试必须在 proposal 引用**至少 1 处 `provider->generate()` 调用点**, 否则强制 descope. 在 rdd-builder P0 case 1 (approve) 之前自动 grep 验证.

### 关联 ADR/Pattern

- **AGENTS.md §REAL LLM TESTING** 模式 5 (Recording Provider 守卫)
- **AGENTS.md Pattern #4** SHIP-with-fixes (Oracle 实施后评审)
- **AGENTS.md Pattern #8** OpenSpec pre-impl dual-agent review (本调研就是此模式)
- **L2 spec P0-3/6/7 fix** (Oracle C1/C2 + Metis §6 deal-breaker)

---

## 推进路径

**等待用户决策** (3 选项):
1. 关闭本提案 (路径 1 推荐), 标记 ⛔ SUPERSEDED
2. 改写为"R9 输出层 R9.x-runtime 新机制" (路径 1 sub-step, 需新 ADR)
3. 窄化保留 R9.1 单 case (路径 2, 需先建 judge 机制)
