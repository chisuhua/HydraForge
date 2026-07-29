# adr-0002-busevent-contract Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 形式化 ship EventBus 基础设施链 Change A（BusEvent 公开契约一次性收敛），验证 InMemoryBus 已实装 BusEvent 公开契约 + 3 个测试 Mock 迁移 + 双路径分发 + 10000 事件 soak 测试通过。

**Architecture:** 本 change 形式化已 ship 的代码（commit `e8c9a49`，2026-07-26 "refactor: Oracle 重构 EventBus 基础设施链"），不引入新代码逻辑。Plan 任务是验证类而非实现类：每步运行既有的 ctest/test 确认架构目标已达成。

**Tech Stack:** C++20, Catch2, CMake 3.20+, IInteractionBus 契约层, nlohmann/json (test mock)

---

## File Structure

无需创建/修改生产代码（已 ship）。所有验证通过既有的测试文件 + ctest。

### Production Code (already shipped, do NOT modify)

| File | Responsibility | Status |
|---|---|---|
| `include/agenticdsl/contract/bus_event.h` | BusEvent struct 5 字段 | ✅ 已 ship |
| `include/agenticdsl/contract/iinteraction_bus.h` | IInteractionBus 接口（emit(BusEvent) + emit(string,string) 双重） | ✅ 已 ship |
| `include/agenticdsl/contract/inmemory_bus.h` | InMemoryBus 实现（queue<BusEvent> + dual-path dispatch） | ✅ 已 ship |
| `src/common/contract/inmemory_bus.cpp` | InMemoryBus 实现 + glob_match + has_wildcard | ✅ 已 ship |

### Tests (verification targets, do NOT modify)

| File | Coverage |
|---|---|
| `tests/test_event_bus_soak.cpp` | BusEvent default/with-fields + 10000 事件 soak |
| `tests/test_interaction_bus.cpp` | IInteractionBus 接口契约 |
| `tests/test_engine_bus_integration.cpp` | DSLEngine + InMemoryBus 集成 |
| `tests/test_interaction_bus_glob.cpp` | glob subscribe 6 cases（虽属 0019 scope 但基础设施在本 change 验证） |
| `tests/test_escalation_triggers.cpp` | MockBusForEscalation 验证 |
| `tests/test_skill_interpreter.cpp` | MockBus 验证 |
| `tests/test_tool_coordinator.cpp` | MockInteractionBus 验证 |

---

### Task 1: 验证 BusEvent struct + IInteractionBus 接口契约

**Files:**
- Verify: `include/agenticdsl/contract/bus_event.h` (struct definition)
- Verify: `include/agenticdsl/contract/iinteraction_bus.h` (interface)
- Test: `tests/test_event_bus_soak.cpp` (BusEvent default + with-fields)

- [ ] **Step 1: 检查 BusEvent struct 5 字段**

```bash
grep -E '^\s*(topic|payload|timestamp|causal_time|priority)\s' /workspace/project/HydraForge/include/agenticdsl/contract/bus_event.h
```
Expected: 5 行匹配，类型分别为 `std::string` / `ToolResult` / `int64_t` / `uint64_t` / `int`

- [ ] **Step 2: 检查 IInteractionBus emit 重载**

```bash
grep -E 'virtual.*emit' /workspace/project/HydraForge/include/agenticdsl/contract/iinteraction_bus.h
```
Expected: 至少 `emit(const BusEvent&)` 匹配

- [ ] **Step 3: 编译并运行 test_event_bus_soak**

```bash
cd /workspace/project/HydraForge
cmake --preset tests -DAGENTICDSL_BUILD_TESTS=ON 2>&1 | tail -3
make -j$(nproc) test_event_bus_soak 2>&1 | tail -3
./build/tests/test_event_bus_soak --reporter compact 2>&1 | tail -10
```
Expected: 全部断言 PASS（BusEvent default construction, with fields, 10000 events no loss, try_pop returns BusEvent）

- [ ] **Step 4: 验证 InMemoryBus 已用 queue<BusEvent>**

```bash
grep -E 'queue<BusEvent>|deque<BusEvent>' /workspace/project/HydraForge/include/agenticdsl/contract/inmemory_bus.h /workspace/project/HydraForge/src/common/contract/inmemory_bus.cpp
```
Expected: 至少 1 处匹配，确认迁移完成

- [ ] **Step 5: Commit verification record**

```bash
cd /workspace/project/HydraForge
git add openspec/changes/adr-0002-busevent-contract/tasks.md
git commit -m "verify(ship): adr-0002-busevent-contract BusEvent struct + InMemoryBus verified"
```

### Task 2: 验证 3 个测试 Mock 迁移

**Files:**
- Verify: `tests/test_escalation_triggers.cpp` (MockBusForEscalation)
- Verify: `tests/test_skill_interpreter.cpp` (MockBus)
- Verify: `tests/test_tool_coordinator.cpp` (MockInteractionBus)

- [ ] **Step 1: MockBusForEscalation 验证**

```bash
grep -A2 "class MockBusForEscalation" /workspace/project/HydraForge/tests/test_escalation_triggers.cpp | head -20
grep "void emit" /workspace/project/HydraForge/tests/test_escalation_triggers.cpp
```
Expected: MockBusForEscalation 有 `emit(const BusEvent&)` 方法

- [ ] **Step 2: MockBus 验证**

```bash
grep "void emit" /workspace/project/HydraForge/tests/test_skill_interpreter.cpp
```
Expected: MockBus 有 `emit(const BusEvent&)` 方法

- [ ] **Step 3: MockInteractionBus 验证**

```bash
grep "void emit" /workspace/project/HydraForge/tests/test_tool_coordinator.cpp
```
Expected: MockInteractionBus 有 `emit(const BusEvent&)` 方法

- [ ] **Step 4: 编译并运行 3 个 mock 测试**

```bash
cd /workspace/project/HydraForge
make -j$(nproc) test_escalation_triggers test_skill_interpreter test_tool_coordinator 2>&1 | tail -5
./build/tests/test_escalation_triggers --reporter compact 2>&1 | tail -5
./build/tests/test_skill_interpreter --reporter compact 2>&1 | tail -5
./build/tests/test_tool_coordinator --reporter compact 2>&1 | tail -5
```
Expected: 3 个测试 binary 全部 PASS

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/HydraForge
git add -A
git commit -m "verify(ship): adr-0002-busevent-contract 3 mock migration verified"
```

### Task 3: 验证 emit/subscribe 签名一致性 + glob subscribe

**Files:**
- Verify: `include/agenticdsl/contract/iinteraction_bus.h` (subscribe 签名)
- Verify: `include/agenticdsl/contract/inmemory_bus.h` (exact_subscribers_ + wildcard_subscribers_)
- Test: `tests/test_interaction_bus_glob.cpp` (6 glob cases)

- [ ] **Step 1: 验证 subscribe 签名接受 pattern**

```bash
grep -E "subscribe\(" /workspace/project/HydraForge/include/agenticdsl/contract/iinteraction_bus.h
```
Expected: `subscribe(const std::string& event_type, ...)` 签名

- [ ] **Step 2: 验证 InMemoryBus 双路径**

```bash
grep -E "exact_subscribers_|wildcard_subscribers_" /workspace/project/HydraForge/include/agenticdsl/contract/inmemory_bus.h
```
Expected: 2 个成员变量都存在

- [ ] **Step 3: 编译并运行 glob 测试**

```bash
cd /workspace/project/HydraForge
make -j$(nproc) test_interaction_bus_glob 2>&1 | tail -3
./build/tests/test_interaction_bus_glob --reporter compact 2>&1 | tail -10
```
Expected: 6 个 TEST_CASE 全部 PASS（精确匹配 / 单通配符 / 多通配符 / 无匹配 / unsubscribe / 并发竞争）

- [ ] **Step 4: 验证 DSLEngine subscribe 回调签名保持**

```bash
grep -A2 "subscribe" /workspace/project/HydraForge/src/core/engine.h | grep -E "ToolResult|BusEvent"
```
Expected: DSLEngine 内部仍为 `void(const ToolResult&)`（内部包装，向后兼容）

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/HydraForge
git add -A
git commit -m "verify(ship): adr-0002-busevent-contract glob subscribe + signature compat verified"
```

### Task 4: 全量 ctest 验证零回归

**Files:**
- Run: `ctest` 全套测试

- [ ] **Step 1: 配置 + 编译**

```bash
cd /workspace/project/HydraForge
cmake --preset tests -DAGENTICDSL_BUILD_TESTS=ON 2>&1 | tail -3
make -j$(nproc) 2>&1 | tail -3
```
Expected: 编译零错误

- [ ] **Step 2: 全量 ctest**

```bash
cd /workspace/project/HydraForge
ctest --output-on-failure 2>&1 | tail -20
```
Expected: 全部 PASS（baseline 至少 83 个测试 + 任何新增测试）

- [ ] **Step 3: openspec validate**

```bash
cd /workspace/project/HydraForge
openspec validate adr-0002-busevent-contract --json 2>&1 | tail -5
```
Expected: `passed: true`, 0 issues

- [ ] **Step 4: 标记 tasks.md 全部完成**

```bash
cd /workspace/project/HydraForge
sed -i 's/^- \[ \]/- [x]/g' openspec/changes/adr-0002-busevent-contract/tasks.md
```

- [ ] **Step 5: Commit final verification**

```bash
cd /workspace/project/HydraForge
git add openspec/changes/adr-0002-busevent-contract/tasks.md
git commit -m "verify(ship): adr-0002-busevent-contract full ctest 0 regression, tasks.md complete"
```
