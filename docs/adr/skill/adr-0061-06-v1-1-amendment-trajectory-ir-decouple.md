# ADR-0061-06 v1.1 amendment: Trajectory IR 独立序列化视图(不改 ParsedGraph)

**日期**: 2026-08-24
**状态**: ✅ **Approved + Shipped (2026-08-27, T15)** — OpenSpec change `t15-trajectory-ir` 实施完成: commits `3ba9f2c`/`53a0f17`/`1fd5c4b`/`7b24973`; 9 cases / 55 assertions PASS; ParsedGraph 零修改; ctest 零新增回归 (评审通过 2026-08-25, Oracle session `ses_fcba5e477ffeG9wEBHVhU64J0o`, capability-application-map §八 G14)
**父 ADR**: [adr-0061-06-trajectory-ir.md](adr-0061-06-trajectory-ir.md) v1
**父 ADR 兄弟**: adr-0061-02 (T14 行为回归已 ship), adr-0061-13 (蒸馏输出格式)

**前置文档**:
- `docs/architecture/capability-application-map-2026-08.md` §八 Oracle 评审
- Oracle session `ses_fcba5e477ffeG9wEBHVhU64J0o`
- ADR-0061-06 (v1) — 当前标题"升级 ParsedGraph"被 Oracle 标记为耦合风险

---

## 背景

**ADR-0061-06 v1 标题与决策 1 原文**：

> # ADR-0061-06: AgentIR-style Trajectory IR **升级 ParsedGraph**
>
> ### 决策 1 — 三级 IR
> HydraForge 现有 `ParsedGraph` 结构需要**升级**为多级 IR，以支持跨框架 trace 兼容 + pass pipeline + 多 backend 输出。

**Oracle 评审关键发现**（2026-08-24, session `ses_fcba5e477ffeG9wEBHVhU64J0o`）：

> **风险**：Trajectory IR 按 0061-06 标题"升级 ParsedGraph"会把**训练数据格式**耦合进**运行时图结构**——经典耦合风险。
>
> **结论**：应改为**独立序列化视图**（separate serialization view），**别动 ParsedGraph**。

**当前耦合风险分析**：

```
若按 v1 实施 "升级 ParsedGraph":
  ParsedGraph 修改 → 影响 L0 运行时所有示例 + 7 个 pdk plugin
  训练数据格式 → 必须在 ParsedGraph 演化时同步演化
  反向耦合：训练工具消费运行时图结构（不安全）
  
若按 v1.1 修正为 "独立序列化视图":
  TrajectoryIR 是独立类,与 ParsedGraph 通过 Converter 桥接
  训练数据 → 仅与 TrajectoryIR schema 耦合
  ParsedGraph 演化不影响 TrajectoryIR
```

---

## 决策

### 决策 1 — 标题与定位修订

**v1 标题**: "AgentIR-style Trajectory IR 升级 ParsedGraph"
**v1.1 标题**: "AgentIR-style Trajectory IR — 独立序列化视图"

**核心差异**:

| 维度 | v1 (ParsedGraph 升级) | **v1.1 (独立序列化视图)** |
|---|---|---|
| **核心类** | TrajectoryIR = ParsedGraph 升级版 | TrajectoryIR = 独立类 |
| **关系** | "是" (TrajectoryIR 继承/扩展 ParsedGraph) | "桥接" (ParsedGraph → Converter → TrajectoryIR) |
| **修改影响** | 改 TrajectoryIR 必须改 ParsedGraph | 改 TrajectoryIR 不影响 ParsedGraph |
| **演化速度** | 跟随 ParsedGraph（L0 运行时） | 独立演化（训练方向） |
| **风险** | 训练数据依赖运行时 | 训练数据独立 |

### 决策 2 — 三级 IR（修订措辞）

```cpp
// src/core/parsed_graph.h — 不修改,保持 v3.10 不变
struct ParsedGraph {
    // ... 既有 L0 运行时定义 ...
};

// include/agenticdsl/ir/trajectory_ir.h — 新增独立类 (V1.1 关键)
namespace agenticdsl::ir {

class TrajectoryIR {
 public:
    // V1.1: 独立 schema,与 ParsedGraph 完全分离
    enum class IRLevel { RawIR, ParsedIR, CanonicalIR };
    
    struct RawIR {       // 文本（接近 DSL）
        std::string dsl_text;
        nlohmann::json metadata;
    };
    
    struct ParsedIR {    // 结构化 JSON
        std::vector<NodeRecord> nodes;
        std::vector<EdgeRecord> edges;
        std::vector<StepRecord> steps;  // ReAct 序列
    };
    
    struct CanonicalIR { // 规范化（pass pipeline 输出）
        std::vector<NodeRecord> canonical_nodes;
        // ... pass 优化结果 ...
    };
    
    // Converter: ParsedGraph → TrajectoryIR::ParsedIR (单向)
    static ParsedIR from_parsed_graph(const ParsedGraph& pg);
    
    // Backends: CanonicalIR → SFT/RL/eval/observability
    static nlohmann::json to_sft_data(const CanonicalIR& canonical);
    static nlohmann::json to_rl_data(const CanonicalIR& canonical);
    static nlohmann::json to_eval_data(const CanonicalIR& canonical);
    static nlohmann::json to_otel_spans(const CanonicalIR& canonical);
};

}  // namespace agenticdsl::ir
```

**关键不变量**: ParsedGraph 与 TrajectoryIR **无继承关系**,通过 `from_parsed_graph()` 单向 Converter 桥接。

### 决策 3 — Converter 策略 (V1 简化)

```cpp
// src/core/parsed_graph_to_trajectory_ir.cpp (V1 实现)
TrajectoryIR::ParsedIR TrajectoryIR::from_parsed_graph(const ParsedGraph& pg) {
    ParsedIR result;
    
    // 1. Nodes → NodeRecord (浅拷贝,值类型)
    for (const auto& node : pg.nodes) {
        NodeRecord rec;
        rec.id = node.id;
        rec.type = node.type;  // 字符串,无 enum 依赖
        rec.metadata = node.metadata;
        result.nodes.push_back(std::move(rec));
    }
    
    // 2. Edges → EdgeRecord
    for (const auto& edge : pg.edges) {
        EdgeRecord rec;
        rec.from = edge.from;
        rec.to = edge.to;
        rec.weight = 1.0;  // V1 简化: 无权重
        result.edges.push_back(std::move(rec));
    }
    
    // 3. Steps (V1: 占位,从 ExecutionSession TraceRecord 推)
    // V2: 与 ADR-0061-13 DistillationRecord.reward 集成
    
    return result;
}
```

**V1 简化策略**:
- 仅实现 `from_parsed_graph()` 单向 Converter
- V1 backends: 仅 `to_sft_data()` + `to_otel_spans()` (其余 V2)
- Pass Pipeline 仅 `ConstantFoldingPass` 占位 (V2 扩展)

### 决策 4 — Frontends 修订

**v1 列 5 个 framework**: HydraForge DSL / LangGraph / CrewAI / AutoGen / OpenAI SDK
**v1.1 V1 仅 2 个**: HydraForge DSL (via Converter) + 用户 YAML DSL

**理由**: 多 framework 兼容推迟 V2,V1 聚焦 HydraForge 内生路径 + 用户 YAML。

### 决策 5 — Backends 修订 (V1 仅 SFT + OTel)

| Backend | v1.1 V1 实施 | V2 推迟 |
|---|---|---|
| **SFT 数据** | ✅ (与 ADR-0061-13 集成) | — |
| **可观测性** | ✅ (OTel spans 输出) | — |
| RL 训练 | V2 | T20 AFlow + ADR-0078 |
| 评测数据 | V2 | 与 IEvaluator (ADR-0083) 集成 |

**V1 SFT 输出与 ADR-0061-13 桥接**:
```cpp
// TrajectoryIR::to_sft_data() 输出 → ADR-0061-13 IDistillationWriter::write_record()
nlohmann::json sft = TrajectoryIR::to_sft_data(canonical_ir);
DistillationRecord record;
record.input = sft["prompt"];
record.output = sft["completion"];
record.steps = convert_steps(sft["steps"]);  // 与 ADR-0061-13 StepRecord 对齐
// reward 由 IEvaluator (ADR-0083) 填充
```

---

## 不变量

- 不变量 1: `ParsedGraph` 与 `TrajectoryIR` 无继承关系（**关键**）
- 不变量 2: `TrajectoryIR::from_parsed_graph()` 是**单向** Converter（不回写 ParsedGraph）
- 不变量 3: `TrajectoryIR` schema 版本化（`schema_version` 字段）,与 ParsedGraph 独立演化
- 不变量 4: V1 backends 仅 SFT + OTel（其他推迟 V2）

---

## 与现有架构的集成

| 集成点 | 集成方式 |
|---|---|
| `ParsedGraph` (L0 运行时) | **只读** 单向 Converter 源 |
| `ExecutionSession TraceRecord` | V2: TrajectoryIR Steps 从 TraceRecord 聚合 |
| `EventLog` (ADR-0080 D10.v1.2 Training) | V2: TrajectoryIR steps 可源自 EventLog causal_time |
| `IDistillationWriter` (ADR-0061-13) | V1: 消费 `TrajectoryIR::to_sft_data()` 输出 |
| `IEvaluator` (ADR-0083) | V2: DistillationRecord.reward 填充 |
| `Behavioral Regression` (T14) | V2: Student vs Teacher TrajectoryIR 等价性 |

---

## 实施

- **文件**:
  - `include/agenticdsl/ir/trajectory_ir.h` (独立类, V1 ~200 行)
  - `include/agenticdsl/ir/raw_ir.h` / `parsed_ir.h` / `canonical_ir.h` (V2 拆分)
  - `src/core/parsed_graph_to_trajectory_ir.cpp` (V1 Converter 单向 ~100 行)
  - `src/modules/ir/sft_backend.cpp` (V1 ~150 行)
  - `src/modules/ir/otel_backend.cpp` (V1 ~100 行)
  - `tests/test_trajectory_ir.cpp` (≥ 6 cases)
- **估时**: 2 sprint (T15, 与 ADR-0061-13 集成)
- **优先级**: P0 (Oracle 评审: 本周最高杠杆)

---

## 风险

| 风险 | 影响 | 缓解 |
|---|---|---|
| Converter 性能开销 | 大图转 IR 慢 | V1 简化: 仅浅拷贝 + 字符串映射, 不做深层复制 |
| Pass Pipeline V2 范围不清 | 后续 scope creep | V1 仅占位, V2 单独 ADR |
| 与 ParsedGraph schema 漂移 | Converter 失败 | schema_version 字段 + Converter 版本检测 |
| 多 framework frontends (LangGraph/CrewAI/AutoGen/OpenAI) V2 推迟 | 用户期望落差 | 文档明确 V1 仅 HydraForge + YAML |

---

## v1 → v1.1 修订章节对照

| 章节 | v1 措辞 | v1.1 措辞 |
|---|---|---|
| **标题** | "AgentIR-style Trajectory IR 升级 ParsedGraph" | "AgentIR-style Trajectory IR — 独立序列化视图" |
| **决策 1** | "HydraForge 现有 `ParsedGraph` 结构需要升级为多级 IR" | "`TrajectoryIR` 是独立类,通过 Converter 单向桥接 ParsedGraph" |
| **决策 3** Frontends | 5 个 framework | V1 仅 2 个 (HydraForge + YAML), 多 framework V2 |
| **决策 4** Backends | 4 个全部 | V1 仅 SFT + OTel, RL + Eval V2 |
| **实施** | "include/agenticdsl/ir/, src/modules/ir/" | 新增独立目录 + Converter 文件明确归属 |

---

## 关联变更

- `capability-application-map-2026-08.md` §八 G14 状态更新（v1 → v1.1 amendment）
- `docs/adr/adr-0061-06-trajectory-ir.md` 头部状态从 ✅ Approved v1 → ✅ Approved v1.1
- 新增 OpenSpec change: `2026-08-24-adr-0061-06-v1-1-amendment-trajectory-ir-decouple`
- 解锁: T15 (Trajectory IR 实施) + B6 蒸馏方向 (与 ADR-0061-13 集成)

---

## 参考

- Oracle 评审: session `ses_fcba5e477ffeG9wEBHVhU64J0o`
- ADR-0061-06 v1 (line 1: "升级 ParsedGraph" — Oracle 标记耦合风险)
- MLIR 设计哲学: https://mlir.llvm.org/ (多级 IR, 独立 schema, 不修改源语言 AST)
- AgentIR 论文: github.com/WhitzardAgent/agentir (设计灵感)
- LLVM: 多级 IR 演化的工业范例