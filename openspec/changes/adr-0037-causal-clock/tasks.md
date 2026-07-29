## 1. CausalClock 结构体 + 单元测试

- [ ] 1.1 确认 `include/agenticdsl/contract/causal_clock.h` 存在且包含 tick/now/merge/happens_before 4 方法
- [ ] 1.2 确认 `tests/test_causal_clock.cpp` 包含 5 个 TEST_CASE (单调性 / 线程安全 / merge / happens_before / soak)
- [ ] 1.3 编译 `test_causal_clock` 确认 5/5 TEST_CASE 全部 PASS
- [ ] 1.4 确认 `CausalClock::tick()` 使用 `fetch_add(1, memory_order_relaxed)` 实现

## 2. InMemoryBus emit 自动 tick + attach

- [ ] 2.1 确认 `InMemoryBus` 包含 `event::CausalClock causal_clock_` 成员
- [ ] 2.2 确认 `emit(const BusEvent&)` 中调用 `causal_clock_.tick()` 并赋值到 `e.causal_time`
- [ ] 2.3 确认 `emit(const string&, const string&)` 中同样调用 `causal_clock_.tick()` 并赋值到 `e.causal_time`
- [ ] 2.4 确认 `inmemory_bus.h` 包含 `#include "agenticdsl/contract/causal_clock.h"`

## 3. Causal monotonicity soak 测试

- [ ] 3.1 确认 `test_causal_clock.cpp` 的 soak test 使用 3 个 std::thread 生产者
- [ ] 3.2 确认每个生产者记录 100 次 tick 返回值
- [ ] 3.3 确认每个生产者线程内时间戳序列严格单调递增
- [ ] 3.4 确认 `clk.now() == 300` final assertion

## 4. ctest 全量验证 + 架构合规性检查

- [ ] 4.1 执行 `cmake --preset tests -DAGENTICDSL_BUILD_TESTS=ON` 配置测试构建
- [ ] 4.2 执行 `make -j$(nproc)` 编译
- [ ] 4.3 执行 `ctest --output-on-failure` 确认所有测试通过
- [ ] 4.4 确认 `test_causal_clock` 5/5 PASS
- [ ] 4.5 确认 `causal_clock.h` 路径为 `include/agenticdsl/contract/`（契约层，非 src/ 内部）
- [ ] 4.6 运行 `clangd --check` 关键文件零错误
- [ ] 4.7 运行 `openspec validate adr-0037-causal-clock` 确认通过