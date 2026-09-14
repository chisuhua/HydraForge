// chat_session.h — 1 Sprint 兼容 shim (chat-session-pdk-lift Change 1)
//
// 实现已提升到 PDK: include/agenticdsl/pdk/chat_session.h (namespace hydraforge::pdk)。
// 本文件保留为转发 shim, 把 pdk_chat_demo::* 旧名 alias 到新 PDK 类型,
// 避免一次性断链 16 个既有 includer (main.cpp / 3 个 commands / 12 个测试)。
//
// 兼容期: 1 个 Sprint。到期后删除本文件 + 全量迁移到 hydraforge::pdk。

#pragma once

#include <agenticdsl/pdk/chat_session.h>

namespace pdk_chat_demo {

using hydraforge::pdk::AgentConfig;
using hydraforge::pdk::ChatConfig;
using hydraforge::pdk::ChatResult;
using hydraforge::pdk::ChatSession;
using hydraforge::pdk::InputMessage;
using hydraforge::pdk::ObservabilityConfig;
using hydraforge::pdk::PluginConfig;
using hydraforge::pdk::QueueKind;
using hydraforge::pdk::SessionConfig;

}  // namespace pdk_chat_demo
