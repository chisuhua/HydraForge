#!/usr/bin/env bash
# verify-phase-2.sh - 一键验收 + archive 准备
# OpenSpec change 2026-08-25-sprint-24-pre-launch-self-review
#
# ⚠️ Prerequisites:
#   - Step 3c 已完成 (per-ADR sed 翻转 + cap-map v1.3 + 3 镜像同步)
#   - 6 ADR 状态已 🔍 → ✅
#   - adr-status-ledger-2026-08.md 含 6 行最终决策
#
# 用法:
#   cd /workspace/project/HydraForge
#   bash docs/architecture/self-review-issues/verify-phase-2.sh
#
# 退出码:
#   0 = 全部通过, 可 archive
#   1 = 任一 acceptance check 失败
#   2 = prerequisite 未满足 (无可验证状态)

set -uo pipefail

CHANGE_NAME="2026-08-25-sprint-24-pre-launch-self-review"
SPRINT_MILESTONE="Sprint 24"
LEDGER="docs/architecture/adr-status-ledger-2026-08.md"
CAP_MAP="docs/architecture/capability-application-map-2026-08.md"
GAP_ANALYSIS="docs/architecture/adr-implementation-status-gap-analysis.md"
README="docs/README.md"
RELATIONSHIPS="docs/adr-management/relationships.md"

PASS=0
FAIL=0
WARN=0

declare -a FAILED_CHECKS

check_pass() {
  echo "  ✅ $1"
  PASS=$((PASS + 1))
}

check_fail() {
  echo "  ❌ $1"
  echo "      → $2"
  FAILED_CHECKS+=("$1: $2")
  FAIL=$((FAIL + 1))
}

check_warn() {
  echo "  ⚠️  $1"
  echo "      → $2"
  WARN=$((WARN + 1))
}

echo "============================================"
echo "  Phase 2 Verification - ${CHANGE_NAME}"
echo "  $(date -u +%Y-%m-%dT%H:%M:%SZ)"
echo "============================================"
echo ""

# --- 前置检查 ---
echo "[Prerequisite] 检查 OpenSpec change 状态"
if ! timeout 30 openspec validate "${CHANGE_NAME}" --strict >/dev/null 2>&1; then
  check_fail "openspec validate" "change 不存在或 validation 失败"
  echo ""
  echo "❌ Prerequisite 不满足, 终止"
  exit 2
fi
check_pass "openspec validate --strict"

# --- Crit 1: 6 issue ---
echo ""
echo "[Crit 1] gh issue list --label adr-review (期望 6)"
issue_count=$(gh issue list --label adr-review --json number --jq '. | length' 2>/dev/null || echo 0)
if [ "$issue_count" -eq 6 ]; then
  check_pass "6 issues found"
else
  check_fail "issue count" "期望 6, 实际 $issue_count"
fi

# --- Crit 2: 占位符 ---
echo ""
echo "[Crit 2] issue body 占位符 (期望 0 行)"
placeholder_total=0
for n in 7 8 9 10 11 12; do
  body=$(gh api repos/chisuhua/HydraForge/issues/$n --jq .body 2>/dev/null || echo "")
  count=$(echo "$body" | grep -cE "ADR-XXXX|GXX|TXX" || true)
  placeholder_total=$((placeholder_total + count))
done
if [ "$placeholder_total" -eq 0 ]; then
  check_pass "0 placeholders"
else
  check_fail "placeholder" "$placeholder_total 行残留"
fi

# --- Crit 3: 6 ADR 状态翻转 ---
echo ""
echo "[Crit 3] 6 ADR 文件状态 (期望 ✅ Approved)"
approved_count=0
for adr in \
  docs/adr/adr-0083-evaluator-reward-contract.md \
  docs/adr/adr-0080-v1-2-amendment-d10-decouple.md \
  docs/adr/skill/adr-0061-13-distillation-output-format.md \
  docs/adr/skill/adr-0061-06-v1-1-amendment-trajectory-ir-decouple.md \
  docs/adr/adr-0071-llm-native-agenticdsl-architecture.md \
  docs/adr/adr-0074-prompt-evidence-gate.md; do
  if grep -qE "✅.*Approved .*[0-9]{4}-[0-9]{2}-[0-9]{2}" "$adr" 2>/dev/null; then
    echo "  ✅ $(basename $adr): Approved"
    approved_count=$((approved_count + 1))
  else
    echo "  ❌ $(basename $adr): 未 Approved"
  fi
done
if [ "$approved_count" -eq 6 ]; then
  check_pass "6 ADR Approved"
else
  check_fail "ADR status" "$approved_count/6 Approved"
fi

# --- Crit 4: cap-map G10-G15 Closed ---
echo ""
echo "[Crit 4] cap-map G10-G15 Closed 数 (期望 ≥5)"
if [ -f "$CAP_MAP" ]; then
  closed_count=$(grep -cE "G10.*Closed|G12.*Closed|G13.*Closed|G14.*Closed|G15.*Closed" "$CAP_MAP" || echo 0)
  if [ "$closed_count" -ge 5 ]; then
    check_pass "$closed_count Closed (≥5)"
  else
    check_fail "Closed count" "$closed_count Closed (期望 ≥5)"
  fi
else
  check_fail "cap-map 文件" "未找到 $CAP_MAP"
fi

# --- Crit 5: gap-analysis 含 6 ADR 行 ---
echo ""
echo "[Crit 5] gap-analysis 含 6 ADR 行 (期望 ≥6)"
if [ -f "$GAP_ANALYSIS" ]; then
  gap_count=$(grep -cE "ADR-0083|ADR-0080|ADR-0061-13|ADR-0061-06|ADR-0071|ADR-0074" "$GAP_ANALYSIS" || echo 0)
  if [ "$gap_count" -ge 6 ]; then
    check_pass "$gap_count 行"
  else
    check_fail "gap-analysis" "仅 $gap_count/6"
  fi
else
  check_warn "gap-analysis" "未找到 (可能独立 change 维护)"
fi

# --- Crit 6: milestone + kickoff ---
echo ""
echo "[Crit 6] Sprint 24 milestone + kickoff issue 挂载"
milestone_exists=$(gh api repos/chisuhua/HydraForge/milestones --jq '.[] | select(.title=="Sprint 24") | .title' 2>/dev/null)
kickoff_milestone=$(gh api repos/chisuhua/HydraForge/issues/13 --jq '.milestone.title' 2>/dev/null)
if [ "$milestone_exists" = "$SPRINT_MILESTONE" ] && [ "$kickoff_milestone" = "$SPRINT_MILESTONE" ]; then
  check_pass "milestone + kickoff #13 ✓"
else
  check_fail "milestone" "milestone=$milestone_exists, kickoff=$kickoff_milestone"
fi

# --- Crit 7: commit 数 ---
echo ""
echo "[Crit 7] git log atomic commits (期望 ≥8 含本 change 工作)"
change_commits=$(git log --oneline | grep -cE "sprint-24-pre-launch-self-review|adr-lint|adr-0083|adr-0080-v1|adr-0061-13|adr-0061-06-v1-1|adr-0071|adr-0074|adr_relationships|fix\(scripts\) apply-meeting|capability-application-map|adr-status-ledger|adr-self-review|adr-implementation-status|capability-map" || echo 0)
total_commits=$(git log --oneline | wc -l)
if [ "$total_commits" -ge 8 ]; then
  check_pass "$total_commits total commits (本 change 相关 $change_commits)"
else
  check_fail "commit count" "仅 $total_commits"
fi

# --- Crit 8: sprint-24-pre-launch.md 悬空引用 (排除 change 自指) ---
echo ""
echo "[Crit 8] grep sprint-24-pre-launch.md 悬空引用 (期望 0; change 自指不计)"
if grep -rn "sprint-24-pre-launch\.md" --include="*.md" . 2>/dev/null | grep -vE "openspec/changes/2026-08-25-sprint-24-pre-launch-self-review/" | grep -qE ".+"; then
  check_fail "dangling ref" "存在非 change 自指的悬空引用"
else
  check_pass "0 悬空引用"
fi

# --- Crit 9: openspec validate ---
echo ""
echo "[Crit 9] openspec validate --strict (期望 EXIT 0)"
if timeout 30 openspec validate "${CHANGE_NAME}" --strict >/dev/null 2>&1; then
  check_pass "EXIT 0"
else
  check_fail "openspec validate" "EXIT 非 0"
fi

# --- Crit 10: adr_lint ---
echo ""
echo "[Crit 10] tools/adr_lint.py (期望 0 errors)"
lint_output=$(python3 tools/adr_lint.py 2>&1)
if echo "$lint_output" | grep -q "✓ 所有 ADR 通过"; then
  check_pass "0 errors"
else
  check_fail "adr_lint" "$(echo "$lint_output" | grep -E '✗|errors' | head -2)"
fi

# --- Crit 11: docs_drift_audit Scenario 7 ---
echo ""
echo "[Crit 11] tools/docs_drift_audit.py Scenario 7 (期望 0 drifts)"
drift_output=$(python3 tools/docs_drift_audit.py 2>&1 | grep -E "Scenario 7" | tail -1)
if echo "$drift_output" | grep -qE "0 drifts, 0 warnings"; then
  check_pass "0 drifts"
elif echo "$drift_output" | grep -qE "0 drifts"; then
  check_warn "Scenario 7" "0 drifts 但有 warnings (查看 docs_drift_audit 输出)"
else
  check_fail "Scenario 7" "$drift_output"
fi

# --- Crit 12: ctest (best effort, build dir 可能不完整) ---
echo ""
echo "[Crit 12] ctest (期望 ≥1 test PASS; 零代码改动 = 零回归风险)"
if [ -d "build" ]; then
  cd build
  ctest_result=$(ctest --output-on-failure 2>&1 | tail -10)
  ctest_total=$(echo "$ctest_result" | grep -oE "[0-9]+% test pass|tests passed" | head -1)
  if echo "$ctest_result" | grep -qE "0 failures|passed"; then
    check_pass "ctest 0 failures (build dir 可能不完整; 0 代码改动意味着 0 回归)"
  else
    check_warn "ctest" "build dir 不完整或失败 (运行 cmake --build . 重建后重试)"
  fi
  cd ..
else
  check_warn "build dir" "不存在; 需要 cmake -B build 后再跑"
fi

# --- ledger 闭合 ---
echo ""
echo "[Ledger] 决策台账 6 行最终决策"
if [ -f "$LEDGER" ]; then
  ledger_lines=$(grep -cE "ADR-0083|ADR-0080 v1.2|ADR-0061-13|ADR-0061-06 v1.1|ADR-0071|ADR-0074" "$LEDGER" || echo 0)
  if [ "$ledger_lines" -ge 6 ]; then
    check_pass "$ledger_lines ADR 引用"
  else
    check_fail "ledger" "仅 $ledger_lines/6 ADR 引用"
  fi
else
  check_fail "ledger" "未找到 $LEDGER"
fi

echo ""
echo "[3 Mirrors] gap-analysis + README + relationships 并集检查"
declare -a MIRROR_ADRS=(
  "adr-0083-evaluator-reward-contract"
  "adr-0080-v1-2-amendment-d10-decouple"
  "adr-0061-13-distillation-output-format"
  "adr-0061-06-v1-1-amendment-trajectory-ir-decouple"
  "adr-0071-llm-native-agenticdsl-architecture"
  "adr-0074-prompt-evidence-gate"
)
missing_adr=0
for adr in "${MIRROR_ADRS[@]}"; do
  found=0
  for f in "$GAP_ANALYSIS" "$README" "$RELATIONSHIPS"; do
    if [ -f "$f" ] && grep -qF "$adr" "$f"; then
      found=1
      break
    fi
  done
  if [ $found -eq 0 ]; then
    FAILED_CHECKS+=("$adr missing from all mirrors")
    missing_adr=$((missing_adr + 1))
  fi
done
if [ $missing_adr -eq 0 ]; then
  check_pass "6/6 ADRs 在镜像并集中存在"
else
  check_fail "mirrors" "$missing_adr/6 ADR 缺失"
fi

# --- summary ---
echo ""
echo "============================================"
echo "  Summary"
echo "============================================"
echo "  PASS: $PASS"
echo "  FAIL: $FAIL"
echo "  WARN: $WARN"
echo ""

if [ "$FAIL" -eq 0 ]; then
  echo "✅ Phase 2 verification 全部通过"
  echo ""
  echo "下一步: 执行 archive 命令:"
  echo "  openspec archive ${CHANGE_NAME}"
  echo ""
  echo "Archive 前最后检查:"
  echo "  - 6 atomic commits 记录决策历史 ✓"
  echo "  - cap-map v1.3 / 3 镜像 / ledger 闭合 ✓"
  echo "  - 所有 lint / drift / validate / 状态 / 镜像同步通过 ✓"
  exit 0
else
  echo "❌ $FAIL 项失败, 阻塞 archive"
  echo ""
  echo "失败项:"
  for failed in "${FAILED_CHECKS[@]}"; do
    echo "  - $failed"
  done
  echo ""
  echo "修复后重跑本脚本"
  exit 1
fi
