## ADDED Requirements

### Requirement: resource-node-no-circular-include

`src/core/types/resource.h` MUST NOT 直接或间接 `#include "node.h"`，打破 `node.h ↔ resource.h` 循环依赖。`resource.h` 应当仅 include `context.h` + 标准库头文件，前向声明或字符串别名解决其他依赖。

#### Scenario: 单向依赖链
- **WHEN** 编译 `src/core/types/resource.h`
- **THEN** 仅 include `context.h` 和标准库 (`<string>`, `<nlohmann/json.hpp>`)
- **AND** 不 include `node.h`
- **AND** `node.h` 仍可正常使用 `ResourceType` (因为 `node.h` 先 include `resource.h`)

#### Scenario: 头守卫仍生效
- **WHEN** 同一翻译单元同时 include `node.h` 和 `resource.h`
- **THEN** 头守卫防止无限递归
- **AND** 编译器不报 "main file cannot be included recursively" 错误

### Requirement: tool-result-error-enum-only

`ToolResult::error()` MUST 仅暴露接受 `ErrorCode` 枚举的重载。接受 `std::string` 错误码的 `@deprecated` 重载 MUST 被移除。

#### Scenario: 仅 ErrorCode 重载可调用
- **WHEN** 编译 `src/core/types/tool_result.h`
- **THEN** 公开 API 仅有 `static ToolResult error(ErrorCode code, std::string msg, ...)`
- **AND** `static ToolResult error(std::string code, std::string msg, ...)` 已移除
- **AND** 全代码库 (src/ + tests/ + include/) 调用点已迁移到 ErrorCode 重载

#### Scenario: 调用方迁移零残留
- **WHEN** 运行 `grep -rn 'ToolResult::error("' src/ tests/ include/`
- **THEN** 输出 0 行 (无字符串错误码调用残留)

### Requirement: library-loader-pimpl-lite

`src/modules/library/library_loader.h` MUST 使用 PIMPL-lite 模式解耦 `MarkdownParser` 直接依赖。前向声明 `MarkdownParser`，通过 `std::unique_ptr<MarkdownParser>` 间接持有。

#### Scenario: header 不再 include parser
- **WHEN** 编译 `src/modules/library/library_loader.h`
- **THEN** 不再 include `modules/parser/markdown_parser.h`
- **AND** `class MarkdownParser;` 前向声明存在
- **AND** 完整 include 移到 `src/modules/library/library_loader.cpp`

#### Scenario: 析构外置
- **WHEN** 持有 `unique_ptr<MarkdownParser>` 成员
- **THEN** 析构函数 `~LibraryLoader()` 必须 out-of-line 定义 (在 .cpp 中)
- **AND** 头文件中只有 `~LibraryLoader();` 声明

### Requirement: adr-0032-status-approved

`docs/archive/adr/adr-0032-cost-collector.md` 状态 MUST 从 `🟡 Partial` 同步到 `✅ Approved`，反映 `tests/test_cost_collector.cpp` (2026-06-14) 已 ship 的实际状态。

#### Scenario: ADR 状态字段正确
- **WHEN** 阅读 ADR-0032 头部元数据
- **THEN** 状态为 `✅ Approved`
- **AND** 包含 commit 引用: `tests/test_cost_collector.cpp`
- **AND** 包含 ship 日期: `2026-06-14`

#### Scenario: adr_lint 验证通过
- **WHEN** 运行 `python3 tools/adr_lint.py docs/archive/adr/adr-0032-cost-collector.md`
- **THEN** exit 0，无格式错误