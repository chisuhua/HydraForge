# Tasks: ADR-0034 — IModelRouter (PDK 示例 Plugin)

> **STATUS: PLACEHOLDER** ⚠️ — 详细 tasks 待 Sprint 16 启动前填充
> **预估工时**: 1-2 周 (Sprint 17 主体)
> **关联 master plan**: `docs/superpowers/plans/2026-06-26-sprint-11-to-18-roadmap.md` §四 C7
> **前置依赖**: 无硬依赖 (PDK + PluginLoader 已 ship)

---

## 1. 决策前置 (Sprint 16 收官后)

- [ ] 1.1 业务确认: 3 种路由策略的实际使用场景
- [ ] 1.2 决策: plugin 内置多策略 vs 单策略多 plugin
- [ ] 1.3 评估: 双仓库同步机制 (sync-pdk.sh 已就绪)

## 2. 详细制定 (Sprint 17 Day 0)

- [ ] 2.1 写本 change proposal.md (What Changes 详细化)
- [ ] 2.2 写 design.md (5 个 Decision)
- [ ] 2.3 写本 tasks.md (10-15 sections, 30-50 tasks)
- [ ] 2.4 写 specs/model-router-plugin/spec.md (5-8 ADDED Requirements)
- [ ] 2.5 移除所有 "PLACEHOLDER" 标记, 更新 STATUS 行

## 3. IModelRouter 接口定义 (Sprint 17 Day 1-2)

- [ ] 3.1 TBD: IModelRouter 接口
- [ ] 3.2 TBD: RoutingContext 结构
- [ ] 3.3 TBD: ModelCapability 结构

## 4. Runtime 数据抽象 (Sprint 17 Day 3-4)

- [ ] 4.1 TBD: ILLMProvider::available_models() 默认实现
- [ ] 4.2 TBD: LLMConfig 扩展
- [ ] 4.3 TBD: 各 Provider 实现 available_models()

## 5. DefaultModelRouter plugin 实施 (Sprint 17 Day 5-7)

- [ ] 5.1 TBD: ModelRouterPlugin 入口
- [ ] 5.2 TBD: DefaultModelRouterPolicy (成本路由)
- [ ] 5.3 TBD: QualityModelRouterPolicy (质量路由)
- [ ] 5.4 TBD: LatencyModelRouterPolicy (延迟路由)
- [ ] 5.5 TBD: ModelRegistry 工具 (DECLARE_TOOL 暴露 query_model)

## 6. 集成验证 (Sprint 17 Day 8-9)

- [ ] 6.1 TBD: examples/phase1_model_router_plugin
- [ ] 6.2 TBD: PluginLoader 集成
- [ ] 6.3 TBD: 双仓库同步

## 7. 测试 (Sprint 17 Day 10-12)

- [ ] 7.1 TBD: IModelRouter 单元测试
- [ ] 7.2 TBD: 3 策略单元测试
- [ ] 7.3 TBD: PluginLoader 加载验证
- [ ] 7.4 TBD: 集成测试

## 8. 验证 (Sprint 17 Day 13)

- [ ] 8.1 `ctest --output-on-failure` ≥ 47/47 + 新增 N 个 PASS
- [ ] 8.2 `cmake --preset tsan && ctest` 0 race
- [ ] 8.3 `cmake --preset asan && ctest` 0 leak
- [ ] 8.4 `python3 tools/adr_lint.py` exit 0
- [ ] 8.5 `python3 tools/docs_drift_audit.py` 0 critical drift
- [ ] 8.6 `openspec validate 2026-06-26-adr-0034-model-router-plugin` exit 0
- [ ] 8.7 `git status` clean

## 9. 同步与归档 (Sprint 17 Day 14)

- [ ] 9.1 更新 `docs/adr/plugin/adr-0034-model-router.md`: 🔍 Proposed → ✅ Approved (plugin-candidate)
- [ ] 9.2 更新 `docs/README.md` § adr/plugin/ 状态表
- [ ] 9.3 更新 `docs/roadmap-status.md` §一 Phase 4 状态
- [ ] 9.4 更新 `AGENTS.md` § Recent Changes
- [ ] 9.5 同步 PDK 头文件 + 双仓库: `./scripts/sync-pdk.sh`
- [ ] 9.6 `openspec archive 2026-06-26-adr-0034-model-router-plugin --yes`
- [ ] 9.7 同步 master plan C7 行: 状态更新

---

## 验证检查清单 (C7 ship gate)

- [ ] 1. ADR-0034 完整 design
- [ ] 2. IModelRouter 接口完整
- [ ] 3. Runtime 数据抽象可用
- [ ] 4. 3 路由策略可用
- [ ] 5. PluginLoader 加载工作
- [ ] 6. 双仓库同步成功
- [ ] 7. ctest 全绿 (含新增 tests)
- [ ] 8. ASan/TSan 100% clean
- [ ] 9. `openspec validate` exit 0
- [ ] 10. ADR-0034 status ✅ Approved
- [ ] 11. master plan C7 状态更新
