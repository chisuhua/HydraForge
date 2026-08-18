// src/common/hooks/env_validation_hook.h
// 功能描述：EnvValidationHook — ToolCoordinator pre-hook (ADR-0075 D5 / C13)
// 设计依据：ADR-0069 §决策 D3 hook 顺序 (pre-hook 在 ApprovalHandler 之前)
//          + ADR-0075 §不变量 6 (dangerous 类工具必走, 不允许 hook 旁路)
// 适配说明：ADR-0069 hook 体系为 IToolHookRegistry + PreHook std::function,
//          本 hook 以工厂函数形态产出 PreHook lambda。
//          ToolCategory 无 "Dangerous" 枚举值, 实际目标为 ToolCategory::Execute
//          (shell.exec 类工具的对应类目)。
// 作者：from-roadmap-phase-6c-execution-envbackend change
// 最后修改日期：2026-08-18
#pragma once

#include "agenticdsl/contract/itool_hook_registry.h"
#include "agenticdsl/policy/backend_policy.h"

namespace agenticdsl {

/// @brief 构造 env validation pre-hook (ADR-0075 D5)
///
/// 校验逻辑 (仅对 ToolCategory::Execute 工具生效, 其他类目 Continue):
///  1. backend spec (args["backend"], 默认 "local") 命中 BackendPolicy, 未知 → Deny
///  2. docker 镜像 allowlist (policy.image_allowlist 非空时强制)
///  3. env 白名单: args 中 "env.<NAME>" 键, 不在 allowed_env_vars →
///     Deny("env var not allowed: <NAME>")
///  4. working_dir 白名单: args["working_dir"] 前缀不匹配 allowed_paths →
///     Deny("working_dir not allowed: <path>")
///  5. approval gate: policy.requires_approval && args["__approved"] != "true" →
///     Deny("Backend policy requires approval")
///
/// 注册方式:
///   hooks.register_pre_hook("*", make_env_validation_hook(config), 0,
///                           HookErrorPolicy::FailClosed);
PreHook make_env_validation_hook(BackendConfig config);

}  // namespace agenticdsl
