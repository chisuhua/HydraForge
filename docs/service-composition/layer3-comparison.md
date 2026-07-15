# Layer 3 Comparison — Divergence Analysis

> **用途**: 对比 Primary Engineer 和 Reviewer Engineer 各自的 Layer 3 memo，识别**一致**（高置信度）与**发散**（正交信号）的发现。
> **关联**: ADR-0051 Phase 6 PDK Composition Spike, tasks.md §5.9
> **日期**: 2026-07-15

---

## ✅ 一致性发现 (AGREE — 高置信度)

这些 pattern 在 Primary 和 Reviewer 的独立反思中**同时被捕获**，置信度最高。

| # | 发现 | Primary 提及 | Reviewer 提及 | 优先级 |
|---|------|-------------|--------------|--------|
| A-1 | **LLM callback 签名不支持 error return** — `string→string` 类型无法表达 failure mode (Defect #6 根因) | §5.4 (callback 模式 copy-paste) | §2 (callback 类型不表达 failure mode) | 🔴 P0 |
| A-2 | **魔术字符串 "G3: no response queued" 伪装为有效 answer** — 无 callback 时 fallback 进入正常 data flow | §2 ("这不是 exception 也不是 error") | §2 ("隐式成功路径") | 🔴 P0 |
| A-3 | **Hardcoded tool name 无编译时检查** — G1 用字符串 `"knowledge_base/query"` 匹配 G3 | §3 ("运行时发现失败") | §1 ("期望 find_package 风格") | 🟠 P1 |
| A-4 | **缺失 shared contract header** — G1/G3 合约在 README.md 而非 .h 文件中 | §4 (H1: 缺失服务契约层) | §3 ("假装类型安全") | 🟠 P1 |
| A-5 | **LLM callback pattern copy-paste** — G1 和 G3 各自实现相同功能的 set/reset/enqueue | §3 (item 1) | §3 (item 3) | 🟡 P2 |
| A-6 | **call_tool 签名保真度不足** — `string→json` 丢失了类型信息 | §4 (H1) | §4 (H2) | 🟠 P1 |

**结论**: 6 项一致发现，其中 2 项 P0 (callback error type / magic string)，3 项 P1。这些是 Spike 最有价值的输出 — 两个独立审查者同时看到同样的根本性问题，表明这不是个人偏好而是**系统性设计缺口**。

---

## 🔀 发散性发现 (DIVERGE — 正交信号)

每个审查者捕获了对方面没有注意到的 pattern — 这些是**互补的**，不是冲突的。

| # | Primary 独有的发现 | Reviewer 独有的发现 | 互补性 |
|---|-------------------|---------------------|--------|
| D-1 | **30 行 handler 约束限制错误处理** — `handle_knowledge_base_query` 16 行太短，无法添加 try-catch/error_code 映射 (§3 item 3) | **handler 无 try-catch 保护 LLM 调用** — 异常穿透导致进程崩溃 (§2 第一条) | 🟢 Strong — Primary 看到约束，Reviewer 看到后果，两者互补揭示了根源 |
| D-2 | **MockLLMProvider 多线程不安全性** — Worker pool 并发调用时数据竞争 (§2 第一条) | **G1 registry 指针 data race** — 写带锁读不带锁 (§2 第二条) | 🟢 Strong — Primary 看到 LLM 线程问题，Reviewer 看到 registry 线程问题，两个独立的 race condition 都需要修复 |
| D-3 | **错误传播丢失根因** — G1 把 G3 error 替换为通用消息 (§2 第三条) | **callback 异常未捕获** — `g3_internal_llm()` 的异常会穿透整个调用栈 (§2 第一条) | 🟡 Moderate — Primary 关注数据流中的错误丢失，Reviewer 关注控制流中的异常穿透 |
| D-4 | **Agent 间缺少类型级区分** — 都是"注册一个 tool 的 plugin" (§5) | **Agent 角色分类缺失** — 不区分 orchestrator vs compute (§3 item 3 / §5 item 1) | 🟢 Strong — Primary 和 Reviewer 从不同角度看到了同一个根因：Agent 类型系统的缺失 |

**结论**: 4 组 divergent findings，全部互补而非冲突。Primary 关注**实施中的约束和体验**（30 行 handler 太紧、错误被吞没的感觉），Reviewer 关注**架构和类型系统的缺陷**（data race、异常穿透、角色分类）。两者共同指向同一个根本结论：**v1 Spike 需要引入 AgentRole 概念和更强的类型安全**。

---

## 📊 净信号评估

| 指标 | 值 | 解读 |
|------|-----|------|
| 共识发现数 | 6 | High confidence patterns |
| 发散发现数 | 4 | 互补信号，无 conflict |
| P0 问题总数 | 3 | callback error type + magic string + LLM 异常穿透 |
| 触及 ADR-0051 决策数 | 4/6 | Decisions 2, 3, 4, 5 受影响 |

---

## 🚦 建议: 是否加速 DECLARE_SERVICE 形式化？

**当前状态**: DECLARE_SERVICE 宏被推迟到 v2 (ADR-0051 §Decision 1)，等待"2+ 不同类别 awkward pattern 涌现"触发形式化。

**本分析结果**:
- **类别数**: 5 个 checklist 类别中，4 个类别出现 P0/P1 问题（Contract Drift, Lifecycle Coupling, Error Propagation, Resource Lifetime）
- **严重度**: 3 个 P0（callback error type, magic string, exception passthrough）
- **双审查收敛**: 6 项一致 + 4 项互补，零冲突

**建议**: 🔴 **YES — 满足形式化触发条件。** 虽然 ADR-0051 §Decision 1 说"2+ different awkward pattern categories + L1 reviewer agreement + Layer 3 dual memo convergence"，这里满足全部三个条件：
1. 4 个类别有 P0/P1 问题 ✓
2. L1 reviewer 一致性高（6 项 AGREE，零 CONFLICT）✓
3. Layer 3 dual memo convergence（§A-1 到 A-6 同时被捕获）✓

**具体建议**: 在 v1 Spike ship 时同步创建一个 `DECLARE_SERVICE` 提案草稿 (ADR-0052)，定义最小可行的服务契约宏，包含：
- shared contract header 自动生成
- tool name 常量化
- args/return schema 编译时验证
- AgentRole 分类

**不立即兑现的原因**（保持 ADR-0051 §Decision 1 的谨慎性）:
- 当前仅 G1+G3 两个 agent — 2-sample bias，样本不够
- G2/G4/G5 可能揭示新的 pattern 类别（如 Streaming/Cache/Browser-specific）
- W2-W3 被 Stage Gate + capacity blocker 卡住，没有实施窗口