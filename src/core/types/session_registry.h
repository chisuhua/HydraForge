// src/core/types/session_registry.h
// 功能描述：SessionRegistry — 线程安全的 UserSession 注册表 (C11 Phase 5 Stage 1 Step 1)
//           持有多个 UserSession (unordered_map<id, unique_ptr<UserSession>>),
//           通过 std::shared_mutex 实现 read-shared / write-exclusive 并发模型,
//           遵循 ToolRegistry (src/common/tools/registry.h) 与 DomainWorkerPool
//           (include/agenticdsl/cognitive/domain_worker_pool.h) 的并发模式.
// 设计依据：C11 proposal (Phase 5 Stage 1 Step 1) + ADR-0033 Session Hierarchy
//           + ToolRegistry shared_mutex 模式 (src/common/tools/registry.h)
// 关键决策：
//   - 非 singleton: 由 DSLEngine / 上层持有实例 (与 ToolRegistry 一致)
//   - shared_mutex: 多读少写场景 (list/get_sessions 频繁, create/destroy 低频)
//   - unique_ptr<UserSession>: PIMPL-lite, 析构 out-of-line (头文件不拖入完整类型)
//   - generate_session_id: steady_clock + 静态 atomic 计数器 (无外部 UUID 依赖)
// 作者：AgenticDSL Phase 5 / Sprint 20 C11
// 最后修改日期：2026-07-04

#ifndef AGENTICDSL_CORE_TYPES_SESSION_REGISTRY_H
#define AGENTICDSL_CORE_TYPES_SESSION_REGISTRY_H

#include <memory>          // std::unique_ptr
#include <shared_mutex>    // std::shared_mutex
#include <string>          // std::string
#include <unordered_map>   // std::unordered_map
#include <vector>          // std::vector

#include "agenticdsl/types/session_config.h"  // SessionConfig (public header)
#include "core/types/session.h"               // UserSession

namespace agenticdsl {

/**
 * @brief 线程安全的 UserSession 注册表 (C11 Phase 5 Stage 1 Step 1)
 *
 * 设计要点:
 *  - 持有 std::unordered_map<std::string, std::unique_ptr<UserSession>> sessions_
 *  - 并发模型: std::shared_mutex, read-shared / write-exclusive
 *    * 读路径 (get_session / is_in_flight / list_sessions): std::shared_lock
 *    * 写路径 (create_session / destroy_session): std::unique_lock
 *  - 生命周期: 由上层 (DSLEngine / 服务进程) 持有实例, 非 singleton
 *  - PIMPL-lite: 析构函数 out-of-line 定义 (头文件不引入 UserSession 完整类型析构链)
 *
 * 锁顺序约定 (CP.22 协议):
 *  - 仅持有一把 mutex_, 无嵌套获取, 无死锁风险
 *  - 持锁期间 MUST NOT 调用外部回调 (避免持锁递归)
 *
 * 异常安全:
 *  - create_session: 强保证 (map 插入失败时无副作用, 异常透传)
 *  - destroy_session: 强保证 (in-flight 等待 + 强制销毁, 不抛异常)
 *  - get_session: 若 id 不存在抛 std::out_of_range
 *  - is_in_flight / list_sessions: noexcept-friendly (仅读操作)
 */
class SessionRegistry {
 public:
  /// @brief 默认构造 — 空注册表
  SessionRegistry() = default;

  /// @brief 析构函数 (out-of-line, .cpp 中定义)
  ///  PIMPL-lite: unique_ptr<UserSession> + shared_mutex 需完整类型析构
  ~SessionRegistry();

  // 禁止拷贝/移动 (shared_mutex + unordered_map 不易移动)
  SessionRegistry(const SessionRegistry&) = delete;
  SessionRegistry& operator=(const SessionRegistry&) = delete;
  SessionRegistry(SessionRegistry&&) = delete;
  SessionRegistry& operator=(SessionRegistry&&) = delete;

  // === 写路径 (unique_lock) ===

  /**
   * @brief 创建新的 UserSession 并注册到表中
   * @param config Session 配置 (name/max_concurrent_tasks/timeout_ms/policy_mode/...)
   * @return 新分配的 session_id (timestamp + counter, 形如 "sess-<n>-<ts>")
   *
   * 行为:
   *  1. 生成全局唯一 session_id (generate_session_id)
   *  2. std::make_unique<UserSession>(config.name) 构造实例
   *  3. unique_lock 插入 sessions_ 表
   *  4. 返回 session_id
   *
   * 异常: 若 session_id 极小概率重复 (理论不可达, 由 generate_session_id 保证),
   *       抛 std::runtime_error
   */
  std::string create_session(const SessionConfig& config);

  /**
   * @brief 销毁指定 UserSession
   * @param id session_id (由 create_session 返回)
   *
   * 行为:
   *  1. 若 is_in_flight(id) == true: 等待最多 5 秒 (sleep+check 循环, 100ms 间隔)
   *  2. 等待结束后仍 in-flight: 强制销毁 (记录到 stderr, 不抛异常)
   *  3. unique_lock 下从 sessions_ 移除并析构 UserSession
   *  4. id 不存在: 静默 no-op (幂等)
   */
  void destroy_session(const std::string& id);

  // === 读路径 (shared_lock) ===

  /**
   * @brief 获取指定 UserSession 引用
   * @param id session_id
   * @return UserSession& 引用 (引用稳定性由 unique_ptr 指针保证, unordered_map
   *         插入/删除不会使其他元素指针失效)
   *
   * 异常: 若 id 不存在, 抛 std::out_of_range
   *
   * 注意: 返回的引用在调用方持有期间, 若另一线程调用 destroy_session(id)
   *       可能导致悬挂引用 — 调用方需自行保证生命周期对齐
   */
  UserSession& get_session(const std::string& id);

  /**
   * @brief 检查指定 session 是否正在执行 (in-flight)
   * @param id session_id
   * @return true 若 current_task_session() != nullptr 且 status() 不是
   *         "completed"/"failed"; false 若 session 不存在或已结束
   *
   * 并发: shared_lock 读, 不阻塞其他读, 阻塞写
   */
  bool is_in_flight(const std::string& id) const;

  /**
   * @brief 列出所有已注册的 session_id
   * @return session_id 字符串向量 (无序, 调用方不应依赖顺序)
   *
   * 并发: shared_lock 读
   */
  std::vector<std::string> list_sessions() const;

 private:
  /**
   * @brief 生成全局唯一 session_id
   * @return 形如 "sess-<counter>-<steady_clock_ns>" 的字符串
   *
   * 实现:
   *  - 静态 std::atomic<std::uint64_t> 计数器 (线程安全递增)
   *  - std::chrono::steady_clock::now().time_since_epoch().count() 时间戳
   *  - 拼接为 "sess-<counter>-<ts>"
   *
   * 无外部 UUID 库依赖 (YAGNI, 与 DomainWorkerPool next_worker_ 一致风格)
   */
  std::string generate_session_id() const;

  // === 成员 ===
  std::unordered_map<std::string, std::unique_ptr<UserSession>> sessions_;
  mutable std::shared_mutex mutex_;  // read-shared / write-exclusive
};

}  // namespace agenticdsl

#endif  // AGENTICDSL_CORE_TYPES_SESSION_REGISTRY_H
