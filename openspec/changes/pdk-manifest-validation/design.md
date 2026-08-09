# pdk-manifest-validation Design

## Context

### 当前状态

- `PluginLoader` (位于 `src/modules/plugin/plugin_loader.cpp`) 通过 `dlopen` → `dlsym(pdk_plugin_info)` → ABI 检查的流程加载 plugin
- `PluginInfo` POD (per `plugin-loader` spec) 含 `name`, `version`, `abi_version`, `capabilities` 4 类基础元数据
- `CURRENT_ABI_VERSION = 2`, `SUPPORTED_ABI_VERSIONS = {1, 2}` (dual-ABI, per `include/agenticdsl/plugin/plugin_info.h:76-77`)
- 12 个现有 PDK plugin (`pdk/{llama_engine,model_router,loop_agent,fs_tools,shell_tools,provider_agent,budget_agent,session_agent,temporal_agent,g1_coding_assistant,g3_knowledge_base,...}/`) 无 manifest 文件
- 加载流程 **无法** 在 dlopen 之前做版本/信任/工具 schema 校验
- 服务化场景 (Phase 6c) 要求 OS 能远程发现 plugin 元数据 (per ADR-0052 §背景)

### 约束 (per AGENTS.md)

- C++20 + CMake 3.20+
- 2 空格缩进, 中文注释
- nlohmann::json 已 vendored, 不引入新依赖
- PluginLoader Sprint 5 contract lock: 不修改 4 公开方法签名 (`load_all`/`load_so`/`list_loaded`/`unload_plugin`), 仅可加新方法
- 现有 PluginLoader API 必须向后兼容 (Sprint 18-19 PIMPL pattern)

### 利益相关方

- Phase 6b AgentForge (下游消费者)
- Phase 6c 服务化重评 (消费 manifest)
- 现有 12 个 PDK plugin 维护者 (向后兼容关键)

## Goals / Non-Goals

### Goals

1. 定义 `pdk_manifest.json` 文件格式 (per ADR-0052 §决策 1-3 完整履行, 9 必填 + 8 推荐 + 1 可选字段)
2. 实现 `ManifestValidator` (独立类, 双 ABI 支持, 严格类型检查)
3. 实现 `ManifestFinder` (独立类, symlink 安全, weakly_canonical, max 16 层 bound)
4. 修改 `PluginLoader::load_so()` 流程为 manifest-first (per ADR-0052 §决策 4)
5. 现有 12 个 plugin **不**被强制迁移 (向后兼容, 缺 manifest 仅 warn)
6. 19+ 单元测试 + 5+ 集成测试覆盖全部 spec 场景
7. IInteractionBus opt-in 注入 (setter, 默认 nullptr, per ADR-0031 §决策 5)

### Non-Goals

- ❌ `.hfpkg` 包格式 (ADR-0052 §决策 1 提及, Phase 6a 不做)
- ❌ Wasm 嵌入 manifest (Phase 6a 不做)
- ❌ Trust 签名验证 (per ADR-0052 §决策 7, Phase 6a 仅声明字段)
- ❌ 现有 12 个 PDK plugin 迁移 (后续 follow-up)
- ❌ 跨 plugin 资源协调 (仅记录声明)
- ❌ `pdk_create_llm_provider` 符号交叉验证 (per ADR-0041, 推迟至 follow-up)

## Decisions

### Decision 1: Manifest 文件位置 — 插件根目录 (per ADR-0052 §决策 1)

**选择**: `pdk_manifest.json` 必须与 Plugin `.so` 同目录或上层目录

**理由**:
- 符合 ADR-0052 规范
- 允许 src/manifest + build/.so 分离布局
- 检测算法: 从 `.so` 路径 `std::filesystem::weakly_canonical()` → 向上查找 (max 16 层) 直到找到 manifest 或达到文件系统根

**替代方案考虑**:
- 强制 manifest 在 `.so` 同目录 → 拒绝 `build/` 布局, 过于严格
- 仅支持 `*.manifest.json` 后缀 → 增加识别复杂度, 无收益

### Decision 2: Validator 设计 — 独立类 + Result<T, Error> 模式 + 严格类型检查

**选择**: `ManifestValidator` 独立类, 提供静态方法 `validate(json_content) -> ManifestValidationResult`

```cpp
struct ValidationError {
  std::string field;      // "id" / "tools[0].input_schema" / "abi_version"
  std::string reason;     // "required" / "invalid_semver" / "mismatch" / "wrong_type" / "invalid_enum"
  std::string value;      // actual value (optional, for debugging)
  std::string expected;   // expected value/type/enum (optional)
};

struct ManifestValidationResult {
  bool valid;
  std::optional<Manifest> manifest;       // valid=true 时填充
  std::vector<ValidationError> errors;    // valid=false 时填充
};
```

**类型严格策略** (避免 nlohmann 隐式转换坑):
- `uint32` 字段收到 string `"1"` → reject `reason="wrong_type"`
- `uint32` 字段收到 float `1.0` → reject `reason="wrong_type"` (nlohmann 默认会 truncate, 我们禁用)
- `uint32` 字段收到负数 → reject `reason="wrong_type"`
- `bool` 字段收到 string `"true"` → reject `reason="wrong_type"`
- `string` 字段收到 `null` → reject `reason="required"` (字段缺) 或 `reason="wrong_type"`

**理由**:
- 独立类便于单测 (无 PluginLoader 依赖)
- Result 模式比抛异常更适合热路径 (避免 stack unwinding 开销)
- 与 ADR-0068 EventBuilder 链式 API 风格一致
- 严格类型策略避免 nlohmann 隐式转换导致 silent 接受错误数据

**替代方案考虑**:
- 抛异常 → 性能 + 调用方必 try-catch, 过于重
- 输出 bool + 写日志 → 不可恢复错误信息丢失, 不可单测
- nlohmann 默认宽松类型 → silent 接受 string "1" 为 uint32, 不可控

### Decision 3: PluginLoader 加载流程前移 — manifest-first (per ADR-0052 §决策 4)

**选择**: 修改 `load_so()` 流程为:
1. 从 `.so` 路径 `weakly_canonical` → 向上查找 manifest (max 16 层)
2. 找到 manifest → 读 + 校验
   - 校验失败 → 拒绝 load (返回 false + emit `plugin.manifest.invalid`)
3. 未找到 manifest → 按 `require_manifest` 参数决定:
   - `require_manifest=false` (默认) → warn 日志 + 继续旧 dlopen 流程
   - `require_manifest=true` → 拒绝 load (返回 false + emit `plugin.manifest.missing`)
4. `dlopen` + `dlsym(pdk_plugin_info)` + 交叉验证:
   - `PluginInfo.abi_version != manifest.abi_version` → emit warn (以 PluginInfo 为准, per ADR-0052 §决策 4 注释)
   - 优先级: PluginInfo > manifest (因为 dlopen 后从 .so 读取, 反映真实编译产物)

**理由**:
- 校验前置: 拒绝不合规 manifest 早于 dlopen, 避免加载垃圾 .so
- 向后兼容: 缺 manifest 不阻塞, 旧 plugin 可渐进迁移
- 交叉验证: 双源元数据不一致时 warn 但不 fail (per ADR-0052 §决策 4)

**替代方案考虑**:
- 强制 manifest (无 manifest 即拒绝) → 阻塞现有 12 个 plugin, 违反向后兼容目标
- 完全异步预读 → 增加复杂度, 本阶段无收益

### Decision 4: IInteractionBus 注入 — setter opt-in (per ADR-0031 §决策 5)

**选择**: `PluginLoader::set_interaction_bus(IInteractionBus* bus)` setter + `clear_interaction_bus()` 重置, 默认 `nullptr` 跳过 emit

```cpp
// include/agenticdsl/plugin/plugin_loader.h
class PluginLoader {
 public:
  // ... 4 existing public methods unchanged ...
  
  // New: opt-in bus injection (Sprint 22+ pattern)
  void set_interaction_bus(IInteractionBus* bus);
  void clear_interaction_bus();
  
 private:
  IInteractionBus* bus_ = nullptr;  // 默认为空, 不引入构造签名 BREAKING
};
```

**理由**:
- 镜像 ADR-0031 §决策 5 ToolCoordinator opt-in 模式 (避免 2026-06-30 那种 default-on 引入 2 个测试回归)
- 构造签名零变化, 现有调用方零修改
- bus 指针生命周期由调用方管理, PluginLoader 仅持有 weak 引用
- nullptr 时静默跳过 emit, 不污染日志

**替代方案考虑**:
- 构造函数必填注入 → BREAKING 现有 4 个 PluginLoader 构造调用
- 全局 Bus 实例查找 → 隐式全局状态, 与 ADR-0019 跨模块协作原则不符

### Decision 5: 双 ABI 支持 — 镜像 SUPPORTED_ABI_VERSIONS

**选择**: Manifest 校验器接受 `abi_version ∈ {1, 2}`, 与 `PluginLoader` 实际行为对齐

```cpp
// manifest_validator.cpp
constexpr std::array<uint32_t, 2> kSupportedAbiVersions = {1, CURRENT_ABI_VERSION};

bool is_supported_abi(uint32_t v) {
  for (auto supported : kSupportedAbiVersions) {
    if (v == supported) return true;
  }
  return false;
}
```

**理由**:
- 实际 `include/agenticdsl/plugin/plugin_info.h:77` 已声明 `SUPPORTED_ABI_VERSIONS[] = {1, 2}`
- 强制 `=CURRENT_ABI_VERSION(=2)` 会把 v1 plugin 锁在 manifest 体系外
- 与 ADR-0022 §3.2 dual-ABI 设计原则一致

**替代方案考虑**:
- 硬匹配 `CURRENT_ABI_VERSION` → 拒绝 v1 manifest, 与现有 PluginLoader 实际行为分裂
- 软匹配 + warn → 中间态, 增加 spec 复杂度

### Decision 6: Manifest 字段定义 — 完整履行 ADR-0052 §决策 1-3

**选择**: 9 必填 + 8 推荐 + 1 可选 (signature)

```json
{
  "id": "string (reverse-DNS, max 64)",
  "name": "string (human-readable, max 128)",
  "version": "semver string",
  "abi_version": "uint32 (1 or 2)",
  "min_host_version": "semver string (soft constraint, warn only)",
  "max_host_version": "semver string (soft constraint, warn only)",
  "implementation_forms": ["skill" | "dsl" | "cpp" | "wasm"] (string[], non-empty),
  "entry_tool": "string (must be in provided_tools[])",
  "provided_tools": ["code_review/run", "code_review/suggest"] (string[], non-empty),
  "interface_versions": ["IAgentV1"] (string[], recommended),
  "capabilities": ["code_review", "static_analysis"] (string[], recommended),
  "input_schema": { /* JSON Schema 2020-12 object */ },
  "output_schema": { /* JSON Schema 2020-12 object */ },
  "requires_isolation": false (bool, recommended),
  "resources": {
    "timeout_ms": 30000 (uint32, default 30000),
    "max_concurrent": 4 (uint32, default 1)
  },
  "publisher": "string (recommended)",
  "trust_level": "high" | "medium" | "low" | "untrusted" (string, recommended),
  "activation_events": ["on_session_start"] (string[], recommended),
  "signature": "string (optional, Phase 6a 仅记录不验签)"
}
```

**注意** (per ADR-0052 §决策 4 软约束):
- `min_host_version` / `max_host_version` 是**软约束**: 校验器只检查结构 (max >= min), 实际 host version 比较 **推迟**至 PluginLoader 集成阶段 (因 validator 无 host context)
- `abi_version` 是**硬约束**: 不在 SUPPORTED_ABI_VERSIONS 即 reject
- `entry_tool` MUST be in `provided_tools[]` (cross-field 校验)
- `implementation_forms` MUST non-empty 且每个值 ∈ {skill, dsl, cpp, wasm}
- `approval_policy` 枚举顺序 (per DECLARE_TOOL macro `make_approval()` 一致性): "always" / "plan" / "agent" / "yolo"

## Risks / Trade-offs

### Risk 1: 现有 12 个 PDK plugin 缺 manifest 时行为

**[Risk]** 缺 manifest 的 plugin 仍可加载 (warn-only), 但失去前置校验保护

**Mitigation**:
- 本 change 不强制迁移, 显式文档化"建议但非强制"
- 后续 follow-up change 单独迁移每个 plugin
- 在 `agenticdsl_core` 启动日志中显式列出缺 manifest 的 plugin

### Risk 2: 向上查找 manifest 的 I/O 开销

**[Risk]** 每次 load_so 都要做文件系统 stat + 读取, 可能成为热点

**Mitigation**:
- 查找结果 cache (per PluginLoader 实例, LRU)
- cache key = `.so` 绝对路径 (post weakly_canonical)
- Phase 6a 不实现 cache (如需, 后续优化 change)

### Risk 3: Manifest 文件被恶意篡改

**[Risk]** 攻击者可修改 `pdk_manifest.json` 改变声明的工具 schema (社交工程)

**Mitigation**:
- Phase 6a 不做验签, 文档化此为已知 risk
- signature 字段已声明, 为 Phase 7+ 验签实施留位
- 与 ADR-0052 §目标 4 一致 (已识别 trust 检查需求)

### Risk 4: nlohmann::json 隐式类型转换

**[Risk]** 默认行为下 `"abi_version": "1"` (string) 会被接受为 uint32 1, silent 失败

**Mitigation**:
- Decision 2 严格类型策略: 任何类型不匹配 MUST reject with `reason="wrong_type"`
- 实施时显式使用 `j.is_number_unsigned()` / `j.is_string()` 等类型检查, 不依赖 `j.get<uint32_t>()`

### Risk 5: 12 个 plugin 全是 v2 ABI, 暂时没 v1

**[Risk]** 双 ABI 支持的 v1 路径可能无真实使用场景, 过度设计

**Mitigation**:
- 支持 v1 仅为向后兼容 (per `SUPPORTED_ABI_VERSIONS[]` 已声明)
- 测试覆盖 v1 + v2 两条路径, 但不强制任何 plugin 立即迁移 v1

## Migration Plan

### Phase 6a 阶段 1 (本 change, 3 天)

1. 实现 `ManifestValidator` (独立 TDD, 11 测试)
2. 实现 `ManifestFinder` (独立 TDD, 8 测试)
3. 集成到 PluginLoader (manifest-first 流程 + IInteractionBus setter)
4. 写 5 集成测试 (load_so + load_all + events)
5. Ship + 验证 140/143 ctest (3 pre-existing 不变)

### Phase 6a 阶段 2 (后续 follow-up change)

- 迁移 12 个 PDK plugin 各自添加 manifest
- 每个 plugin 一个 follow-up change (保持 atomic)

### Phase 6a 阶段 3 (后续优化 change)

- 启动时 warn 列表 (缺 manifest 的 plugin)
- Manifest 路径 cache (如需)
- `pdk_create_llm_provider` 符号交叉验证 (per ADR-0041)

### Rollback Strategy

- `PluginLoader::load_so()` 新增 manifest-first 分支完全可选
- 通过 `require_manifest=false` (默认) 关闭 manifest-first 行为
- 回滚方式: revert commit + 重 build (无 schema 迁移, 无数据库迁移)
- 风险: 极低 (纯加法变更, 无破坏现有 API)

## Open Questions

1. **Q1**: `entry_tool` 是否必须存在于 `provided_tools[]`? 倾向: 是 (cross-field 校验)
2. **Q2**: `implementation_forms` 缺省时如何处理? 倾向: reject (per ADR-0052 §决策 2 必填)
3. **Q3**: `resources.timeout_ms` 默认值 30000ms 是否合理? 待验证: 与现有 12 PDK plugin 实际超时对比
4. **Q4**: `trust_level` 缺省时如何处理? 倾向: warn + 默认 "untrusted"

## Architecture Compliance

- ✅ ADR-0019 §1.4 (engine.h 跨模块 include 最小化) — 新模块 `src/modules/pdk/`, 不引入反向依赖
- ✅ ADR-0022 §3.2 (PluginLoader dual-ABI) — 双 ABI 支持镜像 `SUPPORTED_ABI_VERSIONS[]`
- ✅ ADR-0031 §决策 5 (ToolCoordinator opt-in 模式) — IInteractionBus setter 注入
- ✅ ADR-0052 §决策 1-7 全部履行
- ✅ ADR-0068 事件契约 — 失败时 emit `plugin.manifest.invalid` 事件
- ✅ PIMPL 模式 (Sprint 18-19 pattern) — `PluginLoader` 修改保持 ABI 稳定

## Related Documents

- `docs/adr/adr-0052-agent-plugin-manifest.md` — manifest 格式 ADR (Source of Truth)
- `docs/adr/adr-0022-plugin-loading.md` — PluginLoader 现有机制
- `docs/adr/adr-0021-pdk-design.md` — PDK 设计
- `docs/adr/adr-0031-execution-policy.md` — ToolCoordinator opt-in 模式参考
- `docs/adr/adr-0041-pluginloader-lifecycle-extension.md` — 5 符号导出契约
- `docs/adr/adr-0068-event-emission-contract.md` — 事件契约
- `docs/audits/2026-08-09-phase6-reevaluation-post-wave3a.md` — Phase 6 启动审计
- `include/agenticdsl/plugin/plugin_info.h:76-77` — `CURRENT_ABI_VERSION=2` + `SUPPORTED_ABI_VERSIONS={1,2}`
