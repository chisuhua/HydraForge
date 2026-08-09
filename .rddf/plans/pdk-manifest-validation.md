# pdk-manifest-validation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement `pdk_manifest.json` file format + `ManifestValidator` + `ManifestFinder` per ADR-0052, integrate manifest-first load into `PluginLoader::load_so()` (with `IInteractionBus` opt-in injection per ADR-0031 §决策 5). Add 24+ unit tests + 5+ integration tests, zero regression on existing 140/143 ctest baseline.

**Architecture:**
- New module: `src/modules/pdk/` with 3 classes (`Manifest`, `ManifestValidator`, `ManifestFinder`)
- Public headers: `include/agenticdsl/pdk/manifest.h` + `manifest_validator.h` + `manifest_finder.h`
- Modify: `src/modules/plugin/plugin_loader.{h,cpp}` — add `set_interaction_bus()` + `clear_interaction_bus()` + `require_manifest` param to `load_so()`
- Backward compatible: 缺 manifest 仅 warn, 不阻塞现有 12 个 PDK plugin

**Tech Stack:** C++20 (std::filesystem, std::optional, std::variant), Catch2 v3, nlohmann::json (vendored, `external/nlohmann_json/`), existing PDK conventions.

---

## Scope Adjustments vs proposal

**Adopted scope** (per Metis review 2026-08-09):
- 9 必填字段 (ADR-0052 §决策 2 完整)
- 8 推荐字段 (ADR-0052 §决策 3 完整)
- 双 ABI 支持 (镜像 `SUPPORTED_ABI_VERSIONS={1,2}`)
- 严格类型检查 (避免 nlohmann 隐式转换)
- IInteractionBus setter 注入 (per ADR-0031 §决策 5)
- 向后兼容: 缺 manifest 仅 warn, 不阻塞

**Deferred to follow-up changes**:
- ❌ 12 个现有 PDK plugin 迁移 manifest — 后续每个 plugin 一个 change
- ❌ `pdk_create_llm_provider` 符号交叉验证 (per ADR-0041)
- ❌ Wasm manifest 嵌入 (per ADR-0052 §决策 1)
- ❌ Trust 签名验证 (per ADR-0052 §决策 7)
- ❌ Manifest 路径 cache (per design Risk 2)

---

## File Structure

### Production Code (5 new + 2 modified)

| File | Responsibility |
|---|---|
| `include/agenticdsl/pdk/manifest.h` | Manifest/ToolSpec/Resources/ValidationError/ManifestValidationResult POD types |
| `include/agenticdsl/pdk/manifest_validator.h` | ManifestValidator class declaration (static validate) |
| `include/agenticdsl/pdk/manifest_finder.h` | ManifestFinder class declaration (static find) |
| `src/modules/pdk/manifest.cpp` | Pure data structures (zero nlohmann) |
| `src/modules/pdk/manifest_validator.cpp` | JSON parse + validate logic |
| `src/modules/pdk/manifest_finder.cpp` | Filesystem walk + symlink + permission handling |
| `src/modules/pdk/CMakeLists.txt` | New module CMake |
| `include/agenticdsl/plugin/plugin_loader.h` | Add `set_interaction_bus` + `clear_interaction_bus` + `require_manifest` param |
| `src/modules/plugin/plugin_loader.cpp` | Add manifest-first load flow in `load_so` + `load_all` |

### Test Code (3 new)

| File | Coverage |
|---|---|
| `tests/test_pdk_manifest_validator.cpp` | 11 cases: valid, missing field, semver, abi, type mismatch, null, impl_form, entry_tool, tools, approval_policy |
| `tests/test_pdk_manifest_finder.cpp` | 8 cases: same dir, parent, not found, closest, symlink, max depth, permission, hidden |
| `tests/test_plugin_loader_manifest.cpp` | 8 cases: load + events + backward compat (includes emit + no-bus silent) |

### CMake Integration

- `CMakeLists.txt` (root): add `add_subdirectory(src/modules/pdk)`
- `src/modules/pdk/CMakeLists.txt`: link nlohmann_json, generate `agenticdsl_modules_pdk` static lib
- `tests/CMakeLists.txt`: `file(GLOB test_*.cpp)` already auto-discovers

---

## 1. Setup & Types

### Task 1.1: Write failing test for Manifest POD types (header compile test)

**Files:**
- Create: `include/agenticdsl/pdk/manifest.h` (placeholder, will fail to compile)
- Create: `tests/test_pdk_manifest_types.cpp`

**Step 1 — Write failing test (RED):**

```cpp
// tests/test_pdk_manifest_types.cpp
#include <catch2/catch_test_macros.hpp>
#include "agenticdsl/pdk/manifest.h"

TEST_CASE("Manifest POD types are constructible", "[pdk][types]") {
  agenticdsl::pdk::Manifest m;
  m.id = "test.plugin";
  m.abi_version = 2;
  REQUIRE(m.id == "test.plugin");
  REQUIRE(m.abi_version == 2);
}

TEST_CASE("ValidationError carries field/reason/value/expected", "[pdk][types]") {
  agenticdsl::pdk::ValidationError err;
  err.field = "abi_version";
  err.reason = "mismatch";
  err.value = "3";
  err.expected = "1|2";
  REQUIRE(err.field == "abi_version");
  REQUIRE(err.reason == "mismatch");
}
```

**Step 2 — Verify test fails (RED):**
```bash
cmake --build build --target test_pdk_manifest_types 2>&1 | head -20
# Expected: "fatal error: 'agenticdsl/pdk/manifest.h' file not found"
```

**Step 3 — Implement minimum to compile:**

```cpp
// include/agenticdsl/pdk/manifest.h
#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <optional>
#include <filesystem>

namespace agenticdsl::pdk {

struct ToolSpec {
  std::string name;
  std::string description;
  std::string input_schema;  // JSON Schema 2020-12 as string
  std::string output_schema;
  std::string approval_policy;  // "always"|"plan"|"agent"|"yolo"
};

struct Resources {
  uint32_t timeout_ms = 30000;
  uint32_t max_concurrent = 1;
};

struct Manifest {
  std::string id;
  std::string name;
  std::string version;
  uint32_t abi_version = 0;
  std::string min_host_version;
  std::string max_host_version;
  std::vector<std::string> implementation_forms;
  std::string entry_tool;
  std::vector<std::string> provided_tools;
  std::vector<std::string> interface_versions;
  std::vector<std::string> capabilities;
  std::string input_schema;
  std::string output_schema;
  bool requires_isolation = false;
  Resources resources;
  std::string publisher;
  std::string trust_level = "untrusted";
  std::vector<std::string> activation_events;
  std::optional<std::string> signature;
};

struct ValidationError {
  std::string field;
  std::string reason;
  std::string value;
  std::string expected;
};

struct ManifestValidationResult {
  bool valid = false;
  std::optional<Manifest> manifest;
  std::vector<ValidationError> errors;
};

}  // namespace agenticdsl::pdk
```

**Step 4 — Verify test passes (GREEN):**
```bash
cmake --build build --target test_pdk_manifest_types && ctest -R test_pdk_manifest_types
# Expected: 2 assertions PASS
```

**Step 5 — Commit:**
```bash
GIT_MASTER=1 git add include/agenticdsl/pdk/manifest.h tests/test_pdk_manifest_types.cpp
GIT_MASTER=1 git commit -m "feat(pdk): add Manifest POD types (Phase 6a step 1)" \
  -m "9 必填 + 8 推荐 + 1 可选字段, 零 nlohmann 依赖. 2 unit tests PASS." \
  -m "Ultraworked with [Sisyphus](https://github.com/code-yeongyu/oh-my-openagent)" \
  -m "Co-authored-by: Sisyphus <clio-agent@sisyphuslabs.ai>"
```

---

### Task 1.2: Add ManifestValidator + ManifestFinder empty headers (compile stub)

**Files:**
- Create: `include/agenticdsl/pdk/manifest_validator.h`
- Create: `include/agenticdsl/pdk/manifest_finder.h`

**Step 1 — Create empty class declarations:**

```cpp
// include/agenticdsl/pdk/manifest_validator.h
#pragma once
#include "agenticdsl/pdk/manifest.h"
#include <string>

namespace agenticdsl::pdk {

class ManifestValidator {
 public:
  static ManifestValidationResult validate(const std::string& json_content);
};

}  // namespace agenticdsl::pdk
```

```cpp
// include/agenticdsl/pdk/manifest_finder.h
#pragma once
#include <filesystem>
#include <optional>

namespace agenticdsl::pdk {

class ManifestFinder {
 public:
  static std::optional<std::filesystem::path> find(const std::filesystem::path& so_path);
};

}  // namespace agenticdsl::pdk
```

**Step 2 — Verify compilation (no tests yet):**
```bash
cmake --build build 2>&1 | tail -10
# Expected: linker errors for undefined symbols (no .cpp yet) — that's OK, just need headers to parse
```

**Step 3 — Commit (header-only):**
```bash
GIT_MASTER=1 git add include/agenticdsl/pdk/manifest_validator.h include/agenticdsl/pdk/manifest_finder.h
GIT_MASTER=1 git commit -m "feat(pdk): add ManifestValidator + ManifestFinder empty headers" \
  -m "Stub declarations, no .cpp implementation yet. Following TDD: tests in next task." \
  -m "Ultraworked with [Sisyphus](https://github.com/code-yeongyu/oh-my-openagent)" \
  -m "Co-authored-by: Sisyphus <clio-agent@sisyphuslabs.ai>"
```

---

### Task 1.3: Add src/modules/pdk/CMakeLists.txt

**Files:**
- Create: `src/modules/pdk/CMakeLists.txt`
- Modify: `CMakeLists.txt` (root) — add `add_subdirectory(src/modules/pdk)`

**Step 1 — Create CMakeLists.txt:**

```cmake
# src/modules/pdk/CMakeLists.txt
# PDK Manifest 模块 - Phase 6a step 1
add_library(agenticdsl_modules_pdk STATIC
  manifest.cpp
  manifest_validator.cpp
  manifest_finder.cpp
)

target_include_directories(agenticdsl_modules_pdk
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}
)

# nlohmann::json is at external/nlohmann_json
find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp
  PATHS ${CMAKE_SOURCE_DIR}/external/nlohmann_json/include
  NO_DEFAULT_PATH
)
if(NOT NLOHMANN_JSON_INCLUDE_DIR)
  message(FATAL_ERROR "nlohmann/json.hpp not found at external/nlohmann_json/include")
endif()
target_include_directories(agenticdsl_modules_pdk PUBLIC ${NLOHMANN_JSON_INCLUDE_DIR})

# C++20 + warnings per AGENTS.md
target_compile_features(agenticdsl_modules_pdk PUBLIC cxx_std_20)
target_compile_options(agenticdsl_modules_pdk PRIVATE
  -Wall -Wextra -Wpedantic
)
```

**Step 2 — Modify root CMakeLists.txt** (find the right location):

```bash
# Find existing add_subdirectory calls in root CMakeLists.txt
grep -n "add_subdirectory" CMakeLists.txt | head -20
```

Then add `add_subdirectory(src/modules/pdk)` after other modules.

**Step 3 — Verify CMake configure:**
```bash
cmake -B build 2>&1 | tail -10
# Expected: configure succeeds, agenticdsl_modules_pdk target created
```

**Step 4 — Commit:**
```bash
GIT_MASTER=1 git add src/modules/pdk/CMakeLists.txt CMakeLists.txt
GIT_MASTER=1 git commit -m "build(pdk): add src/modules/pdk/ module + CMake target" \
  -m "agenticdsl_modules_pdk static lib, links nlohmann_json, cxx_std_20, -Wall -Wextra -Wpedantic" \
  -m "Ultraworked with [Sisyphus](https://github.com/code-yeongyu/oh-my-openagent)" \
  -m "Co-authored-by: Sisyphus <clio-agent@sisyphuslabs.ai>"
```

---

### Task 1.4: Add stub .cpp files to satisfy linker

**Files:**
- Create: `src/modules/pdk/manifest.cpp` (empty, just to satisfy CMake)
- Create: `src/modules/pdk/manifest_validator.cpp` (stub returning invalid result)
- Create: `src/modules/pdk/manifest_finder.cpp` (stub returning nullopt)

**Step 1 — Create stubs:**

```cpp
// src/modules/pdk/manifest.cpp
// Pure data types are header-only, no .cpp needed
// This file exists to satisfy CMake target_sources
```

```cpp
// src/modules/pdk/manifest_validator.cpp
#include "agenticdsl/pdk/manifest_validator.h"
namespace agenticdsl::pdk {
ManifestValidationResult ManifestValidator::validate(const std::string&) {
  return ManifestValidationResult{};  // Stub: real impl in Task 2.x
}
}  // namespace agenticdsl::pdk
```

```cpp
// src/modules/pdk/manifest_finder.cpp
#include "agenticdsl/pdk/manifest_finder.h"
namespace agenticdsl::pdk {
std::optional<std::filesystem::path> ManifestFinder::find(const std::filesystem::path&) {
  return std::nullopt;  // Stub: real impl in Task 3.x
}
}  // namespace agenticdsl::pdk
```

**Step 2 — Verify build:**
```bash
cmake --build build 2>&1 | tail -10
# Expected: clean build, agenticdsl_modules_pdk links
```

**Step 3 — Commit:**
```bash
GIT_MASTER=1 git add src/modules/pdk/
GIT_MASTER=1 git commit -m "feat(pdk): add stub .cpp files for module target" \
  -m "Empty implementations, real logic in TDD tasks 2-4. Build clean." \
  -m "Ultraworked with [Sisyphus](https://github.com/code-yeongyu/oh-my-openagent)" \
  -m "Co-authored-by: Sisyphus <clio-agent@sisyphuslabs.ai>"
```

---

## 2. TDD: ManifestValidator (11 test cases)

> **Test pattern**: For each case, write failing test first, verify it fails for the right reason, implement, verify it passes, commit.

### Task 2.1: valid_minimal_manifest

**Test file**: `tests/test_pdk_manifest_validator.cpp`

**Step 1 — Write failing test:**
```cpp
TEST_CASE("ManifestValidator: valid_minimal_manifest", "[pdk][validator]") {
  std::string json = R"({
    "id": "code.review",
    "name": "Code Review Agent",
    "version": "0.1.0",
    "abi_version": 2,
    "min_host_version": "2.0.0",
    "max_host_version": "3.0.0",
    "implementation_forms": ["cpp"],
    "entry_tool": "review/run",
    "provided_tools": ["review/run", "review/suggest"]
  })";
  auto result = agenticdsl::pdk::ManifestValidator::validate(json);
  REQUIRE(result.valid == true);
  REQUIRE(result.errors.empty());
  REQUIRE(result.manifest.has_value());
  REQUIRE(result.manifest->id == "code.review");
  REQUIRE(result.manifest->abi_version == 2);
}
```

**Step 2 — Verify test fails (RED):**
```bash
cmake --build build --target test_pdk_manifest_validator 2>&1 | tail -10
ctest -R test_pdk_manifest_validator 2>&1 | tail -10
# Expected: stub returns valid=false → REQUIRE fails
```

**Step 3 — Implement minimum (GREEN):**
```cpp
// src/modules/pdk/manifest_validator.cpp
#include "agenticdsl/pdk/manifest_validator.h"
#include <nlohmann/json.hpp>
#include <regex>

namespace agenticdsl::pdk {

namespace {

bool is_valid_semver(const std::string& s) {
  static const std::regex semver_re(R"(^\d+\.\d+\.\d+(-[a-zA-Z0-9.]+)?(\+[a-zA-Z0-9.]+)?$)");
  return std::regex_match(s, semver_re);
}

constexpr uint32_t CURRENT_ABI_VERSION = 2;
constexpr std::array<uint32_t, 2> SUPPORTED_ABI_VERSIONS = {1, CURRENT_ABI_VERSION};

bool is_supported_abi(uint32_t v) {
  for (auto s : SUPPORTED_ABI_VERSIONS) {
    if (v == s) return true;
  }
  return false;
}

}  // namespace

ManifestValidationResult ManifestValidator::validate(const std::string& json_content) {
  ManifestValidationResult result;
  
  nlohmann::json j;
  try {
    j = nlohmann::json::parse(json_content);
  } catch (const std::exception&) {
    result.errors.push_back({"json", "parse_error", "", ""});
    return result;
  }
  
  // Helper for strict type check + extraction
  auto require_string = [&](const char* field) -> std::optional<std::string> {
    if (!j.contains(field)) {
      result.errors.push_back({field, "required", "", ""});
      return std::nullopt;
    }
    if (!j[field].is_string()) {
      result.errors.push_back({field, "wrong_type", j[field].dump(), "string"});
      return std::nullopt;
    }
    return j[field].get<std::string>();
  };
  
  // ... similar for uint32, bool, array, etc.
  // (Full implementation in subsequent tasks)
  
  // For now, just check abi_version to pass this test
  if (j.contains("abi_version") && j["abi_version"].is_number_unsigned()) {
    uint32_t abi = j["abi_version"].get<uint32_t>();
    if (!is_supported_abi(abi)) {
      result.errors.push_back({"abi_version", "mismatch", std::to_string(abi), "1|2"});
      return result;
    }
    Manifest m;
    m.abi_version = abi;
    m.id = j.value("id", "");
    m.name = j.value("name", "");
    m.version = j.value("version", "");
    result.manifest = m;
    result.valid = true;
  } else {
    result.errors.push_back({"abi_version", "required", "", "uint32"});
  }
  
  return result;
}

}  // namespace agenticdsl::pdk
```

**Step 4 — Verify test passes (GREEN):**
```bash
cmake --build build --target test_pdk_manifest_validator && ctest -R test_pdk_manifest_validator
# Expected: 1 case PASS
```

**Step 5 — Commit:**
```bash
GIT_MASTER=1 git add tests/test_pdk_manifest_validator.cpp src/modules/pdk/manifest_validator.cpp
GIT_MASTER=1 git commit -m "feat(pdk): ManifestValidator validate() minimal + valid_minimal_manifest test" \
  -m "TDD: 1 test case PASS. abi_version hard check + minimal id/name/version extraction." \
  -m "Ultraworked with [Sisyphus](https://github.com/code-yeongyu/oh-my-openagent)" \
  -m "Co-authored-by: Sisyphus <clio-agent@sisyphuslabs.ai>"
```

---

### Task 2.2-2.11: Other 10 test cases

**Pattern** (each case follows the same TDD flow):
1. Write failing test for the specific scenario
2. Verify RED (test fails for right reason)
3. Implement minimum to pass
4. Verify GREEN
5. Commit

**Cases** (each is one task):
- 2.2: `missing_required_field_id`
- 2.3: `invalid_semver_version`
- 2.4: `abi_version_out_of_range` (abi_version=3 → reject)
- 2.5: `wrong_type_string_for_uint32` (abi_version: "1" string → reject)
- 2.6: `wrong_type_null_for_string` (name: null → reject)
- 2.7: `invalid_implementation_forms_value` (impl_form=["python"] → reject)
- 2.8: `entry_tool_not_in_provided_tools` (cross-field)
- 2.9: `tools_missing_input_schema`
- 2.10: `invalid_approval_policy_value`
- 2.11: 完整 version check + min/max_host_version + entry_tool existence

**Each task:**
- Add TEST_CASE to `tests/test_pdk_manifest_validator.cpp`
- Run ctest, verify fail
- Extend `manifest_validator.cpp` to handle the case
- Run ctest, verify pass
- Single commit per case

**Final verification:**
```bash
ctest -R test_pdk_manifest_validator --output-on-failure
# Expected: 11+ cases PASS
```

---

## 3. TDD: ManifestFinder (8 test cases)

### Task 3.1-3.8: 8 cases

**Pattern**: Same TDD flow as Task 2.

**Cases**:
- 3.1: `find_manifest_same_dir` — same directory
- 3.2: `find_manifest_parent_dir` — 1 level up
- 3.3: `find_manifest_not_found` — return nullopt
- 3.4: `find_manifest_closest_wins` — shallow priority
- 3.5: `find_manifest_symlink_resolved` — weakly_canonical
- 3.6: `find_manifest_max_depth_16` — bound check
- 3.7: `find_manifest_permission_denied_skip` — graceful degradation
- 3.8: `find_manifest_hidden_dirs` — don't skip hidden

**Test file**: `tests/test_pdk_manifest_finder.cpp`
**Implementation**: `src/modules/pdk/manifest_finder.cpp`

**Each task**:
- Write test with temp directory setup (`std::filesystem::temp_directory_path()`)
- Run ctest, verify fail
- Implement filesystem walk in `find()`
- Run ctest, verify pass
- Single commit

**Final verification:**
```bash
ctest -R test_pdk_manifest_finder --output-on-failure
# Expected: 8+ cases PASS
```

---

## 4. PluginLoader Integration (5 test cases)

### Task 4.1: Add setter + require_manifest param (header change)

**Files:**
- Modify: `include/agenticdsl/plugin/plugin_loader.h`

**Step 1 — Add declarations:**

```cpp
// Add to PluginLoader class (after existing 4 public methods, before private)
void set_interaction_bus(IInteractionBus* bus);
void clear_interaction_bus();

// Modify existing load_so signature (default param = backward compat)
bool load_so(const std::string& path, IToolRegistry& registry, 
             bool strict_version = true, bool require_manifest = false);
```

**Step 2 — Forward-declare IInteractionBus** (per Sprint 18-19 PIMPL pattern):
```cpp
// At top of file, after existing forward declarations
namespace agenticdsl::contract { class IInteractionBus; }
using IInteractionBus = agenticdsl::contract::IInteractionBus;
```

**Step 3 — Verify existing callers compile (4 public methods unchanged):**
```bash
cmake --build build 2>&1 | tail -10
# Expected: existing callers still compile (default params)
```

**Step 4 — Commit (header-only):**
```bash
GIT_MASTER=1 git add include/agenticdsl/plugin/plugin_loader.h
GIT_MASTER=1 git commit -m "refactor(plugin-loader): add IInteractionBus setter + require_manifest param" \
  -m "PIMPL pattern per Sprint 18-19. set_interaction_bus/clear_interaction_bus opt-in (ADR-0031 §决策 5). require_manifest default false (backward compat)." \
  -m "Ultraworked with [Sisyphus](https://github.com/code-yeongyu/oh-my-openagent)" \
  -m "Co-authored-by: Sisyphus <clio-agent@sisyphuslabs.ai>"
```

---

### Task 4.2: Implement manifest-first load flow in load_so

**Files:**
- Modify: `src/modules/plugin/plugin_loader.cpp`

**Step 1 — Write failing integration test (RED):**

```cpp
// tests/test_plugin_loader_manifest.cpp
#include <catch2/catch_test_macros.hpp>
#include "agenticdsl/plugin/plugin_loader.h"
#include <filesystem>

TEST_CASE("PluginLoader: load with valid manifest", "[plugin][manifest]") {
  // Setup: create a mock .so + manifest in temp dir
  auto tmp = std::filesystem::temp_directory_path() / "test_pdk_manifest_valid";
  std::filesystem::create_directories(tmp);
  std::ofstream(tmp / "pdk_manifest.json") << R"({
    "id": "test.plugin", "name": "Test", "version": "0.1.0",
    "abi_version": 2, "min_host_version": "2.0.0", "max_host_version": "3.0.0",
    "implementation_forms": ["cpp"], "entry_tool": "test/run",
    "provided_tools": ["test/run"]
  })";
  // Note: no actual .so, expect manifest validation to pass but dlopen to fail
  // OR mock a real .so if test infrastructure allows
  
  agenticdsl::PluginLoader loader;
  // ... assert manifest found + validated
}
```

**Step 2 — Verify test fails (RED):** test infrastructure depends on test fixture availability

**Step 3 — Implement flow in `load_so()`:**

```cpp
// In PluginLoader::load_so(), before existing dlopen code:
auto manifest_path = ManifestFinder::find(so_path);
if (manifest_path) {
  std::ifstream f(*manifest_path);
  std::stringstream ss; ss << f.rdbuf();
  auto validation = ManifestValidator::validate(ss.str());
  if (!validation.valid) {
    emit_event("plugin.manifest.invalid", *manifest_path, validation.errors);
    return false;  // Reject before dlopen
  }
  manifest_ = validation.manifest;  // Cache for cross-validation
} else if (require_manifest) {
  emit_event("plugin.manifest.missing", so_path, {});
  return false;
} else {
  emit_event("plugin.manifest.missing", so_path, {fallback_loaded=true});
  // Continue legacy path
}

// After dlopen + dlsym, cross-validate:
if (manifest_ && manifest_->abi_version != info.abi_version) {
  LOG_WARN("abi_version mismatch: manifest={} plugin={}", manifest_->abi_version, info.abi_version);
  // PluginInfo wins per ADR-0052 §决策 4
}
```

**Step 4 — Verify test passes (GREEN)**

**Step 5 — Commit:**
```bash
GIT_MASTER=1 git add src/modules/plugin/plugin_loader.cpp tests/test_plugin_loader_manifest.cpp
GIT_MASTER=1 git commit -m "feat(plugin-loader): manifest-first load flow + cross-validation" \
  -m "ADR-0052 §决策 4 落地. emit plugin.manifest.invalid / missing events. PluginInfo 优先." \
  -m "Ultraworked with [Sisyphus](https://github.com/code-yeongyu/oh-my-openagent)" \
  -m "Co-authored-by: Sisyphus <clio-agent@sisyphuslabs.ai>"
```

---

### Task 4.3-4.5: Other integration tests

**Cases**:
- 4.3: `load_with_invalid_manifest_rejected` (manifest 校验失败 → 返回 false, 不调 dlopen)
- 4.4: `load_without_manifest_warn_continue` (缺 manifest + require_manifest=false → warn, 继续 dlopen)
- 4.5: `load_with_require_manifest_true_missing` (require_manifest=true + 缺 manifest → 拒绝)

Each follows TDD pattern.

---

### Task 4.6-4.8: load_all + edge cases

**Cases**:
- 4.6: `load_all_with_mixed_manifests`
- 4.7: `strict_version_x_require_manifest_AND_semantics`
- 4.8: PluginInfo 优先 cross-validation

---

## 5. EventBus Integration (3 test cases)

### Task 5.1-5.3: Event emission tests

**Cases**:
- 5.1: `emits_invalid_manifest_event` (use InMemoryBus mock)
- 5.2: `emits_missing_manifest_event`
- 5.3: `no_bus_no_emit_silent`

Each test uses InMemoryBus (from existing test infrastructure) to verify event payload.

---

## 6. Backward Compatibility Verification

### Task 6.1: Full ctest regression

```bash
ctest --output-on-failure 2>&1 | tail -30
# Expected: 143-165 total (was 143, +24 new tests) → 167 PASS, 3 pre-existing FAIL
```

### Task 6.2: Verify 12 existing PDK plugins still load

```bash
# Build all existing PDK plugins
cmake --build build --target all
# Run examples/phase1_plugin_demo --load-plugin
./build/examples/phase1_plugin_demo/phase1_plugin_demo --load-plugin=./pdk/llama_engine/build/llama_engine.so
# Expected: works with warn "manifest not found"
```

### Task 6.3: Verify PluginLoader 4 public methods unchanged

```bash
grep -rn "loader\.load_so\|loader\.load_all\|loader\.list_loaded\|loader\.unload_plugin" --include="*.cpp" --include="*.h" tests/ examples/
# Expected: all call sites still compile with default params
```

---

## 7. Architecture Compliance

### Task 7.1-7.5: Module build + tools validation

- Verify `agenticdsl_modules_pdk` + `agenticdsl_modules_plugin` compile cleanly
- `tools/adr_lint.py` exit 0
- `tools/docs_drift_audit.py` exit 0
- `openspec validate pdk-manifest-validation --strict` exit 0
- `make -j$(nproc)` zero errors

---

## 8. Commit & Sync (master plan update + archive)

### Task 8.1-8.7: Final commit + master plan sync + archive

- 8.1: Final ctest 140+ PASS
- 8.2: git-master atomic commit per unit (already done per task)
- 8.3: Update ADR-0052 status line + ship evidence
- 8.4: Update `docs/active-status.md` with ship record
- 8.5: Update `docs/superpowers/plans/2026-07-15-phase6-agentforge-mvp.md`:
  - §十 状态 → superseded-by-audit-2026-08-09
  - §十一 追加 Wave 3-A 前置完成注记
  - §七 决策日志追加 2026-08-09 行
- 8.6: `tools/check_roadmap_drift.py` 0 CRITICAL
- 8.7: `openspec archive pdk-manifest-validation`

---

## Estimated Time

| Section | Tasks | Hours |
|---|---|---|
| 1. Setup & Types | 1.1-1.4 | 1.5h |
| 2. ManifestValidator TDD | 2.1-2.11 | 4h |
| 3. ManifestFinder TDD | 3.1-3.8 | 2h |
| 4. PluginLoader Integration | 4.1-4.8 | 3h |
| 5. EventBus Integration | 5.1-5.3 | 1.5h |
| 6. Backward Compatibility | 6.1-6.3 | 1h |
| 7. Architecture Compliance | 7.1-7.5 | 0.5h |
| 8. Commit & Sync | 8.1-8.7 | 1.5h |
| **Total** | **48 tasks** | **15h** |

Plus buffer: 3-7h for integration debugging + spec amendments.

**Realistic estimate: 18-22h (3 days calendar), Solo dev mode.**

---

## Critical Reminders

- **TDD discipline**: RED → GREEN → COMMIT. Never implement before test fails for the right reason.
- **Backward compat**: 缺 manifest must NOT block existing 12 PDK plugins.
- **Strict type check**: nlohmann::json is lenient. Always check `j.is_string()` / `j.is_number_unsigned()` before `j.get<T>()`.
- **Dual ABI**: Mirror `SUPPORTED_ABI_VERSIONS={1, 2}` (per `include/agenticdsl/plugin/plugin_info.h:77`).
- **PluginInfo priority**: After dlopen, PluginInfo wins for abi_version (per ADR-0052 §决策 4).
- **PIMPL pattern**: PluginLoader uses `unique_ptr<Impl>` (Sprint 18-19). bus_ is in Impl.
- **No BREAKING**: 4 public methods unchanged, default params only.
