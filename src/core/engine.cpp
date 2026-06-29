// src/core/engine.cpp
#include "engine.h"
#include "common/log/log.h"  // agenticdsl::log facade
// P2.C (2026-06-24): LLM/budget 头文件均移除
// engine.h 已通过 common/llm/llm_types.h 提供 LLMConfig 完整类型
// BudgetController 完整类型由 topo_scheduler.h 间接提供 (execution_session.h → budget_controller.h)
#include "modules/parser/markdown_parser.h"
#include "modules/budget/budget_controller.h"
#include "modules/scheduler/factory.h"
#include "modules/system/system_nodes.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <thread>
#include <stdexcept>
#include <utility> // Phase 1 Sprint 1b (S1b.T2): std::move for bus injection

#include "common/tools/tool_coordinator.h" // C4 Sprint 14 (ADR-0031 P3-P4): ToolCoordinator 构造

namespace agenticdsl {
// P2.C (2026-06-24): forward-declared factories (decouple engine.cpp from concrete headers)
class IBudgetController;
namespace tools {
std::unique_ptr<IToolRegistry> create_tool_registry();
} // namespace tools
namespace llm {
std::unique_ptr<IProviderFactory> create_provider_factory();
} // namespace llm
namespace budget {
std::unique_ptr<IBudgetController> create_controller();
} // namespace budget

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

    // P1.T1 + P2.C: 通过 factory 创建默认 LLM provider (mock 路径)
    // LLMProviderFactory::create() 保证未知 provider 兜底返回 Mock provider (永不 nullptr)
    LLMConfig mock_config;
    mock_config.provider = "mock";
    llm_provider_ = provider_factory_->create(mock_config);

// ADR-0031 (2026-07-31): 默认 Agent 模式执行策略
  policy_ = PolicyFactory::create(PolicyMode::Agent);
  approval_handler_ = std::make_unique<ApprovalHandler>(
    policy_,
    make_test_auto_callback(true),
    300000);

  // C4 Sprint 14 (ADR-0031 P3-P4): ToolCoordinator 默认不创建 (opt-in 兼容)
  tool_coordinator_ = nullptr;

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
IBudgetController& DSLEngine::get_budget_controller() { return *budget_controller_; }
const IBudgetController& DSLEngine::get_budget_controller() const { return *budget_controller_; }

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

    agenticdsl::scheduler::SchedulerConfig scheduler_cfg;
    scheduler_cfg.initial_budget = std::move(budget);
    scheduler_cfg.approval_handler = approval_handler_.get(); // ADR-0031 (2026-07-31): 传递审批处理器
    scheduler_cfg.tool_coordinator = tool_coordinator_.get(); // C4 Sprint 14 (ADR-0031 P3-P4): 传递 ToolCoordinator
    auto scheduler_unique = agenticdsl::scheduler::create(
        std::move(scheduler_cfg), *tool_registry_, llm_provider_.get(), &full_graphs_);
    IScheduler& scheduler = *scheduler_unique;

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

// ADR-0031 (2026-07-31): 设置执行策略模式
void DSLEngine::set_execution_policy(PolicyMode mode) {
  policy_ = PolicyFactory::create(mode);
  approval_handler_ = std::make_unique<ApprovalHandler>(
    policy_,
    make_test_auto_callback(true),
    300000);
}

// C4 Sprint 14 (ADR-0031 P3-P4, Oracle §决策 5): 显式激活 ToolCoordinator (opt-in)
void DSLEngine::set_tool_coordinator(std::unique_ptr<ToolCoordinator> coordinator) {
  tool_coordinator_ = std::move(coordinator);
}

} // namespace agenticdsl
