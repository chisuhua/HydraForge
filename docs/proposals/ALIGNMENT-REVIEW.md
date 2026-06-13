# 文档对齐审查报告

**日期**: 2026-05-23
**审查范围**: `docs/adr/`、`docs/specs/`、`docs/guides/`、`docs/README.md` 与 `docs/proposals/`

---

## 1. 整体发现

| 检查项 | 现状 | 结论 |
|--------|------|------|
| `docs/README.md` 目录列举 | 仅列 6 个子目录，实际 13 个 | ❌ 缺失 |
| `agenticdsl/README.md` 文档清单 | 仅列 6 类文档，缺失 7 类新文档 | ❌ 缺失 |
| ADR → agenticdsl 反向引用 | 18 个 ADR 均未引用 agenticdsl | ❌ 缺失 |
| 新文档 → ADR 引用 | phase-0 / security / test 等未引 ADR | ❌ 缺失 |
| `docs/README.md` 更新记录 | 最后更新 2026-05-20 | ❌ 过期 |
| `SPECS-ALIGNMENT.md` 状态 | 部分规范待更新 | ⚠️ 待更新 |

---

## 2. 详细问题

### 🔴 严重问题

#### 问题 1: `docs/ads/README.md` 缺少 7 个新目录

当前版本（第 122 行）：
```
文档组织按**话题领域**（而非文档类型），共 6 个子目录：vision/, skill-system/, session-state/, inference-stdlib/, language-extensions/, implementation-roadmap/
```

但实际有 13 个子目录，缺少：
- `research/` — 推理引擎调研报告
- `architecture/` — 推理架构、路由器、质量评估器
- `optimization/` — 优化方向方案
- `implementation/` — 自举路径、阶段 0 实施方案
- `testing/` — 测试策略
- `api/` — CloudLLMAdapter API 设计
- `operations/` — 安全规范、性能基准

#### 问题 2: `docs/proposals/README.md` 缺少 7 类新文档

文档清单仅覆盖原有 6 类（vision, skill-system, session-state, inference-stdlib, language-extensions, implementation-roadmap），缺失：

| 类别 | 文档 | 重要性 |
|------|------|--------|
| research/ | RES-001: 推理引擎调研报告 | 高 |
| architecture/ | ARCH-001: 总体推理架构 | 高 |
| architecture/ | ROUTER-001: 推理路由器设计 | 高 |
| architecture/ | QUALITY-001: 质量评估器设计 | 高 |
| optimization/ | OPT-001: 优化方向方案 | 中 |
| implementation/ | BOOT-001: 自举实施路径 | 高 |
| implementation/ | Phase 0 实施方案 | 高 |
| testing/ | TEST-001: 测试策略 | 中 |
| api/ | API-001: CloudLLMAdapter 设计 | 中 |
| operations/ | SEC-001: 安全规范 | 中 |
| operations/ | BENCH-001: 性能基准 | 低 |

#### 问题 3: ADR 文件无 agenticdsl 反向引用

18 个 ADR 文件均未引用 `docs/proposals/` 中的任何文档。

**建议添加反向引用的 ADR**：

| ADR | 应引用的 agenticdsl 文档 |
|-----|------------------------|
| ADR-0001: ILLMProvider 流式接口 | `api/cloud-llm-adapter.md`、`inference-stdlib/01-interface-design.md` |
| ADR-0004: ToolRegistry 安全模型 | `operations/security.md` |
| ADR-0005: LLM 后端配置与工厂 | `api/cloud-llm-adapter.md` |
| ADR-0009: DSL 标准库规划 | `skill-system/03-taxonomy-mapping.md`、`language-extensions/03-standard-library.md` |

---

### 🟡 中等问题

#### 问题 4: 新文档缺少 ADR 引用

| 新文档 | 应引用的 ADR |
|--------|-------------|
| `phase-0-implementation.md` | ADR-0001（ILLMProvider 接口）、ADR-0005（后端配置） |
| `security.md` | ADR-0004（ToolRegistry 安全模型） |
| `test-strategy.md` | ADR-0003（线程安全）、ADR-0004（安全模型） |
| `cloud-llm-adapter.md` | ADR-0001（流式接口）、ADR-0005（配置工厂） |
| `inference-router.md` | ADR-0001（ILLMProvider） |
| `quality-evaluator.md` | ADR-0008（结构化 Context） |

---

### 🟢 轻微问题

#### 问题 5: `docs/README.md` 更新记录过期

当前记录：`2026-05-20 | 新增 agenticdsl/ 目录（16 篇语言演进文档）`
需更新为：`2026-05-23 | 扩展至 30+ 篇，新增 research/architecture/optimization/implementation/testing/api/operations 目录`

#### 问题 6: `SPECS-ALIGNMENT.md` 未更新

`rt-guide.md` 和 `specs/architecture.md` 仍标记为待更新。

---

## 3. 修复状态

### ✅ 已修复（P0+P1）

```yaml
1. docs/README.md:
   - ✅ 6 子目录 → 13 子目录
   - ✅ 补充 research/, architecture/, optimization/, implementation/, testing/, api/, operations/
   - ✅ 更新记录：2026-05-23

2. docs/proposals/README.md:
   - ✅ 目录结构：6 → 13 子目录
   - ✅ 文档清单：补充 7 类新文档
   - ✅ 阅读顺序：补充新类别

3. 新文档添加 ADR 引用（6 篇全部修复）:
   - ✅ phase-0-implementation.md → ADR-0001, ADR-0005
   - ✅ security.md → ADR-0004
   - ✅ cloud-llm-adapter.md → ADR-0001, ADR-0005
   - ✅ quality-evaluator.md → ADR-0008
   - ✅ self-bootstrapping-path.md → ADR-0001
   - ✅ inference-architecture.md → ADR-0001
   - ✅ inference-router.md → ADR-0001
   - ✅ inference-optimization-strategies.md → ADR-0001
   - ✅ performance-benchmark.md → ADR-0001
```

### ❌ 待讨论（P2）

```yaml
4. ADR 文件添加 agenticdsl 反向引用（需议）:
   - ADR-0001 → 引 cloud-llm-adapter.md
   - ADR-0004 → 引 security.md
   - ADR-0009 → 引 skill-system/03-taxonomy-mapping.md
   - 方案：在 ADR 末尾添加 `## 关联演进文档` 章节

5. SPECS-ALIGNMENT.md 更新 | rt-guide.md 更新
```

---

## 4. ADR/specs 审查摘要

### 引用方向

```
agenticdsl ←────────── ADR
     │                      │
     │ 12 篇引用 ADR         │ 0 篇引用 agenticdsl
     │                      │
     ▼                      ▼
agenticdsl ←────────── specs
     │                      │
     │ 0 篇引用 specs        │ 0 篇引用 agenticdsl
     ▼                      ▼
```

**结论**：单向引用（agenticdsl → ADR），无反向引用。ADR 格式固定（无关联文档字段），specs 描述基线（v3.10）。

### 内容冲突检查

| specs 文档 | agenticdsl 关联 | 冲突 |
|-----------|----------------|------|
| `dsl.md` v3.10 | `language-extensions/01-type-system.md` | ✅ 无冲突 |
| `dsl-lib.md` v3.10 | `language-extensions/03-standard-library.md` | ✅ 无冲突 |
| `layer0.md` v2.2 | `session-state/` | ✅ 无冲突 |
| `architecture.md` v2.2 | `vision/` | ✅ 无冲突 |

**结论**：specs 描述当前实现，agenticdsl 描述未来提案。关系是"基线 vs 增量"，**无直接冲突**。


---

## 5. 2026-06-08 更新（C1 迁移后）

C1 迁移已于 2026-06-08 完成。以下为本次迁移对本审查报告中"已修复"项的实际落地状态：

- **公共头文件迁移**：原散落各处的 cognitive/contract 头文件已统一至 `include/agenticdsl/{cognitive,contract}/`（commit `f07a4b4`）。
- **LLM 接口统一**：引擎 LLM 接口已统一为 `ILLMProvider` 流式接口，`LLMParams` 现为 `LLMConfig` 的别名（commit `3f28020`，参见 `llm_types.h:60`）。
- **ToolResult MVP 实施**：ADR-0023 中描述的 ToolResult 标准化已部分落地（commit `fe448a0`，P1 范围）。
- **头文件清理**：`using` 声明重复导致的多重定义问题已清理（commit `13cc12e`）。

> 本节为追加更新，前文第 1-4 节所列问题描述以 C1 之前的状态为准，未追溯修改。
