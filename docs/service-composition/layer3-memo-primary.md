# Layer 3 Memo — Primary Engineer

> **作者**: Primary Engineer (G1+G3 实施者，第一人称反思)
> **日期**: 2026-07-15
> **模板**: [`layer3-memo-template.md`](./layer3-memo-template.md)

---

## §1: What I Expected to See

我预期 G1 和 G3 之间有一个**显式的服务发现机制** — 像 `find_package` 或某种 registry query — G1 向平台问"谁有 `knowledge_base/query`？"，平台返回一个 handle。我觉得版协商 (version negotiation) 应该是内置的 — G1 说"我需要 G3 >= v0.1.0"，如果版本不对编译就失败了。

我预期错误会像 exception stack trace 一样**逐层传递** — G3 内部出错 → G1 看到原始错误 → 调用者能 trace back 到根因。而不是在中间被包装成通用消息。

对于 MockLLMProvider 的线程安全，我预期有一个明确的声明"这个是单线程 safe，多线程需要 MOCK_PROVIDER_LOCK 或使用 MockLLMProviderPool"。

## §2: What I Actually Saw (Anomalies, Surprises)

最让我惊讶的是 **MockLLMProvider 在多线程环境下的行为不明确**。G3 的 README 声明 "single-threaded test stub"，但实际代码的 `g3_internal_llm()` 没有做线程安全保证 — 如果 DomainWorkerPool 的多个 jthread worker 并发调用 G3 工具，`g_llm_cb` 和 `g_queued_responses` 在同一个 mutex 下勉强安全，但 MockLLMProvider 本身如果是 stateful，就完蛋了。

另一个惊喜是 **魔术字符串 `"G3: no response queued"`** (`g3_query.cpp:75`)。这不是 exception，不是 error return — 就是一个普通 string，混入 answer 流。如果在生产环境 LLM 回调失效，用户会收到"G3: no response queued"作为知识库的答案，然后继续执行下一个 step。

**错误传播**让我感觉不干净。G1 调用 G3 失败时我写了 `"G1: G3 query failed"` (`g1_agent.cpp:101`) — 这解决了调用者的错误处理需求（知道"某个步骤失败了"），但抹去了 G3 的原始错误信息。我觉得中间层应该在错误消息中保留两者的信息："G1: G3 query failed — original: {g3_error}"。

## §3: What Felt Wrong (Tacit Patterns)

1. **我在 copy-paste LLM callback 模式。** G1 的 `g1_set_llm_callback()` 和 G3 的 `g3_set_llm_callback()` 代码几乎一模一样，但我在两个文件中各自写了一遍。如果 G4 需要 LLM callback，pattern 会变成 3 份 — 这个不对。

2. **Hardcoded tool name 让我不安。** G1 的 `has_tool("knowledge_base/query")` 和 `call_tool("knowledge_base/query", ...)` 都是字符串匹配。我写的时候就在想"如果 G3 改名叫 `"kb/query"`，这个 break 了只在运行时发现。"

3. **30 行的 handler 约束让我没法做好错误处理。** `handle_knowledge_base_query()` 只有 16 行 — 这个数字不错，但我没有空间加 try-catch 或 error_code 映射。错误被压缩成两行 `if (answer.empty()) return error`。

4. **我感觉在一个没有 type system 的环境中写微服务** — G1 和 G3 之间的合约完全靠 comment 和 README.md 维护，没有任何编译时检查或 schema valid 机制。

## §4: Hypotheses for Root Cause

**假说 H1**: 根本原因是**缺失服务契约层**。如果有一个 shared header 定义 G1→G3 的接口（args schema / return schema / error enums），这些问题会减少 80%。

**假说 H2**: MockLLMProvider 的问题源于**技术债务** — MockLLMProvider 是 Phase 0 时为单线程测试设计的，被 Phase 6 的 multi-agent composition 复用但未升级线程模型。

**假说 H3**: copy-paste pattern 的原因是**PDK 缺少"shared utility"的概念** — plugin 之间不能 import 彼此的代码（.so 隔离），但可以有一个 shared header-only library 提供 callback 模式、error 类型等。

## §5: Recommended v2 Changes

1. **引入 service contract header** — 每个 agent plugin 导出一个 `g3_contract.h` 文件，caller 通过 `#include` 获取编译时保证的工具名、args schema、return schema
2. **升级 MockLLMProvider 为线程安全** — 至少加一个 `MockLLMProviderThreadSafe` 变体或文档化单线程约束
3. **共享 PDK utility header** — 把 `set_llm_callback()` / `reset_mock()` / `enqueue_response()` 模式提取到 `agenticdsl/pdk/shared/llm_mock.h`
4. **错误传播保留原始上下文** — G1 在包装 G3 错误时追加原始 `error` 字段，如 `"error": "G1: G3 query failed (original: " + g3_error + ")"`
5. **tool name 常量化** — G1 不直接写 `"knowledge_base/query"` 字符串，而是通过 G3 导出的常量如 `g3::TOOL_NAME_QUERY`