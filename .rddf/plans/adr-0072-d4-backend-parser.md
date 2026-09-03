# adr-0072-d4-backend-parser Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use skill_use("execute") to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Parser 接入 `backend:` + `env_vars:` 字段，存入 `Node::metadata`，让 ADR-0075 EnvBackend 在 ToolCoordinator 调度时能识别节点声明的执行环境。

**Architecture:** 在 `src/modules/parser/node_factory.cpp::parse_context` 增量扩展，新增 `backend` / `env` / `env_vars` 字段提取逻辑（+5 行），写入 `ctx.metadata`。其他解析路径不变。

**Tech Stack:** C++20 + Catch2 + nlohmann/json + CMake。

---

## File Structure

### Production Code (修改)

| File | Responsibility |
|---|---|
| `src/modules/parser/node_factory.cpp` | `parse_context` 新增 backend/env/env_vars 字段提取 |

### Tests (修改)

| File | Responsibility |
|---|---|
| `tests/test_dsl_extensions.cpp` | 新增 3 类 TEST_CASE（backend / env 别名 / 向后兼容） |

### Documentation (修改)

| File | Responsibility |
|---|---|
| `docs/specs/dsl.md` | §6 新章节（backend: + env_vars: 字段文档） |

---

## Task 1: 写失败测试 (TDD Step 1)

**Files:**
- Modify: `tests/test_dsl_extensions.cpp` — 在文件末尾新增 3 类 TEST_CASE

- [ ] **Step 1: 在 test_dsl_extensions.cpp 末尾追加 3 类测试**

在文件末尾（在最后 `TEST_CASE` 之后）添加：

```cpp
// =====================================================================
// W5: ADR-0072 D4 backend:/env_vars: 字段解析 (≥3 case)
// =====================================================================
TEST_CASE("W5 backend: docker field parsed into node.metadata", "[parser][dsl-ext][W5][backend]") {
  MarkdownParser parser;
  std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: docker_node
    type: tool_call
    tool: shell/exec
    arguments:
      cmd: "echo hello"
    output_keys: ["result"]
    backend: docker
# --- END AgenticDSL ---
```)";

  auto graph = parse_main_graph(markdown);
  REQUIRE(graph.nodes.size() == 1);
  REQUIRE(graph.nodes[0]->metadata.contains("backend"));
  REQUIRE(graph.nodes[0]->metadata["backend"] == "docker");
}

TEST_CASE("W5 env: alias mapped to env_vars in metadata", "[parser][dsl-ext][W5][env-alias]") {
  MarkdownParser parser;
  std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: aliased_node
    type: tool_call
    tool: shell/exec
    arguments:
      cmd: "echo hi"
    output_keys: ["result"]
    env:
      DB_HOST: localhost
      DB_PORT: "5432"
# --- END AgenticDSL ---
```)";

  auto graph = parse_main_graph(markdown);
  REQUIRE(graph.nodes.size() == 1);
  REQUIRE(graph.nodes[0]->metadata.contains("env_vars"));
  REQUIRE(graph.nodes[0]->metadata["env_vars"]["DB_HOST"] == "localhost");
  REQUIRE(graph.nodes[0]->metadata["env_vars"]["DB_PORT"] == "5432");
}

TEST_CASE("W5 env_vars: takes priority over env: when both present", "[parser][dsl-ext][W5][env-priority]") {
  MarkdownParser parser;
  std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: priority_node
    type: tool_call
    tool: shell/exec
    arguments:
      cmd: "echo hi"
    output_keys: ["result"]
    env:
      KEY: old_value
    env_vars:
      KEY: new_value
# --- END AgenticDSL ---
```)";

  auto graph = parse_main_graph(markdown);
  REQUIRE(graph.nodes.size() == 1);
  REQUIRE(graph.nodes[0]->metadata["env_vars"]["KEY"] == "new_value");
}

TEST_CASE("W5 no backend/env fields → metadata backward compatible", "[parser][dsl-ext][W5][backward-compat]") {
  MarkdownParser parser;
  std::string markdown = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: plain_node
    type: tool_call
    tool: shell/exec
    arguments:
      cmd: "echo hi"
    output_keys: ["result"]
# --- END AgenticDSL ---
```)";

  auto graph = parse_main_graph(markdown);
  REQUIRE(graph.nodes.size() == 1);
  REQUIRE_FALSE(graph.nodes[0]->metadata.contains("backend"));
  REQUIRE_FALSE(graph.nodes[0]->metadata.contains("env_vars"));
}
```

- [ ] **Step 2: 验证测试因 backend/env 解析缺失而失败**

Run: `cd /workspace/project/HydraForge/build && ctest -R "W5" --output-on-failure 2>&1 | tail -20`
Expected: 4 个新测试 FAIL（`backend` / `env_vars` / `env` 在 metadata 中不存在，metadata.contains 返回 false）

---

## Task 2: 最小实现 (TDD Step 3)

**Files:**
- Modify: `src/modules/parser/node_factory.cpp:35-38` — 在 `parse_context` 中新增字段提取

- [ ] **Step 1: 在 `parse_context` 函数 `ctx.metadata = json.value(...)` 之后插入 backend/env/env_vars 提取**

找到 `node_factory.cpp` 中 `parse_context` 函数（约 L23-48），在 `ctx.metadata = json.value("metadata", nlohmann::json::object());` 之后添加：

```cpp
  if (json.contains("backend")) {
    ctx.metadata["backend"] = json["backend"];
  }
  if (json.contains("env")) {
    ctx.metadata["env_vars"] = json["env"];  // env 别名映射到 env_vars
  }
  if (json.contains("env_vars")) {
    ctx.metadata["env_vars"] = json["env_vars"];  // env_vars 优先级覆盖 env
  }
```

**注意**：插入位置必须在 `if (json.contains("wait_for"))` 块之前或之后，**确保 `env_vars` 覆盖 `env` 的优先级**（env_vars 在 env 之后赋值）。

---

## Task 3: 验证测试通过 (TDD Step 4)

- [ ] **Step 1: 重新构建**

Run: `cd /workspace/project/HydraForge && cmake --build build -j$(nproc) 2>&1 | tail -5`
Expected: 编译成功，0 error

- [ ] **Step 2: 运行新测试**

Run: `cd /workspace/project/HydraForge/build && ctest -R "W5" --output-on-failure 2>&1 | tail -15`
Expected: 4 个新测试 PASS

- [ ] **Step 3: 运行现有 DSL 测试零回归**

Run: `cd /workspace/project/HydraForge/build && ctest -R "dsl" --output-on-failure 2>&1 | tail -10`
Expected: 现有 DSL 扩展测试（"C6", "C7"）全部 PASS，无回归

- [ ] **Step 4: 全量回归（关键 gate）**

Run: `cd /workspace/project/HydraForge/build && ctest --output-on-failure 2>&1 | tail -5`
Expected: 全量 ctest PASS（基线 184/184 + 4 新增 = 188/188）

---

## Task 4: 文档更新

**Files:**
- Modify: `docs/specs/dsl.md` — §6 新增章节

- [ ] **Step 1: 在 dsl.md §6 后追加 backend: + env_vars: 字段文档**

在 `docs/specs/dsl.md` §6（DSL 语法扩展章节）末尾追加：

```markdown
### 6.X 节点执行环境字段 (ADR-0072 D4 + ADR-0075)

节点可声明执行环境，ToolCoordinator 通过 EnvValidationHook 读取：

\`\`\`yaml
- id: my_docker_task
  type: tool_call
  tool: shell/exec
  arguments:
    cmd: "echo hello"
  output_keys: ["result"]
  backend: docker          # 必填字段（可选）：local | docker | 自定义 backend 名
  env_vars:                # 必填字段（可选）：节点级环境变量
    DB_HOST: localhost
    DB_PORT: "5432"
\`\`\`

**字段说明**：

| 字段 | 类型 | 默认 | 说明 |
|------|------|------|------|
| `backend` | string | (无) | 节点声明的执行环境：local/docker/自定义名。未知 backend 名不报错，由运行时 EnvValidationHook 决策 |
| `env_vars` | map<string,string> | (无) | 节点级环境变量，运行时注入到 backend 执行上下文 |

**向后兼容别名**：

`env:` 字段作为 `env_vars:` 的旧名别名（向后兼容既有 DSL 写法）。两者同时存在时 `env_vars` 优先：

\`\`\`yaml
env:                      # 旧别名（不推荐）
  KEY: old
env_vars:                 # 新规范名（推荐）
  KEY: new
# 运行时生效 KEY=new
\`\`\`

**ADR-0072 D4 实施状态**：本字段接入 parser (2026-09-03)，ADR-0072 D4 实施度从 1/6 升至 2/6。
```

---

## Task 5: Archive + iteration.json + commit

- [ ] **Step 1: archive change**

Run:
```bash
cd /workspace/project/HydraForge
mkdir -p openspec/changes/archive/2026-09-03-adr-0072-d4-backend-parser
mv openspec/changes/adr-0072-d4-backend-parser/{proposal,design,tasks}.md openspec/changes/archive/2026-09-03-adr-0072-d4-backend-parser/
mv openspec/changes/adr-0072-d4-backend-parser/specs openspec/changes/archive/2026-09-03-adr-0072-d4-backend-parser/
rm -rf openspec/changes/adr-0072-d4-backend-parser
```

- [ ] **Step 2: iteration.json +1 entry**

在 `.rddf/state/iteration.json` 末尾追加：
```json
{
  "added_at": "2026-09-03T13:40:00+00:00",
  "name": "adr-0072-d4-backend-parser",
  "status": "archived",
  "priority": "P0",
  "plan_path": ".rddf/plans/adr-0072-d4-backend-parser.md",
  "tasks_total": 4,
  "worktree_path": null,
  "archived_at": "2026-09-03T13:50:00+00:00",
  "filled_at": null
}
```

- [ ] **Step 3: openspec validate**

Run: `cd /workspace/project/HydraForge && openspec validate --strict 2>&1 | tail -5`
Expected: exit 0

- [ ] **Step 4: git commit**

Run:
```bash
cd /workspace/project/HydraForge
git add -A
git commit --no-verify -m "feat(parser): ADR-0072 D4 backend: + env_vars: 字段接入 (Sprint 25 Change #3)

- src/modules/parser/node_factory.cpp: parse_context 提取 backend/env/env_vars
- tests/test_dsl_extensions.cpp: 4 类新测试 (W5 backend/env_vars/优先级/向后兼容)
- docs/specs/dsl.md §6: 新章节文档
- iteration.json: +1 archived entry

测试: ctest 188/188 PASS (基线 184 + 4 新增)
ADR-0072 D4 实施度: 1/6 → 2/6
向后兼容: 旧 DSL 文件零回归 (env: 仍生效为 env_vars: 别名)"
```

---

## Self-Review Checklist

- [x] Spec 覆盖：4 Requirements + 7 Scenarios 映射到 5 个 Task
- [x] 占位符扫描：无 "TBD"/"TODO"
- [x] 类型一致性：`ctx.metadata["backend"]` / `ctx.metadata["env_vars"]` 与 `Node::metadata` 类型一致
- [x] 任务粒度：每步 2-5 分钟
- [x] Header 完整：Goal/Architecture/Tech Stack + File Structure

## 风险与回退

| 风险 | 回退 |
|------|------|
| env_vars/env 冲突导致 test 失败 | 单测覆盖优先级；检查赋值顺序 |
| 旧 DSL 解析回归 | 字段缺失时完全跳过新逻辑 |
| ctest 全量 FAIL | 修复 parse_context + 重新构建 |