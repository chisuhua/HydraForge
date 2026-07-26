// src/modules/cognitive/cognitive_worker.cpp
// 文件头注释
// 功能描述：CognitiveWorker 完整实现 — 认知智能体工作线程（ADR-0020 §3.1）。
//          构造时强制注入 bus (F7), 显式状态机约束, 析构隐式 stop()+join (TD-CW-02),
//          内部委托 SimpleCognitiveOrchestrator 单轮 ReAct, 通过 error_code_from_string
//          bridge 将 SimpleCognitiveOrchestrator 9 处 legacy string 路径映射为
//          ErrorCode enum (TD-CW-03), 通过 IInteractionBus 推送 cognitive.task.* 事件。
// 设计依据：ADR-0020 §2.2.1 + §3.1 (amended) + ADR-0019 + ADR-0023 P2-P4
//          + openspec/changes/2026-06-23-cognitive-worker
// 作者：AgenticDSL Phase 1 Sprint 2
// 最后修改日期：2026-06-18

#include "agenticdsl/cognitive/cognitive_worker.h"

#include "agenticdsl/cognitive/simple_orchestrator.h"
#include "agenticdsl/contract/bus_event.h"
#include "core/engine.h"

#include <chrono>
#include <exception>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

namespace agenticdsl {

namespace {

// === TD-CW-03 Bridge: legacy meta["error_code"] string -> ErrorCode enum ===
//
// 背景: SimpleCognitiveOrchestrator.cpp 9 处 error 路径使用 ToolResult::error(string, string)
// deprecated 重载, 仅写入 meta["error_code"] 字符串, 不填充 result.error_code (P2 enum)。
// Sprint 2 在 worker_loop 中 bridge 一次, 满足 cognitive-worker-error-propagation 契约。
// Sprint 1c 后续: 扩展 ErrorCode enum + 升级 SimpleCognitiveOrchestrator 重载, 删除此 bridge。
//
// 映射表覆盖 SimpleCognitiveOrchestrator 当前 9 处 error 路径全部 (proposal §决策 7):
//   ERR_LLM.NETWORK           -> ErrorCode::Retry
//   ERR_LLM.RATE_LIMITED      -> ErrorCode::Retry
//   ERR_LLM.AUTH              -> ErrorCode::PermissionDenied
//   ERR_LLM.CANCELLED         -> ErrorCode::Abort
//   ERR_LLM.INVALID_REQUEST   -> ErrorCode::Unknown
//   ERR_LLM.SERVER            -> ErrorCode::Unknown
//   ERR_LLM.CONTEXT_OVERFLOW  -> ErrorCode::ResourceExhausted
//   ERR_ORCHESTRATOR.*        -> ErrorCode::Unknown
//   ERR_TOOL.NOT_FOUND        -> ErrorCode::ToolNotRegistered
//   ERR_TOOL.EXECUTION_FAILED -> ErrorCode::Unknown
//   ERR_TOOL.RUNTIME          -> ErrorCode::Unknown
// 未匹配字符串 -> nullopt (保持现状, meta["error_code"] 仍保留)
std::optional<ErrorCode> error_code_from_string(const std::string& s) {
  if (s == "ERR_LLM.NETWORK")           return ErrorCode::Retry;
  if (s == "ERR_LLM.RATE_LIMITED")      return ErrorCode::Retry;
  if (s == "ERR_LLM.AUTH")              return ErrorCode::PermissionDenied;
  if (s == "ERR_LLM.CANCELLED")         return ErrorCode::Abort;
  if (s == "ERR_LLM.INVALID_REQUEST")   return ErrorCode::Unknown;
  if (s == "ERR_LLM.SERVER")            return ErrorCode::Unknown;
  if (s == "ERR_LLM.CONTEXT_OVERFLOW")  return ErrorCode::ResourceExhausted;
  if (s == "ERR_TOOL.NOT_FOUND")        return ErrorCode::ToolNotRegistered;
  // ERR_ORCHESTRATOR.* / ERR_TOOL.EXECUTION_FAILED / ERR_TOOL.RUNTIME / 未知
  //  -> Unknown (无更精确 enum)
  return ErrorCode::Unknown;
}

} // namespace

// =====================================================================
// 构造: 强制注入 bus (F7 ordering), 校验非空
// =====================================================================
CognitiveWorker::CognitiveWorker(std::unique_ptr<DSLEngine> engine,
                                 std::shared_ptr<IInteractionBus> bus)
    : engine_(std::move(engine)), bus_(std::move(bus)) {
  if (!engine_) {
    throw std::invalid_argument("CognitiveWorker: engine must not be null");
  }
  if (!bus_) {
    throw std::invalid_argument("CognitiveWorker: bus must not be null");
  }
  // F7: 构造时立即将 bus 注入 engine, 保证后续 engine.run() / subscribe() 触发的
  // dsl.call.* / tool.* 事件通过 Worker 的 bus 转发 (一致性)
  engine_->set_interaction_bus(bus_);
}

// =====================================================================
// 析构: state_ == running 时隐式 stop() (TD-CW-02 修复)
// 必须 out-of-line 定义, 因为 unique_ptr<DSLEngine> 析构需 DSLEngine 完整类型
// =====================================================================
CognitiveWorker::~CognitiveWorker() {
  if (state_.load() == State::running) {
    // 隐式 stop(): 内部已 join thread (no hang)
    stop();
  }
  // 隐式析构成员按声明逆序:
  //   queue_cv_ / queue_mutex_ / task_queue_ / worker_thread_ / bus_ / engine_
  // engine_ 析构在最后, 需 DSLEngine 完整类型, 故 ctor/dtor 必须在 .cpp 定义
}

// =====================================================================
// start: state_ == idle -> running, 启动 std::thread
// =====================================================================
void CognitiveWorker::start() {
  State expected = State::idle;
  if (!state_.compare_exchange_strong(expected, State::running)) {
    throw std::logic_error(
        "CognitiveWorker::start: invalid state (expected idle, got " +
        std::string(expected == State::running ? "running" : "stopped") + ")");
  }
  worker_thread_ = std::thread([this] { worker_loop(); });
}

// =====================================================================
// submit_task: 加锁入队 + notify_one, 立即返回
// =====================================================================
void CognitiveWorker::submit_task(const std::string& task_id,
                                  const std::string& prompt) {
  if (state_.load() != State::running) {
    throw std::logic_error(
        "CognitiveWorker::submit_task: invalid state (Worker not running)");
  }
  {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    task_queue_.emplace(task_id, prompt);
  }
  queue_cv_.notify_one();
}

// =====================================================================
// stop: state_ == running -> stopped, 唤醒 worker, join thread
// =====================================================================
void CognitiveWorker::stop() {
  State expected = State::running;
  if (!state_.compare_exchange_strong(expected, State::stopped)) {
    // idle / stopped: no-op (幂等)
    return;
  }
  // 唤醒 worker_loop 退出 wait
  queue_cv_.notify_all();
  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }
}

// =====================================================================
// worker_loop: 阻塞消费任务, 委托 SimpleCognitiveOrchestrator, 转发事件
// =====================================================================
void CognitiveWorker::worker_loop() {
  while (state_.load() == State::running) {
    std::string task_id;
    std::string prompt;

    // 1) 阻塞等待任务 / stop 唤醒
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      queue_cv_.wait(lock, [this] {
        return !task_queue_.empty() || state_.load() != State::running;
      });
      if (state_.load() != State::running) {
        break;  // 优雅退出
      }
      auto front = std::move(task_queue_.front());
      task_queue_.pop();
      task_id = std::move(front.first);
      prompt = std::move(front.second);
    }

    // 2) 推送 cognitive.task.started 事件
    //    topic 遵循 <module>.<verb> 约定 (e.g. dsl.call.started)
    //    task_id 通过 meta["task_id"] 关联 (不嵌入 topic, 与现有约定一致)
    ToolResult started;
    started.ok = true;
    started.meta["task_id"] = task_id;
    bus_->emit(BusEvent{"cognitive.task.started", started, std::chrono::steady_clock::now()});

    // 3) 委托 SimpleCognitiveOrchestrator 单轮 ReAct
    //    注入 P1 抽象: engine_->get_tool_registry() (IToolRegistry&)
    //                 engine_->get_llm_provider() (ILLMProvider*)
    SimpleCognitiveOrchestrator orch(&engine_->get_tool_registry(),
                                     engine_->get_llm_provider());

    ToolResult result;
    orch.process(task_id, [&result](ToolResult r) { result = std::move(r); });

    // 4) TD-CW-03 Bridge: meta["error_code"] string -> ErrorCode enum
    //    失败时 SimpleCognitiveOrchestrator 仅写入 meta["error_code"] 字符串;
    //    bridge 后填充 result.error_code (P2 enum), 满足 spec 契约
    if (!result.ok && !result.error_code.has_value() &&
        result.meta.contains("error_code") &&
        result.meta["error_code"].is_string()) {
      result.error_code = error_code_from_string(
          result.meta["error_code"].get<std::string>());
    }

    // 5) P3 字段: result.trace_id = task_id (ADR-0023)
    //    约定: trace_id 为 caller-supplied 不透明字符串
    result.trace_id = task_id;

    // 6) 推送 cognitive.task.completed 事件
    bus_->emit(BusEvent{"cognitive.task.completed", result, std::chrono::steady_clock::now()});
  }
}

} // namespace agenticdsl
