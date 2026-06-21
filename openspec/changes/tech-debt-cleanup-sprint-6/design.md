## Context

**当前状态**（2026-06-21 code-review-graph 全图扫描）：

| 指标 | 值 | 状态 |
|---|---|---|
| 知识图谱节点 | 2498 | — |
| 跨边 | 14793 | — |
| src/ 函数节点 | 1902 | — |
| TESTED_BY 边 | 90 | 4.7% 函数覆盖 |
| `topo_scheduler::execute` LOC | 308 | 🔴 Hub 出度 89 |
| `markdown_parser::create_node_from_json` LOC | 216 | 🟠 Hub 出度 103 |
| engine.cpp 跨模块 include | 10 | 🟡 PIMPL-lite 收益打折 |
| TODO 标记 | 3 | 全部 ADR 追踪 |
| `catch(...)` | 14 | 9 处隔离 + 5 处模块边界 |
| `reinterpret_cast` | 1 | dlsym 不可避免 |
| 未提交文件 | 7 | plugin_loader Sprint 5 WIP |

**ADR 状态滞后**：
- ADR-0020 (Thread Model): 文档 🟡 Partial, 实际 §2.2.1/3.2 ✅ Resolved
- ADR-0021 (PDK Design): 文档 🔍 Proposed, Sprint 4 已 ship
- ADR-0022 (Plugin Loading): 文档 🔍 Proposed, 代码已实现 plugin_loader

**已闭合的 2026-06-09 审计**（6 项 P0 + 13 项 P1 中已解决）：
- ✅ P0-1 ILLMAdapter 迁移（commit 248d209）
- ✅ P0-2 std::cout [DEBUG] 替换为 log facade
- ✅ P0-3 req1.md LLMCallNode 归档
- ✅ P0-4 ADR-0020 README 更新
- ✅ P0-5 AgenticOS_* 死链修复
- ✅ P0-6 ADR-0001 日期替换
- 🟡 P1-2 残留：`yaml_json.cpp:72,75` 2 处 `[DEBUG-removed]` 死注释未清理

**关联 ADR**：
- ADR-0019 §1.4 (engine.h decoupling)：当前 2→1 跨模块 include 已 ship，本 change 推进 engine.cpp 完全解耦
- ADR-0020 §3.2 (WorkerPool isolation)：DomainWorkerPool ✅ Resolved，文档同步滞后
- ADR-0021 §7 (PDK Dual-Repo)：Sprint 4 ship，文档状态 🔍 → 🟡
- ADR-0022 §1.2 (PluginInfo POD)：Sprint 5 实现中，文档状态 🔍 → 🟡

## Goals / Non-Goals

**Goals:**
1. 将 `TopoScheduler::execute` 从 308 行 god function 拆为 3 子函数（prepare / dispatch / handle），Hub 出度从 89 降至 <30
2. 引入 `NodeFactoryRegistry` 注册表模式，消除 `create_node_from_json` 216 行 if-else 链
3. `engine.cpp` 通过各模块工厂函数，跨模块 include 数 10 → 3（PIMPL-lite 收益最大化）
4. 同步 3 个 ADR（0020/0021/0022）状态至代码实际状态
5. 提交 plugin_loader Sprint 5 WIP（14/59 tasks 标记完成），工作树 clean
6. 清理 yaml_json.cpp 2 处 `[DEBUG-removed]` 死注释（闭合 2026-06-09 审计 P1-2）
7. test_plugin_loader.cpp 增强至 ≥ 7 个 test case（ABI mismatch / dlsym / 路径扫描 / RAII / 并发）

**Non-Goals:**
- 不修改 PDK / CognitiveWorker / DomainWorkerPool 公共 API
- 不引入新第三方依赖（仅标准库）
- 不实现 ADR-0007 LLM 压缩 / ADR-0031/0033 实质化（属 P3）
- 不完成 plugin_loader Sprint 5 全 ship（仅 WIP 锁定）

## Decisions

### Decision 1: TopoScheduler 三段式流水线

**方案 A**（已选）：`prepare_dag_state()` + `dispatch_ready_nodes()` + `handle_node_completion()`，每个子函数纯函数式（输入 DAG + Context，输出新状态）

**方案 B**：引入状态机（idle / running / draining / stopped），按状态切换调用各 helper

**选 A 原因**：
- 当前 `execute()` 已是同步阻塞调用，引入状态机增加复杂度
- 3 子函数便于 Catch2 单测覆盖（无需 mock 整个 scheduler）
- 与 `execute_single_branch` (118 行) 保持一致风格

**接口**：
```cpp
class TopoScheduler {
 private:
  // 准备阶段: 解析 + 拓扑排序 + 入度计算
  struct DagState prepare_dag_state(const ParsedGraph& graph, Context& ctx);
  // 派发阶段: 从 ready 队列取出节点并启动 worker
  size_t dispatch_ready_nodes(DagState& state, ExecutionSession& session);
  // 完成阶段: 收集结果 + 失败传播 + 触发下游
  bool handle_node_completion(DagState& state, const NodeResult& result);
  
 public:
  // 编排层: ≤ 60 行
  ExecutionResult execute(const ParsedGraph& graph, Context& ctx);
};
```

### Decision 2: NodeFactoryRegistry 注册表模式

**方案 A**（已选）：`std::unordered_map<NodeType, Factory>` + 全局静态注册

**方案 B**：链式 Builder 模式（每个 NodeType 自带 `register()`）

**选 A 原因**：
- O(1) 查找，与现有 parser hot path 性能对齐
- 静态注册与现有 NodeType 枚举生命周期一致
- 测试时易注入 mock factory

**接口**：
```cpp
// include/agenticdsl/parser/node_factory.h
class NodeFactoryRegistry {
 public:
  using Factory = std::function<std::unique_ptr<Node>(const nlohmann::json&)>;
  
  void register_factory(NodeType type, Factory factory);
  std::unique_ptr<Node> create(NodeType type, const nlohmann::json& spec) const;
  bool has_factory(NodeType type) const;
  size_t size() const;
  
  // 全局注册表 (单例, 线程安全初始化)
  static NodeFactoryRegistry& global();
  
 private:
  mutable std::shared_mutex mutex_;
  std::unordered_map<NodeType, Factory> factories_;
};

// src/modules/parser/markdown_parser.cpp
static bool _ = [] {
  NodeFactoryRegistry::global().register_factory(NodeType::LLM, make_llm_node);
  NodeFactoryRegistry::global().register_factory(NodeType::Tool, make_tool_node);
  // ... 13 个现有 NodeType
  return true;
}();
```

### Decision 3: engine.cpp 工厂模式

**方案 A**（已选）：各模块暴露 `create_*()` 自由函数，engine.cpp 仅 include contract 抽象

**方案 B**：引入 `EngineBuilder` 模式（类似 `std::async(std::launch::async, ...)`）

**选 A 原因**：
- 与现有 `make_unique<ToolRegistry>()` 模式最小差异
- engine.cpp 头部 include 最小化（仅 types + 2 contract）
- 各模块维护自己的工厂实现

**新工厂**：
```cpp
// src/modules/scheduler/factory.h
namespace agenticdsl::scheduler {
  std::unique_ptr<IScheduler> create(const SchedulerConfig& cfg = {});
}

// src/modules/budget/factory.h
namespace agenticdsl::budget {
  std::unique_ptr<IBudgetController> create_controller(const BudgetConfig& cfg = {});
}

// src/common/llm/factory.h
namespace agenticdsl::llm {
  std::unique_ptr<IProviderFactory> create_provider_factory(const LLMConfig& cfg = {});
}

// src/core/engine.cpp - after refactor:
#include "agenticdsl/contract/ischeduler.h"      // 接口
#include "agenticdsl/contract/ibudget_controller.h" // 接口 (新建)
#include "agenticdsl/contract/iprovider_factory.h"  // 接口
// 跨模块 include: 0 (此前 10)
auto scheduler = agenticdsl::scheduler::create();
auto budget = agenticdsl::budget::create_controller();
auto provider_factory = agenticdsl::llm::create_provider_factory();
```

### Decision 4: ADR 文档同步策略

**方案 A**（已选）：状态字段直接修改 + 引用本 OpenSpec change 作为变更依据

**方案 B**：创建新 ADR (`adr-0020-resolution-2026-06-21.md`) 单独记录

**选 A 原因**：
- 符合现有 ADR 状态字段规范（顶部单行状态）
- 引用本 change 提供完整审计链
- 避免 ADR 数量膨胀（已有 17 个）

## Risks / Trade-offs

| 风险 | 影响 | 缓解 |
|---|---|---|
| [RISK-1] scheduler 重构引入回归 | 🔴 高 | (1) 保留 `execute_single_branch` 不动 (2) 提取纯函数前后逐行对比 (3) ≥ 7 个新 test case (4) TSan + ASan 全矩阵验证 |
| [RISK-2] NodeFactoryRegistry 静态注册顺序未定义 | 🟡 中 | (1) 使用 `std::shared_mutex` 保护查找 (2) 注册阶段在 main() 之前完成 (3) 单测验证并发读取 |
| [RISK-3] engine.cpp 工厂化暴露隐藏依赖 | 🟡 中 | (1) 渐进式迁移（每次 1 个工厂）(2) 每步验证 ctest 32/32 (3) 引入 `IBudgetController` 抽象 |
| [RISK-4] plugin_loader WIP 提交影响后续 ship | 🟢 低 | (1) commit message 标注 WIP (2) tasks.md 14/59 进度明确 (3) 不影响 main 分支 |
| [RISK-5] ADR 状态更新与代码不同步 | 🟢 低 | (1) commit message 引用本 change (2) docs/audits/2026-07-21-tech-debt-sprint-6.md 记录决议 |
| [RISK-6] CMake 重编译时间增加（NodeFactoryRegistry 全局注册） | 🟢 低 | (1) 注册表仅 .cpp 静态初始化 (2) 无新增头文件依赖 |
| [RISK-7] 新增 `IBudgetController` 抽象但 engine.cpp 仍直接 include budget.h | 🟡 中 | (1) 与 ADR-0019 §1.4 同步推进 (2) 后续 Sprint 7 闭环 |

## Migration Plan

**Sprint 6a (W1, 1 周)** — P0 全部 + P1-5
- W1D1: Action 1 (plugin_loader WIP) + Action 3 (yaml_json 死注释) + Action 2 (ADR 同步)
- W1D2-3: Action 5 (plugin_loader 测试增强 7 个 case)
- W1D4-5: 回归测试 + WIP commit 验证

**Sprint 6b (W2, 1 周)** — P1-4 scheduler 拆分
- W2D1: Decision 1 接口设计 + 纯函数提取 (`prepare_dag_state`)
- W2D2: `dispatch_ready_nodes` + `handle_node_completion` 提取
- W2D3: `execute()` 编排层重写 (≤ 60 行)
- W2D4-5: ≥ 7 个新 test case + ctest + TSan

**Sprint 6c (W3, 1 周)** — P2-6 + P2-7 架构层
- W3D1-2: Action 6 (NodeFactoryRegistry 实施 + 13 个 NodeType 迁移)
- W3D3-4: Action 7 (engine.cpp 工厂化 + 3 个工厂函数 + IBudgetController 抽象)
- W3D5: 全量回归 + ADR/docs 同步 + OpenSpec archive 准备

**回滚策略**：
- 每个 Action 独立 commit, revert 单 commit 即可回滚
- scheduler 重构保留 `execute_single_branch` 作 fallback（不删除旧路径）
- NodeFactoryRegistry 失败时降级到原 if-else 分发

**部署检查**：
- 每个 Sprint 结束前必须 `ctest --output-on-failure` 全 pass
- 每个 commit 必须 `lsp_diagnostics` 干净
- W3 结束 `python3 tools/adr_lint.py docs/adr/` exit 0

## Open Questions

1. **`IBudgetController` 抽象是否本 Sprint 引入**？当前 BudgetController 是 struct (POD-style)，提升为接口需更多设计。建议 W3 Action 7.3 评估工作量，超出则延后 Sprint 7。
2. **`execute_single_branch` 是否同步拆分**？当前 118 行可接受，但若 execute 重构成功可顺势拆分。决策点：W2D3 review。
3. **plugin_loader Sprint 5 余下 45 tasks 是否需要单独的 change archive**？建议 W1D5 评估，与本 change 解耦。
4. **NodeFactoryRegistry 是否暴露给 PDK 用户**？影响 ADR-0022 §4.2 演进方向。建议 W3D2 review。