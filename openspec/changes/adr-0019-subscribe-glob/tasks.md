## 1. 验证 glob_match 算法实现与边界覆盖

- [ ] 1.1 确认 `glob_match()` 在 `inmemory_bus.cpp` 中已实现（双指针回溯，支持 `*` 和 `?`）
- [ ] 1.2 确认 `glob_match()` 边界情况：空 pattern/空 topic、纯 `*`、无通配符、`?` 单字符
- [ ] 1.3 确认 `has_wildcard()` 函数正确路由 wildcard 与非 wildcard pattern

## 2. 验证 subscribe() 接口语义扩展

- [ ] 2.1 确认 `IInteractionBus::subscribe()` 签名不变（`size_t subscribe(const std::string& pattern, std::function<void(const BusEvent&)> callback)`）
- [ ] 2.2 确认 `subscribe()` 参数命名从 `event_type` 改为 `pattern`（语义变更反映在注释中）
- [ ] 2.3 确认 `InMemoryBus::subscribe()` 调用 `has_wildcard()` 路由到 `exact_subscribers_` 或 `wildcard_subscribers_`

## 3. 验证双路径分发架构

- [ ] 3.1 确认 `exact_subscribers_` 使用 `unordered_map<string, vector<handler>>`（O(1) 精确匹配）
- [ ] 3.2 确认 `wildcard_subscribers_` 使用 `unordered_map<string, vector<handler>>` + `glob_match()` 线性扫描
- [ ] 3.3 确认 `dispatch_loop()` 在锁内收集 callback 副本，锁外执行回调（无死锁）
- [ ] 3.4 确认 `unsubscribe()` 线性扫描两个映射，找到 token 后立即返回

## 4. 验证 glob 测试覆盖

- [ ] 4.1 确认 `test_interaction_bus_glob.cpp` 6 个 TEST_CASE 全部 PASS
- [ ] 4.2 确认精确匹配场景（期望 1 次匹配 + 1 次不匹配）
- [ ] 4.3 确认单通配符 `*` 场景（3 次匹配）
- [ ] 4.4 确认多通配符 `*.error.*` 场景（2 次匹配 + 1 次不匹配）
- [ ] 4.5 确认无匹配场景（0 次回调）
- [ ] 4.6 确认 unsubscribe 场景（unsubscribe 后零回调）
- [ ] 4.7 确认并发竞争场景（subscribe/unsubscribe 期间 dispatch，无崩溃，计数 >= 1000）

## 5. 架构合规性检查

- [ ] 5.1 确认 `subscribe()` 签名在 `IInteractionBus` 接口中未破坏向后兼容性
- [ ] 5.2 确认 `InMemoryBus` 双路径分发遵循 ADR-0019 契约分离原则
- [ ] 5.3 确认 `glob_match()` 无外部依赖（仅标准库 string）
- [ ] 5.4 运行 `clangd --check` 关键文件（`inmemory_bus.h`、`inmemory_bus.cpp`、`iinteraction_bus.h`）零错误
- [ ] 5.5 运行 `openspec validate adr-0019-subscribe-glob` 确认通过

## 6. 全量回归测试

- [ ] 6.1 执行 `cmake --preset tests -DAGENTICDSL_BUILD_TESTS=ON && make -j$(nproc) && ctest --output-on-failure`
- [ ] 6.2 确认 `test_interaction_bus_glob` 所有 6 个 TEST_CASE PASS
- [ ] 6.3 确认全量 ctest 零回归