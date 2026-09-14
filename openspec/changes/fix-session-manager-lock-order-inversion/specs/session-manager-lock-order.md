# Spec — fix-session-manager-lock-order-inversion

## ADDED Requirements

### R1: SessionManager 锁顺序统一为 write→index (强制)

`SessionManager` 所有写路径**必须**遵循 `write_mutex_ → index_mutex_` 顺序。不得出现 `index_mutex_ → write_mutex_` 嵌套。

**SCENARIO R1.1**: 线程 A 调用 `open(session_id)`
- **GIVEN** SessionManager 实例 mgr + 临时 dir
- **WHEN** A 调 `mgr.open("s")`
- **THEN** 内部锁序列: `write_mutex_` (L61) → `index_mutex_` (L89)
- **AND** 锁按字典序 (`write_` < `index_`) 获取 → 无 inversion

**SCENARIO R1.2**: 线程 A 调用 `flush_append(node)`
- **WHEN** A 调 `mgr.flush_append(n)`
- **THEN** 内部锁序列: `write_mutex_` (L122) → `index_mutex_` (L178)

**SCENARIO R1.3**: 线程 A 调用 `migrate_legacy_json(path)`
- **WHEN** A 调 `mgr.migrate_legacy_json(p)`
- **THEN** 内部锁序列不出现 `index → write` 嵌套
- **AND** `flush_append_internal(branches_["main"])` 调用**不**在 `index_mutex_` 临界区内
- **AND** 通过 BranchMeta 锁内快照实现: 锁内 `main_meta = branches_["main"]`,锁外 `flush_append_internal(main_meta)`

### R2: `write_mutex_` 不可递归 (强制)

`write_mutex_` 为不可递归锁 (`std::mutex` 而非 `std::recursive_mutex`)。任何路径不得在持 `write_mutex_`(直接或间接)时再次进入取 `write_mutex_` 的代码路径。

**SCENARIO R2.1**: `open()` 公开签名不含 `legacy_path` 参数
- **GIVEN** SessionManager 头文件 `session_manager.h`
- **WHEN** grep `legacy_path` 在 `open` 声明附近
- **THEN** 返回 0 行

**SCENARIO R2.2**: 编译期断言防止 `legacy_path` 回归
- **GIVEN** 静态断言 `static_assert(!std::is_invocable_v<...>)`
- **WHEN** 任何人未来在 `open()` 声明加回 `legacy_path` 参数
- **THEN** 编译失败 + 错误信息含 "write→write recursion risk"

**SCENARIO R2.3**: 零调用方使用两参 `open()`
- **WHEN** grep `open(.*,.*legacy` 在 examples/ src/ tests/ pdk/
- **THEN** 返回 0 行 (除测试自身的 static_assert 注释)

### R3: 并发 migrate + flush_append 零死锁

**SCENARIO R3.1**: 两线程并发触发不同锁顺序
- **GIVEN** SessionManager mgr + legacy `.json` 路径已写入 3 条 messages
- **AND** 线程 A 调 `mgr.migrate_legacy_json(legacy_path)`(触发 `open() → write→index`, 末段 `index 快照 → write`)
- **AND** 线程 B 同步 `mgr.flush_append(new_node)`,重试直到 A 内部 `open()` 设了 `current_path_`
- **WHEN** 两线程并行执行 ≤ 2s
- **THEN** 两线程均在 ≤ 1s 内返回
- **AND** TSan 报告 `lock-order-inversion` warning **0** 条
- **AND** `mgr.list_all_nodes()` 返回 ≥ 4 条 (3 from legacy + ≥ 1 from thread B)

**SCENARIO R3.2**: 锁顺序断言 (单元级, 100 iter 压力)
- **GIVEN** TSan 模式下运行 R3.1 的混合场景 100 iter
- **WHEN** 每 iter 都执行 R3.1 (新建 SessionManager + 新 temp dir)
- **THEN** 100/100 iter 零死锁 + 零 race warning
- **AND** 累计 wall-clock ≤ 30s (单 iter ≤ 300ms)

## MODIFIED Requirements

### M1: SessionManager::migrate_legacy_json 末段 BranchMeta 锁内快照

**原始** (`src/core/session_manager.cpp:593-597`):
```cpp
// 写 main branch meta
{
    std::lock_guard<std::mutex> lock(index_mutex_);
    flush_append_internal(branches_["main"]);
}
```

**修改为**:
```cpp
// 写 main branch meta — BranchMeta 在 index_mutex_ 下快照, 锁外调 flush_append_internal
// 保持全局 write→index 锁序, 消除与 flush_append 的 ABBA 死锁; 与 fork() L349-355 同模式
BranchMeta main_meta;
{
    std::lock_guard<std::mutex> lock(index_mutex_);
    main_meta = branches_["main"];
}
flush_append_internal(main_meta);
```

**SCENARIO M1.1**: migrate 后 main branch meta 已持久化
- **WHEN** `migrate_legacy_json(p)` 完成
- **THEN** `<dir>/<session_id>.jsonl` 末尾包含 `{"type":"branch","branch_id":"main",...}` 行
- **AND** `branches_.count("main") == 1` (L566 锁块保留, 不变)
- **AND** `current_branch_ == "main"` (L575 锁块保留, 不变)

**SCENARIO M1.2**: migrate 与并发 flush_append 无死锁
- 复用 R3.1, 预期不变

### M2: SessionManager::open 删除 legacy_path 参数 (BREAKING API)

**原始** (`src/core/session_manager.h:157`):
```cpp
SessionHandle open(const std::string& session_id,
                   std::optional<std::string> legacy_path = std::nullopt);
```

**修改为**:
```cpp
SessionHandle open(const std::string& session_id);
```

**对应实现** (`src/core/session_manager.cpp:60-118`):
- 删除 L62-65 `legacy_path` 参数检查
- 删除 L80-87 legacy 分支(`if (!jsonl_existed && legacy_path...)` + 6 行)
- 函数体剩余部分 (创建 dir、jsonl 路径、初始化 `branches_["main"]`/`current_branch_`/`current_session_id_`/`current_path_`) 保持不变

**SCENARIO M2.1**: 一参 `open()` 行为不变
- **WHEN** `mgr.open("s")` (新会话, jsonl 不存在)
- **THEN** 创建 `<dir>/s.jsonl` + 初始化 main branch + 返回 `SessionHandle{session_id="s", jsonl_path=<dir>/s.jsonl}`

**SCENARIO M2.2**: 显式 `migrate_legacy_json` 仍可调
- **WHEN** 用户先 `mgr.open("s")` 然后 `mgr.migrate_legacy_json(legacy_path)`
- **THEN** 迁移成功(与原 `open("s", legacy_path)` 等价,但要求用户显式调两步)

**SCENARIO M2.3**: 零调用方依赖两参 `open()`
- **WHEN** 全库 grep 两参调用点
- **THEN** 0 行 — 不存在迁移成本

## REMOVED Requirements

### R-REMOVED-1: SessionManager::open 的 legacy_path 参数签名

**移除原因**: 构造性消除 `open(id, legacy_path) → migrate_legacy_json → open(id)` 的 write→write 递归(原潜伏自死锁), 改用显式两步调用。`migrate_legacy_json` 本身仍是公开 API, 行为不变。

## UNCHANGED Requirements

`SessionManager::open()` / `flush_append()` / `append_to_branch()` / `fork()` / `switch_branch()` / `compact()` / `build_context_entries()` / `get_branch_leaf()` / `list_all_nodes()` / `migrate_legacy_json()` **除上述 M1+M2 修改外的**其余语义**不变**。
