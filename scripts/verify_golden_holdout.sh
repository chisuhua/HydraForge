#!/usr/bin/env bash
# ADR-0074 D-2: Golden Suite Hold-out Verification
# Verifies 51 golden task_ids do NOT appear in fewshot examples
# Exit 0 = clean, Exit 1 = leak detected (CI failure)

set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

GOLDEN_DIR="lib/prompts/golden"
FEWSHOT_DIR="lib/prompts/fewshot"

if [ ! -d "$GOLDEN_DIR" ]; then
    echo "ERROR: $GOLDEN_DIR does not exist"
    exit 1
fi

LEAKS=0

# Collect all task_ids from golden tasks
GOLDEN_IDS=$(grep -rh "^task_id:" "$GOLDEN_DIR" 2>/dev/null | awk '{print $2}' | sort -u)

# Check each task_id against fewshot directory
for id in $GOLDEN_IDS; do
    if [ -z "$id" ]; then continue; fi
    matches=$(grep -rn "$id" "$FEWSHOT_DIR" 2>/dev/null || true)
    if [ -n "$matches" ]; then
        echo "LEAK: task_id '$id' found in fewshot:"
        echo "$matches"
        LEAKS=$((LEAKS + 1))
    fi
done

if [ "$LEAKS" -gt 0 ]; then
    echo ""
    echo "❌ Hold-out FAILED: $LEAKS golden task_id(s) leaked into fewshot"
    exit 1
fi

echo "✅ Hold-out PASSED: $(echo "$GOLDEN_IDS" | wc -l) golden task_ids clean"
exit 0
