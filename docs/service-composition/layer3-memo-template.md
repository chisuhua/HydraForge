# Layer 3: Reflection Memo Template

> **用途**: 工程师在完成 G1+G3 composition 实施后，以第一人称撰写 1-page"什么感觉不对"的反思。
> **关联**: ADR-0051 Phase 6 PDK Composition Spike, tasks.md §5.4

---

## §1: What I Expected to See

*(描述实施前的预期 — 预期的合约形式、工具发现方式、错误模式)*

例: "我预期 G1 通过某种服务注册发现 G3，而不是直接 hardcode `knowledge_base/query` 字符串。我预期错误会像 exception 一样逐层传播，而不是被中间层包装为通用失败消息。"

---

## §2: What I Actually Saw (Anomalies, Surprises)

*(描述实际遇到的反直觉事件、与预期偏差)*

例: "MockLLMProvider 的回调必须在 library 加载前设置，否则 handler 返回 `"G3: no response queued"` — 这不是异常，只是一个看起来很奇怪的字符串。错误信息丢失了 — G3 返回 error 时，G1 能看到 `success: false` 但看不到 G3 的原始错误原因。"

---

## §3: What Felt Wrong (Tacit Patterns)

*(描述直觉层面的不适感 — 不易用 checklist 量化的模式)*

例: "感觉在重复自己 — G1 和 G3 的 LLM callback 注册模式一模一样，但我就是各自写了一遍。我感觉如果再加 G4/G5，这些模式会传播而不是收敛。"

---

## §4: Hypotheses for Root Cause

*(对 ❗3 现象的根因假说)*

例: "假设根本原因是缺失 `IDL/服务合约层` — 没有共享的 schema 定义让 Agent 协商 args/return 格式。tool name 通过字符串匹配做类型检查。"

---

## §5: Recommended v2 Changes

*(对 DECLARE_SERVICE 形式化或 v2 contract 修订的具体建议)*

例: "建议 v2 引入 service contract 枚举（至少 shared header），让 caller 通过类型系统表达 `I depends_on T`，而不是 `if has_tool("magic_string")`。"

---

## 如何使用

1. **Primary Engineer** 撰写一份 (保存为 `layer3-memo-primary.md`)
2. **Reviewer Engineer** 独立撰写一份 (保存为 `layer3-memo-reviewer.md`)，禁止参考 primary memo
3. **比较** 两份 memo 的 divergence — 一致的发现表示高置信度 pattern，发散表示正交信号 (§5.9)

---

**最后修改**: 2026-07-15
**基于**: ADR-0051 | tasks.md §5.4