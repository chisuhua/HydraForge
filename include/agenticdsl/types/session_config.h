// agenticdsl/types/session_config.h
// 功能描述：SessionConfig 结构体 — 创建 Session 时的配置参数
// 设计依据：C11 proposal (Phase 5 Stage 1 Step 1) + Oracle Risk 7 mitigation
// 作者：AgenticDSL Phase 5 / Sprint 20 C11
// 最后修改日期：2026-07-04
#pragma once

#include <cstdint>
#include <string>

#include "common/policy/policy_factory.h"  // PolicyMode

namespace agenticdsl {

/**
 * @brief Session 创建配置
 *
 * Oracle 预留：schema_version 为未来 Stage 2/3 Session 序列化迁移预留版本戳。
 */
struct SessionConfig {
  std::string name;                         // Session 名称（便于日志/审计）
  uint32_t max_concurrent_tasks = 4;        // 最大并发 TaskSession 数
  uint32_t timeout_ms = 30000;              // 操作超时（毫秒）
  PolicyMode policy_mode = PolicyMode::Agent;  // 默认执行策略
  uint32_t schema_version = 1;              // 序列化版本戳（Oracle: 未来迁移预留）
  uint32_t compact_threshold_tokens = 4096;  // 上下文压缩阈值（0=禁用）
};

}  // namespace agenticdsl