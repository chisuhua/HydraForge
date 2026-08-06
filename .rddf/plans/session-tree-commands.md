# session-tree-commands Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 `pdk_chat_demo` 中新增 `/tree` `/fork` `/clone` 三个 slash 命令，经 `DECLARE_COMMAND` 注册，通过 `ToolCoordinator` 治理路径操作 `SessionManager`，并提供 ANSI 会话树渲染 + 窄终端降级。

**Architecture:** 扩展 `SessionManager` 只读 API（`list_all_nodes`, `get_node_by_short_id`, `get_branch_leaf_node`）；新增 `examples/pdk_chat_demo/commands/` 下 `tree_command` / `fork_command` / `clone_command`；新增 `examples/pdk_chat_demo/tools/` 下 `session_fork` / `session_clone` 工具注册；新增 `examples/pdk_chat_demo/tui/tree_renderer` 负责渲染；`main.cpp` 仅注册命令，零 hardcode 分支；测试覆盖命令派发、渲染 golden file、fork/clone 持久化恢复。

**Tech Stack:** C++20, Catch2, nlohmann::json, ANSI escape, `ioctl(TIOCGWINSZ)`, HydraForge AgenticOS core (SessionManager, ToolCoordinator, CommandRegistry, DECLARE_COMMAND).

---

## File Structure

### Production Code

| File | Responsibility |
|---|---|
| `src/core/session_manager.h` | 新增 `list_all_nodes`, `get_node_by_short_id`, `get_branch_leaf_node` 只读 API |
| `src/core/session_manager.cpp` | 实现上述 API |
| `examples/pdk_chat_demo/commands/tree_command.h` | `/tree` 命令声明 |
| `examples/pdk_chat_demo/commands/tree_command.cpp` | `/tree` 命令体：参数解析 + 渲染调用 |
| `examples/pdk_chat_demo/commands/fork_command.h` | `/fork` 命令声明 |
| `examples/pdk_chat_demo/commands/fork_command.cpp` | `/fork` 命令体：调用 `session/fork` 工具 |
| `examples/pdk_chat_demo/commands/clone_command.h` | `/clone` 命令声明 |
| `examples/pdk_chat_demo/commands/clone_command.cpp` | `/clone` 命令体：调用 `session/clone` 工具 |
| `examples/pdk_chat_demo/commands/help_command.h` | `/help` 内置命令（复用 chat-slash-cmd 的提取） |
| `examples/pdk_chat_demo/commands/exit_command.h` | `/exit` 内置命令（复用 chat-slash-cmd 的提取） |
| `examples/pdk_chat_demo/commands/compact_command.h` | `/compact` 内置命令（复用 chat-slash-cmd 的提取） |
| `examples/pdk_chat_demo/tools/session_fork.h` | `session/fork` 工具注册声明 |
| `examples/pdk_chat_demo/tools/session_fork.cpp` | `session/fork` 工具注册实现 |
| `examples/pdk_chat_demo/tools/session_clone.h` | `session/clone` 工具注册声明 |
| `examples/pdk_chat_demo/tools/session_clone.cpp` | `session/clone` 工具注册实现 |
| `examples/pdk_chat_demo/tui/tree_renderer.h` | 树渲染器声明 |
| `examples/pdk_chat_demo/tui/tree_renderer.cpp` | ANSI 树 + 窄终端降级 |
| `examples/pdk_chat_demo/CMakeLists.txt` | 追加新的 commands/tools/tui 源文件 |
| `examples/pdk_chat_demo/main.cpp` | 注册三个新命令，移除 hardcode 分支 |
| `examples/pdk_chat_demo/input_loop.h` | 提取输入循环体（复用 chat-slash-cmd 的提取） |
| `examples/pdk_chat_demo/input_loop.cpp` | 输入循环体实现 |

### Tests

| File | Responsibility |
|---|---|
| `tests/test_session_tree_read_api.cpp` | `SessionManager` 新只读 API 单元测试 |
| `tests/test_session_fork_clone_tool.cpp` | `session/fork` + `session/clone` 工具注册 + layer check |
| `tests/test_tree_renderer.cpp` | 宽/窄/空 session golden file 渲染测试 |
| `tests/test_session_tree_commands.cpp` | 命令参数解析 + 派发测试 |
| `tests/test_pdk_chat_session_tree.cpp` | mock LLM + 真实 SessionManager 的 E2E 集成 |
| `tests/test_main_tree_hardcode_audit.cpp` | grep 审计 main.cpp 无 `/tree`/`/fork`/`/clone` 硬编码 |

---

## Task 1: Extend SessionManager read-only APIs

**Files:**
- Modify: `src/core/session_manager.h`
- Modify: `src/core/session_manager.cpp`
- Test: `tests/test_session_tree_read_api.cpp`

- [ ] **Step 1: Write the failing test**

Create `tests/test_session_tree_read_api.cpp`:
```cpp
#include <catch_amalgamated.hpp>
#include "core/session_manager.h"

TEST_CASE("SessionManager::list_all_nodes returns all appended nodes", "[session-tree]") {
  agenticdsl::SessionManager sm("/tmp/session_tree_test_1");
  sm.open("test");
  sm.append_to_branch("hello");
  sm.append_to_branch("world");
  auto nodes = sm.list_all_nodes();
  REQUIRE(nodes.size() == 2);
}

TEST_CASE("SessionManager::get_node_by_short_id matches unique prefix", "[session-tree]") {
  agenticdsl::SessionManager sm("/tmp/session_tree_test_2");
  sm.open("test");
  auto id = sm.append_to_branch("hello");
  auto short_id = id.substr(0, 8);
  auto result = sm.get_node_by_short_id(short_id);
  REQUIRE(result.has_value());
  REQUIRE(result->id == id);
}

TEST_CASE("SessionManager::get_branch_leaf_node returns latest leaf", "[session-tree]") {
  agenticdsl::SessionManager sm("/tmp/session_tree_test_3");
  sm.open("test");
  sm.append_to_branch("a");
  auto leaf = sm.append_to_branch("b");
  auto result = sm.get_branch_leaf_node(sm.current_branch());
  REQUIRE(result.has_value());
  REQUIRE(result->first == sm.current_branch());
  REQUIRE(result->second.id == leaf);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_session_tree_read_api && ctest -R test_session_tree_read_api`
Expected: build fails because the methods do not exist.

- [ ] **Step 3: Write minimal implementation**

Add to `src/core/session_manager.h` after `find_node`:
```cpp
  /// @brief 获取所有节点 (O(N))
  std::vector<SessionNode> list_all_nodes() const;

  /// @brief 短前缀匹配节点 (8 字符), 歧义返回 nullopt
  std::optional<SessionNode> get_node_by_short_id(const std::string& short_id) const;

  /// @brief 返回分支元信息 + 该分支最新 leaf 节点
  std::optional<std::pair<BranchMeta, SessionNode>> get_branch_leaf_node(
      const std::string& branch_id) const;
```

Implement in `src/core/session_manager.cpp`:
```cpp
std::vector<SessionNode> SessionManager::list_all_nodes() const {
  std::lock_guard<std::mutex> lock(index_mutex_);
  std::vector<SessionNode> out;
  out.reserve(nodes_.size());
  for (const auto& [id, node] : nodes_) out.push_back(node);
  return out;
}

std::optional<SessionNode> SessionManager::get_node_by_short_id(
    const std::string& short_id) const {
  std::lock_guard<std::mutex> lock(index_mutex_);
  std::optional<SessionNode> match;
  for (const auto& [id, node] : nodes_) {
    if (id.compare(0, short_id.size(), short_id) == 0) {
      if (match.has_value()) return std::nullopt;  // ambiguous
      match = node;
    }
  }
  return match;
}

std::optional<std::pair<BranchMeta, SessionNode>> SessionManager::get_branch_leaf_node(
    const std::string& branch_id) const {
  std::lock_guard<std::mutex> lock(index_mutex_);
  auto it = branches_.find(branch_id);
  if (it == branches_.end()) return std::nullopt;
  auto leaf_id = get_branch_leaf(branch_id);
  if (leaf_id.empty()) return std::nullopt;
  auto nit = nodes_.find(leaf_id);
  if (nit == nodes_.end()) return std::nullopt;
  return std::make_pair(it->second, nit->second);
}
```

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest -R test_session_tree_read_api`
Expected: PASS.

- [ ] **Step 5: Defer commit**

---

## Task 2: Create tree / fork / clone command files

**Files:**
- Create: `examples/pdk_chat_demo/commands/tree_command.{h,cpp}`
- Create: `examples/pdk_chat_demo/commands/fork_command.{h,cpp}`
- Create: `examples/pdk_chat_demo/commands/clone_command.{h,cpp}`
- Modify: `examples/pdk_chat_demo/CMakeLists.txt`
- Test: `tests/test_session_tree_commands.cpp`

- [ ] **Step 1: Write the failing test**

Create `tests/test_session_tree_commands.cpp`:
```cpp
#include <catch_amalgamated.hpp>
#include <common/tools/command_registry.h>
#include "examples/pdk_chat_demo/commands/tree_command.h"
#include "examples/pdk_chat_demo/commands/fork_command.h"
#include "examples/pdk_chat_demo/commands/clone_command.h"

TEST_CASE("/tree /fork /clone specs are registered", "[session-tree]") {
  agenticdsl::CommandRegistry reg;
  pdk_chat_demo::g_command_coordinator = nullptr;
  REQUIRE(reg.register_command(pdk_chat_demo::make_tree_command_spec()));
  REQUIRE(reg.register_command(pdk_chat_demo::make_fork_command_spec()));
  REQUIRE(reg.register_command(pdk_chat_demo::make_clone_command_spec()));
  auto help = reg.render_help();
  REQUIRE(help.find("/tree") != std::string::npos);
  REQUIRE(help.find("/fork") != std::string::npos);
  REQUIRE(help.find("/clone") != std::string::npos);
}
```

- [ ] **Step 2: Run test to verify it fails**

Run: `cmake --build build --target test_session_tree_commands && ctest -R test_session_tree_commands`
Expected: build fails because files do not exist.

- [ ] **Step 3: Write minimal implementation**

Create command headers and stub bodies that return "not implemented" strings. Register them in `main.cpp` (Task 6). Add source files to CMake.

- [ ] **Step 4: Run test to verify it passes**

Expected: PASS after CMake wiring.

- [ ] **Step 5: Defer commit**

---

## Task 3: Implement session/fork + session/clone tools

**Files:**
- Create: `examples/pdk_chat_demo/tools/session_fork.{h,cpp}`
- Create: `examples/pdk_chat_demo/tools/session_clone.{h,cpp}`
- Modify: `examples/pdk_chat_demo/CMakeLists.txt`
- Test: `tests/test_session_fork_clone_tool.cpp`

- [ ] **Step 1: Write the failing test**

Create `tests/test_session_fork_clone_tool.cpp` with mock `SpyRegistry` or real `SessionManager` verifying the tools are registered and layer-denied on Cognitive.

- [ ] **Step 2: Run test to verify it fails**

Expected: build fails.

- [ ] **Step 3: Write minimal implementation**

Implement `session_fork.cpp`:
```cpp
registry.register_tool_function("session_fork", meta,
  [](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
    // real SessionManager call from main.cpp context is not available here;
    // use global g_session_manager pointer or pass via args.
  });
```

Use `pdk_chat_demo::g_session_manager` global pointer set in main.cpp after `ChatSession` initialization, similar to `g_command_coordinator`.

- [ ] **Step 4: Run test to verify it passes**

Expected: PASS.

- [ ] **Step 5: Defer commit**

---

## Task 4: Implement ANSI tree renderer with narrow fallback

**Files:**
- Create: `examples/pdk_chat_demo/tui/tree_renderer.{h,cpp}`
- Modify: `examples/pdk_chat_demo/CMakeLists.txt`
- Test: `tests/test_tree_renderer.cpp`

- [ ] **Step 1: Write the failing test**

Create `tests/test_tree_renderer.cpp` with golden files for wide (120 col), narrow (40 col), and empty session.

- [ ] **Step 2: Run test to verify it fails**

Expected: build fails.

- [ ] **Step 3: Write minimal implementation**

Implement `tree_renderer.cpp`:
- `get_terminal_width()` via `ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)`.
- `render_tree(branches, nodes, current_leaf_id, width)`:
  - If width >= 60: build ASCII tree `├──`/`└──`/`│`, highlight current leaf with green ANSI.
  - If width < 60: list mode `<branch_id> <node_count> <created_at>`.

- [ ] **Step 4: Run test to verify it passes**

Run: `ctest -R test_tree_renderer`
Expected: PASS.

- [ ] **Step 5: Defer commit**

---

## Task 5: Wire /tree argument parsing + leaf switching

**Files:**
- Modify: `examples/pdk_chat_demo/commands/tree_command.cpp`
- Modify: `examples/pdk_chat_demo/input_loop.cpp` (hold SessionManager pointer)
- Test: `tests/test_session_tree_commands.cpp`

- [ ] **Step 1: Write the failing test**

Extend `test_session_tree_commands.cpp` with argument parsing cases:
```cpp
TEST_CASE("/tree with empty arg renders", "[session-tree]") { ... }
TEST_CASE("/tree with short id switches leaf", "[session-tree]") { ... }
TEST_CASE("/tree with ambiguous short id returns error", "[session-tree]") { ... }
```

- [ ] **Step 2: Run test to verify it fails**

Expected: fails because command body only returns stub.

- [ ] **Step 3: Write minimal implementation**

`tree_command.cpp`:
```cpp
spec.handler = [](ToolCallContext& tctx) -> std::string {
  auto& args = tctx.args;
  auto it = args.find("arg");
  if (it == args.end() || it->second.empty()) {
    // render
    return pdk_chat_demo::render_session_tree(g_session_manager, g_current_leaf_id);
  }
  // switch leaf
  auto match = g_session_manager->get_node_by_short_id(it->second);
  if (!match) return "error: ambiguous or unknown node id";
  g_current_leaf_id = match->id;
  return "switched to leaf " + match->id;
};
```

Add `g_current_leaf_id` global in `input_loop.cpp` or `tree_command.cpp`.

- [ ] **Step 4: Run test to verify it passes**

Expected: PASS.

- [ ] **Step 5: Defer commit**

---

## Task 6: Wire fork / clone commands and register in main.cpp

**Files:**
- Modify: `examples/pdk_chat_demo/commands/fork_command.cpp`
- Modify: `examples/pdk_chat_demo/commands/clone_command.cpp`
- Modify: `examples/pdk_chat_demo/main.cpp`
- Test: `tests/test_pdk_chat_session_tree.cpp`

- [ ] **Step 1: Write the failing test**

Create `tests/test_pdk_chat_session_tree.cpp` with E2E sequence using real SessionManager in a temp directory.

- [ ] **Step 2: Run test to verify it fails**

Expected: fails because fork/clone not implemented.

- [ ] **Step 3: Write minimal implementation**

`fork_command.cpp`:
```cpp
spec.handler = [](ToolCallContext& tctx) -> std::string {
  if (g_command_coordinator == nullptr || g_session_manager == nullptr) return "error: not initialized";
  auto node_id = tctx.args.count("node_id") ? tctx.args.at("node_id") : g_current_leaf_id;
  agenticdsl::ToolMetadata meta;
  meta.name = "session_fork";
  meta.category = agenticdsl::ToolCategory::Workflow;
  tctx.caller_layer = "workflow";
  auto r = g_command_coordinator->execute(meta, tctx, {{"node_id", node_id}, {"branch_name", "fork-" + std::to_string(now_ms())}});
  if (!r.ok) return "error: " + r.meta.dump();
  auto new_branch = r.data.value("branch_id", "");
  g_current_leaf_id = g_session_manager->get_branch_leaf(new_branch);
  return "Forked to branch " + new_branch + " (auto-switched)";
};
```

`clone_command.cpp`:
```cpp
spec.handler = [](ToolCallContext& tctx) -> std::string {
  if (g_command_coordinator == nullptr || g_session_manager == nullptr) return "error: not initialized";
  auto branch_id = tctx.args.count("branch_id") ? tctx.args.at("branch_id") : g_current_branch_id;
  agenticdsl::ToolMetadata meta;
  meta.name = "session_clone";
  meta.category = agenticdsl::ToolCategory::Workflow;
  tctx.caller_layer = "workflow";
  auto r = g_command_coordinator->execute(meta, tctx, {{"branch_id", branch_id}});
  if (!r.ok) return "error: " + r.meta.dump();
  auto new_session = r.data.value("session_id", "");
  return "Cloned to session " + new_session + " (use --session " + new_session + " to switch)";
};
```

In `main.cpp`, register all commands via `register_default_commands()` and set global pointers after constructing `SessionManager`/`ToolCoordinator`.

- [ ] **Step 4: Run test to verify it passes**

Expected: PASS.

- [ ] **Step 5: Defer commit**

---

## Task 7: Zero hardcode audit + input loop regression

**Files:**
- Modify: `examples/pdk_chat_demo/main.cpp`
- Create: `tests/test_main_tree_hardcode_audit.cpp`
- Test: `tests/test_pdk_chat_session_tree.cpp`

- [ ] **Step 1: Write the failing test**

Create `tests/test_main_tree_hardcode_audit.cpp`:
```cpp
#include <catch_amalgamated.hpp>
#include <cstdlib>

TEST_CASE("main.cpp has no hardcoded tree/fork/clone branches", "[session-tree]") {
  const char* cmd =
      "grep -nE '\"\\/(tree|fork|clone)' examples/pdk_chat_demo/main.cpp | "
      "grep -v 'input.front() ==' | grep -v 'starts_with(\"/\")'";
  int rc = std::system(cmd);
  REQUIRE(rc != 0);
}
```

- [ ] **Step 2: Run test to verify it fails**

Expected: may fail if main.cpp still has inline registrations.

- [ ] **Step 3: Write minimal implementation**

Ensure main.cpp uses `register_default_commands()` and no `"/tree"`, `"/fork"`, `"/clone"` string literals except in comments.

- [ ] **Step 4: Run test to verify it passes**

Expected: PASS.

- [ ] **Step 5: Defer commit**

---

## Task 8: Full ctest regression and docs sync

**Files:**
- Modify: `docs/active-status.md`
- Modify: `openspec/changes/session-tree-commands/tasks.md`

- [ ] **Step 1: Run full ctest**

Run: `cmake --build build && ctest --output-on-failure -j$(nproc)`
Expected: all pass except pre-existing `test_cost_tracking_decorator`.

- [ ] **Step 2: Run validation tools**

Run: `openspec validate session-tree-commands --strict`
Run: `tools/adr_lint.py`
Run: `tools/docs_drift_audit.py`
Expected: clean.

- [ ] **Step 3: Update docs/active-status.md**

Append ship note under Phase 6a.

- [ ] **Step 4: Defer commit**
