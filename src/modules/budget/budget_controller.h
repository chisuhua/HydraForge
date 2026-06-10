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

// BudgetController 类封装了预算的管理和检查逻辑
class BudgetController {
public:
    // CostTracker 嵌套结构：累计 LLM 调用的成本与 token 消耗
    // 设计原则：
    //   1. 字段简单：仅跟踪总额 / token / 上次调用成本
    //   2. 成本计算为 token 数 × 模型单 token 价格（USD）
    //   3. 默认单价表（USD per token）：
    //        - llama 本地模型：0.0（无网络成本）
    //        - gpt-4o-mini    ：0.00000015
    //        - gpt-3.5-turbo  ：0.000002
    //        - 未知模型        ：0.000001（保守估值）
    //   4. 原子计数器（token/cost）保证多线程下 cost 累加不丢数据
    struct CostTracker {
        // 累计成本（USD）
        double total_cost_usd = 0.0;
        // 累计 token 数
        std::atomic<int> tokens_consumed{0};
        // 最近一次调用的成本（USD），便于调试
        double last_call_cost_usd = 0.0;
    };

    // 构造函数，接受一个可选的初始预算配置
    explicit BudgetController(std::optional<ExecutionBudget> initial_budget = std::nullopt);

    // 尝试消耗一个节点预算
    // 返回 true 表示消耗成功，false 表示超出预算或预算为无限
    bool try_consume_node();

    // 尝试消耗一个 LLM 调用预算
    // 返回 true 表示消耗成功，false 表示超出预算或预算为无限
    bool try_consume_llm_call();

    bool try_consume_subgraph_depth();

    // 检查当前预算是否已超限
    bool exceeded() const;

    // 获取预算超限时的跳转目标节点路径
    // 返回 std::nullopt 如果没有超限或没有配置跳转路径
    void set_termination_target(const NodePath& target) { termination_target_ = target; }
    std::optional<NodePath> get_termination_target() const;



    // 获取当前预算配置的副本
    const std::optional<ExecutionBudget>& get_budget() const;

    // 设置预算配置
    void set_budget(std::optional<ExecutionBudget> budget);

    // === 成本跟踪 API (阶段 4 任务 4.1) ===
    // 记录一次 LLM 调用：累加 token 计数与按模型计价的成本
    // 参数：
    //   tokens - 本次调用消耗的 token 数
    //   model  - 模型名（用于查询单价表）
    void record_llm_call(int tokens, const std::string& model);

    // 获取累计成本（USD）
    double get_total_cost_usd() const;

    // 获取 CostTracker 的常量引用（用于外部读取多个字段）
    const CostTracker& cost_tracker() const { return cost_tracker_; }

    // 重置所有成本累计字段（测试用，亦可用于新会话）
    void reset();

private:
    std::optional<ExecutionBudget> budget_opt_;
    NodePath termination_target_ = "/__system__/budget_exceeded"; // 默认超限跳转路径
    CostTracker cost_tracker_; // 嵌套的 cost tracker 实例
    // 保护 total_cost_usd / last_call_cost_usd 的写入（token 用 atomic 已自保护）
    mutable std::mutex cost_mutex_;
};

} // namespace agenticdsl

#endif // AGENTICDSL_MODULES_BUDGET_BUDGET_CONTROLLER_H
