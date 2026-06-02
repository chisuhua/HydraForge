# Pre-Phase 测试修复实施计划

> **⚠️ 归档说明 (2026-06-03)**：本计划 7 个任务已全部执行，12 个测试已 100% 通过。归档至 `docs/archive/superpowers/plans/`，仅供历史参考。
> - 归档原因：目标已达成，文件无 inbound 引用
> - 实施记录见 `docs/roadmap-status.md` 实施日志
> - 实施 commits：`1148845` (llm), `d6e8ce5` (library), `4ae97d9` (parser), `4b45a5b` (scheduler), `0166f1e` (executor)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 7 个失败的测试，使全部 12 个测试通过，确保基础模块稳定后再启动 Pre-Phase

**Architecture:** 按模块分组，每个失败测试对应一个修复任务，互不依赖可并行执行

**Tech Stack:** Catch2 测试框架, CMake, C++20

---

## 任务总览

| # | 任务 | 失败测试 | 优先级 | 预计工时 |
|---|------|----------|--------|----------|
| 1 | 修复 Parser signature 解析 | test_parser.cpp | P0 | 1-2h |
| 2 | 修复 Scheduler DAG 调度 | test_scheduler.cpp | P0 | 2-3h |
| 3 | 修复 Library 库加载路径 | test_library_loader.cpp | P0 | 0.5h |
| 4 | 修复 NoLLM JSON 解析 | test_no_llm.cpp | P0 | 1h |
| 5 | 修复 Engine 执行流程 | test_engine.cpp | P1 | 1-2h |
| 6 | 修复 Basic 基础功能 | test_basic.cpp | P1 | 1h |
| 7 | 修复 LLM Tool 可用性检查 | test_llm_tool.cpp | P2 | 0.5h |

---

## Task 1: 修复 Parser signature 解析

**文件:**
- 修改: `src/modules/parser/markdown_parser.cpp:create_node_from_json()`
- 测试: `tests/test_parser.cpp:320`

**问题:** `Argument 'a' is not a string` - signature 中的 `arguments: {a: "1", b: "2", op: "+"}` 在解析时类型校验失败

**修复步骤:**

- [ ] **Step 1: 运行测试确认失败**

```bash
cd build && ctest -R test_parser --output-on-failure
```

预期输出:
```
/workspace/project/HydraForge/tests/test_parser.cpp:320: FAILED:
due to unexpected exception with message:
  Error parsing block '/lib/math/add': Argument 'a' is not a string
```

- [ ] **Step 2: 检查 create_node_from_json 中的 signature 解析逻辑**

```bash
grep -n "signature" src/modules/parser/markdown_parser.cpp | head -30
```

- [ ] **Step 3: 检查 DSL lib 规范中 arguments 的类型定义**

检查 `docs/specs/dsl-lib.md` 中关于 signature 的 arguments 类型规范

- [ ] **Step 4: 修复类型校验逻辑**

在 `create_node_from_json` 中，signature arguments 应接受 number 类型或 string 类型

- [ ] **Step 5: 运行测试验证修复**

```bash
cd build && ctest -R test_parser --output-on-failure
```

预期: test_parser 全部 12 个子测试通过

- [ ] **Step 6: 提交**

```bash
git add src/modules/parser/markdown_parser.cpp tests/test_parser.cpp
git commit -m "fix(parser): handle number type in signature arguments"
```

---

## Task 2: 修复 Scheduler DAG 调度

**文件:**
- 修改: `src/modules/scheduler/topo_scheduler.cpp`
- 测试: `tests/test_scheduler.cpp` (6个子测试，4个失败)

**失败测试:**
- `Concurrent Branches with wait_for` - FAILED
- `Soft Termination Continues Parent Flow` - FAILED
- `Lib Utils Noop Executes as Soft End` - FAILED
- `Cross-Graph Edge Execution` - FAILED

**修复步骤:**

- [ ] **Step 1: 运行测试确认失败模式**

```bash
cd build && ctest -R test_scheduler --output-on-failure
```

- [ ] **Step 2: 检查 TopoScheduler 的 wait_for 处理逻辑**

```bash
grep -n "wait_for" src/modules/scheduler/topo_scheduler.cpp
```

- [ ] **Step 3: 检查跨图调用的边连接逻辑**

```bash
grep -n "cross" src/modules/scheduler/topo_scheduler.cpp
grep -n "all_of" src/modules/scheduler/topo_scheduler.cpp
```

- [ ] **Step 4: 检查 __system__/noop 是否正确注册**

```bash
grep -rn "__system__" src/modules/
```

- [ ] **Step 5: 逐个修复每个失败的测试场景**

根据错误输出确定具体修复哪个函数

- [ ] **Step 6: 验证所有 6 个子测试通过**

```bash
cd build && ctest -R test_scheduler --output-on-failure
```

预期: 全部 6 个子测试通过

- [ ] **Step 7: 提交**

```bash
git add src/modules/scheduler/topo_scheduler.cpp tests/test_scheduler.cpp
git commit -m "fix(scheduler): DAG wait_for and cross-graph execution"
```

---

## Task 3: 修复 Library 库加载路径

**文件:**
- 修改: `src/modules/library/library_loader.cpp`
- 测试: `tests/test_library_loader.cpp`

**问题:** `found == false` - 找不到 `/lib/utils/noop`，但 `lib/utils/noop.md` 文件存在

**修复步骤:**

- [ ] **Step 1: 运行测试确认失败**

```bash
cd build && ctest -R test_library_loader --output-on-failure
```

预期输出:
```
REQUIRE( found )
with expansion:
  false
```

- [ ] **Step 2: 检查库加载器初始化**

```bash
grep -n "noop" src/modules/library/library_loader.cpp
grep -n "lib/" src/modules/library/library_loader.cpp
```

- [ ] **Step 3: 检查 StandardLibraryLoader 如何扫描 lib 目录**

```bash
grep -n "get_available_libraries" src/modules/library/library_loader.cpp
```

- [ ] **Step 4: 检查路径映射逻辑**

测试期望 path 为 `/lib/utils/noop`，实际文件是 `lib/utils/noop.md`

- [ ] **Step 5: 修复路径映射**

确保 `lib/utils/noop.md` 映射为 `/lib/utils/noop`

- [ ] **Step 6: 运行测试验证**

```bash
cd build && ctest -R test_library_loader --output-on-failure
```

预期: test_library_loader 通过

- [ ] **Step 7: 提交**

```bash
git add src/modules/library/library_loader.cpp tests/test_library_loader.cpp
git commit -m "fix(library): correct lib path mapping for StandardLibraryLoader"
```

---

## Task 4: 修复 NoLLM JSON 解析

**文件:**
- 修改: `src/modules/parser/markdown_parser.cpp` 或相关 JSON 处理
- 测试: `tests/test_no_llm.cpp`

**问题:** `[json.exception.type_error.302] type must be string, but is number`

**失败测试:**
- `Execute Assign + ToolCall Workflow` - FAILED
- `Execute Assign with Inja Functions` - FAILED
- `Resource Node Injection` - FAILED

**修复步骤:**

- [ ] **Step 1: 运行测试确认失败**

```bash
cd build && ctest -R test_no_llm --output-on-failure
```

- [ ] **Step 2: 检查具体哪个字段解析出错**

从错误信息看，某个字段期望 string 但收到 number

- [ ] **Step 3: 检查 YAML 解析后的类型处理**

```bash
grep -n "type_error" src/
```

- [ ] **Step 4: 检查 `assign` 节点中 number 类型如何处理**

assign 节点可能接受 `{num1: 15, num2: 27}` 但某处要求 string

- [ ] **Step 5: 修复类型兼容性问题**

确保 JSON/YAML 解析时 number 可以被当作 string 使用（Inja 模板可能需要）

- [ ] **Step 6: 运行测试验证**

```bash
cd build && ctest -R test_no_llm --output-on-failure
```

预期: 全部 3 个子测试通过

- [ ] **Step 7: 提交**

```bash
git add src/ tests/test_no_llm.cpp
git commit -m "fix(parser): handle number type in assign node values"
```

---

## Task 5: 修复 Engine 执行流程

**文件:**
- 修改: `src/core/engine.cpp`
- 测试: `tests/test_engine.cpp`

**问题:** `result.paused_at.has_value() == false` - 执行流程未按预期暂停

**修复步骤:**

- [ ] **Step 1: 运行测试确认失败**

```bash
cd build && ctest -R test_engine --output-on-failure
```

- [ ] **Step 2: 检查 test_engine 的期望行为**

```bash
cat tests/test_engine.cpp
```

- [ ] **Step 3: 检查 DSLEngine 的 run 逻辑**

```bash
grep -n "paused_at" src/core/engine.cpp
```

- [ ] **Step 4: 检查 DSLEngine::run() 的返回结果结构**

- [ ] **Step 5: 修复执行流程**

可能是 LLM call 节点的处理逻辑问题

- [ ] **Step 6: 运行测试验证**

```bash
cd build && ctest -R test_engine --output-on-failure
```

- [ ] **Step 7: 提交**

```bash
git add src/core/engine.cpp tests/test_engine.cpp
git commit -m "fix(engine): correct execution flow and paused_at state"
```

---

## Task 6: 修复 Basic 基础功能

**文件:**
- 修改: `src/core/engine.cpp` 或相关
- 测试: `tests/test_basic.cpp`

**问题:** `result.success == false` - 2个子测试失败

**失败测试:**
- `Engine Execution (Simple DSL)` - FAILED
- `Tool Call Execution (Conceptual)` - FAILED

**修复步骤:**

- [ ] **Step 1: 运行测试确认失败**

```bash
cd build && ctest -R test_basic --output-on-failure
```

- [ ] **Step 2: 检查 test_basic 的测试场景**

```bash
grep -A20 "Engine Execution" tests/test_basic.cpp
```

- [ ] **Step 3: 检查 test_basic 使用的 DSL 流程**

- [ ] **Step 4: 修复执行问题**

可能是 tool_call 节点执行或 context 传递问题

- [ ] **Step 5: 运行测试验证**

```bash
cd build && ctest -R test_basic --output-on-failure
```

预期: 5个子测试全部通过

- [ ] **Step 6: 提交**

```bash
git add src/core/engine.cpp tests/test_basic.cpp
git commit -m "fix(engine): basic DSL execution and tool call flow"
```

---

## Task 7: 修复 LLM Tool 可用性检查

**文件:**
- 修改: `src/common/llm/llama_tool.cpp` 或相关
- 测试: `tests/test_llm_tool.cpp`

**问题:** `tool.is_available() == false` 期望但实际返回 `true`

**修复步骤:**

- [ ] **Step 1: 运行测试确认失败**

```bash
cd build && ctest -R test_llm_tool --output-on-failure
```

- [ ] **Step 2: 检查 LlamaTool::is_available() 实现**

```bash
grep -n "is_available" src/common/llm/llama_tool.cpp
```

- [ ] **Step 3: 检查测试环境是否有 llama 模型**

```bash
ls -la models/ 2>/dev/null || echo "No models directory"
cat llm_config.json 2>/dev/null | head -20
```

- [ ] **Step 4: 修复 is_available() 逻辑**

如果测试环境无模型，应返回 false

- [ ] **Step 5: 运行测试验证**

```bash
cd build && ctest -R test_llm_tool --output-on-failure
```

预期: test_llm_tool 5个子测试全部通过

- [ ] **Step 6: 提交**

```bash
git add src/common/llm/ tests/test_llm_tool.cpp
git commit -m "fix(llm): correct is_available() when model unavailable"
```

---

## 验收标准

- [ ] `ctest --output-on-failure` 显示 12/12 测试通过
- [ ] 无新的编译警告或错误
- [ ] 所有失败测试的根因已修复（非 workaround）

---

## 执行后更新文档

修复完成后更新 `docs/roadmap-status.md` 的验证状态部分，将以下测试标记为通过：

```
| test_basic | 基础 | ✅ | <today> | 5/5 |
| test_engine | Engine | ✅ | <today> | 1/1 |
| test_scheduler | Scheduler | ✅ | <today> | 6/6 |
| test_parser | Parser | ✅ | <today> | 12/12 |
| test_library_loader | 标准库 | ✅ | <today> | 1/1 |
| test_no_llm | 无 LLM 模式 | ✅ | <today> | 3/3 |
| test_llm_tool | LLM | ✅ | <today> | 5/5 |
```

---

**Plan saved to:** `docs/superpowers/plans/YYYY-MM-DD-test-fixes-for-prephase.md`
