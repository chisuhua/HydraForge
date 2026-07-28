cmake_minimum_required(VERSION 3.20)
# pdk/temporal_agent/CMakeLists.txt
# 功能描述：Temporal Agent Plugin 编译配置
#          编译产出 libTemporalAgent.so，注册 5 个 Temporal 工作流编排工具
#          (start_workflow / start_async / poll / signal / query)
# 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md
# 参考范式：pdk/session_agent/ (PDK plugin template)
# 作者：pkgm-temporal-agent change
# 最后修改日期：2026-07-27
#
# gRPC/Protobuf 回退策略:
#   本环境缺少 protoc/grpc_cpp_plugin 开发工具 (仅运行时 libgrpc++1.51 / libprotobuf32).
#   设计采用编译期零 gRPC 依赖的 TemporalClient 抽象:
#     - 默认实现 InMemoryTemporalBackend (进程内模拟, 供测试与开发)
#     - 真实 gRPC 实现通过 GrpcTemporalBackend (条件编译, 需 -DTEMPORAL_ENABLE_GRPC=ON
#       + gRPC/protobuf 开发头文件 + protoc 生成的 WorkflowService stub)
#   完整 gRPC 集成为 Phase 2 follow-up (见 tasks.md §7).

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ===== gRPC / Protobuf 可选依赖 (Phase 2, 默认 OFF) =====
option(TEMPORAL_ENABLE_GRPC "Enable real gRPC Temporal client (requires protoc + gRPC dev)" OFF)

if(TEMPORAL_ENABLE_GRPC)
  # Phase 2: 真实 gRPC 链接
  # find_package(Protobuf CONFIG REQUIRED)
  # find_package(gRPC CONFIG REQUIRED)
  # FetchContent_Declare(temporal_api
  #   GIT_REPOSITORY https://github.com/temporalio/api.git
  #   GIT_TAG v1.27.0)
  # FetchContent_MakeAvailable(temporal_api)
  # protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS ${temporal_api_SOURCE_DIR}/temporal/api/workflowservice/v1/service.proto)
  message(FATAL_ERROR "TEMPORAL_ENABLE_GRPC=ON requires gRPC dev packages (not yet wired - Phase 2)")
endif()

add_library(TemporalAgent SHARED
    src/pdk_entry.cpp
    src/temporal_client.cpp
    src/temporal_client_pool.cpp
    src/workflow_callback_channel.cpp
)

target_include_directories(TemporalAgent PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/external/nlohmann_json/single_include
)

target_link_libraries(TemporalAgent PRIVATE
    hydraforge_pdk
)

target_compile_features(TemporalAgent PRIVATE cxx_std_20)

# 输出: libTemporalAgent.so
set_target_properties(TemporalAgent PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "TemporalAgent"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/pdk/temporal_agent"
)

install(TARGETS TemporalAgent LIBRARY DESTINATION lib/pdk/temporal_agent)
