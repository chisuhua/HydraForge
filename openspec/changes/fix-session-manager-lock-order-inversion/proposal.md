## Why

`SessionManager` 在 `migrate_legacy_json` 函数末尾（L594-597）持 `index_mutex_` 时调 `flush_append_internal`，而后者取 `write_mutex_`（L638）— **lock-order inversion**，与全文件统一的 `write → index` 锁序冲突。

### 证据 1 — TSan 报告 #191（2026-09-15 实测）

`ctest --test-dir build-tsan -R '^test_session_manager_legacy$'` 报 4 项 lock-order-inversion warning：

```
Cycle in lock order graph: M0 (write_mutex_) => M1 (index_mutex_) => M0

Mutex M1 acquired here while holding M0:
  #4 ...SessionManager::open(...)         session_manager.cpp:89
  #5 ...SessionManager::migrate_legacy_json(...) session_manager.cpp:562

Mutex M0 acquired here while holding M1:
  #4 ...SessionManager::flush_append_internal(...) session_manager.cpp:638
  #5 ...SessionManager::migrate_legacy_json(...) session_manager.cpp:596
```

**关键观察**：TSan 锁序图环检测**不要求真并发** — 同一线程先后观察到 M0→M1 (open() L89) 与 M1→M0 (migrate 末段 L596→flush_append_internal L638) 两条边即闭环。`migrate_legacy_json` 单线程调用即触发，**4 个 TEST_CASE 全部 40/40 assertion PASS 但 TSan 仍 FAIL**（test binary exit non-zero）。

### 证据 2 — 真实的 TSan 基线（2026-09-15 实测，build-tsan）

`ctest -R 'session|causal|domain_worker|concurrent'` 实际失败清单（6 项，其中 3 项与本 change 相关）：

| # | Test | 根因 | 与本 change 关系 |
|---|---|---|---|
| **191** | **test_session_manager_legacy** | **lock-order-inversion L562+L596** | **✅ 本 change 修复目标** |
| 85 | test_domain_worker_pool | libstdc++ ctype<char> locale | pre-existing KI（AGENTS.md 早记录） |
| 199 | test_session_writer_eventlog_integration | libstdc++ writev + streambuf xsputn | pre-existing KI（9-13 fix 修的是 ofstream file_mutex_，与本基线不同根因） |
| 1 | test_chat_session | chat_session.cpp:291 in ~Impl() | 已知 race（chat-session-pdk-lift Change 1/2 ship 引入，独立 follow-up） |
| 15 | test_chat_session_queues | chat_session.cpp:296 in ~Impl() | 已知 race（chat-session-pdk-lift Change 1/2 ship 引入，独立 follow-up） |
| 195 | test_session_registry | Catch2 framework isActive + assertionPassed race | pre-existing KI（Catch2 + TSan 已知交互） |

**修复后预期**：#191 翻绿（从 lock graph 移除 M1→M0 边）；其余 5 项不变。

### 证据 3 — 锁序枚举（全 `src/core/session_manager.cpp`）

| 函数 | 取锁顺序 | 评估 |
|---|---|---|
| `open()` L61→L89 | write → index | ✓ |
| `flush_append()` L122→L178 | write → index | ✓ |
| `migrate_legacy_json` L562→L89（内部 open）| write → index（释放后）| ✓ |
| `migrate_legacy_json` L594-597 → L638 | **index → write** | **⚠ INVERSION — 本 change 修复** |
| `compact()` L411→L425 | write → index | ✓ |
| `rename_session()` L613→L632 | write → index | ✓ |
| `fork()` L350 释放 index 后才调 `flush_append_internal` | 无嵌套 | ✓ |
| `append_to_branch` L372 同上 | 无嵌套 | ✓ |

**唯一**的 inversion 边是 L594-597。修复后全局统一 `write → index`。

### 证据 4 — 2026-09-11 原始审计的处方（`docs/superpowers/plans/2026-09-11-chat-session-pdk-lift-change2.md` L779-783）

审计 L783 字面要求是修 **`migrate_legacy_json` 不在持 `write_mutex_` 时调 `open()`**（即 `open(id, legacy_path)` 路径的 `open→migrate→open` 递归自死锁）。**这是一个独立的潜伏 bug**：两参 `open()` 持 `write_mutex_` 时调 `migrate_legacy_json`，后者 L562 调 `open()` 重入 L61（`std::mutex` 非递归，必然死锁）。**全库 grep 零调用方**（`main.cpp:465` / `chat_session.cpp:935` / 全部 tests 都用单参 `open(id)`）→ bug 潜伏但真实。

**本 change 同时修复两个 bug**（Oracle 审查的 SHIP-with-fixes 判定）：

1. **L594-597 index→write inversion**（TSan #191 报告的根因，4 TEST_CASE 触发）— 主修复
2. **`open(id, legacy_path)` 路径的递归自死锁**（无调用方、不被 TSan 报告、但审计点名）— 同 change 修复，避免未来回归

**两者必须捆绑**（拆分会让后到的 change 面对"TSan 仍失败"的混乱基线）。

### Oracle 优先级

`MEDIUM-HIGH`（UB-class 缺陷 + 审计点名 + 4 TEST_CASE 触发 TSan 报告，与 2026-09-13 `fix(session_writer): serialize file writes` 模式同源）。

## What Changes

### 修复 1 — L594-597 index→write inversion（Critical 1，Oracle 修单）

**Before** (L593-597):

```cpp
// 写 main branch meta
{
  std::lock_guard<std::mutex> lock(index_mutex_);
  flush_append_internal(branches_["main"]);  // branches_["main"] 在锁内, 但 flush_append 取 write_mutex_ → inversion
}
```

**After**:

```cpp
// 写 main branch meta — BranchMeta 在 index_mutex_ 下快照, 锁外调 flush_append_internal
// 保持全局 write→index 锁序, 消除与 flush_append 的 ABBA 死锁
BranchMeta main_meta;
{
  std::lock_guard<std::mutex> lock(index_mutex_);
  main_meta = branches_["main"];
}
flush_append_internal(main_meta);
```

**为什么需要快照（而非简单删除锁块）**：`branches_` 是活 `unordered_map`（`fork()` L351、`load_jsonl` L224/L250、`compact()` L492 都会写）。直接删除锁块会让 `branches_["main"]` 求值变成无锁 `operator[]` 访问 → TSan 必报新 data race。快照模式与 `fork()` L349-355 已验证模式一致。

### 修复 2 — `open()` 删除 `legacy_path` 参数（Critical 2，Oracle 修单 + Metis 替代方案）

**Before** (`src/core/session_manager.h:157`):

```cpp
SessionHandle open(const std::string& session_id,
                   std::optional<std::string> legacy_path = std::nullopt);
```

**After** (硬性 BREAKING, **构造性消除递归**):

```cpp
SessionHandle open(const std::string& session_id);
```

**对应实现** (`src/core/session_manager.cpp:60-118`)：

- 删除 L62-65 `legacy_path` 参数检查
- 删除 L80-87 legacy 分支（含 `migrate_legacy_json` 调用）
- 函数体剩余部分（L88-118 创建/打开 jsonl 文件、初始化 `branches_["main"]`、`current_branch_`、`current_session_id_`、`current_path_`）保持不变

**Why 删参数而非 hoist-with-recheck**：

| 方案 | 评估 | 决策 |
|---|---|---|
| Hoist（lock 外 exists() + 锁内 recheck）| 5 行；引入 TOCTOU 风险；保留 BREAKING 兼容 | ❌ |
| **删参数** | **零调用方**（已 grep 验证）；**构造性消除 write→write 递归**（物理不可能）；零 TOCTOU；4-5 行删除；公开 API 显式化（"migrate 需用户显式调"，语义清晰）| **✅** |
| 不修 | 审计点名的潜伏 bug 挂着，未来误用 hang | ❌ |

迁移指引：当前所有 5+ 个调用方都是单参 `open(id)`，**零** 迁移成本。`migrate_legacy_json` 本身**仍是公开 API**，tests 与未来用户显式调用即可。

### 修复 3 — 不变量 R1 增补（spec 补强）

`specs/session-manager-lock-order.md` R1 增列："`write_mutex_` 为不可递归锁，任何路径不得在持 `write_mutex_`（直接或间接）时再次进入取 `write_mutex_` 的代码路径。" 防止未来有人在持锁路径里再塞 helper。

### 修复 4 — 新增并发回归测试

`tests/test_session_manager_lock_order.cpp`（独立二进制，GLOB 自动注册）：

- **Case 1 (TSan-gate)**: 线程 A `migrate_legacy_json` + 线程 B `flush_append`（retry-until-open 对齐 + 100 iter 压力循环，spec R2.2 要求）— TSan lock-graph 触发 cycle 即报警，无需真实死锁重叠
- **Case 2 (functional regression for Critical 2)**: 单线程直调 `open(id, legacy_path)` 编译期失败（参数已删）+ grep 验证声明不存在 — 文档化为本 change **构造性消除**该路径，**未来若有人加回参数**，编译期立即报警
- **Case 3 (functional regression for write→index 不变量)**: 单线程 `migrate_legacy_json` 完成后调 `flush_append` 验证 JSONL 内容正确（不变量保持，零行为变化）

### Non-goals

- 不重写 SessionManager 为 lock-free / finer-grained lock
- 不重写 `migrate_legacy_json` 语义（仍是 open → 写 main branch → 迁移 messages → 写 main branch meta）
- 不动 ChatSession 锁序（已通过 `chat-session-pdk-lift Change 2` Task 11.1 D8 注释固化 + #1/#15 in ~Impl() 单独 follow-up）
- 不修 #85 / #199 / #195 等其他 TSan pre-existing KI
- 不回填 `open(id, legacy_path)` 兼容层（零调用方 + 编译期 fail-fast 已是天然保护）

## 影响面

| 类型 | 范围 |
|---|---|
| 生产代码 | `src/core/session_manager.cpp` (~5 行修复 1 + ~10 行删除修复 2) |
| 头文件 | `src/core/session_manager.h` (~1 行参数删除) |
| 新测试 | `tests/test_session_manager_lock_order.cpp` (~80 行, 1 test binary, GLOB 自动注册) |
| 公开 API | **BREAKING**: `open(const std::string&, std::optional<std::string>)` → `open(const std::string&)`（5+ 调用方全部已是单参，零迁移成本） |
| ABI | **0 改动** (无 vtable/symbol 签名变化) |
| 现有测试 | **0 改动** (修复消除 inversion, 不引入新行为) |

## 验收

- **A1** `grep -n "lock_guard.*index_mutex" src/core/session_manager.cpp` 在 `migrate_legacy_json` 函数末尾**无** `flush_append_internal` 包裹
- **A2** `grep -n "legacy_path" src/core/session_manager.h` 返回 0 行（参数已删）
- **A3** `grep -rn "open(.*legacy" --include="*.cpp" --include="*.h" examples/ src/ tests/ pdk/` 返回 0 行（无调用方使用过两参）
- **A4** `cmake --build build` 全部 target 编译通过
- **A5** `ctest --test-dir build` 234/234 PASS 不变基线
- **A6** `ctest --test-dir build-tsan -R '^test_session_manager_legacy$'` **从 FAIL 翻 PASS**（TSan 锁序图 M1→M0 边消失）
- **A7** `ctest --test-dir build-tsan -R '^test_session_manager_lock_order$'` PASS 0 warning
- **A8** 基线回归：`ctest --test-dir build-tsan -R 'session|causal|domain_worker|concurrent'` 应**仅** #85 + #199 + #1 + #15 + #195 失败（5 项不变 pre-existing KI），**无新增**

## 风险

| 风险 | 等级 | 缓解 |
|---|---|---|
| BREAKING API 删 `legacy_path` 参数 | **零** | 5+ 调用方全部单参，已 grep 验证；编译期 fail-fast |
| `branches_["main"]` 快照后 `flush_append_internal` 内若改 `branches_` → race | 低 | `flush_append_internal` 函数体 (`session_manager.cpp:637-660`) 仅取 `write_mutex_` + 写 jsonl 文件，**不读/写** `branches_` map |
| TSan pre-existing #1 #15 chat_session.cpp:291/296 in ~Impl race 不被本 change 修 | 中 | 已在验收 A8 显式登记，独立 follow-up 处理 |
| TSan false positive in libstdc++ locale（Catch2 framework） | 低 | A8 接受 #85 / #199 / #195 继续失败（AGENTS.md 早记录 KI） |
| `migrate_legacy_json` 内部 `fs::copy` 抛异常（备份失败）| 低 | Pre-existing 行为不变；若 hoist 后调用位置变化，抛点相对锁的语义需重新核对——本 change 删参数后该路径不存在，**风险自动消除** |
| `open(id, legacy_path)` 删除后未来误加回 → 死锁回归 | 低 | 新测试 Case 2 grep 验证声明 + 编译期 fail-fast 提供双重保护 |
