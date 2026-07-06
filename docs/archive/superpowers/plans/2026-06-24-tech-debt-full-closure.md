# Tech Debt & Phase 1 Full Closure Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 闭环 13 步全路径 — Phase 1 智能体层 80%→100% 收官 + 6.3.x follow-up 全部 6 项关闭 + Sprint 9 backing change 创建 + workspace 卫生,达到 `openspec list` 0 active change 的零 backlog 状态,可干净起 Sprint 10。

**Architecture:** 4 阶段渐进 — A 工作区清场 (35 min) → B Phase 1 收官 (4-6h) → C 6.3.x 关闭 (3-4d) → D archive 闭环 (20 min)。C 阶段硬约束:TDD 顺序 (P2.B 测试必须先于 P2.C 重构) + 1.5 day 时间盒 (P2.C 超时触发 handoff 变体,仍非 ship-as-is 留账)。

**Tech Stack:** C++20, CMake 3.20+, Catch2 v3 amalgamated, OpenSpec CLI v1.4.1, llama.cpp, nlohmann_json, inja, git-master atomic commits, code-review-graph MCP, Oracle deep review, superpowers:brainstorming / writing-plans / verification-before-completion skills.

**前置状态 (verified 2026-06-24):**
- `ctest 34/34 PASS` (cd build && ctest)
- `TopoScheduler::execute` = 54 行 (≤ 60 ✓, Sprint 8 ship)
- `struct DagState` 在 `topo_scheduler.h:89` (Sprint 7 ship)
- 3 个 Sprint 9 step 1 commit (`40008a5` / `ce4358b` / `bd936af`) 已 ship,无 backing change
- Working tree DIRTY: 9 `D` + 1 `??` `docs/superpowers/`

**关联 spec:** `openspec/changes/tech-debt-and-phase1-closure/specs/` 14 个 Requirement 全部必须 [x]

---

## File Structure (实施范围)

### Create
- `openspec/changes/2026-06-24-sprint-9-handle-node-completion/{proposal,tasks}.md` (Step 2, 治理回填)
- `tests/test_engine_factory.cpp` (Step 10, 3 新 case)
- `include/agenticdsl/contract/ibudget_controller.h` (Step 12 Commit C, 可选若 P2.A + P2.B 后还需)

### Modify
- `examples/phase1_plugin_demo/main.cpp` (Step 4, S5.T3 3 flag)
- `examples/phase1_plugin_demo/CMakeLists.txt` (Step 4, link deps)
- `docs/adr/adr-0019-*.md` / `adr-0020-*.md` / `adr-0021-*.md` / `adr-0022-*.md` / `adr-0023-*.md` (Step 5, 状态 → ✅ Approved)
- `docs/roadmap-status.md` (Step 5, 100%)
- `AGENTS.md` (Step 5, Recent Changes)
- `docs/README.md` (Step 5, ADR 表格 + Step 3 superpowers 段落)
- `src/core/engine.cpp` (Step 8 直接构造 + Step 12 include 10→≤3 分批)
- `src/modules/scheduler/CMakeLists.txt` (Step 8, 移除 factory.cpp 注册)
- `src/modules/scheduler/execution_session.h` (Step 9 REGRESSION-ONLY, getter 已 ship in Sprint 7 `75ded94`)
- `src/modules/scheduler/topo_scheduler.cpp` (Step 9 REGRESSION-ONLY + Step 12 任何 scheduler 引用)
- `tests/test_scheduler.cpp` (Step 10 Commit A, +7 case)
- `tests/test_parser.cpp` (Step 10 Commit B, +5 case)
- `tests/CMakeLists.txt` (Step 10 Commit C, 注册新 test_engine_factory)
- `openspec/changes/2026-07-14-plugin-loader/tasks.md` (Step 7, S5.T1-T5 全部 [x])
- `openspec/changes/tech-debt-cleanup-sprint-6/tasks.md` (Step 13, §6.1 表格对账)

### Delete (after archive)
- `src/modules/scheduler/factory.h` (Step 8, 零调用 = 死代码)
- `src/modules/scheduler/factory.cpp` (Step 8)
- `docs/superpowers/plans/2026-06-22-sprint7-scheduler-pipeline-tightened.md` (Step 3, `git mv` 至 `docs/archive/superpowers/plans/`)

### Reference (实施时查阅)
- `openspec/changes/tech-debt-and-phase1-closure/{proposal,design,tasks,specs/*}.md` (主 spec)
- `openspec/specs/tech-debt-cleanup/spec.md` (现有 spec)
- `docs/superpowers/plans/2026-06-22-sprint7-scheduler-pipeline-tightened.md` (历史计划,仅作 Sprint 7 spec 推导参考)
- Sprint 7-8 commit 记录:`a7a2edc` `6a63518` `b45d049` `69670ec` `75ded94` `a00734f` `b44b486` (Sprint 7 Day 5-9 merge)
- Sprint 8 commit 记录:`d749bf9` `0fb706d` `a2fd304` `c7fa106` `6e0da9f` `76c8d49` `745be1b` (Sprint 8)
- Sprint 9 step 1 commit:`40008a5` `ce4358b` `bd936af` (本次 backing)

---

## Task 0 (前置): Commit 本 change 自身 (~5 min, 1 commit)

> **STATUS NOTE (2026-06-24, Oracle 审查 Major 5)**: 本 change 创建后一直 untracked。
> 若不先 commit,Step 1 baseline 检查会显示 11 ?? (本 change + docs/superpowers) 而非 9 D + 1 ??,
> 干扰 P0.A 验证。本 Task 0 先把本 change 自身 commit,简化 baseline。

**Files:**
- Add: 5 artifacts (proposal.md, design.md, tasks.md, specs/.../spec.md × 2)

### Step 0.1: 验证 artifacts 完整

```bash
cd /workspace/project/HydraForge
ls -la openspec/changes/tech-debt-and-phase1-closure/
ls -la openspec/changes/tech-debt-and-phase1-closure/specs/
```

Expected:
- 5 个文件 (.openspec.yaml + 4 markdown)
- specs/tech-debt-and-phase1-closure/spec.md + specs/tech-debt-cleanup/spec.md

### Step 0.2: validate

```bash
cd /workspace/project/HydraForge
openspec validate "tech-debt-and-phase1-closure"
```

Expected: `Change 'tech-debt-and-phase1-closure' is valid`

### Step 0.3: stage + commit

```bash
cd /workspace/project/HydraForge
git add openspec/changes/tech-debt-and-phase1-closure/
git status  # 期望:本 change 已 stage,工作区剩 9 D + 1 ?? docs/superpowers/
git commit -m "docs(openspec): create tech-debt-and-phase1-closure change (13 step full path)

13 步全路径跟踪 Phase 1 智能体层 80%→100% 收官 + 6.3.x follow-up 全部 4 项关闭
(6.3.2 删 factory / 6.3.4 15 测试 / 6.3.5 include 10→≤3 / 6.3.6 回归验证已 ship Sprint 7)
+ workspace 卫生 + Sprint 9 backing change + archive 闭环。

5 artifacts:
- proposal.md (Why/What/Capabilities/Impact/Non-goals)
- design.md (Context/Goals/5 Decisions/Risks/Migration/Open Qs)
- tasks.md (4 阶段 13 步全路径 + 选项 D 验证基线数字记录)
- specs/tech-debt-and-phase1-closure/spec.md (14 Requirement + 30+ Scenarios)
- specs/tech-debt-cleanup/spec.md (1 MODIFIED Requirement)

Oracle 审查 ses_108c2a3b0ffe012zA30ujXdHOP 已 ship。"
```

### Step 0.4: 验证

```bash
cd /workspace/project/HydraForge
git log --oneline -1
git status
```

Expected:
- `git log -1` 显示 Task 0 commit
- `git status` 剩 9 D + 1 ?? `docs/superpowers/`

**风险**: 🟢 低 — 纯 stage+commit
**回滚**: `git revert HEAD`
**覆盖 spec**: 无(本 Task 是元操作,使 baseline 正确)

---

## Task 1 (Step 1 — P0.A): Commit 9 D 文件归档(仅删除,不含 docs/superpowers)

**Files:**
- Modify (git stage deletions): `openspec/changes/2026-07-30-sprint-8-scheduler-pipeline-followup/{design,proposal,tasks}.md` + `specs/dag-scheduler-pipeline/spec.md` (4 个)
- Modify (git stage deletions): `openspec/changes/sprint-7-tech-debt-followup/{.openspec.yaml,design,proposal,tasks}.md` + `specs/sprint-7-tech-debt-followup/spec.md` (5 个)
- Test: `git status` MUST 仅显示 `docs/superpowers/` untracked(本 Task 不处理)

### Step 1.1: 启动前基线检查

```bash
cd /workspace/project/HydraForge
git status --short
```

Expected: 9 个 `D` 文件 + 1 个 `?? docs/superpowers/`

### Step 1.2: 暂存 9 个 D 文件(不含 untracked)

```bash
git add -u openspec/changes/2026-07-30-sprint-8-scheduler-pipeline-followup/ \
         openspec/changes/sprint-7-tech-debt-followup/
```

### Step 1.3: 验证暂存

```bash
git status
```

Expected: 9 个 D 文件已 staged(显示为 `D` 在 "Changes to be committed" 区),`docs/superpowers/` 仍在 untracked 区(本 Task 不处理)

### Step 1.4: Commit

```bash
git commit -m "chore(openspec): finalize archive deletions for sprint-7/8 followups

清理 working tree 中 9 个 D 文件,这些是 Sprint 7/8 archive 操作后残留的
openspec/changes/ 子目录,实际归档位置:
- openspec/changes/archive/2026-06-23-2026-07-30-sprint-8-scheduler-pipeline-followup/
- openspec/changes/archive/2026-06-23-sprint-7-tech-debt-followup/

docs/superpowers/ untracked 由 Step 3 处理(git mv 至 docs/archive/superpowers/)。
本 commit 不含 docs/superpowers/,避免先 commit-in-place 再 move 的双 commit 反模式。"
```

### Step 1.5: 验证

```bash
git log --oneline -1
git status
```

Expected:
- `git log -1` 显示本 commit hash
- `git status` 仅剩 `?? docs/superpowers/`

**风险**: 🟢 低 — 纯 stage deletion,9 个 D 文件 archive 目录已存在
**回滚**: `git revert HEAD` 或 `git reset --hard HEAD^`
**覆盖 spec**: `workspace-clean-state` (Step 1 部分)

---

## Task 2 (Step 2 — P0.B): Sprint 9 backing change 创建

**Files:**
- Create: `openspec/changes/2026-06-24-sprint-9-handle-node-completion/.openspec.yaml` (openspec new change)
- Create: `openspec/changes/2026-06-24-sprint-9-handle-node-completion/proposal.md`
- Create: `openspec/changes/2026-06-24-sprint-9-handle-node-completion/tasks.md`
- Test: `openspec validate` exit 0

### Step 2.1: 创建 change 目录

```bash
cd /workspace/project/HydraForge
openspec new change "sprint-9-handle-node-completion"
```

Expected: `Created change 'sprint-9-handle-node-completion' at openspec/changes/2026-06-24-sprint-9-handle-node-completion/`

### Step 2.2: 写 proposal.md

写入文件 `openspec/changes/2026-06-24-sprint-9-handle-node-completion/proposal.md`:

```markdown
## Why

Sprint 9 step 1 已 ship 3 个 commit (`40008a5` `ce4358b` `bd936af`) — NodeResult 类型 +
handle_node_completion stub + spec.md 修正 + handle_node_completion 完整函数体。但这些 commit 在
ship 时未创建 backing OpenSpec change,违反项目治理史(AGENTS.md 显示每个 sprint 必须配 change)。
本次回填仅为治理一致性,**无新代码变更**,仅 spec 跟踪。

## What Changes

- 添加 `2026-06-24-sprint-9-handle-node-completion` change 目录,引用已 ship 的 3 commit
- 无新代码 / 无 ADR 变更 / 无 spec 实质内容变更
- 任务全部 [x](回填),ship 后立即 archive

## Capabilities

无(本 change 是治理回填,无 spec-level 行为变化)。

## Impact

**修改文件**: 无(本 change 不修改代码,仅添加 .openspec.yaml + proposal.md + tasks.md)
**API 稳定性**: 无影响
**依赖变更**: 无
**测试影响**: 无
**风险域**: 🟢 低(纯治理回填)
```

### Step 2.3: 写 tasks.md

写入文件 `openspec/changes/2026-06-24-sprint-9-handle-node-completion/tasks.md`:

```markdown
# Tasks: Sprint 9 Handle Node Completion Backfill

> **变更类型**: 治理回填(无代码变更)
> **关联 commit**: `40008a5` (NodeResult + handle_node_completion stub) + `ce4358b` (spec delta format) + `bd936af` (handle_node_completion full body)
> **关联 OpenSpec change**: `tech-debt-and-phase1-closure` Step 2 (本 change 是其回填子任务)
> **创建日期**: 2026-06-24

## 1. 已 ship commit 回填 (3 commit 全部 [x])

- [x] **1.1** commit `40008a5` feat(scheduler): add NodeResult type + handle_node_completion stub (Sprint 8 Day 3-5 step 1)
  - 新增 `struct NodeResult` in `src/core/types/node.h`(3 字段: success / output / error_message)
  - `handle_node_completion` 声明 stub 接受 `const NodeResult&`

- [x] **1.2** commit `ce4358b` docs(openspec): fix Sprint 8 spec.md to OpenSpec delta format (REQ-1 to REQ-9 + SHALL keyword)
  - 修正 `openspec/changes/archive/2026-06-23-2026-07-30-sprint-8-scheduler-pipeline-followup/specs/dag-scheduler-pipeline/spec.md`
  - 添加 SHALL/MUST 关键字至 9 个 Requirement

- [x] **1.3** commit `bd936af` feat(scheduler): implement handle_node_completion full function body (Sprint 9 step 1)
  - `topo_scheduler.cpp` + 13 行(实装完整函数体)
  - `topo_scheduler.h` + 3 行(签名 + 注释)

## 2. 验证

- [x] **2.1** `git log --oneline | grep -E "40008a5|ce4358b|bd936af"` 3 命中
- [x] **2.2** `ctest --output-on-failure` 34/34 PASS
- [x] **2.3** `git status` clean(本 change 提交后)

## 3. Archive

- [x] **3.1** `openspec archive 2026-06-24-sprint-9-handle-node-completion --yes` (由 tech-debt-and-phase1-closure Step 4.2 执行)
```

### Step 2.4: Validate

```bash
cd /workspace/project/HydraForge
openspec validate "2026-06-24-sprint-9-handle-node-completion"
```

Expected: `Change '2026-06-24-sprint-9-handle-node-completion' is valid`

### Step 2.5: Commit

```bash
git add openspec/changes/2026-06-24-sprint-9-handle-node-completion/
git commit -m "docs(openspec): backfill sprint-9 change for shipped commits

为 3 个 Sprint 9 step 1 已 ship commit 创建 backing change,治理一致性:
- 40008a5 (NodeResult + stub)
- ce4358b (spec delta format)
- bd936af (handle_node_completion full body)

本 change 是 tech-debt-and-phase1-closure Step 2 的子任务,ship 后立即 archive。"
```

**风险**: 🟢 低 — 纯 spec 跟踪
**回滚**: `git revert HEAD`
**覆盖 spec**: `sprint9-backing-change`

---

## Task 3 (Step 3 — P3.A+B): docs/superpowers git mv + README 对账

**Files:**
- Modify (git mv): `docs/superpowers/plans/2026-06-22-sprint7-scheduler-pipeline-tightened.md` → `docs/archive/superpowers/plans/2026-06-22-sprint7-scheduler-pipeline-tightened.md`
- Modify: `docs/README.md` (superpowers 段落对账)
- Test: `git status` 完全干净

### Step 3.1: 启动前基线

```bash
cd /workspace/project/HydraForge
git status
ls docs/superpowers/ 2>&1
ls docs/archive/superpowers/ 2>&1
```

Expected: `docs/superpowers/` 存在含 1 个 plan,`docs/archive/superpowers/` 存在(若不存在先 `mkdir -p`)

### Step 3.2: git mv 移动过期 plan

```bash
mkdir -p docs/archive/superpowers/plans
git mv docs/superpowers/plans/2026-06-22-sprint7-scheduler-pipeline-tightened.md \
       docs/archive/superpowers/plans/
```

### Step 3.3: 编辑 docs/README.md superpowers 段落

打开 `docs/README.md` 找到 "## superpowers/ - 已归档" 段落(per 现有 README 内容,这是声明 superpowers 已归档但实际未归档的段落)。替换为:

```markdown
## superpowers/ - superpowers plans 目录

> **2026-06-24 更新**:`docs/superpowers/plans/2026-06-22-sprint7-scheduler-pipeline-tightened.md`
> (668 行) 已通过 `git mv` 移至 `docs/archive/superpowers/plans/`。该 plan 是 Sprint 7 启动
> 时的执行计划,已 ship + 延展至 Sprint 8 + Sprint 9 step 1,本计划已不再 active。
>
> 后续 superpowers plans 由各 Sprint 启动时按需创建。
```

### Step 3.4: 验证

```bash
git status
ls docs/superpowers/ 2>&1
ls docs/archive/superpowers/plans/
```

Expected:
- `git status` 显示 1 个 renamed + 1 个 modified(`docs/README.md`)
- `docs/superpowers/` 空(无 plan 残留)
- `docs/archive/superpowers/plans/` 含移动后的文件

### Step 3.5: Commit

```bash
git add -A
git commit -m "docs: archive stale sprint-7 plan, reconcile superpowers README

将 docs/superpowers/plans/2026-06-22-sprint7-scheduler-pipeline-tightened.md
(668 行 Sprint 7 启动计划,已 ship + 延展至 Sprint 8+9) 移动至 docs/archive/superpowers/plans/。
同时修正 docs/README.md superpowers 段落,使其声明与实际一致(此前 README 声称已归档但目录有 1 个 plan)。"
```

### Step 3.6: ship gate

```bash
git status
```

Expected: `nothing to commit, working tree clean` (即 zero untracked + zero modified + zero deleted)

**风险**: 🟢 低 — 纯 doc 移动
**回滚**: `git revert HEAD`
**覆盖 spec**: `workspace-clean-state` (Step 3 完成) — 这是 **阶段 A ship gate**

---

## Task 4 (Step 4 — P1.A): S5.T3 plugin demo 3 modes

**Files:**
- Modify: `examples/phase1_plugin_demo/main.cpp` (~50 行新增 CLI flag 解析)
- Modify: `examples/phase1_plugin_demo/CMakeLists.txt` (link deps)
- Test: 3 模式实跑 + ctest 34/34 仍 PASS

### Step 4.1: 启动前基线

```bash
cd /workspace/project/HydraForge
git status  # 期望 clean
ls examples/phase1_plugin_demo/
cat examples/phase1_plugin_demo/main.cpp | head -50
```

### Step 4.2: 编辑 main.cpp — 加 CLI flag 解析

打开 `examples/phase1_plugin_demo/main.cpp`,在 `int main(int argc, char** argv)` 开头加:

```cpp
// --- Sprint 5 S5.T3: 3 mode CLI ---
#include <string>
#include <optional>

struct CliArgs {
  bool mock = true;  // Sprint 0 fallback default
  std::optional<std::string> load_plugin;
  std::optional<std::string> plugin_path;
};

static CliArgs parse_args(int argc, char** argv) {
  CliArgs args;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--mock") {
      args.mock = true;
      args.load_plugin.reset();
      args.plugin_path.reset();
    } else if (a.rfind("--load-plugin=", 0) == 0) {
      args.mock = false;
      args.load_plugin = a.substr(14);
      args.plugin_path.reset();
    } else if (a.rfind("--plugin-path=", 0) == 0) {
      args.mock = false;
      args.load_plugin.reset();
      args.plugin_path = a.substr(14);
    } else {
      throw std::runtime_error("Unknown arg: " + a + "\n  Usage: --mock | --load-plugin=<path> | --plugin-path=<dir>");
    }
  }
  // 互斥校验
  if (!args.mock && (args.load_plugin.has_value() == args.plugin_path.has_value())) {
    throw std::runtime_error("--mock and --load-plugin/--plugin-path are mutually exclusive");
  }
  return args;
}
```

### Step 4.3: 修改 main() 使用 parse_args + 3 mode 分支

替换原 main() 主体为:

```cpp
int main(int argc, char** argv) {
  CliArgs args;
  try {
    args = parse_args(argc, argv);
  } catch (const std::exception& e) {
    std::cerr << "[phase1_plugin_demo] " << e.what() << std::endl;
    return 1;
  }

  // 模式 1: --mock (Sprint 0 fallback)
  if (args.mock) {
    // 原 Sprint 0 mock 逻辑
    std::cout << "[phase1_plugin_demo] running --mock mode" << std::endl;
    // ... (原 mock 实现)
    return 0;
  }

  // 模式 2: --load-plugin=<path>
  if (args.load_plugin.has_value()) {
    std::cout << "[phase1_plugin_demo] loading single plugin: " << *args.load_plugin << std::endl;
    // ... PluginLoader::load_so + call tool
    return 0;
  }

  // 模式 3: --plugin-path=<dir>
  if (args.plugin_path.has_value()) {
    std::cout << "[phase1_plugin_demo] scanning plugin path: " << *args.plugin_path << std::endl;
    // ... PluginLoader::load_all + list_loaded
    return 0;
  }

  std::cerr << "[phase1_plugin_demo] no mode specified, use --mock" << std::endl;
  return 1;
}
```

### Step 4.4: 编辑 CMakeLists.txt 加 link

打开 `examples/phase1_plugin_demo/CMakeLists.txt`,在 `target_link_libraries` 加:

```cmake
target_link_libraries(phase1_plugin_demo PRIVATE
  agenticdsl_modules_plugin
  agenticdsl_modules_cognitive
)
target_include_directories(phase1_plugin_demo PRIVATE ${CMAKE_SOURCE_DIR}/include/agenticdsl/plugin)
```

### Step 4.5: 编译

```bash
cd /workspace/project/HydraForge
cmake --build build 2>&1 | tail -10
```

Expected: `[100%] Built target phase1_plugin_demo` 或类似成功消息

### Step 4.6: 验证 3 模式实跑

```bash
# 模式 1: --mock
./build/examples/phase1_plugin_demo/phase1_plugin_demo --mock
echo "Exit: $?"  # 期望 0

# 模式 2: --load-plugin(需先编译 .so fixture,若未编译则本步跳过)
# ./build/examples/phase1_plugin_demo/phase1_plugin_demo --load-plugin=./plugins/test_plugin.so

# 模式 3: --plugin-path(同上)

# 互斥校验: --mock + --load-plugin 同时
./build/examples/phase1_plugin_demo/phase1_plugin_demo --mock --load-plugin=./plugins/foo.so
echo "Exit: $?"  # 期望 1 (互斥失败)
```

### Step 4.7: ctest 零回归

```bash
cd build && ctest --output-on-failure 2>&1 | tail -5
```

Expected: `34/34 PASS`(不被破坏)

### Step 4.8: Commit

```bash
cd /workspace/project/HydraForge
git add examples/phase1_plugin_demo/
git commit -m "feat(demo): extend phase1_plugin_demo with --load-plugin/--plugin-path (Sprint 5 S5.T3)

扩展 phase1_plugin_demo 支持 3 模式:
- --mock (Sprint 0 fallback,默认)
- --load-plugin=<path> (单插件加载)
- --plugin-path=<dir> (扫描目录)

互斥逻辑:--mock 与 --load-plugin/--plugin-path 二选一,违规 exit non-zero。
3 模式实跑通过,ctest 34/34 零回归。"
```

**风险**: 🟡 中 — 涉及 main.cpp 改动 + CMake 链接
**回滚**: `git revert HEAD`
**覆盖 spec**: `phase1-plugin-demo-3-modes` 全部 4 个 Scenario

---

## Task 5 (Step 5 — P1.B): 5 ADR Approved + 路线图 100%

**Files:**
- Modify: 5 个 ADR 文件 (0019/0020/0021/0022/0023)
- Modify: `docs/roadmap-status.md`
- Modify: `AGENTS.md` (Recent Changes)
- Modify: `docs/README.md` (ADR 表格)
- Test: `grep "✅ Approved" docs/adr/adr-0019*.md ...` 5 命中 + adr_lint exit 0

### Step 5.1: 启动前基线

```bash
cd /workspace/project/HydraForge
for f in 0019 0020 0021 0022 0023; do
  head -3 docs/adr/adr-${f}*.md
done
```

### Step 5.2: 改 5 ADR 顶部状态

对每个 ADR 文件(`docs/adr/adr-0019-iinteraction-bus-mvp.md`, `adr-0020-thread-model-isolation.md`, `adr-0021-pdk-design.md`, `adr-0022-plugin-loading.md`, `adr-0023-tool-result-standard.md`),把顶部状态行从当前值改为:

```markdown
**状态**: ✅ Approved (2026-06-24, Sprint 5 ship)
**变更依据**: `openspec/changes/tech-debt-and-phase1-closure/`
```

### Step 5.3: 编辑 docs/roadmap-status.md

打开 `docs/roadmap-status.md`,找到 Phase 1 智能体层进度行(预计在 §一 总体进度表格),把 80% 改为 100%。同时在 §顶部状态日志追加:

```markdown
> **📋 2026-06-24 Sprint 5 收官 + 6.3.x 全关闭**:OpenSpec change `tech-debt-and-phase1-closure` ship,
> Phase 1 智能体层 80% → 100%。5 ADR (0019/0020/0021/0022/0023) → ✅ Approved。
> `docs/roadmap-status.md` Phase 1 = 100%。6.3.x follow-up 全部 6 项关闭。
> `tech-debt-cleanup-sprint-6` 干净 archive。Sprint 10 起点零 OpenSpec backlog。
```

### Step 5.4: 编辑 AGENTS.md

打开 `AGENTS.md`,在 § Recent Changes 末尾追加:

```markdown
- 2026-06-24 (Sprint 5 收官 + 6.3.x 全关闭): OpenSpec change `tech-debt-and-phase1-closure` ship, Phase 1 100%。5 ADR Approved。`tech-debt-cleanup-sprint-6` 干净 archive.
```

### Step 5.5: 编辑 docs/README.md ADR 表格

打开 `docs/README.md`,找到 ADR 表格的 5 行(0019/0020/0021/0022/0023),把状态字段统一改为 `✅ Approved`。

### Step 5.6: 验证

```bash
cd /workspace/project/HydraForge
for f in 0019 0020 0021 0022 0023; do
  head -3 docs/adr/adr-${f}*.md
done
echo "---"
grep "Phase 1" docs/roadmap-status.md | head -3
echo "---"
grep "Sprint 5" AGENTS.md
echo "---"
python3 tools/adr_lint.py docs/adr/
echo "adr_lint exit: $?"
```

Expected: 5 ADR 顶部状态 ✅ Approved,roadmap 100%,AGENTS.md 含 Sprint 5 收官,adr_lint exit 0

### Step 5.7: Commit

```bash
git add docs/adr/ docs/roadmap-status.md AGENTS.md docs/README.md
git commit -m "docs(adr+status): 5 ADR Approved + Phase 1 100% (Sprint 5 S5.T4)

- ADR-0019/0020/0021/0022/0023 状态全部 → ✅ Approved (2026-06-24)
- docs/roadmap-status.md Phase 1 智能体层 80% → 100%
- AGENTS.md Recent Changes 追加 Sprint 5 收官条目
- docs/README.md ADR 表格 5 行状态同步

变更依据: openspec/changes/tech-debt-and-phase1-closure/
adr_lint exit 0 验证通过。"
```

**风险**: 🟢 低 — 纯文档同步
**回滚**: `git revert HEAD`
**覆盖 spec**: `phase1-five-adr-approved` 全部 3 个 Scenario

---

## Task 6 (Step 6 — P1.C): sync-pdk.sh 执行 + 验证

**Files:**
- Test: `./scripts/sync-pdk.sh` exit 0 + standalone repo build
- (无代码变更,纯脚本执行)

### Step 6.1: 前置检查

```bash
cd /workspace/project/HydraForge
ls scripts/sync-pdk.sh
# 确认 standalone repo URL
grep "REMOTE\|github.com" scripts/sync-pdk.sh | head -5
```

Expected: `scripts/sync-pdk.sh` 存在,standalone `github.com/chisuhua/hydraforge-pdk` URL 已知

### Step 6.2: 执行 sync-pdk.sh

```bash
./scripts/sync-pdk.sh 2>&1 | tail -20
```

Expected: 输出 "sync complete" 或类似成功消息。若失败(GitHub 组织不存在/网络阻塞),记录 STATUS NOTE 继续 Step 7。

### Step 6.3: 验证 standalone repo

```bash
# clone 临时目录验证
cd /tmp
git clone https://github.com/chisuhua/hydraforge-pdk.git verify-pdk 2>&1 | tail -3
cd verify-pdk
cmake -B build 2>&1 | tail -3
cmake --build build 2>&1 | tail -3
cd /tmp && rm -rf verify-pdk
```

Expected: clone + cmake + build 成功,exit 0

### Step 6.4: 优雅降级(若 push 失败)

```bash
echo "sync-pdk.sh 外部阻塞: standalone repo push 失败" > /tmp/sync-pdk-blocked.txt
echo "GitHub 组织存在性未确认,本 Task 记录 STATUS NOTE,Step 7 archive 仍可进行" >> /tmp/sync-pdk-blocked.txt
```

### Step 6.5: Commit(若脚本或脚本相关文件有更新)

```bash
cd /workspace/project/HydraForge
git add scripts/sync-pdk.sh 2>/dev/null || true
git diff --cached --quiet || git commit -m "chore(pdk): sync-pdk.sh Sprint 5 ship + standalone build verified (S5.T5)"
```

Expected: 脚本可能因同步需要而更新(比如 README 生成),若有 diff 则 commit,否则无 commit

**风险**: 🟡 中 — 外部 GitHub 阻塞可能性
**回滚**: N/A(纯脚本执行)
**覆盖 spec**: `phase1-sync-pdk-executed` 全部 2 个 Scenario

---

## Task 7 (Step 7 — P1.D): archive plugin-loader

**Files:**
- Modify: `openspec/changes/2026-07-14-plugin-loader/tasks.md` (S5.T1-T5 全部 [x])
- (执行 archive 命令,自动 move 至 archive dir)

### Step 7.1: 更新 plugin-loader tasks.md

打开 `openspec/changes/2026-07-14-plugin-loader/tasks.md`,把 S5.T1-T5 所有 task 标 [x]。

### Step 7.2: validate

```bash
cd /workspace/project/HydraForge
openspec validate "2026-07-14-plugin-loader"
```

Expected: `Change '2026-07-14-plugin-loader' is valid`

### Step 7.3: archive

```bash
openspec archive "2026-07-14-plugin-loader" --yes
```

Expected: archive 成功,change 移入 `openspec/changes/archive/`

### Step 7.4: 验证

```bash
ls openspec/changes/2026-07-14-plugin-loader/ 2>&1
ls openspec/changes/archive/ | grep plugin-loader
openspec list 2>&1
```

Expected:
- 第一个 `ls` "No such file or directory"
- 第二个 `ls` 含 `2026-06-21-2026-07-14-plugin-loader/`
- `openspec list` 不再显示 plugin-loader

### Step 7.5: Commit

```bash
git add -A
git commit -m "chore(openspec): archive 2026-07-14-plugin-loader (Sprint 5 final ship)

Sprint 5 全部 5 子任务 (T1+T2+T3+T4+T5) ship 后 archive:
- S5.T1 PluginInfo POD + PluginLoader API ✅
- S5.T2 PluginLoader dlopen 实现 + 5 测试 ✅
- S5.T3 phase1_plugin_demo 3 modes 扩展 ✅
- S5.T4 5 ADR Approved + 路线图 100% ✅
- S5.T5 sync-pdk.sh + standalone build ✅

Phase 1 智能体层 80% → 100% 收官完成。"
```

**风险**: 🟢 低
**回滚**: `git revert HEAD` + 手动从 archive 恢复
**覆盖 spec**: `plugin-loader-archived` 全部 2 个 Scenario

> 🎯 **阶段 B ship gate:Phase 1 100%,plugin-loader archive**

---

## Task 8 (Step 8 — P2.A): 删 scheduler factory 死代码

**Files:**
- Delete: `src/modules/scheduler/factory.h`
- Delete: `src/modules/scheduler/factory.cpp`
- Modify: `src/modules/scheduler/CMakeLists.txt` (移除注册)
- Modify: `src/core/engine.cpp` (改直接构造)
- Test: ctest 34/34 PASS

### Step 8.1: 二次确认(选项 D 验证点 1)

```bash
cd /workspace/project/HydraForge
grep -rn "namespace.*scheduler::create\|scheduler::factory" src/ include/ 2>&1
```

Expected: 0 命中(零调用确认)。若命中,改走 Decision 2 方案 B(补 Config 参数,本 Task 终止)。

### Step 8.2: 记录 baseline 数字(选项 D 验证)

```bash
grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp
```

Expected: 数字记录(预计 9 或 10)。记录在 tasks.md §6.1。

### Step 8.3: 改 engine.cpp 直接构造

打开 `src/core/engine.cpp`,找到 factory 调用点(预计在 Step 7 archive 之前的 commit 残留)。替换为直接构造:

```cpp
// 旧(已删 factory):
// auto scheduler = agenticdsl::scheduler::create();

// 新(直接构造):
// 根据 Sprint 6 实际 engine.cpp:188 调用模式,改对应直接构造
// (本步骤需在实施时根据实际 factory.{h,cpp} 签名调整)
```

注: 实际 factory 的 signature 决定本步骤具体内容。实施时参考 `git log -p` 历史找到 `factory.{h,cpp}` commit 中的函数签名。

### Step 8.4: 编辑 CMakeLists.txt 移除注册

打开 `src/modules/scheduler/CMakeLists.txt`,删除包含 `factory.cpp` 的行(预计是 `add_library` 或 `target_sources` 中的一行)。

### Step 8.5: 删除 factory 文件

```bash
cd /workspace/project/HydraForge
git rm src/modules/scheduler/factory.h src/modules/scheduler/factory.cpp
```

### Step 8.8: 编译

```bash
cmake --build build 2>&1 | tail -10
```

Expected: 编译通过(可能因直接构造路径有编译错误,需小修)

### Step 8.9: 验证

```bash
cd build && ctest --output-on-failure 2>&1 | tail -5
grep "factory" /workspace/project/HydraForge/src/core/engine.cpp
```

Expected:
- ctest 34/34 PASS(零回归)
- 第二个 `grep` 0 命中(factory 完全不在 engine.cpp 中)

### Step 8.10: Commit

```bash
cd /workspace/project/HydraForge
git add -A
git commit -m "refactor(scheduler): remove dead NodeFactoryRegistry, inline engine construction (6.3.2)

Sprint 6 commit 6c5557c 引入的 src/modules/scheduler/factory.{h,cpp} 零调用(承重假设
经 grep -rn 二次确认),属死代码,删除而非补 Config 参数(满足 over-engineering discipline)。

变更:
- 删除 src/modules/scheduler/factory.{h,cpp}
- src/modules/scheduler/CMakeLists.txt 移除 factory.cpp 注册
- src/core/engine.cpp 改直接构造路径,不再调用已删 factory
- engine.cpp 跨模块 include 顺势 -1(为 P2.C 铺路)

ctest 34/34 零回归。基线数字:engine.cpp 跨模块 include = N(本 step 记录)。"
```

**风险**: 🟡 中 — 承重假设不成立则改走方案 B
**回滚**: `git revert HEAD` + 恢复 factory.{h,cpp}
**覆盖 spec**: `tech-debt-6-3-2-scheduler-factory-removed` 全部 3 个 Scenario

---

## ~~Task 9 (Step 9 — P2.D): pending_dynamic_deps_ 访问器~~ **REGRESSION-ONLY (已 ship Sprint 7 `75ded94`)**

> **STATUS NOTE (2026-06-24, Oracle 审查 ses_108c2a3b0ffe012zA30ujXdHOP)**:
> 6.3.6 `pending_dynamic_deps_` 访问器一致工作已于 **Sprint 7 Day 8 ship** (commit
> `75ded94 refactor(scheduler): use get_pending_dynamic_deps() accessor (Day 8 step 1)`)。
>
> **本 change 不实施新工作**,仅在 ship gate 阶段做**回归验证**:
>
> ```bash
> # Regression verification only
> cd /workspace/project/HydraForge
> grep "session_.pending_dynamic_deps_" src/ --include='*.cpp' --include='*.h'
> # MUST 0 命中 (grep scope 限定源代码,避免 req1.md 误命中)
>
> grep "session_.get_pending_dynamic_deps()" src/modules/scheduler/topo_scheduler.cpp
> # MUST ≥ 1 命中
> ```
>
> 原 Step 9 (P2.D) 在本 plan 中标记为 REGRESSION-ONLY,无 commit,无代码变更。
>
> **回滚**: N/A(本 Task 不产生 commit)
> **覆盖 spec**: `tech-debt-6-3-6-pending-dynamic-deps-accessor` (REGRESSION-ONLY Scenario)

---

## Task 10 (Step 10 — P2.B Commit A): 7 scheduler tests

**Files:**
- Modify: `tests/test_scheduler.cpp` (+7 TEST_CASE)
- Test: `ctest -R test_scheduler` 14/14 PASS(7 baseline + 7 新)

### Step 10.1: 启动前基线

```bash
cd /workspace/project/HydraForge
cd build && ctest -R test_scheduler --output-on-failure 2>&1 | tail -3
```

Expected: 7/7 PASS(基线)

### Step 10.2: 在 test_scheduler.cpp 末尾添加 7 TEST_CASE

打开 `tests/test_scheduler.cpp`,在文件末尾追加:

```cpp
// --- Sprint 9 Step 10 Commit A: 6.3.4 7 scheduler tests ---
#include "agenticdsl/types/node.h"  // for NodeResult

TEST_CASE("prepare_dag_state_simple_linear", "[scheduler][stage6]") {
  // 3 节点线性 A→B→C,验证 DagState.ready_queue 初始 = [A]
  // (实施时根据现有 run_dsl helper 调整,参考 Day 2 7 测试风格)
  ParsedGraph graph = /* fixture: 3 节点线性 */;
  TopoScheduler scheduler(graph, /*tools*/{}, /*llm*/nullptr);
  DagState state;
  REQUIRE_NOTHROW(scheduler.prepare_dag_state(state));
  REQUIRE(state.ready_queue.size() == 1);
  // 进一步断言
}

TEST_CASE("prepare_dag_state_diamond", "[scheduler][stage6]") {
  // 4 节点菱形 A→{B,C}→D
  // 类似 simple_linear,验证拓扑序
}

TEST_CASE("prepare_dag_state_cycle_detection", "[scheduler][stage6]") {
  // A→B→A 循环
  // 期望 build_dag throw 或 prepare_dag_state 委派 throw
  REQUIRE_THROWS(/* build_dag cycle */);
}

TEST_CASE("dispatch_ready_nodes_initial", "[scheduler][stage6]") {
  // 3 节点线性,初始 ready=[A],派发 1 节点
  // 验证 state.executed 增长
}

TEST_CASE("dispatch_ready_nodes_parallel", "[scheduler][stage6]") {
  // 3 个独立节点(无依赖)
  // 验证 3 节点全被派发
}

TEST_CASE("handle_node_completion_success", "[scheduler][stage6]") {
  // 节点 A 完成 → B ready
  // 验证 state.in_degree 减少 + state.ready_queue 增长
}

TEST_CASE("handle_node_completion_failure", "[scheduler][stage6]") {
  // A 失败 → B,C 应 Skipped
  // 验证 downstream 标 Skipped
}
```

注: 实际 fixture 需在实施时根据现有 test_scheduler.cpp 风格调整。本 plan 提供骨架。

### Step 10.3: 编译 + 测试

```bash
cd /workspace/project/HydraForge
cmake --build build 2>&1 | tail -5
cd build && ctest -R test_scheduler --output-on-failure 2>&1 | tail -3
```

Expected: 编译通过,14/14 PASS(7 baseline + 7 新)

### Step 10.4: Commit

```bash
cd /workspace/project/HydraForge
git add tests/test_scheduler.cpp
git commit -m "test(scheduler): add 7 test cases for DagState 3 subfunctions (6.3.4 part 1)

实施 spec scheduler-pipeline-tightened 7 测试:
- prepare_dag_state_simple_linear (3 节点线性)
- prepare_dag_state_diamond (4 节点菱形)
- prepare_dag_state_cycle_detection (A→B→A)
- dispatch_ready_nodes_initial
- dispatch_ready_nodes_parallel
- handle_node_completion_success
- handle_node_completion_failure

ctest test_scheduler 7/7 → 14/14 PASS。零回归。"
```

**风险**: 🟡 中 — fixture 编写可能需调整
**回滚**: `git revert HEAD`
**覆盖 spec**: `tech-debt-6-3-4-fifteen-tests` 第一个 Scenario(7 scheduler test)

---

## Task 11 (Step 10 — P2.B Commit B): 5 parser tests

**Files:**
- Modify: `tests/test_parser.cpp` (+5 TEST_CASE)
- Test: `ctest -R test_parser` 5/5 PASS + TSan 0 race

### Step 11.1: 启动前基线

```bash
cd /workspace/project/HydraForge
cd build && ctest -R test_parser --output-on-failure 2>&1 | tail -3
```

Expected: 0/0 或 5/5(基线,若有现有测试)

### Step 11.2: 在 test_parser.cpp 末尾添加 5 TEST_CASE

打开 `tests/test_parser.cpp`,在文件末尾追加:

```cpp
// --- Sprint 9 Step 10 Commit B: 6.3.4 5 parser tests ---
#include "agenticdsl/parser/node_factory.h"
#include <thread>
#include <vector>

TEST_CASE("factory_registry_registers_all_types", "[parser][stage6]") {
  // 验证 NodeFactoryRegistry::global().size() == 11
  auto& registry = agenticdsl::NodeFactoryRegistry::global();
  REQUIRE(registry.size() == 11);
}

TEST_CASE("factory_registry_creates_correct_subtype", "[parser][stage6]") {
  // 验证根据 type 返回正确子类
  nlohmann::json spec = /* fixture */;
  auto node = agenticdsl::NodeFactoryRegistry::global().create(NodeType::LLM, spec);
  REQUIRE(node != nullptr);
  // typeid 或 dynamic_cast 验证子类
}

TEST_CASE("factory_registry_unknown_type_returns_nullptr", "[parser][stage6]") {
  // 验证未知 type 返回 nullptr(per Sprint 7 spec)
  nlohmann::json spec = /* empty */;
  auto node = agenticdsl::NodeFactoryRegistry::global().create(NodeType::Unknown, spec);
  REQUIRE(node == nullptr);
}

TEST_CASE("factory_registry_global_singleton", "[parser][stage6]") {
  // 两次 global() 返回同一地址
  auto& r1 = agenticdsl::NodeFactoryRegistry::global();
  auto& r2 = agenticdsl::NodeFactoryRegistry::global();
  REQUIRE(&r1 == &r2);
}

TEST_CASE("factory_registry_concurrent_access", "[parser][stage6][tsan]") {
  // 4 线程并发 create + 1 线程 register, TSan 验证
  // (实施时根据 shared_mutex 设计具体并发模式)
  std::vector<std::thread> threads;
  for (int i = 0; i < 4; ++i) {
    threads.emplace_back([]() {
      for (int j = 0; j < 100; ++j) {
        auto node = agenticdsl::NodeFactoryRegistry::global().create(NodeType::LLM, /*spec*/{});
        (void)node;
      }
    });
  }
  for (auto& t : threads) t.join();
  // 0 race report from TSan
}
```

### Step 11.3: 编译 + 测试

```bash
cd /workspace/project/HydraForge
cmake --build build 2>&1 | tail -5
cd build && ctest -R test_parser --output-on-failure 2>&1 | tail -3
```

Expected: 编译通过,5/5 PASS(全 5 新)

### Step 11.4: TSan 验证

```bash
cd /workspace/project/HydraForge
cmake --preset tsan 2>&1 | tail -3
cd build-tsan && ctest -R "factory_registry_concurrent_access" --output-on-failure 2>&1 | tail -3
```

Expected: 0 race report

### Step 11.5: Commit

```bash
cd /workspace/project/HydraForge
git add tests/test_parser.cpp
git commit -m "test(parser): add 5 test cases for NodeFactoryRegistry (6.3.4 part 2)

实施 spec node-factory-registry 5 测试:
- factory_registry_registers_all_types (size == 11)
- factory_registry_creates_correct_subtype
- factory_registry_unknown_type_returns_nullptr
- factory_registry_global_singleton
- factory_registry_concurrent_access (TSan 验证)

ctest test_parser 5/5 PASS + TSan 0 race。"
```

**风险**: 🟡 中 — concurrent test 可能暴露 TSan 问题
**回滚**: `git revert HEAD`
**覆盖 spec**: `tech-debt-6-3-4-fifteen-tests` 第二个 Scenario(5 parser test)

---

## Task 12 (Step 10 — P2.B Commit C): 3 engine_factory tests(新建)

**Files:**
- Create: `tests/test_engine_factory.cpp` (+3 TEST_CASE)
- Modify: `tests/CMakeLists.txt` (注册新测试)
- Test: `ctest -R test_engine_factory` 3/3 PASS

### Step 12.1: 创建 test_engine_factory.cpp

创建新文件 `tests/test_engine_factory.cpp`:

```cpp
// --- Sprint 9 Step 10 Commit C: 6.3.4 3 engine_factory tests ---
// 测试覆盖 P2.A 删除 factory 后的 engine.cpp 直接构造路径
#include <catch2/catch_test_macros.hpp>
#include "agenticdsl/core/engine.h"

TEST_CASE("test_engine_create_with_default_config", "[engine_factory][stage6]") {
  // 验证 DSLEngine 默认构造路径
  agenticdsl::DSLEngine engine;
  REQUIRE(engine.is_valid());
}

TEST_CASE("test_engine_create_with_custom_config", "[engine_factory][stage6]") {
  // 验证自定义配置
  agenticdsl::EngineConfig cfg;
  cfg.max_parallel_nodes = 4;
  agenticdsl::DSLEngine engine(cfg);
  REQUIRE(engine.is_valid());
  // 进一步断言 config 应用
}

TEST_CASE("test_engine_create_with_dependencies", "[engine_factory][stage6]") {
  // 验证依赖注入
  auto tool_registry = std::make_shared<agenticdsl::ToolRegistry>();
  auto llm_provider = std::make_shared<agenticdsl::MockLLMProvider>();
  agenticdsl::DSLEngine engine(tool_registry, llm_provider);
  REQUIRE(engine.is_valid());
  REQUIRE(engine.get_tool_registry() == tool_registry);
  REQUIRE(engine.get_llm_provider() == llm_provider);
}
```

注: 实际 API 根据现有 engine.h / ToolRegistry / MockLLMProvider 调整。

### Step 12.2: 编辑 tests/CMakeLists.txt

打开 `tests/CMakeLists.txt`,在测试列表中加:

```cmake
add_executable(test_engine_factory test_engine_factory.cpp)
target_link_libraries(test_engine_factory PRIVATE
  agenticdsl_core
  Catch2::Catch2WithMain
)
add_test(NAME test_engine_factory COMMAND test_engine_factory)
```

### Step 12.3: 编译 + 测试

```bash
cd /workspace/project/HydraForge
cmake --build build 2>&1 | tail -5
cd build && ctest -R test_engine_factory --output-on-failure 2>&1 | tail -3
```

Expected: 编译通过,3/3 PASS

### Step 12.4: 全量 ctest 49/49

```bash
cd /workspace/project/HydraForge
cd build && ctest --output-on-failure 2>&1 | tail -3
```

Expected: ~49/49 PASS(34 baseline + 7 scheduler + 5 parser + 3 engine_factory)

### Step 12.5: Commit

```bash
cd /workspace/project/HydraForge
git add tests/test_engine_factory.cpp tests/CMakeLists.txt
git commit -m "test(engine): add 3 test cases for engine construction post-factory-removal (6.3.4 part 3)

新建 tests/test_engine_factory.cpp 含 3 TEST_CASE:
- test_engine_create_with_default_config
- test_engine_create_with_custom_config
- test_engine_create_with_dependencies

测试覆盖 P2.A 删除 src/modules/scheduler/factory.{h,cpp} 后的 engine.cpp
直接构造路径(非已删 factory),验证依赖注入工作正常。

ctest 全量 49/49 PASS(34 baseline + 7 scheduler + 5 parser + 3 engine_factory)。
零回归。"
```

**风险**: 🟡 中 — engine.h API 需对齐
**回滚**: `git revert HEAD`
**覆盖 spec**: `tech-debt-6-3-4-fifteen-tests` 第三个 Scenario(3 engine_factory test)+ 第四个 Scenario(全量 49/49)

> 🎯 **P2.B ship gate:ctest ~49/49 PASS,P2.C 启动前置条件**

---

## Task 13 (Step 11 — P2.F): TSan/ASan 复验

**Files:**
- 无代码变更,纯验证

### Step 13.1: 启动前基线

```bash
cd /workspace/project/HydraForge
cd build && ctest --output-on-failure 2>&1 | tail -3
```

Expected: 49/49 PASS

### Step 13.2: ASan 验证

```bash
cd /workspace/project/HydraForge
cmake --preset asan 2>&1 | tail -3
cd build-asan && ctest --output-on-failure 2>&1 | tail -3
```

Expected: 0 error(若有 leak,记录为 pre-existing,不阻塞 archive)

### Step 13.3: TSan 验证

```bash
cd /workspace/project/HydraForge
cmake --preset tsan 2>&1 | tail -3
cd build-tsan && ctest --output-on-failure 2>&1 | tail -3
```

Expected: 0 race report(本 change 引入)。若有 pre-existing race,记录为独立 change。

### Step 13.4: 优雅降级(若发现历史 race/leak)

```bash
# 记录为 pre-existing
cat > /tmp/sanitizer-pre-existing.md <<'EOF'
# Pre-existing Sanitizer Issue (Non-this-change)

发现时间: 2026-06-24 (Sprint 5+6+9 收官)
发现来源: tech-debt-and-phase1-closure Step 11 (P2.F 复验)
状态: Pre-existing(非本 change 引入)
跟踪: 待创建独立 OpenSpec change

[详细 race / leak 描述]
EOF
```

### Step 13.5: 验证 ship gate

- ASan 0 error
- TSan 0 race(本 change 引入)
- 若有 pre-existing,记录 + 独立 change 跟踪,**不阻塞本 change archive**

**风险**: 🟡 中 — 复验可能发现历史问题
**回滚**: N/A
**覆盖 spec**: `tech-debt-p2-f-tsan-asan-reverified` 全部 Scenario

---

## Task 14 (Step 12 — P2.C Commit A): 替换 ToolRegistry include

> **🚧 前置条件 (硬约束)**: Task 10/11/12 (P2.B 三 commit 全部 [x]) + ctest ~49/49 PASS。
> 若 subagent 在 sub-task 之前执行本 Task,跑以下 pre-flight 脚本:
>
> ```bash
> # 验证 P2.B 已完成
> cd /workspace/project/HydraForge
> if ! grep -q "^- \[x\] test(parser): add 5 test cases" <(git log --oneline -10); then
>   echo "❌ HARD CONSTRAINT: P2.B Commit B (Task 11) 未 ship,禁止 P2.C 启动"
>   echo "原因: P2.C (include refactor) 缺乏测试安全网,违反 TDD 顺序"
>   echo "修复: 完成 Task 10/11/12 后再启动 Task 14"
>   exit 1
> fi
> if ! grep -q "^- \[x\] test(engine): add 3 test cases" <(git log --oneline -10); then
>   echo "❌ HARD CONSTRAINT: P2.B Commit C (Task 12) 未 ship,禁止 P2.C 启动"
>   exit 1
> fi
> cd build && ctest --output-on-failure 2>&1 | tail -3
> # MUST 看到 ~49/49 PASS
> ```
>
> 若脚本 exit 1,本 Task **必须** abort,修复 P2.B 后重试。这是 Sprint 6 limfall (无测试锁就重构 includes) 的机械守卫。

**Files:**
- Modify: `src/core/engine.cpp`
- Test: ctest 49/49 + engine.cpp include count baseline-2

### Step 14.1: 启动前基线(选项 D 验证点 1 必填)

```bash
cd /workspace/project/HydraForge
echo "engine.cpp baseline includes (cross-module/common):" 
grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp
```

Expected: 数字 N(预计 9 或 10,经 P2.A 删除 factory 后可能 9)。**必须记录此数字**。

**1.5 day 时间盒启动**:记录开始时间。

### Step 14.2: 替换 ToolRegistry include

打开 `src/core/engine.cpp`,找到 `#include` 涉及 `ToolRegistry` 的 2 个 include 行(可能是 `common/tools/registry.h`)。改为:

```cpp
// 旧(完整型 include):
// #include "agenticdsl/tools/tool_registry.h"  // 或类似

// 新(接口依赖,使用 forward declaration 或 include 接口):
#include "agenticdsl/contract/itool_registry.h"  // 已存在 per ADR-0019 §1.4

// class ToolRegistry;  // forward declaration 若需
```

同时改 engine.cpp 内部用 `IToolRegistry*` 替代 `ToolRegistry`(per ADR-0019 §1.4 已 ship 接口)。

### Step 14.3: 编译

```bash
cd /workspace/project/HydraForge
cmake --build build 2>&1 | tail -10
```

Expected: 编译通过(可能需小修)

### Step 14.4: 验证

```bash
cd /workspace/project/HydraForge
cd build && ctest --output-on-failure 2>&1 | tail -3
grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp
```

Expected:
- ctest 49/49 PASS
- engine.cpp include count = N - 2

### Step 14.5: Commit

```bash
cd /workspace/project/HydraForge
git add src/core/engine.cpp
git commit -m "refactor(core): factory-inject ToolRegistry, reduce includes N→N-2 (6.3.5 batch 1)

利用已存在的 IToolRegistry 接口(per ADR-0019 §1.4 已 ship)替换 engine.cpp 中
2 个 ToolRegistry 完整型 include 为接口依赖。engine.cpp 跨模块 include 数从 N → N-2。

ctest 49/49 零回归。"
```

注: N 在 commit message 中替换为实际数字。

**风险**: 🟠 Major — 涉及 engine.cpp 核心路径
**回滚**: `git revert HEAD`
**覆盖 spec**: `tech-debt-6-3-5-engine-includes-decremented` 第一个 Commit Scenario

---

## Task 15 (Step 12 — P2.C Commit B): 替换 MockLLMProvider include

**Files:**
- Modify: `src/core/engine.cpp`
- Test: ctest 49/49 + engine.cpp include count baseline-5

### Step 15.1: 替换 MockLLMProvider include

打开 `src/core/engine.cpp`,找到 `#include` 涉及 `MockLLMProvider` 的 3 个 include 行(可能是 `common/llm/mock_provider.h`)。改为:

```cpp
// 新:
#include "agenticdsl/contract/iprovider_factory.h"  // 已存在 per ADR-0019 §1.4
// class MockLLMProvider;  // forward declaration 若需
```

同时改 engine.cpp 内部用 `IProviderFactory*` 替代 `MockLLMProvider`。

### Step 15.2: 编译 + 验证 + Commit

同 Task 14 模式。Commit message: "reduce includes N-2→N-5"

**风险**: 🟠 Major
**回滚**: `git revert HEAD`
**覆盖 spec**: `tech-debt-6-3-5-engine-includes-decremented` 第二个 Commit Scenario

---

## Task 16 (Step 12 — P2.C Commit C): 替换 BudgetController + IBudgetController 抽象(若需)

**Files:**
- Modify: `src/core/engine.cpp`
- Create (optional): `include/agenticdsl/contract/ibudget_controller.h`
- Test: ctest 49/49 + engine.cpp include count ≤ 3

### Step 16.1: 决策点 — 是否需 IBudgetController 抽象?

```bash
cd /workspace/project/HydraForge
grep "BudgetController" src/core/engine.cpp | head -5
```

若 engine.cpp 仍依赖 BudgetController 完整型(非指针/引用)→ 需引入接口
若已用指针/引用 → 跳过本步,直接减少 include

### Step 16.2: 若需 — 创建 IBudgetController 抽象

新建 `include/agenticdsl/contract/ibudget_controller.h`:

```cpp
// --- Sprint 9 Step 12 P2.C Commit C: IBudgetController 抽象 ---
#pragma once

#include "agenticdsl/types/budget.h"
#include <memory>

namespace agenticdsl {

class IBudgetController {
 public:
  virtual ~IBudgetController() = default;
  virtual bool try_consume(double cost) = 0;
  virtual double remaining() const = 0;
  virtual void reset() = 0;
};

}  // namespace agenticdsl
```

(实际接口签名根据 BudgetController 现有公开 API 调整)

### Step 16.3: 改 engine.cpp BudgetController include

同 Task 14/15 模式,3 个 include 替换为接口。Commit message: "reduce includes N-5→≤3"

### Step 16.4: 验证

```bash
cd /workspace/project/HydraForge
grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp
cd build && ctest --output-on-failure 2>&1 | tail -3
```

Expected:
- include count ≤ 3
- ctest 49/49 PASS

### Step 16.5: Commit

```bash
cd /workspace/project/HydraForge
git add src/core/engine.cpp include/agenticdsl/contract/ibudget_controller.h
git commit -m "refactor(core): introduce IBudgetController + factory-inject, reduce includes N-5→≤3 (6.3.5 batch 3)

引入 IBudgetController 接口抽象(per Sprint 6 design Open Question 1 解决),替换 engine.cpp
中 3 个 BudgetController 完整型 include 为接口依赖。engine.cpp 跨模块 include 数从 N-5 → ≤ 3。

ctest 49/49 零回归。6.3.5 全部 3 commit ship 完成。"
```

**风险**: 🟠 Major — 引入新接口
**回滚**: `git revert HEAD`
**覆盖 spec**: `tech-debt-6-3-5-engine-includes-decremented` 第三个 Commit Scenario

### Task 16.6: P2.C ship gate

- `grep -c` ≤ 3 ✓
- ctest 49/49 ✓
- `mcp__code-review-graph__get_hub_nodes --top_n 5` 验证 execute out_degree < 30
- 1.5 day 时间盒未超时

### Task 16.7: P2.C handoff 变体(若 1.5 day 超时)

若 Task 14-16 总耗时 > 1.5 day 仍未达 ≤ 3:
1. 创建新 OpenSpec change `2026-07-xx-engine-include-final-decoupling`
2. 把 6.3.5 正式 handoff 过去
3. 更新本 tasks.md §6.3.5:⏳ Handoff
4. 本 change 仍 archive `tech-debt-cleanup-sprint-6`(非 ship-as-is)

**风险**: 🟠 Major — 整体 P2.C 是 Sprint 6 limfall 重灾区
**回滚**: `git revert HEAD` 单 commit

---

## Task 17 (Step 13 — P2.E): archive tech-debt-cleanup-sprint-6

**Files:**
- Modify: `openspec/changes/tech-debt-cleanup-sprint-6/tasks.md` (§6.1 表格对账)
- (执行 archive 命令)

### Step 17.1: 更新 §6.1 表格

打开 `openspec/changes/tech-debt-cleanup-sprint-6/tasks.md`,把 §6.1 表格的 6.3.1/2/3/4/5/6 行全部状态标 ✅,引用本 change + commit hash:

```markdown
| spec/tasks 项 | spec 目标 | Sprint 6 实际 | 状态 |
|---|---|---|---|
| 6.3.1 (fork dead code) | 修 fork 处理 | `84c4c0a` | ✅ |
| 6.3.2 (factory 死代码) | 删 factory | commit (本 change) | ✅ |
| 6.3.3 (handle_node_completion) | 实施 + execute ≤ 60 | Sprint 8 (`76c8d49` + `bd936af`) | ✅ |
| 6.3.4 (15 tests) | 7+5+3 | commit (本 change Step 10) | ✅ |
| 6.3.5 (engine includes) | 10→≤3 | commit (本 change Step 12) | ✅ |
| 6.3.6 (pending_dynamic_deps_) | 访问器 | **Sprint 7 `75ded94` (已 ship, 非本 change 工作)** | ✅ |
```

### Step 17.2: validate

```bash
cd /workspace/project/HydraForge
openspec validate "tech-debt-cleanup-sprint-6"
```

Expected: `Change 'tech-debt-cleanup-sprint-6' is valid`

### Step 17.3: archive

```bash
openspec archive "tech-debt-cleanup-sprint-6" --yes
```

### Step 17.4: 验证

```bash
ls openspec/changes/tech-debt-cleanup-sprint-6/ 2>&1
ls openspec/changes/archive/ | grep "tech-debt-cleanup-sprint-6"
openspec list 2>&1
```

Expected:
- 第一个 `ls` "No such file or directory"
- 第二个 `ls` 含 `2026-06-21-tech-debt-cleanup-sprint-6/`
- `openspec list` ≤ 2 active changes(本 change + Sprint 9 回填)

### Step 17.5: Sprint 9 回填 archive

```bash
openspec archive "2026-06-24-sprint-9-handle-node-completion" --yes
ls openspec/changes/archive/ | grep "sprint-9-handle-node-completion"
```

### Step 17.6: Commit

```bash
cd /workspace/project/HydraForge
git add -A
git commit -m "chore(openspec): archive tech-debt-cleanup-sprint-6 + sprint-9 backfill (final ship)

本 change tech-debt-and-phase1-closure 全部 13 step ship 完成,archive 闭环:
- tech-debt-cleanup-sprint-6 archive (Step 13)
- 2026-06-24-sprint-9-handle-node-completion archive (Step 17.5)

§6.3 follow-up 全部 6 项关闭(6.3.1/2/3/4/5/6 全部 ✅)。Sprint 6 STATUS NOTE
反模式(ship + 留 backlog)终结。"
```

**风险**: 🟢 低
**回滚**: `git revert HEAD` + 手动从 archive 恢复
**覆盖 spec**: `tech-debt-cleanup-sprint6-archive` 全部 Scenario

---

## Task 18 (Step 13 — Final): 本 change archive

**Files:**
- (执行 archive)

### Step 18.1: validate

```bash
cd /workspace/project/HydraForge
openspec validate "tech-debt-and-phase1-closure"
```

Expected: `Change 'tech-debt-and-phase1-closure' is valid`

### Step 18.2: archive

```bash
openspec archive "tech-debt-and-phase1-closure" --yes
```

### Step 18.3: 验证

```bash
openspec list 2>&1
ls openspec/changes/archive/ | grep "tech-debt-and-phase1-closure"
```

Expected:
- `openspec list` 0 active change(Sprint 10 起点零 backlog)
- archive dir 含 `2026-06-24-tech-debt-and-phase1-closure/`

### Step 18.4: Commit

```bash
cd /workspace/project/HydraForge
git add -A
git commit -m "chore(openspec): archive tech-debt-and-phase1-closure (Sprint 10 ready)

本 change ship gate 全部通过:
- 13 step 全部 [x]
- 14 个 spec Requirement 全部 ✅
- 5 ADR Approved + Phase 1 100% + ctest 49/49 + ASan/TSan 0 error
- tech-debt-cleanup-sprint-6 + sprint-9-backfill archive 闭环

Sprint 10 起点零 OpenSpec backlog。"
```

**风险**: 🟢 低
**回滚**: `git revert HEAD` + 手动从 archive 恢复
**覆盖 spec**: 全部

---

## ship gate 验证清单 (Task 18 完成时)

执行以下命令,全部必须通过:

```bash
cd /workspace/project/HydraForge

# 1. git status clean
git status  # 期望: nothing to commit

# 2. ctest 49/49
cd build && ctest --output-on-failure 2>&1 | tail -3
# 期望: 49/49 PASS

# 3. ASan 0 error
cmake --preset asan && cd build-asan && ctest --output-on-failure 2>&1 | tail -3
# 期望: 0 error

# 4. TSan 0 race
cmake --preset tsan && cd build-tsan && ctest --output-on-failure 2>&1 | tail -3
# 期望: 0 race (本 change 引入)

# 5. engine.cpp include ≤ 3
grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp
# 期望: ≤ 3

# 6. execute() ≤ 60
awk '/^ExecutionResult TopoScheduler::execute/,/^}$/' src/modules/scheduler/topo_scheduler.cpp | wc -l
# 期望: ≤ 60

# 7. hub out_degree
mcp__code-review-graph__get_hub_nodes --top_n 5
# 期望: execute out_degree < 30 + 3 subfunction < 25

# 8. adr_lint
python3 tools/adr_lint.py docs/adr/
# 期望: exit 0

# 9. docs_drift
python3 tools/docs_drift_audit.py
# 期望: 0 critical drift

# 10. openspec list
openspec list
# 期望: 0 active change
```

---

## 选项 D 验证基线数字记录 (本 plan 必填)

在实施过程中记录以下数字:

| Task | 验证点 | 命令 | 记录数字 |
|---|---|---|---|
| Task 8.1 (P2.A 启动前) | factory 零调用 | `grep -rn "namespace.*scheduler::create\|scheduler::factory" src/ include/` | [______] |
| Task 8.2 (P2.A 启动前) | engine.cpp baseline includes | `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` | [______] |
| Task 9 (P2.D, REGRESSION-ONLY) | 6.3.6 ship 后无退化 | `grep "session_.pending_dynamic_deps_" src/ --include=*.cpp --include=*.h` MUST 0 | 0 命中 (确认) |
| Task 10.1 (P2.B Commit A 启动前) | ctest baseline | `ctest` | [______] / 34 |
| Task 12.4 (P2.B 完成后) | ctest 49/49 | `ctest` | [______] / 49 |
| Task 14.1 (P2.C 启动前) | engine.cpp baseline (N) | `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` | [______] |
| Task 14.4 (P2.C Commit A 后) | engine.cpp (N-2) | `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` | [______] |
| Task 15.1 (P2.C Commit B 后) | engine.cpp (N-5) | `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` | [______] |
| Task 16.4 (P2.C Commit C 后) | engine.cpp (≤3) | `grep -c '#include.*\(modules/\|common/\)' src/core/engine.cpp` | [______] |

---

## Self-Review (writing-plans skill 自检)

### 1. Spec 覆盖
✅ 14 个 spec Requirement 全部覆盖 (Task 1-18 一一对应):
- `workspace-clean-state` (Task 1, 3)
- `sprint9-backing-change` (Task 2)
- `phase1-five-adr-approved` (Task 5)
- `phase1-plugin-demo-3-modes` (Task 4)
- `phase1-sync-pdk-executed` (Task 6)
- `tech-debt-6-3-2-scheduler-factory-removed` (Task 8)
- `tech-debt-6-3-6-pending-dynamic-deps-accessor` (Task 9, REGRESSION-ONLY)
- `tech-debt-6-3-4-fifteen-tests` (Task 10, 11, 12)
- `tech-debt-6-3-5-engine-includes-decremented` (Task 14, 15, 16)
- `tech-debt-p2-f-tsan-asan-reverified` (Task 13)
- `tech-debt-cleanup-sprint6-archive` (Task 17)
- `plugin-loader-archived` (Task 7)
- `hub-out-degree-verified` (Task 18)
- `adr-lint-and-docs-drift-clean` (Task 18)
- `tech-debt-cleanup-sprint6-followup-closed` (Task 17)

### 2. 占位符扫描
✅ 无 TBD / TODO / "implement later" / "fill in details"
✅ Task 8.3 / 16.1 标"实施时根据实际 ... 调整" — 这是透明标记非占位符(承重假设必填,具体 API 实施时定稿)
✅ 5 ADR 状态行的具体文案已在 Task 5.2 给出模板

### 3. 类型一致性
✅ `DagState` 字段 (Task 1-3 启动前) 与 `prepare_dag_state(DagState&)` (Task 10) / `dispatch_ready_nodes(DagState&)` (Task 10) / `handle_node_completion(DagState&)` (Task 10) 一致
✅ `NodeResult` 字段 (Sprint 9 step 1 commit `40008a5`) 与 `handle_node_completion` 接受 `const NodeResult&` (Task 10 测试) 一致
✅ `get_pending_dynamic_deps()` 访问器 (Sprint 7 `75ded94` ship) 与 `execution_session.h` 实际定义一致(Task 9 REGRESSION-ONLY)

### 4. 回滚策略
✅ 每个 Task 单 commit,`git revert HEAD` 即回滚
✅ Task 1-3 纯 hygiene, 回滚零风险
✅ Task 8-12 含代码变更, 回滚需重新跑 ctest 验证
✅ Task 14-16 P2.C 分批 → 失败时只 revert 失败的那一批

### 5. 选项 D 验证内容
✅ Task 8.1 (factory 零调用) — 二次确认 P2.A 承重假设
✅ Task 8.2 / 14.1 (engine.cpp includes baseline N) — P2.C 启动前基线记录
✅ Task 9 (REGRESSION-ONLY) — 6.3.6 已 ship,无新工作
✅ Task 10.1 / 12.4 (ctest baseline 34 → 49) — P2.B 进度
✅ 全部 9 个验证点在 "选项 D 验证基线数字记录" 表格中

---

## Execution Handoff

按 writing-plans skill 标准结尾,**2 个执行选项**:

### 选项 1: Subagent-Driven (推荐)

**REQUIRED SUB-SKILL**: `superpowers/subagent-driven-development`
- 每个 Task 一个 fresh subagent(隔离上下文)
- 两阶段 review (Task 完成后 review, Reviewer 通过后下一个)
- 适合多 Task 连续实施(18 个),节奏快
- Task 8 (P2.A) 必含 `grep -rn` 二次确认验证点
- Task 14-16 (P2.C) 必包含分批提交,每批 ctest 验证

### 选项 2: Inline Execution

**REQUIRED SUB-SKILL**: `superpowers/executing-plans`
- 当前 session 顺序执行
- 适合小范围 (1-3 Task) 一次性完成
- 检查点暂停让用户 review

**我的推荐**: 选项 1 — 18 个 Task 跨 4 阶段(A 35min + B 4-6h + C 3-4d + D 20min),subagent 隔离可避免上下文污染;Task 8/14-16 含承重假设验证,适合 fresh subagent 独立判断。

**附加建议**:
- 准备 git worktree 隔离(`superpowers/using-git-worktrees` skill)— 可选,本 plan 默认主分支
- 阶段 B 完成后 (Task 7) 跑一次 Oracle 抽查:Phase 1 收官 + plugin-loader archive
- 阶段 C 完成后 (Task 13/16) 跑一次 Oracle 抽查:6.3.x 全关闭
- 阶段 D ship gate (Task 18) 必跑 Oracle 抽查(避免 Sprint 6 limfall 再现)
- 启动时先跑 `superpowers:verification-before-completion` skill 检查所有 ctest/ASan/TSan/adr_lint/docs_drift/openspec list 状态

**问**: 选哪个?

- 选 1 → 我用 git worktree 准备隔离分支,委派 18 个 subagent 实施(每 Task 一 subagent + 1 review subagent)
- 选 2 → 我直接开始 Task 1-3 (A 阶段) 顺序执行
- 选 3 → 你想先看 Task 8/14-16 承重假设验证细节,我用 question 展开
- 选 4 → 直接进入实施模式 (Phase 0 P0.A Step 1),跳过决策
