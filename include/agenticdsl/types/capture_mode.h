// include/agenticdsl/types/capture_mode.h
// 功能描述: CaptureMode 三态枚举（Off / Online / Training）
// 设计依据: docs/adr/adr-0080-v1-2-amendment-d10-decouple.md
//          + openspec/changes/capture-mode-and-distillation-writer-v1/spec.md
// 关键不变量:
//   - Off = 0 (生产路径, 零捕获开销)
//   - Online = 1 (pdk_chat_demo 默认, 实时观测无 PII 风险)
//   - Training = 2 (离线蒸馏, 三重保护 + WARNING)
//   - 强类型 enum class (禁止魔法值/字符串)
//   - uint8_t 底层类型稳定（JSONL header 持久化需求）
// 作者：AgenticDSL / capture-mode-and-distillation-writer-v1 Phase 0
// 最后修改日期：2026-08-29
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace agenticdsl {

enum class CaptureMode : uint8_t {
  Off = 0,
  Online = 1,
  Training = 2,
};

constexpr CaptureMode kDefaultCaptureMode = CaptureMode::Off;

inline std::string to_string(CaptureMode m) {
  switch (m) {
    case CaptureMode::Off: return "Off";
    case CaptureMode::Online: return "Online";
    case CaptureMode::Training: return "Training";
  }
  throw std::invalid_argument("Unknown CaptureMode value");
}

inline CaptureMode parse_capture_mode(const std::string& s) {
  if (s == "Off") return CaptureMode::Off;
  if (s == "Online") return CaptureMode::Online;
  if (s == "Training") return CaptureMode::Training;
  throw std::invalid_argument(
      "Invalid CaptureMode string: '" + s +
      "' (expected 'Off', 'Online', or 'Training', case-sensitive)");
}

}  // namespace agenticdsl
