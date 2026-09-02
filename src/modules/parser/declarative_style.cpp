// src/modules/parser/declarative_style.cpp
// C6: ADR-0072 D3 declarative style `exec: [...]` 解析 + 自动 fork/join DAG 包装
// 设计依据: openspec/changes/from-roadmap-phase-6c-execution-dsl/specs/dsl-extensions/spec.md
// 作者: Solo Dev
// 日期: 2026-09-02

#include "parser/declarative_style.h"
#include "agenticdsl/parser/node_factory.h"
#include "common/utils/parser_utils.h"
#include "common/log/log.h"
#include "common/utils/yaml_json.h"
#include <stdexcept>
#include <sstream>

namespace agenticdsl {

bool DeclarativeStyleParser::has_exec_key(const nlohmann::json& node_json) {
    return node_json.contains("exec") && node_json["exec"].is_array();
}

nlohmann::json DeclarativeStyleParser::extract_exec_array(const nlohmann::json& node_json) {
    return node_json["exec"];
}

std::string DeclarativeStyleParser::make_branch_path(const std::string& base_path, size_t index) {
    return base_path + "/branch_" + std::to_string(index);
}

std::string DeclarativeStyleParser::make_fork_path(const std::string& base_path) {
    return base_path + "_fork";
}

std::string DeclarativeStyleParser::make_join_path(const std::string& base_path) {
    return base_path + "_join";
}

void DeclarativeStyleParser::validate_exec_array(const nlohmann::json& exec_array,
                                                 const std::string& base_path) const {
    if (!exec_array.is_array()) {
        throw ParseError("'exec' must be an array", base_path, 0,
                         "'exec: [tool_a, tool_b]' 声明式语法要求数组形式");
    }
    if (exec_array.empty()) {
        throw ParseError("'exec' array must not be empty", base_path, 0,
                         "至少需要一个子节点");
    }
}

// 创建 fork 节点 — 与 node_factory.cpp make_fork 语义对齐
std::unique_ptr<ForkNode> DeclarativeStyleParser::create_fork_node(
    const std::string& path,
    const std::vector<std::string>& branch_paths,
    const std::vector<std::string>& next_paths,
    const nlohmann::json& metadata) {
    auto node = std::make_unique<ForkNode>(path, branch_paths, next_paths);
    node->metadata = metadata;
    // 记录展开来源，便于 Trace 与 lint 溯源
    node->metadata["exec_expanded"] = true;
    node->metadata["exec_kind"] = "fork";
    return node;
}

// 创建 join 节点 — 与 node_factory.cpp make_join 语义对齐
std::unique_ptr<JoinNode> DeclarativeStyleParser::create_join_node(
    const std::string& path,
    const std::vector<std::string>& wait_for_paths,
    const std::vector<std::string>& next_paths,
    const nlohmann::json& metadata) {
    auto node = std::make_unique<JoinNode>(path, wait_for_paths, "error_on_conflict", next_paths);
    node->metadata = metadata;
    node->metadata["exec_expanded"] = true;
    node->metadata["exec_kind"] = "join";
    return node;
}

// 解析单个子节点定义 (支持完整节点字段; 仅需 type + tool)
std::unique_ptr<Node> DeclarativeStyleParser::parse_child_node(
    const std::string& child_path,
    const nlohmann::json& child_json,
    const std::vector<std::string>& next_paths,
    const nlohmann::json& metadata) {
    // 字符串简写形式优先: exec: [shell/exec, fs/read] → tool_call
    if (child_json.is_string()) {
        std::string tool_name = child_json.get<std::string>();
        nlohmann::json tc = {
            {"type", "tool_call"},
            {"tool", tool_name},
            {"arguments", nlohmann::json::object()},
            {"output_keys", nlohmann::json::array({tool_name + "_result"})}
        };
        auto node = NodeFactoryRegistry::global().create("tool_call", child_path, tc);
        if (node) {
            node->next = next_paths;
            return node;
        }
        throw ParseError("Failed to create tool_call node for '" + tool_name + "'",
                         child_path, 0, "内部 NodeFactory 创建失败");
    }

    if (!child_json.is_object()) {
        throw ParseError("'exec' array element must be an object or a string", child_path, 0,
                         "支持 { type: ... } 对象 或 \"tool/name\" 字符串简写");
    }

    // 记录原始路径到 metadata (exec 展开溯源)
    nlohmann::json node_meta = metadata;
    node_meta["exec_parent"] = child_path;

    // 支持 exec: [shell/exec, fs/read] 字符串简写 → 转为 tool_call 节点
    if (child_json.contains("type")) {
        std::string type_str = child_json["type"].get<std::string>();
        if (type_str == "tool_call" || type_str == "dsl_call" || type_str == "llm_call" ||
            type_str == "assign" || type_str == "resource" || type_str == "assert" ||
            type_str == "generate_subgraph" || type_str == "yield") {
            // 复用 NodeFactoryRegistry 创建具体节点 (forward-compatible)
            auto node = NodeFactoryRegistry::global().create(type_str, child_path, child_json);
            if (node) {
                // 合并 metadata 与 exec 溯源 (node_factory 的 apply_context 会覆盖 metadata)
                if (node_meta.is_object()) {
                    for (auto& [k, v] : node_meta.items()) {
                        if (!node->metadata.contains(k)) {
                            node->metadata[k] = v;
                        }
                    }
                }
                // next 由调用方 (expand_exec_array) 统一设置, 不覆盖此处
                node->next = next_paths;
                return node;
            }
            throw ParseError("Failed to create node of type '" + type_str + "'",
                             child_path, 0, "内部 NodeFactory 创建失败");
        }
        // 未知子节点类型 → 报错 (不做隐式 tool_call 转换, 避免歧义)
        throw ParseError("Unsupported node type '" + type_str + "' in exec array",
                         child_path, 0,
                         "exec: 子节点支持 tool_call/dsl_call/llm_call/assign/assert/resource/generate_subgraph/yield");
    }

    // 字符串简写形式: exec: [shell/exec, fs/read] → tool_call
    if (child_json.is_string()) {
        std::string tool_name = child_json.get<std::string>();
        nlohmann::json tc = {
            {"type", "tool_call"},
            {"tool", tool_name},
            {"arguments", nlohmann::json::object()},
            {"output_keys", nlohmann::json::array({tool_name + "_result"})}
        };
        auto node = NodeFactoryRegistry::global().create("tool_call", child_path, tc);
        if (node) {
            node->next = next_paths;
            return node;
        }
        throw ParseError("Failed to create tool_call node for '" + tool_name + "'",
                         child_path, 0, "内部 NodeFactory 创建失败");
    }

    throw ParseError("'exec' array element must be an object or a string", child_path, 0,
                     "支持 { type: ... } 对象 或 \"tool/name\" 字符串简写");
}

ExecExpansionResult DeclarativeStyleParser::expand_exec_array(
    const std::string& base_path,
    const nlohmann::json& exec_array,
    const std::vector<std::string>& next_paths,
    const nlohmann::json& metadata,
    int current_exec_depth) {
    ExecExpansionResult result;
    validate_exec_array(exec_array, base_path);

    // 深度限制: 仅支持一层嵌套 (max_exec_depth=1)
    if (current_exec_depth >= MAX_EXEC_DEPTH) {
        throw ParseError("'exec' nesting exceeds max_exec_depth=" + std::to_string(MAX_EXEC_DEPTH),
                         base_path, 0,
                         "仅支持一层 exec: 嵌套, 递归留 Sprint 28+");
    }

    // 检测嵌套 exec: 子节点含 exec: key → 触发深度超限 (per design D-2 单层限制)
    for (const auto& child : exec_array) {
        if (child.is_object() && child.contains("exec") && child["exec"].is_array()) {
            throw ParseError("'exec' nesting exceeds max_exec_depth=" + std::to_string(MAX_EXEC_DEPTH),
                             base_path, 0,
                             "嵌套 exec: 子节点含 exec: 数组触发深度超限, 仅支持一层");
        }
    }

    const size_t n = exec_array.size();

    // 单元素优化: exec: [single_tool] → 直接执行, 无 fork/join 开销
    if (n == 1) {
        std::string child_path = make_branch_path(base_path, 0);
        auto child = parse_child_node(child_path, exec_array[0], next_paths, metadata);
        result.generated_nodes.push_back(std::move(child));
        result.branch_paths.push_back(child_path);
        result.single_item_optimized = true;
        return result;
    }

    // 多元素: 生成 fork + N 子节点 + join
    const std::string fork_path = make_fork_path(base_path);
    const std::string join_path = make_join_path(base_path);

    // 收集各分支节点路径
    std::vector<std::string> branch_paths;
    for (size_t i = 0; i < n; ++i) {
        branch_paths.push_back(make_branch_path(base_path, i));
    }

    // 1) fork 节点 → branches 指向所有子节点, next 指向 join
    result.generated_nodes.push_back(
        create_fork_node(fork_path, branch_paths, {join_path}, metadata));

    // 2) N 个子节点 → next 空 (join 通过 wait_for 等待触发, 不冗余 next 边)
    for (size_t i = 0; i < n; ++i) {
        std::string child_path = make_branch_path(base_path, i);
        auto child = parse_child_node(child_path, exec_array[i], {}, metadata);
        result.generated_nodes.push_back(std::move(child));
    }

    // 3) join 节点 → wait_for 所有子节点, next 指向调用方 next_paths
    result.generated_nodes.push_back(
        create_join_node(join_path, branch_paths, next_paths, metadata));

    result.fork_path = fork_path;
    result.join_path = join_path;
    result.branch_paths = std::move(branch_paths);

    return result;
}

} // namespace agenticdsl