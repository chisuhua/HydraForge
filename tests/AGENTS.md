# tests

**Generated:** 2026-05-11

## OVERVIEW
Catch2 单元测试，15 个测试文件。

## FRAMEWORK
- Catch2 (amalgamated 单文件版)
- 标签格式：`[module][stageN]` (如 `[scheduler][stage2]`)
- 运行：`ctest --output-on-failure`

## FILES (15)
| File | Coverage |
|------|----------|
| test_engine.cpp | DSLEngine |
| test_parser.cpp | MarkdownParser |
| test_scheduler.cpp | TopoScheduler |
| test_executor.cpp | NodeExecutor |
| test_tool_registry.cpp | ToolRegistry |
| test_llm_tool.cpp | LlamaAdapter |
| test_library_loader.cpp | StandardLibraryLoader |
| test_path_resolution.cpp | 路径解析 |
| test_basic.cpp | 基础功能 |
| test_prompt_builder.cpp | Prompt 构建 |
| test_no_llm.cpp | 无 LLM 模式 |
| main_test_runner.cpp | 测试入口 |