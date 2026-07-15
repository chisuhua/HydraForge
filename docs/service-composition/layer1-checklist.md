# Layer 1: Static Code Review Checklist — Awkward Pattern Detection

> **用途**: 在 G1+G3 composition 源码上运行 5 类审查，每类别 3-5 个 concrete 代码位置。
> **关联**: ADR-0051 Phase 6 PDK Composition Spike, tasks.md §5.1
> **最后修改**: 2026-07-15
> **审查对象**: `pdk/g1_coding_assistant/src/g1_agent.cpp` + `pdk/g3_knowledge_base/src/g3_query.cpp`

---

## 类别 1: Contract Drift (合约漂移)

**关注点**: 调用方与被调用方对 args/return schema 的解释不一致。

| # | 检查项 | 代码位置 | 严重度 |
|---|--------|----------|--------|
| 1.1 | **Args 签名隐式合约** — G1 给 G3 传 `unordered_map<string,string>`，但未形式化 schema（无 header 文件或 IDL 声明）。G3 未来新增 required arg 时 G1 编译期不报错 | `g1_agent.cpp:56-57` (call_tool args 构建) vs `g3_query.cpp:93-108` (handler 签名) | 🟠 HIGH |
| 1.2 | **Return schema 口头约定** — G1 在 `g1_agent.cpp:99` 读 `g3_resp.value("success", false)` 假设 G3 返回 `{"success": bool, "answer": string}`，此合约仅在 README.md 描述，无类型级保证 | `g1_agent.cpp:99-101` (读 G3 返回值) | 🟠 HIGH |
| 1.3 | **错误信息丢失** — G1 在 `g1_agent.cpp:101` 吞掉 G3 原始错误返回 `"G1: G3 query failed"`，丢失 G3 的 `error` 字段内容，导致排障时无法区分"G3 内部错误"vs"G3 不可达" | `g1_agent.cpp:100-101` (`step1_invoke_g3` 错误处理) | 🔴 HIGH |
| 1.4 | **Session ID 命名空间无契约** — G1 用 `"g1_default_session"` 作默认值 (`g1_agent.cpp:97`)，G3 无默认机制。不同 Agent 的 session_id 可能碰撞 | `g1_agent.cpp:96-97` vs `g3_state.h:42-44` | 🟡 MEDIUM |
| 1.5 | **Json 字段命名约定未文档化** — G3 用 `"answer"` 字段 (`g3_query.cpp:107`)，G1 用 `"answer"` 字段读取 (`g1_agent.cpp:103`)，但无枚举/常量定义保证一致性 | `g3_query.cpp:107` + `g1_agent.cpp:103` | 🟡 MEDIUM |

---

## 类别 2: Lifecycle Coupling (生命周期耦合)

**关注点**: Agent 之间对初始化/销毁顺序的隐含假设。

| # | 检查项 | 代码位置 | 严重度 |
|---|--------|----------|--------|
| 2.1 | **Raw pointer dangling risk** — G1 在 `g1_state.h:30` 存储 `IToolRegistry* registry = nullptr` (非拥有指针)。PluginLoader 先析构时 G1 handler 访问 dangling pointer → UB | `g1_state.h:30` (registry 指针) | 🔴 HIGH |
| 2.2 | **跨编译单元静态单例初始化顺序** — G3 用 `static SessionStore& g3_sessions()` (`g3_query.cpp:82-85`), G1 用 `static G1State s` (`g1_agent.cpp:30-31`)，C++ 跨编译单元静态初始化顺序未定义 | `g3_query.cpp:82-85` + `g1_agent.cpp:29-32` | 🟠 HIGH |
| 2.3 | **LLM callback 生命周期无 RAII** — G1 的 `llm_callback` (`g1_state.h:34`) 和 G3 的 `g_llm_cb` (`g3_query.cpp:38`) 均为裸 `std::function`，如果 callback 捕获了已析构的 lambda 引用 → UB | `g1_state.h:34` + `g3_query.cpp:38` | 🔴 HIGH |
| 2.4 | **SessionStore 增长无界** — `SessionStore::get_or_create()` (`g3_state.h:42-44`) 无限插入新条目，无 TTL/eviction 策略。多 Agent 共享时导致内存泄漏 | `g3_state.h:42-44` (get_or_create) | 🟡 MEDIUM |

---

## 类别 3: Error Propagation Gaps (错误传播缺口)

**关注点**: 错误在服务链中如何传递、转换或吞没。

| # | 检查项 | 代码位置 | 严重度 |
|---|--------|----------|--------|
| 3.1 | **G3 错误被 G1 扁平化吞没** — `g1_agent.cpp:101` 返回 `"G1: G3 query failed"` 替代 G3 原始 error，调用者完全不知道 G3 内部的真实错误原因 | `g1_agent.cpp:100-101` | 🔴 HIGH |
| 3.2 | **LLM 空响应伪装为有效结果** — `g3_internal_llm()` 无 queued response 时返回 `"G3: no response queued"` (`g3_query.cpp:75`)，此字符串进入 answer 字段被 G1 当作合法 LLM 输出。Defect #6 精确场景 | `g3_query.cpp:70-75` | 🔴 HIGH |
| 3.3 | **Tool not found vs tool execution failed 无法区分** — G1 用 `has_tool()` 检查存在性 (`g1_agent.cpp:52`)，但如果 tool 存在但 handler 返回 error，G1 无区分逻辑 | `g1_agent.cpp:52-54` (has_tool 检查) | 🟠 HIGH |
| 3.4 | **call_tool 返回值不传递 error_code** — G3 handler 返回 `{"success": false, "error": "..."}` 但无 error_code 枚举，G1 无法根据错误类型决定重试/跳过/终止 | `g3_query.cpp:97,105` (error 字段) | 🟡 MEDIUM |
| 3.5 | **≤30 行限制挤压错误分类** — `handle_knowledge_base_query()` 只有 16 行 (`g3_query.cpp:93-108`)，无空间添加细粒度错误码映射 | `g3_query.cpp:93-108` | 🟡 MEDIUM |

---

## 类别 4: Resource Lifetime Risks (资源生命周期风险)

**关注点**: MockLLMProvider、shared state 的线程安全与所有权。

| # | 检查项 | 代码位置 | 严重度 |
|---|--------|----------|--------|
| 4.1 | **MockLLMProvider 单线程约定被多线程打破** — G3 README 声明 MockLLMProvider 是 "single-threaded test stub"，但 `g3_internal_llm()` 可能在 DomainWorkerPool (Sprint 3, std::jthread) 的多线程环境中被并发调用 | `g3_query.cpp:65-76` (g3_internal_llm) | 🔴 HIGH |
| 4.2 | **Registry 指针读写不在同一锁下** — G1 的 `register_g1_tools()` 写入 `state.registry` (`g1_agent.cpp:110`)，但 `step1_invoke_g3()` 读取 `registry` (`g1_agent.cpp:48`) 不在同一 mutex 下 → data race | `g1_agent.cpp:48` (读) vs `g1_agent.cpp:110` (写) | 🔴 HIGH |
| 4.3 | **SessionStore 写锁期间 rehash 风险** — `get_or_create()` 持写锁插入新 key (`g3_state.h:44`) 可能触发 `unordered_map::rehash()`，使其他线程持有的引用/迭代器失效 | `g3_state.h:42-44` | 🟠 HIGH |
| 4.4 | **g_queued_responses 向量 erase 效率** — `g3_internal_llm()` 在 `g3_query.cpp:72` 执行 `g_queued_responses.erase(begin())` 是 O(n) — 无性能上限保证 | `g3_query.cpp:70-72` | 🟡 MEDIUM |

---

## 类别 5: Reusability Barriers (可复用性障碍)

**关注点**: 阻碍 G3 被 G2/G4/G5 等新 Agent 复用的因素。

| # | 检查项 | 代码位置 | 严重度 |
|---|--------|----------|--------|
| 5.1 | **硬编码 tool name** — G1 硬编码字符串 `"knowledge_base/query"` (`g1_agent.cpp:52`)，若 G3 改名或加版本号 G1 需重新编译 | `g1_agent.cpp:52` | 🟡 MEDIUM |
| 5.2 | **无 tool discovery 机制** — G1 通过 `has_tool("knowledge_base/query")` 单一检查 (`g1_agent.cpp:52`)，若 G3 注册多个 tool G1 无能力发现 | `g1_agent.cpp:52` | 🟡 MEDIUM |
| 5.3 | **无版本协商** — G3 注册为 v0.1.0 (`g3_entry.cpp:38`) 但 G1 无版本检查逻辑，API break 时静默行为变化 | `g3_entry.cpp:38` (semver) | 🟡 MEDIUM |
| 5.4 | **LLM callback 模式重复无复用** — G1 和 G3 的 `set_llm_callback()` 模式完全一致 (`g1_agent.cpp:38-43` vs `g3_query.cpp:43-46`)，但各自独立实现无共享 trait | `g1_agent.cpp:38-43` + `g3_query.cpp:43-46` | 🟡 MEDIUM |
| 5.5 | **无 find_package/pkg-config 支持** — G3 的 `.so` 路径硬编码在测试中 (`test_g3_knowledge_base.cpp:86-92`)，新 Agent 需手动指定路径 | `test_g3_knowledge_base.cpp:86-92` | 🟢 LOW |
| 5.6 | **Tool manifest 为单元素硬编码** — G1 的 `tool_manifest` 在注册时 push 一次 (`g1_agent.cpp:113`)，不支持多依赖声明 | `g1_agent.cpp:111-114` | 🟢 LOW |

---

## 审查汇总

| 类别 | 严重度 | 发现数 | 关键风险 |
|------|--------|--------|----------|
| 1. Contract Drift | 🟠 HIGH | 5 | 隐式合约无编译时检查 |
| 2. Lifecycle Coupling | 🔴 HIGH | 4 | Raw pointer + 静态初始化顺序 UB |
| 3. Error Propagation Gaps | 🔴 HIGH | 5 | 静默错误传播 (Defect #6) |
| 4. Resource Lifetime Risks | 🔴 HIGH | 4 | 多线程 data race + rehash |
| 5. Reusability Barriers | 🟡 MEDIUM | 6 | 扩展到 5 Agent 时阻塞 |
| **总计** | | **24** | **3 🔴 / 2 🟠 / 1 🟡** |

---

## 版本历史

- **2026-07-15**: 基于 G1+G3 实际源码重新编写，替换预生成的通用类别为 concrete 代码引用