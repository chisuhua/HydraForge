# G1 Coding Assistant Plugin (`pdk/g1_coding_assistant/`)

> **Phase 6 W1** | **STATUS**: 🟡 MVP 实施中
> **关联 OpenSpec**: `openspec/changes/phase6-service-ification-v1/`
> **关联决策**: `docs/adr/adr-0051-phase6-pdk-composition-spike.md` (Decision 3/4/5)

## 插件概述

**G1 Coding Assistant Plugin** 是 HydraForge 的首个 orchestrator PDK plugin，通过 2-step ReAct 循环编排 G3 Knowledge Base 的知识检索 + LLM 合成，实现代码审查功能。使用 `DEFINE_AGENT(CodingAssistant, AgentLoopType::React)` 2 参数宏 (Sprint 20)。

### 注册工具清单 (1 个)

| 命名空间 | 工具 | 类型 | 审批策略 |
|---------|------|------|---------|
| `coding_assistant/` | `review` | Execute | agent + plan (yolo 不审) |

### 功能

- 接收 `{request, code, session_id?}` 参数
- Step 1: 调用 G3 `knowledge_base/query` 工具检索相关知识
- Step 2: 使用 MockLLMProvider 合成代码审查评论
- Tool manifest: 发现并声明对 `knowledge_base/query` 的依赖
- DEFINE_AGENT 宏: 仅用于 spec 合规 (实际编排走 handler 函数)

## 文件结构

```
pdk/g1_coding_assistant/
├── CMakeLists.txt
├── README.md
└── src/
    ├── g1_entry.cpp      # 入口 (pdk_register_tools + pdk_plugin_info)
    ├── g1_agent.cpp      # DEFINE_AGENT + 2-step handler + manifest
    └── g1_state.h        # G1State (manifest + registry pointer)
```

## 构建

```bash
cmake --preset debug
cmake --build build/debug --target hydraforge_g1_coding_assistant -j$(nproc)
# 产物: build/debug/pdk/g1_coding_assistant/libhydraforge_g1_coding_assistant.so
```