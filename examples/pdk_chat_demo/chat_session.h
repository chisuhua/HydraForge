// chat_session.h - Chat Session 编排器
// 关联: docs/adr/adr-0060-agent-composition.md
//      docs/adr/adr-0033-session-hierarchy.md

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace agenticdsl {
    class DSLEngine;
    class IToolRegistry;
    class IInteractionBus;
    class IBudgetController;
}

namespace pdk_chat_demo {

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

// ChatSession: 多轮对话编排器
// - 持有 UserSession (ADR-0033)
// - 每轮：emit user.input → call_tool("loop/run") → 收集 result
class ChatSession {
public:
    ChatSession(
        agenticdsl::DSLEngine* engine,
        std::shared_ptr<agenticdsl::IInteractionBus> bus,
        agenticdsl::IToolRegistry* registry,
        const AgentConfig& agent_cfg,
        const SessionConfig& session_cfg
    );

    ~ChatSession();

    // 处理一轮用户输入
    ChatResult chat(const std::string& user_input);

    // 获取 session ID
    const std::string& session_id() const { return session_id_; }

    // 获取历史消息
    std::vector<nlohmann::json> history() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    std::string session_id_;
};

}  // namespace pdk_chat_demo