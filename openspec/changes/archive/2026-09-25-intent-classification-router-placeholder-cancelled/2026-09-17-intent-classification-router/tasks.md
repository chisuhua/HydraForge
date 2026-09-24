# Tasks: Intent Classification + Loop Type Routing
# STATUS: 待起草 (Oracle ses_f4fd88215ffeUWSe2StAWtFPSQ 调研结论指导)
#
# Phase A: DSL 子图 (~30 行 YAML)
#   - [ ] 创建 lib/loop/intent_classify.agent.md (subgraph, ~30 lines)
#         - start → switch (rule-based 入口分流: trivial/math/coding/research)
#         - think (llm_call 调 classify_intent tool)
#         - dispatch (tool_call: loop/run_subgraph 桥接)
#   - [ ] 4 intent types + complexity 字段 (4-level: trivial/simple/complex/multi-step)
#
# Phase B: C++ 工具 + ChatSession wiring (~50 行 C++)
#   - [ ] pdk/loop_agent 注册 loop/classify_intent (~30 行)
#   - [ ] ChatSession "auto" routing (~20 行, 两次平级 loop/run 调用)
#   - [ ] 测试覆盖: 6 cases (3 happy + 3 error)
#
# Phase C: 真实 LLM E2E (chat-real-llm-coverage 模式)
#   - [ ] [realllm] tag cases: 3 happy paths (trivial → simple loop, complex → react, multi-step → plan_execute)
#   - [ ] [realllm] tag cases: 3 error paths (parse failure / unknown intent / timeout)
#   - [ ] CI 默认 skip, manual `HYDRAFORGE_SKIP_REAL_LLM=0` 启用
#
# Phase D: ship-with-fixes
#   - [ ] Metis dual-agent review (mandatory per master plan §九)
#   - [ ] Oracle review (mandatory for cross-file)
#   - [ ] ctest 245/245 零回归
#   - [ ] openspec archive