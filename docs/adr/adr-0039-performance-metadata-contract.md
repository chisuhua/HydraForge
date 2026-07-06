# ADR-0039: 推理引擎性能元数据契约

## 状态

🔍 Proposed (2026-07-06 — 架构方案讨论产出, 待 review); **2026-07-06 renumber**: 兄弟 ADR-0036 → ADR-0045 (编排 plugin), ADR-0037 → ADR-0046 (通信协议), 避免与旧 ADR-0036-三层服务协议 / ADR-0037-因果序冲突

## 领域

基座 / Inference Engine / Observability

## 关联

- ADR-0035 (Inference Engine Plugin Spec)
- ADR-0046 (Plugin Communication Protocol) — 通道 ② Event Layer + 通道 ④ Query Layer
- ADR-0019 (IInteractionBus) — EventBus 基础设施

---

## 背景

编排 Plugin 做模型选择/路由/调度决策时需要推理引擎的性能数据。需要定义: 什么指标通过 EventBus 推送, 什么指标通过 query tool 拉取。

---

## 决策

### 1. EventBus: 仅低频高语义生命周期事件 (Oracle 推荐, P1 fix 用 dot 分隔符对齐 ADR-0046 §2)

| Topic (dot 分隔符) | 触发条件 | 频率 | Payload |
|-------|---------|:----:|---------|
| `inference.lifecycle.{state}` | 状态转换: idle/running/model_loaded/model_unloaded/session_active/context_overflow/oom | 每推理 ~1-2 次 | `{state, model_id, timestamp}` |
| `inference.model.{action}` | 模型加载/卸载/切换 (loaded, unloaded, switched) | 低频 | `{model_id, action, n_params}` |
| `inference.error.{code}` | 推理错误 | 异常时 | `{code, message, retryable, session_id}` |
| `inference.session.{action}` | Session 创建/销毁 (P1 fix per Oracle review) | 低频 | `{session_id, model_id, action}` |

**订阅示例** (编排 Plugin per ADR-0045 §4):
```cpp
bus.subscribe_topic("inference.lifecycle.*", callback);     // glob 匹配所有 lifecycle
bus.subscribe_topic("inference.error.oom", callback);       // 单事件精确订阅
```

**不推送**: 性能指标 (t/s, KV cache%, GPU mem) — 高频数据会触发 Sprint 12 bridge 背压。

### 2. Query Tool: 按需快照 (atomic 读取, P1 fix 与 ADR-0035 §6 schema 对齐)

`inference/get/status`:

```json
{
  "engine": {
    "uptime_seconds": 3600,
    "backend": "triton",
    "backend_version": "CUDA 12.4, SM 89"
  },
  "device": {
    "name": "NVIDIA RTX 4090",
    "total_mem_mb": 24576,
    "free_mem_mb": 16384,
    "compute_cap": "8.9"
  },
  "models": [{
    "model_id": "qwen3-7b",
    "n_params": 7000000000,
    "n_ctx": 8192,
    "kv_cache_used_pct": 0.35,
    "avg_tg_tok_s": 45.2,
    "avg_pp_tok_s": 3200
  }],
  "sessions": [{
    "session_id": "ses_001",
    "model_id": "qwen3-7b",
    "state": "idle"
  }],
  "performance": {
    "avg_tg_tok_s": 45.2,
    "avg_pp_tok_s": 3200,
    "p50_latency_ms": 22,
    "p99_latency_ms": 45
  }
}
```

**字段类型与单位** (P1 fix per Oracle review):
| 字段 | 类型 | 单位 | 范围 |
|------|------|------|------|
| `n_params` | uint64 | (无量纲) | 1B-1T |
| `total_mem_mb`/`free_mem_mb` | uint32 | MB | 0-N |
| `kv_cache_used_pct` | float | [0.0-1.0] | (0=空, 1=满) |
| `avg_tg_tok_s`/`avg_pp_tok_s` | float | tokens/sec | >0 |
| `p50/p99_latency_ms` | uint32 | ms | ≥0 |
| `uptime_seconds` | uint64 | seconds | ≥0 |
| `compute_cap` | string | "8.9" (major.minor) | — |

`inference/get/models`:

```json
[{
  "model_id": "qwen3-7b",
  "model_name": "Qwen3 7B",
  "arch": "Qwen3",
  "n_params": 7000000000,
  "n_ctx_train": 32768,
  "n_ctx_actual": 8192,
  "kv_cache_used_pct": 0.35,
  "avg_tg_tok_s": 45.2,
  "avg_pp_tok_s": 3200,
  "status": "loaded"
}]
```

**`inference/get/models` ↔ `ILLMProvider::available_models()` 映射** (P1 fix per Oracle review):

| C++ `ModelCapability` 字段 (待 ADR-0001 附录补) | JSON 字段 |
|-----|-----|
| `id` | `model_id` |
| `name` | `model_name` |
| `arch` | `arch` |
| `n_ctx_train` | `n_ctx_train` |
| (runtime) | `n_ctx_actual` |
| (runtime) | `status` |
| `per_token_cost` | (defer to C7 ADR-0034, 不在本 ADR scope) |
| `tags` | (defer) |
| (runtime) | `kv_cache_used_pct`/`avg_*_tok_s` |

### 3. 快照一致性保证

所有字段一次性 atomic 读取 (Plugin 内部 `std::lock_guard` 保护共享状态), 避免拼装错位。

**快照新鲜度** (P1 fix): 数值字段 (`avg_*`, `kv_cache_used_pct`, latency) 基于最近 N=100 次生成的滚动平均, `p50/p99_latency_ms` 基于最近 N=200 次请求。TTL 不缓存 — 每次 `inference/get/status` 调用触发新鲜测量 (Plugin 维护滑动窗口)。高频轮询的开销由 Plugin 内部 spinlock + atomic 实现可控 (纳秒级)。

---

*创建日期*: 2026-07-06
*依赖*: ADR-0035, ADR-0046
