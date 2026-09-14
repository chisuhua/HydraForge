# session-manager-lock-order Spec Deltas

**Capability**: `session-manager-lock-order` (新增)
**关联审计**: `docs/superpowers/plans/2026-09-11-chat-session-pdk-lift-change2.md` L779-783 (TSan #151/#191 pre-existing lock-order-inversion)
**关联代码**: `src/core/session_manager.h` + `src/core/session_manager.cpp`
**TSan 基线**: 2026-09-15 实测 build-tsan (`04f8351` + `4d2c97a` 后 #191 翻绿)

## ADDED Requirements

### Requirement: lock-order-write-then-index

`SessionManager` 所有写路径 MUST 遵循 `write_mutex_ → index_mutex_` 顺序。任何路径 MUST NOT 出现 `index_mutex_ → write_mutex_` 嵌套。

`migrate_legacy_json` 末段写 main branch meta 时 MUST 采用锁内快照模式: 在 `index_mutex_` 下拷贝 `BranchMeta`, 锁外调 `flush_append_internal` (仅取 `write_mutex_`)。该模式与 `fork()` (L349-355) 已验证模式一致。

#### Scenario: open() 锁序

- **WHEN** 调用 `mgr.open("s")`
- **THEN** 内部锁序为 `write_mutex_` (L61) → `index_mutex_` (L89)
- **AND** 不出现反向嵌套

#### Scenario: flush_append() 锁序

- **WHEN** 调用 `mgr.flush_append(node)`
- **THEN** 内部锁序为 `write_mutex_` (L122) → `index_mutex_` (L178)

#### Scenario: migrate_legacy_json() 锁序

- **WHEN** 调用 `mgr.migrate_legacy_json(legacy_path)`
- **THEN** 内部锁序不出现 `index → write` 嵌套
- **AND** `flush_append_internal` 调用点在 `index_mutex_` 临界区之外
- **AND** `branches_["main"]` 通过锁内快照传递, 不在锁外求值

#### Scenario: TSan 报告 #191 翻绿

- **GIVEN** `build-tsan` 构建 (cmake --preset tsan)
- **WHEN** 运行 `ctest -R '^test_session_manager_legacy$'`
- **THEN** PASS 且 `ThreadSanitizer` 报告 0 条 lock-order-inversion warning

### Requirement: write-mutex-non-recursive

`write_mutex_` 为不可递归锁 (`std::mutex`)。任何路径 MUST NOT 在持 `write_mutex_`(直接或间接)时再次进入取 `write_mutex_` 的代码路径。`open()` MUST NOT 接受 `legacy_path` 参数 —— 旧版 `open(id, legacy_path) → migrate_legacy_json → open(id)` 构成 write→write 递归自死锁。

#### Scenario: open() 签名不含 legacy_path

- **WHEN** grep `legacy_path` 在 `session_manager.h` 的 `open` 声明
- **THEN** 返回 0 行
- **AND** `open()` 签名为 `SessionHandle open(const std::string& session_id)`

#### Scenario: 编译期断言防止回归

- **WHEN** 任何人未来在 `open()` 声明加回 `legacy_path` 参数
- **THEN** `tests/test_session_manager_lock_order.cpp` 的 `static_assert` 编译失败
- **AND** 错误信息含 "write->write recursion risk"

#### Scenario: 零调用方依赖两参 open()

- **WHEN** grep `open(.*,.*legacy` 在 examples/ src/ tests/ pdk/
- **THEN** 返回 0 行 (除测试自身的 static_assert)

### Requirement: concurrent-migrate-flush-append-no-deadlock

并发 `migrate_legacy_json` + `flush_append` MUST 不产生死锁或 TSan lock-order-inversion 报告。

#### Scenario: 两线程并发 migrate + flush_append

- **GIVEN** SessionManager mgr + legacy `.json` 含 3 条 messages
- **WHEN** 线程 A 调 `migrate_legacy_json(legacy_path)` 且线程 B 用 bounded retry-until-open 调 `flush_append(node)`
- **THEN** 两线程均在 ≤ 5s 内返回
- **AND** TSan 报告 `lock-order-inversion` warning 0 条
- **AND** `mgr.list_all_nodes()` 返回 ≥ 4 条 (3 legacy + ≥1 thread B)

#### Scenario: 压力循环锁序断言

- **GIVEN** TSan 模式运行上述混合场景 ≥5 iter
- **WHEN** 每 iter 新建 SessionManager + temp dir
- **THEN** 全部 iter 完成无死锁 + 零 race warning
- **AND** 单 iter ≤ 300ms (functional) / ≤ 2s (TSan)

#### Scenario: JSONL 内容完整性

- **WHEN** 单线程 `migrate_legacy_json` (3 messages) 完成后 `flush_append` 追加 2 节点
- **THEN** `list_all_nodes()` 返回 5 条
- **AND** JSONL 文件行数为 6 (3 legacy node + 1 branch meta + 2 extra node)

## MODIFIED Requirements

### Requirement: migrate-legacy-json-branch-meta-snapshot

`migrate_legacy_json` 末段写 main branch meta MUST 在 `index_mutex_` 下先快照 `BranchMeta`, 再在锁外调 `flush_append_internal(main_meta)`。MUST NOT 在持 `index_mutex_` 时调 `flush_append_internal` (后者取 `write_mutex_`, 构成 index→write 倒置)。

#### Scenario: migrate 后 main branch meta 已持久化

- **WHEN** `migrate_legacy_json(p)` 完成
- **THEN** `<dir>/<session_id>.jsonl` 含 `{"type":"branch","branch_id":"main",...}` 行
- **AND** `branches_.count("main") == 1`
- **AND** `current_branch_ == "main"`

### Requirement: open-single-arg-signature

`SessionManager::open` MUST 接受单参 `open(const std::string& session_id)`。旧版 `legacy_path` 参数已删除 — 迁移由调用方显式两步完成 (`open()` 后调 `migrate_legacy_json()`)。`migrate_legacy_json` 保持公开 API 不变。

#### Scenario: 一参 open() 行为不变

- **WHEN** `mgr.open("s")` (jsonl 不存在)
- **THEN** 创建 `<dir>/s.jsonl` + 初始化 main branch
- **AND** 返回 `SessionHandle{session_id="s", jsonl_path=<dir>/s.jsonl}`

#### Scenario: 显式 migrate 仍可用

- **WHEN** 用户 `mgr.open("s")` 后调 `mgr.migrate_legacy_json(legacy_path)`
- **THEN** 迁移成功, 等价于旧两参行为
- **AND** `.backup` 文件创建于 legacy_path 同目录

## REMOVED Requirements

### Requirement: open-legacy-path-parameter

`open()` 的 `legacy_path` 参数已移除 —— 构造性消除 `open(id, legacy_path) → migrate_legacy_json → open(id)` 的 write→write 递归自死锁路径。零调用方使用该参数 (2026-09-15 grep 验证), 无迁移成本。
