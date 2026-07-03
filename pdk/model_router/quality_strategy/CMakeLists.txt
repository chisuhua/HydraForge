# pdk/model_router/quality_strategy/CMakeLists.txt
# 功能描述：QualityModelRouter Plugin 构建配置 (C7 Phase 2)
#          生成 SHARED 库 libhydraforge_model_router_quality.so,
#          export extern "C" 符号 pdk_register_tools.
#          链接 hydraforge_pdk (PDK 头文件) + nlohmann_json (JSON 解析).
# 设计依据：openspec/changes/2026-06-26-adr-0034-model-router-plugin/design.md Decision 1-2
# 作者：C7 Phase 2
# 最后修改日期：2026-07-02

add_library(hydraforge_model_router_quality SHARED
  quality_router.cpp
  quality_router.h
)

# 头文件 include 路径 (PDK 头 + nlohmann_json)
target_include_directories(hydraforge_model_router_quality PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include
)

# 链接 PDK 接口库 (header-only, 提供 model_router.h + tool_macros.h + contract 类型)
target_link_libraries(hydraforge_model_router_quality PRIVATE
  hydraforge_pdk
)

# C++20 标准
target_compile_features(hydraforge_model_router_quality PRIVATE cxx_std_20)