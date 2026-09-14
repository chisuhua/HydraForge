# pdk/chat_session/CMakeLists.txt
# ChatSession + CancellationRegistry PDK lift (chat-session-pdk-lift Change 1)
# 关联: docs/superpowers/specs/2026-09-11-chat-session-pdk-lift-design.md §4

# 2026-09-14 实施偏差: plan 原设计 OBJECT 库 (pdk_chat_session_obj)。
# 实测 OBJECT→OBJECT 链接不传递对象文件到最终消费者
# (undefined reference to hydraforge::pdk::CancellationRegistry::*)。
# 改为 STATIC 库: target_link_libraries 传递语义可靠, 与 agenticdsl_common 同模式。
add_library(pdk_chat_session STATIC
    src/cancellation_registry.cpp
)

target_include_directories(pdk_chat_session PUBLIC
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)

target_link_libraries(pdk_chat_session PUBLIC
    agenticdsl_common
)

target_compile_features(pdk_chat_session PUBLIC cxx_std_20)

# 被 PDK .so / 宿主二进制链接, 需 PIC
set_target_properties(pdk_chat_session PROPERTIES POSITION_INDEPENDENT_CODE ON)
