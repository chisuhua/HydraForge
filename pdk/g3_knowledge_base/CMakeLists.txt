# pdk/g3_knowledge_base/CMakeLists.txt
# 功能描述：G3 Knowledge Base Plugin 编译配置 (Phase 6 W1)
#          编译产出 libhydraforge_g3_knowledge_base.so, 注册 1 个工具
# 设计依据：openspec/changes/phase6-service-ification-v1/
#          tasks.md §2, specs/knowledge-base-agent/spec.md
# 参考范式：pdk/llama_engine/ (C14 Phase 5 B2.1)
# 作者：Phase 6 W1 (Sisyphus-Junior)
# 最后修改日期：2026-07-15

add_library(hydraforge_g3_knowledge_base SHARED
  src/g3_entry.cpp
  src/g3_query.cpp
)

target_include_directories(hydraforge_g3_knowledge_base PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include
)

target_link_libraries(hydraforge_g3_knowledge_base PRIVATE
  hydraforge_pdk
)

target_compile_features(hydraforge_g3_knowledge_base PRIVATE cxx_std_20)

set_target_properties(hydraforge_g3_knowledge_base PROPERTIES
  PREFIX "lib"
  OUTPUT_NAME "hydraforge_g3_knowledge_base"
)