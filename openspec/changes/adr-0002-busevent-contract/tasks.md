## 1. 验证现有关联文件状态

- [ ] 1.1 确认 `BusEvent` struct 定义已存在 (`include/agenticdsl/contract/bus_event.h` — 5 字段: topic, payload, timestamp, causal_time, priority)
- [ ] 1.2 确认 `IInteractionBus` 接口已存在 (`include/agenticdsl/contract/iinteraction_bus.h` — `emit(const BusEvent&)` + `emit(const string&, const string&)`)
- [ ] 1.3 确认 `InMemoryBus` 实现已存在 (`include/agenticdsl/contract/inmemory_bus.h` + `src/common/contract/inmemory_bus.cpp` — `queue<BusEvent>` + 双路径分发)
- [ ] 1.4 确认 3 个测试 Mock 已实现 `emit(const BusEvent&)`:
  - `tests/test_escalation_triggers.cpp::MockBusForEscalation`
  - `tests/test_skill_interpreter.cpp::MockBus`
  - `tests/test_tool_coordinator.cpp::MockInteractionBus`
- [ ] 1.5 确认 `test_event_bus_soak.cpp` 已验证 10000 事件 soak

## 2. 运行 ctest 验证零回归

- [ ] 2.1 执行 `cmake --preset tests -DAGENTICDSL_BUILD_TESTS=ON` 配置测试构建
- [ ] 2.2 执行 `make -j$(nproc)` 编译
- [ ] 2.3 执行 `ctest --output-on-failure` 确认所有测试通过
- [ ] 2.4 确认 `test_interaction_bus`、`test_engine_bus_integration`、`test_event_bus_soak`、`test_interaction_bus_glob` 全部 PASS

## 3. 开销测试 (BusEvent 类型 + InMemoryBus soak)

- [ ] 3.1 确认 `test_event_bus_soak.cpp` 的 `BusEvent default construction` 用例 PASS
- [ ] 3.2 确认 `test_event_bus_soak.cpp` 的 `BusEvent construction with fields` 用例 PASS
- [ ] 3.3 确认 `test_event_bus_soak.cpp` 的 `InMemoryBus soak — 10000 events no loss` 用例 PASS
- [ ] 3.4 确认 `test_event_bus_soak.cpp` 的 `InMemoryBus try_pop returns BusEvent` 用例 PASS

## 4. 验证 emit/subscribe 签名一致性

- [ ] 4.1 确认所有 `emit(const BusEvent&)` 调用点使用正确构造语法
- [ ] 4.2 确认所有 `emit(const string&, const string&)` 调用点保持向后兼容
- [ ] 4.3 确认 `subscribe` 回调签名均为 `void(const BusEvent&)`
- [ ] 4.4 确认 `DSLEngine::subscribe` 回调签名保持 `void(const ToolResult&)`（内部包装）

## 5. 验证 glob subscribe 测试

- [ ] 5.1 确认 `test_interaction_bus_glob.cpp` 的 6 个 TEST_CASE 全部 PASS
- [ ] 5.2 确认精确匹配 / 单通配符 / 多通配符 / 无匹配 / unsubscribe / 并发竞争覆盖完整

## 6. 验证 Mock 一致性

- [ ] 6.1 确认 `MockBusForEscalation::emit(const BusEvent&)` 记录 topic 到 `events_` 向量
- [ ] 6.2 确认 `MockBus::emit(const BusEvent&)` 空实现不抛出异常
- [ ] 6.3 确认 `MockInteractionBus::emit(const BusEvent&)` 记录 topic 到 `emit_log_` 向量

## 7. 架构合规性检查

- [ ] 7.1 确认 `bus_event.h` 路径为 `include/agenticdsl/contract/`（契约层，非 src/ 内部）
- [ ] 7.2 确认 `IInteractionBus` 接口无多余依赖（仅 `bus_event.h` + 标准库）
- [ ] 7.3 确认 `InMemoryBus` 实现遵循 ADR-0019 契约分离原则
- [ ] 7.4 运行 `clangd --check` 关键文件零错误
- [ ] 7.5 运行 `openspec validate adr-0002-busevent-contract` 确认通过