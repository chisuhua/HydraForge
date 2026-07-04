// modules/scheduler/src/execution_session.cpp
#include "scheduler/execution_session.h"
// Sprint 19 D-8: PIMPL-lite — 完整类型移至 .cpp (header 仅前向声明)
#include "modules/context/context_engine.h"
#include "modules/budget/budget_controller.h"
#include "modules/trace/trace_exporter.h"
#include "modules/executor/node_executor.h"
#include "modules/parser/markdown_parser.h"
#include "modules/library/library_loader.h"
#include "resource_manager.h"
#include "common/llm/llm_types.h" // C₁.3: 需要完整 ILLMProvider 定义
#include "common/utils/template_renderer.h" // 引入 InjaTemplateRenderer (for Trace context delta)
//#include "agenticdsl/llm/prompt_builder.h" // 引入 PromptBuilder
#include <stdexcept>
#include <algorithm> // For std::find

namespace agenticdsl {

// C₁.3 迁移：从 LlamaAdapter* 改为 ILLMProvider*
// P1.T4 (2026-06-18): ToolRegistry& → IToolRegistry& (依赖倒置)
// Sprint 19 D-8: PIMPL-lite — 4 个值成员改用 make_unique 构造
ExecutionSession::ExecutionSession(
    const std::string& session_id,
    std::optional<ExecutionBudget> initial_budget,
    IToolRegistry& tool_registry,
    ILLMProvider* llm_provider,
    ResourceManager& resource_manager, // ← 新增
    const std::vector<ParsedGraph>* full_graphs,
    AppendGraphsCallback append_graphs_callback)
    : session_id_(session_id),
      resource_manager_(resource_manager), // ← 初始化 (引用)
      context_engine_(std::make_unique<ContextEngine>()),
      budget_controller_(std::make_unique<BudgetController>(std::move(initial_budget))),
      trace_exporter_(std::make_unique<TraceExporter>()),
      node_executor_(std::make_unique<NodeExecutor>(tool_registry, llm_provider)),
      full_graphs_(full_graphs),
      append_graphs_callback_(std::move(append_graphs_callback)) { // Store callback

    node_executor_->set_append_graphs_callback(append_graphs_callback_);
    if (initial_budget.has_value()) {
        size_t max_snapshots = (initial_budget->max_snapshots >= 0)
            ? static_cast<size_t>(initial_budget->max_snapshots) : 10;
        context_engine_->set_snapshot_limits(max_snapshots, initial_budget->snapshot_max_size_kb);
    } else {
        context_engine_->set_snapshot_limits(10, 512); // dev default
    }
}

// Sprint 19 D-8: PIMPL-lite — 析构 out-of-line (unique_ptr 不完整类型要求)
// 完整类型 (ContextEngine/BudgetController/TraceExporter/NodeExecutor) 在此翻译单元可见,
// 编译器可在 unique_ptr<X>::~unique_ptr() 完整实例化, 无 "incomplete type" 错误。
ExecutionSession::~ExecutionSession() = default;

// C10 Phase 5 Stage 1 Step 0: per-module lazy-init json state
nlohmann::json& ExecutionSession::ensure_module_state(const std::string& module_path) {
    auto it = module_states_.find(module_path);
    if (it == module_states_.end()) {
        it = module_states_.emplace(module_path, nlohmann::json::object()).first;
    }
    return it->second;
}

const nlohmann::json* ExecutionSession::get_module_state(const std::string& module_path) const {
    auto it = module_states_.find(module_path);
    return (it != module_states_.end()) ? &it->second : nullptr;
}

bool ExecutionSession::has_module_state(const std::string& module_path) const {
    return module_states_.find(module_path) != module_states_.end();
}

// C12 Phase 5 Stage 1 Step 2 §4 + §6b: pending_yield_ atomic accessors
// 跨线程安全: 字段级 yield_mutex_ (Oracle Risk 10 mitigation)
void ExecutionSession::set_pending_yield(YieldState state) {
    std::lock_guard<std::mutex> lock(yield_mutex_);
    pending_yield_ = std::move(state);
}

std::optional<YieldState> ExecutionSession::get_pending_yield() const {
    std::lock_guard<std::mutex> lock(yield_mutex_);
    return pending_yield_;
}

void ExecutionSession::clear_pending_yield() {
    std::lock_guard<std::mutex> lock(yield_mutex_);
    pending_yield_.reset();
}

// Sprint 19 D-8: PIMPL-lite — set_approval_handler / set_tool_coordinator 透传逻辑
// 移到 .cpp (header 中 node_executor_ 是 unique_ptr<不完整类型>, 不可在 header 解引用)。
void ExecutionSession::set_approval_handler(IApprovalHandler* handler) {
    if (node_executor_) {
        node_executor_->set_approval_handler(handler);
    }
}

void ExecutionSession::set_tool_coordinator(ToolCoordinator* coordinator) {
    if (node_executor_) {
        node_executor_->set_tool_coordinator(coordinator);
    }
}

nlohmann::json ExecutionSession::build_available_subgraphs_context() const {
    nlohmann::json libs = nlohmann::json::array();

    // 1. 静态标准库（/lib/**）
    auto& loader = StandardLibraryLoader::instance();
    for (const auto& entry : loader.get_available_libraries()) {
        if (entry.is_subgraph) {
            nlohmann::json lib;
            lib["path"] = entry.path;
            if (entry.output_schema && !entry.output_schema->is_null()) {
                lib["signature"] = {
                    {"outputs", entry.output_schema}
                };
            }
            lib["permissions"] = entry.permissions;
            lib["stability"] = "stable";
            libs.push_back(std::move(lib));
        }
    }

    // 2. 动态生成子图（/dynamic/**）
    if (full_graphs_) {
        for (const auto& graph : *full_graphs_) {
            if (graph.path.rfind("/dynamic/", 0) == 0 && graph.output_schema && !graph.output_schema->is_null()) {
                nlohmann::json lib;
                lib["path"] = graph.path;
                lib["signature"] = {
                    {"outputs", graph.output_schema}
                };
                lib["permissions"] = graph.permissions;
                lib["stability"] = "dynamic";
                libs.push_back(std::move(lib));
            }
        }
    }

    return libs;
}

std::string ExecutionSession::inject_subgraphs_into_prompt(
    const std::string& base_prompt,
    const Context& context) const {
    Context ctx = context;
    ctx["available_subgraphs"] = build_available_subgraphs_context();
    return InjaTemplateRenderer::render(base_prompt, ctx);
}


ExecutionSession::ExecutionResult ExecutionSession::execute_node(Node* node, const Context& initial_context) {
    ExecutionResult result;
    result.success = true;
    result.message = "Node executed successfully";

    Context context_with_resources = initial_context;
    auto resources_ctx = resource_manager_.get_resources_context(); // ← 需要 ExecutionSession 持有 resource_manager_
    if (!resources_ctx.empty()) {
        context_with_resources["resources"] = std::move(resources_ctx);
    }

    // C10 Phase 5 Stage 1 Step 0: inject module_states_ for dsl_call access
    // 将 per-module 持久化状态序列化为 context 键, dsl_call 内部可引用/修改后由下方同步回
    nlohmann::json module_states_json = nlohmann::json::object();
    for (const auto& [path, state] : module_states_) {
        module_states_json[path] = state;
    }
    context_with_resources["__module_states__"] = std::move(module_states_json);

    // v3.1: Check for snapshot trigger BEFORE execution
    bool snapshot_needed = needs_snapshot(node);
    if (snapshot_needed) {
        context_engine_->save_snapshot(node->path, context_with_resources); // Snapshot *before* execution
        result.snapshot_key = node->path;
    }

    // 1. 检查预算
    if (node->type == NodeType::DSL_CALL) {
        if (!budget_controller_->try_consume_llm_call()) {
            result.success = false;
            result.message = "Budget exceeded: LLM call limit reached";
            return result;
        }
    }
    if (node->type == NodeType::GENERATE_SUBGRAPH) {
        if (!budget_controller_->try_consume_subgraph_depth()) { // Assume BudgetController has this method
            result.success = false;
            result.message = "Budget exceeded: Subgraph depth limit reached";
            return result;
        }
    }
    if (!budget_controller_->try_consume_node()) {
        result.success = false;
        result.message = "Budget exceeded: Node limit reached";
        return result;
    }

    // 2. 记录 Trace 开始
    nlohmann::json initial_ctx_json = context_with_resources;
    trace_exporter_->on_node_start(node->path, node->type, initial_ctx_json, budget_controller_->get_budget());

    // 3. 执行节点
    try {
        // 统一执行路径
        BudgetChecker yield_checker = [this]() { return !budget_controller_->exceeded(); };

        auto execution_result = context_engine_->execute_with_snapshot(
            [this, node, yield_checker](const Context& ctx) {
                if (node->type == NodeType::GENERATE_SUBGRAPH) {
                    const GenerateSubgraphNode* gsn = static_cast<const GenerateSubgraphNode*>(node);
                    std::string rendered_prompt = this->inject_subgraphs_into_prompt(gsn->prompt_template, ctx);
                    Context new_ctx = ctx;
                    new_ctx["__rendered_prompt__"] = rendered_prompt;
                    return node_executor_->execute_node(node, new_ctx, yield_checker);
                }
                return node_executor_->execute_node(node, ctx, yield_checker);
            },
            context_with_resources,
            snapshot_needed,
            node->path
        );

        result.new_context = std::move(execution_result.new_context);
        result.snapshot_key = execution_result.snapshot_key;

        // C10: sync module_states_ changes back from context after dsl_call
        if (result.new_context.contains("__module_states__") && result.new_context["__module_states__"].is_object()) {
            for (auto it = result.new_context["__module_states__"].begin();
                 it != result.new_context["__module_states__"].end(); ++it) {
                module_states_[it.key()] = it.value();
            }
        }

        // C12 §5: YIELD 后处理 — 标记 pending_yield_ + paused_at (TopoScheduler 据此挂起主循环)
        if (node->type == NodeType::YIELD && result.new_context.contains("__yield_mode__")) {
            YieldState ys;
            ys.module_path = node->path;
            ys.resume_context = nlohmann::json::object();
            set_pending_yield(std::move(ys));
            result.paused_at = node->path;
        }

        // --- v3.1: Check for LLM Call Pause ---
        if (node->type == NodeType::DSL_CALL) {
             result.paused_at = node->path;
        }

    } catch (const std::exception& e) {
        result.success = false;
        result.message = std::string("Node execution failed: ") + e.what();
        // DSL_CALL nodes that fail (e.g., LLM unavailable) should pause so caller can supply content
        if (node->type == NodeType::DSL_CALL) {
            result.paused_at = node->path;
        }
    }

    // 4. 记录 Trace 结束
    nlohmann::json final_ctx_json = result.new_context;
    trace_exporter_->on_node_end(
        node->path,
        result.success ? "success" : "failed",
        result.success ? std::nullopt : std::make_optional(result.message),
        initial_ctx_json,
        final_ctx_json,
        result.snapshot_key,
        budget_controller_->get_budget()
    );

    return result;
}
void ExecutionSession::check_and_requeue_dynamic_deps(const std::unordered_set<NodePath>& newly_executed_nodes) {
    std::vector<NodePath> satisfied_nodes;
    for (auto it = pending_dynamic_deps_.begin(); it != pending_dynamic_deps_.end();) {
        NodePath node_path = it->first;
        std::vector<NodePath>& deps = it->second;

        // Remove deps that were just executed
        deps.erase(std::remove_if(deps.begin(), deps.end(),
                                  [&newly_executed_nodes](const NodePath& dep) {
                                      return newly_executed_nodes.count(dep) > 0;
                                  }),
                   deps.end());

        if (deps.empty()) {
            // All dependencies for this node are now satisfied
            satisfied_nodes.push_back(node_path);
            it = pending_dynamic_deps_.erase(it); // Remove from pending list
        } else {
            ++it;
        }
    }

}

bool ExecutionSession::is_budget_exceeded() const {
    return budget_controller_->exceeded();
}

const TraceExporter& ExecutionSession::get_trace_exporter() const {
    return *trace_exporter_;
}

const BudgetController& ExecutionSession::get_budget_controller() const {
    return *budget_controller_;
}

const ContextEngine& ExecutionSession::get_context_engine() const {
    return *context_engine_;
}

bool ExecutionSession::needs_snapshot(Node* node) const {
    // v3.1: Automatic snapshot triggers
    if (node->metadata.contains("snapshot_before_execution") &&
        node->metadata["snapshot_before_execution"].get<bool>()) {
        return true;
    }

    // Check for a hypothetical 'assert' node type (requires enum update)
    // if (node->type == NodeType::ASSERT) { // Assuming this enum value exists
    //     return true;
    // }

    // Check for tool_call with rollback flag (requires metadata field)
    if (node->type == NodeType::TOOL_CALL) {
        if (node->metadata.contains("rollback_on_failure") && node->metadata["rollback_on_failure"].get<bool>()) {
            return true;
        }
    }

    // v3.1: Fork, GenerateSubgraph, Assert always trigger
    if (node->type == NodeType::FORK || node->type == NodeType::GENERATE_SUBGRAPH || node->type == NodeType::ASSERT /* Assume assert exists */) {
        return true;
    }
    return false;
}

std::vector<NodePath> ExecutionSession::parse_dynamic_wait_for(const nlohmann::json& expr, const Context& ctx) {
    std::vector<NodePath> result;
    if (expr.is_string()) {
        std::string rendered = InjaTemplateRenderer::render(expr.get<std::string>(), ctx);
        try {
            auto arr = nlohmann::json::parse(rendered);
            if (arr.is_array()) {
                for (const auto& item : arr) {
                    if (item.is_string()) result.push_back(item.get<std::string>());
                }
            } else if (arr.is_string()) {
                 result.push_back(arr.get<std::string>()); // Single path
            }
        } catch (...) {
            // If parsing fails, treat as single path
            result.push_back(rendered);
        }
    } else if (expr.is_array()) {
        for (const auto& item : expr) {
            if (item.is_string()) {
                result.push_back(InjaTemplateRenderer::render(item.get<std::string>(), ctx));
            }
        }
    }
    return result;
}


} // namespace agenticdsl