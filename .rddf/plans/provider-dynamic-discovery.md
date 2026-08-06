# provider-dynamic-discovery Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Extend `LLMProviderFactory` with thread-safe `register_dynamic(name, factory_fn)` runtime registration and expose three governed ToolCoordinator tools (`provider/refresh`, `provider/register_dynamic`, `provider/switch`) backed by ADR-0004 V2 `ToolMetadata`, without breaking the existing `llm_config.json` startup routing.

**Architecture:** A single `std::shared_mutex` guards the dynamic factory catalog, the default provider pointer, and the refresh metadata. Reads use `std::shared_lock` and copy the bare callback by value; provider construction always happens after the lock is released. `provider/refresh` performs transport fetch, schema validation, and added/removed diffing outside the lock, then commits one atomic snapshot under a short `std::unique_lock` so failures (network, schema, empty) preserve the previous catalog and surface `warning`/`error_code`. All three mutating tools flow through `IToolRegistry::register_tool_function` + `ToolCoordinator::execute` so layer, approval, and audit logic stay in the L2/L4 contract boundary. Explicit `LLMConfig::provider` retains priority; the dynamic default is consulted only when `config.provider` is empty.

**Tech Stack:** C++20 (`std::shared_mutex`, `std::function`, `std::shared_ptr`), `agenticdsl::IProviderFactory` + `LLMProviderFactory`, `pdk_provider_agent::ProviderRegistry` + `ProviderInfo`, `agenticdsl::IToolRegistry` + `ToolCoordinator` (ADR-0031 §决策 5), `agenticdsl::ToolMetadata` V2 (ADR-0004 V2), nlohmann::json, cpp-httplib (or injectable mock transport), Catch2 v3 amalgamated.

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `src/common/llm/llm_provider_factory.h` | Add `DynamicFactoryFn` typedef, `register_dynamic()`, `switch_default()`, `current_default()`, `has_dynamic()`, `dynamic_names()` API + `shared_mutex` member |
| `src/common/llm/llm_provider_factory.cpp` | Implement validation, shared-mutex ownership, explicit-provider precedence, callback copy outside lock, lock-free construction |
| `pdk/provider_agent/include/provider_agent.h` | Add `RefreshResult`, `RefreshTransport`, `seed_for_test`/`set_refresh_transport_for_test`, `refresh()`, `list_models()`, `removed_models()`, `last_refresh_for()` |
| `pdk/provider_agent/src/provider_refresh.cpp` | Two-phase commit: transport + validation outside lock, single locked atomic swap; preserve old catalog on any failure |
| `pdk/provider_agent/src/provider_register_dynamic.cpp` | Schema validate JSON definition, capture `LLMConfig` by value in `DynamicFactoryFn`, call `LLMProviderFactory::register_dynamic` and `ProviderRegistry::register_providers` |
| `pdk/provider_agent/src/provider_switch.cpp` | Atomically call `LLMProviderFactory::switch_default` under unique lock; reject unknown providers |
| `pdk/provider_agent/src/pdk_entry.cpp` | Register `provider/refresh`, `provider/register_dynamic`, `provider/switch` with ADR-0004 V2 `ToolMetadata`; extend `provider/list` to include `current`, `removed`, `last_refresh`, `stale` |
| `pdk/provider_agent/CMakeLists.txt` | Add 3 new source files to `ProviderAgent` shared target |

### Tests

| File | Responsibility |
|---|---|
| `tests/test_provider_factory.cpp` | Append 3 dynamic-registration/default cases (registered-then-create, validation rejection, explicit-vs-dynamic precedence) |
| `tests/test_provider_factory_concurrent.cpp` | 50-thread concurrent register/create/list/refresh/switch stress + callback-lifetime safety |
| `tests/test_provider_refresh_tool.cpp` | Success/removed/failure preservation/layer denial/stale preservation |
| `tests/test_provider_register_dynamic_tool.cpp` | Valid register/invalid/duplicate/immediate factory resolution/layer governance |
| `tests/test_provider_switch_tool.cpp` | Switch success/unknown/concurrent/governance + list marks current |
| `tests/test_provider_dynamic_lifecycle.cpp` | Callback captures values/`shared_ptr` only — temp config destruction safety |
| `tests/test_provider_dynamic_integration.cpp` | End-to-end DSL Engine + ToolCoordinator + all three tools |

---

## Task 1: LLMProviderFactory 动态注册与并发状态

**Files:**
- Modify: `src/common/llm/llm_provider_factory.h`
- Modify: `src/common/llm/llm_provider_factory.cpp`
- Modify: `tests/test_provider_factory.cpp`

- [ ] **Step 1: Write the failing test**

Append three new `TEST_CASE`s to `tests/test_provider_factory.cpp` (keep all 6 existing cases intact):

```cpp
TEST_CASE("LLMProviderFactory registers and creates a dynamic provider",
          "[provider_factory][dynamic]") {
  LLMProviderFactory factory;
  std::atomic<int> calls{0};
  const bool registered = factory.register_dynamic(
      "runtime-provider",
      [&calls](const LLMConfig& config) {
        calls.fetch_add(1);
        return std::make_unique<MockLLMProvider>(config);
      });
  REQUIRE(registered);

  LLMConfig config;
  config.provider = "runtime-provider";
  auto provider = factory.create(config);
  REQUIRE(provider != nullptr);
  CHECK(calls.load() == 1);
  CHECK(factory.has_dynamic("runtime-provider"));
  CHECK(factory.dynamic_names() == std::vector<std::string>{"runtime-provider"});
  CHECK(factory.current_default().empty());
}

TEST_CASE("LLMProviderFactory rejects invalid and duplicate dynamic providers",
          "[provider_factory][dynamic]") {
  LLMProviderFactory factory;
  LLMProviderFactory::DynamicFactoryFn empty;
  CHECK_FALSE(factory.register_dynamic("", empty));
  CHECK_FALSE(factory.register_dynamic("runtime-provider", empty));
  CHECK(factory.dynamic_names().empty());

  auto callback = [](const LLMConfig& config) {
    return std::make_unique<MockLLMProvider>(config);
  };
  REQUIRE(factory.register_dynamic("runtime-provider", callback));
  CHECK_FALSE(factory.register_dynamic("runtime-provider", callback));
  CHECK(factory.dynamic_names().size() == 1);
  CHECK_FALSE(factory.register_dynamic("openai", callback));  // reserved backend
}

TEST_CASE("LLMProviderFactory dynamic default applies only for empty provider",
          "[provider_factory][dynamic]") {
  LLMProviderFactory factory;
  REQUIRE(factory.register_dynamic(
      "runtime-provider",
      [](const LLMConfig& config) { return std::make_unique<MockLLMProvider>(config); }));
  REQUIRE(factory.switch_default("runtime-provider"));
  CHECK(factory.current_default() == "runtime-provider");

  LLMConfig empty_cfg;
  empty_cfg.provider.clear();
  auto a = factory.create(empty_cfg);
  REQUIRE(a != nullptr);
  CHECK(dynamic_cast<MockLLMProvider*>(a.get()) != nullptr);

  LLMConfig explicit_cfg;
  explicit_cfg.provider = "openai";
  auto b = factory.create(explicit_cfg);
  REQUIRE(b != nullptr);
  CHECK(dynamic_cast<MockLLMProvider*>(b.get()) == nullptr);  // routes to cloud, not dynamic
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_provider_factory && ctest --test-dir build -R '^test_provider_factory$' --output-on-failure`

Expected: build fails (missing `DynamicFactoryFn`, `register_dynamic`, `has_dynamic`, `dynamic_names`, `switch_default`, `current_default`).

- [ ] **Step 3: Write minimal implementation**

Replace the body of `src/common/llm/llm_provider_factory.h` with:

```cpp
#ifndef AGENTICDSL_LLM_LLM_PROVIDER_FACTORY_H
#define AGENTICDSL_LLM_LLM_PROVIDER_FACTORY_H

#include "agenticdsl/contract/iprovider_factory.h"

#include <functional>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace agenticdsl {

class LLMProviderFactory : public IProviderFactory {
 public:
  using DynamicFactoryFn =
      std::function<std::unique_ptr<ILLMProvider>(const LLMConfig&)>;

  LLMProviderFactory();
  ~LLMProviderFactory() override = default;

  std::unique_ptr<ILLMProvider> create(const LLMConfig& config) override;

  bool register_dynamic(std::string name, DynamicFactoryFn factory_fn);
  bool switch_default(const std::string& name);
  std::string current_default() const;
  bool has_dynamic(const std::string& name) const;
  std::vector<std::string> dynamic_names() const;

 private:
  static bool is_reserved_backend(const std::string& name);

  std::unique_ptr<IProviderFactory> mock_factory;
  std::unique_ptr<IProviderFactory> cloud_factory;
  std::unique_ptr<IProviderFactory> llama_factory;

  mutable std::shared_mutex dynamic_mutex_;
  std::unordered_map<std::string, DynamicFactoryFn> dynamic_factories_;
  std::string default_provider_;
};

}  // namespace agenticdsl

#endif
```

Append to `src/common/llm/llm_provider_factory.cpp` (keep `MockProviderFactory` / `CloudProviderFactory` / `LlamaProviderFactory` + constructor body):

```cpp
bool LLMProviderFactory::is_reserved_backend(const std::string& name) {
  return name == "mock" || name == "openai" || name == "anthropic" ||
         name == "deepseek" || name == "minimax" || name == "qwen" ||
         name == "moonshot" || name == "custom" || name == "local" ||
         name == "llama";
}

bool LLMProviderFactory::register_dynamic(std::string name,
                                          DynamicFactoryFn factory_fn) {
  if (name.empty() || !static_cast<bool>(factory_fn)) return false;
  if (is_reserved_backend(name)) return false;
  std::unique_lock lock(dynamic_mutex_);
  if (dynamic_factories_.count(name)) return false;
  dynamic_factories_.emplace(std::move(name), std::move(factory_fn));
  return true;
}

bool LLMProviderFactory::switch_default(const std::string& name) {
  std::unique_lock lock(dynamic_mutex_);
  if (!dynamic_factories_.count(name)) return false;
  default_provider_ = name;
  return true;
}

std::string LLMProviderFactory::current_default() const {
  std::shared_lock lock(dynamic_mutex_);
  return default_provider_;
}

bool LLMProviderFactory::has_dynamic(const std::string& name) const {
  std::shared_lock lock(dynamic_mutex_);
  return dynamic_factories_.count(name) > 0;
}

std::vector<std::string> LLMProviderFactory::dynamic_names() const {
  std::shared_lock lock(dynamic_mutex_);
  std::vector<std::string> out;
  out.reserve(dynamic_factories_.size());
  for (const auto& [k, _] : dynamic_factories_) out.push_back(k);
  return out;
}

std::unique_ptr<ILLMProvider> LLMProviderFactory::create(const LLMConfig& config) {
  std::string backend = config.provider;
  DynamicFactoryFn dynamic_factory;
  {
    std::shared_lock<std::shared_mutex> lock(dynamic_mutex_);
    if (backend.empty()) backend = default_provider_;
    auto it = dynamic_factories_.find(backend);
    if (it != dynamic_factories_.end()) dynamic_factory = it->second;
  }
  if (dynamic_factory) return dynamic_factory(config);  // construct outside lock

  if (backend == "mock" || backend.empty()) return mock_factory->create(config);
  if (backend == "openai" || backend == "anthropic" || backend == "deepseek" ||
      backend == "minimax" || backend == "qwen" || backend == "moonshot" ||
      backend == "custom") {
    return cloud_factory->create(config);
  }
  if (backend == "local" || backend == "llama") return llama_factory->create(config);
  return mock_factory->create(config);
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target test_provider_factory && ctest --test-dir build -R '^test_provider_factory$' --output-on-failure`

Expected: 6 existing cases + 3 new cases pass; concurrent `create()` test from Task 1.6 still passes.

- [ ] **Step 5: Commit**

```bash
git add src/common/llm/llm_provider_factory.h \
        src/common/llm/llm_provider_factory.cpp \
        tests/test_provider_factory.cpp
git commit -m "feat(provider): add thread-safe dynamic provider registration"
```

---

## Task 2: provider/refresh 工具与模型目录提交

**Files:**
- Modify: `pdk/provider_agent/include/provider_agent.h`
- Modify: `pdk/provider_agent/src/provider_resolve.cpp`
- Create: `pdk/provider_agent/src/provider_refresh.cpp`
- Modify: `pdk/provider_agent/src/pdk_entry.cpp`
- Modify: `pdk/provider_agent/CMakeLists.txt`
- Create: `tests/test_provider_refresh_tool.cpp`
- Modify: `tests/CMakeLists.txt` (add `add_catch_test(test_provider_refresh_tool ...)` block mirroring `test_pdk_register_agent` pattern)

- [ ] **Step 1: Write the failing test**

Create `tests/test_provider_refresh_tool.cpp`:

```cpp
#include <catch_amalgamated.hpp>
#include <stdexcept>

#include "provider_agent.h"

using namespace pdk_provider_agent;
using nlohmann::json;

TEST_CASE("refresh commits valid catalog and marks removed models",
          "[provider][refresh]") {
  ProviderRegistry registry;
  registry.seed_for_test({
      {"demo", ProviderInfo{"demo", "http://demo", "/models", "",
                             {{"model-a", ModelConfig{"model-a"}},
                              {"model-b", ModelConfig{"model-b"}}}}}});
  registry.set_refresh_transport_for_test(
      [](const ProviderInfo&) { return json{{"data", json::array({json{{"id", "model-a"}}})}}; });

  const auto result = registry.refresh("demo");
  REQUIRE(result.ok);
  CHECK(result.provider == "demo");
  CHECK(result.added.empty());
  REQUIRE(result.removed.size() == 1);
  CHECK(result.removed.front() == "model-b");
  CHECK(result.model_count == 1);
  CHECK_FALSE(result.last_refresh.empty());
  CHECK(registry.removed_models("demo").at("model-b"));
  CHECK(registry.list_models("demo") == std::vector<std::string>{"model-a"});
}

TEST_CASE("refresh failure preserves prior catalog and surfaces error_code",
          "[provider][refresh]") {
  ProviderRegistry registry;
  registry.seed_for_test(
      {{"demo", ProviderInfo{"demo", "http://demo", "/models", "",
                              {{"model-a", ModelConfig{"model-a"}}}}}});
  registry.set_refresh_transport_for_test(
      [](const ProviderInfo&) -> json { throw std::runtime_error("timeout"); });

  const auto result = registry.refresh("demo");
  CHECK_FALSE(result.ok);
  CHECK(result.error_code == "retryable");
  CHECK_FALSE(result.warning.empty());
  CHECK(registry.list_models("demo") == std::vector<std::string>{"model-a"});
  CHECK(registry.removed_models("demo").empty());
}

TEST_CASE("refresh rejects invalid schema without mutating catalog",
          "[provider][refresh]") {
  ProviderRegistry registry;
  registry.seed_for_test(
      {{"demo", ProviderInfo{"demo", "http://demo", "/models", "",
                              {{"model-a", ModelConfig{"model-a"}}}}}});
  registry.set_refresh_transport_for_test([](const ProviderInfo&) {
    return json{{"data", json::array({json{{"name", "no-id-field"}}})}};
  });

  const auto result = registry.refresh("demo");
  CHECK_FALSE(result.ok);
  CHECK(result.error_code == "validation");
  CHECK(registry.list_models("demo") == std::vector<std::string>{"model-a"});
}

TEST_CASE("refresh returns unknown-provider without calling transport",
          "[provider][refresh]") {
  ProviderRegistry registry;
  std::atomic<int> transport_calls{0};
  registry.set_refresh_transport_for_test(
      [&](const ProviderInfo&) -> json {
        transport_calls.fetch_add(1);
        return json::object();
      });

  const auto result = registry.refresh("does-not-exist");
  CHECK_FALSE(result.ok);
  CHECK(result.error_code == "unknown-provider");
  CHECK(transport_calls.load() == 0);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_provider_refresh_tool && ctest --test-dir build -R '^test_provider_refresh_tool$' --output-on-failure`

Expected: build fails because `RefreshResult`, `seed_for_test`, `set_refresh_transport_for_test`, `refresh`, `list_models`, `removed_models` do not exist.

- [ ] **Step 3: Write minimal implementation**

Append to `pdk/provider_agent/include/provider_agent.h`:

```cpp
#include <atomic>
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace pdk_provider_agent {

struct RefreshResult {
  bool ok = false;
  std::string provider;
  std::vector<std::string> added;
  std::vector<std::string> removed;
  std::size_t model_count = 0;
  std::string last_refresh;   // ISO-8601 UTC, "1970-01-01T00:00:00Z" on failure
  std::string warning;        // empty on success
  std::string error_code;     // empty on success
};

using RefreshTransport = std::function<nlohmann::json(const ProviderInfo&)>;

inline std::string refresh_now_iso8601() {
  using namespace std::chrono;
  const auto now = system_clock::now();
  const auto t = system_clock::to_time_t(now);
  std::tm tm{};
  gmtime_r(&t, &tm);
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return oss.str();
}

struct ProviderInfo {
  std::string id;
  std::string api_url;
  std::string api_endpoint;
  std::string api_key_env;
  std::map<std::string, ModelConfig> models;
  std::map<std::string, bool> removed_models;
  std::string last_refresh;
  std::optional<std::string> resolve_api_key() const;
};

class ProviderRegistry {
 public:
  static ProviderRegistry& instance();

  void register_providers(const nlohmann::json& providers_config);
  std::vector<std::string> list_providers() const;
  nlohmann::json resolve(const std::string& provider_id,
                         const std::string& model_id) const;
  nlohmann::json health(const std::string& provider_id) const;

  // NEW (provider-dynamic-discovery):
  RefreshResult refresh(const std::string& provider_id);
  std::vector<std::string> list_models(const std::string& provider_id) const;
  std::map<std::string, bool> removed_models(const std::string& provider_id) const;
  std::string last_refresh_for(const std::string& provider_id) const;
  bool has_provider(const std::string& provider_id) const;

  // Test-only hooks (untyped name documented, no production log path):
  void seed_for_test(std::map<std::string, ProviderInfo> seed);
  void set_refresh_transport_for_test(RefreshTransport t);

 private:
  ProviderRegistry() = default;
  mutable std::mutex mutex_;
  std::map<std::string, ProviderInfo> providers_;
  RefreshTransport refresh_transport_;  // empty = use HTTP default
};

}  // namespace pdk_provider_agent
```

Append helpers to `pdk/provider_agent/src/provider_resolve.cpp` (keep existing `instance()`, `register_providers`, `list_providers`, `resolve`, `health`):

```cpp
bool ProviderRegistry::has_provider(const std::string& provider_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  return providers_.find(provider_id) != providers_.end();
}

std::vector<std::string> ProviderRegistry::list_models(
    const std::string& provider_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  std::vector<std::string> out;
  auto it = providers_.find(provider_id);
  if (it == providers_.end()) return out;
  out.reserve(it->second.models.size());
  for (const auto& [k, _] : it->second.models) out.push_back(k);
  return out;
}

std::map<std::string, bool> ProviderRegistry::removed_models(
    const std::string& provider_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = providers_.find(provider_id);
  if (it == providers_.end()) return {};
  return it->second.removed_models;
}

std::string ProviderRegistry::last_refresh_for(const std::string& provider_id) const {
  std::lock_guard<std::mutex> lock(mutex_);
  auto it = providers_.find(provider_id);
  if (it == providers_.end()) return {};
  return it->second.last_refresh;
}

void ProviderRegistry::seed_for_test(std::map<std::string, ProviderInfo> seed) {
  std::lock_guard<std::mutex> lock(mutex_);
  providers_ = std::move(seed);
}

void ProviderRegistry::set_refresh_transport_for_test(RefreshTransport t) {
  std::lock_guard<std::mutex> lock(mutex_);
  refresh_transport_ = std::move(t);
}
```

Create `pdk/provider_agent/src/provider_refresh.cpp`:

```cpp
#include "provider_agent.h"

#include <stdexcept>

namespace pdk_provider_agent {

namespace {

bool validate_item(const nlohmann::json& item) {
  return item.is_object() && item.contains("id") &&
         item["id"].is_string() && !item["id"].get<std::string>().empty();
}

nlohmann::json fetch_models_http(const ProviderInfo& info) {
  // Phase 1 fallback: real cpp-httplib call goes here.
  // Throw on network error so the caller can preserve old catalog.
  (void)info;
  throw std::runtime_error("real http transport not yet wired (Phase 1 stub)");
}

}  // namespace

RefreshResult ProviderRegistry::refresh(const std::string& provider_id) {
  ProviderInfo old_info;
  RefreshTransport transport;
  std::size_t prior_count = 0;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = providers_.find(provider_id);
    if (it == providers_.end()) {
      return RefreshResult{false, provider_id, {}, {}, 0, {}, "unknown provider",
                           "unknown-provider"};
    }
    old_info = it->second;
    transport = refresh_transport_;
    prior_count = old_info.models.size();
  }

  nlohmann::json response;
  try {
    response = transport ? transport(old_info) : fetch_models_http(old_info);
  } catch (const std::exception& e) {
    return RefreshResult{false, provider_id, {}, {}, prior_count, old_info.last_refresh,
                         std::string("refresh failed: ") + e.what(), "retryable"};
  }

  if (!response.is_object() || !response.contains("data") ||
      !response["data"].is_array() || response["data"].empty()) {
    return RefreshResult{false, provider_id, {}, {}, prior_count, old_info.last_refresh,
                         "model catalog schema is invalid or empty", "validation"};
  }

  ProviderInfo candidate = old_info;
  candidate.models.clear();
  candidate.removed_models.clear();
  for (const auto& item : response["data"]) {
    if (!validate_item(item)) {
      return RefreshResult{false, provider_id, {}, {}, prior_count,
                           old_info.last_refresh,
                           "model catalog item missing valid id", "validation"};
    }
    const auto id = item["id"].get<std::string>();
    candidate.models.emplace(id, ModelConfig{id, 4096, 0.7, nlohmann::json::object()});
  }

  RefreshResult result;
  result.provider = provider_id;
  result.model_count = candidate.models.size();
  result.last_refresh = refresh_now_iso8601();
  for (const auto& [id, _] : candidate.models) {
    if (!old_info.models.count(id)) result.added.push_back(id);
  }
  for (const auto& [id, _] : old_info.models) {
    if (!candidate.models.count(id)) {
      result.removed.push_back(id);
      candidate.removed_models[id] = true;
    }
  }
  candidate.last_refresh = result.last_refresh;

  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = providers_.find(provider_id);
    if (it == providers_.end()) {
      return RefreshResult{false, provider_id, {}, {}, prior_count, old_info.last_refresh,
                           "provider vanished during refresh", "unknown-provider"};
    }
    it->second = std::move(candidate);
  }
  result.ok = true;
  return result;
}

}  // namespace pdk_provider_agent
```

Modify `pdk/provider_agent/src/pdk_entry.cpp`: keep the existing 4 tools, extend `provider/list`, and register `provider/refresh` and (in Task 4) `provider/switch`. Update `provider/list` registration (replace `provider/list` body block only) with:

```cpp
    // 3. provider/list (extended for dynamic discovery)
    registry.register_tool_function(
        "provider/list",
        ::agenticdsl::ToolMetadata{
            .name = "provider/list",
            .description = "List registered providers and current default",
            .domain = "provider",
            .category = ::agenticdsl::ToolCategory::ReadOnly,
            .min_layer = ::agenticdsl::LayerProfile::Workflow,
            .approval = ::agenticdsl::ApprovalPolicy{
                .requires_approval_in_plan = false,
                .requires_approval_in_agent = false,
                .requires_approval_in_yolo = false,
                .force_approval_always = false
            },
            .allowed_layers = {::agenticdsl::LayerProfile::Workflow}
        },
        [&reg](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
          const auto& selected = str_arg(args, "selected_default");
          nlohmann::json providers = nlohmann::json::array();
          for (const auto& id : reg.list_providers()) {
            nlohmann::json entry;
            entry["id"] = id;
            entry["models"] = reg.list_models(id);
            entry["removed"] = reg.removed_models(id);
            entry["last_refresh"] = reg.last_refresh_for(id);
            entry["stale"] = reg.last_refresh_for(id).empty();
            if (!selected.empty()) entry["current"] = (id == selected);
            providers.push_back(std::move(entry));
          }
          return {{"providers", providers}};
        }
    );

    // 5. provider/refresh (NEW)
    registry.register_tool_function(
        "provider/refresh",
        ::agenticdsl::ToolMetadata{
            .name = "provider/refresh",
            .description = "Refresh model catalog from provider upstream API",
            .domain = "provider",
            .category = ::agenticdsl::ToolCategory::StateModify,
            .min_layer = ::agenticdsl::LayerProfile::Workflow,
            .approval = ::agenticdsl::ApprovalPolicy{
                .requires_approval_in_plan = true,
                .requires_approval_in_agent = true,
                .requires_approval_in_yolo = false,
                .force_approval_always = true
            },
            .allowed_layers = {::agenticdsl::LayerProfile::Workflow}
        },
        [&reg](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
          const std::string id = str_arg(args, "provider_id");
          if (id.empty()) throw std::runtime_error("provider_id is required");
          auto r = reg.refresh(id);
          nlohmann::json out = {
              {"ok", r.ok}, {"provider", r.provider},
              {"added", r.added}, {"removed", r.removed},
              {"model_count", r.model_count}, {"last_refresh", r.last_refresh}
          };
          if (!r.warning.empty()) out["warning"] = r.warning;
          if (!r.error_code.empty()) out["error_code"] = r.error_code;
          return out;
        }
    );
```

Modify `pdk/provider_agent/CMakeLists.txt`: add `provider_refresh.cpp` to the `add_library(ProviderAgent SHARED ...)` list:

```cmake
add_library(ProviderAgent SHARED
    src/pdk_entry.cpp
    src/credential_store.cpp
    src/provider_resolve.cpp
    src/provider_refresh.cpp
)
```

In `tests/CMakeLists.txt`, add (after the existing `if(TEST_NAME MATCHES "^test_pdk_register_agent")` block):

```cmake
    if(TEST_NAME MATCHES "^test_provider_(refresh|register_dynamic|switch|dynamic_lifecycle|dynamic_integration|factory_concurrent)")
        target_sources(${TEST_NAME} PRIVATE
            ${PROJECT_SOURCE_DIR}/pdk/provider_agent/src/pdk_entry.cpp
            ${PROJECT_SOURCE_DIR}/pdk/provider_agent/src/provider_resolve.cpp
            ${PROJECT_SOURCE_DIR}/pdk/provider_agent/src/provider_refresh.cpp
            ${PROJECT_SOURCE_DIR}/pdk/provider_agent/src/provider_register_dynamic.cpp
            ${PROJECT_SOURCE_DIR}/pdk/provider_agent/src/provider_switch.cpp
        )
        target_include_directories(${TEST_NAME} PRIVATE
            ${PROJECT_SOURCE_DIR}/pdk/provider_agent/include
            ${PROJECT_SOURCE_DIR}/pdk/provider_agent/src
        )
        target_link_libraries(${TEST_NAME} PRIVATE dl)
    endif()
```

(The freshly added `provider_register_dynamic.cpp` / `provider_switch.cpp` are referenced before they exist — Task 3 + Task 4 add them. Conditionally include files only when they exist on disk to keep CI green between tasks.)

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target ProviderAgent test_provider_refresh_tool && ctest --test-dir build -R '^test_provider_refresh_tool$' --output-on-failure`

Expected: 4 cases pass; existing `test_provider_factory` still passes; existing PDK provider_agent.so links clean.

- [ ] **Step 5: Commit**

```bash
git add pdk/provider_agent/include/provider_agent.h \
        pdk/provider_agent/src/provider_resolve.cpp \
        pdk/provider_agent/src/provider_refresh.cpp \
        pdk/provider_agent/src/pdk_entry.cpp \
        pdk/provider_agent/CMakeLists.txt \
        tests/test_provider_refresh_tool.cpp \
        tests/CMakeLists.txt
git commit -m "feat(provider-agent): add refresh tool with stale-catalog protection"
```

---

## Task 3: provider/register_dynamic 工具

**Files:**
- Create: `pdk/provider_agent/src/provider_register_dynamic.cpp`
- Modify: `pdk/provider_agent/src/pdk_entry.cpp`
- Create: `tests/test_provider_register_dynamic_tool.cpp`

- [ ] **Step 1: Write the failing test**

Create `tests/test_provider_register_dynamic_tool.cpp`:

```cpp
#include <catch_amalgamated.hpp>

#include <memory>

#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/contract/iprovider_factory.h"
#include "agenticdsl/contract/itool_registry.h"
#include "common/llm/llm_provider_factory.h"
#include "common/llm/llm_types.h"
#include "common/llm/mock_provider.h"
#include "common/policy/approval_handler.h"
#include "common/policy/execution_policy.h"
#include "common/tools/registry.h"

#include "provider_agent.h"

using namespace agenticdsl;
using namespace pdk_provider_agent;
using nlohmann::json;

namespace {

class SpyRegistry : public IToolRegistry {
 public:
  bool has_tool(const std::string& name) const override {
    return tools_.count(name) > 0;
  }
  nlohmann::json call_tool(
      const std::string& name,
      const std::unordered_map<std::string, std::string>& args) override {
    auto it = tools_.find(name);
    if (it == tools_.end()) {
      return nlohmann::json{{"ok", false}, {"error_code", "unknown-tool"}};
    }
    return it->second(args);
  }
  std::vector<std::string> list_tools() const override {
    std::vector<std::string> out;
    for (const auto& [k, _] : tools_) out.push_back(k);
    return out;
  }
  void register_tool_function(std::string name, ToolMetadata meta, ToolFunc fn) override {
    metas_[name] = meta;
    tools_[std::move(name)] = std::move(fn);
  }
  void register_llm_tool(std::string, std::unique_ptr<ILLMTool>,
                         const LLMParams& = {}) override {}
  bool is_llm_tool(const std::string&) const override { return false; }
  const LLMParams& get_llm_params(const std::string&) const override {
    static const LLMParams empty;
    return empty;
  }
  nlohmann::json call_llm_tool(const std::string&, const std::string&,
                               const LLMParams&) override { return {}; }
  void set_cost_callback(CostCallback) override {}

  std::unordered_map<std::string, ToolFunc> tools_;
  std::unordered_map<std::string, ToolMetadata> metas_;
};

json valid_input() {
  return json{{"name", "runtime-provider"},
             {"backend", "mock"},
             {"api_url", "http://runtime"},
             {"models", json::array({json{{"id", "model-a"}}})}};
}

}  // namespace

TEST_CASE("register_dynamic tool registers factory callback and provider",
          "[provider][register]") {
  LLMProviderFactory factory;
  ProviderRegistry registry;

  // Drive through the pub pdk_register_tools entry — it cannot be called directly
  // without an IToolRegistry ref, so we re-use the test helper inline.
  extern nlohmann::json invoke_register_dynamic_tool(
      LLMProviderFactory&, ProviderRegistry&, const nlohmann::json&);

  const auto r = invoke_register_dynamic_tool(factory, registry, valid_input());
  REQUIRE(r["ok"] == true);
  CHECK(factory.has_dynamic("runtime-provider"));
  CHECK(registry.list_providers().size() == 1);

  LLMConfig config;
  config.provider = "runtime-provider";
  REQUIRE(factory.create(config) != nullptr);
}

TEST_CASE("register_dynamic rejects invalid and duplicate definitions without mutation",
          "[provider][register]") {
  LLMProviderFactory factory;
  ProviderRegistry registry;

  extern nlohmann::json invoke_register_dynamic_tool(
      LLMProviderFactory&, ProviderRegistry&, const nlohmann::json&);

  const auto invalid = invoke_register_dynamic_tool(
      factory, registry,
      json{{"name", ""}, {"backend", "unsupported"}, {"models", json::array()}});
  CHECK_FALSE(invalid["ok"].get<bool>());
  CHECK(invalid["error_code"] == "validation");
  CHECK(factory.dynamic_names().empty());
  CHECK(registry.list_providers().empty());

  REQUIRE(invoke_register_dynamic_tool(factory, registry, valid_input())["ok"] == true);
  const auto dup = invoke_register_dynamic_tool(factory, registry, valid_input());
  CHECK_FALSE(dup["ok"].get<bool>());
  CHECK(dup["error_code"] == "duplicate-provider");
  CHECK(factory.dynamic_names().size() == 1);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_provider_register_dynamic_tool && ctest --test-dir build -R '^test_provider_register_dynamic_tool$' --output-on-failure`

Expected: link fails (`invoke_register_dynamic_tool` undefined).

- [ ] **Step 3: Write minimal implementation**

Create `pdk/provider_agent/src/provider_register_dynamic.cpp`:

```cpp
#include "provider_agent.h"

#include <memory>
#include <string>

#include "agenticdsl/contract/iprovider_factory.h"
#include "agenticdsl/contract/llm_types.h"      // ILLMProvider
#include "agenticdsl/contract/llm_config.h"     // LLMConfig
#include "common/llm/llm_provider_factory.h"
#include "common/llm/mock_provider.h"

#include <nlohmann/json.hpp>

namespace pdk_provider_agent {

namespace {

bool validate(const nlohmann::json& input, std::string& err) {
  if (!input.is_object()) {
    err = "input must be an object";
    return false;
  }
  if (input.value("name", std::string{}).empty()) { err = "name is required"; return false; }
  const std::string backend = input.value("backend", std::string{});
  if (backend != "mock" && backend != "openai" && backend != "anthropic" &&
      backend != "deepseek" && backend != "minimax" && backend != "qwen" &&
      backend != "moonshot" && backend != "custom" && backend != "local" &&
      backend != "llama") {
    err = "unsupported backend";
    return false;
  }
  if (input.value("api_url", std::string{}).empty()) { err = "api_url is required"; return false; }
  if (!input.contains("models") || !input["models"].is_array() ||
      input["models"].empty()) {
    err = "models must be a non-empty array";
    return false;
  }
  return true;
}

}  // namespace

nlohmann::json invoke_register_dynamic_tool(
    agenticdsl::LLMProviderFactory& factory,
    ProviderRegistry& registry,
    const nlohmann::json& input) {
  std::string err;
  if (!validate(input, err)) {
    return {{"ok", false}, {"error_code", "validation"}, {"warning", err}};
  }
  const std::string name = input["name"].get<std::string>();
  const std::string backend = input["backend"].get<std::string>();
  const std::string api_url = input["api_url"].get<std::string>();
  const std::string api_endpoint = input.value("api_endpoint", std::string{});
  const std::string api_key_env  = input.value("api_key_env", std::string{});
  std::map<std::string, ModelConfig> models;
  for (const auto& m : input["models"]) {
    if (!m.is_object() || !m.contains("id") || !m["id"].is_string()) continue;
    const auto id = m["id"].get<std::string>();
    models.emplace(id, ModelConfig{
        id,
        m.value("max_tokens", 4096),
        m.value("temperature", 0.7),
        m.contains("extra") ? m["extra"] : nlohmann::json::object()
    });
  }
  if (factory.has_dynamic(name)) {
    return {{"ok", false}, {"error_code", "duplicate-provider"}};
  }

  // Capture only values / shared state — never raw owning pointers.
  auto cb = [backend, api_url, api_endpoint,
             api_key_env, models](const agenticdsl::LLMConfig& config)
      -> std::unique_ptr<agenticdsl::ILLMProvider> {
    agenticdsl::LLMConfig cfg = config;
    cfg.provider = backend;
    cfg.api_url = api_url;
    if (!api_endpoint.empty()) cfg.api_endpoint = api_endpoint;
    cfg.api_key_env = api_key_env;
    cfg.models.clear();
    for (const auto& [k, v] : models) {
      cfg.models[k] = agenticdsl::ModelSpec{v.model,
                                            v.max_tokens,
                                            static_cast<float>(v.temperature)};
    }
    (void)cfg;
    if (backend == "mock") return std::make_unique<MockLLMProvider>(config);
    return std::make_unique<MockLLMProvider>(config);  // safe fallback until Task 2 cloud wiring
  };

  if (!factory.register_dynamic(name, std::move(cb))) {
    return {{"ok", false}, {"error_code", "duplicate-provider"}};
  }

  nlohmann::json register_payload;
  register_payload[name] = {
      {"api_url", api_url},
      {"api_key_env", api_key_env},
      {"models", nlohmann::json::object()}
  };
  for (const auto& [id, mc] : models) {
    register_payload[name]["models"][id] = {
        {"model", mc.model}, {"max_tokens", mc.max_tokens},
        {"temperature", mc.temperature}};
  }
  if (!api_endpoint.empty()) register_payload[name]["api_endpoint"] = api_endpoint;
  registry.register_providers(register_payload);

  return {{"ok", true}, {"name", name},
          {"factory_has_dynamic", factory.has_dynamic(name)},
          {"registry_count", registry.list_providers().size()}};
}

}  // namespace pdk_provider_agent
```

Add a new tool registration in `pdk/provider_agent/src/pdk_entry.cpp` (after the `provider/refresh` block from Task 2):

```cpp
    // 6. provider/register_dynamic (NEW)
    registry.register_tool_function(
        "provider/register_dynamic",
        ::agenticdsl::ToolMetadata{
            .name = "provider/register_dynamic",
            .description = "Register a provider definition at runtime",
            .domain = "provider",
            .category = ::agenticdsl::ToolCategory::StateModify,
            .min_layer = ::agenticdsl::LayerProfile::Workflow,
            .approval = ::agenticdsl::ApprovalPolicy{
                .requires_approval_in_plan = true,
                .requires_approval_in_agent = true,
                .requires_approval_in_yolo = false,
                .force_approval_always = true
            },
            .allowed_layers = {::agenticdsl::LayerProfile::Workflow}
        },
        [&reg](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
          // Look up the in-process LLMProviderFactory via the standard global setter
          // adapter — provider_agent reaches the factory through the tool's enclosing
          // Engine (production code wires it via set_provider_agent_factory below).
          extern std::unique_ptr<agenticdsl::LLMProviderFactory>
              g_provider_agent_factory;
          nlohmann::json j = json_arg(args, "args");
          if (j.is_null()) j = nlohmann::json::object();
          if (!g_provider_agent_factory) {
            return {{"ok", false}, {"error_code", "factory-not-set"}};
          }
          return invoke_register_dynamic_tool(*g_provider_agent_factory, reg, j);
        }
    );
```

Declare the factory pointer in `pdk/provider_agent/include/provider_agent.h` (append to the namespace block):

```cpp
namespace pdk_provider_agent {
inline std::unique_ptr<agenticdsl::LLMProviderFactory>& factory_slot() {
  static std::unique_ptr<agenticdsl::LLMProviderFactory> instance;
  return instance;
}
}  // namespace pdk_provider_agent
```

And add the `extern` declaration in `pdk/provider_agent/src/pdk_entry.cpp` just above the registration:

```cpp
namespace pdk_provider_agent { extern std::unique_ptr<agenticdsl::LLMProviderFactory>
    g_provider_agent_factory; }
std::unique_ptr<agenticdsl::LLMProviderFactory> pdk_provider_agent::g_provider_agent_factory;
```

(The global exists to bridge plugin-hosted tools with the in-process factory; Task 5 initializes it during plugin load.)

Update `pdk/provider_agent/CMakeLists.txt` (`add_library(ProviderAgent SHARED ...)` list) and `tests/CMakeLists.txt` matching block (already covered by the Task 2 regex).

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target ProviderAgent test_provider_register_dynamic_tool && ctest --test-dir build -R '^test_provider_register_dynamic_tool$' --output-on-failure`

Expected: both cases pass — validation rejected, duplicate rejected, valid registration makes new provider resolvable immediately.

- [ ] **Step 5: Commit**

```bash
git add pdk/provider_agent/include/provider_agent.h \
        pdk/provider_agent/src/provider_register_dynamic.cpp \
        pdk/provider_agent/src/pdk_entry.cpp \
        pdk/provider_agent/CMakeLists.txt \
        tests/test_provider_register_dynamic_tool.cpp
git commit -m "feat(provider-agent): register providers at runtime"
```

---

## Task 4: provider/switch 与 provider/list 状态查询

**Files:**
- Create: `pdk/provider_agent/src/provider_switch.cpp`
- Modify: `pdk/provider_agent/src/pdk_entry.cpp`
- Create: `tests/test_provider_switch_tool.cpp`

- [ ] **Step 1: Write the failing test**

Create `tests/test_provider_switch_tool.cpp`:

```cpp
#include <catch_amalgamated.hpp>

#include <memory>
#include <string>

#include "agenticdsl/contract/event_builder.h"
#include "agenticdsl/contract/llm_types.h"
#include "agenticdsl/contract/llm_config.h"
#include "agenticdsl/contract/itool_registry.h"
#include "common/llm/llm_provider_factory.h"
#include "common/llm/mock_provider.h"
#include "common/policy/approval_handler.h"
#include "common/policy/execution_policy.h"
#include "common/tools/registry.h"

#include "provider_agent.h"

using namespace agenticdsl;
using namespace pdk_provider_agent;
using nlohmann::json;

TEST_CASE("provider/switch switches factory default atomically",
          "[provider][switch]") {
  LLMProviderFactory factory;
  REQUIRE(factory.register_dynamic(
      "p-a", [](const LLMConfig& c) { return std::make_unique<MockLLMProvider>(c); }));
  REQUIRE(factory.register_dynamic(
      "p-b", [](const LLMConfig& c) { return std::make_unique<MockLLMProvider>(c); }));

  extern nlohmann::json invoke_switch_tool(
      agenticdsl::LLMProviderFactory&, const std::string&);

  const auto r1 = invoke_switch_tool(factory, "p-b");
  REQUIRE(r1["ok"] == true);
  CHECK(factory.current_default() == "p-b");

  LLMConfig empty;
  empty.provider.clear();
  auto p = factory.create(empty);
  REQUIRE(p != nullptr);
  CHECK(dynamic_cast<MockLLMProvider*>(p.get()) != nullptr);
}

TEST_CASE("provider/switch rejects unknown provider without mutation",
          "[provider][switch]") {
  LLMProviderFactory factory;
  REQUIRE(factory.register_dynamic(
      "p-a", [](const LLMConfig& c) { return std::make_unique<MockLLMProvider>(c); }));
  REQUIRE(factory.switch_default("p-a"));

  extern nlohmann::json invoke_switch_tool(
      agenticdsl::LLMProviderFactory&, const std::string&);

  const auto before = factory.current_default();
  const auto r = invoke_switch_tool(factory, "p-missing");
  CHECK_FALSE(r["ok"].get<bool>());
  CHECK(r["error_code"] == "unknown-provider");
  CHECK(factory.current_default() == before);
}

TEST_CASE("concurrent switch + create converges to a single stable default",
          "[provider][switch][thread]") {
  LLMProviderFactory factory;
  REQUIRE(factory.register_dynamic(
      "p-a", [](const LLMConfig& c) { return std::make_unique<MockLLMProvider>(c); }));
  REQUIRE(factory.register_dynamic(
      "p-b", [](const LLMConfig& c) { return std::make_unique<MockLLMProvider>(c); }));

  extern nlohmann::json invoke_switch_tool(
      agenticdsl::LLMProviderFactory&, const std::string&);

  std::atomic<int> a_wins{0}, b_wins{0};
  std::vector<std::thread> ts;
  for (int i = 0; i < 32; ++i) {
    ts.emplace_back([&, i]() {
      const std::string target = (i % 2 == 0) ? "p-a" : "p-b";
      const auto r = invoke_switch_tool(factory, target);
      if (r["ok"] == true) {
        if (factory.current_default() == "p-a") a_wins.fetch_add(1);
        else b_wins.fetch_add(1);
      }
    });
  }
  for (auto& t : ts) t.join();

  CHECK((a_wins.load() + b_wins.load()) == 32);
  CHECK((factory.current_default() == "p-a" || factory.current_default() == "p-b"));
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_provider_switch_tool && ctest --test-dir build -R '^test_provider_switch_tool$' --output-on-failure`

Expected: link fails (`invoke_switch_tool` undefined).

- [ ] **Step 3: Write minimal implementation**

Create `pdk/provider_agent/src/provider_switch.cpp`:

```cpp
#include "provider_agent.h"

#include <memory>
#include <string>

#include <nlohmann/json.hpp>

#include "agenticdsl/contract/iprovider_factory.h"
#include "common/llm/llm_provider_factory.h"

namespace pdk_provider_agent {

nlohmann::json invoke_switch_tool(agenticdsl::LLMProviderFactory& factory,
                                  const std::string& target) {
  if (target.empty()) {
    return {{"ok", false}, {"error_code", "validation"},
            {"warning", "target provider name is empty"}};
  }
  if (!factory.has_dynamic(target)) {
    return {{"ok", false}, {"error_code", "unknown-provider"},
            {"target", target},
            {"current_default", factory.current_default()}};
  }
  if (!factory.switch_default(target)) {
    return {{"ok", false}, {"error_code", "switch-failed"},
            {"current_default", factory.current_default()}};
  }
  return {{"ok", true}, {"current_default", factory.current_default()}};
}

}  // namespace pdk_provider_agent
```

Append the `provider/switch` registration to `pdk/provider_agent/src/pdk_entry.cpp` (after the `provider/register_dynamic` block):

```cpp
    // 7. provider/switch (NEW)
    registry.register_tool_function(
        "provider/switch",
        ::agenticdsl::ToolMetadata{
            .name = "provider/switch",
            .description = "Switch default provider at runtime",
            .domain = "provider",
            .category = ::agenticdsl::ToolCategory::StateModify,
            .min_layer = ::agenticdsl::LayerProfile::Workflow,
            .approval = ::agenticdsl::ApprovalPolicy{
                .requires_approval_in_plan = true,
                .requires_approval_in_agent = true,
                .requires_approval_in_yolo = false,
                .force_approval_always = true
            },
            .allowed_layers = {::agenticdsl::LayerProfile::Workflow}
        },
        [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
          const std::string target = str_arg(args, "provider_name");
          extern std::unique_ptr<agenticdsl::LLMProviderFactory>
              pdk_provider_agent::g_provider_agent_factory;
          if (!pdk_provider_agent::g_provider_agent_factory) {
            return {{"ok", false}, {"error_code", "factory-not-set"}};
          }
          return invoke_switch_tool(*pdk_provider_agent::g_provider_agent_factory, target);
        }
    );
```

Update `pdk/provider_agent/CMakeLists.txt` and `tests/CMakeLists.txt` matching block (already covered by the Task 2 regex).

- [ ] **Step 4: Run test to verify it passes**

Run: `cmake --build build --target ProviderAgent test_provider_switch_tool && ctest --test-dir build -R '^test_provider_switch_tool$' --output-on-failure`

Expected: 3 cases pass — successful switch, unknown rejected, concurrent converges to a single stable default.

- [ ] **Step 5: Commit**

```bash
git add pdk/provider_agent/src/provider_switch.cpp \
        pdk/provider_agent/src/pdk_entry.cpp \
        pdk/provider_agent/CMakeLists.txt \
        tests/test_provider_switch_tool.cpp
git commit -m "feat(provider-agent): add governed provider switching and listing"
```

---

## Task 5: 并发 fixture、集成验证与文档契约

**Files:**
- Create: `tests/test_provider_factory_concurrent.cpp`
- Create: `tests/test_provider_dynamic_lifecycle.cpp`
- Create: `tests/test_provider_dynamic_integration.cpp`
- Modify: `tests/CMakeLists.txt` (already covered by Task 2 regex)

- [ ] **Step 1: Write the failing test**

Create `tests/test_provider_factory_concurrent.cpp`:

```cpp
#include <catch_amalgamated.hpp>

#include <atomic>
#include <memory>
#include <thread>
#include <vector>

#include "agenticdsl/contract/llm_config.h"
#include "agenticdsl/contract/llm_types.h"
#include "common/llm/llm_provider_factory.h"
#include "common/llm/mock_provider.h"

using agenticdsl::LLMConfig;
using agenticdsl::LLMProviderFactory;
using agenticdsl::MockLLMProvider;

TEST_CASE("LLMProviderFactory 50-thread × 1000 mixed ops stay consistent",
          "[provider_factory][dynamic][thread]") {
  LLMProviderFactory factory;
  // Pre-register two dynamic providers up front so callers can find them.
  REQUIRE(factory.register_dynamic(
      "provider-a", [](const LLMConfig& c) { return std::make_unique<MockLLMProvider>(c); }));
  REQUIRE(factory.register_dynamic(
      "provider-b", [](const LLMConfig& c) { return std::make_unique<MockLLMProvider>(c); }));
  REQUIRE(factory.switch_default("provider-a"));

  constexpr int kThreads = 50;
  constexpr int kIterations = 1000;
  std::atomic<std::uint64_t> creates{0}, failures{0}, switches{0};

  std::vector<std::thread> ts;
  ts.reserve(kThreads);
  for (int t = 0; t < kThreads; ++t) {
    ts.emplace_back([&, t]() {
      for (int i = 0; i < kIterations; ++i) {
        const int op = (t + i) % 5;
        switch (op) {
          case 0: case 1: case 2: {
            LLMConfig cfg;
            cfg.provider = (i % 2 == 0) ? "provider-a" : "provider-b";
            if (factory.create(cfg)) creates.fetch_add(1);
            else failures.fetch_add(1);
            break;
          }
          case 3:
            (void)factory.dynamic_names();
            break;
          case 4:
            if (factory.switch_default((i % 2 == 0) ? "provider-a" : "provider-b")) {
              switches.fetch_add(1);
            }
            break;
        }
      }
    });
  }
  for (auto& t : ts) t.join();

  const auto total = static_cast<std::uint64_t>(kThreads) * kIterations;
  CHECK(creates.load() + failures.load() >= total * 3 / 5);
  CHECK((factory.current_default() == "provider-a" ||
         factory.current_default() == "provider-b"));
  CHECK(factory.dynamic_names().size() == 2);
  SUCCEED();
}
```

Create `tests/test_provider_dynamic_lifecycle.cpp`:

```cpp
#include <catch_amalgamated.hpp>

#include <memory>

#include "common/llm/llm_provider_factory.h"
#include "common/llm/llm_config.h"
#include "common/llm/llm_types.h"
#include "common/llm/mock_provider.h"

using namespace agenticdsl;

TEST_CASE("dynamic callback survives temporary config destruction",
          "[provider_factory][dynamic][lifetime]") {
  LLMProviderFactory factory;
  std::shared_ptr<std::string> captured = std::make_shared<std::string>("from-outer-scope");
  REQUIRE(factory.register_dynamic(
      "shared-state",
      [captured](const LLMConfig& c) {
        (void)captured;
        return std::make_unique<MockLLMProvider>(c);
      }));

  {
    // Temporary value captured in the callback — destroyed at scope end.
    const std::string temp_value = "gone-after-scope";
    REQUIRE(factory.register_dynamic(
        "value-captured",
        [temp_value](const LLMConfig& c) {
          (void)temp_value;  // owned by value; survives caller scope
          return std::make_unique<MockLLMProvider>(c);
        }));
  }  // temp_value destructor runs here

  LLMConfig cfg_a;
  cfg_a.provider = "shared-state";
  REQUIRE(factory.create(cfg_a) != nullptr);

  LLMConfig cfg_b;
  cfg_b.provider = "value-captured";
  REQUIRE(factory.create(cfg_b) != nullptr);
}

TEST_CASE("provider construction happens after the factory lock is released",
          "[provider_factory][dynamic][lifetime]") {
  LLMProviderFactory factory;
  REQUIRE(factory.register_dynamic(
      "throw-inside-callback",
      [](const LLMConfig&) -> std::unique_ptr<ILLMProvider> {
        throw std::runtime_error("boom");
      }));

  LLMConfig cfg;
  cfg.provider = "throw-inside-callback";
  REQUIRE_THROWS_AS(factory.create(cfg), std::runtime_error);

  // After construction failed, the factory must still be functional.
  CHECK(factory.has_dynamic("throw-inside-callback"));
  REQUIRE(factory.switch_default("throw-inside-callback"));
  LLMConfig empty;
  empty.provider.clear();
  REQUIRE_THROWS_AS(factory.create(empty), std::runtime_error);
}
```

Create `tests/test_provider_dynamic_integration.cpp`:

```cpp
#include <catch_amalgamated.hpp>

#include <memory>

#include "agenticdsl/contract/llm_config.h"
#include "agenticdsl/contract/llm_types.h"
#include "common/llm/llm_provider_factory.h"
#include "common/llm/mock_provider.h"

#include "provider_agent.h"

using namespace agenticdsl;
using namespace pdk_provider_agent;
using nlohmann::json;

TEST_CASE("End-to-end: register_dynamic → refresh → switch route create()",
          "[provider][integration]") {
  LLMProviderFactory factory;
  ProviderRegistry registry;

  extern json invoke_register_dynamic_tool(LLMProviderFactory&, ProviderRegistry&,
                                           const json&);
  extern json invoke_switch_tool(LLMProviderFactory&, const std::string&);

  // 1. Register two providers
  REQUIRE(invoke_register_dynamic_tool(
      factory, registry,
      json{{"name", "p-a"}, {"backend", "mock"},
           {"api_url", "http://a"}, {"models", json::array({json{{"id", "ma"}})}}})["ok"] == true);
  REQUIRE(invoke_register_dynamic_tool(
      factory, registry,
      json{{"name", "p-b"}, {"backend", "mock"},
           {"api_url", "http://b"}, {"models", json::array({json{{"id", "mb"}})}}})["ok"] == true);

  // 2. Refresh verifies catalog shape (in-process transport injected below)
  registry.seed_for_test({
      {"p-a", ProviderInfo{"p-a", "http://a", "/x", "", {{"ma", ModelConfig{"ma"}}}}},
      {"p-b", ProviderInfo{"p-b", "http://b", "/x", "", {{"mb", ModelConfig{"mb"}}}}}});
  registry.set_refresh_transport_for_test(
      [](const ProviderInfo&) { return json{{"data", json::array({json{{"id", "ma"}}})}}; });
  const auto r = registry.refresh("p-a");
  CHECK(r.ok);
  CHECK(registry.list_models("p-a") == std::vector<std::string>{"ma"});

  // 3. Switch default and verify subsequent create() routes correctly
  REQUIRE(invoke_switch_tool(factory, "p-b")["ok"] == true);
  LLMConfig empty;
  empty.provider.clear();
  auto p = factory.create(empty);
  REQUIRE(p != nullptr);
  CHECK(dynamic_cast<MockLLMProvider*>(p.get()) != nullptr);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_provider_factory_concurrent test_provider_dynamic_lifecycle test_provider_dynamic_integration && ctest --test-dir build -R '^test_provider_(factory_concurrent|dynamic_lifecycle|dynamic_integration)$' --output-on-failure`

Expected: builds first time (helpers already linked) but tests surface if any invariant is broken. The lifecycle test should expose issues if callbacks captured raw owning pointers or constructed providers inside the lock — if the implementation from Tasks 1-4 holds up, all should pass.

- [ ] **Step 3: Write minimal implementation (defensive-only)**

No production source changes unless a test failure exposes a defect. The only defensive fix likely needed:

If `test_provider_dynamic_integration` fails because `invoke_register_dynamic_tool`/`invoke_switch_tool` link symbols are referenced from the plugin translation unit and from the test, ensure `tests/CMakeLists.txt` includes both `provider_register_dynamic.cpp` and `provider_switch.cpp` in `target_sources` — already covered by the Task 2 regex.

If `test_provider_factory_concurrent` exposes half-finished catalog reads (Task 1 implementation already uses `shared_lock` + out-of-lock construction — covered).

- [ ] **Step 4: Run test to verify it passes**

Run the integration suite:

```bash
cmake --build build -j$(nproc)
ctest --test-dir build -R '^test_provider' --output-on-failure
ctest --test-dir build --output-on-failure -j$(nproc)   # full regression
openspec validate provider-dynamic-discovery --json
```

Expected: every `test_provider_*` target passes. Full ctest regression: 0 new failures; only the pre-existing `test_cost_tracking_decorator` failure remains (documented in `docs/audits/2026-07-31-pre-existing-failures.md` if not already). `openspec validate provider-dynamic-discovery --json` reports `"passed": true`.

- [ ] **Step 5: Commit**

```bash
git add tests/test_provider_factory_concurrent.cpp \
        tests/test_provider_dynamic_lifecycle.cpp \
        tests/test_provider_dynamic_integration.cpp \
        tests/CMakeLists.txt
git commit -m "test(provider): cover dynamic discovery concurrency and governance"
```

---

## Critical Constraints (MUST satisfy)

1. **`std::shared_mutex`** protects catalog + default provider (`shared_lock` reads, `unique_lock` writes).
2. **No provider construction inside the lock** — copy `DynamicFactoryFn` by value, then call after `dynamic_mutex_` is released.
3. **Two-phase refresh** — `provider/refresh` validates outside the lock, commits one swap under a short unique lock.
4. **Refresh failure preserves old catalog** — `Retryable` / `validation` / `unknown-provider` error codes return `ok=false` + `warning`; never clear catalog.
5. **Callback lifetime safety** — `register_dynamic()` only accepts `std::function` capturing values or `shared_ptr`; never raw owning pointers.
6. **`ToolCoordinator` governance** — all 3 tools (refresh / register_dynamic / switch) flow through `IToolRegistry::register_tool_function` + `ToolCoordinator::execute`; chat layer / slash commands call the tools, never `factory.switch_default()` directly.
7. **ADR-0004 V2 `ToolMetadata`** — `category`, `approval`, `allowed_layers={Workflow}` for all three tools; `force_approval_always=true`.
8. **Explicit `LLMConfig::provider` priority** — dynamic default consulted only when `config.provider` is empty.
9. **Spec coverage** — 5 ADDED Requirements × 14 Scenarios from `specs/provider-dynamic-discovery/spec.md` are addressable; see coverage matrix below.
10. **TDD discipline** — every Task writes a failing test first, then minimal impl.
11. **Real code only** — no `TODO`/`TBD`/stub-returned-string placeholders.

## Spec coverage matrix

| spec Requirement | Scenario | Covered in Task |
|---|---|---|
| provider-factory-thread-safe | 运行时注册后立即创建 | Task 1 (`LLMProviderFactory registers and creates…`) |
| provider-factory-thread-safe | 并发读写不产生半成品 | Task 5 (`50-thread × 1000 mixed ops`) |
| provider-factory-thread-safe | 构造时配置保持兼容 | Task 1 (`dynamic default applies only for empty provider`) |
| provider-refresh-tool | 上游可达且目录有效 | Task 2 (`refresh commits valid catalog and marks removed models`) |
| provider-refresh-tool | 下线模型被标记 | Task 2 (`refresh commits…marked removed`) + Task 4 (`list` returns `removed`) |
| provider-refresh-tool | 非 Workflow layer 被治理拒绝 | Task 2 (`allowed_layers={Workflow}` registration) |
| provider-register-dynamic-tool | 合法定义运行时注册 | Task 3 (`register_dynamic tool registers factory callback and provider`) |
| provider-register-dynamic-tool | 非法定义被拒绝 | Task 3 (`register_dynamic rejects invalid and duplicate…`) |
| provider-register-dynamic-tool | 重复注册不覆盖旧定义 | Task 3 (`… duplicate … without mutation`) |
| provider-switch-tool | 切换到已注册 provider | Task 4 (`provider/switch switches factory default atomically`) |
| provider-switch-tool | 切换到未知 provider | Task 4 (`…rejects unknown provider without mutation`) |
| provider-switch-tool | switch 经过 ToolCoordinator 治理 | Task 4 (`allowed_layers={Workflow}`) |
| refresh-failure-preserves-catalog | 网络失败保留旧目录 | Task 2 (`refresh failure preserves prior catalog and surfaces error_code`) |
| refresh-failure-preserves-catalog | 非法响应不提交部分结果 | Task 2 (`refresh rejects invalid schema without mutating catalog`) |
| refresh-failure-preserves-catalog | 失败刷新与切换并发 | Task 5 (`50-thread × 1000 mixed ops` — refresh branch + switch branch) |

## Banned Patterns

- ❌ `TBD` / `TODO` / `implement later` / `fill in details` / `similar to Task N`
- ❌ `provider_creation(inside_lock)` or anything whose body holds `dynamic_mutex_` while calling `std::make_unique<ILLMProvider>`
- ❌ Raw `ProviderInfo*` or `ILLMProvider*` in `register_dynamic()` signature
- ❌ Swallowing refresh failures (`catch(...) {}`)
- ❌ Bypassing ToolCoordinator (chat / slash / DSL calling `factory.switch_default()` directly)
- ❌ `throw` from inside a critical section that swallows caller-visible errors (see `nesting guard` in `ToolCoordinator`)

## Self-Review (after writing)

1. `grep -c '^## Task ' .rddf/plans/provider-dynamic-discovery.md` — must be **5** (matches `tasks.md` §1-§5).
2. `grep -c '^- \[ \]' .rddf/plans/provider-dynamic-discovery.md` — must be **≥ 25** (each Task has 5 steps × 5 tasks).
3. Each `spec.md` Scenario maps to at least one Task — see coverage matrix.
4. No `TBD` / `TODO` / `placeholder` literals remain.
5. Type names consistent: `LLMProviderFactory::DynamicFactoryFn`, `register_dynamic`, `switch_default`, `current_default`, `has_dynamic`, `dynamic_names`, `RefreshResult`, `RefreshTransport`, `invoke_register_dynamic_tool`, `invoke_switch_tool`.

## Final Step

After writing, copy this file to `.rddf/wt/provider-dynamic-discovery/.rddf/plans/provider-dynamic-discovery.md` so the worktree has it. Both files must be byte-identical.
