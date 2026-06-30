## ADDED Requirements

### Requirement: agent-simple-compilable

`examples/agent_simple/simple.cpp` MUST 可编译并使用 MockLLMProvider 模式 (不依赖真实模型权重)。CMakeLists.txt MUST 存在并链接 `agenticdsl_core` + `agenticdsl_modules_cognitive`。

#### Scenario: 编译成功
- **WHEN** 运行 `cmake --preset debug -DAGENTICDSL_BUILD_EXAMPLES=ON && cmake --build build --target agent_simple`
- **THEN** 编译 exit 0
- **AND** `build/agent_simple` 二进制存在

#### Scenario: 启动运行
- **WHEN** 运行 `./build/agent_simple < examples/agent_simple/initial.md`
- **THEN** exit 0，输出包含 `[INFO] Engine ready` 或类似状态信息
- **AND** 零 LLM 真实调用 (MockLLMProvider 拦截)

#### Scenario: 无外部依赖
- **WHEN** 编译 `simple.cpp`
- **THEN** 不引用 `agenticdsl::LlamaAdapter` (已废弃)
- **AND** 不引用已删除的 `common/utils.h` (实际在 `common/utils/parser_utils.h`)

### Requirement: agent-loop-compilable

`examples/agent_loop/agent_loop.cpp` MUST 可编译并使用 MockLLMProvider 模式。CMakeLists.txt MUST 存在并链接必要模块。

#### Scenario: 编译成功
- **WHEN** 运行 `cmake --preset debug -DAGENTICDSL_BUILD_EXAMPLES=ON && cmake --build build --target agent_loop`
- **THEN** 编译 exit 0
- **AND** `build/agent_loop` 二进制存在

#### Scenario: 不引用已删除 API
- **WHEN** 编译 `agent_loop.cpp`
- **THEN** 不引用 `agenticdsl::PromptBuilder` (已删除)
- **AND** 不调用 `DSLEngine::get_llm_adapter()` (已删除，改 `get_llm_provider()`)

#### Scenario: 启动运行
- **WHEN** 运行 `./build/agent_loop`
- **THEN** exit 0
- **AND** 多轮循环演示可见 (info log 表明循环进度)

### Requirement: examples-build-target

根 `CMakeLists.txt` MUST 添加 `AGENTICDSL_BUILD_EXAMPLES` 选项 (默认 OFF)，启用时构建所有 examples/ 子目录。

#### Scenario: Examples 默认不构建
- **WHEN** 运行 `cmake --preset debug` (不带 `-DAGENTICDSL_BUILD_EXAMPLES=ON`)
- **THEN** 不构建 examples 二进制
- **AND** 不影响主 ctest 49/49 PASS

#### Scenario: Examples opt-in 构建
- **WHEN** 运行 `cmake --preset debug -DAGENTICDSL_BUILD_EXAMPLES=ON && cmake --build build`
- **THEN** 5+ 个 example 二进制构建 (agent_basic + agent_simple + agent_loop + slice_01_tool_call + phase1_*)
- **AND** 编译 exit 0

### Requirement: agents-md-debt-removed

`AGENTS.md` line 46 审计债注释 MUST 被移除 (since D-2 now resolved)。`examples/` 描述表 MUST 更新反映新启用的 2 个 examples。

#### Scenario: 审计债注释移除
- **WHEN** 阅读 `AGENTS.md` line 40-50
- **THEN** 不再包含 "examples/agent_simple/ 和 examples/agent_loop/ 的 DEPRECATED 注释基于错误删除假设撰写" 段落
- **AND** examples 表中 `agent_simple` 和 `agent_loop` 标记为 ✅ Active

#### Scenario: AGENTS.md adr_lint 通过
- **WHEN** 运行 `python3 tools/adr_lint.py`
- **THEN** exit 0 (如果存在该工具) 或手动验证 markdown 格式正确