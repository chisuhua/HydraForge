# from-roadmap-phase-7-control-client

> **status: GATED by Phase 7a + 7b (2026-09-02, 3/6 FAIL)** — 2026-09-02 cleanup.
> Phase 7b 本身 gated by Phase 7a 不启动. 外部 MCP client 依赖 7a 启动.
> 复评时机: 7a 完成后才评估 7b.
> 详见 `proposal-suggestions.md` §3.3.

**优先级**: P1 | **来源**: from-roadmap (phase-7/control-client) ⏸ Phase 7b gated
**阶段**: phase-7 | **分类**: control-client
**类型**: feature
**主题**: 外部MCP client；backend policy

## 架构依据

ADR-0079 Control Plane `control-client` 项是 Phase 7b 启动后解锁：

- 外部 MCP client 模式让 HydraForge 主动拉取外部 MCP server 的工具，扩展能力边界。
- backend policy 强制让外部 client 拉取的 tool 必须经过 ToolCoordinator 治理路径（layer check + ApprovalHandler + hooks）。
- pi-agent 借鉴路径 §九 P0.4：双向通信是 Control Plane 完整形态。

## 范围

- **In Scope**:
  - `pdk_chat_demo` `--mcp-client-servers <json>` flag（外部 server 列表 + 各自 stdio path）。
  - 启动时连接每个 server，握手 + tools/list 拉取工具注册到 `IToolRegistry`（带 `mcp_external/` 前缀避免命名冲突）。
  - 工具调用通过 MCP client 转发（写 JSON-RPC 到外部 server stdin，读响应）。
  - backend policy：所有 mcp_external/* 工具调用经 ToolCoordinator 治理路径，不允许绕过（与本地工具完全等价）。
  - 4 类测试：连接外部 server / 拉取工具 / 调用转发 / backend policy 强制。
- **Out of Scope**:
  - HTTP+SSE 外部 server 拉取（→ control-http-sse Phase 7c descoped）。
  - 多 server 负载均衡（无需求）。
  - 工具同步冲突解决（命名空间前缀避免冲突）。

## 关键场景

- GIVEN pdk_chat_demo 启动 + 外部 MCP server 提供 `external/grep`
  WHEN 启动参数含 server 配置
  THEN 外部工具 `mcp_external/external__grep` 注册到 IToolRegistry，tools/list 可见。

- GIVEN `mcp_external/external__grep` 已注册
  WHEN 内部 LLM 决定调用
  THEN 经 ToolCoordinator.execute → MCP client → 外部 server → 返回结果，全程 ApprovalHandler + hooks 生效。

- GIVEN 外部 server 断开（EOF / timeout）
  WHEN 工具调用发起
  THEN 返回 JSON-RPC error `-32005 Connection lost`，IToolRegistry 标记工具不可用。

- GIVEN backend policy 配置 `deny_layers=[Cognitive]`
  WHEN `mcp_external/*` 调用尝试进入 Cognitive 层
  THEN ToolCoordinator 拒绝（layer check），返回 Permission denied。

## 技术约束

- MUST 外部工具命名空间前缀 `mcp_external/` + server 名（如 `mcp_external/external__grep`）。
- MUST 外部 client 调用全程经 ToolCoordinator，禁止 IToolRegistry 直接转发。
- MUST 外部 server 连接超时 + 重试策略文档化（默认 5s 连接 + 30s 调用）。
- MUST 外部 server 断开时 tools/list 同步移除（避免调用悬挂）。
- MUST NOT 暴露外部 server 内部细节给 host 进程（仅透传工具结果）。
- MUST NOT 允许外部 server 通过 tools/list 修改本机 IToolRegistry（只读拉取）。

## 验收标准

- 外部 server 连接 + 工具拉取测试通过。
- 工具调用转发 + 治理路径生效测试通过。
- backend policy 强制（layer check 拒绝路径）测试通过。
- 外部 server 断开恢复测试通过（reconnect + tools/list 同步）。
- ctest 全量零回归。
- 阻塞 phase-7b 启动评估 `control-client` 项 ✅。\n\n## Status: ⏸ GATED by Phase 7a + 7b (2026-09-02, 3/6 FAIL)\n\n**Status 段补全** (per Pattern #10 hygiene, 2026-10-15 audit verification):\n\n**5-维判定**:\n- 维度 1 self-declared: header `> **status: GATED by Phase 7a + 7b (2026-09-02, 3/6 FAIL)**` 已标注\n- 维度 2 archive: ❌ (Phase 7 不启动, 无 OpenSpec change archive)\n- 维度 3 commit: ✅ commit `911d19f` 2026-09-03 (mark Phase 6b SUPERSEDED batch) + 相关 Phase 7 governance commits\n- 维度 4 ADR: 关联 ADR-0079 Control Plane 评估 (per 6c-control-plane-eval 提案)\n- 维度 5 fallback: Phase 7a 不启动决策 (per `from-roadmap-phase-6c-control-plane-eval` 🟡 PARTIAL ship C2 模式实证)\n\n**判定依据**:\n- **Phase 7a 不启动**: per 2026-09-02 cleanup 决议, 6 项启动条件中 3 项 FAIL (Solo Dev 容量 <80h 双周 + 仅 1 AgentForge agent ship + AgentForge 治理节奏未达标)\n- **Phase 7b 阻塞**: 依赖 Phase 7a CLII 端实施 + ClientLoop 实验 ship + 同 SDK\n- **本提案阻塞**: `control-client` 是 Phase 7b 启动条件之一, 当前 gating 状态\n\n**判定结论**: ⏸ GATED (Phase 7a 不启动 → Phase 7b 阻塞 → 本提案不能 ship, 保留作 Phase 7+ 启动时设计意图参考)\n\n**Pattern 沉淀**: Phase 7 7 个 control-* 提案 (client / http-sse / prompts / resources / stdio / token / tools) 头注已 GATED (per commit `911d19f` 2026-09-03 批量标记), 但**缺标准 ## Status 段** (本日 hygiene 模式 5 字段格式). 本 commit 补全标准化 Status 段.