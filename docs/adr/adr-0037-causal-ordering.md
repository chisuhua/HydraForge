# ADR-0037: 跨 Worker 事件因果序与逻辑时间戳

## 状态

**🔍 Proposed (2026-06-26)** — 新 ADR,解决跨 CognitiveWorker 和 DomainWorkerPool 的事件顺序问题。

## 背景

### 现有架构

HydraForge 当前实现了多智能体异步通信架构:

| 组件 | 通信机制 | 顺序保证 |
|------|---------|---------|
| **IInteractionBus** | 异步事件总线 (InMemoryBus) | ❌ 无全局顺序 |
| **CognitiveWorker** | 单线程任务队列 + `task_id` 关联 | ✅ 同 Worker 内 FIFO |
| **DomainWorkerPool** | N 个 jthread 共享 FIFO 队列 | ✅ 同 Worker 内 FIFO |
| **跨 Worker** | 通过 `trace_id` / `parent_trace` 关联 | ❌ 无因果序保证 |

### 问题

1. **乱序事件**: 异步事件总线不保证事件按发射顺序到达消费者
2. **因果链断裂**: 当 Worker-A 的结果触发 Worker-B 的任务时,无法通过时间戳判断 `happens-before` 关系
3. **调试困难**: 日志中跨 Worker 的事件交织,难以重建执行序列
4. **并发冲突检测缺失**: 无法检测两个任务是否真正并发执行 (无因果关系)

### 场景示例

```
CognitiveWorker-A 完成任务 ──→ 触发 CognitiveWorker-B 新任务
     (task_id: A-123)              (task_id: B-456, parent: A-123)
     emit("completed")             emit("started")
     
问题: 消费者可能先收到 B.started,后收到 A.completed (乱序)
当前: 只能通过 parent_trace 手动重组
缺失: 自动的 happens-before 判定机制
```

### 相关文档

| ADR | 内容 | 与本 ADR 关系 |
|-----|------|--------------|
| ADR-0019 | IInteractionBus 接口 | 事件总线基础设施 |
| ADR-0020 | 多智能体线程模型 | Worker 隔离模型 |
| ADR-0023 | ToolResult 标准化 | 事件载荷结构 |
| [architecture.md](../specs/architecture.md#L945) | 向量时钟 (未来) | 分布式同步 (Phase 3+) |

---

## 决策

### 1. 为什么不选 Lamport 时间戳？

#### Lamport 的局限性 (单机多线程场景)

| 维度 | Lamport 设计目标 | 我们的场景 | 匹配度 |
|------|----------------|-----------|--------|
| **系统边界** | 跨节点/跨进程分布式 | 单进程多线程 | ❌ 不匹配 |
| **时钟同步** | 物理时钟不可靠 (网络延迟) | 共享内存,可用 `steady_clock` | ❌ 不需要 |
| **消息传递** | 不可靠网络,可能丢失/乱序 | 内存 queue,可靠传递 | ❌ 过度设计 |
| **复杂度** | O(1) 标量,但需严格协议 | 额外维护成本 | ⚠️ 收益低 |
| **可调试性** | 时间戳不直观 (逻辑值) | 开发者需要可读时间 | ❌ 不友好 |

**结论**: Lamport 解决的是**分布式系统物理时钟不可靠**问题,我们的单机场景不存在此问题。

### 2. 为什么不选向量时钟 (Vector Clock)？

#### 向量时钟的局限性

| 维度 | Vector Clock 特性 | 我们的场景 | 配度 |
|------|------------------|-----------|--------|
| **空间复杂度** | O(N),N=节点数 | N=Worker 数量 (动态变化) | ⚠️ 可扩展性问题 |
| **序列化开销** | 传递整个向量 | 事件高频,载荷增大 | ❌ 性能影响 |
| **使用场景** | 多端同步、CRDT 冲突检测 | 单机任务编排 | ❌ 过度设计 |
| **实现复杂度** | 需严格 merge 语义 | 增加维护成本 | ❌ ROI 低 |

**结论**: Vector Clock 适用于**多端分布式状态同步**,我们的单机多 Worker 场景不需要。

### 3. 选择方案: 增强型因果序追踪 (Causal Ordering with Logical Timestamps)

#### 3.1 核心设计

结合 **单调时钟 + 显式因果链 + 轻量序列号**,实现三层顺序保证:

```
┌─────────────────────────────────────────────────────────┐
│  Layer 1: 全局序列号 (Total Ordering)                     │
│  - std::atomic<uint64_t> sequence_number                │
│  - 用于事件日志排序、调试追踪                              │
│  - O(1) 原子递增,无锁                                     │
├─────────────────────────────────────────────────────────┤
│  Layer 2: 因果链 (Causal Chain)                          │
│  - trace_id: 当前任务唯一标识                             │
│  - parent_trace: 父任务 ID (显式声明依赖)                  │
│  - 用于重建任务依赖 DAG                                   │
├─────────────────────────────────────────────────────────┤
│  Layer 3: 单调时间戳 (Monotonic Timestamp)                │
│  - std::chrono::steady_clock::time_point                │
│  - 不受 NTP 调整/系统时间回拨影响                          │
│  - 用于性能分析、延迟计算                                  │
└─────────────────────────────────────────────────────────┘
```

#### 3.2 ToolResult 增强

```cpp
// src/core/types/tool_result.h 新增字段
struct ToolResult {
  // === 现有字段 (P0-P4) ===
  bool ok = false;
  nlohmann::json data = nlohmann::json::object();
  nlohmann::json meta = nlohmann::json::object();
  std::optional<ErrorCode> error_code;
  std::optional<std::uint64_t> latency_ms;
  std::optional<std::string> trace_id;
  std::optional<nlohmann::json> metadata;

  // === ADR-0037 新增 (3 字段) ===
  
  // Layer 1: 全局序列号 (事件发射时自动分配)
  std::optional<std::uint64_t> sequence_number;

  // Layer 2: 父任务追踪 (显式因果依赖)
  std::optional<std::string> parent_trace;
  // Layer 3: 单调时间戳 (steady_clock,不可回拨)
  std::optional<std::chrono::steady_clock::time_point> timestamp;

  // 序列化格式说明:
  // timestamp 在 JSON 中序列化为 timestamp_ns (int64_t,纳秒计数)
  // 表示从 steady_clock::epoch 开始的纳秒数
  // 注意: steady_clock 不可转换为绝对时间,仅用于相对延迟计算
};
```

#### 3.3 EventSequencer 实现

```cpp
// include/agenticdsl/contract/event_sequencer.h
namespace agenticdsl {

/**
 * @brief 全局事件序列号生成器 (依赖注入,线程安全)
 * 
 * 设计要点:
 *  - 非单例,通过依赖注入创建 (避免测试隔离问题)
 *  - std::atomic<uint64_t> 保证无锁递增
 *  - memory_order_relaxed (单变量递增,无需同步其他内存)
 *  - 从 1 开始 (0 保留表示"未分配")
 *  - 默认构造创建新实例,可通过工厂方法创建
 */
class EventSequencer {
 public:
  // 工厂方法 (推荐:每次创建新实例,保证测试隔离)
  static std::unique_ptr<EventSequencer> create() {
    return std::unique_ptr<EventSequencer>(new EventSequencer());
  }

  uint64_t next_sequence() {
    return counter_.fetch_add(1, std::memory_order_relaxed) + 1;
  }

  // 仅测试用 (生产代码不应调用)
  void reset() {
    counter_.store(0, std::memory_order_relaxed);
  }

 private:
  EventSequencer() = default;
  std::atomic<uint64_t> counter_{0};
};

} // namespace agenticdsl
```

**依赖注入使用**:
```cpp
class InMemoryBus : public IInteractionBus {
 public:
  // 构造时注入 EventSequencer (可选,默认创建新实例)
  explicit InMemoryBus(std::shared_ptr<EventSequencer> seq = nullptr)
      : sequencer_(seq ? seq : EventSequencer::create()) {}

 private:
  std::shared_ptr<EventSequencer> sequencer_;
};
```

#### 3.4 发射端集成

```cpp
// InMemoryBus::emit 增强 (锁内分配序列号,保证连续性)
void InMemoryBus::emit(const std::string& event_type,
                       const ToolResult& payload) {
  // ADR-0037: 自动注入序列号和 timestamp (在锁内,保证连续性)
  std::lock_guard<std::mutex> lock(mtx_);
  
  ToolResult enriched = payload;
  if (!enriched.sequence_number.has_value()) {
    enriched.sequence_number = sequencer_->next_sequence();
  }
  if (!enriched.timestamp.has_value()) {
    enriched.timestamp = std::chrono::steady_clock::now();
  }

  queue_.push({event_type, std::move(enriched)});
  cv_.notify_one();
  // ... 锁外通知 subscribers
}
```

#### 3.5 提交端集成

```cpp
// CognitiveWorker::submit_task 增强
void CognitiveWorker::submit_task(const std::string& task_id,
                                  const std::string& prompt,
                                  std::optional<std::string> parent_trace = std::nullopt) {
  std::lock_guard<std::mutex> lock(queue_mutex_);
  task_queue_.emplace(task_id, prompt);
  
  // 存储 parent_trace 供后续事件使用
  task_contexts_[task_id].parent_trace = std::move(parent_trace);
}

// worker_loop 中发射事件时注入
ToolResult started;
started.meta["task_id"] = task_id;
started.parent_trace = task_contexts_[task_id].parent_trace;  // ← 因果链
bus_->emit("cognitive.task.started", started);
```

---

### 4. 因果关系判定算法

#### 4.1 happens-before 判定

```cpp
/**
 * @brief 判定两个 ToolResult 的因果关系
 * 
 * 规则:
 *  1. a.sequence_number < b.sequence_number → a 可能先于 b
 *  2. a.trace_id == b.parent_trace → a happens-before b (明确因果)
 *  3. !(a→b) && !(b→a) → a 与 b 并发 (concurrent)
 * 
 * @return -1: a→b, 1: b→a, 0: concurrent
 */
int causal_order(const ToolResult& a, const ToolResult& b) {
  // 规则 2: 显式因果链 (最高优先级)
  if (a.trace_id.has_value() && b.parent_trace.has_value() &&
      a.trace_id.value() == b.parent_trace.value()) {
    return -1;  // a → b
  }
  if (b.trace_id.has_value() && a.parent_trace.has_value() &&
      b.trace_id.value() == a.parent_trace.value()) {
    return 1;   // b → a
  }
  
  // 规则 1: 序列号 (弱序,仅供参考)
  if (a.sequence_number.has_value() && b.sequence_number.has_value()) {
    uint64_t sa = a.sequence_number.value();
    uint64_t sb = b.sequence_number.value();
    if (sa < sb) return -1;
    if (sb < sa) return 1;
  }
  
  // 规则 3: 并发
  return 0;
}
```

#### 4.2 消费者端事件重组

```cpp
class EventReorderBuffer {
 public:
  void add(ToolResult evt) {
    buffer_[evt.sequence_number.value()] = std::move(evt);
    deliver_in_order();
  }

 private:
  void deliver_in_order() {
    while (buffer_.count(expected_seq_)) {
      auto evt = std::move(buffer_.at(expected_seq_));
      buffer_.erase(expected_seq_);
      deliver(std::move(evt));  // 交付给上层
      expected_seq_++;
    }
  }

  std::map<uint64_t, ToolResult> buffer_;
  uint64_t expected_seq_ = 1;
};
```

---

### 5. 替代方案

#### 方案 A: Lamport 时间戳 (被否决)

```cpp
class LamportClock {
  std::atomic<uint64_t> time_{0};
  
  uint64_t tick() {
    return time_.fetch_add(1, std::memory_order_acq_rel) + 1;
  }
  
  void receive(uint64_t remote_time) {
    uint64_t current = time_.load(std::memory_order_acquire);
    time_.store(std::max(current, remote_time) + 1, std::memory_order_release);
  }
};
```

**否决理由**:
- ❌ 需要所有参与者严格调用 `receive()` (协议脆弱)
- ❌ 只保证偏序,不保证全序
- ❌ 调试不直观 (逻辑值 vs 物理时间)
- ❌ 单机场景无收益 (共享内存已有可靠传递)

#### 方案 B: 向量时钟 (被否决)

```cpp
struct VectorClock {
  std::unordered_map<std::string, uint64_t> clocks_;
  
  void merge(const VectorClock& other) {
    for (const auto& [id, time] : other.clocks_) {
      clocks_[id] = std::max(clocks_[id], time);
    }
  }
};
```

**否决理由**:
- ❌ O(N) 空间 (N=Worker 数量,动态变化)
- ❌ 序列化开销大 (事件载荷增大)
- ❌ 适用于多端同步,单机过度设计
- ❌ 实现复杂度高 (merge 语义易错)

---

## 权衡

| 决策 | 选择 | 理由 |
|------|------|------|
| **逻辑时间戳** | sequence_number + steady_clock | 简单、可调试、全序 |
| **因果链** | parent_trace 显式声明 | 明确依赖、易理解 |
| **Lamport** | ❌ 不采用 | 单机场景无收益 |
| **Vector Clock** | ❌ 不采用 | 过度设计 |
| **序列号原子操作** | memory_order_relaxed | 单变量递增,无需同步 |
| **timestamp 类型** | steady_clock (非 system_clock) | 不受 NTP 调整影响 |

---

## 实施计划

| Phase | 任务 | 产出 | 估时 |
|-------|------|------|------|
| **T1** | 新增 EventSequencer (依赖注入) | `include/agenticdsl/contract/event_sequencer.h` | 1h |
| **T2** | ToolResult 新增 3 字段 | `src/core/types/tool_result.h` + 序列化支持 (含 timestamp_ns) | 3h |
| **T3** | InMemoryBus::emit 自动注入 | 修改 emit() 逻辑 + 构造注入 EventSequencer | 2h |
| **T4** | CognitiveWorker 支持 parent_trace | 修改 submit_task 签名 + RAII 清理 | 3h |
| **T5** | DomainWorkerPool 支持 parent_trace | 修改 submit_task 签名 | 2h |
| **T6** | 新增因果序判定工具函数 | `include/agenticdsl/contract/causal_order.h` + 传递性测试 | 3h |
| **T7** | 单元测试覆盖 | 10+ test cases (序列号/因果链/并发检测/传递性) | 5h |
| **T8** | 集成测试 (跨 Worker 事件重组) | `tests/test_causal_ordering.cpp` + TSan 排查 | 5h |
| **T9** | 文档更新 | 本 ADR + 开发者指南 + 审查反馈 | 1h |

**总估时**: ~25 小时 (3-4 工作日)

> **估时调整说明** (2026-06-26 审查后修正):
> - 原估时 18h 偏乐观,未计入:
>   - 序列化调试 (timestamp_ns 格, +1h)
>   - TSan 问题排查 (历史 pre-existing, +3h)
>   - 文档审查与团队评审 (+1h)
> - 调整后 25h 更符合实际,留足缓冲应对意外

---

## 验证标准

| 标准 | 验证方法 |
|------|---------|
| **序列号单调递增** | 并发发射 10000 个事件,验证 sequence_number 无重复、无回退 |
| **因果链正确性** | A→B→C 链式任务,验证 parent_trace 传递正确 |
| **happens-before 判定** | 构造已知因果关系,验证 causal_order() 返回正确 |
| **并发检测** | 两个无依赖任务同时提交,验证判定为 concurrent (返回 0) |
| **steady_clock 不可回拨** | 模拟系统时间调整,验证 timestamp 仍单调 |
| **TSan 干净** | 并发测试无 data race (TSan 验证) |
| **零回归** | 现有 34/34 ctest pass 保持不变 |

---

## 参考

- [ADR-0019: IInteractionBus](./adr-0019-iinteraction-bus-mvp.md)
- [ADR-0020: 多智能体线程模型](./adr-0020-thread-model-isolation.md)
- [ADR-0023: ToolResult 标准化](./adr-0023-tool-result-standard.md)
- [architecture.md §向量时钟](../specs/architecture.md#L945) (未来 Phase 3+ 场景)
- Leslie Lamport, "Time, Clocks, and the Ordering of Events in a Distributed System", 1978
- Fidge, "Timestamps in Message-Passing Systems That Preserve the Partial Ordering", 1988

---

## 未来扩展

### 与 ADR-0002 EventBus 集成

当 EventBus (MPMC 有界队列) 实现后,EventSequencer 可重构为委托模式:

```cpp
class EventSequencer {
 public:
  // 检测 EventBus 是否可用
  uint64_t next_sequence(EventBus& bus) {
    if (bus.has_global_sequence()) {
      return bus.global_sequence();  // EventBus 已保证全序
    }
    return local_sequence();  // fallback 到本地计数器
  }

 private:
  uint64_t local_sequence() {
    return counter_.fetch_add(1, std::memory_order_relaxed) + 1;
  }
};
```

**迁移路径**:
1. InMemoryBus 保留现有 EventSequencer (默认创建)
2. EventBus 实现后,InMemoryBus 构造时可选注入 EventBus
3. EventSequencer 检测 EventBus 可用时委托
4. 最终 InMemoryBus 废弃,完全迁移到 EventBus

**兼容性保证**:
- sequence_number 字段保持不变 (uint64_t)
- 消费者端因果序判定逻辑不变
- parent_trace 机制不受影响

---

## 附录 A: 文件变更清单

| 操作 | 文件路径 | 说明 |
|------|---------|------|
| **新建** | `include/agenticdsl/contract/event_sequencer.h` | EventSequencer 单例 |
| **新建** | `include/agenticdsl/contract/causal_order.h` | 因果序判定工具函数 |
| **修改** | `src/core/types/tool_result.h` | 新增 3 字段 |
| **修改** | `src/core/types/tool_result.cpp` | 序列化/反序列化支持 |
| **修改** | `include/agenticdsl/contract/inmemory_bus.h` | emit() 自动注入 |
| **修改** | `src/common/contract/inmemory_bus.cpp` | 实现注入逻辑 |
| **修改** | `include/agenticdsl/cognitive/cognitive_worker.h` | submit_task 签名 |
| **修改** | `src/modules/cognitive/cognitive_worker.cpp` | parent_trace 传递 |
| **修改** | `include/agenticdsl/cognitive/domain_worker_pool.h` | submit_task 签名 |
| **修改** | `src/modules/cognitive/domain_worker_pool.cpp` | parent_trace 传递 |
| **新建** | `tests/test_event_sequencer.cpp` | EventSequencer 测试 |
| **新建** | `tests/test_causal_ordering.cpp` | 因果序集成测试 |
| **修改** | `tests/test_tool_result.cpp` | 新增字段序列化测试 |

---

## 附录 B: 使用示例

### 示例 1: 跨 Worker 因果链

```cpp
// Worker-A 完成任务,触发 Worker-B
auto bus = std::make_shared<InMemoryBus>();

// Worker-A
CognitiveWorker worker_a(engine_a, bus);
worker_a.submit_task("A-123", "analyze code");

// Worker-A 完成后,发射事件 (自动注入 sequence_number + timestamp)
// evt.sequence_number = 42
// evt.trace_id = "A-123"

// Worker-B 提交 (显式声明因果依赖)
CognitiveWorker worker_b(engine_b, bus);
worker_b.submit_task("B-456", "fix bug", "A-123");  // ← parent_trace

// Worker-B 发射事件
// evt.sequence_number = 43
// evt.trace_id = "B-456"
// evt.parent_trace = "A-123"

// 消费者判定
int order = causal_order(evt_a, evt_b);
// order == -1 (A→B,因为 evt_a.trace_id == evt_b.parent_trace)
```

### 示例 2: 并发任务检测

```cpp
// 两个无依赖任务同时提交
worker_a.submit_task("A-1", "task 1");  // 无 parent_trace
worker_b.submit_task("B-1", "task 2");  // 无 parent_trace

// 消费者判定
int order = causal_order(evt_a, evt_b);
// order == 0 (concurrent,无因果关系)
```

### 示例 3: 事件重组缓冲

```cpp
EventReorderBuffer reorder_buf;
std::vector<ToolResult> delivered;

// 乱序到达
reorder_buf.add(evt_3);  // sequence_number = 3
reorder_buf.add(evt_1);  // sequence_number = 1 → 立即交付
reorder_buf.add(evt_2);  // sequence_number = 2 → 立即交付
                         // evt_3 缓存,等 evt_1,evt_2 交付后才交付

// delivered = [evt_1, evt_2, evt_3] (已排序)
```

### 示例 4: 传递性验证 (A→B→C)

```cpp
// 三跳因果链
ToolResult a, b, c;
a.trace_id = "A-1";
b.trace_id = "B-1"; b.parent_trace = "A-1";
c.trace_id = "C-1"; c.parent_trace = "B-1";

// 验证传递性
REQUIRE(causal_order(a, b) == -1);  // A → B
REQUIRE(causal_order(b, c) == -1);  // B → C
REQUIRE(causal_order(a, c) == -1);  // A → C (传递性)

// 注意: causal_order() 仅判定直接因果 (1-hop)
// 如需传递性,需构建 DAG 并计算可达性
```

### 示例 5: 时间戳序列化

```cpp
ToolResult r;
r.timestamp = std::chrono::steady_clock::now();

// 序列化为 JSON
auto j = r.to_json();
// j["timestamp_ns"] = 1234567890123456789 (纳秒计数)

// 反序列化
auto rt = ToolResult::from_json(j);
// rt.timestamp 重建为 steady_clock::time_point

// 计算延迟 (同一 steady_clock 起点)
auto latency = rt.timestamp.value() - start_time;
// latency 可用于性能分析,但不可转换为绝对时间
```

