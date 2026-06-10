#ifndef AGENTICDSL_COMMON_TOOLS_REGISTRY_H
#define AGENTICDSL_COMMON_TOOLS_REGISTRY_H

// 文件头注释
// 功能描述：工具注册表 —— 支持函数工具和 LLM 工具两种类型
//          新增 cost tracking 回调钩子（阶段 4 任务 4.2）
// 作者：tech-debt-and-doc-cleanup change
// 最后修改日期：2026-06-10

#include <memory>
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

#include <nlohmann/json.hpp>

#include "common/llm/llm_tool.h"

namespace agenticdsl {

class ToolRegistry {
public:
    // === Cost tracking 回调签名 ===
    // 阶段 4 任务 4.2: 在 call_llm_tool 成功后，回调此钩子
    // 回调实现位于 BudgetController（详见 record_llm_call）
    using CostCallback = std::function<void(int tokens, const std::string& model)>;

    ToolRegistry();

    template<typename Func>
    void register_tool(std::string name, Func&& func) {
        tools_[std::move(name)] = std::forward<Func>(func);
    }

    bool has_tool(const std::string& name) const;
    nlohmann::json call_tool(const std::string& name, const std::unordered_map<std::string, std::string>& args);
    std::vector<std::string> list_tools() const;

    // LLM tool methods
    void register_llm_tool(std::string name, std::unique_ptr<ILLMTool> tool, const LLMParams& default_params = {});
    bool is_llm_tool(const std::string& name) const;
    const LLMParams& get_llm_params(const std::string& name) const;
    nlohmann::json call_llm_tool(const std::string& name, const std::string& prompt, const LLMParams& params = {});

    // === 阶段 4 任务 4.2: 设置成本跟踪回调 ===
    // 注入外部回调（在 DSLEngine 中绑定到 BudgetController::record_llm_call）。
    // 传入空回调可禁用成本跟踪。Engine 在每次 run() 启动时设置一次。
    void set_cost_callback(CostCallback cb) { cost_callback_ = std::move(cb); }

    // 是否有 cost callback 已设置（用于测试与诊断）
    bool has_cost_callback() const { return static_cast<bool>(cost_callback_); }

private:
    void register_default_tools();
    std::unordered_map<
        std::string,
        std::function<nlohmann::json(const std::unordered_map<std::string, std::string>&)>
    > tools_;

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
