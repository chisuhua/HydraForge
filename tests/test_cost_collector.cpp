// tests/test_cost_collector.cpp
// 文件头注释
// 功能描述：CostTracker 单元测试 —— 覆盖 LLM 成本累加、多次累加、reset 清零
// 设计依据：tech-debt-and-doc-cleanup 阶段 4 任务 4.4 (REQ-cost-tracker-integration)
// 作者：tech-debt-and-doc-cleanup change
// 最后修改日期：2026-06-10
#include "catch_amalgamated.hpp"
#include "budget/budget_controller.h"
#include "common/tools/registry.h"
#include "common/llm/llm_tool.h"

#include <cmath>
#include <memory>
#include <string>

using namespace agenticdsl;

// 浮点比较辅助：因 double 累加可能有微小误差，使用 epsilon 比较
static bool approx_eq(double a, double b, double eps = 1e-12) {
    return std::fabs(a - b) <= eps;
}

// 用于测试的 Mock LLM 工具：返回固定的 token 数
class MockCostLLMTool : public ILLMTool {
public:
    explicit MockCostLLMTool(int tokens_per_call) : tokens_(tokens_per_call) {}

    LLMResult generate(const std::string& prompt, const LLMParams& params = {}) override {
        LLMResult result;
        result.success = true;
        result.text = "ok";
        result.tokens_generated = tokens_;
        return result;
    }

    bool is_available() const override { return true; }
    std::string name() const override { return "mock_cost_llm"; }

private:
    int tokens_;
};

// 1) 单次调用累积 cost —— 验证 record_llm_call 正确按模型计费
TEST_CASE("CostTracker single call accumulates cost", "[cost_collector][stage4]") {
    BudgetController bc;

    // gpt-3.5-turbo 单价 0.000002 USD/token
    bc.record_llm_call(/*tokens=*/500, /*model=*/"gpt-3.5-turbo");

    // 期望成本 = 500 * 0.000002 = 0.001 USD
    REQUIRE(approx_eq(bc.get_total_cost_usd(), 0.001));
    REQUIRE(bc.cost_tracker().tokens_consumed.load() == 500);
    REQUIRE(approx_eq(bc.cost_tracker().last_call_cost_usd, 0.001));
    REQUIRE(approx_eq(bc.get_total_cost_usd(), bc.cost_tracker().total_cost_usd));
}

// 2) 多次调用累加 —— 验证累加语义与多模型混合计价
TEST_CASE("CostTracker multiple calls add up", "[cost_collector][stage4]") {
    BudgetController bc;

    // 第一次：1000 tokens，gpt-3.5-turbo => 0.002 USD
    bc.record_llm_call(1000, "gpt-3.5-turbo");
    // 第二次：2000 tokens，gpt-4o-mini (0.00000015) => 0.0003 USD
    bc.record_llm_call(2000, "gpt-4o-mini");
    // 第三次：500 tokens，llama local (0.0) => 0 USD
    bc.record_llm_call(500, "llama-2-7b-local");

    // 累计：0.002 + 0.0003 + 0.0 = 0.0023 USD
    const double expected = 0.002 + 0.0003 + 0.0;
    REQUIRE(approx_eq(bc.get_total_cost_usd(), expected));
    REQUIRE(bc.cost_tracker().tokens_consumed.load() == 1000 + 2000 + 500);
    // last_call_cost_usd 应为最近一次（500 tokens llama = 0）
    REQUIRE(approx_eq(bc.cost_tracker().last_call_cost_usd, 0.0));
}

// 3) reset() 后清零 —— 验证可重入用于新会话或测试隔离
TEST_CASE("CostTracker reset zeroes all fields", "[cost_collector][stage4]") {
    BudgetController bc;
    bc.record_llm_call(1234, "gpt-3.5-turbo");
    bc.record_llm_call(5678, "gpt-4o-mini");
    REQUIRE(bc.get_total_cost_usd() > 0.0);
    REQUIRE(bc.cost_tracker().tokens_consumed.load() > 0);

    bc.reset();

    REQUIRE(approx_eq(bc.get_total_cost_usd(), 0.0));
    REQUIRE(bc.cost_tracker().tokens_consumed.load() == 0);
    REQUIRE(approx_eq(bc.cost_tracker().last_call_cost_usd, 0.0));
    REQUIRE(approx_eq(bc.cost_tracker().total_cost_usd, 0.0));

    // reset 后应能继续累加（无状态污染）
    bc.record_llm_call(100, "gpt-3.5-turbo");
    REQUIRE(approx_eq(bc.get_total_cost_usd(), 100 * 0.000002));
}
