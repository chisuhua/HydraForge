## Change B: 扩展 subscribe 支持 glob (~3h)

### 接口与存储

- [ ] 1.1 `inmemory_bus.h` — 新增 `wildcard_subscribers_` map
  - `std::unordered_map<string, vector<pair<size_t, Callback>>> wildcard_subscribers_;`
- [ ] 1.2 `subscribe()` 路由逻辑：
  - 检测 `*` 或 `?` → wildcard_subscribers_
  - 无通配符 → exact_subscribers_（现有逻辑）
- [ ] 1.3 `unsubscribe()` 同时处理两个 map

### glob_match 实现

- [ ] 2.1 `inmemory_bus.cpp` — 新增 `static glob_match(pattern, topic)`
  - 支持 `*` 任意字符匹配
  - 支持 `?` 单字符匹配
  - 单元测试先行

### 分发逻辑

- [ ] 3.1 `dispatch_loop()` — 精确 match 后追加 wildcard 遍历
  - 性能注释：wildcard 数量 <50 时 O(n*m) 可接受
- [ ] 3.2 `emit()` 不变（标准 BusEvent 入队路径）

### 测试

- [ ] 4.1 新建 `tests/test_interaction_bus_glob.cpp` — ≥5 cases
  - 精确匹配（无通配符）→ callback called
  - 单通配符 `"inference.*"` → match `"inference.lifecycle.idle"` ✓
  - 多通配符 `"*.lifecycle.*"` → match `"inference.lifecycle.idle"` ✓
  - 无匹配 `"other.*"` → callback NOT called
  - **Race test**: subscribe + unsubscribe 并发 dispatch（lock semantics verify）

### 验收

- [ ] 5.1 `ctest -R test_interaction_bus_glob` 全绿
- [ ] 5.2 `ctest -R test_interaction_bus` 全绿（零回归）
- [ ] 5.3 `ctest` 全量零回归