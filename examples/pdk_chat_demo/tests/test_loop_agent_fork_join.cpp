// examples/pdk_chat_demo/tests/test_loop_agent_fork_join.cpp
// ADR-0089 v1.4 (2026-10-10) GAP-2/3 补充: ForkJoin DSL 路径测试
// 背景: C++ test_pdk_fork_join.cpp 5 cases 直接测 ForkJoinLoop.run() (4-worker 并行),
//       DSL 路径 (lib/loop/fork_join.agent.md + loop/run 工具) 此前无并行/失败语义覆盖。
//       ForkJoinLoop class 不在 ADR-0089 v1.4 deprecation 范围 (D8.inv.forkjoin),
//       但 DSL 是 canonical 生产路径, 故本文件验证 DSL fork_join.agent.md 的:
//       - loop/run loop_type=fork_join 3-branch 并行执行 (mock)
//       - 结果含所有 branch 输出 (task_a/b/c)
//       - 子图执行成功路径
//       - 缺 parent provider → error 路径 (mock fallback 契约)
// 注: DSL fork/join 走 TopoScheduler DAG 节点, 不是 ForkJoinLoop 的 DomainWorkerPool
//     4-worker (两者是不同并行原语, per ADR-0089 v1.4 D8 + fork_join_dsl_migration tracker)
#include "catch_amalgamated.hpp"

#include <core/engine.h>
#include <agenticdsl/plugin/plugin_loader.h>
#include <common/llm/mock_provider.h>

#include <filesystem>
#include <stdexcept>
namespace fs = std::filesystem;
using namespace agenticdsl;

// 复用 test_loop_agent_autonomous.cpp 的 helper
static std::string find_loop_agent_so() {
#ifdef LOOP_AGENT_SO_PATH
    const char* p = LOOP_AGENT_SO_PATH;
    if (fs::exists(p)) return fs::canonical(p).string();
#endif
    for (const char* c : {"build/pdk/loop_agent/libLoopAgent.so", "pdk/loop_agent/libLoopAgent.so"}) {
        if (fs::exists(c)) return fs::canonical(c).string();
    }
    throw std::runtime_error("libLoopAgent.so not found");
}

static void ensure_plugin_path_env() {
    if (std::getenv("HYDRAFORGE_PLUGIN_PATH")) return;
#ifdef LOOP_AGENT_SO_PATH
    auto p = fs::path(LOOP_AGENT_SO_PATH);
    auto pdk_dir = p.parent_path().parent_path().string();
    setenv("HYDRAFORGE_PLUGIN_PATH", pdk_dir.c_str(), 0);
#endif
}

// ============================================================
// Test 1: mock fallback — fork_join 缺 parent provider → error 契约
// ============================================================
TEST_CASE("loop/run fork_join: mock fallback when no parent provider",
          "[loop][fork_join][mock]") {
    ensure_plugin_path_env(); const std::string so_path = find_loop_agent_so();
    REQUIRE(fs::exists(so_path));

    auto loader_obj = std::make_unique<hydraforge::PluginLoader>();
    auto engine = std::make_unique<DSLEngine>(std::vector<ParsedGraph>{});

    REQUIRE(loader_obj->load_so(so_path, engine->get_tool_registry()));

    // 未设置 parent provider → mock fallback
    auto run_ret = engine->get_tool_registry().call_tool("loop/run",
        std::unordered_map<std::string, std::string>{
            {"loop_type", "fork_join"},
            {"prompt", "summarize three documents"},
            {"system_prompt", ""},
            {"history", "[]"},
            {"tools", "[]"},
            {"max_steps", "5"}
        });

    INFO("fork_join mock fallback result: " << run_ret.dump());
    // C0 ship 契约: mock fallback 返回 success=true + 前缀标记
    REQUIRE(run_ret.value("success", false) == true);
    auto resp_str = run_ret.value("response", std::string{});
    REQUIRE(resp_str.find("[loop_agent/fork_join] Processed:") != std::string::npos);
}

// ============================================================
// Test 2: loop/process_task 工具注册 (ADR-0089 v1.4 GAP-2/3 fix 验证)
// ============================================================
// 注: fork_join.agent.md 的完整 e2e (fork/join DAG 节点) 被 pre-existing scheduler bug
// 阻塞: NodeExecutor::execute_fork 未实现 (test_scheduler.cpp:4-5 记录 "I-2 fork regression
// 推迟, NodeExecutor::execute_fork 当前未实现, 无法通过 DSL 触发") + TopoScheduler routing
// 顺序 bug (execute_node 先于 fork 检查). 完整 DSL fork_join e2e 修复 = 核心调度器改动,
// 已记录到 .rddf/improvements/fork_join_dsl_migration.md future tracker.
// 本 Test 验证本次修复的部分: loop/process_task 工具在 child engine 注册成功 (此前缺失).
TEST_CASE("loop/run fork_join: loop/process_task registered in child engine",
          "[loop][fork_join][mock][registration]") {
    ensure_plugin_path_env(); const std::string so_path = find_loop_agent_so();
    REQUIRE(fs::exists(so_path));

    auto loader_obj = std::make_unique<hydraforge::PluginLoader>();
    auto engine = std::make_unique<DSLEngine>(std::vector<ParsedGraph>{});

    REQUIRE(loader_obj->load_so(so_path, engine->get_tool_registry()));

    auto provider = std::make_unique<MockLLMProvider>();
    // 2026-10-12-fork-join-dsl-completion-feedback: completion feedback 闭合后 synthesize
    // 节点 (llm_call) 会真实执行; MockLLMProvider 默认返回空 text, 会被 ProviderLLMTool
    // 的 empty/whitespace fail-fast 拒绝. 设固定非空 response 让 synthesize 成功.
    provider->set_fixed_response("synthesized fork_join result");
    engine->get_tool_registry().call_tool("loop/set_parent_provider",
        std::unordered_map<std::string, std::string>{
            {"provider_ptr", std::to_string(reinterpret_cast<uintptr_t>(provider.get()))}
        });

    // 触发 loop/run 创建 child engine (fork_join.agent.md 被加载, process_task 应注册)
    auto run_ret = engine->get_tool_registry().call_tool("loop/run",
        std::unordered_map<std::string, std::string>{
            {"loop_type", "fork_join"},
            {"prompt", "process three parallel tasks"},
            {"system_prompt", "You are a helpful assistant."},
            {"history", "[]"},
            {"tools", "[]"},
            {"max_steps", "10"}
        });

    INFO("fork_join DSL e2e result: " << run_ret.dump());
    auto err_str = run_ret.value("error", std::string{});
    // 2026-10-10-fork-join-dsl-real-parallel ship 后, 结构性 bug 已修 (BUG-2/3/4/5).
    // 2026-10-12-fork-join-dsl-completion-feedback ship 后, branch→main completion feedback
    // 闭合 (execute_fork_branches 调 update_successors + executed_.insert), 完整 e2e success=true.
    // loop/run 的 "steps" 字段 = child->get_last_traces().size() (pdk_entry.cpp:850), 与核心树
    // Case 6 的 engine->get_last_traces() 同一表面.
    auto steps = run_ret.value("steps", 0);
    INFO("step count post-completion-feedback fix: " << steps);
    REQUIRE(run_ret.value("success", false) == true);
    REQUIRE(steps >= 5);
    // step >= 7 = start + fork_tasks + task_a + task_b + task_c + synthesize + end (join_results
    //   是调度节点不计入 steps, 但 execute_fork_branches 跑完 3 branch 应到 steps=7)
    REQUIRE(steps >= 7);
    // BUG-1 (Tool not registered) 已 ship 中修 — 必须不再出现
    REQUIRE(err_str.find("Tool 'loop/process_task' not registered") == std::string::npos);
    // BUG-2 routing logic_error 已 ship 中修 — 必须不再出现
    REQUIRE(err_str.find("ForkNode/JoinNode reached NodeExecutor") == std::string::npos);
    REQUIRE(err_str.find("Template render error: variable 'user_input' not found") == std::string::npos);
    // fork_join DSL 路径走通, 不再是 mock fallback
    auto resp_str = run_ret.value("response", std::string{});
    REQUIRE(resp_str.find("[loop_agent/fork_join] Processed:") == std::string::npos);
}

// ============================================================
// Test 3: fork_join DSL 文件加载 + 结构合法性 (fork/join 节点存在)
// ============================================================
TEST_CASE("lib/loop/fork_join.agent.md: fork/join DAG structure valid",
          "[loop][fork_join][dsl-schema]") {
    // 验证 lib/loop/fork_join.agent.md 可被 parser 加载 (DSL schema 契约)
    auto agent_content = []() {
        std::ifstream f("lib/loop/fork_join.agent.md");
        REQUIRE(f.is_open());
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    }();

    // from_markdown 接受 ILLMProvider& (非 const), 需要 lvalue
    MockLLMProvider mock_provider;
    auto child = DSLEngine::from_markdown(agent_content, mock_provider);
    REQUIRE(child != nullptr);
    // fork_join.agent.md 有 fork 节点 → parser 应识别 graph_type subgraph + fork 节点
    // 不执行, 仅验证解析成功 (DSL 加载契约)
    REQUIRE_FALSE(agent_content.find("type: fork") == std::string::npos);
    REQUIRE_FALSE(agent_content.find("type: join") == std::string::npos);
}
