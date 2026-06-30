# Tasks: 修复审计快速债 (Quick Audit Debt Fixes)

## 1. D-3 修复 `node.h ↔ resource.h` 循环 include

- [ ] 1.1 删除 `src/core/types/resource.h:5` 的 `#include "node.h"` 行
- [ ] 1.2 删除 `src/core/types/resource.h:4` 的 `#include "context.h"` 行 (如未使用)
- [ ] 1.3 修正 `src/core/types/resource.h` 顶部注释（"引入 Context/Value" → 删除或重写）
- [ ] 1.4 验证编译：`cmake --build build/tests -j$(nproc)` 0 错误
- [ ] 1.5 验证测试：`cd build/tests && ctest` 48/48 PASS
- [ ] 1.6 验证 LSP 不再报告 "main file cannot be included recursively" 错误

## 2. D-4 移除 `ToolResult::error(string)` `@deprecated` 重载

- [ ] 2.1 删除 `src/core/types/tool_result.h:90-92` 的 `error(std::string, std::string, json)` 声明 + Doxygen `@deprecated` 注释
- [ ] 2.2 删除 `src/core/types/tool_result.cpp` 中对应实现
- [ ] 2.3 迁移 `src/modules/cognitive/simple_orchestrator.cpp:127` `ToolResult::error("ERR_ORCHESTRATOR.PARSE_FAILED", ...)` → `ToolResult::error(ErrorCode::ParseError, ...)` (新增 ErrorCode 枚举值或复用 Unknown)
- [ ] 2.4 迁移 `tests/test_tool_result.cpp:28` + `:55` 两处字符串错误码调用
- [ ] 2.5 验证调用方零残留：`grep -rn 'ToolResult::error("' src/ tests/ include/` 输出 0 行
- [ ] 2.6 验证编译：`cmake --build build/tests -j$(nproc)` 0 错误
- [ ] 2.7 验证测试：`cd build/tests && ctest` 48/48 PASS

## 3. D-7 `library_loader.h` PIMPL-lite 解耦

- [ ] 3.1 `src/modules/library/library_loader.h`: 删除 `#include "modules/parser/markdown_parser.h"`
- [ ] 3.2 `src/modules/library/library_loader.h`: 添加 `class MarkdownParser;` 前向声明
- [ ] 3.3 `src/modules/library/library_loader.h`: 析构函数改为 `~LibraryLoader();` 声明 (out-of-line)
- [ ] 3.4 `src/modules/library/library_loader.cpp`: 添加 `#include "modules/parser/markdown_parser.h"` 完整类型
- [ ] 3.5 `src/modules/library/library_loader.cpp`: 添加析构外置 `LibraryLoader::~LibraryLoader() = default;`
- [ ] 3.6 验证编译：`cmake --build build/tests -j$(nproc)` 0 错误
- [ ] 3.7 验证测试：`cd build/tests && ctest` 48/48 PASS
- [ ] 3.8 验证解耦：`grep -n 'parser' src/modules/library/library_loader.h` 不再有 include 行

## 4. D-11 ADR-0032 状态同步

- [ ] 4.1 编辑 `docs/archive/adr/adr-0032-cost-collector.md`: 头部状态字段 `🟡 Partial` → `✅ Approved`
- [ ] 4.2 添加 commit 引用注释：`tests/test_cost_collector.cpp ship 2026-06-14`
- [ ] 4.3 验证 `python3 tools/adr_lint.py docs/archive/adr/adr-0032-cost-collector.md` exit 0

## 5. 架构合规性 + Ship Gate

- [ ] 5.1 运行 `python3 tools/adr_lint.py` exit 0 (全 ADR 状态正确)
- [ ] 5.2 运行 `python3 tools/docs_drift_audit.py` 0 critical drift
- [ ] 5.3 运行 `cmake --build build/tests -j$(nproc)` 0 错误
- [ ] 5.4 运行 `cd build/tests && ctest` 48/48 PASS
- [ ] 5.5 运行 `lsp_diagnostics` 在 4 个改动文件 (`resource.h`, `tool_result.h`/`.cpp`, `library_loader.h`/`.cpp`, ADR-0032) 无新增错误
- [ ] 5.6 `git status` 检查无意外修改
- [ ] 5.7 `git diff --stat` 范围合理 (预估 ~30 行 +/-)
- [ ] 5.8 按 D-3/D-4/D-7/D-11 分 4 个独立 commit (符合 Sprint 17 模式)
- [ ] 5.9 更新 AGENTS.md 添加 Sprint 18 ship 记录

## 6. 归档

- [ ] 6.1 `openspec validate fix-audit-quick-debt-2026-06 --strict` exit 0
- [ ] 6.2 `openspec archive fix-audit-quick-debt-2026-06 --yes` (4 个 commit 全部 ship 后)