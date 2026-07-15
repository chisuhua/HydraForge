# Layer 1 Review — Primary Engineer Findings

> **审查人**: Primary Engineer (实施者视角)
> **审查日期**: 2026-07-15
> **审查范围**: `pdk/g1_coding_assistant/src/g1_agent.cpp` + `pdk/g3_knowledge_base/src/g3_query.cpp`
> **依据**: [`layer1-checklist.md`](./layer1-checklist.md)

---

## Findings by Category

### 类别 1: Contract Drift

| # | 发现 | 代码位置 | 严重度 |
|---|------|----------|--------|
| F-1.1 | **G1/G3 合约只有口头约定。** G3 handler 返回 `{"success": bool, "answer": string}` 的约定写在一个单独的 README.md 中，不是 shared header。如果 G3 未来在 answer 旁边加了 `"confidence": 0.95`，G1 的代码不报错只是忽略 | `g3_query.cpp:107` (return schema) | 🟠 HIGH |
| F-1.2 | **错误被包装成通用字符串。** G1 在 `step1_invoke_g3()` 返回 `{"success": false, "error": "G1: G3 query failed"}` — G3 的原始 `error` 字段完全丢失。如果你搜日志只能看到 "G1: G3 query failed" 猜不到 G3 说 "Missing required args" | `g1_agent.cpp:100-101` | 🔴 HIGH |
| F-1.3 | **call_tool 调用的 args 没有 schema 约束。** G1 构建 `{"question", "session_id"}` — 如果 question 为空字符串或 session_id 含特殊字符，G3 不会验证就直接传给 LLM | `g1_agent.cpp:56-57` args 构建 | 🟡 MEDIUM |

### 类别 2: Lifecycle Coupling

| # | 发现 | 代码位置 | 严重度 |
|---|------|----------|--------|
| F-2.1 | **IToolRegistry* 是裸指针，由外部拥有。** `g1_state.h:30` 的 `registry` 指针在 `register_g1_tools()` 里设置，在 `step1_invoke_g3()` 里用。中间的 gap 里有无数杀死指针的机会 | `g1_state.h:30` (registry) | 🔴 HIGH |
| F-2.2 | **LLM callback 的 std::function 捕获没有生命周期保证。** 测试里传 lambda 没问题，但如果生产代码中 callback 捕获了 `this` 或者临时引用，实现期不报错只 UB | `g1_state.h:34` (function) + `g3_query.cpp:38` (g_llm_cb) | 🔴 HIGH |
| F-2.3 | **SessionStore 无限增长没有 eviction。** 开发测试没问题，但生产环境 W2 跑 10000 session，每个 session 存几轮 Q/A，内存会悄悄吃掉 | `g3_state.h:42-44` (get_or_create) | 🟡 MEDIUM |

### 类别 3: Error Propagation

| # | 发现 | 代码位置 | 严重度 |
|---|------|----------|--------|
| F-3.1 | **"G3: no response queued" 不是 error 但也不是正确 answer。** `g3_internal_llm()` 在无 queued response 时返回 magic string (`g3_query.cpp:75`) — 这个 string 进入 answer 字段后被当成 LLM 输出，可能被 G1 的 synthesis 引燃 | `g3_query.cpp:75` (fallback) | 🔴 HIGH |
| F-3.2 | **G3 不返回 error_code。** `{"success": false, "error": "..."}` 给了人类可读的错误但没给机器可判的枚举。G1 无法程序化决定是否重试 | `g3_query.cpp:97,105` (error schema) | 🟠 HIGH |
| F-3.3 | **如果 call_tool 返回 null/empty json，G1 会出什么？不确定。** `step1_invoke_g3()` 第 100 行读 `value("success", false)` — 如果 registry 返回空 json，默认 false → 触发 error → 也没关系（至少不 crash），但这路径没测试 | `g1_agent.cpp:99-100` (no null check) | 🟡 MEDIUM |

### 类别 4: Resource Lifetime

| # | 发现 | 代码位置 | 严重度 |
|---|------|----------|--------|
| F-4.1 | **G3 的 static 全局变量在多 worker 线程下不安全。** `g_cb_mutex` 只保护 `g_llm_cb`/`g_call_prompts`/`g_queued_responses` — 但如果 MockLLMProvider 自身是 single-threaded (README 声明)，多 worker 同时调 `g3_internal_llm()` 会撞 | `g3_query.cpp:37-41` (global state) | 🔴 HIGH |
| F-4.2 | **Registry 指针的读/写不在同一 mutex。** `register_g1_tools()` 在 `mtx` 锁内写 `registry`，但 `step1_invoke_g3()` 读 `registry` 不拿锁 — 竞态条件 | `g1_agent.cpp:48` vs `g1_agent.cpp:109-110` | 🔴 HIGH |
| F-4.3 | **g_queued_responses.erase(begin()) 是线性时间。** 如果 queued responses 比较长（比如测试里预装了 1000 条），每次 `g3_internal_llm()` 擦除都是 O(n) | `g3_query.cpp:70-72` (erase) | 🟢 LOW |

### 类别 5: Reusability

| # | 发现 | 代码位置 | 严重度 |
|---|------|----------|--------|
| F-5.1 | **G1 硬编码 `"knowledge_base/query"` 字符串。** 如果 G3 未来注册为 `"knowledge_base/v1/query"` 或 `"kb/search"`，G1 的 `has_tool()` 检查直接失败，但编译期无报 | `g1_agent.cpp:52` (hardcoded) | 🟡 MEDIUM |
| F-5.2 | **G1 和 G3 的 set_llm_callback 模式完全拷贝。** 两 plugin 实现了相同功能但零共享代码。加 G4/G5 会复制粘贴 → 维护噩梦 | `g1_agent.cpp:38-43` + `g3_query.cpp:43-46` | 🟡 MEDIUM |
| F-5.3 | **Tool manifest 只支持单 tool。** G1 的 `g1_tool_manifest()` 返回 `knowledge_base/query` 一个 string — 如果未来 G1 需要调用 G3 + G4，manifest 语义失效 | `g1_agent.cpp:111-114` (manifest) | 🟢 LOW |

---

## 汇总

| 类别 | 🔴 | 🟠 | 🟡 | 🟢 | 总计 |
|------|-----|-----|-----|-----|------|
| 1. Contract Drift | 1 | 1 | 1 | 0 | **3** |
| 2. Lifecycle Coupling | 2 | 0 | 1 | 0 | **3** |
| 3. Error Propagation | 1 | 1 | 1 | 0 | **3** |
| 4. Resource Lifetime | 2 | 0 | 0 | 1 | **3** |
| 5. Reusability | 0 | 0 | 2 | 1 | **3** |
| **总计** | **6** | **2** | **5** | **2** | **15** |

**最严重的 pattern**: 类别 3.1 (LLM fallback string 伪装为有效 answer) — 这是 Defect #6 的直接体现，错误被静默传递。
**最危险的 pattern**: 类别 4.1 — MockLLMProvider 单线程约定在多 worker 下违反，生产环境必崩。
**最急迫的 pattern**: 类别 2.1 — 裸指针 dangling 是 UB，代码评审 zero tolerance。