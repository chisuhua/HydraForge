# src/modules/plugin/CMakeLists.txt
# 功能描述: PluginLoader 实现模块 (Sprint 5 实施, ADR-0022)
#          Linux only: dlopen/dlsym/dlclose 实现
#          agenticdsl_modules_plugin STATIC 库
# 设计依据: ADR-0022 §2-3 + openspec/changes/2026-07-14-plugin-loader
# 作者: AgenticDSL Phase 1 Sprint 5
# 最后修改日期: 2026-06-19

# STATIC 库 (Linux only — dlopen)
# 注: 由于依赖 <dlfcn.h> (Linux only), 编译时需要 __linux__ 检查
add_library(agenticdsl_modules_plugin STATIC plugin_loader.cpp)

target_link_libraries(agenticdsl_modules_plugin PUBLIC
  agenticdsl_includes
  agenticdsl_common
  agenticdsl_core
)

# dlopen/dlsym/dlclose (Linux 系统库)
target_link_libraries(agenticdsl_modules_plugin PUBLIC ${CMAKE_DL_LIBS})

# 头文件路径: agenticdsl_hdr_plugin 提供 plugin_loader.h / plugin_info.h
target_link_libraries(agenticdsl_modules_plugin PUBLIC agenticdsl_hdr_plugin)

# Linux 系统库 (无 dlfcn.h for Linux)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(agenticdsl_modules_plugin PUBLIC dl)
endif()

target_include_directories(agenticdsl_modules_plugin PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)