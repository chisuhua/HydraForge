// agenticdsl/core/engine.h
#ifndef AGENTICDSL_CORE_ENGINE_H
#define AGENTICDSL_CORE_ENGINE_H

#include "modules/scheduler/topo_scheduler.h" // ← 直接依赖 TopoScheduler
#include "modules/parser/markdown_parser.h"
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

    DSLEngine(std::vector<ParsedGraph> initial_graphs);
private:

    std::vector<ParsedGraph> full_graphs_;
    ToolRegistry tool_registry_;          // ← 成员变量（非单例）
    std::unique_ptr<ILLMProvider> llm_provider_; // C₁.4: 默认 MockLLMProvider
    std::vector<TraceRecord> last_traces_; // ← 存储 Trace
};

} // namespace agenticdsl

#endif
