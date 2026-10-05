## ADDED Requirements

### Requirement: DSL llm_call 节点空响应 fail-fast 契约扩展

`llm_call` 节点的 `output_keys` 写入的 context 字段必须 (SHALL) 非空且非空白, 覆盖 3 类空响应场景: `null` JSON 值 / whitespace-only 字符串 (`" "` / `"\n"` / `"\t"` / 全空白字符) / empty string (`""`)。当检测到任何一类空响应时, `NodeExecutor` 必须 (SHALL) 立即抛出 `std::runtime_error`, 错误信息含节点路径 + output_key + 诊断提示。

#### Scenario: null response 抛错

- **WHEN** ProviderLLMTool 返回成功结果但 `result["text"]` 为 `null` JSON 值
- **THEN** `node_executor.cpp` main path (L204-214) 检测 `is_null()`, 抛 `std::runtime_error` 含 "LLM call returned null/empty/whitespace response for node 'think' output_key 'llm_response'"
- **NOTE**: 节点路径示例为 `lib/loop/react.agent.md` 中的 `think` 节点, output_key 示例为 `llm_response`. 真实路径根据调用栈上下文动态确定.

#### Scenario: whitespace-only response 抛错

- **WHEN** ProviderLLMTool 返回成功结果但 `result["text"]` 为 whitespace-only 字符串 (`" "` / `"\n"` / `"\t"` / 全空白)
- **THEN** `node_executor.cpp` main path 检测 `is_string() && all_of(isspace)`, 抛 `std::runtime_error` 含 "LLM call returned null/empty/whitespace response"

#### Scenario: empty string response 抛错 (F1 regression guard)

- **WHEN** ProviderLLMTool 返回成功结果但 `result["text"]` 为 `""` empty string
- **THEN** `node_executor.cpp` main path 检测 `is_string() && empty()`, 抛 `std::runtime_error` (F1 现有行为保留, regression guard)

#### Scenario: streaming 路径同契约

- **WHEN** `llm_call` 节点走 streaming 路径 (line 134-167)
- **THEN** stream path (L147-156) 应用同样的 null / whitespace / empty 检查, 与 non-streaming 行为一致

### Requirement: ProviderLLMTool 防御纵深契约

`ProviderLLMTool::generate()` 在 `provider_.generate()` 返回 success 后, 必须 (SHALL) 校验 `out.text` 非空且非空白, 检测到空响应时抛 `std::runtime_error` 含 provider name + model name + 诊断提示。

#### Scenario: empty text 抛错 (现有行为保留)

- **WHEN** `provider_.generate()` 返回成功且 `out.text == ""`
- **THEN** ProviderLLMTool 抛 `std::runtime_error` 含 "ProviderLLMTool: LLM returned empty/whitespace text"

#### Scenario: whitespace-only text 抛错 (V2 新增)

- **WHEN** `provider_.generate()` 返回成功且 `out.text` 全为空白字符 (`" "` / `"\n"` / `"\t"` 等)
- **THEN** ProviderLLMTool 检测 `all_of(isspace)`, 抛 `std::runtime_error` 含 provider name + model name

#### Scenario: 合法 falsy 值不误伤

- **WHEN** `provider_.generate()` 返回成功且 `out.text == "0"` 或 `out.text == "false"` 或 `out.text == "0.0"`
- **THEN** ProviderLLMTool 不抛错（这些是合法字符串值, 非空响应）

### Requirement: test_e2e_real_llm 直接运行可移植性契约

`examples/pdk_chat_demo/tests/test_e2e_real_llm.cpp` 必须 (SHALL) 在 `main()` 入口前注入 `HYDRAFORGE_PLUGIN_PATH` 环境变量, 默认值 `${PROJECT_BINARY_DIR}/pdk` (硬编码 `/workspace/project/HydraForge/build/pdk`), 当 env var 未设置时自动注入, 当 env var 已设置时不覆盖。

#### Scenario: 直接运行 binary 不撞白名单墙

- **WHEN** 开发者本地直接运行 `./build/examples/pdk_chat_demo/tests/test_e2e_real_llm`（无 ctest env injection）
- **THEN** `PluginPathSetter` 自动 setenv `HYDRAFORGE_PLUGIN_PATH`, PluginLoader 白名单允许加载 `libLoopAgent.so`, 测试正常进入 LLM 调用阶段

#### Scenario: ctest 模式 env injection 不被覆盖

- **WHEN** ctest 通过 CMake `set_tests_properties(... ENVIRONMENT ...)` 注入 `HYDRAFORGE_PLUGIN_PATH`
- **THEN** `PluginPathSetter` 检测到 env var 已设置, 不覆盖 (test 优先于 default)

### Requirement: regression guard 测试契约

`tests/test_node_executor_empty_response.cpp` 必须 (SHALL) 提供 3 个独立 test cases, 通过 Mock 注入空响应场景, 验证 fail-fast 行为:
- Case 1: null response
- Case 2: whitespace-only response
- Case 3: empty string response (F1 regression guard)

**断言强度分层**（per AGENTS.md 模式 #3 — TDD RED 阶段不能假 GREEN）:
- 必须断言异常消息含 substring `"LLM call returned null/empty/whitespace response"`，而非仅 `REQUIRE_THROWS`
- **理由**: Case 1 RED 阶段 `result["text"] = nullptr` 在 `get<std::string>()` 即抛 `nlohmann::type_error.302`（不是 silent）→ 仅 `REQUIRE_THROWS` 会假 GREEN

#### Scenario: Case 1 null response fail-fast

- **WHEN** Mock ProviderLLMTool 返回 `result["text"] = nullptr`
- **AND** DSL 执行 `llm_call` 节点
- **THEN** NodeExecutor 抛 `std::runtime_error` **消息含 substring** "LLM call returned null/empty/whitespace response for node 'think' output_key 'llm_response'"

#### Scenario: Case 2 whitespace-only response fail-fast

- **WHEN** Mock ProviderLLMTool 返回 `result["text"] = "   "` 或 `"\n\t"`
- **AND** DSL 执行 `llm_call` 节点
- **THEN** NodeExecutor 抛 `std::runtime_error` **消息含 substring** "LLM call returned null/empty/whitespace response"

#### Scenario: Case 3 empty string fail-fast (regression guard)

- **WHEN** Mock ProviderLLMTool 返回 `result["text"] = ""`
- **AND** DSL 执行 `llm_call` 节点
- **THEN** NodeExecutor 抛 `std::runtime_error` (F1 行为保留, regression guard)

### Requirement: test_e2e_real_llm 直接运行可移植性契约 (动态查找)

`examples/pdk_chat_demo/tests/test_e2e_real_llm.cpp` 必须 (SHALL) 在 `main()` 入口前注入 `HYDRAFORGE_PLUGIN_PATH` 环境变量, 当 env var 未设置时通过 `find_plugin_dir()` 动态查找 build/pdk 路径并 setenv, 当 env var 已设置时不覆盖。

#### Scenario: 直接运行 binary 不撞白名单墙

- **WHEN** 开发者本地直接运行 `./build/examples/pdk_chat_demo/tests/test_e2e_real_llm`（无 ctest env injection）
- **THEN** `find_plugin_dir()` 从 `fs::current_path()` 向上搜索 `build/pdk` 目录（复用既有 `find_loop_dir()` 模式）, `PluginPathSetter` 自动 setenv `HYDRAFORGE_PLUGIN_PATH`, PluginLoader 白名单允许加载 `libLoopAgent.so`, 测试正常进入 LLM 调用阶段

#### Scenario: ctest 模式 env injection 不被覆盖

- **WHEN** ctest 通过 CMake `set_tests_properties(... ENVIRONMENT ...)` 注入 `HYDRAFORGE_PLUGIN_PATH` (CMakeLists.txt:349)
- **THEN** `PluginPathSetter` 检测到 env var 已设置, 不覆盖 (ctest 优先于 default)

#### Scenario: 跨环境可移植 (clone 到非默认 build dir)

- **WHEN** 开发者将项目 clone 到 `/home/user/work/HydraForge/`（非 `/workspace/project/HydraForge/`）
- **AND** 直接运行 `./build/examples/pdk_chat_demo/tests/test_e2e_real_llm`
- **THEN** `find_plugin_dir()` 仍能从 `fs::current_path()` 向上找到 `/home/user/work/HydraForge/build/pdk`, 测试正常加载
- **NOTE**: 不依赖任何 hard-coded 绝对路径, 跨环境可移植

### Requirement: react.agent.md decide 节点契约 (R3 amendment)

`lib/loop/react.agent.md` 的 `decide` 节点 `args: response: "{{llm_response}}"` 必须 (SHALL) 保证 `llm_response` 在 `loop/decide_react` 工具的 `parse_react_decision(response)` 输入前为非空字符串。V2 扩展将 F1 R3 "empty string" 检查升级为 "null / whitespace / empty string" 3 类空响应 fail-fast。

#### Scenario: think 节点 3 类空响应均显式失败

- **WHEN** LLM 返回 null / whitespace-only / empty string text（e.g., reasoning 模型 content 在 reasoning_content, 或 model 遮蔽返回 200 但 success=true + 空 body）
- **THEN** think 节点检测 3 类空响应, **NodeExecutor 抛 `std::runtime_error`** 含 "LLM call returned null/empty/whitespace response for node '...' output_key '...'"
- **AND** 错误沿 scheduler → engine run → DSL CALL 路径传播, 调用方捕获
- **NOTE**: 不引入新 ErrorCode (per AGENTS.md 模式 #1 minimal fix); 不发射 `loop.error` event

## MODIFIED Requirements

### Requirement: react-agent-llm-ctx-bridge R3 (decide 节点 fail-fast 契约升级)

`lib/loop/react.agent.md` 的 `decide` 节点 `args: response: "{{llm_response}}"` MUST 保证 `llm_response` 在 `loop/decide_react` 工具的 `parse_react_decision(response)` 输入前为非空字符串。V2 扩展将 F1 R3 (F1 ship 2026-09-18, commit a96842e) "empty string" 检查升级为 "null / whitespace / empty string" 3 类空响应 fail-fast。

#### Scenario: think 节点 3 类空响应均显式失败

- **WHEN** LLM 返回 null / whitespace-only / empty string text（e.g., reasoning 模型 content 在 reasoning_content, 或 model 遮蔽返回 200 但 success=true + 空 body）
- **THEN** think 节点检测 3 类空响应, **NodeExecutor 抛 `std::runtime_error`** 含 "LLM call returned null/empty/whitespace response for node '...' output_key '...'"
- **AND** 错误沿 scheduler → engine run → DSL CALL 路径传播, 调用方捕获
- **NOTE**: 不引入新 ErrorCode (per AGENTS.md 模式 #1 minimal fix); 不发射 `loop.error` event