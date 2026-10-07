# l2-evolution-real-llm-provider

## Why

L2 reference example (`examples/pdk_chat_demo_evolution/`) 暴露 `--real-llm <provider>` 标志,但实施层 `evolution_session.cpp` 是 stub — ctor 接受 `opts.real_llm` 字符串但不使用,无 provider factory / API key 注入 / HTTP call。所有 L2 测试都是 mock 模式 (9/9 l2-evolution PASS)。

后果:
- R8 反向指标门 + R8.1 红线 (`drop_ratio ≤ 5%`) 当前仅 mock 验证,生产红线数据空缺
- R3 元指标实证 (≥ 3 类 ContextRequest) 在真实 LLM 下未验证
- L2 无法作为 autonomous evaluator 在 Wave 3 Phase 2 D4 训练数据回流阶段使用

Per AGENTS.md §Reverse Indicator Rule + §SINGLE-DEVELOPER MODE + Pattern #10 hygiene (deferred work tracking),本 change 立项 `l2-evolution-real-llm-provider` 真实接线 `--real-llm` 模式,补 L2 reference example 真实 LLM 闭环。

## What Changes

- **修改 LLM provider 接线** (`examples/pdk_chat_demo_evolution/src/evolution_session.cpp`): ctor 接受 `real_llm` provider 名 → 调 `agenticdsl::LLMProviderFactory::create(provider, api_key_env)` 注入 → 替换 `mock_provider_` 默认行为
- **新增真实 LLM 端到端测试 binary** (`examples/pdk_chat_demo_evolution/tests/test_l2_real_llm_e2e.cpp`): must_realllm + l2-evolution 双标签,跑 6-phase demo 在 `--real-llm deepseek` 模式下
- **复用现有 helper** (`tests/test_helpers/real_llm_env.h`): 与 must_realllm 测试共享 `require_real_llm_env()` + `real_llm_config()`
- **新增 `--metrics-output <path>` 强制要求** (real LLM 模式): 真实 LLM drop_ratio 必须写入文件 (避免 CWD pollution)
- **新增 R8.1 红线生产验证**: 真实 LLM 跑 drop_ratio > 5% 触发 exit non-zero (当前 mock 验证无意义)
- **不动 mock 模式** (`--mock` 行为保持)

## Capabilities

### New Capabilities

无新增 capability。

### Modified Capabilities

- `pdk-chat-demo-evolution`: 修改 `standalone-binary-and-dual-mode` requirement + 新增 `real-llm-provider-construction` requirement (delta spec at `specs/pdk-chat-demo-evolution/spec.md`)

## Impact

- **代码影响**:
  - `examples/pdk_chat_demo_evolution/src/evolution_session.cpp` (主修改, ~30 行)
  - `examples/pdk_chat_demo_evolution/tests/test_l2_real_llm_e2e.cpp` (新增, ~150 行)
  - `examples/pdk_chat_demo_evolution/tests/CMakeLists.txt` (新增 binary 注册)
  - `examples/pdk_chat_demo_evolution/main.cpp` (real LLM 模式 fail-fast 验证)
- **依赖**:
  - `src/common/llm/llm_provider_factory.h` + `.cpp` (现有, 必须暴露 `create(provider, api_key_env)` API)
  - `tests/test_helpers/real_llm_env.h` (现有, 与 must_realllm 共享)
  - `agenticdsl::chat_session` (现有 11-param ctor)
- **Token 成本**: ~50-100 LLM calls per CI run (real LLM mode 6-phase demo)
- **现有测试**: 9/9 l2-evolution mock 测试零回归 (mock 路径不变)
