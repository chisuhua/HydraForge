// src/core/engine.cpp
#include "engine.h"
#include "agenticdsl/contract/i_llm_provider_decorator.h"  // Phase 5: ILLMProviderDecorator base
#include "common/llm/cost_tracking_decorator.h"            // Phase 5: REQ-IPD-002 budget hole fix
#include "common/llm/compliance_decorator.h"              // Phase 5: REQ-IPD-003 opt-in
#include "common/llm/rate_limit_decorator.h"              // Phase 5: REQ-IPD-004 opt-in
#include "common/llm/tracing_decorator.h"                 // Phase 6a: ADR-0068 §决策 3 llm.request/llm.response 发射
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

#include "agenticdsl/plugin/plugin_loader.h" // D5 (C14): load_plugin() 需要完整类型
#include "common/tools/tool_coordinator.h" // C4 Sprint 14 (ADR-0031 P3-P4): ToolCoordinator 构造
#include "common/tools/registry.h"         // C11: ToolRegistry downcast for session_registry injection
#include "agenticdsl/plugin/plugin_loader.h" // D5 (C14): 显式 plugin 加载
#include "core/event_log.h"                 // ADR-0080 v1.1: AppendOnlyEventLog 写入器
#include "core/session_writer.h"            // ADR-0079 v1.1 + P5: SessionWriter (D5 + D6)
#include "core/types/session_writer_config.h"  // P5: SessionWriterConfig
#include "core/types/event_log_config.h"    // ADR-0080 v1.1 D11: EventLogConfig 结构

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

    // P1.T1: DSLEngine 构造器已通过 provider_factory_ 创建默认 owned_provider_ (mock 路径)
    // 不再需要在 from_markdown 中显式构造 LLMProvider
    auto engine = std::make_unique<DSLEngine>(std::move(graphs));
    return engine;
}

// loop-agent-dsl-execution: 新重载 — 子引擎继承父引擎的已装饰 LLM provider
// 实现：复用基础 from_markdown → 释放默认 Mock → 非拥有借用父 provider
std::unique_ptr<DSLEngine> DSLEngine::from_markdown(
    const std::string& markdown_content,
    ILLMProvider& parent_provider)
{
    auto engine = from_markdown(markdown_content);
    engine->owned_provider_.reset();                  // 释放 MockLLMProvider
    engine->borrowed_provider_ = &parent_provider;     // 非拥有借用，不调用 set_llm_provider()
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

DSLEngine::DSLEngine() : DSLEngine(std::vector<ParsedGraph>{}) {}

DSLEngine::DSLEngine(std::vector<ParsedGraph> initial_graphs)
    : full_graphs_(std::move(initial_graphs)),
      tool_registry_(agenticdsl::tools::create_tool_registry()),
      provider_factory_(agenticdsl::llm::create_provider_factory()),
      budget_controller_(agenticdsl::budget::create_controller()),
      session_registry_(std::make_unique<SessionRegistry>()) {
    LOG_INFO("Graphs loaded: " << full_graphs_.size());

    // P1.T1 + P2.C: 通过 factory 创建默认 LLM provider (mock 路径)
    // LLMProviderFactory::create() 保证未知 provider 兜底返回 Mock provider (永不 nullptr)
    // Phase 5 (REQ-ICC-008 / REQ-IPD-005): 立即用 CostTrackingDecorator 包装, 修 budget hole
    LLMConfig mock_config;
    mock_config.provider = "mock";
    auto base_provider = provider_factory_->create(mock_config);
    owned_provider_ = decorate_provider(std::move(base_provider));

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

    // C11: 注册 4 个 session.* 工具
    if (auto* tr = dynamic_cast<ToolRegistry*>(tool_registry_.get())) {
        tr->register_tool("session.create",
            ToolMetadata{"session.create", "Create a new session", "builtin", ToolCategory::StateModify, LayerProfile::Workflow, ApprovalPolicy{false, true, false, false}},
            [this](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
                SessionConfig config;
                auto name_it = args.find("name");
                if (name_it != args.end()) config.name = name_it->second;
                return nlohmann::json{{"session_id", session_registry_->create_session(config)}};
            });

        tr->register_tool("session.destroy",
            ToolMetadata{"session.destroy", "Destroy a session", "builtin", ToolCategory::StateModify, LayerProfile::Workflow, ApprovalPolicy{true, true, false, true}},
            [this](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
                auto it = args.find("session_id");
                if (it == args.end()) return nlohmann::json{{"error", "Missing session_id"}};
                session_registry_->destroy_session(it->second);
                return nlohmann::json{{"destroyed", it->second}};
            });

        tr->register_tool("session.set_var",
            ToolMetadata{"session.set_var", "Set a session variable", "builtin", ToolCategory::StateModify, LayerProfile::Workflow, ApprovalPolicy{true, true, false, false}},
            [this](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
                auto sid = args.find("session_id"), key = args.find("key"), val = args.find("value");
                if (sid == args.end() || key == args.end() || val == args.end())
                    return nlohmann::json{{"error", "Missing session_id/key/value"}};
                try {
                    auto& s = session_registry_->get_session(sid->second);
                    s.set_session_var(key->second, nlohmann::json::parse(val->second));
                    return nlohmann::json{{"ok", true}};
                } catch (const std::exception& e) { return nlohmann::json{{"error", e.what()}}; }
            });

        tr->register_tool("session.get_var",
            ToolMetadata{"session.get_var", "Get a session variable", "builtin", ToolCategory::ReadOnly, LayerProfile::Workflow, ApprovalPolicy{true, true, false, false}},
            [this](const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
                auto sid = args.find("session_id"), key = args.find("key");
                if (sid == args.end() || key == args.end())
                    return nlohmann::json{{"error", "Missing session_id/key"}};
                try {
                    return session_registry_->get_session(sid->second).get_session_var(key->second);
                } catch (const std::exception& e) { return nlohmann::json{{"error", e.what()}}; }
            });
    }
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

// === ADR-0080 v1.1 D12: opt-in 启用 AppendOnlyEventLog ===
// 调用前置: set_interaction_bus(bus) 必须先执行
// agent_id 空字符串 throw (fail-closed: 多 agent 场景避免静默合并到 default)
// 双调用 throw: 同一 engine 不允许重复启用 (析构由 unique_ptr 管理)
void DSLEngine::enable_event_log(const std::string& agent_id,
                                 CaptureMode capture_mode,
                                 std::size_t max_file_size,
                                 std::size_t max_rotation_files) {
    if (event_log_) {
        throw std::logic_error(
            "DSLEngine::enable_event_log: already enabled (idempotent call forbidden)");
    }
    if (agent_id.empty()) {
        throw std::invalid_argument(
            "DSLEngine::enable_event_log: agent_id empty (fail-closed)");
    }
    if (!bus_) {
        throw std::logic_error(
            "DSLEngine::enable_event_log: bus not injected (call set_interaction_bus first)");
    }

    EventLogConfig cfg;
    cfg.event_log_enabled = true;
    cfg.event_log_agent_id = agent_id;
    cfg.capture_mode = capture_mode;  // ✅ Phase 1 迁移
    cfg.max_file_size = max_file_size;
    cfg.max_rotation_files = max_rotation_files;

    event_log_ = std::make_unique<EventLogWriter>(cfg, bus_);
}

// ADR-0079 v1.1 + P5 session-writer-bridge: opt-in 启用 SessionWriter (默认 OFF)
// 对称 enable_event_log: 独立互斥锁、独立 JSONL、过滤 D6 白名单 13 topic
// session_id 空 throw (fail-closed, 多 session 场景避免静默合并到 default)
// 双调用 throw: 同一 engine 不允许重复启用 (析构由 unique_ptr 管理)
void DSLEngine::enable_session_writer(const std::string& session_id,
                                       const std::filesystem::path& writer_dir) {
    if (session_writer_) {
        throw std::logic_error(
            "DSLEngine::enable_session_writer: already enabled (idempotent call forbidden)");
    }
    if (session_id.empty()) {
        throw std::invalid_argument(
            "DSLEngine::enable_session_writer: session_id empty (fail-closed)");
    }
    if (!bus_) {
        throw std::logic_error(
            "DSLEngine::enable_session_writer: bus not injected (call set_interaction_bus first)");
    }

    SessionWriterConfig cfg;
    cfg.session_writer_enabled = true;
    cfg.session_id = session_id;
    if (!writer_dir.empty()) {
        cfg.writer_dir = writer_dir;
    }

    session_writer_ = std::make_unique<SessionWriter>(cfg, bus_);
}

// === Phase 5 (REQ-ICC-008 / REQ-IPD-005): decorate_provider 私有 helper ===
// 按 REQ-IPD-005 默认部署顺序包装 Decorator 链:
//   TracingDecorator(opt-in, bus_) -> CostTrackingDecorator(最外层, 强制) -> Compliance(opt-in) -> RateLimit(opt-in) -> inner
// 链深度限制由 ILLMProviderDecorator::wrap_chain 强制 ≤ 5 (含 inner) — Phase 6a (ADR-0068) 提升到 5 以容纳 TracingDecorator
std::unique_ptr<ILLMProvider> DSLEngine::decorate_provider(std::unique_ptr<ILLMProvider> base) {
  if (!base) {
    return nullptr;  // 防御性编程, factory 保证非 nullptr, 但 wrap_chain 需要非 nullptr
  }
  std::vector<std::function<std::unique_ptr<ILLMProvider>(std::unique_ptr<ILLMProvider>)>> factories;

  // 0. TracingDecorator (Phase 6a, ADR-0068) — opt-in, 仅在 bus_ 已注入时启用
  //    发射 llm.request / llm.response 事件到 InteractionBus
  //    链深度: 4 decorators (Tracing+CostTracking+Compliance+RateLimit) + inner = 5 ≤ MAX_CHAIN_DEPTH
  if (bus_) {
    std::shared_ptr<IInteractionBus> bus = bus_;
    factories.push_back([bus](std::unique_ptr<ILLMProvider> inner) {
      return std::make_unique<TracingDecorator>(std::move(inner), bus);
    });
  }

  // 1. CostTrackingDecorator (强制, 最外层) — 修 budget hole
  std::shared_ptr<IBudgetController> budget = std::shared_ptr<IBudgetController>(
      budget_controller_.get(), [](IBudgetController*) {});
  factories.push_back([budget](std::unique_ptr<ILLMProvider> inner) {
    return std::make_unique<CostTrackingDecorator>(std::move(inner), budget);
  });

  // 2. ComplianceDecorator (opt-in)
  if (compliance_enabled_ && bus_) {
    std::shared_ptr<IInteractionBus> bus = bus_;
    factories.push_back([bus](std::unique_ptr<ILLMProvider> inner) {
      return std::make_unique<ComplianceDecorator>(std::move(inner), bus);
    });
  }

  // 3. RateLimitDecorator (opt-in)
  if (rate_limit_enabled_) {
    int tpm = rate_limit_tokens_per_minute_;
    factories.push_back([tpm](std::unique_ptr<ILLMProvider> inner) {
      return std::make_unique<RateLimitDecorator>(std::move(inner), "default", tpm);
    });
  }

  if (factories.empty()) {
    return base;
  }
  return ILLMProviderDecorator::wrap_chain(std::move(base), std::move(factories));
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
    // 包装 ToolResult 回调为 BusEvent 回调，适配 IInteractionBus::subscribe 新签名
    return bus_->subscribe(topic, [cb = std::move(cb)](const BusEvent& event) {
        cb(event.payload);
    });
}

// D5 (C14, decisions-2026-07-07.md Option B): 显式加载 PDK plugin
// 删除默认注入 — 调用方必须显式调用 load_plugin() 加载 .so
bool DSLEngine::load_plugin(const std::string& plugin_name) {
    if (!plugin_loader_) {
        plugin_loader_ = std::make_unique<hydraforge::PluginLoader>();
    }
    try {
        return plugin_loader_->load_so(plugin_name, get_tool_registry());
    } catch (const std::exception& e) {
        LOG_WARN("Failed to load plugin '" << plugin_name << "': " << e.what());
        return false;
    }
}

ExecutionResult DSLEngine::run(const Context& context) {
    // Sprint 20 (2026-07-01) / OpenSpec migrate-context-to-layered:
    // 旧签名桥接到新签名 — 1 行委托, 0 重复逻辑。
    return run(to_context(context));
}

ExecutionResult DSLEngine::run(const LayeredContext& ctx) {
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
        std::move(scheduler_cfg), *tool_registry_, get_llm_provider(), &full_graphs_);
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

    // Sprint 20 桥接期: scheduler.execute() 仍接受 flat Context
    // LayeredContext 的 L3 working 层是当前任务数据, 直接透传保持向后兼容。
    auto result = scheduler.execute(ctx.working);

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

// Wave 4: 上下文压缩器注入 (context-compactor Task 8)
void DSLEngine::set_context_compactor(std::unique_ptr<IContextCompactor> compactor) {
  context_compactor_ = std::move(compactor);
}

void DSLEngine::check_and_compact(LayeredContext& ctx) {
  if (!context_compactor_) return;
  std::string json = ctx.dump();
  size_t tokens = context_compactor_->count_tokens(json);
  if (!context_compactor_->should_compact(tokens)) return;
  // session_id 从 ctx.meta 读取，默认 "main"
  std::string sid = ctx.meta.value("session_id", "main");
  context_compactor_->on_compact_before(sid, tokens);
  std::string summary = context_compactor_->compact(json, *get_llm_provider());
  if (summary.empty()) return;
  size_t tokens_after = context_compactor_->count_tokens(summary);
  ctx.append_original(json);
  ctx.set_working_view(summary);
  auto record = context_compactor_->make_record(tokens, tokens_after, summary.size());
  ctx.set_metadata("compaction_record", nlohmann::json{
      {"tokens_before", record.tokens_before},
      {"tokens_after", record.tokens_after},
      {"summary_length", record.summary_length},
      {"timestamp", record.timestamp}
  });
  context_compactor_->on_compact_after(sid, tokens, tokens_after);
}

// ============================================================
// ADR-0033 Session Hierarchy (Sprint 15 / C5): 会话感知 run 实现
// ============================================================

/// @brief 将 ExecutionResult 转换为 ToolResult 信封格式以追加到 UserSession.messages
static ToolResult to_tool_result(const ExecutionResult& r) {
  ToolResult tr;
  tr.ok = r.success;
  tr.meta = nlohmann::json::object({
    {"message", r.message},
    {"paused_at", r.paused_at.has_value() ? nlohmann::json(r.paused_at.value()) : nlohmann::json(nullptr)}
  });
  if (!r.success) {
    tr.error_code = ErrorCode::Unknown;
  }
  return tr;
}

ExecutionResult DSLEngine::run(UserSession& user_sess, const std::string& message,
                                 const Context& initial_ctx) {
  // 1. 将 message 写入 ctx["user_input"]
  Context ctx = initial_ctx;
  ctx["user_input"] = message;

  // 2. 创建或复用 TaskSession
  TaskSession* task_sess_ptr = user_sess.current_task_session();
  if (!task_sess_ptr || task_sess_ptr->status() == "failed") {
    task_sess_ptr = &user_sess.create_task_session();
  }

  // 3. 检查失败模式：≥3 次可重试失败则自动分裂
  if (task_sess_ptr->determine_failure_mode() == TaskSession::FailureMode::NewSession) {
    task_sess_ptr = &user_sess.create_task_session();
  }

  // 4. 设置当前执行策略（与 DSLEngine 共享）
  task_sess_ptr->set_policy(policy_);

  // 5. 替换 context（顶层覆盖）
  task_sess_ptr->set_context(std::move(ctx));

  // 6. 执行 — 委托到现有 run_impl
  auto result = run_impl(*task_sess_ptr, message);

  // 7. 记录失败（仅可重试错误递增 failure_count）
  task_sess_ptr->record_failure(result);

  // 8. 追加到 UserSession.messages
  user_sess.append_message(to_tool_result(result));

  // 9. 更新状态
  task_sess_ptr->set_status(result.success ? "completed" : "failed");

  last_traces_ = {}; // 会话感知路径暂不缓存 trace（由 SubtaskSession 归档）

  return result;
}

ExecutionResult DSLEngine::run(UserSession& user_sess, const std::string& message,
                                 const LayeredContext& initial_lctx) {
  return run(user_sess, message, initial_lctx.working);
}

ExecutionResult DSLEngine::run_impl(TaskSession& task_sess, const std::string& /*message*/) {
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
  scheduler_cfg.approval_handler = approval_handler_.get();
  scheduler_cfg.tool_coordinator = tool_coordinator_.get();
  auto scheduler_unique = agenticdsl::scheduler::create(
      std::move(scheduler_cfg), *tool_registry_, get_llm_provider(), &full_graphs_);
  IScheduler& scheduler = *scheduler_unique;

  // 注册所有节点
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

  // 使用 TaskSession 的 context 执行
  auto result = scheduler.execute(task_sess.context());

  // fork/join 分支归档 — SubtaskSession 包装
  // 设计决策 D5: TopoScheduler 签名不变，SubtaskSession 创建/归档在 DSLEngine 层
  // 当前实现：run_impl 内部对 fork/join 分支创建 SubtaskSession 并归档
  // 注：Fork/Join 的 SubtaskSession 隔离在后续迭代中深化（当前为最小可行集成）

  return result;
}

SessionRegistry* DSLEngine::get_session_registry() {
  return session_registry_.get();
}

const SessionRegistry* DSLEngine::get_session_registry() const {
  return session_registry_.get();
}

} // namespace agenticdsl