#ifndef AGENTICDSL_CORE_TYPES_NODE_H
#define AGENTICDSL_CORE_TYPES_NODE_H

#include "resource.h" // 引入 ResourceType
#include "common/llm/llm_tool.h" // 引入 LLMParams
#include "budget.h" // 引入 ExecutionBudget
#include "agenticdsl/types/layered_context.h" // Sprint 20 / LayeredContext execute 签名
#include "agenticdsl/types/context_flatten.h" // Sprint 20 / to_context / from_context
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include <optional>

namespace agenticdsl {

// 节点路径类型
using NodePath = std::string; // e.g., "/main/step1"

// 节点类型枚举
enum class NodeType : uint8_t {
    START,
    END,
    ASSIGN,
    DSL_CALL,  // Changed from LLM_CALL (v3.10)
    TOOL_CALL,
    RESOURCE,
    FORK,
    JOIN,
    GENERATE_SUBGRAPH,
    ASSERT,
    YIELD  // C12: token-by-token 流式生成节点
};

// C12: YieldNode 模式枚举
enum class YieldMode : uint8_t {
    NEXT = 0,      // 单 token 拉取 + 暂停
    CONTINUE = 1,  // 连续拉取到 stream 结束或预算耗尽
    STOP = 2       // 终止流 + 跳到 stop_path
};

// Forward declarations for Node structure
class InjaTemplateRenderer; // Declared here, defined elsewhere

// Base Node
struct Node {
    NodePath path;
    NodeType type;
    std::vector<NodePath> next;
    nlohmann::json metadata;

    std::optional<std::string> signature;     // e.g., "(input: string) -> {result: number}"
    std::vector<std::string> permissions;     // e.g., ["network", "file:read"]

    Node(NodePath path,
         NodeType type,
         std::vector<NodePath> next = {},
         nlohmann::json metadata = nlohmann::json::object(),
         std::optional<std::string> sig = std::nullopt,
         std::vector<std::string> perms = {})
        : path(std::move(path)),
          type(type),
          next(std::move(next)),
          metadata(std::move(metadata)),
          signature(std::move(sig)),
          permissions(std::move(perms)) {}

    virtual ~Node() = default;
    // Sprint 20 (2026-07-01) / OpenSpec migrate-context-to-layered:
    // 新签名 — 接受 LayeredContext (5-层结构化, ADR-0008)。
    [[nodiscard]] virtual LayeredContext execute(LayeredContext& ctx) = 0;

    // 旧签名 — Sprint 20 期间保留, 通过基类默认实现委托到新签名。
    [[nodiscard]] [[deprecated("use LayeredContext overload (Sprint 20 / ADR-0008)")]]
    virtual Context execute(Context& context);

    virtual std::unique_ptr<Node> clone() const = 0; // Required for scheduler
};

// 旧 execute(Context&) 的基类默认实现 — 委托到新签名。
inline Context Node::execute(Context& context) {
    LayeredContext lc = to_context(context);
    LayeredContext result = execute(lc);
    return result.working;
}

struct ParsedGraph {
    std::vector<std::unique_ptr<Node>> nodes;
    NodePath path; // e.g., /main
    nlohmann::json metadata; // graph-level metadata
    std::optional<ExecutionBudget> budget; // 从 /__meta__ 解析
    std::optional<std::string> signature; // 子图签名
    std::vector<std::string> permissions; // 子图权限
    bool is_standard_library = false; // 路径以 /lib/ 开头
    std::optional<nlohmann::json> output_schema; // v3.1: 解析 signature.outputs 为 JSON Schema

    ParsedGraph() = default;
    ParsedGraph(ParsedGraph&&) = default;                // 允许移动
    ParsedGraph& operator=(ParsedGraph&&) = default;     // 允许移动赋值
    ParsedGraph(const ParsedGraph&) = delete;            // 禁止拷贝
    ParsedGraph& operator=(const ParsedGraph&) = delete; // 禁止拷贝赋值
};


// Start Node
struct StartNode : public Node {
    StartNode(NodePath path, std::vector<NodePath> next_paths = {});
    [[nodiscard]] LayeredContext execute(LayeredContext& ctx) override;
    std::unique_ptr<Node> clone() const override;
};

// End Node
struct EndNode : public Node {
    EndNode(NodePath path);
    [[nodiscard]] LayeredContext execute(LayeredContext& ctx) override;
    std::unique_ptr<Node> clone() const override;
};

// Assign Node (v1.1: renamed from Set)
struct AssignNode : public Node {
    std::unordered_map<std::string, std::string> assign;

    AssignNode(NodePath path,
               std::unordered_map<std::string, std::string> assigns,
               std::vector<NodePath> next_paths = {});
    [[nodiscard]] LayeredContext execute(LayeredContext& ctx) override;
    std::unique_ptr<Node> clone() const override;
};

// DSL Node (for LLM-generated DSL, v3.10)
struct DSLNode : public Node {
    std::string prompt_template;
    std::string llm_tool_name;  // e.g., "llama-7b", "gpt-4"
    LLMParams llm_params;       // Generation parameters
    std::vector<std::string> output_keys;

    DSLNode(NodePath path,
            std::string prompt,
            std::string llm_tool_name,
            LLMParams llm_params = {},
            std::vector<std::string> output_keys = {},
            std::vector<NodePath> next_paths = {});
    [[nodiscard]] LayeredContext execute(LayeredContext& ctx) override;
    std::unique_ptr<Node> clone() const override;
};

// Tool Call Node
struct ToolCallNode : public Node {
    std::string tool_name;
    std::unordered_map<std::string, std::string> arguments;
    std::vector<std::string> output_keys;

    ToolCallNode(NodePath path,
                 std::string tool_name,
                 std::unordered_map<std::string, std::string> arguments,
                 std::vector<std::string> output_keys,
                 std::vector<NodePath> next_paths = {});
    [[nodiscard]] LayeredContext execute(LayeredContext& ctx) override;
    std::unique_ptr<Node> clone() const override;
};

// Resource Node (v1.1)
struct ResourceNode : public Node {
    ResourceType resource_type; // ResourceType must be defined in resource.h
    std::string uri;
    std::string scope;

    ResourceNode(NodePath path,
                 ResourceType type,
                 std::string uri,
                 std::string scope = "global",
                 nlohmann::json metadata = nlohmann::json::object());
    [[nodiscard]] LayeredContext execute(LayeredContext& ctx) override;
    std::unique_ptr<Node> clone() const override;
};

// --- 新增：Fork Node (v3.1) ---
struct ForkNode : public Node {
    std::vector<NodePath> branches; // List of subgraph paths to execute in parallel

    ForkNode(NodePath path, std::vector<NodePath> branch_paths, std::vector<NodePath> next_paths = {});
    [[nodiscard]] LayeredContext execute(LayeredContext& ctx) override; // Implementation in executor
    std::unique_ptr<Node> clone() const override;
};

// --- 新增：Join Node (v3.1) ---
struct JoinNode : public Node {
    std::vector<NodePath> wait_for; // List of nodes to wait for
    std::string merge_strategy; // e.g., "error_on_conflict", "last_write_wins", etc.

    JoinNode(NodePath path, std::vector<NodePath> deps, std::string strategy, std::vector<NodePath> next_paths = {});
    [[nodiscard]] LayeredContext execute(LayeredContext& ctx) override; // Implementation in executor
    std::unique_ptr<Node> clone() const override;
};

// --- 新增：GenerateSubgraph Node (v3.1) ---
struct GenerateSubgraphNode : public Node {
    std::string prompt_template;
    std::vector<std::string> output_keys; // e.g., ["generated_graph_path"]
    std::string signature_validation = "strict"; // v3.1: strict, warn, ignore
    std::optional<NodePath> on_signature_violation; // v3.1

    GenerateSubgraphNode(NodePath path, std::string prompt, std::vector<std::string> output_keys, std::vector<NodePath> next_paths = {});
    [[nodiscard]] LayeredContext execute(LayeredContext& ctx) override; // Implementation in executor
    std::unique_ptr<Node> clone() const override;
};

struct AssertNode : public Node {
    std::string condition; // Inja boolean expression
    std::optional<NodePath> on_failure; // Jump path on failure

    AssertNode(NodePath path, std::string condition, std::optional<NodePath> on_fail, std::vector<NodePath> next_paths = {});
    [[nodiscard]] LayeredContext execute(LayeredContext& ctx) override;
    std::unique_ptr<Node> clone() const override;
};

// C12: YIELD/STREAM 节点 - token-by-token 流式生成
struct YieldNode : public Node {
    std::string yield_value;     // 模板表达式
    YieldMode mode = YieldMode::NEXT;
    NodePath stop_path;          // STOP 模式跳转目标

    YieldNode(NodePath path,
              std::vector<NodePath> next = {},
              nlohmann::json metadata = nlohmann::json::object(),
              std::optional<std::string> sig = std::nullopt,
              std::vector<std::string> perms = {},
              std::string yield_value = "",
              YieldMode mode = YieldMode::NEXT,
              NodePath stop_path = "")
        : Node(std::move(path), NodeType::YIELD, std::move(next),
               std::move(metadata), std::move(sig), std::move(perms)),
          yield_value(std::move(yield_value)),
          mode(mode),
          stop_path(std::move(stop_path)) {}

    [[nodiscard]] LayeredContext execute(LayeredContext& ctx) override {
        // §3 实施 execute_yield() 时替换此处
        return ctx;
    }

    std::unique_ptr<Node> clone() const override {
        return std::make_unique<YieldNode>(*this);
    }
};

struct NodeResult {
    bool success = true;
    nlohmann::json output;
    std::string error_message;
};


} // namespace agenticdsl

#endif // AGENTICDSL_COMMON_TYPES_NODE_H
