# ADR-0002 实现范围审计 (Implementation Scope Audit)

> **✅ DECIDED (2026-06-13)** — 本文件由 OpenSpec change `docs-code-drift-audit-2026-06` 创建 (2026-06-13)。
> 状态：决策已完成（选项 A + 附加步骤），同步更新了 `adr-0002-eventbus-bounded-queue.md` 头部状态行。

## ADR 描述（引用 ADR-0002 原文要点）

ADR-0002 (EventBus 有界队列架构) 描述：
- **系统背景**：HydraForge **Phase 1** 需要 FTXUI 主线程与后台 HarnessEngine 执行线程之间的线程安全通信
- **核心组件**：`EventBus` 类（基于 Taskflow v4.0 + async_simple 双层异步架构）
- **功能**：HarnessEngine 通过 EventBus 向 FTXUI 推送 LLM token 流、工具执行状态、错误信息等
- **V2 扩展**：从纯 UI 事件总线扩展为全系统可观测性总线；DispatchMode 分发机制

## 代码实际状态（grep 验证 2026-06-13）

### 验证命令
```bash
$ grep -rn "class.*EventBus" src/ include/ 2>&1
src/common/contract/CMakeLists.txt    # 仅 build 引用
src/common/contract/inmemory_bus.cpp # 提到 EventBus 但不是类定义

# 实际验证：仓库中无 EventBus 类定义
$ find src include -name "*.h" -o -name "*.hpp" | xargs grep -l "^class EventBus\|^class.*EventBus " 2>&1
# （无输出）

$ find src include -name "*.h" -o -name "*.hpp" | xargs grep -l "^class InMemoryBus" 2>&1
include/agenticdsl/contract/inmemory_bus.h
```

### 实际状态

| ADR-0002 描述的类/功能 | 代码库实际存在？ | 备注 |
|---|---|---|
| `EventBus` 类 | ❌ **不存在** | ADR-0002 描述的 FTXUI/HarnessEngine 系统不在 AgenticDSL 代码库中 |
| `InMemoryBus` | ✅ 存在 (`include/agenticdsl/contract/inmemory_bus.h` + `src/common/contract/inmemory_bus.cpp`) | 这是 ADR-0019 MVP 的简化实现，使用 `std::mutex`，与 ADR-0002 的 Taskflow+async_simple 架构不同 |
| `DispatchMode` 分发机制 | ❌ 未实现 | `InMemoryBus` 使用简单的同步 mutex 分发 |
| FTXUI 主线程 + HarnessEngine 后台线程 | ❌ **整个 FTXUI/HarnessEngine 子系统不在仓库中** | AgenticDSL 是 DSL 执行引擎；FTXUI 是独立 TUI 项目 |

### 关键观察

1. **ADR-0002 描述的是 HydraForge 项目的 Phase 1**（含 FTXUI TUI + HarnessEngine 后台），但本仓库（AgenticDSL）是 HydraForge 的**子项目**——DSL 执行引擎层
2. **AgenticDSL 仓库中确实有"事件总线"对应物**——`InMemoryBus`（ADR-0019 §1.1 MVP 简化）
3. **ADR-0002 的引用关系是正确的**——它被 ADR-0019 §关联 引用，**但实际实现走了不同的简化路径**

## 决策需求

**问题**：ADR-0002 的描述与 AgenticDSL 仓库的实现类不对应。`EventBus` 类从未实现；AgenticDSL 用的是更简单的 `InMemoryBus`。

### 选项 A：保留 ADR-0002 作为 HydraForge 母项目设计历史（**推荐**）

- 标记 ADR-0002 状态为 "📦 设计历史 (FTXUI/HarnessEngine 系统)"
- 添加注脚 "本 ADR 描述的 EventBus 在 HydraForge Phase 1 FTXUI 子系统中；AgenticDSL 仓库对应实现见 ADR-0019 + InMemoryBus"
- **不修改** ADR-0002 内容
- 优点：保留设计意图，不破坏 history
- 缺点：读 ADR-0002 的人若不知道 HydraForge 母项目可能困惑

### 选项 B：重新起草 ADR-0002 描述 InMemoryBus

- 改写 ADR-0002 内容为 `InMemoryBus` 的实际架构
- 优点：ADR 与实现一一对应
- 缺点：丢失原 FTXUI 设计意图；需要 ADR 决策流程

### 选项 C：把 ADR-0002 移到 `docs/archive/adr/`，新建 ADR-0002 描述 InMemoryBus

- 优点：清晰分离已废弃 vs 当前有效
- 缺点：13 个已废弃 ADR 已归档，再归档一个可能让目录结构变复杂

## 决策记录 (2026-06-13)

**用户决策**：选项 A + 附加步骤。

**已执行动作**：
1. ✅ `adr-0002-eventbus-bounded-queue.md` 头部状态行： `✅ Approved` → `📦 设计历史 (未实施)`，附 2026-06-13 审计备注与未来重新评估触发条件说明。**主体内容未修改**。
2. ✅ `adr-0019-iinteraction-bus-mvp.md` §1.1 "MVP 简化" 段落末尾：增加 "重新评估 EventBus 实施的触发条件" 段落，包含 4 个量化触发条件（吞吐瓶颈 / Per-Session 隔离 / 优先级背压 / 多 Agent 协作）与不触发则不实施原则。
3. ✅ `adr-0004-impl-scope.md` 同问题一并处理（见该文件决策记录）。

**决策依据**：
- 架构蓝图已通过 `IInteractionBus` 抽象接口与 EventBus **解耦**——`InMemoryBus` 是当前实现，未来 EventBus 实现可作为新的 `IInteractionBus` 实现类平滑替换，不破坏 API 表面。
- 阶段性目标（Phase 1 TUI Chat）实际吞吐 < 1K events/s，InMemoryBus (mutex + queue) 足够。
- 重新评估路径已具象化为**量化触发条件**，避免"未来某天再说"式债务。

**未触动项**（明确不动）：
- ADR-0002 主体内容（V2 架构设计、代码示例、权衡分析等）保持原样
- `IInteractionBus` 接口签名不变
- `InMemoryBus` 实现不变
- `examples/agent_chat/` TUI 仍按原计划推进（独立 OpenSpec change 范围）