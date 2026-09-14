#pragma once

#include <memory>

#include <agenticdsl/pdk/cancellation_registry.h>

namespace pdk_chat_demo {

// chat-session-pdk-lift Change 1: 实现已提升到 PDK; 此处保留 1 Sprint 兼容 alias
// (原为全局 `class CancellationRegistry;` 前置声明)。
using CancellationRegistry = hydraforge::pdk::CancellationRegistry;

// §4.0.1: shared CancellationRegistry across ChatSession + loop_agent
// Solves C1 (token identity mismatch when ChatSession owns one registry
// and loop_agent owns another). nullptr = "non-cancellable-but-executable"
// fallback for test binaries that don't initialize the global.
extern std::shared_ptr<CancellationRegistry> g_cancellation_registry;

}  // namespace pdk_chat_demo
