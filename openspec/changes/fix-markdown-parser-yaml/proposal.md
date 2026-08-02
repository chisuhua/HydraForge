## Why

`DslValidator` 的元数据解析路径在生产路径上**实际失效**。`lib/loop/*.agent.md` 等生产文件使用 markdown 标题 + fenced yaml 代码块格式（首行 `### AgenticDSL \`/path\``，紧跟 ` ```yaml ... ``` ` 块包含 `# --- BEGIN AgenticDSL ---` 标记），但当前 `examples/pdk_chat_demo/main.cpp` 的检测逻辑（`markdown_content.find("**") == std::string::npos`）将无 `**` 标记的文件识别为 YAML 后**直接跳过 schema 校验**，仅打印 stderr 警告。这导致 pdk_chat_demo T2 的 DSL Schema 校验在生产路径上**形同虚设**。

现有回归网覆盖的是 bold 格式（`**key**: value`），与生产真实格式脱节，无法阻止非法配置进入主分支。缺失能力分析定性为 0.5 Sprint 轻量级债务修复，不另开 ADR。

本 change 在不扩展 DSL 语法的前提下，让 `MarkdownParser`/`DslValidator` 自动识别并解析 fenced yaml 代码块（包含 `# --- BEGIN AgenticDSL ---` 标记），复用既有 schema 校验路径，并删除 main.cpp 的跳过分支。

## What Changes

- **新增** `MarkdownParser` fenced yaml 块解析前端：识别 ` ```yaml ... ``` ` 块内含 `# --- BEGIN AgenticDSL ---` 标记的部分，使用 `yaml-cpp` 解析为与 bold 格式等价的元数据映射。
- **修改** `DslValidator` 增加双格式自动检测：扫描文本是否包含 fenced yaml 块且含 `# --- BEGIN AgenticDSL ---` 标记，是则走 yaml 解析路径，否则走原有 bold 解析路径；两条路径共享同一 schema 校验逻辑。
- **修改** 测试 fixture：迁移或增加 yaml 格式 fixture（覆盖真实生产格式），保留部分 bold fixture 作为回归网，确保向后兼容。
- **修改** `examples/pdk_chat_demo/main.cpp` 删除检测到 YAML 即跳过校验的分支，使生产路径真正参与 schema 校验。
- **不修改** DSL 语法本身（如新增 `.agent.md` schema 字段）——归 `docs/proposals/` 独立提案。
- **不修改** 现有 schema 校验规则（审批策略、Layer 矩阵、必需字段等）。

## Capabilities

### New Capabilities
- `yaml-fenced-block-detection`: DSL 文件格式自动检测（fenced yaml 代码块 vs Markdown bold），为 `dsl-validator` 提供可扩展的双格式入口。

### Modified Capabilities
- `dsl-validator`: 支持 fenced yaml 代码块（含 `# --- BEGIN AgenticDSL ---` 标记）格式解析，并将解析结果导入与 bold 格式完全相同的 schema 校验路径；非法 yaml 同样产生 line-level 错误报告。

## Impact

- **生产代码**:
  - `src/modules/parser/markdown_parser.h/.cpp`（fenced yaml 块解析前端）
  - `examples/pdk_chat_demo/dsl_validator.cpp` 或对应文件（双格式分发与 schema 复用）
  - `examples/pdk_chat_demo/main.cpp`（删除跳过校验分支）
- **测试代码**:
  - 新增/迁移 fixture 至 yaml 格式
  - 保留部分 bold fixture 作为回归网
  - 新增非法 yaml fixture，验证 line-level 错误路径
- **API 兼容性**:
  - ✅ bold 格式行为完全保持向后兼容
  - ✅ `DslValidator` 公开接口签名不变（新增内部解析分支）
- **依赖**:
  - ✅ 无新依赖，复用 `external/yaml-cpp`
- **文档**:
  - 不新增 ADR（0.5 Sprint 轻量修复）
- **风险**:
  - 低 - 解析前端改动，schema 校验逻辑零复制；bold 路径作为回归网得到保留