# ADR-0005: LLM 后端配置与工厂模式

## 状态

**已批准** (2026-05-12)

## 背景

HydraForge Phase 1 需要支持多后端 LLM（OpenAI、Anthropic、llama-server），通过配置驱动的工厂模式创建和管理 Provider 实例。

**参考**：
- ADR-1: ILLMProvider 流式接口设计
- ADR-3: 多 Agent 架构，每个 Agent 有独立 DSLEngine 实例
- ADR-4: 工具安全模型

**设计原则**：
- 配置与代码分离
- API Key 安全管理
- 支持多 Agent 多后端
- 长期可扩展（新增 backend 不改代码）

---

## 决策

### 1. 配置格式：YAML

```yaml
# llm_config.yaml

# 默认后端
default_backend: openai

# 后端定义
backends:
  # OpenAI (云端)
  openai:
    type: openai
    model: gpt-4o
    api_key_env: OPENAI_API_KEY
    api_base: https://api.openai.com/v1
    timeout_seconds: 120
    max_retries: 3

  # Anthropic (云端)
  claude:
    type: anthropic
    model: claude-3-5-sonnet-20241022
    api_key_env: ANTHROPIC_API_KEY
    api_base: https://api.anthropic.com
    timeout_seconds: 120
    max_retries: 3

  # 本地 llama-server
  local:
    type: llama
    model: qwen-0.6b
    api_url: http://localhost:8080/v1
    timeout_seconds: 300
    max_retries: 0

# 默认生成参数（可被 DSL 调用参数覆盖）
default_params:
  temperature: 0.7
  max_tokens: 1024
  top_p: 0.95
```

**为什么 YAML 而非 JSON？**

| 特性 | YAML | JSON |
|------|------|------|
| 注释支持 | ✅ `#` 注释 | ❌ 无注释 |
| 可读性 | ✅ 分层缩进 | ⚠️ 嵌套括号 |
| yaml-cpp | ✅ 项目已有 | N/A |
| Schema 验证 | ✅ 支持 | ✅ 支持 |
| 工具生态 | ✅ yq, schemac | ✅ ajv, etc |

### 2. API Key 管理：环境变量

```cpp
// API Key 解析器接口
using ApiKeyResolver = std::function<std::optional<std::string>(const std::string& key)>;

// 内置：环境变量解析器
ApiKeyResolver env_resolver = [](const std::string& key_name) -> std::optional<std::string> {
    const char* value = std::getenv(key_name.c_str());
    if (!value) return std::nullopt;
    return std::string(value);
};

// 内置：密钥文件解析器
ApiKeyResolver file_resolver = [](const std::string& path) -> std::optional<std::string> {
    std::ifstream f(path);
    if (!f) return std::nullopt;
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
};
```

**配置示例**：
```yaml
# 环境变量引用
api_key_env: OPENAI_API_KEY

# 或密钥文件引用
api_key_file: /run/secrets/anthropic.key
```

**安全原则**：
- `api_key_env` 优先于 `api_key_file`
- 不支持 `api_key: "sk-xxx"` 直接写入
- 配置文件中不存储明文密钥

### 3. 工厂接口设计

```cpp
// ============================================================
// 配置数据结构
// ============================================================

struct BackendConfig {
    std::string type;           // "openai", "anthropic", "llama"
    std::string model;
    std::optional<std::string> api_key_env;
    std::optional<std::string> api_key_file;
    std::optional<std::string> api_base;    // 自定义 endpoint
    int timeout_seconds = 120;
    int max_retries = 3;
};

struct LLMConfig {
    std::string default_backend;
    std::map<std::string, BackendConfig> backends;
    GenerationParams default_params;
};

// ============================================================
// Provider 创建器（注册机制）
// ============================================================

class ProviderCreator {
public:
    virtual ~ProviderCreator() = default;
    virtual std::unique_ptr<ILLMProvider> create(
        const BackendConfig& config,
        ApiKeyResolver key_resolver
    ) const = 0;
    virtual bool supports(const std::string& type) const = 0;
};

class OpenAICreator : public ProviderCreator {
public:
    std::unique_ptr<ILLMProvider> create(
        const BackendConfig& config,
        ApiKeyResolver key_resolver
    ) const override {
        auto api_key = resolve_key(config, key_resolver);
        return std::make_unique<OpenAIAdapter>(api_key, config.model);
    }
    bool supports(const std::string& type) const override { return type == "openai"; }
};

class AnthropicCreator : public ProviderCreator {
public:
    std::unique_ptr<ILLMProvider> create(
        const BackendConfig& config,
        ApiKeyResolver key_resolver
    ) const override {
        auto api_key = resolve_key(config, key_resolver);
        return std::make_unique<AnthropicAdapter>(api_key, config.model);
    }
    bool supports(const std::string& type) const override { return type == "anthropic"; }
};

class LlamaCreator : public ProviderCreator {
public:
    std::unique_ptr<ILLMProvider> create(
        const BackendConfig& config,
        ApiKeyResolver key_resolver
    ) const override {
        return std::make_unique<LlamaAdapter>(config.api_url.value());
    }
    bool supports(const std::string& type) const override { return type == "llama"; }
};

// ============================================================
// LLMProviderFactory
// ============================================================

class LLMProviderFactory {
public:
    // 构造时注册所有已知 creator
    LLMProviderFactory() {
        register_creator(std::make_unique<OpenAICreator>());
        register_creator(std::make_unique<AnthropicCreator>());
        register_creator(std::make_unique<LlamaCreator>());
    }

    // 从配置文件加载
    static LLMConfig load_config(const std::string& path) {
        // 使用 yaml-cpp 解析
        YAML::Node node = YAML::LoadFile(path);
        return parse_config(node);
    }

    // 按名称创建 provider
    std::unique_ptr<ILLMProvider> create(
        const std::string& backend_name,
        const LLMConfig& config,
        ApiKeyResolver key_resolver = env_resolver
    ) {
        auto it = config.backends.find(backend_name);
        if (it == config.backends.end()) {
            throw std::invalid_argument("Unknown backend: " + backend_name);
        }

        const auto& backend_config = it->second;
        auto* creator = find_creator(backend_config.type);
        if (!creator) {
            throw std::invalid_argument("Unsupported backend type: " + backend_config.type);
        }

        return creator->create(backend_config, key_resolver);
    }

    // 创建默认 provider
    std::unique_ptr<ILLMProvider> create_default(
        const LLMConfig& config,
        ApiKeyResolver key_resolver = env_resolver
    ) {
        return create(config.default_backend, config, key_resolver);
    }

    // 注册新 creator（用于扩展）
    void register_creator(std::unique_ptr<ProviderCreator> creator) {
        creators_.push_back(std::move(creator));
    }

    // 列出可用后端
    std::vector<std::string> available_backends(const LLMConfig& config) const {
        std::vector<std::string> result;
        for (const auto& [name, cfg] : config.backends) {
            if (auto* c = find_creator(cfg.type)) {
                result.push_back(name);
            }
        }
        return result;
    }

private:
    std::vector<std::unique_ptr<ProviderCreator>> creators_;

    ProviderCreator* find_creator(const std::string& type) const {
        for (auto& c : creators_) {
            if (c->supports(type)) return c.get();
        }
        return nullptr;
    }

    static std::optional<std::string> resolve_key(
        const BackendConfig& config,
        ApiKeyResolver resolver
    ) {
        if (config.api_key_env) {
            return resolver(*config.api_key_env);
        }
        if (config.api_key_file) {
            return resolver(*config.api_key_file);
        }
        return std::nullopt;
    }

    static LLMConfig parse_config(const YAML::Node& node) {
        LLMConfig config;
        config.default_backend = node["default_backend"].as<std::string>();

        for (const auto& [name, backend] : node["backends"]) {
            BackendConfig bc;
            bc.type = backend["type"].as<std::string>();
            bc.model = backend["model"].as<std::string>();
            if (backend["api_key_env"])
                bc.api_key_env = backend["api_key_env"].as<std::string>();
            if (backend["api_key_file"])
                bc.api_key_file = backend["api_key_file"].as<std::string>();
            if (backend["api_base"])
                bc.api_base = backend["api_base"].as<std::string>();
            bc.timeout_seconds = backend["timeout_seconds"].as<int>(120);
            bc.max_retries = backend["max_retries"].as<int>(3);
            config.backends[name.as<std::string>()] = std::move(bc);
        }

        if (node["default_params"]) {
            auto& p = node["default_params"];
            config.default_params.temperature = p["temperature"].as<float>(0.7f);
            config.default_params.max_tokens = p["max_tokens"].as<int>(2048);  // Track 0.1 M1.3: 默认从 1024 调整为 2048
            config.default_params.top_p = p["top_p"].as<float>(0.95f);
        }

        return config;
    }
};
```

### 4. Agent 绑定架构

```cpp
// ============================================================
// Agent 拥有独立 LLM Provider
// ============================================================

class Agent {
public:
    Agent(std::string id, LLMProviderFactory& factory,
          const LLMConfig& config, const std::string& backend_name)
        : id_(std::move(id))
        , llm_(factory.create(backend_name, config))
        , engine_(/* ... */)
    {
        // 每个 Agent 有独立的 provider 实例
    }

    const std::string& id() const { return id_; }
    ILLMProvider* llm() const { return llm_.get(); }

private:
    std::string id_;
    std::unique_ptr<ILLMProvider> llm_;  // 独立实例
    DSLEngine engine_;
};

// ============================================================
// HarnessEngine (Phase 1: 单 Agent)
// ============================================================

class HarnessEngine {
public:
    void initialize(const std::string& config_path) {
        config_ = LLMProviderFactory::load_config(config_path);
        factory_ = std::make_unique<LLMProviderFactory>();

        // 使用默认后端
        llm_ = factory_->create_default(config_);

        // 设置 DSL Engine
        engine_.set_llm_provider(llm_.get());
    }

    // Phase 2: 支持多 Agent
    void add_agent(const std::string& id, const std::string& backend_name) {
        auto agent = std::make_shared<Agent>(id, *factory_, config_, backend_name);
        agents_[id] = agent;
    }

private:
    std::unique_ptr<LLMProviderFactory> factory_;
    LLMConfig config_;
    std::unique_ptr<ILLMProvider> llm_;  // 默认后端 provider
    DSLEngine engine_;

    // Phase 2: 多 Agent
    std::map<std::string, std::shared_ptr<Agent>> agents_;
};
```

### 5. Stream/Sync 运行时决定

```cpp
// 配置不指定 stream/sync 模式
// 调用方根据场景选择

// 场景 1: TUI 流式显示 → generate_stream()
void on_tui_mode(Agent& agent) {
    auto stream = agent.llm()->generate_stream(req, token);
    while (auto token_opt = stream->next(token)) {
        event_bus_->push(UIEvent::llm_token(*token_opt));
    }
}

// 场景 2: 简单查询 → generate()
void on_batch_mode(Agent& agent) {
    auto result = agent.llm()->generate(req, token);
    if (result) {
        process(result->text);
    }
}
```

---

## 配置验证

```cpp
// ============================================================
// 配置验证（启动时检查）
// ============================================================

struct ConfigValidationResult {
    bool valid;
    std::vector<std::string> errors;
    std::vector<std::string> warnings;
};

ConfigValidationResult validate_config(const LLMConfig& config) {
    ConfigValidationResult result;

    // 检查 default_backend 存在
    if (config.backends.find(config.default_backend) == config.backends.end()) {
        result.errors.push_back("default_backend '" + config.default_backend + "' not found in backends");
    }

    // 检查每个后端
    for (const auto& [name, backend] : config.backends) {
        // 检查 type
        if (backend.type != "openai" && backend.type != "anthropic" && backend.type != "llama") {
            result.errors.push_back("backend '" + name + "': unknown type '" + backend.type + "'");
        }

        // 检查 API key
        if ((backend.type == "openai" || backend.type == "anthropic")
            && !backend.api_key_env && !backend.api_key_file) {
            result.errors.push_back("backend '" + name + "': no api_key_env or api_key_file specified");
        }

        // 检查 timeout
        if (backend.timeout_seconds <= 0) {
            result.warnings.push_back("backend '" + name + "': timeout_seconds should be positive");
        }
    }

    result.valid = result.errors.empty();
    return result;
}
```

---

## 长期扩展支持

### 新增 Backend (Phase 2+)

```cpp
// 1. 实现新 Creator
class VertexAICreator : public ProviderCreator {
public:
    std::unique_ptr<ILLMProvider> create(
        const BackendConfig& config,
        ApiKeyResolver key_resolver
    ) const override {
        auto api_key = resolve_key(config, key_resolver);
        return std::make_unique<VertexAIAdapter>(api_key, config.model);
    }
    bool supports(const std::string& type) const override {
        return type == "vertex" || type == "gemini";
    }
};

// 2. 注册到 Factory
factory.register_creator(std::make_unique<VertexAICreator>());

// 3. 配置中添加
// vertex:
//   type: vertex
//   model: gemini-pro
//   api_key_env: VERTEX_API_KEY
```

### 配置迁移

```cpp
// 版本迁移支持
struct ConfigVersion {
    int major;
    int minor;
    std::string changelog;
};

ConfigVersion current_version = {1, 0, "Initial version"};

// 迁移函数
LLMConfig migrate_config(const YAML::Node& node, int from_version) {
    LLMConfig config = parse_config(node);
    if (from_version < 1) {
        // v1.0 迁移逻辑
    }
    return config;
}
```

---

## 权衡

### 为什么每次 create() 创建新实例？

| 方案 | 优点 | 缺点 |
|------|------|------|
| **每次创建** | 隔离清晰，生命周期明确 | 略有开销 |
| **单例** | 共享连接 | 跨 Agent 污染，生命周期复杂 |
| **池化** | 连接复用 | 过早优化，复杂度高 |

**选择"每次创建"的理由**：
- HTTP 连接 stateless，共享无意义
- 每个 Agent 独立 provider，隔离性好
- Phase 1 简单直接

### 为什么 Stream/Sync 运行时决定？

```
同一 OpenAI backend 可以：
- 同步模式用于：单元测试、快速查询、不需要 TUI
- 流式模式用于：TUI 打字机效果、实时展示

配置描述"能做什么"，不限制"怎么用"
```

---

## 实现要求

### Phase 1 必须完成

| # | 任务 | 验证方式 |
|---|------|---------|
| 1 | LLMProviderFactory 实现 | 单元测试：create("openai") 返回正确类型 |
| 2 | YAML 配置加载 | 测试：llm_config.yaml 正确解析 |
| 3 | API Key 解析器 | 测试：环境变量/文件读取正确 |
| 4 | 配置验证 | 测试：缺少 api_key 报错 |
| 5 | Agent 绑定 | 集成测试：两个 Agent 使用不同 backend |

### 测试用例

```cpp
TEST_CASE("LLMProviderFactory creates correct backend") {
    auto factory = LLMProviderFactory();
    auto config = LLMConfig::load_config("test_config.yaml");

    auto openai = factory.create("openai", config);
    REQUIRE(dynamic_cast<OpenAIAdapter*>(openai.get()) != nullptr);

    auto claude = factory.create("claude", config);
    REQUIRE(dynamic_cast<AnthropicAdapter*>(claude.get()) != nullptr);
}

TEST_CASE("ApiKeyResolver from environment") {
    std::setenv("TEST_API_KEY", "sk-test-123", 1);
    auto resolver = env_resolver;

    auto key = resolver("TEST_API_KEY");
    REQUIRE(key.has_value());
    CHECK(*key == "sk-test-123");
}
```

---

## 影响范围

| 组件 | 变更 |
|------|------|
| `src/common/llm/llm_provider_factory.h/cpp` | 新增 Factory 类 |
| `src/common/llm/openai_adapter.h/cpp` | 实现 ProviderCreator 接口 |
| `src/common/llm/anthropic_adapter.h/cpp` | 实现 ProviderCreator 接口 |
| `src/common/llm/llama_adapter.h/cpp` | 实现 ProviderCreator 接口 |
| `src/harness/harness_engine.h/cpp` | Agent 生命周期管理 |
| `config/llm_config.yaml` | 配置文件 |

---

## 替代方案

### 替代 1：JSON 配置（被否决）

**否决理由**：JSON 不支持注释，大配置文件可读性差。

### 替代 2：单例 Provider（被否决）

**否决理由**：跨 Agent 污染，生命周期难管理。

### 替代 3：配置指定 Stream/Sync（被否决）

**否决理由**：同一 backend 需要不同模式，运行时决定更灵活。

---

## 结论

采用配置驱动的工厂模式：

- **YAML 配置**：注释友好，可读性强
- **环境变量 API Key**：安全标准做法
- **Agent 绑定 Provider**：每个 Agent 独立实例
- **每次 create()**：隔离清晰，生命周期明确
- **运行时选择 Stream/Sync**：灵活，不限制使用方式
- **注册机制扩展**：新增 backend 只需注册 Creator

此设计支持：
- **Phase 1**：OpenAI/Anthropic/llama-server 三种后端
- **Phase 2**：新增 Vertex AI、Grok 等后端
- **长期**：配置版本迁移，多 Agent 多后端

---

*文档版本: v1.0*
*最后更新: 2026-05-12*