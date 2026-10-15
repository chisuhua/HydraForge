# from-roadmap-phase-7-control-http-sse

> **status: GATED by Phase 7c descoped (2026-09-02)** — 2026-09-02 cleanup.
> Phase 7c 已被 descoped per Phase 6c 评估. 不复评, 保留作历史.
> 详见 `proposal-suggestions.md` §3.3.

**优先级**: P2 | **来源**: from-roadmap (phase-7/control-http-sse) ⏸ Phase 7c descoped
**阶段**: phase-7 | **分类**: control-http-sse
**类型**: feature
**主题**: HTTP+SSE；SSE transport

## 架构依据

ADR-0079 Control Plane `control-http-sse` 项 Phase 7c descoped（远期 long-tail）：

- HTTP+SSE 是 MCP 协议的次选 transport（与 stdio 平行），适合 Web 客户端 / 长连接场景。
- httplib 已 vendored 到 external/（pdk_chat_demo --mock HTTP health check 验证）。
- 当前 Phase 7a stdio transport 已 ship，HTTP+SSE 是补充而非前置。

## 范围

- **In Scope**:
  - `include/agenticdsl/contract/imcp_transport.h` 增加 HTTP+SSE transport 实现。
  - `pdk/mcp_stdio_server/` 注册 MCP HTTP+SSE 端点（httplib server + SSE 流）。
  - 鉴权复用 control-token 的 Bearer header。
  - 长连接 keep-alive + heartbeat（SSE comment 帧）。
  - 3 类测试：HTTP POST 工具调用 / SSE 流推送 / 鉴权 header 校验。
- **Out of Scope**:
  - WebSocket transport（MCP 0 未支持，留独立 follow-up）。
  - HTTPS / TLS（生产环境建议 reverse proxy 承担）。
  - 大规模并发（暂不优化，stdio 为主）。

## 关键场景

- GIVEN pdk_chat_demo 启动 + HTTP+SSE transport 监听 8080
  WHEN 浏览器 fetch `POST /mcp/tools/list`
  THEN 返回 JSON-RPC 2.0 响应 + SSE 流事件。

- GIVEN 长连接 SSE 已建立
  WHEN server 主动推送 `notifications/tools/list_changed`
  THEN client 收到 SSE `event: message` 帧（不需 reconnect）。

- GIVEN HTTP 端点启用鉴权
  WHEN 客户端未带 `Authorization: Bearer <token>`
  THEN 返回 401 + JSON-RPC error `-32001`。

## 技术约束

- MUST HTTP+SSE transport 与 stdio transport 共享 JSON-RPC 2.0 实现层（不重复编码）。
- MUST SSE 心跳 30s 一次（防 NAT timeout）。
- MUST HTTP 端口可配置（默认 8080，避免 privileged port）。
- MUST POST 请求体 Content-Type 固定 `application/json`（MCP 规范）。
- MUST NOT 在本期引入 HTTPS（依赖 reverse proxy）。
- MUST 阻塞于 control-stdio Phase 7a ship（实现复用）。

## 验收标准

- HTTP+SSE transport E2E 测试通过（POST + SSE 流 + 鉴权）。
- 与 stdio transport 行为一致性测试通过（同一请求两 transport 返回相同结果）。
- 心跳机制验证（30s interval）。
- ctest 全量零回归。
- 阻塞 phase-7c descoped 项，Phase 8+ 启动评估参考。\n\n## Status: ⛔ DESCOPED by Phase 7c descoped (2026-09-02)\n\n**Status 段补全** (per Pattern #10 hygiene):\n\n**5-维判定**:\n- 维度 1: header `> **status: GATED by Phase 7c descoped (2026-09-02)**` 已标注 (注: 本提案不同其他 6 个, 是 descoped 而非 GATED)\n- 维度 2: 无 archive (Phase 7c descoped, 无 change)\n- 维度 3: 911d19f 命中\n- 维度 4: 关联 ADR-0079\n- 维度 5: phase-7c 显式 descoped 决议\n\n**判定结论**: ⛔ DESCOPED (Phase 7c 整体 descoped, 本提案不再 ship, 保留作历史设计意图记录)\n\n**Pattern 沉淀**: Phase 7 7 个 control-* 提案头注 GATED/DESCOPED + ## Status 段标准化批量补全中. 本提案是 7 个中唯一的 ⛔ DESCOPED (其他 6 个是 ⏸ GATED).