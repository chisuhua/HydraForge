# Spec: PDK SafeExec Tests + Minimal jthread Fix

> Phase 6a (PDK 生产化) — 阻断 Phase 6 Candidate B 启动条件 #1 的子任务
> 变更来源: `docs/superpowers/plans/2026-07-15-phase6-agentforge-mvp.md` §三 任务 2 + `docs/audits/2026-08-09-phase6-reevaluation-post-wave3a.md` §3.3

## ADDED Requirements

### Requirement: safe-exec-timeout-semantics

`SafeExec` 类 MUST 在超时触发时立即唤醒调用方 (caller-visible 等待 ≤ `with_timeout` + 小常数),**不应**阻塞至 worker 线程自然结束. 超时后 worker 线程 MUST 被请求停止 (cooperative cancellation), 在 `grace_period` (默认 50ms) 内未停止 MUST 标记 detached 而非 join.

#### Scenario: 超时立即抛出 (caller 不等待 fn 完成)

- **GIVEN** `SafeExec().with_timeout(50ms)` 配置
- **AND** lambda 内部 `std::this_thread::sleep_for(2s)` (远超超时)
- **WHEN** 调用 `exec.run(lambda)`
- **THEN** `run()` MUST 在 ≤100ms 内 (50ms timeout + ≤50ms grace) 抛 `std::runtime_error("SafeExec: tool execution timed out after 50ms")`
- **AND** 调用方 wall-clock time MUST < 100ms (基于 `std::chrono::steady_clock` 测量)
- **AND** 调用方 MUST NOT 等待 2s (旧 `std::async` 实现的失败模式)

#### Scenario: 超时后 worker 线程收到 stop_token

- **GIVEN** `SafeExec().with_timeout(50ms)`
- **AND** lambda 接受 `std::stop_token st` 并在 `st.stop_requested()` 时返回 `0`
- **WHEN** `exec.run(lambda)` 超时
- **THEN** SafeExec MUST 调用 `std::stop_source::request_stop()` 通知 worker
- **AND** worker 收到 stop 请求后 MUST 在 ≤`grace_period` 内退出 lambda (否则标记 detached)
- **AND** 调用方 `run()` 抛 `std::runtime_error` (无论 worker 是否在 grace 内退出)

#### Scenario: 旧 std::async 行为不保留 (FAIL 测试)

- **GIVEN** SafeExec 旧实现使用 `std::async(std::launch::async, fn)` + `future.wait_for(timeout)`
- **WHEN** 超时触发后 `run()` 返回
- **THEN** `std::future` 析构调用 `wait()` 会阻塞至 fn 完成 (标准规定, `std::async` launched async future destructor blocks)
- **AND** 旧实现 caller-visible 等待时间 = fn 实际执行时间 (非 timeout), 与本 Requirement 冲突
- **AND** 本测试 MUST 在新实现下 PASS, 在旧实现下 FAIL (证明 std::async 语义不可接受)

### Requirement: safe-exec-no-thread-leak

`SafeExec` MUST NOT 永久泄漏 worker 线程. worker 在超时后 MUST 处于以下两种状态之一: (a) 已自然退出; (b) detached. 测试通过观察 `std::thread::hardware_concurrency()` + 短时阻塞后再次查询活动线程数验证无泄漏.

#### Scenario: 超时 100 次后无线程累积

- **GIVEN** `SafeExec().with_timeout(20ms)`
- **AND** lambda 内部 `for(;;) std::this_thread::sleep_for(1ms);` 无限循环, 仅依赖 stop_token 退出
- **WHEN** 在循环中调用 `exec.run(lambda)` 共 100 次 (每次超时)
- **THEN** 每次 `run()` MUST 在 ≤100ms 内返回 (timeout + grace)
- **AND** 100 次调用完成后, 活动线程数 MUST ≤ 初始基线 + 10 (允许 detached worker 在测试期间运行, 但不应无限增长)

#### Scenario: detached worker 在 grace 后停止

- **GIVEN** `SafeExec().with_timeout(50ms)` + 默认 grace_period = 50ms
- **AND** lambda 忽略 stop_token (不检查) 但循环 5s 后自然退出
- **WHEN** `exec.run(lambda)` 超时
- **THEN** worker 在 grace 50ms 内未停止
- **AND** SafeExec MUST 将 worker `detach()` 而非 `join()` (避免 RAII 析构死锁)
- **AND** worker 在 ~5s 后自然退出 (测试不强制 join, 仅验证不阻塞 caller)

### Requirement: safe-exec-future-result-types

`SafeExec::run()` MUST 支持任意 `std::invoke_result_t<F>` 返回类型, 与原 `std::async` 实现保持类型推导一致.

#### Scenario: 返回 int / string / json / void 各类型

- **WHEN** lambda 返回 `int` / `std::string` / `nlohmann::json` / `void`
- **THEN** `exec.run(lambda)` MUST 返回对应类型 (类型推导与 std::async 兼容)
- **AND** 调用方代码 MUST 无需类型注解 (template auto-deduction)

### Requirement: safe-exec-exception-propagation-unchanged

`SafeExec::run()` MUST 保持原异常传播语义: fn 抛出的异常透传至调用方 (不包装). 修复仅影响超时路径, 不影响异常路径.

#### Scenario: fn 抛 std::runtime_error 透传

- **GIVEN** `SafeExec().with_timeout(1000ms)`
- **AND** lambda 抛 `std::runtime_error("disk full")`
- **WHEN** `exec.run(lambda)`
- **THEN** 调用方 MUST 捕获 `std::runtime_error` 类型
- **AND** 异常消息 MUST 完整为 `"disk full"` (不包装, 不修改)

#### Scenario: fn 抛 std::invalid_argument 透传

- **GIVEN** lambda 抛 `std::invalid_argument("bad input")`
- **WHEN** `exec.run(lambda)` 在 timeout 内返回
- **THEN** 调用方 MUST 捕获 `std::invalid_argument`
- **AND** 异常消息 MUST 为 `"bad input"`

### Requirement: safe-exec-pdk-doxygen-coverage

`include/agenticdsl/pdk/safe_exec.h` MUST 包含完整的 Doxygen 注释 (`@file` / `@brief` / `@tparam` / `@param` / `@return` / `@throws`), 覆盖率 ≥ 90% (基于 `tools/check_doxygen_coverage.sh` 静态扫描).

#### Scenario: safe_exec.h Doxygen 覆盖率审计

- **WHEN** 运行 `tools/check_doxygen_coverage.sh include/agenticdsl/pdk/safe_exec.h`
- **THEN** 输出 MUST 报告 ≥ 90% 覆盖率 (public API 函数 / 类 / 模板参数)
- **AND** 缺失注释 MUST 列出 (失败列表)
- **AND** 退出码 MUST 0 表示通过

### Requirement: pdk-developer-readme

`pdk/README.md` MUST 提供 PDK 开发者指南, 覆盖 SafeExec 实战 + 3 种 Agent Loop 选择指南 + AgentForge 衔接示例. 文档 MUST 包含至少 1 个可编译示例片段 (≤20 行 C++ 代码).

#### Scenario: PDK README 章节完整性

- **WHEN** 检查 `pdk/README.md` 文件
- **THEN** MUST 包含 `## SafeExec 实战` 章节 (覆盖超时 + 异常 + stop_token 协同模式)
- **AND** MUST 包含 `## 3 种 Agent Loop 选择指南` 章节 (React vs PlanExecute vs ForkJoin)
- **AND** MUST 包含 `## AgentForge 衔接` 章节 (PDK 与 AgentForge MVP 集成示例)
- **AND** MUST 包含至少 1 个 ≤ 20 行的可编译 C++ 代码片段 (DECLARE_TOOL + SafeExec 组合示例)

#### Scenario: Doxygen 覆盖率 audit 在 PDK README 存在后通过

- **WHEN** 运行 `tools/check_doxygen_coverage.sh include/agenticdsl/pdk/safe_exec.h pdk/README.md`
- **THEN** 退出码 MUST 0
- **AND** README 章节标题 MUST 出现在 audit 输出
