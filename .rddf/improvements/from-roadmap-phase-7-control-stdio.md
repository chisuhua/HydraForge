# from-roadmap-phase-7-control-stdio

**优先级**: P0 | **来源**: from-roadmap (phase-7/control-stdio)
**阶段**: phase-7 | **分类**: control-stdio
**类型**: feature
**主题**: JSON-RPC 2.0；stdio transport

## 架构依据

ADR-0079 (Control Plane / MCP) 在 Phase 7 启动条件满足后激活。本提案是 phase-7/control-stdio 分类的实施载体：

- MCP（Model Context Protocol）stdio transport 是 pi-agent 借鉴路径 §三 Phase 7 Control Plane 的入口。
- 当前 `pdk_chat_demo` 仅支持进程内 loop_agent，外部 MCP 客户端/服务端双向通信未实现。
- JSON-RPC 2.0 是 MCP 协议层基础（与 LSP 同源），stdio transport 是 MCP 0 默认传输（最高优先级）。
- 与现有 `IInteractionBus` 完全解耦：MCP 是外部边界抽象，Bus 是进程内事件抽象。

## 范围

- **In Scope**:
  - `include/agenticdsl/contract/imcp_transport.h` L3 契约：stdio transport interface (read_frame / write_frame / poll events)
  - `include/agenticdsl/contract/ijsonrpc.h` JSON-RPC 2.0 最小实现（request/response/notification/error 编码与解析）
  - `src/modules/control/mcp_stdio_transport.cpp` stdio transport（fork + pipe + JSON-RPC 帧）
  - `pdk/mcp_stdio_server/` plugin：1 个演示用 stdio MCP server（暴露 `mcp/echo` + `mcp/time`）
  - JSON-RPC 2.0 framing 测试（错误帧、超大帧、UTF-8）
  - stdio transport 父子进程 fork 测试 + EOF 检测测试
  - 与现有 `IInteractionBus` 解耦测试（同一消息经 MCP 转 Bus 0 副作用）
- **Out of Scope**:
  - HTTP+SSE transport（→ control-http-sse Phase 7c descoped）
  - 鉴权（→ control-token）
  - tools/list / tools/call 实际暴露（→ control-tools）
  - resources/* prompts/*（→ control-resources / control-prompts Phase 7b）
  - MCP 客户端拉取外部 server 工具（→ control-client）

## 关键场景

- GIVEN pdk_chat_demo 启动 MCP stdio server（plugin 加载）
  WHEN 外部进程 stdin 写入合法 JSON-RPC 2.0 `initialize` 请求
  THEN server 在 stdout 返回 `result.capabilities` 与 protocolVersion，握手完成。

- GIVEN 握手后外部进程发送 `tools/call`（method）
  WHEN server 解析请求
  THEN 经 ToolCoordinator.call_tool 路径执行，返回 JSON-RPC 2.0 `result` 或 `error`。

- GIVEN 父子进程 stdin/stdout 关闭
  WHEN EOF 事件触发
  THEN server 走有序清理路径（无 SIGSEGV，依赖 fix-tool-registry-signal-handler-shutdown ship 模式）。

- GIVEN 非法 JSON 输入或超大帧
  WHEN transport 解析
  THEN 返回 JSON-RPC `ParseError`(-32700) 不进入 MCP handler。

## 技术约束

- MUST MCP stdio transport 使用 pipe + 双进程（fork + execve），禁止单进程直连外部 stdin（stdin 冲突）。
- MUST JSON-RPC 2.0 framing 严格遵循规范（Content-Length header + UTF-8 body + \r\n 分隔）。
- MUST 与 `IInteractionBus` 解耦：MCP 消息不进 bus，bus 事件不进 MCP（独立传输层）。
- MUST fork 后进程顺序：MCP server fork → 子进程初始化 → 子进程 exec，避免 Init 状态泄漏到父进程。
- MUST ASan / TSan 全量零回归（pipe 双进程交互无 data race）。
- MUST NOT 引入第二套事件总线或第二套 tool 执行路径。

## 验收标准

- 1 个 MCP stdio server plugin ship（含 2 个演示工具）。
- JSON-RPC 2.0 framing 单元测试通过（合法帧 / 非法帧 / 超大帧 / UTF-8 边界）。
- stdio transport 父子进程 fork + EOF 测试通过。
- 与 IInteractionBus 解耦测试通过（消息不交叉）。
- ctest 全量零回归（pre-existing 失败保持不变）。
- 阻塞 phase-7 启动评估 `control-stdio` 项 ✅（4 项启动条件之一）。