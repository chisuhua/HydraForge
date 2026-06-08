// src/core/types/tool_result.h
// 文件头注释
// 功能描述：工具执行结果的标准信封（envelope）数据结构。
//          Phase 0 X 阶段产物：定义 ToolResult 的 MVP 形态，
//          作为 Cognitive（B）与 InteractionBus（A）的共同契约前置。
// 设计依据：ADR-0023（ToolResult 标准化）+ plan §3 X 阶段。
// 作者：AgenticDSL Phase 0 / Track X
// 最后修改日期：2026-06-08
#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <utility>

namespace agenticdsl {

// 工具执行结果的标准信封（MVP）
//
// 设计要点：
// - ok 字段是唯一的状态判定依据（bool flag），data/meta 不参与判定。
// - data 用于承载工具的实际输出（结构由工具自身约定）。
// - meta 用于承载错误码、错误消息、追踪信息等元数据。
// - error() 工厂会将 error_code / error_message 自动注入 meta，
//   避免与 data 混淆（data 保留工具的原始输出）。
//
// 注意：本阶段不引入 schema 版本号、不添加 trace_id/timestamp 等 Phase 1 字段，
// 以避免在 B/A 阶段的接口迁移前过早固化结构。
struct ToolResult {
  bool ok = false;
  nlohmann::json data = nlohmann::json::object();
  nlohmann::json meta = nlohmann::json::object();

  /**
   * @brief 构造成功结果
   * @param d 工具输出数据（移动语义）
   * @param m 可选元数据（移动语义）
   * @return ok=true 的 ToolResult
   */
  static ToolResult success(nlohmann::json d, nlohmann::json m = nlohmann::json::object());

  /**
   * @brief 构造失败结果
   * @param code 错误码（如 "ERR_LLM.NETWORK"）
   * @param msg 错误消息
   * @param m 可选额外元数据（移动语义）
   * @return ok=false 的 ToolResult，其中 meta 中会注入 error_code / error_message
   */
  static ToolResult error(std::string code,
                          std::string msg,
                          nlohmann::json m = nlohmann::json::object());

  /**
   * @brief 序列化为 JSON 对象
   * @return 包含 ok/data/meta 三个字段的 JSON
   */
  nlohmann::json to_json() const;

  /**
   * @brief 从 JSON 反序列化（缺失字段使用默认值）
   * @param j 输入 JSON
   * @return 重建的 ToolResult
   */
  static ToolResult from_json(const nlohmann::json& j);
};

} // namespace agenticdsl