# Layer 1 — Static Code Review Checklist (5 类别)

> **用途**: W2 D5-D7 期间,Primary Engineer + Reviewer 各独立运行此 checklist,在 G1/G3 源码中寻找 5 类 awkward pattern。每类别至少 2 个 concrete signals 辅助判定。

## 类别 1: Stateful Tool (状态化工具)

**判定**: handler 外部存在 mutable state,且被多次调用读取/修改。

**Signals**:
- (a) handler 函数体外有 `std::unordered_map` / `std::map` 等容器,且 handler 内调用 `.find()` / `operator[]` / `.insert()` / `.erase()` 非 const 方法
- (b) 同一个 `session_id` 在多次调用间产生不同行为(基于 stored state),且 handler 签名无 `const` 标记
- (c) state 的生命周期 > 单次 handler 调用,且未通过 RAII scope guard 自动清理

**反例 (Not pattern #1)**: handler 仅读取 `args_map["key"]`,无外部 mutable state。

## 类别 2: Nested Agent Behind Tool (工具后嵌套 Agent)

**判定**: callee 工具内部调用了 `ILLMProvider::generate()` 或等价 LLM 调用,且返回结果被 caller 再次解释。

**Signals**:
- (a) tool handler 调用链到达 `MockLLMProvider::generate()` 或 `LlamaAdapter::generate()`
- (b) LLM 输出在 handler 内被 JSON 解析、文本截取、或作为 downstream decision 的输入(非简单透传)
- (c) handler 维护 `session_store` + 将 LLM output 反哺回同一个 session 的下一次调用(形成 agent loop 的影子)

**反例 (Not pattern #2)**: tool 调 LLM 但仅做文本格式化后直接返回,无状态积累。

## 类别 3: Context Threading via Args (上下文透传 via args)

**判定**: G1 通过 args 显式传递 session/context token,而非通过隐式的 Platform 层管理。

**Signals**:
- (a) G1 的 `call_tool("knowledge_base/query", args)` 中 `args["session_id"]` 由 G1 显式生成/管理(而非 Platform 自动注入)
- (b) 多个 tool 调用共享同一个 `session_id`,且该 ID 的生成逻辑在 plugin 内部
- (c) G1 在不同调用间手动传递 context token (如 `args["conversation_history"] = json.dump(...)`)

**反例 (Not pattern #3)**: session_id 由 DSLEngine/TopoScheduler 自动分配,G1 不需要手动管理。

## 类别 4: Error Flattening (错误扁平化)

**判定**: 内层 tool 的错误被外层封装为 success payload,导致 caller 误认为操作成功。

**Signals**:
- (a) tool handler 捕获异常后返回 `{success: true, answer: "<error occurred>"}`,而不是 `{success: false, error: "..."}`
- (b) G1 的 ReAct loop 在收到 `{success: true}` 后未检查 `answer` 字段内容就直接用于 synthesis
- (c) G3 内部 MockLLMProvider 调用失败 → handler 返回 `{success: true, answer: ""}` 空字符串,被 G1 透传

**反例 (Not pattern #4)**: handler 区分 success/error 路径且字段正确设置。

## 类别 5: Sync-Async Impedance (同步-异步阻抗)

**判定**: caller 同步阻塞等待 callee,但 callee 内部有异步事件流或 long-running task 未返回。

**Signals**:
- (a) G1 调用 G3 时 `call_tool()` 阻塞 G1 的主循环(单线程 React loop),且 timeout 无上限
- (b) G3 内部 MockLLMProvider 被同步调用,但设计意图是 streaming(如 G5 Browser 的场景)
- (c) G1 需要等待多个 tool 的并发结果,但串行逐个调用(sync impedance)

**反例 (Not pattern #5)**: call_tool 超时设置合适,或 callee 本身就是同步操作。

---

## 使用方式

1. **Primary Engineer** 在 W2 D5-6 对 G3 源码运行此 checklist,对 G1 源码运行此 checklist。记录发现的 pattern 号码 + 源码行号。
2. **Reviewer Engineer** 在 W2 D6-7 对 G1+G3 源码独立运行此 checklist。独立记录,不参考 primary 结果。
3. 完成后**合排 diverge**: §5.9 规定“识别 divergence → signals orthogonal findings”。

---

## 版本历史

- **2026-07-16**: 创建 (Metis A3 要求量化判定标准;每类别至少 2 concrete signals)

---

**相关文档**:
- specs/pdk-service-composition/spec.md §Awkward Pattern Detection Methodology
- tasks.md §5 Awkward Pattern Detection Method
- docs/service-composition/layer3-memo-template.md (待 W2 创建)
