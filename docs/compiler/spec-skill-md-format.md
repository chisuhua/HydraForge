# SPEC-SKILL-MD: SKILL.md 源文件格式 v1

**状态**: 已决 (2026-05-25)
**关联**: SPEC-SSL-IR, SPEC-COMPILED

---

## 1. 概述

SKILL.md 是编译器消费的源格式。一个标准 SKILL.md 包含：
- **文件头声明**：`### AgenticDSL` 块定义元信息
- **人类可读段落**：`## 描述`, `## 指令`, `## 工具` 等（供人阅读和 LLM 归一化使用）
- **SSL YAML 块**：` ```yaml ssl_version: "1.0" ... ``` `（供编译器确定性解析）

## 2. 文件结构

```markdown
### AgenticDSL `/lib/skills/{name}@{version}`
type: resource_declare
signature:
  inputs:
    - name: {param}
      type: string
      required: true
      description: "参数说明"
  outputs:
    - name: {result}
      type: any
      description: "输出说明"
  version: "1.0"
  stability: stable

---
# Skill: {name}

## 描述
当用户请求{trigger}时触发

## 指令
1. 第一步：{instruction}
2. 第二步：{instruction}
3. 第三步：{instruction}

## 工具
- {tool_name}: {tool_description}

## 参考资料
- {path}: {description}

---
```yaml
ssl_version: "1.0"
scheduling:
  name: "{name}"
structural:
  stages:
    # ...
logical:
  operations:
    # ...
```
```

## 3. 各部分要求

### 3.1 文件头（`### AgenticDSL`）

声明技能的输入/输出签名和权限。

| 字段 | 必需 | 说明 |
|------|------|------|
| `type: resource_declare` | ✅ | 固定值 |
| `signature.inputs` | ✅ | 编译输入的参数列表 |
| `signature.outputs` | ✅ | 编译输出的结果列表 |
| `permissions` | ✅ | 执行所需权限 |
| `stability` | 否 | `stable` 或 `experimental` |

### 3.2 人类段落

| 段落 | 必需 | 用途 |
|------|------|------|
| `## 描述` | ✅ | 路由匹配（写入 registry.description） |
| `## 指令` | ✅ | 工作流定义（供 LLM 归一化 Path B 使用） |
| `## 工具` | ✅ | 工具清单（验证 SSL tools 一致性） |
| `## 参考资料` | 否 | 外部文档引用（用于 `[LOAD_REF]` 协议） |

### 3.3 SSL YAML 块

位于文件末尾，被 ```yaml … ``` 包裹。

```
```yaml
ssl_version: "1.0"
scheduling: ...
structural: ...
logical: ...
```
```

解析规则：
- 扫描 ```yaml 代码块，查找包含 `ssl_version:` 的行
- 找到后解析整个 YAML 块为 SSL IR
- 如果存在多个 SSL 块，以最后一个为准

## 4. 文件路径约定

```
/skills/
├── {skill-name}/
│   ├── SKILL.md           # 主文件（含 SSL 块）
│   ├── refs/              # 参考资料目录
│   │   ├── tag-taxonomy.md
│   │   └── severity-levels.md
│   └── scripts/           # 本地脚本目录
│       └── helper.py
```

- 编译器搜索：`/skills/{name}/SKILL.md`
- Ref 路径：相对于 SKILL.md 的 `refs/` 目录
- Script 路径：相对于 SKILL.md 的 `scripts/` 目录

## 5. 编译流程（文件视角）

```
SKILL.md
    │
    ├─ P1: fs.read 读取全文
    ├─ P1b: 检测 ssl_version 标记
    │
    ├─ [SSL 存在] → P2A: 确定性解析 → P4: 结构层映射 → ...
    │
    └─ [SSL 不存在] → P2B: LLM 归一化
                     → [enforce_ssl=true] → 编译失败，提示 transpile
                     → [enforce_ssl=false] → 继续编译
```

## 6. 完整示例

放在 `lib/` 文件中以展示完整的样例，详见 `docs/examples/skills/daily-report/SKILL.md`。

## 7. SSL IR 格式

见 `spec-ssl-ir-format.md`。
