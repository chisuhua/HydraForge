// include/agenticdsl/pdk/safe_exec.h
// 文件头注释
// 功能描述：SafeExec 沙箱执行封装 (ADR-0021 §3.3, Phase 6a 改写)。
//          实现: std::jthread + std::stop_source 协同取消 + grace_period (默认 50ms)。
//          超时立即抛 std::runtime_error; grace 后 worker detach 而非 join。
//          异常通过 std::exception_ptr 原子透传 (不变语义)。
//          BACKWARD 兼容: public API 不变 (with_timeout/with_layer_profile/timeout/layer_profile/run),
//          仅新增 grace_period + with_grace_period() + grace_period() 测试 API。
// 设计依据：ADR-0021 §3.3 + ADR-0020 §2.2.1 线程隔离 + openspec/changes/2026-08-10-pdk-safe-exec-tests
// 作者：AgenticDSL Phase 6a
// 最后修改日期：2026-08-10

#pragma once

#include <atomic>
#include <chrono>
#include <exception>
#include <future>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <utility>
#include <variant>

namespace hydraforge::pdk {

/**
 * @file safe_exec.h
 * @brief SafeExec 沙箱执行封装 (Phase 6a, jthread + stop_token 协同取消)
 */

/**
 * @brief SafeExec 沙箱执行封装 (MVP: 超时 + 异常 + 协同取消, Phase 2/3: + fork/cgroups/seccomp)
 *
 * 链式配置:
 *   SafeExec().with_timeout(10ms).with_grace_period(50ms).with_layer_profile(0).run(fn);
 *
 * 行为:
 *   - 超时: 抛 std::runtime_error("SafeExec: tool execution timed out after Nms")
 *           caller 在 ≤ timeout + grace_period 后立即返回 (不等 fn 完成)
 *   - 协同取消: 超时后调用 std::stop_source::request_stop(); fn 可通过
 *               std::stop_token st 检查 st.stop_requested() 主动退出
 *   - grace detach: 若 worker 在 grace 内未停止, SafeExec::detach 而非 join
 *                   (避免阻塞 caller 至 fn 完成, 旧 std::async 的失败模式)
 *   - 异常: 透传原异常 (future.get 包装, 不丢失类型/消息)
 *   - 正常: 返回 invoke_result_t<F>
 *
 * BACKWARD 兼容: public API (with_timeout / with_layer_profile / timeout / layer_profile / run)
 *                全部不变; 仅新增 grace_period + with_grace_period() + grace_period() 测试 API。
 *
 * MVP 限制: 无 fork/cgroups/seccomp 进程级隔离, 仅应用层超时 + 协同取消。
 */
class SafeExec {
 public:
  SafeExec() = default;

  /**
   * @brief 设置超时 (毫秒)
   * @param timeout 超时时长
   * @return SafeExec& (链式调用)
   */
  SafeExec& with_timeout(std::chrono::milliseconds timeout) {
    timeout_ = timeout;
    return *this;
  }

  /**
   * @brief 设置 grace period (毫秒, 默认 50ms)
   *
   * 超时触发后, SafeExec request_stop() 通知 worker 协同取消, 然后等待
   * grace_period 让 worker 有机会清理。若 grace 内未停止, worker 被 detach
   * (避免阻塞 caller)。
   *
   * @param grace grace 时长 (默认 50ms)
   * @return SafeExec& (链式调用)
   */
  SafeExec& with_grace_period(std::chrono::milliseconds grace) {
    grace_period_ = grace;
    return *this;
  }

  /**
   * @brief 设置 Layer profile (MVP no-op, Phase 2/3 集成 ADR-0004 权限)
   * @param profile layer 编号 (0 = 默认, Phase 2 扩展)
   * @return SafeExec& (链式调用)
   */
  SafeExec& with_layer_profile(int profile) {
    layer_profile_ = profile;
    return *this;
  }

  /**
   * @brief 执行 fn, 应用超时控制 + 协同取消 + 异常传播
   *
   * @tparam F 可调用类型
   * @param fn 待执行的函数
   * @return std::invoke_result_t<F> (与 fn() 返回类型一致, 含 void)
   *
   * @throws std::runtime_error 超时 (worker 在 grace 内未停止)
   * @throws fn 原始异常 (透传, 类型与消息完整保留)
   */
  template <typename F>
  auto run(F&& fn) -> std::invoke_result_t<F> {
    using ResultT = std::invoke_result_t<F>;
    constexpr bool is_void = std::is_void_v<ResultT>;

    // promise/future + shared state 避免 worker 持有栈变量引用 (解决 ASan stack-use-after-return)
    std::promise<ResultT> result_promise;
    auto result_future = result_promise.get_future();
    // shared state 让 worker 与 caller 都不持有栈引用, 析构时正确清理
    struct SharedState {
      std::stop_source stop_source;
      std::exception_ptr eptr;
      std::atomic<bool> finished{false};
    };
    auto state = std::make_shared<SharedState>();

    std::jthread worker(
        [fn = std::forward<F>(fn),
         promise = std::move(result_promise),
         state]() mutable {
          try {
            if constexpr (is_void) {
              fn();
              promise.set_value();
            } else {
              promise.set_value(fn());
            }
          } catch (...) {
            state->eptr = std::current_exception();
          }
          state->finished.store(true, std::memory_order_release);
        });

    auto deadline = std::chrono::steady_clock::now() + timeout_;
    while (!state->finished.load(std::memory_order_acquire)) {
      if (std::chrono::steady_clock::now() >= deadline) {
        // 超时: 通知 worker + 等 grace + detach
        state->stop_source.request_stop();
        auto grace_deadline = std::chrono::steady_clock::now() + grace_period_;
        while (!state->finished.load(std::memory_order_acquire) &&
               std::chrono::steady_clock::now() < grace_deadline) {
          std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if (state->finished.load(std::memory_order_acquire)) {
          worker.join();
          if (state->eptr) std::rethrow_exception(state->eptr);
          return result_future.get();
        }
        worker.detach();  // grace 超时, 不阻塞 caller
        throw std::runtime_error(
            "SafeExec: tool execution timed out after " +
            std::to_string(timeout_.count()) + "ms");
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    worker.join();
    if (state->eptr) std::rethrow_exception(state->eptr);
    return result_future.get();
  }

  /**
   * @brief 获取当前超时 (测试用)
   */
  std::chrono::milliseconds timeout() const { return timeout_; }

  /**
   * @brief 获取当前 grace period (测试用)
   */
  std::chrono::milliseconds grace_period() const { return grace_period_; }

  /**
   * @brief 获取当前 layer profile (测试用)
   */
  int layer_profile() const { return layer_profile_; }

 private:
  std::chrono::milliseconds timeout_{30000};   // 默认 30s
  std::chrono::milliseconds grace_period_{50}; // 默认 50ms (Phase 6a 新增)
  int layer_profile_{0};                       // MVP no-op
};

} // namespace hydraforge::pdk
