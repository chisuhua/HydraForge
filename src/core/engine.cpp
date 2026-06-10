// src/core/engine.cpp
#include "engine.h"
#include "common/llm/llama_adapter.h" // C₁.4: 保留，仅用于 LlamaAdapterProvider（向后兼容）
#include "common/log/log.h"  // agenticdsl::log facade
#include "common/llm/llama_adapter_provider.h" // C₁.4: 适配器（可选真实 provider）
#include "modules/scheduler/topo_scheduler.h"
#include "modules/system/system_nodes.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <stdexcept>
#include <filesystem>

namespace agenticdsl {

static LlamaAdapter::Config load_llm_config(const std::string& config_path = "llm_config.json") {
    namespace fs = std::filesystem;

    LlamaAdapter::Config config;
    config.api_url = "http://localhost:8080";
    config.api_endpoint = "/v1/chat/completions";
    config.model = "gpt-3.5-turbo";
    config.n_ctx = 2048;
    config.n_threads = std::thread::hardware_concurrency();
    config.temperature = 0.7f;

    std::ifstream file(config_path);
    if (!file.is_open()) {
        return config;
    }

    try {
        nlohmann::json j;
        file >> j;

        if (j.contains("api_url") && j["api_url"].is_string()) {
            config.api_url = j["api_url"].get<std::string>();
        }
        if (j.contains("api_endpoint") && j["api_endpoint"].is_string()) {
            config.api_endpoint = j["api_endpoint"].get<std::string>();
        }
        if (j.contains("api_key") && j["api_key"].is_string()) {
            config.api_key = j["api_key"].get<std::string>();
        }
        if (j.contains("model") && j["model"].is_string()) {
            config.model = j["model"].get<std::string>();
        }
        if (j.contains("n_ctx") && j["n_ctx"].is_number_integer()) {
            config.n_ctx = j["n_ctx"].get<int>();
        }
        if (j.contains("n_threads") && j["n_threads"].is_number_integer()) {
            int threads = j["n_threads"].get<int>();
            config.n_threads = (threads > 0) ? threads : std::thread::hardware_concurrency();
        }
        if (j.contains("temperature") && j["temperature"].is_number()) {
            config.temperature = static_cast<float>(j["temperature"].get<double>());
        }
    } catch (const std::exception& e) {
    }

    return config;
}

std::unique_ptr<DSLEngine> DSLEngine::from_markdown(const std::string& markdown_content) {
    MarkdownParser parser;
    auto graphs = parser.parse_from_string(markdown_content);

    // Ensure /main exists
    bool has_main = false;
    for (const auto& g : graphs) {
        if (g.path == "/main") {
            has_main = true;
            break;
        }
    }
    if (!has_main) {
        throw std::runtime_error("Required /main subgraph not found");
    }

    (void)load_llm_config(); // 保留配置加载（向后兼容），但不再自动创建 LlamaAdapter

    // C₁.4: 默认使用 MockLLMProvider（CI 永远可运行，无需本地 LLM）
    // 如需真实 LLM，用户可通过 set_llm_provider() 注入自定义 provider
    auto llm_provider = std::make_unique<MockLLMProvider>();

    auto engine = std::make_unique<DSLEngine>(std::move(graphs));
    engine->llm_provider_ = std::move(llm_provider);
    return engine;
}

std::unique_ptr<DSLEngine> DSLEngine::from_file(const std::string& file_path) {
    std::ifstream file(file_path);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open file: " + file_path);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return from_markdown(buffer.str());
}

DSLEngine::DSLEngine(std::vector<ParsedGraph> initial_graphs)
    : full_graphs_(std::move(initial_graphs)),
      tool_registry_() {
    LOG_INFO("Graphs loaded: " << full_graphs_.size());

    // 阶段 4 任务 4.3: 一次性将 BudgetController::record_llm_call 绑定到 tool_registry_
    // 注意：budget_controller_ 是非静态成员，按引用捕获以保证生命周期与 engine 一致。
    // 此回调在每次 LLM tool 成功调用后触发。
    tool_registry_.set_cost_callback(
        [this](int tokens, const std::string& model) {
            budget_controller_.record_llm_call(tokens, model);
        }
    );
}

// 阶段 4 任务 4.3: 返回 session 累计成本
double DSLEngine::get_session_cost() const {
    return budget_controller_.get_total_cost_usd();
}

ExecutionResult DSLEngine::run(const Context& context) {
    // 提取预算（从 /__meta__）
    std::optional<ExecutionBudget> budget;
    for (auto& g : full_graphs_) {
        if (g.budget.has_value()) {
            budget = std::move(g.budget);
            break;
        }
    }

    // 创建调度器
    TopoScheduler::Config config;
    config.initial_budget = std::move(budget);
    // C₁.4 迁移：传递 ILLMProvider* 而非 LlamaAdapter*
    TopoScheduler scheduler(std::move(config), tool_registry_, llm_provider_.get(), &full_graphs_);

    // 注册所有节点（包括系统节点）
    auto sys_nodes = create_system_nodes();
    for (auto& node : sys_nodes) {
        scheduler.register_node(std::move(node));
    }
    for (const auto& graph : full_graphs_) {
        for (const auto& node : graph.nodes) {
            if (node) {
                scheduler.register_node(node->clone());
            }
        }
    }
    scheduler.build_dag();

    auto result = scheduler.execute(context);

    last_traces_ = scheduler.get_last_traces();

    return result;
}

void DSLEngine::register_llm_tool(std::string name, std::unique_ptr<ILLMTool> tool, const LLMParams& default_params) {
    tool_registry_.register_llm_tool(std::move(name), std::move(tool), default_params);
}

void DSLEngine::append_graphs(std::vector<ParsedGraph> new_graphs) {
    for (auto& graph : new_graphs) {
        full_graphs_.push_back(std::move(graph));
    }
}

void DSLEngine::continue_with_generated_dsl(const std::string& generated_dsl) {
    if (generated_dsl.empty()) return;

    MarkdownParser parser;
    auto new_graphs = parser.parse_from_string(generated_dsl);

    // 校验：每个新图必须是合法子图（可选：验证 signature / permissions）
    // 此处暂略，后续可集成 NodeValidator
    append_graphs(std::move(new_graphs)); // 复用逻辑
}

} // namespace agenticdsl
