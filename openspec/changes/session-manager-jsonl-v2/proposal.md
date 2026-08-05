## Why

第一轮 `session-manager-jsonl`（archived `2026-08-04-session-manager-jsonl/`，commit `9310cfb`）已 ship 完整 `SessionManager` C++ 类（`src/core/session_manager.{h,cpp}` 共 842 行 + `tests/test_session_manager.cpp` 24 cases / 1092 assertions，commit `155fafb`），覆盖 `open / flush_append / load_jsonl / fork / switch_branch / append_to_branch / compact / build_context_entries / migrate_legacy_json` 全部公开 API + 内存索引（`nodes_/branches_/children_`）+ `write_mutex_` 写盘保护 + 旧格式迁移 C++ 实现。

**但 v1 留有以下 3 个未完成事项，本 change (v2) 接管收尾：**

1. **`session.persisted` 事件发射未真正实现** — `SessionManager::set_bus(IInteractionBus)` setter 与 `bus_` 字段已声明，但 `flush_append()` 实际**未调用** `bus_->emit(...)`（v1 commit message 误声称已实现，代码与文档不符）。这导致 ADR-0068 附录 A 唯一已注册会话生命周期主题 `session.persisted` 在 EventHandler 订阅端就位后仍无任何发射，订阅方永不触发。
2. **v1 临时绕过的 `tests/CMakeLists.txt` bypass 仍生效** — v1 Task 1.11 + 9 因 session_manager.cpp 未接入 `agenticdsl_core` 而把 `src/core/session_manager.cpp` 拉入 2 个 test target 直接编译；遗留物在 stash@{0}（含 `test_session_manager_legacy` 扩展的 CMake 改动）。v2 须将 `session_manager.cpp` 接入 `agenticdsl_core` 生产库，移除 test target bypass，避免生产库与测试代码二元来源。
3. **v1 in-progress 文件尚未合并** — stash@{0} 保留的 `tools/migrate_session_json.py`（独立 Python CLI 迁移工具）+ `tests/test_session_manager_legacy.cpp`（4 cases 迁移等价性测试）+ `tests/CMakeLists.txt` bypass 扩展，是 v1 提交时未合并的 work-in-progress 资源。v2 worktree 启动后须 `git stash pop` 恢复并 ship。

本 change (v2) 收尾这三项，目标是让 v1 已 ship 的 SessionManager 形成完整闭环——`flush_append` 真实发射 `session.persisted` 事件 + 生产库正式接入 + 迁移 CLI + 迁移测试全到位。

## What Changes

- **修改** `src/core/session_manager.cpp`：`flush_append()` 在 `::fsync(fd)` 成功 + `::close(fd)` 之后、index 更新之前，按 ADR-0068 §决策 5 EventBuilder 链式调用 `bus_->emit(EventBuilder("session.persisted", ToolResult).args({{"session_id",...},{"node_id",...},{"branch_id",...},{"timestamp",<unix_ms>}}).build())`。`bus_ == nullptr` 时跳过 emit（不抛异常，保证单测无 bus 可跑）。失败路径（write/fsync 抛 `std::system_error`）不 emit，错误传播给调用方。
- **新增** `tests/test_session_persisted_event.cpp`：覆盖 `session.persisted` 在 `flush_append` 成功前发射 + payload 4 字段（session_id / node_id / branch_id / timestamp）+ 失败路径不发射（mock write 失败注入）+ `bus_ == nullptr` 跳过 emit 不抛异常。
- **修改** `CMakeLists.txt` + `src/CMakeLists.txt`（或对应模块 CMakeLists）：将 `src/core/session_manager.cpp` 接入 `agenticdsl_core` 生产库。
- **修改** `tests/CMakeLists.txt`：移除 v1 Task 1.11 + 9 的临时绕过（`session_manager.cpp` 已在 `agenticdsl_core` 内，test target 不再单独拉）。
- **新增** `tools/migrate_session_json.py`（从 stash@{0} 恢复，v1 Task 4.4 独立 Python CLI 迁移工具，204 行）。
- **新增** `tests/test_session_manager_legacy.cpp`（从 stash@{0} 恢复，v1 Task 6.8-6.12 迁移等价性测试，241 行 / 4 cases）。
- **更新** `AGENTS.md` CODE MAP：`SessionManager` 关键符号标注"事件发射已 ship"（与现有 CODE MAP 一致，ship 后追加 note）。
- **更新** ADR-0068 附录 A：`session.persisted` owner 字段从 `ChatSession / session_agent` 改为 `SessionManager`（v2 ship 后唯一发射方）。
- **更新** ADR-0033 实施范围说明：v2 收尾后，存储层 "v2 完整 ship（事件发射 + 生产库集成）" 注记。
- **不修改** `SessionManager` 公开 API 签名（v1 已 ship 24 test cases 验证契约稳定）。
- **不修改** v1 已 ship 的 fork / branch / compact / build_context_entries / migrate_legacy_json 任何行为。
- **不实施** `session.before.switch` / `session.before.fork` / `session.before.compact` 三个主题的发射（需独立提案先行在 ADR-0068 附录 A 注册）。

## Capabilities

### New Capabilities
- `session-persisted-emission`：`SessionManager::flush_append` 成功路径通过 EventBuilder 构造并发射 `session.persisted` 事件，payload 4 字段遵循 ADR-0068 附录 A；失败路径不发射；`bus_ == nullptr` 跳过 emit 不抛异常。

### Modified Capabilities
- `jsonl-session-storage`（delta spec）：v2 补充 REQUIREMENT `persist-emits-session-persisted-on-flush-success`（v1 规范描述了 `session.persisted` 行为但 v1 未真正实现；v2 落地实际发射），`persist-skips-emit-on-flush-failure`（v1 规范要求失败路径不发射，v2 实际保证），`persist-skips-emit-when-bus-null`（v2 新增契约：单测可注入 null bus）。

## Impact

- **生产代码**:
  - `src/core/session_manager.cpp`（v1 已有；v2 在 `flush_append` 内追加 `bus_->emit` 调用 ~10 行）
  - `CMakeLists.txt`（v2 接入 `session_manager.cpp` 到 `agenticdsl_core` 库 + ~3 行）
  - `src/core/CMakeLists.txt`（若存在；v2 注册 `session_manager.cpp` 源文件 + 1 行）
- **测试代码**:
  - `tests/test_session_persisted_event.cpp`（新，~150 行 / 6 cases 覆盖 4 场景）
  - `tests/test_session_manager_legacy.cpp`（从 stash 恢复，241 行 / 4 cases）
  - `tests/CMakeLists.txt`（移除 v1 临时绕过 -8 行；恢复 stash 中的扩展 +2 行）
- **工具**:
  - `tools/migrate_session_json.py`（从 stash 恢复，204 行 CLI）
- **API 兼容性**:
  - `SessionManager` 公开 API 签名零修改（v1 24 cases 验证）。
  - `set_bus(IInteractionBus)` 签名不变（v1 已 ship，v2 仅实际使用）。
  - `agenticdsl_core` 静态库新增 `session_manager.cpp` 编译产物（不破坏下游 link）。
- **依赖**:
  - 复用 `include/agenticdsl/contract/event_builder.h`（ADR-0068 Wave 2 V2 EventBuilder 全 payload 构造器，Ship V2 commit 已落地）。
  - 复用 `IInteractionBus`（`include/agenticdsl/contract/iinteraction_bus.h`，ADR-0030 V2 已 ship）。
- **文档**:
  - `AGENTS.md` CODE MAP 追加 `SessionManager` "事件发射已 ship" note。
  - ADR-0068 附录 A owner 字段更新为 `SessionManager`。
  - ADR-0033 实施范围说明追加 v2 收尾 note。
- **风险**:
  - 低 - `flush_append` 内部追加 emit 调用，不改写公开 API 签名；行为仅在成功路径追加 emit，失败路径行为不变。
  - 低 - `agenticdsl_core` 静态库增加 1 个编译单元，下游 link 影响极小。
  - 中 - stash 恢复的 `migrate_session_json.py` 与 `test_session_manager_legacy.cpp` 是 v1 完成的代码，v2 须验证与 v1 ship 的 `migrate_legacy_json` C++ 实现行为一致。
