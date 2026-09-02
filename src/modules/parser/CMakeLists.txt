add_library(agenticdsl_modules_parser STATIC
    markdown_parser.cpp
    node_factory.cpp
    declarative_style.cpp
)
target_include_directories(agenticdsl_modules_parser PUBLIC include)
target_link_libraries(agenticdsl_modules_parser PUBLIC agenticdsl_common agenticdsl_modules_executor)
# ... 其他依赖 ...
