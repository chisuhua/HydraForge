// modules/scheduler/src/topo_scheduler.cpp
#include "scheduler/topo_scheduler.h"
#include "core/types/node.h"
#include "modules/scheduler/resource_manager.h" // Sprint 17 C.4: 完整类型 (PIMPL-lite 从头文件移出)
#include "common/llm/llm_types.h" // C₁.3: 需要完整 ILLMProvider 定义
#include "common/utils/template_renderer.h"
#include "common/log/log.h"        // agenticdsl::log 日志门面（tech-debt-and-doc-cleanup）
#include <taskflow/taskflow.hpp>  // C2 Day 1-2: tf::Executor + tf::Taskflow (完整定义, 避开 TBB 在头文件中与 std::queue 冲突)
// Sprint 19 D-8: PIMPL-lite — 头文件不再拖入以下完整类型
#include "modules/context/context_engine.h" // ContextEngine::merge + get_snapshot (topo_scheduler.cpp 调用点)
#include "common/policy/approval_handler.h" // ApprovalHandler : public IApprovalHandler (set_approval_handler 参数转换需要完整定义)
#include "modules/trace/trace_exporter.h" // get_last_traces() 内联函数 (topo_scheduler.h:62) 调用 TraceExporter::get_traces
#include <stdexcept>
#include <algorithm>
#include <mutex>
#include <set>
#include <queue>
#include <string_view>
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
      session_("", std::move(config.initial_budget), tool_registry, llm_provider, *resource_manager_,
               full_graphs_, config.execution_flags,
               [this](std::vector<ParsedGraph> graphs) { this->append_dynamic_graphs(std::move(graphs)); }) { // Pass callback to ExecutionSession
    // ADR-0031 (2026-07-31): 传递审批处理器到执行会话
    if (config.approval_handler) {
        session_.set_approval_handler(config.approval_handler);
    }
    // C4 Sprint 14 (ADR-0031 P3-P4): 传递 ToolCoordinator 到执行会话
    if (config.tool_coordinator) {
        session_.set_tool_coordinator(config.tool_coordinator);
    }
    // tf-integration-coverage: 缓存 num_workers,execute_parallel() 读这个成员
    config_num_workers_ = config.num_workers;
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
            std::vector<NodePath> deps;
            collect_wait_for_deps(node->metadata["wait_for"], deps);

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

    // 2026-10-10-fork-join-dsl-real-parallel D4 fix (per design D4):
    //   fork→branch 边计为 in-edge (branch 节点 in_degree > 0).
    //   此前 parse_node_wait_for_deps 只处理 node->next + wait_for, fork 的 branches 字段
    //   未被 build_dag 引用, 导致 task_a/b/c in_degree=0 在 seed_initial_ready_queue 被 seed,
    //   而 start_fork_simulation 调 execute_fork_branches 又执行一次 (BUG-4 fix 后会暴露双执行).
    //   现在: fork→branch 显式边 → branch in_degree > 0 → seed 阶段跳过 → execute_fork_branches
    //   显式调度是唯一触发点, 防双执行.
    for (const auto& node_ptr : all_nodes_) {
        Node* node = node_ptr.get();
        if (node->type == NodeType::FORK) {
            const auto* fork_node = dynamic_cast<const ForkNode*>(node);
            if (fork_node && !fork_node->branches.empty()) {
                for (const auto& branch_path : fork_node->branches) {
                    if (node_map_.count(branch_path) == 0) {
                        throw std::runtime_error("Fork branch node not found: " + branch_path);
                    }
                    reverse_edges_[branch_path].push_back(node->path);
                    in_degree_[branch_path]++;
                }
            }
        }
    }
}

void TopoScheduler::collect_wait_for_deps(const nlohmann::json& wf, std::vector<NodePath>& deps) const {
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
            // Note: 'any_of' requires more complex scheduling logic; for basic topo,
            // treated as all_of. Full impl needs different scheduling model.
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

void TopoScheduler::copy_dag_state_to(DagState& state) const {
    // Sprint 18 D-5: DRY 化 7 字段状态迁移 (execute + execute_parallel + rebuild_dynamic_graph 共用)
    state.nodes = node_map_;
    state.reverse_edges = reverse_edges_;
    state.wait_for_dependents = wait_for_dependents_;
    state.in_degree = in_degree_;
    state.dynamic_graphs.clear();
    state.executed.clear();
    while (!state.ready_queue.empty()) state.ready_queue.pop();
    state.ready_queue = ready_queue_;
}

ExecutionResult TopoScheduler::execute(const Context& initial_context) {
    // Sprint 18 D-5/D-5.1: setup + 主循环委托给 run_main_loop
    Context context = initial_context;
    DagState state;
    if (auto early = prepare_dag_state(state); early.has_value()) return *early;
    build_dag();
    copy_dag_state_to(state);
    return run_main_loop(state, context);
}

ExecutionResult TopoScheduler::run_main_loop(DagState& state, Context& context) {
    // Sprint 18 D-5.1: 串行主调度循环 (execute() 内部主循环)
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

        // fix-fork-dsl-context-propagation BUG-2-A fix (per ADR-0089 v1.4 follow-up commit 83883bc):
        // FORK/JOIN 节点是调度语义 (非节点执行语义), 必须在 execute_node 之前路由.
        // 此前 line 214-220 的 post-execute 类型检查永远走不到, 导致 NodeExecutor 抛 logic_error.
        // 参考 line 192-204 YIELD 检查模式 — 但 YIELD 在 execute_node 后 (语义不同), FORK 必须在之前.
        // 2026-10-10-fork-join-dsl-real-parallel 修复 1+2 (BUG-2 JOIN 前移 + BUG-3 fork update_successors):
        //   - JOIN 节点也必须前移到 execute_node 之前 (否则 execute_node 抛 logic_error, JOIN 永远不可达)
        //   - FORK 路径调 handle_node_completion 推进 fork.next 边 (修 BUG-3: fork.next 死边)
        if (found.node->type == NodeType::FORK) {
            handle_fork_node(found.node, context);
            executed_.insert(found.path);
            // BUG-3: 调用 handle_node_completion 推进 fork.next 边 (例 fork→join_results),
            //   否则 join_results in_degree 永远不减 0, JOIN 节点永远不可达
            NodeResult fork_fake_result;
            fork_fake_result.success = true;
            if (auto err = handle_node_completion(state, fork_fake_result,
                                                 found.node, found.path); err) return *err;
            continue;
        }
        if (found.node->type == NodeType::JOIN) {
            // BUG-2: JOIN 是调度语义 (非节点执行语义), 前移到 execute_node 之前.
            //   此前 line 228-230 post-execute JOIN check 是死代码 (execute_node 先抛 logic_error)
            process_fork_join(found.node, context);
            executed_.insert(found.path);
            // JOIN 同样调 handle_node_completion 推进 next 边 (join_results.next = synthesize)
            NodeResult join_fake_result;
            join_fake_result.success = true;
            if (auto err = handle_node_completion(state, join_fake_result,
                                                 found.node, found.path); err) return *err;
            continue;
        }

        auto session_result = session_.execute_node(found.node, context);
        if (!session_result.success) {
            if (process_jump(session_result.message, found.path)) continue;
            return {false, session_result.message, context, session_result.paused_at};
        }

        context = std::move(session_result.new_context);

        // C12 §5.1: YIELD 暂停 — 序列化 DAG 状态, 写回 pending_yield_, 切换 SchedulerState
        auto pending_yield = session_.get_pending_yield();
        if (pending_yield.has_value()) {
            ready_queue_.push(found.path);
            YieldState snapshot;
            snapshot.module_path = found.path;
            snapshot.resume_context = serialize_dag_state();
            session_.set_pending_yield(snapshot);

            scheduler_state_ = SchedulerState::YIELDED;
            yielded_context_ = context;
            yielded_node_path_ = found.path;
            LOG_INFO("Scheduler YIELDED at " << found.path);
            return {true, "YIELDED at " + found.path, context, std::nullopt};
        }

        executed_.insert(found.path);

        // fix-fork-dsl-context-propagation BUG-2-A + 2026-10-10-fork-join-dsl-real-parallel BUG-2 fix:
        //   移除原 line 214-220 + line 228-230 的 post-execute 类型检查
        //   (FORK 已在 line 192-208 处理, JOIN 已在 line 209-220 处理, 此处保留注释避免误回滚).
        // if (found.node->type == NodeType::FORK) { handle_fork_node(...); continue; }  // ← removed
        // if (found.node->type == NodeType::JOIN) { process_fork_join(...); }  // ← removed
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
    // Sprint 18 D-4/D-5: 精简为 setup + 调用 execute_dag_loop 编排 (≤30 行)
    Context context = initial_context;
    DagState state;
    if (auto early = prepare_dag_state(state); early.has_value()) return *early;
    build_dag();
    copy_dag_state_to(state);

    if (!parallel_executor_) {
        // tf-integration-coverage: Config::num_workers 注入,0 退化到 hardware_concurrency
        size_t workers = config_num_workers_;
        if (workers == 0) {
            workers = std::max(1u, std::thread::hardware_concurrency());
        }
        parallel_executor_ = std::make_unique<tf::Executor>(workers);
    }
    if (!parallel_taskflow_) {
        parallel_taskflow_ = std::make_unique<tf::Taskflow>();
    }

    return execute_dag_loop(state, context);
}

ExecutionResult TopoScheduler::execute_dag_loop(DagState& state, const Context& context) {
    // Sprint 18 D-4: 拆自 execute_parallel — tf::Taskflow 构建 + run + 收集结果
    parallel_taskflow_->clear();
    std::unordered_map<NodePath, tf::Task> tf_tasks;
    std::vector<NodePath> locally_executed;
    std::mutex locally_executed_mutex;
    locally_executed.reserve(state.nodes.size());
    for (const auto& [path, _] : state.nodes) {
        tf_tasks[path] = parallel_taskflow_->emplace([this, path, &state, &locally_executed, &locally_executed_mutex]() {
            Context node_context;
            Node* current_node = state.nodes[path];
            auto session_result = session_.execute_node(current_node, node_context);
            if (!session_result.success) {
                if (process_jump(session_result.message, path)) return;
                return;
            }
            {
                std::lock_guard<std::mutex> lock(locally_executed_mutex);
                locally_executed.push_back(path);
            }
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
    dynamic_graphs_.insert(dynamic_graphs_.end(), std::make_move_iterator(new_graphs.begin()), std::make_move_iterator(new_graphs.end()));
    // 主 execute 循环会检查此列表并在需要时重建 DAG (rebuild_dynamic_graph)
}

void TopoScheduler::start_fork_simulation(const ForkNode* fork_node, const Context& fork_context_snapshot) {
    // fix-fork-dsl-context-propagation BUG-2-B fix (per ADR-0089 v1.4 follow-up commit 83883bc):
    // 此前注释 "The fork_context_snapshot is already saved by ExecutionSession" — 但 routing fix
    // (commit b51adea) 后 FORK 节点**不再**进 execute_node (handle_fork_node 在 execute_node 之前拦截),
    // 所以 ExecutionSession.save_snapshot 从未被调用 → execute_fork_branches 的 get_snapshot 返回 nullptr.
    // 必须显式保存 fork 前的 context snapshot, 含 user_input 等已注入 template variables,
    // 各 branch 节点通过 execute_fork_branches line 340 的 `*fork_snapshot` 深拷贝继承.
    // const_cast: save_snapshot 非 const 方法 (line 45) 而 get_context_engine() 是 const 返回 (line 111),
    // 临时绕过 const 限制. 后续 cleanup 可考虑将 save_snapshot 改为 const (snapshot 修改涉及内部状态,
    // 但 save_snapshot 语义上就是写操作, const 不合适). 用 mutable std 解决更优但需改 ContextEngine API.
    const_cast<agenticdsl::ContextEngine&>(session_.get_context_engine())
        .save_snapshot(fork_node->path, fork_context_snapshot);

    current_fork_node_path_ = fork_node->path;
    current_fork_branches_ = fork_node->branches; // Store the branches to execute
    current_fork_branch_results_.clear(); // Clear previous results if any
    current_fork_branch_index_ = 0;
    is_executing_fork_branches_ = true;
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

        Context branch_initial_ctx = *fork_snapshot; // Copy the snapshot

        try {
            // 2026-10-12-fork-join-dsl-completion-feedback D2: execute_single_branch 返回值
            // 从 Context 扩展为 pair<Context, NodePath> (final_ctx + 最后执行节点路径).
            auto [branch_final_ctx, last_node_path] = execute_single_branch(branch_path, branch_initial_ctx);

            // 2026-10-12-fork-join-dsl-completion-feedback D1: branch→main scheduler completion feedback.
            // 此前 execute_fork_branches 仅维护本地 branch_in_degree 副本, 不推进主调度器
            // in_degree_[consumer] (例 join_results), 导致 join_results 永远不可达 → e2e 必现
            // "Unmet dependencies or cycles" (Oracle ses_ed35f7205ffeCm6OlJTBrbVhtG Critical-1).
            auto last_node_it = node_map_.find(last_node_path);
            if (last_node_it != node_map_.end()) {
                // M1 修正: node_map_ 是 unordered_map<NodePath, Node*>, .second 已是 Node*, 无 .get().
                update_successors(last_node_it->second, last_node_path);
                // C2 修复 (Oracle 静态分析): branch 节点必须写入成员 executed_, 否则
                // finalize_execution (line 651-674) 计算 all_nodes_ - executed_ 时把 task_a/b/c
                // 误判未执行 → result.success = false 即便 in_degree_ 全部清零.
                executed_.insert(last_node_path);
            }

            current_fork_branch_results_.push_back(std::move(branch_final_ctx));
            current_fork_branch_index_++;
        } catch (const HardEndException& e) {
            // 硬终点: 向主执行循环传播终止
            throw;
        }

        LOG_DEBUG("Branch " << branch_path << " completed. Result stored. Branch " << current_fork_branch_index_ << " / " << current_fork_branches_.size() << " done.");
    }

    // All branches executed, simulation phase is done for fork
    // The join logic will be handled when the corresponding JoinNode is encountered
    LOG_DEBUG("All fork branches completed. Ready for join.");
}
std::pair<Context, NodePath> TopoScheduler::execute_single_branch(const NodePath& branch_path, const Context& initial_context) {
    NodePath start_node_path = find_branch_start_node(branch_path);
    LOG_DEBUG("Found start node for branch " << branch_path << ": " << start_node_path);

    auto [branch_ready_queue, branch_executed, branch_in_degree, branch_reverse_edges] =
        init_branch_state(branch_path);

    Context current_ctx = initial_context;
    // 2026-10-12-fork-join-dsl-completion-feedback D2: 跟踪分支最后执行的节点路径,
    // 供 caller (execute_fork_branches) 调 update_successors 推进 consumer in_degree.
    NodePath last_node_path;
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
        last_node_path = current_path;  // D2: 每次 execute_node 成功后更新

        if (process_branch_end_node(node, current_path, branch_path, current_ctx)) {
            break;
        }

        for (const auto& next_path : node->next) {
            if (node_map_.count(next_path) > 0 && --branch_in_degree[next_path] == 0) {
                branch_ready_queue.push(next_path);
            }
        }
    }

    return {std::move(current_ctx), last_node_path};
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

  // 2026-10-10-fork-join-dsl-real-parallel D4 fix 协同:
  //   parse_node_wait_for_deps 加了 fork→branch 边, 防止 seed_initial_ready_queue 误 seed branches.
  //   但 init_branch_state 也用 in_degree_, 会导致 branch 节点 in_degree=1 (来自 fork→branch 边),
  //   不被 seed. 协同修复: 在 init_branch_state 内, branch_path 自身 (或其在当前 fork 分支内的根节点)
  //   必须被 seed, 不论 in_degree. 先找 start node 再 seed.
  NodePath branch_root = find_branch_start_node(branch_path);
  if (!branch_root.empty()) {
    branch_ready_queue.push(branch_root);
  }

  // 其它分支内节点: 在 branch subgraph 内可达, 才 seed (即分支内还有分支或 wait_for 等)
  for (const auto& [path, node] : node_map_) {
    if (path == branch_root) continue;  // 已 seed
    if (path.rfind(branch_path + "/", 0) == 0 || path == branch_path) {
      if (branch_in_degree[path] == 0) {
        branch_ready_queue.push(path);
      }
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
    // 2026-10-10-fork-join-dsl-real-parallel Critical-2 fix (per Oracle post-impl verdict):
    //   capture join path before reset — LOG_DEBUG 宏经 format_msg lambda **先求值**再 emit level gate,
    //   即 .value() 无条件执行. optional 为空 → bad_optional_access 抛. 必炸点:
    //   pre-fix 是死代码 (execute_node 先抛 logic_error), BUG-2 前移把 join 路由短路路径激活后
    //   第一个跑通 join 的用例立即撞这里. 修后 join path 字符串缓存, reset 后日志安全.
    std::string join_path_str = current_join_node_path_.has_value()
                                    ? current_join_node_path_.value()
                                    : std::string("<unknown>");
    current_join_node_path_.reset();
    current_fork_branch_results_.clear();
    join_wait_for_.clear();
    LOG_DEBUG("Finished join simulation for node " << join_path_str);
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

    auto pop_ready_node = [&context, this]() -> std::variant<std::monostate, NodeLookupResult, ExecutionResult> {
        if (ready_queue_.empty()) return std::monostate{};
        NodePath current_path = ready_queue_.front();
        ready_queue_.pop();
        auto node_it = node_map_.find(current_path);
        if (node_it == node_map_.end()) {
            return ExecutionResult{false, "Node not found in map: " + current_path, context, std::nullopt};
        }
        return NodeLookupResult{current_path, node_it->second};
    };

    if (!ready_queue_.empty() && !is_executing_fork_branches_) {
        return pop_ready_node();
    }

    if (!ready_queue_.empty() && is_executing_fork_branches_ &&
        current_fork_branch_index_ == current_fork_branches_.size()) {
        std::unordered_set<NodePath> dummy_executed;
        session_.check_and_requeue_dynamic_deps(dummy_executed);
        auto result = pop_ready_node();
        if (!std::holds_alternative<std::monostate>(result)) return result;
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
    // 2026-10-10-fork-join-dsl-real-parallel Major-2 fix (per Oracle post-impl verdict):
    //   对齐 handle_fork_node:749-752 空检查. 若 type 标记与实例不符 (防御性), 不带检查的
    //   dynamic_cast 会传 nullptr 给 start_join_simulation, line 530 join_node->wait_for 空指针解引用.
    //   1 行对齐修复.
    const JoinNode* join_node = dynamic_cast<const JoinNode*>(current_node);
    if (!join_node) {
        throw std::runtime_error("Node type JOIN but not JoinNode instance");
    }
    start_join_simulation(join_node);
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
    all_nodes_.clear();
    // register_node 重建 node_map_ + all_nodes_（build_dag 依赖 node_map_ 校验 next 边）
    for (auto& n : all_nodes_copy) {
        register_node(std::move(n));
    }
    build_dag();
    // Sprint 18 D-5: 复用 copy_dag_state_to helper (替代原 7 行内联迁移)
    copy_dag_state_to(state);
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
    (void)result; // 失败路径由调用方处理, 此处仅做后继调度
    update_successors(current_node, current_path);
    return std::nullopt;
}

bool TopoScheduler::process_jump(const std::string& message, const NodePath& current_path) {
    static constexpr std::string_view kJumpMarker = "Jumping to:";
    size_t pos = message.find(kJumpMarker);
    if (pos == std::string::npos) return false;
    NodePath target = message.substr(pos + kJumpMarker.size());
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

nlohmann::json TopoScheduler::serialize_dag_state() const {
    nlohmann::json j;
    j["ready_queue"] = nlohmann::json::array();
    {
        std::queue<NodePath> copy = ready_queue_;
        while (!copy.empty()) {
            j["ready_queue"].push_back(copy.front());
            copy.pop();
        }
    }
    j["in_degree"] = nlohmann::json::object();
    for (const auto& [path, deg] : in_degree_) {
        j["in_degree"][path] = deg;
    }
    j["executed"] = nlohmann::json::array();
    for (const auto& path : executed_) {
        j["executed"].push_back(path);
    }
    return j;
}

void TopoScheduler::restore_dag_state(const nlohmann::json& j) {
    if (!j.is_object()) return;
    while (!ready_queue_.empty()) ready_queue_.pop();
    if (j.contains("ready_queue") && j["ready_queue"].is_array()) {
        for (const auto& item : j["ready_queue"]) {
            if (item.is_string()) ready_queue_.push(item.get<std::string>());
        }
    }
    in_degree_.clear();
    if (j.contains("in_degree") && j["in_degree"].is_object()) {
        for (auto it = j["in_degree"].begin(); it != j["in_degree"].end(); ++it) {
            if (it.value().is_number()) in_degree_[it.key()] = it.value().get<int>();
        }
    }
    executed_.clear();
    if (j.contains("executed") && j["executed"].is_array()) {
        for (const auto& item : j["executed"]) {
            if (item.is_string()) executed_.insert(item.get<std::string>());
        }
    }
}

ExecutionResult TopoScheduler::resume_yield(const Context& updated_context) {
    if (scheduler_state_ != SchedulerState::YIELDED) {
        return {false, "resume_yield called but scheduler not in YIELDED state (current=" +
                       std::to_string(static_cast<int>(scheduler_state_)) + ")",
                yielded_context_, std::nullopt};
    }
    auto pending_opt = session_.get_pending_yield();
    if (!pending_opt.has_value()) {
        return {false, "resume_yield: pending_yield_ missing", yielded_context_, std::nullopt};
    }
    YieldState pending = std::move(*pending_opt);

    restore_dag_state(pending.resume_context);

    Context merged = yielded_context_;
    for (auto it = updated_context.begin(); it != updated_context.end(); ++it) {
        merged[it.key()] = it.value();
    }

    session_.clear_pending_yield();
    scheduler_state_ = SchedulerState::RUNNING;
    yielded_context_ = Context{};
    yielded_node_path_.clear();

    DagState state;
    copy_dag_state_to(state);
    return run_main_loop(state, merged);
}


} // namespace agenticdsl
