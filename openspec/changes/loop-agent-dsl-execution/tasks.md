# Tasks: Loop Agent 真实 DSL 执行

> **Metis/Oracle 审查状态**: 2026-07-19 — 设计修复完成（B1 存储矛盾、B2 静态全局、B3 PlanExecute/ForkJoin 覆盖已解决）
>
> **实施状态**: 2026-07-19 — Q1-Q7 用户已确认，代码已实施，ctest 82/82 PASS，git commit `ab8a888`
>
> 估时 ~5-7 天（实际 ~4 天）
> 依赖: 无外部依赖

## §0. 设计决策预澄清（实施前，~0.5 天）

> Metis 审查发现 7 个歧义点，已全部确认。

- [x] 0.1 Q1: `from_markdown` 第二参数传 parent 的已装饰 provider（`*parent.get_llm_provider()`）✅
- [x] 0.2 Q2: `thread_local` 覆盖行为 = last-write-wins + warning log ✅
- [x] 0.3 Q3: 旧 `from_markdown(content)` 单参数签名保留 + 向后兼容 ✅
- [x] 0.4 Q4: child DSL 错误通过 return json 的 error 字段传播 ✅
- [x] 0.5 Q5: `loop/set_parent_provider` → StateModify / force_approval_always / {Workflow} ✅
- [x] 0.6 Q6: child budget 归 parent = 预期行为 ✅
- [x] 0.7 Q7: 无既有 mock 测试，从零编写测试 ✅

## 1. DSLEngine 双字段存储 + from_markdown 新重载

- [x] 1.1 `src/core/engine.h`: `ILLMProvider* borrowed_provider_ = nullptr` 字段 ✅
- [x] 1.2 `src/core/engine.h`: `get_llm_provider()` 双字段路由 ✅
- [x] 1.3 `src/core/engine.h`: `set_borrowed_provider(ILLMProvider&)` setter ✅
- [x] 1.4 `src/core/engine.h`: `from_markdown(const std::string&, ILLMProvider&)` 重载声明 ✅
- [x] 1.5 `src/core/engine.cpp`: 新重载实现 ✅
- [x] 1.6 grep 验证: 新重载无 `set_llm_provider()` 调用 ✅
- [x] 1.7 不变式验证: `owned_provider_` 和 `borrowed_provider_` 互斥 ✅
- [x] 1.8 向后兼容: `from_markdown(content)` 单参数签名不变 ✅

## 2. Loop Agent Plugin 真实 DSL 执行

- [x] 2.1 `pdk_entry.cpp`: `static thread_local ILLMProvider* tls_parent_provider` ✅
- [x] 2.2 `pdk_entry.cpp`: `loop/set_parent_provider` 工具 (StateModify / force_approval_always) ✅
- [x] 2.3 `pdk_entry.cpp`: `loop/run` lambda → `DSLEngine::from_markdown(content, *tls_parent_provider)` ✅
- [x] 2.4 `pdk_entry.cpp`: mock fallback 保留 (tls_parent_provider == nullptr 时) ✅
- [x] 2.5 `pdk_entry.cpp`: `loop_type` 校验 (react/plan_execute/fork_join) ✅
- [-x] 2.6 `pdk/loop_agent/CMakeLists.txt`: 不移除 agenticdsl_core 链接 ⚠️ (reverted: .so 不可链接静态 lib，改用宿主 --export-dynamic)
- [x] 2.7 `examples/pdk_chat_demo/main.cpp`: bootstrap set_parent_provider 调用 ✅

## 3. 测试 — Provider 传播与装饰器链

- [x] 3.1 新 overload 使用传入 provider ✅
- [x] 3.2 单参数 from_markdown 仍然 Mock ✅
- [x] 3.3 双字段不变式 ✅
- [~] 3.4 装饰器链继承 — CostTracking 指针身份验证 ✅ (单次计费验证 deferred: 需 budget infrastructure)
- [x] 3.5 新 overload 无 set_llm_provider 调用 (test 中指针身份验证) ✅
- [~] 3.6 `thread_local` 隔离 — deferred (需 TSan + 多线程 fixture)
- [x] 3.7 `set_borrowed_provider()` setter ✅

## 4. 测试 — Loop Agent 真实执行

- [x] 4.1 React DSL 解析验证 ✅ (test_llm_provider_propagation 内联 DSL)
- [x] 4.2 PlanExecute DSL 解析验证 ✅
- [x] 4.3 ForkJoin DSL 解析验证 ✅
- [x] 4.4 非法 loop_type 返回错误 ✅ (test_loop_agent_plugin)
- [~] 4.5 file-not-found 错误路径 — deferred (需 mock filesystem)
- [x] 4.6 未设 provider 时 mock fallback ✅ (test_loop_agent_plugin)
- [x] 4.7 安全元数据 ✅ (test_loop_agent_plugin 验证 tool 注册和调用)
- [x] 4.8 overwrite warning ✅ (logic implemented, tested via call_tool)
- [x] 4.9 测试隔离 ✅ (每个 TEST_CASE 独立 engine)
- [~] 4.10 并发测试 — deferred (需 TSan + 多线程 fixture)

## 5. 构建与验证

- [x] 5.1 cmake --preset tests 零编译错误 ✅
- [x] 5.2 cmake --preset tests 全量编译 ✅
- [x] 5.3 ctest -j$(nproc) — 82/82 PASS ✅
- [~] 5.4 ASan — engine tests 通过 (test_llm_provider_propagation + test_engine*), 未跑全量 ASan (build timeout)
- [~] 5.5 TSan — deferred (build timeout >10min, CI 中运行)
- [x] 5.6 `openspec validate --strict` ✅
- [x] 5.7 `git commit` → `ab8a888` ✅

## 6. Deferred（Phase 6+ ADR-0052 候选）

- [ ] 6.1 [记录] ADR-0052 PDKContext 抽象化：扩展 `pdk_register_tools(IToolRegistry&, PluginInitContext&)` 签名
  - 触发条件: 第三个 PDK plugin 需要 parent engine 上下文
  - 涉及: PluginLoader ABI v2、ADR-0022 修订
- [ ] 6.2 [记录] `shared_ptr<ILLMProvider>` 升级评估
  - 触发条件: Sprint 25+ 引入 async DSL execution
  - 影响: `borrowed_provider_` 字段移除 + getter 简化