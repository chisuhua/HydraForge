# session-manager-jsonl

**优先级**: P1 | **来源**: layer-based-missing-capabilities-analysis.md §四 L0-1 + §五 L1-3 + §六 L2-2（三项捆绑，同一存储层）
**阶段**: wave-2 | **分类**: core-impl
**类型**: feature

## 架构依据
- ADR-0033 三层 Session 执行模型已 ship，但**存储层仍是线性 JSON**（`chat_session.cpp::save_to_disk` 单文件原子写），无 JSONL 树状持久化、无 open/fork/branch/compact API——`/tree` `/fork` `/clone` 借鉴路径（§三 P0.1）的核心阻塞。
- pi-agent `SessionManager`（`buildContextEntries` 从叶子到根遍历）为借鉴蓝本。
- L1-3：`session_before_switch` / `session_before_fork` / `session_before_compact` / `session.persisted` 生命周期事件无发射载体，须随 SessionManager 一并落地（订阅已在 EventHandler，发射按 ADR-0068 附录 A）。
- L2-2：`pdk/session_agent/` 当前仅复用 chat_session 内联实现，未独立为通用 plugin。

## 范围
- **In Scope**: 新建 `src/core/session_manager.{h,cpp}`（JSONL 树状存储：每消息一条记录 + parent 指针 + branch 元数据）；open/fork/branch/compact API；叶子到根上下文重建；旧线性 JSON 迁移工具；4 个 session 生命周期事件发射；`pdk/session_agent/` 改为委托 SessionManager。
- **Out Scope**: `/tree` `/fork` TUI（session-tree-tui 提案）；ContextCompactor LLM 摘要（context-compactor 提案）；分布式会话（无需求）。

## 关键场景
- GIVEN 一个多轮会话，WHEN 调用 `fork(node_id)`，THEN 产生新 branch，JSONL 追加 branch 记录，且 `session_before_fork` 事件先于写盘发射。
- GIVEN 旧格式线性 JSON 会话文件，WHEN 迁移工具执行，THEN 转为 JSONL 树且上下文重建结果与旧格式等价。
- GIVEN fork 后的两个 branch，WHEN 分别追加消息，THEN 互不可见，`buildContextEntries` 各自从叶子到正确根路径。

## 技术约束
- MUST 存储格式追加-only（JSONL append），禁止全量重写（崩溃安全）。
- MUST 旧格式迁移工具随本提案交付，禁止丢弃用户历史会话。
- MUST 事件发射遵循 ADR-0068（字段按附录 A，EventBuilder 若已落地则复用）。
- MUST NOT 在本提案实现 TUI 渲染（归 session-tree-tui）。

## 验收标准
- fork/branch/compact API 单测覆盖（含并发追加）。
- 旧格式迁移等价性测试通过；4 个生命周期事件真实发射测试通过。
- `pdk/session_agent/` 不再包含 chat_session 内联实现的复制代码。
- ctest 全量零回归；`python3 tools/docs_drift_audit.py` 0 DRIFT。
