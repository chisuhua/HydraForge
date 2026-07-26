## 接口扩展 (3h)

### IInteractionBus

- [ ] 1.1 `include/agenticdsl/contract/iinteraction_bus.h` — 新增 2 纯虚方法
  - `virtual size_t subscribe_topic(const std::string& pattern, TopicCallback cb) = 0;`
  - `virtual void unsubscribe(size_t token) = 0;`
- [ ] 1.2 类型别名: `using TopicCallback = std::function<void(const std::string&, const nlohmann::json&)>;`
- [ ] 1.3 文档注释: glob pattern 语义说明 + 返回值含义

### InMemoryBus 实现

- [ ] 2.1 `inmemory_bus.h` — 新增 TopicSubscription 结构 + topic_subscribers_ map + next_token_ atomic
- [ ] 2.2 `inmemory_bus.cpp` — 实现 `subscribe_topic()` / `unsubscribe()`
- [ ] 2.3 `dispatch_loop()` 扩展: session dispatch 后追加 topic dispatch（glob_match 过滤）
- [ ] 2.4 `glob_match()` helper: 支持 `*` 和 `?` wildcard

### 测试

- [ ] 3.1 新建 `tests/test_interaction_bus_topic.cpp` — ≥3 test cases
  - 精确匹配: subscribe_topic("inference.lifecycle.idle", cb) → emit → cb called
  - Glob 匹配: subscribe_topic("inference.*", cb) → emit "inference.lifecycle.idle" → cb called
  - 退订: unsubscribe(token) → emit → cb NOT called
  - 无匹配: emit "other.topic" → cb NOT called
  - 多 subscriber: 2 topic subscribers + 1 session subscriber → 全部收到

### 验收

- [ ] 4.1 `ctest -R test_interaction_bus_topic` 全绿
- [ ] 4.2 `ctest -R test_interaction_bus` 全绿（零回归）
- [ ] 4.3 `ctest` 全量零回归