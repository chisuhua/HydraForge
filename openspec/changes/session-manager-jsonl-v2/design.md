## Context

第一轮 `session-manager-jsonl`（archived `2026-08-04-session-manager-jsonl/`，commit `9310cfb` 完成归档 + commit `155fafb` ship 核心 C++ 实现）已完成 90% 工作量：
- ✅ `SessionManager` C++ 类（`src/core/session_manager.{h,cpp}` 共 842 行）已 ship，含 13 个 public 方法。
- ✅ 24 个 test cases / 1092 assertions（`tests/test_session_manager.cpp`）ctest 116/118 PASS（2 pre-existing failures 与本 change 无关）。
- ✅ 内存索引（`nodes_/branches_/children_`）+ `write_mutex_` 写盘保护 + 旧格式 C++ 迁移实现（`migrate_legacy_json`）。
- ❌ **`session.persisted` 事件发射未真正实现** — `set_bus(IInteractionBus)` setter 与 `bus_` 字段已声明，但 `flush_append()` 未调用 `bus_->emit(...)`（v1 commit message 误声称已 ship，代码与文档不符）。
- ❌ **`session_manager.cpp` 未接入 `agenticdsl_core` 生产库** — `tests/CMakeLists.txt` 仍用 v1 Task 1.11 + 9 临时绕过把 `session_manager.cpp` 拉入 2 个 test target，stash@{0} 保留此绕过 + 扩展（`test_session_manager_legacy` 加入 bypass）。
- 🟠 **v1 in-progress 资源尚未合并** — stash@{0} 保留 `tools/migrate_session_json.py`（CLI）+ `tests/test_session_manager_legacy.cpp`（4 cases）+ CMake 扩展。

v1 archive `2026-08-04-session-manager-jsonl/design.md` 5 个 Decision 全部 ship，本 v2 不重写决策，仅 v1 Decision 4（`session.persisted` 发射）的"实施"环节——v1 写了 spec 但没写实现代码。本 v2 收尾实现。

`session.persisted` 事件发射依赖 ADR-0068 EventBuilder（`include/agenticdsl/contract/event_builder.h`，已在 adr-0068-event-emission-contract ship Wave 2 V2 落地）+ `IInteractionBus`（`include/agenticdsl/contract/iinteraction_bus.h`，ADR-0030 V2 ship）。两个依赖项均已 ship，v2 仅需按既有契约调用。

`agenticdsl_core` 静态库当前不包含 `src/core/session_manager.cpp`（仅 2 个 test target 通过 bypass 拉入）。v2 接入后下游 PDK plugin / examples / 集成测试可直接 link 到 `agenticdsl_core` 而不需各自 bypass。

## Goals / Non-Goals

**Goals:**
- 在 `SessionManager::flush_append` 内 fsync 成功 + close 之后、index 更新之前，按 ADR-0068 §决策 5 EventBuilder 链式调用 `bus_->emit(...)` 发射 `session.persisted` 事件。
- 失败路径（write/fsync 抛 `std::system_error`）不发射 `session.persisted`，错误传播给调用方。
- `bus_ == nullptr` 时跳过 emit 不抛异常（保证单测可注入 null bus 跑通）。
- `src/core/session_manager.cpp` 接入 `agenticdsl_core` 生产库（移除 v1 临时 bypass）。
- 从 stash@{0} 恢复 `tools/migrate_session_json.py` + `tests/test_session_manager_legacy.cpp` + CMake 扩展。
- 新建 `tests/test_session_persisted_event.cpp`（6 cases 覆盖 4 场景：成功发射 / 失败不发射 / null bus 跳过 / payload 字段完整）。
- 更新 ADR-0068 附录 A owner 字段 + ADR-0033 实施范围说明 + `AGENTS.md` CODE MAP。
- 维持 ctest 全量零回归（v1 baseline 116/118 PASS + v2 新增 6 cases + stash 恢复 4 cases）。

**Non-Goals:**
- 不修改 `SessionManager` 公开 API 签名（v1 已 ship 24 cases 验证契约稳定）。
- 不重写 v1 已 ship 的 fork / branch / compact / build_context_entries / migrate_legacy_json 任何行为。
- 不实施 `session.before.switch` / `session.before.fork` / `session.before.compact` 三个主题的发射（需独立提案先行在 ADR-0068 附录 A 注册）。
- 不重写 v1 archive design.md 的 5 个 Decision（append-only / parent pointer / migration / persisted emission 限定 / 不动 pdk_session_agent）。
- 不修改 v1 ship 的 `tests/test_session_manager.cpp`（24 cases 保持原样）。

## Decisions

### Decision 1: emit 时机 — fsync 成功 + close 之后、index 更新之前

**Rationale**:
- ADR-0068 §决策 4 要求"在 `flush_append` 成功返回**前**发射"，保证订阅方在调用方观察到 `flush_append` 返回时已收到事件。
- 顺序：write → fsync → close → emit → index update。
  - **write/fsync 失败**抛 `std::system_error`，不进入 emit 分支（防御性）。
  - **emit 失败**（EventBuilder.build() 抛异常或 bus 内部异常）按 ADR-0068 §决策 7 operation-result vs telemetry 分类：telemetry 失败不传播（订阅方异常不应阻塞核心存储），但当前 EventBuilder V2 API 不抛异常（仅在 `meta` 字段非法时 warn），故不需 try-catch。
  - **index update** 在 emit 之后，因 index 是 in-memory cache 而非持久化数据，订阅方事件不依赖 index 状态。
- 与 v1 archive `2026-08-04-session-manager-jsonl/design.md` Decision 4（仅发射 `session.persisted`）保持一致——v2 落地该 decision 的实施部分。

**Alternatives Considered**:
- **emit 在 write 之后、fsync 之前**：fsync 失败时订阅方收到"已持久化"假信号，违反 ADR-0068 §决策 4 契约。
- **emit 在 index update 之后**：订阅方可能观察到"事件已发但 in-memory 索引尚未更新"的瞬态不一致（极小窗口，但理论上违反 read-your-write 假设）。
- **emit 通过独立后台线程批量处理**：引入额外线程与延迟，违背 `flush_append` 同步语义（订阅方假设同步收到事件）。

### Decision 2: payload 字段顺序与类型严格遵循 ADR-0068 附录 A

**Rationale**:
- 4 字段：`session_id`（string）/ `node_id`（string）/ `branch_id`（string）/ `timestamp`（uint64 unix 毫秒）。
- `session_id` 取当前 session 的 basename（`<dir>/<session_id>.jsonl` → `<session_id>`，不含 `.jsonl` 后缀）。
- `node_id` 取 `node.id`（`next_node_id()` 输出）。
- `branch_id` 取 `node.branch_id`（`append_to_branch` 设置的当前 branch）。
- `timestamp` 取 `std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count()`。
- EventBuilder 链式：`.args({{"session_id",...},{"node_id",...},{"branch_id",...},{"timestamp",...}}).build()`。

**Alternatives Considered**:
- **payload 用 `nlohmann::json` 对象而非 `.args(map)`**：EventBuilder V2 提供 `.args(json)` 接受 vector<pair> 与 `.meta(json)` 区分业务字段与 trace 上下文；v2 业务字段用 `.args`，无 trace 上下文时不调 `.meta`。
- **timestamp 用 ISO 8601 字符串而非 unix 毫秒数字**：订阅方在 EventHandler 内可读性更好但需每次解析；unix 毫秒数字更易聚合（监控/分析）。选 unix 毫秒（与 v1 `BranchMeta::created_at` 风格统一）。

### Decision 3: `bus_ == nullptr` 跳过 emit，不抛异常

**Rationale**:
- 单测 `test_session_manager_legacy.cpp`（v1 Task 6.8-6.12，stash 恢复）不注入 bus（v1 设计为 nullptr 默认）；v1 24 cases 在 bus_ == nullptr 下必须不抛异常。
- 跳过的实现：`if (bus_) { bus_->emit(...); }`，10 行内嵌于 `flush_append` index 更新前。
- 与 v1 archive `2026-08-04-session-manager-jsonl/design.md` Decision 4 一致（v1 当时未实际发射，本 decision 落地"不发射时为何不抛异常"契约）。

**Alternatives Considered**:
- **bus 必填（构造时强制注入）**：破坏 v1 ship 的 `set_bus` 可选性，且要求所有单测构造 SessionManager 时必须 mock bus。
- **`std::optional<shared_ptr<IInteractionBus>>`**：增加类型复杂度，无实际收益（nullptr 与 optional 等价）。

### Decision 4: `session_manager.cpp` 接入 `agenticdsl_core` 而非新增 library

**Rationale**:
- `SessionManager` 是 core 组件（`src/core/`），按项目约定属于 `agenticdsl_core` 静态库。
- 接入方法：根 `CMakeLists.txt` 中 `agenticdsl_core` target_sources 追加 `src/core/session_manager.cpp`（或 `src/core/CMakeLists.txt` 子模块注册）。
- 接入后 `tests/CMakeLists.txt` 移除 v1 临时 bypass（`session_manager.cpp` 已在 `agenticdsl_core` 内，test target 自动 link）。
- 接入后下游 PDK plugin / examples / 集成测试直接 link `agenticdsl_core` 即可使用 SessionManager，无需各自 bypass。

**Alternatives Considered**:
- **新增 `agenticdsl_core_session` 静态库**：与 ADR-0019 §1.4 跨模块 include 治理方向相悖（保持 core 模块内聚）。
- **保留 v1 临时 bypass + 文档化**：长期债务，下游每次新增 test target 须记得加 bypass 行；接入生产库消除此债务。

## Risks / Trade-offs

### Risk 1: emit 失败影响 flush_append 返回

**Mitigation**:
- EventBuilder V2 API 当前 `.args(json).build()` 在合法输入下不抛异常（仅非法 `meta` 时 warn）。
- 若未来 EventBuilder API 变更引入异常路径，须在 emit 调用外加 try-catch 并 log，不传播给 `flush_append` 调用方（telemetry 失败不阻塞核心存储路径）。
- 单测 `test_session_persisted_event.cpp` 覆盖"EventBuilder 构造异常时不传播"场景（v2 防御性测试）。

### Risk 2: agenticdsl_core link 体积增长

**Mitigation**:
- `session_manager.cpp` 565 行编译后 ~50KB object size；`agenticdsl_core` 静态库当前 12.87MB，增量 < 0.5% 可忽略。
- 增量影响下游 link 性能微乎其微（modern linker 优化）。

### Risk 3: stash 恢复的 v1 代码与 v1 ship 的 `migrate_legacy_json` C++ 实现可能不一致

**Mitigation**:
- stash@{0} 保留的 `migrate_session_json.py` 是 v1 Task 4.4 独立 Python CLI，调用约定与 C++ `migrate_legacy_json` 共享：旧格式 `{"messages": [...]}` → JSONL 树 + 根节点 + main branch + .backup 保留。
- stash@{0} 保留的 `test_session_manager_legacy.cpp` 测试 `migrate_legacy_json` 的 C++ 实现（不是 Python CLI），v2 pop 后 4 cases 直接验证 C++ 实现行为。
- v2 ship gate 须运行 `cmake --build build --target test_session_manager test_session_manager_legacy test_session_persisted_event && ctest --output-on-failure` 验证 3 个 test target 全 PASS。

### Risk 4: 移除 v1 临时 bypass 后下游 link 失败

**Mitigation**:
- v1 临时 bypass 把 `session_manager.cpp` 拉到 `test_session_manager` + `test_session_build_context` 两个 test target。
- v2 接入 `agenticdsl_core` 后，2 个 test target 通过 link `agenticdsl_core` 自动获得 `session_manager.cpp`（因 `add_catch_test` 函数已 link `agenticdsl_core`）。
- v2 ship gate 须 ctest 全量通过验证 link 正确。

### Trade-off 1: emit 调用点增加 ~10 行代码 vs ADR-0068 契约完整性

**Trade-off**: `flush_append` 增加 `if (bus_) { ... emit ... }` 分支 10 行。
**Decision**: 接受（v1 声明了 `set_bus` 与 `bus_` 但未实际使用，构成 dead code 风险；v2 落地使用消除 dead code）。

### Trade-off 2: agenticdsl_core 编译时间增加 ~0.5s vs 消除下游 bypass 债务

**Trade-off**: `session_manager.cpp` 接入 `agenticdsl_core` 每次 build 多编译 1 个 TU。
**Decision**: 接受（增量 < 0.5s，下游 6+ test target / 2 PDK plugin 各自 bypass 节省 > 3s 累计）。
