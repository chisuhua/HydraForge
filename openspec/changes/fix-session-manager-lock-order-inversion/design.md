# Design — fix-session-manager-lock-order-inversion

## 关键证据 (2026-09-15 TSan 实测)

```
WARNING: ThreadSanitizer: lock-order-inversion (potential deadlock) (pid=...)
  Cycle in lock order graph: M0 (0x...) => M1 (0x...) => M0

  Mutex M1 acquired here while holding mutex M0 in main thread:
    #4 ...SessionManager::open(...)         session_manager.cpp:89
    #5 ...SessionManager::migrate_legacy_json(...) session_manager.cpp:562

  Mutex M0 acquired here while holding mutex M1 in main thread:
    #4 ...SessionManager::flush_append_internal(...) session_manager.cpp:638
    #5 ...SessionManager::migrate_legacy_json(...) session_manager.cpp:596
```

`M0=write_mutex_`, `M1=index_mutex_`. 两条边:
- **M0→M1 (write→index)**: `migrate_legacy_json` L562 调 `open()`, `open()` L61 持 write_mutex_ → L89 取 index_mutex_
- **M1→M0 (index→write)**: `migrate_legacy_json` L595 持 index_mutex_ → L596 调 `flush_append_internal` L638 取 write_mutex_

**同一线程**先后出现两条边 → TSan 锁序图闭环 → 报警(不需要真并发重叠)。

## 锁序枚举 (`src/core/session_manager.cpp` 实测)

| 函数 | 行号 | 取锁顺序 | 评估 |
|---|---|---|---|
| `open()` | L61 → L89 | write → index | ✓ |
| `flush_append()` | L122 → L178 | write → index | ✓ |
| `migrate_legacy_json` 内部 `open()` | L562 → L89 (释放) | write → index | ✓ |
| `migrate_legacy_json` 末段 | L594-597 → L638 | **index → write** | **⚠ 唯一 inversion** |
| `compact()` | L411 → L425 | write → index | ✓ |
| `rename_session()` | L613 → L632 | write → index | ✓ |
| `fork()` | L350 释放后调 `flush_append_internal` | 无嵌套 | ✓ |
| `append_to_branch` | L372 同上 | 无嵌套 | ✓ |

**唯一**的 inversion 是 `migrate_legacy_json` 末段。修复后全局统一 `write → index`。

## 关键设计决策

### D1: Critical 1 — 快照模式(而非直接删除锁块)

**Before** (L593-597):
```cpp
{
  std::lock_guard<std::mutex> lock(index_mutex_);
  flush_append_internal(branches_["main"]);
}
```

**问题**: 直接删除锁块 → `branches_["main"]` 在锁外求值 → 对 `unordered_map` 做 `operator[]`(含潜在的插入) → 活 map(`fork()` L351 / `load_jsonl` L224/L250 / `compact()` L492 都会写)→ TSan 必报新 data race。原提案 risk R2 误写为"`branches_` 是只读快照"是**错的**(活 map,非快照)。

**After** (Oracle 修单 + 与 `fork()` L349-355 一致模式):
```cpp
BranchMeta main_meta;
{
  std::lock_guard<std::mutex> lock(index_mutex_);
  main_meta = branches_["main"];  // 锁内快照 POD
}
flush_append_internal(main_meta);  // 锁外调用, 与全局 write→index 一致
```

`BranchMeta` 是 POD-like (见 `src/core/session_manager.h` 定义),按值复制零开销。

### D2: Critical 2 — 删除 `open()` 的 `legacy_path` 参数(构造性消除递归)

**为什么删参数而非 hoist-with-recheck**:

| 方案 | 评估 | 决策 |
|---|---|---|
| Hoist(lock 外 `exists()` + 锁内 recheck)| 5 行;引入 TOCTOU 风险;保留 BREAKING 兼容 | ❌ |
| **删参数** | **零调用方**(grep 验证:`main.cpp:465` / `chat_session.cpp:935` / 全部 tests 都用单参 `open(id)`);**构造性消除 write→write 递归**(物理不可能);零 TOCTOU;4-5 行删除;公开 API 显式化("migrate 需用户显式调",语义清晰)| **✅** |
| 不修 | 审计点名的潜伏 bug 挂着,未来误用 hang | ❌ |

**关键论据**:
1. `grep -rn "open(.*,.*legacy\|open(.*legacy" examples/ src/ tests/ pdk/` 返回 0 行(无任何调用方传两参)
2. `migrate_legacy_json` 本身**仍是公开 API** (`session_manager.h:254`),tests 直调 (`test_session_manager_legacy.cpp:75`, `L211`, `L234`),未来用户可显式调
3. 删参数 = 构造性消除"open→migrate→open"递归路径,物理上不可触发
4. 与 2026-09-11 审计 L783 处方实质等价("migrate 不在持 write_mutex_ 时调 open()"),但**更彻底**(直接消除"持 write_mutex_ 时调 migrate"路径)

**Before** (`session_manager.h:157`):
```cpp
SessionHandle open(const std::string& session_id,
                   std::optional<std::string> legacy_path = std::nullopt);
```

**After**:
```cpp
SessionHandle open(const std::string& session_id);
```

**实现侧** (`session_manager.cpp:60-118`):

```cpp
SessionHandle SessionManager::open(const std::string& session_id) {
  if (session_id.empty()) {
    throw std::runtime_error("SessionManager::open: session_id is empty");
  }
  if (session_id.find('/') != std::string::npos) {
    throw std::runtime_error(
        "SessionManager::open: session_id contains path separator");
  }
  std::filesystem::create_directories(dir_);
  const auto path = dir_ / (session_id + kSessionFileExt);
  // 旧版: 若 !jsonl_existed && legacy_path 且 exists → migrate_legacy_json
  // 新版: 删除此分支, migrate_legacy_json 必须由用户显式调用
  std::lock_guard<std::mutex> idx_lock(index_mutex_);
  if (branches_.find("main") == branches_.end()) {
    BranchMeta bm;
    bm.branch_id = "main";
    bm.name = "main";
    bm.forked_from_node = "";
    bm.created_at = ...;
    branches_["main"] = bm;
    current_branch_ = "main";
  }
  current_session_id_ = session_id;
  current_path_ = path;
  SessionHandle h;
  h.session_id = current_session_id_;
  h.jsonl_path = current_path_;
  return h;
}
```

**原 L82-87 删除的 6 行**:
```cpp
  if (!jsonl_existed && legacy_path.has_value() &&
      std::filesystem::exists(*legacy_path)) {
    migrate_legacy_json(*legacy_path);
    SessionHandle h; h.session_id = current_session_id_; h.jsonl_path = current_path_; return h;
  }
```

注意: 原 L82-87 的 early-return **在 L61 取 write_mutex_ 之前**,**不会**触发 write→write 递归(L61 还没取锁)。**真正的 write→write 递归**是 `migrate_legacy_json` 自身 L562 调 `open()`,因为 L562 已在 migrate 函数体内,**调用栈深处**重入 `open()` 的 L61 持 write_mutex_ 路径。**删 L82-87 只是去掉 L562→L89 (M0→M1) 边中可能重复的 legacy 调用入口**,不直接修递归。**构造性消除 L562→L61 write→write 重入**靠**调用方约定**(migrate 不在持 write_mutex_ 时被调)— 当前所有调用方都是先 `mgr.open(id)` 再 `mgr.migrate_legacy_json(...)`(不持 write_mutex_),或直接 `mgr.migrate_legacy_json(...)`(不持 write_mutex_)。**只要未来不出现"在持 write_mutex_ 时调 migrate"的代码模式**,write→write 递归物理不可能。

### D3: Spec R1 增补不变量

**Before**:
> R1: 所有写路径必须 write→index 不得 index→write

**After**:
> R1: SessionManager 满足以下两条不变量:
> (a) 所有写路径必须 write→index 不得 index→write
> (b) `write_mutex_` 为不可递归锁,任何路径不得在持 `write_mutex_`(直接或间接)时再次进入取 `write_mutex_` 的代码路径

**Why (b)**: 本次 Critical 2 修复 = 构造性消除"持 write_mutex_ 时调 migrate"路径;增列 (b) 防止未来有人写出 `flush_append()` 内部调 `open()` 之类的辅助函数(更隐蔽的 write→write 递归,TSan 看不见,hang 在生产)。

### D4: 测试设计 — Case 1 (TSan-gate) + Case 2 (构造性消除回归) + Case 3 (functional)

**Case 1 (TSan-gate, `migrate + flush_append` 并发)**:
```cpp
TEST_CASE("TSan lock-order-inversion regression",
          "[session_manager][lock_order][tsan]") {
  // 线程 A: migrate_legacy_json (内部 open, 内 L89 M0→M1; 末段修复后无 M1→M0)
  // 线程 B: 重试 flush_append 直到 A 内部 open() 设了 current_path_
  // 100 iter 压力循环
  // 备注: 本测试依赖 TSan lock-graph 检测, 不依赖真实死锁重叠时序
  //       (TSan 在两条边执行过即建图, 即便不重叠也报)
  // 修复后: 4 TEST_CASE + 本并发测试 PASS, 0 warning
}
```

**Case 2 (构造性消除回归 — 编译期 fail-fast)**:
```cpp
// 不写 TEST_CASE, 而是在 SessionManager 测试夹具顶部加编译期断言:
static_assert(!std::is_invocable_v<
    decltype(&agenticdsl::SessionManager::open),
    agenticdsl::SessionManager, const std::string&,
    std::optional<std::string>>,
    "open() must NOT have legacy_path parameter — write→write recursion risk");
```

**Case 3 (functional — 不变量保持)**:
```cpp
TEST_CASE("migrate_legacy_json + flush_append interleaving produces correct JSONL",
          "[session_manager][lock_order][functional]") {
  // 单线程: migrate → flush_append → flush_append → reload → 验证 content
  // 验证修复不引入新行为, JSONL 字节级一致
}
```

### D5: 风险缓解

| 风险 | 缓解 |
|---|---|
| BREAKING API 删 `legacy_path` 参数 | 5+ 调用方全部单参,grep 验证零使用;编译期 fail-fast |
| `branches_` 快照后 `flush_append_internal` 仍读 `branches_` | 函数体实测**不读** `branches_`,仅取 `write_mutex_` + 写 jsonl |
| 删 `legacy_path` 后未来误加回 → 死锁回归 | Case 2 `static_assert` + 编译期 fail-fast;grep A3 验证零调用 |
| TSan pre-existing #1 #15 chat_session.cpp:291/296 in ~Impl() | A8 显式登记,独立 follow-up(不在本 change 范围)|
| TSan pre-existing #85 #199 #195 | A8 接受继续失败(AGENTS.md 早记录 KI)|

## 实施步骤(高层)

1. **Step 1**: 修改 `src/core/session_manager.h` L157 — 删除 `legacy_path` 参数
2. **Step 2**: 修改 `src/core/session_manager.cpp` L60-118 — 删除 L62-65 `legacy_path` 参数检查 + L80-87 legacy 分支
3. **Step 3**: 修改 `src/core/session_manager.cpp` L593-597 — 应用 BranchMeta 快照模式
4. **Step 4**: 新建 `tests/test_session_manager_lock_order.cpp` — Case 1 + Case 2 + Case 3(GLOB 自动注册,无 CMake 改动)
5. **Step 5**: 验证 A1-A8
6. **Step 6**: OpenSpec archive

## 验收命令

```bash
# A1: migrate_legacy_json 末段无 index_mutex_ 包裹 flush_append_internal
grep -B 1 -A 2 "flush_append_internal" src/core/session_manager.cpp

# A2: open() 参数已删
grep -n "legacy_path" src/core/session_manager.h

# A3: 全库零两参 open() 调用
grep -rn "open(.*,.*legacy\|open(.*legacy" --include="*.cpp" --include="*.h" examples/ src/ tests/ pdk/

# A4: 全量编译
cmake --build build -j$(nproc)

# A5: functional ctest
ctest --test-dir build -j4 --timeout 180

# A6: #191 翻绿
ctest --test-dir build-tsan -R '^test_session_manager_legacy$' --output-on-failure

# A7: 新并发测试 PASS
ctest --test-dir build-tsan -R '^test_session_manager_lock_order$' --output-on-failure

# A8: 基线回归
ctest --test-dir build-tsan -R 'session|causal|domain_worker|concurrent' 2>&1 | grep -E 'FAILED|tests passed'
# 预期: 仅 #85 + #199 + #1 + #15 + #195 fail, #191 翻绿
```

## 备选方案

| 方案 | 评估 | 决策 |
|---|---|---|
| 仅修 Critical 1(snapshot)不改 Critical 2 | 审计处方落空;#191 翻绿但审计报告的"递归自死锁"挂着 | ❌ |
| 仅修 Critical 2(删参数)不改 Critical 1 | 删参数后 migrate 末段 `branches_["main"]` 仍裸读 → TSan 必报新 race;**#191 不翻绿** | ❌ |
| Hoist-with-recheck 替代删参数 | TOCTOU 风险;5+ 调用方无迁移但保留两参 open() 接口 | ❌ |
| **本设计(D1+D2)** | 双 bug 一次修;零调用方 BREAKING;构造性消除递归;TSan 锁序图收敛 | ✅ |
