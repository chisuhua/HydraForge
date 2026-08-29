# ADR-0061-13: 蒸馏输出格式 (Distillation Output Format & Behavior Cloner Contract)

**日期**: 2026-08-24
**状态**: ✅ **Approved (评审通过 2026-08-25)** ✅ **+ Phase 0 code ship 2026-08-29** (Oracle 评审识别为架构层缺口, capability-application-map §八 G15)
✅ **Phase 0 code ship (commit `11d3515`)** — `include/agenticdsl/contract/idistillation_writer.h` (3 虚函数 + 1 工厂, 完全对齐 §决策 3) + `include/agenticdsl/types/distillation_record.h` (字段全集对齐 §决策 2) + `include/agenticdsl/types/capture_mode.h` 已 ship. Phase 1 (FileDistillationWriter V1 + EventLogConfig BREAKING 迁移 + 闭环 1 第 1 环) 待启动.
⏳ **tracking: in-progress** — Phase 0 ✅ ship, Phase 1-3 待启动: see `openspec/changes/capture-mode-and-distillation-writer-v1/` (2026-08-29 创建, P0 路线图级)
**父 ADR**: [../adr-0061-agent-evolution-and-solidification.md](../adr-0061-agent-evolution-and-solidification.md)
**父 ADR 兄弟**: adr-0061-02 (T14 行为回归已 ship), adr-0061-06 (Trajectory IR), adr-0061-09 (GEPA)

**前置文档**:
- `docs/architecture/capability-application-map-2026-08.md` §八 Oracle 评审
- Oracle session `ses_fcba5e477ffeG9wEBHVhU64J0o`
- ADR-0061-02 (行为回归, 已 ship, T14)
- ADR-0061-06 (Trajectory IR, Approved 但零代码)

---

## 背景

**Oracle 评审关键发现**（2026-08-24, session `ses_fcba5e477ffeG9wEBHVhU64J0o`）：

> ADR-0061 家族 12 子项中**没有一项覆盖"蒸馏输出层"**——即如何把教师 Agent 的能力**编码**为学生模型的训练数据。当前项目讨论 SKILL.md → .agent.md → C++ → Wasm 演化路径,但**未定义反方向**：教师 → 学生蒸馏输出格式。
>
> 这是"行为克隆器"契约——GEPA/AFlow/BehaviorCloning 三者都需要它,作为输入格式。

**蒸馏最小闭环分析**（Oracle §8.2）：

```
采集 (ADR-0080 D10.v1.2) → 数据集结构 (本 ADR) → 评估信号 (ADR-0083) 
→ 训练管线 (ADR-0078) → 模型服务 (model_router ✅) → 回归门 (T14 ✅) 
→ 蒸馏输出格式 (本 ADR)
```

当前闭环 7 环中已 ship/已 Approved: 5 环（T14 ✅, model_router ✅, ADR-0080 ✅, ADR-0083 🔍 本周, ADR-0061-06 🔍）

**缺口**: 蒸馏输出格式（行为克隆器契约）= 本 ADR 起草。

---

## 决策

### 决策 1 — 三层输出格式

蒸馏输出分为三个文件类型，命名约定 `<agent_id>_<seq>.distill.v1.jsonl`:

| 文件类型 | 内容 | 行格式 |
|---|---|---|
| **trajectory.jsonl** | 教师 Agent 完整轨迹（输入/动作/观察）| `{trace_id, input, action, observation, reward, metadata}` |
| **policy.jsonl** | 学生模型训练用的"输入-动作"对 | `{prompt, completion, weight, metadata}` |
| **meta.json** | 元数据（数据集统计、hash、版本）| `{version, total_examples, dataset_hash, generation_config}` |

### 决策 2 — `DistillationRecord` 核心类型

```cpp
struct DistillationRecord {
  // 输入: 教师 Agent 接收的 user_input + context snapshot
  std::string input;            // 必须 ≤ 64KB (与 ADR-0080 D10.4 一致)
  
  // 输出: 教师 Agent 的 response_text (与 ADR-0080 D10.2 response_text 对齐)
  std::string output;           // 必须 ≤ 1MB
  
  // 动作序列 (针对 ReAct): thought → action → observation
  std::vector<StepRecord> steps;  // V1: ≤ 20 步
  
  // 评估信号 (与 ADR-0083 RewardSignal 对齐)
  RewardSignal reward;
  
  // 元数据
  std::string trace_id;         // EventLog causal_time 引用
  std::string source_event;     // llm.request / llm.response event_id
  std::string agent_id;
  std::string teacher_version;  // 教师 Agent 版本 (e.g. "v1.0.0")
  std::uint64_t generation_timestamp_ms;
};

// V1 简化: Step 仅记录工具调用 + 观察
struct StepRecord {
  std::string thought;          // ReAct thought (可选)
  std::string tool_name;        // 若 action 是 tool call
  nlohmann::json tool_args;
  std::string observation;
  std::uint64_t latency_ms;
};
```

### 决策 3 — `IDistillationWriter` 契约

```cpp
class IDistillationWriter {
 public:
  virtual ~IDistillationWriter() = default;
  
  // 写入单条 record（V1: 同步落盘 + fsync）
  virtual void write_record(const DistillationRecord& record) = 0;
  
  // flush + 关闭 (析构时自动调用)
  virtual void close() = 0;
  
  // 元数据 (生成结束后写入 meta.json)
  virtual void finalize(const DistillationMetadata& meta) = 0;
  
  // 工厂函数
  static std::unique_ptr<IDistillationWriter> make_file_writer(
      const std::filesystem::path& output_dir,
      const std::string& agent_id);
};

// V1 实现: FileDistillationWriter (同步写,rotation 由 event_log 复用)
```

### 决策 4 — 与现有架构的集成

| 集成点 | 集成方式 |
|---|---|
| **EventLog (ADR-0080 D10.v1.2 Training 模式)** | D10 输出的 prompt_text/response_text 作为 DistillationRecord.input/output 直接来源 |
| **Trajectory IR (ADR-0061-06)** | IR 提供序列化的轨迹 → DistillationWriter 接收 IR 输出 |
| **IEvaluator (ADR-0083)** | DistillationRecord.reward 由 IEvaluator::evaluate() 填充 |
| **Hotelling T² (T14)** | 学生模型产出后,用 T² 验证与教师等价 (回归门) |
| **Behavioral Cloning 训练 (V2)** | 读取 policy.jsonl + trajectory.jsonl → LoRA 训练 (与 ADR-0078 §D4 对齐) |

### 决策 5 — 隐私与去标识化 (与 ADR-0080 v1.2 对齐)

- **必须** 在 `CaptureMode::Training` 模式下使用 (与 ADR-0080 v1.2 amendment G12 联动)
- 文件头必须包含 `"capture_mode": "Training"` (沿用 D10.v1.2.4 约定)
- `input` 字段**禁止**包含: API key / password / 邮箱 / IP (**调用方**负责 PII scrub)

**Scrub 责任划分** (避免与 ADR-0080 v1.2 amendment 决策 D10.v1.2.2 "Training 模式跳过 scrub 强制" 措辞混淆):

| 维度 | ADR-0080 v1.2 amendment | ADR-0061-13 (本 ADR) |
|---|---|---|
| **scrub hook 是否强制 ship** | ❌ Training 模式不强制 ADR-0081 scrub hook（fail-open 宽松启动）| N/A（不直接依赖） |
| **PII scrub 责任** | N/A | ✅ **调用方负责**（写入前自行处理）|
| **V2 自动化 scrub 集成** | ✅ ADR-0081 ✅ Approved 2026-08-21 | 调用方接入 ADR-0081 即可自动化 |

- `meta.json.dataset_hash` 用于审计追踪,但不暴露原始数据

---

## 不变量

- 不变量 1: `DistillationRecord.input + output` 总大小 ≤ 1.5 MB (防止内存爆炸)
- 不变量 2: `trace_id` 必须与 EventLog causal_time 对齐（可追溯审计）
- 不变量 3: `teacher_version` 必填（确保可重现性）
- 不变量 4: 文件命名 `<agent_id>_<seq>.distill.v1.jsonl` 必须唯一

---

## 与 ADR-0061-06 (Trajectory IR) 的边界

| 维度 | Trajectory IR (0061-06) | Distillation Output (本 ADR) |
|---|---|---|
| **目标** | 序列化 Agent 执行轨迹 | 编码为学生模型训练数据 |
| **粒度** | 单次执行完整轨迹 | 单次执行的"输入-输出"对 |
| **消费方** | 调试/审计/分析 | 训练管线 (LoRA / DPO / SFT) |
| **格式** | 通用 IR (AgentIR-style) | ML 训练特定 (policy.jsonl) |
| **评估字段** | 无 | **必有** (reward) |

**结论**: 两者正交。Trajectory IR 可作为 DistillationRecord 的"上游序列化视图",DistillationRecord 加 ML 训练所需字段（reward, completion）。

---

## 实施

- **文件**:
  - `include/agenticdsl/contract/idistillation_writer.h` (L1 契约层)
  - `include/agenticdsl/types/distillation_record.h` (值类型)
  - `src/modules/distillation/file_writer.cpp` (V1 FileDistillationWriter)
  - `tests/test_distillation_writer.cpp` (≥ 5 cases)
- **估时**: 1 sprint (含与 Trajectory IR 集成测试)
- **优先级**: P0 (Oracle: 本周最高杠杆)

---

## 风险

| 风险 | 影响 | 缓解 |
|---|---|---|
| 输入字段过大 (PII / 凭据) | 隐私泄漏 | 强制 Training 模式 + PII 自动 scrub (待 ADR-0081) |
| trajectory.jsonl 与 policy.jsonl 不一致 | 训练数据错误 | 元数据 meta.json 校验两者行数 + hash |
| 学生模型训练失败无诊断 | 浪费数据 | 集成 IEvaluator (ADR-0083) 与回归门 (T14) 双层验证 |

---

## 关联变更

- `docs/architecture/capability-application-map-2026-08.md` §八 G15 状态更新
- 新增 OpenSpec change: `2026-08-24-adr-0061-13-distillation-output-format`
- 解锁: T22 Fine-tune (ADR-0078) — fine-tune 训练管线的"数据格式"侧已就绪
- 关联: B6 应用 (Agent 蒸馏环境) — 完整 ship 后可构建

---

## 参考

- Oracle 评审: session `ses_fcba5e477ffeG9wEBHVhU64J0o`
- Hugging Face Datasets 格式 (arrow / jsonl)
- OpenAI fine-tuning 数据格式 (messages array)
- Anthropic Claude fine-tuning 数据格式 (multi-turn)
- ADR-0061-02 (行为回归, 已 ship, T14) - 回归门
- ADR-0083 (IEvaluator 契约) - reward signal 来源
- ADR-0080 v1.2 amendment - Training 模式 capture