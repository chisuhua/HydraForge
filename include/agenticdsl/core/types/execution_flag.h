// include/agenticdsl/core/types/execution_flag.h
// Wave 2 P0 (2026-09-17): ExecutionFlag enum — 控制 DSLEngine::run 的行为模式
// Oracle M3: 单 flag 枚举 (非 bitmask), YAGNI 防止过度设计
// Oracle M2: enum class → 整数转换需 static_cast, helper 包装转换
#pragma once
#include <cstdint>

namespace agenticdsl {

enum class ExecutionFlag : uint8_t {
    None              = 0,
    Autonomous        = 1,   // child run: 禁用 DSL_CALL pause, 让 loop 完整执行
    ResumeFromPause   = 2,   // future: 恢复暂停 (本 change 不实现)
};

// Oracle M2: 包装 enum class → int 转换, 避免 `flags & ExecutionFlag::Autonomous` 类型不匹配
inline bool has_autonomous_flag(int flags) {
    return (flags & static_cast<int>(ExecutionFlag::Autonomous)) != 0;
}

}  // namespace agenticdsl