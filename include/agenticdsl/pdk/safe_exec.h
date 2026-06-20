// include/agenticdsl/pdk/safe_exec.h
// 文件头注释
// 功能描述：SafeExec 沙箱执行封装 (ADR-0021 §3.3)。
//          MVP 实现仅支持超时控制 + 异常传播, Phase 2/3 扩展 fork/cgroups/seccomp。
//          使用 std::async + wait_for(timeout) 实现超时, 异常通过 future.get() 传播至调用方。
// 设计依据：ADR-0021 §3.3 + ADR-0004 ToolRegistry 安全模型 (layer profile 预留)
//          + openspec/changes/2026-07-07-pdk-skeleton
// 作者：AgenticDSL Phase 1 Sprint 4
// 最后修改日期：2026-06-19

#pragma once

#include <chrono>
#include <future>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace hydraforge::pdk {

/**
 * @brief SafeExec 沙箱执行封装 (MVP: 超时 + 异常, Phase 2/3: + fork/cgroups/seccomp)
 *
 * 链式配置:
 *   SafeExec().with_timeout(10ms).with_layer_profile(0).run(fn);
 *
 * 行为:
 *   - 超时: 抛 std::runtime_error("SafeExec: tool execution timed out after Nms")
 *   - 异常: 传播原异常至调用方 (future.get() 不包装)
 *   - 正常: 返回 invoke_result_t<F> (与 fn() 返回类型一致)
 *
 * MVP 限制: 无 fork/cgroups/seccomp 进程级隔离, 仅应用层超时。
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
   * @brief 设置 Layer profile (MVP no-op, Phase 2/3 集成 ADR-0004 权限)
   * @param profile layer 编号 (0 = 默认, Phase 2 扩展)
   * @return SafeExec& (链式调用)
   */
  SafeExec& with_layer_profile(int profile) {
    layer_profile_ = profile;
    return *this;
  }

  /**
   * @brief 执行 fn, 应用超时控制 + 异常传播
   * @tparam F 可调用类型
   * @param fn 待执行的函数
   * @return std::invoke_result_t<F> (与 fn() 返回类型一致)
   *
   * 异常:
   *   - 超时: std::runtime_error("SafeExec: tool execution timed out after Nms")
   *   - fn 抛异常: 原异常透传 (不包装)
   */
  template <typename F>
  auto run(F&& fn) -> std::invoke_result_t<F> {
    auto future = std::async(std::launch::async, std::forward<F>(fn));
    auto status = future.wait_for(timeout_);

    if (status == std::future_status::timeout) {
      throw std::runtime_error(
          "SafeExec: tool execution timed out after " +
          std::to_string(timeout_.count()) + "ms");
    }

    // 异常传播: future.get() 抛原异常
    return future.get();
  }

  /**
   * @brief 获取当前超时 (测试用)
   */
  std::chrono::milliseconds timeout() const { return timeout_; }

  /**
   * @brief 获取当前 layer profile (测试用)
   */
  int layer_profile() const { return layer_profile_; }

 private:
  std::chrono::milliseconds timeout_{30000};  // 默认 30s
  int layer_profile_{0};                       // MVP no-op
};

} // namespace hydraforge::pdk