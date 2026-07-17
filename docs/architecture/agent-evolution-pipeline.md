# Agent 进化管线（Evolution Pipeline）

**日期**: 2026-07-16
**状态**: 🟡 Proposed (架构讨论中)
**作者**: Architecture Working Group
**关联**: `docs/architecture/agent-as-plugin-architecture-v1.1.md`

---

## 一、核心思想

Agent 不是一次性写成的，而是**从原型到生产、从解释执行到原生编译、从高动态到高稳定**的渐进产物。

本管线定义四个阶段：

```
SKILL.md ──[固化]──→ .agent.md ──[性能化]──→ C++ ──[可移植化]──→ Wasm
   │                     │                     │                   │
   阶段 1: Prototype     阶段 2: Production    阶段 3: Native     阶段 4: Portable
   快速验证              结构化生产          性能优化           跨平台/强隔离
```

**关键原则**：
- 每个阶段都保持**同一外部契约**（tools + manifest + input/output schema）。
- 阶段转换是**行为保持的**（semantics-preserving），只改变实现质量。
- 任一 Agent 可以**停留在任一阶段**，也可以**继续向前进化**。
- 反向流动（Wasm → C++ → DSL → Skill）仅在必要时发生，且通常需要人工干预。

---

## 二、四阶段详解

### 2.1 阶段 1：Prototype (SKILL.md)

**目标**：用自然语言/半结构化描述快速验证 Agent 行为。

**文件形态**：
```
pdk/{agent}/skills/core/SKILL.md
pdk/{agent}/skills/extensions/SKILL.md
```

**SKILL.md 示例**（Code Review Agent）：

```markdown
# Code Review Agent

## 角色
你是一名资深代码审查员，专注于发现 bug、安全漏洞和可维护性问题。

## 输入
- `code`: 待审查代码片段
- `language`: 编程语言
- `severity`: 审查严格程度（low/medium/high）

## 流程
1. 先通读代码，理解整体结构。
2. 按以下维度检查：
   - 安全风险（SQL 注入、XSS、缓冲区溢出）
   - 逻辑错误（空指针、越界、竞态条件）
   - 可维护性（命名、复杂度、重复代码）
3. 输出 JSON 格式的审查报告，每个 issue 包含：
   - `line`: 行号
   - `severity`: 严重级别
   - `message`: 问题描述
   - `suggestion`: 修改建议

## 输出格式
```json
{"issues": [{"line": 42, "severity": "high", "message": "...", "suggestion": "..."}]}
```
```

**执行环境**：
- 由 OS `SkillInterpreter` 在**隔离环境**中执行
- 隔离方式：
  - 进程隔离（Process sandbox）
  - WebAssembly 沙箱（如果 SKILL 编译为 Wasm）
  - 受限脚本解释器（如无网络、无文件系统访问）

**关键约束**：
- `requires_isolation = true`
- `timeout_ms` 强制（默认 30s）
- `max_concurrent` 限制
- `side_effects` 必须声明

**优势**：
- 零编译，即写即跑
- 非技术人员可参与
- LLM 可直接修改
- 可复用现有技能生态（OpenCode / Claude Code Skills）

**限制**：
- 非结构化，难以验证
- 解释执行开销大
- 运行时不可控
- 安全风险高，必须隔离

---

### 2.2 阶段 2：Production DSL (.agent.md)

**目标**：把非结构化的 SKILL.md 转化为结构化、可验证、可审计的 AgenticDSL 图。

**转换过程：固化（Solidification）**

```
SKILL.md ──→ LLM / 规则引擎 ──→ .agent.md ──→ DSL Validator ──→ 通过
```

**转换示例**：

输入（SKILL.md 片段）：
```markdown
## 流程
1. 先通读代码，理解整体结构。
2. 按以下维度检查：安全风险、逻辑错误、可维护性。
3. 输出 JSON 格式的审查报告。
```

输出（`.agent.md`）：
```markdown
# Code Review Agent (Solidified)

## metadata
- version: 1.0
- agent_id: code.review
- form: dsl
- budget_inheritance: strict

## nodes

### read_input
- type: assign
- code: "{{input.code}}"
- language: "{{input.language}}"
- severity: "{{input.severity}}"
- output: request

### analyze_security
- type: generate
- prompt: "Review this {{language}} code for security issues:\n```\n{{code}}\n```"
- output: security_issues

### analyze_logic
- type: generate
- prompt: "Review this {{language}} code for logic errors:\n```\n{{code}}\n```"
- output: logic_issues

### analyze_maintainability
- type: generate
- prompt: "Review this {{language}} code for maintainability issues:\n```\n{{code}}\n```"
- output: maintainability_issues

### merge_report
- type: assign
- issues: "{{merge_issues(security_issues, logic_issues, maintainability_issues)}}"
- output: report

### format_output
- type: assign
- output: "{{to_json(report)}}"
- type: end
```

**固化验证器（DSL Validator）**：
- 检查所有输入变量是否被声明
- 检查所有 `generate` 节点是否有 prompt
- 检查输出格式是否符合 schema
- 检查预算继承策略是否合法
- 检查工具调用是否遵循 Layer Profile

**优势**：
- 结构化、可可视化
- 每个节点有 Trace 记录
- 可形式化验证部分属性
- 可热更新（无需重新编译）
- 预算控制内建

**限制**：
- 转换依赖 LLM，可能引入语义偏差
- 复杂控制流（循环、递归）需要更强大的 DSL 原语
- 性能仍受解释器限制

---

### 2.3 阶段 3：Native C++

**目标**：在保持 DSL 外层编排的同时，将热点路径替换为 C++ 实现，提升性能。

**两种迁移策略**：

#### 策略 A：节点替换（Node Replacement）

保持 `.agent.md` 图不变，将某些节点类型替换为 C++ 实现。

```markdown
### analyze_security
- type: tool
- name: "cpp_security_analyzer"   # 替换原来的 generate 节点
- args: {code: "{{code}}", language: "{{language}}"}
- output: security_issues
```

C++ 实现：
```cpp
DECLARE_TOOL(cpp_security_analyzer, "C++ security analyzer", ReadOnly, "plan",
    auto code = args["code"].get<std::string>();
    auto language = args["language"].get<std::string>();
    
    SecurityAnalyzer analyzer;
    auto issues = analyzer.scan(code, language);
    
    return issues.to_json();
)
```

#### 策略 B：完全重写（Full Rewrite）

当整个 Agent 逻辑都高度优化时，用 `DEFINE_AGENT` 宏完全重写为 C++。

```cpp
DEFINE_AGENT(CodeReviewAgent, AgentLoopType::React)

extern "C" void pdk_register_tools(IToolRegistry& registry) {
    registry.register_tool_function(
        "code_review/run",
        ToolMetadata{ToolCategory::ReadOnly, ...},
        [](const auto& args) -> nlohmann::json {
            auto code = args["code"].get<std::string>();
            auto language = args["language"].get<std::string>();
            auto severity = args["severity"].get<std::string>();
            
            CodeReviewAnalyzer analyzer(language, severity);
            auto report = analyzer.review(code);
            return report.to_json();
        }
    );
}
```

**优势**：
- 微秒级延迟
- 可做多线程、缓存、持久化
- 系统级操作（如调用静态分析器）
- 编译时类型安全

**限制**：
- 修改需重新编译
- 开发门槛高
- 可解释性下降

---

### 2.4 阶段 4：Portable Wasm

**目标**：将 DSL 或 C++ 实现编译为 WebAssembly，实现跨平台分发和强隔离。

#### 来源 1：DSL → Wasm

```
.agent.md ──→ DSL Compiler ──→ Wasm bytecode (with graph interpreter embedded)
```

这种 Wasm 内部包含一个轻量级 DSL 解释器，加载后在沙箱中执行图。

#### 来源 2：C++ → Wasm

```
C++ source ──→ Emscripten / wasi-sdk ──→ Wasm binary
```

这种 Wasm 是编译型，性能接近原生。

**Wasm Runtime 设计**：

```cpp
class WasmRuntime {
public:
    // 加载 .wasm 模块
    WasmModule load(const std::string& wasm_path, 
                    const CapabilitySet& capabilities);
    
    // 调用入口函数
    nlohmann::json invoke(WasmModule& module,
                          const std::string& entry_function,
                          const nlohmann::json& args);
    
    // 销毁模块
    void unload(WasmModule& module);
};
```

**Host Functions（Capability-limited）**：

```cpp
// 允许（通过 capability 授权）
extern "C" nlohmann::json host_call_tool(const char* name, const char* args_json);
extern "C" void host_emit_event(const char* topic, const char* payload_json);
extern "C" bool host_consume_budget(double amount);
extern "C" void host_log(const char* level, const char* message);

// 禁止（除非显式 capability）
// host_read_file, host_write_file, host_network_request, host_env_get
```

**优势**：
- 强沙箱（内存安全 + capability-based）
- 跨平台分发（x86/ARM/边缘设备）
- 启动快
- 适合不可信环境

**限制**：
- 需要 Wasm 运行时支持
- 某些系统级操作难以在 Wasm 中实现
- C++ → Wasm 编译链路复杂

---

## 三、进化管线服务

### 3.1 所需 OS 服务

| 服务 | 阶段 | 职责 |
|------|------|------|
| `SkillInterpreter` | 1 | 隔离解释 SKILL.md |
| `AgenticDSLCompiler` | 2 | .agent.md → ParsedGraph |
| `DSLValidator` | 2 | 验证 DSL 正确性 |
| `SolidificationEngine` | 1→2 | SKILL.md → .agent.md 自动/半自动转换 |
| `C++CodeGenerator` | 2→3 | 从 DSL 生成 C++ 节点模板 |
| `WasmCompiler` | 3→4 | DSL/C++ → Wasm |
| `WasmRuntime` | 4 | 加载并执行 Wasm Agent |
| `RegressionSuite` | 全阶段 | 行为一致性测试 |

### 3.2 回归测试策略

每个阶段转换都必须通过行为一致性测试：

```cpp
TEST_CASE("code_review_agent evolution preserves semantics") {
    auto input = R"({"code": "int main() { ... }", "language": "cpp", "severity": "high"})";
    
    auto skill_result = SkillInterpreter::run("skills/core/SKILL.md", input);
    auto dsl_result = DSLEngine::from_markdown_file("agents/code_review.agent.md")->run(input);
    auto cpp_result = call_tool("code_review/run", input);
    auto wasm_result = WasmRuntime::load("wasm/code_review.wasm").invoke("run", input);
    
    // 语义一致性：issue 集合必须等价（允许顺序不同）
    REQUIRE(issue_sets_equivalent(skill_result, dsl_result, cpp_result, wasm_result));
}
```

---

## 四、典型进化案例

### 案例：Code Review Agent

| 阶段 | 形态 | 触发条件 | 产出 |
|------|------|---------|------|
| 1 | SKILL.md | 产品经理验证审查维度 | 0.1.0 原型 |
| 2 | .agent.md | 审查逻辑稳定，需要审计 | 1.0.0 生产版 |
| 3 | C++ | 性能不满足（>500ms） | 2.0.0 高性能版 |
| 4 | Wasm | 需要在 IDE 插件中运行 | 3.0.0 跨平台版 |

### 案例：Loop Agent

| 阶段 | 形态 | 触发条件 | 产出 |
|------|------|---------|------|
| 1 | SKILL.md | 探索新循环策略 | 实验性 Loop |
| 2 | .agent.md | React/PlanExecute/ForkJoin 策略确定 | 标准 Loop Agent |
| 3 | C++ | 高频调用（如 IDE 自动补全） | 微秒级 Loop Agent |
| 4 | Wasm | 浏览器/VS Code Web 扩展 | 可移植 Loop Agent |

---

## 五、安全与隔离矩阵

| 形态 | 隔离要求 | 默认 capability | 适用场景 |
|------|---------|-----------------|----------|
| SKILL.md | **必须强隔离** | 仅 `call_tool` / `emit_event` / `consume_budget` | 不可信/探索性 |
| .agent.md | 中 | 通过 ToolMetadata + Layer Profile 控制 | 可信生产 |
| C++ | 低 | 通过 OS 权限系统控制 | 可信高性能 |
| Wasm | 强 | capability-limited host functions | 不可信/边缘 |

---

## 六、决策树：何时推进到下一阶段

```
阶段 1 (Skill) → 阶段 2 (DSL) 当：
  ✓ 业务逻辑稳定，不再频繁变化
  ✓ 需要审计或合规
  ✓ 需要预算控制或资源限制
  ✓ 需要多工程师协作

阶段 2 (DSL) → 阶段 3 (C++) 当：
  ✓ 性能成为瓶颈（延迟 > 100ms 或吞吐不足）
  ✓ 需要复杂状态管理
  ✓ 需要系统级操作
  ✓ 需要强类型安全

阶段 3 (C++) → 阶段 4 (Wasm) 当：
  ✓ 需要跨平台分发
  ✓ 需要在不可信环境运行
  ✓ 需要边缘部署
  ✓ 需要启动速度优化

不推进：
  ✗ 业务逻辑仍在快速探索
  ✗ 性能满足需求
  ✗ 团队缺乏 C++ / Wasm 能力
```

---

## 七、与现有架构的关系

| 文档 | 关系 |
|------|------|
| `agent-as-plugin-architecture-v1.1.md` | 本管线是 L2 Plugin Layer 的核心机制 |
| `application-layer-sota-positioning.md` | 进化管线是 Phase A→B→C 的技术基础 |
| `adr-0021-pdk-design.md` | PDK 是 C++ 形态的脚手架；未来需要 Wasm 脚手架 |
| `adr-0051-phase6-pdk-composition-spike.md` | Spike 已验证 PDK 插件可组合；本管线扩展为形态进化 |

---

## 八、下一步行动

1. **设计 `SkillInterpreter` 隔离模型**（P1）
   - 选择隔离技术：进程沙箱 / Wasm 解释器 / seccomp
   - 定义 SKILL.md 的语法子集（禁止哪些构造？）
   - 设计 capability 注入机制

2. **设计 `SolidificationEngine` 初版**（P2）
   - 用 LLM 将 SKILL.md 转写为 .agent.md
   - 用 DSL Validator 检查输出
   - 建立回归测试集

3. **评估 Wasm 技术栈**（P2）
   - C++ → Wasm：Emscripten vs wasi-sdk
   - DSL → Wasm：自定义编译器 vs 嵌入解释器
   - Host function 设计

4. **更新 PDK 工具链**（P1）
   - 为 `AgentDescriptor` 添加 `forms` 字段
   - 添加 `pdk_manifest()` 导出函数
   - 添加 `requires_isolation` 安全声明

---

**核心结论**：Agent 进化管线让 HydraForge 的 Agent 既能在**原型期**快速迭代（SKILL），又能在**生产期**保证可审计和性能（DSL/C++），最终还能在**边缘期**安全分发（Wasm）。这是 HydraForge 区别于所有现有 SOTA 框架的关键差异化能力。
