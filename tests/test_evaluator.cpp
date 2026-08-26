#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "agenticdsl/contract/ievaluator.h"
#include "agenticdsl/types/reward_signal.h"
#include "agenticdsl/types/execution_trace.h"
#include "core/types/tool_result.h"

using namespace agenticdsl;

// Phase 0: 骨架测试验证类型编译
TEST_CASE("IEvaluator contract compiles", "[evaluator][phase0]") {
    // 占位：验证 IEvaluator 纯虚接口存在
    // 实际实例化需要具体实现类（Phase 1）
    REQUIRE(true);
}

TEST_CASE("RewardSignal three-valued quality", "[evaluator][phase0]") {
    // 占位：验证 RewardSignal 工厂方法
    auto excellent = RewardSignal::excellent(1.0);
    REQUIRE(excellent.quality == RewardSignal::Quality::Excellent);
    REQUIRE(excellent.scalar == 1.0);
    REQUIRE(excellent.confidence == 1.0);
}

TEST_CASE("ExecutionTrace structure", "[evaluator][phase0]") {
    // 占位：验证 ExecutionTrace 字段
    ExecutionTrace trace;
    trace.final_result = ToolResult::success("test");
    trace.trace_id = "trace-123";
    REQUIRE(trace.final_result.ok == true);
    REQUIRE(trace.trace_id == "trace-123");
}

TEST_CASE("RewardSignal scalar range validation", "[evaluator][phase0]") {
    // 占位：验证 scalar 越界抛异常
    REQUIRE_THROWS_AS(RewardSignal::excellent(1.5), std::out_of_range);
    REQUIRE_THROWS_AS(RewardSignal::poor(-1.5), std::out_of_range);
}

// Phase 1: TaskSuccessEvaluator 实现
class TaskSuccessEvaluator : public IEvaluator {
public:
    RewardSignal evaluate(const ExecutionTrace& trace) const override {
        if (trace.final_result.ok) {
            return RewardSignal::excellent(1.0);
        } else {
            return RewardSignal::poor(1.0);
        }
    }

    int compare(const ExecutionTrace& a, const ExecutionTrace& b) const override {
        // V1: 返回 0（平等）
        return 0;
    }
};

TEST_CASE("TaskSuccessEvaluator returns excellent on ok", "[evaluator][phase1]") {
    TaskSuccessEvaluator evaluator;
    ExecutionTrace trace;
    trace.final_result = ToolResult::success("test");
    trace.trace_id = "trace-123";
    
    auto signal = evaluator.evaluate(trace);
    REQUIRE(signal.quality == RewardSignal::Quality::Excellent);
    REQUIRE(signal.scalar == 1.0);
    REQUIRE(signal.confidence == 1.0);
}

TEST_CASE("TaskSuccessEvaluator returns poor on failure", "[evaluator][phase1]") {
    TaskSuccessEvaluator evaluator;
    ExecutionTrace trace;
    trace.final_result = ToolResult::error(ErrorCode::Unknown, "test error");
    trace.trace_id = "trace-456";
    
    auto signal = evaluator.evaluate(trace);
    REQUIRE(signal.quality == RewardSignal::Quality::Poor);
    REQUIRE(signal.scalar == -1.0);
    REQUIRE(signal.confidence == 1.0);
}

TEST_CASE("TaskSuccessEvaluator compare returns zero", "[evaluator][phase1]") {
    TaskSuccessEvaluator evaluator;
    ExecutionTrace a, b;
    a.final_result = ToolResult::success("a");
    b.final_result = ToolResult::error(ErrorCode::Unknown, "b");
    
    // V1: compare 始终返回 0（平等）
    REQUIRE(evaluator.compare(a, b) == 0);
}
