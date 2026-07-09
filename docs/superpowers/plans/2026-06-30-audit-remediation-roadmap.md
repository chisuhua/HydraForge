# Audit Remediation Roadmap (Sprint 15-17) (2026-06-30)

> **目的**: 修复 2026-06-30 全项目审计发现的架构债 / 代码债 / 测试覆盖缺口，建立 3 个新的 OpenSpec change 跟踪 P0-P1 修复工作。
> **创建日期**: 2026-06-30
> **审计输入**: 2026-06-30 全项目审计 (code-review-graph + 直接工具 + 2 explore agent)
> **审计基线**: 41/41 ctest PASS, build clean, 当前 commit `4f6d184` (Sprint 14 C4 ship)
> **关联文件**: `openspec/changes/2026-06-30-*` (本规划创建的 3 个新 change)
> **关联 docs**: `docs/active-status.md` (活跃变更看板) / `docs/archive/implementation-roadmap.md` (旧静态蓝图) / `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` (上游 9-change 路线图)

---

## 一、审计发现基线 (2026-06-30)

| 类别 | 数量 | 优先级分布 |
|------|------|-----------|
| 严重架构债 | 3 | 🔴 立即 (P0) |
| 中等架构债 | 3 | 🟠 Sprint 16 (P1) |
| 代码债 | 8 | 🟡 Sprint 15-16 (P1-P2) |
| 测试覆盖缺失 (MISSING) | 4 | 🔴 Sprint 15 (P0) |
| 测试覆盖部分 (PARTIAL) | 5 | 🟡 Sprint 15-16 (P1-P2) |
| 文档漂移 | 1 | 🟢 立即 (5 分钟) |
| **总计** | **24 项** | |

**审计证据**:
- `code-review-graph`: 2694 节点 / 16108 边 / 11 社区 / 风险评分 0.00 (低)
- 8 个高复杂度文件专项分析 + 48 个生产 .cpp × 40 个测试文件覆盖矩阵
- 41/41 ctest PASS (1.53 秒), 无 sanitizer 错误

---

## 二、新 Change 依赖关系图

```
                  [本规划创建 3 个新 change]
[Sprint 15]                        [Sprint 16]                    [Sprint 17]
┌─────────────────────────────────┐
│ A: 2026-06-30-audit-quick-wins  │  (1 周, P0)
│   - AGENTS.md 漂移修复            │
│   - fork/join stub 删除           │
│   - execution_session 注释清理    │
│   - 8 项代码债清理                 │
└─────────────────────────────────┘
                                   ┌─────────────────────────────────┐
                                   │ B: 2026-06-30-audit-coverage-   │  (1 周, P1)
                                   │      backfill                   │
                                   │   - 4 个 MISSING 测试补齐         │
                                   │   - 5 个 PARTIAL 测试补齐         │
                                   └─────────────────────────────────┘
                                                                      ┌─────────────────────────┐
                                                                      │ C: 2026-06-30-audit-    │  (2 周, P1)
                                                                      │      arch-refactor       │
                                                                      │   - layered_context      │
                                                                      │     thread safety        │
                                                                      │   - execute_tool_call    │
                                                                      │     拆分 (138 行 → 3 函数) │
                                                                      │   - execute_single_branch│
                                                                      │     拆分 (117 行)         │
                                                                      │   - topo_scheduler.h     │
                                                                      │     跨模块解耦            │
                                                                      └─────────────────────────┘

并行车道 (与 Sprint 11-18 路线图的关系):
- 主线: A → B → C (顺序, 因为 C 依赖 B 的测试基础设施)
- 并行 1: 与现有 C5 (session-hierarchy) 并行 — 独立模块, 零冲突
- 并行 2: 与现有 C6 (metadata-approval) 并行 — 独立模块, 零冲突
- 并行 3: 与现有 C7 (model-router-plugin) 并行 — 独立模块, 零冲突
```

**关键依赖事实**:
- A 零外部依赖, 可立即启动
- B 依赖 A 中"代码债清理"的部分基础设施 (compute_backoff 等)
- C 依赖 B 完成 thread safety 测试基础设施
- A/B/C 与现有 C5/C6/C7 完全独立

---

## 三、3 个新 Change 总览

| # | Change 名 | 类型 | 估时 | 依赖 | 状态 | Sprint |
|---|-----------|------|------|------|------|--------|
| **A** | `2026-06-30-audit-quick-wins` | 实施 | 0.5 周 | — | ⚪ 待创建 | Sprint 15 |
| **B** | `2026-06-30-audit-coverage-backfill` | 实施 | 1 周 | A (部分) | ⚪ 待创建 | Sprint 16 |
| **C** | `2026-06-30-audit-arch-refactor` | 实施 | 2 周 | B | ⚪ 待创建 | Sprint 17 |

**估时合计**: 3.5 周 (Sprint 15 启动 → Sprint 17 结束, 与 C5/C6/C7 同期 ship)

---

## 四、Change A: `2026-06-30-audit-quick-wins` (Sprint 15, 0.5 周)

### 范围

修复审计中识别的高 ROI 低风险项：

1. 文档漂移 (5 分钟)
2. 永远 throw 的死代码 (30 分钟)
3. 残留注释块清理 (5 分钟)
4. 8 项代码债清理 (2-3 天)

### 任务清单 (TDD 风格, 原子步骤)

#### Task A.1: 修复 AGENTS.md 文档漂移

**Files:**
- Modify: `AGENTS.md:79` (cross-module include 计数行)

- [ ] **Step 1: 读取当前 AGENTS.md 第 75-85 行**

```bash
sed -n '75,85p' AGENTS.md
```

预期输出: 看到当前 "engine.h 跨模块 include 计数 **2→1** (仅 `common/llm/llm_types.h` types 例外)" 段。

- [ ] **Step 2: 修改为正确计数**

将 `engine.h 跨模块 include 计数 **2→1**` 改为 `engine.h 跨模块 include 计数 **2→4** (Sprint 13 ADR-0031 新增 common/policy/{policy_factory,approval_handler,approval_callbacks}.h + Sprint 1b common/llm/llm_types.h types 例外)`.

- [ ] **Step 3: 同步更新 ADR-0019 §1.4 状态**

如果 `docs/adr/adr-0019-iinteraction-bus-mvp.md` §1.4 也提到该计数，同步更新。

- [ ] **Step 4: 验证 AGENTS.md 中所有 "1 个 types 例外" 表述**

```bash
grep -n "1 个 types 例外\|仅 common/llm/llm_types.h" AGENTS.md docs/adr/*.md 2>/dev/null
```

预期: 全部已更新或确认不需要更新。

- [ ] **Step 5: Commit**

```bash
git add AGENTS.md docs/adr/*.md
git commit -m "docs(agents): fix engine.h cross-module include count drift (1 → 4 after Sprint 13)"
```

#### Task A.2: 删除 node_executor.cpp 永远 throw 的 fork/join stub

**Files:**
- Modify: `src/modules/executor/node_executor.cpp:367-391`
- Modify: `src/modules/executor/node_executor.cpp:57, 61` (execute_node 中的 fork/join case)
- Modify: `src/modules/executor/node_executor.h` (声明部分)
- Test: `tests/test_executor.cpp` (验证新行为)

- [ ] **Step 1: 验证 fork/join 在测试中不被直接调用**

```bash
grep -rn "execute_fork\|execute_join" src/ tests/ --include="*.cpp" --include="*.h"
```

预期: 看到 `node_executor.cpp` 定义 + `node_executor.cpp:57,61` 在 `execute_node` switch 中调用，无任何测试引用 `execute_fork`/`execute_join`。

- [ ] **Step 2: 写锁定测试，确认 fork/join 路径不被 NodeExecutor 处理**

修改 `tests/test_executor.cpp`，添加测试用例：

```cpp
TEST_CASE("Executor: ForkNode should be rejected by NodeExecutor (scheduler handles it)") {
  // Arrange: 创建 ForkNode + NodeExecutor
  // Act + Assert: 调用 execute_node 验证抛 std::runtime_error
  // 这是当前行为锁定，删除 stub 后修改为 unreachable 代码
}
```

- [ ] **Step 3: 运行测试确认 PASS**

```bash
cmake --build build -j$(nproc) --target test_executor && ctest -R test_executor --output-on-failure
```

预期: PASS (新加测试通过 + 现有测试无回归)。

- [ ] **Step 4: 修改 execute_node switch 删除 fork/join case**

在 `src/modules/executor/node_executor.cpp` 中删除：
```cpp
case NodeType::FORK:
  return execute_fork(dynamic_cast<const ForkNode*>(node), context_with_resources);
case NodeType::JOIN:
  return execute_join(dynamic_cast<const JoinNode*>(node), context_with_resources);
```

替换为：
```cpp
case NodeType::FORK:
case NodeType::JOIN:
  // ForkNode/JoinNode 由 TopoScheduler::process_fork_join 处理, 不应到达 NodeExecutor
  throw std::logic_error("ForkNode/JoinNode reached NodeExecutor - scheduler routing bug");
```

- [ ] **Step 5: 删除 execute_fork 和 execute_join 函数体**

删除 `src/modules/executor/node_executor.cpp:367-391` 整段函数定义。

- [ ] **Step 6: 在 header 中删除声明**

删除 `src/modules/executor/node_executor.h` 中 `execute_fork` 和 `execute_join` 的声明（如有）。

- [ ] **Step 7: 运行测试套件确认无回归**

```bash
cmake --build build -j$(nproc) && ctest --output-on-failure
```

预期: 41/41 PASS（锁定测试因修改为 unreachable 而 SKIP 或改 assertion 检查 logic_error）。

- [ ] **Step 8: 更新锁定测试**

将 Step 2 的测试改为验证 throw `std::logic_error` 而非 `std::runtime_error`。

- [ ] **Step 9: Commit**

```bash
git add src/modules/executor/node_executor.cpp src/modules/executor/node_executor.h tests/test_executor.cpp
git commit -m "refactor(executor): remove unreachable fork/join stubs (scheduler handles them)"
```

#### Task A.3: 清理 execution_session.cpp 注释代码块

**Files:**
- Modify: `src/modules/scheduler/execution_session.cpp:148-156`

- [ ] **Step 1: 读取当前注释块**

```bash
sed -n '145,160p' src/modules/scheduler/execution_session.cpp
```

预期: 看到一段 `/* ... */` 注释掉的旧 `generate_subgraph_with_callback` 路径代码。

- [ ] **Step 2: 删除注释块**

直接删除 L148-156 的整个 `/* */` 块。

- [ ] **Step 3: 运行测试验证**

```bash
cmake --build build -j$(nproc) && ctest --output-on-failure
```

预期: 41/41 PASS（纯删除注释，无逻辑变化）。

- [ ] **Step 4: Commit**

```bash
git add src/modules/scheduler/execution_session.cpp
git commit -m "chore(scheduler): remove commented legacy code block in execution_session"
```

#### Task A.4: cloud_adapter.cpp 提取 compute_backoff()

**Files:**
- Modify: `src/common/llm/cloud_adapter.cpp:262-270, 284-291`
- Test: `tests/test_cloud_llm.cpp` (添加单元测试)

- [ ] **Step 1: 写 compute_backoff 单元测试**

在 `tests/test_cloud_llm.cpp` 添加：
```cpp
TEST_CASE("CloudLLMAdapter::compute_backoff: exponential with jitter") {
  // 验证 attempt=0 返回 base_ms 附近
  // 验证 attempt=3 返回 max_ms 附近
  // 验证抖动范围在 ±25% 内
}
```

- [ ] **Step 2: 运行测试确认 FAIL**

```bash
cmake --build build -j$(nproc) --target test_cloud_llm && ctest -R test_cloud_llm --output-on-failure
```

预期: FAIL（compute_backoff 未定义）。

- [ ] **Step 3: 实现 compute_backoff**

在 `src/common/llm/cloud_adapter.cpp` 添加私有静态方法：
```cpp
static int compute_backoff(int attempt, int base_ms, int max_ms, std::mt19937& rng) {
  int delay_ms = std::min(max_ms, base_ms << attempt);
  int jitter = delay_ms / 4;
  std::uniform_int_distribution<int> dist(-jitter, jitter);
  delay_ms += dist(rng);
  return std::max(0, delay_ms);
}
```

- [ ] **Step 4: 替换两处重复调用**

将 L262-270 和 L284-291 的内联退避代码替换为 `compute_backoff(attempt, base_ms, max_ms, rng_)`。

- [ ] **Step 5: 运行测试 PASS**

```bash
cmake --build build -j$(nproc) && ctest --output-on-failure
```

预期: 41/41 + 1 new PASS。

- [ ] **Step 6: Commit**

```bash
git add src/common/llm/cloud_adapter.cpp tests/test_cloud_llm.cpp
git commit -m "refactor(llm): extract compute_backoff() to eliminate duplication in CloudLLMAdapter"
```

#### Task A.5: topo_scheduler.cpp build_dag() 去重

**Files:**
- Modify: `src/modules/scheduler/topo_scheduler.cpp:147-155, 166-171`
- Test: `tests/test_scheduler.cpp` (现有测试保护)

- [ ] **Step 1: 验证测试覆盖 build_dag 行为**

```bash
grep -n "build_dag\|TEST_CASE" tests/test_scheduler.cpp | head -20
```

预期: 看到 build_dag 的现有测试覆盖（如 `TEST_CASE("TopoScheduler::build_dag builds linear chain")`）。

- [ ] **Step 2: 提取 find_ready_nodes 辅助方法**

将两处 "find zero in-degree nodes and push to ready_queue" 逻辑提取为：
```cpp
void TopoScheduler::collect_ready_nodes(DagState& state) {
  for (auto& [node_id, node_state] : state.node_states) {
    if (node_state.remaining_deps == 0 && !node_state.in_ready_queue) {
      state.ready_queue.push(&state.nodes.at(node_id));
      node_state.in_ready_queue = true;
    }
  }
}
```

- [ ] **Step 3: 在两个 build_dag 重载中调用**

替换 L147-155 和 L166-171 的内联逻辑为 `collect_ready_nodes(state)`。

- [ ] **Step 4: 运行测试 PASS**

```bash
cmake --build build -j$(nproc) && ctest --output-on-failure
```

预期: 41/41 PASS。

- [ ] **Step 5: Commit**

```bash
git add src/modules/scheduler/topo_scheduler.cpp src/modules/scheduler/topo_scheduler.h
git commit -m "refactor(scheduler): extract collect_ready_nodes() to deduplicate build_dag()"
```

#### Task A.6: plugin_loader.cpp 非 Linux stub + 日志统一

**Files:**
- Modify: `src/modules/plugin/plugin_loader.cpp:32-42, 152, 293-295`

- [ ] **Step 1: 检查项目日志 API**

```bash
grep -rn "agenticdsl::log\|namespace log" include/ src/ --include="*.h" | head -5
```

预期: 找到项目统一日志宏/函数（如 `AGENTICDSL_LOG_ERROR`）。

- [ ] **Step 2: 替换 std::cerr 为统一日志**

将 L32-42 的 `std::cerr` 改为项目日志 API。

- [ ] **Step 3: 修改非 Linux 编译分支**

将 L293-295 的 `#error` 改为：
```cpp
#else
// 非 Linux 平台: PluginLoader 为 stub, 所有方法返回失败
std::vector<std::string> PluginLoader::discover(const std::string&) { return {}; }
bool PluginLoader::load(const std::string&) { return false; }
#endif
```

- [ ] **Step 4: 添加非 Linux stub 测试**

在 `tests/test_plugin_loader.cpp` 添加：
```cpp
TEST_CASE("PluginLoader: non-Linux stub returns empty/false gracefully") {
  // 仅当编译目标为非 Linux 时激活
}
```

- [ ] **Step 5: 运行测试 PASS**

```bash
cmake --build build -j$(nproc) && ctest --output-on-failure
```

预期: 41/41 + 1 new PASS。

- [ ] **Step 6: Commit**

```bash
git add src/modules/plugin/plugin_loader.cpp tests/test_plugin_loader.cpp
git commit -m "refactor(plugin): non-Linux stub + unified logging in PluginLoader"
```

#### Task A.7: node_factory.cpp 合并 make_llm_call / make_dsl_call

**Files:**
- Modify: `src/modules/parser/node_factory.cpp:113-144`

- [ ] **Step 1: 验证测试覆盖**

```bash
grep -n "make_llm_call\|make_dsl_call\|LLMCall\|DSLCall" tests/test_parser.cpp | head -10
```

预期: 看到两个 NodeType 的解析测试。

- [ ] **Step 2: 提取统一工厂**

将两个函数合并：
```cpp
static std::unique_ptr<Node> make_llm_or_dsl_call(const json& config) {
  // 共享逻辑: 提取 tool_name / params / output_key
  // 区别: llm_call 需要 llm_tool_name 默认 "llama-default"
}
```

- [ ] **Step 3: 更新注册表**

将 `make_llm_call` 和 `make_dsl_call` 都指向新函数或保留为薄包装。

- [ ] **Step 4: 运行测试 PASS**

```bash
cmake --build build -j$(nproc) && ctest --output-on-failure
```

预期: 41/41 PASS。

- [ ] **Step 5: Commit**

```bash
git add src/modules/parser/node_factory.cpp
git commit -m "refactor(parser): merge make_llm_call and make_dsl_call factories"
```

#### Task A.8: layered_context.h split_path + at() 双重导航

**Files:**
- Modify: `include/agenticdsl/types/layered_context.h:201-213, 122-153`
- Test: `tests/test_layered_context.cpp`

- [ ] **Step 1: 修改 split_path 返回类型**

将 `bool split_path()` 改为 `void split_path()`，移除永真返回值。

- [ ] **Step 2: 合并 at() 中的双重导航**

将 L122-153 的"探测路径 + 实际导航"两次执行合并为一次。

- [ ] **Step 3: 更新测试调用点**

```bash
grep -rn "split_path" src/ tests/ --include="*.cpp" --include="*.h"
```

更新所有调用点，移除对返回值的检查。

- [ ] **Step 4: 运行测试 PASS**

```bash
cmake --build build -j$(nproc) && ctest --output-on-failure
```

预期: 41/41 PASS（纯重构 + 测试更新）。

- [ ] **Step 5: Commit**

```bash
git add include/agenticdsl/types/layered_context.h tests/test_layered_context.cpp
git commit -m "refactor(context): remove redundant split_path return + merge at() double navigation"
```

### Change A 风险评估

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| fork/join stub 删除破坏 scheduler 集成路径 | 低 | 高 | 锁定测试 + scheduler 现有测试覆盖 |
| compute_backoff 提取行为改变 | 中 | 中 | 单元测试验证抖动范围 |
| build_dag 去重破坏 fork/join 行为 | 低 | 高 | test_execute_parallel 已覆盖 |
| plugin_loader 日志替换破坏现有错误流 | 低 | 低 | 现有 test_plugin_loader 覆盖 |

### Change A 成功标准

- [ ] 41 → 42+ ctest PASS (新增 ≥ 1 测试)
- [ ] ASan/TSan 41/41 (100%)
- [ ] AGENTS.md 与实际状态一致
- [ ] 8 项代码债全部清理
- [ ] node_executor.cpp -25 行 (fork/join stub + 注释)
- [ ] cloud_adapter.cpp -20 行 (compute_backoff 提取)
- [ ] topo_scheduler.cpp -15 行 (build_dag 去重)

---

## 五、Change B: `2026-06-30-audit-coverage-backfill` (Sprint 16, 1 周)

### 范围

为审计识别的 4 个 MISSING + 5 个 PARTIAL 源文件补充测试覆盖。

### 任务清单 (高层概览, 详细 TDD 步骤在 Sprint 16 启动时展开)

#### Task B.1: test_trace_exporter.cpp (1 天)

**Files:**
- Create: `tests/test_trace_exporter.cpp`
- Target: `src/modules/trace/trace_exporter.cpp` (111 行)

测试用例:
- `TEST_CASE: TraceExporter::on_node_start records timestamp`
- `TEST_CASE: TraceExporter::on_node_end records duration`
- `TEST_CASE: TraceExporter::export_traces produces valid JSON`
- `TEST_CASE: TraceExporter::export_json handles empty trace set`
- `TEST_CASE: TraceExporter reset clears state`

#### Task B.2: test_context_engine.cpp (1 天)

**Files:**
- Create: `tests/test_context_engine.cpp`
- Target: `src/modules/context/context_engine.cpp` (193 行)

测试用例:
- `TEST_CASE: ContextEngine::merge handles disjoint keys`
- `TEST_CASE: ContextEngine::merge handles overlapping keys (child wins)`
- `TEST_CASE: ContextEngine::size returns correct byte count`
- `TEST_CASE: ContextEngine::trim removes oldest entries when oversized`

#### Task B.3: test_resource_manager.cpp (0.5 天)

**Files:**
- Create: `tests/test_resource_manager.cpp`
- Target: `src/modules/scheduler/resource_manager.cpp` (31 行)

测试用例:
- `TEST_CASE: ResourceManager registers resources by name`
- `TEST_CASE: ResourceManager::get returns null for unknown name`
- `TEST_CASE: ResourceManager serializes to/from JSON`

#### Task B.4: test_http_adapter.cpp (1.5 天)

**Files:**
- Create: `tests/test_http_adapter.cpp`
- Target: `src/common/llm/http_adapter.cpp` (281 行)

测试用例:
- `TEST_CASE: HttpLLMAdapter::generate returns response on 200`
- `TEST_CASE: HttpLLMAdapter::generate handles 4xx errors`
- `TEST_CASE: HttpLLMAdapter::generate_stream yields chunks`
- `TEST_CASE: HttpLLMAdapter error mapping (HTTP 500 → ProviderError)`

注: 需要 httplib::Server 启动 mock HTTP 服务（测试本地端口 0 动态分配）。

#### Task B.5: test_system_nodes.cpp (0.5 天)

**Files:**
- Create: `tests/test_system_nodes.cpp`
- Target: `src/modules/system/system_nodes.cpp` (28 行)

测试用例:
- `TEST_CASE: create_system_nodes returns expected node count`
- `TEST_CASE: System nodes have correct metadata (type, layer)`

#### Task B.6: test_llama_adapter_provider.cpp (0.5 天)

**Files:**
- Create: `tests/test_llama_adapter_provider.cpp`
- Target: `src/common/llm/llama_adapter_provider.cpp` (124 行)

测试用例:
- `TEST_CASE: LlamaAdapterProvider creates adapter from config`
- `TEST_CASE: LlamaAdapterProvider returns null for invalid config`

#### Task B.7: test_budget_factory.cpp (0.5 天)

**Files:**
- Create: `tests/test_budget_factory.cpp`
- Target: `src/modules/budget/factory.cpp` (12 行)

测试用例:
- `TEST_CASE: budget::create_controller returns IBudgetController`
- `TEST_CASE: budget::create_controller configures limits from JSON`

#### Task B.8: test_llm_factory.cpp (0.5 天)

**Files:**
- Create: `tests/test_llm_factory.cpp`
- Target: `src/common/llm/factory.cpp` (12 行)

测试用例:
- `TEST_CASE: llm::create_provider_factory returns IProviderFactory`
- `TEST_CASE: llm::create_provider_factory dispatches by backend name`

### Change B 风险评估

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| http_adapter 测试需要真实网络端口 | 中 | 中 | 使用 httplib::Server + 端口 0 动态分配 |
| context_engine 行为未明确文档化 | 中 | 中 | 通过现有集成测试推断正确行为 |
| resource_manager API 不稳定 | 低 | 中 | 锁定当前行为，测试可调整 |

### Change B 成功标准

- [ ] 41 → 49+ ctest PASS (新增 8 测试文件, 25+ 测试用例)
- [ ] FULL 覆盖率: 64.6% → 95%+ (45/48 文件)
- [ ] ASan/TSan 49/49 (100%)
- [ ] 代码覆盖率报告 (gcov/lcov) 显示新增覆盖 ≥ 80% 行覆盖

---

## 六、Change C: `2026-06-30-audit-arch-refactor` (Sprint 17, 2 周)

### 范围

修复审计识别的 3 个严重 + 2 个中等架构债。

### 任务清单 (高层概览)

#### Task C.1: layered_context.h 静态 null_j → thread_local (2 天)

**Files:**
- Modify: `include/agenticdsl/types/layered_context.h:226, 242`
- Test: `tests/test_layered_context.cpp` (添加并发测试)

实施:
- 将 `static nlohmann::json null_j` 改为 `static thread_local nlohmann::json null_j`
- 添加并发测试: 10 线程 × 1000 次 at() 调用
- 验证 TSan: `cmake --preset tsan && ctest`

#### Task C.2: execute_tool_call() 拆分 (3 天)

**Files:**
- Modify: `src/modules/executor/node_executor.cpp:165-303`
- Test: `tests/test_executor.cpp` (锁定测试 + 新增拆分测试)

实施:
- 锁定当前行为 (characterization tests)
- 拆分为 3 个函数:
  - `dispatch_tool_call()` (~40 行): 决定 tool_coordinator / approval_handler / direct 路径
  - `process_tool_result()` (~50 行): error_code switch + output_keys 映射
  - `emit_tool_events()` (~30 行): bus 推送 + 延时计算
- 每个函数独立单元测试

#### Task C.3: execute_single_branch() 拆分 (1 周)

**Files:**
- Modify: `src/modules/scheduler/topo_scheduler.cpp:340-457`
- Test: `tests/test_executor_with_mock_provider.cpp` (新增分支隔离测试)

实施:
- 阶段 1 (2 天): 写完整的 characterization test, 锁定当前行为
- 阶段 2 (3 天): 拆分 117 行函数为 4-5 个子函数:
  - `branch_setup()`
  - `branch_main_loop()`
  - `branch_handle_dynamic_deps()`
  - `branch_terminate()`
- 阶段 3 (2 天): 移除 HardEndException 反模式, 改用正常返回 + status enum

#### Task C.4: topo_scheduler.h 跨模块解耦 (3 天)

**Files:**
- Modify: `src/modules/scheduler/topo_scheduler.h:12-13`
- Modify: `src/modules/scheduler/topo_scheduler.h` (PIMPL-lite 化)

实施:
- 阶段 1 (1 天): 评估方案 — PIMPL-lite vs 接口 vs 前向声明
- 阶段 2 (1 天): 实施 (优先方案 PIMPL-lite)
- 阶段 3 (1 天): 验证 engine.h 不再被牵连解析 parser/types

### Change C 风险评估

| 风险 | 概率 | 影响 | 缓解 |
|------|------|------|------|
| execute_single_branch 重构破坏现有并行执行行为 | 高 | 高 | characterization tests 必须先于重构 |
| HardEndException 移除破坏异常传播路径 | 中 | 高 | 完整 scheduler 集成测试覆盖 |
| thread_local 在某些平台性能差 | 低 | 低 | TSan 验证 + benchmark |
| PIMPL-lite 引入间接调用开销 | 低 | 低 | benchmark 验证 |

### Change C 成功标准

- [ ] 49 → 52+ ctest PASS (新增 3 测试文件)
- [ ] execute_single_branch < 60 行
- [ ] execute_tool_call 拆分为 3 个 < 60 行函数
- [ ] TSan 52/52 (100%, 0 warnings)
- [ ] ASan 52/52 (100%)
- [ ] topo_scheduler.h 0 个 modules/ include
- [ ] engine.h 间接解析开销下降 (benchmark)

---

## 七、跨 Change 通用约定

### TDD 工作流

每个 task 严格遵循:
1. 写失败测试 (锁定当前行为)
2. 运行测试确认 FAIL
3. 实施最小修改
4. 运行测试 PASS
5. 运行完整测试套件 (零回归)
6. Commit

### Commit 规范

```
<type>(<scope>): <subject>

<body - 解释 what 和 why, 不解释 how>
```

Types: feat / refactor / fix / test / docs / chore
Scope: 4 字母以内 (如 exec, sched, llm, ctx)

### Ship Gate (每个 Change 归档前)

1. `cmake --preset debug -DAGENTICDSL_BUILD_TESTS=ON` 编译通过
2. `cmake --preset asan -DAGENTICDSL_BUILD_TESTS=ON && ctest` 100% PASS
3. `cmake --preset tsan -DAGENTICDSL_BUILD_TESTS=ON && ctest` 100% PASS (0 warnings)
4. `openspec validate <change-name>` exit 0
5. AGENTS.md "Recent Changes" 段追加 entry
6. ADR 状态更新（如有）

---

## 八、3 个 Change 完成后项目基线预测 (2026-07-21 估算)

| 维度 | 当前 (2026-06-30) | 目标 (2026-07-21) |
|------|------------------|------------------|
| 测试用例数 | 41 | **52+** |
| ASan/TSan | 41/41 (100%) | 52/52 (100%) |
| FULL 覆盖率 | 31/48 (64.6%) | **45/48 (93.8%)** |
| 严重架构债 | 3 | **0** |
| 中等架构债 | 3 | **0** |
| engine.h 跨模块 include | 4 | **0** (PIMPL-lite 后) |
| 文档漂移 | 1 | **0** |

---

## 九、Sprint 启动 Checklist (Sprint 15 启动时执行)

- [ ] 创建 OpenSpec change: `openspec new change 2026-06-30-audit-quick-wins --description "..."`
- [ ] 编写 proposal.md + design.md + tasks.md + 1+ specs
- [ ] 创建 worktree (如需要)
- [ ] 创建 GitHub branch: `git checkout -b audit/sprint-15-quick-wins`
- [ ] 复制本规划 Sprint 15 部分作为 tasks.md 输入
- [ ] 按 TDD 流程执行 8 个 task
- [ ] 每个 task 完成后勾选本文件对应 `- [ ]` 项

---

## 十、关联文件与依赖

**上游文档**:
- `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` — 主线 9-change 路线图
- `docs/archive/roadmap-status.md` — 总进度看板 (已归档)
- `docs/audits/2026-06-30-full-project-audit.md` — 审计源报告 (待创建)

**下游文档**:
- `openspec/changes/2026-06-30-audit-quick-wins/` — Change A
- `openspec/changes/2026-06-30-audit-coverage-backfill/` — Change B
- `openspec/changes/2026-06-30-audit-arch-refactor/` — Change C
- `docs/superpowers/plans/2026-07-01-sprint-15-quick-wins.md` — Sprint 15 执行计划 (Sprint 15 启动时创建)

---

## 文档更新记录

| 日期 | 更新内容 |
|------|---------|
| 2026-06-30 | 初版: 审计基线 + 3 个 Change 路线图 + Sprint 15-17 任务清单 |