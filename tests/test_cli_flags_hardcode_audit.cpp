#include <catch_amalgamated.hpp>
#include <cstdlib>
#include <cstring>

TEST_CASE("main.cpp has no hand-rolled argv scan", "[cli][stage3][audit]") {
  int result = std::system(
      "grep -nE 'vector<string>.*argv|argv\\[|argc > 1.*--mock|args\\[i\\].*--session' "
      "examples/pdk_chat_demo/main.cpp > /dev/null 2>&1");
  CHECK(result != 0);
}
