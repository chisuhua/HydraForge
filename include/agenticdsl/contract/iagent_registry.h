// include/agenticdsl/contract/iagent_registry.h
// 功能描述：Agent first-class registry L3 契约 (ADR-0082 §决策 7)
//
//          设计要点：
//          - per-engine 注册粒度 (与 ADR-0022 ToolRegistry 对齐)
//          - 字符串 ID (与 PluginInfo::name 对齐，避免双 ID 系统)
//          - 4 个核心 API：register / create / unregister / list
//          - register_agent 返回 false（不抛）— 17 ErrorCode 中无 AlreadyRegistered
//          - C5 决议：与 ADR-0069 tool hook + ADR-0081 agent hook 三层正交
//
//          Amendment (2026-08-21)：register 返回 bool 而非抛异常（Metis 修正，
//          tool_result.h 17 枚举无 AlreadyRegistered，错误由调用方通过返回值判断）。
//
// 设计依据：ADR-0082 §决策 7 + ADR-0022 per-engine 注册粒度 + ADR-0080 v1.1 ship + ADR-0079 v1.2 ship
// 作者：HydraForge Sprint 22 / adr-0082-promote-to-approved
// 最后修改日期：2026-08-21

#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace agenticdsl {

// Agent 实例最小抽象接口（V1 最小骨架）
//
// 注：V1 不展开完整 Agent 接口（如 run / state / lifecycle 事件发射等），
// 完整接口定义留给"Agent hook 实施"独立 OpenSpec change（Sprint 24+）。
class IAgent {
 public:
  virtual ~IAgent() = default;

  // Agent 类型标识 (e.g. "react-loop-v1")
  virtual const std::string& name() const = 0;

  // Agent 实例唯一 ID (per-engine 唯一，由 create() 生成)
  virtual const std::string& id() const = 0;
};

// Agent 创建配置 (V1 最小集)
// 完整字段（loop_type / llm_provider / max_spawn_depth 等）留给实施阶段
struct AgentConfig {
  std::string instance_id;     // 实例 ID（若空，create() 自动生成）
};

// Agent 工厂函数签名
// create(string_id, config) 时调用，返回 IAgent 实例
using AgentFactory = std::function<std::unique_ptr<IAgent>(const AgentConfig&)>;

/**
 * * Agent first-class registry L3 契约
 *
 * C2 决议：per-engine 注册粒度（与 ADR-0022 对齐）。
 * C4 决议：plugin 形态为主，subprocess 形态 Phase 2。
 *
 * 线程模型由实现决定（V1: shared_mutex，read-shared / write-exclusive）。
 */
class IAgentRegistry {
 public:
  virtual ~IAgentRegistry() = default;

  // 注册 agent 类型 (C1 决议: string_id 与 PluginInfo::name 对齐)
  // 重复注册同一 string_id：返回 false，不静默覆盖，不抛异常
  // （Amendment 2026-08-21：Metis 修正，17 ErrorCode 无 AlreadyRegistered）
  virtual bool register_agent(const std::string& string_id,
                              AgentFactory factory) = 0;

  // 创建 agent 实例
  // string_id 未注册：返回 nullptr（V1 行为）
  virtual std::unique_ptr<IAgent> create(const std::string& string_id,
                                          const AgentConfig& config) = 0;

  // 注销 agent 类型
  // string_id 不存在：返回 false；存在：标记 pending unregister，
  // 实例仍运行时不立即删除（避免 use-after-free），简单实现为 V1 同步删除
  // （Amendment 2026-08-21：V1 简化，pending 语义留 Sprint 24+）
  virtual bool unregister(const std::string& string_id) = 0;

  // 列出所有已注册 string_id
  virtual std::vector<std::string> list_registered() const = 0;

  // 查询 string_id 是否已注册
  virtual bool is_registered(const std::string& string_id) const = 0;

  // 已注册类型数量
  virtual size_t size() const = 0;
};

// 工厂函数：创建 InMemory 参考实现（与 ToolRegistry 模式一致）
// 由 src/core/agent_registry.cpp 实现
std::unique_ptr<IAgentRegistry> make_in_memory_agent_registry();

// 测试 helper：创建 mock IAgent 实例（暴露给 test_agent_registry.cpp）
// 由 src/core/agent_registry.cpp 实现
std::unique_ptr<IAgent> make_mock_agent_for_test(const std::string& name,
                                                 const std::string& id);

}  // namespace agenticdsl