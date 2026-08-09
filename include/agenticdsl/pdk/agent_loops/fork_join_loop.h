// include/agenticdsl/pdk/agent_loops/fork_join_loop.h
// 文件头注释
// 功能描述：ForkJoinLoop — PDK Agent 三阶段并行分支循环 (Sprint 20 实施, ADR-0021 §3.2)。
//          状态机: Forking → Executing → Joining → Done
//          Fork 阶段: DomainWorkerPool::submit_task 并发派发 N 个 branch 任务 (Sprint 3 复用)
//          Executing 阶段: DomainWorkerPool jthread workers 并行执行 (Sprint 3 内置)
//          Joining 阶段: 合并 branch 结果到 LayeredContext.working (branch_id 排序 + 后覆盖前)
//          错误处理: 1 个 branch 失败 → 整体失败 (fail-fast)
//          通过 IInteractionBus (Sprint 2 InMemoryBus) 订阅 domain.task.* 事件同步等待
// 设计依据：ADR-0021 §3.2 + ADR-0020 §3.2 DomainWorkerPool + ADR-0019 IInteractionBus
//          + ADR-0008 LayeredContext + openspec/changes/pdk-plan-execute-fork-join
// 作者：AgenticDSL Phase 1 Sprint 20
// 最后修改日期：2026-08-01

#pragma once

#include "agenticdsl/contract/iinteraction_bus.h"
#include "agenticdsl/cognitive/domain_worker_pool.h"
#include "agenticdsl/pdk/agent_loops/loop_result.h"
#include "agenticdsl/types/layered_context.h"
#include "core/engine.h"
#include "core/types/tool_result.h"

#include <nlohmann/json.hpp>

#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace hydraforge::pdk {

/**
 * @brief ForkJoinLoop — PDK Agent 三阶段并行分支循环 (Sprint 20)
 *
 * 状态机 (4 状态):
 *   Forking → Executing → Joining → Done (成功路径, 全部 branch 成功)
 *   Forking / Executing → Done (失败, fail-fast: 1 branch 失败立即返回)
 *
 * Fork 阶段契约:
 *   - 创建 DomainWorkerPool (num_threads 默认 4, >= branches.size())
 *   - 注册 "branch" 域处理器 (handler 默认返回 {branch_id, data: args})
 *   - start() + submit_task(branches.size())
 *   - 通过 InMemoryBus 订阅 "domain.task.completed" + "domain.task.failed"
 *   - 用 mutex + condition_variable 同步等待所有 branch 完成 (或任一失败)
 *
 * Executing 阶段契约:
 *   - 由 DomainWorkerPool jthread workers 内部处理 (Sprint 3)
 *   - 本类不直接管理 worker 线程生命周期
 *
 * Joining 阶段契约:
 *   - 按 branch_id 排序合并 (按传入 branches vector 的顺序)
 *   - LayeredContext.working["data"] 累积所有 branch 输出 (后覆盖前)
 *   - 任一 branch 失败 → LoopResult.success=false + failed_phase="Executing"
 *
 * 异常隔离:
 *   - DomainWorkerPool::process_task 内部 try-catch + catch(...) (Sprint 3)
 *   - branch handler 抛异常 → 单 branch 失败, 不影响其他 branch (Sprint 3 不变)
 */
class ForkJoinLoop {
 public:
  /**
   * @brief 4 状态机 (Forking / Executing / Joining / Done)
   */
  enum class State { Forking, Executing, Joining, Done };

  /**
   * @brief 构造 ForkJoinLoop
   * @param engine       DSLEngine unique_ptr (F7 顺序契约注入)
   * @param bus          IInteractionBus shared_ptr (订阅 domain.task.* 事件, 必传非空)
   * @param num_threads  DomainWorkerPool 工作线程数 (默认 4, >= 1)
   *
   * 契约:
   *   - bus 必须非空 (本循环依赖事件订阅同步)
   *   - num_threads 必须 >= 1
   *   - 构造时调用 engine_->set_interaction_bus(bus_) (F7)
   *   - 构造后 state_ == Forking, pool_ 状态 == idle (start() 时才 running)
   *
   * 异常:
   *   - bus 为空: std::invalid_argument
   *   - num_threads == 0: std::invalid_argument
   */
  ForkJoinLoop(std::unique_ptr<agenticdsl::DSLEngine> engine,
               std::shared_ptr<agenticdsl::IInteractionBus> bus,
               std::size_t num_threads = 4)
      : engine_(std::move(engine)),
        bus_(std::move(bus)),
        num_threads_(num_threads) {
    if (!bus_) {
      throw std::invalid_argument(
          "ForkJoinLoop: IInteractionBus must be non-null");
    }
    if (num_threads_ == 0) {
      throw std::invalid_argument(
          "ForkJoinLoop: num_threads must be > 0");
    }
    if (engine_ && bus_) {
      engine_->set_interaction_bus(bus_);
    }
    pool_ = std::make_unique<agenticdsl::DomainWorkerPool>(num_threads_, bus_);
    state_ = State::Forking;
  }

  /**
   * @brief 析构: 隐式 stop pool (若仍 running), unsubscribe token
   */
  ~ForkJoinLoop() {
    // DomainWorkerPool 析构时自身隐式 stop (TD-CW-02 模式, Sprint 3)
    // IInteractionBus 析构由调用方负责, 此处仅 unsubscribe 主动订阅
  }

  // 禁止拷贝/移动 (内部 mutex + condition_variable + unique_ptr<DomainWorkerPool>)
  ForkJoinLoop(const ForkJoinLoop&) = delete;
  ForkJoinLoop& operator=(const ForkJoinLoop&) = delete;
  ForkJoinLoop(ForkJoinLoop&&) = delete;
  ForkJoinLoop& operator=(ForkJoinLoop&&) = delete;

  /**
   * @brief 执行三阶段循环
   * @param branches branch 标识列表 (字符串, 作为 output_key + branch_id)
   * @param ctx      初始 LayeredContext (Join 阶段 base)
   * @return LoopResult
   *
   * 成功条件: 所有 branches 完成且无失败 → LoopResult.success=true
   * 失败条件:
   *   - branches 为空 → 立即 success=false (无任务可执行)
   *   - 任何 branch 失败 → fail-fast, success=false
   *   - DomainWorkerPool::submit_task 抛 logic_error (state 不对) → success=false
   *
   * final_context.working["data"] 包含:
   *   - 每个 branch 的输出按 branches 输入顺序插入
   *   - 重复 key 后 branch 覆盖前 branch
   */
LoopResult run(const std::vector<std::string>& branches,
               const agenticdsl::LayeredContext& ctx,
               std::stop_token token = {}) {
    LoopResult result;
    result.final_context = ctx;

    if (branches.empty()) {
      result.success = false;
      result.message = "ForkJoinLoop: branches list is empty";
      result.failed_phase = "Forking";
      state_ = State::Done;
      return result;
    }

    // Phase B Step 4: 取消 token early-exit
    if (token.stop_requested()) {
      result.success = false;
      result.message = "ForkJoinLoop: cancelled before forking";
      result.failed_phase = "Forking";
      state_ = State::Done;
      return result;
    }

    // === Fork 阶段: 准备 + 启动 pool + 订阅事件 ===
    state_ = State::Forking;

    // 共享 tracker: 同步等待所有 branch 完成
    struct Tracker {
      std::mutex mtx;
      std::condition_variable cv;
      std::map<std::string, agenticdsl::ToolResult> results;
      std::atomic<bool> any_failed{false};
      std::string first_failure_msg;
      std::atomic<bool> first_failure_recorded{false};
    };
    auto tracker = std::make_shared<Tracker>();
    tracker->results.clear();

    // 订阅 domain.task.completed
    size_t token_completed = bus_->subscribe(
        "domain.task.completed",
        [tracker](const agenticdsl::BusEvent& event) {
          std::string output_key =
              event.payload.meta.value("output_key", std::string{});
          if (output_key.empty()) return;
          {
            std::lock_guard<std::mutex> lock(tracker->mtx);
            tracker->results[output_key] = event.payload;
          }
          tracker->cv.notify_all();
        });

    // 订阅 domain.task.failed
    size_t token_failed = bus_->subscribe(
        "domain.task.failed",
        [tracker](const agenticdsl::BusEvent& event) {
          std::string output_key =
              event.payload.meta.value("output_key", std::string{});
          std::string err_msg =
              event.payload.meta.value("error_message", std::string{"unknown"});
          if (output_key.empty()) return;
          {
            std::lock_guard<std::mutex> lock(tracker->mtx);
            tracker->results[output_key] = event.payload;
            tracker->any_failed.store(true, std::memory_order_release);
            // 记录首个失败 (不覆盖已有, fail-fast 信息保留)
            bool expected = false;
            if (tracker->first_failure_recorded.compare_exchange_strong(
                    expected, true)) {
              tracker->first_failure_msg = err_msg;
            }
          }
          tracker->cv.notify_all();
        });

    // 注册 "branch" 域处理器 (默认 handler: 返回 {branch_id, data: arguments})
    // 简化: 每个 branch 返回 {"branch_id": <output_key>, "data": <arguments>}
    // (Phase 3 可扩展: handler 调用 SimpleCognitiveOrchestrator.process 执行 agent)
    pool_->register_domain_handler(
        "branch",
        [](const agenticdsl::DomainTask& task) -> nlohmann::json {
          return nlohmann::json{
              {"branch_id", task.output_key},
              {"data", task.arguments}};
        });

    // start pool + submit all branches
    try {
      pool_->start();
    } catch (const std::exception& e) {
      bus_->unsubscribe(token_completed);
      bus_->unsubscribe(token_failed);
      result.success = false;
      result.message =
          std::string("ForkJoinLoop: pool start failed: ") + e.what();
      result.failed_phase = "Forking";
      state_ = State::Done;
      return result;
    }

    state_ = State::Executing;

    try {
      for (const auto& branch : branches) {
        agenticdsl::DomainTask task;
        task.domain = "branch";
        task.tool_name = branch;
        task.output_key = branch;
        task.arguments = nlohmann::json{{"branch_input", branch}};
        pool_->submit_task(std::move(task));
      }
    } catch (const std::exception& e) {
      pool_->stop();
      bus_->unsubscribe(token_completed);
      bus_->unsubscribe(token_failed);
      result.success = false;
      result.message =
          std::string("ForkJoinLoop: submit failed: ") + e.what();
      result.failed_phase = "Forking";
      state_ = State::Done;
      return result;
    }

    // 等待所有 branch 完成 (成功或失败)
    {
      std::unique_lock<std::mutex> lock(tracker->mtx);
      tracker->cv.wait(lock, [&] {
        return tracker->results.size() >= branches.size() ||
               tracker->any_failed.load(std::memory_order_acquire) ||
               token.stop_requested();
      });

      // Phase B Step 4: token cancellation → pool stop
      if (token.stop_requested() &&
          tracker->results.size() < branches.size()) {
        result.success = false;
        result.message = "ForkJoinLoop: cancelled by stop_token";
        result.failed_phase = "Joining";
      }
    }

    if (token.stop_requested()) {
      pool_->stop();
      bus_->unsubscribe(token_completed);
      bus_->unsubscribe(token_failed);
      state_ = State::Done;
      return result;
    }

    // 清理 pool + 订阅
    // 注: pool_->stop() 内部已 join 所有 worker jthread, 保证 in-flight task 完成
    // bus_->unsubscribe() 立即移除订阅, in-flight callback 完成无副作用
    // (Sprint 3 DomainWorkerPool::stop 模式 + Sprint 2 IInteractionBus 行为)
    pool_->stop();
    bus_->unsubscribe(token_completed);
    bus_->unsubscribe(token_failed);

    // === Joining 阶段: 按 branch 顺序合并 ===
    state_ = State::Joining;
    result.total_steps = static_cast<int>(branches.size());

    // fail-fast: 任一失败 → 整体失败
    if (tracker->any_failed.load(std::memory_order_acquire)) {
      // 仍 merge 部分结果 (记录失败 branch 到 meta)
      for (const auto& branch : branches) {
        auto it = tracker->results.find(branch);
        if (it != tracker->results.end() && it->second.ok) {
          // 成功 branch 的输出仍写入 (便于诊断)
          if (it->second.data.contains(branch)) {
            result.final_context.working["data"][branch] =
                it->second.data[branch];
          }
        } else if (it != tracker->results.end()) {
          result.final_context.working["meta"]["failed_branches"].push_back(
              branch);
        }
      }
      result.success = false;
      result.message =
          "ForkJoinLoop: branch failed: " + tracker->first_failure_msg;
      result.failed_phase = "Executing";
      state_ = State::Done;
      return result;
    }

    // 全成功: 按 branches 输入顺序合并到 final_context.working.data
    // (重复 key 后覆盖前, 确定性顺序)
    for (const auto& branch : branches) {
      auto it = tracker->results.find(branch);
      if (it != tracker->results.end() && it->second.ok) {
        // handler 返回的 JSON 在 r.data[output_key]
        if (it->second.data.contains(branch)) {
          result.final_context.working["data"][branch] =
              it->second.data[branch];
        }
      }
    }

    result.success = true;
    result.message = "ForkJoinLoop: completed";
    state_ = State::Done;
    return result;
  }

  /**
   * @brief 当前状态 (测试用)
   */
  State state() const { return state_; }

 private:
  std::unique_ptr<agenticdsl::DSLEngine> engine_;
  std::shared_ptr<agenticdsl::IInteractionBus> bus_;
  std::size_t num_threads_;
  std::unique_ptr<agenticdsl::DomainWorkerPool> pool_;
  State state_ = State::Forking;
};

} // namespace hydraforge::pdk