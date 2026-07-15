# pdk/g1_coding_assistant/CMakeLists.txt
# 功能描述：G1 Coding Assistant Plugin 编译配置 (Phase 6 W1)
#          编译产出 libhydraforge_g1_coding_assistant.so, 注册 1 个工具
#          使用 DEFINE_AGENT(CodingAssistant, AgentLoopType::React) 2 参数宏
# 设计依据：openspec/changes/phase6-service-ification-v1/
#          tasks.md §3, specs/coding-assistant-agent/spec.md
# 参考范式：pdk/g3_knowledge_base/ (Phase 6 W1) + pdk/llama_engine/ (C14)
# 作者：Phase 6 W1 (Sisyphus-Junior)
# 最后修改日期：2026-07-15

add_library(hydraforge_g1_coding_assistant SHARED
  src/g1_entry.cpp
  src/g1_agent.cpp
)

target_include_directories(hydraforge_g1_coding_assistant PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include
)

target_link_libraries(hydraforge_g1_coding_assistant PRIVATE
  hydraforge_pdk
)

target_compile_features(hydraforge_g1_coding_assistant PRIVATE cxx_std_20)

set_target_properties(hydraforge_g1_coding_assistant PROPERTIES
  PREFIX "lib"
  OUTPUT_NAME "hydraforge_g1_coding_assistant"
)