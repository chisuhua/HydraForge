# from-roadmap-phase-6b-platform — Tasks

## 1. DslValidator YAML 解析重构

- [x] 1.1 在 `dsl_validator.cpp` 添加 `#include <yaml-cpp/yaml.h>` 和 `#include "common/utils/yaml_json.h"`
- [x] 1.2 删除/废弃 `yaml_field_value()` 函数（D1 + D2 决策）
- [x] 1.3 删除/废弃 `yaml_nodes_json()` 函数（D1 + D3 决策）
- [x] 1.4 实现新私有 `yaml_block_to_json(const std::string& yaml_text, nlohmann::json& out, std::string& error_path)` 函数，使用 `YAML::Load(yaml_text)` + `agenticdsl::yaml_to_json()` 转换
- [x] 1.5 实现新私有 `extract_required_string_field(json, key)` 函数，从 JSON 中按 `contains()` + `is_string()` 提取 frontmatter 必填字段
- [x] 1.6 实现新私有 `extract_nodes_array(json)` 函数，从 JSON 中按 `contains("nodes")` + `is_array()` 提取节点列表
- [x] 1.7 `DslValidator::validate()` 重写 YAML 分支：调用新私有函数，替换原正则/子串路径
- [x] 1.8 捕获 `YAML::ParserException`，提取 `mark.line`/`mark.column` 写入 `ValidationError::path`（D6 决策）

## 2. 错误模型扩展

- [x] 2.1 保留既有 4 类 `ValidationError::type`（MISSING_REQUIRED_FIELD / INVALID_NODE_TYPE / MISSING_TOOL_DEPENDENCY / PARSE_ERROR）
- [x] 2.2 新增 `INVALID_YAML` 错误类型（yaml-cpp 解析失败时使用）
- [x] 2.3 统一 `path` 字段为点分隔格式：`frontmatter.<field>` / `node[N].<field>` / `yaml_block[L:C]`（D4 决策）
- [x] 2.4 移除 `dsl_validator.cpp:227-229` 的 fail-fast `return result;`，改为追加 INVALID_YAML 错误后继续（D5 决策）
- [x] 2.5 在 `nodes` 字段存在但非数组的情况下，追加 `INVALID_NODES_TYPE` 错误（D3 决策）

## 3. 测试覆盖（6 类回归场景）

- [x] 3.1 新建 `examples/pdk_chat_demo/tests/test_dsl_validator_yaml.cpp` Catch2 测试文件
- [x] 3.2 测试 1：合法 YAML 块（含完整 frontmatter + nodes）→ `valid=true` + `errors.empty()`
- [x] 3.3 测试 2：缺 frontmatter 必填字段 → `errors` 含 `MISSING_REQUIRED_FIELD` + `path` 以 `frontmatter.` 开头
- [x] 3.4 测试 3：node 缺 `id` 或 `type` → `errors` 含 `MISSING_REQUIRED_FIELD` + `path` 以 `node[N].` 开头
- [x] 3.5 测试 4：非法节点类型 → `errors` 含 `INVALID_NODE_TYPE` + `path` 为 `node[N].type`
- [x] 3.6 测试 5：`call_tool` 引用未注册工具 + 提供 `MockToolRegistry` → `errors` 含 `MISSING_TOOL_DEPENDENCY`
- [x] 3.7 测试 6：YAML 含非法语法（截断的 mapping）→ `errors` 含 `INVALID_YAML` + `path` 形如 `yaml_block[L:C]`
- [x] 3.8 测试 7：同一合法 `.agent.md` 在 LF 和 CRLF 行尾下产生等价校验结果
- [x] 3.9 测试 8：mock 模式跳过校验的现有行为不回归（5 个 `--mock` 启动 fixture 全部通过）

## 4. 生产 fixture 收集

- [x] 4.1 从 `lib/loop/react.agent.md` 抽取完整 YAML 块（`/__meta__` + `/main`），保存为 `examples/pdk_chat_demo/tests/fixtures/react_golden.agent.md`
- [x] 4.2 从 `lib/loop/plan_execute.agent.md` 抽取，保存为 `plan_execute_golden.agent.md`
- [x] 4.3 从 `lib/loop/fork_join.agent.md` 抽取，保存为 `fork_join_golden.agent.md`
- [x] 4.4 为 3 个 golden fixture 各生成一个 CRLF 版本（行尾转换），保存为 `<name>_crlf.agent.md`
- [x] 4.5 1 个非法 fixture：`examples/pdk_chat_demo/tests/fixtures/invalid_yaml.agent.md`（含截断 mapping）

## 5. CMake / 构建系统

- [x] 5.1 `examples/pdk_chat_demo/CMakeLists.txt` 添加 yaml-cpp 链接依赖到 `test_dsl_validator_yaml` 目标
- [x] 5.2 `examples/pdk_chat_demo/CMakeLists.txt` 注册 `test_dsl_validator_yaml` 到 ctest（`add_test`）
- [x] 5.3 确认 `agenticdsl::yaml_to_json` 在 `agenticdsl_core` 中已暴露（通过 `target_link_libraries` 间接可见）

## 6. 回归验证 + 文档

- [x] 6.1 跑 `cmake --preset tests` 重新生成构建系统，0 错误
- [x] 6.2 跑 `ctest -R 'pdk_chat|dsl_validation' --output-on-failure`，新增 8 个测试全部 PASS + 既有 0 回归
- [x] 6.3 跑 `ctest --output-on-failure` 全量，0 回归（ground truth: 147/147 PASS）
- [x] 6.4 跑 `lsp_diagnostics` 对 `examples/pdk_chat_demo/dsl_validator.{h,cpp}` 0 错误
- [x] 6.5 `examples/pdk_chat_demo/README.md`（如有）追加 1 段 "YAML DSL 校验链" 说明
- [x] 6.6 git commit（单一聚合 commit，commit message: `feat(pdk-chat-demo): YAML DSL validator — yaml-cpp structured parsing`）

## 7. 架构合规性验证

- [x] 7.1 确认 `src/modules/parser/markdown_parser.{h,cpp}` 无改动
- [x] 7.2 确认 `src/modules/parser/` 目录 0 新文件
- [x] 7.3 确认改动仅在 `examples/pdk_chat_demo/dsl_validator.{h,cpp}` + `tests/`
- [x] 7.4 ADR-0058 边界检查：tool input/output schema 校验不在本提案改动范围内