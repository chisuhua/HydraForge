// src/core/engine.cpp
#include "engine.h"
#include "common/llm/llama_adapter.h" // C₁.4: 保留，仅用于 LlamaAdapterProvider（向后兼容）
#include "common/log/log.h"  // agenticdsl::log facade
#include "common/llm/llama_adapter_provider.h" // C₁.4: 适配器（可选真实 provider）
#include "common/llm/llm_config.h" // P1.T1: LLMConfig 完整类型
#include "common/llm/mock_provider.h" // P1.T1 fallback: 兜底直接构造 MockLLMProvider
// P1.T4: ToolRegistry 完整类型仅在 .cpp 可见 (PIMPL-lite 解耦, engine.h 改为 IToolRegistry 抽象)
// P2.C (2026-06-24): 通过 factory 构造, 无需 include common/tools/registry.h
#include "modules/budget/factory.h"
#include "common/llm/factory.h"
#include "modules/scheduler/topo_scheduler.h"
#include "modules/system/system_nodes.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <stdexcept>
#include <filesystem>
#include <utility> // Phase 1 Sprint 1b (S1b.T2): std::move for bus injection

namespace agenticdsl {
// P2.C (2026-06-24): forward-declared factory for ToolRegistry (decouple from registry.h)
namespace tools {
std::unique_ptr<IToolRegistry> create_tool_registry();
} // namespace tools

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

    // P1.T1: DSLEngine 构造器已通过 provider_factory_ 创建默认 llm_provider_ (mock 路径)
    // 不再需要在 from_markdown 中显式构造 LLMProvider
    auto engine = std::make_unique<DSLEngine>(std::move(graphs));
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
      tool_registry_(agenticdsl::tools::create_tool_registry()),
      provider_factory_(agenticdsl::llm::create_provider_factory()),
      budget_controller_(agenticdsl::budget::create_controller()) {
    LOG_INFO("Graphs loaded: " << full_graphs_.size());

    // P1.T1: 通过 factory 创建默认 LLM provider (默认 LLMConfig{} → MockLLMProvider)
    LLMConfig default_config;  // 默认 provider="openai" 但 LLMProviderFactory 兜底返回 Mock
    // 实际: openai 是 OpenAI 兼容协议, factory 会返回 CloudLLMAdapter
    // CI 环境无 API key, 我们强制 fallback 到 mock: 通过 set_provider_factory + MockProviderFactory
    // 见 from_markdown() 中的处理
    if (provider_factory_) {
        // 尝试创建, 失败 fallback (空 provider 走 mock 路径)
        LLMConfig mock_config;  // 默认全空 → mock 路径
        mock_config.provider = "mock";  // 显式 mock (CI 永远可运行)
        llm_provider_ = provider_factory_->create(mock_config);
    }
    if (!llm_provider_) {
        // 兜底: 直接构造 MockLLMProvider (防止 nullptr)
        llm_provider_ = std::make_unique<MockLLMProvider>();
    }

    // 阶段 4 任务 4.3: 一次性将 BudgetController::record_llm_call 绑定到 tool_registry_
    // 注意：budget_controller_ 是非静态成员，按引用捕获以保证生命周期与 engine 一致。
    // 此回调在每次 LLM tool 成功调用后触发。
    // P1.T4: tool_registry_ 改 unique_ptr<IToolRegistry>, 通过 -> 调用
    tool_registry_->set_cost_callback(
        [this](int tokens, const std::string& model) {
            budget_controller_->record_llm_call(tokens, model);
        }
    );
}

// 阶段 4 任务 4.3: 返回 session 累计成本
double DSLEngine::get_session_cost() const {
    return budget_controller_->get_total_cost_usd();
}

// Stage 4 / Task 19: 类外定义 — 在 budget_controller.h 已 include 的 TU 中，析构可见完整类型
DSLEngine::~DSLEngine() = default;

// Stage 4 / Task 19: 类外 accessor 定义 — 头文件中仅有前向声明，这里返回引用才需要完整类型
BudgetController& DSLEngine::get_budget_controller() { return *budget_controller_; }
const BudgetController& DSLEngine::get_budget_controller() const { return *budget_controller_; }

// === Phase 1 Sprint 1b (S1b.T2): IInteractionBus 注入/访问/订阅 实现 ===
// 设计依据: design.md §决策 1 (shared_ptr 持有所有权) + §决策 4 (nullptr 静默 no-op) +
//           §决策 5 (subscribe 透传 token，不缓存)
void DSLEngine::set_interaction_bus(std::shared_ptr<IInteractionBus> bus) {
    bus_ = std::move(bus);
}

std::shared_ptr<IInteractionBus> DSLEngine::get_interaction_bus() const {
    return bus_;
}

size_t DSLEngine::subscribe(const std::string& topic,
                            std::function<void(const ToolResult&)> cb) {
    // bus 未注入时返回 0（无效 token），不抛异常（REQ-BUS-002 Scenario: bus 为 nullptr 时）
    if (!bus_) {
        return 0;
    }
    // 透传 token 到 InMemoryBus::subscribe，由 bus 统一管理生命周期
    return bus_->subscribe(topic, std::move(cb));
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
    TopoScheduler scheduler(std::move(config), *tool_registry_, llm_provider_.get(), &full_graphs_);

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
    // P1.T4: tool_registry_ 改 unique_ptr<IToolRegistry>, 通过 -> 调用
    tool_registry_->register_llm_tool(std::move(name), std::move(tool), default_params);
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
