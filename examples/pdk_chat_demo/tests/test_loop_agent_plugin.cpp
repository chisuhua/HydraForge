// tests/test_loop_agent_plugin.cpp
// loop-agent-dsl-execution: Loop Agent 插件集成测试
#include "catch_amalgamated.hpp"
#include <core/engine.h>
#include <common/llm/mock_provider.h>
#include <agenticdsl/plugin/plugin_loader.h>
#include <agenticdsl/contract/itool_registry.h>
#include <filesystem>
#include <sstream>
namespace fs = std::filesystem;
using namespace agenticdsl;

static std::string find_loop_agent_so() {
#ifdef LOOP_AGENT_SO_PATH
    const char* p = LOOP_AGENT_SO_PATH;
    if (fs::exists(p)) return fs::canonical(p).string();
#endif
    for (const char* c : {"../pdk/loop_agent/libLoopAgent.so", "pdk/loop_agent/libLoopAgent.so"}) {
        if (fs::exists(c)) return fs::canonical(c).string();
    }
    throw std::runtime_error("libLoopAgent.so not found");
}
static std::string ptr_to_str(ILLMProvider* p) {
    std::stringstream ss; ss << reinterpret_cast<uintptr_t>(p); return ss.str();
}

TEST_CASE("LoopAgent loads and registers tools", "[loop-agent][plugin][load]") {
    hydraforge::PluginLoader loader;
    auto engine = std::make_unique<DSLEngine>(std::vector<ParsedGraph>{});
    // engine 必须在 loader 之前析构（避免 dlclose 后遗留 function pointer）
    REQUIRE(loader.load_so(find_loop_agent_so(), engine->get_tool_registry()));
    engine.reset(); // 先释放 engine（清理 ToolRegistry 中的 lambda），再析构 loader
}

TEST_CASE("loop/set_parent_provider callable", "[loop-agent][plugin]") {
    hydraforge::PluginLoader loader;
    auto engine = std::make_unique<DSLEngine>(std::vector<ParsedGraph>{});
    REQUIRE(loader.load_so(find_loop_agent_so(), engine->get_tool_registry()));
    MockLLMProvider mock;
    auto result = engine->get_tool_registry().call_tool("loop/set_parent_provider",
        std::unordered_map<std::string, std::string>{{"provider_ptr", ptr_to_str(&mock)}});
    REQUIRE(result.value("success", false) == true);
    engine.reset();
}

TEST_CASE("loop/run mock fallback when no provider", "[loop-agent][plugin][fallback]") {
    hydraforge::PluginLoader loader;
    auto engine = std::make_unique<DSLEngine>(std::vector<ParsedGraph>{});
    REQUIRE(loader.load_so(find_loop_agent_so(), engine->get_tool_registry()));
    auto result = engine->get_tool_registry().call_tool("loop/run",
        std::unordered_map<std::string, std::string>{{"loop_type", "react"}, {"prompt", "test"}});
    REQUIRE(result.contains("response"));
    engine.reset();
}

TEST_CASE("loop/run rejects invalid loop_type", "[loop-agent][plugin][validation]") {
    hydraforge::PluginLoader loader;
    auto engine = std::make_unique<DSLEngine>(std::vector<ParsedGraph>{});
    REQUIRE(loader.load_so(find_loop_agent_so(), engine->get_tool_registry()));
    MockLLMProvider mock;
    engine->get_tool_registry().call_tool("loop/set_parent_provider",
        std::unordered_map<std::string, std::string>{{"provider_ptr", ptr_to_str(&mock)}});
    auto result = engine->get_tool_registry().call_tool("loop/run",
        std::unordered_map<std::string, std::string>{{"loop_type", "invalid"}, {"prompt", "x"}});
    REQUIRE(result.value("success", false) == false);
    engine.reset();
}
