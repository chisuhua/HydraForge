// src/core/types/tool_result.cpp
// 文件头注释
// 功能描述：ToolResult 的方法实现（success/error 工厂、JSON 序列化/反序列化）。
// 设计依据：ADR-0023（ToolResult 标准化）+ plan §3 X 阶段。
// 作者：AgenticDSL Phase 0 / Track X
// 最后修改日期：2026-06-08
#include "core/types/tool_result.h"

namespace agenticdsl {

ToolResult ToolResult::success(nlohmann::json d, nlohmann::json m) {
  ToolResult r;
  r.ok = true;
  r.data = std::move(d);
  r.meta = std::move(m);
  return r;
}

ToolResult ToolResult::error(std::string code,
                             std::string msg,
                             nlohmann::json m) {
  ToolResult r;
  r.ok = false;
  // 将错误码 / 错误消息注入 meta（而非 data），保证 data 保留工具的原始输出语义
  if (!m.is_object()) {
    m = nlohmann::json::object();
  }
  m["error_code"] = std::move(code);
  m["error_message"] = std::move(msg);
  r.meta = std::move(m);
  // data 保持空对象（默认构造）
  return r;
}

nlohmann::json ToolResult::to_json() const {
  nlohmann::json j;
  j["ok"] = ok;
  j["data"] = data;
  j["meta"] = meta;
  return j;
}

ToolResult ToolResult::from_json(const nlohmann::json& j) {
  ToolResult r;
  // 缺失字段使用默认值（ok=false，data/meta 为空对象）
  if (j.is_object()) {
    if (j.contains("ok") && j["ok"].is_boolean()) {
      r.ok = j["ok"].get<bool>();
    }
    if (j.contains("data")) {
      r.data = j["data"];
    }
    if (j.contains("meta")) {
      r.meta = j["meta"];
    }
  }
  // 非对象 JSON：返回默认（ok=false）
  return r;
}

} // namespace agenticdsl