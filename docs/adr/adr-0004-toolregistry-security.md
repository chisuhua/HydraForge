# ADR-0004: ToolRegistry 安全模型

## 状态

**🟡 Partial (同步路径已实施)** (2026-06-13) — **V2 版** 设计文档已锁定；当前实现：

- ✅ `IExecutionPolicy` 族系 (`plan_mode_policy` / `agent_mode_policy` / `yolo_mode_policy`) — ADR-0031
- ✅ `ToolCategory` / `ApprovalPolicy` / `ToolMetadata` / `ToolCallContext` / `LayerProfile` 值类型 — `src/common/policy/execution_policy.h`
- ✅ **`PathPolicy` (同步路径)** — `include/agenticdsl/policy/path_policy.h` + `src/common/policy/path_policy.cpp`
- ✅ **`ShellGuard` (同步路径，子串检测)** — 同上头文件
- ✅ **`SecureToolRegistry` 装饰器 (同步 `call_direct` + `call_passthrough`)** — `include/agenticdsl/tools/secure_tool_registry.h` + `src/common/tools/secure_tool_registry.cpp`
- 🟡 `call_secure` 异步路径 (依赖 EventBus + TUI 确认) — 待 Phase 2
- 🟡 ToolRegistry ↔ PathPolicy 自动注入 (当前需手动 `SecureToolRegistry` 装饰) — 待 Engine 集成
- ❌ OS 级沙箱 (bubblewrap / Seatbelt) — 待 Phase 2 独立 OpenSpec change

> **2026-06-13 审计备注（OpenSpec change `docs-code-drift-audit-2026-06`）**：状态由 "✅ Approved" 调整为 "🟡 Partial"。
> - 事实依据：`grep -rn "class.*\(PathPolicy\|ShellGuard\|SecureToolRegistry\|ToolCategory\|ApprovalPolicy\)" src/ include/` → 0 hits（统一 ApprovalPolicy 抽象未实现，仅三个具体 mode 类）。
> - 决策：保留本 ADR 作为 ToolRegistry 安全层的**设计意图蓝图**，**不修改主体内容**。
> - 已完成部分（ADR-0031 议题 3 实施）：
>   - `src/common/policy/execution_policy.h` (IExecutionPolicy 抽象)
>   - `src/common/policy/{plan,agent,yolo}_mode_policy.{h,cpp}`（三个具体实现）
> - 未实施部分（需独立 OpenSpec change 评估）：
>   - `PathPolicy`（路径白名单/黑名单）
>   - `ShellGuard`（Shell 命令白名单/参数校验）
>   - `SecureToolRegistry`（ToolRegistry 安全包装）
>   - `ToolCategory` 枚举（ReadOnly/WriteFile/Execute/Network/StateModify）
>   - 统一 `ApprovalPolicy` 抽象（目前是三个独立类）
>   - ToolRegistry 与 Policy 的集成绑定（目前松散关联）
> - 未来触发条件：若 Phase 2 出现 OS 级工具调用需求（fs.read / shell.exec / network）且无沙箱兜底，**重新评估** PathPolicy/ShellGuard 实施；否则 IExecutionPolicy 族系继续承担审批职责。

## 背景

HydraForge Agent 通过 ToolRegistry 调用外部工具（文件系统、Shell、网络等）。LLM 可能生成恶意的工具调用（如 `fs.read /etc/passwd`、`shell.exec rm -rf /`）。Phase 1 需要基础安全防护，Phase 2/3 需要 OS 级沙箱隔离。

**参考框架**：
- Claude Code：分层权限 (Allow/Ask/Deny) + bubblewrap/Seatbelt 沙箱 + 用户确认
- DeepSeek-TUI：3 种执行模式 + Workspace 边界 + 审批门控
- DeerFlow 2.0：配置化工具 + Docker 容器隔离

**核心原则**：纵深防御 = 技术控制 + 权限分层 + 用户确认

> **V2 变更**：
> - 增加 **ToolCategory**（工具安全分类：ReadOnly/WriteFile/Execute/Network/StateModify）
> - 增加 **ApprovalPolicy**（三模式审批策略：Plan/Agent/YOLO）
> - 增加 **LayerProfile**（调用层级限制：Workflow/Thinking/Cognitive）
> - 与 ADR-0031 (IExecutionPolicy) 对齐，将 Allow/Ask/Deny 映射到 ApprovalPolicy

---

## 决策

### 1. 安全架构：三层防御

```
┌─────────────────────────────────────────────────────────────┐
│  Layer 1: 权限分层 (Permission Tiers)                        │
│  Allow → Ask → Deny                                        │
│                                                              │
│  Layer 2: 路径策略 (Path Policy)                             │
│  allowed_prefixes + denied_patterns                         │
│                                                              │
│  Layer 3: Shell 参数校验 (Shell Guard)                       │
│  危险命令检测                                               │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│  用户确认层 (User Confirmation)                              │
│  EventBus → USER_INPUT → TUI 确认对话框                      │
│  用户点击"确认"或"拒绝"                                      │
└─────────────────────────────────────────────────────────────┘
```

### 2. 权限分层：Allow/Ask/Deny 三级

```cpp
// ============================================================
// 权限级别
// ============================================================

enum class ToolPermission {
    Allow,   // 直接执行，无确认
    Ask,     // 发送 USER_INPUT 事件，等待用户确认
    Deny     // 直接拒绝，返回错误
};

// ============================================================
// 工具规格
// ============================================================

struct ToolSpec {
    std::string name;
    ToolPermission permission = ToolPermission::Ask;
    std::optional<PathPolicy> path_policy;  // 文件系统工具的路径策略
    std::optional<std::string> description;   // 用户可见的描述
    std::optional<std::string> risk_level;   // "low", "medium", "high", "critical"
};

// ============================================================
// 默认安全配置
// ============================================================

struct ToolSecurityConfig {
    // 默认权限
    ToolPermission default_permission = ToolPermission::Ask;

    // 默认 Ask 的工具（需要确认）
    std::vector<std::string> ask_tools = {
        "fs.write",
        "fs.delete",
        "shell.exec"
    };

    // 默认 Deny 的工具（危险）
    std::vector<std::string> deny_tools = {
        "net.http_post"  // 防止数据外泄
    };

    // 默认 Allow 的工具（安全）
    std::vector<std::string> allow_tools = {
        "web.search",
        "calculate",
        "llm.call"
    };

    // 路径策略（用于 fs.* 工具）
    PathPolicy fs_policy;
};

// ============================================================
// 危险级别定义
// ============================================================

enum class RiskLevel {
    Low,      // 只读，无副作用
    Medium,   // 有副作用但可恢复
    High,     // 不可逆操作
    Critical  // 系统级操作，可能影响安全
};

std::string to_string(RiskLevel r) {
    switch (r) {
        case RiskLevel::Low: return "low";
        case RiskLevel::Medium: return "medium";
        case RiskLevel::High: return "high";
        case RiskLevel::Critical: return "critical";
    }
}
```

### 3. 路径策略：组合模式

```cpp
// ============================================================
// 路径策略
// ============================================================

struct PathPolicy {
    // 允许的前缀目录（jail root）
    std::vector<std::string> allowed_prefixes = {
        "/tmp/hydraforge",      // 临时工作区
        "./workspace"            // 项目工作区（相对路径）
    };

    // 拒绝的模式（正则表达式，优先级更高）
    std::vector<std::regex> denied_patterns = {
        std::regex(R"(/etc/passwd)"),
        std::regex(R"(/\.ssh/)"),
        std::regex(R"(/proc/)"),
        std::regex(R"(/\.aws/)"),
        std::regex(R"(C:\\Windows)"),     // Windows 系统目录
        std::regex(R"(/\.config/)")
    };

    // 检查结果
    struct CheckResult {
        bool allowed;
        std::string reason;
        std::optional<std::string> matched_denied;  // 匹配到的拒绝模式
    };

    CheckResult check(const std::string& path) const {
        // 1. 先检查 denied patterns（优先级高）
        std::string canonical;
        try {
            canonical = std::filesystem::canonical(path);
        } catch (...) {
            return {false, "invalid_path", std::nullopt};
        }

        for (const auto& pattern : denied_patterns) {
            if (std::regex_search(canonical, pattern)) {
                return {false, "path_matches_denied_pattern", pattern.str()};
            }
        }

        // 2. 再检查 allowed prefixes
        bool in_allowed = allowed_prefixes.empty();  // 空 = 允许所有
        for (const auto& prefix : allowed_prefixes) {
            if (canonical.starts_with(prefix)) {
                in_allowed = true;
                break;
            }
        }

        if (!in_allowed) {
            return {false, "path_not_in_allowed_prefix", std::nullopt};
        }

        return {true, "allowed", std::nullopt};
    }
};
```

### 4. Shell 参数校验

```cpp
// ============================================================
// Shell 命令危险检测
// ============================================================

struct ShellGuard {
    // 危险命令模式
    static constexpr std::array DANGEROUS_PATTERNS = {
        "rm -rf",
        "rm -r /",
        "dd if=",
        "mkfs",
        "| bash",
        "; bash",
        "&& bash",
        "> /dev/",
        "curl | sh",
        "wget | sh",
        "chmod 777",
        "chown root"
    };

    // 危险信号检测
    static bool is_dangerous(const std::string& command) {
        std::string lower_cmd;
        lower_cmd.reserve(command.size());
        std::transform(command.begin(), command.end(), std::back_inserter(lower_cmd),
                      ::tolower);

        for (const auto& pattern : DANGEROUS_PATTERNS) {
            if (lower_cmd.find(pattern) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    // 建议替换命令
    static std::optional<std::string> suggest_safe_alternative(const std::string& command) {
        if (command.find("rm -rf") != std::string::npos) {
            return "rm -i (interactive mode)";
        }
        if (command.find("| bash") != std::string::npos) {
            return "use a dedicated tool instead";
        }
        return std::nullopt;
    }
};
```

### 5. 增强的 SecureToolRegistry

```cpp
// ============================================================
// 安全错误类型
// ============================================================

struct SecurityError {
    enum class Code {
        PermissionDenied,     // 权限不足
        PathViolation,        // 路径违规
        DangerousCommand,      // 危险命令
        UserCancelled,        // 用户取消
        Unknown
    };
    Code code;
    std::string message;
    std::string tool_name;
    std::string details;
};

// ============================================================
// SecureToolRegistry
// ============================================================

class SecureToolRegistry {
public:
    explicit SecureToolRegistry(ToolSecurityConfig config)
        : config_(std::move(config)) {
        // 初始化默认工具权限
        initialize_default_permissions();
    }

    // -------------------- 安全调用接口 --------------------

    // 带安全检查的工具调用（用于需要用户确认的场景）
    // 返回 expected：成功返回结果，失败返回 SecurityError
    std::expected<nlohmann::json, SecurityError> call_secure(
        const std::string& tool_name,
        const nlohmann::json& args,
        EventBus* event_bus,
        std::stop_token cancel_token = {}
    );

    // 直接执行（仅用于 Allow 级别工具）
    std::expected<nlohmann::json, SecurityError> call_direct(
        const std::string& tool_name,
        const nlohmann::json& args
    );

    // -------------------- 权限管理 --------------------

    void set_permission(const std::string& tool_name, ToolPermission perm) {
        std::lock_guard lock(mutex_);
        tool_specs_[tool_name].permission = perm;
    }

    void set_path_policy(const std::string& tool_name, PathPolicy policy) {
        std::lock_guard lock(mutex_);
        tool_specs_[tool_name].path_policy = std::move(policy);
    }

    ToolPermission get_permission(const std::string& tool_name) const {
        std::shared_lock lock(mutex_);
        auto it = tool_specs_.find(tool_name);
        return it != tool_specs_.end() ? it->second.permission : config_.default_permission;
    }

    // -------------------- 底层注册（保持兼容） --------------------

    void register_tool(const std::string& name, ToolFunc func) {
        base_registry_.register_tool(name, std::move(func));
    }

    bool has_tool(const std::string& name) const {
        return base_registry_.has_tool(name);
    }

private:
    ToolRegistry base_registry_;                    // 底层注册表
    mutable std::shared_mutex mutex_;              // 读写锁
    std::unordered_map<std::string, ToolSpec> tool_specs_;
    ToolSecurityConfig config_;

    // 内部安全检查
    std::expected<void, SecurityError> check_security(
        const std::string& tool_name,
        const nlohmann::json& args
    );

    // 发送用户确认请求
    std::expected<nlohmann::json, SecurityError> request_confirmation(
        const std::string& tool_name,
        const nlohmann::json& args,
        EventBus* event_bus,
        std::stop_token cancel_token
    );

    void initialize_default_permissions();
};

// ============================================================
// 安全检查实现
// ============================================================

std::expected<void, SecurityError> SecureToolRegistry::check_security(
    const std::string& tool_name,
    const nlohmann::json& args
) {
    auto permission = get_permission(tool_name);

    // Deny：直接拒绝
    if (permission == ToolPermission::Deny) {
        return std::unexpected(SecurityError{
            SecurityError::Code::PermissionDenied,
            "Tool '" + tool_name + "' is denied by security policy",
            tool_name,
            "permission=deny"
        });
    }

    // Ask 和 Allow 都需要检查路径/参数
    if (tool_name.starts_with("fs.")) {
        if (args.contains("path")) {
            auto path_result = config_.fs_policy.check(args["path"]);
            if (!path_result.allowed) {
                return std::unexpected(SecurityError{
                    SecurityError::Code::PathViolation,
                    "Path '" + args["path"] + "' " + path_result.reason,
                    tool_name,
                    path_result.matched_denied.value_or("unknown")
                });
            }
        }
    }

    if (tool_name == "shell.exec" && args.contains("command")) {
        if (ShellGuard::is_dangerous(args["command"])) {
            return std::unexpected(SecurityError{
                SecurityError::Code::DangerousCommand,
                "Shell command contains dangerous pattern",
                tool_name,
                args["command"]
            });
        }
    }

    return {};
}

// ============================================================
// 用户确认流程
// ============================================================

std::expected<nlohmann::json, SecurityError> SecureToolRegistry::request_confirmation(
    const std::string& tool_name,
    const nlohmann::json& args,
    EventBus* event_bus,
    std::stop_token cancel_token
) {
    if (!event_bus) {
        return std::unexpected(SecurityError{
            SecurityError::Code::PermissionDenied,
            "No EventBus provided for interactive confirmation",
            tool_name,
            "ask_mode_requires_eventbus"
        });
    }

    // 构造确认请求事件
    UIEvent confirm_request;
    confirm_request.type = EventType::USER_INPUT;
    confirm_request.priority = EventPriority::Critical;
    confirm_request.payload = {
        {"intent", "tool_confirmation"},
        {"tool", tool_name},
        {"args", args},
        {"risk_level", get_risk_level(tool_name)},
        {"description", tool_specs_[tool_name].description.value_or("")}
    };

    // 创建 promise/future 等待用户响应
    std::promise<std::optional<bool>> promise;
    auto future = promise.get_future();

    // 注册一次性回调
    event_bus->subscribe(EventType::USER_INPUT, [&, tool_name](const UIEvent& response) {
        if (response.payload.value("intent", "") == "tool_confirmation_response" &&
            response.payload.value("tool", "") == tool_name) {
            promise.set_value(response.payload.value("approved", false));
        }
    });

    // 发送确认请求
    event_bus->push(confirm_request);

    // 等待用户响应（或取消）
    while (future.wait_for(100ms) == std::future_status::timeout) {
        if (cancel_token.stop_requested()) {
            promise.set_value(std::nullopt);  // 用户取消
            break;
        }
    }

    auto result = future.get();
    if (!result.has_value()) {
        return std::unexpected(SecurityError{
            SecurityError::Code::UserCancelled,
            "User cancelled tool execution",
            tool_name,
            "cancelled"
        });
    }

    if (!result.value()) {
        return std::unexpected(SecurityError{
            SecurityError::Code::UserCancelled,
            "User denied tool execution",
            tool_name,
            "denied_by_user"
        });
    }

    // 用户确认后执行
    return call_direct(tool_name, args);
}

// ============================================================
// 主入口
// ============================================================

std::expected<nlohmann::json, SecurityError> SecureToolRegistry::call_secure(
    const std::string& tool_name,
    const nlohmann::json& args,
    EventBus* event_bus,
    std::stop_token cancel_token
) {
    // 1. 安全检查
    if (auto check = check_security(tool_name, args); !check) {
        return std::unexpected(check.error());
    }

    auto permission = get_permission(tool_name);

    // 2. 根据权限执行
    if (permission == ToolPermission::Allow) {
        return call_direct(tool_name, args);
    } else if (permission == ToolPermission::Ask) {
        return request_confirmation(tool_name, args, event_bus, cancel_token);
    }

    return std::unexpected(SecurityError{
        SecurityError::Code::Unknown,
        "Unknown permission",
        tool_name,
        "invalid_permission"
    });
}
```

### 6. 用户确认 TUI 集成

```cpp
// ============================================================
// TUI 确认对话框组件
// ============================================================

class ConfirmationDialog {
public:
    struct Response {
        bool approved;
        bool dont_ask_again;  // 可选：记住用户选择
    };

    // 显示确认对话框，返回用户选择
    std::future<Response> show(const UIEvent& request) {
        promise_ = std::promise<Response>{};
        return promise_.get_future();
    }

    // 从 EventBus 处理确认响应
    void on_user_response(const UIEvent& response) {
        if (promise_) {
            Response r{
                .approved = response.payload.value("approved", false),
                .dont_ask_again = response.payload.value("dont_ask_again", false)
            };
            promise_.set_value(r);
            promise_ = std::nullopt;
        }
    }

private:
    std::optional<std::promise<Response>> promise_;
};

// TUI 中的使用
void HarnessTUI::handle_user_input(const UIEvent& ev) {
    if (ev.payload.value("intent", "") == "tool_confirmation") {
        auto dialog = std::make_shared<ConfirmationDialog>();
        auto response_future = dialog->show(ev);

        // 在 TUI 中渲染确认对话框
        render_confirmation_dialog(
            ev.payload["tool"],
            ev.payload["args"],
            ev.payload["risk_level"]
        );

        // 等待响应后发送回 EventBus
        response_future.then([this, ev](auto response) {
            UIEvent resp;
            resp.type = EventType::USER_INPUT;
            resp.payload = {
                {"intent", "tool_confirmation_response"},
                {"tool", ev.payload["tool"]},
                {"approved", response.approved},
                {"dont_ask_again", response.dont_ask_again}
            };
            agent_->event_bus()->push(resp);
        });
    }
}
```

---

## 配置示例

```json
// tool_security.json
{
    "default_permission": "ask",
    "ask_tools": ["fs.write", "fs.delete", "shell.exec"],
    "deny_tools": ["net.http_post"],
    "allow_tools": ["web.search", "calculate", "llm.call"],
    "path_policy": {
        "allowed_prefixes": [
            "/tmp/hydraforge",
            "./workspace"
        ],
        "denied_patterns": [
            "/etc/passwd",
            "/\\.ssh/",
            "/proc/",
            "/\\.aws/"
        ]
    }
}
```

---

## Phase 2/3 扩展

### Phase 2: OS 级沙箱

| 平台 | 技术 | 实现方式 |
|------|------|---------|
| Linux | Landlock | 内核 5.13+，轻量级系统调用过滤 |
| macOS | Seatbelt | App Sandbox，签名验证 |
| Windows | Windows Sandbox | 容器隔离 |

```cpp
// Phase 2: 沙箱执行器
class SandboxedExecutor {
    // Landlock (Linux)
    void apply_landlock_rules(const std::vector<LandlockRule>& rules);

    // Seatbelt (macOS)
    void apply_sandbox_profile(const std::string& profile);

    // 执行在沙箱中
    std::expected<json, Error> execute_in_sandbox(
        const std::string& tool_name,
        const json& args
    );
};
```

### Phase 3: 容器级隔离

```cpp
// Phase 3: 容器执行器（用于运行不受信任的 Agent）
class ContainerExecutor {
    std::string image_;  // Docker 镜像

    // 启动隔离容器
    void start();

    // 在容器中执行工具
    std::future<json> execute(const std::string& cmd);

    // 停止容器
    void stop();
};
```

---

## 权衡

### 为什么三层防御？

| 层级 | 作用 | 失效时的保护 |
|------|------|-------------|
| 权限分层 | 阻止所有高危工具 | 用户确认作为兜底 |
| 路径策略 | 阻止路径遍历攻击 | Deny 模式兜底 |
| Shell 校验 | 阻止危险命令 | 禁用 Shell 工具兜底 |

### 为什么 Ask 是默认而非 Deny？

- 完全 Deny 会阻止正常开发流程
- Ask 模式让用户决定，平衡安全与效率
- 用户可以在配置中调整为 Deny

---

## 实现要求

### Phase 1 必须完成

| # | 任务 | 验证方式 |
|---|------|---------|
| 1 | SecureToolRegistry 核心实现 | 单元测试：fs.read /etc/passwd 被拒绝 |
| 2 | 路径策略 (组合模式) | 测试：allowed_prefixes + denied_patterns |
| 3 | Shell 校验 | 测试：rm -rf / 被拒绝，ls /tmp 允许 |
| 4 | Ask 模式 + EventBus | 集成测试：TUI 显示确认对话框 |
| 5 | 配置文件加载 | JSON 配置正确解析并应用 |

### 安全测试用例

```cpp
TEST_CASE("SecureToolRegistry blocks path traversal") {
    SecureToolRegistry registry(config_with_path_policy);

    // 尝试读取 /etc/passwd
    auto result = registry.call_secure("fs.read",
        {{"path", "/etc/passwd"}}, nullptr);

    REQUIRE(!result);
    CHECK(result.error().code == SecurityError::Code::PathViolation);
}

TEST_CASE("SecureToolRegistry blocks dangerous shell") {
    SecureToolRegistry registry(default_config);

    // 尝试 rm -rf
    auto result = registry.call_secure("shell.exec",
        {{"command", "rm -rf /home"}}, nullptr);

    REQUIRE(!result);
    CHECK(result.error().code == SecurityError::Code::DangerousCommand);
}

TEST_CASE("Ask mode triggers confirmation") {
    SecureToolRegistry registry(config_ask_mode);
    MockEventBus bus;

    auto result = registry.call_secure("fs.write",
        {{"path", "./workspace/test.txt"}, {"content", "hello"}}, &bus);

    // 应该返回错误（需要用户确认）
    REQUIRE(!result);
    CHECK(result.error().code == SecurityError::Code::PermissionDenied);
    CHECK(bus.published<EventType::USER_INPUT>());  // 确认事件已发布
}
```

---

## 影响范围

| 组件 | 变更 |
|------|------|
| `src/common/tools/registry.h/cpp` | SecureToolRegistry 新增 |
| `src/harness/event_bus.h/cpp` | USER_INPUT 事件类型 |
| `src/harness/tui/harness_tui.h/cpp` | 确认对话框组件 |
| `src/harness/tools/filesystem_tools.h/cpp` | 集成 PathPolicy |
| `src/harness/tools/shell_tools.h/cpp` | 集成 ShellGuard |

---

## 替代方案

### 替代 1：仅黑名单（被否决）

```cpp
std::vector<std::string> blocked = {"/etc/passwd", "~/.ssh"};
```

**否决理由**：永远有遗漏，攻击者会找到新路径。

### 替代 2：完全禁用 Shell（被否决）

**否决理由**：Shell 对真实工作流是必要的。Claude Code 等成熟框架都支持 Shell，只是不直接允许。

### 替代 3：OS 沙箱优先（被否决）

**否决理由**：Phase 1 复杂度过高。Landlock/seccomp 实现复杂，且需要内核支持。先实现应用层安全，Phase 2 再加 OS 沙箱。

---

## 结论

采用三层纵深防御架构：

- **Layer 1**: Allow/Ask/Deny 权限分层
- **Layer 2**: 路径策略（允许前缀 + 拒绝模式）
- **Layer 3**: Shell 命令危险检测
- **用户确认层**: EventBus USER_INPUT + TUI 确认对话框

---

## V2 新增：与 ADR-0031 (IExecutionPolicy) 对齐

### 6. ToolCategory（工具安全分类）

```cpp
// ============================================================
// V2: 工具安全分类
// ============================================================

enum class ToolCategory {
    ReadOnly,       // 只读操作：ls, cat, grep, search
    WriteFile,      // 文件写入：edit_file, create_file, delete_file
    Execute,        // 命令执行：exec_shell, run_tests
    Network,        // 网络操作：http_request, api_call
    StateModify     // 状态修改：set_mode, clear_context
};
```

### 7. ApprovalPolicy（三模式审批策略）

```cpp
// ============================================================
// V2: 审批策略（与 ADR-0031 IExecutionPolicy 对齐）
// ============================================================

struct ApprovalPolicy {
    bool requires_approval_in_plan = true;    // Plan 模式
    bool requires_approval_in_agent = true;   // Agent 模式
    bool requires_approval_in_yolo = false;   // YOLO 模式
    
    // 安全底线：即使 YOLO 也强制审批的操作
    bool force_approval_always = false;       // 用于 delete_file, rm -rf 等
};

// ADR-0004 Allow/Ask/Deny → ApprovalPolicy 映射
ApprovalPolicy map_permission(ToolPermission perm) {
    switch (perm) {
        case ToolPermission::Allow:
            return {false, false, false, false};  // 所有模式免审
        case ToolPermission::Ask:
            return {true, true, false, false};    // Plan/Agent 审批
        case ToolPermission::Deny:
            return {true, true, true, true};      // 所有模式拒绝（force_approval）
    }
}
```

### 8. LayerProfile（调用层级限制）

```cpp
// ============================================================
// V2: Layer Profile（哪些层级可以调用此工具）
// ============================================================

enum class LayerProfile {
    Workflow   = 0,   // L2：完全允许，沙箱内
    Thinking   = 1,   // L3：只读工具
    Cognitive  = 2    // L4：禁止 tool_call（仅 state.read）
};

// 层级权限检查
bool check_layer_permission(ToolCategory category, LayerProfile caller_layer) {
    switch (caller_layer) {
        case LayerProfile::Workflow:
            return true;  // L2 可以调用所有工具
        case LayerProfile::Thinking:
            return category == ToolCategory::ReadOnly;  // L3 只能调用只读工具
        case LayerProfile::Cognitive:
            return false;  // L4 禁止所有工具调用
    }
}
```

### 9. 完整的工具元数据（V2）

```cpp
// ============================================================
// V2: 完整的工具元数据
// ============================================================

struct ToolMetadata {
    std::string name;                   // "code::edit_file"
    std::string description;            // "编辑指定文件"
    std::string domain;                 // "code"
    
    ToolCategory category;              // 安全分类
    LayerProfile min_layer;             // 最低调用层级
    ApprovalPolicy approval;            // 审批策略
    
    // 参数 schema（用于校验）
    nlohmann::json param_schema;
    
    // 预算消耗预估
    struct CostHint {
        bool consumes_llm_tokens = false;
        int estimated_duration_ms = 100;
    } cost_hint;
};

// 编程助手工具注册示例
void register_code_tools(ToolRegistry& registry) {
    // 只读工具——所有模式免审批
    registry.register_tool({
        .name = "code::read_file",
        .category = ToolCategory::ReadOnly,
        .min_layer = LayerProfile::Thinking,
        .approval = {false, false, false, false}
    }, &impl_read_file);
    
    // 写入工具——Plan/Agent 需审批，YOLO 免审批
    registry.register_tool({
        .name = "code::edit_file",
        .category = ToolCategory::WriteFile,
        .min_layer = LayerProfile::Workflow,
        .approval = {true, true, false, false}
    }, &impl_edit_file);
    
    // 危险工具——所有模式都需审批（安全底线）
    registry.register_tool({
        .name = "code::delete_file",
        .category = ToolCategory::WriteFile,
        .min_layer = LayerProfile::Workflow,
        .approval = {true, true, true, true}  // force_approval_always!
    }, &impl_delete_file);
}
```

### 10. 决策矩阵

| 工具 | Category | Plan | Agent | YOLO | Layer |
|------|----------|:----:|:-----:|:----:|:-----:|
| code::read_file | ReadOnly | 免审 | 免审 | 免审 | L3+ |
| code::list_dir | ReadOnly | 免审 | 免审 | 免审 | L3+ |
| code::edit_file | WriteFile | **审批** | **审批** | 免审 | L2 |
| code::delete_file | WriteFile | **审批** | **审批** | **审批** | L2 |
| code::exec_shell | Execute | **审批** | **审批** | **审批** | L2 |
| code::run_tests | Execute | **审批** | 免审 | 免审 | L2 |

---

## 结论

采用纵深防御安全架构：

- **Layer 1**: 权限分层（Allow/Ask/Deny）→ 映射到 ApprovalPolicy
- **Layer 2**: 路径策略（允许前缀 + 拒绝模式）
- **Layer 3**: Shell 命令危险检测
- **V2 新增**: ToolCategory + LayerProfile + ApprovalPolicy
- **用户确认层**: EventBus USER_INPUT + TUI 确认对话框
- **ADR-0031 集成**: IExecutionPolicy 定义三模式行为

此设计支持：
- **Phase 1**：基础安全防护，无需 OS 沙箱
- **Phase 2**：Landlock/Seatbelt OS 级隔离
- **Phase 3**：容器级完全隔离

---

## 与 ADR-0036 的集成补充

### IExecutionPolicy 注入机制

根据 ADR-0036 的混合内核架构，`call_tool_with_policy()` 归属基座层 `ToolCoordinator`（中间件），认知层负责注入 `IExecutionPolicy*`。

**注入流程**：

```
应用启动时：
  InfrastructureServices infra;               // 基座层
  auto cognitive = make_unique<CognitiveOrch>(infra);
  
  // 认知层创建时构造自己的 Policy
  auto policy = make_unique<PlanModePolicy>();
  
  // 认知层启动 ToolCoordinator，注入 policy
  ToolCoordinator coordinator(
      infra.tool_registry,
      infra.event_bus,
      policy.get(),          // IExecutionPolicy* 由认知层注入
      /* PreviewGenerator 由领域层注册 */
  );
  
  infra.tool_coordinator = &coordinator;
  infra.cognitive = std::move(cognitive);
```

**调用链**：

```
CognitiveOrchestrator
  → ToolCoordinator.call_with_policy(tool_call)     // 基座层中间件
    → IExecutionPolicy::requires_approval()            // 认知层策略
    → [如需审批] PreviewGenerator::generate()          // 领域层预览
    → [EventBus] 等待用户确认                         // 基座层通信
    → ToolRegistry::call()                             // 基座层执行
    → [EventBus] emit("tool.call.finished")            // 基座层审计
```

**关键原则**：
- `ToolCoordinator` 在基座层，但**不硬编码**任何审批逻辑——所有策略决策委托给 `IExecutionPolicy*`
- 认知层可以通过切换 policy 指针实现模式切换（Plan → Agent → YOLO）
- 领域层通过 `PreviewGenerator` 接口注册预览生成器（延迟到 IDomainAgent 就绪后）

---

*文档版本: v2.0*
*最后更新: 2026-05-27*