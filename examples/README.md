# examples/ — 示例与参考目录

> 8 个条目：6 个编译运行示例 + 2 个参考文档

## C++ 编译运行示例

通过 `-DAGENTICDSL_BUILD_EXAMPLES=ON` 启用构建。

| 目录 | 用途 | Mock 模式 | 关键特性 |
|------|------|:---:|------|
| `agent_basic/` | 主示例：加载 `.agent.md` 工作流 | ✅ `--mock` | DSL 解析 + 执行 |
| `agent_simple/` | 简化示例：MockLLMProvider 单轮 ReAct | ✅ 默认 | Sprint 19 migration |
| `agent_loop/` | 循环示例：MockLLMProvider 多轮 | ✅ 默认 | Sprint 19 migration |
| `slice_01_tool_call/` | 端到端：SimpleCognitiveOrchestrator + 工具调用 | ✅ `--mock` | Track 0.2 三层调用链 |
| `phase1_model_router_plugin/` | Model Router 演示：4 个 .so 加载 + 3 种路由策略 | ✅ `--mock` | C7 ADR-0034, PluginLoader |
| `phase1_plugin_demo/` | PluginLoader 验证：3 modes (mock/list/plugin) | ✅ `--mock` | Sprint 5, PluginInfo + dlopen |

## 参考文档

非构建目标，用于文档和体系对标。

| 目录 | 类型 | 用途 |
|------|------|------|
| `skill_porting/` | `.md` DSL 定义 | Skill 分类体系 (5 轴 × 39 技能) 与 AgenticDSL 实现对照 |
| `superpowers/` | `.agent.md` 工作流文件 | 12 个 Superpowers 技能的 AgenticDSL 重写对标 |

## 构建

```bash
mkdir build && cd build
cmake .. -DAGENTICDSL_BUILD_EXAMPLES=ON
make -j$(nproc)

# 运行
./examples/agent_basic/agent_basic lib/auth/login.md --mock
./examples/slice_01_tool_call/slice_01_tool_call --mock
```