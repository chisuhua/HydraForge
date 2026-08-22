# ADR-0082 实现范围审计 (Implementation Scope Audit)

## 状态

**📋 Partial (证据基础，Batch 2)** (2026-08-22) — 本文件是 ADR-0082 实施范围审计，记录 Batch 2 P7 (`adr-0082-promote-to-approved`) 骨架 ship 的实施证据。状态变更历史见头部备注段落。

> **📋 Audit (本审计)** (2026-08-22) — 本文件由 Batch 2 ship gate 收官审计创建。
> 范围：ADR-0082 V1 骨架（IAgentRegistry L3 契约 + InMemory 参考实现）已 ship；
> 完整 AgentWorker + spawn_agent DSL + YAML 配置 + subprocess 形态推迟 Sprint 24+ 独立 change。
> **本审计不修改任何 C++ 或测试源代码。**

## ADR 描述（引用 ADR-0082 §决策 7 要点）

ADR-0082 (Agent as First-Class Registry) 描述：
- **D1**: IAgentRegistry L3 契约（4 个核心 API: register / create / unregister / list_registered）
- **D2**: 字符串 ID 标识（与 PluginInfo::name 对齐，C1 决议）
- **D3**: per-engine 注册粒度（与 ADR-0022 对齐，C2 决议）
- **D4**: AgentConfig V1 最小集（仅 instance_id）
- **D5**: IAgent 最小抽象（name + id）
- **D6**: 状态持久化通过 EventLog + SessionWriter 双重事件流（C3 决议，留 EventLog 集成期实施）
- **D7**: marketplace 接口契约 — V1 plugin 形态（C4 决议，subprocess 推迟 Phase 2）
- **D8**: 与 ADR-0022/0069/0081 三层 hook 正交（C5 决议）

## 代码实际状态（grep 验证 2026-08-22）

### 验证命令

```bash
# IAgentRegistry 契约头文件
ls include/agenticdsl/contract/iagent_registry.h
# → exists: True (80 行)

# IAgentHookRegistry 契约头文件（ADR-0081 联动）
ls include/agenticdsl/contract/iagent_hook_registry.h
# → exists: True (85 行)

# InMemory 参考实现
ls src/core/agent_registry.cpp src/core/agent_hook_registry.cpp
# → both exist (130 行 + 130 行)

# 测试
ls tests/test_agent_registry.cpp tests/test_agent_hook_registry_contract.cpp
# → both exist (5 cases / 29 assertions + 4 cases / 18 assertions PASS)
```

### 实现范围分类（ADR-0082 §决策 7）

| Decision | 描述 | V1 实施状态 | 完整实施计划 |
|----------|------|-------------|--------------|
| **D1** IAgentRegistry 契约 | 4 API (register/create/unregister/list_registered) | ✅ Ship (P7) | 已完成 |
| **D2** 字符串 ID | 与 PluginInfo::name 对齐 | ✅ Ship (P7) | 已完成 |
| **D3** per-engine 注册粒度 | 与 ADR-0022 对齐 | ✅ Ship (P7) | 已完成 |
| **D4** AgentConfig V1 最小集 | 仅 instance_id | ✅ Ship (P7) | 完整字段（loop_type / llm_provider / max_spawn_depth 等）推迟 Sprint 24+ |
| **D5** IAgent 最小抽象 | name + id | ✅ Ship (P7) | 完整生命周期接口留 Agent hook 实施 change |
| **D6** 状态持久化 | EventLog + SessionWriter 双重事件流 | 🟡 Partial | EventLog 集成测试待 Sprint 24+ Agent hook 实施 change |
| **D7** V1 plugin 形态 marketplace | V1 plugin 形态 ship | ✅ Ship (P7) | subprocess 形态推迟 Phase 2 |
| **D8** 三层 hook 正交 | 与 ADR-0022/0069/0081 正交 | ✅ Ship (P7+P3) | 已完成 |

## 关键发现

### ✅ V1 骨架完整 ship（Batch 2 P7）

- `IAgentRegistry` L3 契约：register / create / unregister / list_registered / is_registered / size (6 API)
- InMemoryAgentRegistry 内存参考实现：shared_mutex + string-keyed factory map
- 测试 5 cases / 29 assertions PASS（C1-C5 决议对应）
- register_agent 返回 bool（不抛 — Metis 修正 17 ErrorCode 无 AlreadyRegistered）
- unregister 同步删除（V1 简化，pending 语义留 Sprint 24+）

### 🟡 完整实施推迟 Sprint 24+

**完整 AgentWorker 类（React/PlanExecute/ForkJoin 三循环分发）**：
- 当前缺失 — 仅 V1 简化版（`SimpleAgent` 仅 name + id）
- 推迟原因：ADR-0082 决策 7 已说明 — AgentWorker 需泛化 CognitiveWorker（仅支持 React），需 ~3-5 天工作量
- 关联：`include/agenticdsl/pdk/agent_loops/{react,plan_execute,fork_join}_loop.h` 已有 3 循环骨架，但未与 AgentRegistry 集成

**spawn_agent DSL 节点**：
- 当前缺失 — V1 未实施
- 推迟原因：依赖 AgentWorker
- 关联：`src/modules/parser/node_factory.cpp` 可加 pattern，NodeExecutor 需 case 分发

**YAML 配置加载**：
- 当前缺失 — V1 未实施
- 推迟原因：依赖 AgentWorker + spawn_agent
- 关联：`src/common/utils/yaml_json.cpp` 已有 YAML→JSON 桥

**subprocess 形态 Agent**：
- 当前缺失 — V1 未实施（与 PRD 一致）
- 推迟原因：涉及 IPC + sandbox + lifecycle 跨进程管理
- 关联：`src/modules/skill_interpreter/skill_interpreter.cpp` 已有 posix_spawn + IPC 经验

#### 缺失功能（V1 未实施，待 Sprint 24+）

1. **AgentWorker 完整实现**：React/PlanExecute/ForkJoin 三循环分发 + DSLEngine 集成
2. **spawn_agent DSL 节点**：NodeFactoryRegistry 一行 + NodeExecutor case
3. **YAML 配置**：`~/.hydraforge/agents/assistant.yaml` 加载 + schema 校验
5. **subprocess 形态**：PDK Wasm / posix_spawn 子进程
6. **EventLog 集成测试**：C3 决议"全量事件 + 会话结构事件 = 完整生命周期可重放"
7. **IAgent 完整接口**：run / state / lifecycle 事件发射 / 状态机

#### ADR-0081 联动（D3 决议）

ADR-0081 (Pre-Step Hook Contract) 与 ADR-0082 同期 ship (Batch 2 P3)：
- `IAgentHookRegistry` 4 API（register_pre/post + apply_pre/post）
- HookErrorPolicy 复用 ADR-0069（避免双轨）
- agent_glob 通配（`*` / `?`）— 与 ADR-0043 naming 一致
- priority 高→低 排序
- FailClosed → deny 不可覆盖；FailOpen → 警告 + Continue
- 测试 4 cases / 18 assertions PASS（contract compile / glob / priority / policy）

C5 决议验证：plugin hook (ADR-0022) + tool hook (ADR-0069) + agent hook (ADR-0081) 三层正交
- IAgentRegistry / IToolHookRegistry / IAgentHookRegistry 接口独立，无类型/字段耦合
- 调用顺序：agent step → tool call，hook 触发点不重叠

#### 后续追踪

- **Sprint 24+**: Agent hook 实施 change（独立 OpenSpec change 提案）
  - AgentWorker + spawn_agent DSL + YAML 配置（缺陷 3.1 完整 ship）
  - Agent loop 集成 hook（缺陷 4.2 完整 ship）
- **Sprint 24+ ADR 状态翻转**: ADR-0082 从 "Partial (V1 骨架)" → "Approved (完整 ship)"
- **Sprint 25+**: subprocess 形态 Agent（Phase 2 roadmap）

---

**审计工具**: `python3 tools/docs_drift_audit.py` 验证本审计存在 + ADR-0082 实施范围
**审计依据**: `docs/architecture/defect-truth-table-2026-08.md` §缺陷 3.1 + §缺陷 4.2
**审批**: Batch 2 ship gate 收官审计（2026-08-22, Sisyphus + 架构组 human-gate）