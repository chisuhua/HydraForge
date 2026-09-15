// include/agenticdsl/pdk/chat_session.h
// ChatSession — 多轮对话编排器 (PDK lift)
// 关联: docs/adr/adr-0060-agent-composition.md
//      docs/adr/adr-0033-session-hierarchy.md
//      docs/superpowers/specs/2026-09-11-chat-session-pdk-lift-design.md §4 (D6.1)
//
// Lift 溯源: examples/pdk_chat_demo/chat_session.h (namespace pdk_chat_demo)。
// 本 lift 移入 `hydraforge::pdk` (D6.1) 并引入 IInputSource/ILogger 注入 (§6.1)。
//
// 兼容: examples/pdk_chat_demo/chat_session.h 保留为转发 shim + namespace alias,
//   pdk_chat_demo::* 旧名继续可用 (1 个 Sprint 兼容期)。
//
// Sprint 32 教训保留: 用完整 include 而非 forward decl block —
//   forward decl 在 commands/*.cpp 被嵌套 include 时会变成 pdk_chat_demo::agenticdsl,
//   导致类型不可见 (Sprint 30 的 PIMPL void* workaround 根因)。

#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <stop_token>

#include <nlohmann/json.hpp>

#include <agenticdsl/contract/ilogger.h>
#include <agenticdsl/contract/iinput_source.h>
#include <agenticdsl/contract/iinteraction_bus.h>
#include <agenticdsl/contract/itool_registry.h>
#include <agenticdsl/contract/resume_token.h>
#include <agenticdsl/contract/timer_service.h>
#include <agenticdsl/pdk/cancellation_registry.h>
#include <core/engine.h>
#include <core/session_manager.h>

namespace hydraforge::pdk {

namespace detail {
// 路由 helper (Oracle C2 抽离): 静态上下文统一入口。set 时走 ILogger, 未 set 时
// fallback std::cerr。测试通过此公开声明直接单测路由逻辑 (无需 chmod/root 失败注入)。
void log_static_diag(agenticdsl::LogLevel level, const std::string& msg);
}  // namespace detail

struct AgentConfig {
    std::string loop_type = "react";
    std::string provider = "mock";
    std::string model = "test";
    std::string system_prompt;
    std::vector<std::string> tools;
    int max_steps = 50;
    int timeout_ms = 300000;
    double budget_limit_usd = 1.0;
};

struct SessionConfig {
    std::string persist_dir = "~/.hydraforge/sessions/";
    int compact_threshold_tokens = 8000;
    bool branch_on_user_request = true;
    // ⚠️ 2026-09-09 默认值从 true 改为 false (fail-safe, AGENTS.md 模式 #5):
    // 默认 true 会让所有 SessionConfig{} 构造的 ChatSession 启动 stdin 读取线程,
    // 在交互终端 (stdin=TTY) 下永久阻塞 → ctest TIMEOUT kill。
    // 生产入口 main.cpp 显式置 true; 需要 stdin 的测试显式置 true。
    bool enable_input_thread = false;  // single-reader mode (chat-async-io-consumer-loop)
};

struct PluginConfig {
    std::string id;
    std::string path;
    std::string type = "so";      // so | skill | dsl | wasm
    std::string lifecycle = "eager";  // eager | lazy
    std::vector<std::string> activation_events;
    bool requires_isolation = false;
};

struct ObservabilityConfig {
    bool otel_enabled = false;
    std::string endpoint = "http://localhost:4318";
    double sample_rate = 1.0;
    std::string export_format = "otlp+http";
};

struct ChatConfig {
    std::string schema_version = "1.0";
    std::string app_id = "pdk_chat_demo";

    nlohmann::json providers;
    AgentConfig agent;
    std::vector<PluginConfig> plugins;
    nlohmann::json orchestration;
    ObservabilityConfig observability;
    SessionConfig session;
    nlohmann::json safety;

    // 从 JSON 文件加载
    static ChatConfig from_json(const std::string& path);

    // 切换到 mock provider (--mock flag)
    void override_provider(const std::string& provider, const std::string& model);

    void override_system_prompt(const std::string& overwrite,
                                const std::string& append);

    // 校验 manifest（schema 必填字段）
    void validate() const;
};

struct ChatResult {
    std::string response;
    int total_steps = 0;
    int total_tokens = 0;
    double cost_usd = 0.0;
    bool success = true;
    std::string error_message;
};

// QueueKind 标识 steering vs follow-up 队列
enum class QueueKind { Steering, FollowUp };

// InputMessage: 从队列取出的消息包装 (chat-async-io-consumer-loop §1.1)
struct InputMessage {
    QueueKind kind;
    std::string text;
};

// ChatSession: 多轮对话编排器
// - 持有 UserSession (ADR-0033)
// - 每轮：emit user.input -> call_tool("loop/run") -> 收集 result
// - 持久化 (load_from_disk/save_to_disk) + Budget 告警轮询
//
// 2026-09-14 实施偏差 (vs plan Task 7.1): 新增 input/logger 参数**追加在末尾**,
// 不插入中间。plan 原设计把新参数插在 cancellation_registry 之前/之后,
// 会让既有 20+ 个调用点 (5-7 参形态) 形参错位 → 全部编译失败。
class ChatSession {
public:
    ChatSession(
        agenticdsl::DSLEngine* engine,
        std::shared_ptr<agenticdsl::IInteractionBus> bus,
        agenticdsl::IToolRegistry* registry,
        const AgentConfig& agent_cfg,
        const SessionConfig& session_cfg,
        std::shared_ptr<CancellationRegistry> registry_arg = nullptr,
        // Sprint 31-32: ITimerService* (nullptr = D9 lazy)。
        // C1 lift 后 timer 仅用于周期性唤醒 input source, self-pipe 归 IInputSource 所有。
        agenticdsl::ITimerService* timer = nullptr,
        // chat-session-pdk-lift C1 (§6.1): I/O 注入。
        // nullptr → Impl 内部 fallback 到 StdinInputSource / StderrLogger,
        // 保持 lift 前行为 (Pattern 5 fail-safe: 显式注入才启用测试替身)。
        std::unique_ptr<agenticdsl::IInputSource> input = nullptr,
        std::unique_ptr<agenticdsl::ILogger> logger = nullptr,
        // Change 2 (§4, Task 0): 观察者指针, nullptr = 不做 JSONL 持久化/恢复。
        // 生命周期由调用方保证 (ChatSession 不持有)。
        agenticdsl::SessionManager* session_manager = nullptr,
        // Change 2 (§6.3): 断线恢复上下文。nullopt = 全新会话。
        std::optional<agenticdsl::ResumeToken> resume = std::nullopt
    );

    ~ChatSession();

    // Request cancellation of any in-flight chat operation.
    void request_stop();

    // Wave 3-A Phase C: request model switch for next turn.
    // Returns true if accepted, false if rejected (e.g., mock mode + non-mock provider).
    bool request_model_switch(const std::string& provider_name);

    // Wave 3-A Phase C: get pending model switch target (empty = no pending switch).
    std::string next_model() const;

    // Existing API: now with optional stop_token
    ChatResult chat(const std::string& user_input);
    ChatResult chat(const std::string& user_input, std::stop_token token);

    const std::string& session_id() const { return session_id_; }

    std::vector<nlohmann::json> history() const;

    // === Queue infrastructure (Phase A: steering + follow-up bounded queues) ===
    // queue_size returns current entry count (thread-safe, O(1))
    size_t queue_size(QueueKind kind) const;

    // try_clear_queue atomically empties the queue, returns count cleared
    size_t try_clear_queue(QueueKind kind);

    // Test-only injection helpers (production code uses input thread)
    bool try_push_steering_for_test(const std::string& msg);
    bool try_push_follow_up_for_test(const std::string& msg);

    // === chat-async-io-consumer-loop §1.x consumer API ===
    // try_pop_input: non-blocking priority pop (steering > follow-up); returns nullopt if both empty
    std::optional<InputMessage> try_pop_input();

    // pop_next_input: blocking pop with timeout; returns nullopt on timeout OR shutdown
    std::optional<InputMessage> pop_next_input(std::chrono::milliseconds timeout);

    // try_peek_input: non-blocking peek at front (steering > follow-up); does NOT consume
    std::optional<InputMessage> try_peek_input() const;

    // §7.4 fix: distinguish timeout vs shutdown (true after EOF / signal)
    bool is_input_thread_shutdown() const;

    // === T1: Session 持久化 (design.md §Session 持久化) ===
    // 从磁盘加载 session (persist_dir/<id>.json)
    bool load_from_disk(const std::string& session_id);

    // 保存当前 session 到磁盘 (原子写入: tmp + rename)
    bool save_to_disk();

    // 列出 persist_dir 下的所有 session_id (扫描 *.json)
    static std::vector<std::string> list_sessions(const std::string& persist_dir);

    // 清理 >24h 未活跃的 session 文件 (启动时调用)
    static void cleanup_stale(const std::string& persist_dir, long long max_age_seconds = 86400);

    // === T1: Budget 告警线程安全 (design.md §线程模型) ===
    // bus 回调置位此 flag; 主循环检查后渲染告警并重置
    std::atomic<bool> budget_alert_flag_{false};

    // 检查并消费 budget alert (主线程调用, 返回 true 表示有告警需渲染)
    bool consume_budget_alert();

    // chat-session-static-logger-injection: 进程级 default logger, 用于 ensure_dir_0700
    // / cleanup_stale 等无 Impl 实例的静态上下文。线程安全约束: main 启动期 set 一次,
    // 之后只读; 测试串行 set/clear。Meyers singleton per-binary (每个 link 单元独立副本)。
    static void set_default_logger(std::unique_ptr<agenticdsl::ILogger> logger);
    static agenticdsl::ILogger* get_default_logger();
    static void clear_default_logger();  // 显式 reset (测试 teardown)

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    std::string session_id_;

    static std::unique_ptr<agenticdsl::ILogger>& default_logger_slot();
};

}  // namespace hydraforge::pdk
