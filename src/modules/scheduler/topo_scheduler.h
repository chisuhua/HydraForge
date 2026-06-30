// modules/scheduler/include/scheduler/topo_scheduler.h
// P1.T4 (2026-06-18): ToolRegistry& → IToolRegistry& (依赖倒置, ADR-0019 §1.4)
#ifndef AGENTICDSL_MODULES_SCHEDULER_TOPO_SCHEDULER_H
#define AGENTICDSL_MODULES_SCHEDULER_TOPO_SCHEDULER_H

#include "core/types/context.h" // 引入 Context
#include "core/types/node.h"    // 引入 NodePath, Node
#include "core/types/budget.h"  // 引入 ExecutionBudget
#include "scheduler/execution_session.h" // 引入 ExecutionSession
// P1.T4: 改为 IToolRegistry 抽象 (不再拖入 common/tools/registry.h)
#include "agenticdsl/contract/itool_registry.h" // P1.T2: IToolRegistry 抽象接口
#include "modules/parser/markdown_parser.h" // 引入 ParsedGraph (std::vector<ParsedGraph> 需要完整类型)
// ResourceManager 前向声明 (Sprint 17 C.4: PIMPL-lite 解耦)
#include "agenticdsl/contract/ischeduler.h"  // ADR-0019 §1.4：实现 IScheduler 抽象接口
#include <vector>
#include <memory> // For unique_ptr<Node>
#include <unordered_map>
#include <unordered_set>
#include <queue>
#include <optional>

namespace tf { class Executor; class Taskflow; class Task; }

namespace agenticdsl {

// C₁.3: 前向声明 ILLMProvider（避免循环 include）
class ILLMProvider;

class ApprovalHandler; // ADR-0031 (2026-07-31): 前向声明
class ToolCoordinator; // C4 Sprint 14 (ADR-0031 P3-P4): 前向声明
class ResourceManager; // Sprint 17 C.4: PIMPL-lite 解耦 (替代 include modules/scheduler/resource_manager.h)

class TopoScheduler : public IScheduler {
public:
    struct Config {
        std::optional<ExecutionBudget> initial_budget;
        ApprovalHandler* approval_handler{nullptr}; // ADR-0031 (2026-07-31): 审批处理器
        ToolCoordinator* tool_coordinator{nullptr}; // C4 Sprint 14 (ADR-0031 P3-P4): ToolCoordinator
        // Add other config options if needed
        Config() = default;
    };

    // C₁.3 迁移：从 LlamaAdapter* 改为 ILLMProvider*
    // P1.T4 (2026-06-18): ToolRegistry& → IToolRegistry& (依赖倒置, ADR-0019 §1.4)
    TopoScheduler(Config config, IToolRegistry& tool_registry, ILLMProvider* llm_provider, const std::vector<ParsedGraph>* full_graphs_ = nullptr);

    void register_node(std::unique_ptr<Node> node) override;
    void build_dag() override; // 构建依赖图
    ExecutionResult execute(const Context& initial_context) override;

    // Method for DSLEngine to call to add new graphs dynamically
    void append_dynamic_graphs(std::vector<ParsedGraph> new_graphs) override;

    // C2 Day 1-2 (ADR-0030 V2): 并行 DAG 执行 — Taskflow tf::Executor 集成
    // 行为: 复用已构建的 DAG, 用 tf::Executor 并行派发无依赖节点
    // 兼容: 当前 execute() 仍串行执行, execute_parallel() 为可选优化
    ExecutionResult execute_parallel(const Context& initial_context);

    std::vector<TraceRecord> get_last_traces() const override {
        return session_.get_trace_exporter().get_traces();
    }

private:
    const std::vector<ParsedGraph>* full_graphs_ = nullptr; // ← 新增
    std::unique_ptr<ResourceManager> resource_manager_; // Sprint 17 C.4: PIMPL-lite 化
    ExecutionSession session_;
    std::vector<std::unique_ptr<Node>> all_nodes_;
    std::unordered_map<NodePath, Node*> node_map_;
    std::unordered_map<NodePath, std::vector<NodePath>> reverse_edges_; // 后继 -> 前驱
    std::unordered_map<NodePath, std::vector<NodePath>> wait_for_dependents_; // 被 wait_for 引用 -> 等待者
    std::unordered_map<NodePath, int> in_degree_;
    std::queue<NodePath> ready_queue_;
    std::unordered_set<NodePath> executed_;
    std::vector<NodePath> call_stack_; // 用于 soft end
    //

    void register_resources();
    // Sprint 18 D-2: build_dag() 拆分 - wait_for 依赖解析 + ready_queue 填充
    void parse_node_wait_for_deps();
    void seed_initial_ready_queue();

    std::vector<ParsedGraph> dynamic_graphs_; // Store newly generated graphs
    //
    void load_graphs(const std::vector<std::unique_ptr<Node>>& nodes); // Helper for registration/building
    //
    std::optional<NodePath> current_fork_node_path_; // Path of the ForkNode currently being processed
    std::vector<NodePath> current_fork_branches_; // List of branches from the ForkNode
    std::vector<Context> current_fork_branch_results_; // Results from each executed branch
    size_t current_fork_branch_index_ = 0; // Index of the branch currently being executed
    bool is_executing_fork_branches_ = false; // Flag indicating if in branch execution mode
    std::string join_merge_strategy_ = "error_on_conflict"; // Strategy for the corresponding JoinNode
    std::vector<NodePath> join_wait_for_; // Dependencies for the JoinNode (if needed for complex scenarios, but basic impl uses all fork branches)
    std::optional<NodePath> current_join_node_path_; // Path of the JoinNode currently being processed
    // --- END v3.1 ---

    std::unique_ptr<tf::Executor> parallel_executor_;
    std::unique_ptr<tf::Taskflow> parallel_taskflow_;

    // --- v3.1: Helper methods for Fork/Join ---
    void start_fork_simulation(const ForkNode* fork_node, const Context& fork_context_snapshot);
    void execute_fork_branches();
    Context execute_single_branch(const NodePath& branch_path, const Context& initial_context);
    void finish_fork_simulation();
    void start_join_simulation(const JoinNode* join_node);
    void finish_join_simulation(Context& main_context);
    // Sprint 17 C.3: execute_single_branch helper methods
    NodePath find_branch_start_node(const NodePath& branch_path) const;
    auto init_branch_state(const NodePath& branch_path)
        -> std::tuple<std::queue<NodePath>, std::unordered_set<NodePath>,
                      std::unordered_map<NodePath, int>,
                      std::unordered_map<NodePath, std::vector<NodePath>>>;
    bool process_branch_end_node(Node* node, const NodePath& current_path,
                                  const NodePath& branch_path, const Context& current_ctx);

    // Sprint 7 Day 5: DagState (Oracle A + 4 纠正, 7 字段契约). nodes 是非拥有视图, 不要改 unique_ptr.
    struct DagState {
        std::unordered_map<NodePath, Node*> nodes;
        std::unordered_map<NodePath, std::vector<NodePath>> reverse_edges;
        std::unordered_map<NodePath, std::vector<NodePath>> wait_for_dependents;
        std::unordered_map<NodePath, int> in_degree;
        std::queue<NodePath> ready_queue;
        std::unordered_set<NodePath> executed;
        std::vector<ParsedGraph> dynamic_graphs;
    };

    std::optional<ExecutionResult> prepare_dag_state(DagState& state);
    // Sprint 18 reduce-topo-scheduler-complexity D-1: build_dag(DagState&) 重载已删除, 显式迁移到 execute_parallel()
    std::optional<ExecutionResult> resolve_dynamic_waits(
        Node* current_node, const NodePath& current_path, const Context& context, bool& can_execute);
    void process_fork_join(Node* current_node, Context& context);
    void rebuild_dynamic_graph(DagState& state);
    void handle_fork_branches_block();
    void handle_fork_node(Node* current_node, const Context& context);
    bool check_end_termination(Node* current_node, const NodePath& current_path);
    struct NodeLookupResult {
        NodePath path;
        Node* node;
    };
    std::variant<std::monostate, NodeLookupResult, ExecutionResult>
    dispatch_ready_nodes(DagState& state, const Context& context);
    ExecutionResult finalize_execution(DagState& state, const Context& context);
    std::optional<ExecutionResult> handle_node_completion(
        DagState& state, const NodeResult& result, Node* current_node, const NodePath& current_path);
    bool process_jump(const std::string& message, const NodePath& current_path);
    void update_successors(Node* current_node, const NodePath& current_path);
};

} // namespace agenticdsl

#endif // AGENTICDSL_MODULES_SCHEDULER_TOPO_SCHEDULER_H
