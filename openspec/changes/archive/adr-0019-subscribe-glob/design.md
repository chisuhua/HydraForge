# Subscribe Globbing 设计

## Context

Change A 完成后，IInteractionBus 已统一为 `subscribe(event_type, callback(BusEvent))`。本 change 扩展 event_type 接受 glob pattern，无需新增单独 API。

Oracle 评审建议：合并 subscribe + subscribe_topic 为单一接口，无通配符字符串退化为精确匹配（零开销）。实现 O(1) map + O(n) glob match 双路径分发。

## Design

### 接口（零破坏）

```cpp
// IInteractionBus 接口不变，仅语义扩展：
//   event_type 现在接受 glob pattern ("inference.lifecycle.*")
//   subscribe 仍返回 size_t token
//   unsubscribe(size_t token) 不变（已存在）
class IInteractionBus {
public:
    virtual size_t subscribe(const std::string& event_type,  // 精确串 OR glob
                             std::function<void(const BusEvent&)>) = 0;
    virtual void unsubscribe(size_t token) = 0;             // 不变
};
```

**关键语义**：
- `subscribe("inference.lifecycle.idle", cb)` → 精确匹配，走现有 O(1) map
- `subscribe("inference.*", cb)` → glob match，走新增 wildcard 列表
- 退订：`unsubscribe(token)` 同时处理精确 + glob 路径

### InMemoryBus 分发逻辑

```cpp
void InMemoryBus::dispatch_loop() {
    while (true) {
        auto event = queue_.try_consume();
        if (!event) continue;

        // Fast path: 精确匹配 map（O(1), 现有逻辑，不变）
        auto it = exact_subscribers_.find(event.topic);
        if (it != exact_subscribers_.end()) {
            for (auto& [token, cb] : it->second) cb(event);
        }

        // Slow path: 遍历 wildcard subscribers（新增）
        for (auto& [pattern, cbs] : wildcard_subscribers_) {
            if (glob_match(pattern, event.topic)) {
                for (auto& [token, cb] : cbs) cb(event);
            }
        }
    }
}
```

### Glob 匹配

```cpp
// 内部 helper: glob_match("inference.*", "inference.lifecycle.idle") → true
// 支持 * 匹配任意字符; ? 匹配单个字符
// 不使用 regex (编译开销大，glob 足够覆盖 PDK 事件命名)
static bool glob_match(const std::string& pattern, const std::string& topic);
```

**性能特征**：
- 无通配符 pattern → 零额外开销（走现有 exact_subscribers_）
- 含通配符 pattern → O(w) 其中 w = wildcard subscriber 数量
- 预期 <50 wildcard subscribers，<10K events/s → 可忽略

### subscribe/unsubscribe 变更

```cpp
// 内部存储
std::unordered_map<std::string,
    std::vector<std::pair<size_t, Callback>>> exact_subscribers_;     // 现有
std::unordered_map<std::string,
    std::vector<std::pair<size_t, Callback>>> wildcard_subscribers_;  // 新增
```

`subscribe()` 根据 pattern 是否含通配符路由到不同 map；`unsubscribe()` 从对应 map 移除。

### InFlight 语义

Oracle 提醒：dispatch 在锁内拷贝回调、锁外执行。unsubscribe 期间 in-flight 回调仍会执行一次（与现有行为一致，锁语义文档化）。

## Risks / Trade-offs

| 风险 | 缓解 |
|------|------|
| glob_match O(n*m) 性能 | n=wildcards (<50), m=topic length (<50) → ~2500 char 比较 / event |
| 双 map 存储开销 | wildcard 数量极低，内存可忽略 |
| 与 subscribe 精确语义冲突 | pattern 含 `*` 或 `?` 时路由至 wildcard，不冲突 |