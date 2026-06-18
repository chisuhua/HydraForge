#ifndef AGENTICDSL_COMMON_TOOLS_REGISTRY_H
#define AGENTICDSL_COMMON_TOOLS_REGISTRY_H

// 文件头注释
// 功能描述：工具注册表 —— 支持函数工具和 LLM 工具两种类型
//          新增 cost tracking 回调钩子（阶段 4 任务 4.2）
//          Phase 1 P1.T2 (2026-06-18): 实现 IToolRegistry 抽象接口 (9 override)
// 作者：tech-debt-and-doc-cleanup change + Phase 1 P1
// 最后修改日期：2026-06-18 [Phase 1 P1.T2: IToolRegistry 集成]

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

#include <nlohmann/json.hpp>

#include "agenticdsl/contract/itool_registry.h"  // P1.T2: IToolRegistry 抽象接口
#include "common/llm/llm_tool.h"

namespace agenticdsl {

/**
 * @brief 工具注册表主实现
 *
 * Phase 1 P1.T2 (2026-06-18): 实现 IToolRegistry 9 虚函数.
 * register_tool<Func> 模板保持 (非虚), 内部委托到 register_tool_function 虚函数.
 */
class ToolRegistry : public IToolRegistry {
public:
    // === Cost tracking 回调签名 (从 IToolRegistry 继承 CostCallback) ===
    using CostCallback = IToolRegistry::CostCallback;
    using ToolFunc = IToolRegistry::ToolFunc;

    ToolRegistry();

    // === 函数工具注册 (模板) ===
    // P1.T2: 模板成员函数保持 (不 virtual), 内部委托到 register_tool_function 虚函数
    template<typename Func>
    void register_tool(std::string name, Func&& func) {
        // 类型擦除后委托到虚函数
        ToolFunc erased = [fn = std::forward<Func>(func)](
            const std::unordered_map<std::string, std::string>& args) -> nlohmann::json {
            return fn(args);
        };
        register_tool_function(std::move(name), std::move(erased));
    }

    // === IToolRegistry 9 虚函数实现 (P1.T2 新增) ===

    // 基础查询 (3)
    bool has_tool(const std::string& name) const override;
    nlohmann::json call_tool(
        const std::string& name,
        const std::unordered_map<std::string, std::string>& args) override;
    std::vector<std::string> list_tools() const override;

    // 函数工具注册 (1, 模板桥接)
    void register_tool_function(std::string name, ToolFunc fn) override;

    // LLM 工具管理 (4)
    void register_llm_tool(
        std::string name,
        std::unique_ptr<ILLMTool> tool,
        const LLMParams& default_params = {}) override;
    bool is_llm_tool(const std::string& name) const override;
    const LLMParams& get_llm_params(const std::string& name) const override;
    nlohmann::json call_llm_tool(
        const std::string& name,
        const std::string& prompt,
        const LLMParams& params = {}) override;

    // 成本回调 (1)
    void set_cost_callback(CostCallback cb) override { cost_callback_ = std::move(cb); }

    // === 不在 IToolRegistry 的 ToolRegistry 公共方法 (保持兼容) ===
    // has_cost_callback: 零调用点 (YAGNI 移除, 但 ToolRegistry 仍可作为诊断 API 保留)
    bool has_cost_callback() const { return static_cast<bool>(cost_callback_); }

private:
    void register_default_tools();
    std::unordered_map<std::string, ToolFunc> tools_;

    // LLM tool storage
    struct LLMToolEntry {
        std::unique_ptr<ILLMTool> tool;
        LLMParams default_params;
    };
    std::unordered_map<std::string, LLMToolEntry> llm_tools_;

    // 成本跟踪回调（由 DSLEngine 在 run() 时注入）
    CostCallback cost_callback_;
};

} // namespace agenticdsl

#endif // COMMON_TOOLS_REGISTRY_H
