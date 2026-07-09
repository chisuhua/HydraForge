# 参考报告 3: B1 lib/inference 真实状态与 B2 起点假设验证

> **来源**: explore agent (bg_594ba48f, 2m34s)
> **关联 session**: `ses_0cb0fbfdeffeWyeCdcN1Ja5kP1`
> **日期**: 2026-07-06

---

## 1. lib/inference/ 当前真实状态

### 1.1 目录内容

```bash
$ ls -la lib/inference/
# engine.md   (101 行, 2026-07-04) — PLACEHOLDER
# model.md    (108 行, 2026-07-04) — PLACEHOLDER
# session.md  (107 行, 2026-05-22) — ✅ 完整 ship
```

### 1.2 7/7 子图实际状态

| 子图 | 状态 | 行数 | 创建日期 | 备注 |
|------|------|:----:|----------|------|
| session.md | ✅ ship | 107 | 2026-05-22 | B1 唯一完整 ship 的子图 |
| engine.md | ⚪ PLACEHOLDER | 101 | 2026-07-04 | commit `245fb4d`, 5 项 B2 清单全部未勾选 |
| model.md | ⚪ PLACEHOLDER | 108 | 2026-07-04 | commit `245fb4d`, 5 项 B2 清单全部未勾选 |
| prefix_cache.md | ❌ 不存在 | — | — | C13 task 1, 未创建 |
| kv_cache.md | ❌ 不存在 | — | — | C13 task 2, 未创建 |
| decoding.md | ❌ 不存在 | — | — | C13 task 3, 未创建 |
| batching.md | ❌ 不存在 | — | — | C15 范围, 未创建 |
| cloud_engine.md | ❌ 不存在 | — | — | C13 task 4, 未创建 |

**实际覆盖率**: **1/7 ship + 2/7 占位 + 4/7 缺失** (而非 master plan 声称的 "7/7 子图覆盖率")

### 1.3 占位文件边界

两个占位文件均在 commit `245fb4d` (2026-07-04) 创建, 自创建以来**零变更**。后续 17 个 commits 全部是 docs cleanup, 未触及 `lib/inference/`。

**engine.md 边界**:
- PLACEHOLDER 标记 (5 行)
- DSL YAML subgraph (43 行, 4 节点: init_engine → check_status → handle_error/success_output)
- 说明 (34 行)
- B2 实施清单 (5 行, 全部未勾选)

**工具引用**: `tool: inference.engine_init` / `tool: inference.model_load` — 使用 `inference.*` 命名空间

---

## 2. B1 推理标准库是否真 ship 了 7/7

**否。这是 drift。**

| master plan 声称 | 仓库现实 |
|------------------|----------|
| §5.4: "3/7 ship" (已修正) | 1/7 ship (session.md) |
| §十六.5: "推理 stdlib 子图覆盖率 7/7" | 1/7 ship + 2/7 占位 + 4/7 缺失 |
| handoff line 482: `{engine,session,model}.md 模板` (已修正) | engine.md/model.md 是占位, 非模板 |

Oracle 已识别此 drift 并通过 commit `6b7f607` 在 master plan 纠正。但纠正后 B2 实施尚未开始。

---

## 3. C13 32 tasks 完成状态

| Task 组 | Tasks | 完成数 |
|---------|-------|:------:|
| 1.x prefix_cache.md schema | 3 | 0/3 |
| 2.x kv_cache.md schema | 2 | 0/2 |
| 3.x decoding.md schema | 3 | 0/3 |
| 4.x cloud_engine.md schema | 1 | 0/1 |
| 5.x docs 同步 | 3 | 0/3 |
| 6.x 验证 | 6 | 0/6 |
| 7.x Git + archive | 4 | 0/4 |
| **总计** | **32** | **0/32** |

**C13 是"规划完整但零代码"。3 个 artifacts (proposal/spec/tasks) 已全部就位, 但 4 个 schema 文件 + 1 个头文件 + 3 处文档同步全部零实施。**

---

## 4. C12 YIELD/STREAM 与 B2 batching 集成挑战

| 挑战 | 严重度 | 说明 |
|------|:------:|------|
| IGenerationStream 是单流 | 🔴 高 | 当前假设 1 次 generate_stream = 1 个 IGenerationStream, BatchingQueue 需多路并发流 |
| pull_loop 在单线程消费 | 🟡 中 | YieldStreamBridge::pull_loop 是同步 for-loop |
| CONTINUE + batching 互斥 | 🟡 中 | CONTINUE 要求连续 pull 同一 stream, BatchingQueue 合并多个请求, 语义冲突 |
| BudgetChecker 跨请求 | 🟡 中 | 当前 per-session, BatchingQueue 需 per-request |
| 无 batching 任何代码 | ✅ 确定 | `grep -r "batch\|Batch" src/` → 空 |

---

## 5. 仓库现实: 假设验证矩阵

| # | 假设 | 验证结果 | 证据 |
|---|------|---------|------|
| 1 | "lib/inference B1 已 ship 7/7" | ❌ 假。1/7 ship + 2/7 占位 + 4/7 缺失 | ls output + 32 uncompleted tasks |
| 2 | "0 个 examples 用并行 LLM" | ✅ 真 | grep 返回空 |
| 3 | "PDK v0.1.0 + PluginLoader 已 ship" | ✅ 真 | include/agenticdsl/pdk/ 存在 9 个头文件 |
| 4 | "C13 32 tasks ready to ship" | ⚠️ 规划完成, ❌ 零实施 | 32 tasks 全部 `- [ ]` |
| 5 | "llama.cpp 原生集成可用" | ❌ 假。LlamaAdapter 通过 HTTP 转发 | llama_adapter.cpp 使用 HttpLLMAdapter |
| 6 | "B2 可在 Sprint 23-24 装下" | ⚠️ 勉强。总计 5-8 天 | Sprint 容量紧张 |
| 7 | "C13 0.5-1 天估时合理" | ⚠️ 纯 schema 层面合理, 但忽略了 C14 依赖链 | 实际 head start 2-3h |

### 最关键的差距

1. **C13 零实施**: B2 启动的最直接阻塞点 — 但也是最小的阻塞点 (2-3 小时工作)
2. **C14 llama.cpp 集成**: LlamaAdapter 当前通过 HTTP 代理到外部 LLM 服务, 不是原生 llama.cpp C API。C14 的 engine plugin 要么继续走 HTTP (非真正自举), 要么解决原生 llama.cpp 集成 (尚未完成的工程)
3. **C15 不确定**: BatchingQueue 的跨线程设计是未探索领域