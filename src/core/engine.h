// agenticdsl/core/engine.h
// 文件头注释
// 功能描述：DSLEngine 入口 —— 加载 DSL、运行图、暴露 session 级别的 cost 跟踪
// 设计依据：tech-debt-and-doc-cleanup 阶段 4 任务 4.3 (REQ-cost-tracker-integration)
//          + project-organization 计划 Stage 4 / Task 19 (engine.h 解耦第一阶段)
// 作者：tech-debt-and-doc-cleanup change
// 最后修改日期：2026-06-12 (Stage 4 / Task 19 — FULL decoupling, 3/3 modules/ includes removed)

#ifndef AGENTICDSL_CORE_ENGINE_H
#define AGENTICDSL_CORE_ENGINE_H

// === Stage 4 / Task 19: FULL decoupling achieved (3/3 deep modules/ includes removed) ===
// 已移除：topo_scheduler.h、markdown_parser.h（这 2 个仅在 .cpp 中使用，
//        engine.h 本身不引用 TopoScheduler / MarkdownParser 类型，
//        故 include 是纯传递依赖，删除对编译无影响）。
// 已移除：budget_controller.h（原本因 BudgetController 是 DSLEngine 的成员类型、
//        内联 accessor 需要完整类型而保留。本 Task 采用 PIMPL-lite 技术解耦：
//        前向声明 + std::unique_ptr<BudgetController> 成员 + 类外定义
//        accessor 与 destructor，从而将完整类型依赖限制在 engine.cpp 内）。
// 改为：移除的 2 个（topo_scheduler / markdown_parser）被替换为 contract 抽象接口
//      （IScheduler / IParser）作为后续 Task 17-21 的演进基础。
// 保留：3 个 common/ 头文件 + 1 个 leaf modules/trace/ 头文件（见下方说明）。

#include "common/llm/llm_types.h"      // ILLMProvider*, ILLMTool, LLMParams 接口 (保留)
#include "common/llm/mock_provider.h"  // 默认 LLM provider 实现 (保留)
#include "common/tools/registry.h"     // ToolRegistry 成员 (保留 - Task 20 处理)
#include "agenticdsl/contract/ischeduler.h" // IScheduler 抽象接口 (Stage 4 / Task 16)
#include "agenticdsl/contract/iparser.h"    // IParser 抽象接口 (Stage 4 / Task 16)
// 例外：TraceRecord 当前仅由 engine 暴露给外部 (get_last_traces 返回 std::vector<TraceRecord>)。
// 该类型是 POD 结构体，定义在 modules/trace/trace_exporter.h。
// 完整解耦需在后续 Task 将 TraceRecord 上移到 include/agenticdsl/types/ 或 contract 层。
#include "modules/trace/trace_exporter.h" // TraceRecord POD 定义 (Stage 4+ 待迁移)

#include <memory>
#include <string>

// TODO(Stage 4 / Task 20-21): 进一步解耦 ToolRegistry（BudgetController 已 PIMPL-lite 解耦）。
//                              ToolRegistry 仍直接返回具体类型 (ToolRegistry&)，
//                              完整解耦需先扩展 API 表面 (breaking change，留待独立 ADR)。

namespace agenticdsl {

class ILLMProvider; // C₁.4: 前向声明（已在 common/llm/llm_types.h 中前向声明）
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

    ~DSLEngine(); // Stage 4 / Task 19: 显式声明 — 头文件外定义，使 unique_ptr<BudgetController> 析构在完整类型下进行
    DSLEngine(std::vector<ParsedGraph> initial_graphs);
private:

    std::vector<ParsedGraph> full_graphs_;
    ToolRegistry tool_registry_;          // ← 成员变量（非单例）
    std::unique_ptr<ILLMProvider> llm_provider_; // C₁.4: 默认 MockLLMProvider
    std::vector<TraceRecord> last_traces_; // ← 存储 Trace
    std::unique_ptr<BudgetController> budget_controller_; // 阶段 4 任务 4.3: PIMPL-lite — 持有 CostTracker（session 级）
};

} // namespace agenticdsl

#endif
