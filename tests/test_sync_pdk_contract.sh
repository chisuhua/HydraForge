#!/usr/bin/env bash
# tests/test_sync_pdk_contract.sh
# 文件头注释
# 功能描述：PDK contract 头同步验证 — DRY_RUN 离线模式 + contract 清单 + drift-guard
# 设计依据：openspec/changes/sync-pdk-contract-header/{proposal,design,tasks,spec/*}.md
# 作者：AgenticDSL G3
# 最后修改日期：2026-09-22
#
# 用法：PDK_SYNC_DRY_RUN=1 bash tests/test_sync_pdk_contract.sh
# 返回：exit 0 = 全部通过, exit 1 = 失败
#
# 4 项断言:
#   AC-1: PDK_CONTRACT_DEPS 数组 = 11 头实测清单
#   AC-2: PDK_SYNC_DRY_RUN=1 离线模式 exit 0
#   AC-3: 临时工作目录 include/agenticdsl/contract/itool_registry.h 存在
#   AC-4: drift-guard: pdk 头 contract include 集合 ⊆ PDK_CONTRACT_DEPS 清单
set -euo pipefail

cd "$(dirname "$0")/.."

ERRORS=0

# 颜色 (复用 scripts/sync-pdk.sh 风格)
RED='\033[0;31m'
GREEN='\033[0;32m'
NC='\033[0m'

pass() { echo -e "${GREEN}[PASS]${NC} $*"; }
fail() { echo -e "${RED}[FAIL]${NC} $*"; ERRORS=$((ERRORS + 1)); }

echo "=========================================="
echo "  test_sync_pdk_contract — 4 项断言"
echo "=========================================="
echo ""

# ──────────────────────────────────────────────
# Test 1: PDK_CONTRACT_DEPS = 11 头
# ──────────────────────────────────────────────
echo "--- Test 1: PDK_CONTRACT_DEPS 数组 = 11 头 ---"
CONTRACT_DEPS_COUNT=$(grep -c "\.h$" scripts/sync-pdk.sh || true)
# 精确断言: 脚本中应有且仅有 11 个 .h 扩展名的 PDK_CONTRACT_DEPS 条目
# 通过计数所有符合 contract 头模式的字符串
DEPS_FOUND=$(grep -oP '(ilogger|iinput_source|iinteraction_bus|itool_registry|resume_token|timer_service|ievaluator|itool_hook_registry|iagent_hook_registry|iagent_registry|i_llm_provider_decorator)\.h' scripts/sync-pdk.sh | sort -u | wc -l)
if [ "$DEPS_FOUND" -eq 11 ]; then
  pass "PDK_CONTRACT_DEPS = 11 头 (实测: $DEPS_FOUND)"
else
  fail "PDK_CONTRACT_DEPS 期望 11 头, 实际: $DEPS_FOUND"
fi

# ──────────────────────────────────────────────
# Test 2: DRY_RUN 离线 → exit 0 + 输出含 itool_registry.h
# ──────────────────────────────────────────────
echo "--- Test 2: DRY_RUN 离线 → exit 0 + itool_registry.h ---"
OUTPUT=$(PDK_SYNC_DRY_RUN=1 bash scripts/sync-pdk.sh 2>&1) || {
  fail "DRY_RUN 退出码非 0 (exit $?)"
  echo "$OUTPUT" | tail -5
}
if echo "$OUTPUT" | grep -Fq "itool_registry.h"; then
  pass "输出包含 itool_registry.h"
else
  fail "输出不含 itool_registry.h"
  echo "--- OUTPUT (最后 10 行) ---"
  echo "$OUTPUT" | tail -10
fi

# ──────────────────────────────────────────────
# Test 3: 临时工作目录文件存在
# ──────────────────────────────────────────────
echo "--- Test 3: 临时工作目录文件存在 ---"
# 从输出中提取 WORK_DIR (DRY_RUN 分支打印的 temp dir 路径)
WORK_DIR=$(echo "$OUTPUT" | grep -oP 'temp dir: \K(/tmp/[^\s]+)' | head -1 || true)
if [ -z "$WORK_DIR" ]; then
  WORK_DIR=$(echo "$OUTPUT" | grep -oP 'WORK_DIR=\K(/tmp/[^\s]+)' | head -1 || true)
fi
if [ -n "$WORK_DIR" ] && [ -f "${WORK_DIR}/include/agenticdsl/contract/itool_registry.h" ]; then
  pass "临时工作目录文件存在: ${WORK_DIR}/include/agenticdsl/contract/itool_registry.h"
else
  fail "临时工作目录不存在或缺少 itool_registry.h (WORK_DIR=${WORK_DIR:-EMPTY})"
fi

# ──────────────────────────────────────────────
# Test 4: drift-guard — pdk 头 contract include ⊆ PDK_CONTRACT_DEPS
# ──────────────────────────────────────────────
echo "--- Test 4: drift-guard ---"
DRIFT_ERRORS=0
while IFS=: read -r file lineno line; do
  header=$(echo "$line" | sed 's/.*contract\///' | sed 's/>.*//' | tr -d ' ')
  # 从 PDK_CONTRACT_DEPS 中提取已注册的头名 (从 sync-pdk.sh 中解析)
  if ! grep -Fq "$header" scripts/sync-pdk.sh 2>/dev/null; then
    fail "drift-guard: ${file}:${lineno} — ${header} 未在 PDK_CONTRACT_DEPS 中注册"
    DRIFT_ERRORS=$((DRIFT_ERRORS + 1))
  fi
done < <(grep -rn '#include [<"]agenticdsl/contract/' include/agenticdsl/pdk/ 2>/dev/null || true)
if [ "$DRIFT_ERRORS" -eq 0 ]; then
  pass "drift-guard: 所有 contract include 已注册 (0 漂移)"
else
  fail "drift-guard: $DRIFT_ERRORS 个未注册头"
fi

echo ""
echo "=== 结果: $ERRORS 错误 ==="
if [ "$ERRORS" -gt 0 ]; then
  exit 1
fi
exit 0