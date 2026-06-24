// src/common/tools/factory.h
// 功能描述：ToolRegistry 工厂函数
//          为 engine.cpp 等调用方提供不依赖 registry.h 完整类型的构造入口
// 设计依据：ADR-0019 §1.4 (P1.T4 tool_registry_ PIMPL-lite 解耦的延伸)
// 作者：2026-06-24-engine-include-final-decoupling
// 最后修改日期：2026-06-24

#pragma once

#include <memory>

namespace agenticdsl {

class IToolRegistry;

namespace tools {

/**
 * @brief 创建默认 ToolRegistry 实现
 * @return unique_ptr<IToolRegistry> 指向 ToolRegistry 实例
 *
 * 工厂返回抽象接口，调用方无需 include common/tools/registry.h 完整类型。
 */
std::unique_ptr<IToolRegistry> create_tool_registry();

} // namespace tools
} // namespace agenticdsl