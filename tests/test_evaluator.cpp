#define CATCH_CONFIG_MAIN
#include "catch_amalgamated.hpp"
#include "agenticdsl/contract/ievaluator.h"
#include "agenticdsl/types/reward_signal.h"
#include "agenticdsl/types/execution_trace.h"
#include "agenticdsl/cognitive/behavioral_equivalence_evaluator.h"
#include "agenticdsl/cognitive/composite_evaluator.h"
#include "core/types/tool_result.h"

// Phase 2/3: Worker setter 注入 + 事件发射
#include "agenticdsl/cognitive/cognitive_worker.h"
#include "agenticdsl/cognitive/domain_worker_pool.h"
#include "agenticdsl/contract/inmemory_bus.h"
#include "agenticdsl/contract/i_llm_provider_decorator.h"
#include "core/engine.h"
#include "common/llm/llm_types.h"
#include "common/llm/mock_provider.h"

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

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

// =====================================================================
// Phase 2: Worker setter 注入 (T2.1-T2.3)
// =====================================================================
namespace {

// 计数评估器 — 记录 evaluate() 调用次数, 验证 setter 注入后被调用
class CountingEvaluator : public IEvaluator {
public:
    mutable std::atomic<int> evaluate_count{0};

    RewardSignal evaluate(const ExecutionTrace& trace) const override {
        evaluate_count.fetch_add(1, std::memory_order_relaxed);
        if (trace.final_result.ok) {
            return RewardSignal::excellent(1.0);
        }
        return RewardSignal::poor(1.0);
    }

    int compare(const ExecutionTrace&, const ExecutionTrace&) const override {
        return 0;
    }
};

const std::string kEmptyDsl = R"(
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
    next: ["/main/end"]
  - id: end
    type: end
# --- END AgenticDSL ---
```
)";

std::unique_ptr<DSLEngine> make_engine_with_mock(const std::string& response) {
    auto engine = DSLEngine::from_markdown(kEmptyDsl);
    ILLMProvider* p = engine->get_llm_provider();
    auto* mock = dynamic_cast<MockLLMProvider*>(p);
    if (!mock) {
        if (auto* d = dynamic_cast<ILLMProviderDecorator*>(p)) {
            mock = dynamic_cast<MockLLMProvider*>(d->inner());
        }
    }
    if (mock) {
        mock->set_fixed_response(response);
    }
    return engine;
}

void register_echo_tool(DSLEngine& engine) {
    engine.register_tool(
        "echo",
        ToolMetadata{"echo", "test", "test", ToolCategory::ReadOnly,
                     LayerProfile::Workflow},
        [](const std::unordered_map<std::string, std::string>& args)
            -> nlohmann::json {
            return nlohmann::json{{"echoed", args.at("message")}};
        });
}

DomainTask make_test_domain_task() {
    DomainTask task;
    task.domain = "test";
    task.tool_name = "test::noop";
    task.output_key = "out";
    return task;
}

template <typename Pred>
void wait_until(Pred&& pred, std::chrono::milliseconds timeout =
                                 std::chrono::milliseconds(5000)) {
    const auto start = std::chrono::steady_clock::now();
    while (!pred()) {
        if (std::chrono::steady_clock::now() - start > timeout) {
            FAIL("wait_until: timeout");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

} // namespace

TEST_CASE("CognitiveWorker set_evaluator invokes evaluate on task completion",
          "[evaluator][phase2]") {
    auto bus = std::make_shared<InMemoryBus>();
    auto engine = make_engine_with_mock(R"({"tool":"echo","args":{"message":"hi"}})");
    register_echo_tool(*engine);

    auto evaluator = std::make_shared<CountingEvaluator>();

    CognitiveWorker worker(std::move(engine), bus);
    worker.set_evaluator(evaluator);
    worker.start();
    worker.submit_task("eval-cog-1", "hello");

    wait_until([&] {
        return evaluator->evaluate_count.load(std::memory_order_relaxed) == 1;
    });
    worker.stop();
}

TEST_CASE("DomainWorkerPool set_evaluator invokes evaluate on task completion",
          "[evaluator][phase2]") {
    auto bus = std::make_shared<InMemoryBus>();
    DomainWorkerPool pool(1, bus);
    pool.register_domain_handler(
        "test", [](const DomainTask&) -> nlohmann::json {
            return nlohmann::json{{"ok", true}};
        });

    auto evaluator = std::make_shared<CountingEvaluator>();
    pool.set_evaluator(evaluator);

    pool.start();
    pool.submit_task(make_test_domain_task());

    wait_until([&] {
        return evaluator->evaluate_count.load(std::memory_order_relaxed) == 1;
    });
    pool.stop();
}

TEST_CASE("null evaluator does not crash and emits no evaluation event",
          "[evaluator][phase2]") {
    auto bus = std::make_shared<InMemoryBus>();
    std::atomic<int> eval_event_count{0};
    std::atomic<int> cog_completed{0};
    std::atomic<int> dom_completed{0};
    bus->subscribe("evaluation.result",
                   [&](const BusEvent&) { ++eval_event_count; });
    bus->subscribe("cognitive.task.completed",
                   [&](const BusEvent&) { ++cog_completed; });
    bus->subscribe("domain.task.completed",
                   [&](const BusEvent&) { ++dom_completed; });

    // CognitiveWorker 无 evaluator (默认 nullptr)
    auto engine = make_engine_with_mock(R"({"tool":"echo","args":{"message":"x"}})");
    register_echo_tool(*engine);
    CognitiveWorker worker(std::move(engine), bus);
    worker.start();
    worker.submit_task("null-cog-1", "hi");

    // DomainWorkerPool 无 evaluator (默认 nullptr)
    DomainWorkerPool pool(1, bus);
    pool.register_domain_handler(
        "test", [](const DomainTask&) -> nlohmann::json {
            return nlohmann::json{{"ok", true}};
        });
    pool.start();
    pool.submit_task(make_test_domain_task());

    wait_until([&] { return cog_completed.load() == 1; });
    wait_until([&] { return dom_completed.load() == 1; });
    worker.stop();
    pool.stop();

    // 无 evaluator -> 不发射 evaluation.result
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    REQUIRE(eval_event_count.load() == 0);
}

// =====================================================================
// Phase 3: evaluation.result 事件发射 (T3.1-T3.2)
// =====================================================================
TEST_CASE("evaluation.result event emitted after task completion",
          "[evaluator][phase3]") {
    auto bus = std::make_shared<InMemoryBus>();
    std::atomic<int> eval_event_count{0};
    bus->subscribe("evaluation.result",
                   [&](const BusEvent&) { ++eval_event_count; });

    auto evaluator = std::make_shared<CountingEvaluator>();

    // CognitiveWorker 路径
    auto engine = make_engine_with_mock(R"({"tool":"echo","args":{"message":"hi"}})");
    register_echo_tool(*engine);
    CognitiveWorker worker(std::move(engine), bus);
    worker.set_evaluator(evaluator);
    worker.start();
    worker.submit_task("eval-emit-cog", "hello");

    // DomainWorkerPool 路径
    DomainWorkerPool pool(1, bus);
    pool.register_domain_handler(
        "test", [](const DomainTask&) -> nlohmann::json {
            return nlohmann::json{{"ok", true}};
        });
    pool.set_evaluator(evaluator);
    pool.start();
    pool.submit_task(make_test_domain_task());

    wait_until([&] { return eval_event_count.load() == 2; });
    worker.stop();
    pool.stop();
}

TEST_CASE("evaluation.result event contains required fields",
          "[evaluator][phase3]") {
    auto bus = std::make_shared<InMemoryBus>();
    std::atomic<bool> captured{false};
    ToolResult eval_payload;
    std::mutex payload_mutex;
    bus->subscribe("evaluation.result", [&](const BusEvent& e) {
        std::lock_guard<std::mutex> lock(payload_mutex);
        eval_payload = e.payload;
        captured = true;
    });

    auto evaluator = std::make_shared<CountingEvaluator>();

    auto engine = make_engine_with_mock(R"({"tool":"echo","args":{"message":"hi"}})");
    register_echo_tool(*engine);
    CognitiveWorker worker(std::move(engine), bus);
    worker.set_evaluator(evaluator);
    worker.start();
    worker.submit_task("eval-fields-1", "hello");

    wait_until([&] { return captured.load(); });
    worker.stop();

    std::lock_guard<std::mutex> lock(payload_mutex);
    const auto& data = eval_payload.data;
    // 必填字段 (spec: evaluation.result schema)
    REQUIRE(data.contains("evaluation_id"));
    REQUIRE(data["evaluation_id"].is_string());
    REQUIRE_FALSE(data["evaluation_id"].get<std::string>().empty());
    REQUIRE(data.contains("schema_version"));
    REQUIRE(data["schema_version"] == "v1");
    REQUIRE(data.contains("trace_ref"));
    REQUIRE(data["trace_ref"] == "eval-fields-1");
    REQUIRE(data.contains("quality"));
    REQUIRE(data["quality"] == "Excellent");
    REQUIRE(data.contains("scalar"));
    REQUIRE(data["scalar"] == 1.0);
    REQUIRE(data.contains("confidence"));
    REQUIRE(data["confidence"] == 1.0);
    REQUIRE(data.contains("evaluation_refs"));
    REQUIRE(data["evaluation_refs"].is_array());
}

TEST_CASE("composite_aggregate_two_evaluators", "[evaluator][v2][phase4]") {
    auto excellent = std::make_shared<TaskSuccessEvaluator>();
    auto acceptable = std::make_shared<BehavioralEquivalenceEvaluator>();
    CompositeEvaluator composite({excellent, acceptable}, {3.0, 1.0});
    ExecutionTrace trace;
    trace.final_result = ToolResult::success("ok");

    auto signal = composite.evaluate(trace);
    REQUIRE(signal.scalar == Catch::Approx(0.75));
    REQUIRE(signal.quality == RewardSignal::Quality::Excellent);
    // confidence 取 min: TaskSuccessEvaluator=1.0, BehavioralEquivalence=0.5
    REQUIRE(signal.confidence == Catch::Approx(0.5));
}

TEST_CASE("composite_aggregate_empty_evaluators_throws", "[evaluator][v2][phase4]") {
    REQUIRE_THROWS_AS(CompositeEvaluator({}, {}), std::invalid_argument);
}

TEST_CASE("composite_weights_mismatch_throws", "[evaluator][v2][phase4]") {
    auto evaluator = std::make_shared<TaskSuccessEvaluator>();
    REQUIRE_THROWS_AS(CompositeEvaluator({evaluator}, {}), std::invalid_argument);
}

// =====================================================================
// Phase 4: V2 评估器 — BehavioralEquivalenceEvaluator (evaluator-v2-composite, T0)
// =====================================================================
TEST_CASE("behavioral_equivalence_compare_pass_pair", "[evaluator][v2][phase4]") {
    BehavioralEquivalenceEvaluator evaluator;
    ExecutionTrace a, b;
    // 相似 fingerprint: 两个成功 trace, 无 latency/tokens 差异
    a.final_result = ToolResult::success("a");
    b.final_result = ToolResult::success("b");
    // Hotelling Pass → compare 返回 0
    REQUIRE(evaluator.compare(a, b) == 0);
}

TEST_CASE("behavioral_equivalence_compare_fail_pair", "[evaluator][v2][phase4]") {
    BehavioralEquivalenceEvaluator evaluator;
    ExecutionTrace good, bad;
    good.final_result = ToolResult::success("good");
    bad.final_result = ToolResult::error(ErrorCode::Unknown, "bad");
    // 差异 fingerprint (ok vs error → success_rate/error_rate 特征差) + Hotelling Fail
    // → compare 返回 +1 (good 优于 bad)
    REQUIRE(evaluator.compare(good, bad) == 1);
    REQUIRE(evaluator.compare(bad, good) == -1);
}

TEST_CASE("behavioral_equivalence_evaluate_single_returns_acceptable",
          "[evaluator][v2][phase4]") {
    BehavioralEquivalenceEvaluator evaluator;
    ExecutionTrace trace;
    trace.final_result = ToolResult::success("t");
    // V1: 单 trace 无法评估等价性 → 占位 Acceptable(0.5)
    auto signal = evaluator.evaluate(trace);
    REQUIRE(signal.quality == RewardSignal::Quality::Acceptable);
    REQUIRE(signal.scalar == 0.0);
    REQUIRE(signal.confidence == 0.5);
}
