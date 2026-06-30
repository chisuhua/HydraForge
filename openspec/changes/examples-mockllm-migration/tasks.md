# Tasks: 迁移 examples 到 MockLLMProvider

## 1. 重新启用 `examples/agent_simple/`

- [x] 1.1 创建 `examples/agent_simple/CMakeLists.txt` (参考 `examples/slice_01_tool_call/CMakeLists.txt` 模板)
- [x] 1.2 重写 `examples/agent_simple/simple.cpp` 头部注释 (移除 ACTUAL STATE NOTE 段)
- [x] 1.3 替换 `agenticdsl::LlamaAdapter` 为 `MockLLMProvider` 模式
- [x] 1.4 修复 `#include "common/utils.h"` → `#include "common/utils/parser_utils.h"`
- [x] 1.5 修复 `from_markdown(content, context)` 双参数调用为 `from_markdown(content)` 单参数
- [x] 1.6 验证编译: `cmake --preset debug -DAGENTICDSL_BUILD_EXAMPLES=ON && cmake --build build --target agent_simple` 0 错误
- [x] 1.7 验证运行: `./build/agent_simple < examples/agent_simple/initial.md` exit 0

## 2. 重新启用 `examples/agent_loop/`

- [x] 2.1 创建 `examples/agent_loop/CMakeLists.txt`
- [x] 2.2 重写 `examples/agent_loop/agent_loop.cpp` 头部注释 (移除 ACTUAL STATE NOTE 段)
- [x] 2.3 实现新 `build_prompt()` helper (替代已删除的 `PromptBuilder::inject_libraries_into_prompt`)
- [x] 2.4 替换 `engine->get_llm_adapter()->generate(prompt)` 为 `engine.get_llm_provider()->generate({prompt, params})`
- [x] 2.5 验证编译: `cmake --build build --target agent_loop` 0 错误
- [x] 2.6 验证运行: `./build/agent_loop` exit 0

## 3. CMake 集成

- [x] 3.1 在根 `CMakeLists.txt` 添加 `option(AGENTICDSL_BUILD_EXAMPLES "Build examples" OFF)`
- [x] 3.2 在根 `CMakeLists.txt` 添加 `if(AGENTICDSL_BUILD_EXAMPLES) add_subdirectory(examples/agent_simple) ... endif()`
- [x] 3.3 验证默认配置 (无 flag) 仍 48/48 ctest PASS
- [x] 3.4 验证 opt-in 配置 (`-DAGENTICDSL_BUILD_EXAMPLES=ON`) 5+ examples 编译成功

## 4. AGENTS.md 同步

- [x] 4.1 移除 `AGENTS.md` line 46 的审计债注释段
- [x] 4.2 更新 `examples/` 描述表 (line 47-67) 标记 agent_simple + agent_loop 为 ✅ Active
- [x] 4.3 验证 `python3 tools/adr_lint.py` exit 0 (如存在)
- [x] 4.4 验证 markdown 格式正确 (无断链)

## 5. 架构合规性 + Ship Gate

- [x] 5.1 运行 `cmake --build build -j$(nproc)` 0 错误
- [x] 5.2 运行 `cd build/tests && ctest` 48/48 PASS
- [x] 5.3 运行 `lsp_diagnostics` 在重写文件 (simple.cpp + agent_loop.cpp) 无新增错误
- [x] 5.4 `git diff --stat` 显示合理修改范围 (~150 行 +/-)
- [x] 5.5 按 3 个步骤分 3 个 commit (simple + agent_loop + AGENTS)
- [x] 5.6 更新 AGENTS.md 添加 Sprint 19 ship 记录

## 6. 归档

- [x] 6.1 `openspec validate examples-mockllm-migration --strict` exit 0
- [x] 6.2 `openspec archive examples-mockllm-migration --yes` (3 commit 全部 ship 后)