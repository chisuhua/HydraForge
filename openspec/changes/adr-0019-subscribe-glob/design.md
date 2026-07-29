## Context

`IInteractionBus::subscribe()` 当前接受 `event_type` 字符串，语义为精确匹配。Oracle 评审 (2026-07-26) 要求扩展 `subscribe()` 支持 glob pattern，避免新增 `subscribe_topic()` API。`InMemoryBus` Change B（2026-07-26）已实现双路径分发架构，但缺少正式设计文档。本 Change 完成设计文档化与架构合规性审计。

依赖 `change:adr-0002-busevent-contract` — `BusEvent.topic` 是 glob 匹配的目标字段，`BusEvent` 5 字段契约（topic, payload, timestamp, causal_time, priority）已定义。

现有状态：
- `IInteractionBus::subscribe(const std::string&, callback)` 签名不变，参数语义从精确扩展为 glob pattern
- `InMemoryBus` 内部已实现 `exact_subscribers_` + `wildcard_subscribers_` 双路径映射
- `glob_match()` 匿名函数支持 `*` 和 `?` 两种通配符
- `has_wildcard()` 函数在 `subscribe()` 时路由到精确或通配符映射
- `test_interaction_bus_glob.cpp` 已包含 6 个测试用例（精确匹配 / 单通配符 / 多通配符 / 无匹配 / unsubscribe / 并发竞争）

## Goals / Non-Goals

**Goals:**
- 正式化 `subscribe()` 的 glob pattern 语义作为 `IInteractionBus` 契约的一部分
- 文档化 `InMemoryBus` 双路径分发架构设计
- 文档化 `glob_match()` 算法规范
- 确保 `test_interaction_bus_glob.cpp` 6 个测试覆盖全部关键场景
- 确保并发场景（subscribe/unsubscribe 期间 dispatch）无数据竞争

**Non-Goals:**
- 不修改 `subscribe()` 签名（保持 `size_t subscribe(const std::string& pattern, std::function<void(const BusEvent&)> callback)`）
- 不修改 `IInteractionBus` 抽象接口（仅语义扩展）
- 不支持正则表达式（仅 glob：`*` 任意序列 + `?` 单字符）
- 不支持优先级路由（priority 字段预留，Phase 2 使用）
- 不引入 lock-free 队列（现有 mutex+queue 满足性能需求）

## Decisions

### Decision 1: 扩展 subscribe() 而非新增 subscribe_topic()
- **选择**: 单个 `subscribe()` 方法接受 `pattern` 参数，无通配符时 O(1) 精确匹配，含通配符时 O(w) 线性扫描
- **理由**: Oracle 评审明确要求避免 API 膨胀；`has_wildcard()` 函数在注册时路由到精确/通配符映射，发射时并行扫描两个映射
- **替代方案**: 新增 `subscribe_topic(topic, cb)` + `subscribe_pattern(pattern, cb)` — 拒绝，API 膨胀增加下游使用复杂度

### Decision 2: glob 语法
- **选择**: 仅支持 `*`（匹配任意字符序列，包括空序列）和 `?`（匹配单个字符）。不支持 `**`、`[...]`、`{a,b}` 等扩展语法
- **理由**: PDK 事件命名约定为 `domain.subdomain.action` 三级结构，`*` 和 `?` 足以覆盖所有使用场景（`tool.*`、`inference.*`、`*.error.*`）；简单语法避免引入 `fnmatch` 或 regex 库依赖
- **替代方案**: 支持完整 glob 语法 (`fnmatch`) — 拒绝，过度设计，当前场景无需求

### Decision 3: 双路径分发架构
- **选择**: `exact_subscribers_` 使用 `unordered_map<string, vector<handler>>`（O(1) 查找），`wildcard_subscribers_` 使用 `unordered_map<string, vector<handler>>` + 线性扫描（O(w)，w < 50）
- **理由**: 精确匹配是常见路径（PDK 事件大部分为固定 topic），O(1) 保证零开销；通配符订阅者数量 < 50，线性扫描可接受
- **替代方案**: 统一 `unordered_multimap<string, handler>` + 每次遍历 — 拒绝，精确匹配路径退化到 O(n) 不可接受

### Decision 4: 并发安全策略
- **选择**: 统一 `mtx_` 保护 `exact_subscribers_` 和 `wildcard_subscribers_` 两个映射；dispatch 在锁内收集 callback 副本，锁外执行回调
- **理由**: 避免死锁（ADR-0030 V2 原则 — callback 在锁外调用）；统一锁比两把锁更简单，竞争窗口更小；`subscribe()` / `unsubscribe()` 是低频操作，不构成性能瓶颈
- **替代方案**: 读写锁 (`shared_mutex`) — 拒绝，`subscribe()`/`unsubscribe()` 与 `dispatch_loop()` 竞争锁时间极短，RWLock 的额外开销不划算

### Decision 5: glob_match 算法
- **选择**: 简单双指针状态机，支持 `*`（回溯匹配）和 `?`（单字符匹配），无递归
- **理由**: 迭代算法 O(p*t) 最坏时间复杂度，但 p 和 t 均 < 200 字符，实际性能可忽略；无递归避免栈溢出风险
- **替代方案**: 递归回溯 — 拒绝，深度嵌套的 `*` 可能导致栈溢出；正则表达式库 — 拒绝，外部依赖

## Risks / Trade-offs

- `unsubscribe()` 线性扫描两个映射 — O(n) 总订阅者数，但 unsubscribe 是低频操作，不影响 hot path
- 通配符订阅者数量假设 < 50 — 当前无硬性限制，若超过 50 需考虑 Trie 或 Segment Tree 优化
- glob_match 使用 `std::string::npos` 哨兵 — 若 pattern 只有 `*` 且 topic 极长，`star_t` 递增可能导致 topic 遍历完所有字符，但 topic 长度 < 200 字符，可忽略
- 并发测试中 `received.load() >= 1000` 而非 `== 1000` — 因为 `unsubscribe` 可能在 callback 执行后、计数到 1000 前生效，导致某些事件丢失计数；宽松断言避免假阴性