# pdk/model_router/slm_strategy/CMakeLists.txt
# 功能描述：SLMModelRouter Plugin 构建配置 (T16, ADR-0061-04)
#          生成 SHARED 库 libhydraforge_model_router_slm.so,
#          export extern "C" 符号 pdk_register_tools.
#          链接 hydraforge_pdk (PDK 头文件) + nlohmann_json (JSON 解析).
# 设计依据：openspec/changes/2026-08-24-adr-0061-04-slm-routing/
#          ADR-0061-04 SLM 路由优先 (NVIDIA Position Paper 2025)
#          capability-application-map-2026-08.md §八 T16
# 作者：HydraForge Sprint 23 T16 ship (Phase C opportunist)
# 最后修改日期：2026-08-24

add_library(hydraforge_model_router_slm SHARED
  slm_router.cpp
  slm_router.h
)

# 头文件 include 路径 (PDK 头 + nlohmann_json)
target_include_directories(hydraforge_model_router_slm PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include
)

# 链接 PDK 接口库 (header-only, 提供 model_router.h + tool_macros.h + contract 类型)
target_link_libraries(hydraforge_model_router_slm PRIVATE
  hydraforge_pdk
)

# C++20 标准
target_compile_features(hydraforge_model_router_slm PRIVATE cxx_std_20)