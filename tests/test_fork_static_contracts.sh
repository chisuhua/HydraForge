#!/usr/bin/env bash
# tests/test_fork_static_contracts.sh
# Sprint 7 Day 3 quick win (Oracle Top 2 - 推迟自 Day 2 spot-check):
# 静态 grep 锁定 fork 状态契约, 防止 Day 1 fork dedup 回归。
#
# 背景: Oracle session ses_111741590ffeF6HMxhgYq7YKrr Day 2 抽查发现
# - Day 1 fix (commit 84c4c0a) 修复了 execute() L161-167 与 dispatch_next_node() L636-642
#   重复 fork 块, 但无自动化测试锁定契约
# - I-2 fork regression test 推迟到 Day 5+ scheduler-pipeline-tightened
#   (NodeExecutor::execute_fork 当前未实现, 无法通过 DSL 触发)
# - 本脚本作为过渡: 静态契约锁定到 ctest, Day 5+ 替换为真实 unit test
#
# 关联 spec: openspec/changes/sprint-7-tech-debt-followup/specs/sprint-7-tech-debt-followup/spec.md
# 关联 Oracle: ses_111741590ffeF6HMxhgYq7YKrr (Day 2 spot-check)
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
SCHEDULER_CPP="${REPO_ROOT}/src/modules/scheduler/topo_scheduler.cpp"

if [[ ! -f "${SCHEDULER_CPP}" ]]; then
  echo "FAIL: ${SCHEDULER_CPP} 不存在" >&2
  exit 1
fi

PASS=0
FAIL=0

# ─────────────────────────────────────────────────────────────
# 契约 1: fork 处理仅 1 个调用点 (位于 execute() 内, 排除函数定义与文档注释)
# Day 1 fix (commit 84c4c0a) 后 dispatch_next_node 不再含 fork 处理块
# 排除项: 函数定义 `void TopoScheduler::execute_fork_branches()` 等
# ─────────────────────────────────────────────────────────────
# grep 仅匹配 *调用* (不在 :: 分辨符后, 不在注释中)
# 模式: 行末有 `(`, 且不含 `::` (排除函数定义), 且不以 // 开头
COUNT=$(grep -cE "execute_fork_branches\(" "${SCHEDULER_CPP}" || true)
CALL_COUNT=$(grep -E "execute_fork_branches\(" "${SCHEDULER_CPP}" | grep -v "::" | grep -v "^[ ]*//" | wc -l)
DEF_COUNT=$(grep -E "void.*TopoScheduler::execute_fork_branches" "${SCHEDULER_CPP}" | wc -l)
if [[ "${CALL_COUNT}" -ne 1 ]]; then
  echo "FAIL: 契约 1 - fork 处理应仅 1 个调用点, 实际调用 ${CALL_COUNT} 次 (def=${DEF_COUNT}, raw=${COUNT})" >&2
  grep -nE "execute_fork_branches" "${SCHEDULER_CPP}" | head -10 >&2
  FAIL=$((FAIL + 1))
else
  echo "PASS: 契约 1 - execute_fork_branches 仅 1 个调用点 (在 execute() 内)"
  PASS=$((PASS + 1))
fi

# ─────────────────────────────────────────────────────────────
# 契约 2: finish_fork_simulation 是 idempotent (重复调用无副作用)
# 必须含 4 个清理操作: is_executing_fork_branches_ = false +
# current_fork_node_path_.reset() + current_fork_branches_.clear()
# (current_fork_branch_results_ 故意保留到 join 处理)
# ─────────────────────────────────────────────────────────────
if ! grep -q "is_executing_fork_branches_ = false" "${SCHEDULER_CPP}"; then
  echo "FAIL: 契约 2 - finish_fork_simulation 缺 'is_executing_fork_branches_ = false'" >&2
  FAIL=$((FAIL + 1))
elif ! grep -q "current_fork_node_path_.reset()" "${SCHEDULER_CPP}"; then
  echo "FAIL: 契约 2 - finish_fork_simulation 缺 'current_fork_node_path_.reset()'" >&2
  FAIL=$((FAIL + 1))
elif ! grep -q "current_fork_branches_.clear()" "${SCHEDULER_CPP}"; then
  echo "FAIL: 契约 2 - finish_fork_simulation 缺 'current_fork_branches_.clear()'" >&2
  FAIL=$((FAIL + 1))
else
  echo "PASS: 契约 2 - finish_fork_simulation 含 3 个 idempotent 清理操作"
  PASS=$((PASS + 1))
fi

# ─────────────────────────────────────────────────────────────
# 契约 3: dispatch_next_node 函数体内不再含 execute_fork_branches 调用
# (Day 1 fix 核心: fork 处理统一在 execute() L161-167 入口)
# 检测: awk 提取 dispatch_next_node 函数体, 检查无 execute_fork_branches 调用
# ─────────────────────────────────────────────────────────────
DISPATCH_BODY=$(awk '/^std::variant.*TopoScheduler::dispatch_next_node/,/^}$/' "${SCHEDULER_CPP}" || true)
if echo "${DISPATCH_BODY}" | grep -qE "execute_fork_branches\(" ; then
  echo "FAIL: 契约 3 - dispatch_next_node 函数体内仍含 execute_fork_branches 调用 (Day 1 dedup 回归)" >&2
  echo "${DISPATCH_BODY}" | grep -nE "execute_fork_branches" >&2 || true
  FAIL=$((FAIL + 1))
else
  echo "PASS: 契约 3 - dispatch_next_node 函数体不含 fork 处理调用"
  PASS=$((PASS + 1))
fi

# ─────────────────────────────────────────────────────────────
# 契约 4: finish_fork_simulation 含 LOG_DEBUG 日志
# (审计要求: 状态转换必须有日志, 便于 Sprint 6 Oracle 排查)
# ─────────────────────────────────────────────────────────────
# 提取 finish_fork_simulation 函数体
FINISH_BODY=$(awk '/^void TopoScheduler::finish_fork_simulation\(\)/,/^}$/' "${SCHEDULER_CPP}" || true)
if ! echo "${FINISH_BODY}" | grep -q "LOG_DEBUG" ; then
  echo "FAIL: 契约 4 - finish_fork_simulation 缺 LOG_DEBUG 日志" >&2
  FAIL=$((FAIL + 1))
else
  echo "PASS: 契约 4 - finish_fork_simulation 含 LOG_DEBUG 日志"
  PASS=$((PASS + 1))
fi

# ─────────────────────────────────────────────────────────────
# 总结
# ─────────────────────────────────────────────────────────────
echo ""
echo "Fork 静态契约: ${PASS} pass, ${FAIL} fail"
if [[ "${FAIL}" -ne 0 ]]; then
  exit 1
fi
exit 0
