// agenticdsl/core/engine.h
// 文件头注释
// 功能描述：DSLEngine 入口 —— 加载 DSL、运行图、暴露 session 级别的 cost 跟踪
// 设计依据：tech-debt-and-doc-cleanup 阶段 4 任务 4.3 (REQ-cost-tracker-integration)
//          + project-organization 计划 Stage 4 / Task 19 (engine.h 解耦第一阶段)
//          + Phase 1 Sprint 1b (S1b.T1): IInteractionBus 集成 (ADR-0019 P2)
//          + Phase 1 P1.T1-T1.4 (2026-06-18): IProviderFactory 注入, mock_provider.h 移除
//          + Phase 1 P1.T2 (2026-06-18): IToolRegistry 抽象接口
//          + Phase 1 P1.T4 (2026-06-18): tool_registry_ PIMPL-lite 化, 移除 tools/registry.h
// 作者：tech-debt-and-doc-cleanup change + Phase 1 P1
// 最后修改日期：2026-06-18 [Phase 1 P1.T4: tool_registry_ PIMPL-lite 化, 移除 tools/registry.h]

#ifndef AGENTICDSL_CORE_ENGINE_H
#define AGENTICDSL_CORE_ENGINE_H

// === Stage 4 / Task 19 + Phase 1 P1: COMPLETE decoupling (5 modules/ + 1 common/ 移除, 仅留 types 例外) ===
// 已移除：
//   - modules/scheduler/topo_scheduler.h (PIMPL-lite, 仅 .cpp 用)
//   - modules/parser/markdown_parser.h (PIMPL-lite, 仅 .cpp 用)
//   - modules/budget/budget_controller.h (PIMPL-lite, unique_ptr + 类外 accessor/destructor)
//   - modules/trace/trace_exporter.h (P1.T3: TraceRecord 上移到 include/agenticdsl/types/)
//   - common/llm/mock_provider.h (P1.T1: IProviderFactory 抽象, engine.h 改用接口)
//   - common/tools/registry.h (P1.T4: IToolRegistry 抽象, tool_registry_ 改 unique_ptr<IToolRegistry>)
// 改为 contract 抽象接口: IScheduler / IParser / IInteractionBus / IProviderFactory / IToolRegistry
//
// 当前保留 (1 头文件, types 头文件例外):
//   - common/llm/llm_types.h (ILLMProvider, ILLMTool, LLMParams 接口 — types 头文件)
//   - include/agenticdsl/types/trace_record.h (data-only struct, ADR-0019 §1.4 退出标准通过)

#include "common/llm/llm_types.h"      // ILLMProvider*, ILLMTool, LLMParams 接口 (types 头文件例外, 保留)
#include "agenticdsl/contract/ischeduler.h" // IScheduler 抽象接口 (Stage 4 / Task 16)
#include "agenticdsl/contract/iparser.h"    // IParser 抽象接口 (Stage 4 / Task 16)
#include "agenticdsl/contract/iinteraction_bus.h" // Phase 1 Sprint 1b (S1b.T1): IInteractionBus 注入契约 (ADR-0019 P2)
// P1.T1 (2026-06-18): IProviderFactory contract 抽象 (替代 common/llm/mock_provider.h 直接 include)
// LLMProviderFactory 路由类在 src/common/llm/llm_provider_factory.h (PIMPL-lite, 完整类型仅 .cpp 可见)
#include "agenticdsl/contract/iprovider_factory.h" // IProviderFactory 抽象 (P1.T1, 替代 mock_provider.h)
// P1.T2 (2026-06-18): IToolRegistry contract 抽象 (替代 common/tools/registry.h 直接 include)
// ToolRegistry 完整类型仅在 .cpp 可见 (PIMPL-lite)
#include "agenticdsl/contract/itool_registry.h"

namespace hydraforge { class PluginLoader; } // IToolRegistry 抽象 (P1.T2, 替代 tools/registry.h)
// P1.T3 (2026-06-18): TraceRecord data-only struct 上移到 types 头文件 (from modules/trace/trace_exporter.h)
#include "agenticdsl/types/trace_record.h" // TraceRecord data-only struct (P1.T3 迁移自 modules/trace/trace_exporter.h)
// Sprint 20 / migrate-context-to-layered: LayeredContext (5-层结构化, ADR-0008) + 桥接辅助
#include "agenticdsl/types/layered_context.h"
#include "agenticdsl/types/context_flatten.h"

// ADR-0033 Session Hierarchy: 三层会话模型 (Sprint 15 / C5)
#include "core/types/session.h"
#include "agenticdsl/types/session_registry_fwd.h"

// ADR-0031 (2026-07-31): Policy + ApprovalHandler 集成
#include "common/policy/policy_factory.h"
#include "common/policy/approval_handler.h"
#include "common/policy/approval_callbacks.h"

// SessionRegistry (C11 Phase 5 Stage 1)
#include "core/types/session_registry.h"

#include <memory>
#include <string>

namespace agenticdsl {

class ILLMProvider; // C₁.4: 前向声明（已在 common/llm/llm_types.h 中前向声明）
class IToolRegistry; // P1.T2: 前向声明 (PIMPL-lite 解耦 — unique_ptr + 类外 accessor)
class IProviderFactory; // P1.T1: 前向声明 (PIMPL-lite 解耦 — unique_ptr + 类外构造)
class IBudgetController; // C1 Day 6.2 (2026-06-27): 抽象接口取代具体类 (ADR-0019 §1.4 延伸)
class ToolCoordinator; // C4 Sprint 14 (ADR-0031 P3-P4): 前向声明 (PIMPL-lite 保持)

class DSLEngine {
public:
    static std::unique_ptr<DSLEngine> from_markdown(const std::string& markdown_content);
    static std::unique_ptr<DSLEngine> from_file(const std::string& file_path);

    // Sprint 20 (2026-07-01) / OpenSpec migrate-context-to-layered:
    // 推荐签名 — 接受 LayeredContext (5-层结构化, ADR-0008)。
    ExecutionResult run(const LayeredContext& ctx);

[[deprecated("use LayeredContext overload (Sprint 20 / ADR-0008)")]]
    ExecutionResult run(const Context& context = Context{});

    // ADR-0033 Session Hierarchy (Sprint 15 / C5): 会话感知接口
    // Context 版本（主入口）— message 写入 ctx["user_input"]
    ExecutionResult run(UserSession& user_sess, const std::string& message,
                        const Context& initial_ctx = Context{});
    // LayeredContext 桥接版本 — 委托到 Context 版本
    ExecutionResult run(UserSession& user_sess, const std::string& message,
                        const LayeredContext& initial_lctx);

    void continue_with_generated_dsl(const std::string& generated_dsl);
    void append_graphs(std::vector<ParsedGraph> new_graphs);

    // P1.T2 + C6: register_tool 模板改用 register_tool_function 虚函数 (避免模板 virtual)
    // C6: 增加 ToolMetadata 参数 — 所有调用点需补 ToolMetadata (BREAKING)
    template <typename Func>
    void register_tool(std::string_view name, ToolMetadata meta, Func&& func) {
        // 委托到 IToolRegistry::register_tool_function (类型擦除 std::function)
        tool_registry_->register_tool_function(
            std::string(name),
            std::move(meta),
            [fn = std::forward<Func>(func)](
                const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
                return fn(args);
            });
    }

    void register_llm_tool(std::string name, std::unique_ptr<ILLMTool> tool, const LLMParams& default_params = {});

    // P1.T4: get_tool_registry() 返回 IToolRegistry& (PIMPL-lite 化后)
    // SimpleCognitiveOrchestrator 已改为 IToolRegistry* (P1.T2 依赖倒置), 6 个调用点零修改
    IToolRegistry& get_tool_registry() { return *tool_registry_; }
    const IToolRegistry& get_tool_registry() const { return *tool_registry_; }

    // C11: SessionRegistry 访问器
    SessionRegistry* get_session_registry();
    const SessionRegistry* get_session_registry() const;

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
    IBudgetController& get_budget_controller();
    const IBudgetController& get_budget_controller() const;

    // === Phase 1 Sprint 1b (S1b.T1): IInteractionBus 注入 API (ADR-0019 P2) ===
    // 注入/访问 IInteractionBus 实例；nullptr 表示未注入（emit/subscribe 走静默 no-op）
    void set_interaction_bus(std::shared_ptr<IInteractionBus> bus);
    std::shared_ptr<IInteractionBus> get_interaction_bus() const;

    // 订阅事件 topic：透传到注入 bus 的 subscribe()，返回 token（0 表示未注入 bus）
    size_t subscribe(const std::string& topic,
                     std::function<void(const ToolResult&)> cb);

    // ADR-0031 (2026-07-31): 设置执行策略模式 (Plan/Agent/Yolo)
    void set_execution_policy(PolicyMode mode);

// C4 Sprint 14 (ADR-0031 P3-P4, Oracle §决策 5): 获取 ToolCoordinator (opt-in, 可能返回 nullptr)
ToolCoordinator* get_tool_coordinator() { return tool_coordinator_.get(); }

// C4 Sprint 14 (ADR-0031 P3-P4, Oracle §决策 5): 显式激活 ToolCoordinator (opt-in, 向后兼容)
void set_tool_coordinator(std::unique_ptr<ToolCoordinator> coordinator);

    // D5 (C14, decisions-2026-07-07.md): 显式加载 PDK plugin (删除默认注入)
    // 返回 true 表示加载成功; false 表示 .so 不存在或加载失败
    // 使用示例: engine->load_plugin("pdk/llama_engine");
    bool load_plugin(const std::string& plugin_name);

    ~DSLEngine(); // Stage 4 / Task 19 + P1.T4: 显式声明 — 头文件外定义, 使 unique_ptr<IBudgetController> + unique_ptr<IToolRegistry> 析构在完整类型下进行
    DSLEngine(std::vector<ParsedGraph> initial_graphs);
private:
    // ADR-0033 Session Hierarchy (Sprint 15 / C5): 内部执行委托
    ExecutionResult run_impl(TaskSession& task_sess, const std::string& message);

    std::vector<ParsedGraph> full_graphs_;
    std::unique_ptr<IToolRegistry> tool_registry_; // P1.T4: PIMPL-lite 化 (从 ToolRegistry 值成员改为 unique_ptr<IToolRegistry>)
    std::unique_ptr<SessionRegistry> session_registry_; // C11: PIMPL-lite, 与 tool_registry_ 模式一致
    std::unique_ptr<ILLMProvider> llm_provider_; // C₁.4: 默认 MockLLMProvider
    std::unique_ptr<IProviderFactory> provider_factory_; // P1.T1: 默认 LLMProviderFactory (PIMPL-lite)
    std::vector<TraceRecord> last_traces_; // ← 存储 Trace
    std::unique_ptr<IBudgetController> budget_controller_; // C1 Day 6.2: IBudgetController 抽象接口 (持有 CostTracker)

    // Phase 1 Sprint 1b (S1b.T1): 默认 nullptr；持有所有权（shared_ptr 允许多 consumer 共享）
    std::shared_ptr<IInteractionBus> bus_;

    // ADR-0031 (2026-07-31): 执行策略 + 审批处理器 (shared_ptr 被 ApprovalHandler 共享)
    std::shared_ptr<IExecutionPolicy> policy_;
    std::unique_ptr<ApprovalHandler> approval_handler_;

    // C4 Sprint 14 (ADR-0031 P3-P4, Oracle §决策 5): ToolCoordinator
    std::unique_ptr<ToolCoordinator> tool_coordinator_;

    // D5 (C14): 显式 plugin 加载器 — DSLEngine 不再默认注入 plugin
    std::unique_ptr<hydraforge::PluginLoader> plugin_loader_;
};

} // namespace agenticdsl

#endif
