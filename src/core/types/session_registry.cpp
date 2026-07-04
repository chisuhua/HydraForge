// src/core/types/session_registry.cpp
// 功能描述：SessionRegistry 实现 (C11 Phase 5 Stage 1 Step 1)
//           6 个方法实现: 析构 + create_session + destroy_session +
//                         get_session + is_in_flight + list_sessions +
//                         generate_session_id (private)
// 设计依据：session_registry.h + ADR-0033 + ToolRegistry shared_mutex 模式
// 作者：AgenticDSL Phase 5 / Sprint 20 C11
// 最后修改日期：2026-07-04

#include "core/types/session_registry.h"

#include <atomic>        // std::atomic<std::uint64_t>
#include <chrono>        // std::chrono::steady_clock, std::this_thread::sleep_for
#include <cstdint>       // std::uint64_t
#include <cstdio>        // std::fprintf (stderr) — forced destroy warning
#include <mutex>         // std::unique_lock, std::shared_lock
#include <stdexcept>     // std::out_of_range, std::runtime_error
#include <thread>        // std::this_thread::sleep_for
#include <utility>       // std::move, std::make_unique

namespace agenticdsl {

// ============================================================
// 析构 (out-of-line, PIMPL-lite)
// ============================================================

SessionRegistry::~SessionRegistry() = default;

// ============================================================
// generate_session_id (private)
// ============================================================

std::string SessionRegistry::generate_session_id() const {
  // 静态原子计数器 — 线程安全递增 (无 mutex 依赖, 与 DomainWorkerPool::next_worker_ 风格一致)
  static std::atomic<std::uint64_t> counter{0};
  const std::uint64_t seq = counter.fetch_add(1, std::memory_order_relaxed);

  // steady_clock 时间戳 — 单调递增, 不受系统时间回拨影响
  const auto now = std::chrono::steady_clock::now().time_since_epoch();
  const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now).count();

  // 拼接: "sess-<counter>-<ts_ns>"
  return "sess-" + std::to_string(seq) + "-" + std::to_string(ns);
}

// ============================================================
// create_session (write path)
// ============================================================

std::string SessionRegistry::create_session(const SessionConfig& config) {
  const std::string id = generate_session_id();

  // 构造 UserSession (UserSession 构造签名: explicit UserSession(std::string user_id))
  auto session = std::make_unique<UserSession>(config.name);

  // 写锁: unique_lock<shared_mutex>
  std::unique_lock<std::shared_mutex> lock(mutex_);

  // 极小概率重复检测 (理论不可达, generate_session_id 保证唯一性)
  if (sessions_.find(id) != sessions_.end()) {
    throw std::runtime_error("SessionRegistry::create_session: session_id collision: " + id);
  }

  // 插入表 (unordered_map 插入不会使其他元素指针失效)
  sessions_.emplace(id, std::move(session));

  return id;
}

// ============================================================
// destroy_session (write path, with in-flight wait)
// ============================================================

void SessionRegistry::destroy_session(const std::string& id) {
  // Step 1: 若 in-flight, 等待最多 5 秒 (100ms 间隔 sleep+check 循环)
  //         等待期间释放锁, 允许其他线程读写 (避免持锁 sleep 死锁)
  constexpr int kMaxWaitMs = 5000;
  constexpr int kCheckIntervalMs = 100;
  int waited_ms = 0;

  while (waited_ms < kMaxWaitMs) {
    // 短暂读锁检查 in-flight 状态 (不持锁 sleep)
    {
      std::shared_lock<std::shared_mutex> lock(mutex_);
      auto it = sessions_.find(id);
      if (it == sessions_.end()) {
        // id 不存在 — 幂等 no-op
        return;
      }
      // 检查 in-flight: current_task_session != nullptr && status 不是 completed/failed
      UserSession* session = it->second.get();
      TaskSession* task = session->current_task_session();
      if (task == nullptr) {
        break;  // 无在执行任务, 可直接销毁
      }
      const std::string& status = task->status();
      if (status == "completed" || status == "failed") {
        break;  // 任务已结束, 可直接销毁
      }
      // 仍在执行, 继续等待
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(kCheckIntervalMs));
    waited_ms += kCheckIntervalMs;
  }

  // Step 2: 写锁销毁
  std::unique_lock<std::shared_mutex> lock(mutex_);
  auto it = sessions_.find(id);
  if (it == sessions_.end()) {
    // 并发场景: 等待期间已被另一线程销毁 — 幂等 no-op
    return;
  }

  // 检查是否仍 in-flight (等待超时场景)
  UserSession* session = it->second.get();
  TaskSession* task = session->current_task_session();
  bool still_running = (task != nullptr &&
                        task->status() != "completed" &&
                        task->status() != "failed");

  if (still_running) {
    // 强制销毁 — 记录到 stderr (不抛异常, 与 DomainWorkerPool 异常隔离风格一致)
    std::fprintf(stderr,
                 "[SessionRegistry] WARNING: force-destroying in-flight session %s "
                 "(waited %dms, status=%s)\n",
                 id.c_str(), waited_ms,
                 task ? task->status().c_str() : "unknown");
  }

  // 从 map 移除并析构 UserSession (unique_ptr 自动释放)
  sessions_.erase(it);
}

// ============================================================
// get_session (read path, throws if not found)
// ============================================================

UserSession& SessionRegistry::get_session(const std::string& id) {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  auto it = sessions_.find(id);
  if (it == sessions_.end()) {
    throw std::out_of_range("SessionRegistry::get_session: session not found: " + id);
  }
  return *(it->second);
}

// ============================================================
// is_in_flight (read path)
// ============================================================

bool SessionRegistry::is_in_flight(const std::string& id) const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  auto it = sessions_.find(id);
  if (it == sessions_.end()) {
    return false;  // session 不存在 — 视为非 in-flight
  }
  UserSession* session = it->second.get();
  TaskSession* task = session->current_task_session();
  if (task == nullptr) {
    return false;  // 无当前任务
  }
  const std::string& status = task->status();
  // in-flight 定义: 有 current_task_session 且 status 不是 completed/failed
  return (status != "completed" && status != "failed");
}

// ============================================================
// list_sessions (read path)
// ============================================================

std::vector<std::string> SessionRegistry::list_sessions() const {
  std::shared_lock<std::shared_mutex> lock(mutex_);
  std::vector<std::string> ids;
  ids.reserve(sessions_.size());
  for (const auto& [k, v] : sessions_) {
    ids.push_back(k);
  }
  return ids;
}

}  // namespace agenticdsl
