// tests/test_stream_to_bus.cpp
// C2 Day 1-2 (2026-06-27, Sprint 12 P1, Oracle Q2 决议)
#include "catch_amalgamated.hpp"

#include "common/llm/stream_to_bus.h"
#include "common/llm/mock_provider.h"
#include "common/llm/llm_types.h"
#include "agenticdsl/contract/inmemory_bus.h"

#include <stop_token>
#include <string>
#include <vector>

using namespace agenticdsl;
using namespace agenticdsl::llm;

namespace {

class EventCollector {
public:
    explicit EventCollector(InMemoryBus& bus) {
        token_ = bus.subscribe(event_type::kLlmToken,
            [this](const ToolResult& p) { tokens_.push_back(p); });
        done_token_ = bus.subscribe(event_type::kLlmTokenDone,
            [this](const ToolResult& p) { done_.push_back(p); });
        err_token_ = bus.subscribe(event_type::kLlmTokenError,
            [this](const ToolResult& p) { errors_.push_back(p); });
    }
    const std::vector<ToolResult>& tokens() const { return tokens_; }
    const std::vector<ToolResult>& done() const { return done_; }
    const std::vector<ToolResult>& errors() const { return errors_; }
private:
    size_t token_ = 0, done_token_ = 0, err_token_ = 0;
    std::vector<ToolResult> tokens_;
    std::vector<ToolResult> done_;
    std::vector<ToolResult> errors_;
};

std::unique_ptr<IGenerationStream> make_stream(MockLLMProvider& provider,
                                                std::vector<std::string> tokens) {
    provider.set_stream_tokens(std::move(tokens));
    return provider.generate_stream(GenerationRequest("test"), std::stop_token{});
}

}  // namespace

TEST_CASE("run_stream_to_bus emits llm.token per chunk in order",
          "[llm][stream_to_bus][c2-day1]") {
    MockLLMProvider provider;
    auto stream = make_stream(provider, {"Hello", " ", "world", "!"});

    InMemoryBus bus;
    EventCollector collector(bus);

    auto result = run_stream_to_bus(*stream, bus, std::stop_token{}, "req-1");
    bus.wait_for_drain();
    REQUIRE(result.text == "Hello world!");
    REQUIRE(result.completion_tokens == 4);
    REQUIRE(result.finish_reason == "stop");

    REQUIRE(collector.tokens().size() == 4);
    REQUIRE(collector.tokens()[0].data["token"] == "Hello");
    REQUIRE(collector.tokens()[1].data["token"] == " ");
    REQUIRE(collector.tokens()[2].data["token"] == "world");
    REQUIRE(collector.tokens()[3].data["token"] == "!");
    for (const auto& e : collector.tokens()) {
        REQUIRE(e.data["request_id"] == "req-1");
    }
}

TEST_CASE("run_stream_to_bus emits llm.token.done on completion",
          "[llm][stream_to_bus][c2-day1]") {
    MockLLMProvider provider;
    auto stream = make_stream(provider, {"a", "b", "c"});

    InMemoryBus bus;
    EventCollector collector(bus);
    auto result = run_stream_to_bus(*stream, bus, std::stop_token{}, "req-2");
    bus.wait_for_drain();
    REQUIRE(result.finish_reason == "stop");
    REQUIRE(result.completion_tokens == 3);

    REQUIRE(collector.done().size() == 1);
    auto& payload = collector.done()[0].data;
    REQUIRE(payload["request_id"] == "req-2");
    REQUIRE(payload["finish_reason"] == "stop");
    REQUIRE(payload["token_count"] == 3);
    REQUIRE(payload["text"] == "abc");
}

TEST_CASE("run_stream_to_bus respects stop_token cancellation",
          "[llm][stream_to_bus][c2-day1]") {
    MockLLMProvider provider;
    auto stream = make_stream(provider, {"t1", "t2", "t3", "t4", "t5"});

    InMemoryBus bus;
    EventCollector collector(bus);
    std::stop_source stop_source;
    stop_source.request_stop();
    auto result = run_stream_to_bus(*stream, bus, stop_source.get_token(), "req-3");
    bus.wait_for_drain();
    REQUIRE(result.finish_reason == "cancelled");

    REQUIRE(collector.done().size() == 1);
    REQUIRE(collector.done()[0].data["finish_reason"] == "cancelled");
}

TEST_CASE("run_stream_to_bus aggregates final GenerationResult",
          "[llm][stream_to_bus][c2-day1]") {
    MockLLMProvider provider;
    auto stream = make_stream(provider, {"p1", "p2", "p3", "p4"});

    InMemoryBus bus;
    EventCollector collector(bus);
    auto result = run_stream_to_bus(*stream, bus, std::stop_token{}, "req-4");
    bus.wait_for_drain();
    REQUIRE(result.text == "p1p2p3p4");
    REQUIRE(result.completion_tokens == 4);
    REQUIRE(result.finish_reason == "stop");
    REQUIRE(result.prompt_tokens == 0);
}

TEST_CASE("run_stream_to_bus preserves token order across event types",
          "[llm][stream_to_bus][c2-day1]") {
    MockLLMProvider provider;
    auto stream = make_stream(provider, {"alpha", "beta", "gamma"});

    InMemoryBus bus;
    std::vector<std::string> event_order;
    bus.subscribe(event_type::kLlmToken, [&event_order](const ToolResult& p) {
        event_order.push_back("token:" + p.data["token"].get<std::string>());
    });
    bus.subscribe(event_type::kLlmTokenDone, [&event_order](const ToolResult&) {
        event_order.push_back("done");
    });

    run_stream_to_bus(*stream, bus, std::stop_token{}, "req-5");
    bus.wait_for_drain();
    REQUIRE(event_order.size() == 4);
    REQUIRE(event_order[0] == "token:alpha");
    REQUIRE(event_order[1] == "token:beta");
    REQUIRE(event_order[2] == "token:gamma");
    REQUIRE(event_order[3] == "done");
}