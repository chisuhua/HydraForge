# Tasks: Fix GenerateSubgraphNode static-next latent gap
# STATUS: 待起草
#
# Phase A: 根因诊断 (1-2d)
#   - [ ] 验证 parse_node_wait_for_deps 是否接受 "/dynamic/x" 静态 next 引用
#   - [ ] 若不接受: 决定方案 (a) build_dag 加 /dynamic/ 豁免 OR (b) 修正 dsl.md 文档
#   - [ ] dsl.md §423/§438/§1114 描述与实现矛盾具体定位
#
# Phase B: 实施 + 真实 LLM E2E (3-5d)
#   - [ ] 实施选定方案 (build_dag 修复 OR dsl.md 文档重写)
#   - [ ] 3 happy path tests (generate → register → execute 完整链路)
#   - [ ] 3 error path tests (parse failure / next not found / runtime error)
#   - [ ] [realllm] tag: 真实 DeepSeek LLM 端到端
#
# Phase C: ship-with-fixes
#   - [ ] Oracle review (mandatory for cross-file)
#   - [ ] ctest 245/245 零回归
#   - [ ] openspec archive