# Ideal DSL Extension 04: domain_skill

**文件**: `04_domain_skill.md`
**状态**: 提案
**解决的问题**: 领域/工具类技能（轴2）的 DSL 模式标准化

---

## 动机

轴2（领域/工具）技能与轴1完全不同：

| 维度 | 轴1-流程 | 轴2-领域 |
|------|---------|---------|
| 目的 | 定义工作流 | 提供工具能力 |
| 触发 | 用户意图 | 具体问题 |
| 执行 | 顺序阶段 | 命令调用 |
| 输出 | 文档/决策 | 数据/结果 |
| 状态 | 复杂状态机 | 简单上下文 |

轴2 技能的典型结构：
```
用户描述问题 → 选择工具 → 执行命令 → 分析结果 → 给出建议
```

---

## 现有领域类技能的共同结构

### cmake_workflow
```
用户输入 CMake 任务
    ↓
分析任务类型（new_target/dependency/configuration/cross_compile/sanitizer）
    ↓
检查现有 CMakeLists.txt
    ↓
设计 CMake 修改
    ↓
应用修改
    ↓
测试配置（cmake configure）
    ↓
测试构建（make）
    ↓
成功/失败 → 汇总报告
```

### cpp_debug
```
用户描述问题
    ↓
分类问题类型（crash/deadlock/memory/bounds/dangling/logic/ub）
    ↓
准备调试环境（编译 debug 版本）
    ↓
根据类型选择诊断工具
    ↓
fork: 并行运行多个诊断工具
    ↓
合并分析结果
    ↓
形成假设 → 验证
    ↓
生成修复
    ↓
验证
```

### 共同模式识别

1. **问题分类** — 输入 → 类型分类
2. **工具选择** — 根据类型选择合适的工具
3. **并行诊断** — 多个工具并行运行
4. **结果合并** — 合并多源诊断结果
5. **动作执行** — 根据诊断结果执行动作

---

## 提案：领域类技能标准 DSL 模式

### 核心语法

```markdown
## /cppdbg/start
type: domain_skill
skill: "cpp_debug"
domain: cpp_runtime

# 输入处理
input:
  problem_description:
    type: user_input
    prompt: "描述 C++ 运行时问题"
    required: true

# 问题分类器
classifiers:
  - name: detect_problem_type
    type: dsl_call
    llm_tool: gpt-4
    output: problem_type
    categories:
      - crash
      - deadlock
      - memory_leak
      - out_of_bounds
      - dangling_ptr
      - logic_error
      - undefined_behavior

# 工具选择器
tool_selector:
  type: switch
  input: "{{problem_type}}"
  mapping:
    crash: [gdb, asan]
    deadlock: [gdb_thread, strace]
    memory_leak: [valgrind, asan]
    out_of_bounds: [asan, ubsan]

# 诊断执行（并行）
diagnostic:
  parallel: true
  tools: "{{tool_selector.selected}}"
  commands:
    gdb: "gdb -batch -ex run -ex bt {{binary}}"
    asan: "cmake ... -fsanitize=address && make && ./binary"
    valgrind: "valgrind --leak-check=full ./binary"
  merge:
    type: dsl_call
    llm_tool: gpt-4
    prompt: "合并 {{count}} 个诊断工具的输出"

# 结果处理
output:
  root_cause: "{{merge_analysis.root_cause}}"
  fix_suggestion: "{{merge_analysis.fix}}"
```

### 语义

| 字段 | 含义 |
|------|------|
| `domain_skill` | 声明这是一个领域/工具类技能 |
| `input` | 输入定义 |
| `classifiers` | 问题分类器 |
| `tool_selector` | 根据类型选择工具 |
| `diagnostic` | 并行诊断执行配置 |
| `output` | 输出映射 |

---

## 工具调用模板

### 语法

```markdown
## /tool/gdb
type: tool_template
name: gdb_batch
base_command: "gdb -batch {{flags}} {{binary}}"
parameters:
  flags:
    type: string
    default: "-ex run -ex bt"
  binary:
    type: path
    required: true
    validate: executable
timeout: 60
output_format: structured
```

### 使用

```markdown
## /cppdbg/run_gdb
type: tool_call
using: gdb_batch
arguments:
  binary: "{{debug_binary}}"
  flags: "-ex run -ex bt -ex info registers"
output_keys: ["gdb_output"]
```

---

## 诊断工具节点

### 语法

```markdown
## /cppdbg/run_diagnostics
type: diagnostic_tool
tool: "{{selected_tool}}"
target: "{{target}}"
options:
  timeout: 120
  retry: 2
  env:
    ASAN_OPTIONS: "detect_leaks=1"
output:
  stdout: diag_stdout
  stderr: diag_stderr
  exit_code: diag_exit
```

### 内置诊断工具

| 工具 | 适用问题 | 输出格式 |
|------|---------|---------|
| `gdb` | 崩溃、死锁 | structured |
| `valgrind` | 内存泄漏 | leak report |
| `asan` | 内存错误 | error report |
| `perf` | 性能 | flamegraph |
| `strace` | 系统调用 | trace log |

---

## 实现要求

### 1. 领域技能解析器

```cpp
class DomainSkillParser {
public:
    // 解析 domain_skill 节点
    ParsedDomainSkill parse(const YAML::Node& node);

    // 验证工具可用性
    void validate_tools(const ParsedDomainSkill& skill);

    // 生成工具选择逻辑
    std::string generate_tool_selector(const ParsedDomainSkill& skill);
};
```

### 2. 诊断工具注册表

```cpp
class DiagnosticToolRegistry {
public:
    void register_tool(const std::string& name, const DiagnosticTool& tool);

    // 检查工具是否可用
    bool is_available(const std::string& name) const;

    // 获取工具调用命令
    std::string get_command(const std::string& name, const Args& args) const;

    // 解析工具输出
    ParsedOutput parse_output(const std::string& name,
                               const std::string& raw_output) const;
};
```

### 3. 工具选择器生成器

```cpp
class ToolSelectorGenerator {
public:
    // 根据问题类型生成工具选择 switch
    std::string generate(const std::map<std::string, std::vector<std::string>>& mapping);

    // 验证工具选择覆盖所有类型
    void validate_coverage(const ToolSelector& selector);
};
```

---

## 使用示例

### 示例 1：cmake_workflow 用 domain_skill 重写

```markdown
## /cmake/start
type: domain_skill
skill: "cmake_workflow"
domain: build_system

input:
  task_description:
    type: user_input
    prompt: "描述 CMake 任务"

classifiers:
  - name: classify_task
    type: dsl_call
    llm_tool: gpt-4
    output: task_type
    categories:
      - new_target
      - dependency
      - configuration
      - cross_compile
      - sanitizer

tool_selector:
  type: switch
  input: "{{task_type}}"
  mapping:
    new_target: [cmake_add_target]
    dependency: [cmake_find_package, cmake_fetch_content]
    configuration: [cmake_configure, cmake_options]
    cross_compile: [cmake_toolchain]
    sanitizer: [cmake_sanitizer_enable]

diagnostic:
  tools: [cmake_configure, cmake_build]
  parallel: false
  commands:
    cmake_configure: "cmake .. -DCMAKE_BUILD_TYPE={{build_type}}"
    cmake_build: "make -j$(nproc)"

output:
  success: "{{diagnostic.success}}"
  summary: "{{diagnostic.summary}}"
```

### 示例 2：并行工具执行

```markdown
## /debug/parallel_diagnosis
type: domain_skill
skill: "multi_tool_diagnosis"

classifiers:
  - name: detect_problem_type
    type: dsl_call
    output: problem_type

tool_selector:
  type: static
  tools:
    - gdb
    - valgrind
    - asan

diagnostic:
  parallel: true
  timeout_per_tool: 120
  fail_fast: false
  merge:
    type: dsl_call
    llm_tool: gpt-4
    prompt: "分析 {{count}} 个诊断工具的输出，识别根因"

output:
  root_cause: "{{merged.root_cause}}"
  confidence: "{{merged.confidence}}"
  recommendations: "{{merged.recommendations}}"
```

---

## 与 skill_invoke 的关系

```
skill_invoke  = 调用技能
domain_skill  = 技能内部使用领域工具的模式

skill_invoke 包装 domain_skill 提供统一的调用接口
```

---

## 工具箱技能

轴2 技能可以作为"工具箱"被其他技能调用：

```markdown
## /debug/use_cpp_debug
type: skill_invoke
skill: "cpp_debug"
input:
  symptom: "{{user_reported_symptom}}"
  problem_type: auto  # 让 cpp_debug 自己分类
output:
  root_cause: crash_cause
  fix: crash_fix
```

---

## 优先级

**中** — 领域技能可以先用 skill_invoke 实现，再演进到 domain_skill 模式。

---

## 验证方式

1. 将 cmake_workflow 和 cpp_debug 用 domain_skill 重写
2. 验证工具选择正确性
3. 验证并行诊断执行