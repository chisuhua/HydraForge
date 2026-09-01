# Design: adr-0080-and-bus-api-alignment

## Context

三方独立审查确认事件/订阅契约链存在 13 项文档问题（9 项 Sisyphus/Oracle 共同发现 + 4 项 Metis 补充），分布在 5 个 ADR 文件。所有问题均属**纯文档勘误**（不改任何行为契约或代码）。核心是建立 ADR-0068 附录 A 作为 Canonical Topic Registry 的**单一事实源**地位，清理 ADR-0080/0079/0019/0030 与之冲突或越权维护的内容。

## Scope Boundaries

### 范围 IN

#### A. ADR-0080 修改（主要工作量）

- **§决策 D9 主题表（L221-247）整体替换为引用**：删除自建主题表，改为"主题规范以 ADR-0068 附录 A 为准；EventLog 以 `bus_->subscribe("*")` 全量订阅，SessionWriter 的会话子集主题见 ADR-0079 D6"。如保留偶发主题列表，须标注"以下主题当前无生产发射方（机制缺口），发射归口后回填 ADR-0068 附录 A"
- **§附录 A JSONL 示例（L455-459）**：
  - `tool.execution.start` payload：`{"tool":"file.write", "args":{"path":"/tmp/test.py"}}` → `{"tool":"file.write", "layer":"workflow", "ok":true, "duration_ms":42}`（去 args，补 layer/ok/duration_ms）
  - `tool.execution.end` payload：`{"tool":"file.write", "ok":true}` → `{"tool":"file.write", "layer":"workflow", "ok":true, "duration_ms":42}`（补 layer/duration_ms）
  - 5 个示例字段 `ts` → `ts_wall`，秒值 → 毫秒值（如 `1737281400` → `1737281400123`），并补 `causal_time` 必填字段
- **§决策 D10.3 跨文件 amendment 同步**：原"开启后依赖 ADR-0081 pre-step hook 在 emit 前 scrub——未 ship 时不暴露" → 改为"Online 模式依赖、Training 模式 fail-open（已由 `adr-0080-v1-2-amendment-d10-decouple.md` v1.2 修订，引入 CaptureMode 三态 + Training fail-open 三重保护）"。在同段加"另见 v1.2 amendment"内联指针
- **重复的 `## 不变量` 章节（L370-378 与 L380-387）**：合并为一组（保留含 D10.5 capture-off 默认的 7 条版本）
- **§附录 C 计数 L480**："15 个 topic" → "13 个 topic"（与 ADR-0079 D6 / `session_writer.cpp:30-47` whitelist 一致）

#### B. ADR-0068 修改

- **§状态行/§背景 L25**：`BusEvent 信封 + InMemoryBus MPMC + subscribe_glob + CausalClock 均已 ship` → `BusEvent 信封 + InMemoryBus MPMC + subscribe() 通配符支持 + CausalClock 均已 ship`
- **§决策 1 表格 L64**：`emit/subscribe/subscribe_glob/CausalClock` → `emit/subscribe (通配符支持)/CausalClock`
- **§附录 A 标题 L174**：`(v1.6, 2026-08-28)` → `(v2.0, 2026-08-31)`，对齐头部状态行与表格末行
- **§决策 2 L80** 与 **转 Approved 条件 L151**："去重后 22 个 / Registry 22 个主题登记完成" → "v1 初始 22 个（截至 Appendix A v2.0 共 N 个，可复现命令：`grep -c "^| \`" docs/adr/adr-0068-event-emission-contract.md`）"
- **§附录 A 补登 3 行**（A4 反向缺口）：
  - `llm.token` | stream_to_bus | 流式 token | `session_id`, `token`, `index` | ✅
  - `llm.token.done` | stream_to_bus | 流式完成 | `total_tokens` | ✅
  - `llm.token.error` | stream_to_bus | 流式错误 | `error_code`, `message` | ✅

#### C. ADR-0030 修改

- **§参考 L325**：`[ADR-0025: 并行子任务](./adr-0025-parallel-subtasks.md) — Fleet 协议依据` → 行内注记 `- Fleet 并行执行：预留编号 ADR-0025（未创建）；FleetOrchestrator 已 DEFER，见 docs/architecture/adr-implementation-status-gap-analysis.md §2.1 ADR-0030 行`

#### D. ADR-0079 修改

- **§状态 L4** 修订记录追加："v1.2 amendment 2026-08-20：§决策 D7-D10（node-id 稳定寻址 / branch cursor 持久化 / path-extraction fork / 4 套存储命名空间分配）"

#### E. ADR-0019 修改

- **§状态 L5** 注记更新："🟡 Partial (2026-07-06 更新 — glob 通配符订阅已 ship（`subscribe()` 支持 `*`/`?` 通配符，InMemoryBus Change B 2026-07-27）；带 layer 检查的独立 `subscribe_topic` API 当前零消费者，DEFER)"

### 范围 OUT

- **6 个幻影主题的机制修复**（attempt.* / conversation.* / phase.completed / branch.created / attempt.converged 有消费者但零发射方）—— 需代码改动（loop_agent/ChatSession/PlanExecuteLoop 等的 emit 调用点），属独立 OpenSpec change；本文档勘误只做"标注定性"
- 任何 IInteractionBus / EventBuilder / ToolResult 公开 API 改动
- 任何新增事件主题（仅补登已有的 `llm.token` 三主题）
- 任何测试断言改动

## Design Decisions

### D1 — ADR-0080 D9 表整体替换为引用（不保留内容）

**理由**：
- ADR-0068 §决策 1 边界条款已明文禁止两份 ADR 各自维护主题规范
- D9 表 6 主题（attempt.*/conversation.*/phase.completed/branch.created/execution/convergence）有消费者（SessionWriter）但零发射方，**保留**会让 EventLog 实现者按错误指引写代码
- 整段替换为引用比"逐行修正 owner + 标 ⏳ pending"更清晰，避免读者误以为表内主题可用

**反向论据**：若保留为"实施参考"，能降低 EventLog ship 时的工作量——但工作量下降幅度小（几小时文档查找），错误风险大（按错误 owner 写代码）

**裁决**：整段替换为引用

### D2 — A1（ADR-0080 ↔ v1.2 amendment 脱节）必须主文档+amendment 双向指针

**理由**：
- v1.2 amendment 引入 CaptureMode 三态 + Training fail-open 是 ADR-0080 设计意图的关键演进
- 主文档 D10.3 仍写"依赖 scrub hook 才暴露"是**已过时**的 v1.1 表述，与 amendment 矛盾
- Oracle 已识别 D10 被 ADR-0081→0082 Proposed 链锁住，amendment 的目的就是解锁
- 蒸馏实现者（如 `pdk-chat-demo-distill-source-survey-2026-08.md` 提到的 SessionWriter JSONL 过渡数据源）若按主文档原文写代码，会回到"未 ship 时不暴露"死锁

**修复细节**：D10.3 段尾加 "→ 已由 v1.2 amendment（`adr-0080-v1-2-amendment-d10-decouple.md`，2026-08-25 评审通过）修订：Online 模式依赖 scrub hook，Training 模式 fail-open（三重保护）"

### D3 — A4（`llm.token` 反向缺口）补登 3 行而非删除 ADR-0080 D9 中已有行

**理由**：
- `stream_to_bus.cpp` + `stream_to_bus.h:44-48` 实证发射 3 主题
- ADR-0080 D9 旧表 L225-227 已列 3 行（但该表整体将被替换为引用）
- ADR-0068 附录 A 0 处登记
- 补登 3 行到 ADR-0068 附录 A，与"22 → N 个"计数一并更新

**反方论据**：若保持"ADR-0080 D9 整体替换"决策，则 ADR-0080 不再列 `llm.token`——但 ADR-0068 必须有（这是 Canonical Registry 职责）

**裁决**：在 change ① 中，A4 的修复是**仅** ADR-0068 附录 A 补登 3 行（与"22 → N"计数更新一起）

### D4 — A2/A3（版本号/计数）使用实测可复现命令

**理由**：
- 附录 A 真实行数易随 amendment 漂移
- 文档中嵌入可复现命令 `grep -c "^| \`" docs/adr/adr-0068-event-emission-contract.md` 让下次同步无需重数
- 版本号统一为 (v2.0, 2026-08-31) 与头部状态行末次 amendment 日期对齐

### D5 — A6（ADR-0079 v1.1 vs v1.2）头部补记录，不改正文

**理由**：
- 正文 D7-D10 已正确标注 v1.2
- 仅头部修订记录未同步（同类 ADR-0068 A2 问题）
- 修复最小：头部 1 行加 v1.2 记录

## Risks

| 风险 | 概率 | 影响 | 缓解 |
|---|---|---|---|
| **A1 改错方向**（把 D10.3 改得更糟，破坏 v1.2 amendment 解耦意图）| 低 | 高 | D1 明确"Online 依赖、Training fail-open"语义；改后由人 review；amendment 文件原文（`adr-0080-v1-2-amendment-d10-decouple.md`）作权威源 |
| **A2 漏掉 v2.0 后新增的 amendment**（如未来 v2.1）| 中 | 低 | 文档嵌入可复现命令，每次同步都重数 |
| **A4 补登的 payload schema 与 stream_to_bus 实际不符** | 低 | 中 | 实读 `stream_to_bus.h:44-48` 与 `stream_to_bus.cpp` 当前 emit 调用点后定字段；如代码有更新需同步 |
| **删 D9 表后 EventLog 实施者找不到参考** | 中 | 低 | 改为引用 ADR-0068 附录 A + ADR-0079 D6，两个权威源均易查 |
| **修复后 0068/0080 仍漏"6 主题机制缺口"提醒** | 高 | 中 | 在 0080 替换段落显式标注"以下主题当前无生产发射方（机制缺口），发射归口后回填 ADR-0068 附录 A" |
| `adr_lint.py` 数量类校验失败 | 中 | 中 | ship 前预检查 grep 命令与版本号自洽 |
| `docs_drift_audit.py` Scenario 4 触发 | 低 | 中 | 实读 `event_log.cpp:183` 确认 ts_wall 字段后再改 |

## Verification Gates

- ✅ `python3 tools/adr_lint.py` exit 0 且**新增 ERROR = 0**（已有 20+ ADR-TRACKING-01 warning 允许保留）
- ✅ `python3 tools/docs_drift_audit.py` 0 DRIFT（Scenario 4/6 重点确认）
- ✅ `openspec validate adr-0080-and-bus-api-alignment --strict` PASS
- ✅ `grep -c "^| \`" docs/adr/adr-0068-event-emission-contract.md` 与附录 A 标题版本号 + §决策 2 计数自洽
- ✅ ADR-0080 主文档 ↔ v1.2 amendment 双向引用存在（`grep -c "v1.2 amendment" docs/adr/adr-0080-append-only-event-log.md` ≥ 1）
- ✅ `grep "subscribe_glob" docs/adr/adr-00{19,68}*.md` 0 命中（命名错位已修复）
- ✅ `grep "ADR-0025" docs/adr/adr-0030-async-runtime-v2.md` 引用形式为"行内注记"（无独立 `[ADR-0025:...](./adr-0025...)` 链接）
- ✅ ADR-0068 附录 A 含 `llm.token` / `llm.token.done` / `llm.token.error` 3 行（A4 修复）
- ✅ ADR-0080 `## 不变量` 章节计数 = 1（原 L370-378 + L380-387 重复已合并）
- ✅ ADR-0079 §状态 修订记录含 v1.2 amendment 行

## Dependencies

### 不依赖代码（纯文档）
- 无需前置 ship
- 无需测试修改

### 需遵守（项目自身规范）
- ADR-0068 §决策 1 边界条款（两份 ADR 禁止各自维护主题规范）
- ADR-0068 §决策 5 additivity-only 兼容政策（仅修字段不删字段）
- `docs/architecture/README.md` 文档头四字段规范（生成日期/最后验证/作者/状态）
- `tools/adr_lint.py` ADR-TRACKING-01 规则（已 Approved 24h+ 无 tracking change 不视为 ERROR，仅 WARNING）

## Out of Scope (V2 / 其他 change)

- **6 幻影主题机制修复**：attempt.*/conversation.*/phase.completed/branch.created/attempt.converged 有消费者（SessionWriter）零发射方；修复需在 `pdk/loop_agent/` `pdk/plan_execute_loop` `pdk/fork_join_loop` `examples/pdk_chat_demo/chat_session` 等位置加 emit 调用点，属独立 OpenSpec change（含代码改动）
- **ADR-0019 状态翻 Approved**：glob 已 ship 后 ADR-0019 是否可从 🟡 Partial → ✅ Approved，是独立判定
- **ADR-0080 v1.3 同步**：本 change 同步到 v2.0；如未来出 v1.3 amendment（CaptureMode V3）需独立 change
- **`session_writer.cpp` 13 topic whitelist → ADR-0068 附录 A 全量注册**：当前 13 主题中含多个零发射方（attempt.*/conversation.*/phase.completed/branch.created/attempt.converged），与"6 主题机制缺口"重叠，应在同一后续 change 处理

## Success Criteria

- 5 个 ADR 文件按上述 D1-D5 决策完成修改
- 上述 6 条 verification gate 全部通过
- 0 改 0 回归（纯文档）
- OpenSpec archive 完成
