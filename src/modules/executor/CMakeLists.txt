add_library(agenticdsl_modules_executor STATIC
    node_executor.cpp
    node.cpp
    yield_stream_bridge.cpp
    # ... 其他 executor 源文件 ...
)
target_include_directories(agenticdsl_modules_executor PUBLIC include)
target_link_libraries(agenticdsl_modules_executor PUBLIC agenticdsl_includes agenticdsl_common agenticdsl_modules_parser)
# ... 其他依赖 (tools, llm) ...
