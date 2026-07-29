# adr-0037-causal-clock Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]` syntax for tracking.

**Goal:** 形式化 ship CausalClock + InMemoryBus emit auto-tick，验证已实装的因果向量/线程安全/soak 测试通过。

**Architecture:** 本 change 形式化已 ship 的代码（commit `e8c9a49` 2026-07-26 内含 Change C）。CausalClock 单 counter（atomic fetch_add memory_order_relaxed） + happens_before 比较 + InMemoryBus::emit() 自动 tick + BusEvent.causal_time 字段填充。

**Tech Stack:** C++20, std::atomic<uint64_t>, std::chrono, Catch2

---

## File Structure

无需创建/修改生产代码（已 ship）。所有验证通过既有的测试文件 + ctest。

### Production Code (already shipped, do NOT modify)

| File | Responsibility | Status |
|---|---|---|
| `include/agenticdsl/contract/causal_clock.h` | CausalClock struct (atomic tick + now + merge + happens_before) | ✅ 已 ship |
| `src/common/contract/causal_clock.h` | CausalClock implementation | ✅ 已 ship |
| `include/agenticdsl/contract/inmemory_bus.h` | InMemoryBus 持有 `causal_clock_` 成员 | ✅ 已 ship |
| `src/common/contract/inmemory_bus.cpp` | emit() 内 `e.causal_time = causal_clock_.tick()` | ✅ 已 ship |

### Tests (verification targets, do NOT modify)

| File | Coverage |
|---|---|
| `tests/test_causal_clock.cpp` | 5 cases (单调 / 线程安全 / merge / happens_before / 3-producer soak) |

---

### Task 1: 验证 CausalClock 结构 + 单元测试

**Files:**
- Verify: `include/agenticdsl/contract/causal_clock.h`
- Verify: `src/common/contract/causal_clock.h`
- Test: `tests/test_causal_clock.cpp`

- [ ] **Step 1: 检查 CausalClock 4 方法存在**

```bash
grep -E "(uint64_t|void|bool) (tick|now|merge|happens_before)" /workspace/project/HydraForge/include/agenticdsl/contract/causal_clock.h
```
Expected: tick/now/merge/happens_before 4 方法签名

- [ ] **Step 2: 检查 tick() 使用 fetch_add(memory_order_relaxed)**

```bash
grep -E "fetch_add.*memory_order_relaxed" /workspace/project/HydraForge/src/common/contract/causal_clock.h /workspace/project/HydraForge/include/agenticdsl/contract/causal_clock.h
```
Expected: 至少 1 行匹配

- [ ] **Step 3: 编译 + 跑 test_causal_clock 5/5**

```bash
cd /workspace/project/HydraForge
make -j$(nproc) test_causal_clock 2>&1 | tail -3
./build/tests/test_causal_clock --reporter compact 2>&1 | tail -10
```
Expected: 5 个 TEST_CASE 全部 PASS

- [ ] **Step 4: 验证 InMemoryBus 包含 causal_clock_ 成员**

```bash
grep -E "causal_clock_" /workspace/project/HydraForge/include/agenticdsl/contract/inmemory_bus.h /workspace/project/HydraForge/src/common/contract/inmemory_bus.cpp
```
Expected: 成员声明 + tick() 调用

- [ ] **Step 5: Commit verification**

```bash
cd /workspace/project/HydraForge
git add openspec/changes/adr-0037-causal-clock/tasks.md
git commit -m "verify(ship): adr-0037-causal-clock CausalClock 4 methods + 5 tests verified"
```

### Task 2: 验证 InMemoryBus emit auto-tick

**Files:**
- Verify: `src/common/contract/inmemory_bus.cpp`

- [ ] **Step 1: 检查 emit(const BusEvent&) 内 causal_time 赋值**

```bash
grep -B1 -A2 "causal_time = " /workspace/project/HydraForge/src/common/contract/inmemory_bus.cpp
```
Expected: `e.causal_time = causal_clock_.tick()` 或等价

- [ ] **Step 2: 检查 emit(string, string) 重载也填 causal_time**

```bash
grep -A5 "void InMemoryBus::emit.*const std::string&" /workspace/project/HydraForge/src/common/contract/inmemory_bus.cpp
```
Expected: 第二个 emit 重载也设置 causal_time

- [ ] **Step 3: 验证 inmemory_bus.h 包含 causal_clock.h**

```bash
grep "causal_clock.h" /workspace/project/HydraForge/include/agenticdsl/contract/inmemory_bus.h
```
Expected: include 行存在

- [ ] **Step 4: 运行 BusEvent 集成测试 (soak + event_bus)**

```bash
cd /workspace/project/HydraForge
ctest --test-dir build -R "event_bus_soak|causal_clock" --output-on-failure 2>&1 | tail -10
```
Expected: 全部 PASS

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/HydraForge
git add -A
git commit -m "verify(ship): adr-0037-causal-clock emit auto-tick + 2 overloads verified"
```

### Task 3: 全量 ctest + openspec validate

**Files:**
- Run: `ctest` 全套

- [ ] **Step 1: 全量 ctest 零回归**

```bash
cd /workspace/project/HydraForge
ctest --test-dir build --output-on-failure 2>&1 | tail -5
```
Expected: 100% tests passed, 0 failed

- [ ] **Step 2: openspec validate**

```bash
cd /workspace/project/HydraForge
openspec validate adr-0037-causal-clock --json 2>&1 | tail -5
```
Expected: passed: true, 0 issues

- [ ] **Step 3: 标记 tasks.md 全部完成**

```bash
cd /workspace/project/HydraForge
sed -i 's/^- \[ \]/- [x]/g' openspec/changes/adr-0037-causal-clock/tasks.md
```

- [ ] **Step 4: Commit final**

```bash
cd /workspace/project/HydraForge
git add openspec/changes/adr-0037-causal-clock/tasks.md
git commit -m "verify(ship): adr-0037-causal-clock full ctest verified, tasks.md complete"
```
