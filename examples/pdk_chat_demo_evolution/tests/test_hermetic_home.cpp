// examples/pdk_chat_demo_evolution/tests/test_hermetic_home.cpp
// L2 reference example hermetic HOME fixture test (per P0'-4)
// Per AGENTS.md mode #10 hygiene: NEVER pollute host filesystem; tests use hermetic HOME fixture
//
// TDD Step 1 (RED): Create test BEFORE implementation. Expect FAIL with "hermetic_home.h not found".

#include "catch_amalgamated.hpp"
#include "hermetic_home.h"
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

TEST_CASE("setup_hermetic_home creates /tmp/l2-test-XXXXXX", "[l2-evolution]") {
    char prev_home[1024] = {0};
    const char* prev = std::getenv("HOME");
    if (prev) {
        std::strncpy(prev_home, prev, sizeof(prev_home) - 1);
    }

    auto* guard = pdk_chat_demo_evolution::detail::setup_hermetic_home();
    REQUIRE(guard != nullptr);
    REQUIRE(fs::exists(guard->home));
    REQUIRE(guard->home.string().find("/tmp/l2-test-") == 0);

    // 确认 HOME 环境变量已重定向
    const char* new_home = std::getenv("HOME");
    REQUIRE(new_home != nullptr);
    REQUIRE(std::string(new_home) == guard->home.string());

    // 确认 genome 子目录已创建
    REQUIRE(fs::exists(guard->genome_dir));
    REQUIRE(guard->genome_dir.string().find("/.hydraforge/genomes") != std::string::npos);

    pdk_chat_demo_evolution::detail::cleanup_hermetic_home(guard);

    // 恢复 HOME (cleanup 不一定恢复, 由 test fixture 负责)
    if (prev) {
        setenv("HOME", prev_home, 1);
    }
}

TEST_CASE("setup_hermetic_home idempotent on double call", "[l2-evolution]") {
    auto* g1 = pdk_chat_demo_evolution::detail::setup_hermetic_home();
    REQUIRE(g1 != nullptr);

    auto* g2 = pdk_chat_demo_evolution::detail::setup_hermetic_home();
    REQUIRE(g2 != nullptr);

    // 两次调用应该返回相同 home (幂等)
    REQUIRE(g1->home.string() == g2->home.string());

    // cleanup 安全 (双 cleanup 应是 no-op)
    pdk_chat_demo_evolution::detail::cleanup_hermetic_home(g1);
    pdk_chat_demo_evolution::detail::cleanup_hermetic_home(g2);  // safe no-op
    pdk_chat_demo_evolution::detail::cleanup_hermetic_home(nullptr);  // safe no-op
}

TEST_CASE("cleanup_hermetic_home removes temp dir", "[l2-evolution]") {
    auto* guard = pdk_chat_demo_evolution::detail::setup_hermetic_home();
    REQUIRE(guard != nullptr);
    auto path = guard->home;
    REQUIRE(fs::exists(path));

    pdk_chat_demo_evolution::detail::cleanup_hermetic_home(guard);

    REQUIRE_FALSE(fs::exists(path));
}