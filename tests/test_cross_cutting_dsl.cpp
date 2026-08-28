// tests/test_cross_cutting_dsl.cpp
// 文件头注释
// 功能描述：Cross-Cutting DSL 加载器单元测试 (ADR-0085 V1)。
//          2 个 TEST_CASE 覆盖:
//            1. dsl_load_high_security_mode_yaml
//            2. dsl_load_invalid_schema_throws
// 设计依据：openspec/changes/pdk-cross-cutting-patterns (ADR-0085)
// 作者：AgenticDSL Phase 3
// 最后修改日期：2026-08-28

#include "catch_amalgamated.hpp"

#include "agenticdsl/pdk/cross_cutting/cross_cutting_config.h"

#include <nlohmann/json.hpp>
#include <fstream>
#include <stdexcept>

using namespace hydraforge::pdk;

TEST_CASE("DSL loader loads high security mode YAML",
          "[pdk][cross_cutting][dsl]") {
    // 创建临时 YAML 文件
    std::string yaml_content = R"(
### AgenticDSL /__meta__
version: "1.0"
mode: high_security

### AgenticDSL /cross_cutting
patterns:
  - type: decorator-v1
    config:
      decorators: ["CostTracking"]
)";

    std::string temp_file = "/tmp/test_high_security.cc.md";
    std::ofstream file(temp_file);
    file << yaml_content;
    file.close();

    // 加载配置
    CrossCuttingConfig config = CrossCuttingConfig::load(temp_file);

    // 验证
    REQUIRE(config.is_valid());
    nlohmann::json json_config = config.to_json();
    REQUIRE(json_config.contains("patterns"));
    REQUIRE(json_config["patterns"].is_array());
    REQUIRE(json_config["patterns"].size() == 1);

    // 清理
    std::remove(temp_file.c_str());
}

TEST_CASE("DSL loader throws on invalid schema",
          "[pdk][cross_cutting][dsl]") {
    // 创建无效 YAML 文件
    std::string yaml_content = R"(
### AgenticDSL /__meta__
version: "1.0"
mode: invalid

### AgenticDSL /cross_cutting
patterns:
  - type: invalid-type
    config: {}
)";

    std::string temp_file = "/tmp/test_invalid.cc.md";
    std::ofstream file(temp_file);
    file << yaml_content;
    file.close();

    // 加载配置应抛出异常
    REQUIRE_THROWS_AS(CrossCuttingConfig::load(temp_file), std::invalid_argument);

    // 清理
    std::remove(temp_file.c_str());
}
