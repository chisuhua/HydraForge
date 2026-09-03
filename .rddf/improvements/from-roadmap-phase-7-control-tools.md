# from-roadmap-phase-7-control-tools

> **status: GATED by Phase 7a 不启动 (2026-09-02, 3/6 FAIL)** — 2026-09-02 cleanup.
> MCP tools schema 依赖 Phase 7a 启动.
> 复评时机: Sprint 25+ U4 完成后.
> 详见 `proposal-suggestions.md` §3.3.

**优先级**: P0 | **来源**: from-roadmap (phase-7/control-tools)
**阶段**: phase-7 | **分类**: control-tools
**类型**: feature
**主题**: tools/list；tools/call；inputSchema零转换

## 架构依据

ADR-0079 Control Plane 启动条件 `control-tools` 要求 MCP tools/* 暴露就绪：

- MCP 协议核心是 `tools/list` 与 `tools/call` 两方法，是客户端调用 server 能力的主路径。
- 现有 `IInteractionBus` + `IToolRegistry` 已形成完整工具执行栈，MCP 仅是协议适配层。
- ADR-0073 Tool JSON Schema 已 ship（Phase 6b 部分 ship / Phase 6c C8 提前 ship），inputSchema 是 MCP 协议层契约。
- "零转换"指 MCP JSON Schema 与 DECLARE_TOOL V3 自动生成的 inputSchema 字节级一致（不重写、不翻译）。

## 范围

- **In Scope**:
  - `pdk/mcp_stdio_server/` 注册 MCP `tools/list` 与 `tools/call` 方法。
  - `tools/list` 返回 `IInteractionBus::list_tools()` 的所有注册工具 + 各自 inputSchema（从 `IToolRegistry` 读）。
  - `tools/call` 接收 method=`tools/call` params={name, arguments}，经 ToolCoordinator.execute 路径调用（layer check + ApprovalHandler + adr-0069 hooks 全程生效）。
  - inputSchema 零转换：DECLARE_TOOL V3 自动生成的 JSON Schema 直接作为 MCP inputSchema 返回，禁止二次构造。
  - 4 类测试：tools/list 返回正确数 / tools/call 调用成功 / tools/call 参数校验失败（schema 不符） / ToolCoordinator 治理路径生效。
- **Out of Scope**:
  - resources/* / prompts/* 暴露（→ control-resources / control-prompts Phase 7b）。
  - tool output schema 强制（ADR-0073 D3 仍 defer）。
  - tool streaming 响应（依赖 MCP streaming extension，留独立 follow-up）。
  - external client 拉取外部 server 工具（→ control-client）。

## 关键场景

- GIVEN pdk_chat_demo 启动 + 注册 3 个工具（shell/exec, fs/read, fs/write）
  WHEN 外部 MCP client 发送 `tools/list`
  THEN 返回 3 个 tool description，每个含 name + description + inputSchema（与 DECLARE_TOOL 一致）。

- GIVEN 同上 + 外部 client 发送 `tools/call` name=`shell/exec` arguments=`{"cmd": "echo hi"}`
  WHEN MCP server 处理
  THEN 经 ToolCoordinator.execute 路径调用（layer check + ApprovalHandler + hooks），返回 `result.content`。

- GIVEN 外部 client 发送 `tools/call` arguments 不符 inputSchema
  WHEN server 校验
  THEN 返回 JSON-RPC error `-32602 Invalid params`（无调用进入 ToolCoordinator）。

- GIVEN `shell/*` 工具被 layer check 拒绝（来自 Cognitive 层）
  WHEN 外部调用
  THEN 返回 JSON-RPC error `-32002 Permission denied`，不暴露工具存在性。

## 技术约束

- MUST inputSchema 零转换（直接从 `IToolRegistry::get_tool_metadata(name).input_schema` 透传，禁止 JSON 重写）。
- MUST tools/call 全程经 ToolCoordinator 治理路径，禁止绕过 ApprovalHandler。
- MUST tools/list 输出不暴露 layer=internal 的工具（避免信息泄露）。
- MUST NOT 在 MCP 层重复实现 tool 注册逻辑（复用现有 IToolRegistry 单源）。
- MUST 工具调用结果以 MCP `content` 数组形式返回（text/image/resource），禁止自定义返回结构。

## 验收标准

- tools/list 返回 ≥ 3 个工具，inputSchema 字节级等于 DECLARE_TOOL 生成。
- tools/call 调用成功路径 + 失败路径（参数错误 / layer 拒绝）测试通过。
- ToolCoordinator 治理路径验证（layer check + ApprovalHandler 触发计数）。
- ctest 全量零回归。
- 阻塞 phase-7 启动评估 `control-tools` 项 ✅。