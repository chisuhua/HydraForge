# ADR-0062: Agent Marketplace 与包格式

## 状态

✅ Approved (2026-07-16, 架构评审确认)

## 领域

Agent-as-Plugin 架构 / 商业化与分发

## 关联

- [ADR-0052 — Agent Plugin Manifest](./adr-0052-agent-plugin-manifest.md) — manifest.json 格式
- [ADR-0053 — AgentDescriptor](./adr-0053-agent-descriptor-interface.md) — Agent 描述
- [ADR-0054 — Capability Discovery](./adr-0054-capability-discovery.md) — 能力发现
- [ADR-0055 — SKILL.md 执行与隔离](./adr-0055-skill-isolation.md) — 沙箱隔离
- [ADR-0056 — WebAssembly Agent 运行时](./adr-0056-wasm-runtime.md) — Wasm 沙箱
- [ADR-0061 — Agent 进化与固化](./adr-0061-agent-evolution-and-solidification.md) — 包内可能含 4 阶段产物
- [ADR-0064 — PDK Conformance Test Suite](./adr-0064-pdk-conformance-test-suite.md) — Marketplace 质量保障

## 背景

### 问题

Agent-as-Plugin 架构允许任何 Plugin 分发给其他用户。但目前缺少：

1. **包格式**：如何把 Agent Plugin 打包成可分发单元？
2. **签名验证**：如何确保包未被篡改、来源可信？
3. **沙箱隔离**：如何安全执行不可信包？
4. **版本管理**：如何处理兼容性和依赖？
5. **声誉系统**：如何让用户选择可信 Agent？

### 目标

定义 Agent Marketplace 的包格式、签名、沙箱、版本、声誉 5 大支柱。

## 决策

### 决策 1 — 包格式 `.hfpkg`（HydraForge Package）

```
my_agent-0.2.0.hfpkg  (实际是 tar.gz 重命名)
├── manifest.json            # 元数据 + 签名（见 ADR-0052）
├── libMyAgent.so            # Plugin C++ 二进制
├── agents/                  # DSL 工作流（可选）
│   └── react.agent.md
├── skills/                  # SKILL.md 定义（可选）
│   └── core/SKILL.md
├── wasm/                    # 固化产物（可选）
│   └── agent.wasm
├── config/
│   └── default.json          # 默认配置
├── tests/                   # 安装时验证
│   └── test_smoke.cpp
├── LICENSE                  # 许可证
└── CHANGELOG.md             # 变更日志
```

**文件格式**：
- `.hfpkg` 是 `.tar.gz` 重命名（MIME type: `application/x-hydraforge-package`）
- 压缩算法：gzip（默认），可选 zstd

### 决策 2 — manifest.json（扩展自 ADR-0052）

```json
{
  "$schema": "https://schemas.hydraforge.io/package-v1.json",
  "id": "hydraforge-team/code-review",
  "name": "Code Review Agent",
  "version": "0.2.0",
  "publisher": "hydraforge-team",
  "signature": "ed25519:MCowBQYDK2VwAyEAGb9FpmAg1d8...=",
  "trust_level": "high",
  "created_at": "2026-07-16T12:00:00Z",
  "min_host_version": "0.3.0",
  "max_host_version": "1.0.0",
  
  "contents": [
    {"path": "libMyAgent.so", "sha256": "...", "size": 245760},
    {"path": "agents/react.agent.md", "sha256": "...", "size": 4096},
    {"path": "wasm/agent.wasm", "sha256": "...", "size": 180224}
  ],
  
  "agent_metadata": {
    "forms": ["cpp", "wasm", "dsl"],
    "entry_tool": "code_review/run",
    "capabilities": ["code_review", "static_analysis"],
    "category": "axis3-review"
  },
  
  "dependencies": {
    "libFSTools": "^1.0.0",
    "libShellTools": ">=2.0.0 <3.0.0"
  },
  
  "license": "Apache-2.0",
  "homepage": "https://github.com/hydraforge-team/code-review",
  "repository": "https://github.com/hydraforge-team/code-review.git"
}
```

### 决策 3 — 签名验证（Phase 1 = ED25519）

```cpp
class PackageVerifier {
public:
    // 1. 验证 manifest 签名
    bool verify_signature(const Package& pkg, const PublicKey& pubkey);
    
    // 2. 验证每个文件的 SHA256
    bool verify_contents(const Package& pkg);
    
    // 3. 检查 trust_level
    bool check_trust(const Package& pkg);
};
```

**信任级别**：

| Level | 来源 | 行为 |
|-------|------|------|
| `high` | HydraForge 团队 / 已签名 + 白名单 | 默认允许 |
| `medium` | 已签名但不在白名单 | 敏感操作需审批 |
| `low` | 未签名但有声誉 > 50 | 所有操作需审批 |
| `untrusted` | 未签名 + 声誉 < 50 | 必须 `requires_isolation = true` |

### 决策 4 — 沙箱隔离（与 ADR-0055/0056 集成）

```cpp
class PackageSandbox {
public:
    // 解压 .hfpkg
    std::filesystem::path extract(const std::string& pkg_path);
    
    // 验证签名（trust_level = untrusted 必须）
    bool require_isolation(const Package& pkg);
    
    // 加载到隔离环境
    std::unique_ptr<IToolRegistry> load_isolated(const Package& pkg);
};
```

**沙箱级别**（按 trust_level 自动选择）：

| trust_level | 沙箱 |
|-------------|------|
| `high` | 直接加载 |
| `medium` | 进程隔离（fork + seccomp） |
| `low` | Wasm 沙箱 |
| `untrusted` | Wasm 沙箱 + capability-limited host functions |

### 决策 5 — 版本管理（SemVer + 兼容性矩阵）

```yaml
# manifest.json 的 dependencies 字段
"dependencies": {
  "libFSTools": "^1.0.0",           # 兼容 1.x.x
  "libShellTools": ">=2.0.0 <3.0.0", # 精确范围
  "libProvider": "1.2.3"             # 精确版本
}
```

**HydraForge host 版本约束**：

```json
"min_host_version": "0.3.0",
"max_host_version": "1.0.0"
```

**解决冲突**：
- 多个 Package 要求不同版本 → Marketplace 解析器找最近兼容版本
- 找不到 → 安装失败，列出所有可用版本

### 决策 6 — 声誉系统（与 Layer 4.5 集成）

每个 Agent 在 Marketplace 有 reputation score (0-100)：

```
reputation = (
    install_count * 0.2 +
    avg_rating * 0.3 +       // 用户评分（1-5 星）
    behavioral_fingerprint_stability * 0.3 +  // ADR-0061-02
    security_audit_score * 0.2               // 自动安全扫描
)
```

**存储**：
- 链下：本地 SQLite 缓存
- 链上（Phase 2）：Layer 4.5 声誉账本

## 替代方案

### 方案 A：直接用 Docker 镜像作为包

**否决理由**：
- Docker 镜像过大（~100MB+）
- 启动慢（200-500ms）
- 与 HydraForge PDK 哲学不符（PDK 是 .so/.wasm，不是容器）

### 方案 B：每个 Agent 一个 GitHub 仓库

**否决理由**：
- 版本管理复杂
- 缺少签名验证
- 用户体验差（git clone + build）

### 方案 C：MVP 不做签名（Phase 2）

**否决理由**：
- 即使 MVP 也需要基本的来源验证
- ED25519 实现简单，付出小

## 不变量

- 所有 `.hfpkg` 必须包含 `manifest.json`
- 签名验证失败必须拒绝加载
- `trust_level = "untrusted"` 的包必须强制隔离
- `min_host_version` 不满足时拒绝加载（硬约束）
- `max_host_version` 不满足时 warn 但允许（软约束）

## 权衡

| 决策 | 选择 | 理由 |
|------|------|------|
| 包格式 | tar.gz 重命名 | 简单、普遍 |
| 签名 | ED25519 | 标准、安全、高效 |
| 沙箱 | 按 trust_level 自动 | 用户无感知 |
| 版本 | SemVer | 工业标准 |
| 声誉 | 链下 + 链上 | MVP 用链下，Phase 2 升级 |

## 后续行动

- ADR-0064: Conformance Test Suite（Marketplace 包必须通过）
- ADR-0063: OTel Trace（Marketplace 包的可观测性）
- Phase 2: 声誉账本上链

## 参考

- ADR-0052 / 0053 / 0054 / 0055 / 0056 / 0061
- npm package.json: https://docs.npmjs.com/cli/v10/configuring-npm/package-json
- Cargo.toml: https://doc.rust-lang.org/cargo/reference/manifest.html
- OCI Image Spec: https://github.com/opencontainers/image-spec
- ED25519: https://ed25519.cr.yp.to/