#ifndef AGENTICDSL_COMMON_UTILS_TEMPLATE_RENDERER_H
#define AGENTICDSL_COMMON_UTILS_TEMPLATE_RENDERER_H

#include "core/types/context.h" // 引入 Context (nlohmann::json)
#include "agenticdsl/types/layered_context.h" // 引入 LayeredContext (Stage 3 / Task 13)
#include <inja/inja.hpp>
#include <string>
#include <string_view>
#include <filesystem> // Required by Inja for set_include_callback

namespace agenticdsl {

class InjaTemplateRenderer {
public:
    InjaTemplateRenderer();

    // 静态方法：使用默认环境渲染模板
    static std::string render(std::string_view template_str, const Context& context);

    // 实例方法：使用当前实例的环境渲染模板
    std::string render_with_env(std::string_view template_str, const Context& context);

    // ============================================================
    // LayeredContext 重载 (Stage 3 / Task 13 新增)
    // 内部调用 agenticdsl::flatten() 将 5-层结构拍平为 inja 可用的 JSON 对象。
    // 与上面的 Context 重载共存 (类型不同, 无歧义), 旧调用方不受影响。
    // ============================================================

    /**
     * @brief 从 LayeredContext 渲染 inja 模板
     * @param template_str 模板字符串, 支持 {{ var }}
     * @param context 5-层结构化上下文 (L1-L5)
     * @return 渲染结果
     */
    static std::string render(std::string_view template_str,
                              const LayeredContext& context);

    /**
     * @brief 实例方法版本, 使用当前实例环境从 LayeredContext 渲染
     */
    std::string render_with_env(std::string_view template_str,
                                const LayeredContext& context);

private:
    inja::Environment env_;
    void configure_security(); // 配置 Inja 环境以禁用不安全操作
};

} // namespace agenticdsl

#endif // AGENTICDSL_COMMON_UTILS_TEMPLATE_RENDERER_H
