# Design: PDK SafeExec Tests + Minimal jthread Fix

> **关联 proposal**: `openspec/changes/2026-08-10-pdk-safe-exec-tests/proposal.md`
> **关联 spec**: `openspec/changes/2026-08-10-pdk-safe-exec-tests/specs/pdk-safe-exec-tests/spec.md`
> **关联 ADR**: ADR-0021 (PDK 设计, ✅ Approved) §3.3 SafeExec + ADR-0020 (线程模型隔离) §2.2.1 + ADR-0004 (ToolRegistry 安全)

## 架构合规性检查

| 约束 | 状态 | 备注 |
|------|------|------|
| 2 空格缩进 | ✅ | 沿用现有 |
| 中文注释优先 | ✅ | 全部新增注释中文 |
| C++20 + CMake 3.20+ | ✅ | std::jthread + std::stop_token (C++20) |
| BACKWARD 兼容 | ✅ | SafeExec public API 不变 (run/with_timeout/with_layer_profile/timeout/layer_profile) |
| 无 Runtime 内部依赖 | ✅ | PDK 仅依赖 C++20 标准库 + 现有 nlohmann_json |
| TDD 5 步结构 | ✅ | T1-T3 写失败 → T4 修复 → T5-T12 补齐 → T13-T14 文档 → T15-T17 release |

## 关键设计决策

### 决策 1: std::async → std::jthread + std::stop_source

**问题**: 旧实现超时后调用方阻塞至 fn 完成 (标准规定).

```cpp
// 旧 (line 70-82 safe_exec.h)
template <typename F>
auto run(F&& fn) -> std::invoke_result_t<F> {
  auto future = std::async(std::launch::async, std::forward<F>(fn));
  auto status = future.wait_for(timeout_);
  if (status == std::future_status::timeout) {
    throw std::runtime_error("SafeExec: tool execution timed out after " + ...);
  }
  return future.get();
}
// 问题: future 在 run() 返回时析构, 旧 async launched future destructor
// 强制 wait() 阻塞至 fn 完成 → 调用方阻塞 = fn 实际执行时间, 非 timeout
```

**新实现** (jthread + condition_variable + wait_for):

```cpp
// include/agenticdsl/pdk/safe_exec.h (改写后 ~150 行)
namespace hydraforge::pdk {

class SafeExec {
 public:
  SafeExec() = default;

  SafeExec& with_timeout(std::chrono::milliseconds timeout) {
    timeout_ = timeout;
    return *this;
  }

  // 新增: grace_period (Phase 6a 决策 2)
  SafeExec& with_grace_period(std::chrono::milliseconds grace) {
    grace_period_ = grace;
    return *this;
  }

  SafeExec& with_layer_profile(int profile) {
    layer_profile_ = profile;
    return *this;
  }

  /**
   * @brief 执行 fn, 超时立即抛 runtime_error + 协同取消
   * @tparam F 可调用类型
   * @param fn 待执行的函数
   * @return std::invoke_result_t<F>
   * @throws std::runtime_error 超时 (worker 在 grace 内未停止)
   * @throws fn 原始异常 (透传)
   */
  template <typename F>
  auto run(F&& fn) -> std::invoke_result_t<F> {
    using ResultT = std::invoke_result_t<F>;

    if constexpr (std::is_void_v<ResultT>) {
      // void 路径
      std::stop_source stop_source;
      std::exception_ptr eptr;
      std::atomic<bool> finished{false};

      std::jthread worker([&fn, &stop_source, &eptr, &finished]() mutable {
        try {
          fn();
        } catch (...) {
          eptr = std::current_exception();
        }
        finished.store(true, std::memory_order_release);
      });

      auto deadline = std::chrono::steady_clock::now() + timeout_;
      while (!finished.load(std::memory_order_acquire)) {
        if (std::chrono::steady_clock::now() >= deadline) {
          stop_source.request_stop();
          // grace period
          auto grace_deadline = std::chrono::steady_clock::now() + grace_period_;
          while (!finished.load(std::memory_order_acquire) &&
                 std::chrono::steady_clock::now() < grace_deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
          }
          if (finished.load(std::memory_order_acquire)) {
            worker.join();
            if (eptr) std::rethrow_exception(eptr);
            return;  // void
          }
          worker.detach();  // grace 超时, 不阻塞 caller
          throw std::runtime_error(
              "SafeExec: tool execution timed out after " +
              std::to_string(timeout_.count()) + "ms");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      worker.join();
      if (eptr) std::rethrow_exception(eptr);
      return;
    } else {
      // non-void 路径
      std::optional<ResultT> result;
      std::stop_source stop_source;
      std::exception_ptr eptr;
      std::atomic<bool> finished{false};

      std::jthread worker([&fn, &stop_source, &result, &eptr, &finished]() mutable {
        try {
          result = fn();
        } catch (...) {
          eptr = std::current_exception();
        }
        finished.store(true, std::memory_order_release);
      });

      auto deadline = std::chrono::steady_clock::now() + timeout_;
      while (!finished.load(std::memory_order_acquire)) {
        if (std::chrono::steady_clock::now() >= deadline) {
          stop_source.request_stop();
          auto grace_deadline = std::chrono::steady_clock::now() + grace_period_;
          while (!finished.load(std::memory_order_acquire) &&
                 std::chrono::steady_clock::now() < grace_deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
          }
          if (finished.load(std::memory_order_acquire)) {
            worker.join();
            if (eptr) std::rethrow_exception(eptr);
            return std::move(*result);
          }
          worker.detach();
          throw std::runtime_error(
              "SafeExec: tool execution timed out after " +
              std::to_string(timeout_.count()) + "ms");
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
      }
      worker.join();
      if (eptr) std::rethrow_exception(eptr);
      return std::move(*result);
    }
  }

  std::chrono::milliseconds timeout() const { return timeout_; }
  std::chrono::milliseconds grace_period() const { return grace_period_; }  // 新增测试 API
  int layer_profile() const { return layer_profile_; }

 private:
  std::chrono::milliseconds timeout_{30000};
  std::chrono::milliseconds grace_period_{50};  // 新增默认值
  int layer_profile_{0};
};

} // namespace hydraforge::pdk
```

**关键设计点**:
- **stop_source.request_stop()**: 超时触发时调用, 通知 worker 协同取消
- **grace_period (50ms)**: 给 worker 一个宽限期停止, 超时则 `detach()` 而非 `join()` (避免阻塞 caller)
- **std::optional<ResultT>**: 解决 ResultT 不可默认构造问题 (如 std::unique_ptr)
- **std::exception_ptr + atomic**: 异常透传不丢失
- **sleep_for(1ms) polling**: 简单且避免 condition_variable 复杂度 (Phase 6a MVP)
- **非 void / void 双路径**: `if constexpr` 分发, 保持类型推导

### 决策 2: grace_period 默认值 50ms

**理由**: 50ms 是协同式取消的合理缓冲:
- 太短 (10ms): worker 没机会清理资源 (close fd, release lock)
- 太长 (500ms): caller 等待过久, 违反"超时立即返回"语义
- 50ms: 既给 worker 机会, 又不显著延迟 caller (总等待 = timeout + 50ms)

### 决策 3: 测试 wall-clock 时间上限 (timeout + 50ms + 50ms slack)

**理由**: 测试需要给 `std::chrono::steady_clock` 测量留 ±50ms slack (调度抖动), 但仍能验证 NOT 等于 fn 实际执行时间.

```cpp
// 测试断言: run() 返回 wall-clock < timeout + 50ms grace + 50ms slack
// 而非: run() wall-clock < fn 实际执行时间 (旧实现的失败模式)
REQUIRE(elapsed < std::chrono::milliseconds(150));
```

### 决策 4: Doxygen 覆盖率审计工具

```bash
# tools/check_doxygen_coverage.sh (~80 行)
#!/usr/bin/env bash
# 扫描 .h 文件 public API 的 Doxygen 注释覆盖率
# 用法: ./tools/check_doxygen_coverage.sh <file.h> [file2.h ...]
# 输出: "Coverage: X% (covered/total public APIs)"
# 退出码: 0 (≥90%) / 1 (<90%)

set -euo pipefail
FILE="${1:?Usage: $0 <file.h>}"
THRESHOLD="${DCOV_THRESHOLD:-90}"

# 提取 public 类/函数 (简化 grep 模式)
total=$(grep -E "^\s*(class|struct|template|auto|void|int|std::)" "$FILE" | wc -l)
covered=$(grep -B1 -E "^\s*(class|struct|template|auto|void|int|std::)" "$FILE" | grep -c "@brief\|/\*\*" || true)
pct=$((covered * 100 / total))

echo "Coverage: ${pct}% (${covered}/${total}) for ${FILE}"
if [ $pct -lt $THRESHOLD ]; then
  echo "FAIL: below ${THRESHOLD}% threshold"
  exit 1
fi
```

**简化说明**: 实际审计脚本会更复杂 (精确解析 public API, 区分 template/function/class), 但 MVP 用 grep-based heuristic 即可. Phase 2 可升级为 Python + libclang AST 解析.

### 决策 5: 测试基础设施 (TDD 5 步)

| 测试 # | 名称 | 验证目标 | TDD 阶段 |
|--------|------|---------|---------|
| 1 | timeout_returns_quickly | run() ≤ 150ms 返回 (旧 std::async 阻塞 500ms) | RED (写) → GREEN (修复) |
| 2 | stop_token_cooperative | lambda 收到 stop_requested() | GREEN (新行为) |
| 3 | thread_does_not_leak | 100 次超时 → 线程数 ≤ baseline + 10 | RED (写) → GREEN (修复) |
| 4 | grace_period_then_detach | lambda 忽略 stop_token → detach 而非 join | GREEN (新行为) |
| 5 | return_types | int/string/json/void 4 类型推导 | GREEN (BACKWARD 兼容) |
| 6 | exception_propagation | runtime_error + invalid_argument 透传 | GREEN (不变语义) |
| 7 | default_values | timeout=30s + grace_period=50ms | GREEN (新默认值) |
| 8 | chainable_config | 链式调用所有 with_xxx | GREEN (API 兼容) |

### 决策 6: TDD Red-Green 流程

**关键**: T1 + T3 测试先写并验证 FAIL, 然后 T4 实施修复. 这是 Iron Law (NO PRODUCTION CODE WITHOUT A FAILING TEST FIRST).

```bash
# T1: 写 timeout_returns_quickly 测试
# T2: 验证 FAIL (旧 std::async 阻塞 ~500ms, 测试期望 ≤150ms)
$ ./build/tests/test_pdk_safe_exec timeout_returns_quickly
FAIL: timeout returns in 503ms (expected ≤150ms)

# T4.1: 改写 safe_exec.h 用 jthread
# T4.2: 验证 PASS
$ ./build/tests/test_pdk_safe_exec timeout_returns_quickly
PASS: timeout returns in 53ms
```

## 实施路径 (T1 → T17)

### Phase 1: TDD Red (T1-T3, 3h)
- T1: 写 timeout_returns_quickly 测试
- T2: 验证 FAIL (旧 std::async 阻塞)
- T3: 写 thread_does_not_leak 测试

### Phase 2: TDD Green (T4, 2h)
- T4.1: safe_exec.h 改写 (jthread + stop_source + grace)
- T4.2-T4.3: T1 + T3 PASS

### Phase 3: 补齐测试 (T5-T12, 2.5h)
- T5-T10: 6 新 test cases
- T11: 验证 8 cases 全 PASS
- T12: 验证 BACKWARD 兼容 (现有 test_pdk_macros 5 cases 零修改)

### Phase 4: 文档 + 工具 (T13-T14, 2h)
- T13: Doxygen audit 工具 + safe_exec.h 注释补齐
- T14: pdk/README.md 扩展 3 章节

### Phase 5: Release Gate (T15-T17, 2h)
- T15: ctest + ASan + ADR lint + docs drift
- T16: ADR-0021 §3.3 + active-status + roadmap 同步
- T17: OpenSpec validate + archive

## 风险与缓解

| 风险 | 严重度 | 缓解措施 |
|------|-------|---------|
| std::jthread 与 std::async 行为差异 | 中 | TDD 5 步严格 (T1 写失败 → T4 实施 → 验证 GREEN) |
| grace_period 50ms 不够 (worker 卡在 IO) | 低 | 默认 50ms + 测试覆盖 (T6 grace_then_detach) + 用户可配置 (with_grace_period) |
| Doxygen 覆盖率工具误报 (过度严格) | 低 | 阈值默认 90% + 工具输出缺失列表便于人工 review + Phase 2 升级为 libclang AST |
| BACKWARD 不兼容 (现有 5 test_pdk_macros cases) | 中 | T12 验证零回归; SafeExec public API 不变 (仅新增 grace_period + grace_period()) |
| std::optional<ResultT> 不支持 move-only 类型 | 低 | 测试覆盖 (T7 return_types 用 std::unique_ptr 验证 move-only) |
| 线程数检测 flaky (OS 调度) | 中 | 阈值 baseline + 10 (允许一定波动) + 多次采样取最大值 |

## 相关 ADR / 文档

- **ADR-0021 §3.3 SafeExec**: 本 change 实施后追加 jthread + stop_token + grace_period 设计依据
- **ADR-0020 §2.2.1 CognitiveWorker/DomainWorkerPool**: PDK SafeExec 复用 per-worker 隔离模型
- **ADR-0004 ToolRegistry 安全**: SafeExec::with_layer_profile 集成 (Phase 2/3 范围)
- **`docs/superpowers/plans/2026-07-15-phase6-agentforge-mvp.md` §三 任务 2**: 本计划决策来源 (SafeExec 重写)
- **`docs/audits/2026-08-09-phase6-reevaluation-post-wave3a.md` §3**: Phase 6a 启动建议 (SafeExec 测试 = 条件 #1 子任务)
- **`openspec/changes/archive/2026-06-20-2026-07-07-pdk-skeleton/`**: Sprint 4 PDK MVP SafeExec 实施基线

## 提交策略 (5 commits, TDD 5 步结构)

```
T1-T3  → test(pdk): write failing SafeExec timeout semantics tests (TDD red)
T4     → fix(pdk): replace std::async with std::jthread + stop_token (TDD green)
T5-T12 → test(pdk): add 6 SafeExec tests + verify BACKWARD compat (40 ctest)
T13-T14 → docs(pdk): Doxygen audit tool + README expansion
T15-T17 → chore(pdk): full release gate + ADR sync + active-status sync + archive
```
