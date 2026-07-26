## Why

Phase 6a roadmap 要求 `pdk_chat_demo` v1 收尾。当前状态：
- pdk_chat_demo 已具备基础 Chat 功能（Agent-as-Plugin + SkillInterpreter + DeepSeek LLM）
- 存在已知问题：Session 持久化未验证 / Budget 告警未修复 / 缺少 Schema 校验
- `ctest -R pdk_chat` 未全绿

## What Changes

### T1: Session 持久化验证 + Budget 告警修复 (4h)
- 验证 `./pdk_chat_demo --mock` 模式下 Session 跨轮复用正确
- 修复 Budget 超限后的告警信息格式（当前可能静默失败）
- Session 持久化到文件并在下次启动恢复

### T2: Schema 校验基础版 (4h)
- 新增 `.agent.md` 输入格式校验（拒绝错误格式的 DSL）
- 校验项：必填字段缺失 / 节点类型不合法 / 循环依赖检测
- 拒绝后返回明确错误信息（不静默跳过）
- 新增 test case：`test_schema_validation.cpp`（≥3 场景）

## Capabilities

- `pdk-chat-demo-v1`: pdk_chat_demo v1 收尾（Session/Budget/Schema）

## Impact

- `examples/pdk_chat_demo/`：Bug 修复 + Schema 校验新增
- `examples/pdk_chat_demo/tests/`：新增 `test_schema_validation.cpp`
- 不影响 HydraForge 核心代码

## Non-Goals

- 不新增 Chat 功能特性
- 不修改 SkillInterpreter 核心逻辑
- 不引入新的 LLM 后端