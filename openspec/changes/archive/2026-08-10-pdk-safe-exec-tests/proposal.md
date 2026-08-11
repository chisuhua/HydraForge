# Proposal: PDK SafeExec Tests + Minimal jthread Fix

> **变更类型**: 真实实现 (测试驱动 + 最小语义修复 + Doxygen + 文档)
> **作者**: Sisyphus (Phase 6a 启动)
> **创建日期**: 2026-08-10
> **关联 plan**: `docs/superpowers/plans/2026-07-15-phase6-agentforge-mvp.md` §三 任务 2 (SafeExec 重写)
> **关联审计**: `docs/audits/2026-08-09-phase6-reevaluation-post-wave3a.md` §3 (Phase 6a 启动建议)
> **关联 ADR**: ADR-0021 (PDK 设计, 当前 ✅ Approved) + ADR-0020 (线程模型隔离) + ADR-0004 (ToolRegistry 安全)
> **前置**: Sprint 4 PDK Skeleton (已 ship, OpenSpec `2026-07-07-pdk-skeleton` archived) + Sprint 20 PlanExecute/ForkJoin (已 ship) + Wave 3-A chat-async-io 4-phase (已 ship)

## Why

Phase 6 Candidate B 服务化启动条件 #1 要求 "PDK 生产化", 其中 SafeExec 超时/异常隔离真实测试是 `docs/audits/2026-08-09-phase6-reevaluation-post-wave3a.md` §3.3 明确点名的 3 个子任务之一. 当前问题:

1. **超时语义不达预期**: `SafeExec::run()` 使用 `std::async(std::launch::async, fn)` + `future.wait_for(timeout)`. 标准规定 `std::async` launched async 的 `std::future` 析构会调用 `wait()` 阻塞至 fn 完成. 当 timeout 触发时, 调用方实际等待时间 = fn 实际执行时间 (非 timeout). 例: `with_timeout(50ms)` + `sleep_for(2s)` → 调用方阻塞 2s 而非 50ms. 这违反超时语义直觉, 在生产 PDK 用户 (AgentForge 领域 agent) 中可能引发 worker 池耗尽.

2. **PDK API 文档不完整**: `pdk/README.md` 当前仅列出 plugin 清单 + TemporalAgent 详情, 缺少 SafeExec 实战 + Agent Loop 选择指南 + AgentForge 衔接. `agentforge-mvp.md` §三 任务 3 + 任务 4 已识别为 W2-W3 范围但未 ship.

3. **Doxygen 覆盖率不可见**: `safe_exec.h` 当前 Doxygen 注释不完整 (类成员缺 `@brief`/`@tparam`), 无 CI/audit 工具度量.

**Sprint 24-25 范围内**:
- ✅ 写失败测试验证 `std::async` 超时语义缺陷 (确认非真正及时返回)
- ✅ 最小范围修复 SafeExec (`std::async` → `std::jthread` + `std::stop_token` 协同取消)
- ✅ 补齐 SafeExec 测试 (线程不泄漏 + stop_token 协同 + 类型兼容 + 异常传播)
- ✅ Doxygen 覆盖率审计工具 + 修复 safe_exec.h 注释
- ✅ PDK 开发者 README (pdk/README.md §SafeExec 实战 + §3 种 Loop + §AgentForge 衔接)
- ✅ 全量 ctest + ASan + ADR lint + docs drift audit
- ✅ Phase 6a roadmap + active-status 更新

**不解决此问题** (Phase 6a 后续 / Phase 6b 范围):
- ❌ 完整 SafeExec (fork/cgroups/seccomp, Phase 3)
- ❌ PluginLifecycle 类 (Phase 3)
- ❌ MockSandbox / FakeStateStore (Phase 2 测试替身)
- ❌ pdk_chat_demo v2 (Phase 6b)
- ❌ AgentForge 第 2 个领域 agent (Phase 6b 任务 5)

## What Changes

### 决策 1: SafeExec 重写 — `std::async` → `std::jthread` + `std::stop_token`

**问题**: 旧实现超时后调用方阻塞至 fn 完成 (标准规定的 future destructor wait 语义).

**方案**: 用 `std::jthread` 替代 `std::async`, 通过 `std::stop_source` 协同取消.

```cpp
// 新 SafeExec::run() (C++20 jthread + stop_token)
template <typename F>
auto run(F&& fn) -> std::invoke_result_t<F> {
  std::stop_source stop_source;
  std::atomic<bool> finished{false};
  std::atomic<std::invoke_result_t<F>> result{};
  std::exception_ptr eptr;

  std::jthread worker([&fn, &stop_source, &finished, &result, &eptr]() mutable {
    try {
      if constexpr (std::is_void_v<std::invoke_result_t<F>>) {
        fn();
        result.store(0);  // sentinel for void
      } else {
        result.store(fn());
      }
    } catch (...) {
      eptr = std::current_exception();
    }
    finished.store(true, std::memory_order_release);
  });

  // 等待 timeout 或 finished (whichever first)
  worker.join();  // 注: 实际实现用 condition_variable + wait_for(timeout)
  // ... 简化示意, 完整实现见 design.md
}
```

**关键设计点**:
- **jthread 取代 std::async**: jthread 析构默认 request_stop + join, 我们手动控制避免 join 阻塞
- **stop_token 协同**: lambda 可选择接收 `std::stop_token st`, `SafeExec::run()` 在超时时调用 `request_stop()`
- **grace_period (50ms)**: 超时后给 worker 一个停止宽限期, 超时则 detach (而非 join)
- **异常原子传播**: `std::exception_ptr` + `std::atomic<bool>` 保证 worker 抛异常时调用方能正确 rethrow
- **类型推导兼容**: `std::invoke_result_t<F>` 与原 `std::async` 推导一致, 调用方代码零修改

### 决策 2: SafeExec grace_period 可配置 (默认 50ms)

```cpp
class SafeExec {
 public:
  SafeExec& with_grace_period(std::chrono::milliseconds grace) {
    grace_period_ = grace;
    return *this;
  }
 private:
  std::chrono::milliseconds grace_period_{50};  // 默认 50ms
};
```

**理由**: 50ms grace 给协同式 cancel 一个合理缓冲 (network/IO 清理), 又不至于让 caller 等待过久. 测试可显式覆盖.

### 决策 3: SafeExec 不接受 stop_token 参数 (MVP 简化)

**方案**: `run(F&& fn)` 签名不变 (与 Sprint 4 MVP 一致). 内部用 `std::jthread` + `std::stop_source` 实现取消, 但 fn 不感知 stop_token. Phase 2 可扩展为 `run(F&& fn, std::stop_token outer = {})` 重载.

**理由**: BACKWARD 兼容 (5/5 test_pdk_macros 测试零修改通过), 满足 spec.md §safe-exec-timeout-semantics 要求.

### 决策 4: Doxygen 覆盖率审计工具 `tools/check_doxygen_coverage.sh`

**问题**: 当前无度量工具, safe_exec.h / pdk.h 注释覆盖率未知.

**方案**: 新建 shell 脚本, 静态扫描 `.h` 文件 public API 的 Doxygen 注释:

```bash
# tools/check_doxygen_coverage.sh
# 用法: ./tools/check_doxygen_coverage.sh include/agenticdsl/pdk/safe_exec.h
# 扫描:
#   - class / struct 声明前必须有 /** @brief ... */ 或 // 文件级注释
#   - 公共成员函数必须有 /** @brief 或 ///< */
#   - template <typename F> 必须有 @tparam F
# 输出: 覆盖率 % + 缺失项列表 + 退出码 0/1
```

**关键设计点**:
- **纯 shell + grep**: 无新依赖, 沿用 `tools/adr_lint.py` 风格
- **public API 范围**: 仅扫描 `class` / `struct` 的 public 成员 + 文件级注释
- **覆盖率阈值**: 默认 90% (spec 要求), 失败时退出码 1
- **集成**: scripts/sprint-closeout.sh Step 6/7 调用 (与 LSP discipline 模式一致)

### 决策 5: `pdk/README.md` 章节扩展

**问题**: 当前 pdk/README.md 仅列 plugin 清单 + TemporalAgent 详情, 无 SafeExec / Agent Loop / AgentForge 章节.

**方案**: 在 pdk/README.md 末尾追加 3 个章节 (与现有 TemporalAgent §并列):

```markdown
## SafeExec 实战

### 超时控制 (Stop Token 协同)

```cpp
#include "agenticdsl/pdk/safe_exec.h"
using namespace hydraforge::pdk;

auto result = SafeExec()
    .with_timeout(5s)
    .with_grace_period(50ms)
    .run([] {
      // 你的领域逻辑
      return compute_heavy();
    });
```

### 异常传播

[异常类型透传示例]

### 与 DECLARE_TOOL 组合

[5 行 DECLARE_TOOL + SafeExec 示例]

## 3 种 Agent Loop 选择指南

[React vs PlanExecute vs ForkJoin 决策树]

## AgentForge 衔接

[AgentForge MVP 通过 PDK 调用 DSLEngine 示例]
```

**关键设计点**:
- **章节顺序**: SafeExec 实战 → Loop 选择 → AgentForge 衔接 (逻辑递进)
- **代码片段**: 每章 1-3 个 ≤ 20 行可编译示例
- **向后兼容**: 现有 TemporalAgent 章节保留 (Phase 5 ship 内容)

### 代码侧

- `include/agenticdsl/pdk/safe_exec.h` (修改, ~95 → ~150 行) — jthread + stop_token 实现 + grace_period + 完整 Doxygen
- `tools/check_doxygen_coverage.sh` (新建, ~80 行) — Doxygen 覆盖率审计
- `tests/test_pdk_safe_exec.cpp` (新建, ~250 行) — 8 test cases (timeout, stop_token, leak, types, exceptions, grace, config chain, default)
- `pdk/README.md` (修改, 86 → ~200 行) — 追加 3 章节 (SafeExec 实战 + Loop 选择 + AgentForge 衔接)

### 文档侧

- `docs/adr/adr-0021-pdk-design.md` (修改) — §3.3 SafeExec 章节追加 jthread + stop_token 设计依据 + grace_period 默认值
- `docs/active-status.md` (修改) — §一 Quick 状态 + §六 Phase 6a 任务 2 标记完成 + §五 最近完成追加 ship
- `roadmap.md` (修改) — Phase 6a 任务 2 (SafeExec) 标记完成, 完成条件勾选
- `openspec/changes/2026-08-10-pdk-safe-exec-tests/` (新建 OpenSpec change artifacts)

## Impact

- **Affected specs**: 新增 `pdk-safe-exec-tests` spec
- **Affected ADRs**: `adr-0021-pdk-design.md` §3.3 SafeExec 章节追加实现细节
- **Affected code**:
  - `include/agenticdsl/pdk/safe_exec.h` (修改, ~95 → ~150 行)
  - `tools/check_doxygen_coverage.sh` (新建)
  - `tests/test_pdk_safe_exec.cpp` (新建)
  - `pdk/README.md` (扩展)
- **Affected tests**: 现有 32/32 test_pdk_macros 测试零修改 (BACKWARD 兼容) + 新增 8 SafeExec tests = 40/40 ctest pass
- **Breaking change**: 无 (SafeExec public API 不变, 内部 std::async → std::jthread)
- **Runtime 影响**: 零 (PDK 静态链接到插件, Runtime 零感知)

## Success Criteria

- [ ] `include/agenticdsl/pdk/safe_exec.h` 实现 jthread + stop_token (内部 std::async 已移除)
- [ ] `tests/test_pdk_safe_exec.cpp` 8 test cases 全部 PASS
- [ ] 8 test cases 中至少 2 个 TDD-red-green: 先写失败 (std::async 行为), 修复后 PASS (jthread 行为)
- [ ] `tools/check_doxygen_coverage.sh` safe_exec.h 退出 0 (覆盖率 ≥ 90%)
- [ ] `pdk/README.md` 含 §SafeExec 实战 + §3 种 Agent Loop 选择指南 + §AgentForge 衔接
- [ ] 32+8 = 40/40 ctest PASS (现有 32 baseline + 8 new SafeExec)
- [ ] ASan preset 40/40 PASS
- [ ] `tools/adr_lint.py` exit 0 (ADR-0021 §3.3 同步)
- [ ] `tools/docs_drift_audit.py` 0 DRIFT (active-status §六 Phase 6a 任务 2 完成)
- [ ] `roadmap.md` Phase 6a 任务 2 完成条件勾选
- [ ] `openspec validate 2026-08-10-pdk-safe-exec-tests --strict` exit 0

## Out of Scope (Non-goals)

- ❌ 不实现完整 SafeExec (fork/cgroups/seccomp, Phase 3 范围)
- ❌ 不修改 DECLARE_TOOL / DEFINE_AGENT 宏 (PDK MVP 范围已 ship)
- ❌ 不修改 Runtime 任何代码 (P3 静态链接不变)
- ❌ 不实现 PluginLifecycle 类 (Phase 3 范围)
- ❌ 不实现 FakeStateStore / StubLLM (Phase 2 测试替身)
- ❌ 不创建 AgentForge 独立项目 (Phase 6b 范围)
- ❌ 不修改现有 test_pdk_macros.cpp (BACKWARD 兼容 5 test cases)
- ❌ 不修改 include/agenticdsl/pdk/pdk.h (统一入口不变)

## Dependencies

- **Block**: Sprint 4 PDK Skeleton (✅ archived 2026-06-19) + Sprint 20 PlanExecute/ForkJoin (✅ archived 2026-08-01)
- **Block by**: Phase 6b (AgentForge MVP 第 1 领域 agent, 待启动)
- **Related**: Phase 6a 整体范围 (PDK 生产化, 见 `roadmap.md` Phase 6a 章节 + `docs/audits/2026-08-09-phase6-reevaluation-post-wave3a.md`)

## Estimated Effort

~1.5 天 (Solo dev):
- T1-T3 写失败测试 + 验证 fail (3h)
- T4 SafeExec jthread 重构 (2h)
- T5-T8 补齐 SafeExec 测试 (1.5h)
- T9 Doxygen 注释 + audit 工具 (1h)
- T10 pdk/README.md 扩展 (1h)
- T11-T12 release gate (ctest + ASan + lint + drift) (1h)
- T13 active-status + roadmap 同步 (0.5h)
- T14 OpenSpec archive (0.5h)
