# from-roadmap-phase-7-control-prompts

**优先级**: P0 | **来源**: from-roadmap (phase-7/control-prompts) ⏸ Phase 7b gated
**阶段**: phase-7 | **分类**: control-prompts
**类型**: feature
**主题**: MCP prompts；select_subgraphs；Evidence Gate审计

## 架构依据

ADR-0079 Control Plane `control-prompts` 项是 Phase 7b 启动后才解锁（依赖 ADR-0074 baseline V0-V3 + Evidence Gate 决议）：

- MCP `prompts/*` 暴露将 HydraForge 内部 `.agent.md` subgraphs 转换为 LLM-friendly 模板。
- ADR-0074 baseline V0-V3（V1 schema 约束 + V2 few-shot + V3 两阶段注入）为 prompts 模板的 schema 基础。
- Evidence Gate 决议（ADR-0074 D4，parse-valid ≥85% + task-success L1 ≥70%）决定 prompts 是否 ship。

## 范围

- **In Scope**:
  - `pdk/mcp_stdio_server/` 注册 MCP `prompts/list` + `prompts/get` 方法。
  - `prompts/list` 列出 lib/inference/* (engine.md, model.md, session.md) 等 stdlib subgraphs 作为可选 prompt 模板。
  - `prompts/get` 返回指定 prompt 模板内容（包含 few-shot examples + golden suite 引用）。
  - `select_subgraphs` 工具：用户选择 lib/* 子图作为 prompt 上下文（经 IToolRegistry 暴露）。
  - `evidence_gate_audit` 工具：审计最近 100 次 LLM 调用的 parse-valid + task-success 指标（ADR-0074 D4 决议依据）。
  - 3 类测试：prompts/list 列举 / prompts/get 内容 / evidence_gate_audit 指标生成。
- **Out of Scope**:
  - resources/* 暴露（→ control-resources 同期）。
  - 提示模板动态生成（依赖 ADR-0074 V2/V3 实施，留独立 follow-up）。
  - 用户自定义 prompt 模板上传（无 UI 入口需求）。

## 关键场景

- GIVEN pdk_chat_demo 启动 + lib/inference/* subgraphs 已 ship
  WHEN 外部 MCP client 发送 `prompts/list`
  THEN 返回 `inference/engine`、`inference/model`、`inference/session` 等模板列表 + description。

- GIVEN 外部 client 发送 `prompts/get` name=`inference/engine`
  WHEN server 处理
  THEN 返回模板内容（含 ADR-0074 V1 schema 约束 + V2 few-shot 引用）。

- GIVEN pdk_chat_demo 已运行 50 个 session
  WHEN 调用 `evidence_gate_audit`
  THEN 返回 `{parse_valid: 0.83, task_success_l1: 0.65, sample_count: 50}` JSON（未达 Evidence Gate 阈值）。

- GIVEN parse-valid ≥ 85%
  WHEN evidence_gate_audit 调用
  THEN 返回 PASS 决策提示，phase-7 启动条件 `control-prompts` 可勾选。

## 技术约束

- MUST prompts 列表仅暴露 lib/* stdlib subgraphs，禁止暴露用户自定义 subgraph（无审计机制）。
- MUST prompts/get 内容包含 ADR-0074 baseline 版本元数据（V0/V1/V2/V3 标识）。
- MUST evidence_gate_audit 指标采样至少 30 个 session 才有统计意义（< 30 返回 insufficient_data）。
- MUST NOT 暴露 raw LLM 调用历史（含 secret 数据），仅暴露聚合指标。
- MUST 阻塞于 ADR-0074 baseline ship（Phase 6c C3）+ Evidence Gate 决议（Phase 6c C4）。

## 验收标准

- prompts/list + prompts/get E2E 测试通过。
- select_subgraphs + evidence_gate_audit 工具注册并响应。
- evidence_gate_audit insufficient_data 路径测试通过（< 30 session 拒绝）。
- ctest 全量零回归。
- 阻塞 phase-7b 启动评估 `control-prompts` 项 ✅。