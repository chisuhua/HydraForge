# Tasks: ADR-0034 — IModelRouter 模型路由 Plugin

> **STATUS: ACTIVE** 🟢 — Oracle Q1-Q4 决策后填充完成, ready for Sprint 17 Day 1
> **预估工时**: 2.5 人天 (Sprint 17 主体)
> **关联 master plan**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C7
> **前置依赖**: 无硬依赖 (PDK Sprint 4 + PluginLoader Sprint 5 + ToolRegistry V2 Sprint 16 C6 — 全部已 ship)

---

## 1. 决策前置 (Sprint 17 Day 0)

- [x] 1.1 业务确认: 3 种路由策略全部 ship (Oracle Q2 决策, 2026-07-02)
- [x] 1.2 决策: 3 独立 Plugin (.so), 分层工具名 `model_router/<strategy>` (Oracle Q1 决策 — Option B, 2026-07-02)
- [x] 1.3 评估: Dual-Repo 同步机制 — `scripts/sync-pdk.sh` 零改动 (Oracle Q3 决策, 2026-07-02)

## 2. 详细制定 (Sprint 17 Day 0 — 已完成)

- [x] 2.1 写 proposal.md: What Changes 5 节 + Oracle 决策引用 (2026-07-02)
- [x] 2.2 写 design.md: 5 个 Decision (接口位置 / Plugin 入口 / 状态模型 / 错误模型 / 测试策略, 2026-07-02)
- [x] 2.3 写 tasks.md: 12 sections, 47 tasks (2026-07-02)
- [x] 2.4 写 specs/model-router-plugin/spec.md: 6 ADDED Requirements × 2-4 Scenarios (2026-07-02)
- [x] 2.5 移除所有 PLACEHOLDER 标记, 更新 STATUS 行 (2026-07-02)

---

## 3. IModelRouter 接口定义 (Sprint 17 Day 1 — 0.5 人日)

### 3.1 创建 `include/agenticdsl/pdk/model_router.h`

- [ ] 3.1.1 创建 `agenticdsl::pdk` 命名空间下的 `RoutingContext` struct (7 字段: task_type, session_id, max_tokens, budget_remaining, required_tags, preferred_model, is_fleet_mode, 全部 optional 除 task_type)
- [ ] 3.1.2 创建 `agenticdsl::pdk::ModelCapability` struct (9 字段: model_id, model_name, n_ctx, max_tokens, supports_streaming, supports_function_call, per_token_cost, avg_latency_ms, tags)
- [ ] 3.1.3 创建 `agenticdsl::pdk::IModelRouter` 抽象类 (2 纯虚: `route(RoutingContext, vector<ModelCapability>) → string`, `name() → string`)
- [ ] 3.1.4 创建 `agenticdsl::pdk::ModelRoutingError` 异常类 (继承 std::runtime_error, 3 Code enum: NoViableModel/ProviderUnavailable/AmbiguousCapability, static make_message helper)
- [ ] 3.1.5 在 `include/agenticdsl/pdk/pdk.h` 添加 `#include <agenticdsl/pdk/model_router.h>` (1 行)

### 3.2 CMake 验证

- [ ] 3.2.1 确认 `pdk/CMakeLists.txt` 的 `target_include_directories` 已覆盖 `include/agenticdsl/pdk/` 路径 (无需修改, PDK INTERFACE 库已有)

### 3.3 同步验证

- [ ] 3.3.1 执行 `bash scripts/sync-pdk.sh --dry-run` 确认新头文件被纳入同步范围 (零脚本改动)

---

## 4. Runtime 数据抽象 (Sprint 17 Day 1 — 0.1 人日)

### 4.1 MockLLMProvider 扩展测试 hook

- [ ] 4.1.1 在 `src/common/llm/mock_provider.h` 添加 `void set_available_models(std::vector<ModelInfo> models)` 公开方法
- [ ] 4.1.2 在 `src/common/llm/mock_provider.cpp` 实现 `set_available_models()`: 存储传入 models 到私有成员 `std::vector<ModelInfo> test_models_`
- [ ] 4.1.3 重构 `MockLLMProvider::available_models()` 返回 `test_models_` (若非空) 否则返回默认模型列表
- [ ] 4.1.4 验证: `make -j$(nproc)` 零编译错误, `ctest --output-on-failure` 零回归

---

## 5. DefaultModelRouter — cost Plugin (Sprint 17 Day 2-3 — 0.33 人日)

### 5.1 创建 `pdk/model_router/cost_strategy/cost_router.h`

- [ ] 5.1.1 创建 `CostModelRouterPolicy` 类: 继承 `agenticdsl::pdk::IModelRouter`, 实现 `route()` (最低 per_token_cost + tag match) 和 `name()` 返回 `"cost"`
- [ ] 5.1.2 `route()` 算法: (1) 过滤 `required_tags` (所有 tag 必须在 model.tags 中), (2) 过滤 `budget_remaining` (per_token_cost ≤ budget), (3) 排序 per_token_cost asc, (4) 返回第一个, (5) 空结果 throw ModelRoutingError(NoViableModel)

### 5.2 创建 `pdk/model_router/cost_strategy/cost_router.cpp`

- [ ] 5.2.1 实现 `pdk_register_tools(IToolRegistry&)`: (1) 构造 `ToolMetadata` meta (name="model_router/cost", category=ReadOnly, approval=yolo), (2) `make_shared<CostModelRouterPolicy>()`, (3) `registry.register_tool_function("model_router/cost", meta, lambda)`
- [ ] 5.2.2 lambda 体: (1) 从 `args` 解析 `RoutingContext` (helper: `parse_routing_context(const json&)`), (2) 从 `args["candidates"]` 解析 `vector<ModelCapability>`, (3) 调用 `router->route(ctx, candidates)`, (4) 返回 `{{"model_id", result}, {"router", "cost"}}`

### 5.3 创建 `pdk/model_router/cost_strategy/CMakeLists.txt`

- [ ] 5.3.1 `add_library(hydraforge_model_router_cost SHARED cost_router.cpp cost_router.h)`, `target_link_libraries(PRIVATE hydraforge_pdk nlohmann_json)`, `target_include_directories(PRIVATE ${PROJECT_SOURCE_DIR}/include)`

### 5.4 集成到 `pdk/CMakeLists.txt`

- [ ] 5.4.1 在 `pdk/CMakeLists.txt` 末尾添加 `add_subdirectory(model_router/cost_strategy)` (1 行)

### 5.5 构建验证

- [ ] 5.5.1 执行 `cmake --preset debug && make -j$(nproc)` 确认 `libhydraforge_model_router_cost.so` 生成
- [ ] 5.5.2 确认 `nm -D build/lib/libhydraforge_model_router_cost.so | grep pdk_register_tools` 显示 `T pdk_register_tools` (exported 符号)

---

## 6. QualityModelRouter Plugin (Sprint 17 Day 4-5 — 0.33 人日)

### 6.1 创建 `pdk/model_router/quality_strategy/quality_router.h`

- [ ] 6.1.1 创建 `QualityModelRouterPolicy` 类: 继承 `IModelRouter`, `route()` 按 tag 匹配度排序, `name()` 返回 `"quality"`
- [ ] 6.1.2 `route()` 算法: (1) 对每个 candidate 计分 = count(required_tags ∩ candidate.tags), (2) 若所有分数 = 0 → fallback 返回 candidates[0].model_id + emit warning, (3) 若 empty required_tags → 按 `n_ctx + max_tokens` 总分排序, (4) 返回最高分模型

### 6.2 创建 `pdk/model_router/quality_strategy/quality_router.cpp`

- [ ] 6.2.1 实现 `pdk_register_tools(IToolRegistry&)`: 注册 `model_router/quality` (meta: ReadOnly + yolo approval), lambda 调用 QualityModelRouterPolicy::route

### 6.3 创建 `pdk/model_router/quality_strategy/CMakeLists.txt`

- [ ] 6.3.1 `add_library(hydraforge_model_router_quality SHARED quality_router.cpp quality_router.h)` + link hydraforge_pdk + nlohmann_json

### 6.4 集成

- [ ] 6.4.1 在 `pdk/CMakeLists.txt` 添加 `add_subdirectory(model_router/quality_strategy)`

### 6.5 构建验证

- [ ] 6.5.1 `make -j$(nproc)` 确认 `libhydraforge_model_router_quality.so` 生成, exported 符号正确

---

## 7. LatencyModelRouter Plugin (Sprint 17 Day 6 — 0.33 人日)

### 7.1 创建 `pdk/model_router/latency_strategy/latency_router.h`

- [ ] 7.1.1 创建 `LatencyModelRouterPolicy` 类: 继承 `IModelRouter`, `route()` 按 avg_latency_ms 排序, `name()` 返回 `"latency"`
- [ ] 7.1.2 `route()` 算法: (1) 过滤 `required_tags` (与 cost 同), (2) 过滤 `max_latency` (从 RoutingContext 或 args 解析, 若设限), (3) 排序 avg_latency_ms asc, (4) 返回第一个, (5) 空结果 throw NoViableModel

### 7.2 创建 `pdk/model_router/latency_strategy/latency_router.cpp`

- [ ] 7.2.1 实现 `pdk_register_tools(IToolRegistry&)`: 注册 `model_router/latency` (meta: ReadOnly + yolo approval)

### 7.3 创建 `pdk/model_router/latency_strategy/CMakeLists.txt`

- [ ] 7.3.1 `add_library(hydraforge_model_router_latency SHARED latency_router.cpp latency_router.h)` + link hydraforge_pdk + nlohmann_json

### 7.4 集成

- [ ] 7.4.1 在 `pdk/CMakeLists.txt` 添加 `add_subdirectory(model_router/latency_strategy)`

### 7.5 构建验证

- [ ] 7.5.1 `make -j$(nproc)` 确认 `libhydraforge_model_router_latency.so` 生成

---

## 8. ModelRegistry 工具 (Sprint 17 Day 6 — 0.1 人日)

### 8.1 创建 `pdk/model_router/model_registry.cpp`

- [ ] 8.1.1 使用 `DECLARE_TOOL` 宏注册 `model_router/registry`: category=ReadOnly, approval_policy="agent"
- [ ] 8.1.2 实现 body: (1) 从 context layer / Provider 获取 `available_models()` 列表, (2) 若 args 含 `"tag"` 参数, filter by tag, (3) 返回 json array (每元素含 model_id/model_name/n_ctx/tags)
- [ ] 8.1.3 处理 `tag` 参数缺失: 返回全部模型

### 8.2 创建 `pdk/model_router/CMakeLists.txt` (父 CMake)

- [ ] 8.2.1 在 `pdk/model_router/CMakeLists.txt` 添加 `add_library(hydraforge_model_registry SHARED model_registry.cpp)` (或直接 add_subdirectory 到各 strategy 目录的父级)

### 8.3 构建验证

- [ ] 8.3.1 `make -j$(nproc)` 确认 `libhydraforge_model_registry.so` 生成

---

## 9. 集成验证 (Sprint 17 Day 7 — 0.5 人日)

### 9.1 升级 `examples/phase1_model_router_plugin/main.cpp`

- [ ] 9.1.1 删除 Sprint 0 stub (`agenticdsl::plugin_stub::ModelRouterPolicy` 类, ~30 行)
- [ ] 9.1.2 重构为 Plugin 加载演示: (1) 使用 `PluginLoader` 加载 3 个 `.so`, (2) 依次调用 `call_tool("model_router/cost", ...)`, `call_tool("model_router/quality", ...)`, `call_tool("model_router/latency", ...)`, (3) 打印每个策略的推荐模型
- [ ] 9.1.3 保留 `--mock` flag: 使用 MockLLMProvider + `set_available_models(make_test_candidates())`
- [ ] 9.1.4 新增 `--list` flag: 调用 `call_tool("model_router/registry", {})` 打印所有可用模型

### 9.2 升级 `examples/phase1_model_router_plugin/CMakeLists.txt`

- [ ] 9.2.1 添加 `target_link_libraries(phase1_model_router_plugin hydraforge_model_router_cost hydraforge_model_router_quality hydraforge_model_router_latency agenticdsl_plugin_loader)`
- [ ] 9.2.2 添加 `set_target_properties(phase1_model_router_plugin PROPERTIES BUILD_RPATH "$ORIGIN/../../build/lib")` (确保运行时能找到 .so)

### 9.3 验证

- [ ] 9.3.1 `make -j$(nproc) && ./build/examples/phase1_model_router_plugin/phase1_model_router_plugin --mock` 输出 3 策略结果
- [ ] 9.3.2 `./build/examples/phase1_model_router_plugin/phase1_model_router_plugin --list` 输出模型列表

---

## 10. 测试 (Sprint 17 Day 8-10 — 0.25 人日)

### 10.1 创建 `tests/test_model_router_plugins.cpp`

- [ ] 10.1.1 创建测试文件, 包含 `<catch2/catch_test_macros.hpp>` + `"agenticdsl/pdk/model_router.h"`
- [ ] 10.1.2 编写 `make_test_candidates()` helper: 返回 `vector<ModelCapability>` 含 gpt-4 / gpt-3.5-turbo / claude-3 三个测试模型
- [ ] 10.1.3 编写 `make_routing_context(tags)` helper: 构造 RoutingContext, required_tags = 参数

### 10.2 Cost 策略测试 (4 TEST_CASE)

- [ ] 10.2.1 `[model_router][cost]` cheapest-viable: gpt-4(0.03) vs gpt-3.5(0.002), tag=["general"] → gpt-3.5-turbo
- [ ] 10.2.2 `[model_router][cost]` budget-exceeded: budget=0.001, 最便宜 0.002 → throw NoViableModel
- [ ] 10.2.3 `[model_router][cost]` tag-mismatch: required_tags=["vision"], 所有模型无 vision → throw NoViableModel
- [ ] 10.2.4 `[model_router][cost]` single-model: 仅 1 模型 → 返回该模型

### 10.3 Quality 策略测试 (4 TEST_CASE)

- [ ] 10.3.1 `[model_router][quality]` full-tag-match: gpt-4 (reasoning+code) vs gpt-3.5 (general) → gpt-4
- [ ] 10.3.2 `[model_router][quality]` partial-match: claude-3 (reasoning+code=2) vs gpt-4 (code=1) → claude-3
- [ ] 10.3.3 `[model_router][quality]` no-tag-match-fallback: required_tags=["vision"] → fallback candidates[0]
- [ ] 10.3.4 `[model_router][quality]` empty-tag: required_tags=[] → 按 n_ctx+max_tokens 总分排序, claude-3(16384+4096) 最高

### 10.4 Latency 策略测试 (4 TEST_CASE)

- [ ] 10.4.1 `[model_router][latency]` lowest-latency: gpt-4(500ms) vs gpt-3.5(200ms) → gpt-3.5-turbo
- [ ] 10.4.2 `[model_router][latency]` latency-budget: max_latency=300ms → 跳过 gpt-4(500), claude-3(350), 返回 gpt-3.5(200)
- [ ] 10.4.3 `[model_router][latency]` all-exceed: max_latency=100ms → throw NoViableModel
- [ ] 10.4.4 `[model_router][latency]` tag-over-latency: required_tags=["vision"] → gpt-4 即使 latency 更高

### 10.5 Registry 工具测试 (3 TEST_CASE)

- [ ] 10.5.1 `[model_router][registry]` list-all: `call_tool("model_router/registry", {})` → 返回非空数组
- [ ] 10.5.2 `[model_router][registry]` filter-by-tag: `call_tool("model_router/registry", {{"tag", "fast"}})` → 仅返回含 "fast" tag 的模型
- [ ] 10.5.3 `[model_router][registry]` no-match: tag="quantum" → 返回空数组 []

### 10.6 测试构建验证

- [ ] 10.6.1 `cmake --preset tests && make test_model_router_plugins -j$(nproc)` 编译通过
- [ ] 10.6.2 `ctest -R test_model_router_plugins --output-on-failure` 全部新测试 PASS

---

## 11. 验证 (Sprint 17 Day 11 — 0.25 人日)

### 11.1 全线测试

- [ ] 11.1.1 `ctest --output-on-failure` ≥ 72+ PASS (57 baseline + 15 new model_router tests)
- [ ] 11.1.2 `cmake --preset asan && ctest --output-on-failure` 0 memory error
- [ ] 11.1.3 `cmake --preset tsan && ctest --output-on-failure` 0 data race

### 11.2 工具链验证

- [ ] 11.2.1 `python3 tools/adr_lint.py docs/adr/plugin/` exit 0
- [ ] 11.2.2 `python3 tools/docs_drift_audit.py` 0 critical drift (C7 新增文件不影响既有 drift)
- [ ] 11.2.3 `openspec validate 2026-06-26-adr-0034-model-router-plugin` exit 0
- [ ] 11.2.4 `ls pdk/model_router/*/CMakeLists.txt` 返回 3 个 CMakeLists.txt (cost/quality/latency)
- [ ] 11.2.5 `ls build/lib/libhydraforge_model_router_*.so` 返回 3 个 .so
- [ ] 11.2.6 `grep -r "TBD:" openspec/changes/2026-06-26-adr-0034-model-router-plugin/` 返回空 (零 TBD)
- [ ] 11.2.7 `git status` clean

---

## 12. 同步与归档 (Sprint 17 Day 11-12)

### 12.1 文档同步

- [ ] 12.1.1 更新 `docs/adr/plugin/adr-0034-model-router.md`: 🔍 Proposed 状态不变 (C7 fill-in 不升 Approved, 升在 ship 时), 追加"2026-07-02: C7 Oracle Q1-Q4 决策落地, proposal/design/spec/tasks 填充完成"到 §背景 后
- [ ] 12.1.2 更新 `docs/README.md` § adr/plugin/ 状态表: 在 adr-0034 行备注 "C7 Oracle 决策落地, proposal/design/spec/tasks ready (2026-07-02)"
- [ ] 12.1.3 更新 `AGENTS.md` § Recent Changes: 追加 C7 fill-in 行

### 12.2 Master plan 同步

- [ ] 12.2.1 更新 `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C7 行: 状态 ⚪ 占位 → 🟡 active (oracle filled), 追加 Oracle 决策摘要
- [ ] 12.2.2 更新 `docs/roadmap-status.md` §一 Phase 4 行 (若存在)

### 12.3 Dual-Repo 同步

- [ ] 12.3.1 执行 `bash scripts/sync-pdk.sh` (非 dry-run), 确认 `pdk/model_router/` 子目录推送到 `github.com/chisuhua/hydraforge-pdk`

### 12.4 提交

- [ ] 12.4.1 `git add openspec/changes/2026-06-26-adr-0034-model-router-plugin/ docs/adr/plugin/adr-0034-model-router.md docs/README.md AGENTS.md docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md`
- [ ] 12.4.2 `git commit -m "feat(c7-fill): adr-0034 model-router-plugin — Oracle Q1-Q4 决策后 proposal/design/spec/tasks 全部填充完成"`

### 12.5 注意 (Sprint 17 ship 时执行, 不在 C7 fill-in)

- [ ] 12.5.1 (Ship gate) `openspec archive 2026-06-26-adr-0034-model-router-plugin --yes`
- [ ] 12.5.2 (Ship gate) ADR-0034 状态 🔍 Proposed → ✅ Approved
- [ ] 12.5.3 (Ship gate) master plan C7 行: 🟡 active → ✅ shipped

---

## 13. 验证检查清单 (C7 ship gate)

- [ ] 1. ADR-0034 proposal/design/spec/tasks 完整 (Oracle Q1-Q4 决策落地)
- [ ] 2. IModelRouter 接口完整 (`include/agenticdsl/pdk/model_router.h` 含 4 类型)
- [ ] 3. 3 个 Plugin .so 构建成功 (cost / quality / latency)
- [ ] 4. `pdk_register_tools` 入口正确 (3 Plugin 各注册 1 tool)
- [ ] 5. ModelRegistry `model_router/registry` 工具可用
- [ ] 6. MockLLMProvider 扩展 `set_available_models()` 测试 hook
- [ ] 7. `examples/phase1_model_router_plugin --mock` 演示 3 策略
- [ ] 8. ctest ≥ 72+ PASS (含 15 个新增 model_router 测试)
- [ ] 9. ASan 100% clean
- [ ] 10. TSan 100% clean
- [ ] 11. `grep -r "TBD:" openspec/changes/2026-06-26-adr-0034-model-router-plugin/` 返回空
- [ ] 12. `openspec validate 2026-06-26-adr-0034-model-router-plugin` exit 0
- [ ] 13. `scripts/sync-pdk.sh` 零改动, 同步成功
- [ ] 14. ADR-0034 status ✅ Approved
- [ ] 15. master plan C7 行 ✅ shipped