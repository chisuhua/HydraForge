# SPEC-CODELET-CALL: `codelet_call` 节点类型规范

**状态**: 已决 (2026-05-25)
**关联**: ADR-0021 (SSL-as-IR 架构), ADR-0019 (Fork/Join)

---

## 1. 动机

SSL-as-IR 路径 A（确定性 SSL 解析）需要 Python 脚本在编译时执行：
- `parse-ssl.py`: 解析 SKILL.md 中的 SSL YAML 块为 JSON IR
- `audit-operations.py`: 审计 logical.operations 权限声明
- `validate-dsl.py`: 验证编译产物的结构完整性

这三个脚本都遵循同一模式：JSON → JSON 变换，无文件系统副作用。

## 2. 节点类型定义

### 2.1 枚举

```cpp
enum class NodeType : uint8_t {
    // ... 现有类型 ...
    CODELET_CALL,  // 新增
};
```

### 2.2 结构体

```cpp
struct CodeletNode : public Node {
    std::string runtime;                    // "python3"
    std::optional<std::string> code;        // 内联代码（互斥于 code_file）
    std::optional<std::string> code_file;   // 脚本文件路径（互斥于 code）
    std::vector<std::string> args;          // 命令行参数（模板可渲染）
    std::vector<std::string> output_keys;   // 输出映射到 context

    std::vector<std::string> inject_keys;   // 要注入到子进程 context 的 working.data 路径
                                            // 例如 ["source_md", "source_dir"]
};
```

### 2.3 Markdown DSL 语法

```yaml
### AgenticDSL `/main/parse_ssl"
type: codelet_call
runtime: python3
code_file: "./scripts/parse-ssl.py"
args:
  - "{{ input.source_skill_path }}"
output_keys: ["working.data.ssl_parse_result"]
inject_keys:
  - "source_md"
  - "source_dir"
```

或内联代码：

```yaml
### AgenticDSL `/main/check_ssl"
type: codelet_call
runtime: python3
code: |
  import re, json
  md = context['source_md']
  has_ssl = bool(re.search(r'ssl_version:', md))
  print(json.dumps({"has_ssl": has_ssl}))
output_keys: ["working.data.has_ssl_block"]
```

## 3. 子进程执行协议

### 3.1 通信协议

```
NodeExecutor                         Python 子进程
    │                                      │
    ├─ 组合 Context 契约为 JSON ──────────►│ stdin
    │   {                                  │
    │     "args": ["path/to/skill.md"],    │
    │     "context": {                     │
    │       "source_md": "...",           │
    │       "source_dir": "/skills/..."   │
    │     }                                │
    │   }                                  │
    │                                      │ 执行脚本逻辑
    │                                      │
    │◄── stdout JSON ────────────────────│
    │   {                                  │
    │     "has_ssl": true,                │
    │     "ssl": { scheduling: {...} },   │
    │     "errors": []                    │
    │   }                                  │
    │                                      │
    ├─ 解析 stdout JSON
    ├─ output_mapping 到 context
    └─ 继续 DAG 执行
```

### 3.2 协议规则

- **stdin**: NodeExecutor 写入单行 JSON（含 args + 选中的 working.data 字段）
- **stdout**: 子进程输出 JSON 到 stdout 后关闭。NodeExecutor 读取全部 stdout 后解析 JSON
- **stderr**: 捕获到日志（警告/非致命信息），错误时作为异常消息
- **exit code**: 0=成功。非0=失败，NodeExecutor 抛出异常含 stderr 内容
- **超时**: 默认 30 秒。可通过节点 metadata 覆盖

### 3.3 Context 契约安全性

- 仅 `inject_keys` 中列明的 working.data 字段被序列化到子进程
- 禁止传递 `system.*`、`recent.*` 等敏感层级
- 限制在 `working.data.*` 范围内
- 超时后的子进程被强制 kill（SIGTERM → SIGKILL）

## 4. 执行流程

```
NodeExecutor::execute_codelet(node, ctx)
    │
    ├─ 1. 渲染 args 模板（Inja）
    │      args[i] = render(node.args[i], ctx)
    │
    ├─ 2. 解析脚本路径
    │      if node.code_file:
    │          script_path = resolve_relative_path(node.code_file, source_dir)
    │      else if node.code:
    │          script_path = write_temp_file(node.code, ".py")
    │
    ├─ 3. 组合 Context 契约
    │      contract = {
    │          "args": rendered_args,
    │          "context": extract_fields(ctx, node.inject_keys)
    │      }
    │
    ├─ 4. 启动子进程
    │      proc = popen({
    │          "python3",
    │          script_path,
    │          rendered_args[0], ...
    │      }, stdin=PIPE, stdout=PIPE, stderr=PIPE)
    │
    ├─ 5. 写入 stdin + 读取 stdout/stderr
    │      stdin.write(json.dumps(contract))
    │      stdin.close()
    │      stdout_data, stderr_data = proc.communicate(timeout=30)
    │
    ├─ 6. 检查 exit_code + 解析结果
    │      if exit_code != 0:
    │          throw RuntimeError(stderr_data)
    │      result = json.parse(stdout_data)
    │
    └─ 7. output_mapping
           for key in node.output_keys:
               ctx[key] = extract_from_json(result, key)
```

## 5. 脚本目录约定

编译器技能的脚本位置：

```
/skills/skill-compiler/
├── SKILL.md
├── scripts/
│   ├── parse-ssl.py          # P2 路径 A
│   ├── audit-operations.py   # P3
│   └── validate-dsl.py       # P5
└── refs/
    ├── ssl-normalization-guide.md
    ├── phase-codegen-patterns.md
    └── agenticdsl-syntax-v310.md
```

脚本路径解析规则：
- `code_file: "./scripts/parse-ssl.py"` → 相对于 `working.data.source_dir`（即技能目录）
- `code_file: "/absolute/path/script.py"` → 绝对路径（慎用）
- 无 `../` 逃逸限制（但由权限检查控制）

## 6. 错误处理

| 场景 | 行为 |
|------|------|
| exit_code != 0 | 抛 `RuntimeError`，含 stderr 内容 |
| stdout 非合法 JSON | 抛 `RuntimeError("codelet output is not valid JSON")` |
| 超时 (30s) | `proc.kill()` → 抛 `RuntimeError("codelet timed out")` |
| 脚本不存在 | 抛 `RuntimeError("script not found: {path}")` |
| Python 未安装 | 抛 `RuntimeError("runtime python3 not found")` |
| inject_keys 含受限路径 | 静默忽略（不传递 `system.*` 等） |

## 7. 安全约束

- **无网络访问**：子进程继承主进程的网络命名空间，但脚本被设计为纯本地计算
- **无交互式输入**：stdin 仅用于 JSON 契约写入，不保持打开
- **资源限制**：超时（30s）+ 可选的 setrlimit（RLIMIT_CPU, RLIMIT_AS）
- **临时文件清理**：`code:` 生成的临时 .py 文件在子进程结束后删除
- **路径限制**：code_file 不能以 `../` 逃逸出 `source_dir` 范围（`enforce_ssl=true` 时）

## 8. 注册到引擎

`codelet_call` 需要一个注册到 ToolRegistry 的工具函数，该函数封装子进程创建、stdin 写入、stdout 读取和超时控制：

```cpp
// engine.cpp 初始化
engine->register_tool("codelet.run", [](const auto& args) -> json {
    // args["runtime"] = "python3"
    // args["script"] = "/path/to/script.py"
    // args["contract"] = serialized JSON string
    // 创建子进程、写入 stdin、读取 stdout、处理超时
    // 返回 stdout JSON 解析结果
});
```

这个工具仅限内部使用，不在用户 DSL 中直接暴露（用户 DSL 使用更安全的 `codelet_call` 节点类型）。
