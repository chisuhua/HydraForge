# SHIPPED ContextRequest Reference Fixtures

> **P0-10 fixture 目录 (per design.md §3.1.6)** — 3 个 reference ContextRequest JSONL 文件, 用户可 clone + modify.
> Per R13.3 S32, L2 ship 时 MUST 包含此 3 文件, 供用户跑通 ≥3 类 ContextRequest 实证 (per rsi §11.8.4 红线).

## 文件清单

| 文件 | task_class | 用途 | entries |
|------|------------|------|---------|
| `code-class-context.jsonl` | `code_gen` | K8s YAML / Python pytest 示例 | 2 |
| `research-class-context.jsonl` | `research` | 论文摘要 / CRDT 对比 示例 | 2 |
| `debug-class-context.jsonl` | `debug` | k8s RBAC 日志 / HTTP 延迟诊断 示例 | 2 |

## Schema (per spec.md R13.1)

每行 1 个 ContextRequest JSON object, 6 顶层字段 + metadata 对象 4 子字段:

```json
{
  "context_id": "<UUID v4>",
  "turn_input": "<non-empty string>",
  "task_class": "code_gen | research | summary | debug | classify | other",
  "expected_eval_quality": "Acceptable | Poor | Excellent | null",
  "invocation_mode": "mock | real_llm_deepseek | real_llm_custom",
  "metadata": {
    "domain": "<string>",
    "tags": ["<string>", ...],
    "is_hidden": <bool, default false>,
    "sensitivity": "public | internal | confidential"
  }
}
```

## 使用方式

```bash
# 用户 clone 后, 跑通 ≥3 类 ContextRequest 实证:
./pdk_chat_demo_evolution --mock --context-file examples/pdk_chat_demo_evolution/fixtures/contexts/code-class-context.jsonl
./pdk_chat_demo_evolution --mock --context-file examples/pdk_chat_demo_evolution/fixtures/contexts/research-class-context.jsonl
./pdk_chat_demo_evolution --mock --context-file examples/pdk_chat_demo_evolution/fixtures/contexts/debug-class-context.jsonl

# 或合并 3 类对比:
cat examples/pdk_chat_demo_evolution/fixtures/contexts/{code,research,debug}-class-context.jsonl > /tmp/3class.jsonl
./pdk_chat_demo_evolution --mock --context-file /tmp/3class.jsonl --accept-contexts --release-metrics
```

## 修改与扩展

- **新增 task_class**: spec.md R13.1 必须先升档 + 扩闭枚举 (`code_gen | research | summary | debug | classify | other | <new>`); 改完后再加 reference fixture
- **修改字段**: 加字段向后兼容 (用户 clone 的旧 fixture 仍可跑); 改/删字段 MUST bump spec version + 提供 migration guide
- **域覆盖扩展**: 在对应类文件下 append 新 entries; 不要拆 sub-domain 到独立文件 (破坏 ≥3 类合并实证)

## 与 test fixtures 的区别

- **`fixtures/contexts/`** (本目录): SHIPPED, 用户可见, 用户 clone + modify 的 reference
- **`tests/fixtures/context_request/`**: 内部, 驱动 ctest binary, **故意**构造 negative case (如 `invalid_*.jsonl` 触发 S29-S30 reject path)

漂移监控: 任何 SHIPPED reference fixture 的 schema 变更 MUST 同步检查 `tests/fixtures/context_request/` 是否仍兼容.