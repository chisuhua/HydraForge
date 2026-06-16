// modules/executor/src/node_executor.cpp
#include "executor/node_executor.h"
#include "common/llm/llm_types.h" // C₁.2: 需要完整定义（生成 GenerationRequest/ILLMProvider）
#include "common/log/log.h"  // agenticdsl::log facade
#include "common/utils/template_renderer.h" // 引入 InjaTemplateRenderer (for rendering)
#include "core/types/tool_result.h" // Phase 1 Sprint 1a (S1a.T2): ToolResult envelope parsing (ADR-0023)
#include "modules/parser/markdown_parser.h" // Stage 4 Task 20: 仅在 .cpp 内用于默认 shim，不再被 header 引入
#include <stdexcept>
#include <inja/inja.hpp> // For RenderError
#include <algorithm> // For std::find
#include <thread> // For std::this_thread::sleep_for (if needed for mock)
#include <chrono> // For std::chrono_literals + std::chrono::steady_clock
#include <string>

namespace agenticdsl {

// C₁.2 迁移：构造函数从 LlamaAdapter* 改为 ILLMProvider*
// Stage 4 Task 20: 此构造函数保留为 shim，内部创建默认 MarkdownParser 并委托给主构造函数
NodeExecutor::NodeExecutor(ToolRegistry& tool_registry, ILLMProvider* llm_provider)
    : NodeExecutor(tool_registry, llm_provider,
                   std::make_unique<MarkdownParser>()) {
    // llm_provider_ 可能为 nullptr，NodeExecutor 需要处理这种情况
}

// Stage 4 Task 20: 主构造函数，通过 IParser 抽象注入具体解析器
NodeExecutor::NodeExecutor(ToolRegistry& tool_registry, ILLMProvider* llm_provider,
                            std::unique_ptr<IParser> parser)
    : tool_registry_(tool_registry), llm_provider_(llm_provider), parser_(std::move(parser)) {}

Context NodeExecutor::execute_node(Node* node, const Context& ctx) {
    Context context_with_resources = ctx;

    check_permissions(node->permissions, node->path);

    // 根据节点类型分发执行
    switch (node->type) {
        case NodeType::START:
            return execute_start(dynamic_cast<const StartNode*>(node), context_with_resources);
        case NodeType::END:
            return execute_end(dynamic_cast<const EndNode*>(node), context_with_resources);
        case NodeType::ASSIGN:
            return execute_assign(dynamic_cast<const AssignNode*>(node), context_with_resources);
        case NodeType::DSL_CALL:
            return execute_dsl_node(dynamic_cast<const DSLNode*>(node), context_with_resources);
        case NodeType::TOOL_CALL:
            return execute_tool_call(dynamic_cast<const ToolCallNode*>(node), context_with_resources);
        case NodeType::RESOURCE:
            return execute_resource(dynamic_cast<const ResourceNode*>(node), context_with_resources);
        case NodeType::FORK:
            return execute_fork(dynamic_cast<const ForkNode*>(node), context_with_resources);
        case NodeType::JOIN:
            return execute_join(dynamic_cast<const JoinNode*>(node), context_with_resources);
        case NodeType::GENERATE_SUBGRAPH:
            return execute_generate_subgraph(dynamic_cast<const GenerateSubgraphNode*>(node), context_with_resources);
        case NodeType::ASSERT:
            return execute_assert(dynamic_cast<const AssertNode*>(node), context_with_resources);
        default:
            throw std::runtime_error("Unknown node type during execution: " + std::to_string(static_cast<int>(node->type)));
    }
}

void NodeExecutor::check_permissions(const std::vector<std::string>& perms, const NodePath& node_path) {
    // 简单实现：检查是否有权限要求，如果有，假设都需要（实际应用中需更细粒度）
    for (const auto& perm : perms) {
        // Example: "tool: web_search"
        if (perm.substr(0, 5) == "tool:") {
            std::string tool_name = perm.substr(6); // Extract name after "tool: "
            if (!tool_registry_.has_tool(tool_name)) {
                throw std::runtime_error("Permission denied: Tool '" + tool_name + "' not available for node: " + node_path);
            }
        }
        // Add more permission checks as needed (network, file, etc.)
    }
    // 例如，检查 perms 中是否包含 "network" 或 "tool: web_search" 等
    // 这里只是示例，实际权限检查逻辑会更复杂
}

Context NodeExecutor::execute_start(const StartNode* node, const Context& ctx) {
    // Start 节点通常不修改上下文，直接返回
    return ctx;
}

Context NodeExecutor::execute_end(const EndNode* node, const Context& ctx) {
    // End 节点通常不修改上下文，直接返回
    // 其终止逻辑由 TopoScheduler 处理
    return ctx;
}

Context NodeExecutor::execute_assign(const AssignNode* node, const Context& ctx) {
    Context new_context = ctx;
    for (const auto& [key, template_str] : node->assign) {
        try {
            std::string rendered_value = InjaTemplateRenderer::render(template_str, ctx);
            new_context[key] = rendered_value; // 赋值到新的上下文
        } catch (const inja::RenderError& e) {
            throw std::runtime_error("Template rendering failed for key '" + key + "': " + std::string(e.what()));
        }
    }
    return new_context;
}

Context NodeExecutor::execute_dsl_node(const DSLNode* node, const Context& ctx) {
    Context new_context = ctx;

    if (node->output_keys.empty()) {
        throw std::runtime_error("DSLNode has no output_keys: " + node->path);
    }
    const std::string& key = node->output_keys[0];

    // If context already supplies a value for this output_key, treat it as a mock and skip LLM call
    if (ctx.contains(key)) {
        new_context[key] = ctx[key];
        return new_context;
    }

    try {
        // Render prompt template
        std::string rendered_prompt = InjaTemplateRenderer::render(node->prompt_template, ctx);

        // Call LLM via ToolRegistry
        nlohmann::json result = tool_registry_.call_llm_tool(node->llm_tool_name, rendered_prompt, node->llm_params);

        // Check result
        if (!result.value("success", false)) {
            throw std::runtime_error("LLM generation failed: " + result.value("error", "Unknown error"));
        }

        new_context[key] = result["text"].get<std::string>();

    } catch (const inja::RenderError& e) {
        throw std::runtime_error("Prompt template rendering failed for node '" + node->path + "': " + std::string(e.what()));
    }

    return new_context;
}

Context NodeExecutor::execute_tool_call(const ToolCallNode* node, const Context& ctx) {
    Context new_context = ctx;

    // 渲染参数
    std::unordered_map<std::string, std::string> rendered_args;
    for (const auto& [key, tmpl] : node->arguments) {
        rendered_args[key] = InjaTemplateRenderer::render(tmpl, ctx);
    }

    // 调用工具
    if (!tool_registry_.has_tool(node->tool_name)) {
        throw std::runtime_error("Tool '" + node->tool_name + "' not registered for node: " + node->path);
    }

    // Phase 1 Sprint 1a (S1a.T2): 用 std::chrono::steady_clock 测量工具耗时
    // REQ-TR-002: ToolResult.latency_ms 必须自动填充
    const auto t0 = std::chrono::steady_clock::now();
    nlohmann::json raw_result = tool_registry_.call_tool(node->tool_name, rendered_args);
    const auto t1 = std::chrono::steady_clock::now();
    const auto latency_ms = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count());

    // Phase 1 Sprint 1a (S1a.T2): 替换 `if(result.is_object())` 启发式
    // REQ-TR-MOD-001: 显式 ToolResult::from_json() 分发
    // 双模式兼容：检测到 ok boolean 字段视为 ToolResult 信封, 否则视为 P0 旧式裸 JSON 包装为 success。
    ToolResult tool_result;
    if (raw_result.is_object() && raw_result.contains("ok") && raw_result["ok"].is_boolean()) {
        // 信封模式：解析 ErrorCode / latency_ms / trace_id / metadata (REQ-TR-001..004)
        tool_result = ToolResult::from_json(raw_result);
    } else {
        // 旧式模式：raw JSON 即工具输出, 包装为 success 信封保留 P0 行为
        tool_result = ToolResult::success(raw_result);
    }

    // 注入自动采集的 latency_ms (覆盖 envelope 中可能已存在的值)
    tool_result.latency_ms = latency_ms;

    // REQ-TR-003: trace_id 透传 — 调用方可在 arguments 中传 trace_id
    auto trace_it = rendered_args.find("trace_id");
    if (trace_it != rendered_args.end()) {
        tool_result.trace_id = trace_it->second;
    }

    // REQ-TR-001: error_code 分发 (RETRY/SKIP/ABORT 等)
    if (!tool_result.ok) {
        const ErrorCode code = tool_result.error_code.value_or(ErrorCode::Unknown);
        const std::string err_msg = tool_result.meta.value("error_message", std::string{});
        switch (code) {
            case ErrorCode::Retry:
                // Sprint 1a 占位: 抛出标记性异常, 真正的重试策略由独立 change 实现
                throw std::runtime_error("[RETRY] Tool '" + node->tool_name + "': " + err_msg);
            case ErrorCode::Abort:
                // REQ-TR-001 Scenario: 看到 Abort 时 MUST 抛出异常终止整个 Graph
                throw std::runtime_error("[ABORT] Tool '" + node->tool_name + "': " + err_msg);
            case ErrorCode::Skip:
                // Skip: 跳过此节点不抛异常, output_keys 留默认 (空), 由调用方/调度器处理
                // Sprint 1a 行为: 不写 output_keys, 由后续 Stage 集成调度跳过
                return new_context;
            case ErrorCode::PermissionDenied:
            case ErrorCode::PathViolation:
            case ErrorCode::DangerousCommand:
            case ErrorCode::ToolNotRegistered:
            case ErrorCode::Audit:
            case ErrorCode::Timeout:
            case ErrorCode::ResourceExhausted:
            case ErrorCode::Unknown:
            default:
                // 其他错误码: 抛出通用异常 (保留 P0 行为)
                throw std::runtime_error("Tool '" + node->tool_name + "' failed: " + err_msg);
        }
    }

    // 处理 output_keys (基于 tool_result.data, 替代旧的 is_object() 启发式)
    const nlohmann::json& data = tool_result.data;
    if (node->output_keys.size() == 1) {
        new_context[node->output_keys[0]] = data;
    } else if (data.is_object()) {
        for (const auto& key : node->output_keys) {
            if (data.contains(key)) {
                new_context[key] = data[key];
            }
        }
    } else {
        // 如果 data 不是对象，或者 output_keys 多于 1 个，按第一个 key 赋值
        if (!node->output_keys.empty()) {
            new_context[node->output_keys[0]] = data;
        }
    }

    return new_context;
}

Context NodeExecutor::execute_resource(const ResourceNode* node, const Context& ctx) {
    /*
    // 创建 Resource 对象并注册
    Resource resource;
    resource.path = node->path;
    resource.resource_type = node->resource_type;
    resource.uri = node->uri;
    resource.scope = node->scope;
    resource.metadata = node->metadata; // Use node's metadata

    ResourceManager::instance().register_resource(resource);
        */

    // Resource 节点通常不修改上下文，直接返回
    return ctx;
}

Context NodeExecutor::execute_assert(const AssertNode* node, const Context& ctx) {
    // Render the condition expression using the current context
    std::string rendered_condition_str;
    try {
        rendered_condition_str = InjaTemplateRenderer::render(node->condition, ctx);
    } catch (const inja::InjaError& e) {
        throw std::runtime_error("Assert condition template rendering failed for node '" + node->path + "': " + std::string(e.message));
    }

    // Convert rendered result to boolean
    // Inja renders to string, so we check string value
    bool condition_result = false;
    if (rendered_condition_str == "true") {
        condition_result = true;
    } else if (rendered_condition_str == "false") {
        condition_result = false;
    } else {
        // If the rendered result is not explicitly "true" or "false",
        // try to interpret it as a number (0 is false, non-zero is true)
        try {
            double num_val = std::stod(rendered_condition_str);
            condition_result = (num_val != 0.0);
    } catch (...) {
            // If it's not a number either, treat as false or throw error
            // Let's throw an error for non-boolean results
            throw std::runtime_error("Assert condition did not evaluate to a boolean value ('true'/'false' or number): " + rendered_condition_str);
        }
    }

    if (!condition_result) {
        // Condition failed
        if (node->on_failure.has_value()) {
            // This requires the scheduler to handle jumps.
            // For now, throw an error indicating the jump path.
            // The scheduler will catch this and handle the jump.
            throw std::runtime_error("Assert failed. Jumping to: " + node->on_failure.value());
        } else {
            // No jump path, just fail the execution
            throw std::runtime_error("Assert failed at node: " + node->path);
        }
    }
    // Condition passed, context remains unchanged
    return ctx;
}

Context NodeExecutor::execute_fork(const ForkNode* node, const Context& ctx) {
    // ForkNode 的执行逻辑需要由 TopoScheduler 处理，因为它涉及并发和上下文管理。
    // NodeExecutor 本身不能启动新的线程或任务。
    // 因此，这里抛出异常，提示需要在调度器层面实现。
    throw std::runtime_error("ForkNode execution requires concurrent scheduler support, not implemented in NodeExecutor.");
    // In a concurrent scheduler:
    // 1. Create context copies for each branch.
    // 2. Schedule each branch path to run concurrently.
    // 3. Wait for all branches to complete (JoinNode would handle waiting).
    // 4. Trigger snapshot here according to v3.1 spec.
    return ctx; // Placeholder
}

Context NodeExecutor::execute_join(const JoinNode* node, const Context& ctx) {
    // JoinNode 的执行逻辑也需要由 TopoScheduler 处理，因为它需要等待其他分支完成。
    // NodeExecutor 本身无法等待。
    // 因此，这里抛出异常，提示需要在调度器层面实现。
    throw std::runtime_error("JoinNode execution requires concurrent scheduler support, not implemented in NodeExecutor.");
    // In a concurrent scheduler:
    // 1. Wait for all nodes in 'wait_for' to finish.
    // 2. Retrieve their final contexts.
    // 3. Merge them into the current context using 'merge_strategy'.
    // 4. Return the merged context.
    return ctx; // Placeholder
}

Context NodeExecutor::execute_generate_subgraph(const GenerateSubgraphNode* node, const Context& ctx) {
    Context new_context = ctx;
    try {
        if (!ctx.contains("__rendered_prompt__")) {
            throw std::runtime_error("Missing __rendered_prompt__ in context for GenerateSubgraphNode");
        }
        std::string rendered_prompt = ctx.at("__rendered_prompt__").get<std::string>();
        // Add budget info to prompt context if needed by LLM
        // [DEBUG-removed] Context prompt_ctx = ctx;
        // [DEBUG-removed] prompt_ctx["available_subgraphs"] = PromptBuilder::build_available_libraries_context();
         // Add budget info (nodes_left, depth_left, etc.) - This requires access to ExecutionSession's budget
         // [DEBUG-removed] For now, assume budget info is added by the calling context or PromptBuilder
         // [DEBUG-removed] prompt_ctx["budget"] = ...; // Access budget from ExecutionSession

        // 2. Call LLM（C₁.2: 使用 ILLMProvider 接口，Result<T,E> 风格）
        std::string generated_dsl;
        if (llm_provider_) {
            GenerationRequest req;
            req.prompt = rendered_prompt;
            auto result = llm_provider_->generate(req, {});
            if (!result.has_value()) {
                throw std::runtime_error("LLM provider error: " + result.error().message);
            }
            generated_dsl = std::move(result.value().text);
        } else {
            throw std::runtime_error("LLM provider not available for generate_subgraph");
        }

        // 3. Parse LLM output for `### AgenticDSL '/dynamic/...'` blocks
        // This requires access to the main engine's parser and graph storage mechanism.
        // A reference or callback to the engine might be needed here, or the parsing result is returned.
        // For this executor, assume a global or injected mechanism or return the result for the scheduler to handle.
        // Let's assume the scheduler calls a parser and handles the dynamic graph registration.
        // Here, we just parse and return the generated paths in the context.
        // Stage 4 Task 20: 通过 IParser 抽象调用；IParser::parse 返回单 ParsedGraph，包装为 vector 以兼容下游 for-range
        // 注意：ParsedGraph 禁止拷贝（仅可移动），故不能使用 initializer_list 构造；改用 push_back 走移动路径
        std::vector<ParsedGraph> new_graphs;
        new_graphs.push_back(parser_->parse(generated_dsl));
        std::vector<std::string> dynamic_paths; // Collect paths of generated graphs
        for (auto& graph : new_graphs) {
            if (graph.path.rfind("/dynamic/", 0) == 0) { // Ensure it's dynamic
                // 4. Validate signature if present (v3.1)
                if (graph.signature.has_value()) {
                    // Perform validation based on signature_validation policy
                    bool is_valid = true; // Placeholder for actual validation logic
                    if (!is_valid && node->signature_validation == "strict") {
                         if (node->on_signature_violation.has_value()) {
                             // Trigger jump to on_signature_violation path
                             // This requires scheduler logic to handle jumps
                             // For now, throw error
                             throw std::runtime_error("Signature validation failed (strict mode) for generated graph: " + graph.path);
                         } else {
                             // Default behavior for strict violation without jump path
                             throw std::runtime_error("Signature validation failed (strict mode) for generated graph: " + graph.path);
                         }
                    } else if (!is_valid && node->signature_validation == "warn") {
                        // Log warning but continue
                        LOG_WARN("Signature validation failed (warn mode) for generated graph: " << graph.path);
                    } // ignore: do nothing
                }
                dynamic_paths.push_back(graph.path);
                // 5. Register new graph (This logic belongs in the scheduler/engine)
                // g_current_engine->append_graphs({std::move(graph)}); // Placeholder - requires access to engine/scheduler
            }
        }

        // 6. Store generated graph path(s) in context
        if (!node->output_keys.empty()) {
            if (dynamic_paths.size() == 1) {
                new_context[node->output_keys[0]] = dynamic_paths[0];
            } else {
                new_context[node->output_keys[0]] = dynamic_paths; // Store as array if multiple
            }
        }

        // 7. Snapshot trigger happens in ExecutionSession::execute_node, not here.

    } catch (const std::exception & e) {
        throw std::runtime_error("GenerateSubgraphNode execution failed: " + std::string(e.what()));
    }
    return new_context;
}

} // namespace agenticdsl
