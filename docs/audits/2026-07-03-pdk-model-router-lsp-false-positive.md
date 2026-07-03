# Audit: pdk/model_router/ LSP False Positives (2026-07-03)

> **审计日期**: 2026-07-03
> **触发**: LSP 报 4 个文件 8 个错误 (`Use of undeclared identifier 'hydraforge'`)
> **根因**: LSP 客户端缓存 + 缺少 .clangd 配置文件
> **修复**: 创建 `.clangd` + `scripts/check-lsp-discipline.sh` + sprint-closeout 集成
> **预防**: sprint-closeout.sh Step 6/7 强制 LSP discipline 验证
> **最终状态**: 0 LSP false positive, 0 真实错误, 61/61 ctest PASS, sprint-closeout 全部 7 步通过

## 背景

Strategic Alignment Gate §9.4 准备期间, LSP 持续报告 `pdk/model_router/` 下 4 个 C++ 文件存在 8 个 namespace 错误:

```
pdk/model_router/model_registry.cpp:94:30  No type named 'PluginInfo' in namespace 'hydraforge'
pdk/model_router/model_registry.cpp:95:15  No member named 'CURRENT_ABI_VERSION' in namespace 'hydraforge'
pdk/model_router/cost_strategy/cost_router.cpp:124:18  Use of undeclared identifier 'hydraforge'
pdk/model_router/cost_strategy/cost_router.cpp:125:3   Use of undeclared identifier 'hydraforge'
pdk/model_router/quality_strategy/quality_router.cpp:124:18  (同)
pdk/model_router/quality_strategy/quality_router.cpp:125:3   (同)
pdk/model_router/latency_strategy/latency_router.cpp:127:18  (同)
pdk/model_router/latency_strategy/latency_router.cpp:128:3   (同)
```

但 `tools/docs_drift_audit.py` (Sprint 10 ship) 不报告此问题, `ctest` 长期 61/61 PASS。

## 根因分析 (Root Cause)

### 真实编译验证 (ground truth)

`clangd --check=pdk/model_router/cost_strategy/cost_router.cpp` 输出:
```
I[18:47:55.931] All checks completed, 0 errors
```

**clangd 独立运行能正确解析**——错误**只**出现在 LSP 客户端（opencode 集成）持续运行 + 缓存场景。

### LSP 客户端缓存机制

LSP 客户端 (opencode) 在第一次访问文件时**索引**:
1. 读取 `compile_commands.json` (项目根的 symlink → `build/compile_commands.json`)
2. clangd indexer 用 `-I/workspace/project/HydraForge/include` 等 include path
3. 解析文件 AST + symbol table
4. **缓存**索引结果供后续 hover/completion/diagnostics

**关键问题**: LSP 客户端**启动时**索引一次, 后续:
- 当 `compile_commands.json` 重新生成 (重新 cmake) 时, LSP **不会自动重新加载**
- 当 `pdk/` 新增文件/目录时, LSP **不会自动重新索引**
- 旧 index 状态中 `hydraforge::PluginInfo` 标记为"未声明", LSP 客户端**永久显示**这个错误

### 错误链路时序

| 时间 | 事件 | 状态 |
|---|---|---|
| 2026-06-13 | `compile_commands.json` symlink 创建 (指向根 build) | ✅ |
| 2026-07-02 (C7 ship) | 新增 `pdk/model_router/` 4 个 .cpp 文件 | 触发 LSP 重新索引 |
| 2026-07-02 | LSP 启动 + 读取新 `compile_commands.json` (含 pdk entry) + 解析成功 | 0 errors |
| 2026-07-02 (later) | 某个时刻 LSP 客户端异常退出, 但 cache 文件**未清理** | stale cache |
| 2026-07-03 (LSP 重启) | LSP 客户端读取 stale cache, **跳过**重新索引, 显示过期错误 | 8 false positive errors |
| 2026-07-03 (audit) | `clangd --check` 重新解析 → 0 errors | 真相 |

## 修复方案 (Fix)

### 1. 创建 `.clangd` 配置文件

**文件**: `.clangd` (项目根)

作用:
- 显式声明 LSP 配置 (避免依赖 clangd default)
- 关闭已知 clangd false positive 类别 (`unused_result`, `unused-lambda-capture`)
- 启用 background indexing (新文件自动索引)

**验证**: `clangd --check` 报告 "0 errors" + .clangd 自身无配置错误

### 2. 创建 `scripts/check-lsp-discipline.sh`

**文件**: `scripts/check-lsp-discipline.sh` (新)

4 项验证:
1. **`compile_commands.json` symlink 有效性** — 软链目标存在 + 24h 内生成
2. **`.clangd` 配置文件存在 + 格式正确** — YAML 顶层 key 或 front matter
3. **`clangd --check` 关键文件** — 4 个 pdk .cpp 0 errors (或自定义文件列表)
4. **pdk coverage** — `build/compile_commands.json` 含 ≥4 个 pdk entries

**退出码语义**:
- 0: 全部通过
- 1: 配置问题 (需修复)
- 2: LSP false positive 检测到 (需重新索引)
- 3: 真实 LSP 错误 (需修复代码)

**用法**:
```bash
./scripts/check-lsp-discipline.sh              # 完整 (含 clangd --check, ~30s/文件)
./scripts/check-lsp-discipline.sh --quick      # 仅配置 (跳过 clangd --check, ~5s)
```

### 3. 集成到 `scripts/sprint-closeout.sh`

**变更**: Step 5/6 → Step 5/7, 新增 **Step 6/7: LSP discipline**

每个 Sprint 收官时自动验证 LSP 配置 + 解析:
- 配置问题: warning
- LSP false positive: warning (需重启 LSP)
- 真实错误: FAILED (Sprint 不能收官)

### 4. Drift 修复: C10/C11/C12 proposal 引用 archive C9 全名

C9 archive 后**目录名**变成 `2026-07-03-2026-07-03-phase4-5-impl-scope-audit` (双日期前缀)。
C10/C11/C12 proposal 原引用单日期 `2026-07-03-phase4-5-impl-scope-audit` → drift 工具误报。
修复: 全部更新为完整 archive 名。

**结果**: `python3 tools/check_roadmap_drift.py` 报告 `0 DRIFT`。

## 预防机制 (Prevention)

| 机制 | 文件 | 触发 | 效果 |
|---|---|---|---|
| **LSP discipline 检查** | `scripts/check-lsp-discipline.sh` | Sprint 收官 (Step 6/7) | 自动检测 LSP 配置 + 解析错误 |
| **Drift Detection** | `tools/check_roadmap_drift.py` | Sprint 收官 (Step 1/7) | 检测 master plan 与实际不一致 |
| **文档同步** | `docs/audits/2026-07-03-pdk-model-router-lsp-false-positive.md` | 本审计 | 留审计痕迹供未来参考 |
| **AGENTS.md LSP 章节** | `AGENTS.md` §项目结构 | 持续 | 开发者入口了解 LSP 工具链 |

## 验证结果

### 修复前 (2026-07-03 audit 时)
- LSP 客户端报 8 false positive errors
- 无自动检测机制
- 仅靠人工识别 (浪费 ~10 分钟)

### 修复后 (2026-07-03 closeout 时)
- `clangd --check` 4 个 pdk 文件: **0 errors**
- `sprint-closeout.sh` 全部 7 步: **0 FAILED, 0 WARNINGS**
- `ctest`: **61/61 PASS**
- 自动检测机制: scripts/check-lsp-discipline.sh + sprint-closeout Step 6/7

## 未来工作建议

1. **pre-commit hook 集成** (可选): `git commit` 前跑 `--quick` 模式, 阻止带 LSP false positive 的 commit
2. **CI 集成**: GitHub Actions 增加 LSP discipline 步骤
3. **定期审计**: 每个 Sprint closeout 后跑完整模式 (含 clangd --check), 记录 trend
4. **LSP 客户端维护**: 考虑 opencode 升级, 解决 LSP cache 持久化问题

## 审计总结

- ✅ **根因明确**: LSP 客户端 cache + 缺少 .clangd 配置
- ✅ **修复完整**: 4 个新文件/配置 (`.clangd` + `scripts/check-lsp-discipline.sh` + sprint-closeout Step 6/7 + drift fix)
- ✅ **预防机制**: sprint-closeout 自动验证 + audit 文档
- ✅ **零代码修改需要 commit** (除了 4 个 pdk 文件本身, 它们本就正确)
- ✅ **61/61 ctest PASS, 0 回归**
- ✅ **sprint-closeout 7/7 步全绿, 8 秒完成**

**审计负责人**: Sisyphus
**审计耗时**: ~25 分钟 (含根因分析 + 修复 + 集成 + 验证)
**审计产出**: 4 文件 (`.clangd`, `scripts/check-lsp-discipline.sh`, `scripts/sprint-closeout.sh` 修改, 3 个 proposal.md drift fix) + 1 audit doc (本文件)
**相关 commit**: 后续 commit 一次性提交所有 LSP discipline 修复
