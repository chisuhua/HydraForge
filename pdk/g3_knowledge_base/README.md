# G3 Knowledge Base Plugin (`pdk/g3_knowledge_base/`)

> **Phase 6 W1** | **STATUS**: 🟡 MVP 实施中
> **关联 OpenSpec**: `openspec/changes/phase6-service-ification-v1/`
> **关联决策**: `docs/adr/adr-0051-phase6-pdk-composition-spike.md` (Decision 3/4/5)

## 插件概述

**G3 Knowledge Base Plugin** 是 HydraForge 的第三个 PDK plugin，提供知识库检索 + LLM 问答服务。
遵循 ADR-0051 §Decision 3: 使用 `register_tool_function()` 而非 `DECLARE_TOOL`。

### 注册工具清单 (1 个)

| 命名空间 | 工具 | 类型 | 审批策略 |
|---------|------|------|---------|
| `knowledge_base/` | `query` | Execute | agent + plan (yolo 不审) |

### 功能

- 接收 `{question, session_id}` 参数
- 硬编码 3-5 条知识片段检索
- 内部调用 MockLLMProvider 生成答案
- 多轮会话支持（SessionStore 保存历史）
- 返回 `{success, answer?, error?}` 错误 schema

## 文件结构

```
pdk/g3_knowledge_base/
├── CMakeLists.txt
├── README.md
└── src/
    ├── g3_entry.cpp      # 入口 (pdk_register_tools + pdk_plugin_info)
    ├── g3_query.cpp      # knowledge_base/query 工具处理 + MockLLM
    └── g3_state.h        # SessionStore (std::shared_mutex 保护)
```

## 构建

```bash
cmake --preset debug
cmake --build build/debug --target hydraforge_g3_knowledge_base -j$(nproc)
# 产物: build/debug/pdk/g3_knowledge_base/libhydraforge_g3_knowledge_base.so
```