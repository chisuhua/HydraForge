# 项目状态综合审计报告

**审计日期**: 2026-06-09
**审计范围**: 代码 (`src/`, `include/`, `tests/`, `examples/`) + 文档 (`docs/`, `AGENTS.md`) + 流程 (`openspec/`, `roadmap-status.md`)
**审计方法**: 3 个并行 explore agent (各 28-38 分钟, 200+ 次 grep) + 30+ 次直接检查
**审计时长**: ~95 分钟总耗时

---

## 0. 当前活跃状态

| 维度 | 状态 |
|------|------|
| **Git 工作区** | 14 文件已修改未提交 (12 modified + 2 untracked), 来自上一会话 `docs-code-alignment-fixes` |
| **OpenSpec** | 0 活动变更; 1 归档变更 `2026-06-09-docs-code-alignment-fixes`; 1 主 spec (`docs-code-alignment`) |
| **Roadmap** | 当前 Sprint: Phase 0 Track 0.2/0.3; Phase 1 阻塞中 |
| **构建** | ✓ Clean build, 0 warning/error |
| **测试** | 20/20 通过 (3.93 sec) |

---

## 1. 摘要: 发现 36 个问题分 4 个严重级别

| 严重度 | 数量 | 类别 |
|---|---|---|
| 🔴 阻断 (P0) | **6** | 真实代码 bug / 死代码 / 文档撒谎 / 阻塞性问题 |
| 🟠 重要 (P1) | **13** | 缺失文件 / 死链接 / TODO 残留 / 状态失实 |
| 🟡 中等 (P2) | **11** | 时间戳过期 / 目录混乱 / 格式不一致 |
| 🟢 提示 (P3) | **6** | 拼写错误 / 命名约定 |

---

## 2. 🔴 阻断级问题 (P0) — 6 项

### P0-1: `HttpLLMAdapter` 是唯一仍使用 deprecated `ILLMAdapter` 接口的实现

- **位置**: `src/common/llm/llm_adapter.h:46` 标 `[[deprecated("Use ILLMProvider (llm_types.h) instead")]]`
- **唯一继承者**: `src/common/llm/http_adapter.h:10` `class HttpLLMAdapter : public ILLMAdapter`
- **使用点**: `src/common/llm/llama_adapter.cpp:19` 实例化
- **影响**: 阻塞 `llm_adapter.h` 整个文件删除; CI 中 `Wdeprecated-declarations` 可能被全局屏蔽
- **修复**: 用 `ILLMProvider` 重写 `HttpLLMAdapter`, 然后删除 `llm_adapter.h` (节省 58 行 + 一个技术债信号)

### P0-2: 15+ 处 `std::cout << "[DEBUG] ..."` 直接写入生产代码

- **位置**: `src/modules/scheduler/topo_scheduler.cpp` 15 处
- **附加**: `markdown_parser.cpp:187`, `node_executor.cpp:304`, `context_engine.cpp:151`, `engine.cpp:105` (5 处)
- **影响**: 生产环境**污染 stdout**, 破坏 DAG 调度器输出流, 无 verbosity 开关
- **修复**: 创建 `src/common/log/` logger 门面 + CMake `SPDLOG_ACTIVE_LEVEL` 控制, 默认 release 剥除

### P0-3: `req1.md` 残留 `LLMCallNode` 死代码段 (118KB 单文件)

- **位置**: `src/modules/exports/req1.md` 在 line 2151/2388/2439/2496/2514 引用已删除的 `LLMCallNode`
- **历史**: `LLMCallNode` 在 v3.10 已被 `DSLNode` 替代
- **修复**: 删除或加 `<!-- ARCHIVED: superseded by DSLNode (v3.10) -->` 横幅

### P0-4: ADR-0020 README 标记与代码严重不符

- **README 标记**: "🔄 部分实施"
- **实际**: `ThreadModel` / `HarnessEngine` 在 src/ 中**0 命中**
- **修复**: ADR-0020 改为 "❌ 未实施"

### P0-5: 5 处 `AgenticOS_*` 死链指向不存在的文件

- `docs/specs/layer0-refactor.md:6,9,1595` 引用 `AgenticOS_Layer0_Spec.md` / `AgenticOS_Architecture.md`
- `docs/specs/layer0.md:672` 引用 `AgenticOS_Layer0_RefactoringPlan.md`
- **修复**: 路径替换 (可脚本化)

### P0-6: ADR-0001 日期字面量未替换

- `docs/adr/adr-0001-illm-provider-streaming-interface.md:5` 显示 `**已批准** (YYYY-MM-DD)`
- **修复**: 替换为 `2026-05-28` 或最新日期

---

## 3. 🟠 重要级问题 (P1) — 13 项

### 代码侧 (3)

#### P1-1: 4 处死代码/死引用清理
- `src/modules/prompts.yaml` (24 KB, **0 C++ 引用**) → 删除
- `tests/test_prompt_builder.cpp` (8 行 stub, 引用不存在的 `prompt_builder.h`) → 删除或实施
- `examples/agent_loop/tmp.md` (2.6 KB, **0 引用**) → 删除
- `src/modules/exports/req1.md` 中 `LLMCallNode` 段落 → 清理或归档 (见 P0-3)

#### P1-2: 5 处注释掉的死代码
- `src/modules/scheduler/execution_session.cpp:5` 注释掉的 `#include`
- `src/modules/executor/node_executor.cpp:259,261` 注释掉的 `PromptBuilder` 调用
- `src/common/utils/yaml_json.cpp:46,72,75` 注释掉的 `std::cerr`
- `src/modules/library/library_loader.cpp:78` 注释掉的警告
- 修复: 删除 5 处死注释

#### P1-3: `AGENTICDSL_SKILL_PROGRAMMING_GUIDE.md` (1248 行) 几乎零引用
- README 标"早期文档, 保留"; `skill-system/04-skill-compiler-design.md` 已覆盖其内容
- 建议: 重命名为 `_archive` 或并入新指南

### 文档侧 (6)

#### P1-4: 4 处 OpenSpec 链接路径未指向 archive/
- `docs/SPECS-ALIGNMENT.md:113`, `docs/roadmap-status.md:262`, `docs/adr/adr-0029.md:6`, `docs/adr/adr-0035.md:6`
- 全部写 `openspec/changes/docs-code-alignment-fixes/`, 实际文件已移至 `openspec/changes/archive/2026-06-09-docs-code-alignment-fixes/`

#### P1-5: `roadmap-status.md:255` "跨 Phase 活跃变更"措辞误导
- 标题暗示"活跃", 但 OpenSpec 0 活动变更
- 建议: 改为"最近完成的 OpenSpec 变更"

#### P1-6: `app-dev-guide.md:6` 时间戳过期 7 个月
- 文件本身 2026-06-08 更新过, 但行内 `**最后更新**：2025年11月10日` 严重过期

#### P1-7: 6 个 `lib/` 子图未被 parser 注册
- `lib/auth/verify_session.md`, `lib/human/{clarify_input,confirm_action}.md`, `lib/inference/{engine,model,session}.md`
- parser 仅注册 `noop` 和 `add` 2 个

#### P1-8: `adr-0027-three-modes.md` 死链
- `adr-0031-execution-policy.md:423` 和 `adr-0032-cost-collector.md:406` 引用该文件
- 实际不存在 (编号空缺)

#### P1-9: 4 处 ADR 相对路径错误
- `adr-0015-iper-loop.md:89`, `adr-0020:537` 等引用 `../adr-0030` (少一层 `adr/`)

### ADR 状态 (4)

#### P1-10: 9 个 ADR-0010~0018 共 1743 行"愿景性"设计, 代码 0 命中

#### P1-11: 5 个 ADR-0030/0032/0034/0036 同样"愿景性"

#### P1-12: 4 处死测试/示例入口
- `adr-0031` 引用未实现的 `IExecutionPolicy` (影响 `icognitive_orchestrator.h` 编译)
- **建议**: 提供 3 个默认实现 (PlanMode/AgentMode/YoloMode)

#### P1-13: ADR-0003 标记"已批准"但 DSLEngine 0 线程原语
- 与 ADR-0020 同类问题

---

## 4. 🟡 中等问题 (P2) — 11 项

### 代码 (3)
- **P2-1**: 验证 CMake `Wdeprecated-declarations` 是否启用
- **P2-2**: 更新 `simple_orchestrator.{h:5, cpp:105}` 的 TODO 状态
- **P2-3**: `examples/skill_porting/` 在 README 描述与实际不符

### 文档 (8)
- **P2-4**: ADR-0010~0018 文档自身"##状态"节仍写"已批准"未同步
- **P2-5**: `docs/SPECS-ALIGNMENT.md:76-80` 5 处 `TBD` 占位
- **P2-6**: `phase-0-implementation.md:628` 残留 TODO
- **P2-7**: 11 个 ADR 2026-05-12~2026-05-23 间未更新 (> 1 个月)
- **P2-8**: `app-dev-guide.md:473` API key 示例 `xxx` → `<your-api-key>`
- **P2-9**: 9 个 archive 顶层文件零引用
- **P2-10**: `agenticdsl/operations/{performance-benchmark,security}.md` 死链
- **P2-11**: `agenticdsl/api/cloud-llm-adapter.md:573,574` 死链

---

## 5. 🟢 提示级问题 (P3) — 6 项

- **P3-1**: `archive/AgenticDLS_v3.0_vs_v2.3.md` 拼写错误 (DLS → DSL)
- **P3-2**: `AGENTICDSL_ENHANCEMENT_ROADMAP.md` (2179 行) 几乎零引用
- **P3-3**: 4 个 `docs/compiler/plan-phase{1-4}.md` 20-23 行占位
- **P3-4**: ADR 状态字段格式不统一
- **P3-5**: `implementation-roadmap.md:626,633` TODO(mvp) 验证项
- **P3-6**: `docs/compiler/*` 是否应进入 ADR 化

---

## 6. 实施成本估算

| 类别 | 数量 | 估时 |
|------|------|------|
| **P0 一键修复** (路径/标记/日期) | 7 处 | 10 分钟 |
| **P0 重构** (HttpLLMAdapter→ILLMProvider) | 1 文件 | 1-2 天 |
| **P0 重构** (日志门面替换 20+ 处) | 5 文件 | 1-2 天 |
| **P1 文件删除** | 3 文件 / 26 KB | 5 分钟 |
| **P1 ADR 批量废弃** (0010~0018, 0030/0032/0034/0036) | 14 文件 | 30 分钟 |
| **P1 实施 CostCollector** | 1 模块 | 0.5-1 天 |
| **P1 实施 IExecutionPolicy 3 默认实现** | 3 文件 | 1-2 天 |
| **P1 链接/路径修复** | 14 处 | 20 分钟 |
| **P2 文档小修** | ~15 处 | 1 小时 |
| **P3 清理** | ~10 文件 | 30 分钟 |
| **总计** | — | **约 5-7 工作日** |

---

## 7. 优先级建议 (按 ROI 排序)

### 立即可执行 (1-2 小时)

1. **批量修复 P0 一键项** (AgenticOS_* 路径替换 + ADR-0001 日期 + ADR-0020 标记)
2. **删除 P1 死文件** (`prompts.yaml` 24KB + `tmp.md` 2.6KB + `test_prompt_builder.cpp` stub)
3. **修复 4 处 OpenSpec 链接路径** (指向 archive/)

### 短期 Sprint (2-3 天)

4. **批量废弃 ADR-0010~0018 + ADR-0030/0032/0034/0036** 头部加废弃说明
5. **实施 CostCollector** (与 BudgetController 集成)
6. **实施 IExecutionPolicy 3 个默认实现** (解锁 Cognitive 模块)
7. **日志门面替换 20+ 处 std::cout** (引入 `agenticdsl::log` 模块)

### 中期 (1-2 周)

8. **`HttpLLMAdapter` → `ILLMProvider` 迁移** (可删除 `llm_adapter.h`)
9. **lib/ 未注册子图清理** (6 个孤儿)
10. **docs/compiler/* 决策** (进入 ADR 化或归档)

---

## 8. 后续建议: OpenSpec 变更跟踪

由于本次审计又发现 36 个新问题 (超出上次 `docs-code-alignment-fixes` 变更范围), 建议**创建新 OpenSpec change** 跟踪:

- **`code-tech-debt-cleanup`**: 聚焦 P0/P1 修复 (日志门面、ADR 批量废弃、HttpLLMAdapter 迁移)
- **`docs-stale-cleanup`**: 聚焦 P1/P2 文档清理 (链接修复、时间戳、状态节)
- **`adr-implementation-batch`**: 聚焦 P1 实施工作 (CostCollector、IExecutionPolicy、Session 骨架)

---

## 9. 未审计范围 (留给后续)

- `docs/agenticdsl/` 16+ 篇语言演进文档深度审计
- `docs/compiler/` 10 文件与 superpowers archive 关系
- `external/` 第三方依赖状态 (llama.cpp / yaml-cpp / inja / nlohmann_json)
- `examples/` 4 个示例的 CMake 集成状态
- 性能基准 / 安全审计专项

---

## 附录: 审计元数据

- **审计触发**: 用户请求"请检查项目任务执行状态,是否有代码债务,或者文档需要清理"
- **前次变更**: `openspec/changes/archive/2026-06-09-docs-code-alignment-fixes/` (19 个问题已修复归档)
- **本审计范围**: 前次变更之外的剩余问题
- **agent session IDs**:
  - bg_f7a62abc: 代码技术债 (28m 21s)
  - bg_217d3351: 文档清理点 (37m 49s)
  - bg_49e9cfee: ADR 状态一致性 (29m 10s)
- **总 agent 时间**: 95m 20s
- **直接工具调用**: 30+ 次 (grep/bash/glob)
- **报告生成**: 2026-06-09
- **报告作者**: Sisyphus (MiniMax-M3)
