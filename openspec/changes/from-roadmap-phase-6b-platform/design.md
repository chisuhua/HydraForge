# from-roadmap-phase-6b-platform — Design

## Context

`examples/pdk_chat_demo/dsl_validator.{h,cpp}`（位于 `examples/pdk_chat_demo/`）是 PDK Chat Demo 在 `.agent.md` 文件加载阶段调用的 DSL 文档/图结构校验器。在 `main.cpp` 的非 mock 启动路径上，启动时会读取 `.agent.md` 并将 markdown 文本传入 `DslValidator::validate()`，返回的 `ValidationResult` 决定应用是否继续启动（mock 模式跳过本校验）。

现状下，validator 通过 `fix-markdown-parser-yaml` 改造支持了 YAML fenced 块和 Markdown bold 两种格式，但 YAML 路径的实现依赖两类**正则/子串**解析：

1. `yaml_field_value()` — 用正则 `^\s*<key>\s*:\s*([^#\r\n]+?)\s*(?:#.*)?$` 提取 frontmatter 标量字段（name/version/agent_loop）
2. `yaml_nodes_json()` — 从 `nodes:` 子串处截取到文件末尾，将截取的文本直接传给 `nlohmann::json::parse()`

这两条路径在生产 fixture 中暴露**关键失败模式**：

- YAML 列表项含**注释**或空行 → 子串截断污染 JSON，触发 `PARSE_ERROR`
- YAML 多行缩进结构（如 `execution_budget.max_nodes` 下钻路径）→ 正则只匹配首层标量，深层节点结构丢失
- YAML 节点 list 含**多个顶层字段**而非 `nodes:` 在末尾 → 子串截到 EOF 后混入非 nodes 内容

更严重的是，**校验入口在解析失败时直接 `return result;`**（dsl_validator.cpp:227-229），导致用户只看到 `PARSE_ERROR` 而不知道根因（哪个 YAML 字段非法、哪个节点结构有误）。

项目已有 `src/common/utils/yaml_json.{h,cpp}` 提供 `agenticdsl::yaml_to_json(YAML::Node)` 函数，使用 `yaml-cpp` 做完整 YAML→JSON 转换。本提案**复用该函数**，不引入新的 YAML 解析依赖。

约束（来自 proposal.md）：

- 不下沉 demo 级契约到核心 `src/modules/parser/`
- 不修改核心 `MarkdownParser`
- 与 ADR-0058 保持边界（ADR-0058 管 tool JSON Schema，本提案管 DSL 文档/图结构）
- mock 模式跳过校验的行为不变

## Goals / Non-Goals

**Goals:**

- YAML fenced DSL 走完整 `yaml-cpp` 解析路径 → `yaml_to_json` → 既有 JSON 校验链
- frontmatter 必填字段从结构化 JSON 中提取（替代正则）
- nodes 列表从结构化 JSON 中按 `nodes` 字段取值（替代子串截取）
- 收集所有结构错误（frontmatter 缺失 + 节点字段缺失 + 非法类型 + 工具依赖），不 fail-fast
- 错误信息携带**稳定路径**（`frontmatter.<field>` / `node[N].<field>`），便于 CI 诊断
- 保留 LF/CRLF 兼容、Markdown bold 回退路径、mock 模式跳过校验
- 新增生产 YAML fixture + 6 类回归测试（合法 / 缺字段 / 非法类型 / 未注册工具 / LF/CRLF / 注释污染）

**Non-Goals:**

- 不修改 mock 模式跳过校验契约
- 不迁移/重构 `src/modules/parser/markdown_parser.{h,cpp}`
- 不新增核心 parser API
- 不实现 ADR-0058 工具 input/output JSON Schema 执行时校验
- 不做 YAML 锚点/别名/多文档拆分的高级特性支持（若 fixture 含此类结构，校验给出明确 INVALID_YAML_FEATURE 错误，不尝试解析）

## Decisions

### D1. 用 `agenticdsl::yaml_to_json` 替代正则/子串解析

**选择**：替换 `yaml_field_value()` 和 `yaml_nodes_json()` 为基于 `yaml-cpp` + `yaml_to_json` 的结构化路径。

**理由**：

- 项目已有 `yaml-cpp` 依赖（`external/yaml-cpp/`）和 `yaml_to_json()` 助手（`src/common/utils/yaml_json.{h,cpp}`）
- yaml-cpp 处理多行缩进、注释、空行、嵌套 mapping/sequence 均为成熟行为
- 输出 `nlohmann::json` 直接喂给既有 JSON 校验链（`nlohmann::json::parse` → 节点遍历），**零侵入**
- 边界检查通过：`yaml_to_json` 位于 `src/common/utils/`（公共工具），不在 `src/modules/parser/`（核心解析器），不违反"不下沉 demo 级契约到核心 parser"约束

**替代方案**：在 demo 内自己实现 YAML parser → 拒绝（重新造轮子，违反 Don't Repeat Yourself）。

### D2. frontmatter 字段从 JSON 中按 `contains()` 取值，缺失返回空串

**选择**：用 `yaml_block_json.contains("name") && yaml_block_json["name"].is_string()` 判断字段存在性，避免原正则无法处理嵌套 mapping 的缺陷。

**理由**：

- 现有 `extract_frontmatter_value()` 已经处理"取首层标量值"，新版只需改成"取 JSON 路径值"
- 嵌套 frontmatter 字段（如 `execution_budget.max_nodes`）将来若加入必填，可自然支持

### D3. nodes 列表从 JSON 中按 `"nodes"` key 取值

**选择**：用 `yaml_block_json.contains("nodes") && yaml_block_json["nodes"].is_array()` 提取。

**理由**：

- 消除"截到 EOF"的脆弱假设
- 非数组类型（如标量或映射）作为结构错误返回 `INVALID_NODES_TYPE`，而非触发 `PARSE_ERROR`

### D4. 错误信息携带稳定路径 + 错误类型

**选择**：扩展 `ValidationError` 已有的 `type` 字段，新增/复用 4 类：

| type | 触发 | path 格式 | 来源 |
|------|------|-----------|------|
| `MISSING_REQUIRED_FIELD` | frontmatter 或节点必填字段缺失 | `frontmatter.<field>` 或 `node[N].<field>` | 既有 |
| `INVALID_NODE_TYPE` | 节点 type 不在白名单 | `node[N].type` | 既有 |
| `MISSING_TOOL_DEPENDENCY` | `call_tool` 引用未注册工具 | `node[N].tool_name` | 既有 |
| `INVALID_YAML` | yaml-cpp 解析失败 | `yaml_block[L]` 或行号 | 新增 |

`path` 格式统一为点分隔，CI 可按 `frontmatter.name` / `node[3].id` 直接 grep。

### D5. 不 fail-fast，收集所有错误

**选择**：移除 `dsl_validator.cpp:227-229` 的 `return result;`（JSON 解析失败立即返回），改为追加 `INVALID_YAML` 错误后继续尝试其他结构校验（如 frontmatter 字段检查）。

**理由**：

- 用户体验：一次启动看到全部 5 个错误比每次修一个错重启看到下一个错更友好
- 测试可验证：单次 `validate()` 返回多个 `ValidationError`，断言 `errors.size() == N`

**风险**：某些错误级联（如 `nodes` 不是数组）导致后续节点遍历无意义，仍需在该点提前返回，但**不 fail-fast 是默认行为**。

### D6. yaml-cpp 错误携带行号

**选择**：捕获 `YAML::ParserException` 的 `mark.line` / `mark.column`，写入 `ValidationError::path` 字段（格式 `yaml_block[L:C]`）。

**理由**：

- `YAML::ParserException::mark` 自带 `line`/`column` 字段（yaml-cpp 公共 API）
- 用户定位 `.agent.md` 中的非法行直接可用编辑器跳行

## Risks / Trade-offs

- [R1: yaml-cpp 异常路径吞掉部分错误] → Mitigation：在 `try/catch (YAML::ParserException&)` 内捕获单块异常，对每个 YAML 块独立 try；任一块失败不影响其他块验证
- [R2: frontmatter 嵌套结构（旧 fixture 中为扁平）兼容性] → Mitigation：保留 `extract_frontmatter_value()`（bold 路径）作为回退；yaml 路径仅在 fenced 块存在时激活
- [R3: 性能开销（yaml-cpp vs 正则）] → Mitigation：启动期一次性校验，yaml-cpp 解析单文件 < 5ms（典型 `.agent.md` < 200 行），用户感知不到
- [R4: 测试覆盖不足] → Mitigation：新增 6 类回归测试覆盖 D1-D6 决策点（见 tasks.md §3）

## Migration Plan

### 部署步骤

1. **开发**：在 `examples/pdk_chat_demo/dsl_validator.{h,cpp}` 实现 D1-D5
2. **测试**：新增 `tests/test_dsl_validator_yaml.cpp` 覆盖合法/非法/YAML 解析失败/LF/CRLF/工具依赖/Mock 模式跳过 6 类场景
3. **fixture**：从 `lib/loop/*.agent.md` 抽取 3 个生产 YAML 块作为黄金 fixture，落到 `examples/pdk_chat_demo/tests/fixtures/`（git tracked）
4. **回归**：跑 `ctest -R 'pdk_chat|dsl_validation' --output-on-failure` 确认无破坏
5. **文档**：更新 `examples/pdk_chat_demo/README.md`（如有）说明 YAML 校验链

### 回滚策略

若 ship 后发现严重回归，单 PR revert 即可。YamlValidator 改动是**纯增量**（既有 bold 路径 + 新增 yaml 路径），无 API 删除，回滚 = 移除新增代码段。

## Open Questions

- **OQ1**：`MISSING_REQUIRED_FIELD` 当前路径格式为 `node[N]`，新格式 `node[N].id` 是否需要并行兼容？决策：**采用新格式**（path 包含字段名便于 grep），旧 fixture 测试断言需同步更新。
- **OQ2**：yaml-cpp 是否需要支持 YAML 1.2 多文档（`---` 分隔）？决策：**当前不支持**，生产 fixture 不含多文档；如未来需要可独立提案。
- **OQ3**：`YAML::ParserException::mark` 在某些平台（如 WSL）行号从 0 还是 1 起算？决策：以 yaml-cpp 文档为准（默认 0-based），测试断言用 `>=0` 兼容。