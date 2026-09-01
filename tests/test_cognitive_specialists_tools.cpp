// tests/test_cognitive_specialists_tools.cpp
// T5 cognitive-specialists-as-tools: 3 specialist tool 注册测试
#include "catch_amalgamated.hpp"
#include "agenticdsl/cognitive/cognitive_tools.h"
#include "common/tools/registry.h"
#include "common/policy/execution_policy.h"
#include "agenticdsl/cognitive/gepa_loop.h"
#include "agenticdsl/cognitive/mcts_workflow_search.h"
#include "agenticdsl/cognitive/skill_compiler.h"
#include <memory>

using namespace agenticdsl;

namespace {
// 最小 IEvaluator mock (复用 test_mcts_axis6.cpp 模式)
class StubEvaluator : public IEvaluator {
public:
    RewardSignal evaluate(const ExecutionTrace&) const override { return RewardSignal::excellent(0.9); }
    int compare(const ExecutionTrace&, const ExecutionTrace&) const override { return 0; }
};
// 最小 IMutationGovernor mock
class StubGovernor : public IMutationGovernor {
public:
    MutationDecision propose(const MutationContext&) override { return {true, "", ""}; }
    MutationDecision commit(const MutationContext&) override { return {true, "", ""}; }
    void revert(const MutationContext&, const std::string&, const std::string&) override {}
};
// 最小 ILLMProvider mock (复用 test_gepa_phase2.cpp MockLLMProvider 模式)
class StubLLM : public ILLMProvider {
public:
    Result<GenerationResult, LLMError> generate(const GenerationRequest&, std::stop_token) override {
        return Result<GenerationResult, LLMError>::success(
            GenerationResult{"Reflection note", 0, 0, "stop"});
    }
    std::unique_ptr<IGenerationStream> generate_stream(const GenerationRequest&, std::stop_token) override {
        return nullptr;
    }
    std::vector<ModelInfo> available_models() const override { return {}; }
};
}  // namespace

TEST_CASE("register_cognitive_tools: null specialists is a safe no-op", "[tools][cognitive][t5]") {
    ToolRegistry registry;
    REQUIRE_NOTHROW(cognitive::register_cognitive_tools(registry, nullptr, nullptr, nullptr));
    // 无 specialist → 不注册任何 evolution::* tool (registry 自带默认工具不受影响)
    REQUIRE_FALSE(registry.has_tool("evolution::reflect"));
    REQUIRE_FALSE(registry.has_tool("evolution::search"));
    REQUIRE_FALSE(registry.has_tool("evolution::compile"));
}

TEST_CASE("register_cognitive_tools: 3 specialists register 3 evolution::* tools", "[tools][cognitive][t5]") {
    ToolRegistry registry;
    auto evaluator = std::make_shared<StubEvaluator>();
    auto governor = std::make_shared<StubGovernor>();
    auto llm = std::make_shared<StubLLM>();
    auto gepa = std::make_shared<GEPALoop>(evaluator, governor, llm);
    auto mcts = std::make_shared<MCTSWorkflowSearch>(evaluator, governor,
                                                     std::make_shared<BehavioralRegressionGate>());
    auto compiler = std::make_shared<SkillCompiler>();
    REQUIRE_NOTHROW(cognitive::register_cognitive_tools(registry, gepa, mcts, compiler));

    REQUIRE(registry.has_tool("evolution::reflect"));
    REQUIRE(registry.has_tool("evolution::search"));
    REQUIRE(registry.has_tool("evolution::compile"));

    // 只统计 evolution::* 前缀工具 (registry 自带默认工具不计入)
    auto tools = registry.list_tools();
    size_t evolution_count = 0;
    for (const auto& t : tools) {
        if (t.rfind("evolution::", 0) == 0) ++evolution_count;
    }
    REQUIRE(evolution_count == 3);
}

TEST_CASE("register_cognitive_tools: metadata V2 fields populated", "[tools][cognitive][t5]") {
    ToolRegistry registry;
    auto evaluator = std::make_shared<StubEvaluator>();
    auto governor = std::make_shared<StubGovernor>();
    auto llm = std::make_shared<StubLLM>();
    auto gepa = std::make_shared<GEPALoop>(evaluator, governor, llm);
    auto compiler = std::make_shared<SkillCompiler>();
    // 只注册 reflect + compile, 跳过 mcts
    cognitive::register_cognitive_tools(registry, gepa, nullptr, compiler);

    auto metas = registry.list_metadata();
    size_t evolution_metas = 0;
    for (const auto& [name, meta] : metas) {
        if (name.rfind("evolution::", 0) != 0) continue;
        ++evolution_metas;
        REQUIRE(meta.domain == "cognitive");
        REQUIRE(meta.allowed_layers.size() == 2);
        REQUIRE(meta.cost_estimate > 0.0);
        REQUIRE(meta.timeout_ms > 0);
    }
    REQUIRE(evolution_metas == 2);
}

TEST_CASE("evolution::compile tool invokes SkillCompiler via registry", "[tools][cognitive][t5]") {
    ToolRegistry registry;
    auto compiler = std::make_shared<SkillCompiler>();
    cognitive::register_cognitive_tools(registry, nullptr, nullptr, compiler);
    REQUIRE(registry.has_tool("evolution::compile"));

    // SkillCompiler::compile 是纯函数式, 调用不依赖 LLM
    auto result = registry.call_tool("evolution::compile",
                                     {{"skill_md_content", "---\nname: test-skill\n---\n# Test"}});
    REQUIRE(result.contains("ok"));
    REQUIRE(result.contains("compiled_content"));
}