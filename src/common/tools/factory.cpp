// src/common/tools/factory.cpp
#include "common/tools/factory.h"
#include "common/tools/registry.h" // 完整类型仅工厂实现 TU 可见

namespace agenticdsl::tools {

std::unique_ptr<IToolRegistry> create_tool_registry() {
    return std::make_unique<ToolRegistry>();
}

} // namespace agenticdsl::tools