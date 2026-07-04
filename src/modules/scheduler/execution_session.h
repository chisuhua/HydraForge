// modules/scheduler/include/scheduler/execution_session.h
#ifndef AGENTICDSL_MODULES_SCHEDULER_EXECUTION_SESSION_H
#define AGENTICDSL_MODULES_SCHEDULER_EXECUTION_SESSION_H

// Sprint 19 D-8: PIMPL-lite 解耦 — 移除 6 个 `modules/` include + 1 个同模块
// `resource_manager.h`, 完整类型移到 execution_session.cpp。前向声明 + unique_ptr
// 模式与 Sprint 18 D.7 (library_loader.h) / Sprint 17 C.4 (topo_scheduler.h) 一致。
#include "core/types/context.h"
#include "core/types/node.h"
#include "core/types/budget.h"
// P1.T4: 改为 IToolRegistry 抽象 (不再拖入 common/tools/registry.h)
#include "agenticdsl/contract/itool_registry.h" // P1.T2: IToolRegistry 抽象接口
// Sprint 19 (OpenSpec pimpl-node-executor-h): 改用 IApprovalHandler 抽象
// (与 NodeExecutor::set_approval_handler(IApprovalHandler*) 对齐)
#include "agenticdsl/policy/iapproval_handler.h"
#include <optional>
#include <vector>
#include <memory>
#include <functional> // For std::function (callback)
#include <unordered_map>
#include <unordered_set>
#include <map> // C10 Phase 5 Stage 1 Step 0: module_states_ per-module json persistence

namespace agenticdsl {

class DSLEngine; // Forward declaration
class ILLMProvider; // C₁.3: 前向声明 ILLMProvider
class ToolCoordinator; // C4 Sprint 14 (ADR-0031 P3-P4): 前向声明

// Sprint 19 D-8: PIMPL-lite 前向声明 7 类 (完整类型在 execution_session.cpp)
class ContextEngine;
class BudgetController;
class TraceExporter;
class NodeExecutor;
class MarkdownParser;
class StandardLibraryLoader;
class ResourceManager;

//
// ExecutionSession 封装了单次执行的所有状态和逻辑
class ExecutionSession {
public:

    using AppendGraphsCallback = std::function<void(std::vector<ParsedGraph>)>;

    // C₁.3 迁移：从 LlamaAdapter* 改为 ILLMProvider*
    // P1.T4 (2026-06-18): ToolRegistry& → IToolRegistry& (依赖倒置)
    ExecutionSession(
        const std::string& session_id,  // C11: Session 标识
        std::optional<ExecutionBudget> initial_budget,
        IToolRegistry& tool_registry,
        ILLMProvider* llm_provider,
        ResourceManager& resource_manager, // ← 新增参数
        const std::vector<ParsedGraph>* full_graphs, // ← 新增：指向完整图集
        AppendGraphsCallback append_graphs_callback = nullptr // New parameter
    );

    // Sprint 19 D-8: PIMPL-lite 要求 unique_ptr 不完整类型成员的析构 out-of-line
    ~ExecutionSession();

    // 执行一个节点，并处理预算、快照、Trace
    struct ExecutionResult {
        Context new_context;
        bool success;
        std::string message;
        std::optional<NodePath> snapshot_key; // 如果触发了快照
        std::optional<NodePath> paused_at; // 如果暂停在 LLM 调用
    };

    ExecutionResult execute_node(Node* node, const Context& initial_context);
    void check_and_requeue_dynamic_deps(const std::unordered_set<NodePath>& newly_executed_nodes);

    // 检查预算是否超限
    bool is_budget_exceeded() const;

    // 获取 Trace 导出器
    const TraceExporter& get_trace_exporter() const;

    // 获取预算控制器
    const BudgetController& get_budget_controller() const;

    // 获取上下文引擎
    const ContextEngine& get_context_engine() const;
    const std::unordered_map<NodePath, std::vector<NodePath>>& get_pending_dynamic_deps() const { return pending_dynamic_deps_; }
    const std::unordered_map<NodePath, nlohmann::json>& get_dynamic_wait_for_expressions() const { return dynamic_wait_for_expressions_; }

    // C10 Phase 5 Stage 1 Step 0: per-module lazy-init json persistence
    // dsl_call 调用间共享模块状态 (如 prefix_cache 累计 token 统计)
    nlohmann::json& ensure_module_state(const std::string& module_path);
    const nlohmann::json* get_module_state(const std::string& module_path) const;
    bool has_module_state(const std::string& module_path) const;

    // C11: Session 标识与 per-run 变量访问器
    const std::string& get_session_id() const { return session_id_; }
    nlohmann::json& get_session_vars() { return session_vars_; }
    const nlohmann::json& get_session_vars() const { return session_vars_; }

    // ADR-0031 (2026-07-31): 注入审批处理器（透传到 NodeExecutor）
    // Sprint 19: 参数类型 ApprovalHandler* → IApprovalHandler* (依赖抽象, ADR-0019 §1.4)
    // Sprint 19 D-8: 透传逻辑移到 .cpp (node_executor_ 是 unique_ptr 不完整类型)
    void set_approval_handler(IApprovalHandler* handler);

    // C4 Sprint 14 (ADR-0031 P3-P4): 注入 ToolCoordinator（透传到 NodeExecutor）
    // Sprint 19 D-8: 透传逻辑移到 .cpp (node_executor_ 是 unique_ptr 不完整类型)
    void set_tool_coordinator(ToolCoordinator* coordinator);

private:
    ResourceManager& resource_manager_; // ← 成员引用 (PIMPL-lite 后保持)
    // Sprint 19 D-8: PIMPL-lite 模式 — 4 个值成员改为 unique_ptr (PIMPL 间接持有)
    std::unique_ptr<ContextEngine> context_engine_;
    std::unique_ptr<BudgetController> budget_controller_;
    std::unique_ptr<TraceExporter> trace_exporter_;
    std::unique_ptr<NodeExecutor> node_executor_;
    const std::vector<ParsedGraph>* full_graphs_; // ← 指向完整图集
    std::vector<NodePath> call_stack_; // 用于 soft end
    std::map<std::string, nlohmann::json> module_states_; // C10 Phase 5 Step 0: per-module lazy state
    std::string session_id_;              // C11: Session 标识
    nlohmann::json session_vars_;         // C11: per-run Session 变量 (json)
    std::unordered_map<NodePath, std::vector<NodePath>> pending_dynamic_deps_; // NodePath -> [list of unresolved deps]
    std::unordered_map<NodePath, nlohmann::json> dynamic_wait_for_expressions_; // NodePath -> original wait_for expression
    AppendGraphsCallback append_graphs_callback_; // Callback for dynamic graphs

    //Context execute_generate_subgraph_with_callback(const GenerateSubgraphNode* node, const Context& ctx);
    nlohmann::json build_available_subgraphs_context() const;
    std::string inject_subgraphs_into_prompt(const std::string& base_prompt, const Context& context) const;

    // Helper to determine if snapshot is needed for a node type
    bool needs_snapshot(Node* node) const;
    std::vector<NodePath> parse_dynamic_wait_for(const nlohmann::json& expr, const Context& ctx);
friend class TopoScheduler; // Grant TopoScheduler access to private members
};

} // namespace agenticdsl

#endif // AGENTICDSL_MODULES_SCHEDULER_EXECUTION_SESSION_H