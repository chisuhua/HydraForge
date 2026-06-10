// modules/budget/src/budget_controller.cpp
// 文件头注释
// 功能描述：BudgetController 与 CostTracker 的实现 —— 提供 LLM 调用的成本累计
// 设计依据：tech-debt-and-doc-cleanup 阶段 4 任务 4.1 (REQ-cost-tracker-integration)
// 作者：tech-debt-and-doc-cleanup change
// 最后修改日期：2026-06-10
#include "budget/budget_controller.h"
#include "common/log/log.h" // 日志门面
#include <algorithm>
#include <chrono>
#include <iostream> // For debugging if needed
#include <unordered_map>

namespace agenticdsl {

// === 内部：按模型查询单 token 价格（USD） ===
// 默认单价表（USD per token）：
//   - llama 本地模型：0.0（无网络成本）
//   - gpt-4o-mini    ：0.00000015
//   - gpt-3.5-turbo  ：0.000002
//   - 未知模型        ：0.000001（保守估值）
// 注：此处使用简单字符串匹配，复杂场景可替换为外部配置。
static double cost_per_token_for(const std::string& model) {
    // 本地模型关键词：llama / gguf / local —— 视作 0 成本
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
        return 0.00003; // gpt-4 默认估值
    }
    // 未知模型走保守估值
    return 0.000001;
}

BudgetController::BudgetController(std::optional<ExecutionBudget> initial_budget)
    : budget_opt_(std::move(initial_budget)) {
    // 如果提供了初始预算，初始化其 start_time
    if (budget_opt_.has_value()) {
        budget_opt_->start_time = std::chrono::steady_clock::now();
    }
}

bool BudgetController::try_consume_node() {
    if (!budget_opt_.has_value()) {
        // 没有预算限制，总是成功
        return true;
    }

    return budget_opt_->try_consume_node(); // ExecutionBudget 内部处理原子性
}

bool BudgetController::try_consume_llm_call() {
    if (!budget_opt_.has_value()) {
        // 没有预算限制，总是成功
        return true;
    }

    return budget_opt_->try_consume_llm_call(); // ExecutionBudget 内部处理原子性
}

// modules/budget/src/budget_controller.cpp
bool BudgetController::try_consume_subgraph_depth() {
    if (!budget_opt_.has_value()) {
        return true; // 无限制
    }
    return budget_opt_->try_consume_subgraph_depth(); // ExecutionBudget 需实现此方法
}

bool BudgetController::exceeded() const {
    if (!budget_opt_.has_value()) {
        // 没有预算限制，永不超限
        return false;
    }

    return budget_opt_->exceeded(); // ExecutionBudget 内部检查原子计数器和时间
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
        // 重置开始时间
        budget_opt_->start_time = std::chrono::steady_clock::now();
    }
}

// === 成本跟踪实现 (阶段 4 任务 4.1) ===
// 记录一次 LLM 调用：累加 token 计数与按模型计价的成本
void BudgetController::record_llm_call(int tokens, const std::string& model) {
    // 负数 token 视为异常输入，直接忽略（不累计）
    if (tokens < 0) {
        LOG_WARN("record_llm_call 收到负数 tokens=" << tokens << "，已忽略");
        return;
    }
    const double unit_price = cost_per_token_for(model);
    const double call_cost = unit_price * static_cast<double>(tokens);

    // token 累加用 atomic（自保护）
    cost_tracker_.tokens_consumed.fetch_add(tokens, std::memory_order_relaxed);

    // 浮点成本字段用 mutex 保护（防止并发更新丢失小数部分）
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

// 获取累计成本（USD）
double BudgetController::get_total_cost_usd() const {
    std::lock_guard<std::mutex> lock(cost_mutex_);
    return cost_tracker_.total_cost_usd;
}

// 重置所有成本累计字段（用于测试或新会话）
void BudgetController::reset() {
    cost_tracker_.tokens_consumed.store(0, std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(cost_mutex_);
    cost_tracker_.total_cost_usd = 0.0;
    cost_tracker_.last_call_cost_usd = 0.0;
}

} // namespace agenticdsl
