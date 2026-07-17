// pdk/budget_agent/src/budget_store.cpp
// Budget Store 实现
// 关联: docs/adr/adr-0033-session-hierarchy.md

#include "budget_agent.h"

#include <algorithm>
#include <chrono>

namespace pdk_budget_agent {

namespace {

long long now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

}  // namespace

BudgetStore& BudgetStore::instance() {
    static BudgetStore inst;
    return inst;
}

void BudgetStore::set_global_limit(double limit_usd) {
    std::lock_guard<std::mutex> lock(mutex_);
    limit_usd_ = limit_usd;
}

double BudgetStore::remaining_usd() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return std::max(0.0, limit_usd_ - spent_usd_);
}

double BudgetStore::spent_usd() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return spent_usd_;
}

double BudgetStore::limit_usd() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return limit_usd_;
}

bool BudgetStore::try_consume(double cost_usd, int tokens) {
    std::lock_guard<std::mutex> lock(mutex_);

    double new_total = spent_usd_ + cost_usd;
    if (new_total > limit_usd_) {
        return false;  // 超预算
    }

    spent_usd_ = new_total;
    total_tokens_ += tokens;

    // 检查告警
    double ratio = spent_usd_ / limit_usd_;
    for (const auto& alert : alerts_) {
        if (ratio >= alert.threshold) {
            nlohmann::json evt;
            evt["subscriber_id"] = alert.subscriber_id;
            evt["threshold"] = alert.threshold;
            evt["severity"] = alert.severity;
            evt["spent_usd"] = spent_usd_;
            evt["limit_usd"] = limit_usd_;
            evt["ratio"] = ratio;
            triggered_.push_back(evt);
        }
    }
    return true;
}

void BudgetStore::record_session_cost(
    const std::string& session_id,
    double cost_usd, int tokens, int tool_calls
) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto& s = session_costs_[session_id];
    s.session_id = session_id;
    s.spent_usd += cost_usd;
    s.total_tokens += tokens;
    s.tool_calls += tool_calls;
    s.last_updated_ms = now_ms();
}

void BudgetStore::set_session_limit(
    const std::string& session_id,
    double limit_usd
) {
    std::lock_guard<std::mutex> lock(mutex_);
    session_limits_[session_id] = limit_usd;
}

nlohmann::json BudgetStore::cost_breakdown() const {
    std::lock_guard<std::mutex> lock(mutex_);
    nlohmann::json result;
    result["global"] = {
        {"limit_usd", limit_usd_},
        {"spent_usd", spent_usd_},
        {"remaining_usd", std::max(0.0, limit_usd_ - spent_usd_)},
        {"total_tokens", total_tokens_},
        {"session_count", session_costs_.size()}
    };

    nlohmann::json sessions = nlohmann::json::array();
    for (const auto& [sid, sc] : session_costs_) {
        nlohmann::json s;
        s["session_id"] = sid;
        s["spent_usd"] = sc.spent_usd;
        s["total_tokens"] = sc.total_tokens;
        s["tool_calls"] = sc.tool_calls;
        s["last_updated_ms"] = sc.last_updated_ms;
        auto it = session_limits_.find(sid);
        if (it != session_limits_.end()) {
            s["session_limit_usd"] = it->second;
        }
        sessions.push_back(s);
    }
    result["sessions"] = sessions;
    return result;
}

void BudgetStore::register_alert(
    const std::string& subscriber_id,
    double threshold,
    const std::string& severity
) {
    std::lock_guard<std::mutex> lock(mutex_);
    alerts_.push_back({threshold, severity, subscriber_id});
}

std::vector<nlohmann::json> BudgetStore::triggered_alerts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return triggered_;
}

}  // namespace pdk_budget_agent