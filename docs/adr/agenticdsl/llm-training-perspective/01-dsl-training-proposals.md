# AgenticDSL 语言演进提案（LLM 训练视角）

> **文档 ID**: LLMTRN-001-DSL
> **生成日期**: 2026-06-10
> **状态**: 草案 v1.0（面向 LLM 训练的语言改进提案）
> **配套文档**（训练侧，独立维护）:
> - 训练侧综述、数据管线、训练算法、推理时保障、评估、风险、与 VN-001 对齐：见 minimind 仓 `/docs/agenticdsl-training/`
>
> **关联**:
> - 规范基线: [`docs/specs/dsl.md`](../../../specs/dsl.md) v3.10
> - 自举愿景: [`docs/adr/agenticdsl/vision/01-self-bootstrapping-vision.md`](../vision/01-self-bootstrapping-vision.md)
> - 实施路径: [`docs/adr/agenticdsl/implementation/self-bootstrapping-path.md`](../implementation/self-bootstrapping-path.md)
>
> **调研依据**: 4 个并行 Librarian 代理对 2022-2026 SOTA 文献与开源项目的深度调研。

---

## 0. 文档范围

本文档 **只讨论 AgenticDSL 语言层面的演进提案**——即为了让 LLM 可靠生成、修复、续写、验证该语言所需的语法、词法、tokenization 改动。

**不在本文档范围**：
- 训练数据管线、训练算法、奖励设计 → 见 minimind 仓 `agenticdsl-training/` 子目录
- 运行时行为调整（C++ 引擎改动）→ 见 `docs/adr/` 与 `docs/specs/`
- 人类可读性、API 设计 → 见 `docs/adr/agenticdsl/vision/` 与 `docs/adr/agenticdsl/skill-system/`

---

## 1. v3.10 LLM-训练友好度评估（基线）

### 1.1 已具备的优势（保留）

通过对 `examples/agent_basic/workflow.agent.md` 与 `docs/specs/dsl.md` 的逐字分析，v3.10 已具备以下 LLM 训练友好的结构特征：

**1. 显式锚点（Anchors）**
- `### AgenticDSL '/path'` 头 —— 唯一标识子图路径
- `--- BEGIN AgenticDSL ---` / `--- END AgenticDSL ---` —— 显式起止围栏
- 这两者可作为 FIM（Fill-in-Middle）切分的天然锚点，**无需重新设计语法**

**2. 拓扑声明的字段**
- `type` —— 节点类型枚举（10+ 种）
- `next: string | list<string>` —— 显式后继，支持 `@v1` 版本路径
- `wait_for: list | any_of | all_of` —— 并发依赖显式编码
- `branches: list<string>` —— fork 目标列表
- `merge_strategy` —— fork 合并策略
- `on_failure` —— 错误跳转路径

**3. 强类型契约**
- `signature: { inputs, outputs, schema, version, stability }` —— 子图输入/输出契约
- `/lib/**` 强制签名；`/dynamic/**` 可选签名（生成后校验）

**4. 运行时验证能力**
- 引擎有 namespace 违规检测（`ERR_NAMESPACE_VIOLATION`）
- 签名验证三档：`strict` / `warn` / `ignore`
- `/__meta__/resources` 启动时验证

### 1.2 仍需补强的弱点（训练视角）

| 弱点 | 严重度 | 对 LLM 生成的影响 |
|---|---|---|
| **YAML 嵌套缩进** | 高 | BPE 对空格敏感，模型在长 block 中易生成非法缩进 |
| **Inja 模板与 YAML 字符串混用** | 中 | `{{ $.var }}` 边界识别困难 |
| **多子图边界靠空行分隔** | 中 | 模型易在续写时跨越子图边界 |
| **Markdown + YAML 混合** | 中 | 模型可能生成与 DSL 无关的 Markdown 内容 |
| **特殊 token 未注册** | 高 | `### AgenticDSL` 等锚点被 BPE 切分 |

---

## 2. 三阶段语言演进路线

### 2.1 演进原则

- **向后兼容优先**：v3.11 必须能被 v3.10 引擎无修改地解析
- **训练侧优先**：Phase 1 不改规范，仅在训练数据侧做工程处理
- **可选引入**：Phase 2 的新锚点作为可选元素，不强制使用
- **学术严谨最后**：Phase 3 的 indentation-sensitive grammar 仅在数据量积累后做

### 2.2 Phase 1 — 训练侧工程（不改规范）

**目标**：通过 tokenizer、grammar、数据增强，让现有 v3.10 语法可以被 LLM 可靠生成。

**周期**：与 TR-1（基础生成能力训练）并行，4-6 周。

**具体措施**：

**2.2.1 特殊 Token 注册（必做）**

仿 Qwen2.5-Coder FIM token 设计，提议在 HydraForge tokenizer 层添加以下 token：

| Token ID 范围 | Token | 用途 |
|---|---|---|
| `<\|agenticdsl_open\|>` | DSL 块开始 | 单 token 锚点 |
| `<\|agenticdsl_close\|>` | DSL 块结束 | 单 token 锚点 |
| `<\|subgraph_decl\|>` | 子图声明头 | FIM 切分锚点 |
| `<\|node_def\|>` | 节点定义开始 | 流式生成时识别 |
| `<\|inja_expr_open\|>` | `{{` 模板起始 | 避免与 YAML 字符串混用 |
| `<\|inja_expr_close\|>` | `}}` 模板结束 | 同上 |
| `<\|fim_prefix\|>` | FIM prefix | FIM 训练（Qwen2.5-Coder 同款） |
| `<\|fim_middle\|>` | FIM middle | FIM 训练 |
| `<\|fim_suffix\|>` | FIM suffix | FIM 训练 |
| `<\|fim_pad\|>` | FIM padding | FIM 训练 |
| `<\|agenticdsl_eos\|>` | DSL 生成结束 | stop token |

**实施约束**：
- **不要直接 append 到 vocab 末尾**（PickyBPE EMNLP2024 / Teaching Old Tokenizers New Words EACL2026 教训）
- 用 5-10 GB AgenticDSL 语料继续训练现有 BPE，**同步**添加 structural token
- 验证 tokenization efficiency 不退化
- 把 `<|fim_*|>` 和 `<|agenticdsl_*|>` 加入 `stop_token_ids`（Qwen2.5-Coder issue #99 教训：即使训练充分仍会泄漏）

**2.2.2 Canonical Serializer（必做）**

为训练数据设计确定性序列化器：

```yaml
# canonical 规则
indent: 2 spaces (no tabs)
key_order: alphabetical (with mandatory fields first)
string_quotes: double quotes only
boolean: true/false (not yes/no)
null: null (not ~)
list_indent: -2 spaces relative to parent key
multi-line_strings: | literal block
```

**位置**：`src/common/utils/canonical_serializer.{h,cpp}`，与 `markdown_parser.cpp` 配合使用。

**2.2.3 FIM 数据格式定义**

参考 Qwen2.5-Coder / StarCoder2 FIM 设计：

```yaml
# FIM Prefix（输入）
<|fim_prefix|>
### AgenticDSL '/main/start'
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
<|fim_suffix|>
  - id: end
    type: end
# --- END AgenticDSL ---
```
<|fim_middle|>
    next: [/main/query]
<|fim_pad|>
```

**FIM 应用场景**：
- DSL 修复：给定前后文，让模型填充缺失节点
- 子图续写：给定已有节点，让模型生成后续
- 字段补全：给定节点框架，让模型补全字段

### 2.3 Phase 2 — v3.11 规范增量（向后兼容）

**目标**：引入可选显式锚点与字段，缓解 YAML 缩进歧义。

**周期**：与 TR-2（多轮与修复训练）同步，4-6 周。

**2.3.1 可选 `<subgraph>` 锚点**

```yaml
### AgenticDSL '/main/start'
<|subgraph_decl|>
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
<|node_def|> - id: start
    type: start
    next: [/main/query]
<|node_def|> - id: end
    type: end
# --- END AgenticDSL ---
```
```

**作用**：
- `<|subgraph_decl|>` 显式声明子图开始（替代隐式 YAML key）
- `<|node_def|>` 显式声明每个节点开始
- 训练时按这些锚点做流式生成，推理时 grammar 强制要求锚点存在

**兼容性**：v3.10 引擎忽略 `<|subgraph_decl|>` 和 `<|node_def|>`（视为注释），不影响现有工作流。

**2.3.2 可选 `flow` 简写**

为简单线性场景提供单行表达：

```yaml
### AgenticDSL '/main/pipeline'
<|flow_decl|> start -> query -> analyze -> end
```

**适用条件**：仅当子图是纯线性（无 fork、无 join、无 assert）时使用。

**2.3.3 显式类型标注（可选）**

```yaml
- id: query_count
  type: assign
  assign:
    count: "{{ result | int }}"    # 显式 int 类型
    ratio: "{{ result | float }}"  # 显式 float 类型
```

**作用**：帮助模型理解字段类型预期，减少类型不匹配错误。

### 2.4 Phase 3 — v4.0 长期演进（破坏性，需 RFC）

**目标**：引入 indentation-sensitive grammar，从根本上解决 YAML 缩进歧义。

**周期**：待 v3.11 训练数据积累（> 100K）后再启动，6+ 个月。

**2.4.1 Indentation-Sensitive PDA**

参考 Adams2013 "Principled Parsing for Indentation-Sensitive Languages" 框架：

```ebnf
# 把 indentation 形式化为 PDA state 中的 indentation set
# N ∪ {j ∈ N | j > j₀}
# 用 LR(k) 或 GLR parsing algorithm，O(nm) time complexity
```

**实施路径**：
1. fork XGrammar 添加 indentation-aware automaton（基于 byte-level PDA）
2. 用 yaml-spec-1.2 的 211 规则 BNF 改造为 AgenticDSL 子集（仅 block mapping + sequence）
3. 编写测试用例覆盖所有 v3.10 现有工作流

**2.4.2 替代方案评估：JSON Schema 严格化**

如果 indentation-sensitive PDA 工程过大，可考虑：

```yaml
### AgenticDSL '/main/start' (JSON-strict mode)
```json
{
  "graph_type": "subgraph",
  "nodes": [
    {
      "id": "start",
      "type": "start",
      "next": ["/main/query"]
    }
  ]
}
```
```

**权衡**：
- ✅ JSON 模式无缩进歧义，XGrammar 原生支持
- ❌ 失去 Markdown 注释的灵活性
- ❌ 与现有工作流不兼容

**建议**：v4.0 不做 JSON 模式强制；保留 YAML 作为默认表达，indentation-sensitive grammar 作为兜底。

### 2.5 与 v3.10 演进路线的对齐

本文档的演进路线应作为 v3.11+ 规范修订的输入。具体 RFC 流程：

1. 在 `docs/adr/agenticdsl/language-extensions/` 创建 RFC 文档
2. 关联 Oracle 评估
3. 进入 ADR 流程（编号 adr-0037+）
4. 在 `docs/specs/dsl.md` 更新规范

---

## 3. Grammar 编写规范

### 3.1 EBNF 表达（XGrammar 兼容）

AgenticDSL v3.10 的语法可表达为以下 EBNF 框架：

```ebnf
# AgenticDSL v3.10 Grammar Sketch

agenticdsl_doc := markdown_header (agenticdsl_block | markdown_block)+

markdown_header := <any text outside fenced blocks>

agenticdsl_block := "###" "AgenticDSL" path_marker
                    "```yaml"
                    "---" "BEGIN" "AgenticDSL" "---"
                    yaml_body
                    "---" "END" "AgenticDSL" "---"
                    "```"

path_marker := "'" path "'"
path := "/" segment ("/" segment)*
segment := [a-zA-Z0-9_]+ ("@" version)?
version := "v" [0-9]+ ("." [0-9]+)*

yaml_body := meta_block | subgraph_body

meta_block := "version:" string
              "mode:" ("dev" | "prod")
              "entry_point:" path
              "execution_budget:" budget_struct

budget_struct := "max_nodes:" int
                 "max_subgraph_depth:" int
                 "max_duration_sec:" int

subgraph_body := ("graph_type:" "subgraph")?
                 ("signature:" signature_struct)?
                 nodes_list

nodes_list := node_def+

node_def := "- id:" identifier
            "  type:" node_type
            (node_field)*

node_type := "start" | "end" | "assign" | "tool_call" | "dsl_call"
             | "assert" | "fork" | "join" | "generate_subgraph"
             | "llm_call"
             | "codelet_call"

node_field := "next:" path_list
             | "wait_for:" path_list
             | "branches:" path_list
             | "merge_strategy:" merge_strategy
             | "on_failure:" path
             | "on_success:" action
             | "permissions:" permissions_list
             | "output_keys:" string_list
             | "input_keys:" string_list
             | "condition:" inja_expr
             | "assign:" yaml_mapping
             | "tool:" identifier
             | "arguments:" yaml_mapping
             | "prompt_template:" inja_string
             | "llm_tool_name:" identifier
             | "llm_params:" yaml_mapping
             | "signature_validation:" ("strict" | "warn" | "ignore")
             | "namespace_prefix:" path_marker_prefix

inja_string := <literal string potentially containing {{ ... }} expressions>
inja_expr := <literal expression starting with {{ and ending with }}>

merge_strategy := "error_on_conflict" | "last_write_wins"
                  | "first_write_wins" | "deep_merge"

identifier := [a-zA-Z_][a-zA-Z0-9_]*
```

**位置**：`src/modules/parser/grammar/agenticdsl_v3_10.ebnf`（新增），由 `xgrammar_agenticdsl.py` 加载。

### 3.2 YAML 缩进处理策略

**策略 A（推荐先做）—— 隔离 YAML block**：
- EBNF grammar 不试图表达 YAML 缩进
- YAML 块视为 `[^#\n]+` 一行行字符流
- 在 grammar 中锁定 `### AgenticDSL` 和 `--- BEGIN/END ---` 边界
- YAML 解析交给运行时（`yaml-cpp`）

**策略 B（长期）—— indentation-sensitive PDA**：
- 参考 Adams2013 框架
- 用 byte-level PDA 实现缩进敏感的 grammar
- 100% 有效 YAML，但工程量大

**推荐**：先实施策略 A，Phase 3 再迁移到策略 B。

### 3.3 Dynamic Schema 表达

AgenticDSL 的签名是动态注册的（PDK、Skill 系统可在运行时添加）。Grammar 需要支持动态 schema：

```python
# xgrammar_agenticdsl.py
from xgrammar import Grammar

with open("agenticdsl_v3_10.ebnf") as f:
    ebnf_text = f.read()

agenticdsl_grammar = Grammar.from_ebnf(
    ebnf_text,
    backend="compiled",
    dynamic_schema_resolvers={
        "available_subgraphs": lambda ctx: get_registered_signatures(),
        "tool_definitions": lambda ctx: get_registered_tools(),
    }
)
```

**位置**：`src/common/llm/grammar_resolver.cpp`（新增），从 `StandardLibraryLoader` 与 `ToolRegistry` 拉取当前 schema。

---

## 4. Tokenizer 与 Vocab Surgery 规范

### 4.1 设计原则

- **Structural tokens 必须 atomic**：单 token 表示语义边界
- **不加冗余 token**：能用 BPE 表达的（如路径段）不强行单 token 化
- **保留向后兼容**：新 token 不能破坏现有 tokenization 效率
- **Stop token 完整**：所有 structural token 加入 stop_token_ids

### 4.2 实施流程

1. **收集语料**：从 `lib/`、`examples/`、`docs/specs/dsl.md` 抽取 5-10 GB AgenticDSL 文本
2. **训练 BPE 继续**：在 Qwen2.5-Coder-7B 的 tokenizer 上继续训练 5-10 GB 语料
3. **添加 special tokens**：在 BPE 继续训练过程中同步插入 structural tokens
4. **验证效率**：对比新旧 tokenizer 的 tokenization efficiency（不能退化 > 5%）
5. **回填 embedding**：为新 token 初始化 embedding（用同义 token 的平均）

### 4.3 验证清单

- [ ] Tokenizer round-trip test（encode → decode 完全恢复）
- [ ] Tokenization efficiency 对比（不能退化）
- [ ] Special tokens 在 stop_token_ids 中
- [ ] BPE 不切分 `### AgenticDSL`、`<agent>`、`<subgraph>` 等
- [ ] FIM tokens 不在生成中出现

---

## 5. FIM 训练数据格式规范

### 5.1 FIM 数据形态

```json
{
  "task": "node_completion",
  "prefix": "<DSL context before the missing node>",
  "middle": "<missing node>",
  "suffix": "<DSL context after the missing node>",
  "fim_tokens": {
    "prefix": "<|fim_prefix|>",
    "middle": "<|fim_middle|>",
    "suffix": "<|fim_suffix|>",
    "pad": "<|fim_pad|>"
  }
}
```

### 5.2 FIM 应用场景

| 场景 | Prefix | Middle | Suffix |
|---|---|---|---|
| **节点补全** | 子图头部 + 已有节点 | 缺失的下一个节点 | 后续节点 + 子图尾部 |
| **字段补全** | 节点框架（id, type） | 缺失字段（next, arguments） | 后续字段 |
| **DSL 修复** | 错误节点上下文 | 修复后的正确节点 | 后续节点 |
| **子图续写** | 已有子图 | 续写的后续节点 | 子图尾部 |

### 5.3 FIM 训练策略

- **应用比例**：训练数据中 50% 应用 FIM 变换
- **FIM 比例**：prefix:middle:suffix = 1:1:1（Qwen2.5-Coder 默认）
- **随机位置**：FIM 切分点在 DSL 内随机选择（不能跨越子图边界）
- **多 FIM 模式**：可选应用 prefix-only mode（让模型只生成 prefix 续写）

---

## 6. 验证层规范（语法层面）

### 6.1 三层语法验证

```
DSL 输出
   │
   ▼
[L1: Markdown 解析] markdown_parser
   │ < 10ms, 无外部依赖
   │ - 检查 ### AgenticDSL 头
   │ - 解析 YAML block
   │ - 提取 nodes / signature / permissions
   │
   ▼ pass
[L2: 静态校验] signature_validator
   │ - namespace 规则（/lib/** 不可写）
   │ - 节点引用合法性（id 唯一, next 引用存在）
   │ - 必填字段完整性（type 必填, end 节点无 next）
   │ - permission 交集
   │
   ▼ pass
[L3: 沙箱执行] dry_run executor
   │ - max_nodes / max_llm_calls 预算检查
   │ - 工具签名匹配（实际调用 mock tool）
   │ - 状态合并策略验证
   │ - 终止条件可达性
   │
   ▼
通过 → 实际执行
失败 → 返回错误信息（用于训练数据生成）
```

**实现位置**：
- L1: 现有 `src/modules/parser/markdown_parser.cpp` 复用
- L2: 新增 `src/modules/parser/signature_validator.cpp`
- L3: 现有 `src/modules/executor/node_executor.cpp` 的 dry-run 模式

### 6.2 错误信息规范化

训练数据中错误信息必须规范化（便于模型学习）：

```yaml
# 标准错误格式
error_type: ERR_NAMESPACE_VIOLATION
location: line 23, col 5
message: "Cannot write to /lib/memory/state@v1. Namespace /lib/** is read-only."
suggestion: "Use /dynamic/** or /main/** namespace instead."
```

**位置**：`src/core/errors/agenticdsl_errors.{h,cpp}`（新增），统一所有错误码格式。

---

## 7. 与规范演进流程的集成

### 7.1 本文档产生的 RFC 候选

| 提案 | 优先级 | 关联章节 |
|---|---|---|
| 特殊 token 注册 | P0 | §2.2.1 |
| Canonical Serializer | P0 | §2.2.2 |
| FIM 数据格式定义 | P0 | §2.2.3 |
| `<subgraph>` 可选锚点 (v3.11) | P1 | §2.3.1 |
| `flow` 简写 (v3.11) | P2 | §2.3.2 |
| 显式类型标注 (v3.11) | P2 | §2.3.3 |
| Indentation-Sensitive PDA (v4.0) | P3 | §2.4.1 |
| JSON-strict 模式 (v4.0 评估) | P3 | §2.4.2 |

### 7.2 RFC 创建流程

1. 在 `docs/adr/agenticdsl/language-extensions/` 创建 RFC 文档（参考 `01-type-system.md` 格式）
2. 关联 Oracle 评估（`docs/adr/agenticdsl/session-state/03-oracle-qa.md` 模式）
3. 进入 ADR 流程（编号 adr-0037+）
4. 在 `docs/specs/dsl.md` 更新规范

---

## 8. 总结

### 8.1 关键交付

| 维度 | 提案 | SOTA 依据 |
|---|---|---|
| **特殊 Token** | 11 个 structural tokens（含 FIM 范本） | Qwen2.5-Coder / PickyBPE |
| **Canonical Serializer** | 2 空格缩进、字典序键序、确定性字符串 | Voyager / CodeAlpaca |
| **可选锚点 (v3.11)** | `<\|subgraph_decl\|>`、`<\|node_def\|>` | XGrammar-2 Structural Tag |
| **EBNF Grammar** | v3.10 完整语法表达 | XGrammar / Guidance |
| **FIM 格式** | 4 种应用场景 + 50% 应用比例 | Qwen2.5-Coder / StarCoder2 |
| **错误规范化** | 标准错误格式 + 建议字段 | Self-Refine / Voyager |
| **Indentation PDA (v4.0)** | Adams2013 框架 | 学术前沿 |

### 8.2 不做什么（明确边界）

- ❌ 不替换 YAML 为 JSON（破坏 v3.10 兼容性）
- ❌ 不引入 XML 风格 `<agent>` 锚点（已有 `### AgenticDSL` 头足够）
- ❌ 不做纯 S-expression 重设计（v4.0 后再评估）
- ❌ 不改变 `type` 字段的字符串格式（影响大量现有工作流）

### 8.3 时间线

| 提案 | 周期 | 依赖 |
|---|---|---|
| **Phase 1** 训练侧工程 | 4-6 周（与 TR-1 并行） | 无 |
| **Phase 2** v3.11 增量 | 4-6 周（与 TR-2 同步） | Phase 1 数据积累 |
| **Phase 3** v4.0 RFC | 6+ 月 | 100K+ SFT 数据 |

---

**文档版本**: v1.0
**下次审查**: Phase 1 完成后（预计 6 周后）
**Owner**: 待指派（建议与 v3.11 规范 owner 协同）