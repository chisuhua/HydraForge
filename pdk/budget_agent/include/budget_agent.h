// pdk/budget_agent/include/budget_agent.h
// Budget Store - 预算管理 + 跨 session 成本聚合
// 关联: docs/adr/adr-0033-session-hierarchy.md

#pragma once

#include <map>
#include <mutex>
#include <string>

#include <nlohmann/json.hpp>

namespace pdk_budget_agent {

// 单 session 成本跟踪
struct SessionCost {
    std::string session_id;
    double spent_usd = 0.0;
    int total_tokens = 0;
    int tool_calls = 0;
    long long last_updated_ms = 0;
};

// 告警订阅
struct BudgetAlert {
    double threshold;
    std::string severity;  // "warning" | "critical"
    std::string subscriber_id;
};

// 全局 BudgetStore
class BudgetStore {
public:
    static BudgetStore& instance();

    // 初始化全局限额
    void set_global_limit(double limit_usd);

    // 查询当前余额
    double remaining_usd() const;
    double spent_usd() const;
    double limit_usd() const;

    // 扣减预算（cost 来自 LLM API 调用）
    bool try_consume(double cost_usd, int tokens = 0);

    // Per-session 成本
    void record_session_cost(const std::string& session_id,
                             double cost_usd, int tokens, int tool_calls);

    // 设置单 session 限额
    void set_session_limit(const std::string& session_id, double limit_usd);

    // 跨 session 成本拆解
    nlohmann::json cost_breakdown() const;

    // 告警订阅
    void register_alert(const std::string& subscriber_id,
                        double threshold,
                        const std::string& severity);

    // 获取触发的告警列表
    std::vector<nlohmann::json> triggered_alerts() const;

private:
    BudgetStore() = default;

    mutable std::mutex mutex_;
    double limit_usd_ = 1.0;          // 全局默认限额
    double spent_usd_ = 0.0;
    int total_tokens_ = 0;
    std::map<std::string, SessionCost> session_costs_;
    std::map<std::string, double> session_limits_;
    std::vector<BudgetAlert> alerts_;
    std::vector<nlohmann::json> triggered_;
};

}  // namespace pdk_budget_agent