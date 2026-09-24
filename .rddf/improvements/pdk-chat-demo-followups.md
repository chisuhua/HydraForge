# pdk-chat-demo-followups

**优先级**: P2 | **来源**: pdk-chat-demo-deduplicate change (2026-09-24) 审查 + 实施发现
**阶段**: 自由 (phase-n/a) | **分类**: 治理 / 技术债跟踪
**类型**: debt-tracking
**主题**: pdk-chat-demo 与 PDK / core 设施边界收敛后的剩余 follow-up 事项
**状态**: pending
**生成时间**: 2026-09-24
**关联 ADR**: ADR-0021 (PDK 设计), ADR-0033 (session 层次)
**关联 change**: openspec/changes/pdk-chat-demo-deduplicate (archived 2026-09-24)

## 架构依据

`pdk-chat-demo-deduplicate` change 在 2026-09-24 ship:
- 删除 `tools/provider_switch_stub.{h,cpp}` 死代码（零调用方）
- 新增 `SessionManager::dir()` / `current_session_id()` 公开 getter,消除 `tools/session_clone.cpp` 对内部成员 `dir_`/`current_session_id_` 的直接访问
- 接线 `/compact` 到 `SessionManager::compact()` (core 已存在,替代 "Compaction not yet wired" placeholder)
- DESIGN.md + README.md 同步贡献指南

Ship 后审查发现 **8 个 follow-up 事项** 需后续处理。其中 D1-D5 在 change 文档中已声明为 Out-of-Scope,D6-D8 是审查中新发现的治理/UX 改进建议。本文件作为可执行追踪载体,避免 follow-up 散落 proposal / design / README 中后遗忘。

## 范围

### In Scope

- **D1**: `pdk/session_agent` SessionStore 工具从 demo `config.json` 移除决策(当前 lazy 加载,零运行时副作用)
- **D2**: `main.cpp` 设置 `pdk_provider_agent::factory_slot()` 启用 `provider/switch` PDK 工具,补齐 demo 动态 provider 切换能力
- **D3**: SessionStore (扁平) 与 SessionManager (树状) 双后端合并决策(需 ADR 先行)
- **D4**: `SessionManager::compact()` 扩展为 `size_t compact()` 返回 active node count,UX 显示压缩比
- **D5**: `compact_command.cpp` 测试的 `ScopedSessionManager` RAII 模式加固(线程安全 + 异常安全)
- **D6**: 本 followups 文件本身 + `improvement-suggestions.md` 索引注册,建立可持续追踪机制
- **D7**: `test_command_compact:compact_active` 中硬编码 `line_count == 3` 的魔法数字改为动态计算
- **D8**: `SessionManager::compact()` 空路径 (`current_path_.empty()`) 静默 no-op 改为抛 runtime_error 或 emit 业务事件,提升可诊断性

### Out of Scope

- 任何新功能 (本文件仅跟踪已有 follow-up,不引入新 capability)
- 主项目阶段的提案 (phase-1 ~ phase-7) 各自有独立 improvement,本文件专用于 pdk-chat-demo 治理债务

## Why

**为什么现在做**: `pdk-chat-demo-deduplicate` change 的 proposal/design/README 各自声明了 3-5 个 follow-up,但**没有可执行的追踪机制**。散落在文档中的 follow-up 极易遗忘,直到下一个 reader 重新踩坑才发现。本次审查中已捕获 D6-D8 三个新发现项,若不立即建档,后续会重蹈覆辙(参 AGENTS.md Pattern #10 "post-acceptance hygiene fix 打乱资源创建顺序 → fresh-deploy 静默回归" 教训)。

**D1-D5 的成因**:
- **D1/D3** 是历史包袱 — chat-session-pdk-lift 之前 demo 已加载 `session_agent` 插件,但 ChatSession SessionManager 集成后未清理 config.json
- **D2** 是真实可用性缺口 — `provider/switch` 已 ship 但 demo main.cpp 未注入 factory_slot
- **D4/D5** 是测试覆盖/UX 改进,优先级低于核心债务

**D6-D8 的成因**(本审查发现):
- **D6** 是流程债 — 缺乏 follow-up 注册机制导致 D1-D5 散落文档
- **D7** 是测试脆性 — 硬编码魔法数字在 compact 行为微调时易脆断
- **D8** 是诊断缺口 — 静默 no-op 让生产问题排查困难

## What Changes

### 文件结构变更

1. **新增 `.rddf/improvements/pdk-chat-demo-followups.md`**(本文件) — 8 个 follow-up 详细登记
2. **新增 `.rddf/improvement-suggestions.md`**(如不存在)— 注册本改进条目
3. **(后续)**: D1-D8 任一项落地时创建对应 `pdk-chat-session-X` 子 improvement,attach 到合适 phase

### 具体 follow-up 追踪

| ID | 标题 | 优先级 | 关联文件 | 触发时机 |
|----|------|--------|----------|----------|
| D1 | session_agent 插件从 demo config.json 移除 | P3 | `examples/pdk_chat_demo/config.json` | SessionStore 合并决策(D3)前置或独立(若 demo 永远不需要 G3 集成) |
| D2 | factory_slot 注入 + /model 升级 | P2 | `examples/pdk_chat_demo/main.cpp` + `commands/model_command.cpp` | demo 需要"运行时真正切换 LLM provider"时 |
| D3 | SessionStore / SessionManager 合并 | P3 (需 ADR) | `pdk/session_agent/` + `src/core/session_manager.h` | 任何 production consumer 切换后端前必走 ADR |
| D4 | compact() 返回 size_t | P3 | `src/core/session_manager.{h,cpp}` + tests | UX 优化或 debug 需求时 |
| D5 | ScopedSessionManager RAII 强化 | P3 | `examples/pdk_chat_demo/tests/test_command_compact.cpp` | 多线程 test 或异常路径 test 出现时 |
| D6 | followup 追踪机制建立 | **P1**(本 change) | 本文件 + `improvement-suggestions.md` | 即时(本 change 同步完成) |
| D7 | compact_active 测试动态化 | P3 | `tests/test_command_compact.cpp:compact_active` | compact 行为微调时脆性回归即触发 |
| D8 | compact() 空路径异常化 | P3 | `src/core/session_manager.cpp:compact()` | 生产首次出现 "为什么 /compact 没生效" 工单即触发 |

## Acceptance

- [x] **D6 即时完成**: `.rddf/improvements/pdk-chat-demo-followups.md` 创建 + `.rddf/improvement-suggestions.md` 注册
- [ ] **D1**: `examples/pdk_chat_demo/config.json` 移除 `infra.session` 插件加载(`session_agent/libSessionAgent.so` 项删除),且 `grep session/persist examples/pdk_chat_demo/` 在 demo 生产代码返回 0 行
- [ ] **D2**: `main.cpp` 在 `LLMProviderFactory factory;` 后立即 `pdk_provider_agent::factory_slot() = &factory`,且 `commands/model_command.cpp` 改为经 `provider/switch` 工具调用(双路径: switch factory default + `request_model_switch()` 同步)
- [ ] **D3**: 新 ADR 起草(数据模型对齐决策) + 两套 session 后端统一实现(具体方向待 ADR 决策)
- [ ] **D4**: `SessionManager::compact()` 返回 `size_t` (active node count),`/compact` handler 输出 "Compacted session <id>: kept <N> nodes"
- [ ] **D5**: `ScopedSessionManager` 改为 RAII + thread-safe guard,异常路径自动 reset,新增多线程 compact 测试
- [ ] **D7**: `test_command_compact:compact_active` 中 `line_count` 改为 `nodes + branches_count` 动态计算(构造 N nodes + 1 branch meta,期望 N+1)
- [ ] **D8**: `SessionManager::compact()` 在 `current_path_.empty()` 时 `throw std::runtime_error("no open session to compact")`,`/compact` handler 捕获异常返回错误消息
- [ ] **回归**: 全量 ctest 264 测试维持基线,4 pre-existing failures 不变

## Capabilities

### New Capabilities

(无 — 本文件为改进追踪条目,不引入新 capability。每个 D1-D8 follow-up 落地时单独建 `pdk-chat-session-X` 子 improvement,各自承载 capability 定义)

### Modified Capabilities

(无)

## Impact

| 维度 | 影响 |
|------|------|
| **新增** | `.rddf/improvements/pdk-chat-demo-followups.md` + `.rddf/improvement-suggestions.md` 索引条目 |
| **修改** | 无(本 change 仅追踪,不动代码) |
| **测试** | 无(本 change 不引入代码变更) |
| **API** | 无 |
| **构建** | 无 |
| **风险** | 8 个 follow-up 中,D2 优先级最高(影响 demo 真实可用性),D6 即时完成(本 change),其余按需触发 |

## 关联文档

- **pdk-chat-demo-deduplicate change**: `openspec/changes/archive/2026-09-24-pdk-chat-demo-deduplicate/`
  - `proposal.md` §Out of Scope 声明 D1/D3
  - `design.md` §Open Questions Q1/Q2/Q3 展开 D1/D4/D2
  - `specs/pdk-chat-demo-dedup/spec.md` §R6 session-agent-dormancy-documented
- **examples/pdk_chat_demo/DESIGN.md** §Plugin 构成 段 — `infra.session` dormant 状态说明
- **examples/pdk_chat_demo/README.md** §添加新命令 / 工具 段 — follow-up 检查清单
- **AGENTS.md** §Reverse Indicator Rule — ship gate 引用