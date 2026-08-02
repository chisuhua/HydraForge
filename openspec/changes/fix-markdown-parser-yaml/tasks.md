## 1. fenced yaml 块解析实现

- [ ] 1.1 添加私有 helper：`MarkdownParser::detect_format()` 与 `MarkdownParser::parse_yaml_fenced_block()` (src/modules/parser/markdown_parser.cpp)
- [ ] 1.2 验证：编译 `agenticdsl_modules_parser` 目标无新增 warning
- [ ] 1.3 提交：git commit -m "feat(parser): add fenced yaml block detection helpers"

- [ ] 1.4 使用 `yaml-cpp` 解析含 `# --- BEGIN AgenticDSL ---` 标记的 fenced yaml 块，将 YAML 节点转换为 `std::map<std::string, std::string>`
- [ ] 1.5 验证：运行 `tests/test_markdown_parser.cpp` 现有 case 全部通过
- [ ] 1.6 提交：git commit -m "feat(parser): parse fenced yaml block into metadata map"

- [ ] 1.7 统一键名规范化：trim、转小写、下划线替换连字符，与 bold 解析结果保持一致
- [ ] 1.8 验证：新增单测断言 yaml 键 `Approval-Policy` 被规范化为 `approval_policy`
- [ ] 1.9 提交：git commit -m "refactor(parser): normalize fenced yaml keys"

- [ ] 1.10 将 `YAML::ParserException` / `YAML::BadConversion` 转换为 line-level `ValidationError`
- [ ] 1.11 验证：构造非法 yaml fixture，断言错误信息包含行号
- [ ] 1.12 提交：git commit -m "feat(parser): line-level error reporting for invalid yaml"

## 2. 双格式自动检测

- [ ] 2.1 在 `DslValidator::validate()` 入口实现 fenced yaml 块检测：扫描 ` ```yaml ... ``` ` 块，验证块首行含 `# --- BEGIN AgenticDSL ---` 标记
- [ ] 2.2 验证：新增双格式检测单测，断言含 BEGIN 标记的 yaml 文件选择 yaml 路径；bold 文件选择 bold 路径
- [ ] 2.3 提交：git commit -m "feat(validator): auto-detect fenced yaml vs bold format"

- [ ] 2.4 将 yaml 解析后的 metadata map 接入现有 `validate_schema()` 函数，禁止复制校验规则
- [ ] 2.5 验证：非法 yaml 配置触发与非法 bold 配置相同的 schema 错误
- [ ] 2.6 提交：git commit -m "feat(validator): route yaml metadata to existing schema validation"

- [ ] 2.7 保留 bold 路径原有调用，确保现有 bold fixture 无行为变化
- [ ] 2.8 验证：运行 `tests/test_dsl_validator.cpp` 所有 bold fixture 回归通过
- [ ] 2.9 提交：git commit -m "fix(validator): keep bold format path backward compatible"

## 3. 测试 fixture 迁移/参数化

- [ ] 3.1 审计现有 bold fixture 清单，列出文件路径与校验字段
- [ ] 3.2 验证：`grep -R "\*\*" tests/fixtures/ | wc -l` 确认 bold fixture 数量
- [ ] 3.3 提交：git commit -m "docs(test): audit existing bold DSL fixtures"

- [ ] 3.4 新增 3 个 fenced yaml 格式 fixture（保留字段语义不变）
- [ ] 3.5 验证：运行 DslValidator 对新增 fixture 通过，且与等价的 bold 输出结果一致
- [ ] 3.6 提交：git commit -m "test(fixture): add fenced yaml DSL fixtures for production format coverage"

- [ ] 3.7 保留现有 bold fixture 至少 3 个不变（作为回归网）
- [ ] 3.8 验证：bold 与 yaml 两种格式的 fixture 校验结果完全一致
- [ ] 3.9 提交：git commit -m "test(fixture): keep bold regression fixtures for backward compatibility"

- [ ] 3.10 新增非法 yaml fixture：缺少必需字段，验证 line-level 错误
- [ ] 3.11 验证：运行该 fixture 得到非空 `ValidationError` 且包含字段名
- [ ] 3.12 提交：git commit -m "test(fixture): add invalid yaml fixture for line-level schema errors"

## 4. main.cpp 跳过分支删除

- [ ] 4.1 删除 `examples/pdk_chat_demo/main.cpp` 检测到 YAML 即跳过校验的 `if (is_yaml) { ... return; }` 分支
- [ ] 4.2 验证：`grep -n "跳过\|skip validation" examples/pdk_chat_demo/main.cpp` 返回 0
- [ ] 4.3 提交：git commit -m "fix(demo): remove YAML validation skip branch in pdk_chat_demo"

## 5. 测试与验证

- [ ] 5.1 运行 MarkdownParser fenced yaml 解析新增单测
- [ ] 5.2 验证：ctest -R test_markdown_parser --output-on-failure 全部通过
- [ ] 5.3 提交：git commit -m "test(parser): add fenced yaml block parser tests"

- [ ] 5.4 运行 DslValidator 双格式校验单测
- [ ] 5.5 验证：ctest -R test_dsl_validator --output-on-failure 全部通过
- [ ] 5.6 提交：git commit -m "test(validator): add dual-format detection and schema validation tests"

- [ ] 5.7 运行全量 ctest
- [ ] 5.8 验证：`ctest --output-on-failure` 全部通过（零回归）
- [ ] 5.9 提交：git commit -m "test: full ctest regression for fenced yaml support"

## 6. 文档同步与收尾

- [ ] 6.1 运行 `openspec validate fix-markdown-parser-yaml`
- [ ] 6.2 验证：命令退出码为 0，输出 "valid"
- [ ] 6.3 提交：git commit -m "docs(openspec): validate fix-markdown-parser-yaml change artifacts"

- [ ] 6.4 准备 plan-done handoff，写入 `.rddf/state/.plan-handoff.json`
- [ ] 6.5 验证：handoff JSON 包含 change 名、完成 tasks 数、验证结果
- [ ] 6.6 提交：git commit -m "docs: plan-done handoff for fix-markdown-parser-yaml"