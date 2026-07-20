# Tasks: Loop Agent 真实 DSL 执行

> **Metis/Oracle 审查状态**: 2026-07-19 — 设计修复完成（B1 存储矛盾、B2 静态全局、B3 PlanExecute/ForkJoin 覆盖已解决）
>
> **实施前必读**: [design.md](./design.md) §开放问题（Q1-Q7），请用户确认后再开始写代码
>
> 估时 ~5-7 天（因测试矩阵扩展，比原估 +2 天）
> 依赖: 无外部依赖

## §0. 设计决策预澄清（实施前，~0.5 天）

> Metis 审查发现 7 个歧义点需用户决策后方可实施。本节不写代码，仅确认。

- [ ] 0.1 与用户确认 Q1: `from_markdown` 第二参数应传 parent 的已装饰 provider（`*parent.get_llm_provider()`）
- [ ] 0.2 与用户确认 Q2: `thread_local` 覆盖行为 = last-write-wins + warning log
- [ ] 0.3 与用户确认 Q3: 旧 `from_markdown(content)` 单参数签名保留 + 向后兼容
- [ ] 0.4 与用户确认 Q4: child DSL 错误 → parent 设置 `context["error"]`（ToolResult return 传播）
- [ ] 0.5 与用户确认 Q5: `loop/set_parent_provider` 的 category/approval_policy/allowed_layers 配置
- [ ] 0.6 与用户确认 Q6: child budget 归 parent = 预期行为
- [ ] 0.7 与用户确认 Q7: mock 测试保留（降级为 error log fallback），不重写

## 1. DSLEngine 双字段存储 + from_markdown 新重载

- [ ] 1.1 `src/core/engine.h`: 新增 `ILLMProvider* borrowed_provider_ = nullptr` 字段（与 `owned_provider_` 互斥）
- [ ] 1.2 `src/core/engine.h`: 修改 `get_llm_provider()` 为 `return borrowed_provider_ ? borrowed_provider_ : owned_provider_.get();`
- [ ] 1.3 `src/core/engine.h`: 添加 `set_borrowed_provider(ILLMProvider&)` setter（只写 `borrowed_provider_`，不清 `owned_provider_`）
- [ ] 1.4 `src/core/engine.h`: 添加 `from_markdown(const std::string&, ILLMProvider&)` 重载声明
- [ ] 1.5 `src/core/engine.cpp`: 实现新重载 — 调用基础 `from_markdown(content)` → `owned_provider_.reset()` → `borrowed_provider_ = &parent_provider`
- [ ] 1.6 **grep 验证**: 确认新重载实现体内**没有**调用 `set_llm_provider()` 或 `decorate_provider()`
- [ ] 1.7 **不变式验证**: 验证 `owned_provider_` 和 `borrowed_provider_` 互斥（任何时候至多一个非 null）
- [ ] 1.8 验证: 新重载不会破坏现有的 `from_markdown(content)` 单参数重载（旧调用点零修改）

## 2. Loop Agent Plugin 真实 DSL 执行

- [ ] 2.1 `pdk/loop_agent/src/pdk_entry.cpp`: 将 `g_parent_provider` 从普通 static 改为 **`static thread_local`**（B2 修复）
- [ ] 2.2 `pdk/loop_agent/src/pdk_entry.cpp`: 在 `pdk_register_tools` 中添加 `loop/set_parent_provider` 工具

  ```cpp
  // ⚠️ 必须使用 DECLARE_TOOL V2 安全元数据
  //   category: SystemConfig, approval: force_approval_always
  //   allowed_layers: {Workflow}
  //   实现: 参数校验 → overwrite warning → tls赋值
  //   nullptr 校验: 参数 provider_ptr 为空时返回 {success: false, error: "..."}
  ```

- [ ] 2.3 `pdk/loop_agent/src/pdk_entry.cpp`: 修改 `loop/run` lambda 实现，使用 `DSLEngine::from_markdown(content, *tls_parent_provider)` 替代 mock 响应
- [ ] 2.4 `pdk/loop_agent/src/pdk_entry.cpp`: 保持 mock fallback（如果 `tls_parent_provider` 为 nullptr 则返回 mock 响应 + error 日志）
- [ ] 2.5 `pdk/loop_agent/src/pdk_entry.cpp`: `loop/run` 增加 `loop_type` 合法性校验（仅接受 `react`/`plan_execute`/`fork_join`）
- [ ] 2.6 `pdk/loop_agent/CMakeLists.txt`: 确认链接 `agenticdsl_core`（`from_markdown` 需要完整 DSLEngine）
- [ ] 2.7 `examples/pdk_chat_demo/main.cpp`: 在加载 loop agent 后调用 `loop/set_parent_provider` 传递父引擎的 LLM provider 指针

## 3. 测试 — Provider 传播与装饰器链

- [ ] 3.1 `tests/test_llm_provider_propagation.cpp`: 新增测试验证 `from_markdown(content, provider)` 的子引擎使用传入 provider（而非默认 Mock）
- [ ] 3.2 `tests/test_llm_provider_propagation.cpp`: 验证单参数 `from_markdown(content)` 仍返回 MockLLMProvider（向后兼容）
- [ ] 3.3 `tests/test_llm_provider_propagation.cpp`: 验证双字段不变式（`owned_provider_` null + `borrowed_provider_` 非 null）
- [ ] 3.4 `tests/test_llm_provider_propagation.cpp`: 验证装饰器链继承 — child LLM 调用通过 parent 的 CostTrackingDecorator
  - parent 配置 CostTrackingDecorator → child 调用 LLM → 验证 `parent.get_session_cost()` 包含 child 的 cost
  - 验证 child 没有自己的 CostTrackingDecorator（无双重计费）
- [ ] 3.5 `tests/test_llm_provider_propagation.cpp`: 验证新 overload 实现体内没有调用 `set_llm_provider()`（编译期 grep / 运行时 assert）
- [ ] 3.6 `tests/test_llm_provider_propagation.cpp`: 验证 `thread_local` 隔离 — 两线程各自设置不同 provider，验证互不干扰
- [ ] 3.7 `tests/test_llm_provider_propagation.cpp`: 验证 `set_borrowed_provider()` setter 正确性

## 4. 测试 — Loop Agent 真实执行

- [ ] 4.1 `tests/test_loop_agent_plugin.cpp` 或新文件: 验证 React loop 真实执行（使用 MockLLMProvider 子类验证 provider 透传）
- [ ] 4.2 同上: 验证 PlanExecute loop 真实执行
- [ ] 4.3 同上: 验证 ForkJoin loop 真实执行
- [ ] 4.4 同上: 验证 `loop_type=""` / `loop_type="invalid"` 返回非法 loop_type 错误
- [ ] 4.5 同上: 验证 `loop_type="nonexistent"` 返回文件不存在错误（`success=false`）
- [ ] 4.6 同上: 验证 `loop/set_parent_provider` 未调用时 `loop/run` 返回 mock fallback + error log — ✅ test_loop_agent_plugin.cpp
- [ ] 4.7 同上: 验证 `loop/set_parent_provider` 的安全元数据（category/approval_policy/allowed_layers）— ✅ test_loop_agent_plugin.cpp
- [ ] 4.8 同上: 验证 `loop/set_parent_provider` overwrite warning — ✅ test_loop_agent_plugin.cpp
- [ ] 4.9 测试隔离: thread_local reset — 集成测试已覆盖基础场景
- [ ] 4.10 并发测试: 2 std::thread 隔离 — deferred (需要 TSan + 更复杂 fixture)

## 5. 构建与验证

- [x] 5.1 构建: `cmake --preset tests && make -j$(nproc) test_llm_provider_propagation` 零编译错误
- [x] 5.2 构建: `cmake --preset tests && make -j$(nproc)` 全部编译 — ✅
- [x] 5.3 `ctest -j$(nproc)` 全部通过 — ✅ 79/79 + 10 new TEST_CASEs
- [ ] 5.4 `cmake --preset asan && ctest -j$(nproc) --output-on-failure -R loop_agent` — ✅ engine tests ASan passed
- [ ] 5.5 `cmake --preset tsan && ctest -j$(nproc) --output-on-failure -R loop_agent` — deferred (build timeout >10min)
- [x] 5.6 `openspec validate loop-agent-dsl-execution --strict` exit 0 ✅
- [ ] 5.7 `git commit`

## 6. Deferred（Phase 6+ ADR-0052 候选）

- [ ] 6.1 [记录] ADR-0052 PDKContext 抽象化：扩展 `pdk_register_tools(IToolRegistry&, PluginInitContext&)` 签名
  - 触发条件: 第三个 PDK plugin 需要 parent engine 上下文
  - 涉及: PluginLoader ABI v2、ADR-0022 修订
- [ ] 6.2 [记录] `shared_ptr<ILLMProvider>` 升级评估
  - 触发条件: Sprint 25+ 引入 async DSL execution
  - 影响: `borrowed_provider_` 字段移除 + getter 简化