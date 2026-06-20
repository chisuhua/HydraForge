#!/usr/bin/env bash
# scripts/sync-pdk.sh
# 文件头注释
# 功能描述: 将 HydraForge monorepo 的 PDK 源文件同步到独立的 hydraforge-pdk 发布仓库
#          (Option C: vendored + 单独发布 repo, ADR-0021 §7 Dual-Repo Policy)
#
#          工作流:
#          1. 临时 clone hydraforge-pdk 到 /tmp
#          2. 从 monorepo pdk/ + include/agenticdsl/pdk/ 拷贝源文件
#          3. 同步 nlohmann_json + Catch2 bundled headers
#          4. 在临时目录 git commit + push 到 chisuhua/hydraforge-pdk
#
# 设计依据: ADR-0021 §7 Dual-Repo Policy + plan §Sprint 4 T4b
# 作者: AgenticDSL Phase 1 Sprint 4
# 最后修改日期: 2026-06-19

set -euo pipefail

# === 配置 ===
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MONOREPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
WORK_DIR="${PDK_SYNC_WORK_DIR:-/tmp/hydraforge-pdk-sync}"
STANDALONE_REPO="${PDK_STANDALONE_REPO:-git@github.com:chisuhua/hydraforge-pdk.git}"
STANDALONE_BRANCH="${PDK_STANDALONE_BRANCH:-main}"
DRY_RUN="${PDK_SYNC_DRY_RUN:-0}"

# Monorepo 路径
# PDK 头文件实际位置: include/agenticdsl/pdk/ (canonical)
# pdk/ 子目录仅含 CMakeLists.txt + hydraforge/pdk/pdk.h (forward stub)
SRC_INCLUDE_DIR="${MONOREPO_ROOT}/include/agenticdsl/pdk"
SRC_CATCH_H="${MONOREPO_ROOT}/tests/catch_amalgamated.hpp"
SRC_CATCH_CPP="${MONOREPO_ROOT}/tests/catch_amalgamated.cpp"
SRC_NLOHMANN="${MONOREPO_ROOT}/external/nlohmann_json/single_include/nlohmann/json.hpp"

# === 颜色输出 ===
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $*"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $*" >&2; }
log_error() { echo -e "${RED}[ERROR]${NC} $*" >&2; }
log_ok() { echo -e "${GREEN}[OK]${NC} $*"; }

# === 前置检查 ===
preflight_check() {
  log_info "Running preflight checks..."

  local errors=0

  # 检查 monorepo 源文件存在
  if [[ ! -d "${SRC_INCLUDE_DIR}" ]]; then
    log_error "Monorepo include/agenticdsl/pdk/ not found: ${SRC_INCLUDE_DIR}"
    errors=$((errors + 1))
  fi
  if [[ ! -f "${SRC_CATCH_H}" ]]; then
    log_warn "Catch2 header not found: ${SRC_CATCH_H} (will skip Catch2 bundling)"
  fi
  if [[ ! -f "${SRC_NLOHMANN}" ]]; then
    log_error "nlohmann_json not found: ${SRC_NLOHMANN}"
    errors=$((errors + 1))
  fi

  # 检查 git 可达性
  if ! command -v git >/dev/null 2>&1; then
    log_error "git not found in PATH"
    errors=$((errors + 1))
  fi

  if [[ ${errors} -gt 0 ]]; then
    log_error "Preflight failed with ${errors} error(s)"
    exit 1
  fi
  log_ok "Preflight passed"
}

# === 准备临时工作目录 ===
prepare_workdir() {
  log_info "Preparing workdir: ${WORK_DIR}"

  if [[ -d "${WORK_DIR}" ]]; then
    log_info "Existing workdir found, pulling latest..."
    (cd "${WORK_DIR}" && \
      git checkout "${STANDALONE_BRANCH}" >/dev/null 2>&1 && \
      git pull origin "${STANDALONE_BRANCH}" >/dev/null 2>&1) || \
      { log_error "Failed to update existing workdir"; exit 1; }
  else
    log_info "Cloning standalone repo..."
    git clone "${STANDALONE_REPO}" "${WORK_DIR}" >/dev/null 2>&1 || \
      { log_error "Failed to clone ${STANDALONE_REPO}"; exit 1; }
    (cd "${WORK_DIR}" && git checkout "${STANDALONE_BRANCH}" >/dev/null 2>&1) || \
      { log_error "Failed to checkout ${STANDALONE_BRANCH}"; exit 1; }
  fi
}

# === 同步文件 ===
sync_files() {
  log_info "Syncing files from monorepo..."

  local pdk_version
  pdk_version=$(grep -oP 'HYDRAFORGE_PDK_VERSION\s+"\K[^"]+' "${SRC_INCLUDE_DIR}/pdk.h" 2>/dev/null || echo "0.1.0")
  log_info "Detected PDK version: ${pdk_version}"

  # 1. 清理 standalone 旧文件 (保留 .git, README.md, LICENSE 等)
  rm -rf "${WORK_DIR}/include" "${WORK_DIR}/tests" "${WORK_DIR}/external" \
         "${WORK_DIR}/CMakeLists.txt"
  mkdir -p "${WORK_DIR}/include/hydraforge/pdk" \
           "${WORK_DIR}/tests" \
           "${WORK_DIR}/external/nlohmann_json/single_include/nlohmann"

  # 2. 复制 PDK 头文件 (tool_macros.h + safe_exec.h 直接复制, 无 monorepo 依赖)
  cp "${SRC_INCLUDE_DIR}/tool_macros.h"  "${WORK_DIR}/include/hydraforge/pdk/"
  cp "${SRC_INCLUDE_DIR}/safe_exec.h"    "${WORK_DIR}/include/hydraforge/pdk/"
  log_ok "Copied 2 PDK headers (tool_macros + safe_exec) to include/hydraforge/pdk/"

  # 2b. agent_macros.h 特殊处理: monorepo 版本 include monorepo 内部
  #     (agenticdsl/contract/iinteraction_bus.h + core/engine.h 等),
  #     standalone 版本必须替换为前向声明 + minimal stubs。
  cp "${SRC_INCLUDE_DIR}/agent_macros.h" "${WORK_DIR}/include/hydraforge/pdk/.agent_macros.h.monorepo.tmp"
  cat > "${WORK_DIR}/include/hydraforge/pdk/agent_macros.h" << 'AGENT_MACROS_EOF'
// include/hydraforge/pdk/agent_macros.h (standalone version)
// 文件头注释
// 功能描述: DEFINE_AGENT 宏 — Agent 循环脚手架 (ADR-0021 §3.2, 独立仓库版本)
//          展开为 class XXXAgent 含 run(prompt) 方法。MVP 仅支持 React 循环。
// 设计依据: ADR-0021 + openspec/changes/2026-07-07-pdk-skeleton
// 注: 此文件由 scripts/sync-pdk.sh 自动生成 (从 monorepo 版本转换),
//     勿手动编辑。monorepo 原版在 include/agenticdsl/pdk/agent_macros.h

#pragma once

// Standalone 版本: 使用前向声明 + minimal stubs (无 monorepo Runtime 依赖)
// 真实使用需链接 HydraForge Runtime (find_package(hydraforge))

#include <nlohmann/json.hpp>

#include <memory>
#include <string>
#include <utility>

namespace agenticdsl {

// 前向声明 Runtime 类型 (Phase 2 链接时由 HydraForge Runtime 提供完整定义)
class DSLEngine;
class SimpleCognitiveOrchestrator;
class IInteractionBus;

// Minimal ToolResult stub (用于 standalone MVP, Phase 2 替换为 Runtime 版本)
struct ToolResult {
  bool ok = false;
  nlohmann::json data = nlohmann::json::object();
  nlohmann::json meta = nlohmann::json::object();
};

} // namespace agenticdsl

namespace hydraforge::pdk {

enum class AgentLoopType {
  React,
  PlanExecute,
  ForkJoin,
};

#define DEFINE_AGENT(name, loop_type)                                                  \
  static_assert(loop_type == ::hydraforge::pdk::AgentLoopType::React,                  \
                "DEFINE_AGENT MVP only supports AgentLoopType::React. "                 \
                "PlanExecute / ForkJoin are Phase 2 TODO (see ADR-0021 §3.2).");       \
  class name##Agent {                                                                  \
   public:                                                                             \
    name##Agent(std::unique_ptr<agenticdsl::DSLEngine> engine,                         \
                std::shared_ptr<agenticdsl::IInteractionBus> bus)                      \
        : engine_(std::move(engine)), bus_(std::move(bus)) {}                           \
    agenticdsl::ToolResult run(const std::string& prompt) {                             \
      if (!engine_) {                                                                  \
        agenticdsl::ToolResult err;                                                    \
        err.meta["error_message"] = "Agent DSLEngine is null";                         \
        return err;                                                                    \
      }                                                                                \
      agenticdsl::ToolResult result;                                                   \
      result.ok = true;                                                                \
      result.meta["prompt"] = prompt;                                                  \
      result.meta["note"] = "DEFINE_AGENT standalone MVP: orch not invoked";            \
      return result;                                                                  \
    }                                                                                  \
   private:                                                                            \
    std::unique_ptr<agenticdsl::DSLEngine> engine_;                                   \
    std::shared_ptr<agenticdsl::IInteractionBus> bus_;                                 \
  };

} // namespace hydraforge::pdk
AGENT_MACROS_EOF
  log_ok "Generated standalone agent_macros.h (with stubs)"

  # 3. 生成 standalone 版本的 pdk.h (引用 hydraforge/pdk/ 路径)
  cat > "${WORK_DIR}/include/hydraforge/pdk/pdk.h" << 'PDK_H_EOF'
// include/hydraforge/pdk/pdk.h (standalone version)
// 文件头注释
// 功能描述: PDK 统一入口 (独立仓库版本, ADR-0021 §7)
//          引用 3 个子头 + 版本宏
// 设计依据: ADR-0021 + openspec/changes/2026-07-07-pdk-skeleton
// 注: 此文件由 scripts/sync-pdk.sh 自动生成, 勿手动编辑
//     (monorepo 路径在 include/agenticdsl/pdk/pdk.h)

#pragma once

#include <hydraforge/pdk/tool_macros.h>
#include <hydraforge/pdk/agent_macros.h>
#include <hydraforge/pdk/safe_exec.h>

// PDK 版本 (与 monorepo 同步, 由 sync-pdk.sh 注入)
#ifndef HYDRAFORGE_PDK_VERSION
#define HYDRAFORGE_PDK_VERSION_MAJOR 0
#define HYDRAFORGE_PDK_VERSION_MINOR 1
#define HYDRAFORGE_PDK_VERSION_PATCH 0
#define HYDRAFORGE_PDK_VERSION "0.1.0"
#endif
PDK_H_EOF
  log_ok "Generated standalone pdk.h"

  # 4. 复制 Catch2 bundled headers (用于 standalone 测试)
  if [[ -f "${SRC_CATCH_H}" ]]; then
    cp "${SRC_CATCH_H}" "${WORK_DIR}/tests/catch_amalgamated.hpp"
    log_ok "Copied catch_amalgamated.hpp"
  fi
  if [[ -f "${SRC_CATCH_CPP}" ]]; then
    cp "${SRC_CATCH_CPP}" "${WORK_DIR}/tests/catch_amalgamated.cpp"
    log_ok "Copied catch_amalgamated.cpp"
  fi

  # 5. 复制 nlohmann_json bundled header
  if [[ -f "${SRC_NLOHMANN}" ]]; then
    cp "${SRC_NLOHMANN}" "${WORK_DIR}/external/nlohmann_json/single_include/nlohmann/json.hpp"
    log_ok "Copied nlohmann_json header"
  fi

  # 6. 复制 CMakeLists.txt (来自 extract repo, 用 sed 调整路径)
  cat > "${WORK_DIR}/CMakeLists.txt" << 'CMAKE_EOF'
# hydraforge-pdk — independent repo CMakeLists
# 功能描述: PDK 独立仓库根 CMakeLists (K3 决策, ADR-0021 §2.2)
#          此文件由 scripts/sync-pdk.sh 自动生成, 勿手动编辑
# 设计依据: ADR-0021 + openspec/changes/2026-07-07-pdk-skeleton
# 作者: AgenticDSL Phase 1 Sprint 4 (T4b)

cmake_minimum_required(VERSION 3.20)
project(hydraforge_pdk VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# --- PDK INTERFACE 库 (header-only) ---
add_library(hydraforge_pdk INTERFACE)
add_library(hydraforge::pdk ALIAS hydraforge_pdk)

target_include_directories(hydraforge_pdk INTERFACE
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>
)

target_compile_features(hydraforge_pdk INTERFACE cxx_std_20)

# nlohmann_json 依赖 (bundled, 独立仓库版本)
target_include_directories(hydraforge_pdk INTERFACE
  ${CMAKE_CURRENT_SOURCE_DIR}/external/nlohmann_json/single_include
)

# --- 测试 (独立仓库验证) ---
option(HYDRAFORGE_PDK_BUILD_TESTS "Build PDK standalone tests" ON)
if(HYDRAFORGE_PDK_BUILD_TESTS)
  enable_testing()

  # Standalone 测试 (不需要 Runtime, 仅测试 PDK 自身契约)
  if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tests/test_pdk_macros_standalone.cpp")
    add_executable(test_pdk_macros
      tests/test_pdk_macros_standalone.cpp
      tests/catch_amalgamated.cpp
    )
    target_link_libraries(test_pdk_macros PRIVATE hydraforge_pdk)
    target_include_directories(test_pdk_macros PRIVATE tests)
    target_compile_definitions(test_pdk_macros PRIVATE
      CATCH_CONFIG_ENABLE_ALL_STRINGMAKERS=1
      HYDRAFORGE_PDK_STANDALONE=1
    )
    add_test(NAME test_pdk_macros COMMAND test_pdk_macros)
    message(STATUS "[hydraforge_pdk] test_pdk_macros registered")
  endif()
endif()

# --- 安装规则 (Phase 2 后续) ---
include(GNUInstallDirs)
install(TARGETS hydraforge_pdk EXPORT hydraforge_pdkTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
CMAKE_EOF
  log_ok "Generated standalone CMakeLists.txt"

  # 7. 复制 standalone test (从 extract repo)
  if [[ -f "/tmp/opencode/hydraforge-pdk-extract/tests/test_pdk_macros_standalone.cpp" ]]; then
    cp "/tmp/opencode/hydraforge-pdk-extract/tests/test_pdk_macros_standalone.cpp" \
       "${WORK_DIR}/tests/"
    log_ok "Copied standalone test"
  else
    log_warn "Standalone test source not found at /tmp/opencode/hydraforge-pdk-extract/tests/, skipping"
    log_warn "(If this is the first sync, run the bootstrap commands in this script's header first)"
  fi
}

# === 生成 standalone README ===
generate_readme() {
  cat > "${WORK_DIR}/README.md" << 'README_EOF'
# hydraforge-pdk

**Plugin Development Kit (PDK) for HydraForge** — independent, header-only library for building domain plugins.

> **Status**: 🟡 Partial (v0.1.0, 2026-06-19, Sprint 4 MVP)
> This standalone repo is **auto-synced** from [HydraForge monorepo](https://github.com/chisuhua/HydraForge) via `scripts/sync-pdk.sh` (ADR-0021 §7 Dual-Repo Policy).
> Design: [ADR-0021 PDK Design](https://github.com/chisuhua/HydraForge/blob/main/docs/adr/adr-0021-pdk-design.md)

## Dual-Repo Policy

PDK is developed in the HydraForge monorepo (`include/agenticdsl/pdk/` + `pdk/`) and
periodically synced to this standalone repo for external consumers.

- **Vendored in monorepo** for zero-friction internal dev + tests
- **Published standalone** for downstream consumers via `find_package(hydraforge_pdk)`

See ADR-0021 §7 in HydraForge monorepo for full rationale.

## Overview

PDK provides a **standardized development toolkit** for HydraForge domain plugin authors, reducing boilerplate from ~20 lines to ~5 lines per tool:

| Component | Purpose | Status |
|-----------|---------|--------|
| `DECLARE_TOOL` macro | Tool registration scaffold (Schema + permissions + error handling) | ✅ MVP |
| `DEFINE_AGENT` macro | Agent loop template (React MVP; PlanExecute/ForkJoin TODO) | ✅ MVP (React) |
| `SafeExec` wrapper | Sandbox execution (timeout + exception; no fork/seccomp) | ✅ MVP |
| `ToolSpec` / `ToolParam` / `ToolPermissions` | Tool metadata structures | ✅ MVP |
| `FakeStateStore` / `StubLLM` / `MockSandbox` | Test doubles | 🔜 Phase 2 |
| `PluginLifecycle` | Plugin init/load/unload/health_check | 🔜 Phase 3 |
| Full SafeExec with fork/cgroups/seccomp | Process-level isolation | 🔜 Phase 3 |
| CMake project generator | `cmake_init()` / `project_template()` | 🔜 Phase 4 |

## Quick Start

```cpp
// my_plugin.cpp
#include <hydraforge/pdk/pdk.h>
#include <nlohmann/json.hpp>

using namespace hydraforge::pdk;

// DECLARE_TOOL: 5 lines of domain logic
DECLARE_TOOL(echo_tool, "Echo back input",
  return __pdk_args;
)

// DEFINE_AGENT: React loop MVP (auto-generated agent class)
DEFINE_AGENT(my_agent, AgentLoopType::React);
// Expands to: class my_agentAgent { ... };

// SafeExec: timeout + exception isolation
SafeExec().with_timeout(std::chrono::milliseconds(100))
          .run([] { /* your code */ });
```

## Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_CXX_COMPILER=g++-13
cmake --build .
ctest --output-on-failure
```

**Requirements**: C++20, nlohmann_json (bundled), HydraForge Runtime (for full integration in Phase 2+).

## Design Principles (ADR-0021 P1-P6)

1. **P1**: PDK is **not** part of Runtime — independent repo (this one)
2. **P2**: PDK is **optional** — advanced developers can hand-write
3. **P3**: PDK **statically links** to plugins — Runtime has zero awareness, zero overhead
4. **P4**: PDK wraps only **general development patterns** — no domain logic
5. **P5**: PDK version is **decoupled** from Runtime — independent upgrade path
6. **P6**: PDK provides **test doubles** — plugins can be tested in isolation

## Repository Structure

```
hydraforge-pdk/
├── CMakeLists.txt                 # INTERFACE library (header-only)
├── README.md                      # This file
├── include/
│   └── hydraforge/
│       └── pdk/
│           ├── pdk.h              # Unified entry
│           ├── tool_macros.h      # DECLARE_TOOL
│           ├── agent_macros.h     # DEFINE_AGENT
│           └── safe_exec.h        # SafeExec
├── external/
│   └── nlohmann_json/             # Bundled header
└── tests/
    ├── catch_amalgamated.{hpp,cpp}  # Bundled Catch2
    └── test_pdk_macros_standalone.cpp
```

## Migration from Monorepo

The monorepo `HydraForge` (https://github.com/chisuhua/HydraForge) also vendors PDK at `include/agenticdsl/pdk/` and `pdk/`. The two paths are **API-compatible** — switching to standalone is a CMake change only:

**Monorepo** (vendored PDK):
```cmake
add_subdirectory(pdk)
target_link_libraries(my_plugin PRIVATE hydraforge_pdk)
```

**Standalone** (this repo):
```cmake
find_package(hydraforge_pdk 0.1 REQUIRED)
target_link_libraries(my_plugin PRIVATE hydraforge::pdk)
```

## Test Status

- **4/4** standalone test cases pass (31 assertions)
- Covers: DECLARE_TOOL expansion, SafeExec timeout, SafeExec exception, Runtime decoupling
- CI integration: pending (Phase 2)

## Related ADRs (in HydraForge monorepo)

- [ADR-0021 PDK Design](https://github.com/chisuhua/HydraForge/blob/main/docs/adr/adr-0021-pdk-design.md)
- [ADR-0022 Plugin Loading](https://github.com/chisuhua/HydraForge/blob/main/docs/adr/adr-0022-plugin-loading.md)
- [ADR-0019 IInteractionBus](https://github.com/chisuhua/HydraForge/blob/main/docs/adr/adr-0019-iinteraction-bus-mvp.md)
- [ADR-0020 Thread Model Isolation](https://github.com/chisuhua/HydraForge/blob/main/docs/adr/adr-0020-thread-model-isolation.md)

## License

TBD (per HydraForge project license)
README_EOF
  log_ok "Generated standalone README.md"
}

# === 提交 + 推送 ===
commit_and_push() {
  log_info "Committing and pushing..."

  cd "${WORK_DIR}"

  # 配置 git 用户 (如未设置)
  if ! git config user.email >/dev/null 2>&1; then
    git config user.email "sisyphus@hydraforge.dev"
    git config user.name "Sisyphus"
  fi

  # .gitignore
  cat > .gitignore << 'GITIGNORE_EOF'
# Build artifacts
build/
Testing/

# Sync script temp files
*.tmp

# IDE / editor
.vscode/
.idea/
*.swp
*.swo
.DS_Store
GITIGNORE_EOF

  git add -A
  git status --short

  # 检测变更
  if git diff --cached --quiet; then
    log_info "No changes to commit (up-to-date)"
    return 0
  fi

  # DRY_RUN 检查必须在 commit 之前 (避免污染 workdir 历史)
  if [[ "${DRY_RUN}" == "1" ]]; then
    log_info "DRY RUN: would commit + push to ${STANDALONE_REPO}:${STANDALONE_BRANCH}"
    log_info "Files staged (not committed):"
    git diff --cached --stat
    return 0
  fi

  local version
  version=$(grep -oP 'HYDRAFORGE_PDK_VERSION\s+"\K[^"]+' include/hydraforge/pdk/pdk.h 2>/dev/null || echo "0.1.0")

  git commit -m "feat(pdk): sync v${version} from HydraForge monorepo" \
             -m "Automated sync via scripts/sync-pdk.sh (ADR-0021 §7 Dual-Repo Policy).

Source: HydraForge monorepo commit \$(git rev-parse HEAD) on branch \$(git branch --show-current)
" \
             -m "Ultraworked with [Sisyphus](https://github.com/code-yeongyu/oh-my-openagent)" \
             -m "Co-authored-by: Sisyphus <clio-agent@sisyphuslabs.ai>" 2>&1 | tail -5

  git push origin "${STANDALONE_BRANCH}" 2>&1 | tail -5
  log_ok "Pushed to ${STANDALONE_REPO}:${STANDALONE_BRANCH}"
}

# === 验证 standalone 构建 ===
verify_standalone_build() {
  if [[ "${DRY_RUN}" == "1" ]]; then
    log_info "DRY RUN: skipping standalone build verification"
    return 0
  fi

  log_info "Verifying standalone build..."

  local build_dir="${WORK_DIR}/_verify_build"
  rm -rf "${build_dir}"
  mkdir -p "${build_dir}"

  (cd "${build_dir}" && \
    cmake "${WORK_DIR}" -DCMAKE_CXX_COMPILER=g++-13 >/dev/null 2>&1 && \
    cmake --build . -j2 >/dev/null 2>&1) || \
    { log_error "Standalone build failed"; return 1; }

  (cd "${build_dir}" && \
    ./test_pdk_macros 2>&1 | tail -3) || \
    { log_error "Standalone tests failed"; return 1; }

  log_ok "Standalone build + tests passed"
  rm -rf "${build_dir}"
}

# === 主流程 ===
main() {
  echo "=========================================="
  echo "  HydraForge PDK → Standalone Sync"
  echo "  (ADR-0021 §7 Dual-Repo Policy)"
  echo "=========================================="
  echo ""

  preflight_check
  prepare_workdir
  sync_files
  generate_readme
  commit_and_push
  verify_standalone_build

  echo ""
  log_ok "Sync complete!"
  echo ""
  echo "  Standalone repo: ${STANDALONE_REPO}"
  echo "  Branch: ${STANDALONE_BRANCH}"
  echo "  Workdir: ${WORK_DIR}"
  echo ""
  echo "Next steps:"
  echo "  - Verify on GitHub: https://github.com/chisuhua/hydraforge-pdk"
  echo "  - (Optional) Tag a release: cd ${WORK_DIR} && git tag v\$(grep HYDRAFORGE_PDK_VERSION include/hydraforge/pdk/pdk.h | grep -oP '\"\\K[^\"]+') && git push --tags"
}

main "$@"