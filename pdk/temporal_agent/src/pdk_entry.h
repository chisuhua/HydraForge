// pdk/temporal_agent/src/pdk_entry.h
// 功能描述：Temporal Agent Plugin 入口头文件
//          声明 AgentDescriptor 结构 + get_agent_descriptor() + pdk_register_agent 导出
//          供测试包含验证 agent 注册元数据
// 设计依据：openspec/changes/pkgm-temporal-agent/tasks.md §7.7
//          .rddf/plans/pkgm-temporal-agent.md Task 7
// 作者：pkgm-temporal-agent Phase 2
// 最后修改日期：2026-07-28

#pragma once

#include <string>
#include <vector>

namespace pdk_temporal_agent {

struct AgentDescriptor {
  std::string name;
  std::vector<std::string> capabilities;
  std::string version;
};

AgentDescriptor get_agent_descriptor();

}  // namespace pdk_temporal_agent

extern "C" pdk_temporal_agent::AgentDescriptor* pdk_register_agent();
