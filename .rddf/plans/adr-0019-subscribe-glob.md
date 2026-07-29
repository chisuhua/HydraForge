# adr-0019-subscribe-glob Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]` syntax for tracking.

**Goal:** 形式化 ship subscribe() 接受 glob pattern + InMemoryBus 双路径分发（exact O(1) + wildcard O(w)），验证已实装的 glob_match / has_wildcard / race-condition 安全。

**Architecture:** 本 change 形式化已 ship 的代码（commit `e8c9a49` 2026-07-26 内含 Change B），无新代码逻辑。Plan 任务是验证类：grep 确认实现 + 6 个 glob test 跑通 + 并发 race test 通过。

**Tech Stack:** C++20, Catch2, std::function callback, std::unordered_map + vector 双路径

---

## File Structure

无需创建/修改生产代码（已 ship）。所有验证通过既有的测试文件 + ctest。

### Production Code (already shipped, do NOT modify)

| File | Responsibility | Status |
|---|---|---|
| `include/agenticdsl/contract/iinteraction_bus.h` | subscribe(pattern, callback) 接口 | ✅ 已 ship |
| `include/agenticdsl/contract/inmemory_bus.h` | exact_subscribers_ + wildcard_subscribers_ 双成员 | ✅ 已 ship |
| `src/common/contract/inmemory_bus.cpp` | glob_match() + has_wildcard() + dispatch_loop | ✅ 已 ship |

### Tests (verification targets, do NOT modify)

| File | Coverage |
|---|---|
| `tests/test_interaction_bus_glob.cpp` | 6 glob cases (精确 / 单通配 / 多通配 / 无匹配 / unsubscribe / 并发竞争) |

---

### Task 1: 验证 glob_match + has_wildcard 算法实现

**Files:**
- Verify: `src/common/contract/inmemory_bus.cpp` (glob_match, has_wildcard)
- Verify: `include/agenticdsl/contract/inmemory_bus.h` (subscribe routing)

- [ ] **Step 1: 检查 glob_match 函数存在**

```bash
grep -n "bool glob_match" /workspace/project/HydraForge/src/common/contract/inmemory_bus.cpp
```
Expected: 1 行匹配

- [ ] **Step 2: 检查 has_wildcard 函数存在**

```bash
grep -n "has_wildcard" /workspace/project/HydraForge/src/common/contract/inmemory_bus.cpp
```
Expected: 至少 2 行匹配（定义 + 调用）

- [ ] **Step 3: 验证 dual-path 成员**

```bash
grep -E "exact_subscribers_|wildcard_subscribers_" /workspace/project/HydraForge/include/agenticdsl/contract/inmemory_bus.h
```
Expected: 2 个成员都存在

- [ ] **Step 4: 运行 test_interaction_bus_glob 6 cases**

```bash
cd /workspace/project/HydraForge
make -j$(nproc) test_interaction_bus_glob 2>&1 | tail -3
./build/tests/test_interaction_bus_glob --reporter compact 2>&1 | tail -10
```
Expected: 6 个 TEST_CASE 全部 PASS（精确匹配 / 单通配符 / 多通配符 / 无匹配 / unsubscribe / 并发竞争）

- [ ] **Step 5: Commit verification**

```bash
cd /workspace/project/HydraForge
git add openspec/changes/adr-0019-subscribe-glob/tasks.md
git commit -m "verify(ship): adr-0019-subscribe-glob glob_match + dual-path verified"
```

### Task 2: 验证 IInteractionBus subscribe 签名 + 向后兼容

**Files:**
- Verify: `include/agenticdsl/contract/iinteraction_bus.h`
- Verify: `include/agenticdsl/contract/inmemory_bus.h` (subscribe 实现)

- [ ] **Step 1: 检查 subscribe(pattern, callback) 签名**

```bash
grep -A2 "subscribe\(" /workspace/project/HydraForge/include/agenticdsl/contract/iinteraction_bus.h | head -10
```
Expected: pattern + callback 参数

- [ ] **Step 2: 验证 InMemoryBus::subscribe 调用 has_wildcard 路由**

```bash
grep -A4 "size_t InMemoryBus::subscribe" /workspace/project/HydraForge/src/common/contract/inmemory_bus.cpp
```
Expected: subscribe 实现包含 `if (has_wildcard(pattern))` 路由

- [ ] **Step 3: 验证 unsubscribe 同时搜索两个 map**

```bash
grep -B1 -A8 "InMemoryBus::unsubscribe" /workspace/project/HydraForge/src/common/contract/inmemory_bus.cpp
```
Expected: 同时遍历 exact_subscribers_ 和 wildcard_subscribers_

- [ ] **Step 4: 全量 ctest 验证 glob + interaction_bus 兼容**

```bash
cd /workspace/project/HydraForge
ctest --test-dir build -R "interaction_bus" --output-on-failure 2>&1 | tail -10
```
Expected: 所有 interaction_bus 相关测试 PASS

- [ ] **Step 5: Commit**

```bash
cd /workspace/project/HydraForge
git add -A
git commit -m "verify(ship): adr-0019-subscribe-glob subscribe signature + backward-compat verified"
```

### Task 3: 验证 race-condition + 全量 ctest + openspec validate

**Files:**
- Run: `test_interaction_bus_glob` (并发 case)
- Run: `ctest` 全套

- [ ] **Step 1: 跑 glob 并发竞争 case（counts >= 1000）**

```bash
cd /workspace/project/HydraForge
./build/tests/test_interaction_bus_glob "[interaction_bus][glob]" --reporter compact 2>&1 | tail -5
```
Expected: concurrent case PASS

- [ ] **Step 2: 全量 ctest 零回归**

```bash
cd /workspace/project/HydraForge
ctest --test-dir build --output-on-failure 2>&1 | tail -5
```
Expected: 100% tests passed, 0 failed

- [ ] **Step 3: openspec validate**

```bash
cd /workspace/project/HydraForge
openspec validate adr-0019-subscribe-glob --json 2>&1 | tail -5
```
Expected: passed: true, 0 issues

- [ ] **Step 4: 标记 tasks.md 全部完成**

```bash
cd /workspace/project/HydraForge
sed -i 's/^- \[ \]/- [x]/g' openspec/changes/adr-0019-subscribe-glob/tasks.md
```

- [ ] **Step 5: Commit final verification**

```bash
cd /workspace/project/HydraForge
git add openspec/changes/adr-0019-subscribe-glob/tasks.md
git commit -m "verify(ship): adr-0019-subscribe-glob race-test + full ctest verified, tasks.md complete"
```
