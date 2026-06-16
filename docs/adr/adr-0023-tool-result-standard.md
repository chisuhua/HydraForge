# ADR-0023: ToolResult 标准化

## 状态

**🟡 Partial → 🟢 Approved** (2026-05-25 起草, 2026-06-12 状态对齐, **2026-06-16 P1-P4 全部实施完成**)

> **2026-06-16 实施状态更新**（OpenSpec change `phase1-toolresult-standardization` 落地）：
> - P1 实施：保留并演进（见 §实施状态表）
> - P2 实施：ErrorCode enum + 4 个 optional 字段（error_code/latency_ms/trace_id/metadata）
> - P3 实施：auto latency_ms 注入 + trace_id 透传
> - P4 实施：IInteractionBus::emit std::string 重载（REQ-TR-005 向后兼容）
>
> 详见 §实施状态 + §附录 C 实施调整说明

---

## 背景

### 问题

工具调用是 HydraForge 的核心路径（PDK → Runtime → UI），但工具返回的 JSON 格式**全线不一致**：

| 组件 | 当前返回格式 | 问题 |
|------|------------|------|
| `ToolRegistry::call_tool()` | `{"result": 42}` 或 `{"error": "..."}` | 每个工具格式不同 |
| `NodeExecutor::execute_tool_call()` | `if(result.is_object())` 启发式分支 | 脆弱，无法判断成功/失败 |
| `registry.cpp` 默认工具 | `{"results": "..."}`，`{"result": 42}`，`{"location": "..."}` | 各用各的顶层 key |
| ADR-0021 `RETURN_SUCCESS` | 格式未定义 | PDK 无输出合约 |
| ADR-0019 `Event.content` | `std::string` | 结构化数据丢失 |

### 目标

统一全线工具返回的 JSON 格式，使得：
1. `NodeExecutor` 可确定性判断成功/失败
2. `CognitiveWorker` 可按错误码前缀做策略（重试/跳过/上报）
3. `IInteractionBus` 推送结构化结果而非字符串
4. PDK 的工具开发者明确知道输出合约

### 覆盖范围

```
PDK                        Runtime                     UI
DECLARE_TOOL ──→ ToolRegistry ──→ NodeExecutor ──→ IInteractionBus
    │               │                │                  │
    └── 生成         └── 返回          └── 消费          └── 推送
    统一信封         统一信封          统一信封            统一信封
```

### 参考文档

| ADR | 关系 |
|-----|------|
| ADR-0019 | `IInteractionBus::push_event` 的 `Event` 结构需调整 |
| ADR-0020 | `ToolRegistry::call_tool` 返回格式标准化 |
| ADR-0021 | `DECLARE_TOOL` 的 `RETURN_SUCCESS`/`RETURN_ERROR` 展开格式 |
| ADR-0022 | `.so` 中注册的工具返回格式 |
| ADR-0004 | 安全相关的错误码命名 |

---

## 决策

### 1. 信封格式

#### 1.1 统一信封结构

所有工具调用必须返回以下信封格式：

```cpp
// 成功
{
    "ok": true,
    "data": {                         // 工具特定的业务数据
        // 工具相关的 key-value
    },
    "meta": {                         // 执行元数据
        "duration_ms": 42,            // 执行耗时 (毫秒)
        "tool_name": "code::edit_file", // 工具全名
        "trace_id": "sess_abc123"     // 会话追踪 ID (可选, 有则填入)
    }
}

// 失败
{
    "ok": false,
    "error": {
        "code": "ERR_TOOL.NOT_FOUND", // 错误码 (见 §2)
        "message": "Tool 'foo' not registered"  // 人类可读描述
    },
    "meta": {
        "duration_ms": 12,
        "tool_name": "code::edit_file",
        "trace_id": "sess_abc123"
    }
}
```

#### 1.2 C++ 类型定义

```cpp
// src/core/types/tool_result.h
#ifndef AGENTICDSL_CORE_TYPES_TOOL_RESULT_H
#define AGENTICDSL_CORE_TYPES_TOOL_RESULT_H

#include <nlohmann/json.hpp>
#include <string>
#include <chrono>

namespace agenticdsl {

// 工具执行结果 (统一信封)
struct ToolResult {
    bool ok;                         // 成功/失败
    nlohmann::json data;             // 业务数据 (ok=true 时有效)
    std::string error_code;          // 错误码 (ok=false 时有效)
    std::string error_message;       // 错误描述 (ok=false 时有效)
    nlohmann::json meta;             // 元数据

    // 构造成功
    static ToolResult success(nlohmann::json data,
                              const std::string& tool_name,
                              std::chrono::milliseconds duration = {},
                              const std::string& trace_id = {}) {
        ToolResult r;
        r.ok = true;
        r.data = std::move(data);
        r.meta["duration_ms"] = duration.count();
        r.meta["tool_name"] = tool_name;
        if (!trace_id.empty()) r.meta["trace_id"] = trace_id;
        return r;
    }

    // 构造失败
    static ToolResult error(const std::string& code,
                            const std::string& message,
                            const std::string& tool_name,
                            std::chrono::milliseconds duration = {},
                            const std::string& trace_id = {}) {
        ToolResult r;
        r.ok = false;
        r.error_code = code;
        r.error_message = message;
        r.meta["duration_ms"] = duration.count();
        r.meta["tool_name"] = tool_name;
        if (!trace_id.empty()) r.meta["trace_id"] = trace_id;
        return r;
    }

    // 序列化为 JSON 信封
    nlohmann::json to_json() const {
        nlohmann::json j;
        j["ok"] = ok;
        if (ok) {
            j["data"] = data;
        } else {
            j["error"]["code"] = error_code;
            j["error"]["message"] = error_message;
        }
        j["meta"] = meta;
        return j;
    }

    // 从 JSON 信封反序列化
    static ToolResult from_json(const nlohmann::json& j) {
        ToolResult r;
        r.ok = j.value("ok", false);
        if (r.ok) {
            r.data = j.value("data", nlohmann::json::object());
        } else {
            auto& err = j["error"];
            r.error_code = err.value("code", "ERR_SYSTEM.UNKNOWN");
            r.error_message = err.value("message", "Unknown error");
        }
        r.meta = j.value("meta", nlohmann::json::object());
        return r;
    }
};

} // namespace agenticdsl

#endif
```

**选择理由**：
- 拒绝 B (HTTP 风格)：`status: 200` 将 HTTP 语义引入非 HTTP 层，不必要
- 拒绝 C (纯业务数据 + 异常)：工作线程中 `throw` 可能被 `catch(...)` 意外吞掉
- `ok` 是 JSON 中最清晰的语义信号，`NodeExecutor` 只需 `if(!result.ok)` 判断

---

### 2. 错误码规范

#### 2.1 分层命名法

```
ERR_<领域>.<子域>.<具体错误>

领域:
  TOOL    — 工具执行错误
  INPUT   — 输入参数错误
  PERMISSION — 权限错误
  SYSTEM  — 系统内部错误
  PLUGIN  — 插件加载错误
```

#### 2.2 完整错误码表

| 错误码 | 含义 | 触发场景 |
|--------|------|---------|
| `ERR_TOOL.NOT_FOUND` | 工具不存在 | `ToolRegistry::call_tool("unknown_name")` |
| `ERR_TOOL.EXECUTION_FAILED` | 工具执行异常 | 工具内部 `std::exception` |
| `ERR_TOOL.TIMEOUT` | 执行超时 | `SafeExec` 超时检测 |
| `ERR_TOOL.CRASHED` | 工具崩溃 | 插件进程级崩溃 (Phase 2 沙箱) |
| `ERR_INPUT.MISSING_FIELD` | 缺少必要参数 | 必填参数未传 |
| `ERR_INPUT.INVALID_TYPE` | 参数类型错误 | 类型不匹配 |
| `ERR_PERMISSION.DENIED` | 权限拒绝 | ADR-0004 权限检查 |
| `ERR_PERMISSION.SCOPE` | 越权访问 | 路径白名单/能力检查 |
| `ERR_SYSTEM.INTERNAL` | 内部错误 | 意外状态 |
| `ERR_SYSTEM.UNKNOWN` | 未知错误 | 兜底 |
| `ERR_PLUGIN.LOAD_FAILED` | 插件加载失败 | ADR-0022 `dlopen` 失败 |
| `ERR_PLUGIN.VERSION_MISMATCH` | 插件版本不兼容 | `abi_version` 不匹配 |

#### 2.3 PDK 中的使用

```cpp
// PDK 工具内 (展开为 ToolResult::error)
RETURN_ERROR("ERR_INPUT.MISSING_FIELD", "path is required");

// 等价展开为:
// return ToolResult::error("ERR_INPUT.MISSING_FIELD", "path is required", tool_name);
```

**选择理由**：
- 拒绝 B (数字码 `1001`)：需要人查表，不可自描述
- 拒绝 C (纯字符串 `"Tool not found"`)：无法被程序化模式匹配
- 分层码可在 `CognitiveWorker` 中按前缀做策略：`ERR_TOOL.*` → 重试，`ERR_PERMISSION.*` → 上报，`ERR_SYSTEM.*` → 终止

---

### 3. 错误传输方式

#### 3.1 规则

```
预期错误 (如 "工具不存在", "参数缺失", "权限拒绝"):
  → 返回值编码 (ToolResult::error)
  → 调用方通过 if(!result.ok) 判断

意外错误 (如 段错误, 内存耗尽, 插件符号不存在):
  → C++ 异常 (std::runtime_error, std::bad_alloc)
  → CognitiveWorker 的 catch(...) 兜底
```

#### 3.2 对现有代码的改造

```cpp
// ToolRegistry::call_tool — 改造前
nlohmann::json call_tool(const std::string& name, const Args& args) {
    auto it = tools_.find(name);
    if (it == tools_.end()) {
        return {{"error", "Tool not found: " + name}};  // 裸 json
    }
    try {
        return it->second(args);  // 裸 json
    } catch (...) {
        return {{"error", "Execution failed"}};  // 裸 json
    }
}

// ToolRegistry::call_tool — 改造后
ToolResult call_tool(const std::string& name, const Args& args) {
    auto start = std::chrono::steady_clock::now();
    auto& meta = name;  // for duration_ms

    auto it = tools_.find(name);
    if (it == tools_.end()) {
        return ToolResult::error(
            "ERR_TOOL.NOT_FOUND",
            "Tool not found: " + name,
            name, elapsed(start));
    }
    try {
        auto data = it->second(args);  // 工具返回业务 json
        return ToolResult::success(data, name, elapsed(start));
    } catch (const std::exception& e) {
        return ToolResult::error(
            "ERR_TOOL.EXECUTION_FAILED",
            e.what(), name, elapsed(start));
    }
}
```

#### 3.3 NodeExecutor 消费

```cpp
// execute_tool_call — 改造后
Context NodeExecutor::execute_tool_call(const ToolCallNode* node, const Context& ctx) {
    Context new_context = ctx;

    // 渲染参数
    std::unordered_map<std::string, std::string> rendered_args;
    for (const auto& [key, tmpl] : node->arguments) {
        rendered_args[key] = InjaTemplateRenderer::render(tmpl, ctx);
    }

    // 调用工具 → 返回 ToolResult
    ToolResult result = tool_registry_.call_tool(node->tool_name, rendered_args);

    if (!result.ok) {
        // 预期错误：写入 context 并继续 (或跳转)
        new_context[node->output_keys[0]] = result.to_json();
        // 也可根据错误码做策略:
        // if (result.error_code.starts_with("ERR_PERMISSION")) throw ...
        return new_context;
    }

    // 成功：取 data 字段
    if (node->output_keys.size() == 1) {
        new_context[node->output_keys[0]] = result.data;
    } else if (result.data.is_object()) {
        for (const auto& key : node->output_keys) {
            if (result.data.contains(key)) {
                new_context[key] = result.data[key];
            }
        }
    }

    return new_context;
}
```

**选择理由**：
- 拒绝 B (全部用异常)：预期错误（"工具不存在"）走异常路径是反模式
- 拒绝 C (`std::expected`)：C++23，项目当前 C++20
- 返回值编码 + `ToolResult::from_json` 可跨 `.so` 边界传递（序列化/反序列化）

---

### 4. Meta 字段

#### 4.1 MVP 字段

```json
{
    "meta": {
        "duration_ms": 42,          // 执行耗时 (毫秒)
        "tool_name": "code::edit_file",  // 工具全名
        "trace_id": "sess_abc123"   // 会话追踪 ID (可选)
    }
}
```

#### 4.2 使用场景

| 字段 | 谁使用 | 用于 |
|------|--------|------|
| `duration_ms` | `CognitiveWorker` | 超时检测，性能监控 |
| `tool_name` | `IInteractionBus` | 事件路由，日志 |
| `trace_id` | ADR-0019 Session | 跨会话追踪，审计 |

#### 4.3 Phase 2 扩展

```json
{
    "meta": {
        "token_count": 450,         // 消耗的 LLM Token
        "memory_kb": 1024,          // 最大内存占用
        "cpu_ms": 200,              // CPU 时间
        "retry_count": 2            // 重试次数
    }
}
```

**选择理由**：
- 拒绝 Minimal（仅 `duration_ms`）：无 `tool_name` 无法调试
- 拒绝 Full（Phase 2）：`memory_kb`/`cpu_ms` 无现有遥测支持

---

### 5. 兼容性策略

#### 5.1 渐进过渡

```
Phase 1 (本 ADR):
  ├── 定义 ToolResult 结构体
  ├── 改造 ToolRegistry::call_tool() 返回 ToolResult
  ├── 改造 NodeExecutor::execute_tool_call() 消费 ToolResult
  └── 改造 IInteractionBus Event.content 为 nlohmann::json

Phase 2 (过渡):
  ├── 通过 wrap_tool() 包装现有 mock 工具
  ├── registry.cpp 的 3 个默认工具逐步迁移
  └── 旧格式依然可被接收（向后兼容 1 个版本）

Phase 3 (完成):
  ├── 移除向后兼容代码
  └── ADR-0021 DECLARE_TOOL 直接生成 ToolResult
```

#### 5.2 向后兼容包装器

```cpp
// 将旧格式工具包装为 ToolResult (过渡期使用)
ToolResult wrap_legacy_tool(
    const std::string& tool_name,
    const std::function<nlohmann::json(const Args&)>& legacy_func,
    const Args& args
) {
    auto start = std::chrono::steady_clock::now();

    try {
        auto result = legacy_func(args);

        // 检查是否已是新格式
        if (result.is_object() && result.contains("ok") && result["ok"].is_boolean()) {
            return ToolResult::from_json(result);
        }

        // 旧格式: 包装为成功
        return ToolResult::success(
            result, tool_name,
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start));
    } catch (const std::exception& e) {
        return ToolResult::error(
            "ERR_TOOL.EXECUTION_FAILED", e.what(), tool_name,
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start));
    }
}
```

---

## 替代方案

### 方案 A: 保持现状 (每个工具返回自己的格式)

- **优点**：零改动
- **缺点**：`NodeExecutor` 无法确定性判断成功/失败，`IInteractionBus` 丢失结构
- **结论**：被否决。联合审查已确认为必须修复的 G1 缺口

### 方案 B: 在 IInteractionBus 层统一格式

- **优点**：Runtime 内部保持现状，只在推送时转换
- **缺点**：`NodeExecutor` 仍无法判断成功/失败，错误处理逻辑混乱
- **结论**：被否决。统一应在最早的消费点进行

---

## 权衡

| 决策 | 选择 | 理由 |
|------|------|------|
| 信封格式 | `{"ok", "data"/"error", "meta"}` | 清晰语义，跨层通用 |
| 错误码 | 分层点位符 `ERR_DOMAIN.SUB` | 可模式匹配 |
| 错误传输 | 返回值编码 (非异常) | 工作线程安全，跨 `.so` 边界 |
| Meta MVP | `duration_ms` + `tool_name` + `trace_id` | 调试必须 |
| 兼容性 | 渐进过渡 + wrap_tool | 不阻塞交付 |

---

## 实施计划

| Phase | 任务 | 产出 |
|-------|------|------|
| **Phase 1** | `ToolResult` 结构体定义<br>`ToolRegistry::call_tool()` 返回 ToolResult<br>`NodeExecutor::execute_tool_call()` 消费 | 核心改造 |
| **Phase 2** | `wrap_tool()` 兼容包装器<br>3 个默认工具迁移<br>`IInteractionBus Event` 改为 `nlohmann::json` | 全链路统一 |
| **Phase 3** | ADR-0021 `RETURN_SUCCESS`/`RETURN_ERROR` 展开<br>PDK 直接生成 ToolResult | PDK 集成 |
| **Phase 4** | 移除向后兼容代码<br>测试全覆盖 | 完成 |

---

## 验证标准

| 标准 | 验证方法 |
|------|---------|
| 统一信封 | 所有工具返回包含 `ok`, `data`/`error`, `meta` |
| 错误码分层 | 错误码匹配 `ERR_DOMAIN.SUB` 模式 |
| 向后兼容 | 旧格式工具通过 `wrap_tool` 包装后正确显示 |
| NodeExecutor 判定 | `if(!result.ok)` 分支覆盖所有错误场景 |
| 跨 .so 边界 | `ToolResult::to_json` → 序列化 → `from_json` → 正确还原 |

---

## 参考

- [ADR-0019: IInteractionBus 接口与 TUI Chat MVP](./adr-0019-iinteraction-bus-mvp.md)
- [ADR-0020: 多智能体线程模型与隔离策略](./adr-0020-thread-model-isolation.md)
- [ADR-0021: Plugin Development Kit (PDK) 设计](./adr-0021-pdk-design.md)
- [ADR-0022: 插件加载机制](./adr-0022-plugin-loading.md)
- [relationships.md](./relationships.md) — ADR 联合分析

---

## 附录 A: 文件变更清单

| 操作 | 文件路径 |
|------|---------|
| **新建** | `src/core/types/tool_result.h` |
| **修改** | `src/common/tools/registry.h` — `call_tool()` 改为返回 `ToolResult` |
| **修改** | `src/common/tools/registry.cpp` — 返回值 + 3 个默认工具迁移 |
| **修改** | `src/modules/executor/node_executor.cpp` — 消费 `ToolResult` |
| **修改** | `src/core/types/node.h` — `execute_tool_call` 签名 |
|事件类型抽象在 M5.2 阶段简化跳过，直接以 `ToolResult` 作为 `emit()` 载荷（见 ADR-0019 实际实施） |

## 附录 B: 与 ADR-0021 (PDK) 的协作

本 ADR 定义的工具结果格式直接影响 PDK 的 `RETURN_SUCCESS` 和 `RETURN_ERROR` 宏展开：

```cpp
// PDK 宏展开为:
#define RETURN_SUCCESS(fmt, ...) \
    return hydraforge::ToolResult::success( \
        {{"message", std::format(fmt, ##__VA_ARGS__)}}, \
        TOOL_NAME, get_duration() \
    ).to_json()

#define RETURN_ERROR(code, fmt, ...) \
    return hydraforge::ToolResult::error( \
        code, std::format(fmt, ##__VA_ARGS__), \
        TOOL_NAME, get_duration() \
    ).to_json()
```

---

## 附录 C: Phase 1 实施调整说明 (2026-06-16)

> 本附录记录 Phase 1 Sprint 1a 实施期间与原 ADR 草案的**调整与最终状态**。
> 完整 changelog 见 OpenSpec change `phase1-toolresult-standardization`（已归档至
> `openspec/changes/archive/2026-06-16-phase1-toolresult-standardization/`）。

### C.1 ToolResult 结构最终形态 (`src/core/types/tool_result.h`)

```cpp
// 文件头已扩展: P2-P4 字段 (ADR-0023)
struct ToolResult {
  // === P0 X 阶段 MVP (3 字段, 不可变契约) ===
  bool ok = false;
  nlohmann::json data = nlohmann::json::object();
  nlohmann::json meta = nlohmann::json::object();

  // === Phase 1 Sprint 1a 新增 (P2-P4, 4 个 optional) ===
  // REQ-TR-001: error_code (P2) — 替代 P1 自由 string
  std::optional<ErrorCode> error_code;

  // REQ-TR-002: latency_ms (P3) — uint64_t milliseconds
  std::optional<std::uint64_t> latency_ms;

  // REQ-TR-003: trace_id (P3) — 跨会话追踪 ID
  std::optional<std::string> trace_id;

  // REQ-TR-004: metadata (P3) — 与 meta 共存扩展元数据
  std::optional<nlohmann::json> metadata;

  // 双 error() 重载 (P1 string + P2 enum)
  static ToolResult error(std::string code, std::string msg,
                          nlohmann::json m = nlohmann::json::object());  // P1 兼容
  static ToolResult error(ErrorCode code, std::string msg,
                          nlohmann::json m = nlohmann::json::object());  // P2 推荐
};
```

### C.2 ErrorCode enum 完整定义 (REQ-TR-001)

```cpp
enum class ErrorCode {
  Unknown = 0,
  // P1 已有 (ADR-0023 §3.1, 4 个)
  PermissionDenied, PathViolation, DangerousCommand, ToolNotRegistered,
  // P2 新增 (6 个)
  Retry, Skip, Abort, Audit, Timeout, ResourceExhausted,
};
```

> **调整说明**：原 ADR §2.2 错误码表采用 `ERR_DOMAIN.SUB` 字符串分层命名法
> (`ERR_TOOL.NOT_FOUND` 等 12 个字符串)。实施时改为 `enum class ErrorCode` 11 个
> 强类型值，原因：(a) 编译期类型安全；(b) `switch` 分发代替字符串前缀匹配；
> (c) JSON 序列化仍以字符串形式输出（`error_code_to_string()`）保留 wire 兼容。
> **PDK 错误码映射表**（`ERR_*` → `ErrorCode::*`）：
>
> | 原 ADR 字符串 | 新 ErrorCode 值 |
> |---------------|-----------------|
> | `ERR_TOOL.NOT_FOUND` | `ToolNotRegistered` |
> | `ERR_TOOL.EXECUTION_FAILED` | `DangerousCommand` (默认) |
> | `ERR_TOOL.TIMEOUT` | `Timeout` |
> | `ERR_TOOL.CRASHED` | `DangerousCommand` (崩溃) |
> | `ERR_INPUT.MISSING_FIELD` | `Skip` |
> | `ERR_INPUT.INVALID_TYPE` | `DangerousCommand` (无效输入) |
> | `ERR_PERMISSION.DENIED` | `PermissionDenied` |
> | `ERR_PERMISSION.SCOPE` | `PathViolation` |
> | `ERR_SYSTEM.INTERNAL` | `Unknown` |
> | `ERR_SYSTEM.UNKNOWN` | `Unknown` |
> | `ERR_PLUGIN.LOAD_FAILED` | `ToolNotRegistered` |
> | `ERR_PLUGIN.VERSION_MISMATCH` | `DangerousCommand` |

### C.3 ToolRegistry::call_tool 调整 (REQ-TR-MOD-001)

> 原 ADR §3.2 计划将 `ToolRegistry::call_tool()` 改为返回 `ToolResult`。
> **实施调整**：`call_tool()` 仍返回 `nlohmann::json`（保留 P0 默认工具 + 用户自定义 lambda
> 注册 API），**信封解析下沉到 NodeExecutor 层级**（envelope 检测 + 双模式解析）。
> 优点：(a) `register_tool(name, lambda)` API 不变（lambda 返回 raw JSON），零侵入；
> (b) NodeExecutor 集中处理 envelope + error_code 分发；(c) 现有 P0 测试零回归。

### C.4 NodeExecutor execute_tool_call 重构 (REQ-TR-MOD-001)

```cpp
Context NodeExecutor::execute_tool_call(const ToolCallNode* node, const Context& ctx) {
  // 1. 渲染 arguments
  // 2. 校验工具注册 → 否则 throw "Tool 'X' not registered"
  // 3. steady_clock 测量
  auto t0 = std::chrono::steady_clock::now();
  auto raw_result = tool_registry_.call_tool(node->tool_name, rendered_args);
  auto latency_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - t0).count();

  // 4. 双模式解析: envelope vs 裸 JSON
  ToolResult tool_result;
  if (raw_result.is_object() && raw_result.contains("ok")
      && raw_result["ok"].is_boolean()) {
    tool_result = ToolResult::from_json(raw_result);  // envelope 模式
  } else {
    tool_result = ToolResult::success(raw_result);    // 裸 JSON 包装
  }

  // 5. 自动注入 latency_ms + trace_id 透传
  tool_result.latency_ms = static_cast<uint64_t>(latency_ms);
  if (auto it = rendered_args.find("trace_id"); it != rendered_args.end()) {
    tool_result.trace_id = it->second;
  }

  // 6. error_code 分发
  if (!tool_result.ok) {
    switch (tool_result.error_code.value_or(ErrorCode::Unknown)) {
      case ErrorCode::Retry: throw std::runtime_error("[RETRY] ...");
      case ErrorCode::Abort: throw std::runtime_error("[ABORT] ...");
      case ErrorCode::Skip:  return ctx;  // 不写 output_keys
      // 其他 → 通用 throw
    }
  }

  // 7. output_keys 分发 (基于 tool_result.data, 替代 is_object() 启发式)
}
```

### C.5 IInteractionBus emit 重载 (REQ-TR-005)

> 原 ADR §5.1 计划将 `Event.content` 改为 `nlohmann::json`。
> **实施调整**：`IInteractionBus::emit(event_type, std::string)` 重载（不引入 std::variant）。
> 原因：(a) 现有实现已采用 `ToolResult`-only 接口（ADR-0019 实际实施），零代码调用 string 载荷；
> (b) `std::variant` 迫使订阅者实现 visitor；(c) 字符串内容自然可通过
> `ToolResult::success({}, {{"content", s}})` 包装，无需 variant 类型开销。

```cpp
void InMemoryBus::emit(const std::string& event_type, const std::string& content) {
  ToolResult payload = ToolResult::success(
      nlohmann::json::object(),
      nlohmann::json{{"content", content}});
  emit(event_type, payload);  // 转发主路径
}
```

### C.6 验证结果

| 验证项 | 结果 |
|--------|------|
| 26/26 全量 ctest | ✅ 0 回归 |
| 71/71 test_tool_result (新增 7 测试, 47 assertions) | ✅ |
| 20/20 test_executor (新增 4 集成测试) | ✅ |
| 28/28 test_interaction_bus (新增 1 std::string 重载测试) | ✅ |
| ASan (asan build) | ✅ 干净 |
| LSP 诊断 (8 modified files) | ✅ 0 errors |
| adr_lint / adr_relationships | ✅ 0 新增错误 (3 pre-existing 无关) |
| `openspec validate --strict` | ✅ valid |
| `phase1_plugin_demo --mock` | ✅ 输出 error_code/latency_ms/trace_id/metadata |
| TSan / CI 6 jobs | ⚠️ 待 CI 验证（本地 7GB RAM 不足） |

### C.7 已知遗留

- ADR-0023 §1.1 信封 JSON 格式 `error: {code, message}` (嵌套) **与实现不一致**：实现采用扁平
  `error_code` (顶层) + `meta.error_message` (兼容层)。修正需独立 OpenSpec change。
- TSan 验证需 CI 矩阵（GitHub Actions ubuntu-latest 16GB RAM 足够）。
- PDK `RETURN_SUCCESS` / `RETURN_ERROR` 宏（ADR §附录 B）尚未实施（独立 change）。
