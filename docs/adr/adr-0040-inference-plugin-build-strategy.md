# ADR-0040: 推理引擎 Plugin 构建与交付策略

## 状态

✅ Approved (2026-07-10 — OpenSpec change `phase5-llama-engine-plugin` (C14) ship); **2026-07-06 P1 fix**: sync-pdk.sh 关系澄清为新脚本 `sync-inference-plugin.sh`, 补 ABI 版本管理, 补 CMake 依赖管理细节; **2026-07-06 renumber**: 兄弟 ADR-0036 → ADR-0045 (编排 plugin), ADR-0037 → ADR-0046 (通信协议), 避免与旧 ADR-0036-三层服务协议 / ADR-0037-因果序冲突

> **实施依据**: `phase5-llama-engine-plugin` (C14) 已 ship + archived (2026-07-08), 验证: `pdk/llama_engine/` 作为 SHARED 库构建 (per `pdk/llama_engine/CMakeLists.txt`) + ABI v2 协调 (per C16 PluginLoader 5 符号 + ADR-0041 lifecycle 钩子) + ADR-0021 §7 Dual-Repo Policy 同步 (vendored in monorepo `pdk/`, 后续由 `scripts/sync-pdk.sh` 异步同步至 standalone `hydraforge-pdk` repo)。详见 `docs/superpowers/plans/2026-07-03-phase5-self-bootstrapping.md` §三 C14 行 + `openspec/changes/archive/2026-07-08-phase5-llama-engine-plugin/`。

## 领域

基座 / PDK / Build & Delivery

## 关联

- [ADR-0035 (Inference Engine Plugin Spec)](./adr-0035-inference-engine-plugin-spec.md)
- [ADR-0021 (PDK Design) §7 Dual-Repo Policy](./adr-0021-pdk-design.md) — `sync-pdk.sh` 基础设施
- ADR-0022 (Plugin Loading) — `pdk_plugin_info` ABI version
- AgenticLlama (llama.cpp fork) 项目

---

## 背景

AgenticLlama 是独立 CMake 项目 (llama.cpp fork), 输出 `libllama.so`。需要决定它如何作为 HydraForge PDK Plugin 构建和交付。

---

## 决策

### 1. 构建策略: AgenticLlama 作为 Dual-Repo PDK Plugin

```
AgenticLlama (独立仓库)
├── CMakeLists.txt (现有)
├── src/ (现有 llama.cpp)
├── ggml/ (现有 ggml + triton backend)
├── plugin_wrapper/  (P1 fix: 重命名自 hydraforge/ 避免命名冲突)
│   ├── CMakeLists.txt
│   ├── inference_plugin.cpp   (pdk_register_tools + pdk_create_llm_provider)
│   └── llama_wrapper.cpp       (C API → ILLMProvider 桥接)
└── output:
    └── libagenticllama_inference.so

HydraForge (monorepo)
├── include/agenticdsl/
├── plugins/agenticllama/ → PluginLoader 扫描路径
│   ├── libagenticllama_inference.so
│   └── VERSION  (P1 fix: 部署时记录版本号)
```

### 2. 链接: SHARED 库 (非 STATIC)

理由:
- PluginLoader 通过 `dlopen` 加载 `.so`
- llama.cpp 内部状态 (model/context/sampler) 需在 Plugin 生命周期内隔离
- 独立升级: AgenticLlama 可独立迭代, HydraForge 仅更新 `.so`

**MVP 限制** (P1 fix): Linux-only (dlopen)。macOS/Windows 抽象见 ADR-0022 §2 Phase 2+。

### 3. Backend 选择: 编译时决策

```cmake
# AgenticLlama plugin CMake 选项
option(HYDRAFORGE_INFERENCE_BACKEND "Backend: cpu|cuda|triton" "triton")
```

| Backend | CMake 依赖 | 适用场景 |
|---------|------------|---------|
| `cpu` | 无 | CI/开发环境/无 GPU |
| `cuda` | `find_package(CUDAToolkit REQUIRED)` | NVIDIA GPU (无 Triton) |
| `triton` | `find_package(CUDAToolkit REQUIRED)` + 自家 `triton_kernels/` (含 CUBIN) + `libcuda.so` 运行时 | NVIDIA GPU, 自研 kernel |

### 4. 交付: `sync-inference-plugin.sh` 新脚本 (P1 fix 明确与 sync-pdk.sh 区分)

**澄清** (P1 fix per Oracle review): 本脚本是**新的独立脚本**, 不是 [ADR-0021 §7 `sync-pdk.sh`](./adr-0021-pdk-design.md#7-dual-repo-policy) 的复用。两者方向相反:

| 脚本 | 方向 | 用途 | 来源 |
|------|------|------|------|
| `sync-pdk.sh` | HydraForge monorepo → standalone `hydraforge-pdk` repo | PDK SDK 同步 | [ADR-0021 §7.3](./adr-0021-pdk-design.md) |
| `sync-inference-plugin.sh` ⬅️ **新 (本 ADR)** | AgenticLlama repo → HydraForge monorepo `plugins/` | Inference plugin 同步 | 本 ADR |

```bash
# scripts/sync-inference-plugin.sh 用法
scripts/sync-inference-plugin.sh \
  --from ../AgenticLlama/plugin_wrapper/ \
  --to ./plugins/agenticllama/

# 拷贝文件 + pre-build .so + 写 VERSION 文件 + 验证 ABI
```

**脚本职责** (P1 fix):
1. 拷贝 plugin source 从 AgenticLlama → HydraForge monorepo `plugins/agenticllama/`
2. (可选) 触发 out-of-tree build 生成 `.so`
3. 写 `plugins/agenticllama/VERSION` (P1 fix 新增)
4. 验证 `PluginInfo.abi_version == CURRENT_ABI_VERSION` (per [ADR-0022 §4](./adr-0022-plugin-loading.md))

### 5. ABI 版本管理 (P1 fix)

**HydraForge 提供** `CURRENT_ABI_VERSION` 宏 (per ADR-0022 §1.2):
```cpp
namespace hydraforge {
inline constexpr uint32_t CURRENT_ABI_VERSION = 1;
}
```

**AgenticLlama 必须声明**匹配 (否则 PluginLoader 严格拒绝 per ADR-0022 §4):
```cpp
extern "C" const hydraforge::PluginInfo pdk_plugin_info = {
  hydraforge::CURRENT_ABI_VERSION,
  "agenticllama_inference",
  1, 2, 3,  // major, minor, patch
  "AgenticLlama inference engine (ggml-triton)",
  "inference.local,triton"
};
```

**版本协调流程**:
1. HydraForge 升级 `CURRENT_ABI_VERSION` → AgenticLlama release must bump major 或兼容声明
2. Breaking API change → 两边同步升级 + abi_version bump
3. HydraForge CI 加载 .so 时验证 `abi_version` + `dependencies` (P1 fix 后续可能在 PluginInfo 加 dependencies 字段)

**Version pinning**:
- `plugins/agenticllama/VERSION` 文件包含 AgenticLlama 实际版本 (semver) — 由 `sync-inference-plugin.sh` 写入
- `PluginInfo.major_version`/`minor_version`/`patch_version` 字段作为 fallback

### 6. CI 集成 (P1 fix 新增章节)

**HydraForge CI 工作流**:
1. AgenticLlama release tag 触发 HydraForge CI
2. CI checkout AgenticLlama plugin_wrapper + 编译 .so (per backend 选择)
3. `sync-inference-plugin.sh` 拷贝编译产物到 `plugins/agenticllama/`
4. 集成测试 (AgenticLlama + HydraForge 集成): ADR-0035 §8 + ADR-0045 §7 测试列表

**回滚策略** (P1 fix 新增): `.so` 损坏时, `plugins/agenticllama/` 保留上一个 VERSION + .so, 直接 `git checkout HEAD~1 plugins/agenticllama/` 回滚。HydraForge release 与 plugin 部署解耦 (release tag 不绑定 .so 版本)。

### 7. 测试策略 (P1 fix 新增章节)

| # | 测试 | 覆盖 |
|---|------|------|
| 1 | `plugin_pod_layout` | `pdk_plugin_info` ABI version 字段对齐 `CURRENT_ABI_VERSION` |
| 2 | `plugin_abi_mismatch_rejected` | 旧 .so (abi_version < CURRENT) 被严格拒绝 |
| 3 | `plugin_build_cpu_backend` | `-Dhydraforge_inference_backend=cpu` 编译成功 |
| 4 | `plugin_build_cuda_backend` | `-Dhydraforge_inference_backend=cuda` 编译成功 |
| 5 | `plugin_build_triton_backend` | `-Dhydraforge_inference_backend=triton` 编译 + CUBIN 嵌入成功 |
| 6 | `sync_inference_plugin_script` | `sync-inference-plugin.sh` 正确拷贝 + 写 VERSION |
| 7 | `plugin_integration_test` | HydraForge + AgenticLlama .so 加载 + ILLMProvider 三层链 (ADR-0035 §8) |

---

*创建日期*: 2026-07-06
*修订*: 2026-07-06 (P1 fix 应用: sync-pdk.sh vs sync-inference-plugin.sh 区分, ABI 版本管理, 目录重命名, CMake 依赖细节, CI 集成, 测试策略)
*依赖*: ADR-0035, ADR-0021 (§7 Dual-Repo), ADR-0022 (§1.2 PluginInfo + §4 abi_version check)
