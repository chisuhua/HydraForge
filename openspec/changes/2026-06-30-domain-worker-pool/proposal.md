# Proposal: DomainWorkerPool (Sprint 3)

> **变更类型**: 真实实现 (新功能 + 并发原语)
> **作者**: Sisyphus (Phase 1 Sprint 3 启动)
> **创建日期**: 2026-06-16 (placeholder) → 2026-06-19 (filled)
> **追溯范围**: `.omo/plans/phase1-execution.md` §Sprint 3
> **关联 ADR**: docs/adr/adr-0020-thread-model-isolation.md (P2) + ADR-0019 IInteractionBus + ADR-0023 ToolResult
> **前置**: Sprint 1a (ToolResult P2-P4, archived 2026-06-16) + Sprint 1b (IInteractionBus, archived 2026-06-17) + P1 解耦 (T1+T2+T3+T4, 2026-06-18) + Sprint 2 (CognitiveWorker, 2026-06-18)
> **amends**:
>   - ADR-0020 §2.2.1 状态: Sprint 3 ship 后从 🟡 Partial → ✅ Resolved
>   - ADR-0020 §3.2 从"目标设计"提升为"实施参考"

## Why

Phase 1 Sprint 2 已 ship `CognitiveWorker` (per-agent DSLEngine 隔离, 单线程任务队列, 9/9 ctest pass),但 ADR-0020 §2.2 P2 的 **领域智能体工作线程池 (DomainWorkerPool)** 仍未实施。当前架构下:

- 领域任务 (tool 调用、文件 IO、网络请求) 与认知任务 (LLM 推理) 共用单线程 `CognitiveWorker`,导致 IO 阻塞时 LLM 推理也被阻塞
- 1000x 并发任务在单 worker 模型下串行执行,无法利用多核
- 工具处理器 (handler) 与 Worker 强耦合,无法在运行时动态注册新领域 (e.g. "code", "browser", "fs")

**TSan ASLR 已知遗留** (plan §3.3 H1) 已识别为 Sprint 3 收官风险:多线程验证在宿主 ASLR 下报假阳性 data race (内存重排),需通过 `Dockerfile.tsan` 容器化 ASLR=0 解决。Sprint 3 必交付 Dockerfile.tsan 验证 1000x 并发无 data race。

**不解决此问题**:
- (a) 跨 worker 任务优先级/路由 (Phase 1 后续)
- (b) 分布式 worker 池 (Phase 2+ 范围)
- (c) 与 CognitiveWorker 任务合并调度 (Phase 1 后续)
- (d) DomainWorkerPool 性能基准 (vs 单线程) — Phase 2 性能验证

## What Changes

### 决策 1: DomainWorkerPool 作为 CognitiveWorker 的薄包装 (与 ADR-0020 §3.2 对齐)

```cpp
// include/agenticdsl/cognitive/domain_worker_pool.h
namespace agenticdsl {

struct DomainTask {
  std::string domain;          // "code", "browser", "fs"
  std::string tool_name;       // "code::edit_file"
  nlohmann::json arguments;
  std::string output_key;      // 结果写入的 key
};

class DomainWorkerPool {
 public:
  explicit DomainWorkerPool(size_t num_threads = 4);

  // 生命周期: ctor 创建 N 个 std::jthread, 全部 joinable
  // start(): 各 worker 启动 wait_for_task 循环
  // stop():  协作式 cancel (std::stop_token) + join 所有 thread
  // 析构:    隐式 stop() (同 CognitiveWorker 模式, 防 std::terminate)
  void start();
  void stop();
  ~DomainWorkerPool();  // out-of-line, 隐式 stop()

  // 提交任务 (线程安全, 非阻塞, 派发到第一个空闲 worker)
  // 派发策略: round-robin (worker_id = (next_id_++) % num_threads_)
  void submit_task(DomainTask task);

  // 注册领域处理器 (线程安全, shared_mutex 保护)
  // 重复注册: 抛 std::invalid_argument
  // handler 在 worker 线程内同步调用, 抛异常时 worker 隔离
  void register_domain_handler(
      const std::string& domain,
      std::function<nlohmann::json(const DomainTask&)> handler);

  // 取消注册 (线程安全, 抛 std::out_of_range 若未注册)
  void unregister_domain_handler(const std::string& domain);

  // 当前状态
  enum class State { idle, running, stopped };
  State state() const;

 private:
  void worker_loop(std::stop_token st, size_t worker_id);

  size_t num_threads_;
  std::vector<std::jthread> threads_;

  // 领域处理器注册表 — shared_mutex 保护
  std::shared_mutex handlers_mutex_;
  std::unordered_map<std::string,
      std::function<nlohmann::json(const DomainTask&)>> handlers_;

  // 任务队列 (FIFO, 跨所有 worker 共享)
  // MVP: std::mutex + std::queue, Phase 2 可升级为 LockFreeQueue
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::queue<DomainTask> task_queue_;

  // 派发计数器 (原子)
  std::atomic<size_t> next_worker_{0};
  std::atomic<State> state_{State::idle};

  // 可选: IInteractionBus 注入 (Worker 完成事件转发)
  std::shared_ptr<IInteractionBus> bus_;
};

} // namespace agenticdsl
```

**关键设计点**:
- **`std::jthread` + `std::stop_token`**: C++20 协作式取消,比 `std::thread`+`std::atomic<bool>` 更安全 (析构自动 request_stop + join)
- **共享任务队列**: 单 FIFO 队列 + 广播 `condition_variable::notify_all`,所有 worker 抢同一个任务 (避免 dispatcher 线程)
- **round-robin 派发**: `next_worker_++ % N` 仅用于 task_id 关联 (实际 worker 仍抢同一队列, 简化 dispatch 逻辑)
- **handler 异常隔离**: worker_loop 内 try-catch 包 handler(), 异常时不杀掉 worker, 记录到 bus (topic: `domain.task.failed`)
- **状态机**: 复用 CognitiveWorker 的 `idle/running/stopped` 三态

### 决策 2: IInteractionBus 集成 (Sprint 1b 契约)

DomainWorkerPool 接受可选 `shared_ptr<IInteractionBus>` (构造时注入, F7 ordering):

```cpp
// 重载构造: 无 bus 版本 (向后兼容 MVP)
explicit DomainWorkerPool(size_t num_threads = 4);

// 重载构造: 带 bus 版本 (Sprint 3 推荐)
DomainWorkerPool(size_t num_threads, std::shared_ptr<IInteractionBus> bus);
```

事件 topic (遵循 `<module>.<verb>` 约定):
- `domain.task.started` (ToolResult payload, `meta["domain"]`, `meta["tool_name"]`, `meta["output_key"]`)
- `domain.task.completed` (ToolResult payload, `result` 字段含 handler 返回的 json)
- `domain.task.failed` (ToolResult payload, `error_code = ErrorCode::Unknown` + `meta["error_message"]`)

### 决策 3: handler 注册语义 (线程安全)

```cpp
void DomainWorkerPool::register_domain_handler(
    const std::string& domain,
    std::function<nlohmann::json(const DomainTask&)> handler) {
  if (!handler) throw std::invalid_argument("handler must be callable");
  std::unique_lock write_lock(handlers_mutex_);
  if (handlers_.count(domain)) {
    throw std::invalid_argument("domain already registered: " + domain);
  }
  handlers_[domain] = std::move(handler);
}
```

**读路径** (worker_loop 内): `std::shared_lock read_lock(handlers_mutex_)` + 查表,handler 持有锁期间不调用 (避免 handler 持锁递归)。

### 决策 4: worker_loop 协作式取消 (CP.22 协议)

```cpp
void DomainWorkerPool::worker_loop(std::stop_token st, size_t worker_id) {
  while (!st.stop_requested()) {
    DomainTask task;
    {
      std::unique_lock lock(queue_mutex_);
      queue_cv_.wait(lock, [&] {
        return !task_queue_.empty() || st.stop_requested();
      });
      if (st.stop_requested()) break;
      task = std::move(task_queue_.front());
      task_queue_.pop();
    }

    // 1) 推送 domain.task.started 事件
    if (bus_) {
      ToolResult started;
      started.ok = true;
      started.meta["domain"] = task.domain;
      started.meta["tool_name"] = task.tool_name;
      started.meta["output_key"] = task.output_key;
      bus_->emit("domain.task.started", started);
    }

    // 2) 查表 + handler 调用 (异常隔离)
    ToolResult result;
    result.meta["domain"] = task.domain;
    result.meta["tool_name"] = task.tool_name;
    result.meta["output_key"] = task.output_key;

    try {
      std::function<nlohmann::json(const DomainTask&)> handler;
      {
        std::shared_lock read_lock(handlers_mutex_);
        auto it = handlers_.find(task.domain);
        if (it == handlers_.end()) {
          throw std::runtime_error("no handler for domain: " + task.domain);
        }
        handler = it->second;  // 拷贝一份, 释放锁后调用
      }
      nlohmann::json output = handler(task);
      result.ok = true;
      result.data[output_key] = std::move(output);  // ADR-0023 P1 data 字段
    } catch (const std::exception& e) {
      result.ok = false;
      result.error_code = ErrorCode::Unknown;
      result.meta["error_message"] = e.what();

      if (bus_) {
        bus_->emit("domain.task.failed", result);
      }
      continue;  // worker 不退出, 继续处理下一个任务
    }

    // 3) 推送 domain.task.completed 事件
    if (bus_) {
      bus_->emit("domain.task.completed", result);
    }
  }
}
```

### 决策 5: Dockerfile.tsan 容器化 (解决 ASLR 已知遗留)

**风险背景** (plan §3.3 H1):
- TSan 在宿主 ASLR 下报假阳性 data race (内存重排)
- CI 矩阵中 TSan 步骤不可靠
- 1000x 并发测试在 TSan 下可能误报

**S3.T5 解决方案**:
- 新建 `Dockerfile.tsan`,基于 `ubuntu:22.04 + gcc-13 + cmake`
- 容器内 ASLR=0 (`echo 0 > /proc/sys/kernel/randomize_va_space`)
- TSAN_OPTIONS: `halt_on_error=1:abort_on_error=1:exitcode=66`
- 验证 1000x 并发测试在容器内 TSan 干净
- CI 工作流新增 step: `docker build -f Dockerfile.tsan . && docker run hydraforge-tsan ctest`

**验收** (Sprint 3 收官):
- [ ] `docker build -f Dockerfile.tsan -t hydraforge-tsan .` 成功
- [ ] `docker run hydraforge-tsan ctest` 退出码 0, 50/50 PASS
- [ ] 1000x 并发用例在容器内 TSan 干净

### 决策 6: 性能契约 (P2.7 acceptance)

**非性能基线**: Sprint 3 不验证 DomainWorkerPool 加速比 (单 worker vs N workers)。Phase 2 范围。

**并发安全基线** (Sprint 3 必交付):
- 1000x 并发 submit_task (10 thread × 100 task): 零 data race (TSan 干净 via Dockerfile.tsan)
- 1000x 并发 register_domain_handler + submit_task: 零 deadlock (锁顺序: queue_mutex_ → handlers_mutex_)
- 1000x 并发异常 handler: worker 不死, 继续处理下一个任务

### 决策 7: CP.22 协议合规 (S3.T4 审计清单)

**CP.22 协议** (plan §4.2):
- ✅ 锁顺序全局一致: `queue_mutex_` 总是先于 `handlers_mutex_` 获取
- ✅ 无优先级反转: 所有 worker 同优先级, condition_variable 公平唤醒 (FIFO 顺序非保证, 但所有 worker 同优先级)
- ✅ 无递归锁: handlers_ 持锁时不调用 handler()
- ✅ 异常安全: handler() 异常被捕获, worker 继续
- ✅ 析构安全: ~DomainWorkerPool() 显式 stop() + join (同 CognitiveWorker)
- ✅ std::jthread 协作式取消: stop_token 优先于 notify

### 决策 8: 与 CognitiveWorker 协同 (未来扩展)

DomainWorkerPool 与 CognitiveWorker 是**正交**组件:
- CognitiveWorker 处理**认知任务** (LLM 推理),per-agent 隔离
- DomainWorkerPool 处理**领域任务** (tool 调用),per-thread 池

Phase 1 Sprint 3 MVP 不强制 CognitiveWorker 调用 DomainWorkerPool (e.g. LLM ReAct 工具调用仍走 SimpleCognitiveOrchestrator 内部 ToolRegistry)。Phase 2 规划: CognitiveWorker 在 SimpleCognitiveOrchestrator 内部将 tool.call 转发到 DomainWorkerPool,实现"LLM 推理与工具执行真正并行"。

### 代码侧 (新代码)

- `include/agenticdsl/cognitive/domain_worker_pool.h` (新建, ~80 行)
  - `DomainTask` + `DomainWorkerPool` 类声明
  - 状态机 + 异常安全契约注释
- `src/modules/cognitive/domain_worker_pool.cpp` (新建, ~150 行)
  - 构造 + start/stop + submit_task + register/unregister + worker_loop
  - 析构函数 out-of-line (同 CognitiveWorker 模式)
- `src/modules/cognitive/CMakeLists.txt` (修改: 添加 domain_worker_pool.cpp)
- `tests/test_domain_worker_pool.cpp` (新建, 7 case)
  1. Pool 默认构造 (4 threads, state == idle)
  2. submit_task 派发到 worker (InMemoryBus 验证 domain.task.started/completed)
  3. 1000x 并发 submit_task 零 data race (10 thread × 100 task, TSan 干净 via Dockerfile.tsan)
  4. worker 异常隔离 (handler 抛异常, worker 继续)
  5. shutdown() 等待所有任务完成 (in-flight task 完成后再 join)
  6. graceful_shutdown vs forced_shutdown (stop() 协作式 + ~DomainWorkerPool() 隐式 stop)
  7. 与 IInteractionBus 集成 (subscribe domain.task.completed 验证事件 payload)
- `Dockerfile.tsan` (新建, ~30 行)
  - 基于 ubuntu:22.04 + gcc-13 + cmake
  - ASLR=0 + TSAN_OPTIONS
  - 验证命令: `ctest --output-on-failure`

### 文档侧

- 更新 `docs/adr/adr-0020-thread-model-isolation.md` §2.2.1 状态: Sprint 3 ship → ✅ Resolved
- 更新 `docs/adr/adr-0020-thread-model-isolation.md` §3.2: 标记为"已实施" (附 commit hash)
- 更新 `docs/roadmap-status.md` line 44: Sprint 3 状态 (0% → 进行中 → 完成, 43 → 50, +7)
- 更新 `docs/phase1-roadmap.md` §Sprint 3 详细任务
- 更新 `AGENTS.md` NOTES: Sprint 3 ship 注释
- 不更新 `openspec/specs/tech-debt-cleanup/spec.md` (本 change 不修改 §1.4 退出标准)

## Impact

- **Affected specs**: 无 (DomainWorkerPool 是新功能, 不修改既有 spec)
- **Affected ADRs**:
  - `adr-0020-thread-model-isolation.md` §2.2.1 状态 + §3.2 实施参考
- **Affected code**:
  - `include/agenticdsl/cognitive/domain_worker_pool.h` (新建)
  - `src/modules/cognitive/domain_worker_pool.cpp` (新建)
  - `src/modules/cognitive/CMakeLists.txt` (添加 domain_worker_pool.cpp)
  - `tests/CMakeLists.txt` (无需改, GLOB 自动注册)
  - `Dockerfile.tsan` (新建)
- **Affected tests**: 现有 30 测试零回归 (含 Sprint 2 test_cognitive_worker 9 case) + 新增 7 测试 = 37/37 ctest pass
- **Breaking change**: 无 (纯新增类, 不改任何已有 API)
- **依赖变化**: CognitiveWorker 与 DomainWorkerPool 正交, 无运行时依赖

## Success Criteria

- [ ] `DomainWorkerPool` 类编译通过 (C++20 std::jthread + std::stop_token, 无 include 警告)
- [ ] 头文件前向声明所有外部类型 (避免引入 core/engine.h)
- [ ] start/stop/析构 生命周期正确 (std::jthread 协作式 join 无 hang, 析构不调 std::terminate)
- [ ] 7 test_domain_worker_pool 测试通过 (含 1000x 并发, 含 worker 异常隔离)
- [ ] 现有 30/30 ctest 零回归 + 新增 7 测试 = 37/37 ctest pass
- [ ] Dockerfile.tsan 容器化验证 1000x 并发 TSan 干净
- [ ] `tools/adr_lint.py docs/adr/` exit 0 (ADR-0020 §2.2.1 状态更新)
- [ ] `openspec validate 2026-06-30-domain-worker-pool` exit 0
- [ ] ADR-0020 §2.2.1 状态: 🟡 Partial → ✅ Resolved
- [ ] 5 commits (T1 → T2 → T3 → T4 → T5) per plan §Sprint 3

## Out of Scope (Non-goals)

- ❌ 不实现 CognitiveWorker 调用 DomainWorkerPool (Phase 2 范围)
- ❌ 不实现跨 DomainWorkerPool 实例的任务迁移 (Phase 2 范围)
- ❌ 不实现 DomainWorkerPool 性能基准 (单 worker vs N worker 加速比, Phase 2 范围)
- ❌ 不实现动态 thread 数量调整 (Phase 2 范围, 当前 num_threads_ 构造时固定)
- ❌ 不实现分布式 worker 池 (Phase 2+ 范围)
- ❌ 不修改 CognitiveWorker (Sprint 2 已 ship, 不再改)
- ❌ 不引入 LockFreeQueue (Phase 2 范围, MVP 用 std::mutex + std::queue)

## Dependencies

- **Block**: Sprint 1a (✅ shipped 2026-06-16) + Sprint 1b (✅ shipped 2026-06-17) + P1 解耦 (✅ shipped 2026-06-18) + Sprint 2 CognitiveWorker (✅ shipped 2026-06-18, uncommitted in working tree)
- **Block by**: Sprint 4 (PDK 骨架, 2026-07-07 ~ 2026-07-13)
- **Related**:
  - `2026-06-30-domain-worker-pool` (本 change)
  - `2026-07-07-pdk-skeleton` (Sprint 4, DomainWorkerPool 的 handler 是 PDK 工具的运行时实例)
  - `2026-07-14-plugin-loader` (Sprint 5, plugin 加载时注册 handler 到 DomainWorkerPool)

## Estimated Effort

~3.3 天 (单人, 反映 7 测试 + Dockerfile.tsan + CP.22 审计成本):
- T1 DomainWorkerPool 头 + 实现 (含 std::jthread + 协作式取消 + 异常隔离 + bus 集成): 1.5 天
- T2 test_domain_worker_pool (7 case — 含 1000x 并发 + 异常隔离 + bus 集成): 1 天
- T3 Dockerfile.tsan 容器化 + 1000x 并发 TSan 验证: 0.5 天
- T4 CP.22 协议审计 + 文档同步 + ADR-0020 §2.2.1 状态更新 + openspec validate: 0.3 天
- T5 提交策略 5 commits (T1 → T2 → T3 → T4 → T5): 0 天 (集成在上述步骤)
