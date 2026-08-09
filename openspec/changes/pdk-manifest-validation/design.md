# pdk-manifest-validation Design

## Context

### 当前状态

- `PluginLoader` (位于 `src/common/plugin_loader/plugin_loader.cpp`) 通过 `dlopen` → `dlsym(pdk_plugin_info)` → ABI 检查的流程加载 plugin
- `PluginInfo` POD (per `plugin-loader` spec) 仅含 `name`, `version`, `abi_version`, `capabilities` 4 类基础元数据
- 现有 6 个 PDK plugin (`pdk/llama_engine/`, `pdk/model_router/`, `pdk/loop_agent/`, `pdk/fs_tools/`, `pdk/shell_tools/`, `pdk/provider_agent/`) 无 manifest 文件
- 加载流程 **无法** 在 dlopen 之前做版本/信任/工具 schema 校验
- 服务化场景 (Phase 6c) 要求 OS 能远程发现 plugin 元数据(per ADR-0052 §背景)

### 约束 (per AGENTS.md)

- C++20 + CMake 3.20+
- 2 空格缩进,中文注释
- nlohmann::json 已 vendored,不引入新依赖
- 现有 PluginLoader API 必须向后兼容(同 Sprint 18/19 PIMPL pattern)

### 利益相关方

- Phase 6b AgentForge (下游消费者)
- Phase 6c 服务化重评(消费 manifest)
- 现有 6 个 PDK plugin 维护者(向后兼容关键)

## Goals / Non-Goals

### Goals

1. 定义 `pdk_manifest.json` 文件格式与 JSON Schema
2. 实现 `ManifestValidator` (独立类,可复用)
3. 修改 `PluginLoader::load_so()` 流程为 manifest-first (per ADR-0052 §决策 4)
4. 现有 plugin **不**被强制迁移(向后兼容,缺 manifest 仅 warn)
5. 5+ 单元测试覆盖 validator 全部分支路径
6. 1 集成测试覆盖 PluginLoader manifest-first load

### Non-Goals

- ❌ `.hfpkg` 包格式(ADR-0052 §决策 1 提及,Phase 6a 不做)
- ❌ Wasm 嵌入 manifest(Phase 6a 不做)
- ❌ Trust 签名验证(仅声明字段)
- ❌ 现有 6 个 PDK plugin 迁移(后续 follow-up)
- ❌ 跨 plugin 资源协调(仅记录声明)

## Decisions

### Decision 1: Manifest 文件位置 — 插件根目录(per ADR-0052 §决策 1)

**选择**: `pdk_manifest.json` 必须与 Plugin `.so` 同目录或上层目录(允许 `.so` 在 `build/` 而 manifest 在源码根)

**理由**:
- 符合 ADR-0052 规范
- 允许 src/manifest + build/.so 分离布局
- 检测算法: 从 `.so` 路径向上查找直到找到 manifest 或达到根

**替代方案考虑**:
- 强制 manifest 在 `.so` 同目录 → 拒绝 `build/` 布局,过于严格
- 仅支持 `*.manifest.json` 后缀 → 增加识别复杂度,无收益

### Decision 2: Validator 设计 — 独立类 + Result<T, Error> 模式

**选择**: `ManifestValidator` 独立类,提供静态方法 `validate(json_content) -> ManifestValidationResult`

```cpp
struct ManifestValidationResult {
  bool valid;
  std::optional<Manifest> manifest;       // valid=true 时填充
  std::vector<ValidationError> errors;    // valid=false 时填充
};
```

**理由**:
- 独立类便于单测(无 PluginLoader 依赖)
- Result 模式比抛异常更适合热路径(避免 stack unwinding 开销)
- 与 ADR-0068 EventBuilder 链式 API 风格一致

**替代方案考虑**:
- 抛异常 → 性能 + 调用方必 try-catch,过于重
- 输出 bool + 写日志 → 不可恢复错误信息丢失,不可单测

### Decision 3: PluginLoader 加载流程前移 — manifest-first(per ADR-0052 §决策 4)

**选择**: 修改 `load_so()` 流程为:
1. 从 `.so` 路径推导 manifest 路径(向上查找)
2. 读 manifest JSON(找不到 → 仅 warn,继续 dlopen 旧流程)
3. 调 `ManifestValidator::validate()`
4. 不通过 → 拒绝 load(返回 false + emit error event)
5. 通过 → `dlopen` + `dlsym(pdk_plugin_info)` + 交叉验证
6. PluginInfo.abi_version 与 manifest.abi_version 不一致 → emit warn(以 PluginInfo 为准)

**理由**:
- 校验前置: 拒绝不合规 manifest 早于 dlopen,避免加载垃圾 .so
- 向后兼容: 缺 manifest 不阻塞,旧 plugin 可渐进迁移
- 交叉验证: 双源元数据不一致时 warn 但不 fail (per ADR-0052 §决策 4)

**替代方案考虑**:
- 强制 manifest(无 manifest 即拒绝) → 阻塞现有 6 个 plugin,违反向后兼容目标
- 完全异步预读 → 增加复杂度,本阶段无收益

### Decision 4: 错误处理 — emit BusEvent 而非 throw

**选择**: Manifest 校验失败时通过 `IInteractionBus::emit()` 发送 `plugin.manifest.invalid` 事件,payload 含 `path` + `errors[]`

**理由**:
- 与 ADR-0068 事件契约一致(Canonical Topic Registry 模式)
- 可观测性: 失败原因可通过 EventHandler / TUI 渲染
- 不污染调用方异常栈

**替代方案考虑**:
- 仅返回 false → 错误信息丢失
- throw exception → 与现有 PluginLoader 风格不一致(load_so 已返回 bool)

### Decision 5: Manifest 字段定义 — minimum viable + ADR-0052 全字段

**选择**: v1 manifest 字段:
```json
{
  "$schema": "https://schemas.hydraforge.io/pdk-manifest-v1.json",
  "id": "string (kebab-case)",
  "name": "string (human-readable)",
  "version": "semver string",
  "abi_version": "uint32",
  "min_host_version": "semver string",
  "max_host_version": "semver string",
  "tools": [
    {
      "name": "string (kebab-case)",
      "description": "string",
      "input_schema": "JSON Schema object",
      "output_schema": "JSON Schema object (optional)",
      "approval_policy": "string enum [always|plan|agent|yolo]"
    }
  ],
  "resources": {
    "timeout_ms": "uint32 (default 30000)",
    "max_concurrency": "uint32 (default 1)"
  },
  "signature": "string (optional, Phase 6a 仅记录不验签)"
}
```

**理由**:
- 覆盖 ADR-0052 §决策 2 全部必填字段
- tools[] 携带 input_schema 为 Phase 6c 服务化(MCP 暴露) 准备
- signature 字段预留但 Phase 6a 不验签

## Risks / Trade-offs

### Risk 1: 现有 6 个 PDK plugin 缺 manifest 时行为

**[Risk]** 缺 manifest 的 plugin 仍可加载(warn-only),但失去前置校验保护

**Mitigation**:
- 本 change 不强制迁移,显式文档化"建议但非强制"
- 后续 follow-up change 单独迁移每个 plugin
- 在 `agenticdsl_core` 启动日志中显式列出缺 manifest 的 plugin

### Risk 2: 向上查找 manifest 的 I/O 开销

**[Risk]** 每次 load_so 都要做文件系统 stat + 读取,可能成为热点

**Mitigation**:
- 查找结果 cache (per PluginLoader 实例,LRU)
- cache key = `.so` 绝对路径
- Phase 6a 不实现 cache(如需,后续优化 change)

### Risk 3: Manifest 文件被恶意篡改

**[Risk]** 攻击者可修改 `pdk_manifest.json` 改变声明的工具 schema(社交工程)

**Mitigation**:
- Phase 6a 不做验签,文档化此为已知 risk
- signature 字段已声明,为 Phase 7+ 验签实施留位
- 与 ADR-0052 §目标 4 一致(已识别 trust 检查需求)

### Risk 4: nlohmann::json 性能开销

**[Risk]** JSON 解析对热路径(每次 load_so)有 CPU 开销

**Mitigation**:
- 已 vendored 且广泛使用,benchmark 显示 ~1-2μs/小 JSON (足够)
- 编译时 -O2 + JSON 路径仅在 load_so 时执行(非运行时热点)

## Migration Plan

### Phase 6a 阶段 1 (本 change, 2-3 天)

1. 实现 `ManifestValidator` (独立 TDD)
2. 实现 manifest 路径查找 + 读取(集成到 PluginLoader)
3. 写 5+ 单元测试 + 1 集成测试
4. Ship + 验证 140/143 ctest(3 pre-existing 不变)

### Phase 6a 阶段 2 (后续 follow-up change)

- 迁移 `pdk/llama_engine/` 添加 manifest
- 迁移 `pdk/model_router/` 添加 manifest
- 迁移 `pdk/loop_agent/` 添加 manifest
- 迁移 `pdk/fs_tools/` 添加 manifest
- 迁移 `pdk/shell_tools/` 添加 manifest
- 迁移 `pdk/provider_agent/` 添加 manifest

### Phase 6a 阶段 3 (后续优化 change)

- 启动时 warn 列表 (缺 manifest 的 plugin)
- Manifest 路径 cache (如需)

### Rollback Strategy

- `PluginLoader::load_so()` 新增 manifest-first 分支完全可选
- 通过 `require_manifest=false` (默认) 关闭 manifest-first 行为
- 回滚方式: revert commit + 重 build (无 schema 迁移,无数据库迁移)
- 风险: 极低(纯加法变更,无破坏现有 API)

## Open Questions

1. **Q1**: `pdk_manifest.json` 路径是相对 plugin 名还是绝对 `.so` 路径?
   - 倾向: 相对 `.so` 路径向上查找(per Decision 1)
   - 待验证: 实装后看 6 个 plugin 的目录布局

2. **Q2**: manifest 中 `input_schema` 使用哪个 JSON Schema 草案?
   - 倾向: Draft 2020-12(与 MCP RC 2026-07-28 一致,ADR-0073 引用)
   - 待验证: 与 ADR-0073 spec 对齐(若已 ship)

3. **Q3**: `resources.timeout_ms` 默认值 30000ms 是否合理?
   - 倾向: 是(per ADR-0031 §决策 8)
   - 待验证: 与现有 PDK plugin 实际超时对比

## Architecture Compliance

- ✅ ADR-0019 §1.4 (engine.h 跨模块 include 最小化) — `manifest.h` 放 `include/agenticdsl/pdk/`,避免 `src/modules/` 反向依赖
- ✅ ADR-0052 全部 §决策 1-2 落地
- ✅ ADR-0068 事件契约 — 失败时 emit `plugin.manifest.invalid` 事件
- ✅ PIMPL 模式 (Sprint 18-19 pattern) — `PluginLoader` 修改保持 ABI 稳定

## Related Documents

- `docs/adr/adr-0052-agent-plugin-manifest.md` — manifest 格式 ADR
- `docs/adr/adr-0022-plugin-loading.md` — PluginLoader 现有机制
- `docs/adr/adr-0068-event-emission-contract.md` — 事件契约
- `docs/audits/2026-08-09-phase6-reevaluation-post-wave3a.md` — Phase 6 启动审计
