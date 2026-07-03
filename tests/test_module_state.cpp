// tests/test_module_state.cpp
// C10 Phase 5 Stage 1 Step 0: Lazy ModuleState (2026-07-03)
// 验证 ExecutionSession 的 per-module 持久化 json 状态机制
// 6 TEST_CASE: 首次创建 / 不覆盖 / 隔离 / 析构 / Session 隔离 / dsl_call 上下文注入

#include "catch_amalgamated.hpp"
#include "scheduler/execution_session.h"
#include "scheduler/resource_manager.h"  // PIMPL-lite: ResourceManager 完整类型
#include "core/types/node.h"
#include "core/types/budget.h"
#include "core/types/resource.h"
#include "common/llm/mock_provider.h"
#include "common/tools/registry.h"
#include <nlohmann/json.hpp>
#include <string>
#include <memory>
#include <vector>

using agenticdsl::ExecutionSession;
using agenticdsl::ExecutionBudget;
using agenticdsl::MockLLMProvider;
using agenticdsl::ToolRegistry;
using agenticdsl::ResourceManager;
using agenticdsl::ParsedGraph;

namespace {

// Fixture: 最小化 ExecutionSession 构造
struct ModuleStateFixture {
    ToolRegistry tools;
    MockLLMProvider mock_llm;
    ResourceManager resources;
    std::vector<ParsedGraph> empty_graphs;

    std::unique_ptr<ExecutionSession> make_session() {
        ExecutionBudget budget;
        budget.max_nodes = 100;
        budget.max_llm_calls = 50;
        budget.max_subgraph_depth = 10;
        budget.max_snapshots = 20;
        budget.snapshot_max_size_kb = 512;
        return std::make_unique<ExecutionSession>(
            std::make_optional(std::move(budget)),
            tools,
            &mock_llm,
            resources,
            &empty_graphs
        );
    }
};

} // anonymous namespace

// ============================================================
// 3.2: 首次访问创建空 json
// 验证 ensure_module_state 对未存在的 module_path 自动创建空 json 对象
// ============================================================
TEST_CASE("ensure_module_state creates empty json on first access",
          "[module_state][phase5][c10]") {
    ModuleStateFixture f;
    auto session = f.make_session();

    auto& state = session->ensure_module_state("lib/test/my_module");
    CHECK(state.is_object());
    CHECK(state.empty());

    const auto* maybe = session->get_module_state("lib/test/my_module");
    REQUIRE(maybe != nullptr);
    CHECK(maybe->is_object());
    CHECK(maybe->empty());
}

// ============================================================
// 3.3: 重复访问不重置已有状态
// 验证 ensure_module_state 对已存在的 module_path 不覆盖
// ============================================================
TEST_CASE("ensure_module_state does not reset existing state",
          "[module_state][phase5][c10]") {
    ModuleStateFixture f;
    auto session = f.make_session();

    auto& state = session->ensure_module_state("lib/test/counter");
    state["count"] = 5;

    auto& state_again = session->ensure_module_state("lib/test/counter");
    CHECK(state_again["count"] == 5);
}

// ============================================================
// 3.4: 不同 module_path 完全隔离
// 验证两个 module_path 的状态互不影响
// ============================================================
TEST_CASE("module_state isolation across different paths",
          "[module_state][phase5][c10]") {
    ModuleStateFixture f;
    auto session = f.make_session();

    session->ensure_module_state("module_a")["counter"] = 1;
    session->ensure_module_state("module_b")["counter"] = 2;

    CHECK(session->ensure_module_state("module_a")["counter"] == 1);
    CHECK(session->ensure_module_state("module_b")["counter"] == 2);

    // 修改一方不影响另一方
    session->ensure_module_state("module_a")["counter"] = 10;
    CHECK(session->ensure_module_state("module_b")["counter"] == 2);
}

// ============================================================
// 3.5: 析构无泄漏 (通过 ASan/TSan 间接验证)
// 验证构造 10 个 module_states 后析构无 crash / 0 ASan error
// ============================================================
TEST_CASE("module_state cleanup on ExecutionSession destruction",
          "[module_state][phase5][c10]") {
    ModuleStateFixture f;
    auto session = f.make_session();

    // 创建 10 个 module_states, 写入数据
    for (int i = 0; i < 10; ++i) {
        auto path = "mod" + std::to_string(i);
        auto& state = session->ensure_module_state(path);
        state["index"] = i;
        state["payload"] = std::string(1024, 'x');
    }

    // 析构 session — ASan 应报告 0 leak (通过 ASan preset 自动验证)
    session.reset();
    CHECK(true);  // 到达此点即无 crash
}

// ============================================================
// 3.6: Session 隔离 — 两个 session 的 module_states_ 互不影响
// ============================================================
TEST_CASE("module_state isolation across different sessions",
          "[module_state][phase5][c10]") {
    ModuleStateFixture f;
    auto s1 = f.make_session();
    auto s2 = f.make_session();

    s1->ensure_module_state("lib/test/counter")["count"] = 1;
    s2->ensure_module_state("lib/test/counter")["count"] = 2;

    s1->ensure_module_state("lib/test/counter")["count"] =
        s1->ensure_module_state("lib/test/counter")["count"].get<int>() + 1;

    CHECK(s1->ensure_module_state("lib/test/counter")["count"] == 2);
    CHECK(s2->ensure_module_state("lib/test/counter")["count"] == 2);
}

// ============================================================
// 3.7: get_module_state 对不存在的 path 返回 nullptr
// ============================================================
TEST_CASE("get_module_state returns nullptr for unknown path",
          "[module_state][phase5][c10]") {
    ModuleStateFixture f;
    auto session = f.make_session();

    const auto* result = session->get_module_state("nonexistent/module");
    CHECK(result == nullptr);
}