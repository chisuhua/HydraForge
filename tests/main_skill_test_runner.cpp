// tests/main_skill_test_runner.cpp
// 自定义 main() — 用于 test_skill_interpreter
// 支持 --skill-child 早期分支，使测试二进制可 spawn 自身作为子进程
// CATCH_AMALGAMATED_CUSTOM_MAIN 由 CMakeLists.txt 在 test_skill_interpreter 目标定义
#include "catch_amalgamated.hpp"

#include <string>

#include <agenticdsl/skill/skill_interpreter.h>

int main(int argc, char* argv[]) {
    if (argc > 1 && std::string(argv[1]) == "--skill-child") {
        return agenticdsl::skill_child_main(argc, argv);
    }
    Catch::Session session;
    return session.run(argc, argv);
}