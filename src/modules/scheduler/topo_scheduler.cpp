// modules/scheduler/src/topo_scheduler.cpp
#include "scheduler/topo_scheduler.h"
#include "core/types/node.h"
#include "modules/scheduler/resource_manager.h" // Sprint 17 C.4: 完整类型 (PIMPL-lite 从头文件移出)
#include "common/llm/llm_types.h" // C₁.3: 需要完整 ILLMProvider 定义
#include "common/utils/template_renderer.h"
#include "common/log/log.h"        // agenticdsl::log 日志门面（tech-debt-and-doc-cleanup）
#include <taskflow/taskflow.hpp>  // C2 Day 1-2: tf::Executor + tf::Taskflow (完整定义, 避开 TBB 在头文件中与 std::queue 冲突)
#include <stdexcept>
#include <algorithm>
#include <set>
#include <queue>
#include <variant>

namespace agenticdsl {

struct HardEndException : public std::exception {
    const char* what() const noexcept override {
        return "Hard end node encountered in branch, terminating main execution.";
    }
};

// C₁.3 迁移：从 LlamaAdapter* 改为 ILLMProvider*
// P1.T4 (2026-06-18): ToolRegistry& → IToolRegistry& (依赖倒置)
TopoScheduler::TopoScheduler(Config config, IToolRegistry& tool_registry, ILLMProvider* llm_provider, const std::vector<ParsedGraph>* full_graphs)
    : full_graphs_(full_graphs),
      resource_manager_(std::make_unique<ResourceManager>()),
      session_(std::move(config.initial_budget), tool_registry, llm_provider, *resource_manager_,
               full_graphs_,
               [this](std::vector<ParsedGraph> graphs) { this->append_dynamic_graphs(std::move(graphs)); }) { // Pass callback to ExecutionSession
    // ADR-0031 (2026-07-31): 传递审批处理器到执行会话
    if (config.approval_handler) {
        session_.set_approval_handler(config.approval_handler);
    }
    // C4 Sprint 14 (ADR-0031 P3-P4): 传递 ToolCoordinator 到执行会话
    if (config.tool_coordinator) {
        session_.set_tool_coordinator(config.tool_coordinator);
    }
}

void TopoScheduler::register_node(std::unique_ptr<Node> node) {
    NodePath path = node->path;
    node_map_[path] = node.get();
    all_nodes_.push_back(std::move(node));
}

void TopoScheduler::register_resources() {
    for (const auto& node_ptr : all_nodes_) {
        if (node_ptr->type == NodeType::RESOURCE) {
            const ResourceNode* res_node = static_cast<const ResourceNode*>(node_ptr.get());
            Resource res{
                .path = res_node->path,
                .resource_type = res_node->resource_type,
                .uri = res_node->uri,
                .scope = res_node->scope,
                .metadata = res_node->metadata
            };
            resource_manager_->register_resource(res);
        }
    }
}

void TopoScheduler::build_dag() {
    register_resources();
    // Sprint 18 D-2: 拆分 88 行 build_dag → 3 行编排 + 2 helpers
    parse_node_wait_for_deps();
    seed_initial_ready_queue();
}

void TopoScheduler::parse_node_wait_for_deps() {
    for (const auto& node_ptr : all_nodes_) {
        NodePath current_path = node_ptr->path;
        in_degree_[current_path] = 0;
        reverse_edges_[current_path] = {};
        wait_for_dependents_[current_path] = {};
    }

    for (const auto& node_ptr : all_nodes_) {
        NodePath current_path = node_ptr->path;
        Node* node = node_ptr.get();

        for (const auto& next_path : node->next) {
            if (node_map_.count(next_path) == 0) {
                throw std::runtime_error("Next node not found: " + next_path);
            }
            reverse_edges_[next_path].push_back(current_path);
            in_degree_[next_path]++;
        }

        if (node->metadata.contains("wait_for") && !node->metadata["wait_for"].is_string()) {
            const auto& wf = node->metadata["wait_for"];
            std::vector<NodePath> deps;

            if (wf.is_object()) {
                if (wf.contains("all_of")) {
                    const auto& all = wf["all_of"];
                    if (all.is_array()) {
                        for (const auto& item : all) deps.push_back(item.get<std::string>());
                    } else if (all.is_string()) {
                        deps.push_back(all.get<std::string>());
                    }
                }
                if (wf.contains("any_of")) {
                    const auto& any = wf["any_of"];
                    if (any.is_array()) {
                        for (const auto& item : any) deps.push_back(item.get<std::string>());
                    } else if (any.is_string()) {
                        deps.push_back(any.get<std::string>());
                    }
                }
            } else if (wf.is_array()) {
                for (const auto& item : wf) deps.push_back(item.get<std::string>());
            } else if (wf.is_string()) {
                deps.push_back(wf.get<std::string>());
            }

            for (const auto& dep_path : deps) {
                if (node_map_.count(dep_path) == 0) {
                    throw std::runtime_error("wait_for dependency not found: " + dep_path);
                }
                reverse_edges_[current_path].push_back(dep_path);
                wait_for_dependents_[dep_path].push_back(current_path);
                in_degree_[current_path]++;
            }
        }
    }
}

void TopoScheduler::seed_initial_ready_queue() {
    for (const auto& node_ptr : all_nodes_) {
        NodePath path = node_ptr->path;
        if (in_degree_[path] == 0) {
            ready_queue_.push(path);
        }
    }
    LOG_DEBUG("Initial ready queue size: " << ready_queue_.size());
}

ExecutionResult TopoScheduler::execute(const Context& initial_context) {
    Context context = initial_context;
    DagState state;
    if (auto early = prepare_dag_state(state); early.has_value()) return *early;
    build_dag();
    // Sprint 18 D-1: 显式迁移 7 字段到 DagState (替代原 build_dag(DagState&) 重载)
    state.nodes = node_map_;
    state.reverse_edges = reverse_edges_;
    state.wait_for_dependents = wait_for_dependents_;
    state.in_degree = in_degree_;
    state.dynamic_graphs.clear();
    state.executed.clear();
    while (!state.ready_queue.empty()) state.ready_queue.pop();
    state.ready_queue = ready_queue_;

    while (!state.ready_queue.empty() || !session_.get_pending_dynamic_deps().empty()) {
        handle_fork_branches_block();

        auto dispatch_result = dispatch_ready_nodes(state, context);
        if (std::holds_alternative<ExecutionResult>(dispatch_result)) return std::get<ExecutionResult>(dispatch_result);
        if (std::holds_alternative<std::monostate>(dispatch_result)) break;
        auto& found = std::get<NodeLookupResult>(dispatch_result);
        if (executed_.count(found.path) > 0) continue;

        bool can_execute = true;
        if (auto rw_err = resolve_dynamic_waits(found.node, found.path, context, can_execute); rw_err.has_value()) return *rw_err;
        if (!can_execute) continue;

        auto session_result = session_.execute_node(found.node, context);
        if (!session_result.success) {
            if (process_jump(session_result.message, found.path)) continue;
            return {false, session_result.message, context, session_result.paused_at};
        }

        context = std::move(session_result.new_context);
        executed_.insert(found.path);

        if (found.node->type == NodeType::FORK) {
            handle_fork_node(found.node, context);
            continue;
        }
        if (found.node->type == NodeType::JOIN) {
            process_fork_join(found.node, context);
        }
        if (session_result.paused_at.has_value()) {
            return {true, "Paused at LLM call", context, session_result.paused_at};
        }

        NodeResult node_result;
        node_result.success = true;
        if (auto err = handle_node_completion(state, node_result, found.node, found.path); err) return *err;
        if (check_end_termination(found.node, found.path)) break;

        if (!dynamic_graphs_.empty()) {
            rebuild_dynamic_graph(state);
        }

        std::unordered_set<NodePath> newly_executed = {found.path};
        session_.check_and_requeue_dynamic_deps(newly_executed);
    }

    return finalize_execution(state, context);
}

ExecutionResult TopoScheduler::execute_parallel(const Context& initial_context) {
    // C2 Day 1-2 (ADR-0030 V2): 并行 DAG 执行
    // 当前实现: 复用已构建 DAG, 用 tf::Executor 并行派发无依赖节点
    // 完整实现: 将 DagState 转换为 tf::Taskflow, 节点依赖关系映射为 Taskflow precedences
    Context context = initial_context;
    DagState state;
    if (auto early = prepare_dag_state(state); early.has_value()) return *early;
    build_dag();
    // Sprint 18 D-1: 显式迁移到 DagState (替代原 build_dag(DagState&) 重载)
    state.nodes = node_map_;
    state.reverse_edges = reverse_edges_;
    state.wait_for_dependents = wait_for_dependents_;
    state.in_degree = in_degree_;
    state.dynamic_graphs.clear();
    state.executed.clear();
    while (!state.ready_queue.empty()) state.ready_queue.pop();
    state.ready_queue = ready_queue_;

    if (!parallel_executor_) {
        parallel_executor_ = std::make_unique<tf::Executor>(
            std::max(1u, std::thread::hardware_concurrency()));
    }
    if (!parallel_taskflow_) {
        parallel_taskflow_ = std::make_unique<tf::Taskflow>();
    }

    parallel_taskflow_->clear();
    std::unordered_map<NodePath, tf::Task> tf_tasks;
    std::vector<NodePath> locally_executed;
    locally_executed.reserve(state.nodes.size());
    for (const auto& [path, _] : state.nodes) {
        tf_tasks[path] = parallel_taskflow_->emplace([this, path, &state, &locally_executed]() {
            Context node_context;
            Node* current_node = state.nodes[path];
            auto session_result = session_.execute_node(current_node, node_context);
            if (!session_result.success) {
                if (process_jump(session_result.message, path)) return;
                return;
            }
            locally_executed.push_back(path);
            NodeResult node_result;
            node_result.success = true;
            handle_node_completion(state, node_result, current_node, path);
        });
    }
    for (const auto& [path, deps] : state.wait_for_dependents) {
        for (const auto& dep : deps) {
            if (tf_tasks.count(dep) && tf_tasks.count(path)) {
                tf_tasks[path].succeed(tf_tasks[dep]);
            }
        }
    }
    if (!state.ready_queue.empty()) {
        auto initial = parallel_taskflow_->emplace([]() {});
        for (const auto& [path, _] : state.nodes) {
            tf_tasks[path].succeed(initial);
        }
    }

    parallel_executor_->run(*parallel_taskflow_).wait();
    for (const auto& path : locally_executed) {
        executed_.insert(path);
    }
    return finalize_execution(state, context);
}


void TopoScheduler::append_dynamic_graphs(std::vector<ParsedGraph> new_graphs) {
    // Store the new graphs temporarily
    // In a more complex system, this might trigger an event or flag for the main loop
    dynamic_graphs_.insert(dynamic_graphs_.end(), std::make_move_iterator(new_graphs.begin()), std::make_move_iterator(new_graphs.end()));
    // The main execute loop will check this list and rebuild the DAG if necessary.
}

void TopoScheduler::start_fork_simulation(const ForkNode* fork_node, const Context& fork_context_snapshot) {
    current_fork_node_path_ = fork_node->path;
    current_fork_branches_ = fork_node->branches; // Store the branches to execute
    current_fork_branch_results_.clear(); // Clear previous results if any
    current_fork_branch_index_ = 0;
    is_executing_fork_branches_ = true;
    // The fork_context_snapshot is already saved by ExecutionSession
    // We just need to remember the branches to execute.
    LOG_DEBUG("Started fork simulation for node " << fork_node->path << " with " << current_fork_branches_.size() << " branches.");
}

void TopoScheduler::execute_fork_branches() {
    if (!is_executing_fork_branches_ || current_fork_branches_.empty()) return;

    const Context* fork_snapshot = session_.get_context_engine().get_snapshot(current_fork_node_path_.value());
    if (!fork_snapshot) {
        throw std::runtime_error("Snapshot for fork node not found: " + current_fork_node_path_.value());
    }

    // Execute branches sequentially
    while (current_fork_branch_index_ < current_fork_branches_.size()) {
        const NodePath& branch_path = current_fork_branches_[current_fork_branch_index_];
        LOG_DEBUG("Executing fork branch: " << branch_path);

        // 1. Restore snapshot for this branch
        Context branch_initial_ctx = *fork_snapshot; // Copy the snapshot

        try {
            // 2. Execute the branch subgraph
            Context branch_final_ctx = execute_single_branch(branch_path, branch_initial_ctx);

            // 3. Store result
            current_fork_branch_results_.push_back(std::move(branch_final_ctx));
            current_fork_branch_index_++;
        } catch (const HardEndException& e) {
            // Re-throw to be caught in the main execute loop
            throw;
        }

        LOG_DEBUG("Branch " << branch_path << " completed. Result stored. Branch " << current_fork_branch_index_ << " / " << current_fork_branches_.size() << " done.");
    }

    // All branches executed, simulation phase is done for fork
    // The join logic will be handled when the corresponding JoinNode is encountered
    LOG_DEBUG("All fork branches completed. Ready for join.");
}
Context TopoScheduler::execute_single_branch(const NodePath& branch_path, const Context& initial_context) {
    NodePath start_node_path = find_branch_start_node(branch_path);
    LOG_DEBUG("Found start node for branch " << branch_path << ": " << start_node_path);

    auto [branch_ready_queue, branch_executed, branch_in_degree, branch_reverse_edges] =
        init_branch_state(branch_path);

    Context current_ctx = initial_context;
    while (!branch_ready_queue.empty()) {
        NodePath current_path = branch_ready_queue.front();
        branch_ready_queue.pop();

        if (branch_executed.count(current_path) > 0) continue;

        auto node_it = node_map_.find(current_path);
        if (node_it == node_map_.end()) {
            throw std::runtime_error("Node not found in map during branch execution: " + current_path);
        }
        Node* node = node_it->second;

        auto session_result = session_.execute_node(node, current_ctx);
        if (!session_result.success) {
            throw std::runtime_error("Branch execution failed at " + current_path + ": " + session_result.message);
        }
        current_ctx = std::move(session_result.new_context);
        branch_executed.insert(current_path);

        if (process_branch_end_node(node, current_path, branch_path, current_ctx)) {
            break;
        }

        for (const auto& next_path : node->next) {
            if (node_map_.count(next_path) > 0 && --branch_in_degree[next_path] == 0) {
                branch_ready_queue.push(next_path);
            }
        }
    }

    return current_ctx;
}

NodePath TopoScheduler::find_branch_start_node(const NodePath& branch_path) const {
  for (const auto& [path, node] : node_map_) {
    if (path.rfind(branch_path + "/", 0) == 0 || path == branch_path) {
      return path;
    }
  }
  throw std::runtime_error("No starting node found for branch path: " + branch_path);
}

auto TopoScheduler::init_branch_state(const NodePath& branch_path)
    -> std::tuple<std::queue<NodePath>, std::unordered_set<NodePath>,
                  std::unordered_map<NodePath, int>,
                  std::unordered_map<NodePath, std::vector<NodePath>>> {
  std::queue<NodePath> branch_ready_queue;
  std::unordered_set<NodePath> branch_executed;
  std::unordered_map<NodePath, int> branch_in_degree = in_degree_;
  std::unordered_map<NodePath, std::vector<NodePath>> branch_reverse_edges = reverse_edges_;

  for (const auto& [path, node] : node_map_) {
    if ((path.rfind(branch_path + "/", 0) == 0 || path == branch_path) &&
        branch_in_degree[path] == 0) {
      branch_ready_queue.push(path);
    }
  }

  return std::make_tuple(std::move(branch_ready_queue),
                          std::move(branch_executed),
                          std::move(branch_in_degree),
                          std::move(branch_reverse_edges));
}

bool TopoScheduler::process_branch_end_node(Node* node, const NodePath& /*current_path*/,
                                             const NodePath& /*branch_path*/,
                                             const Context& /*current_ctx*/) {
  if (node->type != NodeType::END) return false;
  std::string mode = node->metadata.value("termination_mode", "hard");
  if (mode != "soft") {
    throw HardEndException();
  }
  return true;
}

void TopoScheduler::finish_fork_simulation() {
    // Fork simulation is considered finished when all branches are executed (handled in execute_fork_branches).
    // This function can be used to clean up state if needed after all branches finish.
    is_executing_fork_branches_ = false;
    LOG_DEBUG("Finished fork simulation for node " << current_fork_node_path_.value());
    current_fork_node_path_.reset();
    current_fork_branches_.clear();
    // current_fork_branch_results_ is kept until join is processed
}

void TopoScheduler::start_join_simulation(const JoinNode* join_node) {
    current_join_node_path_ = join_node->path;
    join_merge_strategy_ = join_node->merge_strategy; // Store the strategy from the JoinNode
    // join_wait_for_ might be used if JoinNode has explicit dependencies beyond fork branches
    if (join_node->wait_for.empty()) {
        // Default behavior: wait for all branches from the corresponding Fork
        // This requires tracking which Fork this Join corresponds to.
        // For simplicity, assume the last finished Fork corresponds to this Join.
        // A more robust system would explicitly link Fork and Join nodes.
        // For now, we rely on the fact that all branches from the current fork are collected.
    } else {
        join_wait_for_ = join_node->wait_for; // Use explicit dependencies if provided
    }
    LOG_DEBUG("Started join simulation for node " << join_node->path << " with strategy " << join_merge_strategy_);
}

void TopoScheduler::finish_join_simulation(Context& main_context) {
    if (current_fork_branch_results_.size() != current_fork_branches_.size()) {
        throw std::runtime_error("JoinNode: Not all fork branches have results for merging.");
    }

    LOG_DEBUG("Merging " << current_fork_branch_results_.size() << " branch results using strategy: " << join_merge_strategy_);

    if (!current_fork_branch_results_.empty()) {
        // Apply merge strategy iteratively
        ContextMergePolicy policy;
        policy.default_strategy = join_merge_strategy_;
        for (const auto& branch_ctx : current_fork_branch_results_) {
             ContextEngine::merge(main_context, branch_ctx, policy);
        }
    }

    // Clean up fork/join state
    current_join_node_path_.reset();
    current_fork_branch_results_.clear();
    join_wait_for_.clear();
    LOG_DEBUG("Finished join simulation for node " << current_join_node_path_.value());
}

void TopoScheduler::load_graphs(const std::vector<std::unique_ptr<Node>>& nodes) {
    // Sprint 18 D-3: 拆分单节点 clone+register 到 parse_single_node_spec helper
    // 备注: 此方法目前不在运行时调用路径上 (动态加载走 append_dynamic_graphs),
    // 但保留作为初始 setup 接口供未来使用
    for (const auto& node_ptr : nodes) {
        register_node(parse_single_node_spec(*node_ptr, ""));
    }
}

std::unique_ptr<Node> TopoScheduler::parse_single_node_spec(const Node& node_spec, const std::string& graph_id) {
    (void)graph_id; // 预留: graph_id 可用于将来按图分组追踪
    return node_spec.clone();
}

std::optional<ExecutionResult> TopoScheduler::prepare_dag_state(DagState& state) {
    std::optional<NodePath> entry_point;
    if (full_graphs_) {
        for (const auto& graph : *full_graphs_) {
            if (graph.path == "/__meta__" && graph.metadata.contains("entry_point")) {
                entry_point = graph.metadata["entry_point"].get<std::string>();
                break;
            }
            if (graph.path == "/main" && graph.metadata.contains("entry")) {
                entry_point = graph.path + "/" + graph.metadata["entry"].get<std::string>();
                break;
            }
        }
    }

        if (entry_point.has_value()) {
            std::queue<NodePath> empty;
            ready_queue_.swap(empty);
            if (node_map_.count(entry_point.value()) == 0) {
                return ExecutionResult{false, "Entry point not found: " + entry_point.value(), Context{}, std::nullopt};
            }
            ready_queue_.push(entry_point.value());
        }
        return std::nullopt;
    }

std::variant<std::monostate, TopoScheduler::NodeLookupResult, ExecutionResult>
TopoScheduler::dispatch_ready_nodes(DagState& state, const Context& context) {
    (void)state; // Sprint 7 Day 6: state 参数预留, Day 7-8 实施真实纯函数化迁移到 state.*
    // 注意: fork 分支处理已在 execute() L161-167 完成 (主 while 循环每次迭代开始时调用)。
    // 此函数仅负责派发 ready_queue 中的下一个节点, 不重复处理 fork 状态。
    // Sprint 7 Day 1: 去除与 execute() 重复的 fork 处理块 (Oracle ses_112a9f9c5ffesqpYeefOBgMkjH 决议)

    if (!ready_queue_.empty() && !is_executing_fork_branches_) {
        NodePath current_path = ready_queue_.front();
        ready_queue_.pop();
        auto node_it = node_map_.find(current_path);
        if (node_it == node_map_.end()) {
            return ExecutionResult{false, "Node not found in map: " + current_path, context, std::nullopt};
        }
        return NodeLookupResult{current_path, node_it->second};
    }

    if (!ready_queue_.empty() && is_executing_fork_branches_ &&
        current_fork_branch_index_ == current_fork_branches_.size()) {
        std::unordered_set<NodePath> dummy_executed;
        session_.check_and_requeue_dynamic_deps(dummy_executed);
        if (!ready_queue_.empty()) {
            NodePath current_path = ready_queue_.front();
            ready_queue_.pop();
            auto node_it = node_map_.find(current_path);
            if (node_it == node_map_.end()) {
                return ExecutionResult{false, "Node not found in map: " + current_path, context, std::nullopt};
            }
            return NodeLookupResult{current_path, node_it->second};
        }
    }

    if (!session_.get_pending_dynamic_deps().empty()) {
        return ExecutionResult{false,
            "Execution stopped: Unmet dynamic dependencies. Pending: " +
            nlohmann::json(session_.get_pending_dynamic_deps()).dump(),
            context, std::nullopt};
    }
    return std::monostate{};
}

ExecutionResult TopoScheduler::finalize_execution(DagState& state, const Context& context) {
    (void)state; // Sprint 7 Day 6: state 参数预留, Day 7-8 实施真实纯函数化迁移到 state.*
    if (session_.is_budget_exceeded()) {
        return {false, "Execution stopped: Budget exceeded", context, std::nullopt};
    }

    std::set<NodePath> all_node_paths;
    for (const auto& n : all_nodes_) {
        if (n->path.rfind("/__system__/", 0) == 0) continue;
        all_node_paths.insert(n->path);
    }
    std::set<NodePath> executed_sorted(executed_.begin(), executed_.end());
    std::set<NodePath> unexecuted;
    std::set_difference(all_node_paths.begin(), all_node_paths.end(),
                        executed_sorted.begin(), executed_sorted.end(),
                        std::inserter(unexecuted, unexecuted.begin()));

    if (!unexecuted.empty()) {
        return {false, "Execution stopped: Unmet dependencies or cycles. Unexecuted nodes: " +
                       nlohmann::json(unexecuted).dump(), context, std::nullopt};
    }

    return {true, "Execution completed successfully", context, std::nullopt};
}

std::optional<ExecutionResult> TopoScheduler::resolve_dynamic_waits(
    Node* current_node, const NodePath& current_path, const Context& context, bool& can_execute) {
    if (!current_node->metadata.contains("wait_for") || !current_node->metadata["wait_for"].is_string()) {
        return std::nullopt;
    }
    std::string dynamic_expr = current_node->metadata["wait_for"].get<std::string>();
    try {
        std::string rendered_deps_str = InjaTemplateRenderer::render(dynamic_expr, context);
        auto rendered_deps_json = nlohmann::json::parse(rendered_deps_str);
        std::vector<NodePath> rendered_deps;
        if (rendered_deps_json.is_array()) {
            for (const auto& item : rendered_deps_json) {
                if (item.is_string()) {
                    rendered_deps.push_back(item.get<std::string>());
                }
            }
        } else if (rendered_deps_json.is_string()) {
            rendered_deps.push_back(rendered_deps_json.get<std::string>());
        } else {
            rendered_deps.push_back(rendered_deps_str);
        }
        for (const auto& dep_path : rendered_deps) {
            if (executed_.count(dep_path) == 0) {
                can_execute = false;
                ready_queue_.push(current_path);
                break;
            }
        }
    } catch (const std::exception& e) {
        return ExecutionResult{false, "Failed to resolve dynamic wait_for for node '" + current_path + "': " + e.what(), context, std::nullopt};
    }
    return std::nullopt;
}

void TopoScheduler::process_fork_join(Node* current_node, Context& context) {
    start_join_simulation(dynamic_cast<const JoinNode*>(current_node));
    finish_join_simulation(context);
    finish_fork_simulation();
    LOG_DEBUG("Join completed, merged context.");
}

void TopoScheduler::rebuild_dynamic_graph(DagState& state) {
    if (dynamic_graphs_.empty()) return;
    state.dynamic_graphs.assign(
        std::make_move_iterator(dynamic_graphs_.begin()),
        std::make_move_iterator(dynamic_graphs_.end())
    );
    std::vector<std::unique_ptr<Node>> all_nodes_copy;
    for (auto& n : all_nodes_) {
        all_nodes_copy.push_back(n->clone());
    }
    for (auto& graph : state.dynamic_graphs) {
        for (const auto& node_ptr : graph.nodes) {
            if (node_ptr) {
                all_nodes_copy.push_back(node_ptr->clone());
            }
        }
    }
    node_map_.clear();
    reverse_edges_.clear();
    in_degree_.clear();
    all_nodes_ = std::move(all_nodes_copy);
    build_dag();
    // Sprint 18 D-1: rebuild_dynamic_graph 内部显式重迁 state
    state.nodes = node_map_;
    state.reverse_edges = reverse_edges_;
    state.wait_for_dependents = wait_for_dependents_;
    state.in_degree = in_degree_;
    state.executed.clear();
    while (!state.ready_queue.empty()) state.ready_queue.pop();
    state.ready_queue = ready_queue_;
}

void TopoScheduler::handle_fork_branches_block() {
    if (is_executing_fork_branches_) {
        execute_fork_branches();
        if (current_fork_branch_index_ == current_fork_branches_.size()) {
            LOG_DEBUG("Fork branches done, waiting for JoinNode.");
        }
    }
}

void TopoScheduler::handle_fork_node(Node* current_node, const Context& context) {
    const ForkNode* fork_node = dynamic_cast<const ForkNode*>(current_node);
    if (!fork_node) {
        throw std::runtime_error("Node type FORK but not ForkNode instance");
    }
    start_fork_simulation(fork_node, context);
}

bool TopoScheduler::check_end_termination(Node* current_node, const NodePath& current_path) {
    if (current_node->type != NodeType::END) return false;
    std::string mode = current_node->metadata.value("termination_mode", "hard");
    bool is_system_node = current_path.rfind("/__system__/", 0) == 0;
    if (mode == "hard" && !is_executing_fork_branches_ && !is_system_node) {
        return true;
    }
    return false;
}

std::optional<ExecutionResult> TopoScheduler::handle_node_completion(
    DagState& state, const NodeResult& result, Node* current_node, const NodePath& current_path) {
    (void)state;
    if (!result.success) {
        for (const auto& dependent : wait_for_dependents_[current_path]) {
            (void)dependent;
        }
    }
    update_successors(current_node, current_path);
    return std::nullopt;
}

bool TopoScheduler::process_jump(const std::string& message, const NodePath& current_path) {
    if (message.find("Jumping to:") == std::string::npos) return false;
    size_t pos = message.find("Jumping to:");
    if (pos == std::string::npos) return false;
    NodePath target = message.substr(pos + 12);
    LOG_DEBUG("Node " << current_path << " failed assert, jumping to " << target);
    std::queue<NodePath> empty_queue;
    ready_queue_.swap(empty_queue);
    ready_queue_.push(target);
    return true;
}

void TopoScheduler::update_successors(Node* current_node, const NodePath& current_path) {
    for (const auto& next_path : current_node->next) {
        if (--in_degree_[next_path] == 0) {
            ready_queue_.push(next_path);
        }
    }
    for (const auto& dependent : wait_for_dependents_[current_path]) {
        if (--in_degree_[dependent] == 0) {
            ready_queue_.push(dependent);
        }
    }
}


} // namespace agenticdsl
