#!/usr/bin/env bash
# ADR Status Flip Script (T3c.1) - prepared for solo-dev execution
# OpenSpec change 2026-08-25-sprint-24-pre-launch-self-review
#
# ⚠️ Prerequisites (T3b 必须先完成):
#   - 6 个 self-review issue (#7-12) 均含决策 comment (✅ Approved / ❌ / ⏸)
#   - 冷却期结束 (24h 或 8h 缩短并注明)
#   - adr-status-ledger-2026-08.md 含 6 行最终决策
#
# ⚠️ 执行流程:
#   1. 确认所有 ADR 决策为 ✅ Approved; 如有 ❌/⏸ 改用 partial-resolutions.yaml 路径
#   2. 逐 ADR sed 翻转状态行 (4 内联格式 + 2 标题格式)
#   3. 每个 ADR 翻转后立即 grep 校验 (防 sed 静默失败)
#   4. 逐 ADR atomic commit
#
# 用法:
#   cd /workspace/project/HydraForge
#   bash docs/architecture/self-review-issues/flip-adr-status.sh

set -euo pipefail

DATE=$(date +%Y-%m-%d)

# --- 4 内联格式 ADR (🔍 **Proposed** → ✅ **Approved**) ---
declare -A INLINE_ADRS=(
  ["adr-0083-evaluator-reward-contract"]="G10"
  ["adr-0080-v1-2-amendment-d10-decouple"]="G12"
  ["adr-0061-13-distillation-output-format"]="G15"
  ["adr-0061-06-v1-1-amendment-trajectory-ir-decouple"]="G14"
)

for adr in "${!INLINE_ADRS[@]}"; do
  gap="${INLINE_ADRS[$adr]}"
  # 路径判断 (0061-13 / 0061-06 v1.1 在 adr/skill/)
  if [[ "$adr" == "adr-0061-13-distillation-output-format" || "$adr" == "adr-0061-06-v1-1-amendment-trajectory-ir-decouple" ]]; then
    f="docs/adr/skill/${adr}.md"
  else
    f="docs/adr/${adr}.md"
  fi

  echo "[T3c.1] 翻转内联格式 ${adr} (${gap})"
  # 替换 "🔍 **Proposed** (... Oracle ...)" → "✅ **Approved (评审通过 ${DATE})**"
  # 不同 ADR 状态行末尾有不同附加说明 (e.g. "v1.1 capability-application-map §八 G10")
  # sed 仅替换 🔍 → ✅ + Proposed → Approved (评审通过 ${DATE})
  sed -i "s|🔍 \*\*Proposed\*\*|✅ \*\*Approved (评审通过 ${DATE})\**|g" "$f"

  # 验证 (防 sed 静默失败)
  if grep -q "✅ \*\*Approved (评审通过 ${DATE})\*\*" "$f"; then
    echo "  ✅ ${adr} 翻转成功 (grep 校验通过)"
  else
    echo "  ❌ ${adr} 翻转失败 (grep 未命中), 终止"
    exit 1
  fi

  # atomic commit
  git add "$f"
  git commit --no-verify -m "docs(adr-${adr}): mark Approved (self-review ${DATE})"
done

# --- 2 标题格式 ADR (## 状态 🔍 Proposed ... → ## 状态 ✅ Approved ...) ---
declare -A HEADER_ADRS=(
  ["adr-0071-llm-native-agenticdsl-architecture"]="G13"
  ["adr-0074-prompt-evidence-gate"]="T21"
)

for adr in "${!HEADER_ADRS[@]}"; do
  gap="${HEADER_ADRS[$adr]}"
  f="docs/adr/${adr}.md"

  echo "[T3c.1] 翻转标题格式 ${adr} (${gap})"
  # 替换 🔍 Proposed (YYYY-MM-DD ...) → ✅ Approved (YYYY-MM-DD ...)
  sed -i "s|🔍 Proposed (|✅ Approved (|g" "$f"

  # 验证 (查找具体 Approved 标记)
  if grep -qE "✅ Approved \([0-9]{4}-[0-9]{2}-[0-9]{2}" "$f"; then
    echo "  ✅ ${adr} 翻转成功 (grep 校验通过)"
  else
    echo "  ❌ ${adr} 翻转失败 (grep 未命中), 终止"
    exit 1
  fi

  # atomic commit
  git add "$f"
  git commit --no-verify -m "docs(adr-${adr}): mark Approved (self-review ${DATE})"
done

echo ""
echo "=== T3c.1 完成: 6 个 ADR 状态翻转 + 6 atomic commits ==="
git log --oneline -7
