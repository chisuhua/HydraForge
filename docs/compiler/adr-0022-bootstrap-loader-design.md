# ADR-0022: Bootstrap Loader 设计

**状态**: 已决 (2026-05-25)
**关联决策点**: D6 (Bootstrap Loader 存在形式), SR1 (SSL-as-IR)

---

## 背景

Bootstrap Loader 是用户进入技能系统的入口点。它负责接收用户输入、匹配 skill_registry 中的技能、并按技能类型路由到执行或编译路径。

## 决定

### D6: Bootstrap Loader 存在形式

**决策**: Option A — 混合。C++ 层负责最小初始化（ToolRegistry、StandardLibraryLoader、privileged tools），路由与执行流程在 `bootstrap.agent.md` DSL 文件中定义。

**选择依据**:
- C++ 层必须已存在才能解析和运行任何 DSL（这是不可回避的约束）
- 引导流程写在 DSL 中意味着无需重新编译即可修改行为
- 用户可以覆盖 `bootstrap.agent.md` 定制入口行为

### SSL-as-IR 集成

Bootstrap Loader 通过 `codelet_call` 分类技能类型，然后按类型路由：

| 类型 | 路由 |
|------|------|
| `native` / `compiled` | 直接 `fs.read` + `generate_subgraph` 注入执行 |
| `source` + `has_ssl:true` | `generate_subgraph` 调用编译器（Path A, 确定性） |
| `source` + `has_ssl:false` | `generate_subgraph` 调用编译器（Path B, LLM 归一化）或报错 |

### Registry 条目增强

增加 `type`, `has_ssl`, `ssl_version`, `priority`, `tags`, `stages` 字段，支持分类路由。

### 工具注册

| 工具 | 权限 | 用途 |
|------|------|------|
| `fs.read` | 普通 | 读取技能文件 |
| `registry.lookup` | 普通 | 查注册表获取技能元数据 |
| `codelet.run` | `privileged:` | 执行 Python 分类脚本 |
| `skill.register` | `privileged:` | 注册编译产物（编译器 P7 用） |

## 详细规范

详见 `spec-bootstrap-loader.md`。
