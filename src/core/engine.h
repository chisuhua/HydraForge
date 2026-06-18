// agenticdsl/core/engine.h
// 文件头注释
// 功能描述：DSLEngine 入口 —— 加载 DSL、运行图、暴露 session 级别的 cost 跟踪
// 设计依据：tech-debt-and-doc-cleanup 阶段 4 任务 4.3 (REQ-cost-tracker-integration)
//          + project-organization 计划 Stage 4 / Task 19 (engine.h 解耦第一阶段)
//          + Phase 1 Sprint 1b (S1b.T1): IInteractionBus 集成 (ADR-0019 P2)
// 作者：tech-debt-and-doc-cleanup change
// 最后修改日期：2026-06-17 [Phase 1 Sprint 1b S1b.T1: drop common/tools/registry.h include,
//                              forward-declare ToolRegistry, add IInteractionBus 注入 API]

#ifndef AGENTICDSL_CORE_ENGINE_H
#define AGENTICDSL_CORE_ENGINE_H

// === Stage 4 / Task 19: PARTIAL decoupling (3 deep modules/ removed; 1 leaf modules/trace/ + 3 common/ remain; full = future ADR) ===
// 已移除（3 deep modules/）：topo_scheduler.h、markdown_parser.h（这 2 个仅在 .cpp 中使用，
//        engine.h 本身不引用 TopoScheduler / MarkdownParser 类型，
//        故 include 是纯传递依赖，删除对编译无影响）；
//        budget_controller.h（原本因 BudgetController 是 DSLEngine 的成员类型、
//        内联 accessor 需要完整类型而保留。本 Task 采用 PIMPL-lite 技术解耦：
//        前向声明 + std::unique_ptr<BudgetController> 成员 + 类外定义
//        accessor 与 destructor，从而将完整类型依赖限制在 engine.cpp 内）。
// 改为：移除的 2 个（topo_scheduler / markdown_parser）被替换为 contract 抽象接口
//      （IScheduler / IParser）作为后续 Task 17-21 的演进基础。
// === Phase 1 Sprint 1b (S1b.T1): IInteractionBus 注入 API ===
//        新增 set/get_interaction_bus + subscribe(topic, cb) 三个公开方法，
//        委托给注入的 std::shared_ptr<IInteractionBus>。
//        保留 common/tools/registry.h include：DSLEngine 内联模板 register_tool 与
//        get_tool_registry accessor 需要 ToolRegistry 完整类型，多个调用点
//        （tests/test_simple_orchestrator.cpp, examples/agent_basic/main.cpp 等）
//        依赖此传递包含。P1.T4 include 缩减留待后续 Task 20 独立 ADR。
// 保留（3 头文件，需未来 OpenSpec change 处理）：3 个 common/ 头文件。
// P1.T3 (2026-06-18) 已完成：TraceRecord data-only struct 从 modules/trace/trace_exporter.h
//        上移到 include/agenticdsl/types/trace_record.h，engine.h 不再依赖 modules/trace/。
// 验证：本文件当前保留 3 个 common/ 跨模块 include + 1 个 types 头文件 include
//       （ADR-0019 §1.4 退出标准 = 1 个 modules/common include，本文件当前 3 个 common/。
//        T1+T2 工作 (LLMProviderFactory + IToolRegistry) 完成后将达到 1）。
//       完整审计报告见 OpenSpec change `2026-06-15-residual-engine-h-decoupling`。

#include "common/llm/llm_types.h"      // ILLMProvider*, ILLMTool, LLMParams 接口 (保留)
#include "common/tools/registry.h"     // ToolRegistry 成员 + 内联 register_tool/get_tool_registry 完整类型依赖 (P1.T4 遗留 - Task 20 处理)
#include "agenticdsl/contract/ischeduler.h" // IScheduler 抽象接口 (Stage 4 / Task 16)
#include "agenticdsl/contract/iparser.h"    // IParser 抽象接口 (Stage 4 / Task 16)
#include "agenticdsl/contract/iinteraction_bus.h" // Phase 1 Sprint 1b (S1b.T1): IInteractionBus 注入契约 (ADR-0019 P2)
// P1.T1 (2026-06-18): IProviderFactory contract 抽象 (替代 common/llm/mock_provider.h 直接 include)
// LLMProviderFactory 路由类在 src/common/llm/llm_provider_factory.h (PIMPL-lite, 完整类型仅 .cpp 可见)
#include "agenticdsl/contract/iprovider_factory.h" // IProviderFactory 抽象 (P1.T1, 替代 mock_provider.h)
// P1.T3 (2026-06-18): TraceRecord data-only struct 上移到 types 头文件 (from modules/trace/trace_exporter.h)
// 这是 ADR-0019 §1.4 解耦的第一步 — engine.h 不再依赖 modules/trace/, 改依赖 include/agenticdsl/types/
#include "agenticdsl/types/trace_record.h" // TraceRecord data-only struct (P1.T3 迁移自 modules/trace/trace_exporter.h)

#include <memory>
#include <string>

// TODO(Stage 4 / Task 20-21): 进一步解耦 ToolRegistry（BudgetController 已 PIMPL-lite 解耦）。
//                              ToolRegistry 仍直接返回具体类型 (ToolRegistry&)，
//                              完整解耦需先扩展 API 表面 (breaking change，留待独立 ADR)。

namespace agenticdsl {

class ILLMProvider; // C₁.4: 前向声明（已在 common/llm/llm_types.h 中前向声明）
class IProviderFactory; // P1.T1: 前向声明 (PIMPL-lite 解耦 — unique_ptr + 类外构造)
class BudgetController; // Stage 4 / Task 19: 前向声明 (PIMPL-lite 解耦 — unique_ptr + 类外 accessor/destructor)

class DSLEngine {
public:
    static std::unique_ptr<DSLEngine> from_markdown(const std::string& markdown_content);
    static std::unique_ptr<DSLEngine> from_file(const std::string& file_path);

    ExecutionResult run(const Context& context = Context{});
    void continue_with_generated_dsl(const std::string& generated_dsl);
    void append_graphs(std::vector<ParsedGraph> new_graphs);

    template <typename Func>
    void register_tool(std::string_view name, Func&& func) {
        tool_registry_.register_tool(std::string(name), std::forward<Func>(func));
    }

    void register_llm_tool(std::string name, std::unique_ptr<ILLMTool> tool, const LLMParams& default_params = {});
    ToolRegistry& get_tool_registry() { return tool_registry_; }
    const ToolRegistry& get_tool_registry() const { return tool_registry_; }

    std::vector<TraceRecord> get_last_traces() const { return last_traces_; }

    // C₁.4 迁移：从 LlamaAdapter* 改为 ILLMProvider*
    ILLMProvider* get_llm_provider() { return llm_provider_.get(); }

    // C₁.4: 注入自定义 LLM provider（默认是 MockLLMProvider，可被替换为真实 provider）
    void set_llm_provider(std::unique_ptr<ILLMProvider> provider) {
        llm_provider_ = std::move(provider);
    }

    // === 阶段 4 任务 4.3: 暴露 session cost API ===
    // 返回自 DSLEngine 创建（或上次 reset）以来 LLM 调用的累计成本（USD）
    double get_session_cost() const;

    // 访问 session BudgetController（用于更细粒度查询或测试）
    // Stage 4 / Task 19: 类外定义 — 头文件中仅有前向声明，完整类型仅在 engine.cpp 可见
    BudgetController& get_budget_controller();
    const BudgetController& get_budget_controller() const;

    // === Phase 1 Sprint 1b (S1b.T1): IInteractionBus 注入 API (ADR-0019 P2) ===
    // 注入/访问 IInteractionBus 实例；nullptr 表示未注入（emit/subscribe 走静默 no-op）
    void set_interaction_bus(std::shared_ptr<IInteractionBus> bus);
    std::shared_ptr<IInteractionBus> get_interaction_bus() const;

    // 订阅事件 topic：透传到注入 bus 的 subscribe()，返回 token（0 表示未注入 bus）
    size_t subscribe(const std::string& topic,
                     std::function<void(const ToolResult&)> cb);

    ~DSLEngine(); // Stage 4 / Task 19: 显式声明 — 头文件外定义，使 unique_ptr<BudgetController> 析构在完整类型下进行
    DSLEngine(std::vector<ParsedGraph> initial_graphs);
private:

    std::vector<ParsedGraph> full_graphs_;
    ToolRegistry tool_registry_;          // ← 成员变量（非单例）
    std::unique_ptr<ILLMProvider> llm_provider_; // C₁.4: 默认 MockLLMProvider
    std::unique_ptr<IProviderFactory> provider_factory_; // P1.T1: 默认 LLMProviderFactory (PIMPL-lite)
    std::vector<TraceRecord> last_traces_; // ← 存储 Trace
    std::unique_ptr<BudgetController> budget_controller_; // 阶段 4 任务 4.3: PIMPL-lite — 持有 CostTracker（session 级）

    // Phase 1 Sprint 1b (S1b.T1): 默认 nullptr；持有所有权（shared_ptr 允许多 consumer 共享）
    std::shared_ptr<IInteractionBus> bus_;
};

} // namespace agenticdsl

#endif
