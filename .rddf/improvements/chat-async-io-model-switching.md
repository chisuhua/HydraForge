# chat-async-io-model-switching

**优先级**: P2 | **来源**: docs/audits/2026-08-08-chat-async-io-steering-pre-approval.md Phase C
**阶段**: chat-async-io Phase C | **分类**: demo-chat-v2
**类型**: feature
**主题**: PlanExecute循环

> **拆分说明**: 本提案是原 `chat-async-io-steering` (Wave 3) 的 Phase C 拆分（/model 运行时切换）。
> 拆分理由：`/model` 切换依赖 Phase B 的 stop_token 链路（确认当前 turn 不强制中断），不能与 Phase B 同 change ship。
> 本提案 blocked-by：`chat-async-io-cancellation-chain` (Phase B) + `provider-dynamic-discovery` ✅ (2026-08-06 已 ship)。

## 架构依据
- `provider-dynamic-discovery` 已 ship（2026-08-06），提供 provider 字符串 → ILLMProvider 路由能力，是 `/model` 切换的 provider 解析基础。
- Phase B 提供 stop_token 链路，使"下一个 turn 切换但不中断当前 turn"语义可行。
- `/model` 是 pi-agent 核心交互能力（运行时切换模型无需重启 demo）。

## 范围
- **In Scope**: `/model <name>` 命令注册（DECLARE_COMMAND 模式）；ChatSession 维护 `next_model_` atomic 字符串；每个 turn 开始前读取 next_model 并切换 ILLMProvider；切换不强制中断当前 turn；切换路径上 provider_agent 协同；E2E 测试 `/model deepseek` → `/model mock` 切换。
- **Out Scope**: thinking_level 动态切换（依赖 provider 支持，缓建）；模型预设/收藏；模型 benchmark 自动选型。

## 关键场景
- GIVEN 会话进行中，WHEN 用户输入 `/model deepseek`，THEN 下一个 turn 起使用 deepseek provider，当前正在执行的 turn 不被强制中断。
- GIVEN Agent turn 完成后，WHEN next_model_ 已设置，THEN 下一 turn 使用新 provider，session 持久化 next_model。
- GIVEN `/model` 切换到不存在的 provider，THEN 命令层返回错误，不修改 next_model_。
- GIVEN mock 模式，WHEN `/model openai`，THEN 拒绝切换（mock 模式下 provider 固定 mock）。

## 技术约束
- MUST `/model` 切换不强制中断当前 turn（Phase B stop_token 链路保证）。
- MUST next_model_ 持久化至 session JSONL（重启后生效）。
- MUST `/model` 命令经 DECLARE_COMMAND 注册（ADR-0070 模式）。
- MUST NOT 引入第二套 provider 路由（复用 provider-dynamic-discovery 已 ship 能力）。
- SHOULD `/model` 后打印当前生效 model 确认信息。

## 验收标准
- `/model` 命令 E2E 测试 PASS（mock 模式拒绝非 mock provider；真实模式切换成功）。
- next_model_ 持久化 + 重启后生效测试 PASS。
- 切换不中断当前 turn 的并发测试 PASS（启动长 turn → 切换 → 当前 turn 完成）。
- ctest 全量零回归。