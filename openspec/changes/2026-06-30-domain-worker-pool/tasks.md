# Tasks: DomainWorkerPool (Sprint 3)

> **变更类型**: 真实实现 (5 sub-tasks, 5 commits per plan §Sprint 3)
> **关联 plan**: `.omo/plans/phase1-execution.md` §Sprint 3
> **关联 ADR**: docs/adr/adr-0020-thread-model-isolation.md (P2)
> **关联 change**: `openspec/changes/2026-06-30-domain-worker-pool/`
> **创建日期**: 2026-06-16 (placeholder) → 2026-06-19 (filled)
> **修订说明**: W1D2.5 启动前置 → Sprint 3 启动时填充, 与 plan §Sprint 3 任务列表对齐

## Sprint 3 子任务 (5 commits)

- [ ] S3.T1 — feat(cognitive): add DomainWorkerPool header
- [ ] S3.T2 — feat(cognitive): implement DomainWorkerPool with std::jthread
- [ ] S3.T3 — test(cognitive): add 7 test cases for DomainWorkerPool
- [ ] S3.T4 — docs(adr+status): sync Sprint 3 ship + ADR-0020 §2.2.1 ✅ Resolved
- [ ] S3.T5 — ci(tsan): add Dockerfile.tsan for ASLR-free TSan validation

## S3.T1: DomainWorkerPool 头文件

- [ ] **S3.T1.1** 新建 `include/agenticdsl/cognitive/domain_worker_pool.h` (~80 行)
  - 含 `DomainTask` struct (domain/tool_name/arguments/output_key)
  - 含 `DomainWorkerPool` 类声明 (state machine, lifecycle, dispatch, handlers)
  - 含 `enum class State { idle, running, stopped }`
  - 头文件前向声明所有外部类型 (PIMPL-lite 模式, 同 CognitiveWorker)
  - 双构造重载: `(num_threads)` 与 `(num_threads, shared_ptr<IInteractionBus>)`
  - 公开方法签名: `start()`, `stop()`, `submit_task(DomainTask)`, `register_domain_handler(...)`, `unregister_domain_handler(...)`
  - 禁止拷贝/移动 (`= delete`)

- [ ] **S3.T1.2** 文件头注释完整
  - 功能描述: DomainWorkerPool — 领域智能体工作线程池 (ADR-0020 §3.2)
  - 设计依据: ADR-0020 §2.2.1 P2 + ADR-0019 IInteractionBus + ADR-0023 P1-P4
  - 作者: AgenticDSL Phase 1 Sprint 3
  - 最后修改日期: 2026-06-19

- [ ] **S3.T1.3** 头文件独立编译验证
  - 不引入 core/engine.h, common/tools/registry.h 等
  - 仅前向声明 nlohmann::json, std::jthread, std::shared_mutex, IInteractionBus
  - Doxygen 注释覆盖公开方法

**T1 验收**:
- [ ] 头文件 `domain_worker_pool.h` 存在
- [ ] 头文件被 cognitive_worker.h 之外的至少一个源文件可独立 include (无 missing include)
- [ ] `openspec validate 2026-06-30-domain-worker-pool` 验证 artifacts 完整 (proposal/specs/design/tasks)

## S3.T2: DomainWorkerPool 实现

- [ ] **S3.T2.1** 新建 `src/modules/cognitive/domain_worker_pool.cpp` (~150 行)
  - 构造: `num_threads_` 校验 (> 0, 否则 `std::invalid_argument`), 初始化 `bus_`
  - `start()`: `compare_exchange_strong(idle → running)`, reserve threads_ 容量, emplace_back jthread
  - `stop()`: `compare_exchange_strong(running → stopped)`, `request_stop()` 所有 jthread, `notify_all`, `join` 所有
  - `submit_task(DomainTask)`: 状态机 assert (running), 加锁入队, `notify_one`
  - `register_domain_handler(domain, handler)`: handler 校验非空, `unique_lock(handlers_mutex_)`, 重复注册抛异常
  - `unregister_domain_handler(domain)`: `unique_lock(handlers_mutex_)`, 未注册抛 `std::out_of_range`
  - `worker_loop(stop_token, worker_id)`: while (!stop_requested) wait_for_task + process_task
  - `process_task(worker_id, task)`:
    - 推 `domain.task.started` 事件
    - `shared_lock(handlers_mutex_)` 查表, 拷贝 handler
    - 释放锁后调用 handler (异常隔离 try-catch + catch(...))
    - 推 `domain.task.completed` (ok) 或 `domain.task.failed` (!ok) 事件
  - 析构函数 out-of-line: state == running 时隐式 stop()

- [ ] **S3.T2.2** 修改 `src/modules/cognitive/CMakeLists.txt`
  - 添加 `domain_worker_pool.cpp` 到 `agenticdsl_modules_cognitive` 库
  - 注释说明: Sprint 3 DomainWorkerPool 实施 (ADR-0020 §3.2)

- [ ] **S3.T2.3** 文件头注释完整
  - 功能描述: DomainWorkerPool 完整实现 — 领域智能体工作线程池
  - 设计依据: ADR-0020 §2.2.1 P2 + ADR-0019 + ADR-0023 + openspec/changes/2026-06-30-domain-worker-pool
  - 作者: AgenticDSL Phase 1 Sprint 3
  - 最后修改日期: 2026-06-19

- [ ] **S3.T2.4** 编译验证
  - ctest 编译通过 (CMake build 不报错)
  - 现有 30/30 baseline 测试零回归
  - lsp_diagnostics 在 domain_worker_pool.h/.cpp 上无 error

**T2 验收**:
- [ ] `domain_worker_pool.cpp` 存在且可独立编译
- [ ] ctest 编译通过, 30 baseline 测试零回归
- [ ] no lsp_diagnostics error

## S3.T3: 多线程集成测试 (7 test cases)

- [ ] **S3.T3.1** 新建 `tests/test_domain_worker_pool.cpp` (~250 行)
  - 测试 1: `DomainWorkerPool default construction` — ctor 创建 4 jthread, state == idle
  - 测试 2: `DomainWorkerPool submit dispatches to worker` — submit_task → worker 处理 → InMemoryBus 收到 domain.task.started/completed
  - 测试 3: `DomainWorkerPool 1000x concurrent submit TSan clean` — 10 thread × 100 task 并发, 4 worker 各自处理 ~250 task
  - 测试 4: `DomainWorkerPool worker exception isolation` — handler 抛 std::exception + 抛 int, worker 继续, 推 domain.task.failed
  - 测试 5: `DomainWorkerPool shutdown waits for in-flight tasks` — submit 1000 task, 立即 stop(), in-flight 完成, 无丢失
  - 测试 6: `DomainWorkerPool graceful vs forced shutdown` — stop() 协作式 vs ~DomainWorkerPool() 隐式 stop, 行为一致
  - 测试 7: `DomainWorkerPool bus integration` — subscribe domain.task.completed 验证事件 payload 字段对齐 ADR-0023

- [ ] **S3.T3.2** 文件头注释完整
  - 功能描述: DomainWorkerPool 单元测试 (Phase 1 Sprint 3)
  - 7 个 TEST_CASE 覆盖: lifecycle / dispatch / concurrency / exception / shutdown / bus
  - 设计依据: openspec/changes/2026-06-30-domain-worker-pool + ADR-0020 §3.2

- [ ] **S3.T3.3** 测试基础设施
  - 使用 InMemoryBus (Sprint 1b 已 ship) 作为测试 bus
  - 简单 echo handler: `[](const DomainTask& t) { return json{{"echo", t.arguments}}; }`
  - 异常 handler: `[](const DomainTask&) { throw std::runtime_error("test"); }`
  - 计数器: `std::atomic<size_t> handler_count` 验证 handler 调用次数

- [ ] **S3.T3.4** 测试编译 + 运行
  - 7/7 test case pass
  - 37/37 ctest pass (30 baseline + 7 new)
  - 零回归
  - TSan 干净 (test 3 1000x 并发, test 5 1000x + shutdown)

**T3 验收**:
- [ ] `test_domain_worker_pool.cpp` 存在
- [ ] 7/7 test case pass
- [ ] 37/37 ctest pass (30 baseline + 7 new)
- [ ] 零回归

## S3.T4: CP.22 协议审查 + 文档同步

- [ ] **S3.T4.1** CP.22 协议审计 (S3.T4 audit report)
  - 新建 `.omo/plans/2026-06-30-cp22-audit.md` (审计报告)
  - 验证锁顺序全局一致: `queue_mutex_` 总是先于 `handlers_mutex_` 获取
  - 验证无递归锁: handlers_ 持锁期间 MUST NOT 调用 handler()
  - 验证异常安全: handler() 异常被 process_task try-catch 捕获, worker continue
  - 验证析构安全: ~DomainWorkerPool() 显式 stop() + join
  - 验证 std::jthread 协作式取消: stop_token 优先于 notify_all

- [ ] **S3.T4.2** ADR-0020 状态更新
  - 修改 `docs/adr/adr-0020-thread-model-isolation.md` §2.2.1 状态: 🟡 Partial → ✅ Resolved
  - 修改 ADR-0020 §3.2: 标记为"已实施" (附 commit hash, Sprint 3 ship 后回填)
  - 头部状态行更新: "🟡 Partial (2026-06-18, Sprint 2 增量 ship)" → "✅ Resolved (2026-06-19, Sprint 3 增量 ship)"

- [ ] **S3.T4.3** Roadmap 同步
  - 修改 `docs/roadmap-status.md` line 44: Sprint 3 状态 (0% → 完成), 进度计数 43 → 50 (+7)
  - 修改 `docs/phase1-roadmap.md` §Sprint 3 详细任务: 标记完成

- [ ] **S3.T4.4** AGENTS.md NOTES 同步
  - 添加 "2026-06-19 (Sprint 3 ship)" 注释
  - 说明: DomainWorkerPool 实施, ADR-0020 §2.2.1 P2 ✅ Resolved, Dockerfile.tsan 验证通过

- [ ] **S3.T4.5** openspec validate
  - `openspec validate 2026-06-30-domain-worker-pool` exit 0
  - `tools/adr_lint.py docs/adr/` exit 0

**T4 验收**:
- [ ] CP.22 审计报告完成 (`.omo/plans/2026-06-30-cp22-audit.md`)
- [ ] ADR-0020 §2.2.1 状态更新: ✅ Resolved
- [ ] roadmap-status.md line 44 进度 50 (从 43)
- [ ] AGENTS.md NOTES 同步
- [ ] `openspec validate` exit 0
- [ ] `tools/adr_lint.py` exit 0

## S3.T5: Dockerfile.tsan (解决 ASLR 已知遗留)

- [ ] **S3.T5.1** 新建 `Dockerfile.tsan` (~30 行)
  - 基础镜像: `ubuntu:22.04`
  - 安装: `build-essential`, `cmake`, `ninja-build`, `git`, `g++-13`
  - 切换默认 gcc/g++ 至 gcc-13
  - 禁用 ASLR: `echo 0 > /proc/sys/kernel/randomize_va_space`
  - 设置 `TSAN_OPTIONS="halt_on_error=1:abort_on_error=1:exitcode=66"`
  - WORKDIR `/hydraforge`, COPY 源码
  - 编译: `cmake -G Ninja -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1" -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread"`
  - CMD: `ctest --output-on-failure`

- [ ] **S3.T5.2** 文件头注释完整
  - 功能描述: TSan 容器化验证 (解决 ASLR 已知遗留, plan §3.3 H1)
  - 设计依据: openspec/changes/2026-06-30-domain-worker-pool + ADR-0020 §2.2.1 P2

- [ ] **S3.T5.3** 构建验证
  - `docker build -f Dockerfile.tsan -t hydraforge-tsan .` 成功
  - 容器大小合理 (< 2GB)
  - 编译时间合理 (< 10 分钟)

- [ ] **S3.T5.4** 运行验证
  - `docker run --rm hydraforge-tsan` 成功
  - 退出码 0
  - ctest 50/50 PASS
  - 1000x 并发用例 (test 3) TSan 干净 (0 data race 报告)
  - 1000x shutdown 用例 (test 5) TSan 干净

**T5 验收**:
- [ ] `Dockerfile.tsan` 存在
- [ ] `docker build` 成功
- [ ] `docker run` 退出码 0, 50/50 ctest pass
- [ ] 1000x 并发 TSan 干净

## 提交策略 (5 commits, per plan §Sprint 3)

```
S3.T1 → feat(cognitive): add DomainWorkerPool header (ADR-0020 P2)
S3.T2 → feat(cognitive): implement DomainWorkerPool with std::jthread
S3.T3 → test(cognitive): add 7 test cases for DomainWorkerPool
S3.T4 → docs(adr+status): sync Sprint 3 ship + ADR-0020 §2.2.1 ✅ Resolved
S3.T5 → ci(tsan): add Dockerfile.tsan for ASLR-free TSan validation
```

## 依赖与阻塞

- **Block by**: Sprint 2 CognitiveWorker (✅ shipped 2026-06-18, uncommitted in working tree)
- **Block**: Sprint 4 PDK 骨架 (2026-07-07 ~ 2026-07-13)
- **Related**:
  - `2026-07-07-pdk-skeleton` — DomainWorkerPool 的 handler 是 PDK 工具的运行时实例
  - `2026-07-14-plugin-loader` — plugin 加载时注册 handler 到 DomainWorkerPool

## Sprint 3 收官验收

- [ ] 50/50 ctest PASS (30 baseline + 9 test_cognitive_worker + 7 test_domain_worker_pool + 4 其他)
  - 注: 实际计数以 commit 时 ctest 输出为准
- [ ] 5 commits (T1 → T2 → T3 → T4 → T5) 已 push
- [ ] `openspec validate 2026-06-30-domain-worker-pool` exit 0
- [ ] `docker build -f Dockerfile.tsan` + `docker run` 通过
- [ ] ADR-0020 §2.2.1 状态: ✅ Resolved
- [ ] CP.22 协议审计报告完成
- [ ] 零回归 (Sprint 1a/1b/P1/CognitiveWorker 全部 30 测试不变)
- [ ] TSan 干净 (Dockerfile.tsan 容器化 1000x 并发验证)
