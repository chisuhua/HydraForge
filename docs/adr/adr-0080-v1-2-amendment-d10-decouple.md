# ADR-0080 v1.2 amendment: D10 Capture 与 Scrub Hook 解耦

**日期**: 2026-08-24
**状态**: ✅ **Approved (评审通过 2026-08-25)** (Oracle 评审识别为架构层缺口, capability-application-map §八 G12)
**父 ADR**: [adr-0080-append-only-event-log.md](adr-0080-append-only-event-log.md) v1.1

## 状态

✅ Approved (评审通过 2026-08-25, 详见头部状态字段) — Oracle 评审识别为架构层缺口, capability-application-map §八 G12

**前置文档**:
- `docs/architecture/capability-application-map-2026-08.md` §八 Oracle 评审
- Oracle session `ses_fcba5e477ffeG9wEBHVhU64J0o`
- ADR-0080 v1.1 §决策 D10
- ADR-0081 (Proposed, 推迟至 ADR-0082)
- ADR-0082 (Proposed, 已搁置)

---

## 背景

**ADR-0080 v1.1 §决策 D10.3** 原文：

> 开启后依赖 ADR-0081 pre-step hook 在 emit 前 scrub（先 scrub 后落盘）——**未 ship 时不暴露**。

**Oracle 评审关键发现**（2026-08-24, session `ses_fcba5e477ffeG9wEBHVhU64J0o`）：

> 已 Approved 的 D10 蒸馏数据契约被 Proposed 链锁住——ADR-0081 (Proposed, 自身推迟到 ADR-0082 定稿) → ADR-0082 (已搁置, v1.1 注记"蒸馏需求加强搁置")。
>
> 这是**已 Approved 契约被 Proposed 链锁住**——需 v1.2 amendment 解耦。

**影响链**：

```
ADR-0078 Fine-tune (Proposed)         ─┐
ADR-0061-09 GEPA 反思循环 (Proposed)   ─┤
ADR-0061-08 AFlow MCTS (Proposed)      ─┤── 全部依赖 EventLog 蒸馏数据
ADR-0061-13 蒸馏输出格式 (拟新立)      ─┤
ADR-0080 D10 Distillation Capture      ─┘── 当前被 ADR-0081 → 0082 死锁
       │
       └─ 死锁: ADR-0081 (Proposed, 待 0082) → 0082 (已搁置)
```

**结论**: 自进化方向的**数据面第一步**永远无法启动,除非解耦 D10 capture 与 scrub hook。

---

## 决策

### 决策 D10.v1.2.1 — 三种运行模式

`EventLogConfig::capture_prompt_bytes` 升级为 `CaptureMode` 枚举:

```cpp
enum class CaptureMode {
  Off,           // v1.1 默认: 仅 hash, 零字节落盘 (行为不变)
  Online,        // v1.1 D10 语义: 开启 + 强制 ADR-0081 scrub hook
  Training,      // v1.2 新增: 开启 + 文档化"训练环境专用" + 跳过 scrub 强制
};
```

**三种模式对比**:

| 模式 | 字节落盘 | scrub 强制 | 适用场景 |
|---|---|---|---|
| `Off` | ❌ | — | 生产 / 默认 examples（v1.1 行为） |
| `Online` | ✅ | **必须** ADR-0081 scrub hook 已 ship | Cloud 服务 |
| `Training` | ✅ | **跳过**（fail-open + 警告）| 离线训练 / 数据采集实验 |

### 决策 D10.v1.2.2 — `Training` 模式的 fail-open 条件

`Training` 模式开启必须**同时满足**:

1. `agent_id` 已设置（D6 fail-closed 不变）
2. 输出目录包含 `train` 或 `distill` 子串（如 `/var/hydra/train/`）—— 防止误用生产目录
3. **CLI 显式标志** `--allow-training-capture` —— 防止误开启
4. EventLog 启动日志输出 **WARNING** 级别："Training mode: prompt bytes captured WITHOUT scrubbing — DO NOT deploy to production"

任一条件不满足 → 抛异常,拒绝启动。

### 决策 D10.v1.2.3 — `Online` 模式解锁前置

`Online` 模式仍依赖 ADR-0081 scrub hook,与 v1.1 行为一致。但增加**运行时检查**:

- 启动时若 `Online=true` 但 `IAgentHookRegistry` 未注入 scrub hook → **WARN + 降级为 Training**（而非完全拒绝）
- 记录 `event_log.capture_mode_downgrade` 审计事件

### 决策 D10.v1.2.4 — D10.6 audit 防线扩展

V1.1 D10.6 仅约束 tool args 不落盘。V1.2 新增:

- Training 模式必须在 EventLog JSONL header 标记 `"capture_mode": "Training"`
- `EventLogWriter::read()` 在读取 Training 模式文件时输出 **WARNING**（不阻止读取,但提示非生产数据）
- pdk_chat_demo / examples 默认 `Off`,即使显式设 `Training` 也需 `--allow-training-capture` CLI 标志

### 决策 D10.v1.2.5 — V1 vs V2 拆分

| 版本 | 范围 |
|---|---|
| **V1 (本 amendment)** | CaptureMode 三态 + fail-open 条件 + audit 标记 + 启动降级 |
| **V2 (future ADR)** | 与 ADR-0081 v1.1 集成完整 Online 模式 + 跨进程 scrub 协议 |

**V1 解锁**: 自进化方向可在 **Training 模式**下启动蒸馏数据采集,不等 ADR-0081/0082 ship。

---

## 不变量

- D10.v1.2.invariant.1: Training 模式绝不部署到 production（CLI 标志 + 路径前缀 + WARNING 三重保护）
- D10.v1.2.invariant.2: `CaptureMode` 字段持久化到 EventLog JSONL header,可追溯
- D10.v1.2.invariant.3: 模式降级（Online→Training）必须写审计事件,不留静默路径

---

## 实施

- **文件**:
  - `include/agenticdsl/types/capture_mode.h` (新枚举)
  - `src/core/event_log_config.h` (D10 字段类型升级)
  - `src/core/event_log.cpp` (启动时校验 + WARNING + 降级逻辑)
  - `src/core/engine.cpp` (CLI `--allow-training-capture` 解析)
  - `tests/test_event_log_capture_mode.cpp` (≥ 5 cases)
- **估时**: 0.5 sprint
- **优先级**: P0 (Oracle: 本周最高杠杆)

---

## 风险

| 风险 | 影响 | 缓解 |
|---|---|---|
| Training 模式误用导致 PII 泄漏 | 严重 | CLI 标志 + 路径前缀 + WARNING 三重保护 |
| 模式降级静默发生 | 用户不知情 | `event_log.capture_mode_downgrade` 审计事件 |
| 与 ADR-0081 集成未来冲突 | V2 升级时需小心 | V2 拆分明确, V1 仅做数据面 |

---

## 与现有文档的协调

- ADR-0080 v1.1 §决策 D10.3 修订为 v1.2 文本（保留 Online 语义,新增 Training 模式）
- `capability-application-map-2026-08.md` §八 G12 状态更新
- 新增 OpenSpec change: `2026-08-24-adr-0080-v1-2-amendment-d10-decouple`

---

## 参考

- Oracle 评审: session `ses_fcba5e477ffeG9wEBHVhU64J0o`
- ADR-0080 v1.1 §决策 D10.3 (line 281)
- ADR-0081 (Proposed) - 不再作为 Online 模式前置的唯一路径
- ADR-0082 (Proposed, 已搁置) - 不再阻塞 D10 Training 模式
- 类似模式参考: Python `logging` 模块的 `WARNING` 级别 fail-open