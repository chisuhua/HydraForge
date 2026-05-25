# 安全规范

**ID**: SEC-001
**日期**: 2026-05-23
**状态**: 已批准
**关联**: BOOT-001, API-001, TEST-001, ADR-0004

---

## 概述

本文档定义 AgenticDSL 项目的安全规范，涵盖 API key 管理、数据隐私、输入验证和安全编码实践。

## 1. API Key 管理

### 1.1 核心原则

| 原则 | 说明 |
|------|------|
| **永不明文存储** | API key 禁止硬编码或明文存储在代码/配置中 |
| **最小权限** | 仅授予所需的最小权限 |
| **轮换机制** | 定期轮换 API key |
| **审计日志** | 记录所有 API key 的使用 |

### 1.2 环境变量

**必须使用环境变量存储 API key**：

```cpp
// ✅ 推荐
std::string api_key = std::getenv("OPENAI_API_KEY");
if (api_key.empty()) {
    return LLMError::auth_error("OPENAI_API_KEY not set");
}

// ❌ 禁止
std::string api_key = "sk-xxx...";  // 禁止硬编码
```

**配置优先级**：
1. 环境变量（最高优先级）
2. 密钥管理服务（AWS Secrets Manager、HashiCorp Vault）
3. 加密配置文件（最低优先级）

### 1.3 配置文件格式

```json
{
  "llm": {
    "provider": "openai",
    "api_key_env": "OPENAI_API_KEY",
    "model": "gpt-4"
  },
  "security": {
    "api_key_sources": ["env", "kms"],
    "allowed_providers": ["openai", "anthropic"]
  }
}
```

### 1.4 密钥管理服务集成

```cpp
// src/common/security/kms_client.h
class KMSClient {
public:
    virtual ~KMSClient() = default;

    // 获取密钥
    virtual Result<std::string, SecurityError> get_secret(
        const std::string& key_name) = 0;

    // 轮换密钥
    virtual Result<void, SecurityError> rotate_key(
        const std::string& key_name) = 0;

    // 列出密钥
    virtual std::vector<std::string> list_secrets() = 0;
};

// AWS Secrets Manager 实现
class AWSKMSClient : public KMSClient {
public:
    explicit AWSKMSClient(const AWSConfig& config);

    Result<std::string, SecurityError> get_secret(
        const std::string& key_name) override {
        // 调用 AWS Secrets Manager API
    }
};

// 环境变量实现（备选）
class EnvKMSClient : public KMSClient {
public:
    Result<std::string, SecurityError> get_secret(
        const std::string& key_name) override {
        auto* value = std::getenv(key_name.c_str());
        if (!value) {
            return SecurityError::not_found(key_name);
        }
        return std::string(value);
    }
};
```

---

## 2. 数据隐私

### 2.1 隐私分级

| 级别 | 定义 | 处理方式 |
|------|------|---------|
| **P0 - 敏感** | 密码、密钥、个人身份信息 | 禁止上传到云端 |
| **P1 - 机密** | 业务数据、用户数据 | 本地处理，必要时加密 |
| **P2 - 内部** | 内部文档、代码 | 可选择性上传 |
| **P3 - 公开** | 公开信息 | 无限制 |

### 2.2 路由策略

```cpp
struct TaskProfile {
    // ... 其他字段 ...

    // 隐私分级
    PrivacyLevel privacy_level = PrivacyLevel::INTERNAL;
};

enum class PrivacyLevel {
    PUBLIC = 0,     // 公开信息
    INTERNAL = 1,  // 内部信息
    CONFIDENTIAL = 2, // 机密信息
    SENSITIVE = 3   // 敏感信息
};

// 路由决策时检查隐私级别
RoutingDecision LLMRouter::route(const TaskProfile& profile) {
    // 敏感数据强制本地处理
    if (profile.privacy_level >= PrivacyLevel::CONFIDENTIAL) {
        return {
            .backend = Backend::LOCAL,
            .reason = "隐私级别要求本地处理"
        };
    }

    // 公开信息可以使用云端
    if (profile.privacy_level <= PrivacyLevel::PUBLIC) {
        return route_to_cloud(profile);
    }

    // 其他情况根据其他因素决策
    return route_by_quality(profile);
}
```

### 2.3 数据脱敏

```cpp
// src/common/security/data_sanitizer.h
class DataSanitizer {
public:
    // 脱敏处理
    static std::string sanitize(const std::string& input, PrivacyLevel level);

    // 检测敏感信息
    static bool contains_pii(const std::string& input);

    // 常用模式
    static const std::regex EMAIL_PATTERN;
    static const std::regex PHONE_PATTERN;
    static const std::regex CREDIT_CARD_PATTERN;
    static const std::regex API_KEY_PATTERN;

private:
    static std::string mask_email(const std::string& email);
    static std::string mask_phone(const std::string& phone);
    static std::string mask_credit_card(const std::string& card);
};

std::string DataSanitizer::sanitize(const std::string& input, PrivacyLevel level) {
    std::string result = input;

    if (level >= PrivacyLevel::CONFIDENTIAL) {
        // 脱敏处理
        result = std::regex_replace(result, EMAIL_PATTERN, "[EMAIL]");
        result = std::regex_replace(result, PHONE_PATTERN, "[PHONE]");
        result = std::regex_replace(result, CREDIT_CARD_PATTERN, "[CARD]");
        result = std::regex_replace(result, API_KEY_PATTERN, "[API_KEY]");
    }

    return result;
}
```

---

## 3. 输入验证

### 3.1 Prompt 验证

```cpp
// src/common/security/prompt_validator.h
class PromptValidator {
public:
    struct ValidationResult {
        bool valid;
        std::vector<std::string> warnings;
        std::vector<std::string> errors;
    };

    static ValidationResult validate(const std::string& prompt,
                                      const ValidationConfig& config);

    // 长度限制
    static const size_t MAX_PROMPT_LENGTH = 100000;  // 100KB

    // 复杂度限制（防止 prompt 注入）
    static const size_t MAX_INSTRUCTION_COUNT = 50;
};

PromptValidator::ValidationResult PromptValidator::validate(
    const std::string& prompt,
    const ValidationConfig& config) {

    ValidationResult result;

    // 长度检查
    if (prompt.length() > MAX_PROMPT_LENGTH) {
        result.errors.push_back("Prompt exceeds maximum length");
        result.valid = false;
    }

    // 指令计数（防止 prompt 注入）
    size_t instruction_count = count_instructions(prompt);
    if (instruction_count > MAX_INSTRUCTION_COUNT) {
        result.warnings.push_back("High instruction count - possible injection");
    }

    // 危险模式检测
    static const std::vector<std::regex> DANGEROUS_PATTERNS = {
        std::regex("ignore previous instructions", std::regex::icase),
        std::regex("disregard.*instructions", std::regex::icase),
        std::regex("role.*play.*as.*system", std::regex::icase)
    };

    for (const auto& pattern : DANGEROUS_PATTERNS) {
        if (std::regex_search(prompt, pattern)) {
            result.warnings.push_back("Potential prompt injection detected");
            break;
        }
    }

    return result;
}
```

### 3.2 输出过滤

```cpp
// src/common/security/output_filter.h
class OutputFilter {
public:
    // 过滤输出中的敏感信息
    static std::string filter(const std::string& output);

    // 检测有害内容
    static bool contains_harmful_content(const std::string& output);

private:
    // 敏感信息模式
    static const std::regex INTERNAL_URL_PATTERN;
    static const std::regex API_KEY_PATTERN;
    static const std::regex PRIVATE_KEY_PATTERN;
};

std::string OutputFilter::filter(const std::string& output) {
    std::string result = output;
    result = std::regex_replace(result, API_KEY_PATTERN, "[REDACTED]");
    result = std::regex_replace(result, PRIVATE_KEY_PATTERN, "[REDACTED]");
    result = std::regex_replace(result, INTERNAL_URL_PATTERN, "[INTERNAL_URL]");
    return result;
}
```

---

## 4. 安全编码实践

### 4.1 禁止的模式

```cpp
// ❌ 禁止：使用用户输入构造代码执行
std::string cmd = "ls " + user_input;
system(cmd.c_str());  // 命令注入

// ❌ 禁止：使用用户输入构造 SQL
std::string query = "SELECT * FROM users WHERE name = '" + user_name + "'";

// ❌ 禁止：危险的文件操作
std::ifstream file(user_filename);  // 路径遍历

// ❌ 禁止：硬编码凭证
const char* password = "secret";
```

### 4.2 推荐的模式

```cpp
// ✅ 推荐：参数化查询
auto result = db.query("SELECT * FROM users WHERE name = ?", user_name);

// ✅ 推荐：路径验证
if (!is_safe_path(user_filename)) {
    return error("Invalid path");
}

// ✅ 推荐：输入验证
if (!is_valid_email(user_email)) {
    return error("Invalid email");
}

// ✅ 推荐：使用安全的随机数
std::random_device rd;
std::mt19937 gen(rd());
```

### 4.3 内存安全

```cpp
// ✅ 使用智能指针
auto ptr = std::make_unique<Resource>();
auto shared = std::make_shared<Resource>();

// ✅ 使用 std::string 而非 char*
std::string safe_string = user_input;  // 自动内存管理

// ✅ 使用容器而非原始数组
std::vector<uint8_t> buffer(1024);

// ❌ 避免使用
char* buffer = new char[1024];  // 需要手动 delete
```

---

## 5. HTTPS 强制

```cpp
// 确保所有 HTTP 请求使用 HTTPS
class SecureHttpClient {
public:
    static Result<Response, SecurityError> get(
        const std::string& url,
        const Headers& headers) {

        // URL 验证
        if (!is_https_url(url)) {
            return SecurityError::insecure_url(url);
        }

        return http_client_->get(url, headers);
    }

private:
    static bool is_https_url(const std::string& url) {
        return url.rfind("https://", 0) == 0;
    }
};
```

---

## 6. 日志安全

### 6.1 敏感信息过滤

```cpp
// src/common/security/secure_logger.h
class SecureLogger {
public:
    template<typename... Args>
    void info(const char* fmt, Args&&... args) {
        log("INFO", fmt, std::forward<Args>(args)...);
    }

    template<typename... Args>
    void error(const char* fmt, Args&&... args) {
        log("ERROR", fmt, std::forward<Args>(args)...);
    }

private:
    template<typename... Args>
    void log(const char* level, const char* fmt, Args&&... args) {
        // 格式化消息
        std::string msg = format(fmt, std::forward<Args>(args)...);

        // 过滤敏感信息
        msg = filter_sensitive(msg);

        // 写日志
        write(level, msg);
    }

    static std::string filter_sensitive(const std::string& msg) {
        std::string result = msg;
        result = std::regex_replace(result, API_KEY_PATTERN, "[API_KEY]");
        result = std::regex_replace(result, PASSWORD_PATTERN, "[PASSWORD]");
        return result;
    }

    static const std::regex API_KEY_PATTERN;
    static const std::regex PASSWORD_PATTERN;
};
```

### 6.2 日志级别规范

| 级别 | 内容 | 示例 |
|------|------|------|
| **DEBUG** | 详细调试信息 | 函数入口、变量值 |
| **INFO** | 一般信息 | 请求开始/结束 |
| **WARN** | 警告信息 | 性能问题、配置缺失 |
| **ERROR** | 错误信息 | API 失败、异常 |

**禁止记录**：
- API key、密码
- 用户敏感信息
- 内部系统路径
- 调试后的完整请求/响应（脱敏后可记录）

---

## 7. 审计日志

### 7.1 审计事件

```cpp
// src/common/security/audit_logger.h
struct AuditEvent {
    std::chrono::system_clock::time_point timestamp;
    std::string event_type;
    std::string user_id;
    std::string resource;
    std::string action;
    bool success;
    std::string details;
};

class AuditLogger {
public:
    void log(const AuditEvent& event) {
        // 持久化到审计日志
        write_to_audit_log(event);
    }

    // 记录 API 调用
    void log_api_call(const std::string& provider,
                      const std::string& model,
                      bool success,
                      int tokens_used) {
        log({
            .timestamp = std::chrono::system_clock::now(),
            .event_type = "API_CALL",
            .action = provider + "/" + model,
            .success = success,
            .details = "tokens=" + std::to_string(tokens_used)
        });
    }

    // 记录认证失败
    void log_auth_failure(const std::string& provider,
                          const std::string& reason) {
        log({
            .timestamp = std::chrono::system_clock::now(),
            .event_type = "AUTH_FAILURE",
            .action = provider,
            .success = false,
            .details = reason
        });
    }

    // 记录数据访问
    void log_data_access(const std::string& resource,
                         const PrivacyLevel& level) {
        log({
            .timestamp = std::chrono::system_clock::now(),
            .event_type = "DATA_ACCESS",
            .resource = resource,
            .action = "READ",
            .success = true,
            .details = "privacy_level=" + std::to_string(static_cast<int>(level))
        });
    }
};
```

---

## 8. 安全配置

### 8.1 安全配置文件

```json
{
  "security": {
    "api_key_sources": ["env", "kms"],
    "allowed_providers": ["openai", "anthropic"],
    "require_https": true,
    "max_prompt_length": 100000,
    "rate_limit": {
      "requests_per_minute": 60,
      "tokens_per_minute": 100000
    }
  },
  "privacy": {
    "default_level": "internal",
    "force_local_on_high": true
  }
}
```

### 8.2 环境变量清单

| 变量名 | 必需 | 说明 |
|--------|------|------|
| `OPENAI_API_KEY` | 是 | OpenAI API 密钥 |
| `ANTHROPIC_API_KEY` | 是 | Anthropic API 密钥 |
| `AWS_ACCESS_KEY_ID` | 可选 | AWS 访问密钥（使用 KMS 时） |
| `AWS_SECRET_ACCESS_KEY` | 可选 | AWS 密钥（使用 KMS 时） |
| `KMS_ENDPOINT` | 可选 | KMS 服务端点 |
| `AUDIT_LOG_PATH` | 可选 | 审计日志路径 |

---

## 9. 验证标准

- [ ] API key 从环境变量读取
- [ ] 禁止硬编码凭证
- [ ] HTTPS 强制启用
- [ ] Prompt 注入检测
- [ ] 敏感信息过滤
- [ ] 审计日志完整
- [ ] 输入验证完善
- [ ] 安全编码规范遵循

---

## 关联文档

| 文档 | 关系 |
|------|------|
| [BOOT-001: 自举实施路径](../implementation/self-bootstrapping-path.md) | 安全是阶段 0 前置条件 |
| [API-001: CloudLLMAdapter](../api/cloud-llm-adapter.md) | API 安全实现参考 |
| [TEST-001: 测试策略](test-strategy.md) | 安全测试用例 |
| [ADR-0004: ToolRegistry 安全模型](../../adr/adr-0004-toolregistry-security.md) | 工具注册表安全模型，本文的引擎级参考 |