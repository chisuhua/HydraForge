# chat-async-io-model-switching

**STATUS**: 🔍 Proposed
**日期**: 2026-08-09
**来源**: improvements/chat-async-io-model-switching.md (Wave 3-A Phase C)
**类型**: feature (`/model` runtime command)
**优先级**: P2 (Wave 3-A 终阶段, 独立路径)
**关联依赖**:
- Phase 0 `fix-tool-registry-signal-handler-shutdown` ✅ shipped
- Phase A `chat-async-io-queue-infra` ✅ shipped
- Phase B 7-step wiring ✅ shipped (Step 1+2/3/4/5)
- `provider-dynamic-discovery` ✅ shipped (2026-08-06)

## Why

Phase 0/A/B 已 ship，但用户仍**无法运行时切换 LLM provider/model**。当前 demo 启动时通过 `config.json` + `--provider` flag 锁定 provider，启动后无法在不重启 demo 的情况下切换。

Wave 3-A Phase C 添加 `/model <name>` DECLARE_COMMAND（ADR-0070 模式），允许用户在交互中动态切换模型。这是 chat-async-io-steering 拆分提案的最后一个 phase。

**为何现在**:
- Phase B stop_token 链路已 ship，切换可在下次 turn 自然生效（无需强制中断）
- `provider-dynamic-discovery` ✅ shipped 提供 provider 字符串 → ILLMProvider 路由
- `provider-dynamic-discovery` (2026-08-06) 已 ship，Phase C 可复用其能力

## What Changes

### 核心改动

1. **`/model <name>` DECLARE_COMMAND** — 注册新命令解析 `provider/<model>` 字符串
2. **ChatSession 维护 `next_model_` atomic 字符串** — 记录用户选择的下一 turn 模型
3. **per-turn 模型切换** — 下一次 `chat()` 时读取 `next_model_` + 通过 `LLMProviderFactory` 路由，不强制中断当前 turn
4. **`provider-dynamic-discovery` 集成** — 复用 provider 字符串 → ILLMProvider 路由（已 ship）
5. **session JSONL 持久化** — `next_model_` 写 session_meta，重启后生效
6. **mock 模式特殊处理** — `/model openai` 等拒绝切换（mock 模式下 provider 固定）

### 不修改范围 (Non-goals)

- 不修改当前 `config.json` 加载逻辑
- 不实现 `thinking_level` 抽象（依赖 provider 支持，缓建）
- 不实现 provider 自动选型（benchmark/auto-route）
- 不修改 Phase 0/A/B 已有 wiring

## Capabilities

### New Capabilities

- `chat-async-io-model-switching`: `/model <name>` DECLARE_COMMAND + ChatSession per-turn 模型切换 + provider-dynamic-discovery 集成

### Modified Capabilities

无

## Impact

### 受影响代码

- `examples/pdk_chat_demo/chat_session.h/.cpp` — 新增 `next_model_` atomic + `request_model_switch()` API + `chat()` 前切换 provider
- `examples/pdk_chat_demo/commands/` — 新增 `model_command.cpp`（DECLARE_COMMAND 模式）
- `examples/pdk_chat_demo/main.cpp` — `model_command` 注册到 command_registry
- `examples/pdk_chat_demo/CMakeLists.txt` — 新 source 编译

### API 影响

- 新增公开 API: `ChatSession::request_model_switch(const std::string&)` + `next_model() const`
- 新增 DECLARE_COMMAND: `/model <name>`

### 依赖系统

- `provider-dynamic-discovery` (✅ shipped) — provider 字符串路由
- `LLMProviderFactory` (Phase 1 Sprint 0) — provider 实例化
- DECLARE_COMMAND 宏 (ADR-0070) — 命令注册

### 测试影响

- 新增 `tests/test_model_switching.cpp`: 4 cases
  - `/model deepseek` 成功切换（mock 模式）
  - `/model openai` 在 mock 模式下拒绝
  - next_model_ 持久化到 session_meta
  - per-turn 切换不中断当前 turn

### 风险评估

- **风险等级**: 🟢 LOW-MEDIUM
- **理由**:
  - 仅新增命令 + 切换逻辑，不修改现有 chat() 流
  - provider-dynamic-discovery 已 ship 提供稳定 provider 路由
  - 切换点在下一次 chat() entry，无 race condition 风险
- **缓解**:
  - 切换前验证 provider 可达
  - mock 模式硬编码拒绝非 mock provider
  - 持久化失败不破坏 chat 流（仅 warning）