# ADR-0004 实现范围审计 (Implementation Scope Audit)

> **PENDING DECISION** — 本文件由 OpenSpec change `docs-code-drift-audit-2026-06` 创建 (2026-06-13)。
> 状态：等待用户决策（保留作为 ToolRegistry 安全层理想描述 vs 重新起草为 IExecutionPolicy 族系描述）。
> 决策前 **不修改** `docs/adr/adr-0004-toolregistry-security.md` 原文。

## ADR 描述（引用 ADR-0004 原文要点）

ADR-0004 (ToolRegistry 安全模型) 描述：
- **背景**：HydraForge Agent 通过 ToolRegistry 调用外部工具（文件系统、Shell、网络）；LLM 可能生成恶意的工具调用
- **核心组件**：
  - `ToolCategory` 工具安全分类（ReadOnly/WriteFile/Execute/Network/StateModify）
  - `ApprovalPolicy` 三模式审批策略（Plan/Agent/YOLO）
  - `PathPolicy` 路径策略
  - `ShellGuard` Shell 命令守卫
  - `SecureToolRegistry` 安全 ToolRegistry 实现
- **核心原则**：纵深防御 = 技术控制 + 权限分层 + 用户确认
- **V2 对齐**：与 ADR-0031 IExecutionPolicy 议题 3 对齐更新

## 代码实际状态（grep 验证 2026-06-13）

### 验证命令
```bash
$ grep -rn "class.*\(PathPolicy\|ShellGuard\|SecureToolRegistry\|ToolCategory\|ApprovalPolicy\)" src/ include/ 2>&1
# （无输出 — 所有 5 个类均不存在）

$ find src/common/policy -name "*.h" -o -name "*.cpp" 2>&1
src/common/policy/agent_mode_policy.cpp
src/common/policy/agent_mode_policy.h
src/common/policy/execution_policy.h
src/common/policy/plan_mode_policy.cpp
src/common/policy/plan_mode_policy.h
src/common/policy/yolo_mode_policy.cpp
src/common/policy/yolo_mode_policy.h
```

### 实际状态

| ADR-0004 描述的类/功能 | 代码库实际存在？ | 备注 |
|---|---|---|
| `ToolCategory` (ReadOnly/WriteFile/Execute/Network/StateModify) | ❌ **不存在** | |
| `ApprovalPolicy` (Plan/Agent/YOLO 三模式) | 🟡 **部分对应** | 实际是 `plan_mode_policy` / `agent_mode_policy` / `yolo_mode_policy` 三个独立类，**不是**统一的 `ApprovalPolicy` 抽象 |
| `PathPolicy` | ❌ **不存在** | |
| `ShellGuard` | ❌ **不存在** | |
| `SecureToolRegistry` | ❌ **不存在** | `ToolRegistry` 本身在 `src/common/tools/registry.cpp`，但**无安全包装** |
| 纵深防御 (技术控制 + 权限分层 + 用户确认) | 🟡 **部分对应** | `IExecutionPolicy` 族系实现了模式（plan/agent/yolo），但路径/Shell 安全层**未实现** |

### 关键观察

1. **ADR-0004 描述的是 ToolRegistry 的安全层**——`PathPolicy`/`ShellGuard`/`ToolCategory` 应当作为 `ToolRegistry` 的成员/包装实现
2. **AgenticDSL 仓库中实际有 `IExecutionPolicy` 族系**（ADR-0031 议题 3）——这是 ADR-0004 V2 提到的"对齐 IExecutionPolicy"的**部分实现**：
   - `agent_mode_policy` / `plan_mode_policy` / `yolo_mode_policy` 是具体的 IExecutionPolicy 实现
   - 但**没有 ToolRegistry 集成**——policy 与 ToolRegistry 仍是松散关联
3. **`SecureToolRegistry` / `PathPolicy` / `ShellGuard` 从未实现**——ADR-0004 描述的是设计意图，代码库走了不同路径

## 决策需求

**问题**：ADR-0004 描述的 5 个核心类（`PathPolicy`/`ShellGuard`/`SecureToolRegistry`/`ApprovalPolicy`/`ToolCategory`）中只有 3 个 policy 类有对应实现（但分散、非统一抽象），2 个安全组件（`PathPolicy`/`ShellGuard`）完全未实现。

### 选项 A：保留 ADR-0004 作为 ToolRegistry 安全层设计理想（**推荐**）

- 标记 ADR-0004 状态为 "🟡 Partial（IExecutionPolicy 族系已实现，PathPolicy/ShellGuard/ToolCategory 未实施）"
- 添加注脚 "本 ADR 描述的 PathPolicy/ShellGuard/SecureToolRegistry 是设计意图；当前实际只有 IExecutionPolicy 族系（plan/agent/yolo mode）"
- **不修改** ADR-0004 内容
- 优点：保留设计意图；标注 partial 状态
- 缺点：ADR 描述与实现仍不对应

### 选项 B：重新起草 ADR-0004 描述 IExecutionPolicy 族系

- 改写 ADR-0004 内容为 `agent_mode_policy` / `plan_mode_policy` / `yolo_mode_policy` 的实际架构
- 优点：ADR 与实现一一对应
- 缺点：丢失原 ToolRegistry 安全层设计意图；需要 ADR 决策流程

### 选项 C：把 ADR-0004 移到 `docs/archive/adr/`，新建 ADR-0004 描述实际 policy 实现

- 优点：清晰分离
- 缺点：13 个已废弃 ADR 已归档，再归档一个

## 决策需求 (PENDING)

**需用户输入**：
- 选择 A / B / C / 其他？
- 是否将 ADR-0031 (Execution Policy) 与 ADR-0004 合并（两者主题交叉）？

本 change 仅创建本 audit 文件，**不预设决策**。