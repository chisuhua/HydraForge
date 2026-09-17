// pdk/chat_session/src/chat_session.cpp
// ChatSession 实现 (PDK lift)
// 关联: agenticdsl/pdk/chat_session.h, docs/adr/adr-0060-agent-composition.md
// 日期: 2026-09-11
//
// Lift 溯源: examples/pdk_chat_demo/chat_session.cpp (namespace pdk_chat_demo)。
// 本次 lift 相对原实现的两处结构性变更 (§6.1 + A3):
//   1. std::cin/std::cerr 直连 → IInputSource/ILogger 注入 (可测试)
//   2. Self-pipe 所有权从 Impl 移到 IInputSource —
//      Impl 不再持有 pipe_read_fd_/pipe_write_fd_/pipe2(),
//      timer callback 改调 input_->wake() (幂等唤醒, 不关闭输入源)。
//      Sprint 31 语义保留: poll 多 fd + 100ms clamp + EINTR 重试仍在
//      StdinInputSource::read_line 内 (A3 禁止回退裸 getline)。
//
// 锁顺序契约 (per 2026-09-16 fix-loop-run-return-contract, 文档化 pre-existing):
//   1. steering_mutex_ /  follow_up_mutex_   (双队列锁, Impl 内部)
//   2. input_cv_mutex_                       (pop_next_input 等待)
//   3. cancellation_mutex_                   (chat() line 452, 494 — 本 change 触及路径)
//   4. SessionManager write_mutex_           (flush_append, 经 persist_turn)
//   5. SessionManager index_mutex_           (open/flush_append 内部路径, SessionManager 自身保证)
// 禁止反向持有。SessionManager 内部 index → write  反向路径属
// fix-session-manager-lock-order-inversion (archive) pre-existing known-issue,
// 本 change 不触碰。

#include "agenticdsl/pdk/chat_session.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <utility>

#include <core/engine.h>
#include <core/types/tool_result.h>
#include <agenticdsl/types/layered_context.h>
#include <agenticdsl/contract/itool_registry.h>
#include <agenticdsl/contract/bus_event.h>
#include <agenticdsl/contract/event_builder.h>
#include <agenticdsl/contract/iinteraction_bus.h>
#include <agenticdsl/contract/ilogger.h>
#include <agenticdsl/contract/iinput_source.h>
#include <agenticdsl/contract/timer_service.h>
#include <modules/budget/budget_controller.h>

// I/O 默认实现 (生产 fallback)
#include "common/io/stdin_input_source.h"
#include "common/io/stderr_logger.h"

namespace hydraforge::pdk {

namespace {

std::string ptr_to_str(void* p) {
    std::ostringstream ss;
    ss << reinterpret_cast<uintptr_t>(p);
    return ss.str();
}

// T1.2: 展开 ~ 为 HOME 目录 (优先 HOME env, fallback getpwuid)
std::string expand_home(const std::string& path) {
    if (path.empty() || path[0] != '~') return path;
    const char* home = std::getenv("HOME");
    if (home && home[0] != '\0') {
        return std::string(home) + path.substr(1);
    }
    return path;
}

// T1.10: 创建目录权限 0700
bool ensure_dir_0700(const std::filesystem::path& dir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::create_directories(dir, ec);
    if (ec) {
        const std::string msg = "[session] create_directories failed: " + dir.string()
                              + " (" + ec.message() + ")";
        detail::log_static_diag(agenticdsl::LogLevel::kError, msg);
        return false;
    }
    if (chmod(dir.c_str(), 0700) != 0) {
        const std::string msg = "[session] chmod 0700 failed: " + dir.string();
        detail::log_static_diag(agenticdsl::LogLevel::kError, msg);
    }
    return true;
}

constexpr int kSessionSchemaVersion = 1;
constexpr size_t kDefaultQueueCapacity = 32;

}  // namespace

// --- ChatConfig ---

ChatConfig ChatConfig::from_json(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        throw std::runtime_error("Cannot open config: " + path);
    }
    nlohmann::json j;
    f >> j;
    ChatConfig cfg;

    cfg.schema_version = j.value("schema_version", "1.0");
    cfg.app_id = j.value("app_id", "pdk_chat_demo");

    if (j.contains("providers")) cfg.providers = j["providers"];
    if (j.contains("orchestration")) cfg.orchestration = j["orchestration"];
    if (j.contains("safety")) cfg.safety = j["safety"];

    if (j.contains("agent")) {
        auto& a = j["agent"];
        cfg.agent.loop_type = a.value("loop_type", "react");
        cfg.agent.provider = a.value("provider", "mock");
        cfg.agent.model = a.value("model", "test");
        cfg.agent.system_prompt = a.value("system_prompt", "");
        if (a.contains("tools")) {
            for (auto& t : a["tools"]) cfg.agent.tools.push_back(t.get<std::string>());
        }
        cfg.agent.max_steps = a.value("max_steps", 50);
        cfg.agent.timeout_ms = a.value("timeout_ms", 300000);
        cfg.agent.budget_limit_usd = a.value("budget_limit_usd", 1.0);
    }

    if (j.contains("plugins")) {
        for (auto& p : j["plugins"]) {
            PluginConfig pc;
            pc.id = p.value("id", "");
            pc.path = p.value("path", "");
            pc.type = p.value("type", "so");
            pc.lifecycle = p.value("lifecycle", "eager");
            pc.requires_isolation = p.value("requires_isolation", false);
            if (p.contains("activation_events")) {
                for (auto& e : p["activation_events"]) {
                    pc.activation_events.push_back(e.get<std::string>());
                }
            }
            cfg.plugins.push_back(std::move(pc));
        }
    }

    if (j.contains("observability")) {
        auto& o = j["observability"];
        cfg.observability.otel_enabled = o.value("otel_enabled", false);
        cfg.observability.endpoint = o.value("endpoint", "http://localhost:4318");
        cfg.observability.sample_rate = o.value("sample_rate", 1.0);
        cfg.observability.export_format = o.value("export_format", "otlp+http");
    }

    if (j.contains("session")) {
        auto& s = j["session"];
        cfg.session.persist_dir = s.value("persist_dir", "~/.hydraforge/sessions/");
        cfg.session.compact_threshold_tokens = s.value("compact_threshold_tokens", 8000);
        cfg.session.branch_on_user_request = s.value("branch_on_user_request", true);
    }

    return cfg;
}

void ChatConfig::override_provider(const std::string& provider, const std::string& model) {
    this->agent.provider = provider;
    this->agent.model = model;
}

void ChatConfig::override_system_prompt(const std::string& overwrite,
                                        const std::string& append) {
  if (!overwrite.empty()) {
    agent.system_prompt = overwrite;
  }
  if (!append.empty()) {
    if (!agent.system_prompt.empty()) {
      agent.system_prompt += "\n";
    }
    agent.system_prompt += append;
  }
}

void ChatConfig::validate() const {
    if (schema_version != "1.0") {
        throw std::runtime_error("Unsupported schema_version: " + schema_version);
    }
    if (app_id.empty()) {
        throw std::runtime_error("app_id is required");
    }
    if (agent.provider.empty() || agent.model.empty()) {
        throw std::runtime_error("agent.provider and agent.model are required");
    }
    if (agent.max_steps <= 0) {
        throw std::runtime_error("agent.max_steps must be > 0");
    }
    if (agent.timeout_ms <= 0) {
        throw std::runtime_error("agent.timeout_ms must be > 0");
    }
}

// --- ChatSession::Impl ---

class ChatSession::Impl {
public:
    agenticdsl::DSLEngine* engine;
    std::shared_ptr<agenticdsl::IInteractionBus> bus;
    agenticdsl::IToolRegistry* registry;
    AgentConfig agent_cfg;
    SessionConfig session_cfg;
    std::vector<nlohmann::json> messages;
    std::string provider_mode;
    std::string persist_dir_expanded;

    // Cancellation state (Phase B: chat-async-io-cancellation-chain)
    // current_cancellation_id_ / current_token_ 由 chat() 线程写, request_stop() 任意
    // 线程读 → 需 mutex 保护 (2026-09-14 TSan 暴露的 pre-existing data race,
    // baseline examples/pdk_chat_demo/chat_session.cpp:348/384 同缺陷)。
    std::shared_ptr<CancellationRegistry> cancellation_registry_;
    mutable std::mutex cancellation_mutex_;
    std::string current_cancellation_id_;
    std::stop_token current_token_;

    // Wave 3-A Phase C: pending model switch target (next turn will swap to this)
    std::string next_model_;
    mutable std::mutex next_model_mutex_;

    // Async queue infrastructure (Phase A)
    std::queue<std::string> steering_queue_;
    std::queue<std::string> follow_up_queue_;
    mutable std::mutex steering_mutex_;
    mutable std::mutex follow_up_mutex_;
    size_t capacity_ = kDefaultQueueCapacity;

    // chat-async-io-consumer-loop §1.5: condition variable + atomic counter for blocking pop
    std::condition_variable input_cv_;
    std::mutex input_cv_mutex_;
    std::atomic<size_t> pending_input_count_{0};

    // chat-async-io-consumer-loop §5.3: stop_poll_ flag for interrupt_thread RAII GuardChatScope
    std::atomic<bool> stop_poll_{false};

    // Input thread (async producer)
    std::thread input_thread_;
    std::atomic<bool> stop_input_thread_{false};

    // === Sprint 30 timer (D9 lazy) ===
    agenticdsl::ITimerService* timer_ = nullptr;
    // fix-tsan-residual-2026-09-15: periodic_id_ 改 atomic。~Impl() (line 295) 与
    // input_thread_main TimerGuard 析构 (line 879) 并发 write-write race → TSan 报告。
    std::atomic<agenticdsl::ITimerService::TimerId> periodic_id_{0};
    std::atomic<bool> shutdown_check_pending_{false};
    // fix-tsan-residual-2026-09-15: timer callback `[this]` capture 在
    // timer_->cancel() 返回后仍可能 in-flight (per ITimerService contract line 86
    // "cancel 与 callback 不互斥")。~Impl() 必须 barrier wait 保证 callback 结束前
    // 不进入 step ② timer_=nullptr. 计数 + cv 同步模式。
    std::atomic<int> in_flight_callbacks_{0};
    std::mutex in_flight_mutex_;
    std::condition_variable in_flight_cv_;

    // === chat-session-pdk-lift C1: I/O 注入 (取代 Impl 自有 self-pipe + std::cin/cerr) ===
    std::unique_ptr<agenticdsl::IInputSource> input_;
    std::unique_ptr<agenticdsl::ILogger> logger_;

    // === chat-session-pdk-lift C2: SessionManager 集成 ===
    agenticdsl::SessionManager* session_manager_ = nullptr;
    std::string current_branch_id_ = "main";
    std::string last_node_id_;   // 分支链尾, 供 append 挂 parent_id
    std::string active_session_id_;  // SessionManager 侧 session_id (JSONL 文件名)

    Impl(
        agenticdsl::DSLEngine* e,
        std::shared_ptr<agenticdsl::IInteractionBus> b,
        agenticdsl::IToolRegistry* r,
        const AgentConfig& a,
        const SessionConfig& s,
        std::shared_ptr<CancellationRegistry> registry_arg,
        agenticdsl::ITimerService* timer,
        std::unique_ptr<agenticdsl::IInputSource> input,
        std::unique_ptr<agenticdsl::ILogger> logger,
        agenticdsl::SessionManager* session_manager,
        std::optional<agenticdsl::ResumeToken> resume
    ) : engine(e), bus(std::move(b)), registry(r), agent_cfg(a), session_cfg(s),
        provider_mode(a.provider),
        persist_dir_expanded(expand_home(s.persist_dir)),
        timer_(timer),
        // §4.0.2/§4.0.9 NC3: shared registry if provided, else fallback self-owned
        cancellation_registry_(registry_arg ? registry_arg : std::make_shared<CancellationRegistry>()),
        // Pattern 5 fail-safe: 显式注入才用替身, 否则生产默认实现
        input_(input ? std::move(input) : std::make_unique<agenticdsl::StdinInputSource>()),
        logger_(logger ? std::move(logger) : std::make_unique<agenticdsl::StderrLogger>()),
        session_manager_(session_manager) {
        if (!persist_dir_expanded.empty()) {
            ensure_dir_0700(persist_dir_expanded);
        }
        if (resume.has_value() && session_manager_ != nullptr) {
            hydrate_from_resume(*resume);
        }
        if (session_cfg.enable_input_thread) {
            input_thread_ = std::thread([this]() { input_thread_main(); });
        }
    }

    ~Impl() {
        // fix-tsan-residual-2026-09-15: 5 步析构顺序 + step ①.5 barrier wait
        // (per ITimerService contract line 86 "cancel 与 callback 不互斥"
        //  + "外部注入 timer 必须自行保证生命周期").
        //
        // ① cancel periodic timer (atomic exchange 0)
        auto id = periodic_id_.exchange(0, std::memory_order_acq_rel);
        if (id != 0 && timer_) {
            timer_->cancel(id);
        }
        // ①.5 barrier wait: cancel 不互斥 callback, 必须等所有 in-flight 结束
        // 否则 callback 进入 body 访问 this->shutdown_check_pending_ / this->input_
        // 时 Impl 可能已开始销毁 → UB. in_flight_callbacks_ RAII decrement by callback.
        {
            std::unique_lock<std::mutex> lock(in_flight_mutex_);
            in_flight_cv_.wait(lock, [this] {
                return in_flight_callbacks_.load(std::memory_order_acquire) == 0;
            });
        }
        // ② timer_=nullptr (现在安全, 因为所有 callback 已结束)
        timer_ = nullptr;
        // ③ stop_input_thread_ 置位 + 唤醒 (让阻塞在 read_line 的 input thread 退出)
        stop_input_thread_.store(true);
        if (input_) {
            input_->close();
        }
        // ④ notify cv so any blocked pop_next_input wakes and returns nullopt
        input_cv_.notify_all();
        if (input_thread_.joinable()) {
            input_thread_.join();
        }
        // ⑤ input_/logger_ unique_ptr 自动析构
        //    (self-pipe fd 关在 ~StdinInputSource 内, Sprint 31 D4 语义保留)
    }

private:
    void input_thread_main();

public:
    // Change 2 (Task 3): 从 ResumeToken 恢复 messages (经 SessionManager)
    // 注: 这三个 helper 必须对 ChatSession 可见 (外层类无法访问嵌套类 private 成员),
    // 故放 public 段; Impl 本身是私有实现细节, 不对外暴露。
    void hydrate_from_resume(const agenticdsl::ResumeToken& token);

    // Change 2 (Task 4): 一轮对话落 JSONL (user + assistant 两节点)
    void persist_turn(const std::string& user_input, const std::string& assistant_input,
                      const std::string& session_id);

    // A5.6: topic 分级发射 — persist=true 时标记 meta.persist 供 AppendOnlyEventLog 订阅者识别
    void emit_topic(const std::string& topic, nlohmann::json args, nlohmann::json meta,
                    bool persist);
};

// --- ChatSession ---

ChatSession::ChatSession(
    agenticdsl::DSLEngine* engine,
    std::shared_ptr<agenticdsl::IInteractionBus> bus,
    agenticdsl::IToolRegistry* registry,
    const AgentConfig& agent_cfg,
    const SessionConfig& session_cfg,
    std::shared_ptr<CancellationRegistry> registry_arg,
    agenticdsl::ITimerService* timer,
    std::unique_ptr<agenticdsl::IInputSource> input,
    std::unique_ptr<agenticdsl::ILogger> logger,
    agenticdsl::SessionManager* session_manager,
    std::optional<agenticdsl::ResumeToken> resume
) : impl_(std::make_unique<Impl>(engine, std::move(bus), registry, agent_cfg, session_cfg,
                                 registry_arg, timer, std::move(input), std::move(logger),
                                 session_manager, std::move(resume))) {
    // 生成 session ID (UUID 简化版)
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    std::ostringstream oss;
    oss << "sess_" << std::hex << dis(gen) << dis(gen);
    session_id_ = oss.str();

    // T1 线程安全: budget.checked 回调仅置 atomic flag, 不触 TUI
    if (impl_->bus) {
        impl_->bus->subscribe("budget.checked",
            [this](const agenticdsl::BusEvent&) {
                budget_alert_flag_.store(true, std::memory_order_release);
            });
    }
}

ChatSession::~ChatSession() = default;

void ChatSession::request_stop() {
  // 锁内只复制 id, 锁外做 resolve/request_stop/notify (避免持锁回调)
  std::string cancellation_id;
  {
    std::lock_guard<std::mutex> lock(impl_->cancellation_mutex_);
    cancellation_id = impl_->current_cancellation_id_;
  }
  if (cancellation_id.empty()) return;
  auto source = impl_->cancellation_registry_->resolve_source(cancellation_id);
  if (source) source->request_stop();
  // §2.3: wake any blocked pop_next_input (cheap, idempotent)
  impl_->input_cv_.notify_all();
  impl_->emit_topic("session.disconnected",
                    nlohmann::json{{"session_id", session_id_},
                                   {"reason", "request_stop"}},
                    nlohmann::json{{"session_id", session_id_}}, /*persist=*/true);
}

bool ChatSession::request_model_switch(const std::string& provider_name) {
  if (provider_name.empty()) return false;
  if (impl_->provider_mode == "mock" && provider_name != "mock") {
    impl_->logger_->log(agenticdsl::LogLevel::kWarn,
                        "[chat] mock mode rejects provider: " + provider_name);
    return false;
  }
  std::lock_guard<std::mutex> lock(impl_->next_model_mutex_);
  impl_->next_model_ = provider_name;
  return true;
}

std::string ChatSession::next_model() const {
  std::lock_guard<std::mutex> lock(impl_->next_model_mutex_);
  return impl_->next_model_;
}

ChatResult ChatSession::chat(const std::string& user_input) {
    return chat(user_input, std::stop_token{});
}

ChatResult ChatSession::chat(const std::string& user_input, std::stop_token token) {
    ChatResult result;

    // D8 锁顺序契约 (chat-session-pdk-lift Change 2 acceptance):
    //   1. steering_mutex_ / follow_up_mutex_  (双队列锁)
    //   2. input_cv_mutex_                     (pop_next_input 等待)
    //   3. SessionManager write_mutex_         (flush_append, 经 persist_turn)
    // 禁止反向持有。SessionManager 内部另有 index_mutex_, 其自身顺序为
    // write_mutex_ → index_mutex_ (open/flush_append 路径); 反向路径
    // (index → write) 仅存在于 migrate_legacy_json 的 branch-meta 写入,
    // 属 baseline pre-existing (见 plan Task 11.4 known-issue), 本 change 不触碰。

    // A5.6 分级: turn/steering/followup/resumed/disconnected = slow-path (persist=true),
    // 由 AppendOnlyEventLog 订阅者按 meta.persist 落地。
    impl_->emit_topic("chat.turn.start",
               nlohmann::json{{"session_id", session_id_},
                              {"turn_count", static_cast<int>(impl_->messages.size() / 2)}},
               nlohmann::json{{"session_id", session_id_}}, /*persist=*/true);

    // §4.0.5: ALWAYS build stop_source + register, regardless of token.stop_possible()
    std::string cancellation_id;
    auto source = std::make_shared<std::stop_source>();
    cancellation_id = impl_->cancellation_registry_->register_source(source);
    {
        std::lock_guard<std::mutex> lock(impl_->cancellation_mutex_);
        impl_->current_cancellation_id_ = cancellation_id;
        if (token.stop_possible()) {
            impl_->current_token_ = token;
        }
    }

    // 2. 追加用户消息到历史
    nlohmann::json user_msg = {
        {"role", "user"},
        {"content", user_input},
        {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
    };
    impl_->messages.push_back(user_msg);

    // 2. emit "user.input" 事件 (ADR-0068 §4 — args=业务字段, meta=trace context)
    impl_->bus->emit(agenticdsl::EventBuilder("user.input")
        .args(nlohmann::json{{"input", user_input}})
        .meta(nlohmann::json{{"session_id", session_id_}})
        .build());

    // 3. 获取 LLM 响应 — 统一经 Loop Agent 工具 (loop/run) 执行
    //    loop_agent 内部决定 mock fallback (parent provider 未设置) 或真实 DSL 执行
    try {
        // Loop Agent 工具 — 唯一 ReAct 执行路径
        std::unordered_map<std::string, std::string> loop_args;
        loop_args["loop_type"] = impl_->agent_cfg.loop_type;
        loop_args["prompt"] = user_input;
        loop_args["system_prompt"] = impl_->agent_cfg.system_prompt;
        loop_args["history"] = nlohmann::json(impl_->messages).dump();
        loop_args["tools"] = nlohmann::json(impl_->agent_cfg.tools).dump();
        loop_args["max_steps"] = std::to_string(impl_->agent_cfg.max_steps);
        // 将 bus 与会话 ID 透传给 loop_agent, 用于真实事件发射
        loop_args["bus_ptr"] = ptr_to_str(impl_->bus.get());  // AUDIT: ptr_to_str 唯一合法使用点, 禁止扩展
        loop_args["session_id"] = session_id_;
        loop_args["cancellation_id"] = cancellation_id;

        nlohmann::json loop_result = impl_->registry->call_tool("loop/run", loop_args);

        // Cleanup cancellation state
        if (!cancellation_id.empty()) {
            impl_->cancellation_registry_->unregister(cancellation_id);
            std::lock_guard<std::mutex> lock(impl_->cancellation_mutex_);
            impl_->current_token_ = std::stop_token{};
            impl_->current_cancellation_id_.clear();
        }

        // C3 三层 fallback (per openspec/changes/2026-09-16-fix-loop-run-return-contract):
        //   1. 新契约: 优先读 "ok" 字段
        //   2. 旧 loop/run: 缺 "ok" 时回落到 "success" 字段
        //   3. Registry 错误信封: 两者都缺时,若含 "error" 字段则视为失败
        //   4. 真正兼容: 缺所有字段时视为成功, 保持 legacy happy path
        bool loop_ok = loop_result.value(
            "ok",
            loop_result.value(
                "success",
                !loop_result.contains("error")));
        result.response = loop_result.value("response", "");
        result.total_steps = loop_result.value("steps", 0);
        result.total_tokens = loop_result.value("tokens_used", 0);
        result.cost_usd = loop_result.value("cost_usd", 0.0);
        result.success = loop_ok;
        if (!loop_ok) {
            result.error_message = loop_result.value("error", "Unknown loop failure");
        }

        if (result.success) {
            // 4. 追加 assistant 消息到历史
            nlohmann::json assistant_msg = {
                {"role", "assistant"},
                {"content", result.response},
                {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()},
                {"steps", result.total_steps},
                {"tokens", result.total_tokens}
            };
            impl_->messages.push_back(assistant_msg);

            // 5. emit "loop.done" (ADR-0068 §4 — args=业务字段, meta=trace context)
            impl_->bus->emit(agenticdsl::EventBuilder("loop.done")
                .args(nlohmann::json{
                    {"response", result.response},
                    {"total_steps", result.total_steps},
                    {"total_tokens", result.total_tokens}
                })
                .meta(nlohmann::json{{"session_id", session_id_}})
                .build());

            // T1 Budget 告警: 每轮后轮询 engine budget controller
            // M5 修正: budget 信息只进 budget.checked 事件, 不覆盖 result.success/error_message
            // (成功路径下 budget 超额仍标 success=true, 仅通过事件事件告知 UI)
            if (impl_->engine) {
                const auto& bc = impl_->engine->get_budget_controller();
                if (bc.exceeded()) {
                    double used = bc.get_total_cost_usd();
                    double limit = impl_->agent_cfg.budget_limit_usd;
                    impl_->bus->emit(agenticdsl::EventBuilder("budget.checked")
                        .args(nlohmann::json{
                            {"limit", limit},
                            {"used", used},
                            {"unit", "llm_calls"},
                            {"reason", "cost_limit"},
                            {"ok", false}
                        })
                        .meta(nlohmann::json{{"session_id", session_id_}})
                        .build());
                }
            }

            // 6. 持久化 (异步, 简化版 fire-and-forget)
            if (impl_->session_cfg.persist_dir != "") {
                impl_->bus->emit(agenticdsl::EventBuilder("session.persist_request")
                    .args(nlohmann::json{{"messages", nlohmann::json(impl_->messages)}})
                    .meta(nlohmann::json{{"session_id", session_id_}})
                    .build());
                // T1: 同步落盘 (原子写入) - 确保跨进程可恢复
                save_to_disk();
            }
        } else {
            // C3 新增: 错误路径分支 (loop_result.ok == false)
            // 不追加 assistant 消息 (保持 history 干净)
            // emit loop.error (替代原成功路径的 loop.done)
            // Major #2 修正 (Oracle review): registry "Tool not found" 错误信封
            // 不带 error_code 字段, 默认 "Unknown" 错失语义信息. 在此 remap
            // "Tool not found" 前缀为 "ToolNotRegistered" 对齐 ADR-0023.
            std::string remapped_error_code = loop_result.value("error_code", std::string{"Unknown"});
            if (remapped_error_code == "Unknown" &&
                result.error_message.rfind("Tool not found", 0) == 0) {
                remapped_error_code = "ToolNotRegistered";
            }
            impl_->bus->emit(agenticdsl::EventBuilder("loop.error")
                .args(nlohmann::json{
                    {"error", result.error_message},
                    {"error_code", remapped_error_code}
                })
                .meta(nlohmann::json{{"session_id", session_id_}})
                .build());

            // 错误路径下也检查 budget, 但不覆盖 result.error_message (M5)
            // (budget 信息独立事件化, 不污染 loop 错误主因)
            if (impl_->engine) {
                const auto& bc = impl_->engine->get_budget_controller();
                if (bc.exceeded()) {
                    double used = bc.get_total_cost_usd();
                    double limit = impl_->agent_cfg.budget_limit_usd;
                    impl_->bus->emit(agenticdsl::EventBuilder("budget.checked")
                        .args(nlohmann::json{
                            {"limit", limit},
                            {"used", used},
                            {"unit", "llm_calls"},
                            {"reason", "cost_limit"},
                            {"ok", false}
                        })
                        .meta(nlohmann::json{{"session_id", session_id_}})
                        .build());
                }
            }
        }
    } catch (const std::exception& e) {
        // catch 路径与 ok=false 路径合并: 统一 emit loop.error, 含 error_code="Unknown"
        result.success = false;
        if (result.error_message.empty()) {
            result.error_message = e.what();
        }
        impl_->bus->emit(agenticdsl::EventBuilder("loop.error")
            .args(nlohmann::json{
                {"error", result.error_message},
                {"error_code", "Unknown"}
            })
            .meta(nlohmann::json{{"session_id", session_id_}})
            .build());
    }

    // Change 2 (Task 4): 一轮结束落 JSONL (经 SessionManager) — 断线恢复的持久化来源
    if (impl_->session_manager_ != nullptr) {
        impl_->persist_turn(user_input, result.response, session_id_);
    }

    impl_->emit_topic("chat.turn.end",
               nlohmann::json{{"session_id", session_id_},
                              {"turn_count", static_cast<int>(impl_->messages.size() / 2)},
                              {"ok", result.success}},
               nlohmann::json{{"session_id", session_id_}}, /*persist=*/true);

    return result;
}

std::vector<nlohmann::json> ChatSession::history() const {
    return impl_->messages;
}

// === T1: Session 持久化实现 ===

namespace {

std::filesystem::path session_file_path(const std::string& dir, const std::string& id) {
    return std::filesystem::path(dir) / (id + ".json");
}

}  // namespace

bool ChatSession::load_from_disk(const std::string& session_id) {
    namespace fs = std::filesystem;
    auto path = session_file_path(impl_->persist_dir_expanded, session_id);

    std::error_code ec;
    if (!fs::exists(path, ec)) {
        return false;
    }

    std::ifstream f(path);
    if (!f.is_open()) {
        impl_->logger_->log(agenticdsl::LogLevel::kWarn,
                            "[session/load] cannot open: " + path.string());
        return false;
    }

    nlohmann::json j;
    try {
        f >> j;
    } catch (const nlohmann::json::parse_error& e) {
        (void)e;
        impl_->logger_->log(agenticdsl::LogLevel::kWarn,
                            "[session/load] invalid JSON: " + path.string());
        return false;
    } catch (const std::exception& e) {
        impl_->logger_->log(agenticdsl::LogLevel::kWarn,
                            "[session/load] error: " + path.string() + ": " + e.what());
        return false;
    }

    // schema 版本校验
    int sv = j.value("schema_version", 0);
    if (sv != kSessionSchemaVersion) {
        impl_->logger_->log(agenticdsl::LogLevel::kWarn,
                            "[session/load] unsupported schema_version "
                            + std::to_string(sv) + " (expected "
                            + std::to_string(kSessionSchemaVersion) + "): " + path.string());
        return false;
    }

    // T1.7 provider_mode reconcile: 恢复 history 但用本次 provider
    session_id_ = j.value("session_id", session_id);
    impl_->messages.clear();
    if (j.contains("history") && j["history"].is_array()) {
        for (const auto& msg : j["history"]) {
            impl_->messages.push_back(msg);
        }
    }
    // provider_mode 不恢复 - 保持本次构造时的 provider
    return true;
}

bool ChatSession::save_to_disk() {
    namespace fs = std::filesystem;
    if (impl_->persist_dir_expanded.empty()) return false;

    if (!ensure_dir_0700(impl_->persist_dir_expanded)) return false;

    auto path = session_file_path(impl_->persist_dir_expanded, session_id_);
    auto tmp = path;
    tmp += ".tmp";

    nlohmann::json j;
    j["schema_version"] = kSessionSchemaVersion;
    j["session_id"] = session_id_;
    auto now = std::chrono::system_clock::now();
    auto epoch = now.time_since_epoch().count();
    j["created_at"] = epoch;
    j["updated_at"] = epoch;
    j["provider_mode"] = impl_->provider_mode;
    j["budget"] = {
        {"total", impl_->agent_cfg.budget_limit_usd},
        {"used", impl_->engine ? impl_->engine->get_budget_controller().get_total_cost_usd() : 0.0}
    };
    j["history"] = nlohmann::json(impl_->messages);

    {
        std::ofstream f(tmp);
        if (!f.is_open()) {
            impl_->logger_->log(agenticdsl::LogLevel::kWarn,
                                "[session/save] cannot write tmp: " + tmp.string());
            return false;
        }
        f << j.dump(2);
    }

    std::error_code ec;
    fs::rename(tmp, path, ec);
    if (ec) {
        impl_->logger_->log(agenticdsl::LogLevel::kWarn,
                            "[session/save] rename failed: " + ec.message());
        std::error_code rm_ec;
        fs::remove(tmp, rm_ec);
        return false;
    }

    // ADR-0068 §4 — 仅在原子 rename 成功后 emit session.persisted
    if (impl_->bus) {
        impl_->bus->emit(agenticdsl::EventBuilder("session.persisted")
            .args(nlohmann::json{
                {"session_id", session_id_},
                {"path", path.string()}
            })
            .meta(nlohmann::json{{"session_id", session_id_}})
            .build());
    }
    return true;
}

std::vector<std::string> ChatSession::list_sessions(const std::string& persist_dir) {
    namespace fs = std::filesystem;
    std::vector<std::string> ids;
    auto dir = expand_home(persist_dir);
    if (dir.empty()) return ids;

    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return ids;

    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        auto p = entry.path();
        if (p.extension() == ".json") {
            ids.push_back(p.stem().string());
        }
    }
    return ids;
}

void ChatSession::cleanup_stale(const std::string& persist_dir, long long max_age_seconds) {
    namespace fs = std::filesystem;
    auto dir = expand_home(persist_dir);
    if (dir.empty()) return;

    std::error_code ec;
    if (!fs::exists(dir, ec) || !fs::is_directory(dir, ec)) return;

    auto now = fs::file_time_type::clock::now();
    for (const auto& entry : fs::directory_iterator(dir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".json") continue;

        auto lwt = entry.last_write_time(ec);
        if (ec) {
            const std::string msg = "[session/cleanup] stat failed: " + entry.path().string();
            detail::log_static_diag(agenticdsl::LogLevel::kWarn, msg);
            continue;
        }
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - lwt).count();
        if (age > max_age_seconds) {
            std::error_code rm_ec;
            fs::remove(entry.path(), rm_ec);
            if (rm_ec) {
                const std::string msg = "[session/cleanup] remove failed: " + entry.path().string()
                                      + ": " + rm_ec.message();
                detail::log_static_diag(agenticdsl::LogLevel::kWarn, msg);
            }
        }
    }
}

bool ChatSession::consume_budget_alert() {
    return budget_alert_flag_.exchange(false, std::memory_order_acq_rel);
}

// === Queue infrastructure (Phase A) ===

size_t ChatSession::queue_size(QueueKind kind) const {
    std::lock_guard<std::mutex> lock(
        kind == QueueKind::Steering ? impl_->steering_mutex_ : impl_->follow_up_mutex_);
    return (kind == QueueKind::Steering ? impl_->steering_queue_ : impl_->follow_up_queue_).size();
}

bool ChatSession::try_push_steering_for_test(const std::string& msg) {
    std::lock_guard<std::mutex> lock(impl_->steering_mutex_);
    if (impl_->steering_queue_.size() >= impl_->capacity_) {
        impl_->logger_->log(agenticdsl::LogLevel::kWarn,
                            "[chat] steering queue overflow, rejected length="
                            + std::to_string(msg.size()));
        return false;
    }
    impl_->steering_queue_.push(msg);
    // §2.5 NC2 fix: increment count atomically so pop_next_input predicate wakes
    impl_->pending_input_count_.fetch_add(1, std::memory_order_release);
    impl_->input_cv_.notify_one();
    return true;
}

bool ChatSession::try_push_follow_up_for_test(const std::string& msg) {
    std::lock_guard<std::mutex> lock(impl_->follow_up_mutex_);
    if (impl_->follow_up_queue_.size() >= impl_->capacity_) {
        impl_->logger_->log(agenticdsl::LogLevel::kWarn,
                            "[chat] follow_up queue overflow, rejected length="
                            + std::to_string(msg.size()));
        return false;
    }
    impl_->follow_up_queue_.push(msg);
    // §2.6 NC2 fix: increment count atomically so pop_next_input predicate wakes
    impl_->pending_input_count_.fetch_add(1, std::memory_order_release);
    impl_->input_cv_.notify_one();
    return true;
}

size_t ChatSession::try_clear_queue(QueueKind kind) {
    std::lock_guard<std::mutex> lock(
        kind == QueueKind::Steering ? impl_->steering_mutex_ : impl_->follow_up_mutex_);
    auto& q = (kind == QueueKind::Steering ? impl_->steering_queue_ : impl_->follow_up_queue_);
    size_t count = q.size();
    while (!q.empty()) q.pop();
    // §2.7 NC2 fix: fetch_sub(N) — preserves count invariant for the other queue
    impl_->pending_input_count_.fetch_sub(count, std::memory_order_acq_rel);
    return count;
}

std::optional<InputMessage> ChatSession::try_pop_input() {
    // §2.1 priority: steering before follow-up
    {
        std::lock_guard<std::mutex> lock(impl_->steering_mutex_);
        if (!impl_->steering_queue_.empty()) {
            InputMessage msg{QueueKind::Steering, impl_->steering_queue_.front()};
            impl_->steering_queue_.pop();
            impl_->pending_input_count_.fetch_sub(1, std::memory_order_acq_rel);
            return msg;
        }
    }
    {
        std::lock_guard<std::mutex> lock(impl_->follow_up_mutex_);
        if (!impl_->follow_up_queue_.empty()) {
            InputMessage msg{QueueKind::FollowUp, impl_->follow_up_queue_.front()};
            impl_->follow_up_queue_.pop();
            impl_->pending_input_count_.fetch_sub(1, std::memory_order_acq_rel);
            return msg;
        }
    }
    return std::nullopt;
}

std::optional<InputMessage> ChatSession::pop_next_input(std::chrono::milliseconds timeout) {
    // §2.2 fast-path: skip wait if already pending (avoids spurious wakeup overhead)
    if (impl_->pending_input_count_.load(std::memory_order_acquire) > 0) {
        if (auto msg = try_pop_input()) {
            return msg;
        }
    }
    std::unique_lock<std::mutex> lock(impl_->input_cv_mutex_);
    // §2.2/§2.4 predicate uses stop_input_thread_ as shutdown signal + count, NOT queue.empty()
    bool woken = impl_->input_cv_.wait_for(lock, timeout, [this]() {
        return impl_->stop_input_thread_.load(std::memory_order_acquire) ||
               impl_->pending_input_count_.load(std::memory_order_acquire) > 0;
    });
    if (!woken) return std::nullopt;  // timeout
    if (impl_->stop_input_thread_.load(std::memory_order_acquire) &&
        impl_->pending_input_count_.load(std::memory_order_acquire) == 0) {
        return std::nullopt;  // shutdown with empty queues
    }
    return try_pop_input();
}

std::optional<InputMessage> ChatSession::try_peek_input() const {
    // §2.8 NH1 fix: peek only, never consume, never modify count
    {
        std::lock_guard<std::mutex> lock(impl_->steering_mutex_);
        if (!impl_->steering_queue_.empty()) {
            return InputMessage{QueueKind::Steering, impl_->steering_queue_.front()};
        }
    }
    {
        std::lock_guard<std::mutex> lock(impl_->follow_up_mutex_);
        if (!impl_->follow_up_queue_.empty()) {
            return InputMessage{QueueKind::FollowUp, impl_->follow_up_queue_.front()};
        }
    }
    return std::nullopt;
}

bool ChatSession::is_input_thread_shutdown() const {
    return impl_->stop_input_thread_.load(std::memory_order_acquire);
}

void ChatSession::Impl::input_thread_main() {
    // Sprint 30 (D2/D5) + Sprint 31 (D3): periodic timer (50ms) 唤醒 input 等待。
    // C1 lift 后改写: timer callback 不再直接写自有 pipe fd, 而是调 input_->wake()
    // 让 IInputSource 内部的 self-pipe 完成 poll 中断 (self-pipe 所有权已下沉)。
    // D9 fallback: timer_==nullptr 时不注册 (IInputSource 自身 100ms poll clamp 兜底)。
    // RAII guard cancels timer on all exit paths (EOF break / catch / normal return)。
    //
    // fix-tsan-residual-2026-09-15: timer callback 用 RAII 计数 + notify 追踪 in-flight
    // (per ITimerService contract line 86 "cancel 与 callback 不互斥"). ~Impl()
    // 在 timer_=nullptr 之前 wait in-flight == 0 保证 callback 结束前成员存活。
    if (timer_ != nullptr) {
        periodic_id_.store(timer_->register_periodic(
            std::chrono::milliseconds(50),
            [this] {
                in_flight_callbacks_.fetch_add(1, std::memory_order_acq_rel);
                // RAII: 即使 body 抛异常, 析构仍 decrement + notify (避免 deadlock)
                struct CallbackGuard {
                    std::atomic<int>* ctr;
                    std::condition_variable* cv;
                    ~CallbackGuard() {
                        ctr->fetch_sub(1, std::memory_order_acq_rel);
                        cv->notify_all();
                    }
                } guard{&in_flight_callbacks_, &in_flight_cv_};
                shutdown_check_pending_.store(true, std::memory_order_release);
                if (input_) {
                    input_->wake();
                }
            }), std::memory_order_release);
    }
    struct TimerGuard {
        agenticdsl::ITimerService* timer;
        std::atomic<agenticdsl::ITimerService::TimerId>* id_ptr;
        ~TimerGuard() {
            auto id = id_ptr->load(std::memory_order_acquire);
            if (timer && id != 0) {
                timer->cancel(id);
                id_ptr->store(0, std::memory_order_release);
            }
        }
    } timer_guard{timer_, &periodic_id_};

    while (!stop_input_thread_.load(std::memory_order_acquire)) {
        // 阻塞读 (timeout 100ms) — 实际 poll 多 fd + EINTR 重试在 IInputSource 内
        auto line_opt = input_->read_line(std::chrono::milliseconds(100));
        if (!line_opt.has_value()) {
            if (input_->at_eof() || stop_input_thread_.load(std::memory_order_acquire)) {
                // §3.4 NH2 fix: EOF 必须 signal shutdown AND wake blocked pop_next_input
                stop_input_thread_.store(true, std::memory_order_release);
                input_cv_.notify_all();
                break;
            }
            continue;  // timeout / 仅 wake-up byte
        }
        std::string line = std::move(*line_opt);
        if (line.empty()) continue;

        if (line.front() == '/') {
            std::lock_guard<std::mutex> lock(steering_mutex_);
            if (steering_queue_.size() < capacity_) {
                steering_queue_.push(line);
                // §3.1 push + count + notify in same mutex scope (C3 ordering fix)
                pending_input_count_.fetch_add(1, std::memory_order_release);
                emit_topic("chat.steering.enqueued",
                           nlohmann::json{{"session_id", active_session_id_},
                                          {"line_preview", line.substr(0, 64)}},
                           nlohmann::json::object(), /*persist=*/false);
            } else {
                // §3.3 overflow: do NOT increment count, do NOT notify (no spurious wakeup)
                logger_->log(agenticdsl::LogLevel::kWarn,
                             "[chat] steering queue overflow, rejected length="
                             + std::to_string(line.size()));
                continue;
            }
        } else {
            std::lock_guard<std::mutex> lock(follow_up_mutex_);
            if (follow_up_queue_.size() < capacity_) {
                follow_up_queue_.push(line);
                pending_input_count_.fetch_add(1, std::memory_order_release);
                emit_topic("chat.followup.enqueued",
                           nlohmann::json{{"session_id", active_session_id_},
                                          {"line_preview", line.substr(0, 64)}},
                           nlohmann::json::object(), /*persist=*/false);
            } else {
                logger_->log(agenticdsl::LogLevel::kWarn,
                             "[chat] follow_up queue overflow, rejected length="
                             + std::to_string(line.size()));
                continue;
            }
        }
        // §3.1/§3.2 notify OUTSIDE mutex (avoids holding mutex during wake; reduces contention)
        input_cv_.notify_one();
    }
}

// === chat-session-pdk-lift Change 2: SessionManager 集成 ===

void ChatSession::Impl::hydrate_from_resume(const agenticdsl::ResumeToken& token) {
    // 真实 API (session_manager.h): open(session_id) → SessionHandle (struct),
    // load_jsonl() 无参填充索引, build_context_entries(leaf_node_id) 为 SessionManager 成员。
    session_manager_->open(token.session_id);
    session_manager_->load_jsonl();
    active_session_id_ = token.session_id;

    const auto entries = session_manager_->build_context_entries(token.leaf_node_id);
    for (const auto& node : entries) {
        if (node.content.is_object()) {
            messages.push_back(node.content);
        }
    }
    last_node_id_ = token.leaf_node_id;
    if (!entries.empty() && !entries.back().branch_id.empty()) {
        current_branch_id_ = entries.back().branch_id;
    }

    // model 一致性: 记录 token.model, 不覆盖本次构造的 provider_mode (D4 裁决: 不持久化 provider)
    if (!token.model.empty()) {
        provider_mode = agent_cfg.provider;
    }

    if (bus) {
        bus->emit(agenticdsl::EventBuilder("session.resumed")
                      .args(nlohmann::json{{"session_id", token.session_id},
                                           {"leaf_node_id", token.leaf_node_id},
                                           {"messages_count",
                                            static_cast<int>(messages.size())}})
                      .meta(nlohmann::json{{"session_id", token.session_id}})
                      .build());
    }
    (void)token.budget_used;  // 由 BudgetController 承担, ChatSession 不重复记账
}

void ChatSession::Impl::persist_turn(const std::string& user_input,
                                     const std::string& assistant_input,
                                     const std::string& sid) {
    // SessionNode 真实字段: {id, parent_id, branch_id, content}
    // flush_append 返回 void; append_to_branch 是 1 参数且只接受消息字符串,
    // 故此处用 next_node_id() + flush_append 显式构造两节点 (保留 role 语义)。
    const std::string user_node_id = session_manager_->next_node_id();
    agenticdsl::SessionNode user_node;
    user_node.id = user_node_id;
    user_node.parent_id = last_node_id_;
    user_node.branch_id = current_branch_id_.empty() ? "main" : current_branch_id_;
    user_node.content = nlohmann::json{{"role", "user"},
                                       {"content", user_input},
                                       {"session_id", sid}};
    session_manager_->flush_append(user_node);
    last_node_id_ = user_node_id;

    const std::string assistant_node_id = session_manager_->next_node_id();
    agenticdsl::SessionNode assistant_node;
    assistant_node.id = assistant_node_id;
    assistant_node.parent_id = last_node_id_;
    assistant_node.branch_id = user_node.branch_id;
    assistant_node.content = nlohmann::json{{"role", "assistant"},
                                            {"content", assistant_input},
                                            {"session_id", sid}};
    session_manager_->flush_append(assistant_node);
    last_node_id_ = assistant_node_id;
}

void ChatSession::Impl::emit_topic(const std::string& topic, nlohmann::json args,
                                   nlohmann::json meta, bool persist) {
    if (!bus) return;
    // A5.6 分级: fast-path (steering/followup enqueue) 只走 bus 内存分发;
    // slow-path 额外标记 meta.persist=true, 供 AppendOnlyEventLog 订阅者识别并落地。
    if (persist) {
        meta["persist"] = true;
    }
    bus->emit(agenticdsl::EventBuilder(topic).args(std::move(args)).meta(std::move(meta)).build());
}

// === chat-session-static-logger-injection: 进程级 default logger (Meyers singleton) ===

std::unique_ptr<agenticdsl::ILogger>& ChatSession::default_logger_slot() {
    static std::unique_ptr<agenticdsl::ILogger> slot;  // C++11 magic statics 线程安全初始化
    return slot;
}

void ChatSession::set_default_logger(std::unique_ptr<agenticdsl::ILogger> logger) {
    default_logger_slot() = std::move(logger);
}

agenticdsl::ILogger* ChatSession::get_default_logger() {
    return default_logger_slot().get();
}

void ChatSession::clear_default_logger() {
    default_logger_slot().reset();
}

namespace detail {
void log_static_diag(agenticdsl::LogLevel level, const std::string& msg) {
    if (auto* logger = ChatSession::get_default_logger()) {
        logger->log(level, msg);
    } else {
        std::cerr << msg << std::endl;
    }
}
}

}  // namespace hydraforge::pdk
