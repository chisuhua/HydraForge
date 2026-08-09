# src/modules/pdk/CMakeLists.txt
# PDK Manifest 模块 - Phase 6a step 1
# 包含: ManifestValidator (JSON 校验) + ManifestFinder (路径查找)
# 关联 ADR: ADR-0052, ADR-0022 §3.2 (dual-ABI)

add_library(agenticdsl_modules_pdk STATIC
  manifest.cpp
  manifest_validator.cpp
  manifest_finder.cpp
)

target_include_directories(agenticdsl_modules_pdk
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../../include
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# nlohmann::json 已在 external/nlohmann_json (single header)
find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp
  PATHS ${CMAKE_SOURCE_DIR}/external/nlohmann_json/include
  NO_DEFAULT_PATH
)
if(NOT NLOHMANN_JSON_INCLUDE_DIR)
  message(FATAL_ERROR "nlohmann/json.hpp not found at external/nlohmann_json/include")
endif()
target_include_directories(agenticdsl_modules_pdk PUBLIC ${NLOHMANN_JSON_INCLUDE_DIR})

# C++20 + warnings per AGENTS.md
target_compile_features(agenticdsl_modules_pdk PUBLIC cxx_std_20)
target_compile_options(agenticdsl_modules_pdk PRIVATE
  -Wall -Wextra -Wpedantic
)
