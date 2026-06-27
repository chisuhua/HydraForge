// modules/budget/include/budget/budget_controller.h
// 文件头注释
// 功能描述：预算控制器 —— 封装预算管理与检查逻辑，
//          并聚合 LLM 调用的成本跟踪（CostTracker）
// 设计依据：tech-debt-and-doc-cleanup 阶段 4 任务 4.1 (REQ-cost-tracker-integration)
// 作者：tech-debt-and-doc-cleanup change
// 最后修改日期：2026-06-10

#ifndef AGENTICDSL_MODULES_BUDGET_BUDGET_CONTROLLER_H
#define AGENTICDSL_MODULES_BUDGET_BUDGET_CONTROLLER_H

#include "core/types/budget.h" // 引入 ExecutionBudget (已包含 atomic 计数器)
#include "core/types/node.h" // 引入 NodePath
#include <atomic>
#include <optional>
#include <string>
#include <chrono> // For steady_clock used in ExecutionBudget
#include <mutex>  // Potentially needed if ExecutionBudget needs external protection beyond atomics

namespace agenticdsl {

// IBudgetController 抽象接口
class IBudgetController {
public:
    virtual ~IBudgetController() = default;

    virtual bool try_consume_node() = 0;
    virtual bool try_consume_llm_call() = 0;
    virtual bool try_consume_subgraph_depth() = 0;
    virtual bool exceeded() const = 0;

    virtual void set_termination_target(const NodePath& target) = 0;
    virtual std::optional<NodePath> get_termination_target() const = 0;

    virtual const std::optional<ExecutionBudget>& get_budget() const = 0;
    virtual void set_budget(std::optional<ExecutionBudget> budget) = 0;

    virtual void record_llm_call(int tokens, const std::string& model) = 0;
    virtual double get_total_cost_usd() const = 0;
    virtual void reset() = 0;
};

// BudgetController 类封装了预算的管理和检查逻辑
class BudgetController : public IBudgetController {
public:
    // CostTracker 嵌套结构：累计 LLM 调用的成本与 token 消耗
    struct CostTracker {
        double total_cost_usd = 0.0;
        std::atomic<int> tokens_consumed{0};
        double last_call_cost_usd = 0.0;
    };

    explicit BudgetController(std::optional<ExecutionBudget> initial_budget = std::nullopt);

    bool try_consume_node() override;
    bool try_consume_llm_call() override;
    bool try_consume_subgraph_depth() override;
    bool exceeded() const override;

    void set_termination_target(const NodePath& target) override;
    std::optional<NodePath> get_termination_target() const override;

    const std::optional<ExecutionBudget>& get_budget() const override;
    void set_budget(std::optional<ExecutionBudget> budget) override;

    void record_llm_call(int tokens, const std::string& model) override;
    double get_total_cost_usd() const override;
    void reset() override;

    const CostTracker& cost_tracker() const { return cost_tracker_; }

private:
    std::optional<ExecutionBudget> budget_opt_;
    NodePath termination_target_ = "/__system__/budget_exceeded";
    CostTracker cost_tracker_;
    mutable std::mutex cost_mutex_;
};

} // namespace agenticdsl

#endif // AGENTICDSL_MODULES_BUDGET_BUDGET_CONTROLLER_H
