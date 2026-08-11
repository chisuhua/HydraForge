#!/usr/bin/env bash
# tools/check_doxygen_coverage.sh
# 功能描述：Doxygen 注释覆盖率审计 (Phase 6a 新增)
#          扫描 .h 文件的 public API (类/结构体/方法), 验证 @brief 或 /** 注释存在
#          输出覆盖率 % + 缺失项列表, 阈值默认 90%
# 设计依据：openspec/changes/2026-08-10-pdk-safe-exec-tests + ADR-0021 §3.3 Doxygen 覆盖率
# 作者：AgenticDSL Phase 6a
# 最后修改日期：2026-08-10

set -uo pipefail

THRESHOLD="${DCOV_THRESHOLD:-90}"
FILES=()
for arg in "$@"; do
  FILES+=("$arg")
done

if [ ${#FILES[@]} -eq 0 ]; then
  echo "Usage: $0 <file.h> [file2.h ...]" >&2
  echo "  Env: DCOV_THRESHOLD (default 90)" >&2
  exit 2
fi

total_failures=0
for file in "${FILES[@]}"; do
  if [ ! -f "$file" ]; then
    echo "FAIL: $file not found" >&2
    total_failures=$((total_failures + 1))
    continue
  fi

  total=0
  covered=0
  missing=()
  in_private=0

  while IFS=: read -r line_num content; do
    [ -z "$line_num" ] && continue
    raw=$(sed -n "${line_num}p" "$file")
    if echo "$raw" | grep -qE "^\s*private:"; then
      in_private=1
      continue
    fi
    if echo "$raw" | grep -qE "^\s*public:"; then
      in_private=0
      continue
    fi
    if [ "$in_private" -eq 1 ]; then
      continue
    fi
    total=$((total + 1))
    prev_block=$(sed -n "$((line_num - 30)),$((line_num - 1))p" "$file" 2>/dev/null || echo "")
    if echo "$prev_block" | grep -qE "/\*\*|///<"; then
      covered=$((covered + 1))
    else
      trimmed=$(echo "$content" | sed 's/^[[:space:]]*//')
      missing+=("line $line_num: $trimmed")
    fi
  done < <(grep -nE "^\s{0,2}(class |struct |template |auto |void |int |std::\w+|nlohmann::)" "$file")

  if [ "$total" -eq 0 ]; then
    echo "Coverage: N/A (no public API candidates) for $file"
    continue
  fi

  pct=$((covered * 100 / total))
  echo "Coverage: ${pct}% (${covered}/${total}) for $file"
  if [ "$pct" -lt "$THRESHOLD" ]; then
    echo "  FAIL: below ${THRESHOLD}% threshold"
    echo "  Missing:"
    for m in "${missing[@]}"; do
      echo "    - $m"
    done
    total_failures=$((total_failures + 1))
  else
    echo "  PASS"
  fi
done

if [ "$total_failures" -gt 0 ]; then
  echo "Total files failing: $total_failures"
  exit 1
fi
exit 0
