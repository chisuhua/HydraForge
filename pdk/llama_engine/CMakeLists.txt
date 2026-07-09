# pdk/llama_engine/CMakeLists.txt
# 功能描述：Llama Engine Plugin 编译配置 (C14 Phase 5 B2.1)
#          编译产出 libhydraforge_llama_engine.so，注册 12 个推理工具
#          (4 engine + 4 model + 4 C13 架构工具)
# 设计依据：openspec/changes/phase5-llama-engine-plugin/
#          proposal.md §1-8, tasks.md §1-12
# 参考范式：pdk/model_router/ (C7 Phase 1-2, ADR-0034)
# 作者：C14 Oracle review session
# 最后修改日期：2026-07-07

# 主 .so: 聚合所有 12 个工具注册
add_library(hydraforge_llama_engine SHARED
  src/llama_engine_entry.cpp    # entry point (pdk_register_tools + pdk_plugin_info)
  src/llama_engine.cpp          # 4 engine 工具: init/generate/stream/status
  src/llama_model.cpp           # 4 model 工具: load/unload/list/switch
  src/inference_arch.cpp        # 4 C13 架构工具: prefix_cache/kv_cache/decoding/cloud_engine
)

target_include_directories(hydraforge_llama_engine PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/src
  ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include
  ${PROJECT_SOURCE_DIR}/external/AgenticLlama/include
  ${PROJECT_SOURCE_DIR}/external/AgenticLlama/ggml/include
  # llama.cpp headers (环境相关，构建时注入):
  # ${LLAMA_CPP_INCLUDE_DIR}
)

target_link_libraries(hydraforge_llama_engine PRIVATE
  hydraforge_pdk
  # llama.cpp library (环境相关，构建时注入):
  # llama
  # ggml
)

target_compile_features(hydraforge_llama_engine PRIVATE cxx_std_20)

# 输出命名: libhydraforge_llama_engine.so
set_target_properties(hydraforge_llama_engine PROPERTIES
  PREFIX "lib"
  OUTPUT_NAME "hydraforge_llama_engine"
)