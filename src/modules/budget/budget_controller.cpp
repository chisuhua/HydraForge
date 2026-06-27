// modules/budget/src/budget_controller.cpp
// 文件头注释
// 功能描述：BudgetController 与 CostTracker 的实现 —— 提供 LLM 调用的成本累计
// 设计依据：tech-debt-and-doc-cleanup 阶段 4 任务 4.1 (REQ-cost-tracker-integration)
// 作者：tech-debt-and-doc-cleanup change
// 最后修改日期：2026-06-27 (C1 Day 6.2: override 标记 + set_termination_target 实现)
#include "budget/budget_controller.h"
#include "common/log/log.h"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <unordered_map>

namespace agenticdsl {

static double cost_per_token_for(const std::string& model) {
    auto contains = [](const std::string& s, const std::string& sub) {
        return s.find(sub) != std::string::npos;
    };
    if (contains(model, "llama") || contains(model, "gguf") || contains(model, "local")) {
        return 0.0;
    }
    if (contains(model, "gpt-4o-mini")) {
        return 0.00000015;
    }
    if (contains(model, "gpt-3.5") || contains(model, "gpt-3.5-turbo")) {
        return 0.000002;
    }
    if (contains(model, "gpt-4")) {
        return 0.00003;
    }
    return 0.000001;
}

BudgetController::BudgetController(std::optional<ExecutionBudget> initial_budget)
    : budget_opt_(std::move(initial_budget)) {
    if (budget_opt_.has_value()) {
        budget_opt_->start_time = std::chrono::steady_clock::now();
    }
}

bool BudgetController::try_consume_node() {
    if (!budget_opt_.has_value()) {
        return true;
    }
    return budget_opt_->try_consume_node();
}

bool BudgetController::try_consume_llm_call() {
    if (!budget_opt_.has_value()) {
        return true;
    }
    return budget_opt_->try_consume_llm_call();
}

bool BudgetController::try_consume_subgraph_depth() {
    if (!budget_opt_.has_value()) {
        return true;
    }
    return budget_opt_->try_consume_subgraph_depth();
}

bool BudgetController::exceeded() const {
    if (!budget_opt_.has_value()) {
        return false;
    }
    return budget_opt_->exceeded();
}

void BudgetController::set_termination_target(const NodePath& target) {
    termination_target_ = target;
}

std::optional<NodePath> BudgetController::get_termination_target() const {
    if (exceeded()) {
        return termination_target_;
    }
    return std::nullopt;
}

const std::optional<ExecutionBudget>& BudgetController::get_budget() const {
    return budget_opt_;
}

void BudgetController::set_budget(std::optional<ExecutionBudget> budget) {
    budget_opt_ = std::move(budget);
    if (budget_opt_.has_value()) {
        budget_opt_->start_time = std::chrono::steady_clock::now();
    }
}

void BudgetController::record_llm_call(int tokens, const std::string& model) {
    if (tokens < 0) {
        LOG_WARN("record_llm_call 收到负数 tokens=" << tokens << "，已忽略");
        return;
    }
    const double unit_price = cost_per_token_for(model);
    const double call_cost = unit_price * static_cast<double>(tokens);

    cost_tracker_.tokens_consumed.fetch_add(tokens, std::memory_order_relaxed);

    {
        std::lock_guard<std::mutex> lock(cost_mutex_);
        cost_tracker_.total_cost_usd += call_cost;
        cost_tracker_.last_call_cost_usd = call_cost;
    }
    LOG_DEBUG("record_llm_call: tokens=" << tokens
               << " model=" << model
               << " unit=" << unit_price
               << " call_cost=" << call_cost);
}

double BudgetController::get_total_cost_usd() const {
    std::lock_guard<std::mutex> lock(cost_mutex_);
    return cost_tracker_.total_cost_usd;
}

void BudgetController::reset() {
    cost_tracker_.tokens_consumed.store(0, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(cost_mutex_);
    cost_tracker_.total_cost_usd = 0.0;
    cost_tracker_.last_call_cost_usd = 0.0;
}

} // namespace agenticdsl
