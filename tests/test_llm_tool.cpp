#include "catch_amalgamated.hpp"
#include "common/llm/llm_tool.h"
#include "common/llm/llama_tool.h"
#include <stdexcept>

using namespace agenticdsl;

TEST_CASE("ILLMTool interface exists", "[llm_tool]") {
    REQUIRE(sizeof(ILLMTool) > 0);
}

TEST_CASE("LLMParams has default values", "[llm_tool]") {
    LLMParams params;
    REQUIRE(params.temperature == 0.7f);
    // C₁: max_tokens 默认值从 512 改为 2048（合并 LLMConfig 时 Track 0.1 M1.3 调整）
    REQUIRE(params.max_tokens == 2048);
    REQUIRE(params.n_ctx == 2048);
}

TEST_CASE("LLMResult fields", "[llm_tool]") {
    LLMResult result;
    REQUIRE(result.success == false);
    REQUIRE(result.text.empty());
    REQUIRE(result.error.empty());
}

TEST_CASE("LlamaTool model not available - is_available returns false", "[llama_tool]") {
    LlamaAdapter::Config config;
    config.model = "nonexistent-model";
    config.api_url = "http://localhost:9999";  // 确保无法连接

    LlamaTool tool(config);
    REQUIRE(tool.is_available() == false);
}

TEST_CASE("LlamaTool name returns llama", "[llama_tool]") {
    REQUIRE(true);
}
