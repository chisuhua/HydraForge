## Context

`DslValidator` 的元数据解析入口仅实现了 Markdown bold 解析（`**key**: value`）。然而生产仓库 `lib/loop/*.agent.md` 已全面使用 markdown 标题 + fenced yaml 代码块格式声明 schema 字段、审批策略和层权限。实际生产文件结构（以 `lib/loop/react.agent.md:1-2` 为例）：

```
### AgenticDSL `/main`
```yaml
# --- BEGIN AgenticDSL ---
graph_type: subgraph
nodes:
  - id: start
    type: start
    ...
```

这导致 `examples/pdk_chat_demo/main.cpp` 在加载这种文件时直接绕过校验（因文件中无 `**` 标记），仅打印 stderr 警告。pdk_chat_demo 的 T2 DSL Schema 校验在生产路径上形同虚设。

同时，现有测试 fixture 仍使用 bold 格式，回归网验证的格式与生产实际不一致，无法阻止 yaml 路径上的非法配置进入主分支。

## Goals / Non-Goals

**Goals:**
- 让 `MarkdownParser`/`DslValidator` 支持 fenced yaml 代码块解析（识别含 `# --- BEGIN AgenticDSL ---` 标记的块），并将解析结果导入与 bold 格式相同的 schema 校验路径。
- 实现双格式自动检测（fenced yaml 块含 BEGIN 标记 → yaml；否则 → bold），对调用方透明。
- 迁移或参数化测试 fixture，使回归网覆盖真实生产格式。
- 删除 `examples/pdk_chat_demo/main.cpp` 的"检测到 YAML 即跳过校验"分支，消除生产路径上的校验空洞。
- 保持 ctest 全量零回归。

**Non-Goals:**
- 不扩展 DSL 语法本身（如新增 `.agent.md` schema 字段）。
- 不复制 schema 校验规则到 yaml 路径。
- 不引入新的 yaml 解析库。
- 不修改 schema 校验规则（审批策略、Layer 矩阵、必需字段等）。

## Decisions

### Decision 1: 复用现有 schema 校验逻辑（禁止规则复制）

**Rationale**:
- 双格式的差异仅在于解析前端（bold 行扫描 vs yaml 节点遍历），schema 校验规则（字段类型、审批策略、Layer 矩阵、必需字段）完全一致。
- 复制规则会导致同一规则在两个位置维护，后续修改必产生漂移。
- 将解析结果统一转换为相同的内部 `metadata` map，再传入既有校验函数。

**Alternatives Considered**:
- 为 yaml 单独实现一套校验器 - 造成重复代码，违反 DRY。
- 将 bold 解析结果转换为 yaml 节点再复用 yaml 校验器 - 增加中间层，改动更大，且不利于保留 line-level 错误定位。

### Decision 2: 保持 bold 格式向后兼容

**Rationale**:
- 现有 bold fixture 是核心回归网，必须继续通过。
- 部分内部示例或旧文档可能仍使用 bold 格式，立即废弃会造成不必要破坏。
- 通过格式检测在运行时自动选择解析器，无需调用方改造。

**Alternatives Considered**:
- 一次性废弃 bold 格式并全量迁移 fixture - 改动范围超出 0.5 Sprint 预算，且破坏向后兼容。
- 将 fixture 全部转为 yaml 并删除 bold 解析 - 同上，风险不可接受。

### Decision 3: 通过 fenced yaml 块 + BEGIN 标记自动检测格式

**Rationale**:
- 生产文件统一以 `### AgenticDSL \`/path\`` 开头，紧跟 ` ```yaml ... ``` ` 块，块首部含 `# --- BEGIN AgenticDSL ---` 标记。
- 检测逻辑：扫描文本中是否存在 ` ```yaml ` 起止且块内容首行为 `# --- BEGIN AgenticDSL ---` 的代码块。
- 这种标记方式精确区分"AgenticDSL 元数据"和"普通 yaml 示例"，避免误解析用户文档中的其他 yaml。

**Alternatives Considered**:
- 仅检测 ` ```yaml ` 起止 - 无法区分 AgenticDSL 元数据 vs 用户提供的示例 yaml。
- 按文件扩展名 `.yaml.md` / `.agent.md` 区分 - 生产文件扩展名与 bold 文件一致，无法区分。
- 在 `DslValidator` 外部由调用方显式指定格式 - 增加 API 复杂度，且 main.cpp 已希望自动处理。

### Decision 4: 使用 `external/yaml-cpp` 解析 yaml，禁止引入新库

**Rationale**:
- `yaml-cpp` 已存在于 `external/`，并在构建系统中集成；复用可避免依赖膨胀。
- 项目其他模块（如 `llm_config.json` 解析路径）已有使用先例，团队熟悉其 API 与异常行为。

**Alternatives Considered**:
- 手写 yaml 子集解析器 - 容易引入安全与兼容性漏洞，且 0.5 Sprint 不划算。
- 引入其他第三方库（如 rapidyaml） - 违反"禁止引入新解析库"约束，且需更新 CMake 与 CI。

### Decision 5: 删除 main.cpp 跳过校验分支，不保留静默 fallback

**Rationale**:
- 当前分支输出 stderr 警告后直接返回，使 yaml 文件在校验前逃逸，属于产品缺陷。
- 删除后，yaml 文件与 bold 文件走相同校验；若非法则报错，符合用户预期。
- 若 yaml 解析失败（语法错误），应由解析前端抛出 line-level 错误，而不是被主函数吞掉。

**Alternatives Considered**:
- 保留分支但改为 warning + 继续校验 - 仍然允许部分非法文件通过，且逻辑冗余。
- 将分支改为 error 退出 - 与删除校验分支等价，但留下死代码。

## Risks / Trade-offs

### Risk 1: yaml 解析异常格式导致崩溃

**Mitigation**:
- 所有 `yaml-cpp` 调用用 try/catch 包裹，将 `YAML::ParserException` 转换为 `ValidationError`（含 line/column）。
- 新增非法 yaml fixture 覆盖解析失败路径。

### Risk 2: bold fixture 迁移后失去 bold 覆盖

**Mitigation**:
- 至少保留一组原始 bold fixture 不变（或参数化同时跑两种格式），确保 bold 路径持续回归。
- 双格式自动检测本身即保证任意输入都会走正确路径，无需 fixture 全部覆盖两种格式。

### Risk 3: yaml 键名与 bold 键名大小写/命名差异

**Mitigation**:
- 解析后统一规范化键名（小写、下划线、去首尾空格）。
- schema 校验使用规范化后的键名，与现有 bold 解析结果保持一致。

### Trade-off 1: 解析前端增加 vs 整体重构

**Trade-off**: 在 MarkdownParser 内新增 yaml 分支，会使文件承担两种解析职责；但整体重构为独立 parser 工厂超出 0.5 Sprint 预算。
**Decision**: 接受，将 yaml 解析封装为私有 helper，保持公开接口不变，未来可再提取。

### Trade-off 2: yaml fixture 与 bold fixture 并存

**Trade-off**: 同时维护两套 fixture 会增加测试文件数量，但能保证双格式回归。
**Decision**: 接受；对测试 fixture 优先做参数化（同文件跑两种输入），必要时才复制。