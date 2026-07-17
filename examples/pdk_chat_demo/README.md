# PDK Chat Demo

> **Agent-as-Plugin 架构端到端验证 demo**
> 关联设计: [DESIGN.md](./DESIGN.md)
> 关联 ADR: ADR-0052 ~ ADR-0065

## 概述

`pdk_chat_demo` 演示 HydraForge "AgenticOS" 范式:

- **应用 = Agent 组合** (无业务代码)
- **万物皆 Agent，Agent 皆 Plugin** (6 个独立 Plugin)
- **6 个 Agent 协作**: Chat / Loop / Provider / Session / Budget / Code Review

## 编译

```bash
# 在项目根目录
mkdir build && cd build
cmake .. -DAGENTICDSL_BUILD_PDK_AGENTS=ON -DPDK_CHAT_BUILD_TESTS=ON
make -j$(nproc) pdk_chat_demo
```

## 运行

### Mock 模式（CI 验证）

```bash
./build/examples/pdk_chat_demo/pdk_chat_demo --mock
```

输入测试用例:
```
User> Write a hello world in C++
```

### 真实 LLM 模式

```bash
export OPENAI_API_KEY=sk-...
export ANTHROPIC_API_KEY=sk-ant-...
./build/examples/pdk_chat_demo/pdk_chat_demo
```

默认使用 mock provider，可在 `config.json` 中切换。

## Plugin 构成

| Plugin | 形态 | 路径 |
|--------|------|------|
| Loop Agent | DSL (.agent.md) | `lib/loop/react.agent.md` |
| Provider Agent | C++ (.so) | `pdk/provider_agent/` |
| Session Agent | C++ (.so) | `pdk/session_agent/` |
| Budget Agent | C++ (.so) | `pdk/budget_agent/` |
| FS Tools | C++ (.so) | `pdk/fs_tools/` |
| Shell Tools | C++ (.so) | `pdk/shell_tools/` |
| Code Review Skill | SKILL.md | `skills/code-review/SKILL.md` |

## 事件流示例

```
$ ./pdk_chat_demo --mock
[10:23:45] user.input: "Write a hello world in C++"
[10:23:45] loop.turn.start: turn=1, step=1
[10:23:45] llm.request: model=mock-llm-v1
[10:23:46] llm.response: tokens=85, duration=210ms
[10:23:46] loop.decision: tool_call (shell/exec)
[10:23:46] tool.execution.start: shell/exec
[10:23:47] tool.execution.end: ok=true, duration=890ms
[10:23:47] loop.done: total_steps=2, total_tokens=127
Assistant: Here's the C++ code...
```

## 测试

```bash
cd build
ctest -R pdk_chat --output-on-failure
```

测试用例:
- `test_chat_session`: 单元测试（ChatSession 状态机）
- `test_e2e_mock`: 端到端 mock 模式测试

## 设计文档

完整设计见 [DESIGN.md](./DESIGN.md)，包括:
- 完整架构图
- JSON 配置 schema
- 6 个 Agent 详细设计
- 事件流 + 错误处理
- 测试策略
- 15 项验证清单

## 相关文档

- `docs/architecture/agent-as-plugin-architecture-v1.1.md` - 总架构
- `docs/architecture/agent-evolution-pipeline.md` - 4 阶段管线
- `docs/adr/adr-0052-agent-plugin-manifest.md` - manifest 规范
- `docs/adr/adr-0053-agent-descriptor-interface.md` - AgentDescriptor
- `docs/adr/adr-0054-capability-discovery.md` - Capability 索引
- `docs/adr/adr-0055-skill-isolation.md` - SKILL 隔离
- `docs/adr/adr-0057-agent-lifecycle.md` - Plugin 生命周期
- `docs/adr/adr-0058-tool-schema-validation.md` - Schema 校验
- `docs/adr/adr-0060-agent-composition.md` - 6 种协作模式
- `docs/adr/adr-0061-agent-evolution-and-solidification.md` - Skill 进化