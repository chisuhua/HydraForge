# ADR-0043: PDK 工具命名约定规范

## 状态

✅ Approved (2026-07-10 — OpenSpec changes `phase5-b2-arch-schemas` (C13) + `phase5-llama-engine-plugin` (C14) ship, D3 决策已应用); **2026-07-06 renumber**: 兄弟 ADR-0036 → ADR-0045 (编排 plugin), ADR-0037 → ADR-0046 (通信协议), 避免与旧 ADR-0036-三层服务协议 / ADR-0037-因果序冲突

> **实施依据**: D3 决策 (per `docs/adversarial-reviews/decisions-2026-07-07.md` §D3) 已应用 — (1) C13 4 个 `lib/inference/*.md` schema 工具名统一 slash (`inference/prefix_cache/configure` 等); (2) C14 `pdk/llama_engine/` 12 个工具全部使用 slash 命名 (`inference/engine/init` 等); (3) `scripts/fix-adr-naming-policy-2026-07-08` 已 ship (2026-07-09) 修复命名约定残留 dot 风格。验证: `grep -rn "inference\\.[a-z]" pdk/ lib/inference/ → 0 matches` + C13/C14 ctest 65/65 + 0 回归 + `tools/adr_lint.py` exit 0。详见 `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §三 C13 + C14 行 + `docs/active-status.md` §五 2026-07-07 / 2026-07-08 行。

## 领域

基座 / PDK / Naming Convention

## 关联

- [ADR-0034 (Model Router)](./plugin/adr-0034-model-router.md) — `model_router/*` 先例 (P0 已 ship)
- [ADR-0021 (PDK Design)](./adr-0021-pdk-design.md) — Plugin SDK 模式
- [ADR-0022 (Plugin Loading)](./adr-0022-plugin-loading.md) — ToolMetadata 名称校验
- [ADR-0046 (Plugin Communication Protocol) §1](./adr-0046-plugin-communication-protocol.md) — slash/dot 分隔符决策
- [ADR-0035 (Inference Engine Plugin Spec) §2](./adr-0035-inference-engine-plugin-spec.md) — slash 工具集示例
- [ADR-0045 (Orchestration Plugin Spec) §5](./adr-0045-orchestration-plugin-spec.md) — orchestration/* 工具集

---

## 背景

### 问题

PDK Plugin 注册工具时,工具命名没有标准化约定,导致:

1. **不同 plugin 不同风格**: C7 ADR-0034 用 `model_router/cost` (slash), ADR-0035 用 `inference/generate` (slash) — 一致 (✅),但历史 agent code::edit_file (dot+colon) 与新约定冲突 (per Sprint 20 agent_macros.h)
2. **命名冲突风险**: 多 plugin 命名空间重叠 (eg. 两个 plugin 都注册 `agent/action`)
3. **可发现性差**: 用户调 `call_tool("foo_bar")` 不知哪个 plugin 提供
4. **DSL ↔ Tool 映射模糊**: lib/inference/{engine,model,session}.md 文件中 `tool: inference/engine/init` 是 string match,parser 未做格式校验

### 目标

定义 PDK Plugin 工具命名的**强约束规范** (注册时 validation),消除冲突,提高可发现性。

---

## 决策

### 1. 命名格式

```
{plugin_namespace}/{component}/{action?}
```

- **必填**: `plugin_namespace` (≥3 字符,小写 snake_case)
- **必填**: `component` (≥2 字符,小写 snake_case, **必须是动词性或动作名词**)
- **可选**: `action` (≥2 字符,小写 snake_case)

**最小深度**: 2 段 (namespace + component, eg. `inference/generate`) 允许。当 component 本身是动作时 (eg. `generate`, `list`) 可以不带 action。

**最大深度**: 3 段。更高深度不鼓励。

**分隔符**: 唯一合法分隔符是 `/` (slash)。双 `/` 或尾随 `/` 不允许。

**`plugin_name` 与 `plugin_namespace` 概念区分**:
- **plugin_name**: PluginInfo POD 字段 (`name[64]`), 全局唯一标识 (eg. `"agenticllama_inference"`)
- **plugin_namespace**: 工具名前缀, 与 plugin_name 不必完全相同 (但 namespace 必须在 PluginInfo 里注册时声明, 与 plugin name 一一对应, 见 ADR-0041 §4 dependencies field)

### 2. 三层语义

| 层 | 例子 | 语义 |
|---|------|------|
| **Domain layer** | `inference`, `model_router`, `orchestration`, `filesystem`, `web_search` | 插件 namespace,代表领域 |
| **Component layer** | `engine`, `model`, `session`, `generate`, `cost`, `quality` | 命名空间内的子模块 (名词) 或动作 (动词) |
| **Action layer (可选)** | `init`, `list`, `status`, `create` | 操作的细分 (动作) |

**examples**:

```
✅ 合规:
inference/engine/init      ← 3 段: namespace/inference, component/engine, action/init
inference/generate          ← 2 段: component/generate 是动作
inference/get/status        ← 3 段: component/get, action/status
inference/get/models        ← 同上
inference/sampler/configure ← 3 段: component/sampler, action/configure
model_router/cost           ← model_router 是 namespace (snake_case 含 underscore)
model_router/quality
orchestration/route
orchestration/execute

❌ 不合规:
inference.engine.init    → dot 分隔符 (旧风格, deprecation warning 后硬错误)
inference//init          → 空 component
inference/Init           → 大写
inference                → 缺 component (单段不允)
inference/               → 尾 /
inference/engine/model/list  → 4 段 (超最大深度)
inference/something       → "something" 不是已知动词 (应 describe "what" not just "thing")
inference/Inference      → component 也要小写
```

### 3. Plugin namespace 命名规则 (P1 fix per Oracle review)

| 规则 | 例子 |
|------|------|
| **≥3 字符** | `inference` (5), `tools` (5), `files` (5), `kv_cache` (8) |
| **小写 snake_case** (允许下划线) | `model_router`, `llm_provider`, `web_search` |
| **单数 OR 复合或复数名词** | 单数: `inference`, `orchestration`; 复数: `tools`, `audits`; 复合: `model_router` |
| 不与已 ship plugin namespace 重名 | (由 PluginLoader 注册时校验 per [ADR-0041](./adr-0041-pluginloader-lifecycle-extension.md)) |
| 不与 HydraForge core tool 名重名 | `tool`, `read_file`, `write_file` 等保留 |

**保留 namespace 列表** (HydraForge core 占用, plugin 不可注册):

```
core
read
write
exec
```

### 4. Component 命名规则 (P1 fix `subscribe` 重复已修正)

| Component | Action 模式 | 例子 |
|-----------|-----------|------|
| 生命周期 | `init`, `create`, `destroy`, `start`, `stop`, `reset` | `inference/engine/init`, `session/create` |
| 列表/查询 (ReadOnly) | `list`, `get`, `status`, `count`, `find` | `inference/model/list`, `inference/get/status` |
| 配置/修改 (StateModify) | `configure`, `set`, `update`, `apply`, `register` | `inference/configure`, `kv_cache/configure` |
| 执行 (Execute) | `generate`, `execute`, `run`, `invoke`, `transform` | `inference/generate`, `orchestration/execute` |
| 资源 (StateModify) | `load`, `unload`, `mount`, `unmount` | `inference/model/load` |
| 流式 (Execute) | `stream`, `watch` (P1 fix: `subscribe` 移至 event/subscription 范畴,不在工具命名) | `inference/generate/stream` |

**注意**: action 命名应反映**效果**, 不反映**实现** (`unload` 而非 `close_file`; `get_status` 而非 `query_metrics`)。

### 5. 命名冲突解决

**冲突检测** (PluginLoader::load_so 时):

```cpp
// 伪代码
bool PluginLoader::register_tool(ToolFunc fn, const ToolMetadata& meta) {
  if (registry_.has_tool(meta.name)) {
    log_warning("Tool name conflict: " + meta.name + " already registered");
    // P1 fix decision: 返回 false (拒绝), 而非覆盖
    return false;
  }
  registry_.register(meta.name, fn, meta);
  return true;
}
```

**冲突解决策略**:

| 策略 | 适用 | 行为 |
|------|------|------|
| **拒绝 (默认)** | 两个 plugin 提供同名工具 | PluginLoader 返回 false,警告加载冲突 |
| **显式 override** | 明确声明 (在 PluginInfo 字段) | 保留 (Phase 2) |
| **版本化 namespace** | 多版本共存 (eg. inference_v2/generate) | 强制 namespace 含版本 |

**MVP 仅实现"拒绝"**。Override/version 延后 Phase 2。

### 6. Tool 名校验规则 (PluginLoader::validate_tool_name, P1 fix 改为可编译 std::regex)

```cpp
#include <regex>
#include <unordered_set>

namespace hydraforge {

// 1. 校验正则: 2 或 3 段 lowercase snake_case, slash 分隔
static const std::regex TOOL_NAME_REGEX(R"(^[a-z][a-z0-9_]{2,63}(/[a-z][a-z0-9_]{1,63}){1,2}$)");

// 2. 保留 namespaces (P1 fix: 移除 `core` 之外的冲突命名)
static const std::unordered_set<std::string> RESERVED_NAMESPACES = {
  "core", "read", "write", "exec"
};

// 3. 完整校验函数 (Phase 1 MVP)
ValidationResult validate_tool_name(const std::string& name) {
  if (name.empty() || name.size() > 128) {
    return {false, "name length out of [1, 128] range"};
  }
  if (name.find("//") != std::string::npos) {
    return {false, "double slash not allowed"};
  }
  if (!std::regex_match(name, TOOL_NAME_REGEX)) {
    return {false, "must match pattern: lowercase snake_case with 2-3 segments separated by single /"};
  }

  // 解析 segments
  std::vector<std::string> segments;
  size_t start = 0;
  for (size_t i = 0; i <= name.size(); ++i) {
    if (i == name.size() || name[i] == '/') {
      segments.push_back(name.substr(start, i - start));
      start = i + 1;
    }
  }

  // 首段 (namespace) ≥3 字符
  if (segments[0].size() < 3) {
    return {false, "namespace segment must be ≥3 chars"};
  }

  // 末段 (action) ≥2 字符如果存在
  if (segments.size() == 3 && segments[2].size() < 2) {
    return {false, "action segment must be ≥2 chars"};
  }

  // 保留 namespace 列表
  if (RESERVED_NAMESPACES.count(segments[0]) > 0) {
    return {false, "namespace '" + segments[0] + "' is reserved for HydraForge core"};
  }

  return {true, ""};
}

struct ValidationResult {
  bool valid;
  std::string reason;
};

}  // namespace hydraforge
```

**Phase 2 扩展方向** (非 MVP, P1 fix 记录):
- Unicode namespace 支持 (eg. `推理/generate`)
- 数字开头 namespace (eg. `v2_api/generate`)
- 自动注册已用 namespace (从 PluginInfo.name 提取)

### 7. 与旧 namespace 的兼容性

**移除 ADR-0021 §3.2 `code::edit_file` 风格**:

```cpp
// 旧 (Sprint 4 PDK MVP 内部示例):
DEFINE_AGENT(MyAgent, code, edit_file)
//       ^^^^^^^ namespace 含字母数字, 但用下划线分隔
```

**新约定**: `code/edit_file` (slash 分隔) 或保留旧 code/edit_file 作为 alias (Phase 2 决策)。

**兼容策略** (P1 fix):
1. PluginLoader 接受 slash 风格工具 (新约定)
2. PluginLoader 接受 dot 风格工具 (旧约定, **warning 但接受**, deprecation)
3. ADR-0021 §3.2 示例代码同步更新为 slash 风格
4. release 6 个月后 dot 风格 trigger hard error

### 8. ToolMetadata 标准化

| 字段 | 类型 | 必须 | 规范 | 来源 |
|------|------|:---:|------|------|
| name | string | ✅ | 符合 §1 命名格式 (slash, lowercase, 验证 §6 regex) | 本 ADR |
| description | string | ✅ | ≤200 字符,英文+中文混排 (UTF-8) | 本 ADR |
| domain | enum string | ✅ | 与 namespace 一致 (eg. `domain: "inference"` 与 `name: "inference/..."`) | 本 ADR + ADR-0004 V2 |
| category | ToolCategory | ✅ | ReadOnly/WriteFile/Execute/Network/StateModify | [ADR-0004 V2 §6](./adr-0004-toolregistry-security.md) |
| min_layer | LayerProfile | ✅ | Cognitive/Thinking/Workflow (数值 0/1/2) | ADR-0004 V2 |
| approval_policy | ApprovalPolicy | ✅ | 4-bool: {yolo, plan, agent, always} | ADR-0004 V2 |
| allowed_layers | int[] | ❌ (Phase 2) | 精细 layer 限制 | ADR-0004 V2 |
| cost_estimate | float | ❌ | USD per call | ADR-0004 V2 |
| timeout_ms | int | ❌ (默认 300000) | 单调超时, 超时触发 ToolResult::error(TIMEOUT) | ADR-0023 §C + [ADR-0031 §6](./adr-0031-execution-policy.md) |
| version | int | ❌ (默认 1) | 同名不同版本时 bump | ADR-0004 V2 |

**Cross-reference**: ToolMetadata C++ struct 定义与原始字段列表见 [ADR-0004 V2 §2C.3](./adr-0004-toolregistry-security.md)。本 ADR 不重新定义字段, 只**规范名称校验**与**分类 (Category/Layer/Approval) 决策树**。

**`domain` 字段 vs namespace 关系**: plugin 注册 tool 时, PluginLoader 校验 `domain == segments[0]` (namespace) 一致。Mismatched 返回注册失败。

**Multi-language description 注意事项**:
- `description` 字段 UTF-8 编码 (UTF-8 字符可含 ≥200 byte, 实际 byte 受 128-byte PluginInfo.total_size 限制)
- 推荐英文描述优先 (parser / TUI render 兼容), 中文版放在 `metadata.localized` (Phase 2)
- ASCII 字符以外 (`description` 含 CJK): 校验通过,但需 `PluginInfo` 总字节数 ≤ 128 (per current rule)

### 9. EventBus Topic 命名一致性回顾 (复查 + 强化)

per [ADR-0046 §2](./adr-0046-plugin-communication-protocol.md) 已确立:

| 用途 | 分隔符 | 例子 |
|------|-------|------|
| PDK tool name | slash `/` | `inference/generate` |
| EventBus topic | dot `.` | `inference.lifecycle.idle` |
| ADR 编号 | 四位数字 + `-` | `ADR-0035` |
| C++ namespace | `::` | `agenticdsl::ILLMProvider` |
| DSL module | `::` (C++) / `.` (DSL) | `inference::engine` / `inference.engine` |

**不允许的字符**:
- 工具名: 不含 `.` `:` `;` `,` ` ` `\t` `\n`
- EventBus topic: 不含 `/` `:` `;` `,` ` `

---

## 替代方案

### Option A: 不强制 (现状)

**被拒绝理由**: 用户已识别这是问题。多种风格并存导致 parser/IDE 难统一。

### Option B: 强制 dot 风格 (`inference.generate`)

**被拒绝理由**: ADR-0034 已 ship slash 风格 (model_router/cost),dot 风格需 Breaking change。slash 风格是早期选择, 维护向后兼容选 slash。

### Option C (采用): Slash + 严格 validation

最优解: slash 分层支持深层 hierarchy + PluginLoader 注册时严格校验拒绝不合规名。

---

## 实施顺序

1. ADR-0041 (PluginLoader 生命周期) ship 后,扩展 `is_valid_tool_name` 校验
2. ADR-0021 §3.2 PDK Agent 示例代码改 slash 风格 (release notes 标注 deprecated)
3. ADR-0034 已 ship,无须改动 (model_router/* 已合规)
4. ADR-0035/0036 推 publish release 时同步更新示例
5. 6 months deprecation 之后 dot 风格 trigger error

---

## 测试策略

| # | 测试 | 覆盖 |
|---|------|------|
| 1 | `validate_tool_name_slash_format` | 接受 `inference/generate`, 拒绝 `inference.generate` |
| 2 | `validate_tool_name_segment_count` | 接受 2-3 段, 拒绝 1 段 (≥1 namespace) 或 4+ 段 (避免过度分层) |
| 3 | `validate_tool_name_lowercase_only` | 接受 lowercase + underscore, 拒绝 uppercase/dash |
| 4 | `validate_tool_name_no_double_slash` | 拒绝 `inference//generate` |
| 5 | `validate_tool_name_reserved_namespace` | 拒绝 `core/xxx`, `read/xxx` (保留 namespace) |
| 6 | `register_tool_conflict_rejected` | 两个 plugin 同一工具名 → 第二个失败 |
| 7 | `tool_metadata_domain_matches_namespace` | `domain: "inference"` + `name: "inference/xxx"` 接受; mismatch 拒绝 |
| 8 | `tool_metadata_required_fields` | 缺 name/description/category → 注册失败 |
| 9 | `tool_name_unicode_safe` | 中文描述 accepted,但 name 限 ASCII |
| 10 | `plugin_loader_warn_deprecated_dot_style` | 旧 `inference.generate` 注册 → warning 但接受 |

---

*创建日期*: 2026-07-06
*依赖*: ADR-0022 (ToolRegistry name 校验), ADR-0034 (slash 先例), ADR-0046 (separator decision), ADR-0041 (PluginLoader 校验接入点)
*关联*: ADR-0021 §3.2 (PDK Agent 示例代码需更新)
