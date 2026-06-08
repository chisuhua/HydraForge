# AgenticDSL C++ 模块开发综合指南（企业级合规模板 v2.3）

> **适用对象**：C++ 工程师、Agentic 系统架构师、平台安全与可观测性团队  
> **合规依据**：  
> - *AgenticDSL Application Development Guide v3.9*  
> - *Inference Supplement Specification v0.4*  
> **核心原则**：  
> - **契约先行**：接口、权限、错误、生命周期必须显式声明  
> - **确定性执行**：同步、无状态（推荐）、可重试、幂等（关键场景）  
> - **观测内建**：版本、追踪、延迟自动透传，无需手动埋点  
> - **安全边界**：禁止绕过框架直接调用外部系统或自行鉴权  

> **2026-06-08 注**：本指南基于 v3.9 写作；当前参考执行器为 v3.10（C1 迁移后）。C1 关键变更：引擎 LLM 接口统一为 `ILLMProvider`（参见 `docs/specs/dsl.md` §5.9 与 `docs/adr/adr-0001`）。

---

## 第一章 契约定义：工具的能力边界（What）

### 1.1 工具元数据声明

每个 C++ 模块必须在 `/app/<AppName>/startup.agent.md` 中声明其能力。以金融风控场景为例：

```markdown
### AgenticDSL `/__meta__/resources`
yaml
type: resource_declare
resources:
  - type: tool
    name: risk_score
    scope: internal
    lifecycle: stateless
    capabilities: [risk_scoring]
  - type: tool
    name: fraud_check
    scope: internal
    lifecycle: stateless
    capabilities: [fraud_detection]

### AgenticDSL `/__meta__`
yaml
version: "3.9"
mode: prod
entry_point: "/app/finance/payment_approval"
```

> ✅ **要求**（v3.9 §6.1）：
> - 所有 DSL 文件路径必须为 `/app/<AppName>/...`
> - `lifecycle` 字段不可省略
> - 项目根目录 `README.md` 必须注明：“本模块符合 AgenticDSL v3.9 规范，依赖执行器 ≥ v3.9.0”

---

### 1.2 输入/输出契约（Inference Supplement v0.4 §3.2）

工具的输入输出类型必须严格匹配以下集合：

| 类型标识 | 对应 JSON 类型 | 说明 |
|--------|---------------|------|
| `"string"` | string | UTF-8 字符串 |
| `"number"` | number | 双精度浮点（非整型） |
| `"boolean"` | boolean | true / false |
| `"object"` | object | **仅允许扁平结构**（key → string/number/boolean） |
| `"array_string"` | array | 元素全为 string |
| `"array_number"` | array | 元素全为 number |
| `"null"` | null | 显式空值 |

> ⚠️ **禁止**（v3.9 §4.2）：
> - 嵌套对象（如 `{"user": {"id": "u1"}}`）
> - 混合类型数组（如 `[1, "a"]`）
> - 自定义枚举（需转为 string）

**示例（合规 ToolSchema）**：
```cpp
.inputs = {{"user_id", "string"}, {"amount", "number"}},
.outputs = {{"risk_score", "number"}, {"high_risk", "boolean"}, {"tags", "array_string"}}
```

---

### 1.3 权限模型（结构化声明）

权限必须以结构化对象注册：

```cpp
.required_permissions = {
  agentic::Permission{
    .action = "read",
    .resource = "user.risk_profile",
    .domain = "finance"
  }
}
```

> 🔒 **行为规范**（v3.9 §5.3）：
> - 权限校验由执行器在调用前完成
> - 若缺失权限，**自动抛出**：
>   ```text
>   ERR_SYSTEM.PERMISSION_DENIED: missing permission [action=read, resource=user.risk_profile, domain=finance]
>   ```
> - 此错误**不进入用户定义的 `on_error`**，流程立即终止（返回 HTTP 403）

---

### 1.4 错误分类标准

所有异常必须使用标准化前缀：

| 错误域 | 触发场景 | 是否进入 `on_error` |
|-------|--------|------------------|
| `ERR_INPUT.*` | 缺失字段、类型错误、格式无效 | 是 |
| `ERR_EXEC.*` | 业务逻辑异常（模型崩溃、规则冲突） | 是 |
| `ERR_SYSTEM.*` | DB/API 故障、权限拒绝、超时 | 是（除权限拒绝外） |

> ✅ **实现方式**：
> ```cpp
> throw agentic::ToolError("ERR_INPUT.MISSING_FIELD: user_id required");
> ```

> ⚠️ **禁止**：自定义前缀（如 `ERR_CUSTOM`）

---

## 第二章 完整示例：支付审批风控模块

为便于理解，本章提供一个**端到端完整示例**，涵盖代码、编排、构建与目录结构。

### 2.1 项目目录结构

```text
payment_risk_agent/
├── CMakeLists.txt
├── README.md
├── include/agentic_native/
│   └── tool_interface.hpp          # ← 框架头文件（由平台提供）
├── src/tools/
│   ├── risk_score.cpp              # ← 风控评分工具
│   ├── fraud_check.cpp             # ← 欺诈检测工具
│   └── register_tools.cpp          # ← 工具统一注册
├── app/finance/                    # ← 合规路径（v3.9 §6.1）
│   ├── payment_approval.agent.md   # ← 主编排流程
│   └── startup.agent.md            # ← 资源与元信息声明
├── config/
│   └── default.yaml                # ← 运行时配置模板
├── tests/
│   ├── test_risk_score.cpp
│   └── test_fraud_check.cpp
└── mock/
    └── agentic_mock.hpp            # ← 本地调试辅助
```

> ✅ **关键原则**：
> - 所有 `.cpp` 文件位于 `src/tools/`
> - 所有 AgenticDSL 编排文件必须位于 `/app/<AppName>/`
> - **禁止** C++ 模块间相互调用（v3.9 §2.1）

---

### 2.2 C++ 工具实现

#### `src/tools/risk_score.cpp`

```cpp
#include "agentic_native/tool_interface.hpp"
#include <cmath>

// 模拟风控模型（实际项目中替换为真实模型调用）
namespace RiskModel {
  double predict(const std::string& user_id, double amount) {
    // 从配置读取阈值（见 2.8 节）
    double base = agentic::get_config<double>("risk.base_multiplier", 1.0);
    return std::min(1.0, (amount * base) / 10000.0);
  }
  int last_latency() { return 42; } // 模拟延迟
}

agentic::JsonValue risk_score(const agentic::JsonValue& args) {
  // 1. 输入校验
  if (!args.has("user_id") || !args.has("amount")) {
    throw agentic::ToolError("ERR_INPUT.MISSING_FIELD: user_id and amount required");
  }
  if (!args["amount"].isNumber()) {
    throw agentic::ToolError("ERR_INPUT.INVALID_TYPE: amount must be number");
  }

  // 2. 核心逻辑
  double amount = args["amount"].asNumber();
  double score = RiskModel::predict(args["user_id"].asString(), amount);

  // 3. 返回标准结构
  return agentic::JsonValue::object({
    {"result", agentic::JsonValue::object({
      {"risk_score", agentic::JsonValue::number(score)},
      {"high_risk", agentic::JsonValue::boolean(score > 0.8)},
      {"tags", agentic::JsonValue::array({"finance", "payment"})}
    })},
    {"meta", agentic::JsonValue::object({
      {"tool_version", agentic::JsonValue::string(TOOL_VERSION)},
      {"latency_ms", agentic::JsonValue::number(RiskModel::last_latency())},
      {"backend", agentic::JsonValue::string("risk_model_v1")},
      {"trace_context", args["__trace__"]}
    })}
  });
}
```

---

### 2.3 工具注册

#### `src/tools/register_tools.cpp`

```cpp
#include "agentic_native/tool_interface.hpp"

// 声明函数
agentic::JsonValue risk_score(const agentic::JsonValue& args);
agentic::JsonValue fraud_check(const agentic::JsonValue& args);

extern "C" void register_agentic_tools(agentic::ToolRegistry& reg) {
  reg.registerTool(
    "risk_score",
    risk_score,
    agentic::ToolSchema{
      .inputs = {{"user_id", "string"}, {"amount", "number"}},
      .outputs = {{"risk_score", "number"}, {"high_risk", "boolean"}, {"tags", "array_string"}},
      .required_permissions = {
        agentic::Permission{.action = "read", .resource = "user.risk_profile", .domain = "finance"}
      },
      .lifecycle = agentic::Lifecycle::STATELESS
    }
  );

  reg.registerTool(
    "fraud_check",
    fraud_check,
    agentic::ToolSchema{
      .inputs = {{"user_id", "string"}, {"ip_address", "string"}},
      .outputs = {{"is_fraud", "boolean"}, {"confidence", "number"}},
      .required_permissions = {
        agentic::Permission{.action = "read", .resource = "user.fraud_signals", .domain = "finance"}
      },
      .lifecycle = agentic::Lifecycle::STATELESS
    }
  );
}
```

---

### 2.4 AgenticDSL 编排流程

#### `app/finance/startup.agent.md`（资源声明）

```markdown
### AgenticDSL `/__meta__/resources`
yaml
type: resource_declare
resources:
  - type: tool
    name: risk_score
    scope: internal
    lifecycle: stateless
    capabilities: [risk_scoring]
  - type: tool
    name: fraud_check
    scope: internal
    lifecycle: stateless
    capabilities: [fraud_detection]

### AgenticDSL `/__meta__`
yaml
version: "3.9"
mode: prod
entry_point: "/app/finance/payment_approval"
```

#### `app/finance/payment_approval.agent.md`（主编排）

```yaml
name: payment_approval
description: "Parallel risk and fraud check for payment approval"

steps:
  - id: validate_input
    type: assign
    assign:
      expr: |
        {% if not $.payment.user_id or not $.payment.amount or not $.payment.ip %}
          {"error": "missing required fields"}
        {% else %}
          {"valid": true}
        {% endif %}
    output_mapping:
      validation_result: "context.session.finance.validation"
    on_error: "/app/finance/handle_validation_error"

  - id: parallel_checks
    type: fork
    branches:
      - "/app/finance/risk_assessment"
      - "/app/finance/fraud_assessment"
    next: "/app/finance/make_decision"

  - id: risk_assessment
    type: tool_call
    tool: risk_score
    arguments:
      user_id: "{{ $.payment.user_id }}"
      amount: "{{ $.payment.amount }}"
    output_mapping:
      risk_result: "context.session.finance.risk"

  - id: fraud_assessment
    type: tool_call
    tool: fraud_check
    arguments:
      user_id: "{{ $.payment.user_id }}"
      ip_address: "{{ $.payment.ip }}"
    output_mapping:
      fraud_result: "context.session.finance.fraud"

  - id: make_decision
    type: assign
    assign:
      expr: |
        {% set risk = $.context.session.finance.risk.result %}
        {% set fraud = $.context.session.finance.fraud.result %}
        {% if risk.high_risk or fraud.is_fraud %}
          {"approved": false, "reason": "high_risk_or_fraud"}
        {% else %}
          {"approved": true}
        {% endif %}
    output_mapping:
      final_decision: "context.persistent.finance.decision"
    meta:
      ttl_seconds: 3600
      cleanup_policy: "on_exit"

  - id: handle_validation_error
    type: assign
    assign:
      error_message: "Input validation failed"
    output_mapping:
      error: "context.session.finance.error"
```

---

### 2.5 构建脚本（CMake）

#### `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.14)
project(PaymentRiskAgent LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# 依赖管理：使用 FetchContent（推荐）
include(FetchContent)
FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG v3.11.2
)
FetchContent_MakeAvailable(nlohmann_json)

# 工具版本（语义化）
set(TOOL_VERSION "1.0.0")

# 编译模块
add_library(payment_risk_agent SHARED
  src/tools/risk_score.cpp
  src/tools/fraud_check.cpp
  src/tools/register_tools.cpp
)

# 链接依赖
target_link_libraries(payment_risk_agent PRIVATE nlohmann_json::nlohmann_json)
target_include_directories(payment_risk_agent PUBLIC include PRIVATE src)

# 注入版本号和配置路径
target_compile_definitions(payment_risk_agent PRIVATE 
  TOOL_VERSION="${TOOL_VERSION}"
  CONFIG_PATH="/etc/agents/config.yaml"
)

# 生成无前缀动态库（xxx.so 而非 libxxx.so）
set_target_properties(payment_risk_agent PROPERTIES PREFIX "")

# 安装规则
install(TARGETS payment_risk_agent LIBRARY DESTINATION /opt/agents/lib)
install(DIRECTORY app/ DESTINATION /opt/agents/app)
install(FILES config/default.yaml DESTINATION /etc/agents RENAME config.yaml)
```

> ✅ **依赖策略**（v3.9 §7.1）：
> - 优先使用 `FetchContent` 或平台 SDK
> - 禁止链接系统全局库（如 `/usr/lib/libxxx.so`）
> - 所有依赖必须开源且许可证兼容（MIT/Apache 2.0）

---

### 2.6 单元测试示例

#### `tests/test_risk_score.cpp`

```cpp
#include "../src/tools/risk_score.cpp"
#include <cassert>
#include <iostream>

int main() {
  auto input = agentic::JsonValue::object({
    {"user_id", "u123"},
    {"amount", 5000.0},
    {"__trace__", agentic::JsonValue::object({
      {"trace_id", "t123"},
      {"span_id", "s456"}
    })}
  });

  auto output = risk_score(input);
  
  assert(output["result"]["risk_score"].asNumber() == 0.5);
  assert(output["result"]["high_risk"].asBool() == false);
  assert(output["meta"]["tool_version"].asString() == "1.0.0");

  std::cout << "✅ risk_score test passed\n";
  return 0;
}
```

> ✅ **测试要求**：
> - 覆盖率 ≥ 80%（通过 `gcov` 验证）
> - 验证 `result` 结构 + `meta` 完整性
> - 模拟错误路径（如缺失字段）

---

### 2.7 本地调试支持

#### `mock/agentic_mock.hpp`

```cpp
#pragma once
#include <nlohmann/json.hpp>
#include <unordered_map>

namespace agentic {
  using JsonValue = nlohmann::json;
  struct ToolError : public std::runtime_error {
    using std::runtime_error::runtime_error;
  };
  inline bool is_cancelled() { return false; }

  // 模拟配置系统
  static std::unordered_map<std::string, JsonValue> mock_config = {
    {"risk.base_multiplier", 1.0}
  };
  template<typename T>
  T get_config(const std::string& key, const T& default_val) {
    if (mock_config.count(key)) {
      return mock_config[key].get<T>();
    }
    return default_val;
  }
}
```

---

### 2.8 运行时配置注入

所有可变参数必须通过配置注入，禁止硬编码：

```cpp
double threshold = agentic::get_config<double>("risk.score_threshold", 0.8);
std::string model_endpoint = agentic::get_config<std::string>("model.endpoint", "http://localhost:8080");
```

**配置来源优先级**（由高到低）：
1. DSL 步骤中的 `arguments`（覆盖配置）
2. 环境变量（如 `RISK_SCORE_THRESHOLD=0.9`）
3. 配置文件（`/etc/agents/config.yaml`）
4. 默认值（代码中指定）

**`config/default.yaml` 示例**：
```yaml
risk:
  base_multiplier: 1.2
  score_threshold: 0.85
model:
  endpoint: "https://risk-model.prod/api/v1"
```

> ⚠️ **安全要求**：
> - 配置文件不得包含密钥（密钥通过 Vault 注入环境变量）
> - 配置变更需重启模块（不支持热更新）

---

## 第三章 上下文与编排：状态传递机制（Where）

### 3.1 上下文命名空间

| 路径 | 生命周期 | TTL 支持 | 典型用途 |
|------|--------|--------|--------|
| `context.session.<ns>.*` | 单次请求内 | ❌ | 临时中间结果 |
| `context.persistent.<ns>.*` | 跨请求 | ✅（需声明） | 用户决策、审批状态 |

> ⚠️ **废弃**：`memory.state.*` 在 v3.9 中已完全移除

---

### 3.2 写入语义（v3.9 新增原子性）

- **原子提交**：所有 `output_mapping` 在 step 结束时一次性写入
- **最后写入胜出**：同一 step 多次写同一路径，仅保留最终值
- **禁止**：在 C++ 代码中直接读写 `context.*`（必须通过 DSL 映射）

---

### 3.3 AgenticDSL 编排范式

（略，同 v2.2）

---

## 第四章 工程保障：构建、测试与可观测（Verify）

### 4.1 构建系统（CMake）

（见 2.5 节）

---

### 4.2 单元测试策略

（见 2.6 节）

---

### 4.3 本地开发支持

（见 2.7 节）

---

### 4.4 可观测性要求（v3.9 强制）

- **必须包含**：
  - `meta.tool_version`
  - `meta.trace_context`
- 执行器自动采集：
  - 调用链（Trace/Span）
  - 延迟分布（P50/P95/P99）
  - 错误率（按错误域分类）

---

### 4.5 日志与审计规范

> ⚠️ **禁止**使用 `std::cout`、`printf`、`fprintf` 等标准输出

所有日志必须通过结构化接口记录：

```cpp
agentic::log_info("risk_score_computed", {
  {"user_id", user_id, agentic::PII::USER_ID},
  {"score", score},
  {"amount", amount}
});
```

**日志字段敏感级别**：
- `agentic::PII::USER_ID`：自动脱敏为 `u***123`
- `agentic::PII::NONE`（默认）：明文记录
- 执行器自动 redact PII 字段并记录审计日志

> ✅ **要求**：
> - 日志必须包含 `trace_id`（自动注入）
> - 禁止记录原始密码、身份证、银行卡号

---

## 第五章 合规与安全：企业级要求（Comply）

### 5.1 线程与并发模型

- **执行器保证**：同一工具函数**不会被并发调用**
- **但不同工具可能并发执行** → 全局状态必须加锁
- **推荐**：完全无状态设计（`lifecycle = stateless`）

---

### 5.2 幂等性支持（金融/支付场景）

（略，同 v2.2）

---

### 5.3 安全边界

> ⚠️ **绝对禁止**：
> - 调用 `system()`、`exec`、`popen`
> - 裸 HTTP 请求（如 `curl`）→ 应封装为独立 tool
> - 返回敏感字段（身份证、手机号）→ 必须脱敏或受权限控制

---

### 5.4 资源使用边界

- **CPU 时间**：单次调用不得超过 **500ms**（超时将被强制 cancel）
- **内存限制**：堆内存峰值不得超过 **100MB**
- **中断检查**：长时间循环中必须定期调用 `agentic::is_cancelled()`

```cpp
for (int i = 0; i < large_dataset.size(); ++i) {
  if (agentic::is_cancelled()) {
    throw agentic::ToolError("ERR_SYSTEM.CANCELLED");
  }
  // process item
}
```

> ✅ **监控**：执行器 sandbox 自动采集资源使用指标并告警

---

### 5.5 向后兼容策略

（略，同 v2.2）

---

## 第六章 发布与部署规范

### 6.1 构建产物清单

每个发布包必须包含：

```text
payment_risk_agent_v1.0.0.tar.gz
├── lib/payment_risk_agent.so
├── app/finance/*.agent.md
├── config/config.yaml
└── manifest.json               # ← 新增
```

**`manifest.json` 示例**：
```json
{
  "tool_name": "payment_risk_agent",
  "tool_version": "1.0.0",
  "agentic_dsl_version": "3.9",
  "sha256": "a1b2c3...",
  "dependencies": ["nlohmann_json@3.11.2"],
  "permissions": [
    {"action": "read", "resource": "user.risk_profile", "domain": "finance"}
  ]
}
```

---

### 6.2 CI/CD 合规检查

CI 流程必须执行以下检查：

```bash
# 1. 语法与契约检查
agentic-lint --dir app/

# 2. 输出结构验证
contract-validator --tool risk_score --input test_input.json

# 3. 安全扫描
sast-scan --lang cpp src/

# 4. 端到端模拟
agentic-simulator \
  --dsl app/finance/payment_approval.agent.md \
  --input tests/test_payment.json \
  --expect 'final_decision.approved:true'
```

> ✅ **门禁规则**：任一检查失败则阻断发布

---

### 6.3 上线前验证清单

| 检查项 | 工具/方法 |
|-------|----------|
| DSL 路径合规 | `agentic-lint` |
| 输出结构扁平 | `contract-validator` |
| 无敏感日志 | `log-scanner` |
| 依赖许可证合规 | `license-checker` |
| 资源使用达标 | `sandbox-benchmark` |

---

## 第七章 附录：最佳实践与反模式

### 7.1 推荐实践清单

- 每个 `.cpp` 文件只实现一个工具
- 使用扁平 `object` 替代嵌套结构
- 所有常量通过配置注入
- 单元测试覆盖正常路径 + 错误路径

---

### 7.2 常见反模式（Anti-Patterns）

| 反模式 | 风险 | 正确做法 |
|-------|------|--------|
| `result` 中混入 `latency_ms` | 破坏契约 | 移至 `meta` |
| 自行实现 RBAC | 绕过审计 | 依赖结构化权限 |
| 全局变量存状态 | 并发污染 | 用 `context.session` |
| 硬编码阈值 | 无法灰度 | 通过配置注入 |
| 使用 `std::cout` | 日志丢失 | 用 `agentic::log_*` |

---

### 7.3 合规检查清单（v3.9 + v0.4）

| 检查项 | 合规方式 |
|-------|--------|
| 上下文路径 | 仅 `context.session.*` 或 `context.persistent.*` |
| 输出结构 | `result` 纯业务，`meta` 含 `tool_version` + `trace_context` |
| 权限模型 | 结构化 `Permission{action, resource, domain}` |
| 错误分类 | 使用 `ERR_INPUT.*` / `ERR_EXEC.*` / `ERR_SYSTEM.*` |
| 生命周期 | 显式声明 `lifecycle` |
| TTL 作用域 | 仅对 `context.persistent.*` 有效 |
| 输入/输出类型 | 仅限 7 种标准类型 |
| 路径规范 | DSL 文件位于 `/app/<AppName>/` |
| 日志安全 | 无 PII 明文，使用结构化日志 |
| 资源限制 | ≤500ms CPU, ≤100MB 内存 |
| 配置管理 | 无硬编码，支持环境变量覆盖 |

---

**© 2025 AgenticDSL Working Group**  
*本指南适用于 AgenticDSL v3.9+ 执行环境。所有模块必须通过合规检查清单方可上线。*
