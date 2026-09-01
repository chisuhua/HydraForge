# adr-0080-and-bus-api-alignment

## Why

三方独立审查（Sisyphus grep 实证 + Oracle codegraph/Read 逐项裁决 + Metis 语义通读复核）发现事件/订阅契约链（ADR-0068 ↔ ADR-0019 ↔ ADR-0080 ↔ ADR-0079）存在**文档内容层面**的实质性矛盾，分为三类：

**A 类：ADR 自相矛盾 / ADR 间主题规范冲突（P0）**
1. **ADR-0080 D9 主题表 vs ADR-0068 附录 A 冲突**：`tool.execution.start/end` 实际 emit 点仅在 `src/common/tools/tool_coordinator.cpp:573-576,708-714`（代码注释"ADR-0068 §决策 3"），而 ADR-0080 D9 表标注 owner=NodeExecutor（`node_executor.cpp` 全文件 0 命中）。ADR-0068 §决策 1 明文"两份 ADR 禁止各自维护对方域内的主题规范（单一事实源原则）"——ADR-0080 D9 既是越权维护又是错误归属。
2. **ADR-0080 附录 A JSONL 示例含 `args` 值**（`{"path":"/tmp/test.py"}`）vs D10.6 明文"tool.execution.start/end payload 仅含 `{tool, layer, ok, duration_ms}`，不落 args 值——ADR-0031 audit 防线"。代码 `emit_tool_execution_event()`（tool_coordinator.cpp:83-102）实证无 args 值透传。
3. **ADR-0080 示例字段 `ts` vs D2 schema `ts_wall`**：schema v1.1 定义 `ts_wall` 并"移除 ts"，但 L63 与附录 A L455-459 示例仍写 `"ts"`，且示例值是秒、代码 `event_log.cpp:183` 输出毫秒。
4. **ADR-0030 V2 引用不存在的 ADR-0025**（`adr-0030-async-runtime-v2.md:325`），编号 0024-0028 为预留范围，FleetOrchestrator 已 DEFER。

**B 类：文档声称 vs 代码命名错位（P1）**
5. **ADR-0068 声称 `subscribe_glob` 已 ship**，但代码接口只有 `subscribe(event_type, callback)`（iinteraction_bus.h:51-53），glob 机制以 `inmemory_bus.cpp:78-89` 内部 `has_wildcard()` 分类实现，无独立 API。ADR-0019 L5"待 ship subscribe_topic"注记过时。

**C 类：Metis 补充的 ADR 家族级矛盾（P0/P1/P2）**
- **A1 (P0)**：ADR-0080 主文档 D10.3 与自身 v1.2 amendment（`adr-0080-v1-2-amendment-d10-decouple.md`）正文脱节——主文档仍写"依赖 ADR-0081 scrub hook 才暴露"，而 amendment 的整个目的就是解耦该死锁（CaptureMode 三态 + Training fail-open），且主文档 0 处引用该 amendment 文件。
- **A2 (P1)**：ADR-0068 附录 A 版本号自相矛盾（附录标题 v1.6 / 头部状态 v1.7 / 表格末行 v2.0）。
- **A3 (P1)**：ADR-0068 §决策 2 与转 Approved 条件写"22 个主题"，实际附录 A 已 63 行，从未随 amendment 同步。
- **A4 (P1)**：`llm.token`/`llm.token.done`/`llm.token.error` 三主题真实生产发射（stream_to_bus.cpp）但未登记进 ADR-0068 附录 A（反向缺口）。
- **A5 (P2)**：ADR-0080 D9 中 `execution`/`convergence` 无 `<module>.<verb>` 分隔符，违反同表自述命名约定。
- **A6 (P1)**：ADR-0079 头部声明 v1.1（08-12）但正文 D7-D10 已标注 v1.2 amendment（08-20），头部版本未更新。
- **A7 (P2)**：ADR-0080 附录 C "15 个 topic" vs ADR-0079 D6 / `session_writer.cpp:30-47` 实际 13 个。
- **A8 (P2)**：ADR-0080 重复 `## 不变量` 章节（L370-378 与 L380-387）。

## What Changes

**核心原则**：真相源排序 = 代码 emit 点 → ADR-0068 附录 A → 其余一切。ADR-0080/0019/0030 全部改为**单向引用** ADR-0068 附录 A，不维护自己的主题规范。

1. **ADR-0080**（主要修改对象）：
   - 删除 D9 主题表（L221-247）→ 改为引用 ADR-0068 附录 A + 注明 SessionWriter 会话子集主题见 ADR-0079
   - 修正附录 A JSONL 示例（L455-459）：去 `args`、补 `layer`/`duration_ms`、`ts`→`ts_wall`、秒→毫秒
   - D10.3 加 amendment 指向横幅 + 语义修订（Online 依赖 scrub、Training fail-open）
   - 合并重复 `## 不变量` 章节（保留含 D10.5 的 7 条版本）
   - 附录 C "15 个"→"13 个" 统一
2. **ADR-0068**：`subscribe_glob` 措辞改 `subscribe() 通配符支持`（L25/L64）；附录 A 标题统一版本号；"22 个"计数更新为 v1 初始 + 当前 N 个；附录 A 补登 `llm.token` 三主题
3. **ADR-0030**：L325 死链改行内注记（预留编号 + FleetOrchestrator DEFER）
4. **ADR-0079**：头部补 v1.2 amendment 记录
5. **ADR-0019**：L5 状态注记改为"glob 已 ship，subscribe_topic DEFER"

## Capabilities

### New Capabilities

- `event-topic-single-source`: 事件主题规范收敛到 ADR-0068 附录 A 单一事实源，ADR-0080/0079 只引用不维护

### Modified Capabilities

无（本 change 是纯文档勘误，不改任何行为契约；事件 topic 语义未变，仅文档表述对齐）

## Impact

- **文档**：`docs/adr/adr-0080-append-only-event-log.md`（主文档 + 附录）、`docs/adr/adr-0068-event-emission-contract.md`、`docs/adr/adr-0030-async-runtime-v2.md`、`docs/adr/adr-0079-unified-session-4scope.md`、`docs/adr/adr-0019-iinteraction-bus-mvp.md`，共 5 个 ADR 文件
- **代码**：零改动（纯文档勘误）
- **测试**：零改动（不改任何断言）
- **验证工具**：`adr_lint.py` / `docs_drift_audit.py` / `openspec validate` 须全部通过
- **不覆盖**：6 个幻影主题（attempt.*/conversation.*/phase.completed/branch.created/attempt.converged）的**机制缺口**（有消费者、无发射方）——那是需要代码改动的独立 change（见 `docs-analysis-status-snapshot-sync` 的 out-of-scope 说明），不属于本纯文档勘误批次
