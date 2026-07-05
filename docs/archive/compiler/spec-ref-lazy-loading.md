# SPEC-REF-LOAD: Reference 懒加载规范

**状态**: 已决 (2026-05-25)
**关联**: ADR-0019 (Fork/Join), SPEC-COMPILED

---

## 1. 概述

两种加载模式：
- **静态预声明**：SSL `structural.stages[].refs` — 编译时生成 fork/join 子图
- **动态 [LOAD_REF]**：LLM 执行时以 `[LOAD_REF:path]` 标记请求

## 2. `[LOAD_REF]` 协议

### 2.1 语法

```
[LOAD_REF:path/to/reference.md]
```

- `path` 相对于源 SKILL.md 目录
- 独占一行
- 允许多个在不同行

### 2.2 检测与擦除

```
LLM 输出含 [LOAD_REF:refs/doc.md]
    ↓
check_refs 节点: regex.extract → ref_paths
    ↓
标记从输出中擦除 → phase_output
    ↓
assert: ref_paths 非空？
    ├─ yes → fork/join 并行读取
    └─ no  → 下一 phase
    ↓
下一 dsl_call prompt: "已加载参考资料: doc.md"
```

### 2.3 路径解析

- `./` 开头 → 相对于 `working.data.source_dir`
- `/` 开头 → 绝对路径
- 否则 → 相对于 `source_dir`

## 3. 静态 Refs 子图

### 3.1 结构

```yaml
### AgenticDSL `/main/load_refs_fork_2`
type: fork
fork:
  branches:
    - "/main/load_ref_2_1"
    - "/main/load_ref_2_2"
join:
  wait_for: all
  merge_strategy: deep_merge
next: "/main/phase2_classify"

### AgenticDSL `/main/load_ref_2_1`
type: tool_call
tool: fs.read
arguments:
  path: "{{ source_dir }}/refs/tag-taxonomy.md"
output_mapping:
  "working.data.loaded_refs.tag-taxonomy.md": "result.content"
next: "/main/join_refs_2"
```

### 3.2 并发

- 每个 `load_ref` 分支在独立 `jthread` 中执行（ADR-0019 D1）
- `wait_for: all` — 所有分支完成后才继续
- `merge_strategy: deep_merge` — 合并到 `loaded_refs`

## 4. 输出映射

所有 refs 写入 `working.data.loaded_refs`，key=文件名：

```json
{"working.data.loaded_refs": {
  "tag-taxonomy.md": "内容...",
  "severity-levels.md": "内容..."
}}
```

下一 phase prompt 中引用：

```
{% for name, content in working.data.loaded_refs %}
- {{ name }}: {{ content | truncate(1000) }}
{% endfor %}
```

## 5. 缓存

| 范围 | 行为 |
|------|------|
| 同一 phase 重复请求 | 跳过（已在 loaded_refs） |
| 跨 phase | 共享 loaded_refs |
| 跨执行周期 | 不缓存 |

## 6. 错误处理

- ref 文件不存在 → fs.read 抛异常 → phase 失败
- 路径格式非法 → regex 不匹配 → 跳过
- 内容超限 → truncate(1000) 控制
