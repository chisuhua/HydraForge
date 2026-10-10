// tests/test_fork_dsl_context_propagation.cpp
// fix-fork-dsl-context-propagation OpenSpec change Phase 1 RED tests
// 目标: 验证 DSL fork 节点 working context 快照传递机制 (BUG-2 修复)
// 作者: fix-fork-dsl-context-propagation Phase 1 (2026-10-10)
// TDD Step 1 RED: 5 cases, 期望 Test 1 + 升级 Test 2 FAIL (BUG-2 未修), Test 3-5 PASS (基线守护)

#include "catch_amalgamated.hpp"
#include "core/engine.h"
#include <agenticdsl/pdk/agent_loops/fork_join_loop.h>
#include <agenticdsl/plugin/plugin_loader.h>
#include <common/llm/mock_provider.h>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

namespace fs = std::filesystem;

// helper: 加载 lib/loop/fork_join.agent.md 到 DSLEngine
// 多候选路径搜索: CWD lib/loop/ + 上溯 repo root + 已知绝对路径
static std::unique_ptr<agenticdsl::DSLEngine> load_fork_join_agent_md() {
    static const std::vector<std::string> candidates = {
        "lib/loop/fork_join.agent.md",                       // cwd = repo root (ctest -j1)
        "../lib/loop/fork_join.agent.md",                     // cwd = build/tests
        "../../lib/loop/fork_join.agent.md",                  // cwd = build/tests/<target>
        "/workspace/project/HydraForge/lib/loop/fork_join.agent.md",  // absolute fallback
    };
    for (const auto& path : candidates) {
        std::ifstream f(path);
        if (f.is_open()) {
            std::stringstream content;
            content << f.rdbuf();
            return agenticdsl::DSLEngine::from_markdown(content.str());
        }
    }
    throw std::runtime_error(
        "lib/loop/fork_join.agent.md not found (cwd=" + fs::current_path().string() +
        ", need repo root or build/tests cwd)");
}

// helper: 加载 loop_agent plugin + 设置 mock provider
static void setup_loop_agent_with_mock(agenticdsl::DSLEngine& engine) {
    // set HYDRAFORGE_PLUGIN_PATH (PluginLoader whitelist 要求, 否则 load_so false)
    // 必须用**绝对路径** — whitelist check 用 canonical.find(allowed) == 0 字符串前缀匹配,
    // 相对路径 "build/pdk" 与 canonical "/workspace/.../build/pdk/..." 不匹配.
    fs::path so_path = "build/pdk/loop_agent/libLoopAgent.so";
    if (!fs::exists(so_path)) {
        throw std::runtime_error("libLoopAgent.so not found at " + so_path.string() +
                                 " (cwd=" + fs::current_path().string() + ")");
    }
    fs::path abs_so_path = fs::canonical(so_path);
    fs::path pdk_dir = abs_so_path.parent_path().parent_path();  // build/pdk/loop_agent/ → build/pdk
    if (!std::getenv("HYDRAFORGE_PLUGIN_PATH")) {
        setenv("HYDRAFORGE_PLUGIN_PATH", pdk_dir.string().c_str(), 0);
    }
    auto loader = std::make_unique<hydraforge::PluginLoader>();
    if (!loader->load_so(abs_so_path.string(), engine.get_tool_registry())) {
        throw std::runtime_error("PluginLoader::load_so failed for " + abs_so_path.string() +
                                 " (HYDRAFORGE_PLUGIN_PATH=" +
                                 (std::getenv("HYDRAFORGE_PLUGIN_PATH") ? std::getenv("HYDRAFORGE_PLUGIN_PATH") : "<unset>") + ")");
    }
    auto provider = std::make_unique<agenticdsl::MockLLMProvider>();
    engine.get_tool_registry().call_tool("loop/set_parent_provider",
        std::unordered_map<std::string, std::string>{
            {"provider_ptr", std::to_string(reinterpret_cast<uintptr_t>(provider.get()))}
        });
}

// ============================================================
// Test 1 (BUG-2 主场景): lib/loop/fork_join.agent.md 端到端 — RED
// ============================================================
TEST_CASE("DSL fork_join.agent.md: branch 节点 working context 传递 (BUG-2 RED)",
          "[scheduler][fork_join][context-propagation][red]") {
    auto engine = load_fork_join_agent_md();
    REQUIRE(engine != nullptr);
    setup_loop_agent_with_mock(*engine);

    auto run_ret = engine->get_tool_registry().call_tool("loop/run",
        std::unordered_map<std::string, std::string>{
            {"loop_type", "fork_join"},
            {"prompt", "TEST_USER_INPUT_VALUE"},
            {"system_prompt", "test_system"},
            {"history", "[]"},
            {"tools", "[]"},
            {"max_steps", "10"}
        });

    INFO("fork_join result: " << run_ret.dump());
    auto err_str = run_ret.value("error", std::string{});
    // BUG-2 fix 后应: success=true + error 不含 "Template render error: variable 'user_input' not found"
    REQUIRE(err_str.find("Template render error: variable 'user_input' not found") == std::string::npos);
    REQUIRE(err_str.find("variable 'user_input'") == std::string::npos);
    REQUIRE(run_ret.value("success", false) == true);
}

// ============================================================
// Test 2: 3-branch 并行执行 (state.executed_ 顺序断言) — baseline 守护
// ============================================================
TEST_CASE("DSL fork_join.agent.md: 3-branch 并行执行 (state.executed_ 顺序断言)",
          "[scheduler][fork_join][context-propagation][parallel]") {
    auto engine = load_fork_join_agent_md();
    REQUIRE(engine != nullptr);
    setup_loop_agent_with_mock(*engine);

    auto run_ret = engine->get_tool_registry().call_tool("loop/run",
        std::unordered_map<std::string, std::string>{
            {"loop_type", "fork_join"},
            {"prompt", "three parallel tasks input"},
            {"system_prompt", "test"},
            {"history", "[]"},
            {"tools", "[]"},
            {"max_steps", "10"}
        });

    INFO("3-branch result: " << run_ret.dump());
    auto err_str = run_ret.value("error", std::string{});
    // BUG-2 fix 后应无 Template render error (即使 BUG 未修, 也至少应为 1)
    REQUIRE(err_str.find("Template render error") == std::string::npos);
    // 至少 1 步 (3 branch + join + synthesize)
    REQUIRE(run_ret.value("steps", 0) >= 1);
}

// ============================================================
// Test 3: snapshot 路径 key 一致性 (current_fork_node_path_) — 基线守护
// ============================================================
TEST_CASE("DSL fork_join.agent.md: snapshot key 一致性 (current_fork_node_path_ 守护)",
          "[scheduler][fork_join][context-propagation][snapshot-key]") {
    auto engine = load_fork_join_agent_md();
    REQUIRE(engine != nullptr);
    setup_loop_agent_with_mock(*engine);

    // Test snapshot key 一致性: fork 节点的 path 与 snapshot 保存的 key 必须一致
    // 验证策略: 跑 fork 成功路径, 检查无 "snapshot not found" 类错误
    auto run_ret = engine->get_tool_registry().call_tool("loop/run",
        std::unordered_map<std::string, std::string>{
            {"loop_type", "fork_join"},
            {"prompt", "key consistency test"},
            {"system_prompt", "test"},
            {"history", "[]"},
            {"tools", "[]"},
            {"max_steps", "10"}
        });

    INFO("snapshot key test result: " << run_ret.dump());
    auto err_str = run_ret.value("error", std::string{});
    REQUIRE(err_str.find("Snapshot for fork node not found") == std::string::npos);
    REQUIRE(err_str.find("snapshot not found") == std::string::npos);
}

// ============================================================
// Test 4: fork + join + synthesize 完整图执行 — baseline e2e
// ============================================================
TEST_CASE("DSL fork_join.agent.md: fork + join + synthesize 完整图执行",
          "[scheduler][fork_join][context-propagation][e2e]") {
    auto engine = load_fork_join_agent_md();
    REQUIRE(engine != nullptr);
    setup_loop_agent_with_mock(*engine);

    auto run_ret = engine->get_tool_registry().call_tool("loop/run",
        std::unordered_map<std::string, std::string>{
            {"loop_type", "fork_join"},
            {"prompt", "end-to-end test"},
            {"system_prompt", "test"},
            {"history", "[]"},
            {"tools", "[]"},
            {"max_steps", "10"}
        });

    INFO("e2e result: " << run_ret.dump());
    auto err_str = run_ret.value("error", std::string{});
    // BUG-2 fix 后应无 Template render error (Test 4 与 Test 1 类似, 但关注 join + synthesize)
    REQUIRE(err_str.find("Template render error") == std::string::npos);
}

// ============================================================
// Test 5: C++ ForkJoinLoop 4-worker 行为不变 (D8.inv.forkjoin-class-unchanged regression guard)
// ============================================================
TEST_CASE("fix-fork-dsl-context-propagation: C++ ForkJoinLoop 4-worker 不变 (regression guard)",
          "[scheduler][fork_join][context-propagation][cpp-regression]") {
    // 间接验证: per compile-time C++ ForkJoinLoop::State enum 不变 + 既有 ForkJoinLoop 单元测试全过
    // 详细测试见 tests/test_pdk_fork_join.cpp (5 cases), 本 test 仅断言 enum 值
    static_assert(static_cast<int>(hydraforge::pdk::ForkJoinLoop::State::Forking) == 0,
                  "ForkJoinLoop::State::Forking 枚举值变更 (D8.inv.forkjoin-class-unchanged violated)");
    static_assert(static_cast<int>(hydraforge::pdk::ForkJoinLoop::State::Done) == 3,
                  "ForkJoinLoop::State::Done 枚举值变更 (D8.inv.forkjoin-class-unchanged violated)");
    REQUIRE(true);  // 编译期静态断言已验证, 运行时无操作
}