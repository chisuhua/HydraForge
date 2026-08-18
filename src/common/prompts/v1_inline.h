// ADR-0074 D-3: V1 schema 共享 helper — v2/v3 复用避免循环依赖
#pragma once

#include <string>

namespace agenticdsl::prompts::v1_inline {

inline std::string build_schema_constraint() {
  return R"({"type":"object","properties":{"result":{"type":"string"}}})";
}

}  // namespace agenticdsl::prompts::v1_inline
