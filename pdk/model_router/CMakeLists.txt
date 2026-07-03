# pdk/model_router/CMakeLists.txt
# 功能描述：Model Router Plugin 聚合父 CMake (C7 Phase 2)
#          聚合 3 个策略子目录 + ModelRegistry 工具 (C7 Phase 2 新增)
# 设计依据：openspec/changes/2026-06-26-adr-0034-model-router-plugin/
#          design.md Decision 1-4
# 作者：C7 Phase 2
# 最后修改日期：2026-07-02

add_subdirectory(cost_strategy)
add_subdirectory(quality_strategy)
add_subdirectory(latency_strategy)

# ModelRegistry 工具 (.so, C7 Phase 2 新增)
add_library(hydraforge_model_registry SHARED
  model_registry.cpp
)

target_include_directories(hydraforge_model_registry PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include
)

target_link_libraries(hydraforge_model_registry PRIVATE
  hydraforge_pdk
)

target_compile_features(hydraforge_model_registry PRIVATE cxx_std_20)