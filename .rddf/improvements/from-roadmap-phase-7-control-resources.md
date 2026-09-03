# from-roadmap-phase-7-control-resources

> **status: GATED by Phase 7a 不启动 (2026-09-02, 3/6 FAIL)** — 2026-09-02 cleanup.
> MCP resources schema 依赖 Phase 7a 启动.
> 复评时机: Sprint 25+ U4 完成后.
> 详见 `proposal-suggestions.md` §3.3.

**优先级**: P1 | **来源**: from-roadmap (phase-7/control-resources) ⏸ Phase 7b gated
**阶段**: phase-7 | **分类**: control-resources
**类型**: feature
**主题**: MCP resources；stdlib subgraph URI

## 架构依据

ADR-0079 Control Plane `control-resources` 项是 Phase 7b 启动后解锁：

- MCP `resources/*` 暴露将 HydraForge 内部 lib/* stdlib subgraphs 转换为可寻址 URI 资源。
- pi-agent 借鉴路径 §八 P1.2：resources 是 prompts 的并行能力（模板 vs 数据）。
- lib/auth/*, lib/human/*, lib/math/*, lib/utils/*, lib/inference/* 共 12 个 stdlib subgraphs 可暴露。

## 范围

- **In Scope**:
  - `pdk/mcp_stdio_server/` 注册 MCP `resources/list` + `resources/read` 方法。
  - `resources/list` 返回所有 stdlib subgraphs（URI 格式 `hydraforge://lib/{category}/{name}`）。
  - `resources/read` 返回指定 subgraph 的 YAML 内容 + 解析后的节点列表。
  - URI scheme 注册到 MCP server capabilities。
  - 3 类测试：resources/list 数 ≥ 12 / resources/read 内容字节一致 / URI 解析错误返回。
- **Out of Scope**:
  - 用户自定义 resources 注册（无需求）。
  - resources/subscribe（变化通知，无需求）。
  - 写资源（resources/write，禁止）。

## 关键场景

- GIVEN pdk_chat_demo 启动 + 12 个 stdlib subgraphs 已 ship
  WHEN 外部 MCP client 发送 `resources/list`
  THEN 返回 12 个 resources（含 URI + name + description + mimeType）。

- GIVEN 外部 client 发送 `resources/read` uri=`hydraforge://lib/inference/engine`
  WHEN server 处理
  THEN 返回 YAML 内容 + 节点列表（与 lib/inference/engine.md 字节一致）。

- GIVEN 不存在的 URI `hydraforge://lib/inference/nonexistent`
  WHEN resources/read 调用
  THEN 返回 JSON-RPC error `-32004 Resource not found`，不进入 IToolRegistry。

## 技术约束

- MUST URI scheme 固定 `hydraforge://`，禁止自定义 scheme（避免路径冲突）。
- MUST resources/read 内容与 lib/*.md 字节一致（无转换、无重写）。
- MUST resources/list mimeType 固定 `application/yaml`（subgraph 原生格式）。
- MUST NOT 暴露 lib/* 外的文件（避免路径穿越）。
- MUST 阻塞于 Phase 7b 启动条件（依赖 Phase 7a stdio transport ship）。

## 验收标准

- resources/list + resources/read E2E 测试通过。
- URI scheme + mimeType 注册验证。
- 路径穿越防御测试（`../etc/passwd` 类 URI 拒绝）。
- ctest 全量零回归。
- 阻塞 phase-7b 启动评估 `control-resources` 项 ✅。