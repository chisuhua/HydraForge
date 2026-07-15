# Layer 1 Review — Reviewer Independent Findings

> **审查人**: Reviewer Engineer (独立审查视角，未参考 Primary 结果)
> **审查日期**: 2026-07-15
> **审查范围**: `pdk/g1_coding_assistant/src/g1_agent.cpp` + `pdk/g3_knowledge_base/src/g3_query.cpp`
> **依据**: [`layer1-checklist.md`](./layer1-checklist.md)

---

## Findings by Category

### 类别 1: Contract Drift

| # | 发现 | 代码位置 | 严重度 |
|---|------|----------|--------|
| F-1.1 | **G3 的两个 required args 在 handler 内部验证 (`g3_query.cpp:94-97`)，但 G1 没有 validation 逻辑** — 如果 G1 传了第三个参数（例如 `"temperature": "0.7"`），G3 忽略不报错 → 静默行为变化 | `g3_query.cpp:93-98` (arg 提取) | 🟠 HIGH |
| F-1.2 | **G1 的 handle_coding_assistant_review 有 `request` 和 `code` 两个 required args，但没有文档化的 args schema** — caller 只能通过读源码知道需要什么参数，没有 IDL 或 manifest 可自动发现 | `g1_agent.cpp:88-91` (arg validation) | 🟡 MEDIUM |
| F-1.3 | **G3 的 session_id 没有长度/格式 validation** — 任意字符串直接插入 `unordered_map` (`g3_state.h:44`)，可能包含 SQL injection 风格的特殊字符（虽然在 C++ 里无害，但在 future serialization 上下文中危险） | `g3_state.h:42-44` | 🟢 LOW |

### 类别 2: Lifecycle Coupling

| # | 发现 | 代码位置 | 严重度 |
|---|------|----------|--------|
| F-2.1 | **G1 的 register_g1_tools 既注册 tool 又初始化 state** — 单个函数做了两件事：`state.registry = &registry` 和 `registry.register_tool_function(...)`，如果第二步失败第一步的副作用已经生效（虽然在这个简单案例中不致命） | `g1_agent.cpp:106-130` (register_g1_tools) | 🟡 MEDIUM |
| F-2.2 | **PluginLoader 和 dlsym 的析构顺序** — `G3Plugin::~G3Plugin()` 先 `dlclose(handle)` (`test_g3_knowledge_base.cpp:109`)，但在此期间 loader 的 registry 可能仍然引用已经移出 `.so` 的函数指针 → 如果 registry 没被清空，后续调用会 SIGSEGV | `test_g3_knowledge_base.cpp:109` (dlclose) | 🟠 HIGH |
| F-2.3 | **SessionStore 是跨 tool 调用的共享 mutable state** — 不同 worker 线程对同一 session_id 的并发写（`append()`）用写锁保护，但从不同 worker 的视角看，读到 session 历史的时间点不确定 → 一个 worker 可能看到另一个 worker 刚写入但还没完全的 Q/A 对 | `g3_state.h:48-53` (append 写锁) | 🟡 MEDIUM |

### 类别 3: Error Propagation

| # | 发现 | 代码位置 | 严重度 |
|---|------|----------|--------|
| F-3.1 | **G3 handler 没有 try-catch 包围 `g3_internal_llm()` 调用** — 如果内部 LLM 回调抛出异常（比如 `std::bad_function_call` 因为 callback 已析构），异常会穿透 `call_tool` → `ToolCoordinator::execute` → 调用者。`handle_knowledge_base_query` 没有错误恢复路径 | `g3_query.cpp:103` (无 try-catch) | 🔴 HIGH |
| F-3.2 | **G1 的 step2_synthesize 里 LLM callback 可能返回空字符串** — 但在 `g1_agent.cpp:78-80` 里这个情况被处理为合成一个 fallback review 字符串。好的意图，但 fallback review 的字面内容是 `"[G1 Review] <G3 answer> (code reviewed...)"` — 这个字符串混入调用者的结果流，调用者可能误以为 LLM 正常生成了审查 | `g1_agent.cpp:78-81` (fallback review) | 🟠 HIGH |
| F-3.3 | **G1 不检查 G3 返回的 answer 是否为空** — 如果 G3 返回 `{"success": true, "answer": ""}`，`step2_synthesize()` 把这个空字符串传给 LLM synthesis，LLM 可能产生幻觉评论 | `g1_agent.cpp:99-103` (无 answer 空检测) | 🟡 MEDIUM |

### 类别 4: Resource Lifetime

| # | 发现 | 代码位置 | 严重度 |
|---|------|----------|--------|
| F-4.1 | **g_queued_responses 的 erase(begin()) 修改 vector 的所有后续元素的内存地址** — 如果有某个代码路径持有了 `g_queued_responses[0]` 的引用然后调用 `g3_internal_llm()`（虽然当前不存在此路径），引用会迷向 | `g3_query.cpp:70-72` (erase) | 🟡 MEDIUM |
| F-4.2 | **G3 的 g_call_prompts 不设上限** — 每次 `g3_internal_llm()` 调用 push 一个新 prompt，无 ring buffer 或清理机制。W2 100K prompts → 内存过溢 | `g3_query.cpp:69` (push_back) | 🟡 MEDIUM |
| F-4.3 | **SessionStore::append 按值拷贝 Q/A 字符串** — 长对话场景下 `history` vector 拷贝大量字符串数据 → 时间+空间双开销 | `g3_state.h:52` (emplace_back) | 🟢 LOW |

### 类别 5: Reusability

| # | 发现 | 代码位置 | 严重度 |
|---|------|----------|--------|
| F-5.1 | **G3 的 LLM callback 不返回 error 或 status code** — callback 签名是 `string → string`，如果 LLM 调用失败（timeout/rate limited），没有 way 传回错误 → G3 handler 会盲目继续 | `g3_query.cpp:34` (callback type) | 🟠 HIGH |
| F-5.2 | **G3 的知识库硬编码为 5 行 C++ 字符串** (`g3_state.h:26-35`) — 如果 G4 Memory Agent 想使用知识库，它需要重新实现，不能复用 G3 的这些 snippets | `g3_state.h:26-35` (g3_snippets) | 🟡 MEDIUM |
| F-5.3 | **G1 和 G3 各自定义 set_llm_callback 和 reset 函数** — 这些是 plugin 级别的"mini utility library"，各自实现但不共享。如果未来需要统一 LLM callback 管理，需要改变所有 plugin | `g1_agent.cpp:38-43` + `g3_query.cpp:43-46` | 🟡 MEDIUM |
| F-5.4 | **g3_internal_llm 同时做 mock-queue 消费和生产调用记录** — 一个函数有 3 个职责（callback 路由 / queue 消费 / prompt 记录）。如果未来需要在 callback 和 queue 之间插入新行为，需要重构 | `g3_query.cpp:65-76` | 🟢 LOW |

---

## 汇总

| 类别 | 🔴 | 🟠 | 🟡 | 🟢 | 总计 |
|------|-----|-----|-----|-----|------|
| 1. Contract Drift | 0 | 1 | 1 | 1 | **3** |
| 2. Lifecycle Coupling | 0 | 1 | 2 | 0 | **3** |
| 3. Error Propagation | 1 | 1 | 1 | 0 | **3** |
| 4. Resource Lifetime | 0 | 0 | 2 | 1 | **3** |
| 5. Reusability | 0 | 1 | 2 | 1 | **4** |
| **总计** | **1** | **4** | **8** | **3** | **16** |

**Reviewer 特有的关键发现**: 类别 3.1 (handler 无 try-catch 保护 LLM 调用) 不是 Primary 发现的 — 这是一个新的风险向量，因为异常穿透会导致无控制的进程终止。
**Reviewer 特有的关键发现**: 类别 5.1 (callback 类型不支持 error return) 揭示了一种更深层的设计缺陷 — v1 将成功路径和失败路径耦合在同一类型签名中。