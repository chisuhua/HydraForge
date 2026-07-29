## ADDED Requirements

### Requirement: 通配符订阅模式匹配
`IInteractionBus::subscribe()` SHALL 接受 glob pattern 作为参数，无通配符时精确匹配 O(1)，含通配符时 glob 匹配 O(w)。仅支持 `*`（匹配任意字符序列，包括空序列）和 `?`（匹配单个字符）。

#### Scenario: 精确匹配（无通配符）
- **WHEN** subscribe 模式为 `"inference.lifecycle.idle"` 且 emit topic 为 `"inference.lifecycle.idle"`
- **THEN** handler 被触发 1 次
- **WHEN** emit topic 为 `"inference.lifecycle.running"`
- **THEN** handler 不被触发

#### Scenario: 单通配符 *
- **WHEN** subscribe 模式为 `"inference.*"` 且依次 emit `"inference.lifecycle.idle"`、`"inference.lifecycle.running"`、`"inference.lifecycle.error"`
- **THEN** handler 被触发 3 次

#### Scenario: 多通配符 *.error.*
- **WHEN** subscribe 模式为 `"*.error.*"` 且 emit `"inference.error.oom"` 和 `"temporal.error.timeout"`
- **THEN** handler 被触发 2 次
- **WHEN** emit `"inference.timeout.oom"`（无 "error" 段）
- **THEN** handler 不被触发

#### Scenario: 无匹配
- **WHEN** subscribe 模式为 `"other.*"` 且 emit `"inference.lifecycle.idle"`
- **THEN** handler 不被触发

#### Scenario: 通配符 unsubscribe
- **WHEN** subscribe 模式为 `"inference.*"`，收到事件后 unsubscribe，再次 emit `"inference.lifecycle.running"`
- **THEN** handler 不再被触发

#### Scenario: 通配符模式与 ? 单字符匹配
- **WHEN** subscribe 模式为 `"tool.???"` 且 emit `"tool.call"`
- **THEN** handler 被触发 1 次（`???` 匹配 `call` 的 4 字符）
- **WHEN** emit `"tool.ca"`（3 字符，不匹配 4 字符要求）
- **THEN** handler 不被触发

### Requirement: 并发安全
subscribe/unsubscribe 与 dispatch 并发执行时，SHALL 无数据竞争、无死锁、无崩溃。

#### Scenario: 并发 subscribe/unsubscribe 期间 dispatch
- **WHEN** 1 个 producer 线程发射 1000 事件，同时 1 个 toggler 线程执行 100 次 subscribe/unsubscribe 循环
- **THEN** 无崩溃，received 计数 >= 1000（所有事件至少被初始 subscriber 接收一次）

### Requirement: glob_match 算法正确性
`glob_match(pattern, topic)` 函数 SHALL 正确实现 `*` 回溯匹配和 `?` 单字符匹配，无递归，无外部依赖。

#### Scenario: 边界情况
- **WHEN** pattern 为 `""`，topic 为 `""` — THEN 匹配
- **WHEN** pattern 为 `"*"`，topic 为任意字符串 — THEN 匹配
- **WHEN** pattern 为 `"*"`，topic 为 `""` — THEN 匹配（`*` 匹配空序列）
- **WHEN** pattern 为 `"a"`，topic 为 `"b"` — THEN 不匹配
- **WHEN** pattern 为 `"a"`，topic 为 `"a"` — THEN 匹配