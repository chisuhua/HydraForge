// dsl_validator.h - PDK Chat Demo DSL Schema 校验器
// 关联: openspec/changes/pdk-chat-demo-v1-recap/design.md §T2
// 引用 ADR-0058 (tool-schema-validation) — 非重叠：
//   ADR-0058 校验 tool input/output schema (call_tool 执行路径)
//   本校验器验证 .agent.md DSL 图结构 (解析前)
// 作者: Sisyphus (OhMyOpenCode), 2026-07-27
// 更新: 2026-07-30 — 添加 ToolRegistry 可选参数支持 MISSING_TOOL_DEPENDENCY 检查

#pragma once

#include <string>
#include <vector>

namespace agenticdsl {
class IToolRegistry;
}

namespace pdk_chat_demo {

// ============================================================
// 校验错误结构
// ============================================================
struct ValidationError {
  std::string type;       // 错误类型: MISSING_REQUIRED_FIELD / INVALID_NODE_TYPE / MISSING_TOOL_DEPENDENCY / ...
  std::string node_path;   // 节点路径或字段名
  std::string message;     // 人类可读描述
};

// ============================================================
// 校验结果
// ============================================================
struct ValidationResult {
  bool valid = true;
  std::vector<ValidationError> errors;

  // 无错误时快速构造
  static ValidationResult ok() { return {true, {}}; }

  // 追加错误（非 fail-fast，收集所有错误）
  void add_error(const std::string& type, const std::string& path,
                 const std::string& msg) {
    valid = false;
    errors.push_back({type, path, msg});
  }
};

// ============================================================
// DSL Schema 校验器 (example 侧，不修改 src/modules/parser/)
// ============================================================
// 职责：
//   - 必填字段检查: name, version, agent_loop
//   - 节点类型白名单: start, end, call_tool, llm_generate, condition,
//                      fork, join, assign, resource
//   - 工具依赖完整性: call_tool 节点的 tool_name 存在（若提供 registry）
// 不负责：
//   - DAG 循环检测（topo_scheduler.cpp:588 执行时已有等价检查）
//   - Tool input/output schema 校验（ADR-0058 负责）
//
// 使用示例：
//   DslValidator validator;
//   auto result = validator.validate(markdown_content);
//   // 或带 registry 检查工具依赖:
//   auto result = validator.validate(markdown_content, &registry);
//   if (!result.valid) {
//     for (auto& e : result.errors)
//       std::cerr << e.type << ": " << e.message << std::endl;
//   }
class DslValidator {
 public:
  DslValidator() = default;

  // 校验 .agent.md Markdown 内容
  // @param markdown_content 完整的 .agent.md 文件内容
  // @param registry 可选；非 nullptr 时检查 call_tool 节点的 tool_name 是否注册
  // @return ValidationResult (valid=true 表示通过)
  ValidationResult validate(const std::string& markdown_content,
                            const agenticdsl::IToolRegistry* registry = nullptr);

 private:
  // 解析 Markdown frontmatter（**key**: value 格式）
  std::string extract_frontmatter_value(const std::string& content,
                                        const std::string& key);

  // 提取 ## Nodes 节的 JSON 代码块
  std::string extract_nodes_json(const std::string& content);
};

}  // namespace pdk_chat_demo
