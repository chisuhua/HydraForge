// agenticdsl/core/engine.h
// 文件头注释
// 功能描述：DSLEngine 入口 —— 加载 DSL、运行图、暴露 session 级别的 cost 跟踪
// 设计依据：tech-debt-and-doc-cleanup 阶段 4 任务 4.3 (REQ-cost-tracker-integration)
// 作者：tech-debt-and-doc-cleanup change
// 最后修改日期：2026-06-10

#ifndef AGENTICDSL_CORE_ENGINE_H
#define AGENTICDSL_CORE_ENGINE_H

#include "modules/scheduler/topo_scheduler.h" // ← 直接依赖 TopoScheduler
#include "modules/parser/markdown_parser.h"
#include "modules/budget/budget_controller.h" // 阶段 4 任务 4.3: 持有 BudgetController 以暴露 session cost
#include "common/llm/llm_types.h" // C₁.4: ILLMProvider 接口（ADR-0001）
#include "common/llm/mock_provider.h" // C₁.4: 默认 provider
#include "common/tools/registry.h"
#include <memory>
#include <string>

namespace agenticdsl {

class ILLMProvider; // C₁.4: 前向声明

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
    BudgetController& get_budget_controller() { return budget_controller_; }
    const BudgetController& get_budget_controller() const { return budget_controller_; }

    DSLEngine(std::vector<ParsedGraph> initial_graphs);
private:

    std::vector<ParsedGraph> full_graphs_;
    ToolRegistry tool_registry_;          // ← 成员变量（非单例）
    std::unique_ptr<ILLMProvider> llm_provider_; // C₁.4: 默认 MockLLMProvider
    std::vector<TraceRecord> last_traces_; // ← 存储 Trace
    BudgetController budget_controller_; // 阶段 4 任务 4.3: 持有 CostTracker（session 级）
};

} // namespace agenticdsl

#endif
