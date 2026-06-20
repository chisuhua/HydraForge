// include/agenticdsl/pdk/pdk.h
// 文件头注释
// 功能描述：PDK 统一入口 (Sprint 4 实施, ADR-0021)。
//          引用 3 个 PDK 子头: tool_macros.h + agent_macros.h + safe_exec.h。
//          插件开发者使用: #include <agenticdsl/pdk/pdk.h> 即可访问全部 PDK。
//          (Phase 2 拆分至独立 hydraforge-pdk 仓库后, 也可使用 #include <hydraforge/pdk/pdk.h>,
//           pdk/include/hydraforge/pdk/pdk.h 内部 forward 到本文件)
// 设计依据：ADR-0021 §2.2 + openspec/changes/2026-07-07-pdk-skeleton
// 作者：AgenticDSL Phase 1 Sprint 4
// 最后修改日期：2026-06-19

#pragma once

#include <agenticdsl/pdk/tool_macros.h>
#include <agenticdsl/pdk/agent_macros.h>
#include <agenticdsl/pdk/safe_exec.h>

// PDK 版本 (供 Runtime/Plugin 编译时识别)
#define HYDRAFORGE_PDK_VERSION_MAJOR 0
#define HYDRAFORGE_PDK_VERSION_MINOR 1
#define HYDRAFORGE_PDK_VERSION_PATCH 0
#define HYDRAFORGE_PDK_VERSION "0.1.0"