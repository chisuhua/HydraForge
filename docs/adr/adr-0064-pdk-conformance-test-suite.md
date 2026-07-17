# ADR-0064: PDK Conformance Test Suite

## 状态

✅ Approved (2026-07-16, 架构评审确认)

## 领域

Agent-as-Plugin 架构 / 质量保障

## 关联

- [ADR-0021 — PDK Design](../adr-0021-pdk-design.md) — PDK 宏基础
- [ADR-0052 — Agent Plugin Manifest](./adr-0052-agent-plugin-manifest.md) — manifest 格式
- [ADR-0057 — Agent 生命周期](./adr-0057-agent-lifecycle.md) — Lifecycle
- [ADR-0058 — Tool Schema Validation](./adr-0058-tool-schema-validation.md) — Schema
- [ADR-0061 — Agent 进化与固化](./adr-0061-agent-evolution-and-solidification.md) — 行为指纹
- [ADR-0062 — Agent Marketplace](./adr-0062-agent-marketplace.md) — Marketplace 上架流程
- MCP SEP-2484 — MCP 强制 spec 配套 conformance 测试

## 背景

### 问题

PDK Plugin 来源多样（团队 / Marketplace / 社区），质量参差不齐：

- **现状**：`tests/test_pdk_macros.cpp` 只测 PDK 宏本身（5/5 pass）
- **缺失**：没有针对具体 Plugin 的 conformance 测试
- **风险**：Marketplace 上架的 Plugin 可能不符合规范
- **痛点**：外部开发者不知道"什么算合规"

### 目标

定义 PDK Conformance Test Suite，让 Plugin 开发者可以验证自己的 Plugin 合规，让 Marketplace 自动拒绝不合规的包。

## 决策

### 决策 1 — Conformance Suite 结构（独立 header-only 库）

```cpp
// tests/conformance/conformance_suite.h
namespace hydraforge::pdk::conformance {

class ConformanceSuite {
public:
    // 1. Manifest 校验
    bool check_manifest(const std::string& pkg_path);
    
    // 2. ABI 版本校验
    bool check_abi_version(const PluginInfo& info);
    
    // 3. 必需导出符号校验
    bool check_required_exports(const std::string& so_path);
    
    // 4. 工具注册校验
    bool check_tool_registration(IToolRegistry& reg);
    
    // 5. Schema 校验
    bool check_input_output_schema(const ToolMetadata& meta);
    
    // 6. 行为指纹基准（基于 ADR-0061-02）
    bool check_behavioral_fingerprint(const PluginHandle& plugin);
};

} // namespace hydraforge::pdk::conformance
```

### 决策 2 — 三级别测试

| 级别 | 测试内容 | 谁运行 | 通过标准 |
|------|---------|--------|---------|
| **Level 1: Static** | manifest 格式、ABI 版本、必需符号、JSON Schema 合规、命名约定（ADR-0043） | CI（每次 commit） | 100% 必须通过 |
| **Level 2: Dynamic** | 工具调用往返、事件订阅、Lifecycle（按 ADR-0057）、Capability Discovery（ADR-0054） | Marketplace 上架前 | 100% 必须通过 |
| **Level 3: Behavioral** | 行为指纹对比（ADR-0061-02）、契约回归、性能 baseline | Marketplace 上架前 + 用户反馈时 | 偏差 < 阈值 |

**Level 1 测试项**（12 项）：
1. `manifest.json` 存在且符合 JSON Schema
2. `abi_version` 与 OS 匹配
3. 导出 `pdk_plugin_info`
4. 导出 `pdk_register_tools`
5. 工具命名遵循 ADR-0043 (slash-only)
6. `entry_tool` 在 `provided_tools` 中
7. `input_schema`/`output_schema` 是有效 JSON Schema 2020-12
8. `requires_isolation` 与形态一致（Skill 必须 true）
9. `min_host_version` ≤ OS version
10. `capabilities` 至少 1 个 tag
11. `category` 必填且在 5 维度内
12. `pdk_manifest()` 返回有效 JSON（如果实现）

**Level 2 测试项**（8 项）：
1. Plugin 加载后 `pdk_register_tools` 注册成功
2. 所有 `provided_tools` 工具可调用且返回 ToolResult 信封
3. Lifecycle 状态转换正确（loaded → initialized → registered → active）
4. Capability Discovery 可索引所有 capabilities
5. 事件订阅（IInteractionBus）正常工作
6. activation_events 触发正确
7. 卸载时无内存泄漏（ASan / valgrind）
8. 并发调用工具无 data race（TSan）

**Level 3 测试项**（5 项）：
1. 输入边界用例（空字符串、超长、特殊字符）
2. 行为指纹稳定（Hotelling T² < 阈值，跨 100 次运行）
3. 错误处理正确（错误码符合 ADR-0023）
4. 性能 baseline（p99 latency < 阈值）
5. Capability 输出与 manifest 一致

### 决策 3 — 开发者 CLI 工具

```bash
# CLI 工具
$ hf conformance check my_agent-0.2.0.hfpkg

# 输出
Level 1 (Static):     ✅ PASS  (12/12)
Level 2 (Dynamic):    ✅ PASS  (8/8)
Level 3 (Behavioral): ✅ PASS  (5/5)

总计：✅ CONFORMANCE CERTIFIED (Gold)
```

```bash
# CI 集成（GitHub Actions）
- name: PDK Conformance
  run: |
    hf conformance check build/my_agent-0.2.0.hfpkg --level=1
```

### 决策 4 — 等级徽章

| 徽章 | 要求 | Marketplace 显示 |
|------|------|------------------|
| 🥉 Bronze | Level 1 通过 | "Conformance: Bronze" |
| 🥈 Silver | Level 1 + 2 通过 | "Conformance: Silver" |
| 🥇 Gold | Level 1 + 2 + 3 通过 | "Conformance: Gold" |

**Marketplace 上架最低要求**：Bronze（Level 1 通过）。

### 决策 5 — 与 Marketplace 集成

```cpp
class MarketplaceUpload {
    ErrorCode upload(const std::string& pkg_path) {
        // 1. 必须通过 Level 1
        if (!suite.check_manifest(pkg_path)) {
            return ERR_CONFORMANCE_L1;
        }
        
        // 2. 上传后必须通过 Level 2 + 3
        schedule_level_2_3_validation();
        
        // 3. 通过后才在 Marketplace 显示
    }
};
```

## 替代方案

### 方案 A：只做 Level 1（manifest 校验）

**否决理由**：
- 不足以保证运行时正确性
- Marketplace 用户体验差

### 方案 B：conformance 测试放在 HydraForge 主仓内

**否决理由**：
- 已被 ADR-0021 §7 Dual-Repo 决策否定
- 应作为独立 PDK 库，开发者可独立测试

### 方案 C：自动 conformance（无需开发者运行）

**否决理由**：
- 开发者应在本地先验证
- CI 是二次确认，不是替代

## 不变量

- Level 1 是 Marketplace 上架的硬门槛
- Level 1 + 2 是 marketplace 显示的最低要求
- Level 3 是 Gold 徽章要求
- Conformance Suite 是 PDK 的**一部分**，随 PDK 版本同步发布

## 权衡

| 决策 | 选择 | 理由 |
|------|------|------|
| 测试级别 | 三级 | 静态 + 动态 + 行为全面 |
| 库形式 | header-only | 方便集成 |
| 徽章 | 三档 | 清晰的质量信号 |
| Marketplace 门槛 | Bronze | 不阻挡早期贡献 |

## 后续行动

- ADR-0063: OpenTelemetry Trace（Conformance Level 3 性能 baseline）
- Phase 2: 自动化 conformance 测试平台
- 集成：`hydraforge-pdk` Dual-Repo

## 参考

- ADR-0021 / 0052 / 0057 / 0058 / 0061 / 0062
- MCP SEP-2484: Conformance test suites mandatory
- ATD conformance: github.com/downsea/atd/docs/atd-architecture.md
- Loom CTK: github.com/srijithunni7182/llm4j/tree/main/loom