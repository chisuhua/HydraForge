# src/common/contract/CMakeLists.txt
# 功能描述：A 轨道（Track 0.3）contract 模块构建配置
#          Phase 1 预留 IInteractionBus + InMemoryBus
# 设计依据：plan §12-14
# 作者：AgenticDSL Phase 0 / Track A
# 最后修改日期：2026-06-08

add_library(agenticdsl_contract STATIC
    inmemory_bus.cpp
)

target_link_libraries(agenticdsl_contract PUBLIC
    agenticdsl_includes
    agenticdsl_common
)