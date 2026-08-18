# from-roadmap-phase-7-control-token

**优先级**: P0 | **来源**: from-roadmap (phase-7/control-token)
**阶段**: phase-7 | **分类**: control-token
**类型**: feature
**主题**: 静态token；Bearer鉴权；0600权限

## 架构依据

ADR-0079 Control Plane 启动条件 `control-token` 要求静态 token 鉴权机制落地：

- MCP 协议本身无鉴权层，需 host-side 鉴权（Bearer / X-MCP-Token）。
- pi-agent 借鉴路径 §七 P0.3：control plane 必须有最小鉴权防滥用。
- 0o600 文件权限是 Linux 上 token 文件的标准做法（curl、ssh 沿用）。

## 范围

- **In Scope**:
  - `pdk_chat_demo` 启动 `--mcp-token-file <path>` flag，加载 token 后置 0600 权限校验。
  - `McpStdioTransport` 接收 `Authorization: Bearer <token>` 或 `X-MCP-Token: <token>` header 时与本地 token 字符串匹配。
  - token 不匹配返回 JSON-RPC 2.0 `error.code = -32001`（自定义 Unauthorized）。
  - token 文件缺失或权限 > 0600 启动失败（exit code 非零 + stderr 明确）。
  - token 字符串内存比较使用 constant-time（防御 timing attack）。
  - 4 个测试：合法 token 匹配 / 错误 token 拒绝 / token 文件权限错误 / constant-time 比较验证。
- **Out of Scope**:
  - OAuth / OIDC 鉴权（无需求，留独立 follow-up）。
  - token 刷新 / 撤销（静态 token 模式无）。
  - 进程间 token 传递（依赖 stdio transport 假设同机可信）。

## 关键场景

- GIVEN token 文件含 `secret-abc123` 且权限 0600
  WHEN 外部 client 在 `Authorization: Bearer secret-abc123` 头中传递
  THEN MCP stdio server 接受请求，正常执行。

- GIVEN 同上 token 文件
  WHEN client 发送 `Authorization: Bearer wrong-token`
  THEN server 返回 JSON-RPC error `-32001 Unauthorized，不进入 handler。

- GIVEN token 文件权限 0644
  WHEN pdk_chat_demo 启动加载
  THEN stderr 输出 `ERROR: token file permissions too loose (expect 0600, got 0644)` 并以 exit code 78（EX_CONFIG）退出。

- GIVEN 缺失 `--mcp-token-file` flag
  WHEN 启动
  THEN 走无鉴权路径（mcp server 仅本地 socket，依赖 0600 隐式信任）。

## 技术约束

- MUST 权限校验严格 0600（不允许其他任何 setuid bit 残留）。
- MUST constant-time 比较（防御 timing attack），禁止 `==` 或 `strcmp`。
- MUST token 字符串不写入日志或 bus 事件（防御 secret 泄露）。
- MUST token 文件路径解析支持相对/绝对路径，相对路径相对 cwd。
- MUST NOT 在 stderr 打印 token 内容（仅打印 mask `***` 形态）。

## 验收标准

- 4 类测试全部通过（合法 / 错误 / 权限 / constant-time）。
- token 不出现在 log 文件、stderr、bus 事件。
- pdk_chat_demo `--mcp-token-file` 启动 flag 注册并生效。
- ctest 全量零回归。
- 阻塞 phase-7 启动评估 `control-token` 项 ✅。