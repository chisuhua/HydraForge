// cancellation_registry.h — 1 Sprint 兼容 shim (chat-session-pdk-lift Change 1)
//
// 实现已提升到 PDK: include/agenticdsl/pdk/cancellation_registry.h (namespace hydraforge::pdk)。
// 本文件保留为转发 shim, 把全局名 `CancellationRegistry` alias 到新 PDK 类,
// 避免一次性断链 12 个既有引用点 (chat_session.h / cancellation_globals.cpp /
// 4 个 examples 测试 / pdk/loop_agent pdk_entry.cpp)。
//
// 兼容期: 1 个 Sprint。到期后删除本文件 + 全量迁移到 hydraforge::pdk::CancellationRegistry。

#pragma once

#include <agenticdsl/pdk/cancellation_registry.h>

using hydraforge::pdk::CancellationRegistry;
