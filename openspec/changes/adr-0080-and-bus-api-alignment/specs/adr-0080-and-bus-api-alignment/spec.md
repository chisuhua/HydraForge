## ADDED Requirements

### Requirement: ADR-0080 主题规范收敛至 ADR-0068 单一事实源 (MUST apply)

本条 MUST 满足：ADR-0080 不再维护自己的主题表，改为引用 ADR-0068 附录 A 作为 Canonical Topic Registry；`tool.execution.start/end` owner 归属以代码实发射点（ToolCoordinator）为准。

#### Scenario: D9 主题表被移除
- **WHEN** 阅读 ADR-0080 §决策 D9
- **THEN** 不再存在独立维护的主题表，而是引用 ADR-0068 附录 A 与 ADR-0079 D6
- **AND** 表内不再声称 `tool.execution.start/end` owner 为 NodeExecutor
- **AND** 若有保留的"参考主题"清单，须标注"以下主题当前无生产发射方（机制缺口），发射归口后回填 ADR-0068 附录 A"

#### Scenario: 单一事实源不冲突
- **WHEN** 交叉检查 ADR-0080 与 ADR-0068
- **THEN** 两种文档对 `tool.execution.start/end` 的 owner 描述一致（ToolCoordinator）
- **AND** ADR-0080 不含 ADR-0068 未登记的主题规范（除非显式标注为机制缺口/待回填）

### Requirement: ADR-0080 JSONL 示例与 schema 一致 (MUST apply)

本条 MUST 满足：ADR-0080 附录 A 与 D2 schema 示例的 payload 字段、字段名、单位必须与代码实证一致。

#### Scenario: tool.execution.* 示例不含 args 值
- **WHEN** 阅读 ADR-0080 附录 A JSONL 示例中的 tool.execution.start/end
- **THEN** payload 含 `{tool, layer, ok, duration_ms}`（+ 可选 error_code），不含任何 args 值
- **AND** 与 D10.6"不落 args 值——ADR-0031 audit 防线"表述一致

#### Scenario: 时间戳字段名为 ts_wall
- **WHEN** 阅读 ADR-0080 D2 schema 与附录 A 示例
- **THEN** 字段名为 `ts_wall`（非 `ts`），单位毫秒（非秒），并含必填 `causal_time` 字段
- **AND** 与 `event_log.cpp` 实际序列化字段一致

### Requirement: ADR-0068 subscribe_glob 措辞对齐实际 API (MUST apply)

本条 MUST 满足：ADR-0068/ADR-0019 文档中不出现不存在的 API 名 `subscribe_glob` / `subscribe_topic`（除非标注为未来规划）；glob 能力表述为 `subscribe()` 的通配符支持。

#### Scenario: 文档命名与代码 API 一致
- **WHEN** grep `subscribe_glob` 于 docs/adr/adr-0019-*.md 与 docs/adr/adr-0068-*.md
- **THEN** 0 命中（除"subscribe() 通配符支持"的正确表述）

#### Scenario: ADR-0019 状态注记更新
- **WHEN** 阅读 ADR-0019 §状态
- **THEN** 注记反映"glob 通配符订阅已 ship（subscribe() 支持 * / ?，InMemoryBus Change B 2026-07-27）；独立 subscribe_topic API 零消费者 DEFER"

### Requirement: ADR-0030 悬空 ADR-0025 引用清理 (MUST apply)

本条 MUST 满足：ADR-0030 V2 不得引用不存在的 ADR-0025 文件。

#### Scenario: 死链被替换
- **WHEN** 阅读 adr-0030-async-runtime-v2.md §参考
- **THEN** 不再存在指向 `./adr-0025-parallel-subtasks.md` 的 markdown 链接
- **AND** 预留编号 ADR-0025 以行内注记说明（FleetOrchestrator DEFER，见 gap-analysis §2.1）

### Requirement: ADR-0068 附录 A 版本与计数自洽 (MUST apply)

本条 MUST 满足：ADR-0068 附录 A 的标题版本号、头部状态行的 amendment 记录、表格内行标注三者一致；主题计数反映当前实际。

#### Scenario: 版本号统一
- **WHEN** 阅读 ADR-0068 附录 A 标题
- **THEN** 版本号为最新（v2.0, 2026-08-31），与头部状态行末次 amendment 一致
- **AND** 表格内 amendment 标注不超出版本号

#### Scenario: 主题计数可复现
- **WHEN** 阅读 ADR-0068 §决策 2 与转 Approved 条件
- **THEN** "22 个"表述为"v1 初始 22 个（当前 N 个）"，N 可用 `grep -c "^| \`" docs/adr/adr-0068-event-emission-contract.md` 复现

### Requirement: ADR-0068 附录 A 补登 llm.token 三主题 (MUST apply)

本条 MUST 满足：`llm.token` / `llm.token.done` / `llm.token.error`（stream_to_bus 真实发射）登记进 ADR-0068 附录 A。

#### Scenario: 真实发射主题已登记
- **WHEN** grep `llm.token` 于 ADR-0068 附录 A
- **THEN** ≥ 3 行（llm.token / llm.token.done / llm.token.error，owner=stream_to_bus）

### Requirement: ADR-0080 ↔ v1.2 amendment 双向引用 (MUST apply)

本条 MUST 满足：ADR-0080 主文档 D10.3 反映 v1.2 amendment 的解耦语义（CaptureMode 三态 + Training fail-open），并包含指向 amendment 文件的显式指针。

#### Scenario: 主文档引用 amendment
- **WHEN** 阅读 ADR-0080 D10.3 或 grep "v1.2 amendment" 于 docs/adr/adr-0080-append-only-event-log.md
- **THEN** ≥ 1 处交叉引用 `adr-0080-v1-2-amendment-d10-decouple.md`
- **AND** D10.3 语义为"Online 模式依赖 scrub hook、Training 模式 fail-open"，不再是 v1.1 的"未 ship 时不暴露"单一路径

### Requirement: ADR-0079 头部版本记录同步 (MUST apply)

本条 MUST 满足：ADR-0079 头部修订记录反映正文已有的 v1.2 amendment（D7-D10，2026-08-20）。

#### Scenario: 头部含 v1.2 记录
- **WHEN** 阅读 ADR-0079 §状态/修订记录
- **THEN** 含 "v1.2 amendment 2026-08-20" 记录，且与正文 D7-D10 标注一致

### Requirement: ADR-0080 文档结构清理 (MUST apply)

本条 MUST 满足：ADR-0080 不含重复章节；附录 C 计数与 ADR-0079 D6 / session_writer.cpp 一致。

#### Scenario: 不变量章节唯一
- **WHEN** grep "^## 不变量" 于 docs/adr/adr-0080-append-only-event-log.md
- **THEN** 恰好 1 处

#### Scenario: 会话主题计数一致
- **WHEN** 阅读 ADR-0080 附录 C"会话结构事件"
- **THEN** 计数为 13（与 ADR-0079 D6 / session_writer.cpp whitelist 一致），或注明"15 含 step.*/execution.* 通配前缀"