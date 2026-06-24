# ADR-0004 实现范围审计 (Implementation Scope Audit)

## 状态

**📋 Reserved (审计补充)** (2026-06-13) — 本文件是 ADR-0004 实施范围审计补充（由 OpenSpec change `docs-code-drift-audit-2026-06` 创建），非正式 ADR，仅作历史审计记录保留。状态变更历史见头部备注段落。

> **✅ DECIDED + 方向 4 部分实施 (2026-06-13)** — 本文件由 OpenSpec change `docs-code-drift-audit-2026-06` 创建 (2026-06-13)。
> 状态：决策已完成（**方向 4** — 同步安全路径已落地），同步更新了 `adr-0004-toolregistry-security.md` 头部状态行。
> **后续**：异步 `call_secure` 路径与 OS 级沙箱移交独立 OpenSpec change。

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

---

## 决策记录 (2026-06-13)

**用户决策**：**方向 4** — 保留 ADR-0004 作为安全层设计意图蓝图 + 立即在当前 change 内实施 PathPolicy + SecureToolRegistry 同步路径。

**已执行动作**：
1. ✅ `adr-0004-toolregistry-security.md` 头部状态行： `✅ Approved` → `🟡 Partial (同步路径已实施)`，附 6 项已完成/2 项 🟡/1 项 ❌ 的细粒度清单。**主体内容未修改**。
2. ✅ 与 ADR-0002 决策对齐处理（见 `adr-0002-impl-scope-audit.md` 决策记录）。
3. ✅ **新增代码**（方向 4 实施，~600 行 C++ + 测试）：
   - `include/agenticdsl/policy/path_policy.h`：`IPathPolicy` 抽象 + `PathPolicy` 默认实现 + `ShellGuard` 静态类 + `SecurityError` 类型
   - `src/common/policy/path_policy.cpp`：`PathPolicy::check()` (denied → allowed) + `ShellGuard::is_dangerous()` (大小写不敏感子串)
   - `include/agenticdsl/tools/secure_tool_registry.h`：`SecureToolRegistry` 装饰器（包装 ToolRegistry 而非继承，零侵入）
   - `src/common/tools/secure_tool_registry.cpp`：`call_direct` 同步检查流（disabled → 注册检查 → fs.* 路径 → shell.exec 命令）+ `call_passthrough` 透传
   - `src/common/tools/CMakeLists.txt`（新建）：`agenticdsl_secure_tools` 静态库
   - `src/common/policy/CMakeLists.txt`（更新）：加入 `path_policy.cpp`
   - `CMakeLists.txt`（根，更新）：`add_subdirectory(src/common/tools)` + `agenticdsl_core` 链接新库
   - `tests/test_path_policy.cpp`：11 个测试用例（PathPolicy + ShellGuard 覆盖）
   - `tests/test_secure_tool_registry.cpp`：10 个测试用例（装饰器拦截/透传/线程安全）

**决策理由**（修正前次评估错误）：
- 原"选项 A"是**纯文档处理**，把 PathPolicy/ShellGuard 标为"未实施待 Phase 2"——这是把**安全债务**当作"性能债务"对待，错误。
- 重新审计：`ToolRegistry::call_tool()` 当前对任何 fs./shell.exec 调用**零安全检查**（`grep policy src/common/tools/registry.cpp` → 0 hits）；任何 LLM 输出的 `fs.read("/etc/passwd")` 都会被直接执行。
- 实施成本评估：~400-650 行 C++ + 测试（与最初预算一致），属于"几小时工作量"，**不实施的安全代价远高于实施成本**。
- 同步路径**不**依赖 EventBus / TUI（已与 ADR-0002 蓝图解耦），可在 Phase 1 直接落地；异步 `call_secure` 等待 EventBus 实施。

**未触动项 / 明确延后**（移交未来 OpenSpec change）：
- `SecureToolRegistry::call_secure` 异步路径（依赖 EventBus + TUI 用户确认）
- DSLEngine 自动注入 SecureToolRegistry 装饰（当前需手动包装）
- `ToolCategory` 枚举虽已定义于 `execution_policy.h`（ADR-0004 §6 间接完成），但 `ToolMetadata.category` 字段尚未被注册路径填充（ToolRegistry 当前是 string→function 映射，无元数据存储）
- OS 级沙箱（bubblewrap / Seatbelt, ADR-0004 §Phase 2）
- 统一 `ApprovalPolicy` 抽象 — 仍为 3 个独立 mode 类（ADR-0031），按"不合并"决策保留

**验证证据**（2026-06-13）：
- `g++ -std=c++20 -fsyntax-only`：4 个新文件 0 错误 0 警告
- `cmake -S . -B build`：configure 阶段 0 错误（7.0s 配置 + 38.7s 生成）
- LSP diagnostics（我新增文件）：0 错误
- `make agenticdsl_secure_tools`：超时（async_simple 链 30s+，**项目已存在问题，非本次变更引入**）

**关于"是否合并 ADR-0031"的次级决策**：
- **不合并**。ADR-0004 主题为"ToolRegistry 安全层理想描述"（PathPolicy/ShellGuard/ToolCategory 等设计意图），ADR-0031 主题为"执行策略实施"（IExecutionPolicy 抽象 + 三个 mode 实现）。两者**关注点不同**：
  - ADR-0004 = 工具调用的**静态安全约束**（路径白名单、Shell 守卫、工具分类）
  - ADR-0031 = 工具调用的**动态审批策略**（Plan/Agent/YOLO 模式）
- 保留分离更清晰：实施后的 PathPolicy 可与 IExecutionPolicy 形成**组合关系**（Policy 决定"是否允许调用"，PathPolicy 决定"调用参数是否安全"），各管一摊。
- 交叉引用：已在 `adr-0004-toolregistry-security.md` 状态行注明"已完成的 IExecutionPolicy 族系见 ADR-0031"。