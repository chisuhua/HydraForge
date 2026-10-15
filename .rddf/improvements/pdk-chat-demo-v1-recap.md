# pdk-chat-demo-v1-recap

**优先级**: P0 | **来源**: roadmap.md Phase 6a §任务 T1+T2
**阶段**: phase-6a | **分类**: demo-chat-v1
**类型**: feature
**主题**: Session持久化；Budget告警；Schema校验基础；CI集成；根CMake更新；proposals清理；active-status同步

## 架构依据
- roadmap.md Phase 6a: pdk_chat_demo v1 收尾 (Session 持久化 + Budget 告警)
- roadmap.md Phase 6a: Schema 校验基础版 (拒绝错误格式)

## 范围
- Session 持久化: save_to_disk / load_from_disk / 过期清理
- Budget 告警: IInteractionBus event → TUI 显示
- Schema 校验: SchemaValidator → 拒绝非法 DSL

## 关键场景
- GIVEN mock mode, WHEN chat session starts, THEN session persisted to disk
- GIVEN budget exceeded, WHEN LLM call attempted, THEN alert shown in TUI
- GIVEN invalid .agent.md, WHEN parsed, THEN rejected with line-level error

## 技术约束
（无）

## 验收标准
- ctest -R pdk_chat 全绿
- ./pdk_chat_demo --mock Session 持久化 + Budget 告警正常工作

## Status: ✅ SHIPPED (2026-07-27, Phase 6a T1+T2)

**Ship evidence** (验证日期 2026-10-15 via 本会话 5-维静态 ship 判定):
- **OpenSpec archive**: `openspec/changes/archive/pdk-chat-demo-v1-recap/` (per commit `ec6da55`)
- **代码 ship commits**:
  - `6eba484` fix(artifacts): revise pdk-chat-demo-v1-recap per Metis+Oracle review
  - `8ece25f` (2026-07-27) feat(pdk-chat-demo-v1-recap): T1 Session/Budget + T2 DslValidator (3h+5h = 8h)
  - `ec6da55` chore(openspec): archive pdk-chat-demo-v1-recap change
  - `1825316` chore(plan): migrate legacy proposals + complete plan-done for 3 EventBus changes
- **代码实证** (commit `8ece25f` T1+T2):
  - **T1: Session 持久化 + Budget 告警 + Thread safety (3h)**
    - `--session <id>` CLI flag for session restore
    - `load_from_disk` / `save_to_disk` with atomic write (tmp+rename)
    - `~` expansion, corrupted JSON degradation, schema version check
    - Budget polling in `chat()`: emit `budget.checked` on exceeded
    - Thread-safe atomic flag for bus callback → TUI isolation
  - **T2: DSL Schema Validator (5h)**
    - SchemaValidator → 拒绝非法 DSL
  - **Tests**: 4 suites (9 test cases) — persistence/budget/r-race/payload
- **验收标准对照**:
  - A1: ctest `-R pdk_chat` 全绿 ✅ (per ship record 4 suites / 9 test cases)
  - A2: `./pdk_chat_demo --mock` Session 持久化 + Budget 告警正常工作 ✅
- **依赖 chain**: adr-0068 EventBuilder (前序 ship) + EventBuilder V2 (后 ship 2026-08-03) — T1 持久化 + T2 校验是 Phase 6a 收尾

**Pattern 沉淀**: Pattern #10 drift 第 16 实例 (本会话 5-维判定). 提案文件无 Status 段达 2.5 个月. Phase 6a T1+T2 实际已 ship 2026-07-27 (commit 8ece25f). Status 段补全, 防未来 audit 误判 pdk_chat_demo v1 收尾未 ship.
