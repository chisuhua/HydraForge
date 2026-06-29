// src/common/policy/mode_switch_dialog.h
// 功能描述：ModeSwitchDialog — 模式切换用户确认 (YOLO 切换必须确认)
// 设计依据：ADR-0031 §决策 6 (Oracle session ses_0ee867023ffeaSqWQXET5ESbAo)
// 作者：AgenticDSL Phase3 / Sprint 13 C3 ship
// 最后修改日期：2026-07-31
#pragma once

#include <functional>
#include <iosfwd>
#include <string>

namespace agenticdsl {

/**
 * @brief 用户输入回调 (用于测试注入, 默认从 stdin 读)
 *
 * 返回 true 表示用户输入 "y/Y/yes", false 表示其他.
 * 默认实现: read_user_input_stdin (生产环境使用)
 */
using UserInputFn = std::function<bool(const std::string& prompt)>;

/**
 * @brief 默认 stdin 用户输入 (生产环境)
 */
bool read_user_input_stdin(const std::string& prompt);

/**
 * @brief 确认 YOLO 模式切换 (ADR-0031 §决策 6)
 *
 * 当 from_mode 或 to_mode 涉及 Yolo 时, 必须调用此函数获取用户确认.
 * 显示:
 *   ⚠️  SWITCH TO YOLO MODE
 *   YOLO mode auto-approves ALL tools except force_approval_always.
 *   Continue? [y/N]:
 *
 * @param from_mode 切换前的模式名 ("plan" / "agent" / "yolo")
 * @param input 用户输入回调 (默认 read_user_input_stdin)
 * @return true=confirmed, false=denied
 */
bool confirm_yolo_switch(const std::string& from_mode,
                         UserInputFn input = read_user_input_stdin);

/**
 * @brief 检查切换是否需要 YOLO 确认
 *
 * 规则 (ADR-0031 §决策 6):
 * - Plan ↔ Agent: 无需确认 (return false)
 * - 涉及 Yolo 的切换: 必须确认 (return true)
 */
bool requires_yolo_confirmation(const std::string& from_mode,
                                const std::string& to_mode);

}  // namespace agenticdsl