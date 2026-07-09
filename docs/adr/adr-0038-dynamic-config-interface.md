# ADR-0038: 推理引擎动态配置接口

## 状态

🔍 Proposed (2026-07-06 — 架构方案讨论产出, 待 review; **2026-07-06 P1 fix** 应用: L3a/L3b split, `offload_kqv` 移除, prefer enum 补全, error schema 新增, thread safety 细化); **2026-07-06 renumber**: 兄弟 ADR-0036 → ADR-0045 (编排 plugin), ADR-0037 → ADR-0046 (通信协议), 避免与旧 ADR-0036-三层服务协议 / ADR-0037-因果序冲突

**增量决议** (2026-07-09, per OpenSpec change `phase5-illmprovider-call-chain-v2` Task 7.5 + Adversarial Review 2026-07-06): `BatchingQueue` 接口 **deferred 到第二个推理 backend 实现时** (e.g., vLLM/SGLang plugin 出现)。当前 `llama_engine` plugin 单后端场景下, `BatchingQueue` 抽象 (作为独立 PDK 接口 `include/agenticdsl/pdk/batching_queue.h`) 不提取, 推理请求调度内联到 plugin 内部。提取时机: 出现第二个推理后端 + 调度策略出现实质差异时, 按 ADR-0034 "核心保留契约 + 算法 plugin 化" 范式重新评估。关联 OpenSpec change `phase5-batching-queue-plugin` (C15, 正交)。

## 领域

基座 / Inference Engine / Config

## 关联

- [ADR-0035 (Inference Engine Plugin Spec)](./adr-0035-inference-engine-plugin-spec.md) — 推理引擎 Plugin 配置入口 + §4 L3a/L3b 分层 (P1 fix 对齐源)
- [ADR-0046 (Plugin Communication Protocol)](./adr-0046-plugin-communication-protocol.md) — 通道 ③ Config Layer
- llama.cpp C API — 参数分层依据 (llama.h)

---

## 背景

### 问题

编排 Plugin 需要在运行时动态调节推理引擎参数 (线程数, 采样默认值, 内存偏好等), 但当前无标准化接口。

### 目标

定义 `inference/configure` (L3a) + `inference/sampler/configure` (L3b) tools 的:
1. 完整的参数 Schema (L1-L4 + L3a/L3b 分层)
2. 生效时机分类 (即时 / next-request / needs-restart / session-rebuild)
3. 返回值格式 (含错误)
4. 线程安全策略

---

## 决策

### 1. 参数分四层 (含 L3a/L3b 细分) (P1 fix)

| 层级 | 配置入口 | 粒度 | 生效时机 | 示例参数 |
|------|---------|:----:|:--------:|---------|
| **L1 - 模型层** | `inference/engine/init` | 模型加载时 (仅一次) | 仅初始化时 | `n_gpu_layers`, `split_mode`, `use_mmap`, `use_mlock`, `devices`, `tensor_split`, `main_gpu` |
| **L2 - 上下文层** | `inference/session/create` | 会话创建时 | 仅 session create 时 | `n_ctx`, `n_batch`, `n_ubatch`, `flash_attn_type`, `type_k`, `type_v`, **`offload_kqv`**, `rope_scaling_type` |
| **L3a - 动态参数** | `inference/configure` | 运行时 (即时 / next-request) | 即时 / next-request | `n_threads`, `n_threads_batch`, `default_temperature`, `default_top_k`, `default_top_p`, `default_min_p`, `prefer`, `embeddings_mode`, `warmup` |
| **L3b - 采样策略** | `inference/sampler/configure` | 需重建 sampler chain | session rebuild | `chain_topology`, sampler 类型组合 (greedy/temperature/top_k/mirostat/grammar/DRY) |
| **L4 - 请求层** | `inference/generate` 入参 | 每次生成 | per-request | `temperature`, `top_k`, `top_p`, `min_p`, `seed`, `max_tokens`, `stop`, `grammar`, `penalties`, `mirostat` |

**L3a vs L3b 的区别** (P1 fix per Oracle review):

| 维度 | L3a (`inference/configure`) | L3b (`inference/sampler/configure`) |
|------|:---:|:---:|
| **调整对象** | 采样**数值参数** (温度/top_k 等) | 采样**策略** (chain composition) |
| **生效时机** | 即时 (`n_threads`) / next-request (`default_temperature`) | session rebuild — 需新 session |
| **线程安全** | 单字段 `atomic` (per param type) | 单字段 `atomic<shared_ptr<sampler_chain>>` — 整体替换 |
| **举例** | `default_temperature: 0.5` (改 default) | sampler chain 从 `[top_k(50), temp(0.7)]` 改为 `[mirostat(0.1), grammar(json)]` |

**L3a vs L4 的区别**: L3a 设**默认值** (apply to future requests), L4 设**单次覆盖值** (override for this request only)。

**L3a 与 L3b 完全独立**: L3a 改 `default_temperature` 时不触 L3b (sampler chain 引用 default 时自动用新值); L3b 改 chain 时可引用旧 L3a 值。

### 2. L3a `inference/configure` Schema (P1 fix 移除 `offload_kqv`)

| 参数 | 类型 | 默认 | 范围 | 生效 | 线程安全 | Llama C API 映射 |
|------|------|------|------|:----:|---------|-----------------|
| `n_threads` | int | 4 | [1, 256] | **即时** | `atomic<int>` | `llama_set_n_threads(ctx, n_threads, n_threads_batch)` |
| `n_threads_batch` | int | 4 | [1, 256] | **即时** | `atomic<int>` | 同上 (两参数一次调用) |
| `default_temperature` | float | 0.7 | [0.0, 2.0] | next-request | `atomic<float>` | sampler chain rebuild |
| `default_top_k` | int | 40 | [-1, 1000] | next-request | `atomic<int>` | sampler chain rebuild |
| `default_top_p` | float | 0.9 | [0.0, 1.0] | next-request | `atomic<float>` | sampler chain rebuild |
| `default_min_p` | float | 0.05 | [0.0, 1.0] | next-request | `atomic<float>` | sampler chain rebuild |
| `prefer` | enum | `balanced` | (见下) | next-request | `atomic<Prefers>` | internal scheduler hint |
| `embeddings_mode` | bool | false | — | next-request | `atomic<bool>` | `llama_set_embeddings(ctx, embeddings)` |
| `warmup` | bool | false | — | 即时 | `atomic<bool>` | `llama_set_warmup(ctx, warmup)` |

**`offload_kqv` 移出 L3a (P1 fix per Oracle review)**: `offload_kqv` 是 L2 `inference/session/create` 参数, 不能在 context 运行时修改。若 `inference/configure` 收到 `offload_kqv` 参数, 返回 `rejected: {offload_kqv: "L2 parameter, use inference/session/create"}`。

**`prefer` enum 显式定义** (P1 fix per Oracle review):
```cpp
enum class Prefer { latency, memory_saving, quality, throughput, balanced };
```

- `latency`: 优先低延迟, 减少 batch / 单 token 优化
- `memory_saving`: 减少 KV cache 占用, 降 `n_ctx`
- `quality`: 优先质量, 可启用更慢 sampler 路径
- `throughput`: 优先 batch throughput, 增加 `n_batch`
- `balanced`: 默认, 无偏好

**L3a 参数 vs `inference/generate` 请求覆盖**: L3a 是 default, L4 是该次 override。若 L3a `default_temperature=0.5`, `inference/generate` 入参 `temperature=0.8`, 则该次用 0.8, 之后仍 default 0.5。

### 3. L3b `inference/sampler/configure` Schema

待 [ADR-0035 §2 (L3b 工具定义)](./adr-0035-inference-engine-plugin-spec.md) 单独定义完整 schema; 本 ADR 暂列要点:

```json
// inference/sampler/configure Request
{
  "chain": ["top_k", "temperature", "greedy"],   // 顺序定义 sampler 链
  "samplers": {
    "top_k": {"k": 50},
    "temperature": {"t": 0.7},
    "greedy": {}
  }
}

// Response
{
  "applied": true,
  "requires_new_session": true,  // chain 重建需新 session
  "new_chain_topology": ["top_k", "temperature", "greedy"]
}
```

**L3b 与 L4 关系**: L3b 定 chain 拓扑 + 默认参数。L4 `inference/generate` 的 sampler 参数是 L3b 默认值的覆盖。

### 4. Response Schema (P1 fix 增加 `rejected` + `errors` 字段)

```json
{
  "applied": {
    "n_threads": true,
    "n_threads_batch": true,
    "default_temperature": true,
    "default_top_k": false,
    "prefer": true
  },
  "requires_restart": {},                                       // L3a 不需重启的字段
  "rejected": {
    "default_top_k": "value 2000 > max 1000"
  },
  "errors": [
    {"code": "INVALID_VALUE", "field": "default_top_k", "value": "2000", "max": 1000}
  ],
  "current": {
    "n_threads": 8,
    "n_threads_batch": 8,
    "default_temperature": 0.5,
    "default_top_k": 40,
    "default_top_p": 0.9,
    "default_min_p": 0.05,
    "prefer": "latency",
    "embeddings_mode": false,
    "warmup": false
  }
}
```

**字段含义** (P1 fix):
- `applied`: 哪些参数接受了变更 (true = 已应用, false = 已被下面 `rejected` 解释)
- `requires_restart`: 哪些参数需要重启引擎才生效 (本层无, 仅为 L1/L2 兼容字段)
- `rejected` (P1 fix 新): 哪些参数**被拒绝**及简短原因 (一个 field 一行)
- `errors` (P1 fix 新): 错误详情列表 (一个错误一个 entry, 含 `code`/`field`/`value`/`max`)
- `current`: 当前生效的全部 L3a 配置值 (含未变化的)

**L3b Response** 含额外字段:
- `requires_new_session: bool` — 提示编排 Plugin 新 session 才能用新 chain
- `new_chain_topology: string[]` — 新链顺序

### 5. 线程安全策略 (P1 fix 细化)

| 参数类型 | 容器 | 访问方式 |
|---------|------|---------|
| **L3a 标量** (`n_threads`/`prefer` 等) | `std::atomic<T>` (T=int/float/bool/enum) | `load()`/`store()` (relaxed memory order) |
| **L3b chain** | `std::atomic<std::shared_ptr<SamplerChain>>` | 整体 CAS 替换, 旧 chain 在引用计数归零前仍可用 (无锁 reader) |
| **L2 上下文参数** (`n_ctx`/`flash_attn` 等) | session mutex (per-session `std::mutex`) | session 重建时独占修改 |
| **L1 模型参数** (不可变) | const after init | 仅 `inference/engine/init` 一次性设置 |

**热路径性能**: generate() 读 L3a 用 `atomic.load()` (零开销), 读 L3b 用 `atomic<shared_ptr>.load()` + `samples_chain_.apply(...)` (reference count 拷贝, 纳秒级)。所有读路径无锁。

### 6. 设计原则

1. **最小化热路径开销**: L3 配置在 Plugin 内部用 std::atomic 或 mutex 保护, generate 时不走 JSON 解析路径。
2. **幂等性**: 重复调用相同参数应返回一致的 `applied`/`current` 结果。
3. **非破坏性**: 配置错误不应导致引擎崩溃, 应拒绝无效值并保留 previous 状态 (通过 `rejected`/`errors` 反馈)。
4. **向后兼容**: 增加新参数不破坏现有调用; 旧参数去除时返回 deprecation warning, 隔 release 删除。

---

## 实施要点

- Plugin 内部维护 `InferenceConfig` 结构体 (atomic 字段标量 + atomic<shared_ptr> chain), 每次 generate 时读取最新值
- `inference/configure` (L3a) tool handler: validate → 分类参数 (immediate atomic vs next-request atomic) → apply → build response
- `inference/sampler/configure` (L3b) tool handler: build new chain → atomic<shared_ptr>.store() → set requires_new_session=true
- L1/L2 参数在 `inference/engine/init` 和 `inference/session/create` 中分别处理, L3a/b 不触及

---

*创建日期*: 2026-07-06
*修订*: 2026-07-06 (P1 fix 应用: L3a/L3b split + `offload_kqv` 移除 + Response 增 rejected/errors + prefer enum 显式定义 + thread safety 细化)
*依赖*: [ADR-0035 §4](./adr-0035-inference-engine-plugin-spec.md) (L3a/L3b 划分), [ADR-0046 §3](./adr-0046-plugin-communication-protocol.md) (Config Layer)
