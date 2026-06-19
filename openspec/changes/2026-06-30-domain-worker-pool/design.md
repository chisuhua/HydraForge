# Design: DomainWorkerPool (Sprint 3)

> **变更类型**: 真实实现 — 本 design 描述新代码架构 (DomainWorkerPool + Dockerfile.tsan)
> **关联 proposal**: `openspec/changes/2026-06-30-domain-worker-pool/proposal.md`
> **关联 spec**: `openspec/changes/2026-06-30-domain-worker-pool/specs/domain-worker-pool/spec.md`
> **关联 ADR**: docs/adr/adr-0020-thread-model-isolation.md §3.2 (实施参考) + §2.2.1 (P2 状态变更) + ADR-0019 IInteractionBus + ADR-0023 ToolResult

## 架构合规性检查

| 约束 | 状态 | 备注 |
|------|------|------|
| 2 空格缩进 | ✅ | 沿用现有 |
| 中文注释优先 | ✅ | 全部新增注释中文 |
| C++20 + CMake 3.20+ | ✅ | std::jthread + std::stop_token (C++20) |
| nlohmann_json | ✅ | DomainTask.arguments + handler 返回值 |
| std::shared_mutex | ✅ | C++17 引入, 读写锁 |
| std::atomic<EnumClass> | ✅ | C++20 完整支持 (CognitiveWorker 已用) |
| Anti-pattern 避免 | ✅ | 不删失败测试,提交前 ctest |
| 头文件前向声明 | ✅ | DomainWorkerPool.h 仅前向声明 (PIMPL-lite 模式) |
| 析构外置到 .cpp | ✅ | PIMPL 必须 (同 CognitiveWorker 模式) |
| Dockerfile.tsan | ✅ | 容器化 ASLR=0 解决已知遗留 |

## 关键设计决策

### 决策 1: DomainWorkerPool 作为 N CognitiveWorker 抽象层

**问题**: ADR-0020 §3.2 设计 DomainWorkerPool 时引用了 CognitiveWorker 的 worker_thread_ 模式,但 CognitiveWorker (Sprint 2) 是 per-agent 隔离的单线程 worker,语义上**不直接**与 DomainWorkerPool 的"线程池"对应。

**方案**: DomainWorkerPool **不**包装 N 个 CognitiveWorker (避免重复持有 DSLEngine 实例),而是直接管理 N 个 std::jthread + 共享任务队列 + handlers_ 表,所有 worker 共享 pool 级别的状态。

```cpp
// include/agenticdsl/cognitive/domain_worker_pool.h
class DomainWorkerPool {
 public:
  explicit DomainWorkerPool(size_t num_threads = 4);
  DomainWorkerPool(size_t num_threads, std::shared_ptr<IInteractionBus> bus);
  ~DomainWorkerPool();

  // 禁止拷贝/移动 (jthread + mutex 不可移动)
  DomainWorkerPool(const DomainWorkerPool&) = delete;
  DomainWorkerPool& operator=(const DomainWorkerPool&) = delete;
  DomainWorkerPool(DomainWorkerPool&&) = delete;
  DomainWorkerPool& operator=(DomainWorkerPool&&) = delete;

  void start();
  void stop();

  void submit_task(DomainTask task);
  void register_domain_handler(
      const std::string& domain,
      std::function<nlohmann::json(const DomainTask&)> handler);
  void unregister_domain_handler(const std::string& domain);

  enum class State { idle, running, stopped };
  State state() const { return state_.load(std::memory_order_acquire); }

 private:
  void worker_loop(std::stop_token st, size_t worker_id);

  // === 不可变 (构造时固定) ===
  size_t num_threads_;
  std::shared_ptr<IInteractionBus> bus_;

  // === 生命周期 (start/stop) ===
  std::vector<std::jthread> threads_;
  std::atomic<State> state_{State::idle};

  // === 共享任务队列 (FIFO, 多消费者) ===
  std::mutex queue_mutex_;
  std::condition_variable queue_cv_;
  std::queue<DomainTask> task_queue_;

  // === 领域处理器注册表 (shared_mutex 保护) ===
  std::shared_mutex handlers_mutex_;
  std::unordered_map<std::string,
      std::function<nlohmann::json(const DomainTask&)>> handlers_;

  // === 派发计数器 (调试用) ===
  std::atomic<size_t> next_worker_{0};
};
```

**关键设计点**:
- **不持有 DSLEngine**: DomainWorkerPool 只负责"领域任务"派发,与认知层 (CognitiveWorker + DSLEngine) 正交
- **std::jthread 协作式取消**: C++20 引入,析构自动 request_stop + join,比 std::thread 更安全
- **shared_mutex**: 多读少写的 handlers_ 表用读写锁,submit_task 路径不阻塞
- **PIMPL-lite**: 头文件只声明, .cpp 包含完整类型定义 (nlohmann::json, jthread, shared_mutex 等)

### 决策 2: 共享任务队列 + 多消费者模式 (vs 派发器线程)

**问题**: 是否需要单独的 dispatcher 线程 (主线程 submit → 派发器 → 选 worker)?

**方案 (拒绝 dispatcher 线程)**:
- 单 FIFO 队列 + 条件变量 + N 个 worker 抢同一队列
- 简单 (N+1 个 std::thread,无 dispatcher), N=4 时调度开销可忽略
- Phase 2 优化: per-worker 队列 (work-stealing) — 不在 Sprint 3 范围

**理由**:
- submit_task 频率: Phase 1 预期 < 100 task/s, 锁竞争非瓶颈
- 派发器线程增加复杂度, 且 dispatcher 是单点瓶颈
- worker 抢队列 = 内置负载均衡 (无需 round-robin 调度逻辑)

**实现**:
```cpp
void DomainWorkerPool::worker_loop(std::stop_token st, size_t worker_id) {
  while (!st.stop_requested()) {
    DomainTask task;
    {
      std::unique_lock<std::mutex> lock(queue_mutex_);
      queue_cv_.wait(lock, [&] {
        return !task_queue_.empty() || st.stop_requested();
      });
      if (st.stop_requested()) break;  // 协作式退出
      task = std::move(task_queue_.front());
      task_queue_.pop();
    }
    // 锁外处理 task (允许其他 worker 抢下一个)
    process_task(worker_id, std::move(task));
  }
}
```

### 决策 3: handler 调用异常隔离 (try-catch per task)

**问题**: 用户提供的 handler 可能抛异常 (e.g. 文件 IO 失败、API 调用超时), 若不在 worker_loop 内 try-catch,异常会沿 worker_loop 栈展开 → std::thread 析构 → std::terminate 进程崩溃。

**方案**: worker_loop 内的 process_task 函数包 try-catch:
```cpp
void DomainWorkerPool::process_task(size_t worker_id, DomainTask task) {
  // 1) 推送 started 事件
  if (bus_) {
    ToolResult started;
    started.ok = true;
    started.meta["domain"] = task.domain;
    started.meta["tool_name"] = task.tool_name;
    started.meta["output_key"] = task.output_key;
    started.meta["worker_id"] = worker_id;
    bus_->emit("domain.task.started", started);
  }

  // 2) 查表 (read_lock) + 拷贝 handler (释放锁后调用)
  std::function<nlohmann::json(const DomainTask&)> handler;
  {
    std::shared_lock<std::shared_mutex> read_lock(handlers_mutex_);
    auto it = handlers_.find(task.domain);
    if (it == handlers_.end()) {
      // 域未注册: 推 failed 事件
      ToolResult failed;
      failed.ok = false;
      failed.error_code = ErrorCode::Unknown;
      failed.meta["error_message"] = "no handler for domain: " + task.domain;
      failed.meta["domain"] = task.domain;
      failed.meta["tool_name"] = task.tool_name;
      failed.meta["output_key"] = task.output_key;
      if (bus_) bus_->emit("domain.task.failed", failed);
      return;
    }
    handler = it->second;  // 拷贝 (std::function 是值类型)
  }

  // 3) 调用 handler (异常隔离)
  ToolResult result;
  result.meta["domain"] = task.domain;
  result.meta["tool_name"] = task.tool_name;
  result.meta["output_key"] = task.output_key;
  result.meta["worker_id"] = worker_id;

  try {
    nlohmann::json output = handler(task);
    result.ok = true;
    result.data[task.output_key] = std::move(output);
  } catch (const std::exception& e) {
    result.ok = false;
    result.error_code = ErrorCode::Unknown;
    result.meta["error_message"] = e.what();
  } catch (...) {
    result.ok = false;
    result.error_code = ErrorCode::Unknown;
    result.meta["error_message"] = "unknown exception (non-std)";
  }

  // 4) 推 completed/failed 事件
  if (bus_) {
    if (result.ok) {
      bus_->emit("domain.task.completed", result);
    } else {
      bus_->emit("domain.task.failed", result);
    }
  }
}
```

**关键不变量**:
- handler 调用 MUST 在 handlers_mutex_ **释放后**执行 (避免持锁递归 + handler 死锁)
- handler 异常 MUST 不传播至 worker_loop (try-catch 在 process_task 内)
- worker MUST 继续处理下一个 task (catch 后 return, 不 rethrow)

### 决策 4: 状态机 + 析构安全 (PIMPL-lite 模式)

**复用 CognitiveWorker 模式** (Sprint 2 已 ship,验证 OK):
- 显式 `enum class State { idle, running, stopped }`
- `std::atomic<State>` (C++20 完整支持)
- 公开方法在 entry 处 assert 状态机前置条件
- 析构函数 out-of-line 定义 (PIMPL-lite)

```cpp
// src/modules/cognitive/domain_worker_pool.cpp
DomainWorkerPool::~DomainWorkerPool() {
  if (state_.load(std::memory_order_acquire) == State::running) {
    stop();  // 协作式 + join 所有 jthread
  }
  // 隐式析构成员按声明逆序: handlers_ / queue_ / queue_cv_ / queue_mutex_ / threads_ / bus_
}

void DomainWorkerPool::start() {
  State expected = State::idle;
  if (!state_.compare_exchange_strong(expected, State::running)) {
    throw std::logic_error(
        "DomainWorkerPool::start: invalid state (expected idle)");
  }
  threads_.reserve(num_threads_);
  for (size_t i = 0; i < num_threads_; ++i) {
    threads_.emplace_back([this, i](std::stop_token st) {
      worker_loop(st, i);
    });
  }
}

void DomainWorkerPool::stop() {
  State expected = State::running;
  if (!state_.compare_exchange_strong(expected, State::stopped)) {
    return;  // idle / stopped: 幂等
  }
  // request_stop 唤醒所有 worker (协作式取消)
  for (auto& t : threads_) {
    if (t.joinable()) t.request_stop();
  }
  queue_cv_.notify_all();
  // join 所有 worker
  for (auto& t : threads_) {
    if (t.joinable()) t.join();
  }
}
```

**关键设计**:
- `stop()` 先 `request_stop()` 再 `notify_all()` — 即使 worker 不在 wait 状态 (正在处理 task), 退出 process_task 后也会看到 stop_token 取消
- jthread 析构时自动 request_stop + join (双重保险)
- `num_threads_` 是 const 成员, start() 一次性 reserve, stop() 一次性 join

### 决策 5: IInteractionBus 集成 (Sprint 1b 契约对齐)

**对齐 ADR-0019 IInteractionBus** (Sprint 1b 已 ship) + CognitiveWorker F7 契约 (构造时强制注入):

```cpp
// 双构造重载
DomainWorkerPool::DomainWorkerPool(size_t num_threads)
    : num_threads_(num_threads), bus_(nullptr) {
  if (num_threads_ == 0) {
    throw std::invalid_argument("DomainWorkerPool: num_threads must be > 0");
  }
}

DomainWorkerPool::DomainWorkerPool(size_t num_threads,
                                   std::shared_ptr<IInteractionBus> bus)
    : num_threads_(num_threads), bus_(std::move(bus)) {
  if (num_threads_ == 0) {
    throw std::invalid_argument("DomainWorkerPool: num_threads must be > 0");
  }
}
```

**事件 topic 约定** (遵循 `<module>.<verb>` 模式):
- `domain.task.started` — worker 开始处理 task (含 domain/tool_name/output_key/worker_id)
- `domain.task.completed` — handler 成功返回 (含 data[output_key] + meta)
- `domain.task.failed` — handler 抛异常或 domain 未注册 (含 error_code + error_message)

**事件 payload 字段对齐 ADR-0023 P1-P4**:
- `payload.ok` — bool
- `payload.data[output_key]` — nlohmann::json (P1 字段)
- `payload.meta.*` — std::string (调试/关联键)
- `payload.error_code` — std::optional<ErrorCode> (P2 字段, 失败时填充)
- `payload.meta["error_message"]` — std::string (失败描述)

### 决策 6: CP.22 协议合规 (S3.T4 审计清单)

**CP.22 协议** (plan §4.2):

| 项 | 状态 | 验证方法 |
|---|------|---------|
| 锁顺序全局一致 | ✅ | `queue_mutex_` 总是先于 `handlers_mutex_` 获取 (worker_loop 内 lock queue, 出队后释放 queue, 再 acquire handlers_) |
| 无递归锁 | ✅ | handlers_ 持锁期间 MUST NOT 调用 handler() (process_task 先查表+拷贝, 释放锁, 再调用) |
| 无优先级反转 | ✅ | 所有 worker 同优先级, condition_variable 公平唤醒 (FIFO 顺序非保证, 但无高/低优先级) |
| 异常安全 | ✅ | handler() 异常被 process_task try-catch 捕获, worker continue |
| 析构安全 | ✅ | ~DomainWorkerPool() 显式 stop() + join (同 CognitiveWorker 模式) |
| std::jthread 协作式取消 | ✅ | stop_token 优先于 notify (request_stop 先于 notify_all) |

**S3.T4 审计方法**:
- 静态分析: 阅读 domain_worker_pool.cpp, 验证锁顺序与 try-catch 覆盖
- 动态验证: TSan + 1000x 并发测试 (test case 3 + 5) 检测 data race
- 形式化: 锁顺序矩阵 + 异常路径分析 (写入 S3.T4 audit 报告)

### 决策 7: Dockerfile.tsan 容器化 (S3.T5, 解决 ASLR 已知遗留)

**问题背景** (plan §3.3 H1):
- TSan 在宿主 ASLR 下报假阳性 data race (内存重排误报)
- CI 矩阵中 TSan 步骤不可靠
- 1000x 并发测试在 TSan 下可能误报

**方案**: 容器化 TSan 验证 (ASLR=0 + 固定 TSAN_OPTIONS)

```dockerfile
# Dockerfile.tsan
# 功能描述：TSan 容器化验证 (解决 ASLR 已知遗留, plan §3.3 H1)
#          1000x 并发测试在容器内 TSan 干净, 排除宿主 ASLR 假阳性
# 设计依据：openspec/changes/2026-06-30-domain-worker-pool + ADR-0020 §2.2.1 P2
# 作者：AgenticDSL Phase 1 Sprint 3
# 最后修改日期：2026-06-19

FROM ubuntu:22.04

ENV DEBIAN_FRONTEND=noninteractive
ENV TSAN_OPTIONS="halt_on_error=1:abort_on_error=1:exitcode=66:second_deadlock_stack=1"

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    ca-certificates \
    g++-13 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-13 100 \
    && update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-13 100 \
    && rm -rf /var/lib/apt/lists/*

# 禁用 ASLR (容器内)
RUN echo 0 > /proc/sys/kernel/randomize_va_space || true

WORKDIR /hydraforge

# 复制源码
COPY . /hydraforge

# 编译 (TSan 标志)
RUN mkdir -p build-tsan && cd build-tsan && \
    cmake .. -G Ninja \
        -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_COMPILER=g++-13 \
        -DCMAKE_CXX_FLAGS="-fsanitize=thread -g -O1" \
        -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=thread" \
        -DAGENTICDSL_BUILD_TESTS=ON && \
    ninja -j$(nproc)

# 默认运行 ctest
CMD ["sh", "-c", "cd build-tsan && ctest --output-on-failure"]
```

**验证命令**:
```bash
docker build -f Dockerfile.tsan -t hydraforge-tsan .
docker run --rm hydraforge-tsan
# 预期: 50/50 ctest PASS, 1000x 并发用例 TSan 干净
```

**CI 集成 (后续)**: `.github/workflows/ci.yml` 新增 step:
```yaml
- name: TSan validation (Dockerfile.tsan)
  run: |
    docker build -f Dockerfile.tsan -t hydraforge-tsan .
    docker run --rm hydraforge-tsan
```

### 决策 8: 测试设计 (7 test case, S3.T3)

**Test case 列表** (对齐 plan §Sprint 3 T3):

| # | 测试名 | 验证目标 | 并发级别 |
|---|--------|---------|---------|
| 1 | `DomainWorkerPool default construction` | ctor 创建 4 jthread (未启动), state == idle | 单线程 |
| 2 | `DomainWorkerPool submit dispatches to worker` | submit_task → worker 处理 → InMemoryBus 收到 domain.task.started/completed | 单线程 |
| 3 | `DomainWorkerPool 1000x concurrent submit TSan clean` | 10 thread × 100 task 并发, TSan 0 data race | 1000x 并发 |
| 4 | `DomainWorkerPool worker exception isolation` | handler 抛异常, worker 不死, 推 domain.task.failed | 单线程 |
| 5 | `DomainWorkerPool shutdown waits for in-flight tasks` | submit 1000 task, 立即 stop(), in-flight task 完成, 无丢失 | 1000x 并发 |
| 6 | `DomainWorkerPool graceful vs forced shutdown` | stop() 协作式 vs ~DomainWorkerPool() 隐式 stop, 行为一致 | 单线程 |
| 7 | `DomainWorkerPool bus integration` | subscribe domain.task.completed 验证事件 payload 字段对齐 ADR-0023 | 单线程 |

**Test 基础设施**:
- 使用 InMemoryBus (Sprint 1b 已 ship) 作为测试 bus
- 简单 handler: `[](const DomainTask& t) { return json{{"echo", t.arguments}}; }`
- 异常 handler: `[](const DomainTask&) { throw std::runtime_error("test"); }`

**TSan 验证**: 单独 ctest preset `cmake --preset tsan` (启用 -fsanitize=thread) — Dockerfile.tsan 容器化运行

### 决策 9: 性能契约 (Sprint 3 范围)

**Sprint 3 必交付** (concurrent safety baseline):
- ✅ 1000x 并发 submit_task: 零 data race (TSan 干净 via Dockerfile.tsan)
- ✅ 1000x 并发 register + submit 混合: 零 deadlock (锁顺序一致)
- ✅ 1000x 异常 handler 压力: worker 不死, 100% task 处理

**Sprint 3 不交付** (性能基线, Phase 2 范围):
- ❌ N workers vs 1 worker 加速比 benchmark
- ❌ 吞吐量 (task/s) 性能基准
- ❌ handler 调用延迟 P50/P99
- ❌ 内存占用基线 (handler registry + queue footprint)

## 实施路径 (S3.T1 → T5)

### T1: domain_worker_pool.h (新建, ~80 行)
- 文件: `include/agenticdsl/cognitive/domain_worker_pool.h`
- 内容: DomainTask + DomainWorkerPool 类声明
- 验收: 头文件独立编译 (无 .cpp), 前向声明所有外部类型

### T2: domain_worker_pool.cpp (新建, ~150 行)
- 文件: `src/modules/cognitive/domain_worker_pool.cpp`
- 内容: 构造 + start/stop + submit_task + register/unregister + worker_loop + process_task
- 验收: ctest 编译通过, 30 baseline 测试零回归

### T3: test_domain_worker_pool.cpp (新建, 7 case, ~250 行)
- 文件: `tests/test_domain_worker_pool.cpp`
- 内容: 7 TEST_CASE (决策 8 列表)
- 验收: 7/7 test case pass, 37/37 ctest pass (30 baseline + 7 new)

### T4: CP.22 协议审计 + 文档同步
- 文件: `.omo/plans/2026-06-30-cp22-audit.md` (新建) + ADR-0020 §2.2.1 状态更新
- 内容: 锁顺序 + 异常安全 + 析构安全 + 协作式取消 审计
- 验收: 审计报告完成 + ADR-0020 状态: 🟡 Partial → ✅ Resolved

### T5: Dockerfile.tsan (新建, ~30 行)
- 文件: `Dockerfile.tsan`
- 内容: ubuntu:22.04 + gcc-13 + TSan 编译 + ctest
- 验收: docker build + docker run 成功, 50/50 ctest pass, 1000x 并发 TSan 干净

## 提交策略 (5 commits, per plan §Sprint 3)

```
S3.T1 → feat(cognitive): add DomainWorkerPool header (ADR-0020 P2)
S3.T2 → feat(cognitive): implement DomainWorkerPool with std::jthread
S3.T3 → test(cognitive): add 7 test cases for DomainWorkerPool
S3.T4 → docs(adr+status): sync Sprint 3 ship + ADR-0020 §2.2.1 ✅ Resolved
S3.T5 → ci(tsan): add Dockerfile.tsan for ASLR-free TSan validation
```

## 风险与缓解

| 风险 | 严重度 | 缓解措施 |
|------|-------|---------|
| std::jthread 协作式取消未生效 | 中 | S3.T4 审计: 验证 request_stop() 在 notify_all 之前调用 |
| handlers_ 持锁递归死锁 | 中 | S3.T4 审计: 验证 process_task 释放锁后才调用 handler() |
| 1000x 并发 data race | 高 | S3.T3 压力测试 + S3.T5 Dockerfile.tsan 容器化验证 |
| TSan 假阳性 (ASLR 已知遗留) | 中 | S3.T5 强制容器化, CI 不在宿主直接跑 TSan |
| worker 异常导致进程崩溃 | 高 | process_task try-catch + catch(...) 兜底 |
| 析构函数 std::terminate 风险 | 中 | 复用 CognitiveWorker PIMPL-lite 模式 (Sprint 2 已验证) |
| handler 阻塞 worker (无 timeout) | 低 | Phase 2 加 timeout (Sprint 3 MVP 不实现) |

## 相关 ADR / 文档

- **ADR-0020 §3.2** 实施参考 (本 change 落地)
- **ADR-0020 §2.2.1** 状态变更: 🟡 Partial → ✅ Resolved (S3.T4 同步)
- **ADR-0019** IInteractionBus (Sprint 1b 已 ship, 复用契约)
- **ADR-0023** ToolResult P1-P4 (事件 payload 字段对齐)
- **Sprint 2 CognitiveWorker** (per-agent 隔离, 同步模式参考)
- **Plan §Sprint 3** (本 change 范围与粒度对齐)
