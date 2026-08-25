# pdk_chat_demo JSONL 临时数据源调研报告

> **任务**: B3 (capability-application-map §八 Phase B)
> **日期**: 2026-08-24
> **作者**: Architecture Working Group
> **目标**: 评估 pdk_chat_demo Session JSON 是否可作为 ADR-0080 v1.2 Training 模式 D10 解锁前的**过渡数据源**

---

## 一、pdk_chat_demo Session 现状

### 1.1 存储格式（实测 `chat_session.cpp:480-533`）

**JSON 单文件**（非 JSONL）:
```json
{
  "schema_version": 1,
  "session_id": "sess-uuid",
  "created_at": 1234567890,
  "updated_at": 1234567891,
  "provider_mode": "mock" | "openai" | "deepseek",
  "budget": {
    "total": 10.0,
    "used": 2.5
  },
  "history": [
    {"role": "user", "content": "..."},
    {"role": "assistant", "content": "..."},
    {"role": "tool", "content": "..."}
  ]
}
```

### 1.2 存储位置

- 默认: `~/.hydraforge/sessions/<session_id>.json`（`chat_session.h:39`）
- 单文件 per session（不是 JSONL append-only）

### 1.3 与 DistillationRecord 的差距

| DistillationRecord 字段 | pdk_chat_demo 是否提供 | 差距 |
|---|---|---|
| `input` (user_input) | ✅ `history[i].content` (role=user) | OK |
| `output` (response_text) | ✅ `history[i].content` (role=assistant) | OK |
| `steps` (ReAct 序列) | ❌ 未保留 thought/action/observation | **缺失** |
| `reward` | ⚠️ 仅 success/failure（budget used 间接） | **粗粒度** |
| `trace_id` | ❌ 无 causal_time/event_id 引用 | **缺失** |
| `source_event` | ❌ 无 EventLog event_id | **缺失** |
| `teacher_version` | ❌ provider_mode ≠ agent 版本 | **缺失** |
| `generation_timestamp_ms` | ⚠️ created_at/updated_at（粗粒度）| OK（粗粒度）|

**结论**: pdk_chat_demo Session JSON 提供**部分**蒸馏数据需求，**缺 3 个关键字段**（steps/trace_id/teacher_version）。

---

## 二、pdk_chat_demo SessionWriter 已 ship

`src/core/session_writer.{h,cpp}` 已 ship（T5 P5 session-writer-bridge, 2026-08-20）:
- ✅ 订阅 IInteractionBus 事件
- ✅ D6 白名单 13 topic
- ✅ 行帧 JSONL 输出
- ✅ flush + fsync
- ✅ `tests/test_session_writer.cpp` 8 cases PASS
- ✅ `tests/test_session_writer_eventlog_integration.cpp` 4 cases PASS

**优势**: 与 EventLogWriter 平行订阅,**所有事件落盘**,不仅 chat_session 的 user/assistant messages。

---

## 三、过渡数据源建议

### 3.1 推荐方案: `SessionWriter` JSONL（而非 pdk_chat_demo Session JSON）

| 维度 | pdk_chat_demo JSON | SessionWriter JSONL |
|---|---|---|
| **数据完整度** | 仅 chat 消息 (user/assistant/tool) | 全部 13 D6 topic |
| **可追溯性** | created_at 时间戳 | causal_time + event_id |
| **格式** | 单 JSON per session | JSONL append-only |
| **pdk 耦合** | 高（pdk_chat_demo 特定） | 低（与 EventLog 一致）|
| **现有 ship 状态** | ✅ | ✅ |
| **复用率** | pdk_chat_demo only | 全部 HydraForge 应用 |

**推荐**: 用 **`SessionWriter` JSONL** 作为过渡数据源,而非 pdk_chat_demo Session JSON。

### 3.2 实施步骤（≤ 0.5 sprint）

#### Step 1: 转换脚本 (`scripts/session_to_distill.py`)

```python
# 读取 SessionWriter JSONL 输出
# 过滤 llm.request + llm.response 事件
# 输出为 ADR-0061-13 蒸馏格式 (trajectory.jsonl + policy.jsonl)
import json
import sys

def convert_session_to_distill(jsonl_path, output_dir):
    trajectory_records = []
    policy_records = []
    
    # 配对 llm.request 和 llm.response
    pending_requests = {}
    
    for line in open(jsonl_path):
        event = json.loads(line)
        if event.get("topic") == "llm.request":
            event_id = event.get("event_id") or event.get("causal_time")
            pending_requests[event_id] = event
        elif event.get("topic") == "llm.response":
            event_id = event.get("causal_time") or event.get("event_id")
            req = pending_requests.pop(event_id, None)
            if req and event.get("payload", {}).get("ok"):
                # 输出 DistillationRecord
                trajectory_records.append({
                    "trace_id": f"{event_id}",
                    "input": req["payload"]["args"].get("prompt", ""),
                    "output": event["payload"]["args"].get("text", ""),
                    "reward": {"quality": "Acceptable", "scalar": 0.5},
                    "trace_id_ref": event_id,
                    "source_event": event_id,
                    "teacher_version": "pdk_chat_demo_mock_v1",
                    "generation_timestamp_ms": event.get("timestamp_ms", 0)
                })
                # policy record
                policy_records.append({
                    "prompt": req["payload"]["args"].get("prompt", ""),
                    "completion": event["payload"]["args"].get("text", ""),
                    "weight": 1.0
                })
    
    # 写出
    with open(f"{output_dir}/trajectory.jsonl", "w") as f:
        for r in trajectory_records:
            f.write(json.dumps(r) + "\n")
    with open(f"{output_dir}/policy.jsonl", "w") as f:
        for r in policy_records:
            f.write(json.dumps(r) + "\n")
    # meta.json
    meta = {
        "version": 1,
        "total_examples": len(trajectory_records),
        "dataset_hash": "<sha256>",
        "generation_config": {
            "source": "session_writer_jsonl",
            "capture_mode": "Training"  # 标记为非生产
        }
    }
    with open(f"{output_dir}/meta.json", "w") as f:
        json.dump(meta, f, indent=2)
```

#### Step 2: 验证脚本 (`tests/test_session_to_distill.py`)

```python
# 输入: fixture SessionWriter JSONL (5 events)
# 输出: 3 trajectory + 3 policy records (2 successful pairs)
# 断言: 
#   - meta.json.total_examples == 2
#   - 所有 records 字段对齐 ADR-0061-13
#   - capture_mode="Training" 标记
```

#### Step 3: 文档更新

- 在 `examples/pdk_chat_demo/README.md` 新增 "Distillation Mode" 章节
- 在 `scripts/README.md` 文档化转换脚本

---

## 四、pdk_chat_demo Session JSON 的替代价值

虽然推荐 SessionWriter JSONL,但 pdk_chat_demo Session JSON 仍可用于:

| 用途 | 价值 |
|---|---|
| **快速 demo** | 无需 SessionWriter 启动,直接读取 `~/.hydraforge/sessions/*.json` |
| **用户数据导入** | 用户迁移现有数据到 HydraForge 时可解析 |
| **轻量级复现** | 单元测试 fixture（test_session_persistence 已有）|

---

## 五、风险与局限

### 5.1 已知风险

| 风险 | 影响 | 缓解 |
|---|---|---|
| SessionWriter 仅 ship D6 白名单 13 topic,可能漏掉某些有用事件 | 数据不完整 | 评估 D6 白名单是否覆盖蒸馏需求 |
| `capture_mode` 标记缺失（pdk_chat_demo JSON 无此字段）| 误用风险 | 转换脚本强制写入 `meta.json.capture_mode="Training"` |
| 时间戳精度（created_at 秒级,EventLog causal_time 纳秒级）| trace 对齐误差 | 优先用 EventLog causal_time |
| teacher_version 字段缺失（pdk_chat_demo 仅 provider_mode）| 学生版本无法标注 | 转换脚本硬编码 "pdk_chat_demo_mock_v1" |

### 5.2 数据质量局限

- pdk_chat_demo mock 模式的 responses 是 deterministic,但**不代表真实 LLM 蒸馏价值**
- 真实蒸馏数据应来自 `enable_event_log(CaptureMode::Training)` 配置
- pdk_chat_demo JSON 仅供"概念验证"使用

---

## 六、结论

### 主要推荐

**用 SessionWriter JSONL (已 ship) 作为过渡数据源,不用 pdk_chat_demo Session JSON。**

### 决策点

| 决策 | 推荐 | 理由 |
|---|---|---|
| 过渡数据源选择 | **SessionWriter JSONL** | 数据完整度更高 + 与 ADR-0080 对齐 |
| 转换脚本位置 | `scripts/session_to_distill.py` | 与 SessionWriter 集成 |
| 转换后格式 | ADR-0061-13 DistillationRecord | 与未来蒸馏输出契约对齐 |
| 验证手段 | `tests/test_session_to_distill.py` (≥ 3 cases) | TDD 验证 |
| 与真实 D10 关系 | **过渡方案**,真实蒸馏应等 ADR-0080 v1.2 amendment ship | 标注清晰 |
| 文档位置 | `examples/pdk_chat_demo/README.md` 新章节 | 用户可发现 |

### 与现有文档的协调

- 同步 `capability-application-map-2026-08.md` §八 8.5 优先级排序:
  - 原 "pdk_chat_demo JSONL 临时数据源" → 本报告详细化
- 同步 ADR-0083 (IEvaluator) §决策 5: V1 TaskSuccessEvaluator 可立即消费 SessionWriter 转换输出
- 同步 ADR-0061-13 (蒸馏输出格式): 转换脚本输出对齐 ADR-0061-13 schema

---

## 七、下一阶段

1. **本 Sprint 收官前**:
   - [ ] 创建 `scripts/session_to_distill.py` 转换脚本
   - [ ] 创建 `tests/test_session_to_distill.py` ≥ 3 cases
   - [ ] 在 `examples/pdk_chat_demo/README.md` 追加 "Distillation Mode" 章节
   - [ ] 更新 `capability-application-map-2026-08.md` §八 标注本报告

2. **下个 Sprint** (待 ADR-0080 v1.2 ship 后):
   - 替换为真实 EventLog D10 Training 模式输出
   - 删除本过渡方案

---

**报告状态**: ✅ 调研完成
**关联文档**: `capability-application-map-2026-08.md` §八 B3 / ADR-0061-13 / ADR-0080 v1.2 / ADR-0083
**报告作者**: Architecture Working Group (调研员 + Oracle session `ses_fcba5e477ffeG9wEBHVhU64J0o` 评审输入)