// agenticdsl/contract/itool_registry.h
// 文件头注释
// 功能描述：IToolRegistry 抽象接口 — Phase 1 P1 引擎解耦 (ADR-0019 §1.4 退出标准)
//          替代 engine.h 直接 include common/tools/registry.h
//          工具注册表抽象：屏蔽 ToolRegistry 完整类型依赖
// 设计依据：ADR-0004 (ToolRegistry 安全模型) + ADR-0019 §1.4 + ADR-0023 §C.3
//          + openspec/changes/2026-06-15-residual-engine-h-decoupling T2
// 作者：AgenticDSL Phase 1 P1.T2
// 最后修改日期：2026-06-18
#pragma once

#include "common/llm/llm_tool.h"       // ILLMTool 定义 (T2 hotfix: 实际在 llm_tool.h, 不是 llm_types.h)
#include "common/llm/llm_types.h"      // LLMParams / Result
#include "common/policy/execution_policy.h"  // ToolMetadata definition (P1.T2)

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace agenticdsl {

/**
 * @brief 工具注册表抽象接口 (ADR-0023 §C.3 标准化)
 *
 * 包含 9 个虚函数 (P1.T2 v3 修订, 移除未使用的 has_cost_callback, YAGNI):
 *
 * 基础查询 (3):
 *   - has_tool(name) const
 *   - call_tool(name, args) -> json
 *   - list_tools() const
 *
 * 函数工具注册 (1, 模板桥接):
 *   - register_tool_function(name, std::function<json(args)>)
 *     (替代 v2 的 register_tool 模板 — C++ 禁止模板虚函数)
 *
 * LLM 工具管理 (4):
 *   - register_llm_tool(name, tool, default_params)
 *   - is_llm_tool(name) const
 *   - get_llm_params(name) const
 *   - call_llm_tool(name, prompt, params) -> json
 *
 * 成本回调 (1):
 *   - set_cost_callback(cb)
 *
 * 不在接口 (9 之外的 ToolRegistry 公共方法):
 *   - register_tool (template) — 模板无法 virtual, 改用 register_tool_function 桥接
 *   - has_cost_callback — 零调用点, YAGNI 移除
 *
 * 实现类:
 *   - ToolRegistry (src/common/tools/registry.h) — 主实现
 *   - SecureToolRegistry (include/agenticdsl/tools/secure_tool_registry.h) —
 *     委托式多继承 (v3 修订, 保持现有 registry_ref_ 成员, 9 override 委托到 wrapped ToolRegistry)
 */
class IToolRegistry {
 public:
  // === Cost tracking 回调签名 ===
  // 阶段 4 任务 4.2: 在 call_llm_tool 成功后, 回调此钩子
  using CostCallback = std::function<void(int tokens, const std::string& model)>;

  // === 函数工具类型别名 (替代 v2 模板 register_tool) ===
  // 桥接: ToolRegistry::register_tool<Func> 内部委托到 register_tool_function(this, fn)
  using ToolFunc = std::function<nlohmann::json(
      const std::unordered_map<std::string, std::string>&)>;

  virtual ~IToolRegistry() = default;

  // === 基础查询 (3) ===

  virtual bool has_tool(const std::string& name) const = 0;
  virtual nlohmann::json call_tool(
      const std::string& name,
      const std::unordered_map<std::string, std::string>& args) = 0;
  virtual std::vector<std::string> list_tools() const = 0;

  // === 函数工具注册 (1, 模板桥接) ===

  /**
   * @brief 注册函数工具 (类型擦除 std::function 形式)
   * @param name 工具名称
   * @param fn 工具实现 (类型擦除后的 std::function)
   *
   * 替代 ToolRegistry::register_tool<Func> 模板成员函数 (C++ 禁止模板虚函数).
   * ToolRegistry::register_tool<Func> 内部委托到 register_tool_function(name, fn).
   */
  virtual void register_tool_function(std::string name, ToolMetadata meta, ToolFunc fn) = 0;

  // === LLM 工具管理 (4) ===

  virtual void register_llm_tool(
      std::string name,
      std::unique_ptr<ILLMTool> tool,
      const LLMParams& default_params = {}) = 0;
  virtual bool is_llm_tool(const std::string& name) const = 0;
  virtual const LLMParams& get_llm_params(const std::string& name) const = 0;
  virtual nlohmann::json call_llm_tool(
      const std::string& name,
      const std::string& prompt,
      const LLMParams& params = {}) = 0;

  // === 成本回调 (1) ===

  /**
   * @brief 设置成本跟踪回调
   * @param cb 回调 (空回调可禁用成本跟踪)
   *
   * 调用方负责保证 callback 生命周期. Engine 在每次 run() 启动时设置一次.
   */
  virtual void set_cost_callback(CostCallback cb) = 0;

  // === 故意省略 ===
  // - has_cost_callback() const: 零调用点 (YAGNI 移除)
  // - register_tool<Func>(name, func): 模板无法 virtual, 改用 register_tool_function 桥接
};

}  // namespace agenticdsl
