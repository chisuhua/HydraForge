// src/modules/parser/declarative_style.h
// C6: ADR-0072 D3 declarative style `exec: [...]` 解析 + 自动 fork/join DAG 包装
// 设计依据: openspec/changes/from-roadmap-phase-6c-execution-dsl/specs/dsl-extensions/spec.md
// 作者: Solo Dev
// 日期: 2026-09-02

#pragma once

#include "core/types/node.h"
#include "core/types/budget.h" // ExecutionBudget
#include <string>
#include <vector>
#include <memory>
#include <exception>
#include <nlohmann/json.hpp>
#include <optional>

namespace agenticdsl {

// exec: 语法解析结果
struct ExecExpansionResult {
    std::vector<std::unique_ptr<Node>> generated_nodes; // 生成的 fork + 子节点 + join 节点
    std::string fork_path;                              // fork 节点路径
    std::string join_path;                              // join 节点路径
    std::vector<std::string> branch_paths;              // 各分支节点路径
    bool single_item_optimized = false;                 // 单元素优化: 无 fork/join 开销
};

// 解析错误类型
struct ParseError : public std::exception {
    std::string message;
    std::string file_path;
    size_t line_number = 0;
    std::string snippet; // 错误上下文片段

    ParseError() = default;
    ParseError(const std::string& msg, const std::string& file, size_t line, const std::string& ctx = "")
        : message(msg), file_path(file), line_number(line), snippet(ctx) {}

    const char* what() const noexcept override {
        return message.c_str();
    }
};

// DeclarativeStyleParser: 处理 `exec: [...]` 语法糖展开
class DeclarativeStyleParser {
public:
    // 配置常量
    static constexpr int MAX_EXEC_DEPTH = 1; // 仅支持一层嵌套

    DeclarativeStyleParser() = default;

    // 主入口: 解析 exec: 数组并展开为 fork/join DAG
    // 参数:
    //   - base_path: 父节点路径 (如 "/main/step1")
    //   - exec_array: JSON 数组，每个元素为子节点定义
    //   - next_paths: 展开后 join 节点的后继路径
    //   - metadata: 传递给生成节点的元数据
    //   - current_exec_depth: 当前嵌套深度 (外部传入 0，内部递归 +1)
    // 返回: ExecExpansionResult (成功) 或抛 ParseError (失败)
    ExecExpansionResult expand_exec_array(
        const std::string& base_path,
        const nlohmann::json& exec_array,
        const std::vector<std::string>& next_paths = {},
        const nlohmann::json& metadata = nlohmann::json::object(),
        int current_exec_depth = 0
    );

    // 检查 JSON 对象是否包含 exec: key
    static bool has_exec_key(const nlohmann::json& node_json);

    // 提取 exec: 数组 (已验证存在)
    static nlohmann::json extract_exec_array(const nlohmann::json& node_json);

private:
    // 创建 fork 节点
    std::unique_ptr<ForkNode> create_fork_node(
        const std::string& path,
        const std::vector<std::string>& branch_paths,
        const std::vector<std::string>& next_paths,
        const nlohmann::json& metadata
    );

    // 创建 join 节点
    std::unique_ptr<JoinNode> create_join_node(
        const std::string& path,
        const std::vector<std::string>& wait_for_paths,
        const std::vector<std::string>& next_paths,
        const nlohmann::json& metadata
    );

    // 解析单个子节点定义 (支持 type, tool, prompt_template 等完整节点字段)
    std::unique_ptr<Node> parse_child_node(
        const std::string& child_path,
        const nlohmann::json& child_json,
        const std::vector<std::string>& next_paths,
        const nlohmann::json& metadata
    );

    // 生成唯一子路径: base_path/branch_0, base_path/branch_1, ...
    static std::string make_branch_path(const std::string& base_path, size_t index);

    // 生成 fork/join 路径
    static std::string make_fork_path(const std::string& base_path);
    static std::string make_join_path(const std::string& base_path);

    // 验证 exec 数组格式
    void validate_exec_array(const nlohmann::json& exec_array, const std::string& base_path) const;
};

} // namespace agenticdsl