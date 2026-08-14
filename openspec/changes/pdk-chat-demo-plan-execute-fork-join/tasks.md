# Tasks: PDK Chat Demo — PlanExecuteLoop + ForkJoinLoop DSL 示例与集成

## 1. 创建 DSL 示例文件

- [ ] 1.1 创建 `examples/pdk_chat_demo/dsl/` 目录
- [ ] 1.2 创建 `examples/pdk_chat_demo/dsl/plan_execute_example.agent.md`
  - 展示 PlanExecuteLoop 3 阶段 DSL 格式
  - 包含 `/main` 子图 (Start → ToolCall → End)
- [ ] 1.3 创建 `examples/pdk_chat_demo/dsl/fork_join_example.agent.md`
  - 展示 ForkJoinLoop 4 阶段 DSL 格式
  - 包含 `/main` 子图 (Start → 并发 branches → End)

## 2. 创建集成测试

- [ ] 2.1 创建 `examples/pdk_chat_demo/tests/test_plan_execute_loop_integration.cpp`
  - TEST_CASE: plan success + verify success → Done
  - 使用 MockLLMProvider 预设响应队列
  - 验证 LoopResult.success == true
  - 验证 retries_used == 0
- [ ] 2.2 创建 `examples/pdk_chat_demo/tests/test_fork_join_loop_integration.cpp`
  - TEST_CASE: 3 branches 全成功 → Done
  - 验证 LoopResult.success == true
  - 验证 final_context.working["data"] 包含 3 个 branch 输出
- [ ] 2.3 更新 `examples/pdk_chat_demo/tests/CMakeLists.txt` 注册新测试

## 3. 更新 README

- [ ] 3.1 在 `examples/pdk_chat_demo/README.md` 添加 DSL 示例章节
  - 说明 dsl/ 目录用途
  - 说明 PlanExecuteLoop 和 ForkJoinLoop 示例

## 4. 验证 gates

- [ ] 4.1 cmake build: `cmake --build build -j$(nproc)` 0 errors
- [ ] 4.2 ctest: `ctest -R pdk_chat --output-on-failure` 全 PASS
- [ ] 4.3 adr_lint: `python3 tools/adr_lint.py` exit 0
- [ ] 4.4 docs_drift_audit: `python3 tools/docs_drift_audit.py` 0 CRITICAL
- [ ] 4.5 openspec validate: `openspec validate pdk-chat-demo-plan-execute-fork-join --strict` exit 0

## 5. Git commit + archive

- [ ] 5.1 创建 lightweight branch `feat/pdk-chat-demo-plan-execute-fork-join`
- [ ] 5.2 git add + commit
- [ ] 5.3 openspec archive `pdk-chat-demo-plan-execute-fork-join --yes`
- [ ] 5.4 merge to main via fast-forward or merge commit
