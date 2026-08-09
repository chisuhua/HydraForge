## Context

Wave 3-A Phase C — final phase of chat-async-io-steering 拆分。Phase 0/A/B 已 ship 完整 wiring infrastructure，Phase C 添加 `/model <name>` 命令允许运行时切换 LLM provider/model。

Phase B 已 ship stop_token 链路，per-turn 模型切换可在下一次 `chat()` entry 自然应用（无需强制中断）。Phase C 是 Wave 3-A 收官。

### 现有架构约束

- `ChatSession::chat()` 同步调用当前 provider
- 当前 provider 在 `ChatSession::Impl` 构造时通过 `LLMProviderFactory` 创建
- `provider-dynamic-discovery` (✅ shipped 2026-08-06) 提供 provider 字符串 → ILLMProvider 路由
- DECLARE_COMMAND 宏 (ADR-0070) 已 ship，支持命令注册

## Goals / Non-Goals

**Goals:**
- `/model <name>` DECLARE_COMMAND 解析 `provider/model` 字符串
- ChatSession 维护 `next_model_` atomic 字符串
- 下一次 `chat()` 时读取 `next_model_` + 切换 provider
- 复用 `provider-dynamic-discovery` 路由
- 持久化 `next_model_` 到 session JSONL
- mock 模式硬编码拒绝非 mock provider

**Non-Goals:**
- 不修改当前 `config.json` 加载逻辑
- 不实现 `thinking_level` 抽象
- 不实现 provider 自动选型
- 不修改 Phase 0/A/B 已有 wiring
- 不实现 `<think>` 标签或 streaming 增强（chat-streaming-slash-tui 已 ship）

## Decisions

### Decision 1: per-turn 切换（不强制中断）

**选择**: `next_model_` 在下次 `chat()` entry 时读取并切换 provider；当前正在执行的 turn 不中断

**理由**:
- **UX 友好**: 用户切换模型时不会被强制中断
- **Phase B 兼容**: Phase B 已 ship stop_token 链路，但本次不用
- **简化语义**: per-turn 切换语义清晰（"下一个 turn 起切换"）

**替代方案**:
- **强制中断**: 需要调用 `request_stop()` 触发当前 turn 取消，与 pi-agent `/model` 语义不符
- **实时切换**: 需要 provider swap 线程安全机制，复杂度高

### Decision 2: DECLARE_COMMAND 注册模式（ADR-0070）

**选择**: 新命令注册走现有 DECLARE_COMMAND 体系（与 chat-slash-commands-migration 一致）

**理由**:
- **统一架构**: `/model` 与 `/help` `/exit` 等命令同构
- **零特殊路径**: 复用 command_registry + DECLARE_COMMAND 宏
- **测试友好**: command 可独立单元测试

### Decision 3: provider-dynamic-discovery 集成（不重新实现路由）

**选择**: `/model` 命令委托 `provider-dynamic-discovery` 已 ship 的 provider 字符串 → ILLMProvider 路由

**理由**:
- **零重复**: 避免重新实现 provider 路由
- **稳定基础**: `provider-dynamic-discovery` 已 ship + 测试覆盖
- **一致 UX**: `/model` 接受的 provider 字符串与 `--provider` flag 一致

### Decision 4: session JSONL 持久化 next_model_

**选择**: `next_model_` 写 session_meta JSONL record；重启后 `load_from_disk()` 恢复

**理由**:
- **持久化一致**: ChatSession 已有 `save_to_disk()`/`load_from_disk()` 机制
- **跨重启 UX**: 用户切换模型后重启 demo 仍生效

**替代方案**:
- **仅内存**: 重启丢失切换（不符合预期）

## Risks / Trade-offs

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| provider 路由失败 | 🟢 Low | 🟡 Medium | `provider-dynamic-discovery` 已有错误处理 |
| per-turn 切换时机误解 | 🟡 Medium | 🟢 Low | 文档明确"下一个 turn 起"语义 |
| mock 模式误切换 | 🟢 Low | 🟢 Low | 硬编码拒绝非 mock provider |
| session 持久化失败 | 🟢 Low | 🟢 Low | 仅 warning，不破坏 chat 流 |

## Migration Plan

### 部署

无 schema / API 变更 → 零迁移成本

### 回滚

删除 model_command + ChatSession::next_model_ + 相关测试即可

### 兼容性

- 现有 `config.json` 加载 + `--provider` flag 不受影响
- 现有命令（/help /exit 等）不受影响

## Open Questions

无 — Phase 0/A/B + provider-dynamic-discovery + DECLARE_COMMAND 全部已 ship，Phase C 是纯增量。