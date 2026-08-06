#include <catch_amalgamated.hpp>
#include <cstdlib>
#include <string>

TEST_CASE("main.cpp has no hardcoded slash command strings", "[chat-slash-cmd]") {
  const std::string cmd =
      R"CMD(grep -nE '"\/(help|exit|compact|model|tree|fork|clone)' examples/pdk_chat_demo/main.cpp 2>/dev/null | grep -v 'input.front() ==' | grep -v 'starts_with("/")')CMD";
  int rc = std::system(cmd.c_str());
  REQUIRE(rc != 0);
}
