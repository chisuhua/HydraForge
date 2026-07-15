# Layer 3 Memo — Reviewer Engineer

> **作者**: Reviewer Engineer (独立审查视角，第一人称反思)
> **日期**: 2026-07-15
> **模板**: [`layer3-memo-template.md`](./layer3-memo-template.md)

---

## §1: What I Expected to See

我打开代码前预期看到的是一个**微内核风格的 service mesh** — agent 通过共同的 registry 发现彼此，用类型安全的 `ServiceHandle<T>` 交互。我以为 Error Propagation 会像 Rust 的 `Result<T, E>` 那样，每一层都能判断"这个错误该重试吗？该跳过吗？该终止吗？"

关于 LLM callback，我预期它是一个**标准化的 PDK 接口** — 就像 `IToolRegistry` 一样，`ILLMCallback` 或 `IMockLLM` 应该已经在 PDK head 里定义好了，G1 和 G3 各自实现。

关于线程安全，我预期**每个 agent plugin 有明确的线程安全声明** — "G3 uses internal mutex for all shared state，thread-safe for concurrent calls" 或 "G3 is NOT thread-safe — single worker only"。

## §2: What I Actually Saw (Anomalies, Surprises)

**最大的震惊**: `handle_knowledge_base_query()` 调用 `g3_internal_llm()` **没有 try-catch** (`g3_query.cpp:103`)。如果 LLM callback 抛异常（比如 `std::bad_function_call` 因为 callback 的生命周期结束），异常会穿透 `call_tool` → `ToolCoordinator::execute` → 调用者进程终止。这不是优雅的 error return — 这是进程级别的崩溃。

另一个让我不舒服的发现: **G1 的 registry 指针在 mutex 保护下写，但不在同一 mutex 下读**。`register_g1_tools()` (`g1_agent.cpp:109-110`) 在 `state.mtx` 锁内写 `state.registry`，但 `step1_invoke_g3()` (`g1_agent.cpp:48`) 直接读 `registry` 不拿锁。如果注册和调用发生在不同线程（比如注册发生在主线程、调用发生在 worker 线程），这就是经典的 data race。

**callback 类型不支持 error return** (`g3_query.cpp:34`) — 类型是 `string → string`，如果 LLM 调用失败（timeout/rate limited/network error），没有办法传回错误。这是 Defect #6 的深层根因 — 不是因为某个代码路径写错了，而是**类型系统不表达 failure mode**。

**G3 内部 LLM 在没有 queued response 时返回 magic string** — 这是一个隐式的成功路径："G3: no response queued" 被当作有效 answer 继续传递、被 G1 的 synthesis 处理、被调用者当作知识库的合法输出。

## §3: What Felt Wrong (Tacit Patterns)

1. **我感觉代码在"假装类型安全"。** C++ 给了我们强类型系统，但 Agent 间的交互全部退化为 `unordered_map<string,string>` → `json` → `string` 的转换链。如果中间某层改了 field name，编译器不知道，reviewer 不知道，只有运行时才知道。

2. **错误处理是事后附加的。** 各层的 error handling 像是后来塞进去的 — G3 有 basic error schema，G1 有 basic error wrapper — 但没有系统化的思考说"在 G1→G3→LLM 的三层栈中，error 应该怎么分类、怎么传播、怎么恢复？"

3. **plugin 之间的 symmetry 感觉不对。** G1 和 G3 各自注册一个 tool，但实际上 G1 是 orchestrator（调 G3 + LLM），G3 是 compute service（检索 + LLM）。这个非对称性没有被代码结构反映出来 — 它们看起来是一样的"注册一个 tool 的 plugin"。

4. **测试和产品代码的边界模糊。** `g3_internal_llm()` 既为测试提供 mock-queue 模式，又为产品提供真实的 callback 路由 — 一个函数承担了 test harness + production logic 的双重职责。

## §4: Hypotheses for Root Cause

**假说 H1**: 缺失的"**agent 角色分类**" — platform 不区分 orchestrator vs compute service vs sidecar，所有 agent 都用相同的 `register_tool_function()` API。根源是 Spike 定位太窄 — v1 只验证"in-process composition works"，不验证"正确的 concern separation"。

**假说 H2**: **类型退化的根本原因是 call_tool 的签名为 string→json** — 这是最通用的接口也是最低保真度的接口。如果 agent 之间的交互需要类型安全，体系需要在 register_tool 时声明 "this handler expects TypeA and returns TypeB"。

**假说 H3**: **MockLLMProvider 的 dual-use 导致了代偿行为** — 早期 Phase 0 设计 MockLLMProvider 作为轻量测试 stub，但 Phase 5/6 的实际 composition 需求要它做 production 替身。技术债务累积速度 > 升级速度。

## §5: Recommended v2 Changes

1. **引入 AgentRole 分类** — Orchestrator / Compute / Sidecar 三种角色，不同角色有不同的安全约束和 lifecycle 管理
2. **call_tool 签名升级** — 至少支持可选类型标签 `call_tool<T_in, T_out>(name, args)`，让 template 在编译时验证 args 结构
3. **统一 LLM callback 接口** — 创建 `ILLMCallback` 纯虚接口，替代裸 `std::function<string(string)>`，支持 `generate()`, `generate_with_retry()`, `health_check()`
4. **强制 handler 内的 try-catch** — 在 call_tool 层自动捕获 C++ 异常并转换为 error json，不需要每个 handler 重复写
5. **分离 mock infrastructure 从 product code** — `g3_internal_llm()` 的 queue 模式应移到 test-only compilation unit，product 版本不接受 queued responses