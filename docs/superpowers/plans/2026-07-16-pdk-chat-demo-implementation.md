# PDK Chat Demo 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 实现 `examples/pdk_chat_demo` — 演示 HydraForge AgenticOS 范式的完整 chat 示例

**Architecture:** Chat Agent（编排者）通过 ToolRegistry 调用 Loop Agent（skill `.agent.md`）和 Provider Agent（PDK `.so`），HydraForge 基础设施（DSLEngine/ToolRegistry/ILLMProvider）不包含业务逻辑。OS/Agent 边界由 Oracle 4 测试法确定。

**Tech Stack:** C++20, HydraForge PDK, nlohmann_json, Catch2, CMake 3.20+

**设计文档:** `docs/superpowers/specs/2026-07-16-pdk-chat-demo-design.md`

---

## 文件结构

```
examples/pdk_chat_demo/           # 新建
├── CMakeLists.txt
├── config.json
├── main.cpp
├── chat_session.h
├── chat_session.cpp
├── tools/
│   ├── fs_tools.cpp              # DECLARE_TOOL fs.read / fs.write
│   └── shell_tools.cpp           # DECLARE_TOOL shell.exec
└── lib/loop/
    └── react.agent.md            # Loop Agent skill

pdk/provider_agent/               # 新建
├── CMakeLists.txt
├── include/
│   └── provider_agent.h          # ProviderInfo, CredentialStore
└── src/
    ├── pdk_entry.cpp             # pdk_register_tools + plugin_info
    ├── credential_store.cpp      # 凭据管理
    └── provider_registry.cpp     # provider/register + resolve + list

tests/                            # 新建测试
├── test_chat_config.cpp          # JSON 解析测试
├── test_provider_agent.cpp       # Provider Agent 单元测试
└── test_pdk_chat_integration.cpp # E2E mock 集成测试
```

---

### Task 1: JSON 配置解析测试 + 实现

**Files:**
- Create: `tests/test_chat_config.cpp`
- Create: `examples/pdk_chat_demo/config.json`

- [ ] **Step 1: 落 config.json**

```json
{
  "schema_version": "1.0",
  "providers": {
    "openai": {
      "api_key_env": "OPENAI_API_KEY",
      "api_url": "https://api.openai.com/v1",
      "models": {
        "gpt-4o": { "model": "gpt-4o", "max_tokens": 4096, "temperature": 0.7 },
        "gpt-4o-mini": { "model": "gpt-4o-mini", "max_tokens": 2048 }
      }
    },
    "anthropic": {
      "api_key_env": "ANTHROPIC_API_KEY",
      "api_url": "https://api.anthropic.com/v1",
      "models": {
        "claude-sonnet": { "model": "claude-sonnet-4-20250514", "max_tokens": 4096 }
      }
    },
    "mock": {
      "models": {
        "test": { "model": "mock-llm-v1" }
      }
    }
  },
  "agent": {
    "loop_type": "react",
    "provider": "mock",
    "model": "test",
    "system_prompt": "You are a helpful coding assistant.",
    "tools": ["fs.read", "fs.write", "shell.exec"],
    "max_steps": 50,
    "timeout_ms": 300000
  }
}
```

- [ ] **Step 2: 写 ChatConfig 解析 + 单元测试**

```cpp
// tests/test_chat_config.cpp
#include "examples/pdk_chat_demo/chat_config.h"  // 同 main.cpp 内联定义
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// 内联 ChatConfig 定义（后续提取到独立头文件）
struct ModelConfig {
    std::string model;
    int max_tokens = 2048;
    double temperature = 0.7;
    int n_ctx = 0;
    static ModelConfig from_json(const json& j);
};

struct ProviderConfig {
    std::string api_key_env;
    std::string api_url;
    std::map<std::string, ModelConfig> models;
    static ProviderConfig from_json(const json& j);
};

struct AgentConfig {
    std::string loop_type;
    std::string provider;
    std::string model;
    std::string system_prompt;
    std::vector<std::string> tools;
    int max_steps = 50;
    int timeout_ms = 300000;
    static AgentConfig from_json(const json& j);
    void override_provider(const std::string& provider, const std::string& model);
};

struct ChatConfig {
    std::string schema_version;
    std::map<std::string, ProviderConfig> providers;
    AgentConfig agent;
    static ChatConfig from_json(const json& j);
    static ChatConfig from_file(const std::string& path);
    json to_provider_json() const;  // 序列化为 provider/register 的输入
};

TEST_CASE("ChatConfig::from_json parses minimal config", "[chat_config]") {
    json j = json::parse(R"({
        "schema_version": "1.0",
        "providers": {
            "mock": {
                "models": { "test": { "model": "mock-llm-v1" } }
            }
        },
        "agent": {
            "loop_type": "react",
            "provider": "mock",
            "model": "test",
            "system_prompt": "Hello",
            "tools": ["fs.read"]
        }
    })");
    auto config = ChatConfig::from_json(j);
    REQUIRE(config.schema_version == "1.0");
    REQUIRE(config.agent.loop_type == "react");
    REQUIRE(config.agent.provider == "mock");
    REQUIRE(config.agent.model == "test");
    REQUIRE(config.agent.tools.size() == 1);
    REQUIRE(config.agent.tools[0] == "fs.read");
}

TEST_CASE("ChatConfig::from_json parses multi-provider", "[chat_config]") {
    json j = json::parse(R"({
        "schema_version": "1.0",
        "providers": {
            "openai": {
                "api_key_env": "OPENAI_API_KEY",
                "api_url": "https://api.openai.com/v1",
                "models": {
                    "gpt-4o": { "model": "gpt-4o", "max_tokens": 4096 }
                }
            },
            "mock": {
                "models": { "test": { "model": "mock" } }
            }
        },
        "agent": {
            "loop_type": "react", "provider": "openai", "model": "gpt-4o",
            "system_prompt": "Hi", "tools": []
        }
    })");
    auto config = ChatConfig::from_json(j);
    REQUIRE(config.providers.size() == 2);
    REQUIRE(config.providers["openai"].models["gpt-4o"].max_tokens == 4096);
}

TEST_CASE("ChatConfig::override_provider for --mock mode", "[chat_config]") {
    json j = json::parse(R"({
        "schema_version": "1.0",
        "providers": {
            "openai": { "models": { "gpt-4o": { "model": "gpt-4o" } } },
            "mock":   { "models": { "test": { "model": "mock-llm-v1" } } }
        },
        "agent": {
            "loop_type": "react", "provider": "openai", "model": "gpt-4o",
            "system_prompt": "Hi", "tools": []
        }
    })");
    auto config = ChatConfig::from_json(j);
    config.agent.override_provider("mock", "test");
    REQUIRE(config.agent.provider == "mock");
    REQUIRE(config.agent.model == "test");
}

TEST_CASE("ChatConfig::to_provider_json serializes for provider/register", "[chat_config]") {
    auto config = ChatConfig::from_file("examples/pdk_chat_demo/config.json");
    auto prov_json = config.to_provider_json();
    REQUIRE(prov_json.contains("openai"));
    REQUIRE(prov_json["openai"].contains("api_url"));
}
```

- [ ] **Step 3: 编译测试验证失败**

Run: `cmake --preset tests && make test_chat_config && ctest -R chat_config`
Expected: FAIL — ChatConfig 类未定义

- [ ] **Step 4: 实现 ChatConfig 类**

```cpp
// examples/pdk_chat_demo/main.cpp 内嵌 ChatConfig（先内联，后重构提取）

// ... include nlohmann/json ...

struct ModelConfig {
    std::string model;
    int max_tokens = 2048;
    double temperature = 0.7;
    int n_ctx = 0;
    
    static ModelConfig from_json(const json& j) {
        return {
            j.value("model", ""),
            j.value("max_tokens", 2048),
            j.value("temperature", 0.7),
            j.value("n_ctx", 0)
        };
    }
    
    json to_json() const {
        json j;
        j["model"] = model;
        j["max_tokens"] = max_tokens;
        j["temperature"] = temperature;
        if (n_ctx > 0) j["n_ctx"] = n_ctx;
        return j;
    }
};

struct ProviderConfig {
    std::string api_key_env;
    std::string api_url;
    std::map<std::string, ModelConfig> models;
    
    static ProviderConfig from_json(const json& j) {
        ProviderConfig p;
        p.api_key_env = j.value("api_key_env", "");
        p.api_url = j.value("api_url", "");
        for (auto& [name, m] : j["models"].items())
            p.models[name] = ModelConfig::from_json(m);
        return p;
    }
};

struct AgentConfig {
    std::string loop_type = "react";
    std::string provider = "mock";
    std::string model = "test";
    std::string system_prompt;
    std::vector<std::string> tools;
    int max_steps = 50;
    int timeout_ms = 300000;
    
    static AgentConfig from_json(const json& j) {
        AgentConfig a;
        a.loop_type = j.value("loop_type", "react");
        a.provider = j.value("provider", "mock");
        a.model = j.value("model", "test");
        a.system_prompt = j.value("system_prompt", "");
        if (j.contains("tools"))
            for (auto& t : j["tools"]) a.tools.push_back(t.get<std::string>());
        a.max_steps = j.value("max_steps", 50);
        a.timeout_ms = j.value("timeout_ms", 300000);
        return a;
    }
    
    void override_provider(const std::string& p, const std::string& m) {
        provider = p; model = m;
    }
};

struct ChatConfig {
    std::string schema_version;
    std::map<std::string, ProviderConfig> providers;
    AgentConfig agent;
    
    static ChatConfig from_json(const json& j) {
        ChatConfig c;
        c.schema_version = j.value("schema_version", "1.0");
        for (auto& [name, p] : j["providers"].items())
            c.providers[name] = ProviderConfig::from_json(p);
        c.agent = AgentConfig::from_json(j["agent"]);
        return c;
    }
    
    static ChatConfig from_file(const std::string& path) {
        std::ifstream in(path);
        return from_json(json::parse(in));
    }
    
    json to_provider_json() const {
        json j;
        for (auto& [name, p] : providers) {
            json prov;
            if (!p.api_url.empty()) prov["api_url"] = p.api_url;
            if (!p.api_key_env.empty()) prov["api_key_env"] = p.api_key_env;
            json models_json;
            for (auto& [mname, mcfg] : p.models)
                models_json[mname] = mcfg.to_json();
            prov["models"] = models_json;
            j[name] = prov;
        }
        return j;
    }
};
```

- [ ] **Step 5: 运行测试验证通过**

Run: `cmake --preset tests && make test_chat_config && ctest -R chat_config`
Expected: 4/4 PASS

- [ ] **Step 6: Commit**

```bash
git add examples/pdk_chat_demo/config.json tests/test_chat_config.cpp
git commit -m "feat(pdk_chat): add ChatConfig JSON parser with tests"
```

---

### Task 2: Provider Agent — 接口头文件

**Files:**
- Create: `pdk/provider_agent/include/provider_agent.h`

- [ ] **Step 1: 创建 provider_agent.h**

```cpp
// pdk/provider_agent/include/provider_agent.h
// 功能描述：Provider Agent 数据结构和 CredentialStore 声明
// 设计依据：docs/superpowers/specs/2026-07-16-pdk-chat-demo-design.md §4

#pragma once

#include <map>
#include <string>
#include <nlohmann/json.hpp>

#include "common/llm/llm_config.h"

namespace hydraforge::provider_agent {

using json = nlohmann::json;

/** @brief Provider 凭据存储 — 延迟解析 api_key */
struct CredentialStore {
    std::map<std::string, std::string> env_keys;    // provider_id → env var name
    std::map<std::string, std::string> file_keys;   // provider_id → file path
    
    bool register_env(const std::string& provider_id, const std::string& env_var);
    bool register_file(const std::string& provider_id, const std::string& path);
    std::string resolve(const std::string& provider_id) const;
};

/** @brief Provider 完整配置 */
struct ProviderInfo {
    std::string id;
    std::string api_url;
    std::string api_key_env;
    std::map<std::string, agenticdsl::LLMConfig> models;  // model_name → LLMConfig
    
    static ProviderInfo from_json(const std::string& id, const json& j);
};

/** @brief Provider 注册表 — 内存中的 provider 集合 */
class ProviderRegistry {
public:
    void register_provider(const std::string& id, const ProviderInfo& info);
    bool has(const std::string& id) const;
    
    /** resolve provider + model → LLMConfig (含延迟解析的 api_key) */
    agenticdsl::LLMConfig resolve(const std::string& provider_id,
                                   const std::string& model_id,
                                   const CredentialStore& creds) const;
    
    /** 列出所有注册的 provider */
    json list() const;
    
private:
    std::map<std::string, ProviderInfo> providers_;
};

} // namespace hydraforge::provider_agent
```

- [ ] **Step 2: Commit**

```bash
git add pdk/provider_agent/include/provider_agent.h
git commit -m "feat(provider_agent): add ProviderInfo and CredentialStore headers"
```

---

### Task 3: Provider Agent — credential_store 实现 + 测试

**Files:**
- Create: `pdk/provider_agent/src/credential_store.cpp`
- Create: `tests/test_provider_agent.cpp`（含 CredentialStore 测试部分）

- [ ] **Step 1: 写测试**

```cpp
// tests/test_provider_agent.cpp
#include <catch2/catch_test_macros.hpp>
#include "pdk/provider_agent/include/provider_agent.h"
#include <cstdlib>

using namespace hydraforge::provider_agent;

TEST_CASE("CredentialStore resolve from env", "[credential_store]") {
    setenv("TEST_PROV_KEY", "sk-test-123", 1);
    CredentialStore store;
    REQUIRE(store.register_env("openai", "TEST_PROV_KEY"));
    REQUIRE(store.resolve("openai") == "sk-test-123");
    unsetenv("TEST_PROV_KEY");
}

TEST_CASE("CredentialStore resolve missing returns empty", "[credential_store]") {
    CredentialStore store;
    REQUIRE(store.resolve("nonexistent") == "");
}

TEST_CASE("ProviderRegistry register and list", "[provider_agent]") {
    ProviderRegistry reg;
    ProviderInfo info;
    info.id = "openai";
    info.api_url = "https://api.openai.com/v1";
    reg.register_provider("openai", info);
    
    REQUIRE(reg.has("openai"));
    REQUIRE(!reg.has("nonexistent"));
    
    auto list = reg.list();
    REQUIRE(list.size() == 1);
    REQUIRE(list[0]["id"] == "openai");
}

TEST_CASE("ProviderRegistry resolve with credentials", "[provider_agent]") {
    setenv("TEST_OPENAI_KEY", "sk-test-456", 1);
    
    ProviderRegistry reg;
    ProviderInfo info;
    info.id = "openai";
    info.api_url = "https://api.openai.com/v1";
    info.api_key_env = "TEST_OPENAI_KEY";
    
    agenticdsl::LLMConfig model_cfg;
    model_cfg.provider = "openai";
    model_cfg.model = "gpt-4o";
    info.models["gpt-4o"] = model_cfg;
    
    reg.register_provider("openai", info);
    
    CredentialStore creds;
    creds.register_env("openai", "TEST_OPENAI_KEY");
    
    auto resolved = reg.resolve("openai", "gpt-4o", creds);
    REQUIRE(resolved.provider == "openai");
    REQUIRE(resolved.model == "gpt-4o");
    REQUIRE(resolved.api_key == "sk-test-456");
    
    unsetenv("TEST_OPENAI_KEY");
}
```

- [ ] **Step 2: 运行测试验证失败**

Run: `cmake --preset tests && make test_provider_agent && ctest -R provider_agent`
Expected: FAIL — credential_store.cpp 不存在

- [ ] **Step 3: 实现 credential_store.cpp**

```cpp
// pdk/provider_agent/src/credential_store.cpp
#include "pdk/provider_agent/include/provider_agent.h"
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace hydraforge::provider_agent {

bool CredentialStore::register_env(const std::string& provider_id, const std::string& env_var) {
    env_keys[provider_id] = env_var;
    return true;
}

bool CredentialStore::register_file(const std::string& provider_id, const std::string& path) {
    file_keys[provider_id] = path;
    return true;
}

std::string CredentialStore::resolve(const std::string& provider_id) const {
    // 优先 env
    auto env_it = env_keys.find(provider_id);
    if (env_it != env_keys.end()) {
        const char* val = std::getenv(env_it->second.c_str());
        if (val && val[0] != '\0') return std::string(val);
    }
    // 其次 file
    auto file_it = file_keys.find(provider_id);
    if (file_it != file_keys.end()) {
        std::ifstream in(file_it->second);
        if (in.is_open()) {
            std::stringstream ss;
            ss << in.rdbuf();
            auto key = ss.str();
            if (!key.empty() && key.back() == '\n') key.pop_back();
            return key;
        }
    }
    return "";
}

// ProviderRegistry implementation
void ProviderRegistry::register_provider(const std::string& id, const ProviderInfo& info) {
    providers_[id] = info;
}

bool ProviderRegistry::has(const std::string& id) const {
    return providers_.find(id) != providers_.end();
}

using agenticdsl::LLMConfig;

LLMConfig ProviderRegistry::resolve(const std::string& provider_id,
                                     const std::string& model_id,
                                     const CredentialStore& creds) const {
    auto pit = providers_.find(provider_id);
    if (pit == providers_.end())
        throw std::runtime_error("provider not found: " + provider_id);
    
    auto mit = pit->second.models.find(model_id);
    if (mit == pit->second.models.end())
        throw std::runtime_error("model not found: " + model_id);
    
    LLMConfig cfg = mit->second;
    cfg.provider = provider_id;
    
    if (!pit->second.api_url.empty())
        cfg.api_url = pit->second.api_url;
    
    std::string key = creds.resolve(provider_id);
    if (!key.empty())
        cfg.api_key = key;
    
    return cfg;
}

nlohmann::json ProviderRegistry::list() const {
    nlohmann::json result = nlohmann::json::array();
    for (auto& [id, info] : providers_) {
        nlohmann::json entry;
        entry["id"] = id;
        if (!info.api_url.empty()) entry["api_url"] = info.api_url;
        nlohmann::json models = nlohmann::json::array();
        for (auto& [mname, _] : info.models)
            models.push_back(mname);
        entry["models"] = models;
        result.push_back(entry);
    }
    return result;
}

} // namespace hydraforge::provider_agent
```

- [ ] **Step 4: 运行测试验证通过**

Run: `cmake --preset tests && make test_provider_agent && ctest -R provider_agent`
Expected: 4/4 PASS

- [ ] **Step 5: Commit**

```bash
git add pdk/provider_agent/src/credential_store.cpp tests/test_provider_agent.cpp
git commit -m "feat(provider_agent): add CredentialStore and ProviderRegistry with tests"
```

---

### Task 4: Provider Agent — PDK 插件入口

**Files:**
- Create: `pdk/provider_agent/src/pdk_entry.cpp`
- Create: `pdk/provider_agent/src/provider_registry.cpp`
- Create: `pdk/provider_agent/CMakeLists.txt`

- [ ] **Step 1: 创建 PDK 插件入口**

```cpp
// pdk/provider_agent/src/pdk_entry.cpp
// 功能描述：Provider Agent PDK 入口 — 注册 provider/resolve, register, list 工具
// 设计依据：pdk/model_router/model_registry.cpp 模式 + 设计文档 §4

#include "agenticdsl/contract/itool_registry.h"
#include "agenticdsl/pdk/pdk.h"
#include "agenticdsl/plugin/plugin_info.h"
#include "common/policy/execution_policy.h"
#include "pdk/provider_agent/include/provider_agent.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace hydraforge::provider_agent;

namespace {

// 进程级全局（PDK 插件生命周期内唯一）
ProviderRegistry g_registry;
CredentialStore   g_credentials;

// ── provider/register ──
void handle_register(json args, json& result) {
    for (auto& [id, prov_json] : args.items()) {
        ProviderInfo info = ProviderInfo::from_json(id, prov_json);
        g_registry.register_provider(id, info);
        
        // 注册凭据
        if (!info.api_key_env.empty())
            g_credentials.register_env(id, info.api_key_env);
    }
    result = json{{"ok", true}, {"count", args.size()}};
}

// ── provider/resolve ──
void handle_resolve(json args, json& result) {
    std::string provider_id = args.value("provider_id", "");
    std::string model_id    = args.value("model_id", "");
    
    if (provider_id.empty() || model_id.empty()) {
        result = json{{"error", "provider_id and model_id are required"}};
        return;
    }
    
    auto cfg = g_registry.resolve(provider_id, model_id, g_credentials);
    result["provider"]   = cfg.provider;
    result["model"]      = cfg.model;
    result["api_url"]    = cfg.api_url;
    result["api_key"]    = cfg.api_key;    // 调用方负责安全处理
    result["max_tokens"] = cfg.max_tokens;
    result["n_ctx"]      = cfg.n_ctx;
}

// ── provider/list ──
void handle_list(json /*args*/, json& result) {
    result = g_registry.list();
}

} // namespace

// PDK 入口
extern "C" void pdk_register_tools(::agenticdsl::IToolRegistry& registry) {
    using agenticdsl::ToolMetadata;
    using agenticdsl::ToolCategory;
    using agenticdsl::LayerProfile;
    using agenticdsl::ApprovalPolicy;
    
    registry.register_tool_function(
        "provider/register",
        ToolMetadata{"provider/register", "注册 provider 配置",
                     "provider", ToolCategory::ReadOnly,
                     LayerProfile::Workflow,
                     ApprovalPolicy{true, true, false, false}},  // plan + agent 审批
        [](const std::unordered_map<std::string, std::string>& raw) -> json {
            json args = json::parse(raw.at("args"));
            json result;
            handle_register(args, result);
            return result;
        }
    );
    
    registry.register_tool_function(
        "provider/resolve",
        ToolMetadata{"provider/resolve", "解析 provider + model → LLMConfig",
                     "provider", ToolCategory::ReadOnly,
                     LayerProfile::Workflow,
                     ApprovalPolicy{true, true, false, false}},
        [](const std::unordered_map<std::string, std::string>& raw) -> json {
            json args = json::parse(raw.at("args"));
            json result;
            handle_resolve(args, result);
            return result;
        }
    );
    
    registry.register_tool_function(
        "provider/list",
        ToolMetadata{"provider/list", "列出所有已注册 provider",
                     "provider", ToolCategory::ReadOnly,
                     LayerProfile::Workflow,
                     ApprovalPolicy{true, true, false, false}},
        [](const std::unordered_map<std::string, std::string>& raw) -> json {
            json args = json::parse(raw.at("args"));
            json result;
            handle_list(args, result);
            return result;
        }
    );
}

extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
    "provider_agent",
    "0.1.0",
    "HydraForge Provider Agent — credential management + LLMConfig resolution",
    "HydraForge PDK"
};
```

- [ ] **Step 2: 实现 ProviderInfo::from_json**

```cpp
// pdk/provider_agent/src/provider_registry.cpp
#include "pdk/provider_agent/include/provider_agent.h"

namespace hydraforge::provider_agent {

ProviderInfo ProviderInfo::from_json(const std::string& id, const json& j) {
    ProviderInfo info;
    info.id = id;
    info.api_url = j.value("api_url", "");
    info.api_key_env = j.value("api_key_env", "");
    
    if (j.contains("models")) {
        for (auto& [mname, mjson] : j["models"].items()) {
            agenticdsl::LLMConfig cfg;
            cfg.model = mjson.value("model", mname);
            cfg.max_tokens = mjson.value("max_tokens", 2048);
            cfg.temperature = static_cast<float>(mjson.value("temperature", 0.7));
            if (mjson.contains("n_ctx"))
                cfg.n_ctx = mjson["n_ctx"];
            info.models[mname] = cfg;
        }
    }
    return info;
}

} // namespace hydraforge::provider_agent
```

- [ ] **Step 3: 创建 CMakeLists.txt**

```cmake
# pdk/provider_agent/CMakeLists.txt
add_library(hydraforge_provider_agent SHARED
    src/pdk_entry.cpp
    src/credential_store.cpp
    src/provider_registry.cpp
)

target_include_directories(hydraforge_provider_agent PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    .
)

target_link_libraries(hydraforge_provider_agent PRIVATE
    hydraforge_pdk
    nlohmann_json::nlohmann_json
)

set_target_properties(hydraforge_provider_agent PROPERTIES
    PREFIX "lib"
    SUFFIX ".so"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pdk/provider_agent"
)
```

- [ ] **Step 4: 注册到根 CMake**

在根 `CMakeLists.txt` 的 PDK 子目录部分添加：
```cmake
add_subdirectory(pdk/provider_agent)
```

- [ ] **Step 5: Commit**

```bash
git add pdk/provider_agent/src/pdk_entry.cpp pdk/provider_agent/src/provider_registry.cpp \
        pdk/provider_agent/CMakeLists.txt
git commit -m "feat(provider_agent): add PDK entry with provider/register, resolve, list tools"
```

---

### Task 5: Loop Agent — react.agent.md skill

**Files:**
- Create: `examples/pdk_chat_demo/lib/loop/react.agent.md`

- [ ] **Step 1: 创建 react.agent.md**

```markdown
# ReactLoop Agent

> skill 实现：think → decide → tool_call → observe 循环

## metadata
- version: 0.1
- loop_type: react

## context
- system_prompt: string
- history: string
- user_input: string
- active_tools: json array

## nodes

### think
- type: generate
- prompt: "{{system_prompt}}\n\nConversation history:\n{{history}}\n\nUser: {{user_input}}\n\nDecide: respond directly or call a tool? If calling a tool, use this JSON format:\n{\"tool\": \"tool_name\", \"args\": {...}}"
- output: llm_response

### parse_response
- type: assign
- assign:
    response_text: "{{llm_response.content}}"
    is_tool_call: "{{llm_response.content starts_with '{'}}"

### decide
- type: condition
- condition: "{{is_tool_call}} == true"
- true: execute_tool
- false: respond

### execute_tool
- type: tool
- name: "{{parsed_tool_name}}"
- args: "{{parsed_tool_args}}"
- output: tool_result
- on_error: observe_error

### observe
- type: assign
- assign:
    history: "{{history}}\nTool {{tool_name}}: {{tool_result}}"
    step_count: "{{step_count}} + 1"
- condition:
    condition: "{{step_count}} < {{max_steps}}"
    true: think
    false: respond_limit

### observe_error
- type: assign
- assign:
    history: "{{history}}\nTool error: {{error}}"
    step_count: "{{step_count}} + 1"
- goto: think

### respond
- type: assign
- assign:
    response: "{{response_text}}"
- type: end

### respond_limit
- type: assign
- assign:
    response: "Step limit ({{max_steps}}) reached. Last tool result: {{tool_result}}"
- type: end
```

- [ ] **Step 2: Commit**

```bash
git add examples/pdk_chat_demo/lib/loop/react.agent.md
git commit -m "feat(loop_agent): add react.agent.md skill definition"
```

---

### Task 6: 业务工具 — fs_tools + shell_tools

**Files:**
- Create: `examples/pdk_chat_demo/tools/fs_tools.cpp`
- Create: `examples/pdk_chat_demo/tools/shell_tools.cpp`

- [ ] **Step 1: fs_tools.cpp — DECLARE_TOOL fs.read / fs.write**

```cpp
// examples/pdk_chat_demo/tools/fs_tools.cpp
#include "agenticdsl/pdk/pdk.h"
#include "agenticdsl/pdk/safe_exec.h"
#include <fstream>
#include <sstream>

using json = nlohmann::json;
using namespace hydraforge::pdk;

DECLARE_TOOL(fs_read, "Read a file from the filesystem", ReadOnly, "plan",
    std::string path = __pdk_args.value("path", "");
    int offset = __pdk_args.value("offset", 1);
    int limit = __pdk_args.value("limit", 2000);
    
    std::ifstream in(path);
    if (!in.is_open())
        return json{{"error", "cannot open file: " + path}};
    
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line))
        lines.push_back(line);
    
    int start = std::max(0, offset - 1);
    int end   = std::min((int)lines.size(), start + limit);
    
    std::stringstream ss;
    for (int i = start; i < end; ++i)
        ss << (i + 1) << ": " << lines[i] << "\n";
    
    json result;
    result["content"] = ss.str();
    result["total_lines"] = (int)lines.size();
    result["shown_lines"] = end - start;
    if (end < (int)lines.size())
        result["truncated"] = true;
    result["hint"] = "Use offset=" + std::to_string(end + 1) + " to continue";
    return result;
)

DECLARE_TOOL(fs_write, "Write content to a file (overwrites)", WriteFile, "plan",
    std::string path = __pdk_args.value("path", "");
    std::string content = __pdk_args.value("content", "");
    
    std::ofstream out(path);
    if (!out.is_open())
        return json{{"error", "cannot write to file: " + path}};
    
    out << content;
    out.close();
    
    return json{
        {"ok", true},
        {"path", path},
        {"bytes_written", content.size()}
    };
)
```

- [ ] **Step 2: shell_tools.cpp — DECLARE_TOOL shell.exec**

```cpp
// examples/pdk_chat_demo/tools/shell_tools.cpp
#include "agenticdsl/pdk/pdk.h"
#include "agenticdsl/pdk/safe_exec.h"
#include <cstdio>

using json = nlohmann::json;
using namespace hydraforge::pdk;

DECLARE_TOOL(shell_exec, "Execute a shell command and return output", ReadOnly, "plan",
    std::string command = __pdk_args.value("command", "");
    int timeout_ms = __pdk_args.value("timeout", 30000);
    
    // 安全白名单检查
    if (command.find("rm -rf") != std::string::npos ||
        command.find("mkfs") != std::string::npos ||
        command.find("| sh") != std::string::npos) {
        return json{{"error", "dangerous command blocked"}};
    }
    
    auto result = hydraforge::pdk::SafeExec()
        .with_timeout(timeout_ms)
        .run([&]() -> json {
            std::string full_cmd = command + " 2>&1";
            FILE* pipe = popen(full_cmd.c_str(), "r");
            if (!pipe)
                return json{{"error", "failed to execute command"}};
            
            std::string output;
            char buffer[4096];
            while (fgets(buffer, sizeof(buffer), pipe))
                output += buffer;
            
            int exit_code = pclose(pipe);
            
            json result;
            result["output"] = output;
            result["exit_code"] = exit_code;
            return result;
        });
    
    return result;
)
```

- [ ] **Step 3: Commit**

```bash
git add examples/pdk_chat_demo/tools/
git commit -m "feat(pdk_chat): add fs.read, fs.write, shell.exec business tools"
```

---

### Task 7: ChatSession + main.cpp

**Files:**
- Create: `examples/pdk_chat_demo/chat_session.h`
- Create: `examples/pdk_chat_demo/chat_session.cpp`
- Create: `examples/pdk_chat_demo/main.cpp`
- Create: `examples/pdk_chat_demo/CMakeLists.txt`

- [ ] **Step 1: chat_session.h**

```cpp
// examples/pdk_chat_demo/chat_session.h
#pragma once

#include "core/engine.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include <memory>
#include <string>
#include <vector>

struct ChatResult {
    std::string response;
    int total_steps = 0;
    int tokens_used = 0;
};

struct AgentConfig;  // forward — 定义在 main.cpp

class ChatSession {
public:
    ChatSession(agenticdsl::DSLEngine* engine,
                std::shared_ptr<agenticdsl::IInteractionBus> bus,
                const AgentConfig& config);
    
    ChatResult chat(const std::string& user_input);
    void subscribe();

private:
    agenticdsl::DSLEngine* engine_;
    std::shared_ptr<agenticdsl::IInteractionBus> bus_;
    AgentConfig config_;
    std::vector<std::string> history_;  // 简化版：string vector
    int step_count_ = 0;
    int tokens_used_ = 0;
    
    std::string build_prompt(const std::string& user_input) const;
};
```

- [ ] **Step 2: chat_session.cpp**

```cpp
// examples/pdk_chat_demo/chat_session.cpp
#include "chat_session.h"
#include <nlohmann/json.hpp>

using json = nlohmann::json;

ChatSession::ChatSession(agenticdsl::DSLEngine* engine,
                         std::shared_ptr<agenticdsl::IInteractionBus> bus,
                         const AgentConfig& config)
    : engine_(engine), bus_(bus), config_(config) {}

std::string ChatSession::build_prompt(const std::string& user_input) const {
    std::string prompt = config_.system_prompt + "\n\n";
    for (auto& msg : history_)
        prompt += msg + "\n";
    prompt += "User: " + user_input + "\n";
    return prompt;
}

ChatResult ChatSession::chat(const std::string& user_input) {
    history_.push_back("User: " + user_input);
    
    std::string prompt = build_prompt(user_input);
    
    // 构造 Loop Agent 输入
    json loop_args;
    loop_args["prompt"] = prompt;
    loop_args["system_prompt"] = config_.system_prompt;
    loop_args["history"] = json(history_);
    loop_args["tools"] = json(config_.tools);
    loop_args["max_steps"] = config_.max_steps;
    
    // 调用 Loop Agent（通过 ToolRegistry）
    std::unordered_map<std::string, std::string> raw_args;
    raw_args["args"] = loop_args.dump();
    auto result_json = engine_->get_tool_registry()->call_tool("loop/run", raw_args);
    
    ChatResult result;
    result.response = result_json.value("response", "");
    result.total_steps = result_json.value("steps", 0);
    result.tokens_used = result_json.value("tokens_used", 0);
    
    history_.push_back("Assistant: " + result.response);
    return result;
}

void ChatSession::subscribe() {
    // 监听 loop/tool 事件 → 打印进度到 stderr
    bus_->subscribe("loop.turn.start", [](const json& ev) {
        std::cerr << "[turn " << ev["turn"] << "]" << std::endl;
    });
    bus_->subscribe("tool.execution.start", [](const json& ev) {
        std::cerr << "  → " << ev["name"] << std::endl;
    });
    bus_->subscribe("tool.execution.end", [](const json& ev) {
        std::cerr << "  ← " << ev["name"] << " (" << ev["duration_ms"] << "ms)" << std::endl;
    });
}
```

- [ ] **Step 3: main.cpp**

```cpp
// examples/pdk_chat_demo/main.cpp
// 功能描述：PDK Chat Demo — 演示 HydraForge AgenticOS 范式
// 设计依据：docs/superpowers/specs/2026-07-16-pdk-chat-demo-design.md

#include "core/engine.h"
#include "common/llm/mock_provider.h"
#include "common/llm/llm_provider_factory.h"
#include "common/tools/registry.h"
#include "agenticdsl/plugin/plugin_loader.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/cognitive/simple_orchestrator.h"

#include "chat_session.h"

#include <iostream>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace agenticdsl;

// ── ChatConfig 结构体（Task 1 中定义的内联代码）──
// （此处省略，参见 Task 1 Step 4 的完整实现）

// 声明业务工具注册函数
void register_fs_tools(IToolRegistry& registry);
void register_shell_tools(IToolRegistry& registry);

int main(int argc, char* argv[]) {
    bool mock_mode = (argc > 1 && std::string(argv[1]) == "--mock");
    
    // 1. 解析配置
    auto config = ChatConfig::from_file("examples/pdk_chat_demo/config.json");
    if (mock_mode) config.agent.override_provider("mock", "test");
    
    // 2. 初始化 AgenticOS
    auto engine = DSLEngine::from_markdown("### AgenticDSL `/main`\n```yaml\ngraph_type: subgraph\nnodes:\n  - id: start\n    type: start\n    next: [end]\n  - id: end\n    type: end\n```");
    auto bus = std::make_shared<InMemoryBus>();
    engine->set_interaction_bus(bus);
    
    // 3. 注册业务工具
    register_fs_tools(*engine->get_tool_registry());
    register_shell_tools(*engine->get_tool_registry());
    
    // 4. Mock 模式：设置固定 LLM 响应
    if (mock_mode) {
        auto* mock = dynamic_cast<MockLLMProvider*>(engine->get_llm_provider());
        if (mock) {
            mock->set_fixed_response(
                "I'll read the file for you.\n"
                "{\"tool\": \"fs.read\", \"args\": {\"path\": \"config.json\"}}\n"
                "The config.json shows schema_version 1.0 with mock provider."
            );
        }
    }
    
    // 5. 创建 ChatSession + 订阅事件
    ChatSession session(engine.get(), bus, config.agent);
    session.subscribe();
    
    // 6. 交互循环
    std::cout << "PDK Chat Demo (type 'exit' to quit)" << std::endl;
    std::string input;
    while (true) {
        std::cout << "\n> ";
        if (!std::getline(std::cin, input) || input == "exit") break;
        
        auto result = session.chat(input);
        std::cout << result.response << std::endl;
        std::cerr << "[steps=" << result.total_steps
                  << " tokens=" << result.tokens_used << "]" << std::endl;
    }
    
    return 0;
}
```

- [ ] **Step 4: CMakeLists.txt**

```cmake
# examples/pdk_chat_demo/CMakeLists.txt
add_executable(pdk_chat_demo
    main.cpp
    chat_session.cpp
    tools/fs_tools.cpp
    tools/shell_tools.cpp
)

target_include_directories(pdk_chat_demo PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(pdk_chat_demo PRIVATE
    agenticdsl_core
    nlohmann_json::nlohmann_json
)
```

- [ ] **Step 5: 编译验证**

Run: `cmake .. -DAGENTICDSL_BUILD_EXAMPLES=ON && make pdk_chat_demo`
Expected: BUILD SUCCESS

- [ ] **Step 6: Commit**

```bash
git add examples/pdk_chat_demo/
git commit -m "feat(pdk_chat): add ChatSession, main.cpp, and CMakeLists.txt"
```

---

### Task 8: E2E 集成测试（mock 模式）

**Files:**
- Create: `tests/test_pdk_chat_integration.cpp`

- [ ] **Step 1: 集成测试**

```cpp
// tests/test_pdk_chat_integration.cpp
#include <catch2/catch_test_macros.hpp>
#include "core/engine.h"
#include "common/llm/mock_provider.h"
#include "agenticdsl/contract/iinteraction_bus.h"
#include "examples/pdk_chat_demo/chat_session.h"
#include <nlohmann/json.hpp>

using namespace agenticdsl;

TEST_CASE("ChatSession::chat returns response in mock mode", "[pdk_chat]") {
    auto engine = DSLEngine::from_markdown("minimal");
    auto bus = std::make_shared<InMemoryBus>();
    engine->set_interaction_bus(bus);
    
    // Mock 响应
    auto* mock = dynamic_cast<MockLLMProvider*>(engine->get_llm_provider());
    REQUIRE(mock != nullptr);
    mock->set_fixed_response("Hello! How can I help you?");
    
    AgentConfig cfg;
    cfg.loop_type = "react";
    cfg.system_prompt = "You are helpful.";
    cfg.tools = {"fs.read"};
    cfg.max_steps = 10;
    
    ChatSession session(engine.get(), bus, cfg);
    auto result = session.chat("Hi!");
    
    REQUIRE(!result.response.empty());
}

TEST_CASE("ChatSession multi-turn preserves history", "[pdk_chat]") {
    auto engine = DSLEngine::from_markdown("minimal");
    auto bus = std::make_shared<InMemoryBus>();
    engine->set_interaction_bus(bus);
    
    auto* mock = dynamic_cast<MockLLMProvider*>(engine->get_llm_provider());
    REQUIRE(mock != nullptr);
    mock->set_fixed_response("Response 1");
    
    AgentConfig cfg;
    cfg.system_prompt = "Helpful.";
    cfg.max_steps = 10;
    
    ChatSession session(engine.get(), bus, cfg);
    
    auto r1 = session.chat("Q1");
    REQUIRE(!r1.response.empty());
    
    mock->set_fixed_response("Response 2");
    auto r2 = session.chat("Q2");
    REQUIRE(!r2.response.empty());
    REQUIRE(r2.response == "Response 2");
}
```

- [ ] **Step 2: 运行集成测试验证通过**

Run: `cmake --preset tests && make test_pdk_chat_integration && ctest -R pdk_chat`
Expected: 2/2 PASS

- [ ] **Step 3: Commit**

```bash
git add tests/test_pdk_chat_integration.cpp
git commit -m "test(pdk_chat): add E2E integration tests in mock mode"
```

---

### Task 9: README + 清理

**Files:**
- Create: `examples/pdk_chat_demo/README.md`

- [ ] **Step 1: README.md**

```markdown
# PDK Chat Demo

演示 HydraForge AgenticOS 范式：应用 = Agent 组合，Agent = skill 或 agenticdsl 实现。

## 架构

- **Chat Agent** (`main.cpp` + `ChatSession`) — 编排者
- **Loop Agent** (`lib/loop/react.agent.md`) — skill 实现
- **Provider Agent** (`pdk/provider_agent/`) — agenticdsl 实现
- **业务工具** (`tools/`) — fs.read / fs.write / shell.exec

## 运行

```bash
mkdir build && cd build
cmake .. -DAGENTICDSL_BUILD_EXAMPLES=ON
make -j$(nproc)

# Mock 模式（无需真实 LLM）
./examples/pdk_chat_demo/pdk_chat_demo --mock
```

## 配置

`config.json` 支持多 provider + per-agent loop_type。
```

- [ ] **Step 2: 最终验证**

Run: `cmake --preset tests && ctest -R pdk_chat --output-on-failure`
Expected: ALL PASS (6 test cases: 4 chat_config + 2 pdk_chat)

- [ ] **Step 3: Final commit**

```bash
git add examples/pdk_chat_demo/README.md
git commit -m "docs(pdk_chat): add README and final integration polish"
```

---

## 验证清单

- [ ] `ctest -R chat_config` — 4/4 PASS
- [ ] `ctest -R provider_agent` — 4/4 PASS
- [ ] `ctest -R pdk_chat` — 2/2 PASS（集成测试）
- [ ] `make pdk_chat_demo` — BUILD SUCCESS
- [ ] `./pdk_chat_demo --mock` — 交互运行成功